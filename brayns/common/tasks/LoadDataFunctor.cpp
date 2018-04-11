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

#include "LoadDataFunctor.h"

#include "errors.h"

#include <brayns/common/Progress.h>
#include <brayns/common/camera/Camera.h>
#include <brayns/common/engine/Engine.h>
#include <brayns/common/renderer/FrameBuffer.h>
#include <brayns/common/scene/Scene.h>

#include <brayns/io/MeshLoader.h>
#include <brayns/io/XYZBLoader.h>

#include <brayns/parameters/ParametersManager.h>

namespace brayns
{
const size_t LOADING_PROGRESS_DATA = 100;
const size_t LOADING_PROGRESS_STEP = 10;

LoadDataFunctor::LoadDataFunctor(const std::string& type, EnginePtr engine)
    : _engine(engine)
{
    _blob.type = type;
}

LoadDataFunctor::~LoadDataFunctor()
{
    if (!_empty)
        return;

    // load default if we got cancelled
    Scene& scene = _engine->getScene();
    scene.unload();
    BRAYNS_INFO << "Building default scene" << std::endl;
    scene.buildDefault();

    Progress dummy("", 0, [](const std::string&, const float) {});

    _postLoad(dummy, false);
}

void LoadDataFunctor::operator()(std::string data)
{
    // fix race condition: we have to wait until rendering is finished
    std::unique_lock<std::shared_timed_mutex> lock{_engine->dataMutex(),
                                                   std::defer_lock};
    while (!lock.try_lock_for(std::chrono::seconds(1)))
        cancelCheck();

    _blob.cancelCheck = std::bind(&LoadDataFunctor::cancelCheck, this);
    _blob.progressFunc = _progressFunc;
    _blob.data = std::move(data);

    Progress loadingProgress("Loading scene ...",
                             LOADING_PROGRESS_DATA + 3 * LOADING_PROGRESS_STEP,
                             [& func =
                                  _blob.progressFunc](const std::string& msg,
                                                      const float progress) {
                                 const auto offset =
                                     0.5f; // TODO: same as in ReceiveTask
                                 func(msg, offset + progress * (1.f - offset));
                             });

    Scene& scene = _engine->getScene();

    loadingProgress.setMessage("Unloading ...");
    scene.unload();
    loadingProgress += LOADING_PROGRESS_STEP;
    _empty = true;

    loadingProgress.setMessage("Loading data ...");
    scene.resetMaterials();
    const bool success = _loadData(loadingProgress);

    if (!success || (scene.empty() && !scene.getVolumeHandler()))
    {
        scene.unload();
        BRAYNS_INFO << "Building default scene" << std::endl;
        scene.buildDefault();
    }

    _postLoad(loadingProgress);

    if (!success)
        throw LOADING_BINARY_FAILED(_blob.error);
    _empty = false;
}

bool LoadDataFunctor::_loadData(Progress& loadingProgress)
{
    size_t nextTic = 0;
    const size_t tic = LOADING_PROGRESS_DATA;
    auto updateProgress = [&nextTic, &loadingProgress](const std::string& msg,
                                                       const float progress) {
        loadingProgress.setMessage(msg);

        const size_t newProgress = progress * tic;
        if (newProgress % tic > nextTic)
        {
            loadingProgress += newProgress - nextTic;
            nextTic = newProgress;
        }
    };

    // for unit tests
    if (_blob.type == "forever")
    {
        for (;;)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            _blob.cancelCheck();
        }
        return false;
    }

    if (_blob.type == "xyz")
        return _loadXYZBBlob(updateProgress);

    return _loadMeshBlob(updateProgress);
}

bool LoadDataFunctor::_loadXYZBBlob(
    const Progress::UpdateCallback& progressUpdate)
{
    auto& geometryParameters =
        _engine->getParametersManager().getGeometryParameters();
    auto& scene = _engine->getScene();
    XYZBLoader xyzbLoader(geometryParameters);
    xyzbLoader.setProgressCallback(progressUpdate);
    return xyzbLoader.importFromBlob(_blob, scene);
}

bool LoadDataFunctor::_loadMeshBlob(
    const Progress::UpdateCallback& progressUpdate)
{
    const auto& geometryParameters =
        _engine->getParametersManager().getGeometryParameters();
    auto& scene = _engine->getScene();
    const size_t material =
        geometryParameters.getColorScheme() == ColorScheme::neuron_by_id
            ? NB_SYSTEM_MATERIALS
            : NO_MATERIAL;
    MeshLoader meshLoader(geometryParameters);
    meshLoader.setProgressCallback(progressUpdate);
    return meshLoader.importMeshFromBlob(_blob, scene, Matrix4f(), material);
}

void LoadDataFunctor::_postLoad(Progress& loadingProgress,
                                const bool cancellable)
{
    Scene& scene = _engine->getScene();

    scene.buildEnvironment();

    const auto& geomParams =
        _engine->getParametersManager().getGeometryParameters();
    loadingProgress.setMessage("Building geometry ...");
    scene.buildGeometry();
    if (geomParams.getLoadCacheFile().empty() &&
        !geomParams.getSaveCacheFile().empty())
    {
        scene.saveToCacheFile();
    }

    if (cancellable)
        cancelCheck();

    loadingProgress += LOADING_PROGRESS_STEP;

    loadingProgress.setMessage("Building acceleration structure ...");
    scene.commit();
    loadingProgress += LOADING_PROGRESS_STEP;

    loadingProgress.setMessage("Done");
    BRAYNS_INFO << "Now rendering ..." << std::endl;

    const auto frameSize = Vector2f(_engine->getFrameBuffer().getSize());

    auto& camera = _engine->getCamera();
    camera.setInitialState(_engine->getScene().getWorldBounds());
    camera.setAspectRatio(frameSize.x() / frameSize.y());
    _engine->triggerRender();
}
}
