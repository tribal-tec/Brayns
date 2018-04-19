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

#include <brayns/common/BaseObject.h>

#include <mutex>
#include <string>

namespace brayns
{
class Progress : public BaseObject
{
public:
    Progress() = default;
    explicit Progress(const std::string& operation)
        : _operation(operation)
    {
    }

    void update(const std::string& operation, const float amount)
    {
        std::lock_guard<std::mutex> lock_(_mutex);
        _updateValue(_operation, operation);
        _updateValue(_amount, amount);
    }

    void increment(const std::string& operation, const float amount)
    {
        std::lock_guard<std::mutex> lock_(_mutex);
        _updateValue(_operation, operation);
        _updateValue(_amount, _amount + amount);
    }

    const std::string& operation() const { return _operation; }
    float amount() const { return _amount; }
    std::mutex& mutex() const { return _mutex; }
private:
    std::string _operation;
    float _amount{0.f};
    mutable std::mutex _mutex;
};
}
