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

#include <brayns/io/XYZBLoader.h>

#include <brayns/parameters/ParametersManager.h>

namespace brayns
{
const size_t LOADING_PROGRESS_DATA = 100;
const size_t LOADING_PROGRESS_STEP = 10;

LoadDataFunctor::LoadDataFunctor(const std::string& type, EnginePtr engine)
    : _engine(engine)
    , _meshLoader(engine->getParametersManager().getGeometryParameters())
{
    _blob.type = type;
}

void LoadDataFunctor::operator()(const std::string& data)
{
    // fix race condition: we have to wait until rendering is finished
    std::lock_guard<std::mutex> lock{_engine->dataMutex()};

    _blob.cancelCheck = std::bind(&LoadDataFunctor::cancelCheck, this);
    _blob.progressFunc = _progressFunc;
    _blob.data = std::move(data);

    Progress loadingProgress(
        "Loading scene ...",
        (_blob.data.empty() ? 0 : LOADING_PROGRESS_DATA +
                                      3 * LOADING_PROGRESS_STEP) +
            LOADING_PROGRESS_DATA + 3 * LOADING_PROGRESS_STEP,
        [this](const std::string& msg, const float progress) {
            //            if (_blob.data.empty())
            //            {
            //                std::lock_guard<std::mutex> lock_(
            //                    _engine->getProgress().mutex);
            //                _engine->setLastOperation(msg);
            //                _engine->setLastProgress(progress);
            //            }
            //            else
            {
                _blob.progressFunc(msg, progress);
            }
        });

    if (!_blob.data.empty())
        loadingProgress += LOADING_PROGRESS_DATA + 3 * LOADING_PROGRESS_STEP;

    Scene& scene = _engine->getScene();

    loadingProgress.setMessage("Unloading ...");
    scene.unload();
    loadingProgress += LOADING_PROGRESS_STEP;

    loadingProgress.setMessage("Loading data ...");
    _meshLoader.clear();

    scene.resetMaterials();
    const bool success = _loadData(loadingProgress);

    if (!success || (scene.empty() && !scene.getVolumeHandler()))
    {
        scene.unload();
        _meshLoader.clear();
        BRAYNS_INFO << "Building default scene" << std::endl;
        scene.buildDefault();
    }

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

    if (!success)
        throw LOADING_BINARY_FAILED(_blob.error);
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

    if (!_blob.data.empty())
        return _loadDataFromBlob(updateProgress);

    return false;

    //    auto& geometryParameters = _parametersManager.getGeometryParameters();
    //    auto& volumeParameters = _parametersManager.getVolumeParameters();
    //    auto& sceneParameters = _parametersManager.getSceneParameters();
    //    auto& scene = _engine->getScene();

    //    // set environment map if applicable
    //    const std::string& environmentMap =
    //        _parametersManager.getSceneParameters().getEnvironmentMap();
    //    if (!environmentMap.empty())
    //    {
    //        const size_t materialId =
    //        static_cast<size_t>(MaterialType::skybox);
    //        auto& material = scene.getMaterials()[materialId];
    //        material.setTexture(TT_DIFFUSE, environmentMap);
    //        material.setType(MaterialType::skybox);
    //    }

    //    const std::string& colorMapFilename =
    //        sceneParameters.getColorMapFilename();
    //    if (!colorMapFilename.empty())
    //    {
    //        TransferFunctionLoader transferFunctionLoader;
    //        transferFunctionLoader.loadFromFile(
    //            colorMapFilename, sceneParameters.getColorMapRange(), scene);
    //    }

    //    if (!geometryParameters.getLoadCacheFile().empty())
    //    {
    //        scene.loadFromCacheFile();
    //        loadingProgress += tic;
    //    }

    //    if (!geometryParameters.getPDBFile().empty())
    //    {
    //        _loadPDBFile(geometryParameters.getPDBFile());
    //        loadingProgress += tic;
    //    }

    //    if (!geometryParameters.getPDBFolder().empty())
    //        _loadPDBFolder(updateProgress);

    //    if (!geometryParameters.getSplashSceneFolder().empty())
    //        _loadMeshFolder(geometryParameters.getSplashSceneFolder(),
    //                        updateProgress);

    //    if (!geometryParameters.getMeshFolder().empty())
    //        _loadMeshFolder(geometryParameters.getMeshFolder(),
    //        updateProgress);

    //    if (!geometryParameters.getMeshFile().empty())
    //    {
    //        _loadMeshFile(geometryParameters.getMeshFile());
    //        loadingProgress += tic;
    //    }

    //#if (BRAYNS_USE_BRION)
    //    if (!geometryParameters.getSceneFile().empty())
    //        _loadSceneFile(geometryParameters.getSceneFile(), updateProgress);

    //    if (!geometryParameters.getNESTCircuit().empty())
    //    {
    //        _loadNESTCircuit();
    //        loadingProgress += tic;
    //    }

    //    if (!geometryParameters.getMorphologyFolder().empty())
    //        _loadMorphologyFolder(updateProgress);

    //    if (!geometryParameters.getCircuitConfiguration().empty() &&
    //        geometryParameters.getConnectivityFile().empty())
    //        _loadCircuitConfiguration(updateProgress);

    //    if (!geometryParameters.getConnectivityFile().empty())
    //        _loadConnectivityFile();
    //#endif

    //    if (!geometryParameters.getXYZBFile().empty())
    //    {
    //        _loadXYZBFile(updateProgress);
    //        loadingProgress += tic;
    //    }

    //    if (!geometryParameters.getMolecularSystemConfig().empty())
    //        _loadMolecularSystem(updateProgress);

    //    if (scene.getVolumeHandler())
    //    {
    //        scene.commitTransferFunctionData();
    //        scene.getVolumeHandler()->setCurrentIndex(0);
    //        const Vector3ui& volumeDimensions =
    //            scene.getVolumeHandler()->getDimensions();
    //        const Vector3f& volumeOffset =
    //            scene.getVolumeHandler()->getOffset();
    //        const Vector3f& volumeElementSpacing =
    //            volumeParameters.getElementSpacing();
    //        Boxf& worldBounds = scene.getWorldBounds();
    //        worldBounds.merge(Vector3f(0.f, 0.f, 0.f));
    //        worldBounds.merge(volumeOffset +
    //                          Vector3f(volumeDimensions) *
    //                              volumeElementSpacing);
    //    }
    //    return true;
}

bool LoadDataFunctor::_loadDataFromBlob(
    const Progress::UpdateCallback& updateProgress)
{
    // for unit tests
    if (_blob.type == "forever")
    {
        for (;;)
        {
            _blob.cancelCheck();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
    _meshLoader.setProgressCallback(progressUpdate);
    return _meshLoader.importMeshFromBlob(_blob, scene, Matrix4f(), material);
}
}
