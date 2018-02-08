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
        BRAYNS_INFO << "Initializing Service..." << std::endl;
        auto loop = uvw::Loop::getDefault();
        brayns::Brayns brayns(argc, argv);

        auto renderLoop = uvw::Loop::create();
        auto triggerRendering = renderLoop->resource<uvw::AsyncHandle>();
        auto renderingDone = loop->resource<uvw::AsyncHandle>();

        std::mutex mutex;
        renderingDone->on<uvw::AsyncEvent>(
            [&](const uvw::AsyncEvent&, uvw::AsyncHandle& /*handle*/) {
                std::lock_guard<std::mutex> lock{mutex};
                brayns.postRender();
                if (brayns.getEngine().continueRendering())
                    triggerRendering->send();
            });

        triggerRendering->on<uvw::AsyncEvent>(
            [&](const uvw::AsyncEvent&, uvw::AsyncHandle& /*handle*/) {
                std::lock_guard<std::mutex> lock{mutex};
                if (!brayns.getEngine().getKeepRunning() || !brayns.render())
                {
                    renderLoop->stop();
                    loop->stop();
                }
                else
                    renderingDone->send();
            });

        brayns.getEngine().triggerRender = [&] {
            std::lock_guard<std::mutex> lock{mutex};
            triggerRendering->send();
        };
        brayns.init();

        std::thread render_thread([&] { renderLoop->run(); });

        brayns::Timer timer;
        timer.start();

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
