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

#include "MorphologyLoader.h"
#include "ModelData.h"

#include <brayns/common/material/Material.h>
#include <brayns/common/scene/Model.h>
#include <brayns/common/scene/Scene.h>
#include <brayns/common/types.h>
#include <brayns/common/utils/utils.h>

#include <brain/brain.h>

#include <boost/filesystem.hpp>

#include <unordered_map>

namespace brayns
{
namespace
{
// From http://en.cppreference.com/w/cpp/types/numeric_limits/epsilon
template <class T>
typename std::enable_if<!std::numeric_limits<T>::is_integer, bool>::type
    _almostEqual(T x, T y)
{
    constexpr auto ULPs{100000.f};
    // the machine epsilon has to be scaled to the magnitude of the values used
    // and multiplied by the desired precision in ULPs (units in the last place)
    return std::abs(x - y) <=
               std::numeric_limits<T>::epsilon() * std::abs(x + y) * ULPs
           // unless the result is subnormal
           || std::abs(x - y) < std::numeric_limits<T>::min();
}

using Property = brayns::Property;

const Property PROP_COLOR_SCHEME = {"colorScheme",
                                    brayns::enumToString(
                                        brayns::ColorScheme::none),
                                    brayns::enumNames<brayns::ColorScheme>(),
                                    {"Color scheme"}};
const Property PROP_RADIUS_MULTIPLIER = {"radiusMultiplier",
                                         1.0,
                                         {"Radius multiplier"}};
const Property PROP_RADIUS_CORRECTION = {"radiusCorrection",
                                         0.,
                                         {"Radius correction"}};
const Property PROP_SECTION_TYPES_SOMA = {"sectionTypesSoma",
                                          true,
                                          {"Enable 'Soma' section type"}};
const Property PROP_SECTION_TYPES_AXON = {"sectionTypesAxon",
                                          true,
                                          {"Enable 'Axon' section type"}};
const Property PROP_SECTION_TYPES_DENDRITE = {
    "sectionTypesDendrite", true, {"Enable 'Dendrite' section type"}};
const Property PROP_SECTION_TYPES_APICAL_DENDRITE = {
    "sectionTypesApicalDendrite",
    true,
    {"Enable 'Apical Dendrite' section type"}};
const Property PROP_DAMPEN_BRANCH_THICKNESS_CHANGERATE = {
    "dampenBranchThicknessChangerate",
    false,
    {"Dampen branch thickness changerate"}};
const Property PROP_USE_SDF_GEOMETRIES = {"useSdfGeometries",
                                          false,
                                          {"Use SDF geometries"}};
const Property PROP_GEOMETRY_QUALITY = {
    "geometryQuality",
    brayns::enumToString(brayns::GeometryQuality::high),
    brayns::enumNames<brayns::GeometryQuality>(),
    {"Geometry quality"}};

const auto LOADER_NAME = "morphology";

struct SDFData
{
    std::vector<SDFGeometry> geometries;
    std::vector<std::set<size_t>> neighbours;
    std::vector<size_t> materials;
    // Bifurcation section ID to geometry ID map.
    std::unordered_map<uint32_t, size_t> bifurcations;
    std::unordered_map<uint32_t, std::vector<size_t>> sectionGeometries;
};

bool _isInMask(const brain::neuron::SectionType type, const size_t mask)
{
    switch (type)
    {
    case brain::neuron::SectionType::soma:
        return mask & static_cast<size_t>(MorphologySectionType::soma);
    case brain::neuron::SectionType::axon:
        return mask & static_cast<size_t>(MorphologySectionType::axon);
    case brain::neuron::SectionType::dendrite:
        return mask & static_cast<size_t>(MorphologySectionType::dendrite);
    case brain::neuron::SectionType::apicalDendrite:
        return mask &
               static_cast<size_t>(MorphologySectionType::apical_dendrite);
    case brain::neuron::SectionType::undefined:
    default:
        return true; // Unclassified sections will always be visible.
    }
}

inline float _getLastSampleRadius(const brain::neuron::Section& section)
{
    return section[-1][3] * 0.5;
}

/* Morphology tree processing */

uint32_ts computeTreeTraversalOrder(const brain::neuron::Morphology& morphology,
                                    const size_t sectionTypeMask)
{
    uint32_ts visited;
    uint32_ts stack;

    for (const auto& section : morphology.getSoma().getChildren())
        stack.push_back(section.getID());

    while (!stack.empty())
    {
        auto id = stack.back();
        stack.pop_back();

        // The section types are the same for all the sections comprised by
        // a neurite (i.e. a tree spanned from a first order section). This
        // means that e.g. an apical dendrite section cannot have normal
        // dendrite children. For that reason, if the current section type is
        // discarded, we can discard all its children as well.
        if (!_isInMask(morphology.getSection(id).getType(), sectionTypeMask))
            continue;

        visited.push_back(id);
        for (const auto& section : morphology.getSection(id).getChildren())
            stack.push_back(section.getID());
    }
    return visited;
}

size_t _addSDFGeometry(SDFData& sdfData, const SDFGeometry& geometry,
                       const std::set<size_t>& neighbours,
                       const size_t materialId, const uint32_t sectionId)
{
    const size_t idx = sdfData.geometries.size();
    sdfData.geometries.push_back(geometry);
    sdfData.neighbours.push_back(neighbours);
    sdfData.materials.push_back(materialId);
    sdfData.sectionGeometries[sectionId].push_back(idx);
    return idx;
}

void _addRegularSample(ModelData& model, const Vector3f& current,
                       const Vector3f& previous, const float radius,
                       const float previousRadius, const size_t materialId,
                       const uint64_t offset)
{
    model.addSphere(materialId, {current, radius, offset});
    if (current != previous && previousRadius > 0.f)
    {
        if (_almostEqual(radius, previousRadius))
            model.addCylinder(materialId, {current, previous, radius, offset});
        else
            model.addCone(materialId,
                          {current, previous, radius, previousRadius, offset});
    }
}

void _addSDFSample(SDFData& sdfData, const brain::neuron::Section section,
                   const bool isLast, const Vector3f& current,
                   const Vector3f& previous, const float radius,
                   const float previousRadius, const size_t materialId,
                   const uint64_t offset)
{
    auto sectionId = section.getID();

    {
        const auto geom = (_almostEqual(radius, previousRadius))
                              ? createSDFPill(current, previous, radius, offset)
                              : createSDFConePill(current, previous, radius,
                                                  previousRadius, offset);
        _addSDFGeometry(sdfData, geom, {}, materialId, sectionId);
    }

    const auto connectGeometriesToBifurcation =
        [&sdfData](const std::vector<size_t>& geometries,
                   size_t bifurcationId) {
            const auto& bifGeom = sdfData.geometries[bifurcationId];

            for (size_t geomIdx : geometries)
            {
                // Do not blend yourself
                if (geomIdx == bifurcationId)
                    continue;

                const auto& geom = sdfData.geometries[geomIdx];
                const float dist0 = geom.p0.squared_distance(bifGeom.center);
                const float dist1 = geom.p1.squared_distance(bifGeom.center);
                const float radiusSum = geom.radius + bifGeom.radius;
                const float radiusSumSq = radiusSum * radiusSum;

                if (dist0 < radiusSumSq || dist1 < radiusSumSq)
                {
                    sdfData.neighbours[bifurcationId].insert(geomIdx);
                    sdfData.neighbours[geomIdx].insert(bifurcationId);
                }
            }
        };

    if (isLast)
    {
        const auto& geometries = sdfData.sectionGeometries[sectionId];
        // Since our cone pills already give us a sphere at the end points we
        // don't need to add any sphere between segments except at the
        // bifurcation
        if (!section.getChildren().empty())
        {
            const size_t bifurcationId =
                _addSDFGeometry(sdfData,
                                createSDFSphere(current, radius, offset), {},
                                materialId, sectionId);
            sdfData.bifurcations[sectionId] = bifurcationId;

            // Connect to bifurcation at terminal point
            connectGeometriesToBifurcation(geometries, bifurcationId);
        }

        // And then at initial point
        if (section.hasParent())
        {
            connectGeometriesToBifurcation(
                geometries, sdfData.bifurcations[section.getParent().getID()]);
        }
    }
}

/**
 * Calculates all neighbours and adds the geometries to the model
 * container.
 */
void _finalizeSDFGeometries(ModelData& modelData, SDFData& sdfData)
{
    const size_t numGeoms = sdfData.geometries.size();

    // Extend neighbours to make sure smoothing is applied on all closely
    // connected geometries
    for (size_t rep = 0; rep < 4; rep++)
    {
        const size_t numNeighs = sdfData.neighbours.size();
        auto neighsCopy = sdfData.neighbours;
        for (size_t i = 0; i < numNeighs; i++)
        {
            for (size_t j : sdfData.neighbours[i])
            {
                for (size_t newNei : sdfData.neighbours[j])
                {
                    neighsCopy[i].insert(newNei);
                    neighsCopy[newNei].insert(i);
                }
            }
        }
        sdfData.neighbours = neighsCopy;
    }

    for (size_t i = 0; i < numGeoms; i++)
    {
        // Convert neighbours from set to vector and erase itself from  its
        // neighbours
        std::vector<size_t> neighbours;
        const auto& neighSet = sdfData.neighbours[i];
        std::copy(neighSet.begin(), neighSet.end(),
                  std::back_inserter(neighbours));
        neighbours.erase(std::remove_if(neighbours.begin(), neighbours.end(),
                                        [i](size_t elem) { return elem == i; }),
                         neighbours.end());

        modelData.addSDFGeometry(sdfData.materials[i], sdfData.geometries[i],
                                 neighbours);
    }
}
}

MorphologyLoaderParams::MorphologyLoaderParams(const PropertyMap& properties)
{
    const auto setVariable = [&](auto& variable, const std::string& name,
                                 const auto defaultVal) {
        using T = typename std::remove_reference<decltype(variable)>::type;
        variable = properties.getProperty<T>(name, defaultVal);
    };

    const auto setEnumVariable = [&](auto& variable, const std::string& name,
                                     auto defaultVal) {
        using T = decltype(defaultVal);
        const auto enumStr =
            properties.getProperty<std::string>(name,
                                                enumToString<T>(defaultVal));
        variable = stringToEnum<T>(enumStr);
    };

    setEnumVariable(colorScheme, PROP_COLOR_SCHEME.name, ColorScheme::none);
    setVariable(radiusMultiplier, PROP_RADIUS_MULTIPLIER.name, 1.0);
    setVariable(radiusCorrection, PROP_RADIUS_CORRECTION.name, 0.);

    {
        bool soma = true;
        bool axon = true;
        bool dendrite = true;
        bool apicalDendrite = true;
        setVariable(soma, PROP_SECTION_TYPES_SOMA.name, true);
        setVariable(axon, PROP_SECTION_TYPES_AXON.name, true);
        setVariable(dendrite, PROP_SECTION_TYPES_DENDRITE.name, true);
        setVariable(apicalDendrite, PROP_SECTION_TYPES_APICAL_DENDRITE.name,
                    true);

        if (soma)
            sectionTypes.push_back(MorphologySectionType::soma);
        if (axon)
            sectionTypes.push_back(MorphologySectionType::axon);
        if (dendrite)
            sectionTypes.push_back(MorphologySectionType::dendrite);
        if (apicalDendrite)
            sectionTypes.push_back(MorphologySectionType::apical_dendrite);
    }
    setVariable(dampenBranchThicknessChangerate,
                PROP_DAMPEN_BRANCH_THICKNESS_CHANGERATE.name, false);
    setVariable(useSDFGeometries, PROP_USE_SDF_GEOMETRIES.name, false);
    setEnumVariable(geometryQuality, PROP_GEOMETRY_QUALITY.name,
                    GeometryQuality::high);
}

class MorphologyLoader::Impl
{
public:
    Impl(const MorphologyLoaderParams& params)
        : _params(params)
    {
    }

