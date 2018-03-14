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

#include <brayns/common/volume/Volume.h>

#include <ospray/SDK/volume/Volume.h>

namespace brayns
{
class OSPRayVolume : public Volume
{
public:
    OSPRayVolume(OSPTransferFunction transferFunction);
    ~OSPRayVolume();

    void setDimensions(const Vector3ui& dim) final;
    void setGridSpacing(const Vector3f& spacing) final;
    void setDataRange(const Vector2f& range) final;
    void setDataType(const DataType type) final;
    void setBrick(void* data, const Vector3ui& position,
                  const Vector3ui& size) final;
    void commit() final;

    OSPVolume impl() const { return _volume; }
private:
    OSPVolume _volume;
};
}
