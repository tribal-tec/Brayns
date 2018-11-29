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

#include <deflect/Stream.h>

namespace brayns
{
PropertyMap DeflectParameters::createPropertyMap()
{
    PropertyMap properties;
    properties.setProperty(
        {PARAM_ENABLED, true,
         Property::UserInfo{"Enable streaming", "Enable/disable streaming"}});
    properties.setProperty(
        {PARAM_ID, std::string(),
         Property::UserInfo{
             "Stream ID",
             "The ID/name of the stream, equivalent to DEFLECT_ID"}});
    properties.setProperty(
        {PARAM_HOSTNAME, std::string(),
         Property::UserInfo{"Stream hostname", "Hostname of Deflect server"}});
    properties.setProperty(
        {PARAM_PORT,
         (int32_t)deflect::Stream::defaultPortNumber,
         {1023, 65535},
         Property::UserInfo{"Stream port", "Port of Deflect server"}});
    properties.setProperty(
        {PARAM_COMPRESSION, true,
         Property::UserInfo{"Use JPEG compression", "Use JPEG compression"}});
    properties.setProperty(
        {PARAM_TOP_DOWN, false,
         Property::UserInfo{
             "Stream image top-down",
             "Top-down image orientation instead of bottom-up"}});
    properties.setProperty(
        {PARAM_RESIZING, true,
         Property::UserInfo{
             "Allow resizing",
             "Allow resizing of framebuffers from EVT_VIEW_SIZE_CHANGED"}});
    properties.setProperty(
        {PARAM_QUALITY,
         (int32_t)80,
         {1, 100},
         Property::UserInfo{"JPEG quality", "JPEG quality"}});
    properties.setProperty(
        {PARAM_USE_PIXEL_OP, false,
         Property::UserInfo{"Use per-tile direct streaming",
                            "Use per-tile direct streaming"}});
    properties.setProperty(
        {PARAM_CHROMA_SUBSAMPLING, int32_t(deflect::ChromaSubsampling::YUV444),
         enumNames<deflect::ChromaSubsampling>(),
         Property::UserInfo{
             "Chroma subsampling",
             "Chroma subsampling modes: yuv444, yuv422, yuv420"}});
    return properties;
}
}
