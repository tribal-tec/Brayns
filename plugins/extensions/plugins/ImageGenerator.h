/* Copyright (c) 2015-2017, EPFL/Blue Brain Project
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

#pragma once

#include <brayns/common/types.h>

#ifdef BRAYNS_USE_LIBJPEGTURBO
#include <turbojpeg.h>
#endif

namespace brayns
{
class ImageGenerator
{
public:
    ImageGenerator() = default;

    ~ImageGenerator();

    struct ImageBase64
    {
        std::string data;
    };

    ImageBase64 createImage(FrameBuffer& frameBuffer, const std::string& format,
                            uint8_t quality);

    struct ImageJPEG
    {
#ifdef BRAYNS_USE_LIBJPEGTURBO
        struct tjDeleter
        {
            void operator()(uint8_t* ptr) { tjFree(ptr); }
        };
        using JpegData = std::unique_ptr<uint8_t, tjDeleter>;
#else
        using JpegData = std::unique_ptr<uint8_t>;
#endif
        JpegData data;
        unsigned long size{0};
    };

    ImageJPEG createJPEG(FrameBuffer& frameBuffer, uint8_t quality);

private:
#ifdef BRAYNS_USE_LIBJPEGTURBO
    tjhandle _compressor{tjInitCompress()};

    ImageJPEG::JpegData _encodeJpeg(uint32_t width, uint32_t height,
                                    const uint8_t* rawData, int32_t pixelFormat,
                                    uint8_t quality, unsigned long& dataSize);
#endif
};
}
