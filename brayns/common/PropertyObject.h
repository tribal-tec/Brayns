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
class PropertyObject : public BaseObject
{
public:
    /**
     * Set custom/plugin-specific properties to this camera. They are
     * automatically applied in commit() on the implementation-specific object.
     */
    void setProperties(const PropertyMap& properties)
    {
        _properties = properties;
        markModified();
    }

    void setProperties(const std::string& type, const PropertyMap& properties)
    {
        _mappedProperties[type] = properties;
        markModified();
    }

    /** Update or add the given properties to the existing ones. */
    void updateProperties(const PropertyMap& properties)
    {
        for (auto prop : properties.getProperties())
            _properties.setProperty(*prop);
        markModified();
    }

    void updateProperties(const std::string& type,
                          const PropertyMap& properties)
    {
        for (auto prop : properties.getProperties())
            _mappedProperties.at(type).setProperty(*prop);
        markModified();
    }

    const auto& getProperties() const { return _properties.getProperties(); }
    const auto& getPropertyMap() const { return _properties; }
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
    PropertyMap _properties;
    std::map<std::string, PropertyMap> _mappedProperties;
};
}
