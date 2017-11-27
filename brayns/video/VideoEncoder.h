/* Copyright (c) 2015-2017, EPFL/Blue Brain Project
 *
 * Responsible Author: Daniel.Nachbaur@epfl.ch
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

#include <memory>

namespace brayns
{
/**
 * A class which tracks and reports progress of an operation/execution.
 * It reports on stdout using boost::progress_display and also reports to a
 * user-defined callback.
 *
 * It is safe to use in threaded environments like OpenMP for-loops. In that
 * case, only the first OpenMP thread reports progress, but still tracks from
 * all threads.
 */
class VideoEncoder
{
public:
    VideoEncoder();
    ~VideoEncoder();

    void encode(const uint8_t* rgba);

private:
    class Impl;
    std::unique_ptr<Impl> _impl;
};
}
