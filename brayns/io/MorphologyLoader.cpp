/* Copyright (c) 2015-2017, EPFL/Blue Brain Project
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

#include "MorphologyLoader.h"

#include <brayns/common/geometry/Cone.h>
#include <brayns/common/geometry/Cylinder.h>
#include <brayns/common/geometry/Sphere.h>
#include <brayns/common/log.h>
#include <brayns/common/scene/Scene.h>
#include <brayns/io/algorithms/MetaballsGenerator.h>
#include <brayns/io/simulation/CircuitSimulationHandler.h>

#include <brain/brain.h>
#include <brion/brion.h>
#include <servus/types.h>

//#include <algorithm>
#include <sstream>

namespace
{
// needs to be the same in SimulationRenderer.ispc
const float INDEX_MAGIC = 1e6;
}

namespace brayns
{
struct ParallelSceneContainer
{
public:
    ParallelSceneContainer(SpheresMap& s, CylindersMap& cy, ConesMap& co,
                           TrianglesMeshMap& tm, Materials& m, Boxf& wb)
        : spheres(s)
        , cylinders(cy)
        , cones(co)
        , trianglesMeshes(tm)
        , materials(m)
        , worldBounds(wb)
    {
    }

    void _buildMissingMaterials(const size_t materialId)
    {
        if (materialId >= materials.size())
            materials.resize(materialId + 1);
    }

    void addSphere(const size_t materialId, const Sphere& sphere)
    {
        _buildMissingMaterials(materialId);
        spheres[materialId].push_back(sphere);
        worldBounds.merge(sphere.center);
    }

    void addCylinder(const size_t materialId, const Cylinder& cylinder)
    {
        _buildMissingMaterials(materialId);
        cylinders[materialId].push_back(cylinder);
        worldBounds.merge(cylinder.center);
        worldBounds.merge(cylinder.up);
    }

    void addCone(const size_t materialId, const Cone& cone)
    {
        _buildMissingMaterials(materialId);
        cones[materialId].push_back(cone);
        worldBounds.merge(cone.center);
        worldBounds.merge(cone.up);
    }

    SpheresMap& spheres;
    CylindersMap& cylinders;
    ConesMap& cones;
    TrianglesMeshMap& trianglesMeshes;
    Materials& materials;
    Boxf& worldBounds;
};

class MorphologyLoader::Impl
{
public:
    Impl(const GeometryParameters& geometryParameters,
         const size_t materialOffset)
        : _geometryParameters(geometryParameters)
        , _materialsOffset(materialOffset)
    {
    }

    /**
     * @brief importMorphology imports a single morphology from a specified URI
     * @param uri URI of the morphology
     * @param index Index of the morphology
     * @param material Material to use
     * @param transformation Transformation to apply to the morphology
     * @param compartmentReport Compartment report to map to the morphology
     * @return True is the morphology was successfully imported, false otherwise
     */
    bool importMorphology(const servus::URI& source, Scene& scene,
                          const uint64_t index, const size_t material,
                          const Matrix4f& transformation,
                          const GIDOffsets& targetGIDOffsets,
                          CompartmentReportPtr compartmentReport = nullptr)
    {
        _materialsOffset = scene.getMaterials().size();
        ParallelSceneContainer sceneContainer(scene.getSpheres(),
                                              scene.getCylinders(),
                                              scene.getCones(),
                                              scene.getTriangleMeshes(),
                                              scene.getMaterials(),
                                              scene.getWorldBounds());

        return _importMorphology(source, index, material, transformation,
                                 compartmentReport, targetGIDOffsets,
                                 sceneContainer);
    }

