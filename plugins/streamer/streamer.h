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
    int dst_width{0};
    int dst_height{0};
    int fps{0};
    int bitrate{0};
    std::string profile;
};

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

struct Image
{
    std::vector<char> data;
    brayns::Vector2ui size;
    brayns::FrameBufferFormat format;
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
    void _runCopyLoop();
    void _runLoop();
    void stream_frame();

    AVFormatContext *format_ctx{nullptr};
    AVCodec *out_codec{nullptr};
    AVStream *out_stream{nullptr};
    AVCodecContext *out_codec_ctx{nullptr};
    AVPacket *pkt{nullptr};
    SwsContext *sws_context{nullptr};
    Picture picture;

    StreamerConfig config;

    brayns::Timer _timer;
    float _leftover{0.f};

    Image image;
    std::thread _copyThread;
    std::thread _sendThread;
    lunchbox::Monitor<int> _rgbas;
    lunchbox::Monitor<int> _pkts;

    const brayns::PropertyMap _props;
};

} // namespace streamer
#endif
