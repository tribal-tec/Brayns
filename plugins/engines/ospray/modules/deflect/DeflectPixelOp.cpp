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

namespace
{
template <typename T>
std::future<T> make_ready_future(const T value)
{
    std::promise<T> promise;
    promise.set_value(value);
    return promise.get_future();
}
}

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
    const size_t numTiles = fb->getNumTiles().x * fb->getNumTiles().y;

    if (_futures.size() < numTiles)
    {
        _futures.reserve(numTiles);
        for (size_t i = 0; i < numTiles; ++i)
            _futures.emplace_back(make_ready_future(true));
        //_finishFuture = make_ready_future(true);
    }
#ifdef USE_ALIGNED_MEM
    if (_rgbaBuffers.size() < numTiles)
    {
        _rgbaBuffers.resize(numTiles);

        for (auto& i : _rgbaBuffers)
        {
            if (i)
                continue;

            void* ptr;
            if (posix_memalign(&ptr, 32, TILE_SIZE * TILE_SIZE * 4))
            {
                ptr = calloc(TILE_SIZE * TILE_SIZE * 4, sizeof(char));
                if (!ptr)
                    throw std::bad_alloc();
            }
            i.reset((unsigned char*)ptr);
        }
    }
#else
    if (_rgbaBuffers.size() < numTiles)
        _rgbaBuffers.resize(numTiles);
#endif
}

void DeflectPixelOp::Instance::endFrame()
{
    auto fut = _deflectStream.finishFrame().share();
    for (auto& i : _finishFuture)
        i.second = fut;
    //_finishFuture = _deflectStream.finishFrame().share();
}

inline unsigned char clampCvt(const float f)
{
    return 255.f * std::max(0.0f, std::min(f, 1.0f));
}

void DeflectPixelOp::Instance::postAccum(ospray::Tile& tile)
{
    const size_t tileID =
        tile.region.lower.y / TILE_SIZE * fb->getNumTiles().x +
        tile.region.lower.x / TILE_SIZE;

#ifdef USE_ALIGNED_MEM
    auto pixels = _rgbaBuffers[tileID].get();
#else
    auto pixels = _rgbaBuffers[tileID].data();
#endif
#pragma vector aligned
#pragma ivdep
    for (size_t i = 0; i < TILE_SIZE * TILE_SIZE; ++i)
    {
        pixels[i * 4 + 0] = clampCvt(tile.r[i]);
        pixels[i * 4 + 1] = clampCvt(tile.g[i]);
        pixels[i * 4 + 2] = clampCvt(tile.b[i]);
        pixels[i * 4 + 3] = clampCvt(tile.a[i]);
    }

    deflect::ImageWrapper image(pixels, TILE_SIZE, TILE_SIZE, deflect::RGBA,
                                tile.region.lower.x, tile.region.lower.y);
    image.compressionPolicy = _settings.compression ? deflect::COMPRESSION_ON
                                                    : deflect::COMPRESSION_OFF;
    image.compressionQuality = _settings.quality;
    image.subsampling = deflect::ChromaSubsampling::YUV420;
    auto i = _finishFuture.find(pthread_self());
    if (i == _finishFuture.end())
        _finishFuture.insert({pthread_self(), make_ready_future(true)});
    else
        i->second.wait();
    //_finishFuture[pthread_self()].wait(); // finish previous frame
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
