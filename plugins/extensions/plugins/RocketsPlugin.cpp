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

#include "RocketsPlugin.h"

#include <brayns/Brayns.h>
#include <brayns/common/camera/Camera.h>
#include <brayns/common/engine/Engine.h>
#include <brayns/common/renderer/FrameBuffer.h>
#include <brayns/common/renderer/Renderer.h>
#include <brayns/common/simulation/AbstractSimulationHandler.h>
#include <brayns/common/volume/VolumeHandler.h>
#include <brayns/io/simulation/CADiffusionSimulationHandler.h>
#include <brayns/parameters/ParametersManager.h>
#include <brayns/version.h>

#include <fstream>

#include "SDK.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"

namespace servus
{
inline std::string to_json(const Serializable& obj)
{
    return obj.toJSON();
}
inline bool from_json(Serializable& obj, const std::string& json)
{
    return obj.fromJSON(json);
}
}

namespace
{
const std::string ENDPOINT_API_VERSION = "v1/";
const std::string ENDPOINT_CAMERA = "camera";
const std::string ENDPOINT_DATA_SOURCE = "data-source";
const std::string ENDPOINT_FRAME_BUFFERS = "frame-buffers";
const std::string ENDPOINT_SCENE = "scene";
const std::string ENDPOINT_APP_PARAMS = "application-parameters";
const std::string ENDPOINT_GEOMETRY_PARAMS = "geometry-parameters";
const std::string ENDPOINT_RENDERING_PARAMS = "rendering-parameters";
const std::string ENDPOINT_SCENE_PARAMS = "scene-parameters";
const std::string ENDPOINT_VOLUME_PARAMS = "volume-parameters";
const std::string ENDPOINT_SIMULATION_HISTOGRAM = "simulation-histogram";
const std::string ENDPOINT_VOLUME_HISTOGRAM = "volume-histogram";
const std::string ENDPOINT_VERSION = "version";
const std::string ENDPOINT_PROGRESS = "progress";
const std::string ENDPOINT_FRAME = "frame";
const std::string ENDPOINT_IMAGE_JPEG = "image-jpeg";
const std::string ENDPOINT_MATERIAL_LUT = "material-lut";
const std::string ENDPOINT_CIRCUIT_CONFIG_BUILDER = "circuit-config-builder";
const std::string ENDPOINT_STREAM = "stream";
const std::string ENDPOINT_STREAM_TO = "stream-to";

const std::string JSON_TYPE = "application/json";

const size_t NB_MAX_MESSAGES = 20; // Maximum number of network messages to read
                                   // between each rendering loop

// JSON for websocket text messages
std::string buildJsonMessage(const std::string& event, const std::string data,
                             const bool error = false)
{
    rapidjson::Document message(rapidjson::kObjectType);

    rapidjson::Value eventJson;
    eventJson.SetString(event.c_str(), event.length(), message.GetAllocator());
    message.AddMember("event", eventJson, message.GetAllocator());

    rapidjson::Document dataJson(rapidjson::kObjectType);
    dataJson.Parse(data.c_str(), data.length());
    if (error)
        message.AddMember("error", dataJson.GetObject(),
                          message.GetAllocator());
    else
        message.AddMember("data", dataJson.GetObject(), message.GetAllocator());

    rapidjson::StringBuffer sb;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
    message.Accept(writer);
    return sb.GetString();
}

std::string hyphenatedToCamelCase(const std::string& scoreString)
{
    std::string camelString = scoreString;

    for (size_t x = 0; x < camelString.length(); x++)
    {
        if (camelString[x] == '-')
        {
            std::string tempString = camelString.substr(x + 1, 1);

            transform(tempString.begin(), tempString.end(), tempString.begin(),
                      toupper);

            camelString.erase(x, 2);
            camelString.insert(x, tempString);
        }
    }
    camelString[0] = toupper(camelString[0]);
    return camelString;
}

// get JSON schema from JSON-serializable object
template <class T>
std::string getSchema(T& obj, const std::string& title)
{
    rapidjson::StringBuffer buffer;
    buffer.Clear();

    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    auto schema = staticjson::export_json_schema(&obj);
    schema.AddMember(rapidjson::StringRef("title"),
                     rapidjson::StringRef(title.c_str()),
                     schema.GetAllocator());
    schema.Accept(writer);

    return buffer.GetString();
}
}

