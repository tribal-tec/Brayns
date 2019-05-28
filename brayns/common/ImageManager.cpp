/* Copyright (c) 2015-2017, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
 * Responsible Author: Cyrille Favreau <cyrille.favreau@epfl.ch>
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

#include "ImageManager.h"
#include <brayns/common/log.h>
#include <brayns/common/utils/imageUtils.h>

namespace brayns
{
Texture2DPtr ImageManager::importTextureFromFile(
    const std::string& filename BRAYNS_UNUSED)
{
#ifdef BRAYNS_USE_FREEIMAGE
    auto format = FreeImage_GetFileType(filename.c_str());
    if (format == FIF_UNKNOWN)
        format = FreeImage_GetFIFFromFilename(filename.c_str());
    if (format == FIF_UNKNOWN)
        return {};

    freeimage::ImagePtr image(FreeImage_Load(format, filename.c_str()));
    if (!image)
        return {};

#if FREEIMAGE_COLORORDER == FREEIMAGE_COLORORDER_BGR
// freeimage::SwapRedBlue32(image.get());
#endif

    const auto width = FreeImage_GetWidth(image.get());
    const auto height = FreeImage_GetHeight(image.get());
    const auto bytesPerPixel = FreeImage_GetBPP(image.get()) / 8;
    size_t depth = 1;
    switch (FreeImage_GetImageType(image.get()))
    {
    case FIT_BITMAP:
        depth = 1;
        break;
    case FIT_UINT16:
    case FIT_INT16:
    case FIT_RGB16:
        depth = 2;
        break;
    case FIT_UINT32:
    case FIT_INT32:
    case FIT_RGBA16:
        depth = 4;
        break;
    case FIT_FLOAT:
    case FIT_RGBF:
    case FIT_RGBAF:
        depth = 4;
        break;
    case FIT_DOUBLE:
    case FIT_COMPLEX:
        depth = 8;
        break;
    default:
        return {};
    }

    FreeImage_FlipVertical(image.get());

    unsigned char* temppix = FreeImage_GetBits(image.get());
    std::vector<unsigned char> rawData(temppix,
                                       temppix +
                                           width * height * bytesPerPixel);

    auto texture = std::make_shared<Texture2D>();
    texture->setFilename(filename);
    texture->setWidth(width);
    texture->setHeight(height);
    texture->setNbChannels(bytesPerPixel / depth);
    texture->setDepth(depth);
    texture->setRawData(std::move(rawData));
    return texture;
#else
    BRAYNS_ERROR << "FreeImage is required to load images from file"
                 << std::endl;
    return {};
#endif
}
}
