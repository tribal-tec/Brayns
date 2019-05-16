/* Copyright (c) 2019, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
 *
 * This file is part of Brayns <https://github.com/BlueBrain/Brayns>
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

#include <optix_world.h>
#include "../../CommonStructs.h"
#include "../Helpers.h"

using namespace optix;

//#define TEST

// Scene
rtDeclareVariable(optix::Ray, ray, rtCurrentRay, );
rtDeclareVariable(PerRayData_radiance, prd, rtPayload, );
rtDeclareVariable(float, t_hit, rtIntersectionDistance, );

// Material attributes

rtDeclareVariable(float3, geometric_normal, attribute geometric_normal, );
rtDeclareVariable(float3, shading_normal, attribute shading_normal, );

// Textures
#ifndef TEST
rtDeclareVariable(int, albedoMetallic_map, , );
rtDeclareVariable(int, normalRoughness_map, , );
rtDeclareVariable(float2, texcoord, attribute texcoord, );
#endif
rtDeclareVariable(float, metalness, , );
rtDeclareVariable(float, roughness, , );

// Lights
rtBuffer<BasicLight> lights;

// Vertices
rtDeclareVariable(float3, v0, attribute v0, );
rtDeclareVariable(float3, v1, attribute v1, );
rtDeclareVariable(float3, v2, attribute v2, );
rtDeclareVariable(float2, t0, attribute t0, );
rtDeclareVariable(float2, t1, attribute t1, );
rtDeclareVariable(float2, t2, attribute t2, );

rtDeclareVariable(float2, ddx, attribute ddx, );
rtDeclareVariable(float2, ddy, attribute ddy, );

rtDeclareVariable(uint, use_envmap, , );
rtDeclareVariable(int, envmap_radiance, , );
rtDeclareVariable(int, envmap_irradiance, , );
rtDeclareVariable(int, envmap_brdf_lut, , );
rtDeclareVariable(uint, radianceLODs, , );

static __device__ inline float calculateAttenuation(float3 WorldPos, float3 lightPos)
{
    float distance = length(lightPos - WorldPos);
    return 1.0f / (distance * distance);
}

static __device__ inline float distributionGGX(float3 N, float3 H, float roughness)
{
    const float a = roughness * roughness;
    const float a2 = a*a;
    const float NdotH  = max(dot(N, H), 0.0f);
    const float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = M_PIf * denom * denom;

    return a2 / denom;
}

static __device__ inline float GeometrySchlickGGX(float NdotV, float roughness)
{
    const float r = roughness + 1.0f;
    const float k = (r*r) / 8.0f;

    float denom = NdotV * (1.0 - k) + k;

    return NdotV / denom;
}

static __device__ inline float geometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    const float NdotV = max(dot(N, V), 0.0f);
    const float NdotL = max(dot(N, L), 0.0f);
    const float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    const float ggx1  = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

static __device__ inline float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

//static __device__ inline float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
//{
//    return F0 + (max(make_float3(1.0f - roughness), F0) - F0) * pow(1.0f - cosTheta, 5.0f);
//}

static __device__ inline float3 getIBLContribution(float NdV, float roughness, const float3& n, const float3& reflection, const float3& diffuseColor, const float3& specularColor)
{
    // Sample 2 levels and mix between to get smoother degradation
    const float ENV_LODS = 6.0f;
    float blend = roughness * ENV_LODS;
    float level0 = floor(blend);
    float level1 = min(ENV_LODS, level0 + 1.0f);
    blend -= level0;

    // Sample the specular env map atlas depending on the roughness value
    float2 uvSpec = getEquirectangularUV(reflection);
    uvSpec.y /= 2.0f;

    float2 uv0 = uvSpec;
    float2 uv1 = uvSpec;

    uv0 /= pow(2.0f, level0);
    uv0.y += 1.0f - exp(-M_LN2f * level0);

    uv1 /= pow(2.0f, level1);
    uv1.y += 1.0f - exp(-M_LN2f * level1);

    float2 irradianceUV = getEquirectangularUV(n);

    float3 diffuseLight = make_float3(RGBMToLinear(rtTex2D<float4>(envmap_irradiance, irradianceUV.x, 1.f-irradianceUV.y)));
    float3 specular0 = make_float3(RGBMToLinear(rtTex2D<float4>(envmap_radiance, uv0.x, 1.f-uv0.y)));
    float3 specular1 = make_float3(RGBMToLinear(rtTex2D<float4>(envmap_radiance, uv1.x, 1.f-uv1.y)));

    float3 specularLight = lerp(specular0, specular1, blend);
    float3 diffuse = diffuseLight * diffuseColor;

    const float3 brdf = make_float3(SRGBtoLinear(rtTex2D<float4>(envmap_brdf_lut, NdV, roughness)));

    // Bit of extra reflection for smooth materials
    float reflectivity = pow((1.0 - roughness), 2.0) * 0.05;
    float3 specular = specularLight * (specularColor * brdf.x + brdf.y + reflectivity);

    return diffuse + specular;
}


static __device__ inline void shade()
{
    const float3 world_shading_normal = optix::normalize(
        rtTransformNormal(RT_OBJECT_TO_WORLD, shading_normal));

    //float3 N = optix::faceforward(world_geometric_normal, -ray.direction,
    //                              world_geometric_normal);
    float3 N = world_shading_normal;

#ifndef TEST
    const float3 edge1 = v1 - v0;
    const float3 edge2 = v2 - v0;
    const float2 deltaUV1 = t1 - t0;
    const float2 deltaUV2 = t2 - t0;
    float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

    float3 tangent;
    tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
    tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
    tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
    tangent = normalize(rtTransformNormal(RT_OBJECT_TO_WORLD, tangent));

    tangent = normalize(tangent - dot(tangent, N) * N);

    float3 bitangent = cross(N,tangent);
#endif

    const float3 WorldPos = ray.origin + t_hit * ray.direction;
    const float3 V = -ray.direction;

#ifdef TEST
    const float4 albedoMetallic = make_float4(1.f, 0.f, 0.f, metalness);
    const float3 albedo = make_float3(albedoMetallic);
    const float4 normalRoughness = make_float4(0.f, 0.f, 0.f, roughness);
#else
    const float4 albedoMetallic = SRGBtoLinear(rtTex2DGrad<float4>(albedoMetallic_map, texcoord.x, texcoord.y, ddx, ddy));
    const float3 albedo = make_float3(albedoMetallic);

    const float4 normalRoughness = rtTex2DGrad<float4>(normalRoughness_map, texcoord.x, texcoord.y, ddx, ddy);
    const float3 normal = normalize(make_float3(normalRoughness));

    optix::Matrix3x3 TBN;
    TBN.setCol(0,tangent);
    TBN.setCol(1,bitangent);
    TBN.setCol(2,N);

    N = normalize(TBN * (normal * 2.0f - 1.0f));
#endif

    const float3 F0 = lerp(make_float3(0.04f), albedo, albedoMetallic.w);

    float3 Lo = make_float3(0.0f);
    unsigned int num_lights = 0;//lights.size();
    for (int i = 0; i < num_lights; ++i)
    {
        // per-light radiance
        //const BasicLight& light = lights[i];
        BasicLight light = lights[i];
        light.pos = make_float3(0.5f, 1.0f, 1.5f); // Thisisshit !!
        const float3 L = normalize(light.pos - WorldPos);
        const float3 H = normalize(V + L);
        const float attenuation = calculateAttenuation(WorldPos, light.pos);
        const float3 radiance = light.color * attenuation * 20.0f; // 20.0f is shit !!!

        // cook-torrance brdf
        const float NDF = distributionGGX(N, H, normalRoughness.w);
        const float G = geometrySmith(N, V, L, normalRoughness.w);
        const float3 F = fresnelSchlick(max(dot(H, V), 0.0f), F0);

        const float3 kD = (make_float3(1.0f) - F) * (1.0f - albedoMetallic.w);

        const float3 numerator = NDF * G * F;
        const float NdotL = max(dot(N, L), 0.0f);
        const float denominator = 4.0f * max(dot(N, V), 0.0f) * NdotL;
        const float3 specular = numerator / max(denominator, 0.001f);

        Lo += (kD * albedo / M_PIf + specular) * radiance * NdotL;
    }

    float3 ambient = make_float3(0.03f) * albedo /* * ao*/;
    if (use_envmap)
    {
        float NdV = clamp(fabs(dot(N, V)), 0.001f, 1.0f);
        float3 reflection = normalize(reflect(-V, N));

        float3 f0 = make_float3(0.04);
        float3 diffuseColor = albedo * (make_float3(1.f) - f0) * (1.0 - albedoMetallic.w);
        float3 specularColor = lerp(f0, albedo, albedoMetallic.w);

        ambient = getIBLContribution(NdV, normalRoughness.w, N, reflection, diffuseColor, specularColor);
//        const float2 irradianceUV = getEquirectangularUV(N);
//        const float2 radianceUV = getEquirectangularUV(reflect(-V, N));

//        const float3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, normalRoughness.w);
//        const float3 kD = (make_float3(1.0f) - F) * (1.0f - albedoMetallic.w);

//        float3 irradiance = make_float3(rtTex2D<float4>(envmap_irradiance, irradianceUV.x, irradianceUV.y));
//        //irradiance = pow(irradiance, 1.f/2.2f);
//        const float3 diffuse = irradiance * albedo;

//        // sample both the pre-filter map and the BRDF lut and combine them together as per the Split-Sum approximation to get the IBL specular part.
//        float3 prefilteredColor = make_float3(rtTex2DLod<float4>(envmap_radiance, radianceUV.x, radianceUV.y, normalRoughness.w * float(radianceLODs)));
//        //prefilteredColor = pow(prefilteredColor, 1.f/2.2f);
//        const float2 brdf = make_float2(rtTex2D<float4>(envmap_brdf_lut, max(dot(N, V), 0.0), normalRoughness.w));
//        const float3 specular = prefilteredColor * (F * brdf.x + brdf.y);

//        ambient = (kD * diffuse + specular)/* * ao*/;
    }

    const float3 color = ambient + Lo;
    prd.result = linearToSRGB(color / (color + make_float3(1.0f)));;
}

RT_PROGRAM void any_hit_shadow()
{
    rtTerminateRay();
}

RT_PROGRAM void closest_hit_radiance()
{
    shade();
}