namespace brayns
{
template <class T>
inline std::string to_json(const T& obj)
{
    return staticjson::to_pretty_json_string(obj);
}
template <>
inline std::string to_json(const Version& obj)
{
    return obj.toJSON();
}
template <class T>
inline bool from_json(T& obj, const std::string& json,
                      std::function<void(T&)> updateFunc)
{
    const auto success =
        staticjson::from_json_string(json.c_str(), &obj, nullptr);
    if (updateFunc && success)
        updateFunc(obj);
    return success;
}

RocketsPlugin::RocketsPlugin(ParametersManager& parametersManager)
    : ExtensionPlugin()
    , _parametersManager(parametersManager)
    , _compressor(tjInitCompress())
{
    _setupHTTPServer();
}

RocketsPlugin::~RocketsPlugin()
{
    if (_compressor)
        tjDestroy(_compressor);
}

void RocketsPlugin::_onNewEngine()
{
    if (_httpServer)
    {
        _handle2(ENDPOINT_CAMERA, _engine->getCamera());
        _handleGET2(ENDPOINT_PROGRESS, _engine->getProgress());
        _handleGET2(ENDPOINT_FRAME_BUFFERS, _engine->getFrameBuffer());
        _handle2(ENDPOINT_MATERIAL_LUT,
                 _engine->getScene().getTransferFunction());
        _handle2(ENDPOINT_SCENE, _engine->getScene(),
                 std::function<void(Scene&)>([](Scene& scene) {
                     scene.markModified();
                     scene.commitMaterials(Action::update);
                 }));
    }

    _engine->extensionInit(*this);
    _dirtyEngine = false;
}

void RocketsPlugin::_onChangeEngine()
{
    if (_httpServer)
    {
        _remove(ENDPOINT_CAMERA);
        _remove(ENDPOINT_PROGRESS);
        _remove(ENDPOINT_FRAME_BUFFERS);
        _remove(ENDPOINT_MATERIAL_LUT);
        _remove(ENDPOINT_SCENE);
    }

    try
    {
        _engine->recreate();
    }
    catch (const std::runtime_error&)
    {
    }
}

bool RocketsPlugin::run(EngineWeakPtr engine_, KeyboardHandler&,
                        AbstractManipulator&)
{
    if (engine_.expired())
        return true;

    if (_engine != engine_.lock().get() || _dirtyEngine)
    {
        _engine = engine_.lock().get();
        _onNewEngine();
    }

    if (!_httpServer)
        return !_dirtyEngine;

    try
    {
        _broadcastWebsocketMessages();

        // In the case of interactions with Jupyter notebooks, HTTP messages are
        // received in a blocking and sequential manner, meaning that the
        // subscriber never has more than one message in its queue. In other
        // words, only one message is processed between each rendering loop. The
        // following code allows the processing of several messages and performs
        // rendering after NB_MAX_MESSAGES reads.
        for (size_t i = 0; i < NB_MAX_MESSAGES; ++i)
            _httpServer->process(0);
    }
    catch (const std::exception& exc)
    {
        BRAYNS_ERROR << "Error while handling HTTP/websocket messages: "
                     << exc.what() << std::endl;
    }

    return !_dirtyEngine;
}

void RocketsPlugin::_broadcastWebsocketMessages()
{
    if (_httpServer->getConnectionCount() == 0)
        return;

    if (_engine->isReady() && _engine->getCamera().getModified())
        _httpServer->broadcastText(_wsOutgoing[ENDPOINT_CAMERA]());

    if (_engine->getProgress().getModified())
        _httpServer->broadcastText(_wsOutgoing[ENDPOINT_PROGRESS]());

    if (_engine->isReady() && _engine->getRenderer().hasNewImage())
    {
        const auto fps =
            _parametersManager.getApplicationParameters().getImageStreamFPS();
        if (_timer.elapsed() < 1.f / fps)
            return;

        _timer.restart();

        const auto image = _createJPEG();
        if (image.size > 0)
            _httpServer->broadcastBinary((const char*)image.data.get(),
                                         image.size);
    }
}

rockets::ws::Response RocketsPlugin::_processWebsocketMessage(
    const std::string& message)
{
    try
    {
        rapidjson::Document jsonData;
        jsonData.Parse(message.c_str());
        const std::string event = jsonData["event"].GetString();
        auto i = _wsIncoming.find(event);
        if (i == _wsIncoming.end())
            return buildJsonMessage(event, "Unknown websocket event", true);

        rapidjson::StringBuffer sb;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
        jsonData["data"].Accept(writer);
        if (!i->second(sb.GetString()))
            return buildJsonMessage(event, "Could not update object", true);

        // re-broadcast to all other clients
        return rockets::ws::Response{message, rockets::ws::Recipient::others};
    }
    catch (const std::exception& exc)
    {
        BRAYNS_ERROR << "Error in websocket message handling: " << exc.what()
                     << std::endl;
        return buildJsonMessage("exception", exc.what(), true);
    }
}

void RocketsPlugin::_setupHTTPServer()
{
    try
    {
        _httpServer.reset(
            new rockets::Server{_getHttpInterface(), "rockets", 0});
        BRAYNS_INFO << "Registering http handlers on " << _httpServer->getURI()
                    << std::endl;
    }
    catch (const std::runtime_error& e)
    {
        BRAYNS_ERROR << "HTTP could not be initialized: '" << e.what() << "'"
                     << std::endl;
        return;
    }

    _setupWebsocket();

    _handleVersion();
    _handleStreaming();

    _handle2(ENDPOINT_APP_PARAMS, _parametersManager.getApplicationParameters(),
             std::function<void(ApplicationParameters&)>(
                 [this](ApplicationParameters& params) {
                     params.markModified();
                     if (params.getFrameExportFolder().empty())
                         _engine->resetFrameNumber();
                 }));
    _handle2(ENDPOINT_GEOMETRY_PARAMS,
             _parametersManager.getGeometryParameters(),
             std::function<void(GeometryParameters&)>(
                 [this](GeometryParameters& params) {
                     params.markModified();
                     if (_engine->isReady())
                         _engine->buildScene();
                 }));
    _handle2(ENDPOINT_RENDERING_PARAMS,
             _parametersManager.getRenderingParameters(),
             std::function<void(RenderingParameters&)>(
                 [this](RenderingParameters& params) {
                     params.markModified();
                     if (_engine->name() != params.getEngine())
                         _onChangeEngine();
                 }));
    _handle2(ENDPOINT_SCENE_PARAMS, _parametersManager.getSceneParameters());
    _handle2(ENDPOINT_VOLUME_PARAMS, _parametersManager.getVolumeParameters());

    _handleGET(ENDPOINT_IMAGE_JPEG, _remoteImageJPEG);
    _remoteImageJPEG.registerSerializeCallback([this] { _requestImageJPEG(); });

    _handle(ENDPOINT_FRAME, _remoteFrame);
    _remoteFrame.registerSerializeCallback([this] { _requestFrame(); });
    _remoteFrame.registerDeserializedCallback(
        std::bind(&RocketsPlugin::_frameUpdated, this));

    _httpServer->handle(rockets::http::Method::GET,
                        ENDPOINT_API_VERSION + ENDPOINT_CIRCUIT_CONFIG_BUILDER,
                        std::bind(&RocketsPlugin::_handleCircuitConfigBuilder,
                                  this, std::placeholders::_1));

    _handleGET(ENDPOINT_SIMULATION_HISTOGRAM, _remoteSimulationHistogram);
    _remoteSimulationHistogram.registerSerializeCallback(
        [this] { _requestSimulationHistogram(); });

    _handleGET(ENDPOINT_VOLUME_HISTOGRAM, _remoteVolumeHistogram);
    _remoteVolumeHistogram.registerSerializeCallback(
        [this] { _requestVolumeHistogram(); });
}

void RocketsPlugin::_setupWebsocket()
{
    _httpServer->handleOpen([this]() {
        std::vector<rockets::ws::Response> responses;
        for (auto& i : _wsOutgoing)
            responses.push_back({i.second(), rockets::ws::Recipient::sender,
                                 rockets::ws::Format::text});

        const auto image = _createJPEG();
        if (image.size > 0)
        {
            std::string message;
            message.assign((const char*)image.data.get(), image.size);
            responses.push_back({message, rockets::ws::Recipient::sender,
                                 rockets::ws::Format::binary});
        }
        return responses;
    });
    _httpServer->handleText(std::bind(&RocketsPlugin::_processWebsocketMessage,
                                      this, std::placeholders::_1));
}

std::string RocketsPlugin::_getHttpInterface() const
{
    const auto& params = _parametersManager.getApplicationParameters();
    const auto& args = params.arguments();
    for (int i = 0; i < (int)args.size() - 1; ++i)
    {
        if (args[i] == "--http-server" || args[i] == "--zeroeq-http-server")
            return args[i + 1];
    }
    return std::string();
}

template <class T>
void RocketsPlugin::_handle2(const std::string& endpoint, T& obj,
                             std::function<void(T&)> updateFunc)
{
    _handleGET2(endpoint, obj);
    _handlePUT2(endpoint, obj, updateFunc);
}

template <class T>
void RocketsPlugin::_handleGET2(const std::string& endpoint, T& obj)
{
    using namespace rockets::http;

    _httpServer->handle(Method::GET, ENDPOINT_API_VERSION + endpoint,
                        [&obj](const Request&) {
                            return make_ready_response(Code::OK, to_json(obj),
                                                       JSON_TYPE);
                        });

    _handleSchema2(endpoint, obj);

    _wsOutgoing[endpoint] = [&obj, endpoint] {
        return buildJsonMessage(endpoint, to_json(obj));
    };
}

template <class T>
void RocketsPlugin::_handlePUT2(const std::string& endpoint, T& obj,
                                std::function<void(T&)> updateFunc)
{
    using namespace rockets::http;
    _httpServer->handle(Method::PUT, ENDPOINT_API_VERSION + endpoint,
                        [&obj, updateFunc](const Request& req) {
                            return make_ready_response(
                                from_json(obj, req.body, updateFunc)
                                    ? Code::OK
                                    : Code::BAD_REQUEST);
                        });

    _handleSchema2(endpoint, obj);

    _wsIncoming[endpoint] = [&obj, updateFunc](const std::string& data) {
        return from_json(obj, data, updateFunc);
    };
}

template <class T>
void RocketsPlugin::_handleSchema2(const std::string& endpoint, T& obj)
{
    using namespace rockets::http;
    _httpServer->handle(Method::GET,
                        ENDPOINT_API_VERSION + endpoint + "/schema",
                        [&obj, endpoint](const Request&) {
                            return make_ready_response(
                                Code::OK,
                                getSchema(obj, hyphenatedToCamelCase(endpoint)),
                                JSON_TYPE);
                        });
}

void RocketsPlugin::_handle(const std::string& endpoint,
                            servus::Serializable& obj)
{
    _httpServer->handle(ENDPOINT_API_VERSION + endpoint, obj);
    _handleSchema(endpoint, obj);
    _handleWebsocketEvent(endpoint, obj);
}

void RocketsPlugin::_handleGET(const std::string& endpoint,
                               const servus::Serializable& obj)
{
    _httpServer->handleGET(ENDPOINT_API_VERSION + endpoint, obj);
    _handleSchema(endpoint, obj);
}

void RocketsPlugin::_handlePUT(const std::string& endpoint,
                               servus::Serializable& obj)
{
    _httpServer->handlePUT(ENDPOINT_API_VERSION + endpoint, obj);
    _handleSchema(endpoint, obj);
    _handleWebsocketEvent(endpoint, obj);
}

void RocketsPlugin::_handleSchema(const std::string& endpoint,
                                  const servus::Serializable& obj)
{
    using namespace rockets::http;
    _httpServer->handle(Method::GET,
                        ENDPOINT_API_VERSION + endpoint + "/schema",
                        [&obj](const Request&) {
                            return make_ready_response(Code::OK,
                                                       obj.getSchema(),
                                                       JSON_TYPE);
                        });
}

void RocketsPlugin::_remove(const std::string& endpoint)
{
    _httpServer->remove(ENDPOINT_API_VERSION + endpoint);
    _httpServer->remove(ENDPOINT_API_VERSION + endpoint + "/schema");
}

void RocketsPlugin::_handleWebsocketEvent(const std::string& endpoint,
                                          servus::Serializable& obj)
{
    _wsIncoming[endpoint] = [&obj](const std::string& data) {
        return obj.fromJSON(data);
    };
}

void RocketsPlugin::_handleVersion()
{
    static brayns::Version version;
    using namespace rockets::http;
    _httpServer->handleGET(ENDPOINT_API_VERSION + ENDPOINT_VERSION, version);
    _httpServer->handle(Method::GET,
                        ENDPOINT_API_VERSION + ENDPOINT_VERSION + "/schema",
                        [&](const Request&) {
                            return make_ready_response(Code::OK,
                                                       version.getSchema(),
                                                       JSON_TYPE);
                        });
}

void RocketsPlugin::_handleStreaming()
{
#if BRAYNS_USE_DEFLECT
    _handle2(ENDPOINT_STREAM, _parametersManager.getApplicationParameters()
                                  .getStreamParameters());
    _handlePUT2(
        ENDPOINT_STREAM_TO,
        _parametersManager.getApplicationParameters().getStreamParameters());
#else
    _handleGET2(ENDPOINT_STREAM, _parametersManager.getApplicationParameters()
                                     .getStreamParameters());
    using namespace rockets::http;
    auto respondNotImplemented = [](const Request&) {
        const auto message = "Brayns was not compiled with streaming support";
        return make_ready_response(Code::NOT_IMPLEMENTED, message);
    };
    _httpServer->handle(Method::PUT, ENDPOINT_STREAM, respondNotImplemented);
    _httpServer->handle(Method::PUT, ENDPOINT_STREAM_TO, respondNotImplemented);
#endif
}

void RocketsPlugin::_resizeImage(unsigned int* srcData, const Vector2i& srcSize,
                                 const Vector2i& dstSize, uints& dstData)
{
    dstData.reserve(dstSize.x() * dstSize.y());
    size_t x_ratio =
        static_cast<size_t>(((srcSize.x() << 16) / dstSize.x()) + 1);
    size_t y_ratio =
        static_cast<size_t>(((srcSize.y() << 16) / dstSize.y()) + 1);

    for (int y = 0; y < dstSize.y(); ++y)
    {
        for (int x = 0; x < dstSize.x(); ++x)
        {
            const size_t x2 = ((x * x_ratio) >> 16);
            const size_t y2 = ((y * y_ratio) >> 16);
            dstData[(y * dstSize.x()) + x] = srcData[(y2 * srcSize.x()) + x2];
        }
    }
}

bool RocketsPlugin::_requestImageJPEG()
{
    auto image = _createJPEG();
    if (image.size == 0)
        return false;

    _remoteImageJPEG.setData(image.data.get(), image.size);
    return true;
}

bool RocketsPlugin::_requestFrame()
{
    auto caSimHandler = _engine->getScene().getCADiffusionSimulationHandler();
    auto simHandler = _engine->getScene().getSimulationHandler();
    auto volHandler = _engine->getScene().getVolumeHandler();
    uint64_t nbFrames = simHandler
                            ? simHandler->getNbFrames()
                            : (volHandler ? volHandler->getNbFrames() : 0);
    nbFrames =
        std::max(nbFrames, caSimHandler ? caSimHandler->getNbFrames() : 0);

    const auto& sceneParams = _parametersManager.getSceneParameters();
    const auto current =
        nbFrames == 0 ? 0 : (sceneParams.getAnimationFrame() % nbFrames);
    const auto animationDelta = sceneParams.getAnimationDelta();

    if (current == _remoteFrame.getCurrent() &&
        nbFrames == _remoteFrame.getEnd() &&
        animationDelta == _remoteFrame.getDelta())
    {
        return false;
    }

    _remoteFrame.setCurrent(current);
    _remoteFrame.setDelta(animationDelta);
    _remoteFrame.setEnd(nbFrames);
    _remoteFrame.setStart(0);
    return true;
}

void RocketsPlugin::_frameUpdated()
{
    auto& sceneParams = _parametersManager.getSceneParameters();
    sceneParams.setAnimationFrame(_remoteFrame.getCurrent());
    sceneParams.setAnimationDelta(_remoteFrame.getDelta());

    CADiffusionSimulationHandlerPtr handler =
        _engine->getScene().getCADiffusionSimulationHandler();
    if (handler && _engine->isReady())
    {
        auto& scene = _engine->getScene();
        handler->setFrame(scene, _remoteFrame.getCurrent());
        scene.setSpheresDirty(true);
        scene.serializeGeometry();
        scene.commit();
    }
}

bool RocketsPlugin::_requestSimulationHistogram()
{
    auto simulationHandler = _engine->getScene().getSimulationHandler();
    if (!simulationHandler || !simulationHandler->histogramChanged())
        return false;

    const auto& histogram = simulationHandler->getHistogram();
    _remoteSimulationHistogram.setMin(histogram.range.x());
    _remoteSimulationHistogram.setMax(histogram.range.y());
    _remoteSimulationHistogram.setBins(histogram.values);
    return true;
}

bool RocketsPlugin::_requestVolumeHistogram()
{
    auto volumeHandler = _engine->getScene().getVolumeHandler();
    if (!volumeHandler)
        return false;

    const auto& histogram = volumeHandler->getHistogram();
    _remoteVolumeHistogram.setMin(histogram.range.x());
    _remoteVolumeHistogram.setMax(histogram.range.y());
    _remoteVolumeHistogram.setBins(histogram.values);
    return true;
}

std::future<rockets::http::Response> RocketsPlugin::_handleCircuitConfigBuilder(
    const rockets::http::Request& request)
{
    using namespace rockets::http;

    const auto& params = _parametersManager.getApplicationParameters();
    const auto filename = params.getTmpFolder() + "/BlueConfig";
    if (_writeBlueConfigFile(filename, request.query))
    {
        const std::string body = "{\"filename\":\"" + filename + "\"}";
        return make_ready_response(Code::OK, body, JSON_TYPE);
    }
    return make_ready_response(Code::SERVICE_UNAVAILABLE);
}

RocketsPlugin::JpegData RocketsPlugin::_encodeJpeg(const uint32_t width,
                                                   const uint32_t height,
                                                   const uint8_t* rawData,
                                                   const int32_t pixelFormat,
                                                   unsigned long& dataSize)
{
    uint8_t* tjSrcBuffer = const_cast<uint8_t*>(rawData);
    const int32_t color_components = 4; // Color Depth
    const int32_t tjPitch = width * color_components;
    const int32_t tjPixelFormat = pixelFormat;

    uint8_t* tjJpegBuf = 0;
    const int32_t tjJpegSubsamp = TJSAMP_444;
    const int32_t tjFlags = TJXOP_ROT180;

    const int32_t success = tjCompress2(
        _compressor, tjSrcBuffer, width, tjPitch, height, tjPixelFormat,
        &tjJpegBuf, &dataSize, tjJpegSubsamp,
        _parametersManager.getApplicationParameters().getJpegCompression(),
        tjFlags);

    if (success != 0)
    {
        BRAYNS_ERROR << "libjpeg-turbo image conversion failure" << std::endl;
        return 0;
    }
    return JpegData{tjJpegBuf};
}

RocketsPlugin::ImageJPEG RocketsPlugin::_createJPEG()
{
    if (_processingImageJpeg)
        return ImageJPEG();

    _processingImageJpeg = true;
    const auto& newFrameSize =
        _parametersManager.getApplicationParameters().getJpegSize();
    if (newFrameSize.x() == 0 || newFrameSize.y() == 0)
    {
        BRAYNS_ERROR << "Encountered invalid size of image JPEG: "
                     << newFrameSize << std::endl;

        return ImageJPEG();
    }

    FrameBuffer& frameBuffer = _engine->getFrameBuffer();
    const auto& frameSize = frameBuffer.getSize();
    unsigned int* colorBuffer = (unsigned int*)frameBuffer.getColorBuffer();
    if (!colorBuffer)
        return ImageJPEG();

    unsigned int* resizedColorBuffer = colorBuffer;

    uints resizedBuffer;
    if (frameSize != newFrameSize)
    {
        _resizeImage(colorBuffer, frameSize, newFrameSize, resizedBuffer);
        resizedColorBuffer = resizedBuffer.data();
    }

    int32_t pixelFormat = TJPF_RGBX;
    switch (frameBuffer.getFrameBufferFormat())
    {
    case FrameBufferFormat::bgra_i8:
        pixelFormat = TJPF_BGRX;
        break;
    case FrameBufferFormat::rgba_i8:
    default:
        pixelFormat = TJPF_RGBX;
    }

    ImageJPEG image;
    image.data =
        _encodeJpeg((uint32_t)newFrameSize.x(), (uint32_t)newFrameSize.y(),
                    (uint8_t*)resizedColorBuffer, pixelFormat, image.size);
    _processingImageJpeg = false;
    return image;
}

bool RocketsPlugin::_writeBlueConfigFile(
    const std::string& filename,
    const std::map<std::string, std::string>& params)
{
    std::ofstream blueConfig(filename);
    if (!blueConfig.good())
        return false;

    std::map<std::string, std::string> dictionary = {
        {"morphology_folder", "MorphologyPath"}, {"mvd_file", "CircuitPath"}};

    blueConfig << "Run Default" << std::endl << "{" << std::endl;
    for (const auto& kv : params)
    {
        if (dictionary.find(kv.first) == dictionary.end())
        {
            BRAYNS_ERROR << "BlueConfigBuilder: Unknown parameter " << kv.first
                         << std::endl;
            continue;
        }
        blueConfig << dictionary[kv.first] << " " << kv.second << std::endl;
    }
    blueConfig << "}" << std::endl;
    blueConfig.close();
    return true;
}
}
