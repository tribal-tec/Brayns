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

#include <ospray/mpiCommon/MPIBcastFabric.h>

#ifdef USE_NVPIPE
#include <NvPipe.h>
#endif

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
    void preRender() final;
    void postRender() final;

private:
    bool init(const StreamerConfig &streamer_config);
    void cleanup();
    void _runCopyLoop();
    void _runLoop();
    void encodeFrame(const int width, const int height,
                     const uint8_t *const data);
    void stream_frame(const bool receivePkt = true);
    int threadingLevel() const;
    void _syncHeadPosition();
    bool _syncFrame();
    bool _skipFrame();
    void _barrier();

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

#ifdef USE_NVPIPE
    NvPipe *encoder{nullptr};
#endif

    const brayns::PropertyMap _props;
    size_t _frameCnt{0};

    std::unique_ptr<mpicommon::MPIBcastFabric> mpiFabric;
};

} // namespace streamer
#endif
