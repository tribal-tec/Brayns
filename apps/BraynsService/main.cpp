/* Copyright (c) 2015-2016, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
 * Responsible Author: Cyrille Favreau <cyrille.favreau@epfl.ch>
 *
 * This file is part of Brayns <https://github.com/BlueBrain/Brayns>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <brayns/Brayns.h>
#include <brayns/common/Timer.h>
#include <brayns/common/engine/Engine.h>
#include <brayns/common/log.h>
#include <brayns/common/renderer/Renderer.h>
#include <brayns/common/types.h>

#include <uvw.hpp>

#include <thread>

int main(int argc, const char** argv)
{
    try
    {
        brayns::Timer timer;
        timer.start();

        BRAYNS_INFO << "Initializing Service..." << std::endl;

        brayns::Brayns brayns(argc, argv);

        auto loop = uvw::Loop::getDefault();
        auto renderingDone = loop->resource<uvw::AsyncHandle>();
        auto eventRendering = loop->resource<uvw::IdleHandle>();
        auto accumRendering = loop->resource<uvw::IdleHandle>();
        auto progressUpdate = loop->resource<uvw::TimerHandle>();
        auto checkIdleRendering = loop->resource<uvw::CheckHandle>();
        checkIdleRendering->start();

        auto renderLoop = uvw::Loop::create();
        auto triggerRendering = renderLoop->resource<uvw::AsyncHandle>();
        auto stopRenderThread = renderLoop->resource<uvw::AsyncHandle>();

        // image jpeg creation is not threadsafe (yet), move that to render
        // thread? also data loading and maybe more things used by render() are
        // not safe yet.
        std::mutex mutex;

        // main thread
        const float idleRenderingDelay = 0.1f;
        bool isLoading = false;
        brayns::Timer timeSinceLastEvent;
        {
            // triggered after rendering, send events to rockets
            renderingDone->on<uvw::AsyncEvent>([&](const auto&, auto&) {
                std::lock_guard<std::mutex> lock{mutex};
                brayns.postRender();
            });

            // events from rockets
            brayns.getEngine().triggerRender = [&] {
                if (!isLoading)
                    eventRendering->start();
            };

            brayns.getEngine().buildScene = [&] {
                eventRendering->stop();
                checkIdleRendering->stop();
                isLoading = true;

                progressUpdate->start(std::chrono::milliseconds(0),
                                      std::chrono::milliseconds(100));
                auto work =
                    loop->resource<uvw::WorkReq>([&] { brayns.buildScene(); });

                work->on<uvw::WorkEvent>([&](const auto&, auto&) {
                    progressUpdate->stop();
                    progressUpdate->close();

                    eventRendering->start();
                    checkIdleRendering->start();
                    isLoading = false;
                });

                work->queue();
            };

            // render trigger from events
            eventRendering->on<uvw::IdleEvent>([&](const auto&, auto&) {
                eventRendering->stop();
                accumRendering->stop();
                timeSinceLastEvent.start();

                std::lock_guard<std::mutex> lock{mutex};
                if (!brayns.getEngine().getKeepRunning())
                {
                    stopRenderThread->send();
                    loop->stop();
                    return;
                }

                if (brayns.preRender())
                    triggerRendering->send();
            });

            progressUpdate->on<uvw::TimerEvent>(
                [&](const auto&, auto&) { brayns.sendMessages(); });

            progressUpdate->on<uvw::CloseEvent>(
                [&](const auto&, auto&) { brayns.sendMessages(); });

            // start accum rendering when we have no more other events
            checkIdleRendering->on<uvw::CheckEvent>(
                [&](const auto&, auto&) { accumRendering->start(); });

            // render trigger from going into idle
            accumRendering->on<uvw::IdleEvent>([&](const auto&, auto&) {
                if (timeSinceLastEvent.elapsed() < idleRenderingDelay)
                    return;

                std::lock_guard<std::mutex> lock{mutex};
                if (brayns.getEngine().continueRendering())
                {
                    if (brayns.preRender())
                        triggerRendering->send();
                }

                accumRendering->stop();
            });
        }

        // render thread
        {
            // rendering
            triggerRendering->on<uvw::AsyncEvent>([&](const auto&, auto&) {
                std::lock_guard<std::mutex> lock{mutex};
                brayns.render();
                renderingDone->send();
            });

            // stop render loop
            stopRenderThread->once<uvw::AsyncEvent>(
                [&](const auto&, auto&) { renderLoop->stop(); });
        }

        brayns.init();

        std::thread render_thread([&] { renderLoop->run(); });
        loop->run();
        render_thread.join();

        timer.stop();
        BRAYNS_INFO << "Service was running for " << timer.seconds()
                    << " seconds" << std::endl;
    }
    catch (const std::runtime_error& e)
    {
        BRAYNS_ERROR << e.what() << std::endl;
        return 1;
    }
    return 0;
}
