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

#include <brayns/common/tasks/transwarp.h>
namespace tw = transwarp;

namespace brayns
{
class TaskRuntimeError : public std::runtime_error
{
public:
    TaskRuntimeError(const std::string& message, const int code = -1,
                     const std::string& data = "")
        : std::runtime_error(message.c_str())
        , _code(code)
        , _data(data)
    {
    }

    int code() const { return _code; }
    const std::string& data() const { return _data; }
private:
    const int _code;
    const std::string _data;
};

class TaskFunctor : public tw::functor
{
public:
    TaskFunctor() = default;

    void progress(const std::string& message, const float amount)
    {
        if (_progressFunc)
            _progressFunc(message, amount);
    }

    void cancelCheck()
    {
        transwarp_cancel_point();
    }

    using ProgressFunc = std::function<void(std::string, float)>;

    void setProgressFunc(const ProgressFunc& progressFunc)
    {
        _progressFunc = progressFunc;
    }

protected:
    ProgressFunc _progressFunc;
};

class Task
{
public:
    virtual ~Task() = default;
    void cancel()
    {
        _cancel();
    }
    virtual void wait() = 0;

    virtual void schedule() {}
    void setRequestID(const std::string& requestID)
    {
        _progress.requestID = requestID;
    }

    void progress(const std::string& message, const float amount)
    {
        _progress.setOperation(message);
        _progress.setAmount(amount);
    }

    Progress2& getProgress() { return _progress; }
protected:
    Progress2 _progress{"Scheduling task ..."};

private:
    virtual void _cancel() {}
};

template <typename T>
class TaskT : public Task
{
public:
    static tw::executor& executor()
    {
        static tw::parallel _executor{4};
        //        static tw::sequential _executor;
        return _executor;
    }

    TaskT() = default;

    template <typename F>
    TaskT(F&& functor)
    {
        _setupFunctor(functor);
        _task = tw::make_task(tw::root, functor);
        _task->schedule(executor());
    }

    void wait() final { _task->wait(); }
    auto operator-> () { return &_task; }
    auto task() { return _task; }
protected:
    void _cancel() final { _task->cancel(true); }
    std::shared_ptr<tw::task<T>> _task;

    template <typename F>
    auto&& _setupFunctor(F&& functor)
    {
        if (std::is_base_of<TaskFunctor, F>::value)
        {
            auto& taskFunctor = static_cast<TaskFunctor&>(functor);
            taskFunctor.setProgressFunc([& progress = _progress](
                const std::string& message, const float amount) {
                progress.setOperation(message);
                progress.setAmount(amount);
            });
        }
        return std::move(functor);
    }
};

template <typename T>
class DelayedTask : public TaskT<T>
{
public:
    template <typename F>
    DelayedTask(F&& functor)
    {
        TaskT<T>::_task =
            tw::make_task(tw::root,
                          TaskT<T>::template _setupFunctor(std::move(functor)));
    }

    void schedule() final { TaskT<T>::_task->schedule(TaskT<T>::executor()); }
};
}
