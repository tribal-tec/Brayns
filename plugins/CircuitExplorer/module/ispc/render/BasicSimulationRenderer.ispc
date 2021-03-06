/* Copyright (c) 2018, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
 * Responsible Author: Cyrille Favreau <cyrille.favreau@epfl.ch>
 *
 * This file is part of the circuit explorer for Brayns
 * <https://github.com/favreau/Brayns-UC-CircuitExplorer>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3.0 as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "utils/SimulationRenderer.ih"

struct BasicSimulationRenderer
{
    SimulationRenderer super;

    // Shading attributes
    float alphaCorrection;
    float simulationThreshold;
};

inline vec3f BasicSimulationRenderer_shadeRay(
    const uniform BasicSimulationRenderer* uniform self,
    varying ScreenSample& sample)
{
    Ray ray = sample.ray;
    ray.time = inf;
    sample.z = inf;

    float zDepth = 0.f;
    int depth = 0;
    float oldRefraction = 1.f;

    const uniform Material* previousMaterial = 0;
    vec4f pathColor = make_vec4f(0.f);

    while (pathColor.w < 1.f)
    {
        vec4f colorContribution;
        traceRay(self->super.super.super.model, ray);

        if (ray.geomID < 0)
        {
            // No intersection
            colorContribution = skyboxMapping((Renderer*)self, ray,
                                              self->super.super.bgMaterial);
            colorContribution.w = 1.f;
            composite(colorContribution, pathColor, self->alphaCorrection);
            break;
        }

        // Intersection found.
        // Retreive information about the geometry, typically geometry ID,
        // normal to the surface, material ID, texture coordinates, etc.
        DifferentialGeometry dg;
        postIntersect(self->super.super.super.model, dg, ray,
                      DG_NG | DG_NS | DG_NORMALIZE | DG_FACEFORWARD |
                          DG_MATERIALID | DG_COLOR | DG_TEXCOORD);

        const vec3f intersection = ray.org + ray.t * ray.dir;

        // Material management
        const uniform Material* material = dg.material;

        if (material != previousMaterial)
        {
            // Material attributes
            MaterialShadingMode shadingMode = diffuse;
            const uniform ExtendedMaterial* objMaterial =
                (const uniform ExtendedMaterial*)material;

            vec3f Kd;
            float opacity;

            if (!objMaterial)
            {
                Kd = make_vec3f(dg.color);
                opacity = dg.color.w;
            }
            else
                foreach_unique(mat in objMaterial)
                {
                    Kd = make_vec3f(dg.color) * mat->Kd;
                    opacity = dg.color.w * mat->d;
                    shadingMode = mat->shadingMode;
                    if (opacity < 1.f)
                    {
                        ray.dir = refractedVector(ray.dir, dg.Ns, oldRefraction,
                                                  mat->refraction);
                        oldRefraction = mat->refraction;
                    }
                }

            vec4f simulationContribution = make_vec4f(0.f);
            bool ignoreIntersection = false;
            if (objMaterial->castSimulationData == 1)
            {
                // Get simulation value from geometry
                simulationContribution =
                    getSimulationValue(&self->super, &dg, ray.primID);
                ignoreIntersection =
                    (simulationContribution.w < self->simulationThreshold);
            }

            if (!ignoreIntersection)
            {
                // Head-light shading
                float cosNL =
                    (shadingMode == none ? 1.f
                                         : max(0.f, dot(neg(ray.dir), dg.Ns)));

                switch (shadingMode)
                {
                case electron:
                case electron_transparency:
                    cosNL = 1.f - cosNL;
                    opacity *=
                        (shadingMode == electron_transparency ? cosNL : 1.f);
                    break;
                case diffuse_transparency:
                    opacity *= cosNL;
                    break;
                case cartoon:
                    const unsigned int angleAsInt =
                        cosNL * DEFAULT_CARTOON_GRADIENT;
                    cosNL =
                        (float)(angleAsInt) / (float)(DEFAULT_CARTOON_GRADIENT);
                    break;
                case perlin:
                    const vec3f point = ray.org + ray.t * ray.dir;
                    float n = noise(point);
                    if (ray.t < dg.epsilon * 1.5e4f)
                        n += worleyNoise(point, 2.f);
                    else
                        n *= 2.f;

                    n = clamp(n, 0.f, 1.f);
                    cosNL *= n;
                    break;
                }

                const vec3f color =
                    cosNL * (Kd * (1.f - simulationContribution.w) +
                             simulationContribution.w *
                                 make_vec3f(simulationContribution));

                colorContribution = make_vec4f(color, opacity);

                // Compositing according to alpha correction
                composite(colorContribution, pathColor, self->alphaCorrection);
            }
        }

        // Next ray
        ray.org = intersection;
        ray.t0 = dg.epsilon;
        ray.t = inf;

        ray.primID = -1;
        ray.geomID = -1;
        ray.instID = -1;

        previousMaterial = material;

        // Depth
        if (depth == 0)
            sample.z = ray.t;

        ++depth;
    }

    // Alpha
    sample.alpha = pathColor.w;

    return make_vec3f(pathColor) * self->super.pixelAlpha;
}

void BasicSimulationRenderer_renderSample(uniform Renderer* uniform _self,
                                          void* uniform perFrameData,
                                          varying ScreenSample& sample)
{
    uniform BasicSimulationRenderer* uniform self =
        (uniform BasicSimulationRenderer * uniform) _self;
    sample.rgb = BasicSimulationRenderer_shadeRay(self, sample);
}

// Exports (called from C++)
export void* uniform BasicSimulationRenderer_create(void* uniform cppE)
{
    uniform BasicSimulationRenderer* uniform self =
        uniform new uniform BasicSimulationRenderer;
    Renderer_Constructor(&self->super.super.super, cppE);
    self->super.super.super.renderSample = BasicSimulationRenderer_renderSample;
    return self;
}

export void BasicSimulationRenderer_set(
    void* uniform _self, void* uniform secondaryModel, void* uniform bgMaterial,
    const uniform int& spp, uniform float* uniform simulationData,
    const uniform int64 simulationDataSize,
    const uniform float& alphaCorrection,
    const uniform float& simulationThreshold, const uniform float& pixelAlpha,
    const uniform float& fogThickness, const uniform float& fogStart)
{
    uniform BasicSimulationRenderer* uniform self =
        (uniform BasicSimulationRenderer * uniform) _self;

    self->super.secondaryModel = (uniform Model * uniform) secondaryModel;
    self->super.super.bgMaterial =
        (uniform ExtendedMaterial * uniform) bgMaterial;

    self->super.super.super.spp = spp;

    self->super.simulationData = (uniform float* uniform)simulationData;
    self->super.simulationDataSize = simulationDataSize;

    self->alphaCorrection = alphaCorrection;
    self->simulationThreshold = simulationThreshold;

    self->super.pixelAlpha = pixelAlpha;

    self->super.fogThickness = fogThickness;
    self->super.fogStart = fogStart;
}
