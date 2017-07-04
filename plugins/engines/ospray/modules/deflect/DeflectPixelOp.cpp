/* Copyright (c) 2017, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
 * Responsible Author: Daniel Nachbaur <daniel.nachbaur@epfl.ch>
 *
 * This file is part of DeflectPixelOp
 * <https://github.com/BlueBrain/DeflectPixelOp>
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

#pragma omp declare simd
inline unsigned char clampCvt(float f)
{
    if (f < 0.f)
        f = 0.f;
    if (f > 1.f)
        f = 1.f;
    return f * 255.f;
}
}

namespace bbp
{
DeflectPixelOp::Instance::Instance(ospray::FrameBuffer* fb_,
                                   deflect::Stream& stream, Settings& settings)
    : _deflectStream(stream)
    , _settings(settings)
{
    fb = fb_;
    fb->pixelOp = this;
}

void DeflectPixelOp::Instance::beginFrame()
{
    if (!_settings.streamEnabled)
    {
        fb->pixelOp = nullptr;
        return;
    }

    const size_t numTiles = fb->getTotalTiles();

    if (_sendFutures.size() < numTiles)
    {
        _sendFutures.reserve(numTiles);
        for (size_t i = 0; i < numTiles; ++i)
            _sendFutures.emplace_back(make_ready_future(true));
    }

    if (_pixels.size() < numTiles)
    {
        _pixels.resize(numTiles);

        for (auto& i : _pixels)
        {
            if (i)
                continue;

            void* ptr;
            if (posix_memalign(&ptr, 32, TILE_SIZE * TILE_SIZE * 4))
            {
                std::cerr << "Tile pixels memalign failed" << std::endl;
                ptr = calloc(TILE_SIZE * TILE_SIZE * 4, sizeof(char));
                if (!ptr)
                    throw std::bad_alloc();
            }
            memset(ptr, 255, TILE_SIZE * TILE_SIZE * 4);
            i.reset((unsigned char*)ptr);
        }
    }
}

void DeflectPixelOp::Instance::endFrame()
{
    if (!_settings.streamEnabled)
        return;
    auto sharedFuture = _deflectStream.finishFrame().share();
    for (auto& i : _finishFutures)
        i.second = sharedFuture;
}

void DeflectPixelOp::Instance::postAccum(ospray::Tile& tile)
{
    if (!_settings.streamEnabled)
        return;

    const size_t tileID =
        tile.region.lower.y / TILE_SIZE * fb->getNumTiles().x +
        tile.region.lower.x / TILE_SIZE;

#ifdef __GNUC__
    unsigned char* __restrict__ pixels =
        (unsigned char*)__builtin_assume_aligned(_pixels[tileID].get(), 32);
#else
    unsigned char* __restrict__ pixels = _pixels[tileID].get();
#endif
    float* __restrict__ red = tile.r;
    float* __restrict__ green = tile.g;
    float* __restrict__ blue = tile.b;

#ifdef __INTEL_COMPILER
#pragma vector aligned
#endif
#pragma omp simd
    for (int i = 0; i < TILE_SIZE * TILE_SIZE; ++i)
    {
        pixels[i * 4 + 0] = clampCvt(red[i]);
        pixels[i * 4 + 1] = clampCvt(green[i]);
        pixels[i * 4 + 2] = clampCvt(blue[i]);
    }

    deflect::ImageWrapper image(pixels, TILE_SIZE, TILE_SIZE, deflect::RGBA,
                                tile.region.lower.x, tile.region.lower.y);
    image.compressionPolicy = _settings.compression ? deflect::COMPRESSION_ON
                                                    : deflect::COMPRESSION_OFF;
    image.compressionQuality = _settings.quality;
    image.subsampling = deflect::ChromaSubsampling::YUV420;

    auto i = _finishFutures.find(pthread_self());
    if (i == _finishFutures.end())
    {
        // only for the first frame
        std::lock_guard<std::mutex> _lock(_mutex);
        _finishFutures.insert({pthread_self(), make_ready_future(true)});
    }
    else
        i->second.wait(); // complete previous frame

    _sendFutures[tileID] = _deflectStream.send(image);
}

void DeflectPixelOp::commit()
{
    if (!_deflectStream)
    {
        try
        {
            _deflectStream.reset(new deflect::Stream);
        }
        catch (const std::runtime_error& ex)
        {
            std::cerr << "Deflect failed to initialize. " << ex.what()
                      << std::endl;
        }
    }
    _settings.compression = getParam1i("compression", 1);
    _settings.quality = getParam1i("quality", 80);
    _settings.streamEnabled = _deflectStream && _deflectStream->isConnected();
}

ospray::PixelOp::Instance* DeflectPixelOp::createInstance(
    ospray::FrameBuffer* fb, PixelOp::Instance* /*prev*/)
{
    if (_deflectStream && _deflectStream->isConnected())
        return new Instance(fb, *_deflectStream, _settings);
    return nullptr;
}

} // namespace bbp

namespace ospray
{
OSP_REGISTER_PIXEL_OP(bbp::DeflectPixelOp, DeflectPixelOp);
}
