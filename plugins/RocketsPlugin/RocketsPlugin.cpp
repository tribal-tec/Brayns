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
#include <brayns/common/tasks/Task.h>
#include <brayns/common/volume/VolumeHandler.h>
#include <brayns/pluginapi/PluginAPI.h>

#include <brayns/tasks/UploadBinaryTask.h>
#include <brayns/tasks/UploadPathTask.h>

#ifdef BRAYNS_USE_LIBUV
#include <uvw.hpp>
#endif

#include <fstream>

#include <rockets/jsonrpc/asyncReceiver.h>
#include <rockets/jsonrpc/helpers.h>
#include <rockets/jsonrpc/server.h>
#include <rockets/server.h>

#include "ImageGenerator.h"

namespace
{
const std::string ENDPOINT_API_VERSION = "v1/";
const std::string ENDPOINT_APP_PARAMS = "application-parameters";
const std::string ENDPOINT_CAMERA = "camera";
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
const std::string METHOD_UPLOAD_BINARY = "upload-binary";
const std::string METHOD_UPLOAD_PATH = "upload-path";
const std::string METHOD_RESET_CAMERA = "reset-camera";
const std::string METHOD_SNAPSHOT = "snapshot";

const std::string JSON_TYPE = "application/json";

using Response = rockets::jsonrpc::Response;

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

class BinaryRequests
{
public:
    auto createTask(const BinaryParams& params, uintptr_t clientID,
                    const std::set<std::string>& supportedTypes,
                    EnginePtr engine)
    {
        if (_binaryRequests.count(clientID) != 0)
            throw ALREADY_PENDING_REQUEST;

        auto task = createUploadBinaryTask(params, supportedTypes, engine);
        _binaryRequests.emplace(clientID, task);
        _requests.emplace(task, clientID);

        return task;
    }

    rockets::ws::Response processMessage(const rockets::ws::Request& wsRequest)
    {
        if (_binaryRequests.count(wsRequest.clientID) == 0)
        {
            BRAYNS_ERROR << "Missing RPC " << METHOD_UPLOAD_BINARY
                         << " or cancelled?" << std::endl;
            return {};
        }

        _binaryRequests[wsRequest.clientID]->appendBlob(wsRequest.message);
        return {};
    }

    void removeRequest(const uintptr_t clientID)
    {
        auto i = _binaryRequests.find(clientID);
        if (i == _binaryRequests.end())
            return;

        i->second->cancel();
        _binaryRequests.erase(i);
    }

    void removeTask(TaskPtr task)
    {
        auto i = _requests.find(task);
        if (i == _requests.end())
            return;

        removeRequest(i->second);
        _requests.erase(i);
    }

private:
    std::map<uintptr_t, std::shared_ptr<UploadBinaryTask>> _binaryRequests;
    std::map<TaskPtr, uintptr_t> _requests;
};

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
        // cancel all pending tasks; cancel() will remove itself from _tasks
        while (!_tasks.empty())
        {
            auto task = _tasks.begin()->second;
            _tasks.begin()->first->cancel();
            task->wait();
        }

