/* Copyright (c) 2015-2018, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
 * Responsible Author: Daniel.Nachbaur <daniel.nachbaur@epfl.ch>
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

#include "LoaderRegistry.h"

#include <brayns/common/utils/Utils.h>

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

namespace brayns
{
void LoaderRegistry::registerLoader(LoaderInfo loaderInfo)
{
    _loaders.push_back(loaderInfo);
}

bool LoaderRegistry::isSupported(const std::string& type) const
{
    for (auto entry : _loaders)
    {
        if (_canHandle(entry, type))
            return true;
    }
    return false;
}

std::set<std::string> LoaderRegistry::supportedTypes() const
{
    std::set<std::string> result;
    for (auto entry : _loaders)
    {
        auto&& types = entry.supportedTypes();
        result.insert(types.begin(), types.end());
    }
    return result;
}

void LoaderRegistry::load(Blob&& blob, Scene& scene,
                          const Matrix4f& transformation,
                          const size_t materialID, Loader::UpdateCallback cb)
{
    for (const auto& entry : _loaders)
    {
        if (!_canHandle(entry, blob.type))
            continue;
        auto loader = entry.createLoader();
        loader->setProgressCallback(cb);
        loader->importFromBlob(std::move(blob), scene, transformation,
                               materialID);
        break;
    }
}

void LoaderRegistry::load(const std::string& path, Scene& scene,
                          const Matrix4f& transformation,
                          const size_t materialID, Loader::UpdateCallback cb)
{
    for (const auto& entry : _loaders)
    {
        if (!_canHandle(entry, path))
            continue;

        auto loader = entry.createLoader();
        loader->setProgressCallback(cb);

        if (fs::is_directory(path))
        {
            for (const auto& i :
                 boost::make_iterator_range(fs::directory_iterator(path), {}))
            {
                const auto& currentPath = i.path().string();
                if (_canHandle(entry, currentPath))
                    loader->importFromFile(currentPath, scene, transformation,
                                           materialID);
            }
        }
        else
            loader->importFromFile(path, scene, transformation, materialID);
        return;
    }
    throw std::runtime_error("no loader found");
}

bool LoaderRegistry::_canHandle(const LoaderInfo& loader,
                                const std::string& type) const
{
    // the first file in the folder that is supported by this loader wins
    if (fs::is_directory(type))
    {
        for (const auto& i :
             boost::make_iterator_range(fs::directory_iterator(type), {}))
        {
            if (_canHandle(loader, i.path().string()))
                return true;
        }
        return false;
    }
    auto extension = fs::extension(type);
    if (extension.empty())
        extension = type;
    else
        extension = extension.erase(0, 1);

    auto types = loader.supportedTypes();
    auto found =
        std::find_if(types.cbegin(), types.cend(), [extension](auto val) {
            return lowerCase(val) == lowerCase(extension);
        });
    return found != types.end();
}
}