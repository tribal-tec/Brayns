/* Copyright (c) 2015-2018, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
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

#include <brayns/common/PropertyMap.h>

#include <deflect/types.h>

namespace brayns
{
class DeflectParameters
{
public:
    static PropertyMap createPropertyMap();

    DeflectParameters()
        : _props(createPropertyMap())
    {
    }

    DeflectParameters(PropertyMap&& props)
        : _props(std::move(props))
    {
    }

    /** Streaming enabled */
    bool getEnabled() const { return _enabled; }
    void setEnabled(const bool enabled) { _enabled = enabled; }
    /** Stream compression enabled */
    bool getCompression() const
    {
        return _props.getProperty<bool>("compression");
    }
    void setCompression(const bool enabled)
    {
        _props.updateProperty("compression", enabled);
    }

    /** Stream compression quality, 1 (worst) to 100 (best) */
    unsigned getQuality() const
    {
        return (unsigned)_props.getProperty<int32_t>("quality");
    }
    void setQuality(const unsigned quality)
    {
        _props.updateProperty("quality", (int32_t)quality);
    }

    /** Stream ID; defaults to DEFLECT_ID if empty */
    std::string getId() const { return _props.getProperty<std::string>("id"); }
    void setId(const std::string& id) { _props.updateProperty("id", id); }
    /** Stream hostname; defaults to DEFLECT_HOST if empty */
    std::string getHostname() const
    {
        return _props.getProperty<std::string>("hostname");
    }
    void setHost(const std::string& host)
    {
        _props.updateProperty("hostname", host);
    }

    /** Stream port; defaults to 1701 if empty */
    unsigned getPort() const
    {
        return (unsigned)_props.getProperty<int32_t>("port");
    }
    void setPort(const unsigned port)
    {
        _props.updateProperty("port", (int32_t)port);
    }

    /** Stream resizing disabled */
    bool isResizingDisabled() const
    {
        return _props.getProperty<bool>("disable-resizing");
    }

    bool isTopDown() const { return _props.getProperty<bool>("top-down"); }
    void setIsTopDown(const bool topDown)
    {
        _props.updateProperty("top-down", topDown);
    }

    bool usePixelOp() const { return _props.getProperty<bool>("use-pixelop"); }
    deflect::ChromaSubsampling getChromaSubsampling() const
    {
        return (deflect::ChromaSubsampling)_props.getProperty<int32_t>(
            "chroma-subsampling");
    }
    void setChromaSubsampling(const deflect::ChromaSubsampling subsampling)
    {
        _props.updateProperty("chroma-subsampling", (int32_t)subsampling);
    }

    const PropertyMap& getPropertyMap() const { return _props; }
private:
    bool _enabled{true};
    PropertyMap _props;
};
}
