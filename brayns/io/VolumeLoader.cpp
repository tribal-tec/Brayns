/* Copyright (c) 2015-2018, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
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

#include "VolumeLoader.h"

#include <brayns/common/scene/Model.h>
#include <brayns/common/scene/Scene.h>
#include <brayns/common/volume/SharedDataVolume.h>

#include <boost/filesystem.hpp>

namespace brayns
{
VolumeLoader::VolumeLoader(Scene& scene,
                           const VolumeParameters& volumeParameters)
    : Loader(scene)
    , _volumeParameters(volumeParameters)
{
}

std::set<std::string> VolumeLoader::getSupportedDataTypes()
{
    return {"raw", "mhd"};
}

ModelDescriptorPtr VolumeLoader::importFromBlob(
    Blob&& blob BRAYNS_UNUSED, const size_t index BRAYNS_UNUSED,
    const size_t defaultMaterialId BRAYNS_UNUSED)
{
    auto model = _scene.createModel();
    return std::make_shared<ModelDescriptor>(std::move(model), blob.name);
}

ModelDescriptorPtr VolumeLoader::importFromFile(
    const std::string& filename, const size_t index BRAYNS_UNUSED,
    const size_t defaultMaterialId BRAYNS_UNUSED)
{
    auto model = _scene.createModel();

    Vector3f dimension, spacing;
    DataType type;
    const bool mhd = boost::filesystem::extension(filename) == ".mhd";
    if (mhd)
    {
        // TODO
        dimension = _volumeParameters.getDimensions();
        spacing = _volumeParameters.getElementSpacing();
        type = DataType::UINT8;
    }
    else
    {
        dimension = _volumeParameters.getDimensions();
        spacing = _volumeParameters.getElementSpacing();
        type = DataType::UINT8;
    }

    if (dimension.product() == 0)
        throw std::runtime_error("Volume dimension is empty");

    Vector2f dataRange;
    switch (type)
    {
    case DataType::FLOAT:
        dataRange = {0, 1};
        break;
    case DataType::UINT8:
        dataRange = {std::numeric_limits<uint8_t>::min(),
                     std::numeric_limits<uint8_t>::max()};
        break;
    case DataType::UINT16:
        dataRange = {std::numeric_limits<uint16_t>::min(),
                     std::numeric_limits<uint16_t>::max()};
        break;
    case DataType::UINT32:
        dataRange = {std::numeric_limits<uint32_t>::min() / 100,
                     std::numeric_limits<uint32_t>::max() / 100};
        break;
    case DataType::INT8:
        dataRange = {std::numeric_limits<int8_t>::min(),
                     std::numeric_limits<int8_t>::max()};
        break;
    case DataType::INT16:
        dataRange = {std::numeric_limits<int16_t>::min(),
                     std::numeric_limits<int16_t>::max()};
        break;
    case DataType::INT32:
        dataRange = {std::numeric_limits<int32_t>::min() / 100,
                     std::numeric_limits<int32_t>::max() / 100};
        break;
    }

    auto volume = _scene.createSharedDataVolume(dimension, spacing, type);
    volume->setDataRange(dataRange);
    volume->setData(filename);
    model->addVolume(volume);
    return std::make_shared<ModelDescriptor>(
        std::move(model), filename,
        ModelMetadata{{"dimensions", std::to_string(dimension.x()) + " " +
                                         std::to_string(dimension.y()) + " " +
                                         std::to_string(dimension.z())},
                      {"element-spacing", std::to_string(spacing.x()) + " " +
                                              std::to_string(spacing.y()) +
                                              " " +
                                              std::to_string(spacing.z())}});
}
}
