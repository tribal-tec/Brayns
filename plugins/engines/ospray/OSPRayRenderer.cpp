/* Copyright (c) 2015-2016, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
 * Responsible Author: Cyrille Favreau <cyrille.favreau@epfl.ch>
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

#include <brayns/common/log.h>

#include "OSPRayCamera.h"
#include "OSPRayFrameBuffer.h"
#include "OSPRayMaterial.h"
#include "OSPRayRenderer.h"
#include "OSPRayScene.h"

namespace brayns
{
OSPRayRenderer::OSPRayRenderer(const AnimationParameters& animationParameters,
                               const RenderingParameters& renderingParameters)
    : Renderer(animationParameters, renderingParameters)
{
}

OSPRayRenderer::~OSPRayRenderer()
{
    ospRelease(_renderer);
}

void OSPRayRenderer::render(FrameBufferPtr frameBuffer)
{
    auto osprayFrameBuffer =
        std::static_pointer_cast<OSPRayFrameBuffer>(frameBuffer);
    osprayFrameBuffer->lock();

    _variance = ospRenderFrame(osprayFrameBuffer->impl(), _renderer,
                               OSP_FB_COLOR | OSP_FB_DEPTH | OSP_FB_ACCUM);

    osprayFrameBuffer->incrementAccumFrames();
    osprayFrameBuffer->markModified();
    osprayFrameBuffer->unlock();
}

#define SET_SCALAR(OSP, TYPE) \
    ospSet1##OSP(_renderer, prop->name.c_str(), prop->get<TYPE>());
#define SET_STRING()                            \
    ospSetString(_renderer, prop->name.c_str(), \
                 prop->get<std::string>().c_str());
#define SET_ARRAY(OSP, TYPE, NUM)              \
    ospSet##OSP(_renderer, prop->name.c_str(), \
                prop->get<std::array<TYPE, NUM>>().data());

void OSPRayRenderer::commit()
{
    const AnimationParameters& ap = _animationParameters;
    const RenderingParameters& rp = _renderingParameters;

    if (!ap.isModified() && !rp.isModified() && !_scene->isModified() &&
        !isModified())
    {
        return;
    }

    if (_currentOSPRenderer != getCurrentType())
        createOSPRenderer();

    try
    {
        for (const auto& prop : getProperties(getCurrentType()))
        {
            switch (prop->type)
            {
            case PropertyMap::Property::Type::Float:
                SET_SCALAR(f, float);
                break;
            case PropertyMap::Property::Type::Int:
                SET_SCALAR(i, int32_t);
                break;
            case PropertyMap::Property::Type::Bool:
                SET_SCALAR(i, bool);
                break;
            case PropertyMap::Property::Type::String:
                SET_STRING();
                break;
            case PropertyMap::Property::Type::Vec2f:
                SET_ARRAY(2fv, float, 2);
                break;
            case PropertyMap::Property::Type::Vec2i:
                SET_ARRAY(2iv, int32_t, 2);
                break;
            case PropertyMap::Property::Type::Vec3f:
                SET_ARRAY(3fv, float, 3);
                break;
            case PropertyMap::Property::Type::Vec3i:
                SET_ARRAY(3iv, int32_t, 3);
                break;
            case PropertyMap::Property::Type::Vec4f:
                SET_ARRAY(4fv, float, 4);
                break;
            }
        }
    }
    catch (const std::exception& e)
    {
        BRAYNS_ERROR << "Failed to apply properties for renderer "
                     << getCurrentType() << std::endl;
    }

    ospSet1f(_renderer, "timestamp", ap.getFrame());
    ospSet1i(_renderer, "randomNumber", rand() % 10000);

    Vector3f color = rp.getBackgroundColor();
    ospSet3f(_renderer, "bgColor", color.x(), color.y(), color.z());
    ospSet1f(_renderer, "varianceThreshold", rp.getVarianceThreshold());
    ospSet1i(_renderer, "spp", rp.getSamplesPerPixel());

    auto scene = std::static_pointer_cast<OSPRayScene>(_scene);
    auto bgMaterial = std::static_pointer_cast<OSPRayMaterial>(
        scene->getBackgroundMaterial());
    if (bgMaterial)
    {
        bgMaterial->setDiffuseColor(rp.getBackgroundColor());
        bgMaterial->commit();
        auto ospBgMaterial = bgMaterial->getOSPMaterial();
        ospSetObject(_renderer, "bgMaterial", ospBgMaterial);
    }

    ospSetObject(_renderer, "world", scene->getModel());
    ospSetObject(_renderer, "simulationModel", scene->simulationModelImpl());
    ospCommit(_renderer);
}

void OSPRayRenderer::setCamera(CameraPtr camera)
{
    _camera = static_cast<OSPRayCamera*>(camera.get());
    assert(_camera);
    ospSetObject(_renderer, "camera", _camera->impl());
    markModified();
}

Renderer::PickResult OSPRayRenderer::pick(const Vector2f& pickPos)
{
    OSPPickResult ospResult;
    osp::vec2f pos{pickPos.x(), pickPos.y()};

    // HACK: as the time for picking is set to 0.5 and interpolated in a
    // (default) 0..0 range, the ray.time will be 0. So all geometries that have
    // a time > 0 (like branches that have distance to the soma for the growing
    // use-case), cannot be picked. So we make the range as large as possible to
    // make ray.time be as large as possible.
    ospSet1f(_camera->impl(), "shutterClose", INFINITY);
    ospCommit(_camera->impl());

    ospPick(&ospResult, _renderer, pos);

    // UNDO HACK
    ospSet1f(_camera->impl(), "shutterClose", 0.f);
    ospCommit(_camera->impl());

    PickResult result;
    result.hit = ospResult.hit;
    if (result.hit)
        result.pos = {ospResult.position.x, ospResult.position.y,
                      ospResult.position.z};
    return result;
}

void OSPRayRenderer::createOSPRenderer()
{
    auto newRenderer = ospNewRenderer(getCurrentType().c_str());
    if (!newRenderer)
    {
        BRAYNS_ERROR << getCurrentType() << " is not a registered renderer"
                     << std::endl;
        return;
    }
    if (_renderer)
        ospRelease(_renderer);
    _renderer = newRenderer;
    if (_camera)
        ospSetObject(_renderer, "camera", _camera->impl());
    _currentOSPRenderer = getCurrentType();
}
}