private:
    /**
     * @brief _getCorrectedRadius Modifies the radius of the geometry according
     * to --radius-multiplier and --radius-correction geometry parameters
     * @param radius Radius to be corrected
     * @return Corrected value of a radius according to geometry parameters
     */
    float _getCorrectedRadius(const float radius) const
    {
        return (_geometryParameters.getRadiusCorrection() != 0.f
                    ? _geometryParameters.getRadiusCorrection()
                    : radius * _geometryParameters.getRadiusMultiplier());
    }

    /**
     * @brief _getMaterialFromSectionType return a material determined by the
     * --color-scheme geometry parameter
     * @param index Index of the element to which the material will attached
     * @param material Material that is forced in case geometry parameters
     * do not apply
     * @param sectionType Section type of the geometry to which the material
     * will be applied
     * @return Material ID determined by the geometry parameters
     */
    size_t _getMaterialFromGeometryParameters(
        const uint64_t index, const size_t material,
        const brain::neuron::SectionType sectionType,
        const GIDOffsets& targetGIDOffsets, bool isMesh = false) const
    {
        if (material != NO_MATERIAL)
            return _materialsOffset + material;

        if (!isMesh && _geometryParameters.getCircuitUseSimulationModel())
            return _materialsOffset;

        size_t materialId = 0;
        switch (_geometryParameters.getColorScheme())
        {
        case ColorScheme::neuron_by_id:
            materialId = index;
            break;
        case ColorScheme::neuron_by_segment_type:
            switch (sectionType)
            {
            case brain::neuron::SectionType::soma:
                materialId = 1;
                break;
            case brain::neuron::SectionType::axon:
                materialId = 2;
                break;
            case brain::neuron::SectionType::dendrite:
                materialId = 3;
                break;
            case brain::neuron::SectionType::apicalDendrite:
                materialId = 4;
                break;
            default:
                materialId = 0;
                break;
            }
            break;
        case ColorScheme::neuron_by_target:
            for (size_t i = 0; i < targetGIDOffsets.size() - 1; ++i)
                if (index >= targetGIDOffsets[i] &&
                    index < targetGIDOffsets[i + 1])
                {
                    materialId = i;
                    break;
                }
            break;
        case ColorScheme::neuron_by_etype:
            if (index < _electrophysiologyTypes.size())
                materialId = _electrophysiologyTypes[index];
            else
                BRAYNS_DEBUG << "Failed to get neuron E-type" << std::endl;
            break;
        case ColorScheme::neuron_by_mtype:
            if (index < _morphologyTypes.size())
                materialId = _morphologyTypes[index];
            else
                BRAYNS_DEBUG << "Failed to get neuron M-type" << std::endl;
            break;
        case ColorScheme::neuron_by_layer:
            if (index < _layerIds.size())
                materialId = _layerIds[index];
            else
                BRAYNS_DEBUG << "Failed to get neuron layer" << std::endl;
            break;
        default:
            materialId = NO_MATERIAL;
        }
        return _materialsOffset + materialId;
    }

    /**
     * @brief _getSectionTypes converts Brayns section types into brain::neuron
     * section types
     * @param morphologySectionTypes Brayns section types
     * @return brain::neuron section types
     */
    brain::neuron::SectionTypes _getSectionTypes(
        const size_t morphologySectionTypes) const
    {
        brain::neuron::SectionTypes sectionTypes;
        if (morphologySectionTypes &
            static_cast<size_t>(MorphologySectionType::soma))
            sectionTypes.push_back(brain::neuron::SectionType::soma);
        if (morphologySectionTypes &
            static_cast<size_t>(MorphologySectionType::axon))
            sectionTypes.push_back(brain::neuron::SectionType::axon);
        if (morphologySectionTypes &
            static_cast<size_t>(MorphologySectionType::dendrite))
            sectionTypes.push_back(brain::neuron::SectionType::dendrite);
        if (morphologySectionTypes &
            static_cast<size_t>(MorphologySectionType::apical_dendrite))
            sectionTypes.push_back(brain::neuron::SectionType::apicalDendrite);
        return sectionTypes;
    }

    /**
     * @brief _getIndexAsTextureCoordinates converts a uint64_t index into 2
     * floats so that it can be stored in the texture coordinates of the the
     * geometry to which it is attached
     * @param index Index to be stored in texture coordinates
     * @return Texture coordinates for the given index
     */
    Vector2f _getIndexAsTextureCoordinates(const uint64_t index) const
    {
        Vector2f textureCoordinates;

        // https://stackoverflow.com/questions/2810280
        float x = ((index & 0xFFFFFFFF00000000LL) >> 32) / INDEX_MAGIC;
        float y = (index & 0xFFFFFFFFLL) / INDEX_MAGIC;

        textureCoordinates.x() = x;
        textureCoordinates.y() = y;
        return textureCoordinates;
    }

    /**
     * @brief _importMorphologyAsPoint places sphere at the specified morphology
     * position
     * @param index Index of the current morphology
     * @param transformation Transformation to apply to the morphology
     * @param material Material that is forced in case geometry parameters do
     * not apply
     * @param compartmentReport Compartment report to map to the morphology
     * @param scene Scene to which the morphology should be loaded into
     * @return True if the loading was successful, false otherwise
     */
    bool _importMorphologyAsPoint(const uint64_t index, const size_t material,
                                  const Matrix4f& transformation,
                                  CompartmentReportPtr compartmentReport,
                                  const GIDOffsets& targetGIDOffsets,
                                  ParallelSceneContainer& scene)
    {
        uint64_t offset = 0;
        if (compartmentReport)
            offset = compartmentReport->getOffsets()[index][0];

        const auto radius = _geometryParameters.getRadiusMultiplier();
        const auto textureCoordinates = _getIndexAsTextureCoordinates(offset);
        const auto somaPosition = transformation.getTranslation();
        const auto materialId =
            _getMaterialFromGeometryParameters(index, material,
                                               brain::neuron::SectionType::soma,
                                               targetGIDOffsets);
        scene.addSphere(materialId,
                        {somaPosition, radius, 0.f, textureCoordinates});
        return true;
    }

    /**
     * @brief _createRealisticSoma Creates a realistic soma using the metaballs
     * algorithm.
     * @param uri URI of the morphology for which the soma is created
     * @param index Index of the current morphology
     * @param transformation Transformation to apply to the morphology
     * @param material Material that is forced in case geometry parameters
     * do not apply
     * @param scene Scene to which the morphology should be loaded into
     * @return True if the loading was successful, false otherwise
     */
    bool _createRealisticSoma(const servus::URI& uri, const uint64_t index,
                              const size_t material,
                              const Matrix4f& transformation,
                              const GIDOffsets& targetGIDOffsets,
                              ParallelSceneContainer& scene)
    {
        try
        {
            const size_t morphologySectionTypes =
                enumsToBitmask(_geometryParameters.getMorphologySectionTypes());

            brain::neuron::Morphology morphology(uri, transformation);
            const auto sectionTypes = _getSectionTypes(morphologySectionTypes);
            const auto& sections = morphology.getSections(sectionTypes);

            Vector4fs metaballs;
            if (morphologySectionTypes & size_t(MorphologySectionType::soma))
            {
                // Soma
                const auto& soma = morphology.getSoma();
                const auto center = soma.getCentroid();
                const auto radius = _getCorrectedRadius(soma.getMeanRadius());
                metaballs.push_back(
                    Vector4f(center.x(), center.y(), center.z(), radius));
                scene.worldBounds.merge(center);
            }

            // Dendrites and axon
            for (const auto& section : sections)
            {
                const auto hasParent = section.hasParent();
                if (hasParent)
                {
                    const auto parentSectionType =
                        section.getParent().getType();
                    if (parentSectionType != brain::neuron::SectionType::soma)
                        continue;
                }

                const auto& samples = section.getSamples();
                if (samples.empty())
                    continue;

                const auto samplesFromSoma =
                    _geometryParameters.getMetaballsSamplesFromSoma();
                const auto samplesToProcess =
                    std::min(samplesFromSoma, samples.size());
                for (size_t i = 0; i < samplesToProcess; ++i)
                {
                    const auto& sample = samples[i];
                    const Vector3f position(sample.x(), sample.y(), sample.z());
                    const auto radius = _getCorrectedRadius(sample.w() * 0.5f);
                    if (radius > 0.f)
                        metaballs.push_back(Vector4f(position.x(), position.y(),
                                                     position.z(), radius));

                    scene.worldBounds.merge(position);
                }
            }

            // Generate mesh from metaballs
            const auto gridSize = _geometryParameters.getMetaballsGridSize();
            const auto threshold = _geometryParameters.getMetaballsThreshold();
            MetaballsGenerator metaballsGenerator;
            const auto materialId = _getMaterialFromGeometryParameters(
                index, material, brain::neuron::SectionType::soma,
                targetGIDOffsets);
            metaballsGenerator.generateMesh(metaballs, gridSize, threshold,
                                            scene.materials, materialId,
                                            scene.trianglesMeshes);
        }
        catch (const std::runtime_error& e)
        {
            BRAYNS_ERROR << e.what() << std::endl;
            return false;
        }
        return true;
    }

    /**
     * @brief _importMorphologyFromURI imports a morphology from the specified
     * URI
     * @param uri URI of the morphology
     * @param index Index of the current morphology
     * @param transformation Transformation to apply to the morphology
     * @param material Material that is forced in case geometry parameters
     * do not apply
     * @param compartmentReport Compartment report to map to the morphology
     * @param scene Scene to which the morphology should be loaded into
     * @return True if the loading was successful, false otherwise
     */
    bool _importMorphologyFromURI(const servus::URI& uri, const uint64_t index,
                                  const size_t material,
                                  const Matrix4f& transformation,
                                  CompartmentReportPtr compartmentReport,
                                  const GIDOffsets& targetGIDOffsets,
                                  ParallelSceneContainer& scene) const
    {
        try
        {
            Vector3f translation;

            const size_t morphologySectionTypes =
                enumsToBitmask(_geometryParameters.getMorphologySectionTypes());

            brain::neuron::Morphology morphology(uri, transformation);
            brain::neuron::SectionTypes sectionTypes;

            const MorphologyLayout& layout =
                _geometryParameters.getMorphologyLayout();

            if (layout.nbColumns != 0)
            {
                Boxf morphologyAABB;
                const auto& points = morphology.getPoints();
                for (const auto& point : points)
                    morphologyAABB.merge({point.x(), point.y(), point.z()});

                const Vector3f positionInGrid = {
                    -1.f * layout.horizontalSpacing *
                        static_cast<float>(index % layout.nbColumns),
                    -1.f * layout.verticalSpacing *
                        static_cast<float>(index / layout.nbColumns),
                    0.f};
                translation = positionInGrid - morphologyAABB.getCenter();
            }

            sectionTypes = _getSectionTypes(morphologySectionTypes);

            uint64_t offset = 0;

            if (compartmentReport)
                offset = compartmentReport->getOffsets()[index][0];

            // Soma
            if (!_geometryParameters.useRealisticSomas() &&
                morphologySectionTypes &
                    static_cast<size_t>(MorphologySectionType::soma))
            {
                const auto& soma = morphology.getSoma();
                const size_t materialId = _getMaterialFromGeometryParameters(
                    index, material, brain::neuron::SectionType::soma,
                    targetGIDOffsets);
                const auto somaPosition = soma.getCentroid() + translation;
                const auto radius = _getCorrectedRadius(soma.getMeanRadius());
                const auto textureCoordinates =
                    _getIndexAsTextureCoordinates(offset);
                scene.addSphere(materialId, {somaPosition, radius, 0.f,
                                             textureCoordinates});

                if (_geometryParameters.getCircuitUseSimulationModel())
                {
                    // When using a simulation model, parametric geometries must
                    // occupy as much space as possible in the mesh. This code
                    // inserts a Cone between the soma and the beginning of each
                    // branch.
                    const auto& children = soma.getChildren();
                    for (const auto& child : children)
                    {
                        const auto& samples = child.getSamples();
                        const Vector3f sample{samples[0].x(), samples[0].y(),
                                              samples[0].z()};
                        scene.addCone(materialId, {somaPosition, sample, radius,
                                                   _getCorrectedRadius(
                                                       samples[0].w() * 0.5f),
                                                   0.f, textureCoordinates});
                    }
                }
            }

            // Only the first one or two axon sections are reported, so find the
            // last one and use its offset for all the other axon sections
            uint16_t lastAxon = 0;
            if (compartmentReport &&
                (morphologySectionTypes &
                 static_cast<size_t>(MorphologySectionType::axon)))
            {
                const auto& counts =
                    compartmentReport->getCompartmentCounts()[index];
                const auto& axon =
                    morphology.getSections(brain::neuron::SectionType::axon);
                for (const auto& section : axon)
                {
                    if (counts[section.getID()] > 0)
                    {
                        lastAxon = section.getID();
                        continue;
                    }
                    break;
                }
            }

            // Dendrites and axon
            for (const auto& section : morphology.getSections(sectionTypes))
            {
                if (section.getType() == brain::neuron::SectionType::soma)
                    continue;

                const auto materialId =
                    _getMaterialFromGeometryParameters(index, material,
                                                       section.getType(),
                                                       targetGIDOffsets);
                const auto& samples = section.getSamples();
                if (samples.empty())
                    continue;

                auto previousSample = samples[0];
                size_t step = 1;
                switch (_geometryParameters.getGeometryQuality())
                {
                case GeometryQuality::low:
                    step = samples.size() - 1;
                    break;
                case GeometryQuality::medium:
                    step = samples.size() / 2;
                    step = (step == 0) ? 1 : step;
                    break;
                default:
                    step = 1;
                }

                const float distanceToSoma = section.getDistanceToSoma();
                const floats& distancesToSoma =
                    section.getSampleDistancesToSoma();

                float segmentStep = 0.f;
                if (compartmentReport)
                {
                    const auto& counts =
                        compartmentReport->getCompartmentCounts()[index];
                    // Number of compartments usually differs from number of
                    // samples
                    segmentStep =
                        counts[section.getID()] / float(samples.size());
                }

                bool done = false;
                for (size_t i = step; !done && i < samples.size() + step;
                     i += step)
                {
                    if (i >= samples.size())
                    {
                        i = samples.size() - 1;
                        done = true;
                    }

                    const auto distance = distanceToSoma + distancesToSoma[i];

                    if (compartmentReport)
                    {
                        const auto& offsets =
                            compartmentReport->getOffsets()[index];
                        const auto& counts =
                            compartmentReport->getCompartmentCounts()[index];

                        // update the offset if we have enough compartments aka
                        // a full compartment report. Otherwise we keep the soma
                        // offset which happens for soma reports and use this
                        // for all the sections
                        if (section.getID() < counts.size())
                        {
                            if (counts[section.getID()] > 0)
                                offset = offsets[section.getID()] +
                                         float(i - step) * segmentStep;
                            else
                            {
                                if (section.getType() ==
                                    brain::neuron::SectionType::axon)
                                {
                                    offset = offsets[lastAxon];
                                }
                                else
                                    // This should never happen, but just in
                                    // case use an invalid value to show an
                                    // error color
                                    offset =
                                        std::numeric_limits<uint64_t>::max();
                            }
                        }
                    }

                    const auto sample = samples[i];
                    const auto previousRadius =
                        _getCorrectedRadius(samples[i - step].w() * 0.5f);

                    Vector3f position(sample.x(), sample.y(), sample.z());
                    position += translation;
                    Vector3f target(previousSample.x(), previousSample.y(),
                                    previousSample.z());
                    target += translation;
                    const auto textureCoordinates =
                        _getIndexAsTextureCoordinates(offset);
                    const auto radius =
                        _getCorrectedRadius(samples[i].w() * 0.5f);

                    if (radius > 0.f)
                    {
                        scene.addSphere(materialId, {position, radius, distance,
                                                     textureCoordinates});

                        if (position != target && previousRadius > 0.f)
                        {
                            if (radius == previousRadius)
                                scene.addCylinder(materialId,
                                                  {position, target, radius,
                                                   distance,
                                                   textureCoordinates});
                            else
                                scene.addCone(materialId,
                                              {position, target, radius,
                                               previousRadius, distance,
                                               textureCoordinates});
                        }
                    }
                    previousSample = sample;
                }
            }
        }
        catch (const std::runtime_error& e)
        {
            BRAYNS_ERROR << e.what() << std::endl;
            return false;
        }
        return true;
    }

