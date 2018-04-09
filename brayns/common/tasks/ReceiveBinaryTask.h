/* Copyright (c) 2015-2018, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
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

#include <brayns/common/tasks/Task.h>

namespace brayns
{
struct BinaryParam
{
    size_t size{0};
    std::string type; // file extension or MESH, POINTS, CIRCUIT
};

using BinaryParams = std::vector<BinaryParam>;

class ReceiveBinaryTask : public TaskT<bool>
{
public:
    ReceiveBinaryTask(const BinaryParams& params,
                      const std::set<std::string>& supportedTypes,
                      EnginePtr engine);

    size_t getTotalBytes() const { return _totalBytes; }
    void appendBlob(const std::string& blob);

private:
    std::vector<async::task<void>> tasks;
    std::vector<async::event_task<std::string>> chunks;
    async::event_task<bool> _errorEvent;
    std::vector<async::task<bool>> bla;
    std::string _blob;
    size_t _index{0};

    void _cancel() final
    {
        for (auto& i : chunks)
            i.set_exception(std::make_exception_ptr(async::task_canceled()));
    }
    void _updateTotalBytes()
    {
        for (const auto& param : _params)
            _totalBytes += param.size;
    }
    float _progressBytes() const
    {
        return 0.5f * ((float)_receivedBytes / _totalBytes);
    }

    BinaryParams _params;
    size_t _totalBytes{0};
    size_t _receivedBytes{0};
};

auto createReceiveBinaryTask(const BinaryParams& params,
                             const std::set<std::string>& supportedTypes,
                             EnginePtr engine)
{
    return std::make_shared<ReceiveBinaryTask>(params, supportedTypes, engine);
}
}
