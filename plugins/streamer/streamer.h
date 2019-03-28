#ifndef STREAMER_HPP
#define STREAMER_HPP

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <string>

#include <brayns/common/PropertyMap.h>
#include <brayns/common/Timer.h>
#include <brayns/common/types.h>
#include <brayns/pluginapi/ExtensionPlugin.h>

#include <lunchbox/monitor.h>
#include <thread>

namespace streamer
{
struct StreamerConfig
{
    int dst_width;
    int dst_height;
    int fps;
    int bitrate;
    std::string profile;

    StreamerConfig()
    {
        dst_width = 0;
        dst_height = 0;
        fps = 0;
        bitrate = 0;
    }

    StreamerConfig(int stream_width, int stream_height, int stream_fps,
                   int stream_bitrate, const std::string &stream_profile)
    {
        dst_width = stream_width;
        dst_height = stream_height;
        fps = stream_fps;
        bitrate = stream_bitrate;
        profile = stream_profile;
    }
};

class Picture
{
    static const int align_frame_buffer = 32;

public:
    AVFrame *frame;
    uint8_t *data;

    int init(enum AVPixelFormat pix_fmt, int width, int height)
    {
        frame = nullptr;
        data = nullptr;
        frame = av_frame_alloc();

        int sz = av_image_get_buffer_size(pix_fmt, width, height,
                                          align_frame_buffer);
        int ret = posix_memalign(reinterpret_cast<void **>(&data),
                                 align_frame_buffer, sz);

        av_image_fill_arrays(frame->data, frame->linesize, data, pix_fmt, width,
                             height, align_frame_buffer);
        frame->format = pix_fmt;
        frame->width = width;
        frame->height = height;

        return ret;
    }

    Picture()
    {
        frame = nullptr;
        data = nullptr;
    }

    ~Picture()
    {
        if (data)
        {
            free(data);
            data = nullptr;
        }

        if (frame)
        {
            av_frame_free(&frame);
        }
    }
};

class Streamer : public brayns::ExtensionPlugin
{
public:
    Streamer(const brayns::PropertyMap &props);
    ~Streamer();
    void init() final;
    void postRender() final;

private:
    bool init(const StreamerConfig &streamer_config);
    void cleanup();
    void _runLoop();
    void stream_frame();

    AVFormatContext *format_ctx;
    AVCodec *out_codec;
    AVStream *out_stream;
    AVCodecContext *out_codec_ctx;
    SwsContext *sws_context{nullptr};
    Picture picture;

    double inv_stream_timebase;
    StreamerConfig config;

    brayns::Timer _timer;
    float _leftover{0.f};

    std::thread thread;
    lunchbox::Monitor<int> _pkts;

    const brayns::PropertyMap _props;
};

} // namespace streamer
#endif
