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

#include "AmrHandler.h"

#include <brayns/common/log.h>

namespace brayns
{
AmrHandler::AmrHandler(const VolumeParameters& volumeParameters)
    : _volumeParameters(volumeParameters)
{
}

AmrHandler::~AmrHandler()
{
}

void AmrHandler::attachVolumeToFile(const std::string& volumeFile)
{
    _file = volumeFile;
}

void* AmrHandler::getData() const
{
    return nullptr;
}

Vector3ui AmrHandler::getDimensions() const
{
    return Vector3ui();
}

Vector3f AmrHandler::getElementSpacing() const
{
    return Vector3f();
}

Vector3f AmrHandler::getOffset() const
{
    return Vector3f();
}

uint64_t AmrHandler::getSize() const
{
    return 0;
}
}
