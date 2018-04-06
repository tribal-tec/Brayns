/* Copyright (c) 2015-2018, EPFL/Blue Brain Project
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

// needs to be before RocketsPlugin.h to make template instantiation for
// _handleRPC work
#include "jsonSerialization.h"

#include "RocketsPlugin.h"

#include "jsonUtils.h"

#include <brayns/common/Timer.h>
#include <brayns/common/tasks/ReceiveBinaryTask.h>
#include <brayns/common/tasks/Task.h>
#include <brayns/common/volume/VolumeHandler.h>
#include <brayns/pluginapi/PluginAPI.h>

#include <fstream>
#include <rockets/jsonrpc/helpers.h>

#ifdef BRAYNS_USE_LIBUV
#include "SocketListener.h"
#endif

#include <rockets/jsonrpc/asyncReceiver.h>
#include <rockets/jsonrpc/server.h>
#include <rockets/server.h>

#include "ImageGenerator.h"

namespace
{
const std::string ENDPOINT_API_VERSION = "v1/";
const std::string ENDPOINT_APP_PARAMS = "application-parameters";
const std::string ENDPOINT_CAMERA = "camera";
const std::string ENDPOINT_CIRCUIT_CONFIG_BUILDER = "circuit-config-builder";
const std::string ENDPOINT_DATA_SOURCE = "data-source";
const std::string ENDPOINT_FRAME = "frame";
const std::string ENDPOINT_FRAME_BUFFERS = "frame-buffers";
const std::string ENDPOINT_GEOMETRY_PARAMS = "geometry-parameters";
const std::string ENDPOINT_IMAGE_JPEG = "image-jpeg";
const std::string ENDPOINT_MATERIAL_LUT = "material-lut";
const std::string ENDPOINT_PROGRESS = "progress";
const std::string ENDPOINT_RENDERING_PARAMS = "rendering-parameters";
const std::string ENDPOINT_SCENE = "scene";
const std::string ENDPOINT_SCENE_PARAMS = "scene-parameters";
const std::string ENDPOINT_SIMULATION_HISTOGRAM = "simulation-histogram";
const std::string ENDPOINT_STATISTICS = "statistics";
const std::string ENDPOINT_STREAM = "stream";
const std::string ENDPOINT_STREAM_TO = "stream-to";
const std::string ENDPOINT_VERSION = "version";
const std::string ENDPOINT_VOLUME_HISTOGRAM = "volume-histogram";
const std::string ENDPOINT_VOLUME_PARAMS = "volume-parameters";

const std::string METHOD_INSPECT = "inspect";
const std::string METHOD_QUIT = "quit";
const std::string METHOD_RECEIVE_BINARY = "receive-binary";
const std::string METHOD_RESET_CAMERA = "reset-camera";
const std::string METHOD_SNAPSHOT = "snapshot";

const std::string JSON_TYPE = "application/json";

using Response = rockets::jsonrpc::Response;
const Response ALREADY_PENDING_REQUEST{
    Response::Error{"Already pending binary request", -1730}};
// const Response MISSING_PARAMS{Response::Error{"Missing params", -1731}};
// Response UNSUPPORTED_TYPE(const brayns::BinaryError& error)
//{
//    return {Response::Error{"Unsupported type", -1732, to_json(error)}};
//}
// const Response INVALID_BINARY_RECEIVE{
//    Response::Error{"Invalid binary received; no more files expected or "
//                    "current file is complete",
//                    -1733}};
// Response LOADING_BINARY_FAILED(const std::string& error)
//{
//    return {Response::Error{error, -1734}};
//}
// const Response SNAPSHOT_PENDING{
//    Response::Error{"Snapshot is pending, rejecting data upload", -1735}};

std::string hyphenatedToCamelCase(const std::string& hyphenated)
{
    std::string camel = hyphenated;

    for (size_t x = 0; x < camel.length(); x++)
    {
        if (camel[x] == '-')
        {
            std::string tempString = camel.substr(x + 1, 1);

            transform(tempString.begin(), tempString.end(), tempString.begin(),
                      toupper);

            camel.erase(x, 2);
            camel.insert(x, tempString);
        }
    }
    camel[0] = toupper(camel[0]);
    return camel;
}
}

namespace brayns
{
template <class T, class F>
inline bool from_json(T& obj, const std::string& json, F postUpdateFunc = [] {})
{
    staticjson::ParseStatus status;
    const auto success =
        staticjson::from_json_string(json.c_str(), &obj, &status);
    if (success)
    {
        obj.markModified();
        if (std::function<void(T&)>(postUpdateFunc))
            postUpdateFunc(obj);
    }
    else
        BRAYNS_ERROR << status.description() << std::endl;
    return success;
}

class RocketsPlugin::Impl
{
public:
    Impl(EnginePtr engine, PluginAPI* api)
        : _engine(engine)
        , _parametersManager(api->getParametersManager())
    {
        _setupRocketsServer();
    }

    ~Impl()
    {
        if (_rocketsServer)
            _rocketsServer->setSocketListener(nullptr);
    }

    void preRender()
    {
        if (!_rocketsServer || _socketListener)
            return;

        // https://github.com/BlueBrain/Brayns/issues/342
        // WAR: modifications by braynsViewer have to be broadcasted. Don't do
        // this for braynsService, as otherwise messages that arrive while we're
        // rendering (async rendering!) are re-broadcasted.
        _broadcastWebsocketMessages();

        try
        {
            _rocketsServer->process(0);
        }
        catch (const std::exception& exc)
        {
            BRAYNS_ERROR << "Error while handling HTTP/websocket messages: "
                         << exc.what() << std::endl;
        }
    }

    void _broadcastBinaryRequestsProgress()
    {
        //        for (auto& i : _binaryRequests)
        //        {
        //            auto& request = i.second;
        //            if (request->progress.isModified())
        //            {
        //                _jsonrpcServer->notify(ENDPOINT_PROGRESS,
        //                request->progress);
        //                request->progress.resetModified();
        //            }
        //        }
    }

    void postRender()
    {
        if (!_rocketsServer || _rocketsServer->getConnectionCount() == 0)
            return;

        // only broadcast changes that are a result of the rendering. All other
        // changes are already broadcasted in preRender().
        _wsBroadcastOperations[ENDPOINT_FRAME]();
        _wsBroadcastOperations[ENDPOINT_IMAGE_JPEG]();
        _wsBroadcastOperations[ENDPOINT_PROGRESS]();
        _wsBroadcastOperations[ENDPOINT_STATISTICS]();

        _broadcastBinaryRequestsProgress();
    }

    void postSceneLoading()
    {
        if (!_rocketsServer)
            return;

        _wsBroadcastOperations[ENDPOINT_CAMERA]();
        _wsBroadcastOperations[ENDPOINT_PROGRESS]();
        _wsBroadcastOperations[ENDPOINT_STATISTICS]();

        _broadcastBinaryRequestsProgress();
    }

    std::string _getHttpInterface() const
    {
        const auto& params = _parametersManager.getApplicationParameters();
        const auto& args = params.arguments();
        for (int i = 0; i < (int)args.size() - 1; ++i)
        {
            if (args[i] == "--http-server")
                return args[i + 1];
        }
        return std::string();
    }

    void _setupRocketsServer()
    {
        try
        {
            _rocketsServer.reset(
                new rockets::Server{_getHttpInterface(), "rockets", 0});
            BRAYNS_INFO << "Rockets server running on "
                        << _rocketsServer->getURI() << std::endl;

            _jsonrpcServer.reset(new JsonRpcServer(*_rocketsServer));

#ifdef BRAYNS_USE_LIBUV
            try
            {
                _socketListener =
                    std::make_unique<SocketListener>(*_rocketsServer);
                //_rocketsServer->setSocketListener(_socketListener.get());
            }
            catch (const std::runtime_error& e)
            {
                BRAYNS_DEBUG
                    << "Failed to setup rockets socket listener: " << e.what()
                    << std::endl;
            }
#endif

            _parametersManager.getApplicationParameters().setHttpServerURI(
                _rocketsServer->getURI());
        }
        catch (const std::runtime_error& e)
        {
            BRAYNS_ERROR << "Rockets server could not be initialized: '"
                         << e.what() << "'" << std::endl;
            return;
        }

        _setupWebsocket();
        _registerEndpoints();
        _timer.start();
    }

    void _setupWebsocket()
    {
        _rocketsServer->handleOpen([this](const uintptr_t) {
            std::vector<rockets::ws::Response> responses;
            for (auto& i : _wsClientConnectNotifications)
                responses.push_back({i.second(), rockets::ws::Recipient::sender,
                                     rockets::ws::Format::text});

            const auto image =
                _imageGenerator.createJPEG(_engine->getFrameBuffer(),
                                           _parametersManager
                                               .getApplicationParameters()
                                               .getJpegCompression());
            if (image.size > 0)
            {
                std::string message;
                message.assign((const char*)image.data.get(), image.size);
                responses.push_back({message, rockets::ws::Recipient::sender,
                                     rockets::ws::Format::binary});
            }
            return responses;
        });

        _rocketsServer->handleClose([this](const uintptr_t clientID) {
            //_deleteBinaryRequest(clientID);
            return std::vector<rockets::ws::Response>{};
        });

        _rocketsServer->handleBinary(
            std::bind(&Impl::_processWebsocketBinaryMessage, this,
                      std::placeholders::_1));
    }

    rockets::ws::Response _processWebsocketBinaryMessage(
        const rockets::ws::Request& wsRequest)
    {
        if (_binaryRequests.count(wsRequest.clientID) == 0)
        {
            BRAYNS_ERROR << "Missing RPC " << METHOD_RECEIVE_BINARY
                         << " or cancelled?" << std::endl;
            return {};
        }

        auto request = std::dynamic_pointer_cast<ReceiveBinaryTask>(
            _binaryRequests[wsRequest.clientID]);
        if (request)
            request->appendBlob(wsRequest.message);
        return {};
#if 0
        if (!request->valid())
        {
            request->finish();
            _deleteBinaryRequest(wsRequest.clientID);
            return {};
        }

        request->appendChunk(wsRequest.message);
        request->params[0].size -= wsRequest.message.size();

        // TODO: wrong timer again (leftover!), plus duplicate from
        // braynsService
        if (_timer2.elapsed() >= 0.01)
        {
            if (request->progress.isModified())
            {
                _jsonrpcServer->notify(ENDPOINT_PROGRESS, request->progress);
                request->progress.resetModified();
            }
            _timer2.start();
        }

        if (request->params[0].size == 0)
        {
            if (request->params.size() == 1)
            {
                // last file received, start loading
                // TODO: what do we do for multiple files? now we just load the
                // last received one. Adding one model per file would be the
                // best
                request->fullyReceived();
                _engine->rebuildSceneFromBlob(
                    {request->params[0].type, std::move(request->data),
                     &request->progress,
                     [request] { return request->cancelled(); }},
                    [ this, request, clientID = wsRequest.clientID ](
                        const std::string& error) {
                        request->finish(error);
                        _deleteBinaryRequest(clientID);
                    });
            }
            else
            {
                // prepare receiving of next file
                request->data.clear(); // TODO: might not be needed if moved
                                       // before
                request->data.reserve(request->params[1].size);
            }
            request->params.pop_front();
        }

        return {};
#endif
    }

    void _broadcastWebsocketMessages()
    {
        if (_rocketsServer->getConnectionCount() == 0)
            return;

        for (auto& op : _wsBroadcastOperations)
            op.second();
    }

    template <class T, class F>
    void _handleGET(const std::string& endpoint, T& obj, F modifiedFunc)
    {
        using namespace rockets::http;

        _rocketsServer->handle(Method::GET, ENDPOINT_API_VERSION + endpoint,
                               [&obj](const Request&) {
                                   return make_ready_response(Code::OK,
                                                              to_json(obj),
                                                              JSON_TYPE);
                               });

        _handleObjectSchema(endpoint, obj);

        _wsClientConnectNotifications[endpoint] = [&obj, endpoint] {
            return rockets::jsonrpc::makeNotification(endpoint, obj);
        };

        _wsBroadcastOperations[endpoint] = [this, &obj, endpoint,
                                            modifiedFunc] {
            if (modifiedFunc(obj))
                _jsonrpcServer->notify(endpoint, obj);
        };
    }

    template <class T>
    void _handleGET(const std::string& endpoint, T& obj)
    {
        _handleGET(endpoint, obj, [](const T& o) { return o.isModified(); });
    }

    template <class T>
    void _handlePUT(const std::string& endpoint, T& obj)
    {
        _handlePUT(endpoint, obj, std::function<void(T&)>());
    }

    template <class T, class F>
    void _handlePUT(const std::string& endpoint, T& obj, F postUpdateFunc)
    {
        using namespace rockets::http;
        _rocketsServer->handle(Method::PUT, ENDPOINT_API_VERSION + endpoint,
                               [&obj, postUpdateFunc](const Request& req) {
                                   return make_ready_response(
                                       from_json(obj, req.body, postUpdateFunc)
                                           ? Code::OK
                                           : Code::BAD_REQUEST);
                               });

        _handleObjectSchema(endpoint, obj);

        _jsonrpcServer->bind(endpoint, [this, endpoint, &obj, postUpdateFunc](
                                           rockets::jsonrpc::Request request) {
            if (from_json(obj, request.message, postUpdateFunc))
            {
                _engine->triggerRender();

                const auto& msg =
                    rockets::jsonrpc::makeNotification(endpoint, obj);
                _rocketsServer->broadcastText(msg, {request.clientID});
                return rockets::jsonrpc::Response{"null"};
            }
            return rockets::jsonrpc::Response::invalidParams();
        });
    }

    template <class T>
    void _handle(const std::string& endpoint, T& obj)
    {
        _handleGET(endpoint, obj);
        _handlePUT(endpoint, obj);
    }

    template <class P, class R>
    void _handleRPC(const std::string& method, const RpcDocumentation& doc,
                    std::function<R(P)> action)
    {
        _jsonrpcServer->bind<P, R>(method, action);
        _handleSchema(method, buildJsonRpcSchema<P, R>(method, doc));
    }

    void _handleRPC(const std::string& method, const std::string& description,
                    std::function<void()> action)
    {
        _jsonrpcServer->connect(method, action);
        _handleSchema(method, buildJsonRpcSchema(method, description));
    }

    template <class P, class R>
    void _handleAsyncRPC(
        const std::string& method, const RpcDocumentation& doc,
        std::function<void(P, std::string, uintptr_t,
                           rockets::jsonrpc::AsyncResponse)>
            action,
        rockets::jsonrpc::AsyncReceiver::CancelRequestCallback cancel)
    {
        _jsonrpcServer->bindAsync<P>(method, action, cancel);
        _handleSchema(method, buildJsonRpcSchema<P, R>(method, doc));
    }

    template <class P, class R>
    void _handleTask(
        const std::string& method, const RpcDocumentation& doc,
        std::function<std::shared_ptr<SimpleTask<R>>(P)> createUserTask)
    {
        _timer2.start();
        auto progressCallback =
            [& server = _jsonrpcServer, &timer = _timer2 ](Progress2 & progress)
        {
            // TODO: wrong timer again (leftover!), plus duplicate from
            // braynsService
            if (timer.elapsed() >= 0.01)
            {
                if (progress.isModified())
                {
                    server->notify(ENDPOINT_PROGRESS, progress);
                    progress.resetModified();
                }
                timer.start();
            }
        };

        auto action =
            [& tasks = _tasks, &binaryRequests = _binaryRequests, createUserTask,
             progressCallback ](P params, std::string requestID, uintptr_t clientID,
                                rockets::jsonrpc::AsyncResponse respond)
        {
            auto errorCallback = [respond](const TaskRuntimeError& error) {
                respond(Response{
                    Response::Error{error.what(), error.code(), error.data()}});

            };

            try
            {
                auto readyCallback = [respond](R result) {
                    try
                    {
                        respond(Response{to_json(result)});
                    }
                    catch (const std::runtime_error& e)
                    {
                        respond(Response{Response::Error{e.what(), -1}});
                    }
                };

                auto userTask = createUserTask(params);
                auto task = userTask->task().then(
                    [readyCallback, errorCallback, &tasks, &binaryRequests,
                     requestID, clientID](typename SimpleTask<R>::Type task2) {
                        // TODO
                        // progress.setAmount(1.f);

                        try
                        {
                            readyCallback(task2.get());
                        }
                        catch (const TaskRuntimeError& e)
                        {
                            errorCallback(e);
                        }
                        catch (const std::exception& e)
                        {
                            errorCallback({e.what()});
                        }
                        catch (const async::task_canceled&)
                        {
                            // no response needed
                            std::cout << "Cancelled" << std::endl;
                        }
                        tasks.erase(requestID);
                        binaryRequests.erase(clientID);
                    });

                userTask->setRequestID(requestID);
                userTask->setProgressUpdatedCallback(progressCallback);

                userTask->schedule();

                tasks.emplace(requestID,
                              std::make_pair(std::move(task), userTask));

                // TODO: check is_same receiveBinaryTask; needs generic
                // signature here!
                binaryRequests.emplace(clientID, userTask);
            }
            catch (const TaskRuntimeError& e)
            {
                errorCallback(e);
            }
            catch (const std::exception& e)
            {
                errorCallback({e.what()});
            }
        };
        auto cancel = [& tasks = _tasks](const std::string& requestID)
        {
            auto i = tasks.find(requestID);
            if (i != tasks.end())
            {
                i->second.second->cancel();
                i->second.first.wait();
            }
        };
        _handleAsyncRPC<P, R>(method, doc, action, cancel);
    }

    template <class T>
    void _handleObjectSchema(const std::string& endpoint, T& obj)
    {
        _handleSchema(endpoint,
                      getSchema(obj, hyphenatedToCamelCase(endpoint)));
    }

    void _handleSchema(const std::string& endpoint, const std::string& schema)
    {
        using namespace rockets::http;
        _rocketsServer->handle(Method::GET,
                               ENDPOINT_API_VERSION + endpoint + "/schema",
                               [schema](const Request&) {
                                   return make_ready_response(Code::OK, schema,
                                                              JSON_TYPE);
                               });
    }

    void _registerEndpoints()
    {
        _handleGeometryParams();
        _handleImageJPEG();
        _handleStreaming();
        _handleVersion();
        _handleVolumeParams();

        _handle(ENDPOINT_APP_PARAMS,
                _parametersManager.getApplicationParameters());
        _handle(ENDPOINT_FRAME, _parametersManager.getAnimationParameters());
        _handle(ENDPOINT_RENDERING_PARAMS,
                _parametersManager.getRenderingParameters());
        _handle(ENDPOINT_SCENE_PARAMS, _parametersManager.getSceneParameters());

        _rocketsServer->handle(rockets::http::Method::GET,
                               ENDPOINT_API_VERSION +
                                   ENDPOINT_CIRCUIT_CONFIG_BUILDER,
                               std::bind(&Impl::_handleCircuitConfigBuilder,
                                         this, std::placeholders::_1));

        // following endpoints need a valid engine
        _handle(ENDPOINT_CAMERA, _engine->getCamera());
        _handleGET(ENDPOINT_PROGRESS, _engine->getProgress());
        _handle(ENDPOINT_MATERIAL_LUT,
                _engine->getScene().getTransferFunction());
        _handleGET(ENDPOINT_SCENE, _engine->getScene());
        _handlePUT(ENDPOINT_SCENE, _engine->getScene(),
                   [](Scene& scene) { scene.commitMaterials(Action::update); });
        _handleGET(ENDPOINT_STATISTICS, _engine->getStatistics());

        _handleFrameBuffer();
        _handleSimulationHistogram();
        _handleVolumeHistogram();

        _handleInspect();
        _handleQuit();
        _handleResetCamera();
        _handleSnapshot();

        _handleReceiveBinary();
    }

    void _handleFrameBuffer()
    {
        // don't add framebuffer to websockets for performance
        using namespace rockets::http;
        _rocketsServer->handleGET(ENDPOINT_API_VERSION + ENDPOINT_FRAME_BUFFERS,
                                  _engine->getFrameBuffer());
        _handleObjectSchema(ENDPOINT_FRAME_BUFFERS, _engine->getFrameBuffer());
    }

    void _handleGeometryParams()
    {
        auto& params = _parametersManager.getGeometryParameters();
        auto postUpdate = [this](GeometryParameters&) {
            _engine->markRebuildScene();
        };
        _handleGET(ENDPOINT_GEOMETRY_PARAMS, params);
        _handlePUT(ENDPOINT_GEOMETRY_PARAMS, params, postUpdate);
    }

    void _handleImageJPEG()
    {
        using namespace rockets::http;

        auto func = [&](const Request&) {
            try
            {
                const auto obj =
                    _imageGenerator.createImage(_engine->getFrameBuffer(),
                                                "jpg",
                                                _parametersManager
                                                    .getApplicationParameters()
                                                    .getJpegCompression());
                return make_ready_response(Code::OK, to_json(obj), JSON_TYPE);
            }
            catch (const std::runtime_error& e)
            {
                return make_ready_response(Code::BAD_REQUEST, e.what());
            }
        };
        _rocketsServer->handle(Method::GET,
                               ENDPOINT_API_VERSION + ENDPOINT_IMAGE_JPEG,
                               func);

        _rocketsServer->handle(
            Method::GET, ENDPOINT_API_VERSION + ENDPOINT_IMAGE_JPEG + "/schema",
            [&](const Request&) {
                ImageGenerator::ImageBase64 obj;
                return make_ready_response(
                    Code::OK,
                    getSchema(obj, hyphenatedToCamelCase(ENDPOINT_IMAGE_JPEG)),
                    JSON_TYPE);
            });

        _wsBroadcastOperations[ENDPOINT_IMAGE_JPEG] = [this] {
            if (_engine->getFrameBuffer().isModified())
            {
                const auto& params =
                    _parametersManager.getApplicationParameters();
                const auto fps = params.getImageStreamFPS();
                const auto elapsed = _timer.elapsed() + _leftover;
                const auto duration = 1.0 / fps;
                if (elapsed < duration)
                    return;

                _leftover = elapsed - duration;
                for (; _leftover > duration;)
                    _leftover -= duration;
                _timer.start();

                const auto image =
                    _imageGenerator.createJPEG(_engine->getFrameBuffer(),
                                               params.getJpegCompression());
                if (image.size > 0)
                    _rocketsServer->broadcastBinary(
                        (const char*)image.data.get(), image.size);
            }
        };
    }

    void _handleSimulationHistogram()
    {
        Histogram tmp;
        _handleObjectSchema(ENDPOINT_SIMULATION_HISTOGRAM, tmp);

        using namespace rockets::http;

        auto func = [this](const Request&) {
            auto simulationHandler = _engine->getScene().getSimulationHandler();
            if (!simulationHandler)
                return make_ready_response(Code::NOT_SUPPORTED);
            const auto& histo = simulationHandler->getHistogram();
            return make_ready_response(Code::OK, to_json(histo), JSON_TYPE);
        };
        _rocketsServer->handle(Method::GET, ENDPOINT_API_VERSION +
                                                ENDPOINT_SIMULATION_HISTOGRAM,
                               func);
    }

    void _handleStreaming()
    {
#if BRAYNS_USE_DEFLECT
        _handle(ENDPOINT_STREAM, _parametersManager.getStreamParameters());
        _handlePUT(ENDPOINT_STREAM_TO,
                   _parametersManager.getStreamParameters());
#else
        _handleGET(ENDPOINT_STREAM, _parametersManager.getStreamParameters());
        using namespace rockets::http;
        auto respondNotImplemented = [](const Request&) {
            const auto message =
                "Brayns was not compiled with streaming support";
            return make_ready_response(Code::NOT_IMPLEMENTED, message);
        };
        _rocketsServer->handle(Method::PUT, ENDPOINT_STREAM,
                               respondNotImplemented);
        _rocketsServer->handle(Method::PUT, ENDPOINT_STREAM_TO,
                               respondNotImplemented);
#endif
    }

    void _handleVersion()
    {
        static brayns::Version version;
        using namespace rockets::http;
        _rocketsServer->handleGET(ENDPOINT_API_VERSION + ENDPOINT_VERSION,
                                  version);
        _rocketsServer->handle(
            Method::GET, ENDPOINT_API_VERSION + ENDPOINT_VERSION + "/schema",
            [&](const Request&) {
                return make_ready_response(Code::OK, version.getSchema(),
                                           JSON_TYPE);
            });
        _wsClientConnectNotifications[ENDPOINT_VERSION] = [] {
            return rockets::jsonrpc::makeNotification(ENDPOINT_VERSION,
                                                      version);
        };
    }

    void _handleVolumeHistogram()
    {
        Histogram tmp;
        _handleObjectSchema(ENDPOINT_VOLUME_HISTOGRAM, tmp);

        using namespace rockets::http;

        auto func = [this](const Request&) {
            auto volumeHandler = _engine->getScene().getVolumeHandler();
            if (!volumeHandler)
                return make_ready_response(Code::NOT_SUPPORTED);
            const auto& histo = volumeHandler->getHistogram();
            return make_ready_response(Code::OK, to_json(histo), JSON_TYPE);
        };

        _rocketsServer->handle(Method::GET,
                               ENDPOINT_API_VERSION + ENDPOINT_VOLUME_HISTOGRAM,
                               func);
    }

    void _handleVolumeParams()
    {
        auto& params = _parametersManager.getVolumeParameters();
        auto postUpdate = [this](VolumeParameters&) {
            _engine->markRebuildScene();
        };
        _handleGET(ENDPOINT_VOLUME_PARAMS, params);
        _handlePUT(ENDPOINT_VOLUME_PARAMS, params, postUpdate);
    }

    void _handleInspect()
    {
        using Position = std::array<float, 2>;
        RpcDocumentation doc{"Inspect the scene at x-y position", "position",
                             "x-y position in normalized coordinates"};
        _handleRPC<Position, Renderer::PickResult>(
            METHOD_INSPECT, doc, [this](const Position& position) {
                return _engine->getRenderer().pick(
                    Vector2f(position[0], position[1]));
            });
    }

    void _handleQuit()
    {
        _handleRPC(METHOD_QUIT, "Quit the application", [engine = _engine] {
            engine->setKeepRunning(false);
            engine->triggerRender();
        });
    }

    void _handleResetCamera()
    {
        _handleRPC(METHOD_RESET_CAMERA,
                   "Resets the camera to its initial values", [this] {
                       _engine->getCamera().reset();
                       _jsonrpcServer->notify(ENDPOINT_CAMERA,
                                              _engine->getCamera());
                       _engine->triggerRender();
                   });
    }

    // Forwarded to plugins?
    void _handleSnapshot()
    {
        RpcDocumentation doc{"Make a snapshot of the current view", "settings",
                             "Snapshot settings for quality and size"};
        _handleTask<SnapshotParams, ImageGenerator::ImageBase64>(
            METHOD_SNAPSHOT, doc,
            std::bind(createSnapshotTask, std::placeholders::_1,
                      std::ref(*_engine), std::ref(_imageGenerator)));
    }
    // TODO: executor of size 1 for binary tasks, so multiple ones can be
    // scheduled w/o rejecting them, but only 1 will be executed at a time. so
    // task API must be enhanced with a custom executor that lives here.
    // EDIT: still needed? the tasks are just waiting for data, so all should be
    // fine
    void _handleReceiveBinary()
    {
        RpcDocumentation doc{"Start sending of files", "params",
                             "List of file parameter: size and type"};

        _handleTask<BinaryParams, bool>(
            METHOD_RECEIVE_BINARY, doc,
            std::bind(createReceiveBinaryTask, std::placeholders::_1,
                      _parametersManager.getGeometryParameters()
                          .getSupportedDataTypes(),
                      _engine));

#if 0
        using Params = std::vector<BinaryParam>;
        _handleAsyncRPC<Params, bool>(
            METHOD_RECEIVE_BINARY, doc,
            [& requests = _binaryRequests, &geomParams = _parametersManager.getGeometryParameters(),
             &timer = _timer2, engine = _engine ](const Params& params,
                                const std::string& requestID,
                                const uintptr_t clientID,
                                rockets::jsonrpc::AsyncResponse respond) {
//                if(engine->snapshotPending())
//                {
//                    respond(SNAPSHOT_PENDING);
//                    return;
//                }

                if (requests.count(clientID) != 0)
                {
                    respond(ALREADY_PENDING_REQUEST);
                    return;
                }

                if (params.empty())
                {
                    respond(MISSING_PARAMS);
                    return;
                }

                const auto& supportedTypes = geomParams.getSupportedDataTypes();
                for (size_t i = 0; i < params.size(); ++i)
                {
                    const auto& param = params[i];
                    if(param.type.empty() || param.size == 0)
                    {
                        respond(MISSING_PARAMS);
                        return;
                    }

                    // try exact pattern match
                    // TODO: proper regex check for *.obj and obj cases
                    bool supported = supportedTypes.find(param.type) != supportedTypes.end();
                    if(supported)
                        continue;

                    // fallback to match "ends with extension"
                    supported = false;
                    for(const auto& type : supportedTypes)
                    {
                        if(endsWith(type, param.type))
                        {
                            supported = true;
                            break;
                        }
                    }

                    if(!supported)
                    {
                        respond(UNSUPPORTED_TYPE({i, {supportedTypes.begin(), supportedTypes.end()}}));
                        return;
                    }
                }

                auto request = std::make_shared<BinaryRequest>();
                request->id = requestID;
                request->params.assign(params.begin(), params.end());
                request->data.reserve(request->params[0].size);
                request->respond = respond;
                request->progress.requestID = requestID;
                request->updateTotalBytes();
                requests.emplace(clientID, request);

                timer.start();
            },
            [this](const std::string& requestID) {
                for (auto& i : _binaryRequests)
                {
                    auto& request = i.second;
                    if (request->id == requestID)
                    {
                        request->cancel();
                        break;
                    }
                }
            });
#endif
    }

    // TODO: RPC for remote file loading
    // TODO: need an API for finding the (best) suitable loader giving just a
    // filename (folder?).

    // TODO: no matter if properly finished or cancelled; remove it from our
    // list and
    // update the progress to be fulfilled
    //    void _deleteBinaryRequest(const uintptr_t clientID)
    //    {
    //        auto i = _binaryRequests.find(clientID);
    //        if (i == _binaryRequests.end())
    //            return;

    //        if (i->second->progress.isModified())
    //            _jsonrpcServer->notify(ENDPOINT_PROGRESS,
    //            i->second->progress);
    //        _binaryRequests.erase(i);
    //    }

    std::future<rockets::http::Response> _handleCircuitConfigBuilder(
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

    bool _writeBlueConfigFile(const std::string& filename,
                              const std::map<std::string, std::string>& params)
    {
        std::ofstream blueConfig(filename);
        if (!blueConfig.good())
            return false;

        std::map<std::string, std::string> dictionary = {{"morphology_folder",
                                                          "MorphologyPath"},
                                                         {"mvd_file",
                                                          "CircuitPath"}};

        blueConfig << "Run Default" << std::endl << "{" << std::endl;
        for (const auto& kv : params)
        {
            if (dictionary.find(kv.first) == dictionary.end())
            {
                BRAYNS_ERROR << "BlueConfigBuilder: Unknown parameter "
                             << kv.first << std::endl;
                continue;
            }
            blueConfig << dictionary[kv.first] << " " << kv.second << std::endl;
        }
        blueConfig << "}" << std::endl;
        blueConfig.close();
        return true;
    }

    EnginePtr _engine;

    using WsClientConnectNotifications =
        std::map<std::string, std::function<std::string()>>;
    WsClientConnectNotifications _wsClientConnectNotifications;

    using WsBroadcastOperations = std::map<std::string, std::function<void()>>;
    WsBroadcastOperations _wsBroadcastOperations;

    ParametersManager& _parametersManager;

    std::unique_ptr<rockets::Server> _rocketsServer;
    using JsonRpcServer =
        rockets::jsonrpc::Server<rockets::Server,
                                 rockets::jsonrpc::AsyncReceiver>;
    std::unique_ptr<JsonRpcServer> _jsonrpcServer;

#ifdef BRAYNS_USE_LIBUV
    std::unique_ptr<SocketListener> _socketListener;
#endif

    ImageGenerator _imageGenerator;

    Timer _timer;
    float _leftover{0.f};

    // TODO: move what's needed to new task
    struct BinaryRequest
    {
        BinaryRequest() { progress.setOperation("Waiting for data..."); }
        std::string id;
        std::deque<BinaryParam> params;
        std::string data;
        rockets::jsonrpc::AsyncResponse respond;
        Progress2 progress;

        bool valid() const
        {
            if (params.empty() || params[0].size == 0)
            {
                // respond(INVALID_BINARY_RECEIVE);
                return false;
            }

            return !cancelled();
        }

        void fullyReceived() { _fullyReceived = true; }
        void cancel()
        {
            _cancelled = true;

            // once we're processed by the async loading, wait here
            if (_fullyReceived)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _cond.wait(lock);
            }
        }

        void finish(const std::string& error = "")
        {
            progress.setAmount(1.f);
            progress.setOperation("");

            if (cancelled())
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _cond.notify_one();
            }
            else
            {
                if (error.empty())
                    respond(Response{to_json(true)});
                /*else
                    respond(LOADING_BINARY_FAILED(error))*/;
            }
        }
        bool cancelled() const { return _cancelled; }
        void updateTotalBytes()
        {
            for (const auto& param : params)
                _totalBytes += param.size;
        }

        void appendChunk(const std::string& chunk)
        {
            data += chunk;
            _receivedBytes += chunk.size();
            progress.setAmount(_progress());
            progress.setOperation("Receiving data...");
        }

    private:
        size_t _totalBytes{0};
        size_t _receivedBytes{0};
        float _progress() const
        {
            return 0.5f * ((float)_receivedBytes / _totalBytes);
        }

        bool _cancelled{false};
        std::mutex _mutex;
        std::condition_variable _cond;
        bool _fullyReceived{false};
    };

    // TODO: pair stuff looks wrong, extend Task.h??
    std::map<std::string, std::pair<async::task<void>, TaskPtr>> _tasks;
    std::map<uintptr_t, TaskPtr> _binaryRequests;
    Timer _timer2;
};

