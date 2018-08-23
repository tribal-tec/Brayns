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

#include "Throttle.h"

namespace brayns
{
namespace
{
using namespace std::chrono;
auto now()
{
    return high_resolution_clock::now();
}
auto elapsedSince(const time_point<high_resolution_clock>& last)
{
    return duration_cast<milliseconds>(now() - last).count();
}
}

void Throttle::operator()(const std::function<void()>& fn, const int64_t wait)
{
    [&] {
        if (_haveLast && (elapsedSince(_last) <= wait))
        {
            _timeout.clear();
            auto later = [& _last = _last, fn ]
            {
                _last = now();
                fn();
            };
            _timeout.set(later, wait);
        }
        else
        {
            _last = now();
            _haveLast = true;
            fn();
        }
    }();
}
}
