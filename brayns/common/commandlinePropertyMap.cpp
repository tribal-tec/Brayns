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
#include "log.h"

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

// struct enumFake
//{
//    std::string value;
//    std::set<std::string> values;

//    friend std::istream &operator>>(std::istream &is, enumFake &arg) {
//        is >> arg.value;
//        if(arg.values.count(arg.value) == 0)
//        {
//            std::ostringstream ss;
//                        ss << "value must be in the list, you supplied " <<
//                        std::quoted(arg.value);
//            throw std::invalid_argument(ss.str());
//        }
//        return is;
//    }

//    friend std::ostream &operator<<(std::ostream &os, enumFake const &arg) {
//        return os << arg.value;
//    }
//};

namespace brayns
{
po::options_description toCommandlineDescription(const PropertyMap& propertyMap)
{
    po::options_description desc;
    for (const auto& property : propertyMap.getProperties())
    {
        po::value_semantic* valueSemantic{nullptr};
        switch (property->type)
        {
        case Property::Type::Int:
        {
            if (property->enums.empty())
                valueSemantic = po::value<int32_t>()->default_value(
                    property->get<int32_t>());
            break;
        }
        case Property::Type::Double:
            valueSemantic =
                po::value<double>()->default_value(property->get<double>());
            break;
        case Property::Type::String:
            valueSemantic = po::value<std::string>()->default_value(
                property->get<std::string>());
            break;
        case Property::Type::Bool:
            valueSemantic = po::bool_switch();
            break;
        case Property::Type::Vec2i:
            valueSemantic = po::value<boost::array<int32_t, 2>>();
            break;
        case Property::Type::Vec2d:
            valueSemantic = po::value<boost::array<double, 2>>();
            break;
        case Property::Type::Vec3i:
            valueSemantic = po::value<boost::array<int32_t, 3>>();
            break;
        case Property::Type::Vec3d:
            valueSemantic = po::value<boost::array<double, 3>>();
            break;
        case Property::Type::Vec4d:
            valueSemantic = po::value<boost::array<double, 4>>();
            break;
        default:
            continue;
        }
        if (valueSemantic)
            desc.add(boost::make_shared<po::option_description>(
                property->name.c_str(), valueSemantic,
                property->userInfo.description.c_str()));
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
        case Property::Type::Int:
            property->set(vm[property->name].as<int32_t>());
            break;
        case Property::Type::Double:
            property->set(vm[property->name].as<double>());
            break;
        case Property::Type::String:
            property->set(vm[property->name].as<std::string>());
            break;
        case Property::Type::Bool:
            property->set(vm[property->name].as<bool>());
            break;
        case Property::Type::Vec2i:
            property->set(vm[property->name].as<boost::array<int32_t, 2>>());
            break;
        case Property::Type::Vec2d:
            break;
        case Property::Type::Vec3i:
            break;
        case Property::Type::Vec3d:
            break;
        case Property::Type::Vec4d:
            break;
        default:
            continue;
        }
    }
}

bool parseIntoPropertyMap(int argc, const char** argv, PropertyMap& propertyMap)
{
    try
    {
        po::variables_map vm;
        po::options_description desc;
        desc.add(boost::make_shared<po::option_description>("help",
                                                            po::bool_switch(),
                                                            "Print this help"));

        desc.add(toCommandlineDescription(propertyMap));
        po::parsed_options parsedOptions =
            po::command_line_parser(argc, argv).options(desc).run();
        po::store(parsedOptions, vm);
        po::notify(vm);

        if (vm.count("help"))
        {
            std::cout << desc << std::endl;
            return false;
        }

        commandlineToPropertyMap(vm, propertyMap);
        return true;
    }
    catch (const po::error& e)
    {
        BRAYNS_ERROR << "Failed to load parse commandline for property map: "
                     << e.what() << std::endl;
        return false;
    }
}
}
