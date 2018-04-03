/* Copyright (c) 2015-2018, EPFL/Blue Brain Project
 *
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

#include <brayns/common/types.h>

#include <memory>

#include "transwarp.h"
namespace tw = transwarp;

namespace brayns
{
using ProgressFunc = std::function<void(std::string, float)>;
/**
 *
 */
class TaskFunctor : public transwarp::functor
{
public:
    void progress(const std::string& message, const float amount)
    {
        if (progressFunc)
            progressFunc(message, amount);
    }
    ProgressFunc progressFunc;
};

class Task
{
public:
    virtual ~Task() = default;
    virtual void cancel() = 0;
    virtual void schedule() = 0;
    virtual void wait() = 0;

protected:
    static tw::parallel& executor()
    {
        static tw::parallel _executor{4};
        return _executor;
    }
};

template <typename T>
class TaskT : public Task
{
public:
    template <typename F>
    TaskT(F&& functor)
    {
        if (std::is_base_of<TaskFunctor, F>::value)
        {
            auto& taskFunctor = static_cast<TaskFunctor&>(functor);
            taskFunctor.progressFunc = [this](const std::string& message,
                                              const float amount) {
                _progress.setOperation(message);
                _progress.setAmount(amount);
                if (progressUpdated)
                    progressUpdated(_progress);
            };
        }
        _task = tw::make_task(tw::root, functor);
    }

    void schedule() final
    {
        _progress.setOperation("Scheduling task ...");
        progressUpdated(_progress);
        _task->schedule(executor());
    }

    void cancel() final
    {
        _progress.setAmount(1.f);
        progressUpdated(_progress);
        _task->cancel(true);
    }

    auto get() { return _task->get(); }
    void wait() final { _task->wait(); }
    void setProgressUpdatedCallback(const std::function<void(Progress2&)>& cb)
    {
        progressUpdated = cb;
    }

    std::function<void(Progress2&)> progressUpdated;

    void setRequestID(const std::string& requestID)
    {
        _progress.requestID = requestID;
    }

    auto impl() { return _task; }
private:
    std::shared_ptr<tw::task<T>> _task;

public:
    Progress2 _progress;

    // class Impl;
    // std::unique_ptr<Impl> _impl;
};

template <typename T>
class TaskCallbackT : public Task
{
public:
    template <typename F>
    TaskCallbackT(F&& functor, std::function<void(T)> callback)
        : _task(std::make_shared<TaskT<T>>(functor))
        , _consumer(tw::make_task(tw::consume, callback, _task->impl()))
    {
    }

    TaskCallbackT(std::shared_ptr<TaskT<T>> task,
                  std::function<void(T)> callback)
        : _task(task)
        , _consumer(tw::make_task(tw::consume, callback, _task->impl()))
    {
    }

    void schedule() final
    {
        _task->_progress.setOperation("Scheduling task ...");
        _task->progressUpdated(_task->_progress);
        _consumer->schedule_all(executor());
    }

    void cancel() final
    {
        _task->_progress.setAmount(1.f);
        _task->progressUpdated(_task->_progress);
        _consumer->cancel_all(true);
    }

    void wait() final { _consumer->wait(); }
    void setRequestID(const std::string& requestID)
    {
        _task->setRequestID(requestID);
    }

    void setProgressUpdatedCallback(const std::function<void(Progress2&)>& cb)
    {
        _task->setProgressUpdatedCallback(cb);
    }

private:
    std::shared_ptr<TaskT<T>> _task;
    std::shared_ptr<tw::task<void>> _consumer;
};
}
