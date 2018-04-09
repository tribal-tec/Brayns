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

#include "ReceiveBinaryTask.h"

#include "LoadDataFunctor.h"
#include "errors.h"

namespace brayns
{
inline bool endsWith(const std::string& value, const std::string& ending)
{
    if (ending.size() > value.size())
        return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

ReceiveBinaryTask::ReceiveBinaryTask(
    const BinaryParams& params, const std::set<std::string>& supportedTypes,
    EnginePtr engine)
    : _params(params)
{
    if (params.empty())
        throw MISSING_PARAMS;

    for (size_t i = 0; i < params.size(); ++i)
    {
        const auto& param = params[i];
        if (param.type.empty() || param.size == 0)
            throw MISSING_PARAMS;

        // try exact pattern match
        // TODO: proper regex check for *.obj and obj cases
        bool supported =
            supportedTypes.find(param.type) != supportedTypes.end();
        if (supported)
            continue;

        // fallback to match "ends with extension"
        supported = false;
        for (const auto& type : supportedTypes)
        {
            if (endsWith(type, param.type))
            {
                supported = true;
                break;
            }
        }

        if (!supported)
            throw UNSUPPORTED_TYPE(
                {i, {supportedTypes.begin(), supportedTypes.end()}});
    }
    _updateTotalBytes();

    // chunk event task is set() from appendBlob() once all data has been
    // received, then loading starts.
    chunks.resize(params.size());
    for (size_t i = 0; i < params.size(); ++i)
        tasks.push_back(chunks[i].get_task().then(
            _setupFunctor(LoadDataFunctor{params[i].type, engine})));

    // wait for load data of all files
    auto allLoaded =
        async::when_all(tasks).then([](std::vector<async::task<void>> results) {
            for (auto& result : results)
                result.get(); // exception is propagated to caller
            return true;
        });
    bla.emplace_back(_errorEvent.get_task());
    bla.emplace_back(std::move(allLoaded));

    // either finish with success or error from loading, or external error
    _task = async::when_any(bla).then(
        [](async::when_any_result<std::vector<async::task<bool>>> result) {
            result.tasks[result.index].get(); // exception is propagated to
                                              // caller
            return true;
        });
}

void ReceiveBinaryTask::appendBlob(const std::string& blob)
{
    if (_index >= _params.size())
    {
        _errorEvent.set_exception(
            std::make_exception_ptr(INVALID_BINARY_RECEIVE));
        return;
    }

    _blob += blob;
    _receivedBytes += blob.size();

    _progress.setAmount(_progressBytes());
    _progress.setOperation("Receiving data ...");
    progressUpdated(_progress, false);

    if (_blob.size() == _params[_index].size)
    {
        chunks[_index].set(_blob);

        ++_index;

        if (_index < _params.size())
        {
            _blob.clear();
            _blob.reserve(_params[_index].size);
        }
    }
}
}