        if (_rocketsServer)
            _rocketsServer->setSocketListener(nullptr);
    }

    void preRender()
    {
        if (!_rocketsServer || !_manualProcessing)
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
    }

    void postSceneLoading()
    {
        if (!_rocketsServer)
            return;

        _wsBroadcastOperations[ENDPOINT_CAMERA]();
        _wsBroadcastOperations[ENDPOINT_PROGRESS]();
        _wsBroadcastOperations[ENDPOINT_STATISTICS]();
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
#ifdef BRAYNS_USE_LIBUV
            if (uvw::Loop::getDefault()->alive())
            {
                _rocketsServer.reset(new rockets::Server{uv_default_loop(),
                                                         _getHttpInterface(),
                                                         "rockets"});
                _manualProcessing = false;
            }
            else
#endif
                _rocketsServer.reset(
                    new rockets::Server{_getHttpInterface(), "rockets", 0});

            BRAYNS_INFO << "Rockets server running on "
                        << _rocketsServer->getURI() << std::endl;

            _jsonrpcServer.reset(new JsonRpcServer(*_rocketsServer));

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
            _binaryRequests.removeRequest(clientID);
            return std::vector<rockets::ws::Response>{};
        });

        _rocketsServer->handleBinary(std::bind(&BinaryRequests::processMessage,
                                               std::ref(_binaryRequests),
                                               std::placeholders::_1));
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
    void _handleAsyncRPC(const std::string& method, const RpcDocumentation& doc,
                         std::function<rockets::jsonrpc::CancelRequestCallback(
                             P, uintptr_t, rockets::jsonrpc::AsyncResponse,
                             rockets::jsonrpc::ProgressUpdateCallback)>
                             action)
    {
        _jsonrpcServer->bindAsync<P>(method, action);
        _handleSchema(method, buildJsonRpcSchema<P, R>(method, doc));
    }

    template <class P, class R>
    void _handleTask(
        const std::string& method, const RpcDocumentation& doc,
        std::function<std::shared_ptr<TaskT<R>>(P, uintptr_t)> createUserTask)
    {
        auto action =
            [& tasks = _tasks, &binaryRequests = _binaryRequests, createUserTask, & server = _jsonrpcServer, &mutex = _tasksMutex](P params, uintptr_t clientID,
                                rockets::jsonrpc::AsyncResponse respond, rockets::jsonrpc::ProgressUpdateCallback progressCb)
        {
            auto errorCallback = [respond](const TaskRuntimeError& error) {
                respond({Response::Error{error.what(), error.code(),
                                         error.data()}});
            };

            try
            {
                auto readyCallback = [respond](R result) {
                    try
                    {
                        respond({to_json(result)});
                    }
                    catch (const std::runtime_error& e)
                    {
                        respond({Response::Error{e.what(), -1}});
                    }
                };

                auto userTask = createUserTask(params, clientID);

                std::function<void()> finishProgress = [userTask] {
                    userTask->progress("Done", 1.f);
                };
#ifdef BRAYNS_USE_LIBUV
                if (uvw::Loop::getDefault()->alive())
                {
                    auto progressUpdate =
                        uvw::Loop::getDefault()->resource<uvw::TimerHandle>();

                    auto sendProgress =
                        [ progressCb, &progress = userTask->getProgress() ]
                    {
                        if (progress.isModified())
                        {
                            progressCb(progress.operation(), progress.amount());
                            progress.resetModified();
                        }
                    };
                    progressUpdate->on<uvw::TimerEvent>(
                        [sendProgress](const auto&, auto&) { sendProgress(); });

                    finishProgress = [userTask, progressUpdate, sendProgress] {
                        userTask->progress("Done", 1.f);
                        sendProgress();
                        progressUpdate->stop();
                        progressUpdate->close();
                    };

                    progressUpdate->start(std::chrono::milliseconds(0),
                                          std::chrono::milliseconds(100));
                }
#endif

                auto task =
                    std::make_shared<async::task<void>>(userTask->task().then(
                        [readyCallback, errorCallback, &tasks, &binaryRequests,
                         userTask, finishProgress,
                         &mutex](typename TaskT<R>::Type result) {
                            finishProgress();

                            if (userTask->canceled())
                                userTask->finishCancel();
                            else
                            {
                                try
                                {
                                    readyCallback(result.get());
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
                                }
                            }

                            std::lock_guard<std::mutex> lock(mutex);
                            tasks.erase(userTask);
                            binaryRequests.removeTask(userTask);
                        }));

                userTask->schedule();

                std::lock_guard<std::mutex> lock(mutex);
                tasks.emplace(userTask, task);

                auto cancel = [userTask,
                               task](rockets::jsonrpc::VoidCallback done) {
                    userTask->cancel(done);
                };

                return rockets::jsonrpc::CancelRequestCallback(cancel);
            }
            catch (const BinaryTaskError& e)
            {
                errorCallback({e.what(), e.code(), to_json(e.error())});
            }
            catch (const TaskRuntimeError& e)
            {
                errorCallback(e);
            }
            catch (const std::exception& e)
            {
                errorCallback({e.what()});
            }
            return rockets::jsonrpc::CancelRequestCallback();
        };
        _handleAsyncRPC<P, R>(method, doc, action);
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

        _handleUploadBinary();
        _handleUploadPath();
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
                      std::placeholders::_2, std::ref(*_engine),
                      std::ref(_imageGenerator)));
    }

    void _handleUploadBinary()
    {
        RpcDocumentation doc{"Upload files to load geometry", "params",
                             "Array of file parameter: size and type"};

        _handleTask<BinaryParams, bool>(
            METHOD_UPLOAD_BINARY, doc,
            std::bind(&BinaryRequests::createTask, std::ref(_binaryRequests),
                      std::placeholders::_1, std::placeholders::_2,
                      _parametersManager.getGeometryParameters()
                          .getSupportedDataTypes(),
                      _engine));
    }

    void _handleUploadPath()
    {
        RpcDocumentation doc{"Upload remote path to load geometry from",
                             "params", "Array of path, either file or folder"};

        _handleTask<std::vector<std::string>, bool>(
            METHOD_UPLOAD_PATH, doc,
            std::bind(createUploadPathTask, std::placeholders::_1,
                      std::placeholders::_2,
                      _parametersManager.getGeometryParameters()
                          .getSupportedDataTypes(),
                      _engine));
    }

    EnginePtr _engine;

    using WsClientConnectNotifications =
        std::map<std::string, std::function<std::string()>>;
    WsClientConnectNotifications _wsClientConnectNotifications;

    using WsBroadcastOperations = std::map<std::string, std::function<void()>>;
    WsBroadcastOperations _wsBroadcastOperations;

    ParametersManager& _parametersManager;

    std::unique_ptr<rockets::Server> _rocketsServer;
    using JsonRpcServer = rockets::jsonrpc::Server<rockets::Server>;
    std::unique_ptr<JsonRpcServer> _jsonrpcServer;

    bool _manualProcessing{true};

    ImageGenerator _imageGenerator;

    Timer _timer;
    float _leftover{0.f};

    // TODO: pair stuff looks wrong, extend Task.h??
    std::map<TaskPtr, std::shared_ptr<async::task<void>>> _tasks;
    std::mutex _tasksMutex;

    BinaryRequests _binaryRequests;
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