    /**
     * @brief importMorphology imports a single morphology from a specified URI
     * @param uri URI of the morphology
     * @param index Index of the morphology
     * @param defaultMaterialId Material to use
     * @param transformation Transformation to apply to the morphology
     * @param reportMapping Mapping for applying simulation to the morphology
     * @return Position of the soma
     */
    Vector3f importMorphology(
        const servus::URI& source, Model& model, const uint64_t index,
        const Matrix4f& transformation,
        const size_t defaultMaterialId = NO_MATERIAL,
        const brain::CompartmentReportMapping* reportMapping = nullptr) const
    {
        Vector3f somaPosition;
        auto materialFunc =
            [ defaultMaterialId, colorScheme = _params.colorScheme,
              index ](auto sectionType)
        {
            if (defaultMaterialId != NO_MATERIAL)
                return defaultMaterialId;

            size_t materialId = 0;
            switch (colorScheme)
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
            default:
                materialId = 0;
            }
            return materialId;
        };

        ModelData modelContainer;
        somaPosition =
            importMorphology(source, index, materialFunc, transformation,
                             reportMapping, modelContainer);
        modelContainer.addTo(model);

        model.createMissingMaterials();
        return somaPosition;
    }

    Vector3f importMorphology(
        const servus::URI& source, const uint64_t index,
        MaterialFunc materialFunc, const Matrix4f& transformation,
        const brain::CompartmentReportMapping* reportMapping,
        ModelData& model) const
    {
        if (_params.sectionTypes ==
            std::vector<MorphologySectionType>{MorphologySectionType::soma})
        {
            const auto radius = static_cast<float>(_params.radiusMultiplier);
            const auto position = transformation.getTranslation();
            const auto materialId =
                materialFunc(brain::neuron::SectionType::soma);
            const uint64_t offset =
                reportMapping ? reportMapping->getOffsets()[index][0] : 0;
            model.addSphere(materialId, {position, radius, offset});
            return position;
        }
        return _importMorphologyFromURI(source, index, materialFunc,
                                        transformation, reportMapping, model);
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
        return (_params.radiusCorrection != 0.f
                    ? _params.radiusCorrection
                    : radius * _params.radiusMultiplier);
    }

