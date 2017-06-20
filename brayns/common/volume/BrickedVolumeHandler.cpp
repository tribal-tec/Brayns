/* Copyright (c) 2015-2017, EPFL/Blue Brain Project
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

#include "BrickedVolumeHandler.h"

#include <brayns/common/log.h>

#include <livre/data/DFSTraversal.h>
#include <livre/data/DataSource.h>
#include <livre/data/SelectVisibles.h>
#include <servus/uri.h>

namespace brayns
{
bool BrickedVolumeHandler::_pluginsLoaded{false};

bool BrickedVolumeHandler::isVolumeSupported(const std::string& volumeFile)
{
    if (!_pluginsLoaded)
    {
        livre::DataSource::loadPlugins();
        _pluginsLoaded = true;
    }
    return livre::DataSource::handles(servus::URI(volumeFile));
}

BrickedVolumeHandler::BrickedVolumeHandler()
{
}

BrickedVolumeHandler::~BrickedVolumeHandler()
{
}

void BrickedVolumeHandler::attachVolumeToFile(const std::string& volumeFile)
{
    if (!_pluginsLoaded)
    {
        livre::DataSource::loadPlugins();
        _pluginsLoaded = true;
    }

    _datasource.reset(new livre::DataSource{servus::URI(volumeFile)});
}

BrickedVolumeHandler::DataPtr BrickedVolumeHandler::getData(
    const livre::NodeId& nodeID) const
{
    livre::ConstMemoryUnitPtr dataBlock = _datasource->getData(nodeID);

    const size_t voxels = getVoxelBox(nodeID).product();
    DataPtr data1(new float[voxels * sizeof(float)]);
    for (size_t i = 0; i < voxels; ++i)
        data1.get()[i] = dataBlock->getData<uint8_t>()[i];

    return data1;
}

void* BrickedVolumeHandler::getRawData(const livre::NodeId& nodeID) const
{
    livre::ConstMemoryUnitPtr dataBlock = _datasource->getData(nodeID);
    return (void*)dataBlock->getData<void>();
}

livre::NodeIds BrickedVolumeHandler::getVisibles(const size_t lod) const
{
    const float near = 0.1f;
    const float far = 15.f;
    const livre::Frustumf proj(45, 4.f / 3.f, near, far);
    const livre::Matrix4f modelView(livre::Vector3f{0, 0, -2}, {0, 0, 0},
                                    {0, 1, 0});
    livre::Frustum frustum(modelView, proj.computePerspectiveMatrix());

    livre::SelectVisibles visitor(*_datasource, frustum, 1000, 1 /*sse*/,
                                  lod /*minLOD*/, lod /*maxLOD*/,
                                  livre::Range{0, 1}, {});

    livre::DFSTraversal traverser;
    traverser.traverse(_datasource->getVolumeInfo().rootNode, visitor,
                       0 /*frame*/);

    return visitor.getVisibles();
}

vmml::Vector3ui BrickedVolumeHandler::getVoxelBox(
    const livre::NodeId& nodeID) const
{
    const auto& volInfo = _datasource->getVolumeInfo();
    const auto& node = _datasource->getNode(nodeID);
    return node.getBlockSize() + 2u * volInfo.overlap;
}

vmml::Vector3ui BrickedVolumeHandler::getPosition(
    const livre::NodeId& nodeID) const
{
    const auto& volInfo = _datasource->getVolumeInfo();
    return nodeID.getPosition() *
           (volInfo.maximumBlockSize - 2u * volInfo.overlap);
}

livre::DataType BrickedVolumeHandler::getDataType() const
{
    return _datasource->getVolumeInfo().dataType;
}

Vector3i BrickedVolumeHandler::getDimension(const size_t lod) const
{
    const auto& volInfo = _datasource->getVolumeInfo();
    const uint32_t maxDepth = volInfo.rootNode.getDepth();
    return Vector3i{(int)volInfo.voxels.x() >> (maxDepth - lod - 2),
                    (int)volInfo.voxels.y() >> (maxDepth - lod - 2),
                    (int)volInfo.voxels.z() >> (maxDepth - lod - 2)};
}

Vector3f BrickedVolumeHandler::getGridSpacing(const size_t lod) const
{
    const auto& volInfo = _datasource->getVolumeInfo();
    const size_t maxDepth = volInfo.rootNode.getDepth();
    Vector3f gridSpacing{1, 1, 1};
    if (lod < maxDepth)
    {
        const float spacing = 1 << (maxDepth - lod - 1);
        gridSpacing = {spacing, spacing, spacing};
    }
    return gridSpacing;
}
}
