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

#include <brayns/common/Progress.h>

namespace brayns
{
class LoadDataFunctor : public TaskFunctor
{
public:
    LoadDataFunctor(EnginePtr engine);
    ~LoadDataFunctor();
    void operator()(Blob&& blob);

private:
    void _loadData(Blob&& blob, Progress& loadingProgress);
    void _loadXYZBBlob(Blob&& blob,
                       const Progress::UpdateCallback& progressUpdate);
    void _loadMeshBlob(Blob&& blob,
                       const Progress::UpdateCallback& progressUpdate);

    void _postLoad(Progress& loadingProgress, bool cancellable = true);

    EnginePtr _engine;
    bool _empty{false};
};
}
