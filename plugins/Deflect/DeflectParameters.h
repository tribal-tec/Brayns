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
constexpr auto PARAM_CHROMA_SUBSAMPLING = "chromaSubsampling";
constexpr auto PARAM_COMPRESSION = "compression";
constexpr auto PARAM_ENABLED = "enabled";
constexpr auto PARAM_HOSTNAME = "hostname";
constexpr auto PARAM_ID = "id";
constexpr auto PARAM_PORT = "port";
constexpr auto PARAM_QUALITY = "quality";
constexpr auto PARAM_RESIZING = "resizing";
constexpr auto PARAM_TOP_DOWN = "topDown";
constexpr auto PARAM_USE_PIXEL_OP = "usePixelop";

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
    bool getEnabled() const { return _props.getProperty<bool>(PARAM_ENABLED); }
    void setEnabled(const bool enabled)
    {
        _props.updateProperty(PARAM_ENABLED, enabled);
    }
    /** Stream compression enabled */
    bool getCompression() const
    {
        return _props.getProperty<bool>(PARAM_COMPRESSION);
    }
    void setCompression(const bool enabled)
    {
        _props.updateProperty(PARAM_COMPRESSION, enabled);
    }

    /** Stream compression quality, 1 (worst) to 100 (best) */
    unsigned getQuality() const
    {
        return (unsigned)_props.getProperty<int32_t>(PARAM_QUALITY);
    }
    void setQuality(const unsigned quality)
    {
        _props.updateProperty(PARAM_QUALITY, (int32_t)quality);
    }

    /** Stream ID; defaults to DEFLECT_ID if empty */
    std::string getId() const
    {
        return _props.getProperty<std::string>(PARAM_ID);
    }
    void setId(const std::string& id) { _props.updateProperty(PARAM_ID, id); }
    /** Stream hostname; defaults to DEFLECT_HOST if empty */
    std::string getHostname() const
    {
        return _props.getProperty<std::string>(PARAM_HOSTNAME);
    }
    void setHost(const std::string& host)
    {
        _props.updateProperty(PARAM_HOSTNAME, host);
    }

    /** Stream port; defaults to 1701 if empty */
    unsigned getPort() const
    {
        return (unsigned)_props.getProperty<int32_t>(PARAM_PORT);
    }
    void setPort(const unsigned port)
    {
        _props.updateProperty(PARAM_PORT, (int32_t)port);
    }

    /** Stream resizing enabled */
    bool isResizingEnabled() const
    {
        return _props.getProperty<bool>(PARAM_RESIZING);
    }

    bool isTopDown() const { return _props.getProperty<bool>(PARAM_TOP_DOWN); }
    void setIsTopDown(const bool topDown)
    {
        _props.updateProperty(PARAM_TOP_DOWN, topDown);
    }

    bool usePixelOp() const
    {
        return _props.getProperty<bool>(PARAM_USE_PIXEL_OP);
    }
    deflect::ChromaSubsampling getChromaSubsampling() const
    {
        return (deflect::ChromaSubsampling)_props.getProperty<int32_t>(
            PARAM_CHROMA_SUBSAMPLING);
    }
    void setChromaSubsampling(const deflect::ChromaSubsampling subsampling)
    {
        _props.updateProperty(PARAM_CHROMA_SUBSAMPLING, (int32_t)subsampling);
    }

    const PropertyMap& getPropertyMap() const { return _props; }
    PropertyMap& getPropertyMap() { return _props; }
private:
    PropertyMap _props;
};
}
