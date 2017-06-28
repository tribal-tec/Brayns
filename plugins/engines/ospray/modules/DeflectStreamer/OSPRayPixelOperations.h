#ifndef OSPRAYPIXELOPERATIONS_H
#define OSPRAYPIXELOPERATIONS_H

#include <deflect/Stream.h>
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
            for (auto& future : _futures)
                future.get();

            const size_t numTiles = fb->getNumTiles().x * fb->getNumTiles().y;

            if (_futures.size() < numTiles + 1)
                _futures.resize(numTiles + 1);
            if (_rgbBuffers.size() < numTiles)
                _rgbBuffers.resize(numTiles);
            if (_rgbaBuffers.size() < numTiles)
                _rgbaBuffers.resize(numTiles);
        }

        void endFrame() final
        {
            _futures[_futures.size() - 1] = _deflectStream.finishFrame();
        }

        void postAccum(ospray::Tile& tile) final
        {
            const size_t tileID =
                tile.region.lower.y / TILE_SIZE * fb->getNumTiles().x +
                tile.region.lower.x / TILE_SIZE;

            const void* pixelData;
            if (_settings.compression)
            {
                auto& pixels = _rgbBuffers[tileID];
                for (size_t i = 0; i < TILE_SIZE * TILE_SIZE; ++i)
                {
                    pixels[i * 3 + 0] = std::min(255, int(255.f * tile.r[i]));
                    pixels[i * 3 + 1] = std::min(255, int(255.f * tile.g[i]));
                    pixels[i * 3 + 2] = std::min(255, int(255.f * tile.b[i]));
                }
                pixelData = pixels.data();
            }
            else
            {
                auto& pixels = _rgbaBuffers[tileID];
                for (size_t i = 0; i < TILE_SIZE * TILE_SIZE; ++i)
                {
                    pixels[i * 4 + 0] = std::min(255, int(255.f * tile.r[i]));
                    pixels[i * 4 + 1] = std::min(255, int(255.f * tile.g[i]));
                    pixels[i * 4 + 2] = std::min(255, int(255.f * tile.b[i]));
                    pixels[i * 4 + 3] = std::min(255, int(255.f * tile.a[i]));
                }
                pixelData = pixels.data();
            }

            deflect::ImageWrapper image(pixelData, TILE_SIZE, TILE_SIZE,
                                        _settings.compression ? deflect::RGB
                                                              : deflect::RGBA,
                                        tile.region.lower.x,
                                        tile.region.lower.y);
            image.compressionPolicy = _settings.compression
                                          ? deflect::COMPRESSION_ON
                                          : deflect::COMPRESSION_OFF;
            image.compressionQuality = _settings.quality;
            image.subsampling = deflect::ChromaSubsampling::YUV420;
            _futures[tileID] = _deflectStream.send(image);
        }

        std::string toString() const final { return "DeflectPixelOp"; }
        deflect::Stream& _deflectStream;
        std::vector<std::array<unsigned char, TILE_SIZE * TILE_SIZE * 3>>
            _rgbBuffers;
        std::vector<std::array<unsigned char, TILE_SIZE * TILE_SIZE * 4>>
            _rgbaBuffers;
        std::vector<deflect::Stream::Future> _futures;
        Settings& _settings;
    };

    void commit() final
    {
        if (!_deflectStream || !_deflectStream->isConnected())
        {
            try
            {
                _deflectStream.reset(new deflect::Stream);
            }
            catch (const std::runtime_error& ex)
            {
                std::cout << "Deflect failed to initialize. " << ex.what()
                          << std::endl;
            }
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
