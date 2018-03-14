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

#include "OSPRayVolume.h"

namespace brayns
{
OSPRayVolume::OSPRayVolume(OSPTransferFunction transferFunction)
    : _volume(ospNewVolume("block_bricked_volume"))
{
    ospSetObject(_volume, "transferFunction", transferFunction);
}

OSPRayVolume::~OSPRayVolume()
{
}

void OSPRayVolume::setDimensions(const Vector3ui &dim)
{
    const ospcommon::vec3i dimension(dim.x(), dim.y(), dim.z());
    ospSetVec3i(_volume, "dimensions", (osp::vec3i &)dimension);
}

void OSPRayVolume::setGridSpacing(const Vector3f &gridSpacing)
{
    const ospcommon::vec3f spacing(gridSpacing.x(), gridSpacing.y(),
                                   gridSpacing.z());
    ospSetVec3f(_volume, "gridSpacing", (osp::vec3f &)spacing);
}

void OSPRayVolume::setDataRange(const Vector2f &range)
{
    ospSet2f(_volume, "voxelRange", range.x(), range.y());
}

void OSPRayVolume::setDataType(const DataType type)
{
    switch (type)
    {
    case DataType::FLOAT:
        ospSetString(_volume, "voxelType", "float");
        break;
    case DataType::UINT8:
        ospSetString(_volume, "voxelType", "uchar");
        break;
    case DataType::UINT16:
        ospSetString(_volume, "voxelType", "ushort");
        break;
    case DataType::UINT32:
        ospSetString(_volume, "voxelType", "uint");
        break;
    case DataType::INT8:
        ospSetString(_volume, "voxelType", "char");
        break;
    case DataType::INT16:
        ospSetString(_volume, "voxelType", "short");
        break;
    case DataType::INT32:
        ospSetString(_volume, "voxelType", "int");
        break;
    }
}

void OSPRayVolume::setBrick(void *data, const Vector3ui &position,
                            const Vector3ui &size_)
{
    const ospcommon::vec3i pos{int(position.x()), int(position.y()),
                               int(position.z())};
    const ospcommon::vec3i size{int(size_.x()), int(size_.y()), int(size_.z())};
    ospSetRegion(_volume, data, (osp::vec3i &)pos, (osp::vec3i &)size);
}

void OSPRayVolume::commit()
{
    ospCommit(_volume);
}
}
