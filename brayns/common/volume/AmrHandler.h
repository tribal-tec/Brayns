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

#pragma once

#include <brayns/common/types.h>
#include <brayns/parameters/VolumeParameters.h>

namespace brayns
{
class AmrHandler
{
public:
    AmrHandler(const VolumeParameters& volumeParameters);

    ~AmrHandler();

    /**
     * @brief Returns the dimension of the 8bit volume
     * @return Dimensions of the volume for the specified timestamp
     */
    Vector3ui getDimensions() const;

    /**
     * @brief Returns the voxel size of the 8bit volume
     * @return Voxel size of the volume for the specified timestamp
     */
    Vector3f getElementSpacing() const;

    /**
     * @brief Returns the position offset of the 8bit volume in world
     *        coordinates
     * @return Volume offset position for the specified timestamp
     */
    Vector3f getOffset() const;

    /**
     * @brief Returns the size of the 8bit volume in bytes
     * @return Size of the volume for the specified timestamp
     */
    uint64_t getSize() const;

    /**
     * @brief Returns a pointer to a given frame in the memory mapped file.
     * @return Pointer to volume
     */
    void* getData() const;

    /**
    * @brief Attaches a memory mapped file to the scene so that renderers can
    *        access the data as if it was in memory. The OS is in charge of
    *        dealing with the map file in system memory.
    * @param timestamp Timestamp for the volume
    * @param volumeFile File containing the 8bit volume
    * @return True if the file was successfully attached, false otherwise
    */
    void attachVolumeToFile(const std::string& volumeFile);

    const std::string& getFile() const { return _file; }
    /** Set the histogram of the currently loaded volume. */
    void setHistogram(const Histogram& histogram) { _histogram = histogram; }
    /** @return the histogram of the currently loaded volume. */
    const Histogram& getHistogram();
    /** @return the number of frames of the current volume. */

private:
    const VolumeParameters _volumeParameters;
    Histogram _histogram;
    std::string _file;
};
}
