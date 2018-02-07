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
#include <brayns/common/log.h>
#include <brayns/common/types.h>

#include <uvw.hpp>

//#include <thread>
//#include <uv.h>

// size_t nTimes = 2;

// void timer_callback(uv_timer_t *handle)
//{
//    uv_async_t *other_thread_notifier = (uv_async_t *)handle->data;

//    fprintf(stderr, "Timer expired, notifying other thread\n");

//    if (--nTimes == 0)
//        uv_stop(uv_default_loop());

//    // Notify the other thread
//    uv_async_send(other_thread_notifier);
//}

// void render_loop(uv_loop_t *thread_loop)
//{
//    fprintf(stderr, "Consumer thread will start event loop\n");

//    // Start this loop
//    uv_run(thread_loop, UV_RUN_DEFAULT);
//}

// void consumer_notify(uv_async_t *handle)
//{
//    fprintf(stderr, "Hello from the other thread\n",
//    handle->loop->backend_fd);
//    if (nTimes == 0)
//        uv_stop(handle->loop);
//}

// int main(int argc, char *argv[])
//{
//    uv_async_t async;

//    /* Create and set up the consumer thread */
//    uv_loop_t *thread_loop = uv_loop_new();
//    uv_async_init(thread_loop, &async, consumer_notify);
//    std::thread render_thread(std::bind(&render_loop, thread_loop));

//    /* Main thread will run default loop */
//    uv_loop_t *main_loop = uv_default_loop();
//    uv_timer_t timer_req;
//    uv_timer_init(main_loop, &timer_req);

//    /* Timer callback needs async so it knows where to send messages */
//    timer_req.data = &async;
//    uv_timer_start(&timer_req, timer_callback, 0, 500);

//    fprintf(stderr, "Starting main loop\n");
//    uv_run(main_loop, UV_RUN_DEFAULT);

//    render_thread.join();

//    return 0;
//}

// struct Work
//{
//    size_t n{42};
//};

// void do_work(uv_work_t *req) {
//    Work n = *(Work *) req->data;
//    std::cout << std::this_thread::get_id() << std::endl;
//    fprintf(stderr, "%dth fibonacci\n", n.n);
//}

// void work_cb(uv_work_t *req, int status) {
//    fprintf(stderr, "Done calculating %dth fibonacci\n", *(int *) req->data);
//}

int main(int argc, const char** argv)
{
    try
    {
        BRAYNS_INFO << "Initializing Service..." << std::endl;
        auto loop = uvw::Loop::getDefault();
        brayns::Brayns brayns(argc, argv);
        brayns.render();

        brayns::Timer timer;
        timer.start();

        auto timerHandle = loop->resource<uvw::TimerHandle>();
        timerHandle->on<uvw::TimerEvent>(
            [&](const uvw::TimerEvent&, uvw::TimerHandle&) {
                if (!brayns.render())
                    loop->stop();
            });
        timerHandle->start(std::chrono::milliseconds(0),
                           std::chrono::milliseconds(16));

        loop->run();

        //        bool keepRunning = true;
        //        while (keepRunning)
        //            keepRunning = brayns.render();
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

    //    uv_loop_t *loop = (uv_loop_t *)malloc(sizeof(uv_loop_t));
    //    uv_loop_init(loop);

    //    uv_work_t req, req1, req2;
    //    Work work;
    //    req.data = (void *) &work;
    //    req1.data = (void *) &work;
    //    req2.data = (void *) &work;
    //    uv_queue_work(loop, &req, do_work, work_cb);
    //    uv_queue_work(loop, &req1, do_work, work_cb);
    //    uv_queue_work(loop, &req2, do_work, work_cb);

    //    uv_run(loop, UV_RUN_DEFAULT);

    //    uv_loop_close(loop);
    //    free(loop);
    //    return 0;
}
