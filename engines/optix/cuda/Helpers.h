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

#pragma once

#include <optixu/optixu_math_namespace.h>

// Convert a float3 in [0,1)^3 to a uchar4 in [0,255]^4 -- 4th channel is set to
// 255
#ifdef __CUDACC__
static __device__ __inline__ optix::uchar4 make_color(const optix::float3& c)
{
    return optix::make_uchar4(
        static_cast<unsigned char>(__saturatef(c.x) * 255.99f), /* R */
        static_cast<unsigned char>(__saturatef(c.y) * 255.99f), /* G */
        static_cast<unsigned char>(__saturatef(c.z) * 255.99f), /* B */
        255u);                                                  /* A */
}
#endif

// Sample Phong lobe relative to U, V, W frame
static __host__ __device__ __inline__ optix::float3 sample_phong_lobe(
    optix::float2 sample, float exponent, optix::float3 U, optix::float3 V,
    optix::float3 W)
{
    const float power = expf(logf(sample.y) / (exponent + 1.0f));
    const float phi = sample.x * 2.0f * (float)M_PIf;
    const float scale = sqrtf(1.0f - power * power);

    const float x = cosf(phi) * scale;
    const float y = sinf(phi) * scale;
    const float z = power;

    return x * U + y * V + z * W;
}

// Sample Phong lobe relative to U, V, W frame
static __host__ __device__ __inline__ optix::float3 sample_phong_lobe(
    const optix::float2& sample, float exponent, const optix::float3& U,
    const optix::float3& V, const optix::float3& W, float& pdf, float& bdf_val)
{
    const float cos_theta = powf(sample.y, 1.0f / (exponent + 1.0f));

    const float phi = sample.x * 2.0f * M_PIf;
    const float sin_theta = sqrtf(1.0f - cos_theta * cos_theta);

    const float x = cosf(phi) * sin_theta;
    const float y = sinf(phi) * sin_theta;
    const float z = cos_theta;

    const float powered_cos = powf(cos_theta, exponent);
    pdf = (exponent + 1.0f) / (2.0f * M_PIf) * powered_cos;
    bdf_val = (exponent + 2.0f) / (2.0f * M_PIf) * powered_cos;

    return x * U + y * V + z * W;
}

// Get Phong lobe PDF for local frame
static __host__ __device__ __inline__ float get_phong_lobe_pdf(
    float exponent, const optix::float3& normal, const optix::float3& dir_out,
    const optix::float3& dir_in, float& bdf_val)
{
    using namespace optix;

    float3 r = -reflect(dir_out, normal);
    const float cos_theta = fabs(dot(r, dir_in));
    const float powered_cos = powf(cos_theta, exponent);

    bdf_val = (exponent + 2.0f) / (2.0f * M_PIf) * powered_cos;
    return (exponent + 1.0f) / (2.0f * M_PIf) * powered_cos;
}

// Create ONB from normal.  Resulting W is parallel to normal
static __host__ __device__ __inline__ void create_onb(const optix::float3& n,
                                                      optix::float3& U,
                                                      optix::float3& V,
                                                      optix::float3& W)
{
    using namespace optix;

    W = normalize(n);
    U = cross(W, optix::make_float3(0.0f, 1.0f, 0.0f));

    if (fabs(U.x) < 0.001f && fabs(U.y) < 0.001f && fabs(U.z) < 0.001f)
        U = cross(W, make_float3(1.0f, 0.0f, 0.0f));

    U = normalize(U);
    V = cross(W, U);
}

// Create ONB from normalized vector
static __device__ __inline__ void create_onb(const optix::float3& n,
                                             optix::float3& U, optix::float3& V)
{
    using namespace optix;

    U = cross(n, make_float3(0.0f, 1.0f, 0.0f));

    if (dot(U, U) < 1e-3f)
        U = cross(n, make_float3(1.0f, 0.0f, 0.0f));

    U = normalize(U);
    V = cross(n, U);
}

// Compute the origin ray differential for transfer
static __host__ __device__ __inline__ optix::float3
    differential_transfer_origin(optix::float3 dPdx, optix::float3 dDdx,
                                 float t, optix::float3 direction,
                                 optix::float3 normal)
{
    float dtdx =
        -optix::dot((dPdx + t * dDdx), normal) / optix::dot(direction, normal);
    return (dPdx + t * dDdx) + dtdx * direction;
}

// Compute the direction ray differential for a pinhole camera
static __host__ __device__ __inline__ optix::float3
    differential_generation_direction(optix::float3 d, optix::float3 basis)
{
    float dd = optix::dot(d, d);
    return (dd * basis - optix::dot(d, basis) * d) / (dd * sqrtf(dd));
}

