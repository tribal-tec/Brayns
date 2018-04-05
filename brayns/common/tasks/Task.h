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
using ProgressFunc = std::function<void(std::string, float)>;

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
    TaskFunctor(async::cancellation_token& cancelToken)
        : _cancelToken(&cancelToken)

    {
    }
    void progress(const std::string& message, const float amount)
    {
        if (progressFunc)
            progressFunc(message, amount);
    }

    void cancelCheck()
    {
        if (_cancelToken)
            async::interruption_point(*_cancelToken);
    }

    ProgressFunc progressFunc;
    void setCancelToken(async::cancellation_token& cancelToken)
    {
        _cancelToken = &cancelToken;
    }

private:
    async::cancellation_token* _cancelToken{nullptr};
};

class Task
{
public:
    virtual ~Task() = default;
    void cancel()
    {
        _cancelToken.cancel();
        _cancel();
    }
    virtual void wait() = 0;

    void setProgressUpdatedCallback(const std::function<void(Progress2&)>& cb)
    {
        progressUpdated = cb;
    }

    void setRequestID(const std::string& requestID)
    {
        _progress.requestID = requestID;
    }

protected:
    async::cancellation_token _cancelToken;
    Progress2 _progress;
    std::function<void(Progress2&)> progressUpdated;

private:
    virtual void _cancel() {}
};

template <typename T>
class SimpleTask : public Task
{
public:
    using Type = async::task<T>;

    SimpleTask() = default;
    template <typename F>
    SimpleTask(F&& functor)
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
            taskFunctor.setCancelToken(_cancelToken);
        }

        _task = _e.get_task().then(functor);
    }

    void schedule() { _e.set(); }
    void wait() final { _task.wait(); }
    auto operator-> () { return &_task; }
    auto& task() { return _task; }
protected:
    async::event_task<void> _e;
    Type _task;
};
}
