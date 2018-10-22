/* Copyright (c) 2015-2018, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
 * Responsible Author: Cyrille Favreau <cyrille.favreau@epfl.ch>
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

#include <brayns/common/scene/Model.h>
#include <brayns/parameters/VolumeParameters.h>

#include <ospray.h>
#include <ospray/SDK/common/OSPCommon.h>

namespace brayns
{
namespace
{
template <typename VecT>
OSPData allocateVectorData(const std::vector<VecT>& vec,
                        const OSPDataType ospType,
                        const size_t memoryManagementFlags)
{
    const size_t totBytes = vec.size() * sizeof(VecT);

    if (totBytes >= INT_MAX)
        BRAYNS_INFO << "Buffer allocation (" << std::to_string(totBytes)
                    << " bytes) exceeds ispc 32-bit address space."
                    << std::endl;

    return ospNewData(totBytes / ospray::sizeOf(ospType), ospType, vec.data(),
                    memoryManagementFlags);
}
}
class OSPRayModel : public Model
{
public:
    OSPRayModel(AnimationParameters& animationParameters,
                VolumeParameters& volumeParameters);
    ~OSPRayModel() final;

    void setMemoryFlags(const size_t memoryManagementFlags);

    void commitGeometry() final;
    bool commitTransferFunction();

    OSPModel getModel() const { return _model; }
    OSPModel getBoundingBoxModel() const { return _boundingBoxModel; }
    OSPModel getSimulationModel() const { return _simulationModel; }
    MaterialPtr createMaterial(const size_t materialId,
                               const std::string& name) final;
    SharedDataVolumePtr createSharedDataVolume(const Vector3ui& dimensions,
                                               const Vector3f& spacing,
                                               const DataType type) const final;
    BrickedVolumePtr createBrickedVolume(const Vector3ui& dimensions,
                                         const Vector3f& spacing,
                                         const DataType type) const final;

    void buildBoundingBox() final;

    OSPData simulationData() const { return _ospSimulationData; }
    OSPTransferFunction transferFunction() const
    {
        return _ospTransferFunction;
    }

private:
    void _commitSpheres(const size_t materialId);
    void _commitCylinders(const size_t materialId);
    void _commitCones(const size_t materialId);
    void _commitMeshes(const size_t materialId);
    void _commitStreamlines(const size_t materialId);
    void _commitSDFGeometries();
    bool _commitTransferFunction();
    bool _commitSimulationData();

    using GeometryMap = std::map<size_t, OSPGeometry>;
    template<typename T>
    OSPGeometry _createPrimitive(const size_t materialId, const char* name, GeometryMap& ospContainer, const T& container)
    {
        auto it = ospContainer.find(materialId);
        if (it != ospContainer.end())
        {
            ospRemoveGeometry(_model, it->second);
            ospRelease(it->second);
        }
        else
            it = ospContainer.insert({materialId, ospNewGeometry(name)}).first;

        auto& ospPrimitive = it->second;

        auto primitivData = allocateVectorData(container.at(materialId), OSP_FLOAT,
                                            _memoryManagementFlags);

        ospSetObject(ospPrimitive, name, primitivData);
        ospRelease(primitivData);
        return ospPrimitive;
    }
    void _addPrimitive(const size_t materialId, OSPGeometry primitive);

    AnimationParameters& _animationParameters;
    VolumeParameters& _volumeParameters;

    // Whether this model has set the AnimationParameters "is ready" callback
    bool _setIsReadyCallback{false};

    OSPModel _model{nullptr};

    // Bounding box
    size_t _boudingBoxMaterialId{0};
    OSPModel _boundingBoxModel{nullptr};

    // Simulation model
    OSPModel _simulationModel{nullptr};
    OSPData _ospSimulationData{nullptr};

    OSPTransferFunction _ospTransferFunction{nullptr};

    // OSPRay data
    GeometryMap _ospSpheres;
    GeometryMap _ospCylinders;
    GeometryMap _ospCones;
    GeometryMap _ospMeshes;
    GeometryMap _ospStreamlines;
    GeometryMap _ospSDFGeometryRefs;

    size_t _memoryManagementFlags{OSP_DATA_SHARED_BUFFER};
};
}
