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

#ifdef USE_MPI
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
#endif

#define THROW(msg)                         \
    {                                      \
        std::stringstream s;               \
        s << msg;                          \
        throw std::runtime_error(s.str()); \
    }

#define STREAM_PIX_FMT AV_PIX_FMT_YUV420P

namespace
{
constexpr std::array<double, 3> HEAD_INIT_POS{{0.0, 2.0, 0.0}};

brayns::Property getHeadPositionProperty()
{
    brayns::Property headPosition{"headPosition", HEAD_INIT_POS};
    // headPosition.markReadOnly();
    return headPosition;
}

int set_options_and_open_encoder(AVFormatContext *fctx, AVStream *stream,
                                 AVCodecContext *codec_ctx, AVCodec *codec,
                                 std::string codec_profile, double width,
                                 double height, int fps, int bitrate, int gop,
                                 AVCodecID codec_id)
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

void _copyToImage(streamer::Image &image, brayns::FrameBuffer &frameBuffer)
{
    const auto &size = frameBuffer.getSize();
    const size_t bufferSize = size.x * size.y * frameBuffer.getColorDepth();
    const auto data = frameBuffer.getColorBuffer();

    if (image.data.size() < bufferSize)
        image.data.resize(bufferSize);
    memcpy(image.data.data(), data, bufferSize);
    image.size = size;
}
}

