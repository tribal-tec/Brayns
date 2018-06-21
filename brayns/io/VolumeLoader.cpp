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
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
using boost::property_tree::ptree;

namespace brayns
{
namespace
{
Vector3f to_Vector3f(const std::string& s)
{
  std::vector<float> result;
  std::stringstream ss(s);
  std::string item;
  while(std::getline(ss, item, ' '))
      result.push_back(std::stof(item));
  if(result.size() != 3)
      throw std::runtime_error("Not exactly 3 values for mhd array");
  return Vector3f(result.data());
}

DataType dataTypeFromMET(const std::string& type)
{
    if (type=="MET_FLOAT") return DataType::FLOAT;
    else if (type=="MET_UCHAR") return DataType::UINT8;
    else if (type=="MET_USHORT") return DataType::UINT16;
    else if (type=="MET_UINT") return DataType::UINT32;
    else if (type=="MET_CHAR") return DataType::INT8;
    else if (type=="MET_SHORT") return DataType::INT16;
    else if (type=="MET_INT") return DataType::INT32;
    else throw std::runtime_error("Unknown data type " + type);
}
}

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
    std::string volumeFile = filename;
    const bool mhd = boost::filesystem::extension(filename) == ".mhd";
    if (mhd)
    {
        boost::property_tree::ptree pt;
        boost::property_tree::ini_parser::read_ini(filename, pt);

        if(pt.get<std::string>("ObjectType") != "Image")
            throw std::runtime_error("Wrong object type for mhd file");

        dimension = to_Vector3f(pt.get<std::string>("DimSize"));
        spacing = to_Vector3f(pt.get<std::string>("ElementSpacing"));
        type = dataTypeFromMET(pt.get<std::string>("ElementType"));
        boost::filesystem::path path = pt.get<std::string>("ElementDataFile");
        if(!path.is_absolute())
        {
            boost::filesystem::path basePath(filename);
            path = boost::filesystem::canonical(path, basePath.parent_path());
        }
        volumeFile = path.string();
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
    volume->setData(volumeFile);
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
