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

#include <memory>

#include "transwarp.h"
namespace tw = transwarp;

namespace brayns
{
/**
 *
 */
class Task
{
public:
    template <typename F>
    Task(F&& functor)
        : _task{tw::make_task(tw::root, functor)}
    {
        _task->schedule(executor());
    }

    void cancel() { _task->cancel(true); }
private:
    std::shared_ptr<tw::itask> _task;
    static tw::parallel& executor()
    {
        static tw::parallel _executor{4};
        return _executor;
    }
    // class Impl;
    // std::unique_ptr<Impl> _impl;
};
}
