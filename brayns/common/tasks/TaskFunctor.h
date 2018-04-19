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
class TaskFunctor
{
public:
    TaskFunctor() = default;

    void progress(const std::string& message, const float increment,
                  const float amount)
    {
        cancelCheck();
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
}
