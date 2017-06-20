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

#pragma once

#include <brayns/common/types.h>
#include <brayns/parameters/VolumeParameters.h>
#include <lexis/render/Histogram.h>
#include <livre/data/DataSource.h>
#include <livre/data/NodeId.h>

#include <future>

namespace brayns
{
class BrickedVolumeHandler
{
public:
    BrickedVolumeHandler();
    ~BrickedVolumeHandler();

    template <typename T>
    struct Deleter
    {
        void operator()(T* ptr) { delete[] ptr; }
    };
    using DataPtr = std::unique_ptr<float, Deleter<float>>;

    /**
     * @brief Returns a pointer to a given frame in the memory mapped file.
     * @return Pointer to volume
     */
    DataPtr getData(const livre::NodeId& nodeID) const;

    void* getRawData(const livre::NodeId& nodeID) const;

    /**
    * @brief Attaches a memory mapped file to the scene so that renderers can
    *        access the data as if it was in memory. The OS is in charge of
    *        dealing with the map file in system memory.
    * @param timestamp Timestamp for the volume
    * @param volumeFile File containing the 8bit volume
    * @return True if the file was successfully attached, false otherwise
    */
    void attachVolumeToFile(const std::string& volumeFile);

    /** @return the histogram of the currently loaded volume. */
    const Histogram& getHistogram();

    livre::NodeIds getVisibles(size_t lod) const;
    Vector3ui getVoxelBox(const livre::NodeId& nodeID) const;

    Vector3i getDimension(size_t lod) const;
    Vector3f getGridSpacing(size_t lod) const;
    Vector3ui getPosition(const livre::NodeId& nodeID) const;
    livre::DataType getDataType() const;

    static bool isVolumeSupported(const std::string& volumeFile);

private:
    void _calcHistogram(livre::ConstMemoryUnitPtr dataBlock,
                        const livre::NodeId& nodeID) const;

    Histogram _histogram;
    mutable lexis::render::Histogram _histoAccum;
    mutable std::vector<std::future<lexis::render::Histogram>> _histoFutures;

    std::string _file;
    std::unique_ptr<livre::DataSource> _datasource;
    static bool _pluginsLoaded;
};
}
