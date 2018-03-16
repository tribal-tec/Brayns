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

#include <brayns/parameters/VolumeParameters.h>

namespace brayns
{
OSPRayVolume::OSPRayVolume(VolumeParameters &params,
                           OSPTransferFunction transferFunction)
    : _parameters(params)
    , _volume(ospNewVolume("block_bricked_volume"))
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
        _dataSize = 4;
        break;
    case DataType::UINT8:
        ospSetString(_volume, "voxelType", "uchar");
        _dataSize = 1;
        break;
    case DataType::UINT16:
        ospSetString(_volume, "voxelType", "ushort");
        _dataSize = 2;
        break;
    case DataType::UINT32:
        ospSetString(_volume, "voxelType", "uint");
        _dataSize = 4;
        break;
    case DataType::INT8:
        ospSetString(_volume, "voxelType", "char");
        _dataSize = 1;
        break;
    case DataType::INT16:
        ospSetString(_volume, "voxelType", "short");
        _dataSize = 2;
        break;
    case DataType::INT32:
        ospSetString(_volume, "voxelType", "int");
        _dataSize = 4;
        break;
    }
}

size_t OSPRayVolume::setBrick(void *data, const Vector3ui &position,
                              const Vector3ui &size_)
{
    const ospcommon::vec3i pos{int(position.x()), int(position.y()),
                               int(position.z())};
    const ospcommon::vec3i size{int(size_.x()), int(size_.y()), int(size_.z())};
    ospSetRegion(_volume, data, (osp::vec3i &)pos, (osp::vec3i &)size);
    const size_t sizeInBytes = size_.product() * _dataSize;
    _sizeInBytes += sizeInBytes;
    return sizeInBytes;
}

void OSPRayVolume::commit()
{
    ospSet1i(_volume, "gradientShadingEnabled",
             _parameters.getGradientShading());
    ospSet1f(_volume, "adaptiveMaxSamplingRate",
             _parameters.getAdaptiveMaxSamplingRate());
    ospSet1i(_volume, "adaptiveSampling", _parameters.getAdaptiveSampling());
    ospSet1i(_volume, "singleShade", true);
    ospSet1i(_volume, "preIntegration", true);
    ospSet1f(_volume, "samplingRate", _parameters.getSamplingRate());
    ospSet3fv(_volume, "specular", &_parameters.getSpecular().x());
    ospSet3fv(_volume, "volumeClippingBoxLower",
              &_parameters.getClipBox().getMin().x());
    ospSet3fv(_volume, "volumeClippingBoxUpper",
              &_parameters.getClipBox().getMax().x());
    ospCommit(_volume);
}
}
