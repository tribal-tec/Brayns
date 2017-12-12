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
}
