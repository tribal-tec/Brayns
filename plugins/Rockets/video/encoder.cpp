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

#include "encoder.h"

#include <brayns/common/log.h>
#include <brayns/engine/FrameBuffer.h>

int custom_io_write(void *opaque, uint8_t *buffer, int32_t buffer_size)
{
    auto encoder = (brayns::Encoder *)opaque;
    encoder->_dataFunc((const char *)buffer, buffer_size);
    return buffer_size;
}

namespace brayns
{
Encoder::Encoder(const int width, const int height, const int fps,
                 const int64_t kbps, const DataFunc &dataFunc)
    : _dataFunc(dataFunc)
    , _width(width)
    , _height(height)
{
    formatContext = avformat_alloc_context();
    formatContext->oformat = av_guess_format("mp4", nullptr, nullptr);
    formatContext->flags = AVFMT_FLAG_CUSTOM_IO;

    const AVCodecID codecID = AV_CODEC_ID_H264;
    codec = avcodec_find_encoder(codecID);
    if (!codec)
        BRAYNS_THROW(
            std::runtime_error(std::string("Could not find encoder for ") +
                               avcodec_get_name(codecID)));

#if 0
    const AVRational avFPS = {fps, 1};

    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext)
        BRAYNS_THROW(std::runtime_error("Could not create codec context"));
 
    // if(avcodec_parameters_to_context(codecContext, stream->codecpar) < 0)
    //      BRAYNS_THROW(std::runtime_error("Could not retrieve codec parameters"));
#else
    if (!(stream = avformat_new_stream(formatContext, codec)))
        BRAYNS_THROW(std::runtime_error("Could not create stream"));

    const AVRational avFPS = {fps, 1};
    stream->avg_frame_rate = avFPS;
    stream->time_base = av_inv_q(avFPS);

    codecContext = stream->codec;
#endif
    codecContext->codec_tag = 0;
    codecContext->codec_id = codecID;
    codecContext->codec_type = AVMEDIA_TYPE_VIDEO;
    codecContext->width = width;
    codecContext->height = height;
    codecContext->gop_size = 0;
    codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    codecContext->framerate = avFPS;
    codecContext->time_base = av_inv_q(avFPS);
    codecContext->bit_rate = kbps;
    codecContext->max_b_frames = 0;
    codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    codecContext->profile = 100;
    codecContext->level = 31;

    av_opt_set(codecContext->priv_data, "crf", "12", 0);
    av_opt_set(codecContext->priv_data, "preset", "ultrafast", 0);
    // av_opt_set(codecContext->priv_data, "profile", "main", 0);
    av_opt_set(codecContext->priv_data, "tune", "zerolatency", 0);

#if 0
    if (!(stream = avformat_new_stream(formatContext, codec)))
        BRAYNS_THROW(std::runtime_error("Could not create stream"));

    stream->avg_frame_rate = avFPS;
    stream->time_base = av_inv_q(avFPS);

    if ( avcodec_parameters_from_context( stream->codecpar, codecContext ) < 0 )
        BRAYNS_THROW(std::runtime_error("Could not get params from context"));
#endif
    if (avcodec_open2(codecContext, codec, NULL) < 0)
        BRAYNS_THROW(std::runtime_error("Could not open video encoder!"));

    int avio_buffer_size = 1 * 1024 * 1024;
    void *avio_buffer = av_malloc(avio_buffer_size);

    AVIOContext *custom_io =
        avio_alloc_context((unsigned char *)avio_buffer, avio_buffer_size, 1,
                           (void *)this, NULL, &custom_io_write, NULL);

    formatContext->pb = custom_io;

    AVDictionary *fmt_opts = NULL;
    av_dict_set(&fmt_opts, "brand", "mp42", 0);
    av_dict_set(&fmt_opts, "movflags", "faststart+frag_keyframe+empty_moov", 0);
    av_dict_set(&fmt_opts, "live", "1", 0);
    if (avformat_write_header(formatContext, &fmt_opts) < 0)
        BRAYNS_THROW(std::runtime_error("Could not write header!"));

    picture.init(codecContext->pix_fmt, width, height);
}

Encoder::~Encoder()
{
    if (formatContext)
    {
        av_write_trailer(formatContext);
        av_free(formatContext->pb);
        avcodec_close(codecContext);
        avformat_free_context(formatContext);
    }
}

void Encoder::encode(FrameBuffer &fb)
{
    sws_context =
        sws_getCachedContext(sws_context, fb.getSize().x, fb.getSize().y,
                             AV_PIX_FMT_RGBA, _width, _height,
                             AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, 0, 0, 0);
    const int stride[] = {4 * (int)fb.getSize().x};
    fb.map();
    auto cdata = reinterpret_cast<const uint8_t *const>(fb.getColorBuffer());
    sws_scale(sws_context, &cdata, stride, 0, fb.getSize().y,
              picture.frame->data, picture.frame->linesize);
    fb.unmap();
    picture.frame->pts = _frameNumber++;

    if (avcodec_send_frame(codecContext, picture.frame) < 0)
        return;

    AVPacket pkt;
    av_init_packet(&pkt);
    const auto ret = avcodec_receive_packet(codecContext, &pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
    {
        av_packet_unref(&pkt);
        return;
    }

    av_packet_rescale_ts(&pkt, codecContext->time_base, stream->time_base);
    pkt.stream_index = this->stream->index;
    av_interleaved_write_frame(formatContext, &pkt);
}
}
