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
#include <brayns/common/PropertyMap.h>
#include <brayns/common/types.h>

#include <map>

namespace brayns
{
class MappedPropertyObject : public BaseObject
{
public:
    void setCurrentType(const std::string& type) { _updateValue(_type, type); }
    const std::string& getCurrentType() const { return _type; }
    void setProperties(const std::string& type, const PropertyMap& properties)
    {
        _mappedProperties[type] = properties;
        markModified();
    }

    void updateProperties(const std::string& type,
                          const PropertyMap& properties)
    {
        for (auto prop : properties.getProperties())
            _mappedProperties.at(type).setProperty(*prop);
        markModified();
    }

    bool hasProperties(const std::string& type) const
    {
        return _mappedProperties.count(type) != 0;
    }
    const auto& getProperties(const std::string& type) const
    {
        return _mappedProperties.at(type).getProperties();
    }
    const auto& getPropertyMap(const std::string& type) const
    {
        return _mappedProperties.at(type);
    }
    strings getTypes() const
    {
        strings types;
        for (const auto& i : _mappedProperties)
            types.push_back(i.first);
        return types;
    }

private:
    std::string _type;
    std::map<std::string, PropertyMap> _mappedProperties;
};
}
