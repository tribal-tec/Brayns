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
inline auto lowerCase(std::string str)
{
    std::string retval = str;
    std::transform(retval.begin(), retval.end(), retval.begin(), ::tolower);
    return retval;
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

        if (param.type == "forever")
            continue;

        auto found = std::find_if(supportedTypes.cbegin(),
                                  supportedTypes.cend(), [&](auto val) {
                                      return lowerCase(val).find(
                                                 lowerCase(param.type)) !=
                                             std::string::npos;
                                  });

        if (found == supportedTypes.end())
            throw UNSUPPORTED_TYPE(
                {i, {supportedTypes.begin(), supportedTypes.end()}});
    }

    _updateTotalBytes();

    // chunk event task is set() from appendBlob() once all data has been
    // received, then loading starts.
    _chunks.resize(params.size());
    for (size_t i = 0; i < params.size(); ++i)
        _loadTasks.push_back(_chunks[i].get_task().then(
            _setupFunctor(LoadDataFunctor{params[i].type, engine})));

    // wait for load data of all files
    auto allLoaded =
        async::when_all(_loadTasks)
            .then([](std::vector<async::task<void>> results) {
                for (auto& result : results)
                    result.get(); // exception is propagated to caller
            });

    // either finish with success/error from loading, or cancel
    _finishTasks.emplace_back(_errorEvent.get_task());
    _finishTasks.emplace_back(std::move(allLoaded));
    _task = async::when_any(_finishTasks)
                .then([](async::when_any_result<std::vector<async::task<void>>>
                             results) {
                    results.tasks[results.index].get(); // exception is
                                                        // propagated to
                                                        // caller
                    return true;
                });
}

void ReceiveBinaryTask::appendBlob(const std::string& blob)
{
    if (_index >= _params.size() ||
        (_blob.size() + blob.size() > _params[_index].size))
    {
        _errorEvent.set_exception(
            std::make_exception_ptr(INVALID_BINARY_RECEIVE));
        return;
    }

    _blob += blob;
    _receivedBytes += blob.size();

    _progress.setAmount(_progressBytes());
    _progress.setOperation("Receiving data ...");

    if (_blob.size() == _params[_index].size)
    {
        _chunks[_index].set(_blob);

        ++_index;

        if (_index < _params.size())
        {
            _blob.clear();
            _blob.reserve(_params[_index].size);
        }
    }
}
}
