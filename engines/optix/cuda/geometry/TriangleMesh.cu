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

#include "IntersectionRefinement.h"
#include <optix.h>
#include <optixu/optixu_aabb_namespace.h>
#include <optixu/optixu_math_namespace.h>
#include <optixu/optixu_matrix_namespace.h>
#include "../../CommonStructs.h"

using namespace optix;

// This is to be plugged into an RTgeometry object to represent
// a triangle mesh with a vertex buffer of triangle soup (triangle list)
// with an interleaved position, normal, texturecoordinate layout.

rtBuffer<float3> vertices_buffer;
rtBuffer<float3> normal_buffer;
rtBuffer<float2> texcoord_buffer;
rtBuffer<int3> indices_buffer;

rtDeclareVariable(float2, texcoord, attribute texcoord, );
rtDeclareVariable(float3, v0, attribute v0, );
rtDeclareVariable(float3, v1, attribute v1, );
rtDeclareVariable(float3, v2, attribute v2, );
rtDeclareVariable(float2, t0, attribute t0, );
rtDeclareVariable(float2, t1, attribute t1, );
rtDeclareVariable(float2, t2, attribute t2, );
rtDeclareVariable(float2, ddx, attribute ddx, );
rtDeclareVariable(float2, ddy, attribute ddy, );
rtDeclareVariable(float3, geometric_normal, attribute geometric_normal, );
rtDeclareVariable(float3, shading_normal, attribute shading_normal, );

rtDeclareVariable(float3, back_hit_point, attribute back_hit_point, );
rtDeclareVariable(float3, front_hit_point, attribute front_hit_point, );

rtDeclareVariable(optix::Ray, ray, rtCurrentRay, );
rtDeclareVariable(PerRayData_radiance, prd, rtPayload, );
rtDeclareVariable(unsigned long, simulation_idx, attribute simulation_idx, );

static __device__ bool intersect_triangle_filtered(const Ray&    ray,
                                                   const float3& p0,
                                                   const float3& p1,
                                                   const float3& p2,
                                                   const float3& rayDx,
                                                   const float3& rayDy, 
                                                   float3& n,
                                                   float&  t,
                                                   float3&  beta,
                                                   float3&  gamma)
{
  const float3 e0 = p1 - p0;
  const float3 e1 = p0 - p2;
  n  = cross( e1, e0 );

  const float3 g0 = p0 - ray.origin;

  float3 e2 = ( 1.0f / dot( n, ray.direction ) ) * g0;
  float3 i  = cross( ray.direction, e2 );

  beta.x  = dot( i, e1 );
  gamma.x = dot( i, e0 );
  t     = dot( n, e2 );

  if(!((t<ray.tmax) & (t>ray.tmin) & (beta.x>=0.0f) & (gamma.x>=0.0f) & (beta.x+gamma.x<=1)))
      return false;

  float3 dirX = ray.direction + rayDx;
  e2 = ( 1.0f / dot( n, dirX ) ) * g0;
  i  = cross( dirX, e2 );
  beta.y  = dot( i, e1 );
  gamma.y = dot( i, e0 );

  float3 dirY = ray.direction + rayDy;
  e2 = ( 1.0f / dot( n, dirY ) ) * g0;
  i  = cross( dirY, e2 );
  beta.z  = dot( i, e1 );
  gamma.z = dot( i, e0 );

  return true; 
}

template <bool DO_REFINE>
static __device__ void meshIntersect(int primIdx)
{
    const int3 v_idx = indices_buffer[primIdx];

    const float3 p0 = vertices_buffer[v_idx.x];
    const float3 p1 = vertices_buffer[v_idx.y];
    const float3 p2 = vertices_buffer[v_idx.z];

    // Intersect ray with triangle
    float3 n;
    float t;
    float3 beta, gamma;
    if (intersect_triangle_filtered(ray, p0, p1, p2, prd.rayDx, prd.rayDy, n, t, beta, gamma))
    {
        if (rtPotentialIntersection(t))
        {
            v0 = p0;
            v1 = p1;
            v2 = p2;
            geometric_normal = normalize(n);
            if (normal_buffer.size() == 0)
                shading_normal = geometric_normal;
            else
            {
                float3 n0 = normal_buffer[v_idx.x];
                float3 n1 = normal_buffer[v_idx.y];
                float3 n2 = normal_buffer[v_idx.z];
                shading_normal = normalize(n1 * beta.x + n2 * gamma.x +
                                           n0 * (1.f - beta.x - gamma.x));
            }

            if (texcoord_buffer.size() == 0)
            {
                texcoord = make_float2(0.f, 0.f);
            }
            else
            {
                t0 = texcoord_buffer[v_idx.x];
                t1 = texcoord_buffer[v_idx.y];
                t2 = texcoord_buffer[v_idx.z];
                texcoord = t1 * beta.x + t2 * gamma.x +
                                       t0 * (1.f - beta.x - gamma.x);

                const float2 texcoordX = t1 * beta.y + t2 * gamma.y +
                                         t0 * (1.f - beta.y - gamma.y);
                const float2 texcoordY = t1 * beta.z + t2 * gamma.z +
                                         t0 * (1.f - beta.z - gamma.z);

                ddx = texcoordX - texcoord;
                ddy = texcoordY - texcoord;
            }

            if (DO_REFINE)
                refine_and_offset_hitpoint(ray.origin + t * ray.direction,
                                           ray.direction, geometric_normal, p0,
                                           back_hit_point, front_hit_point);
            simulation_idx = 0;
            rtReportIntersection(0);
        }
    }
}

RT_PROGRAM void intersect(int primIdx)
{
    meshIntersect<false>(primIdx);
}

RT_PROGRAM void intersect_refine(int primIdx)
{
    meshIntersect<true>(primIdx);
}

RT_PROGRAM void bounds(int primIdx, float result[6])
{
    const int3 v_idx = indices_buffer[primIdx];

    const float3 v0 = vertices_buffer[v_idx.x];
    const float3 v1 = vertices_buffer[v_idx.y];
    const float3 v2 = vertices_buffer[v_idx.z];
    const float area = length(cross(v1 - v0, v2 - v0));

    optix::Aabb* aabb = (optix::Aabb*)result;

    if (area > 0.0f && !isinf(area))
    {
        aabb->m_min = fminf(fminf(v0, v1), v2);
        aabb->m_max = fmaxf(fmaxf(v0, v1), v2);
    }
    else
    {
        aabb->invalidate();
    }
}
