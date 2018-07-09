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

#include <boost/any.hpp>
#include <memory>
#include <string>
#include <vector>

namespace brayns
{
/**
 * Container class for holding properties that are mapped by name to a supported
 * C++ type and their respective value.
 */
class PropertyMap
{
public:
    PropertyMap() = default;

    struct Property
    {
        enum class Type
        {
            Int,
            Float,
            String,
            Bool,
            Vec2i,
            Vec2f,
            Vec3i,
            Vec3f,
            Vec4f
        };

        template <typename T>
        Property(const std::string &name_, const std::string &title_,
                 const T &value)
            : name(name_)
            , title(title_)
            , type(getType<T>())
            , _data(value)
            , _min(std::numeric_limits<T>::min())
            , _max(std::numeric_limits<T>::max())
        {
        }

        template <typename T>
        Property(const std::string &name_, const std::string &title_,
                 const T &value, const T &min_, const T &max_)
            : name(name_)
            , title(title_)
            , type(getType<T>())
            , _data(value)
            , _min(min_)
            , _max(max_)
        {
        }

        template <typename T>
        void set(const T &v)
        {
            _data = v;
        }

        template <typename T>
        T get() const
        {
            return boost::any_cast<T>(_data);
        }

        template <typename T>
        T min() const
        {
            return boost::any_cast<T>(_min);
        }

        template <typename T>
        T max() const
        {
            return boost::any_cast<T>(_max);
        }

        void setData(const boost::any &data) { _data = data; }
        const boost::any &getData() const { return _data; }
        const std::string name;
        const std::string title;
        const Type type;

        template <typename T>
        Type getType();

    private:
        boost::any _data;
        const boost::any _min;
        const boost::any _max;
    };

    /** Update the property of the given name */
    template <typename T>
    inline void updateProperty(const std::string &name, const T &t)
    {
        auto property = findProperty(name);
        if (property)
            property->set(t);
    }

    /** Update or add the given property. */
    void setProperty(const Property &newProperty)
    {
        auto property = findProperty(newProperty.name);
        if (!property)
            _properties.push_back(std::make_shared<Property>(newProperty));
        else
            property->setData(newProperty.getData());
    }

    /**
     * @return the property value of the given name or valIfNotFound otherwise.
     */
    template <typename T>
    inline T getProperty(const std::string &name, T valIfNotFound) const
    {
        auto property = findProperty(name);
        if (property)
            return property->get<T>();
        return valIfNotFound;
    }

    /**
     * @return the property value of the given name
     * @throw std::runtime_error if value property value was not found.
     */
    template <typename T>
    inline T getProperty(const std::string &name) const
    {
        auto property = findProperty(name);
        if (property)
            return property->get<T>();
        throw std::runtime_error("No property found with name " + name);
    }

    bool hasProperty(const std::string &name) const
    {
        return findProperty(name) != nullptr;
    }

    Property::Type getPropertyType(const std::string &name)
    {
        auto property = findProperty(name);
        if (property)
            return property->type;
        throw std::runtime_error("No property found with name " + name);
    }

    /** @return all the registered properties. */
    const auto &getProperties() const { return _properties; }
private:
    Property *findProperty(const std::string &name) const
    {
        auto foundProperty =
            std::find_if(_properties.begin(), _properties.end(),
                         [&](const auto &p) { return p->name == name; });

        return foundProperty != _properties.end() ? foundProperty->get()
                                                  : nullptr;
    }

    std::vector<std::shared_ptr<Property>> _properties;
};

template <>
inline PropertyMap::Property::Type PropertyMap::Property::getType<float>()
{
    return PropertyMap::Property::Type::Float;
}
template <>
inline PropertyMap::Property::Type PropertyMap::Property::getType<int32_t>()
{
    return PropertyMap::Property::Type::Int;
}
template <>
inline PropertyMap::Property::Type PropertyMap::Property::getType<std::string>()
{
    return PropertyMap::Property::Type::String;
}
template <>
inline PropertyMap::Property::Type PropertyMap::Property::getType<bool>()
{
    return PropertyMap::Property::Type::Bool;
}
template <>
inline PropertyMap::Property::Type
    PropertyMap::Property::getType<std::array<float, 2>>()
{
    return PropertyMap::Property::Type::Vec2f;
}
template <>
inline PropertyMap::Property::Type
    PropertyMap::Property::getType<std::array<int32_t, 2>>()
{
    return PropertyMap::Property::Type::Vec2i;
}
template <>
inline PropertyMap::Property::Type
    PropertyMap::Property::getType<std::array<float, 3>>()
{
    return PropertyMap::Property::Type::Vec3f;
}
template <>
inline PropertyMap::Property::Type
    PropertyMap::Property::getType<std::array<int32_t, 3>>()
{
    return PropertyMap::Property::Type::Vec3i;
}
template <>
inline PropertyMap::Property::Type
    PropertyMap::Property::getType<std::array<float, 4>>()
{
    return PropertyMap::Property::Type::Vec4f;
}
}