    /**
     * Creates an SDF soma by adding and connecting the soma children using cone
     * pills
     */
    void _connectSDFSomaChildren(const Vector3f& somaPosition,
                                 const float somaRadius,
                                 const size_t materialId, const uint64_t offset,
                                 const brain::neuron::Sections& somaChildren,
                                 SDFData& sdfData) const
    {
        std::set<size_t> childIndices;
        for (const auto& child : somaChildren)
        {
            const auto& samples = child.getSamples();
            const Vector3f sample{samples[0].x(), samples[0].y(),
                                  samples[0].z()};

            // Create a sigmoid cone with half of soma radius to center of soma
            // to give it an organic look.
            const float radiusEnd = _getCorrectedRadius(samples[0].w() * 0.5f);
            const size_t geomIdx =
                _addSDFGeometry(sdfData,
                                createSDFConePillSigmoid(somaPosition, sample,
                                                         somaRadius * 0.5f,
                                                         radiusEnd, offset),
                                {}, materialId, -1);
            childIndices.insert(geomIdx);
        }

        for (size_t c : childIndices)
            sdfData.neighbours[c] = childIndices;
    }

    /**
     * Adds a Soma geometry to the model
     */
    void _addSomaGeometry(const brain::neuron::Soma& soma, uint64_t offset,
                          bool useSDFGeometries, MaterialFunc materialFunc,
                          ModelData& model, SDFData& sdfData) const
    {
        const size_t materialId =
            materialFunc(brain::neuron::SectionType::soma);
        const auto somaPosition = soma.getCentroid();
        const auto somaRadius = _getCorrectedRadius(soma.getMeanRadius());
        const auto& children = soma.getChildren();

        if (useSDFGeometries)
        {
            _connectSDFSomaChildren(somaPosition, somaRadius, materialId,
                                    offset, children, sdfData);
        }
        else
        {
            model.addSphere(materialId, {somaPosition, somaRadius, offset});

            if (_params.useSimulationModel)
            {
                // When using a simulation model, parametric geometries must
                // occupy as much space as possible in the mesh. This code
                // inserts a Cone between the soma and the beginning of each
                // branch.
                for (const auto& child : children)
                {
                    const auto& samples = child.getSamples();
                    const Vector3f sample{samples[0].x(), samples[0].y(),
                                          samples[0].z()};
                    const float sampleRadius =
                        _getCorrectedRadius(samples[0].w() * 0.5f);

                    model.addCone(materialId, {somaPosition, sample, somaRadius,
                                               sampleRadius, offset});
                }
            }
        }
    }

