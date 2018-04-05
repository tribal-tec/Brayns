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

#include "ClientServer.h"

#include <ImageGenerator.h>

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

    class LoadFunctor : public brayns::TaskFunctor
    {
    public:
        using TaskFunctor::TaskFunctor;
        void operator()(std::string blob)
        {
            std::cout << "Loading " << blob << std::endl;
            while (true)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                cancelCheck();
            }
            std::cout << "Loaded " << blob << std::endl;
        }
    };

    class LoadDataFromBlobTask : public brayns::SimpleTask<void>
    {
    public:
        LoadDataFromBlobTask(std::vector<size_t> params)
        //: brayns::SimpleTask<void>()
        {
            _params = params;
            chunks.resize(params.size());

            for (size_t i = 0; i < params.size(); ++i)
            {
                tasks.push_back(
                    chunks[i].get_task().then(LoadFunctor{_cancelToken}));
            }

            _task = async::when_all(tasks).then(
                [](std::vector<async::task<void>> tasks_) {

                    for (auto& task : tasks_)
                    {
                        try
                        {
                            task.get();
                            std::cout << "Finished" << std::endl;
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
        }
        void appendBlob(const std::string& blob)
        {
            _blob += blob;
            if (_blob.size() == _params[0])
                chunks[0].set(_blob);
        }

    private:
        std::vector<async::task<void>> tasks;
        std::vector<async::event_task<std::string>> chunks;
        std::vector<size_t> _params;
        std::string _blob;

        void _cancel() final
        {
            for (auto& i : chunks)
                i.set_exception(
                    std::make_exception_ptr(async::task_canceled()));
        }
    };

    LoadDataFromBlobTask mama{{4}};
    mama.appendBlob("blob");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    mama.cancel();

    // chunks[0].set("blob");
    // c.cancel();
    // chunks[1].cancel();
    // chunks[1].set_exception(std::make_exception_ptr(async::task_canceled()));

    // chunks[1].set("bla");
}

BOOST_AUTO_TEST_CASE(snapshot)
{
    class SnapShotFunctor : public brayns::TaskFunctor
    {
    public:
        SnapShotFunctor(size_t size)
            : _size(size)
        {
        }
        using TaskFunctor::TaskFunctor;
        brayns::ImageGenerator::ImageBase64 operator()()
        {
            for (size_t i = 0; i < _size; ++i)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                cancelCheck();
            }
            throw std::runtime_error("haha");
            return {{"dummy"}};
        }

    private:
        size_t _size;
    };

    //    class SnapshotTask : public BaseTask
    //    {
    //    public:
    //        SnapshotTask()
    //        {
    //            _task = async::spawn(SnapShotFunctor{_cancelToken});
    //        }

    //        auto& task()
    //        {
    //            return _task;
    //        }

    //        using Type = async::task<brayns::ImageGenerator::ImageBase64>;

    //        auto operator->() { return &_task; }

    //    private:
    //        Type _task;
    //    };

    using SnapshotTask =
        brayns::SimpleTask<brayns::ImageGenerator::ImageBase64>;
    SnapshotTask task{SnapShotFunctor{10}};
    task.cancel();

    task->then([](SnapshotTask::Type task) {
        try
        {
            std::cout << task.get().data << std::endl;
            std::cout << "Finished" << std::endl;
        }
        catch (const std::runtime_error& e)
        {
            std::cout << e.what() << std::endl;
        }
        catch (const async::task_canceled& e)
        {
            std::cout << "Cancelled" << std::endl;
        }
    });

    //    try
    //    {
    //        std::cout << task.task().get().data << std::endl;
    //    }
    //    catch (const async::task_canceled& e)
    //    {
    //        std::cout << "Cancelled snapshot" << std::endl;
    //    }
}
