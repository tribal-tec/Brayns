/* Copyright (c) 2015-2017, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
 * Responsible Author: Cyrille Favreau <cyrille.favreau@epfl.ch>
 *
 * This file is part of Brayns <https://github.com/BlueBrain/Brayns>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3.0 as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef ROCKETSPLUGIN_H
#define ROCKETSPLUGIN_H

#include "ExtensionPlugin.h"

#include <brayns/api.h>
#include <rockets/server.h>
#include <turbojpeg.h>

namespace brayns
{
class ImageGenerator
{
public:
    ImageGenerator(RocketsPlugin& parent)
        : _parent(parent)
    {
    }
    ~ImageGenerator()
    {
        if (_compressor)
            tjDestroy(_compressor);
    }

    struct ImageJPEG
    {
        struct tjDeleter
        {
            void operator()(uint8_t* ptr) { tjFree(ptr); }
        };
        using JpegData = std::unique_ptr<uint8_t, tjDeleter>;
        JpegData data;
        unsigned long size{0};
    };

    ImageJPEG createJPEG();

private:
    RocketsPlugin& _parent;
    bool _processingImageJpeg = false;
    tjhandle _compressor{tjInitCompress()};

    void _resizeImage(unsigned int* srcData, const Vector2i& srcSize,
                      const Vector2i& dstSize, uints& dstData);
    ImageJPEG::JpegData _encodeJpeg(const uint32_t width, const uint32_t height,
                                    const uint8_t* rawData,
                                    const int32_t pixelFormat,
                                    unsigned long& dataSize);
};

/**
   The RocketsPlugin is in charge of exposing a both an http/REST interface to
   the outside world. The http server is configured according
   to the --http-server parameter provided by ApplicationParameters.
 */
class RocketsPlugin : public ExtensionPlugin
{
public:
    RocketsPlugin(ParametersManager& parametersManager);
    ~RocketsPlugin();

    /** @copydoc ExtensionPlugin::run */
    BRAYNS_API bool run(EngineWeakPtr engine, KeyboardHandler& keyboardHandler,
                        AbstractManipulator& cameraManipulator) final;

private:
    void _onNewEngine();
    void _onChangeEngine();

    void _setupHTTPServer();
    void _setupWebsocket();
    std::string _getHttpInterface() const;

    template <class T>
    void _handle(const std::string& endpoint, T& obj,
                 std::function<void(T& obj)> updateFunc = [](T& obj) {
                     obj.markModified();
                 });
    template <class T>
    void _handleGET(const std::string& endpoint, T& obj,
                    std::function<void()> pre = std::function<void()>());
    template <class T>
    void _handlePUT(const std::string& endpoint, T& obj,
                    std::function<void(T&)> updateFunc = [](T& obj) {
                        obj.markModified();
                    });
    template <class T>
    void _handleSchema(const std::string& endpoint, T& obj);

    void _remove(const std::string& endpoint);

    void _broadcastWebsocketMessages();
    rockets::ws::Response _processWebsocketMessage(const std::string& message);

    void _handleVersion();
    void _handleStreaming();
    void _handleImageJPEG();

    std::future<rockets::http::Response> _handleCircuitConfigBuilder(
        const rockets::http::Request&);

    bool _writeBlueConfigFile(const std::string& filename,
                              const std::map<std::string, std::string>& params);

    using WsIncomingMap =
        std::map<std::string, std::function<bool(const std::string&)>>;
    WsIncomingMap _wsIncoming;

    using WsOutgoingMap = std::map<std::string, std::function<std::string()>>;
    WsOutgoingMap _wsOutgoing;

    Engine* _engine = nullptr;
    ParametersManager& _parametersManager;

    std::unique_ptr<rockets::Server> _httpServer;

    bool _dirtyEngine = false;

    friend class ImageGenerator;
    ImageGenerator _imageGenerator{*this};

    class Timer
    {
    public:
        using clock = std::chrono::high_resolution_clock;

        void start() { _startTime = clock::now(); }
        void restart() { start(); }
        float elapsed()
        {
            return std::chrono::duration<float>{clock::now() - _startTime}
                .count();
        }

        Timer() { start(); }
    private:
        clock::time_point _startTime;
    } _timer;
};
}

#endif
