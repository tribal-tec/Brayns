#ifndef OSPRAYPIXELOPERATIONS_H
#define OSPRAYPIXELOPERATIONS_H

#include <deflect/Stream.h>
#include <ospray.h>
#include <ospray/SDK/fb/FrameBuffer.h>
#include <ospray/SDK/fb/PixelOp.h>

namespace brayns
{
class DeflectPixelOp : public ospray::PixelOp
{
public:
    DeflectPixelOp();
    ~DeflectPixelOp();

    struct Settings
    {
        bool compression{false};
        unsigned int quality{80};
    };

    struct Instance : public ospray::PixelOp::Instance
    {
        Instance(ospray::FrameBuffer* fb_, PixelOp::Instance*,
                 deflect::Stream& stream, Settings& settings)
            : _deflectStream(stream)
            , _settings(settings)
        {
            fb_->pixelOp = this;
            fb = fb_;
        }

        void beginFrame() final
        {
            ++_frameID;
            const size_t numTiles = fb->getNumTiles().x * fb->getNumTiles().y;
            if (_buffers.size() < numTiles)
            {
                _buffers.resize(numTiles);
                _futures.resize(numTiles);
            }
        }

        void endFrame() final
        {
            for (auto& future : _futures)
                future.get();

            _deflectStream.finishFrame().get();
        }

        void postAccum(ospray::Tile& tile) final
        {
            const size_t tileID =
                tile.region.lower.y / TILE_SIZE * fb->getNumTiles().x +
                tile.region.lower.x / TILE_SIZE;

            auto& rgb = _buffers[tileID];
            for (int i = 0; i < TILE_SIZE * TILE_SIZE; i++)
            {
                int r = std::min(255, int(255.f * tile.r[i]));
                int g = std::min(255, int(255.f * tile.g[i]));
                int b = std::min(255, int(255.f * tile.b[i]));

                rgb[i * 3 + 0] = r;
                rgb[i * 3 + 1] = g;
                rgb[i * 3 + 2] = b;
            }

            deflect::ImageWrapper image(rgb.data(), TILE_SIZE, TILE_SIZE,
                                        deflect::RGB, tile.region.lower.x,
                                        tile.region.lower.y);
            image.compressionPolicy = _settings.compression
                                          ? deflect::COMPRESSION_ON
                                          : deflect::COMPRESSION_OFF;
            image.compressionQuality = _settings.quality;
            // std::lock_guard<std::mutex> guard(_mutex);
            _futures[tileID] = _deflectStream.send(image);
        }

        std::string toString() const final { return "DeflectPixelOp"; }
        deflect::Stream& _deflectStream;
        size_t _frameID{0};
        std::vector<std::array<unsigned char, TILE_SIZE * TILE_SIZE * 3>>
            _buffers;
        std::vector<deflect::Stream::Future> _futures;
        Settings& _settings;
        std::mutex _mutex;
    };

    void _initializeDeflect(const std::string& id, const std::string& host,
                            const size_t port)
    {
        try
        {
            _deflectStream.reset(new deflect::Stream(id, host, port));
        }
        catch (const std::runtime_error& ex)
        {
            std::cout << "Deflect failed to initialize. " << ex.what()
                      << std::endl;
        }
    }

    void commit() final
    {
        if (!_deflectStream || !_deflectStream->isConnected())
        {
            std::string id = getParamString("id", "");
            std::string hostname = getParamString("hostname", "");
            size_t port = getParam1i("port", 1701);
            _initializeDeflect(id, hostname, port);
        }
        _settings.compression = getParam1i("compression", 1);
        _settings.quality = getParam1i("quality", 80);
    }

    ospray::PixelOp::Instance* createInstance(ospray::FrameBuffer* fb,
                                              PixelOp::Instance* prev) final
    {
        if (_deflectStream && _deflectStream->isConnected())
            return new Instance(fb, prev, *_deflectStream, _settings);
        return nullptr;
    }

    std::unique_ptr<deflect::Stream> _deflectStream;
    Settings _settings;
};
}

#endif // OSPRAYPIXELOPERATIONS_H
