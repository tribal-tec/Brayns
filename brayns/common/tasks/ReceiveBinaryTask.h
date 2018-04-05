/* Copyright (c) 2015-2018, EPFL/Blue Brain Project
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

#pragma once

#include <brayns/common/tasks/LoadDataTask.h>
#include <brayns/common/tasks/Task.h>

namespace brayns
{
struct BinaryParam
{
    size_t size{0};
    std::string type; // file extension or MESH, POINTS, CIRCUIT
};

using BinaryParams = std::vector<BinaryParam>;

struct BinaryError
{
    size_t index{0}; // which file param had error
    std::vector<std::string> supportedTypes;
};

const TaskRuntimeError MISSING_PARAMS{"Missing params", -1731};
TaskRuntimeError UNSUPPORTED_TYPE(const BinaryError& /*error*/)
{
    return {"Unsupported type", -1732, "how?" /*to_json(error)*/};
}

inline bool endsWith(const std::string& value, const std::string& ending)
{
    if (ending.size() > value.size())
        return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

class ComposeChunks : public TaskFunctor
{
public:
    ComposeChunks(const size_t totalBytes, std::string& chunks)
        : _totalBytes(totalBytes)
        , _data(chunks)
    {
        //        chunkReceived =
        //            std::bind(&ComposeChunks::appendChunk, this,
        //            std::placeholders::_1);
    }

    ~ComposeChunks() {}
    std::string operator()()
    {
        // TODO: wish to use condition.wait, but copy ctor is not permitted.
        while (!_allReceived())
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

        return _data;
    }

private:
    bool _allReceived() const { return _data.size() == _totalBytes; }
    size_t _totalBytes{0};
    std::string& _data;
};

class LoadData : public TaskFunctor
{
public:
    void operator()(const std::string& data)
    {
        std::cout << "Got data " << data.size() << std::endl;
    }
};

class ReceiveBinaryTask : public TaskFunctor
{
public:
    ReceiveBinaryTask(const BinaryParams& params,
                      const std::set<std::string>& supportedTypes)
        : _params(params.begin(), params.end())
    {
        if (params.empty())
            throw MISSING_PARAMS;

        for (size_t i = 0; i < params.size(); ++i)
        {
            const auto& param = params[i];
            if (param.type.empty() || param.size == 0)
                throw MISSING_PARAMS;

            // try exact pattern match
            // TODO: proper regex check for *.obj and obj cases
            bool supported =
                supportedTypes.find(param.type) != supportedTypes.end();
            if (supported)
                continue;

            // fallback to match "ends with extension"
            supported = false;
            for (const auto& type : supportedTypes)
            {
                if (endsWith(type, param.type))
                {
                    supported = true;
                    break;
                }
            }

            if (!supported)
                throw UNSUPPORTED_TYPE(
                    {i, {supportedTypes.begin(), supportedTypes.end()}});
        }

        // auto request = std::make_shared<BinaryRequest>();
        // request->id = requestID;
        //_data.reserve(_params[0].size); // prepare for first file
        // request->respond = respond;
        // request->progress.requestID = requestID;
        _updateTotalBytes();

#if 0
        auto receiveChunks = [] { return std::string("chunk");};
        auto loadData = [](const std::string& data) { std::cout << "Data: " << data << std::endl;};
        auto receiveBinary = [] { return true; };

        namespace tw =  transwarp;
        auto task1 = tw::make_task(tw::root, receiveChunks);
        auto task2 = tw::make_task(tw::consume, loadData, task1);
        auto task3 = tw::make_task(tw::wait, receiveBinary, task2);
        tw::parallel executor{4};
        task3->schedule_all(executor);
#endif
    }

    bool operator()()
    {
        // transwarp_cancel_point();
        // progress("Render snapshot ...", 1.f);

        std::cout << "Loaded all the shit" << std::endl;

        return false;
    }

    // auto getParents() { return _loadTasks.front(); }

    size_t getTotalBytes() const { return _totalBytes; }
private:
    void _updateTotalBytes()
    {
        for (const auto& param : _params)
            _totalBytes += param.size;
    }

    std::deque<BinaryParam> _params;
    // std::string _data;
    size_t _totalBytes{0};
    // std::vector<std::shared_ptr<TaskT<void>>> _loadTasks;
};

auto createReceiveBinaryTask(const BinaryParams& params,
                             const std::set<std::string>& supportedTypes,
                             std::string& chunks)
{
// how to share progress between all those tasks?
// this tasks here has the load tasks as parents and waits for them?

// how to handle multiple files/chunks? variadic unpack? chunk reuse from
// rockets plugin?

// final functor error propagation? now returns true all the time

#ifdef tw
    auto finalFunctor = ReceiveBinaryTask{params, supportedTypes};

    auto composeChunksTask = std::make_shared<TaskT<std::string>>(
        ComposeChunks{finalFunctor.getTotalBytes(), chunks});

    auto loadDataTask =
        std::make_shared<TaskT<void>>(tw::consume, LoadData{},
                                      composeChunksTask->impl());

    return std::make_shared<TaskT<bool>>(tw::wait, finalFunctor,
                                         loadDataTask->impl());
#else
#endif
}
}
