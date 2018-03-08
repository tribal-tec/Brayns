/* Copyright (c) 2015-2018, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
 * Responsible Author: Cyrille Favreau <cyrille.favreau@epfl.ch>
 *                     Jafet Villafranca <jafet.villafrancadiaz@epfl.ch>
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

#include "PluginAPI.h"

#include "Brayns.h"
#include "common/engine/Engine.h"

namespace brayns
{
class PluginAPI::Impl
{
public:
    Brayns& _brayns;
    Impl(Brayns& brayns)
        : _brayns(brayns)
    {
    }
};

PluginAPI::PluginAPI(Brayns& brayns)
    : _impl(std::make_shared<Impl>(brayns))
{
}

Scene& PluginAPI::getScene()
{
    return _impl->_brayns.getEngine().getScene();
}

ParametersManager& PluginAPI::getParametersManager()
{
    return _impl->_brayns.getParametersManager();
}

ActionInterface* PluginAPI::getActionInterface()
{
    return _impl->_brayns.getActionInterface();
}

KeyboardHandler& PluginAPI::getKeyboardHandler()
{
    return _impl->_brayns.getKeyboardHandler();
}

AbstractManipulator& PluginAPI::getCameraManipulator()
{
    return _impl->_brayns.getCameraManipulator();
}
}