    /**
     * @brief _importMorphologyFromURI imports a morphology from the specified
     * URI
     * @param uri URI of the morphology
     * @param index Index of the current morphology
     * @param materialFunc A function mapping brain::neuron::SectionType to a
     *   material id
     * @param transformation Transformation to apply to the morphology
     * @param reportMapping Mapping for applying simulation to the morphology
     * @param model Model container to which the morphology should be loaded
     *   into
     * @return Position of the soma
     */
    Vector3f _importMorphologyFromURI(
        const servus::URI& uri, const uint64_t index, MaterialFunc materialFunc,
        const Matrix4f& transformation,
        const brain::CompartmentReportMapping* reportMapping,
        ModelData& model) const
    {
        Vector3f somaPosition;

        const bool dampenThickness = _params.dampenBranchThicknessChangerate;
        const bool useSDFGeometries = _params.useSDFGeometries;

        SDFData sdfData;

        brain::neuron::Morphology morphology(uri, transformation);

        const size_t sectionMask = enumsToBitmask(_params.sectionTypes);

        // Soma
        somaPosition = morphology.getSoma().getCentroid();
        if (sectionMask & static_cast<size_t>(MorphologySectionType::soma))
        {
            const uint64_t offset =
                reportMapping ? reportMapping->getOffsets()[index][0] : 0;
            _addSomaGeometry(morphology.getSoma(), offset, useSDFGeometries,
                             materialFunc, model, sdfData);
        }

        // Only the first one or two axon sections are reported, so find the
        // last one and use its offset for all the other axon sections
        uint16_t lastAxon = 0;
        if (reportMapping &&
            (sectionMask & static_cast<size_t>(MorphologySectionType::axon)))
        {
            const auto& counts = reportMapping->getCompartmentCounts()[index];
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
        for (const auto sectionId :
             computeTreeTraversalOrder(morphology, sectionMask))
        {
            const auto& section = morphology.getSection(sectionId);
            const auto& samples = section.getSamples();
            if (samples.empty())
                continue;

            const auto materialId = materialFunc(section.getType());

            const size_t numSamples = samples.size();

            auto previousSample = samples[0];
            size_t step = 1;
            switch (_params.geometryQuality)
            {
            case GeometryQuality::low:
                step = numSamples - 1;
                break;
            case GeometryQuality::medium:
                step = numSamples / 2;
                step = (step == 0) ? 1 : step;
                break;
            default:
                step = 1;
            }

            float segmentStep = 0.f;
            if (reportMapping)
            {
                const auto& counts =
                    reportMapping->getCompartmentCounts()[index];
                // Number of compartments usually differs from number of samples
                segmentStep = counts[sectionId] / float(numSamples);
            }

            float previousRadius = _getCorrectedRadius(
                section.hasParent() ? _getLastSampleRadius(section.getParent())
                                    : samples[0].w() * 0.5);

            bool done = false;
            for (size_t i = step; !done && i < numSamples + step; i += step)
            {
                if (i >= (numSamples - 1))
                {
                    i = numSamples - 1;
                    done = true;
                }

                uint64_t offset = 0;
                if (reportMapping)
                {
                    const auto& offsets = reportMapping->getOffsets()[index];
                    const auto& counts =
                        reportMapping->getCompartmentCounts()[index];

                    // update the offset if we have enough compartments aka a
                    // full compartment report. Otherwise we keep the soma
                    // offset which happens for soma reports and use this for
                    // all the sections
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
                                // This should never happen, but just in case
                                // use an invalid value to show an error color
                                offset = std::numeric_limits<uint64_t>::max();
                        }
                    }
                }

                const auto sample = samples[i];

                const Vector3f position(sample.x(), sample.y(), sample.z());
                const Vector3f target(previousSample.x(), previousSample.y(),
                                      previousSample.z());
                float radius = _getCorrectedRadius(samples[i].w() * 0.5f);
                constexpr float maxRadiusChange = 0.1f;

                const float dist = (target - position).length();
                // The radius of the last sample of a section is never
                // modified
                if (dist > 0.0001f && i != samples.size() - 1 &&
                    dampenThickness)
                {
                    const float radiusChange =
                        std::min(std::abs(previousRadius - radius),
                                 dist * maxRadiusChange);
                    if (radius < previousRadius)
                        radius = previousRadius - radiusChange;
                    else
                        radius = previousRadius + radiusChange;
                }

                if (radius > 0.f)
                {
                    if (useSDFGeometries)
                    {
                        _addSDFSample(sdfData, section, done, position, target,
                                      radius, previousRadius, materialId,
                                      offset);
                    }
                    else
                    {
                        _addRegularSample(model, position, target, radius,
                                          previousRadius, materialId, offset);
                    }
                }
                previousSample = sample;
                previousRadius = radius;
            }
        }

