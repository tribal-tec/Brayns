/* Copyright (c) 2015-2018, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
 * Responsible Author: Daniel Nachbaur <daniel.nachbaur@epfl.ch>
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

#include <brayns/common/types.h>

namespace brayns
{
class Volume
{
public:
    virtual ~Volume() = default;

    virtual void setDimensions(const Vector3ui& dim) = 0;
    virtual void setGridSpacing(const Vector3f& spacing) = 0;
    virtual void setDataRange(const Vector2f& range) = 0;

    enum class DataType
    {
        FLOAT,
        UINT8,
        UINT16,
        UINT32,
        INT8,
        INT16,
        INT32
    };
    virtual void setDataType(const DataType type) = 0;

    virtual void commit() = 0;
};
}
