#include "streamer.h"

#include <cstdio>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>

#include <brayns/engine/Camera.h>
#include <brayns/engine/Engine.h>
#include <brayns/engine/FrameBuffer.h>
#include <brayns/parameters/ParametersManager.h>
#include <brayns/pluginapi/PluginAPI.h>

#include <ospray/mpiCommon/MPICommon.h>
#include <ospray/ospcommon/networking/BufferedDataStreaming.h>

namespace ospcommon
{
namespace networking
{
template <typename T>
inline WriteStream &operator<<(WriteStream &buf, const std::array<T, 3> &rh)
{
    buf.write((const byte_t *)rh.data(), sizeof(T) * 3);
    return buf;
}

template <typename T>
inline ReadStream &operator>>(ReadStream &buf, std::array<T, 3> &rh)
{
    buf.read((byte_t *)rh.data(), sizeof(T) * 3);
    return buf;
}
}
}

// mpv /tmp/test.sdp --no-cache --untimed --vd-lavc-threads=1 -vf=flip
// mpv /tmp/test.sdp --profile=low-latency --vf=vflip
namespace streamer
{
#define STREAM_PIX_FMT AV_PIX_FMT_YUV420P

static int set_options_and_open_encoder(AVFormatContext *fctx, AVStream *stream,
                                        AVCodecContext *codec_ctx,
                                        AVCodec *codec,
                                        std::string codec_profile, double width,
                                        double height, int fps, int bitrate,
                                        int gop, AVCodecID codec_id)
{
    const AVRational dst_fps = {fps, 1};

    codec_ctx->codec_tag = 0;
    codec_ctx->codec_id = codec_id;
    codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_ctx->width = width;
    codec_ctx->height = height;
    codec_ctx->gop_size = gop;
    codec_ctx->pix_fmt = STREAM_PIX_FMT;
    codec_ctx->framerate = dst_fps;
    codec_ctx->time_base = av_inv_q(dst_fps);
    codec_ctx->bit_rate = bitrate;
    codec_ctx->max_b_frames = 0;

    //    codec_ctx->rc_max_rate = 0;
    //    codec_ctx->rc_buffer_size = 0;
    //    codec_ctx->qmin = 10;
    //    codec_ctx->qmax = 51;
    //    codec_ctx->me_subpel_quality = 5;
    //    codec_ctx->i_quant_factor = 0.71;
    //    codec_ctx->qcompress = 0.6;
    //    codec_ctx->max_qdiff = 4;

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
    //    av_dict_set(&codec_options, "crf", "0", 0);
    //    av_opt_set(codec_ctx->priv_data, "crf", "0", AV_OPT_SEARCH_CHILDREN);
    //    av_opt_set(codec_ctx->priv_data, "preset", "slow",
    //    AV_OPT_SEARCH_CHILDREN);

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
    av_register_all();
}

constexpr std::array<double, 3> HEAD_INIT_POS{{0.0, 2.0, 0.0}};
brayns::Property getHeadPositionProperty()
{
    brayns::Property headPosition{"headPosition", HEAD_INIT_POS};
    // headPosition.markReadOnly();
    return headPosition;
}

void Streamer::init()
{
    StreamerConfig streamer_config{_props.getProperty<int>("width"),
                                   _props.getProperty<int>("height"),
                                   _props.getProperty<int>("fps"),
                                   _props.getProperty<int>("bitrate"),
                                   _props.getProperty<std::string>("profile")};
    if (!init(streamer_config))
        return;

    auto &camera = _api->getCamera();
    if (!camera.hasProperty("headPosition"))
    {
        brayns::PropertyMap props;
        props.setProperty(getHeadPositionProperty());
        camera.updateProperties(props);
    }

    _timer.start();

    if (threadingLevel() == 1)
        _sendThread = std::thread(std::bind(&Streamer::_runLoop, this));
    if (threadingLevel() == 2)
        _copyThread = std::thread(std::bind(&Streamer::_runCopyLoop, this));
}

void _copyToImage(Image &image, brayns::FrameBuffer &frameBuffer)
{
    const auto &size = frameBuffer.getSize();
    const size_t bufferSize = size.x * size.y * frameBuffer.getColorDepth();
    const auto data = frameBuffer.getColorBuffer();

    if (image.data.size() < bufferSize)
        image.data.resize(bufferSize);
    memcpy(image.data.data(), data, bufferSize);
    image.size = size;
    image.format = frameBuffer.getFrameBufferFormat();
}

void Streamer::preRender()
{
    _syncFrame();

#ifdef USE_NVPIPE
    if (!_props.getProperty<bool>("gpu"))
        return;
    const auto &frameBuffers = _api->getEngine().getFrameBuffers();
    if (frameBuffers.size() < 1)
        return;
    auto &frameBuffer = frameBuffers[_props.getProperty<int>("fb")];
    frameBuffer->setFormat(brayns::FrameBufferFormat::none);
    switch (_props.getProperty<int>("eye"))
    {
    case 1:
        frameBuffer->setName("L");
        break;
    case 2:
        frameBuffer->setName("R");
        break;
    }
#endif
}

void Streamer::postRender()
{
    const auto &frameBuffers = _api->getEngine().getFrameBuffers();
    if (frameBuffers.size() < 1)
        return;
    auto &frameBuffer = frameBuffers[_props.getProperty<int>("fb")];

#ifdef USE_NVPIPE
    if (auto cudaBuffer = frameBuffer->cudaBuffer())
    {
        if (threadingLevel() == 1)
            _cudaQueue.push(cudaBuffer);
        else
            compressAndSend(cudaBuffer, frameBuffer->getSize());
    }
    else
#endif
    {
        frameBuffer->map();
        if (frameBuffer->getColorBuffer())
        {
            if (threadingLevel() == 2)
            {
                if (_rgbas == 0)
                {
                    _copyToImage(image, *frameBuffer);
                    ++_rgbas;
                }
            }
            else
            {
                const int width = frameBuffer->getSize().x;
                const int height = frameBuffer->getSize().y;
                auto data = reinterpret_cast<const uint8_t *const>(
                    frameBuffer->getColorBuffer());

                encodeFrame(width, height, data);
            }
        }
        frameBuffer->unmap();
    }
    if (_props.getProperty<bool>("stats"))
        printStats();
}

#ifdef USE_NVPIPE
void Streamer::compressAndSend(void *cudaBuffer,
                               const brayns::Vector2ui &fbSize)
{
    static std::vector<uint8_t> compressed;
    const auto bufSize = fbSize.x * fbSize.y * 4;
    if (compressed.size() < bufSize)
        compressed.resize(bufSize);

    const auto gop = _props.getProperty<int>("gop");
    brayns::Timer timer;
    timer.start();
    uint64_t size =
        NvPipe_Encode(encoder, cudaBuffer, fbSize.x * 4, compressed.data(),
                      compressed.size(), fbSize.x, fbSize.y,
                      gop > 0 ? _frameCnt % gop == 0 : false);
    encodeDuration = timer.elapsed();

    av_init_packet(pkt);
    pkt->data = compressed.data();
    pkt->size = size;
    pkt->pts =
        av_rescale_q(_frameCnt, AVRational{1, _props.getProperty<int>("fps")},
                     out_stream->time_base);
    pkt->dts = pkt->pts;
    pkt->stream_index = out_stream->index;

    if (!memcmp(compressed.data(), "\x00\x00\x00\x01\x67", 5))
        pkt->flags |= AV_PKT_FLAG_KEY;

    // stream_frame(false);
    _barrier();
    int ret = av_write_frame(format_ctx, pkt);
    av_write_frame(format_ctx, NULL);
    av_packet_unref(pkt);
}
#endif

void Streamer::cleanup()
{
    if (pkt)
        av_packet_free(&pkt);
    if (out_codec_ctx)
    {
        avcodec_close(out_codec_ctx);
        avcodec_free_context(&out_codec_ctx);
        out_codec_ctx = nullptr;
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
    avformat_network_deinit();
}

void Streamer::_runCopyLoop()
{
    while (_api->getEngine().getKeepRunning())
    {
        _rgbas.waitGT(0);

        const int width = image.size[0];
        const int height = image.size[1];
        auto data = reinterpret_cast<const uint8_t *const>(image.data.data());
        encodeFrame(width, height, data);
    }
}

Streamer::~Streamer()
{
    if (threadingLevel() == 2)
        _copyThread.join();
    if (threadingLevel() == 1)
        _sendThread.join();
#ifdef USE_NVPIPE
    if (encoder)
        NvPipe_Destroy(encoder);
#endif
    cleanup();
}

void Streamer::stream_frame(const bool receivePkt)
{
    _barrier();
    if (receivePkt)
    {
        const auto ret = avcodec_receive_packet(out_codec_ctx, pkt);
        if (ret >= 0)
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                return;
    }

    av_interleaved_write_frame(format_ctx, pkt);
    av_packet_unref(pkt);
}

int Streamer::threadingLevel() const
{
    return _props.getProperty<int>("threading");
}

void Streamer::printStats()
{
    bool flushOnly = true;
    if (_props.getProperty<bool>("mpi"))
    {
        if (_props.getProperty<bool>("master-stats"))
        {
            if (!mpicommon::IamTheMaster())
                return;
            std::cout << '\r';
        }
        else
        {
            std::cout << mpicommon::world.rank << ": ";
            flushOnly = false;
        }
        std::cout << "MPI " << int(mpiDuration * 1000) << "ms | "
                  << "Barrier " << int(barrierDuration * 1000) << "ms | ";
    }
    else
    {
        std::cout << '\r';
    }

    const auto elapsed = _timer.elapsed();
    _timer.start();
    std::cout << "encode " << int(encodeDuration * 1000) << "ms | "
              << "render " << int(_api->getEngine().renderDuration * 1000)
              << "ms | "
              << "total " << int(elapsed * 1000) << "ms | "
              << "overhead "
              << (elapsed - _api->getEngine().renderDuration) * 1000 << "ms | "
              << 1. / elapsed << "/" << 1. / _api->getEngine().renderDuration
              << " FPS";
    if (flushOnly)
        std::cout << std::flush;
    else
        std::cout << std::endl;
}

void Streamer::_syncFrame()
{
    if (!_props.getProperty<bool>("mpi"))
    {
        ++_frameCnt;
        return;
    }

    brayns::Timer timer;
    timer.start();
    auto &camera = _api->getCamera();
    if (mpicommon::IamTheMaster())
    {
        ++_frameCnt;
        const auto head =
            camera.getProperty<std::array<double, 3>>("headPosition");

        ospcommon::networking::BufferedWriteStream stream(*mpiFabric);
        stream << head << _frameCnt;
        stream.flush();
    }
    else
    {
        std::array<double, 3> head;
        ospcommon::networking::BufferedReadStream stream(*mpiFabric);
        stream >> head >> _frameCnt;
        camera.updateProperty("headPosition", head);
    }
    mpiDuration = timer.elapsed();
}

void Streamer::_barrier()
{
    if (_props.getProperty<bool>("mpi"))
    {
        brayns::Timer timer;
        timer.start();
        mpicommon::world.barrier();
        barrierDuration = timer.elapsed();
    }
}

void Streamer::_runLoop()
{
    while (_api->getEngine().getKeepRunning())
    {
#ifdef USE_NVPIPE
        if (encoder)
        {
            auto cudaBuffer = _cudaQueue.pop();
            compressAndSend(
                cudaBuffer,
                _api->getEngine()
                    .getFrameBuffers()[_props.getProperty<int>("fb")]
                    ->getSize());
        }
        else
#endif
        {
            _pkts.waitGT(0);
            --_pkts;

            stream_frame();
        }
    }
}

void Streamer::encodeFrame(const int width, const int height,
                           const uint8_t *const data)
{
    const int stride[] = {4 * width};
    sws_context =
        sws_getCachedContext(sws_context, width, height, AV_PIX_FMT_RGBA,
                             config.dst_width, config.dst_height,
                             STREAM_PIX_FMT, SWS_FAST_BILINEAR, 0, 0, 0);
    brayns::Timer encodeTimer;
    encodeTimer.start();
    sws_scale(sws_context, &data, stride, 0, height, picture.frame->data,
              picture.frame->linesize);

    --_rgbas;
    picture.frame->pts =
        av_rescale_q(_frameCnt, AVRational{1, _props.getProperty<int>("fps")},
                     out_stream->time_base);
    //    picture.frame->pts +=
    //        av_rescale_q(1, out_codec_ctx->time_base, out_stream->time_base);

    if (avcodec_send_frame(out_codec_ctx, picture.frame) >= 0)
    {
        if (threadingLevel() == 1)
            ++_pkts;
        else
            stream_frame();
    }
    encodeDuration = encodeTimer.elapsed();
}

bool Streamer::init(const StreamerConfig &streamer_config)
{
    cleanup();
    if (avformat_network_init() < 0)
        return false;

    config = streamer_config;

    const bool useRTP = !_props.getProperty<bool>("rtsp");
    AVOutputFormat *fmt = av_guess_format(useRTP ? "rtp" : "rtsp", NULL, NULL);
    const char *fmt_name = "h264";
    std::string fileBla =
        useRTP
            ? "rtp://" + _props.getProperty<std::string>("host")
            : "rtsp://" + _props.getProperty<std::string>("host") + "/test.sdp";
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
#ifdef USE_NVPIPE
    if (_props.getProperty<bool>("gpu"))
    {
        format_ctx->oformat = fmt;
        fmt->video_codec = AV_CODEC_ID_H264;

        out_stream = avformat_new_stream(format_ctx, nullptr);
        out_stream->id = 0;
        out_stream->codecpar->codec_id = AV_CODEC_ID_H264;
        out_stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        out_stream->codecpar->width = config.dst_width;
        out_stream->codecpar->height = config.dst_height;
        out_stream->time_base.den = 1;
        out_stream->time_base.num = config.fps;

        encoder = NvPipe_CreateEncoder(NVPIPE_RGBA32, NVPIPE_H264, NVPIPE_LOSSY,
                                       config.bitrate, config.fps,
                                       config.dst_width, config.dst_height);
        if (!encoder)
            std::cerr << "Failed to create encoder: " << NvPipe_GetError(NULL)
                      << std::endl;
    }
    else
#endif
    {
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
        if (!out_codec_ctx)
        {
            fprintf(stderr, "Could not allocate video codec context\n");
            return false;
        }

        if (set_options_and_open_encoder(
                format_ctx, out_stream, out_codec_ctx, out_codec,
                config.profile, config.dst_width, config.dst_height, config.fps,
                config.bitrate, _props.getProperty<int>("gop"), codec_id))
        {
            return false;
        }

        out_stream->codecpar->extradata_size = out_codec_ctx->extradata_size;
        out_stream->codecpar->extradata =
            static_cast<uint8_t *>(av_mallocz(out_codec_ctx->extradata_size));
        memcpy(out_stream->codecpar->extradata, out_codec_ctx->extradata,
               out_codec_ctx->extradata_size);
        picture.init(out_codec_ctx->pix_fmt, config.dst_width,
                     config.dst_height);
    }

    if (avformat_write_header(format_ctx, nullptr) !=
        AVSTREAM_INIT_IN_WRITE_HEADER)
    {
        fprintf(stderr, "Could not write header!\n");
        return false;
    }

    printf("stream time base = %d / %d \n", out_stream->time_base.num,
           out_stream->time_base.den);

    if (useRTP)
    {
        char buf[200000];
        AVFormatContext *ac[] = {format_ctx};
        av_sdp_create(ac, 1, buf, 20000);

        printf("sdp:\n%s\n", buf);
        std::stringstream outFile;
        outFile << "/tmp/test";
        if (_props.getProperty<bool>("mpi"))
            outFile << mpicommon::world.rank;
        outFile << ".sdp";
        FILE *fsdp = fopen(outFile.str().c_str(), "w");
        if (fsdp)
        {
            fprintf(fsdp, "%s", buf);
            fclose(fsdp);
        }
    }

    pkt = av_packet_alloc();

    if (_props.getProperty<bool>("mpi"))
    {
        _barrier();
        if (mpicommon::IamTheMaster())
        {
            MPI_CALL(Comm_split(mpicommon::world.comm, 1, mpicommon::world.rank,
                                &mpicommon::app.comm));

            mpicommon::app.makeIntraComm();

            MPI_CALL(Intercomm_create(mpicommon::app.comm, 0,
                                      mpicommon::world.comm, 1, 1,
                                      &mpicommon::worker.comm));

            mpicommon::worker.makeInterComm();
            mpiFabric =
                std::make_unique<mpicommon::MPIBcastFabric>(mpicommon::worker,
                                                            MPI_ROOT, 0);
        }
        else
        {
            MPI_CALL(Comm_split(mpicommon::world.comm, 0, mpicommon::world.rank,
                                &mpicommon::worker.comm));

            mpicommon::worker.makeIntraComm();

            MPI_CALL(Intercomm_create(mpicommon::worker.comm, 0,
                                      mpicommon::world.comm, 0, 1,
                                      &mpicommon::app.comm));

            mpicommon::app.makeInterComm();
            mpiFabric =
                std::make_unique<mpicommon::MPIBcastFabric>(mpicommon::app,
                                                            MPI_ROOT, 0);
        }
        _barrier();
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
    props.setProperty({"gop", 60});
    props.setProperty({"rtsp", false});
    props.setProperty({"mpi", false});
    props.setProperty({"threading", 0});
    props.setProperty({"stats", false});
    props.setProperty({"master-stats", false});
    props.setProperty({"eye", 0});
#ifdef USE_NVPIPE
    props.setProperty({"gpu", false});
#endif
    if (!props.parse(argc, argv))
        return nullptr;

    if (props.getProperty<bool>("mpi"))
        mpicommon::init(&argc, argv, true);

    return new streamer::Streamer(props);
}