// Compute the direction ray differential for reflection
static __host__ __device__ __inline__ optix::float3
    differential_reflect_direction(optix::float3 dPdx, optix::float3 dDdx,
                                   optix::float3 dNdP, optix::float3 D,
                                   optix::float3 N)
{
    using namespace optix;

    float3 dNdx = dNdP * dPdx;
    float dDNdx = dot(dDdx, N) + dot(D, dNdx);
    return dDdx - 2 * (dot(D, N) * dNdx + dDNdx * N);
}

// Compute the direction ray differential for refraction
static __host__ __device__ __inline__ optix::float3
    differential_refract_direction(optix::float3 dPdx, optix::float3 dDdx,
                                   optix::float3 dNdP, optix::float3 D,
                                   optix::float3 N, float ior, optix::float3 T)
{
    using namespace optix;

    float eta;
    if (dot(D, N) > 0.f)
    {
        eta = ior;
        N = -N;
    }
    else
    {
        eta = 1.f / ior;
    }

    float3 dNdx = dNdP * dPdx;
    float mu = eta * dot(D, N) - dot(T, N);
    float TN = -sqrtf(1 - eta * eta * (1 - dot(D, N) * dot(D, N)));
    float dDNdx = dot(dDdx, N) + dot(D, dNdx);
    float dmudx = (eta - (eta * eta * dot(D, N)) / TN) * dDNdx;
    return eta * dDdx - (mu * dNdx + dmudx * N);
}

// Color space conversions
static __host__ __device__ __inline__ optix::float3 Yxy2XYZ(
    const optix::float3& Yxy)
{
    // avoid division by zero
    if (Yxy.z < 1e-4)
        return optix::make_float3(0.0f, 0.0f, 0.0f);

    return optix::make_float3(Yxy.y * (Yxy.x / Yxy.z), Yxy.x,
                              (1.0f - Yxy.y - Yxy.z) * (Yxy.x / Yxy.z));
}

static __host__ __device__ __inline__ optix::float3 XYZ2rgb(
    const optix::float3& xyz)
{
    const float R =
        optix::dot(xyz, optix::make_float3(3.2410f, -1.5374f, -0.4986f));
    const float G =
        optix::dot(xyz, optix::make_float3(-0.9692f, 1.8760f, 0.0416f));
    const float B =
        optix::dot(xyz, optix::make_float3(0.0556f, -0.2040f, 1.0570f));
    return optix::make_float3(R, G, B);
}

static __host__ __device__ __inline__ optix::float3 Yxy2rgb(optix::float3 Yxy)
{
    using namespace optix;

    // avoid division by zero
    if (Yxy.z < 1e-4)
        return make_float3(0.0f, 0.0f, 0.0f);

    // First convert to xyz
    float3 xyz = make_float3(Yxy.y * (Yxy.x / Yxy.z), Yxy.x,
                             (1.0f - Yxy.y - Yxy.z) * (Yxy.x / Yxy.z));

    const float R = dot(xyz, make_float3(3.2410f, -1.5374f, -0.4986f));
    const float G = dot(xyz, make_float3(-0.9692f, 1.8760f, 0.0416f));
    const float B = dot(xyz, make_float3(0.0556f, -0.2040f, 1.0570f));
    return make_float3(R, G, B);
}

static __host__ __device__ __inline__ optix::float3 rgb2Yxy(optix::float3 rgb)
{
    using namespace optix;

    // convert to xyz
    const float X = dot(rgb, make_float3(0.4124f, 0.3576f, 0.1805f));
    const float Y = dot(rgb, make_float3(0.2126f, 0.7152f, 0.0722f));
    const float Z = dot(rgb, make_float3(0.0193f, 0.1192f, 0.9505f));

    // avoid division by zero
    // here we make the simplifying assumption that X, Y, Z are positive
    float denominator = X + Y + Z;
    if (denominator < 1e-4)
        return make_float3(0.0f, 0.0f, 0.0f);

    // convert xyz to Yxy
    return make_float3(Y, X / (denominator), Y / (denominator));
}

static __host__ __device__ __inline__ optix::float3 tonemap(
    const optix::float3& hdr_value, float Y_log_av, float Y_max)
{
    using namespace optix;

    float3 val_Yxy = rgb2Yxy(hdr_value);

    float Y = val_Yxy.x; // Y channel is luminance
    const float a = 0.04f;
    float Y_rel = a * Y / Y_log_av;
    float mapped_Y = Y_rel * (1.0f + Y_rel / (Y_max * Y_max)) / (1.0f + Y_rel);

    float3 mapped_Yxy = make_float3(mapped_Y, val_Yxy.y, val_Yxy.z);
    float3 mapped_rgb = Yxy2rgb(mapped_Yxy);

    return mapped_rgb;
}

