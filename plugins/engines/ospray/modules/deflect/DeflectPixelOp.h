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

#pragma once

#include <deflect/Stream.h>
#include <ospray/SDK/fb/PixelOp.h>

#define USE_ALIGNED_MEM

namespace brayns
{
class DeflectPixelOp : public ospray::PixelOp
{
public:
    DeflectPixelOp() {}
    ~DeflectPixelOp() {}
    struct Settings
    {
        bool compression{true};
        unsigned int quality{80};
    };

    struct Instance : public ospray::PixelOp::Instance
    {
        Instance(ospray::FrameBuffer* fb_, deflect::Stream& stream,
                 Settings& settings);

        void beginFrame() final;

        void endFrame() final;

        void postAccum(ospray::Tile& tile) final;

        std::string toString() const final { return "DeflectPixelOp"; }
        deflect::Stream& _deflectStream;

#ifdef USE_ALIGNED_MEM
        struct PixelsDeleter
        {
            void operator()(unsigned char* pixels) { free(pixels); }
        };
        typedef std::unique_ptr<unsigned char, PixelsDeleter> Pixels;

        std::vector<Pixels> _rgbaBuffers;
#else
        std::vector<std::array<unsigned char, TILE_SIZE * TILE_SIZE * 4>>
            _rgbaBuffers;
#endif
        std::vector<deflect::Stream::Future> _futures;
        Settings& _settings;
    };

    void commit() final;

    ospray::PixelOp::Instance* createInstance(ospray::FrameBuffer* fb,
                                              PixelOp::Instance* prev) final;

    std::unique_ptr<deflect::Stream> _deflectStream;
    Settings _settings;
};
}
