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

#pragma once

#include <brayns/common/loader/Loader.h>

namespace brayns
{

class LoaderRegistry
{
public:
    LoaderRegistry() = default;

    struct LoaderInfo
    {
        std::function<bool(std::string)> canHandle;
        std::function<LoaderPtr()> createLoader;
    };

    void registerLoader(LoaderInfo loaderInfo) { _loaders.push_back(loaderInfo); }
    void load(Blob&& blob, Scene& scene, const Matrix4f& transformation,
                     const size_t materialID,
                     Loader::UpdateCallback cb)
    {
        for (auto entry : _loaders)
        {
            if (!entry.canHandle(blob.type))
                continue;
            auto loader = entry.createLoader();
            loader->setProgressCallback(cb);
            loader->importFromBlob(std::move(blob), scene, transformation,
                                   materialID);
            break;
        }
    }

    void load(const std::string& filename, Scene& scene,
                     const Matrix4f& transformation, const size_t materialID,
                     Loader::UpdateCallback cb)
    {
        for (auto entry : _loaders)
        {
            if (!entry.canHandle(filename))
                continue;
            auto loader = entry.createLoader();
            loader->setProgressCallback(cb);
            loader->importFromFile(filename, scene, transformation, materialID);
            break;
        }
    }

private:
    std::vector<LoaderInfo> _loaders;
};
}
