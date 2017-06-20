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
template <class SRC_TYPE>
void binDataSlow(const SRC_TYPE* rawData, lexis::render::Histogram& histogram,
                 const Vector3ui& blockSize, const Vector3ui& padding,
                 const uint64_t scaleFactor)
{
    std::map<SRC_TYPE, size_t> values;
    const Vector3ui dataBlockSize = blockSize + padding * 2;
    for (size_t i = padding.x(); i < dataBlockSize.x() - padding.x(); ++i)
        for (size_t j = padding.y(); j < dataBlockSize.y() - padding.y(); ++j)
            for (size_t k = padding.z(); k < dataBlockSize.z() - padding.z();
                 ++k)
            {
                const size_t index = i * dataBlockSize.y() * dataBlockSize.z() +
                                     j * dataBlockSize.z() + k;
                const SRC_TYPE data = rawData[index];
                ++values[data];
            }

    const float minVal =
        std::min(float(values.begin()->first), histogram.getMin());
    const float maxVal =
        std::max(float(values.rbegin()->first), histogram.getMax());
    const float range = maxVal - minVal;

    histogram.setMin(minVal);
    histogram.setMax(maxVal);

    if (range == 0.0f)
    {
        histogram.getBins().clear();
        return;
    }

    const size_t binCount = histogram.getBins().size();
    uint64_t* dstData = histogram.getBins().data();
    const float perBinCount = range / (binCount - 1);
    for (const auto& value : values)
    {
        const size_t binIndex = (value.first - minVal) / perBinCount;
        dstData[binIndex] += (scaleFactor * value.second);
    }
}

template <class SRC_TYPE>
void binData(const SRC_TYPE* rawData, lexis::render::Histogram& histogram,
             const Vector3ui& blockSize, const Vector3ui& padding,
             const uint64_t scaleFactor)
{
    if (padding != Vector3ui())
    {
        binDataSlow(rawData, histogram, blockSize, padding, scaleFactor);
        return;
    }

    SRC_TYPE minVal = histogram.getMin();
    SRC_TYPE maxVal = histogram.getMax();

    std::vector<size_t> values(std::numeric_limits<SRC_TYPE>::max() -
                               std::numeric_limits<SRC_TYPE>::min() + 1);
    const size_t numVoxels = blockSize.x() * blockSize.y() * blockSize.z();
    for (size_t i = 0; i < numVoxels; ++i)
    {
        const SRC_TYPE data = rawData[i];
        ++values[data + std::abs(std::numeric_limits<SRC_TYPE>::min())];
        minVal = std::min(data, minVal);
        maxVal = std::max(data, maxVal);
    }

    histogram.setMin(minVal);
    histogram.setMax(maxVal);
    const float range = maxVal - minVal;

    if (range == 0.0f)
    {
        histogram.getBins().clear();
        return;
    }

    const size_t binCount = histogram.getBins().size();
    uint64_t* dstData = histogram.getBins().data();
    const size_t perBinCount = std::ceil(range / (binCount - 1));

    for (size_t i = 0; i < values.size(); ++i)
    {
        const size_t binIndex = std::lround(i / perBinCount);
        dstData[binIndex] += scaleFactor * values[i];
    }
}

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

