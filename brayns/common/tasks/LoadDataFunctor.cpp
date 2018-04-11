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

LoadDataFunctor::LoadDataFunctor(EnginePtr engine)
    : _engine(engine)
{
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

void LoadDataFunctor::operator()(Blob&& blob)
{
    // fix race condition: we need exclusive access to the scene as we unload
    // the current one. So no rendering & snapshot must occur.
    std::unique_lock<std::shared_timed_mutex> lock{_engine->dataMutex(),
                                                   std::defer_lock};
    while (!lock.try_lock_for(std::chrono::seconds(1)))
        cancelCheck();

    Progress loadingProgress("Loading scene ...",
                             LOADING_PROGRESS_DATA + 3 * LOADING_PROGRESS_STEP,
                             [& func = _progressFunc](const std::string& msg,
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
    try
    {
        _loadData(std::move(blob), loadingProgress);
    }
    catch (const std::exception& e)
    {
        throw LOADING_BINARY_FAILED(e.what());
    }

    if (scene.empty() && !scene.getVolumeHandler())
    {
        scene.unload();
        BRAYNS_INFO << "Building default scene" << std::endl;
        scene.buildDefault();
    }

    _postLoad(loadingProgress);
    _empty = false;
}

void LoadDataFunctor::_loadData(Blob&& blob, Progress& loadingProgress)
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
    if (blob.type == "forever")
    {
        for (;;)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            cancelCheck();
        }
        return;
    }

    if (blob.type == "xyz")
        _loadXYZBBlob(std::move(blob), updateProgress);
    else
        _loadMeshBlob(std::move(blob), updateProgress);
}

void LoadDataFunctor::_loadXYZBBlob(
    Blob&& blob, const Progress::UpdateCallback& progressUpdate)
{
    auto& geometryParameters =
        _engine->getParametersManager().getGeometryParameters();
    auto& scene = _engine->getScene();
    XYZBLoader xyzbLoader(geometryParameters);
    xyzbLoader.setProgressCallback(progressUpdate);
    xyzbLoader.setCancelCheck(std::bind(&LoadDataFunctor::cancelCheck, this));
    xyzbLoader.importFromBlob(blob, scene);
}

void LoadDataFunctor::_loadMeshBlob(
    Blob&& blob, const Progress::UpdateCallback& progressUpdate)
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
    meshLoader.setCancelCheck(std::bind(&LoadDataFunctor::cancelCheck, this));
    meshLoader.importMeshFromBlob(blob, scene, Matrix4f(), material);
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
