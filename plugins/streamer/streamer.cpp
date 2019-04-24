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

// mpv /tmp/test.sdp --no-cache --untimed --vd-lavc-threads=1 -vf=flip
// mpv /tmp/test.sdp --profile=low-latency --vf=vflip

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

void _copyToImage(streamer::Image &image, brayns::FrameBuffer &frameBuffer)
{
    const auto &size = frameBuffer.getSize();
    const size_t bufferSize = size.x * size.y * frameBuffer.getColorDepth();
    const auto data = frameBuffer.getColorBuffer();

    if (image.data.size() < bufferSize)
        image.data.resize(bufferSize);
    memcpy(image.data.data(), data, bufferSize);
}
}

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
    if (codecContext)
    {
        avcodec_close(codecContext);
        avcodec_free_context(&codecContext);
    }

    if (streamContext)
    {
        if (streamContext->pb)
            avio_close(streamContext->pb);
        avformat_free_context(streamContext);
    }
    avformat_network_deinit();
}

void Streamer::init()
{
    av_register_all();
    if (avformat_network_init() < 0)
        THROW("Could not init stream network");

    const bool useRTP = !_props.getProperty<bool>("rtsp");
    const std::string filename =
        useRTP
            ? "rtp://" + _props.getProperty<std::string>("host")
            : "rtsp://" + _props.getProperty<std::string>("host") + "/test.sdp";

    AVOutputFormat *fmt =
        av_guess_format(useRTP ? "rtp" : "rtsp", nullptr, nullptr);
    avformat_alloc_output_context2(&streamContext, fmt, "h264",
                                   filename.c_str());
    if (!streamContext)
        THROW("Could not open format context");

    if (!(streamContext->oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open2(&streamContext->pb, streamContext->filename,
                       AVIO_FLAG_WRITE, nullptr, nullptr) < 0)
            THROW("Failed to open stream output context, stream will not work");
    }

    AVCodecID codecID = AV_CODEC_ID_H264;
    codec = avcodec_find_encoder(codecID);
    if (!(codec))
        THROW("Could not find encoder for "
              << std::quoted(avcodec_get_name(codecID)));

    stream = avformat_new_stream(streamContext, codec);
    if (!stream)
        THROW("Could not allocate stream");

    const AVRational avFPS = {fps(), 1};
    stream->time_base = av_inv_q(avFPS);

    if (useGPU())
    {
        stream->codecpar->codec_id = AV_CODEC_ID_H264;
        stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        stream->codecpar->width = width();
        stream->codecpar->height = height();

#ifdef USE_NVPIPE
        encoder = NvPipe_CreateEncoder(NVPIPE_RGBA32, NVPIPE_H264, NVPIPE_LOSSY,
                                       bitrate(), fps(), width(), height());
        if (!encoder)
            THROW("Failed to create encoder: " << NvPipe_GetError(nullptr));
#endif
    }
    else
    {
        codecContext = avcodec_alloc_context3(codec);
        if (!codecContext)
            THROW("Could not allocate video codec context");

        codecContext->codec_tag = 0;
        codecContext->codec_id = codecID;
        codecContext->codec_type = AVMEDIA_TYPE_VIDEO;
        codecContext->width = width();
        codecContext->height = height();
        codecContext->gop_size = gop();
        codecContext->pix_fmt = STREAM_PIX_FMT;
        codecContext->framerate = avFPS;
        codecContext->time_base = av_inv_q(avFPS);
        codecContext->bit_rate = bitrate();
        codecContext->max_b_frames = 0;

        if (streamContext->oformat->flags & AVFMT_GLOBALHEADER)
            codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        if (avcodec_parameters_from_context(stream->codecpar, codecContext) < 0)
            THROW("Could not initialize stream codec parameters!");

        AVDictionary *codec_options = nullptr;
        av_dict_set(&codec_options, "profile", profile().c_str(), 0);
        av_dict_set(&codec_options, "preset", "ultrafast", 0);
        av_dict_set(&codec_options, "tune", "zerolatency", 0);

        if (avcodec_open2(codecContext, codec, &codec_options) < 0)
            THROW("Could not open video encoder!");
        av_dict_free(&codec_options);

        stream->codecpar->extradata_size = codecContext->extradata_size;
        stream->codecpar->extradata =
            static_cast<uint8_t *>(av_mallocz(codecContext->extradata_size));
        memcpy(stream->codecpar->extradata, codecContext->extradata,
               codecContext->extradata_size);

        picture.init(codecContext->pix_fmt, width(), height());
    }

    if (avformat_write_header(streamContext, nullptr) !=
        AVSTREAM_INIT_IN_WRITE_HEADER)
    {
        THROW("Could not write stream header")
    }

#ifdef USE_MPI
    _initMPI();
#endif

    if (useRTP)
    {
        char buf[200000];
        AVFormatContext *ac[] = {streamContext};
        av_sdp_create(ac, 1, buf, 20000);

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

    frameBuffer->map();

    const void *buffer = frameBuffer->getColorBuffer();
    if (asyncEncode())
    {
        if (useGPU())
        {
            Image img(_frameNumber, frameBuffer->getSize());
            img.buffer = buffer;
            _images.push(img);
        }
        else
        {
            if (asyncCopy())
            {
                Image img(_frameNumber, frameBuffer->getSize());
                _copyToImage(img, *frameBuffer);
                _images.push(img);
            }
            else
                encodeFrame(_frameNumber, frameBuffer->getSize(), buffer);
        }
    }
    else
        encodeFrame(_frameNumber, frameBuffer->getSize(), buffer);

    frameBuffer->unmap();

    if (_props.getProperty<bool>("stats"))
        printStats();
    if (isLocalOrMaster())
        _nextFrame();
}

void Streamer::encodeFrame(const size_t frameNumber,
                           const brayns::Vector2ui &size, const void *data)
{
    brayns::Timer encodeTimer;
    encodeTimer.start();

    const auto pts =
        av_rescale_q(frameNumber, AVRational{1, fps()}, stream->time_base);

    if (useGPU())
    {
        static std::vector<uint8_t> compressed;
        const size_t bufSize = size.x * size.y * 4;
        if (compressed.size() < bufSize)
            compressed.resize(bufSize);

        const auto compressedSize =
#ifdef USE_NVPIPE
            NvPipe_Encode(encoder, data, size.x * 4, compressed.data(),
                          compressed.size(), size.x, size.y,
                          gop() > 0 ? frameNumber % gop() == 0 : false);
#else
            0;
#endif
        av_init_packet(pkt);
        pkt->data = compressed.data();
        pkt->size = compressedSize;
        pkt->pts = pts;
        pkt->dts = pts;
        pkt->stream_index = stream->index;

        if (!memcmp(compressed.data(), "\x00\x00\x00\x01\x67", 5))
            pkt->flags |= AV_PKT_FLAG_KEY;
    }
    else
    {
        sws_context =
            sws_getCachedContext(sws_context, size.x, size.y, AV_PIX_FMT_RGBA,
                                 width(), height(), STREAM_PIX_FMT,
                                 SWS_FAST_BILINEAR, 0, 0, 0);
        const int stride[] = {4 * (int)size.x};
        auto cdata = reinterpret_cast<const uint8_t *const>(data);
        sws_scale(sws_context, &cdata, stride, 0, size.y, picture.frame->data,
                  picture.frame->linesize);
        picture.frame->pts = pts;

        if (avcodec_send_frame(codecContext, picture.frame) < 0)
            return;
    }

    encodeDuration = encodeTimer.elapsed();

    if (asyncEncode() && !useGPU())
        ++_pkts;
    else
        streamFrame(!useGPU());
}

void Streamer::streamFrame(const bool finishEncode)
{
    if (finishEncode)
    {
        brayns::Timer timer;
        timer.start();
        const auto ret = avcodec_receive_packet(codecContext, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        // encodeDuration += timer.elapsed();
        for (double g = encodeDuration;
             !encodeDuration.compare_exchange_strong(g, g + timer.elapsed());)
            ;
    }

    _barrier();
#if 0
    av_write_frame(format_ctx, pkt);
    av_write_frame(format_ctx, nullptr);
#else
    av_interleaved_write_frame(streamContext, pkt);
#endif
    av_packet_unref(pkt);
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
    return useGPU() &&
           _api->getParametersManager()
                   .getApplicationParameters()
                   .getEngine() == "optix";
}

bool Streamer::isLocalOrMaster() const
{
#ifdef USE_MPI
    return !useMPI() || mpicommon::IamTheMaster();
#else
    return true;
#endif
}

void Streamer::_runAsyncEncode()
{
    while (_api->getEngine().getKeepRunning())
    {
        Image img = _images.pop();
        const void *data = img.buffer;
        if (!img.data.empty())
            data = reinterpret_cast<const uint8_t *const>(img.data.data());
        if (!data)
            break;
        encodeFrame(img.frameNumber, img.size, data);
    }
}

void Streamer::_runAsyncEncodeFinish()
{
    while (_api->getEngine().getKeepRunning())
    {
        _pkts.waitGT(0);
        streamFrame();
        --_pkts;
    }
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
    timer.start();
    if (mpicommon::IamTheMaster())
    {
        const auto head =
            camera.getProperty<std::array<double, 3>>("headPosition");

        ospcommon::networking::BufferedWriteStream stream(*mpiFabric);
        stream << head << _frameNumber;
        stream.flush();
    }
    else
    {
        std::array<double, 3> head;
        ospcommon::networking::BufferedReadStream stream(*mpiFabric);
        stream >> head >> _frameNumber;
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
    ++_frameNumber;
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
}

extern "C" brayns::ExtensionPlugin *brayns_plugin_create(int argc,
                                                         const char **argv)
{
    brayns::PropertyMap props;
    props.setProperty({"host", std::string("localhost:49990")});
    props.setProperty({"fps", 60});
    props.setProperty({"bitrate", 10, {"Bitrate", "in MBit/s"}});
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
