/* Copyright (c) 2015-2018, EPFL/Blue Brain Project
 *                          Daniel.Nachbaur@epfl.ch
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

#include "SocketListener.h"

#include <iostream>
#include <poll.h>

namespace brayns
{
SocketListener::SocketListener(rockets::SocketBasedInterface& interface)
    : _iface{interface}
{
}

void SocketListener::onNewSocket(const rockets::SocketDescriptor fd,
                                 const int mode)
{
    auto handle = uvw::Loop::getDefault()->resource<uvw::PollHandle>(fd);
    _handles.emplace(fd, handle);

    uvw::Flags<uvw::PollHandle::Event> flags;
    if (mode & POLLIN)
        flags = flags | uvw::Flags<uvw::PollHandle::Event>(
                            uvw::PollHandle::Event::READABLE);
    if (mode & POLLOUT)
        flags = flags | uvw::Flags<uvw::PollHandle::Event>(
                            uvw::PollHandle::Event::WRITABLE);
    handle->start(flags);

    handle->on<uvw::PollEvent>(
        [this, fd, mode](const uvw::PollEvent&, uvw::PollHandle&) {
            _iface.processSocket(fd, mode);
        });
}

void SocketListener::onUpdateSocket(const rockets::SocketDescriptor /*fd*/,
                                    const int /*mode*/)
{
    // std::cerr << "Unimplemented " << mode << std::endl;
}

void SocketListener::onDeleteSocket(const rockets::SocketDescriptor fd)
{
    _handles.erase(fd);
}
}
