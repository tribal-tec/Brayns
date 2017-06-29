/* Copyright (c) 2015-2017, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
 * Responsible Author: Daniel Nachbaur <daniel.nachbaur@epfl.ch>
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

#include "DeflectPixelOp.h"

#include <ospray/SDK/fb/FrameBuffer.h>

namespace brayns
{
DeflectPixelOp::Instance::Instance(ospray::FrameBuffer* fb_,
                                   deflect::Stream& stream, Settings& settings)
    : _deflectStream(stream)
    , _settings(settings)
{
    fb_->pixelOp = this;
    fb = fb_;
}

void DeflectPixelOp::Instance::beginFrame()
{
    for (auto& future : _futures)
        future.get();

    const size_t numTiles = fb->getNumTiles().x * fb->getNumTiles().y;

    if (_futures.size() < numTiles + 1)
        _futures.resize(numTiles + 1);
    if (_rgbBuffers.size() < numTiles)
        _rgbBuffers.resize(numTiles);
    if (_rgbaBuffers.size() < numTiles)
        _rgbaBuffers.resize(numTiles);
}

void DeflectPixelOp::Instance::endFrame()
{
    _futures[_futures.size() - 1] = _deflectStream.finishFrame();
}

void DeflectPixelOp::Instance::postAccum(ospray::Tile& tile)
{
    const size_t tileID =
        tile.region.lower.y / TILE_SIZE * fb->getNumTiles().x +
        tile.region.lower.x / TILE_SIZE;

    const void* pixelData;
    if (_settings.compression)
    {
        auto& pixels = _rgbBuffers[tileID];
        for (size_t i = 0; i < TILE_SIZE * TILE_SIZE; ++i)
        {
            pixels[i * 3 + 0] = std::min(255, int(255.f * tile.r[i]));
            pixels[i * 3 + 1] = std::min(255, int(255.f * tile.g[i]));
            pixels[i * 3 + 2] = std::min(255, int(255.f * tile.b[i]));
        }
        pixelData = pixels.data();
    }
    else
    {
        auto& pixels = _rgbaBuffers[tileID];
        for (size_t i = 0; i < TILE_SIZE * TILE_SIZE; ++i)
        {
            pixels[i * 4 + 0] = std::min(255, int(255.f * tile.r[i]));
            pixels[i * 4 + 1] = std::min(255, int(255.f * tile.g[i]));
            pixels[i * 4 + 2] = std::min(255, int(255.f * tile.b[i]));
            pixels[i * 4 + 3] = std::min(255, int(255.f * tile.a[i]));
        }
        pixelData = pixels.data();
    }

    deflect::ImageWrapper image(pixelData, TILE_SIZE, TILE_SIZE,
                                _settings.compression ? deflect::RGB
                                                      : deflect::RGBA,
                                tile.region.lower.x, tile.region.lower.y);
    image.compressionPolicy = _settings.compression ? deflect::COMPRESSION_ON
                                                    : deflect::COMPRESSION_OFF;
    image.compressionQuality = _settings.quality;
    image.subsampling = deflect::ChromaSubsampling::YUV420;
    _futures[tileID] = _deflectStream.send(image);
}

void DeflectPixelOp::commit()
{
    if (!_deflectStream || !_deflectStream->isConnected())
    {
        try
        {
            _deflectStream.reset(new deflect::Stream);
        }
        catch (const std::runtime_error& ex)
        {
            std::cout << "Deflect failed to initialize. " << ex.what()
                      << std::endl;
        }
    }
    _settings.compression = getParam1i("compression", 1);
    _settings.quality = getParam1i("quality", 80);
}

ospray::PixelOp::Instance* DeflectPixelOp::createInstance(
    ospray::FrameBuffer* fb, PixelOp::Instance* /*prev*/)
{
    if (_deflectStream && _deflectStream->isConnected())
        return new Instance(fb, *_deflectStream, _settings);
    return nullptr;
}

} // namespace brayns

namespace ospray
{
OSP_REGISTER_PIXEL_OP(brayns::DeflectPixelOp, DeflectPixelOp);
}
