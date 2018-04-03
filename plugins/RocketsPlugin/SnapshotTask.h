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

#include "ImageGenerator.h"
#include <brayns/common/camera/Camera.h>
#include <brayns/common/engine/Engine.h>
#include <brayns/common/renderer/FrameBuffer.h>
#include <brayns/common/renderer/Renderer.h>

namespace brayns
{
struct SnapshotParams
{
    int samplesPerPixel{1};
    Vector2ui size;
    std::string format; // ImageMagick formats apply
    size_t quality{100};
};

std::shared_ptr<TaskT<ImageGenerator::ImageBase64>> createSnapshotTask(
    const SnapshotParams& params, Engine& engine,
    ImageGenerator& imageGenerator)
{
    class Func : public TaskFunctor
    {
    public:
        Func(RendererPtr renderer, CameraPtr camera, FrameBufferPtr frameBuffer,
             const SnapshotParams& params, ImageGenerator& imageGenerator)
            : _renderer(renderer)
            , _camera(camera)
            , _frameBuffer(frameBuffer)
            , _params(params)
            , _imageGenerator(imageGenerator)
        {
        }

        ImageGenerator::ImageBase64 operator()()
        {
            while (_frameBuffer->numAccumFrames() !=
                   size_t(_params.samplesPerPixel))
            {
                transwarp_cancel_point();
                _renderer->render(_frameBuffer);
                progress("Render snapshot ...",
                         float(_frameBuffer->numAccumFrames()) /
                             _params.samplesPerPixel);
            }

            progress("Render snapshot ...", 1.f);
            return _imageGenerator.createImage(*_frameBuffer, _params.format,
                                               _params.quality);
        }

    private:
        RendererPtr _renderer;
        CameraPtr _camera;
        FrameBufferPtr _frameBuffer;
        SnapshotParams _params;
        ImageGenerator& _imageGenerator;
    };

    auto frameBuffer =
        engine.createFrameBuffer(params.size, FrameBufferFormat::rgba_i8, true);
    auto camera = engine.createCamera(engine.getCamera().getType());
    *camera = engine.getCamera();
    camera->setAspectRatio(float(params.size.x()) / params.size.y());
    camera->commit();

    auto renderer = engine.createRenderer(engine.getActiveRenderer());
    renderer->setCamera(camera);
    renderer->setScene(engine.getScenePtr());
    renderer->commit();

    return std::make_shared<TaskT<ImageGenerator::ImageBase64>>(
        Func{renderer, camera, frameBuffer, params, imageGenerator});
}
}
