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

#include <brayns/common/Progress.h>
#include <brayns/common/types.h>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif
#include <async++.h>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

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

class TaskFunctor
{
public:
    TaskFunctor() = default;

    void progress(const std::string& message, const float increment,
                  const float amount)
    {
        if (_progressFunc)
            _progressFunc(message, increment, amount);
    }

    void cancelCheck() const
    {
        if (_cancelToken)
            async::interruption_point(*_cancelToken);
    }

    using ProgressFunc = std::function<void(std::string, float, float)>;

    void setProgressFunc(const ProgressFunc& progressFunc)
    {
        _progressFunc = progressFunc;
    }
    void setCancelToken(async::cancellation_token& cancelToken)
    {
        _cancelToken = &cancelToken;
    }

protected:
    async::cancellation_token* _cancelToken{nullptr};
    ProgressFunc _progressFunc;
};

class Task
{
public:
    virtual ~Task() = default;
    void cancel(std::function<void()> done = {})
    {
        _cancelled = true;
        _cancelDone = done;
        _cancelToken.cancel();
        _cancel();
    }
    bool canceled() const { return _cancelled; }
    virtual void wait() = 0;

    virtual void schedule() {}
    void progress(const std::string& message, const float amount)
    {
        _progress.update(message, amount);
    }

    Progress2& getProgress() { return _progress; }
    void finishCancel()
    {
        if (_cancelDone)
            _cancelDone();
    }

protected:
    async::cancellation_token _cancelToken;
    Progress2 _progress{"Scheduling task ..."};
    std::function<void()> _cancelDone;
    bool _cancelled{false};

private:
    virtual void _cancel() {}
};

template <typename T>
class TaskT : public Task
{
public:
    using Type = async::task<T>;

    using Task::Task;

    TaskT() = default;

    template <typename F>
    TaskT(F&& functor)
    {
        _setupFunctor(functor);
        _task = async::spawn(functor);
    }

    void wait() final { _task.wait(); }
    auto operator-> () { return &_task; }
    auto& task() { return _task; }
protected:
    Type _task;

    template <typename F>
    auto&& _setupFunctor(F&& functor)
    {
        if (std::is_base_of<TaskFunctor, F>::value)
        {
            auto& taskFunctor = static_cast<TaskFunctor&>(functor);
            taskFunctor.setProgressFunc(
                std::bind(&Progress2::update, std::ref(_progress),
                          std::placeholders::_1, std::placeholders::_3));
            taskFunctor.setCancelToken(_cancelToken);
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
        TaskT<T>::_task = _e.get_task().then(
            TaskT<T>::template _setupFunctor(std::move(functor)));
    }

    void schedule() final { _e.set(); }
private:
    async::event_task<void> _e;
};
}
