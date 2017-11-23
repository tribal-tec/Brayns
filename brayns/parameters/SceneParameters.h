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

#ifndef SCENEPARAMETERS_H
#define SCENEPARAMETERS_H

#include "AbstractParameters.h"
#include "AnimationParameters.h"

namespace brayns
{
class SceneParameters final : public AbstractParameters
{
public:
    SceneParameters();

    /** @copydoc AbstractParameters::print */
    void print() final;

    uint32_t getAnimationFrame() const { return _anim.current; }
    void setAnimationFrame(const uint32_t value)
    {
        _anim.updateValue(_anim.current, value);
        updateValue(_anim.current, value);
    }

    /** The (frame) delta to apply for animations to select the next frame. */
    void setAnimationDelta(const int32_t animation)
    {
        _anim.updateValue(_anim.delta, animation);
        updateValue(_anim.delta, animation);
    }
    int32_t getAnimationDelta() const { return _anim.delta; }
    const std::string& getColorMapFilename() const { return _colorMapFilename; }
    /**
       file name of the environment map
    */
    const std::string& getEnvironmentMap() const { return _environmentMap; }
    AnimationParameters& getAnimationParams() { return _anim; }
protected:
    bool _parse(const po::variables_map& vm) final;

    AnimationParameters _anim;
    std::string _colorMapFilename;
    std::string _environmentMap;
};
}
#endif // SCENEPARAMETERS_H
