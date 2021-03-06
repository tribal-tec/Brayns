// ======================================================================== //
// Copyright 2009-2016 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "PerspectiveCamera.h"
#include <limits>
// ispc-side stuff
#include "PerspectiveCamera_ispc.h"

#include <ospray/SDK/common/Data.h>

#ifdef _WIN32
#define _USE_MATH_DEFINES
#include <math.h> // M_PI
#endif

namespace ospray
{
PerspectiveCamera::PerspectiveCamera()
{
    ispcEquivalent = ispc::PerspectiveCamera_create(this);
}

void PerspectiveCamera::commit()
{
    Camera::commit();

    // ------------------------------------------------------------------
    // first, "parse" the additional expected parameters
    // ------------------------------------------------------------------
    fovy = getParamf("fovy", 60.f);
    aspect = getParamf("aspect", 1.f);
    apertureRadius = getParamf("apertureRadius", 0.f);
    focusDistance = getParamf("focusDistance", 1.f);
    // FIXME(jonask): When supported by OSPRay use bool
    stereo = getParam("stereo", 0);
    // the default 63.5mm represents the average human IPD
    interpupillaryDistance = getParamf("interpupillaryDistance", 0.0635f);
    enableClippingPlanes = getParam("enableClippingPlanes", 0);
    clipPlanes =
        enableClippingPlanes ? getParamData("clipPlanes", nullptr) : nullptr;

    // ------------------------------------------------------------------
    // now, update the local precomputed values
    // ------------------------------------------------------------------
    dir = normalize(dir);
    vec3f dir_du = normalize(cross(dir, up));
    vec3f dir_dv =
        cross(dir_du, dir); // rotate film to be perpendicular to 'dir'

    vec3f org = pos;
    const vec3f ipd_offset = 0.5f * interpupillaryDistance * dir_du;

    if (stereo)
    {
        auto bufferTarget = getParamString("buffer_target");
        if (bufferTarget.length() == 2)
        {
            if (bufferTarget.at(1) == 'L')
                org -= ipd_offset;
            if (bufferTarget.at(1) == 'R')
                org += ipd_offset;
        }
    }

    float imgPlane_size_y = 2.f * tanf(deg2rad(0.5f * fovy));
    float imgPlane_size_x = imgPlane_size_y * aspect;

    dir_du *= imgPlane_size_x;
    dir_dv *= imgPlane_size_y;

    vec3f dir_00 = dir - 0.5f * dir_du - 0.5f * dir_dv;

    float scaledAperture = 0.f;
    // prescale to focal plane
    if (apertureRadius > 0.f)
    {
        dir_du *= focusDistance;
        dir_dv *= focusDistance;
        dir_00 *= focusDistance;
        scaledAperture = apertureRadius / imgPlane_size_x;
    }

    const auto clipPlaneData = clipPlanes ? clipPlanes->data : nullptr;
    const size_t numClipPlanes = clipPlanes ? clipPlanes->numItems : 0;

    ispc::PerspectiveCamera_set(getIE(), (const ispc::vec3f&)org,
                                (const ispc::vec3f&)dir_00,
                                (const ispc::vec3f&)dir_du,
                                (const ispc::vec3f&)dir_dv, scaledAperture,
                                aspect, (const ispc::vec3f&)ipd_offset,
                                (const ispc::vec4f*)clipPlaneData,
                                numClipPlanes);
}

OSP_REGISTER_CAMERA(PerspectiveCamera, perspective);

} // namespace ospray