template <typename R>
bool is_ready(std::future<R> const& f)
{
    return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

const Histogram& BrickedVolumeHandler::getHistogram()
{
    if (!_histoFutures.empty())
    {
        for (auto histo = _histoFutures.begin(); histo != _histoFutures.end();)
        {
            if (is_ready(*histo))
            {
                _histoAccum += histo->get();
                histo = _histoFutures.erase(histo);
            }
            else
                ++histo;
        }

        _histogram.range.x() = _histoAccum.getMin();
        _histogram.range.y() = _histoAccum.getMax();
        _histogram.values = _histoAccum.getBinsVector();
    }

    return _histogram;
}

BrickedVolumeHandler::DataPtr BrickedVolumeHandler::getData(
    const livre::NodeId& nodeID) const
{
    livre::ConstMemoryUnitPtr dataBlock = _datasource->getData(nodeID);
    _calcHistogram(dataBlock, nodeID);

    const size_t voxels = getVoxelBox(nodeID).product();
    DataPtr data1(new float[voxels * sizeof(float)]);
    for (size_t i = 0; i < voxels; ++i)
        data1.get()[i] = dataBlock->getData<uint8_t>()[i];

    return data1;
}

void* BrickedVolumeHandler::getRawData(const livre::NodeId& nodeID) const
{
    livre::ConstMemoryUnitPtr dataBlock = _datasource->getData(nodeID);
    _calcHistogram(dataBlock, nodeID);
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

Vector3i BrickedVolumeHandler::getDimension(size_t lod) const
{
    const auto& volInfo = _datasource->getVolumeInfo();
    const size_t maxDepth = volInfo.rootNode.getDepth();
    lod = std::min(lod, maxDepth - 1);
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

void BrickedVolumeHandler::_calcHistogram(livre::ConstMemoryUnitPtr dataBlock,
                                          const livre::NodeId& nodeID) const
{
    _histoFutures.push_back(
        std::async(std::launch::async, [dataBlock, nodeID, this] {
            lexis::render::Histogram histo;

            const livre::LODNode& lodNode = _datasource->getNode(nodeID);
            const auto& volInfo = _datasource->getVolumeInfo();
            const uint64_t scaleFactor1d =
                1 << (volInfo.rootNode.getDepth() - lodNode.getRefLevel() - 1);

            const uint64_t scaleFactor =
                scaleFactor1d * scaleFactor1d * scaleFactor1d;

            const auto voxelBox = lodNode.getVoxelBox().getSize();

            switch (volInfo.dataType)
            {
            case livre::DT_UINT8:
                histo.setMin(std::numeric_limits<uint8_t>::min());
                histo.setMax(std::numeric_limits<uint8_t>::max());
                histo.resize(256);
                binData(dataBlock->getData<uint8_t>(), histo, voxelBox,
                        volInfo.overlap, scaleFactor);
                break;
            case livre::DT_UINT16:
                histo.setMin(std::numeric_limits<uint16_t>::max());
                histo.setMax(std::numeric_limits<uint16_t>::min());
                histo.resize(1024);
                binData(dataBlock->getData<uint16_t>(), histo, voxelBox,
                        volInfo.overlap, scaleFactor);
                break;
            case livre::DT_UINT32:
                histo.setMin(std::numeric_limits<uint32_t>::max());
                histo.setMax(std::numeric_limits<uint32_t>::min());
                histo.resize(4096);
                binDataSlow(dataBlock->getData<uint32_t>(), histo, voxelBox,
                            volInfo.overlap, scaleFactor);
                break;
            case livre::DT_INT8:
                histo.setMin(std::numeric_limits<int8_t>::min());
                histo.setMax(std::numeric_limits<int8_t>::max());
                histo.resize(256);
                binData(dataBlock->getData<uint8_t>(), histo, voxelBox,
                        volInfo.overlap, scaleFactor);
                break;
            case livre::DT_INT16:
                histo.setMin(std::numeric_limits<int16_t>::max());
                histo.setMax(std::numeric_limits<int16_t>::min());
                histo.resize(1024);
                binData(dataBlock->getData<uint16_t>(), histo, voxelBox,
                        volInfo.overlap, scaleFactor);
                break;
            case livre::DT_INT32:
                histo.setMin(std::numeric_limits<int32_t>::max());
                histo.setMax(std::numeric_limits<int32_t>::min());
                histo.resize(4096);
                binDataSlow(dataBlock->getData<uint32_t>(), histo, voxelBox,
                            volInfo.overlap, scaleFactor);
                break;
            case livre::DT_FLOAT:
                histo.setMin(std::numeric_limits<float>::max());
                histo.setMax(std::numeric_limits<float>::min());
                histo.resize(256);
                binDataSlow(dataBlock->getData<float>(), histo, voxelBox,
                            volInfo.overlap, scaleFactor);
                break;
            case livre::DT_UNDEFINED:
            default:
            {
                throw std::runtime_error("Unimplemented data type.");
            }
            }

            return histo;
        }));
}
}