        if (useSDFGeometries)
            _finalizeSDFGeometries(model, sdfData);

        return somaPosition;
    }

private:
    const MorphologyLoaderParams _params;
};

MorphologyLoader::MorphologyLoader(Scene& scene)
    : Loader(scene)
{
}

MorphologyLoader::~MorphologyLoader()
{
}
bool MorphologyLoader::isSupported(const std::string& filename BRAYNS_UNUSED,
                                   const std::string& extension) const
{
    const std::set<std::string> types = {"h5", "swc"};
    return types.find(extension) != types.end();
}

ModelDescriptorPtr MorphologyLoader::importFromBlob(
    Blob&& /*blob*/, const LoaderProgress& /*callback*/,
    const PropertyMap& properties BRAYNS_UNUSED, const size_t /*index*/,
    const size_t /*materialID*/) const
{
    throw std::runtime_error("Load morphology from memory not supported");
}

ModelDescriptorPtr MorphologyLoader::importFromFile(
    const std::string& fileName, const LoaderProgress& callback,
    const PropertyMap& propertiesTmp, const size_t index,
    const size_t defaultMaterialId BRAYNS_UNUSED) const
{
    // Fill property map since the actual property types are known now.
    PropertyMap properties = getProperties();
    properties.merge(propertiesTmp);

    const auto modelName = boost::filesystem::basename({fileName});
    callback.updateProgress("Loading " + modelName + " ...", 0.f);
    auto model = _scene.createModel();
    const auto params = MorphologyLoaderParams(properties);
    Vector3f somaPosition =
        importMorphology(servus::URI(fileName), *model, index, {}, params);
    model->createMissingMaterials();
    callback.updateProgress("Loading " + modelName + " ...", 1.f);

    Transformation transformation;
    transformation.setRotationCenter(somaPosition);
    auto modelDescriptor =
        std::make_shared<ModelDescriptor>(std::move(model), fileName);
    modelDescriptor->setTransformation(transformation);
    return modelDescriptor;
}

