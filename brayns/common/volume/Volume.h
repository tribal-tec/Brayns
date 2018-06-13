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

#include <brayns/common/BaseObject.h>
#include <brayns/common/types.h>

namespace brayns
{
class Volume : public BaseObject
{
public:
    Volume(const Vector3ui& dimension, const Vector3f& spacing,
           const DataType type);
    virtual ~Volume() = default;

    virtual void setDataRange(const Vector2f& range) = 0;

    virtual void commit() = 0;

    size_t getSizeInBytes() const { return _sizeInBytes; }
    Boxf getBounds() const
    {
        return {{0, 0, 0},
                {_dimension.x() * _spacing.x(), _dimension.y() * _spacing.y(),
                 _dimension.z() * _spacing.z()}};
    }

protected:
    size_t _sizeInBytes{0};
    const Vector3ui _dimension;
    const Vector3f _spacing;
    const DataType _dataType;
};
}
