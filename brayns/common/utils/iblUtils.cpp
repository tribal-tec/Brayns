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

#include "iblUtils.h"

#include "imageUtils.h"
#include <boost/progress.hpp>
#include <brayns/common/Timer.h>
#include <brayns/common/log.h>
#include <brayns/common/material/Texture2D.h>

//#define ALT_GGX

namespace brayns
{
namespace
{
Vector3f polarToCartesian(const Vector2f& uv)
{
    const float theta = (uv.x - 0.5f) * (M_PI * 2.f);
    const float phi = uv.y * M_PI;
    const float sinPhi = std::sin(phi);
    const Vector3f n(sinPhi * std::cos(theta), -std::cos(phi),
                     sinPhi * std::sin(theta));
    return glm::normalize(n);
}

// static inline float fast_atan2(float y, float x){
//    static const float c1 = M_PI / 4.0;
//    static const float c2 = M_PI * 3.0 / 4.0;
//    if (y == 0 && x == 0)
//        return 0;
//    float abs_y = fabsf(y);
//    float angle;
//    if (x >= 0)
//        angle = c1 - c1 * ((x - abs_y) / (x + abs_y));
//    else
//        angle = c2 - c1 * ((x + abs_y) / (abs_y - x));
//    if (y < 0)
//        return -angle;
//    return angle;
//}

const float RECIPROCAL_PI2 = 0.15915494f;

Vector2f cartesianToPolar(const Vector3f& n)
{
    Vector2f uv;
    uv.x = std::atan2(n.z, n.x) * RECIPROCAL_PI2 + 0.5f;
    uv.y = std::asin(n.y) * M_1_PI + 0.5f;
    return uv;
}

inline Vector3f bilerp(const Vector2f& frac, const Vector3f& c00,
                       const Vector3f& c01, const Vector3f& c10,
                       const Vector3f& c11)
{
    return glm::lerp(glm::lerp(c00, c01, frac.x), glm::lerp(c10, c11, frac.x),
                     frac.y);
}

struct BilinCoords
{
    Vector2i st0;
    Vector2i st1;
    Vector2f frac;
};

inline Vector2f frac(const Vector2f& x)
{
    return {x.x - std::floor(x.x), x.y - std::floor(x.y)};
}

inline BilinCoords bilinear_coords(const Vector2i& size, const Vector2f& p)
{
    BilinCoords coords;

    // repeat: get remainder within [0..1] parameter space
    // lower sample shifted by half a texel
    const Vector2f halfTexel(0.5f / size.x, 0.5f / size.y);
    Vector2f tc = frac(p - halfTexel);
    tc = max(tc, Vector2f(0.0f)); // filter out inf/NaN

    // scale by texture size
    const Vector2f sizef(nextafter((float)size.x, -1.0f),
                         nextafter((float)size.y, -1.0f));
    tc = tc * sizef;
    coords.frac = frac(tc);

    coords.st0 = Vector2f(tc);
    coords.st1 = coords.st0 + 1;
    // handle border cases
    if (coords.st1.x >= size.x)
        coords.st1.x = 0;
    if (coords.st1.y >= size.y)
        coords.st1.y = 0;

    return coords;
}

Vector3f getTexel(const Texture2D& tex, const Vector2i& uv)
{
    const auto index = (uv.x + uv.y * tex.getWidth()) * tex.getNbChannels();
    const auto ptr = &tex.getRawData()[index];
    return {*ptr / 255.f, *(ptr + 1) / 255.f, *(ptr + 2) / 255.f};
}

Vector3f tex2D(const Texture2D& tex, const Vector2f& uv)
{
    BilinCoords cs = bilinear_coords({tex.getWidth(), tex.getHeight()}, uv);

#if 1
    const Vector3f c00 = getTexel(tex, cs.st0);
    const Vector3f c01 = getTexel(tex, {cs.st1.x, cs.st0.y});
    const Vector3f c10 = getTexel(tex, {cs.st0.x, cs.st1.y});
    const Vector3f c11 = getTexel(tex, cs.st1);

    return bilerp(cs.frac, c00, c01, c10, c11);
#else
    return getTexel(tex, cs.st0);
#endif
}

#ifdef ALT_GGX
glm::mat3 matrixFromVector(const Vector3f& n)
{
    const float a = 1.0f / (1.0f + n.z);
    const float b = -n.x * n.y * a;
    const Vector3f b1(1.0f - n.x * n.x * a, b, -n.x);
    const Vector3f b2(b, 1.0f - n.y * n.y * a, -n.y);
    return glm::mat3(b1, b2, n);
}
#endif

#if 0
float mymod(float x, float y)
{
    return x - y * floor(x/y);
}

float VanDerCorpus(uint n, uint base) {
    float invBase = 1.0f / float(base);
    float denom = 1.0f;
    float result = 0.0;
    for(int i = 0; i < 32; ++i) {
        if(n > 0) {
            denom = mymod(float(n), 2.0f);
            result += denom * invBase;
            invBase = invBase / 2.0f;
            n = int(float(n) / 2.0f);
        }
    }
    return result;
}

Vector2f Hammersley(uint i, uint N)
{
    return Vector2f(float(i) / float(N), VanDerCorpus(i, 2));
}
#else
float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

Vector2f Hammersley(const uint i, const uint N)
{
    return {float(i) / float(N), RadicalInverse_VdC(i)};
}
#endif

#ifdef ALT_GGX
Vector3f importanceSampleGGX(const Vector2f& uv, const glm::mat3& mNormal,
                             const float roughness)
{
    const float a = roughness * roughness;
    const float phi = 2.0f * M_PI * uv.x;
    const float cosTheta =
        std::sqrt((1.0f - uv.y) / (1.0f + (a * a - 1.0f) * uv.y));
    const float sinTheta = std::sqrt(1.0f - cosTheta * cosTheta);
    const Vector3f sampleVec =
        mNormal *
        Vector3f(sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta);
    return glm::normalize(sampleVec);
}
#else
Vector3f importanceSampleGGX(const Vector2f& uv, const Vector3f& N,
                             const float roughness)
{
    const float a = roughness * roughness;
    const float phi = 2.0f * M_PI * uv.x;
    const float cosTheta =
        std::sqrt((1.0f - uv.y) / (1.0f + (a * a - 1.0f) * uv.y));
    const float sinTheta = std::sqrt(1.0f - cosTheta * cosTheta);

    const Vector3f H(std::cos(phi) * sinTheta, std::sin(phi) * sinTheta,
                     cosTheta);

    // from tangent-space vector to world-space sample vector
    const Vector3f up = std::abs(N.z) < 0.999f ? Vector3f(0.0f, 0.0f, 1.0f)
                                               : Vector3f(1.0f, 0.0f, 0.0f);
    const Vector3f tangent = normalize(cross(up, N));
    const Vector3f bitangent = cross(N, tangent);

    const Vector3f sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return glm::normalize(sampleVec);
}
#endif
void saveToFile(const std::vector<Vector3f>& texture, const size_t width,
                const size_t height, const std::string& outfile)
{
    auto img = FreeImage_Allocate(width, height, 24);
    RGBQUAD color;
    for (size_t y = 0; y < height; y++)
    {
        for (size_t x = 0; x < width; x++)
        {
            const auto& val = texture[x + y * width];
            color.rgbRed = val.x * 255;
            color.rgbGreen = val.y * 255;
            color.rgbBlue = val.z * 255;
            FreeImage_SetPixelColor(img, x, y, &color);
        }
    }
    FreeImage_FlipVertical(img);
    FreeImage_Save(FIF_PNG, img, outfile.c_str());
    FreeImage_Unload(img);
}

} // anonymous namespace

namespace iblUtils
{
void computeIrradianceMap(const Texture2D& tex)
{
    const size_t width = 32;
    const size_t height = 32;
    boost::progress_display progress(height);
    std::vector<Vector3f> outtexture(width * height);
    Timer timer;
    timer.start();
    for (size_t y = 0; y < height; ++y)
    {
#pragma omp parallel for
        for (size_t x = 0; x < width; ++x)
        {
            const Vector2f uv(float(x) / width, float(y) / height);
            Vector3f irradiance;

            const Vector3f normal = polarToCartesian(uv);
#if 1
            Vector3f up(0.f, 1.f, 0.f);
            const Vector3f right = glm::cross(up, normal);
            up = glm::cross(normal, right);
            const float delta = 0.025f;
            size_t samples = 0;
            for (float phi = 0.0f; phi < M_PI * 2.f; phi += delta)
            {
                const float sinPhi = std::sin(phi);
                const float cosPhi = std::cos(phi);
                for (float theta = 0.0f; theta < M_PI_2; theta += delta)
                {
                    const float sinTheta = std::sin(theta);
                    const float cosTheta = std::cos(theta);
                    const Vector3f tangent(sinTheta * cosPhi, sinTheta * sinPhi,
                                           cosTheta);
                    const Vector3f dir =
                        tangent.x * right + tangent.y * up + tangent.z * normal;
                    const Vector3f sample = tex2D(tex, cartesianToPolar(dir));
                    irradiance += sample * cosTheta * sinTheta;
                    samples++;
                }
            }
            irradiance = M_PI * irradiance / (float)samples;
#else
            irradiance = tex2D(*tex, cartesianToPolar(normal));
#endif
            outtexture[y * width + x] = irradiance;
        }
        ++progress;
    }
    BRAYNS_INFO << "Irradiance map computed in " << timer.elapsed()
                << " seconds" << std::endl;
    saveToFile(outtexture, width, height, "/tmp/irradiance.png");
}

void computeRadianceMap(const Texture2D& tex)
{
    size_t fraction = 1;
    size_t width = tex.getWidth() / fraction;
    size_t height = tex.getHeight() / fraction;

    boost::progress_display progress(height);
    std::vector<Vector3f> outtexture(width * height);
    const size_t SAMPLES = 1000;
    const float uRoughness = .5f;
    Timer timer;
    timer.start();
    for (size_t y = 0; y < height; ++y)
    {
#pragma omp parallel for
        for (size_t x = 0; x < width; ++x)
        {
            const Vector2f uv(float(x) / width, float(y) / height);
            Vector3f color;

            const Vector3f N = polarToCartesian(uv);
#ifdef ALT_GGX
            const glm::mat3 mNormal = matrixFromVector(normal);
#endif

            float totalWeight = 0.0f;
            for (size_t i = 0; i < SAMPLES; i++)
            {
                const Vector2f r = Hammersley(i, SAMPLES);
#ifdef ALT_GGX
                const Vector3f dir =
                    importanceSampleGGX(r, mNormal, uRoughness);
#else
                const Vector3f dir = importanceSampleGGX(r, N, uRoughness);
#endif

                const Vector3f L =
                    glm::normalize(2.0f * glm::dot(N, dir) * dir - N);
                const float NdotL = std::max(glm::dot(N, L), 0.0f);
                if (NdotL > 0.0f)
                {
                    color += tex2D(tex, cartesianToPolar(dir)) * NdotL;
                    totalWeight += NdotL;
                }
            }
            outtexture[y * width + x] = color / totalWeight;
        }
        ++progress;
    }
    BRAYNS_INFO << "Radiance map computed in " << timer.elapsed() << " seconds"
                << std::endl;
    saveToFile(outtexture, width, height, "/tmp/radiance.png");
}
}
} // namespace brayns