RocketsPlugin::RocketsPlugin(EnginePtr engine, PluginAPI* api)
    : _impl{std::make_shared<Impl>(engine, api)}
{
}

void RocketsPlugin::preRender()
{
    _impl->preRender();
}

void RocketsPlugin::postRender()
{
    _impl->postRender();
}

void RocketsPlugin::postSceneLoading()
{
    _impl->postSceneLoading();
}

void RocketsPlugin::_registerRequest(const std::string& name,
                                     const RetParamFunc& action)
{
    _impl->_jsonrpcServer->bind(name,
                                [action](
                                    const rockets::jsonrpc::Request& request) {
                                    return Response{action(request.message)};
                                });
}

void RocketsPlugin::_registerRequest(const std::string& name,
                                     const RetFunc& action)
{
    _impl->_jsonrpcServer->bind(name,
                                [action](const rockets::jsonrpc::Request&) {
                                    return Response{action()};
                                });
}

void RocketsPlugin::_registerNotification(const std::string& name,
                                          const ParamFunc& action)
{
    _impl->_jsonrpcServer->connect(
        name, [action](const rockets::jsonrpc::Request& request) {
            action(request.message);
        });
}

void RocketsPlugin::_registerNotification(const std::string& name,
                                          const VoidFunc& action)
{
    _impl->_jsonrpcServer->connect(name, [action] { action(); });
}
}