static __device__ inline float3 tonemap2(float3 ldrColor, const float gamma,
                                         const float whitePoint,
                                         const float burnHighlights,
                                         float crushBlacks,
                                         const float saturation,
                                         const float brightness)
{
    const float invGamma = 1.0f / gamma;
    const float invWhitePoint = brightness / whitePoint;
    crushBlacks = crushBlacks + crushBlacks + 1.0f;

    ldrColor = invWhitePoint * ldrColor;
    ldrColor *= (ldrColor * optix::make_float3(burnHighlights) +
                 optix::make_float3(1.0f)) /
                (ldrColor + optix::make_float3(1.0f));

    float luminance =
        optix::dot(ldrColor, optix::make_float3(0.3f, 0.59f, 0.11f));
    ldrColor = optix::lerp(optix::make_float3(luminance), ldrColor,
                           saturation); // This can generate negative values for
                                        // saturation > 1.0f!
    ldrColor = optix::fmaxf(optix::make_float3(0.0f),
                            ldrColor); // Prevent negative values.

    luminance = optix::dot(ldrColor, make_float3(0.3f, 0.59f, 0.11f));
    if (luminance < 1.0f)
    {
        const float3 crushed =
            optix::make_float3(powf(ldrColor.x, crushBlacks),
                               powf(ldrColor.y, crushBlacks),
                               powf(ldrColor.z, crushBlacks));
        ldrColor = optix::lerp(crushed, ldrColor, sqrtf(luminance));
        ldrColor = optix::fmaxf(optix::make_float3(0.0f),
                                ldrColor); // Prevent negative values.
    }
    ldrColor = optix::make_float3(powf(ldrColor.x, invGamma),
                                  powf(ldrColor.y, invGamma),
                                  powf(ldrColor.z, invGamma));
    return ldrColor;
}

// static const Matrix3x3 acesInputMat = {
//  {0.5972782409, 0.0760130499, 0.0284085382},
//  {0.3545713181, 0.9083220973, 0.1338243154},
//  {0.0482176639, 0.0156579968, 0.8375684636}
//};

//// ACES output transform matrix = XYZ_2_REC709_PRI_MAT * D60_2_D65_CAT *
///AP1_2_XYZ_MAT * ODT_SAT_MAT
// static const uniform LinearSpace3f acesOutputMat = {
//  { 1.6047539945, -0.1020831870, -0.0032670420},
//  {-0.5310794927,  1.1081322801, -0.0727552477},
//  {-0.0736720338, -0.0060518756,  1.0760219533}
//};

static __device__ inline float3 hableTonemap(float3 x)
{
    float hA = 0.15;
    float hB = 0.50;
    float hC = 0.10;
    float hD = 0.20;
    float hE = 0.02;
    float hF = 0.30;

    return ((x * (hA * x + hC * hB) + hD * hE) /
            (x * (hA * x + hB) + hD * hF)) -
           hE / hF;
}

static __host__ __device__ __inline__ optix::float3 linear2srgb(
    const optix::float3& x)
{
    return make_float3(pow(x.x, 1.f / 2.2f), pow(x.y, 1.f / 2.2f),
                       pow(x.z, 1.f / 2.2f));
}

static __device__ inline float3 aces_tonemap(float3 color, float exposure,
                                             float a, float b, float c, float d,
                                             uint acesColor)
{
    float3 x = color * exposure;
    if (acesColor != 0)
        x = x.x * make_float3(0.5972782409, 0.0760130499, 0.0284085382) +
            x.y * make_float3(0.3545713181, 0.9083220973, 0.1338243154) +
            x.z * make_float3(0.0482176639, 0.0156579968, 0.8375684636);

    //    x = hableTonemap(x);
    //    float3 hW = make_float3(11.2f);
    //    float3 whiteScale = make_float3(1.0f) / hableTonemap(hW);
    //    x = x * whiteScale;

    x.x = pow(x.x, a) / (pow(x.x, a * d) * b + c);
    x.y = pow(x.y, a) / (pow(x.y, a * d) * b + c);
    x.z = pow(x.z, a) / (pow(x.z, a * d) * b + c);

    // float3 aa = x * (x + 0.0245786f) - 0.000090537f;
    // float3 bb = x * (0.983729f * x + 0.4329510f) + 0.238081f;
    // x = aa / bb;

    //    float aa = 2.51f;
    //    float bb = 0.03f;
    //    float cc = 2.43f;
    //    float dd = 0.59f;
    //    float ee = 0.14f;
    //    x = ((x*(aa*x+bb))/(x*(cc*x+dd)+ee));

    if (acesColor != 0)
        x = x.x * make_float3(1.6047539945, -0.1020831870, -0.0032670420) +
            x.y * make_float3(-0.5310794927, 1.1081322801, -0.0727552477) +
            x.z * make_float3(-0.0736720338, -0.0060518756, 1.0760219533);
    x = optix::clamp(linear2srgb(x), make_float3(0.f), make_float3(1.f));
    return x;
}

#define OPTIX_DUMP_FLOAT(VALUE) rtPrintf(#VALUE " %f\n", VALUE)
#define OPTIX_DUMP_INT(VALUE) rtPrintf(#VALUE " %i\n", VALUE)
