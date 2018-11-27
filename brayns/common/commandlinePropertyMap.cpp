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

#include "commandlinePropertyMap.h"

#include "PropertyMap.h"

#include <boost/make_shared.hpp>

namespace boost
{
template <typename T, std::size_t N>
std::istream& operator>>(std::istream& in, array<T, N>& value)
{
    std::string token;
    in >> token;
    try
    {
        for (std::size_t i = 0; i < N; ++i)
            value[i] = lexical_cast<T>(token);
    }
    catch (const bad_lexical_cast&)
    {
        in.setstate(std::ios_base::failbit);
    }

    return in;
}
}
namespace brayns
{
po::options_description toCommandlineDescription(const PropertyMap& propertyMap)
{
    po::options_description desc;
    for (const auto& property : propertyMap.getProperties())
    {
        po::value_semantic* valueSemantic;
        switch (property->type)
        {
        case PropertyMap::Property::Type::Int:
            valueSemantic = po::value<int32_t>();
        //            if(!property->enums.empty())
        //            break;
        case PropertyMap::Property::Type::Double:
            valueSemantic = po::value<double>();
            break;
        case PropertyMap::Property::Type::String:
            valueSemantic = po::value<std::string>();
            break;
        case PropertyMap::Property::Type::Bool:
            valueSemantic =
                po::bool_switch()->default_value(property->get<bool>());
            break;
        case PropertyMap::Property::Type::Vec2i:
            valueSemantic = po::value<boost::array<int32_t, 2>>();
            break;
        case PropertyMap::Property::Type::Vec2d:
            valueSemantic = po::value<boost::array<double, 2>>();
            break;
        case PropertyMap::Property::Type::Vec3i:
            valueSemantic = po::value<boost::array<int32_t, 3>>();
            break;
        case PropertyMap::Property::Type::Vec3d:
            valueSemantic = po::value<boost::array<double, 3>>();
            break;
        case PropertyMap::Property::Type::Vec4d:
            valueSemantic = po::value<boost::array<double, 4>>();
            break;
        default:
            continue;
        }
        desc.add(
            boost::make_shared<po::option_description>(property->name.c_str(),
                                                       valueSemantic));
    }
    return desc;
}

void commandlineToPropertyMap(const boost::program_options::variables_map& vm,
                              PropertyMap& propertyMap)
{
    for (const auto& property : propertyMap.getProperties())
    {
        if (!vm.count(property->name))
            continue;
        switch (property->type)
        {
        case PropertyMap::Property::Type::Int:
            property->set(vm[property->name].as<int32_t>());
            break;
        case PropertyMap::Property::Type::Double:
            property->set(vm[property->name].as<double>());
            break;
        case PropertyMap::Property::Type::String:
            property->set(vm[property->name].as<std::string>());
            break;
        case PropertyMap::Property::Type::Bool:
            property->set(vm[property->name].as<bool>());
            break;
        case PropertyMap::Property::Type::Vec2i:
            property->set(vm[property->name].as<boost::array<int32_t, 2>>());
            break;
        case PropertyMap::Property::Type::Vec2d:
            break;
        case PropertyMap::Property::Type::Vec3i:
            break;
        case PropertyMap::Property::Type::Vec3d:
            break;
        case PropertyMap::Property::Type::Vec4d:
            break;
        default:
            continue;
        }
    }
}
}