// mpv /tmp/test.sdp --no-cache --untimed --vd-lavc-threads=1 -vf=flip
// mpv /tmp/test.sdp --profile=low-latency --vf=vflip
namespace streamer
{
Streamer::Streamer(const brayns::PropertyMap &props)
    : _props(props)
{
}

Streamer::~Streamer()
{
    if (asyncEncode())
    {
        _images.push(Image{});
        _encodeThread.join();
        if (!useGPU())
        {
            ++_pkts;
            _encodeFinishThread.join();
        }
    }
#ifdef USE_NVPIPE
    if (encoder)
        NvPipe_Destroy(encoder);
#endif

    if (pkt)
        av_packet_free(&pkt);
    if (out_codec_ctx)
    {
        avcodec_close(out_codec_ctx);
        avcodec_free_context(&out_codec_ctx);
    }

    if (format_ctx)
    {
        if (format_ctx->pb)
            avio_close(format_ctx->pb);
        avformat_free_context(format_ctx);
    }
    avformat_network_deinit();
}

void Streamer::init()
{
    av_register_all();
    if (avformat_network_init() < 0)
        THROW("Could not init stream network");

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
        THROW("Could not open output context");

    // AVIOContext for accessing the resource indicated by url
    if (!(format_ctx->oformat->flags & AVFMT_NOFILE))
    {
        int avopen_ret = avio_open2(&format_ctx->pb, format_ctx->filename,
                                    AVIO_FLAG_WRITE, nullptr, nullptr);
        if (avopen_ret < 0)
            THROW("Failed to open stream output context, stream will not work");
    }

    if (useGPU())
    {
        format_ctx->oformat = fmt;
        fmt->video_codec = AV_CODEC_ID_H264;

        out_stream = avformat_new_stream(format_ctx, nullptr);
        out_stream->id = 0;
        out_stream->codecpar->codec_id = AV_CODEC_ID_H264;
        out_stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        out_stream->codecpar->width = width();
        out_stream->codecpar->height = height();
        out_stream->time_base.den = 1;
        out_stream->time_base.num = fps();

#ifdef USE_NVPIPE
        encoder = NvPipe_CreateEncoder(NVPIPE_RGBA32, NVPIPE_H264, NVPIPE_LOSSY,
                                       bitrate(), fps(), width(), height());
        if (!encoder)
            THROW("Failed to create encoder: " << NvPipe_GetError(NULL));
#endif
    }
    else
    {
        // use selected codec
        AVCodecID codec_id = AV_CODEC_ID_H264;
        out_codec = avcodec_find_encoder(codec_id);
        if (!(out_codec))
            THROW("Could not find encoder for "
                  << std::quoted(avcodec_get_name(codec_id)));

        out_stream = avformat_new_stream(format_ctx, out_codec);
        if (!out_stream)
            THROW("Could not allocate stream")

        out_codec_ctx = avcodec_alloc_context3(out_codec);
        if (!out_codec_ctx)
            THROW("Could not allocate video codec context");

        if (set_options_and_open_encoder(format_ctx, out_stream, out_codec_ctx,
                                         out_codec, profile(), width(),
                                         height(), fps(), bitrate(), gop(),
                                         codec_id))
        {
            THROW("Could not open encoder");
        }

        out_stream->codecpar->extradata_size = out_codec_ctx->extradata_size;
        out_stream->codecpar->extradata =
            static_cast<uint8_t *>(av_mallocz(out_codec_ctx->extradata_size));
        memcpy(out_stream->codecpar->extradata, out_codec_ctx->extradata,
               out_codec_ctx->extradata_size);
        picture.init(out_codec_ctx->pix_fmt, width(), height());
    }

    if (avformat_write_header(format_ctx, nullptr) !=
        AVSTREAM_INIT_IN_WRITE_HEADER)
    {
        THROW("Could not write stream header")
    }

    printf("stream time base = %d / %d \n", out_stream->time_base.num,
           out_stream->time_base.den);

#ifdef USE_MPI
    _initMPI();
#endif

    if (useRTP)
    {
        char buf[200000];
        AVFormatContext *ac[] = {format_ctx};
        av_sdp_create(ac, 1, buf, 20000);

        printf("sdp:\n%s\n", buf);
        std::stringstream outFile;
        outFile << "/tmp/test";
#ifdef USE_MPI
        if (useMPI())
            outFile << mpicommon::world.rank;
#endif
        outFile << ".sdp";
        FILE *fsdp = fopen(outFile.str().c_str(), "w");
        if (fsdp)
        {
            fprintf(fsdp, "%s", buf);
            fclose(fsdp);
        }
    }

    pkt = av_packet_alloc();

    auto &camera = _api->getCamera();
    if (!camera.hasProperty("headPosition"))
    {
        brayns::PropertyMap props;
        props.setProperty(getHeadPositionProperty());
        camera.updateProperties(props);
    }

    if (asyncEncode())
    {
        _encodeThread =
            std::thread(std::bind(&Streamer::_runAsyncEncode, this));
        if (!useGPU())
            _encodeFinishThread =
                std::thread(std::bind(&Streamer::_runAsyncEncodeFinish, this));
    }

    _timer.start();
}

void Streamer::preRender()
{
    _syncFrame();

    if (!useCudaBuffer() || _fbModified)
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
    _fbModified = true;
}

void Streamer::postRender()
{
    const auto &frameBuffers = _api->getEngine().getFrameBuffers();
    if (frameBuffers.size() < 1)
        return;
    auto &frameBuffer = frameBuffers[_props.getProperty<int>("fb")];
    const int width = frameBuffer->getSize().x;
    const int height = frameBuffer->getSize().y;

    frameBuffer->map();

    const void *buffer = useCudaBuffer() ? frameBuffer->cudaBuffer()
                                         : frameBuffer->getColorBuffer();
    if (asyncEncode())
    {
        if (useGPU())
            _images.push(Image(buffer, frameBuffer->getSize()));
        else
        {
            if (asyncCopy())
            {
                Image img;
                _copyToImage(img, *frameBuffer);
                _images.push(img);
            }
            else
                encodeFrame(width, height, buffer);
        }
    }
    else
        encodeFrame(width, height, buffer);

    frameBuffer->unmap();
    if (_props.getProperty<bool>("stats"))
        printStats();

    if (isLocalOrMaster())
        _nextFrame();
}

bool Streamer::useGPU() const
{
#ifdef USE_NVPIPE
    return _props.getProperty<bool>("gpu");
#else
    return false;
#endif
}

bool Streamer::useMPI() const
{
#ifdef USE_MPI
    return _props.getProperty<bool>("mpi");
#else
    return false;
#endif
}

bool Streamer::useCudaBuffer() const
{
    return _api->getParametersManager()
                   .getApplicationParameters()
                   .getEngine() == "optix" &&
           useGPU();
}

bool Streamer::isLocalOrMaster() const
{
#ifdef USE_MPI
    return !useMPI() || mpicommon::IamTheMaster();
#else
    return true;
#endif
}

void Streamer::encodeFrame(const int srcWidth, const int srcHeight,
                           const void *data)
{
    brayns::Timer encodeTimer;
    encodeTimer.start();

    if (useGPU())
    {
        static std::vector<uint8_t> compressed;
        const size_t bufSize = srcWidth * srcHeight * 4;
        if (compressed.size() < bufSize)
            compressed.resize(bufSize);

        uint64_t size =
#ifdef USE_NVPIPE
            NvPipe_Encode(encoder, data, srcWidth * 4, compressed.data(),
                          compressed.size(), srcWidth, srcHeight,
                          gop() > 0 ? _frameCnt % gop() == 0 : false);
#else
            0;
#endif
        encodeDuration = encodeTimer.elapsed();

        av_init_packet(pkt);
        pkt->data = compressed.data();
        pkt->size = size;
        pkt->pts = av_rescale_q(_frameCnt, AVRational{1, fps()},
                                out_stream->time_base);
        pkt->dts = pkt->pts;
        pkt->stream_index = out_stream->index;

        if (!memcmp(compressed.data(), "\x00\x00\x00\x01\x67", 5))
            pkt->flags |= AV_PKT_FLAG_KEY;

        stream_frame(false);
        return;
    }

    const int stride[] = {4 * srcWidth};
    sws_context =
        sws_getCachedContext(sws_context, srcWidth, srcHeight, AV_PIX_FMT_RGBA,
                             width(), height(), STREAM_PIX_FMT,
                             SWS_FAST_BILINEAR, 0, 0, 0);
    auto cdata = reinterpret_cast<const uint8_t *const>(data);
    sws_scale(sws_context, &cdata, stride, 0, srcHeight, picture.frame->data,
              picture.frame->linesize);
    picture.frame->pts =
        av_rescale_q(_frameCnt, AVRational{1, fps()}, out_stream->time_base);
    //    picture.frame->pts +=
    //        av_rescale_q(1, out_codec_ctx->time_base, out_stream->time_base);

    const auto ret = avcodec_send_frame(out_codec_ctx, picture.frame);
    encodeDuration = encodeTimer.elapsed();
    if (ret >= 0)
    {
        if (asyncEncode())
            ++_pkts;
        else
            stream_frame();
    }
}

void Streamer::_runAsyncEncode()
{
    while (_api->getEngine().getKeepRunning())
    {
        Image img = _images.pop();
        const int width = img.size[0];
        const int height = img.size[1];
        const void *data = img.buffer;
        if (!img.data.empty())
            data = reinterpret_cast<const uint8_t *const>(img.data.data());
        if (!data)
            break;
        encodeFrame(width, height, data);
    }
}

void Streamer::_runAsyncEncodeFinish()
{
    while (_api->getEngine().getKeepRunning())
    {
        _pkts.waitGT(0);
        stream_frame();
        --_pkts;
    }
}

void Streamer::stream_frame(const bool receivePkt)
{
    _barrier();
    if (receivePkt)
    {
        brayns::Timer timer;
        timer.start();
        const auto ret = avcodec_receive_packet(out_codec_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        encodeDuration += timer.elapsed();
    }

#if 0
    av_write_frame(format_ctx, pkt);
    av_write_frame(format_ctx, NULL);
#else
    av_interleaved_write_frame(format_ctx, pkt);
    av_packet_unref(pkt);
#endif
}

void Streamer::printStats()
{
    bool flushOnly = true;

    if (useMPI())
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
        std::cout << '\r';

    const auto elapsed = _timer.elapsed() + double(_waitTime) / 1e6;
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
#ifdef USE_MPI
    if (!useMPI())
        return;

    brayns::Timer timer;
    auto &camera = _api->getCamera();
    if (mpicommon::IamTheMaster())
    {
        timer.start();
        const auto head =
            camera.getProperty<std::array<double, 3>>("headPosition");

        ospcommon::networking::BufferedWriteStream stream(*mpiFabric);
        stream << head << _frameCnt;
        stream.flush();
    }
    else
    {
        timer.start();
        std::array<double, 3> head;
        ospcommon::networking::BufferedReadStream stream(*mpiFabric);
        stream >> head >> _frameCnt;
        camera.updateProperty("headPosition", head);
    }
    mpiDuration = timer.elapsed();
#endif
}

void Streamer::_nextFrame()
{
    _timer.stop();
    _waitTime = std::max(0., (1.0 / fps()) * 1e6 - _timer.microseconds());
    if (_waitTime > 0)
        std::this_thread::sleep_for(std::chrono::microseconds(_waitTime));

    _timer.start();
    ++_frameCnt;
}

void Streamer::_barrier()
{
#ifdef USE_MPI
    if (useMPI())
    {
        brayns::Timer timer;
        timer.start();
        mpicommon::world.barrier();
        barrierDuration = timer.elapsed();
    }
#endif
}

#ifdef USE_MPI
void Streamer::_initMPI()
{
    if (!useMPI())
        return;

    _barrier();
    if (mpicommon::IamTheMaster())
    {
        MPI_CALL(Comm_split(mpicommon::world.comm, 1, mpicommon::world.rank,
                            &mpicommon::app.comm));

        mpicommon::app.makeIntraComm();

        MPI_CALL(Intercomm_create(mpicommon::app.comm, 0, mpicommon::world.comm,
                                  1, 1, &mpicommon::worker.comm));

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
        mpiFabric = std::make_unique<mpicommon::MPIBcastFabric>(mpicommon::app,
                                                                MPI_ROOT, 0);
    }
    _barrier();
}
#endif

} // namespace streamer

extern "C" brayns::ExtensionPlugin *brayns_plugin_create(int argc,
                                                         const char **argv)
{
    brayns::PropertyMap props;
    props.setProperty({"host", std::string("localhost:49990")});
    props.setProperty({"fps", 60});
    props.setProperty({"bitrate", 10000000});
    props.setProperty({"width", 1920});
    props.setProperty({"height", 1080});
    props.setProperty({"profile", std::string("high444")});
    props.setProperty({"fb", 0});
    props.setProperty({"gop", 60});
    props.setProperty({"rtsp", false});
    props.setProperty({"async-encode", false});
    props.setProperty({"async-copy", false}); // CPU only (sws_scale)
    props.setProperty({"stats", false});
    props.setProperty({"eye", 0});
#ifdef USE_NVPIPE
    props.setProperty({"gpu", false});
#endif
#ifdef USE_MPI
    props.setProperty({"mpi", false});
    props.setProperty({"master-stats", false});
#endif
    if (!props.parse(argc, argv))
        return nullptr;

#ifdef USE_MPI
    if (props.getProperty<bool>("mpi"))
        mpicommon::init(&argc, argv, true);
#endif

    return new streamer::Streamer(props);
}
