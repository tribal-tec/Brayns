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

#include "DeflectParameters.h"

#include "utils.h"
#include <deflect/Stream.h>

//#include <brayns/common/utils/EnumUtils.h>

// namespace
//{
// const std::string PARAM_CHROMA_SUBSAMPLING = "chroma-subsampling";
// const std::string PARAM_COMPRESSION = "disable-compression";
// const std::string PARAM_HOST = "host";
// const std::string PARAM_ID = "id";
// const std::string PARAM_PORT = "port";
// const std::string PARAM_QUALITY = "quality";
// const std::string PARAM_RESIZING = "disable-resizing";
// const std::string PARAM_TOP_DOWN = "top-down";
// const std::string PARAM_USE_PIXELOP = "use-pixelop";
//}

namespace deflect
{
// std::istream& operator>>(std::istream& in, ChromaSubsampling& ss)
//{
//    std::string token;
//    in >> token;
//    try
//    {
//        ss = brayns::stringToEnum<ChromaSubsampling>(token);
//    }
//    catch (const std::runtime_error&)
//    {
//        in.setstate(std::ios_base::failbit);
//    }

//    return in;
//}
}
namespace brayns
{
PropertyMap DeflectParameters::createPropertyMap()
{
    PropertyMap properties;
    properties.setProperty({"id", "Stream ID", std::string()});
    properties.setProperty({"hostname", "Stream hostname", std::string()});
    properties.setProperty({"port",
                            "Stream port",
                            (int32_t)deflect::Stream::defaultPortNumber,
                            {1023, 65535}});
    properties.setProperty({"enabled", "Enable streaming", true});
    properties.setProperty({"compression", "Use compression", true});
    properties.setProperty({"top-down", "Stream image top-down", false});
    properties.setProperty({"resizing", "Resize on Deflect events", true});
    properties.setProperty({"quality", "JPEG quality", (int32_t)80, {1, 100}});
    properties.setProperty(
        {"use-pixelop", "Use optimized, distributed streaming", false});
    properties.setProperty({"chroma-subsampling", "Chroma subsampling",
                            int32_t(deflect::ChromaSubsampling::YUV444),
                            enumNames<deflect::ChromaSubsampling>()});
    return properties;
}
// DeflectParameters::DeflectParameters()
//    : AbstractParameters("Deflect")
//{
//    _parameters.add_options()(PARAM_COMPRESSION.c_str(),
//                              po::bool_switch()->default_value(!_compression),
//                              "Disable JPEG compression")(
//        PARAM_HOST.c_str(), po::value<std::string>(),
//        "Hostname of Deflect server")(PARAM_ID.c_str(),
//                                      po::value<std::string>(),
//                                      "Name of stream")(
//        PARAM_PORT.c_str(), po::value<unsigned>(), "Port of Deflect server")(
//        PARAM_QUALITY.c_str(), po::value<unsigned>(),
//        "JPEG quality of stream")(PARAM_RESIZING.c_str(),
//                                  po::bool_switch()->default_value(!_resizing),
//                                  "Disable stream resizing")(
//        PARAM_USE_PIXELOP.c_str(),
//        po::bool_switch()->default_value(_usePixelOp),
//        "Use optimized pixel op")(PARAM_CHROMA_SUBSAMPLING.c_str(),
//                                  po::value<deflect::ChromaSubsampling>(),
//                                  "Chroma subsampling")(
//        PARAM_TOP_DOWN.c_str(), po::bool_switch()->default_value(_topDown),
//        "TTop-down image orientation instead of bottom-up");
//}

// void DeflectParameters::parse(const po::variables_map& vm)
//{
//    _compression = !vm[PARAM_COMPRESSION].as<bool>();
//    if (vm.count(PARAM_HOST))
//        _host = vm[PARAM_HOST].as<std::string>();
//    if (vm.count(PARAM_ID))
//        _id = vm[PARAM_ID].as<std::string>();
//    if (vm.count(PARAM_PORT))
//        _port = vm[PARAM_PORT].as<unsigned>();
//    if (vm.count(PARAM_QUALITY))
//        _quality = vm[PARAM_QUALITY].as<unsigned>();
//    if (vm.count(PARAM_CHROMA_SUBSAMPLING))
//        _chromaSubsampling =
//            vm[PARAM_CHROMA_SUBSAMPLING].as<deflect::ChromaSubsampling>();
//    _resizing = !vm[PARAM_RESIZING].as<bool>();
//    _usePixelOp = vm[PARAM_USE_PIXELOP].as<bool>();
//    _topDown = vm[PARAM_TOP_DOWN].as<bool>();
//    markModified();
//}

// void DeflectParameters::print()
//{
//    AbstractParameters::print();
//    BRAYNS_INFO << "Stream compression                : "
//                << asString(_compression) << std::endl;
//    BRAYNS_INFO << "Stream host                       : " << _host <<
//    std::endl;
//    BRAYNS_INFO << "Stream ID                         : " << _id << std::endl;
//    BRAYNS_INFO << "Stream port                       : " << _port <<
//    std::endl;
//    BRAYNS_INFO << "Stream quality                    : " << _quality
//                << std::endl;
//    BRAYNS_INFO << "Stream resizing                   : " <<
//    asString(_resizing)
//                << std::endl;
//}
}
