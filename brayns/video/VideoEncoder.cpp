/* Copyright (context) 2015-2017, EPFL/Blue Brain Project
 *
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

#include "VideoEncoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>

#include <libswscale/swscale.h>
}

#include <iostream>

namespace brayns
{
class VideoEncoder::Impl
{
public:
    Impl()
    {
        avcodec_register_all();
        av_register_all();
        avformat_network_init();

        const AVCodecID codec_id = AV_CODEC_ID_H264;
        const int width = 1920;
        const int height = 1080;

        codec = avcodec_find_encoder(codec_id);
        context = avcodec_alloc_context3(codec);

        context->bit_rate = 400000;
        context->width = width;
        context->height = height;
        context->time_base.num = 1;
        context->time_base.den = 25;
        context->gop_size = 25;
        context->max_b_frames = 1;
        context->pix_fmt = AV_PIX_FMT_YUV420P;
        context->codec_type = AVMEDIA_TYPE_VIDEO;
        // context->flags = CODEC_FLAG_GLOBAL_HEADER;

        if (codec_id == AV_CODEC_ID_H264)
        {
            av_opt_set(context->priv_data, "preset", "ultrafast", 0);
            av_opt_set(context->priv_data, "tune", "zerolatency", 0);
        }

        avcodec_open2(context, codec, NULL);

        frame = av_frame_alloc();
        frame->format = context->pix_fmt;
        frame->width = context->width;
        frame->height = context->height;
        av_image_alloc(frame->data, frame->linesize, context->width,
                       context->height, context->pix_fmt, 32);

        _setupStream();
    }

    ~Impl()
    {
        //        // end
        //        avcodec_send_frame(context, NULL);

        //        /* get the delayed frames */
        //        for(;;)
        //        {
        //            bool got_output = false;

        //            fflush(stdout);
        //            AVPacket pkt;
        //            int ret = avcodec_receive_packet(c, &pkt);
        //            if (ret == AVERROR_EOF) {
        //                printf("Stream EOF\n");
        //                break;
        //            } else if (ret == AVERROR(EAGAIN)) {
        //                printf("Stream EAGAIN\n");
        //                got_output = false;
        //            } else {
        //                got_output = true;
        //            }

        //            if (got_output) {
        //                printf("Write frame %3d (size=%5d)\n", outputFrame++,
        //                pkt.size);
        //                av_interleaved_write_frame(avfctx, &pkt);
        //                av_packet_unref(&pkt);
        //            }
        //        }

        avformat_free_context(avfctx);
        avcodec_close(context);
        av_free(context);
        av_freep(&frame->data[0]);
        av_frame_free(&frame);
    }

    void encode(const uint8_t* rgba)
    {
        if (!rgba)
            throw std::invalid_argument("what the fuck");

// we could use threads here for sendframe and receivepacket, but hey...
#ifdef FFMPEG_3_1
        AVPacket* pkt = av_packet_alloc();
        int ret;

        ret = avcodec_send_frame(context, frame);
        if (ret >= 0)
        {
            while (!ret)
            {
                ret = avcodec_receive_packet(context, pkt);
                if (!ret)
                    ret = cb(priv, pkt);
            }
        }

        av_packet_free(&pkt);
//        if (ret == AVERROR(EAGAIN))
//            return 0;
//        return ret;
#else
        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.data = NULL; // packet data will be allocated by the encoder
        pkt.size = 0;
        fflush(stdout);

        // rgba to yuv
        {
            int in_linesize[1] = {4 * context->width};
            sws_context = sws_getCachedContext(sws_context, context->width,
                                               context->height, AV_PIX_FMT_RGBA,
                                               context->width, context->height,
                                               AV_PIX_FMT_YUV420P, 0, 0, 0, 0);

            sws_scale(sws_context, (const uint8_t* const*)&rgba, in_linesize, 0,
                      context->height, frame->data, frame->linesize);
        }
        frame->pts = currentFrame++;

        /* encode the image */
        // avcodec_send_frame(context, frame);
        // avcodec_receive_packet(context, &pkt);

        int got_output = false;
        int ret = avcodec_encode_video2(context, &pkt, frame, &got_output);
        if (ret < 0)
        {
            std::cerr << "video encode error " << ret << std::endl;
            return;
        }

        //        if (ret == AVERROR_EOF) {
        //            got_output = false;
        //            printf("Stream EOF\n");
        //        } else if(ret == AVERROR(EAGAIN)) {
        //            got_output = false;
        //            printf("Stream EAGAIN\n");
        //        } else {
        //            got_output = true;
        //        }

        if (got_output)
        {
            // printf("Write frame %3d (size=%5d)\n", outputFrame++, pkt.size);
            av_interleaved_write_frame(avfctx, &pkt);
            av_packet_unref(&pkt);
        }
#endif
    }

private:
    AVCodec* codec;
    AVCodecContext* context;
    AVFrame* frame;
    AVFormatContext* avfctx;
    size_t currentFrame{0};
    // size_t outputFrame{0};
    SwsContext* sws_context{nullptr};

    void _setupStream()
    {
        AVOutputFormat* fmt = av_guess_format("rtp", NULL, NULL);

        /*int ret = */ avformat_alloc_output_context2(&avfctx, fmt, "h264",
                                                      "rtp://127.0.0.1:49990");

        printf("Writing to %s\n", avfctx->filename);

        avio_open(&avfctx->pb, avfctx->filename, AVIO_FLAG_WRITE);

        // avfctx->bit_rate = 400000;
        // avfctx->video_codec_id = AV_CODEC_ID_H264;
        // avfctx->video_codec

        struct AVStream* stream = avformat_new_stream(avfctx, codec);
        stream->codec->bit_rate = 400000;
        stream->codec->width = context->width;
        stream->codec->height = context->height;
        //        stream->codecpar->codec_id = AV_CODEC_ID_H264;
        //        stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        stream->time_base.num = 1;
        stream->time_base.den = 25;

        avformat_write_header(avfctx, NULL);
        char buf[200000];
        AVFormatContext* ac[] = {avfctx};
        av_sdp_create(ac, 1, buf, 20000);

        printf("sdp:\n%s\n", buf);
        FILE* fsdp = fopen("/tmp/test.sdp", "w");
        if (fsdp)
        {
            fprintf(fsdp, "%s", buf);
            fclose(fsdp);
        }
    }
};

VideoEncoder::VideoEncoder()
    : _impl(new Impl)
{
}

VideoEncoder::~VideoEncoder()
{
}

void VideoEncoder::encode(const uint8_t* rgba)
{
    _impl->encode(rgba);
}
}
