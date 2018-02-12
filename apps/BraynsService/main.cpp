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
        auto loop = uvw::Loop::getDefault();
        brayns::Brayns brayns(argc, argv);

        auto renderLoop = uvw::Loop::create();
        auto triggerRendering = renderLoop->resource<uvw::AsyncHandle>();
        auto stopRenderThread = renderLoop->resource<uvw::AsyncHandle>();
        auto renderingDone = loop->resource<uvw::AsyncHandle>();
        auto checkIdleRendering = loop->resource<uvw::CheckHandle>();
        checkIdleRendering->start();

        auto eventRendering = loop->resource<uvw::IdleHandle>();
        auto accumRendering = loop->resource<uvw::IdleHandle>();

        // image jpeg creation is not threadsafe (yet), move that to render
        // thread?
        // also data loading and maybe more things used by render() are not safe
        // yet
        std::mutex mutex;

        // main thread
        {
            brayns::Timer timeSinceLastEvent;
            const float idleRenderingDelay = 0.1f;

            // triggered after rendering, send events to rockets
            renderingDone->on<uvw::AsyncEvent>(
                [&](const uvw::AsyncEvent&, uvw::AsyncHandle&) {
                    std::lock_guard<std::mutex> lock{mutex};
                    brayns.postRender();
                });

            // events from rockets
            brayns.getEngine().triggerRender = [&] { eventRendering->start(); };

            // render trigger from events
            eventRendering->on<uvw::IdleEvent>(
                [&](const uvw::IdleEvent&, uvw::IdleHandle&) {
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

                    brayns.preRender();
                    triggerRendering->send();
                });

            // start accum rendering when we have no more other events
            checkIdleRendering->on<uvw::CheckEvent>(
                [&](const uvw::CheckEvent&, uvw::CheckHandle&) {
                    accumRendering->start();
                });

            // render trigger from going into idle
            accumRendering->on<uvw::IdleEvent>(
                [&, idleRenderingDelay](const uvw::IdleEvent&,
                                        uvw::IdleHandle&) {
                    if (timeSinceLastEvent.elapsed() < idleRenderingDelay)
                        return;

                    std::lock_guard<std::mutex> lock{mutex};
                    if (brayns.getEngine().continueRendering())
                    {
                        brayns.preRender();
                        triggerRendering->send();
                    }

                    accumRendering->stop();
                });
        }

        // render thread
        {
            // rendering
            triggerRendering->on<uvw::AsyncEvent>(
                [&](const uvw::AsyncEvent&, uvw::AsyncHandle&) {
                    std::lock_guard<std::mutex> lock{mutex};
                    brayns.render();
                    renderingDone->send();
                });

            // stop render loop
            stopRenderThread->on<uvw::AsyncEvent>(
                [&](const uvw::AsyncEvent&, uvw::AsyncHandle&) {
                    renderLoop->stop();
                });
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
