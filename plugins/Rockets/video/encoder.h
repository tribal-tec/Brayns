/* Copyright (c) 2019 EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
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

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include <brayns/common/types.h>

#include <thread>

namespace brayns
{
class Picture
{
public:
    AVFrame *frame{nullptr};

    int init(enum AVPixelFormat pix_fmt, int width, int height)
    {
        frame = av_frame_alloc();
        frame->format = pix_fmt;
        frame->width = width;
        frame->height = height;
        return av_frame_get_buffer(frame, 32);
    }

    ~Picture()
    {
        if (frame)
            av_frame_free(&frame);
    }
};

class Encoder
{
public:
    using DataFunc = std::function<void(const char *data, size_t size)>;

    Encoder(const int width, const int height, const int fps,
            const int64_t kbps, const DataFunc &dataFunc);
    ~Encoder();

    void encode(FrameBuffer &fb);

    DataFunc _dataFunc;
    const int _width;
    const int _height;

private:
    AVFormatContext *formatContext{nullptr};
    AVStream *stream{nullptr};

    AVCodecContext *codecContext{nullptr};
    AVCodec *codec{nullptr};

    SwsContext *sws_context{nullptr};
    Picture picture;

    int64_t _frameNumber{0};

    const bool _async = true;
    std::thread _thread;
    std::atomic_bool _running{true};

    struct Image
    {
        int width{0};
        int height{0};
        std::vector<uint8_t> data;
        bool empty() const { return width == 0 || height == 0; }
    } _image;

    void _runAsync();
    void _encode();

};
}
