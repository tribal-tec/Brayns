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

#include <brayns/parameters/AbstractParameters.h>

#include <deflect/types.h>

// SERIALIZATION_ACCESS(StreamParameters)

namespace brayns
{
class DeflectParameters : public AbstractParameters
{
public:
    DeflectParameters();

    /** @copydoc AbstractParameters::print */
    void print() final;

    /** Streaming enabled */
    bool getEnabled() const { return _enabled; }
    void setEnabled(const bool enabled) { _updateValue(_enabled, enabled); }
    /** Stream compression enabled */
    bool getCompression() const { return _compression; }
    void setCompression(const bool enabled)
    {
        _updateValue(_compression, enabled);
    }

    /** Stream compression quality, 1 (worst) to 100 (best) */
    unsigned getQuality() const { return _quality; }
    void setQuality(const unsigned quality) { _updateValue(_quality, quality); }
    /** Stream ID; defaults to DEFLECT_ID if empty */
    const std::string& getId() const { return _id; }
    void setId(const std::string& id) { _updateValue(_id, id); }
    /** Stream hostname; defaults to DEFLECT_HOST if empty */
    const std::string& getHostname() const { return _host; }
    void setHost(const std::string& host) { _updateValue(_host, host); }
    /** Stream port; defaults to 1701 if empty */
    unsigned getPort() const { return _port; }
    void setPort(const unsigned port) { _updateValue(_port, port); }
    /** Stream resizing enabled */
    bool getResizing() const { return _resizing; }
    void setResizing(const bool enabled) { _updateValue(_resizing, enabled); }
    bool isTopDown() const { return _topDown; }
    void setIsTopDown(const bool topDown) { _updateValue(_topDown, topDown); }
    bool usePixelOp() const { return _usePixelOp; }
    deflect::ChromaSubsampling getChromaSubsampling() const
    {
        return _chromaSubsampling;
    }
    void setChromaSubsampling(const deflect::ChromaSubsampling subsampling)
    {
        _updateValue(_chromaSubsampling, subsampling);
    }
    void parse(const po::variables_map& vm);

private:
    std::string _host;
    bool _enabled{true};
    std::string _id;
    unsigned _port{1701};
    bool _compression{true};
    unsigned _quality{80};
    bool _resizing{true};
    bool _topDown{false};
    bool _usePixelOp{false};
    deflect::ChromaSubsampling _chromaSubsampling{
        deflect::ChromaSubsampling::YUV444};

    // SERIALIZATION_FRIEND(StreamParameters)
};
}
