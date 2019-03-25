#include "streamer.h"

#include <cstdio>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>

#include <brayns/engine/Engine.h>
#include <brayns/engine/FrameBuffer.h>
#include <brayns/parameters/ParametersManager.h>
#include <brayns/pluginapi/PluginAPI.h>

// mpv /tmp/test.sdp --no-cache --untimed --vd-lavc-threads=1 -vf=flip
// mpv /tmp/test.sdp --profile=low-latency --vf=vflip
namespace streamer
{
#define STREAM_PIX_FMT AV_PIX_FMT_YUV420P

static int encode_and_write_frame(AVCodecContext *codec_ctx,
                                  AVFormatContext *fmt_ctx, AVFrame *frame)
{
    AVPacket pkt = {0};
    av_init_packet(&pkt);

#if 1
    int ret = avcodec_send_frame(codec_ctx, frame);
    if (ret < 0)
    {
        fprintf(stderr, "Error sending frame to codec context!\n");
        return ret;
    }

    ret = avcodec_receive_packet(codec_ctx, &pkt);
    if (ret < 0)
    {
        fprintf(stderr, "Error receiving packet from codec context!\n");
        return ret;
    }
#else
    int frameDecodingComplete = 0;
    int ret =
        avcodec_encode_video2(codec_ctx, &pkt, frame, &frameDecodingComplete);
    if (ret < 0)
    {
        fprintf(stderr, "Error sending frame to codec context!\n");
        return ret;
    }
#endif
    av_interleaved_write_frame(fmt_ctx, &pkt);
    av_packet_unref(&pkt);

    return 0;
}

static int set_options_and_open_encoder(AVFormatContext *fctx, AVStream *stream,
                                        AVCodecContext *codec_ctx,
                                        AVCodec *codec,
                                        std::string codec_profile, double width,
                                        double height, int fps, int bitrate,
                                        AVCodecID codec_id)
{
    const AVRational dst_fps = {fps, 1};

    codec_ctx->codec_tag = 0;
    codec_ctx->codec_id = codec_id;
    codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_ctx->width = width;
    codec_ctx->height = height;
    codec_ctx->gop_size = 25;
    codec_ctx->pix_fmt = STREAM_PIX_FMT;
    codec_ctx->framerate = dst_fps;
    codec_ctx->time_base = av_inv_q(dst_fps);
    codec_ctx->bit_rate = bitrate;
    codec_ctx->max_b_frames = 0;

    auto c = codec_ctx;
    c->rc_max_rate = 0;
    c->rc_buffer_size = 0;
    c->gop_size = 5;
    c->max_b_frames = 0;
    c->qmin = 10;
    c->qmax = 51;
    c->me_subpel_quality = 5;
    c->i_quant_factor = 0.71;
    c->qcompress = 0.6;
    c->max_qdiff = 4;

    if (fctx->oformat->flags & AVFMT_GLOBALHEADER)
    {
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    stream->time_base = codec_ctx->time_base; // will be set afterwards by
                                              // avformat_write_header to 1/1000

    int ret = avcodec_parameters_from_context(stream->codecpar, codec_ctx);
    if (ret < 0)
    {
        fprintf(stderr, "Could not initialize stream codec parameters!\n");
        return 1;
    }

    stream->codecpar->video_delay = 0;

    AVDictionary *codec_options = nullptr;
    av_dict_set(&codec_options, "profile", codec_profile.c_str(), 0);
    av_dict_set(&codec_options, "preset", "ultrafast", 0);
    av_dict_set(&codec_options, "tune", "zerolatency", 0);
    av_dict_set(&codec_options, "g", "30", 0);

    // open video encoder
    ret = avcodec_open2(codec_ctx, codec, &codec_options);
    if (ret < 0)
    {
        fprintf(stderr, "Could not open video encoder!\n");
        return 1;
    }
    av_dict_free(&codec_options);
    return 0;
}

Streamer::Streamer(const brayns::PropertyMap &props)
    : _props(props)
{
    format_ctx = nullptr;
    out_codec = nullptr;
    out_stream = nullptr;
    out_codec_ctx = nullptr;
    av_register_all();
    inv_stream_timebase = 30.0;
}

void Streamer::init()
{
    StreamerConfig streamer_config(_props.getProperty<int>("width"),
                                   _props.getProperty<int>("height"),
                                   _props.getProperty<int>("fps"),
                                   _props.getProperty<int>("bitrate"),
                                   _props.getProperty<std::string>("profile"));
    if (!init(streamer_config))
        return;

    _timer.start();

    thread = std::thread(std::bind(&Streamer::_runLoop, this));
}

void _copyToImage(Image &image, brayns::FrameBuffer &frameBuffer)
{
    const auto &size = frameBuffer.getSize();
    const size_t bufferSize = size.x * size.y * frameBuffer.getColorDepth();
    const auto data = frameBuffer.getColorBuffer();

    image.data.resize(bufferSize);
    memcpy(image.data.data(), data, bufferSize);
    image.size = size;
    image.format = frameBuffer.getFrameBufferFormat();
}

void Streamer::postRender()
{
    const auto fps = config.fps;
    if (fps == 0)
        return;

    const auto elapsed = _timer.elapsed() + _leftover;
    const auto duration = 1.0 / fps;
    if (elapsed < duration)
        return;

    _leftover = elapsed - duration;
    for (; _leftover > duration;)
        _leftover -= duration;
    _timer.start();

    const auto &frameBuffers = _api->getEngine().getFrameBuffers();
    if (frameBuffers.size() < 1)
        return;
    auto &frameBuffer = frameBuffers[_props.getProperty<int>("fb")];
    frameBuffer->map();
    if (frameBuffer->getColorBuffer())
    {
        static Image image;
        _copyToImage(image, *frameBuffer);
        stream_frame(image);
    }
    frameBuffer->unmap();
}

void Streamer::cleanup()
{
    if (out_codec_ctx)
    {
        avcodec_close(out_codec_ctx);
        avcodec_free_context(&out_codec_ctx);
    }

    if (format_ctx)
    {
        if (format_ctx->pb)
        {
            avio_close(format_ctx->pb);
        }
        avformat_free_context(format_ctx);
        format_ctx = nullptr;
    }
}

Streamer::~Streamer()
{
    thread.join();
    cleanup();
    avformat_network_deinit();
}

void Streamer::stream_frame(const Image &image)
{
    _rgbas.push(image);
}

void Streamer::_runLoop()
{
    while (_api->getEngine().getKeepRunning())
    {
        const auto &image = _rgbas.pop();
        const int width = image.size[0];
        const int height = image.size[1];
        const int stride[] = {4 * width};
        auto data = reinterpret_cast<const uint8_t *const>(image.data.data());
        sws_context =
            sws_getCachedContext(sws_context, width, height, AV_PIX_FMT_RGBA,
                                 config.dst_width, config.dst_height,
                                 STREAM_PIX_FMT, SWS_FAST_BILINEAR, 0, 0, 0);
        sws_scale(sws_context, &data, stride, 0, height, picture.frame->data,
                  picture.frame->linesize);
        picture.frame->pts +=
            av_rescale_q(1, out_codec_ctx->time_base, out_stream->time_base);
        encode_and_write_frame(out_codec_ctx, format_ctx, picture.frame);
    }
}

bool Streamer::init(const StreamerConfig &streamer_config)
{
    cleanup();
    if (avformat_network_init() < 0)
        return false;

    config = streamer_config;

    AVOutputFormat *fmt = av_guess_format("rtp", NULL, NULL);
    const char *fmt_name = "h264";
    std::string fileBla = "rtp://" + _props.getProperty<std::string>("host");
    const char *filename = fileBla.c_str();

    // initialize format context for output with flv and no filename
    avformat_alloc_output_context2(&format_ctx, fmt, fmt_name, filename);
    if (!format_ctx)
        return false;

    // AVIOContext for accessing the resource indicated by url
    if (!(format_ctx->oformat->flags & AVFMT_NOFILE))
    {
        int avopen_ret = avio_open2(&format_ctx->pb, format_ctx->filename,
                                    AVIO_FLAG_WRITE, nullptr, nullptr);
        if (avopen_ret < 0)
        {
            fprintf(
                stderr,
                "failed to open stream output context, stream will not work\n");
            return false;
        }
    }

    // use selected codec
    AVCodecID codec_id = AV_CODEC_ID_H264;
    out_codec = avcodec_find_encoder(codec_id);
    if (!(out_codec))
    {
        fprintf(stderr, "Could not find encoder for '%s'\n",
                avcodec_get_name(codec_id));
        return false;
    }

    out_stream = avformat_new_stream(format_ctx, out_codec);
    if (!out_stream)
    {
        fprintf(stderr, "Could not allocate stream\n");
        return false;
    }

    out_codec_ctx = avcodec_alloc_context3(out_codec);

    if (set_options_and_open_encoder(format_ctx, out_stream, out_codec_ctx,
                                     out_codec, config.profile,
                                     config.dst_width, config.dst_height,
                                     config.fps, config.bitrate, codec_id))
    {
        return false;
    }

    out_stream->codecpar->extradata_size = out_codec_ctx->extradata_size;
    out_stream->codecpar->extradata =
        static_cast<uint8_t *>(av_mallocz(out_codec_ctx->extradata_size));
    memcpy(out_stream->codecpar->extradata, out_codec_ctx->extradata,
           out_codec_ctx->extradata_size);

    picture.init(out_codec_ctx->pix_fmt, config.dst_width, config.dst_height);

    if (avformat_write_header(format_ctx, nullptr) < 0)
    {
        fprintf(stderr, "Could not write header!\n");
        return false;
    }

    printf("stream time base = %d / %d \n", out_stream->time_base.num,
           out_stream->time_base.den);

    inv_stream_timebase =
        (double)out_stream->time_base.den / (double)out_stream->time_base.num;

    char buf[200000];
    AVFormatContext *ac[] = {format_ctx};
    av_sdp_create(ac, 1, buf, 20000);

    printf("sdp:\n%s\n", buf);
    FILE *fsdp = fopen("/tmp/test.sdp", "w");
    if (fsdp)
    {
        fprintf(fsdp, "%s", buf);
        fclose(fsdp);
    }
    return true;
}

} // namespace streamer

extern "C" brayns::ExtensionPlugin *brayns_plugin_create(int argc,
                                                         const char **argv)
{
    brayns::PropertyMap props;
    props.setProperty({"host", std::string("localhost:49990")});
    props.setProperty({"fps", 30});
    props.setProperty({"bitrate", 10000000});
    props.setProperty({"width", 1920});
    props.setProperty({"height", 1080});
    props.setProperty({"profile", std::string("high444")});
    props.setProperty({"fb", 0});
    if (!props.parse(argc, argv))
        return nullptr;
    return new streamer::Streamer(props);
}
