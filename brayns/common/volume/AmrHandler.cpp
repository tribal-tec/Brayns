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

#include <livre/data/DFSTraversal.h>
#include <livre/data/DataSource.h>
#include <livre/data/SelectVisibles.h>
#include <servus/uri.h>

namespace brayns
{
AmrHandler::AmrHandler(const VolumeParameters& volumeParameters)
    : _volumeParameters(volumeParameters)
{
    livre::DataSource::loadPlugins();
    _datasource.reset(
        new livre::DataSource{servus::URI(volumeParameters.getFilename())});
}

AmrHandler::~AmrHandler()
{
}

void AmrHandler::attachVolumeToFile(const std::string& volumeFile)
{
    _file = volumeFile;
}

AmrHandler::DataPtr AmrHandler::getData(const livre::NodeId& nodeID) const
{
    const auto& volInfo = _datasource->getVolumeInfo();
    const auto& node = _datasource->getNode(nodeID);
    const auto voxelBox = node.getBlockSize() + 2u * volInfo.overlap;

    livre::ConstMemoryUnitPtr dataBlock = _datasource->getData(nodeID);

    const size_t voxels = voxelBox.product();
    DataPtr data1(new float[voxels * sizeof(float)]);
    for (size_t i = 0; i < voxels; ++i)
        data1.get()[i] = dataBlock->getData<uint8_t>()[i];

    return data1;
}

void* AmrHandler::getRawData(const livre::NodeId& nodeID) const
{
    livre::ConstMemoryUnitPtr dataBlock = _datasource->getData(nodeID);
    return (void*)dataBlock->getData<void>();
}

livre::NodeIds AmrHandler::getVisibles(const size_t lod) const
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

Boxui AmrHandler::getBox(const livre::NodeId& nodeID) const
{
#if 0
    const auto& volInfo = _datasource->getVolumeInfo();

    const auto& node = _datasource->getNode(nodeID);
    auto voxelBox = node.getVoxelBox();

    voxelBox = livre::Boxui( voxelBox.getMin(), voxelBox.getMax() + 2u * volInfo.overlap);
    return voxelBox;
#else
    const auto& volInfo = _datasource->getVolumeInfo();
    const auto maxBlockSize = volInfo.maximumBlockSize;

    Vector3ui lower{nodeID.getPosition().x() * maxBlockSize.x(),
                    nodeID.getPosition().y() * maxBlockSize.y(),
                    nodeID.getPosition().z() * maxBlockSize.z()};

    if (lower.x() > 0)
        lower.x() -= 2 * volInfo.overlap.x();
    if (lower.y() > 0)
        lower.y() -= 2 * volInfo.overlap.y();
    if (lower.z() > 0)
        lower.z() -= 2 * volInfo.overlap.z();

    const auto& node = _datasource->getNode(nodeID);
    const auto voxelBox = node.getBlockSize() + 2u * volInfo.overlap;

    Vector3ui upper{lower + voxelBox};

    return Boxui{lower, upper};
#endif
}

vmml::Vector3ui AmrHandler::getVoxelBox(const livre::NodeId& nodeID) const
{
    const auto& volInfo = _datasource->getVolumeInfo();
    const auto& node = _datasource->getNode(nodeID);
    return node.getBlockSize() + 2u * volInfo.overlap;
}

vmml::Vector3ui AmrHandler::getRegionLo(const livre::NodeId& nodeID) const
{
    const auto& volInfo = _datasource->getVolumeInfo();
    return nodeID.getPosition() * volInfo.maximumBlockSize;
}

livre::DataType AmrHandler::getDataType() const
{
    return _datasource->getVolumeInfo().dataType;
}

Vector3i AmrHandler::getDimension(const size_t lod) const
{
    const auto& volInfo = _datasource->getVolumeInfo();
    const uint32_t maxDepth = volInfo.rootNode.getDepth();
    return Vector3i{(int)volInfo.voxels.x() >> (maxDepth - lod - 1),
                    (int)volInfo.voxels.y() >> (maxDepth - lod - 1),
                    (int)volInfo.voxels.z() >> (maxDepth - lod - 1)};
}

Vector3f AmrHandler::getGridSpacing(const size_t lod) const
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
