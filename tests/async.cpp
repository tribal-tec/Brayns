/* Copyright (c) 2018, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
 * Responsible Author: Daniel.Nachbaur@epfl.ch
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

#define BOOST_TEST_MODULE async

#include <async++.h>

#include "ClientServer.h"

BOOST_AUTO_TEST_CASE(bla)
{
    //    async::task<void> tasks[] = {
    //        async::spawn([] {
    //        std::cout << "Load1" << std::endl;
    //    }),
    //                                async::spawn([]{
    //                                    std::cout << "Load2" << std::endl;
    //                                      throw std::runtime_error("haha");
    //                                })
    //    };

    std::vector<async::task<void>> tasks;

    std::vector<async::event_task<std::string>> chunks{2};

    async::cancellation_token c;

    for (size_t i = 0; i < 2; ++i)
    {
        auto task = chunks[i].get_task();
        tasks.push_back(task.then([&c](std::string result) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            async::interruption_point(c);
            std::cout << "chunk " << result << std::endl;
        }));
    }

    auto task =
        async::when_all(tasks).then([](std::vector<async::task<void>> tasks_) {
            std::cout << "done" << std::endl;
            for (auto& task : tasks_)
            {
                try
                {
                    task.get();
                }
                catch (const std::runtime_error& e)
                {
                    std::cout << e.what() << std::endl;
                }
                catch (const async::task_canceled& e)
                {
                    std::cout << "Cancelled" << std::endl;
                }
            }
        });

    chunks[0].set("blob");
    c.cancel();

    // chunks[1].set("bla");
}
