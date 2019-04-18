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

#ifdef USE_MPI
#include <ospray/mpiCommon/MPIBcastFabric.h>
#endif

#ifdef USE_NVPIPE
#include <NvPipe.h>
#include <lunchbox/mtQueue.h>
#endif

namespace streamer
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
    auto width() const { return _props.getProperty<int>("width"); }
    auto height() const { return _props.getProperty<int>("height"); }
    auto fps() const { return _props.getProperty<int>("fps"); }
    auto bitrate() const { return _props.getProperty<int>("bitrate"); }
    auto gop() const { return _props.getProperty<int>("gop"); }
    auto profile() const { return _props.getProperty<std::string>("profile"); }
    auto threadingLevel() const { return _props.getProperty<int>("threading"); }

    void _runCopyLoop();
    void _runLoop();
    void encodeFrame(const int width, const int height,
                     const uint8_t *const data);
    void stream_frame(const bool receivePkt = true);
    void _syncFrame();
    void _barrier();
    void _nextFrame();
    void printStats();

    AVFormatContext *format_ctx{nullptr};
    AVCodec *out_codec{nullptr};
    AVStream *out_stream{nullptr};
    AVCodecContext *out_codec_ctx{nullptr};
    AVPacket *pkt{nullptr};
    SwsContext *sws_context{nullptr};
    Picture picture;

    brayns::Timer _timer;
    int64_t _waitTime{0};

    Image image;
    std::thread _copyThread;
    std::thread _sendThread;
    lunchbox::Monitor<int> _rgbas;
    lunchbox::Monitor<int> _pkts;

#ifdef USE_NVPIPE
    NvPipe *encoder{nullptr};
    void compressAndSend(void *cudaBuffer, const brayns::Vector2ui &size);
    lunchbox::MTQueue<void *> _cudaQueue;
#endif

    const brayns::PropertyMap _props;
    size_t _frameCnt{0};
    double encodeDuration{0};
#ifdef USE_MPI
    void _initMPI();
    double mpiDuration{0};
    double barrierDuration{0};

    std::unique_ptr<ospcommon::networking::Fabric> mpiFabric;
#endif
};

} // namespace streamer
#endif