Vector3f MorphologyLoader::importMorphology(
    const servus::URI& uri, Model& model, const size_t index,
    const Matrix4f& transformation, const MorphologyLoaderParams& params) const
{
    auto impl = MorphologyLoader::Impl(params);
    return impl.importMorphology(uri, model, index, transformation);
}

Vector3f MorphologyLoader::_importMorphology(
    const servus::URI& source, const uint64_t index, MaterialFunc materialFunc,
    const Matrix4f& transformation,
    const brain::CompartmentReportMapping* reportMapping, ModelData& model,
    const MorphologyLoaderParams& params) const
{
    auto impl = MorphologyLoader::Impl(params);
    return impl.importMorphology(source, index, materialFunc, transformation,
                                 reportMapping, model);
}

std::string MorphologyLoader::getName() const
{
    return LOADER_NAME;
}

std::vector<std::string> MorphologyLoader::getSupportedExtensions() const
{
    return {"h5", "swc"};
}

PropertyMap MorphologyLoader::getProperties() const
{
    PropertyMap pm;
    pm.setProperty(PROP_COLOR_SCHEME);
    pm.setProperty(PROP_RADIUS_MULTIPLIER);
    pm.setProperty(PROP_RADIUS_CORRECTION);
    pm.setProperty(PROP_SECTION_TYPES_SOMA);
    pm.setProperty(PROP_SECTION_TYPES_AXON);
    pm.setProperty(PROP_SECTION_TYPES_DENDRITE);
    pm.setProperty(PROP_SECTION_TYPES_APICAL_DENDRITE);
    pm.setProperty(PROP_DAMPEN_BRANCH_THICKNESS_CHANGERATE);
    pm.setProperty(PROP_USE_SDF_GEOMETRIES);
    pm.setProperty(PROP_GEOMETRY_QUALITY);
    return pm;
}
}