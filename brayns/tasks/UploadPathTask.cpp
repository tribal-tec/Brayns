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

#include "UploadPathTask.h"

#include "errors.h"

#include <boost/filesystem.hpp>
#include <fstream>

namespace brayns
{
inline auto lowerCase(std::string str)
{
    std::string retval = str;
    std::transform(retval.begin(), retval.end(), retval.begin(), ::tolower);
    return retval;
}

UploadPathTask::UploadPathTask(const std::string& requestID,
                               std::vector<std::string>&& paths,
                               const std::set<std::string>& supportedTypes,
                               EnginePtr engine)
    : TaskT<bool>(requestID)
{
    if (paths.empty())
        throw MISSING_PARAMS;

    for (size_t i = 0; i < paths.size(); ++i)
    {
        const auto& path = paths[i];

        if (path == "forever")
            continue;

        const boost::filesystem::path path_ = path;
        if (!boost::filesystem::exists(path_))
            throw INVALID_PATH;

        if (boost::filesystem::is_directory(path_))
            throw UNSUPPORTED_FOLDERS;

        const auto extension = boost::filesystem::extension(path_).erase(0, 1);

        auto found =
            std::find_if(supportedTypes.cbegin(), supportedTypes.cend(),
                         [&](auto val) {
                             return lowerCase(val).find(lowerCase(extension)) !=
                                    std::string::npos;
                         });

        if (found == supportedTypes.end())
            throw UNSUPPORTED_TYPE(
                {i, {supportedTypes.begin(), supportedTypes.end()}});
    }

    for (size_t i = 0; i < paths.size(); ++i)
    {
        const auto& path = paths[i];

        LoadDataFunctor functor{engine};
        functor.setCancelToken(_cancelToken);

        functor.setProgressFunc(
            [& progress = _progress,
             amountPerTask = 1.f / paths.size() ](auto msg, auto increment,
                                                  auto) {
                progress.increment(msg, increment * amountPerTask);
            });
        _loadTasks.push_back(
            async::spawn([path] {
                if (path == "forever")
                    return Blob{path, ""};

                std::ifstream file(path, std::ios::binary);
                const boost::filesystem::path path_ = path;
                return Blob{boost::filesystem::extension(path_).erase(0, 1),
                            {std::istreambuf_iterator<char>(file),
                             std::istreambuf_iterator<char>()}};
            }).then(std::move(functor)));
        functor.setProgressFunc([](auto, auto, auto) {});
    }

    // wait for load data of all files
    _task = async::when_all(_loadTasks)
                .then([](std::vector<async::task<void>> results) {
                    for (auto& result : results)
                        result.get(); // exception is propagated to caller
                    return true;
                });
}
}