public:
    bool _importMorphology(const servus::URI& source, const uint64_t index,
                           const size_t material,
                           const Matrix4f& transformation,
                           CompartmentReportPtr compartmentReport,
                           const GIDOffsets& targetGIDOffsets,
                           ParallelSceneContainer& scene)
    {
        bool returnValue = true;
        const size_t morphologySectionTypes =
            enumsToBitmask(_geometryParameters.getMorphologySectionTypes());
        if (morphologySectionTypes ==
            static_cast<size_t>(MorphologySectionType::soma))
            return _importMorphologyAsPoint(index, material, transformation,
                                            compartmentReport, targetGIDOffsets,
                                            scene);
        else if (_geometryParameters.useRealisticSomas())
            returnValue =
                _createRealisticSoma(source, index, material, transformation,
                                     targetGIDOffsets, scene);
        returnValue =
            returnValue &&
            _importMorphologyFromURI(source, index, material, transformation,
                                     compartmentReport, targetGIDOffsets,
                                     scene);
        return returnValue;
    }

private:
    const GeometryParameters& _geometryParameters;
    size_ts _layerIds;
    size_ts _electrophysiologyTypes;
    size_ts _morphologyTypes;
    size_t _materialsOffset{0};
};

MorphologyLoader::MorphologyLoader(const GeometryParameters& geometryParameters,
                                   const size_t materialOffset)
    : _impl(new MorphologyLoader::Impl(geometryParameters, materialOffset))
{
}

