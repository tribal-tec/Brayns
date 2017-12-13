/* Copyright (c) 2015-2017, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
 * Responsible Author: Cyrille Favreau <cyrille.favreau@epfl.ch>
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

#include <brayns/common/camera/Camera.h>
#include <brayns/common/engine/Engine.h>
#include <brayns/common/renderer/FrameBuffer.h>
#include <brayns/common/scene/Scene.h>
#include <brayns/common/transferFunction/TransferFunction.h>

#include "base64/base64.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif
#include "staticjson/staticjson.hpp"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

STATICJSON_DECLARE_ENUM(brayns::CameraStereoMode,
                        {"none", brayns::CameraStereoMode::none},
                        {"left", brayns::CameraStereoMode::left},
                        {"right", brayns::CameraStereoMode::right},
                        {"side_by_side",
                         brayns::CameraStereoMode::side_by_side});

namespace staticjson
{
void init(brayns::Camera* c, ObjectHandler* h)
{
    // thx for the hack: https://stackoverflow.com/questions/11205186
    h->add_property("origin", reinterpret_cast<std::array<float, 3>*>(
                                  &c->_position.array[0]),
                    Flags::Optional);
    h->add_property("look_at", reinterpret_cast<std::array<float, 3>*>(
                                   &c->_target.array[0]),
                    Flags::Optional);
    h->add_property("up",
                    reinterpret_cast<std::array<float, 3>*>(&c->_up.array[0]),
                    Flags::Optional);
    h->add_property("field_of_view", &c->_fieldOfView, Flags::Optional);
    h->add_property("aperture", &c->_aperture, Flags::Optional);
    h->add_property("focal_length", &c->_focalLength, Flags::Optional);
    h->add_property("stereo_mode", &c->_stereoMode, Flags::Optional);
    h->add_property("eye_separation", &c->_eyeSeparation, Flags::Optional);
    h->set_flags(Flags::DisallowUnknownKey);
}

void init(brayns::Engine::Progress* p, ObjectHandler* h)
{
    h->add_property("amount", &p->amount);
    h->add_property("operation", &p->operation);
    h->set_flags(Flags::DisallowUnknownKey);
}

void init(brayns::FrameBuffer* f, ObjectHandler* h)
{
    static brayns::Vector2ui frameSize;
    static std::string diffuse, depth;

    frameSize = f->getSize();
    diffuse = base64_encode(f->getColorBuffer(),
                            frameSize.x() * frameSize.y() * f->getColorDepth());

    if (f->getDepthBuffer())
    {
        depth =
            base64_encode(reinterpret_cast<const uint8_t*>(f->getDepthBuffer()),
                          frameSize.x() * frameSize.y() * sizeof(float));
    }

    h->add_property("width", &frameSize[0]);
    h->add_property("height", &frameSize[1]);
    h->add_property("diffuse", &diffuse);
    h->add_property("depth", &depth, Flags::Optional);
    h->set_flags(Flags::DisallowUnknownKey);
}

void init(brayns::TransferFunction* t, ObjectHandler* h)
{
    h->add_property("range", reinterpret_cast<std::array<float, 2>*>(
                                 &t->getValuesRange()[0]),
                    Flags::Optional);
    h->add_property("diffuse",
                    reinterpret_cast<std::vector<std::array<float, 4>>*>(
                        &t->getDiffuseColors()),
                    Flags::Optional);
    h->add_property("emission",
                    reinterpret_cast<std::vector<std::array<float, 3>>*>(
                        &t->getEmissionIntensities()),
                    Flags::Optional);
    h->add_property("contribution", &t->getContributions(), Flags::Optional);
    h->set_flags(Flags::DisallowUnknownKey);
}

void init(brayns::Boxf* b, ObjectHandler* h)
{
    static brayns::Vector3f bMin, bMax;
    bMin = b->getMin();
    bMax = b->getMax();
    h->add_property("min", reinterpret_cast<std::array<float, 3>*>(&bMin[0]));
    h->add_property("max", reinterpret_cast<std::array<float, 3>*>(&bMax[0]));
    h->set_flags(Flags::DisallowUnknownKey);
}

void init(brayns::Material* m, ObjectHandler* h)
{
    h->add_property("diffuse_color",
                    reinterpret_cast<std::array<float, 3>*>(&m->_color[0]),
                    Flags::Optional);
    h->add_property("specular_color", reinterpret_cast<std::array<float, 3>*>(
                                          &m->_specularColor[0]),
                    Flags::Optional);
    h->add_property("specular_exponent", &m->_specularExponent,
                    Flags::Optional);
    h->add_property("reflection_index", &m->_reflectionIndex, Flags::Optional);
    h->add_property("opacity", &m->_opacity, Flags::Optional);
    h->add_property("refraction_index", &m->_refractionIndex, Flags::Optional);
    h->add_property("light_emission", &m->_emission, Flags::Optional);
    h->add_property("glossiness", &m->_glossiness, Flags::Optional);
    h->add_property("cast_simulation_data", &m->_castSimulationData,
                    Flags::Optional);
    h->set_flags(Flags::DisallowUnknownKey);
}

void init(brayns::Scene* s, ObjectHandler* h)
{
    // FIXME: expose materials as vector directly from scene
    static std::vector<brayns::Material> materials;
    materials.clear();
    materials.reserve(s->getMaterials().size());
    for (size_t materialId = brayns::NB_SYSTEM_MATERIALS;
         materialId < s->getMaterials().size(); ++materialId)
    {
        materials.push_back(s->getMaterial(materialId));
    }
    h->add_property("bounds", &s->getWorldBounds(),
                    Flags::IgnoreWrite | Flags::Optional);
    h->add_property("materials", &materials);
    h->set_flags(Flags::DisallowUnknownKey);
}
}
