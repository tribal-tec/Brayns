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

#include <brayns/common/tasks/Task.h>

namespace brayns
{
class LoadDataTask : public TaskFunctor
{
public:
    LoadDataTask(const size_t totalBytes)
        : _totalBytes(totalBytes)
    {
    }

    void operator()()
    {
        //        std::cout << "Scheduled, waiting ..." << std::endl;

        //        std::unique_lock<std::mutex> lock(_mutex);
        //        _cond.wait(lock);

        std::cout << "Loading" << std::endl;
    }

    void appendChunk(const std::string& chunk)
    {
        _data += chunk;
        _receivedBytes += chunk.size();

        if (_receivedBytes == _totalBytes)
        {
            std::cout << "Got all" << std::endl;
            //            std::unique_lock<std::mutex> lock(_mutex);
            //            _cond.notify_one();
        }
        // progress.setAmount(_progress());
        // progress.setOperation("Receiving data...");
    }

private:
    std::string _data;
    size_t _receivedBytes{0};
    size_t _totalBytes{0};

    //    std::mutex _mutex;
    //    std::condition_variable _cond;
};

// auto createLoadDataTask(const size_t totalBytes)
//{
//    return std::make_shared<TaskT<void>>(LoadDataTask{totalBytes});
//}
}