MorphologyLoader::~MorphologyLoader()
{
}

std::set<std::string> MorphologyLoader::getSupportedDataTypes()
{
    return {"h5", "swc"};
}

void MorphologyLoader::importFromBlob(Blob&& /*blob*/, Scene& /*scene*/,
                                      const Matrix4f& /*transformation*/,
                                      const size_t /*materialID*/)
{
    throw std::runtime_error("Load morphology from memory not supported");
}

void MorphologyLoader::importFromFile(const std::string& filename, Scene& scene,
                                      const Matrix4f& transformation,
                                      const size_t materialID)
{
    importMorphology(servus::URI(filename), scene, 0, materialID,
                     transformation);
}

bool MorphologyLoader::importMorphology(const servus::URI& uri, Scene& scene,
                                        const uint64_t index,
                                        const size_t material,
                                        const Matrix4f& transformation)
{
    const GIDOffsets targetGIDOffsets;
    return _impl->importMorphology(uri, scene, index, material, transformation,
                                   targetGIDOffsets);
}

bool MorphologyLoader::_importMorphology(const servus::URI& source,
                                         const uint64_t index,
                                         const size_t material,
                                         const vmml::Matrix4f& transformation,
                                         CompartmentReportPtr compartmentReport,
                                         const GIDOffsets& targetGIDOffsets,
                                         ParallelSceneContainer& scene)
{
    return _impl->_importMorphology(source, index, material, transformation,
                                    compartmentReport, targetGIDOffsets, scene);
}
}
