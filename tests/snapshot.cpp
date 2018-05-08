/* Copyright (c) 2018, EPFL/Blue Brain Project
 * All rights reserved. Do not distribute without permission.
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

#define BOOST_TEST_MODULE braynsSnapshot

#include <jsonSerialization.h>

#include "ClientServer.h"
#include <brayns/common/engine/Engine.h>
#include <brayns/common/renderer/Renderer.h>

#include <ImageGenerator.h>

BOOST_GLOBAL_FIXTURE(ClientServer);

#ifdef BRAYNS_USE_MAGICKPP
BOOST_AUTO_TEST_CASE(snapshot)
{
    brayns::SnapshotParams params;
    params.format = "jpg";
    params.size = {5, 5};
    params.quality = 75;

    auto image =
        makeRequest<brayns::SnapshotParams,
                    brayns::ImageGenerator::ImageBase64>("snapshot", params);
    BOOST_CHECK_EQUAL(image.data,
                      "/9j/4AAQSkZJRgABAQAAAQABAAD/"
                      "2wBDAAgGBgcGBQgHBwcJCQgKDBQNDAsLDBkSEw8UHRofHh0aHBwgJC4n"
                      "ICIsIxwcKDcpLDAxNDQ0Hyc5PTgyPC4zNDL/"
                      "2wBDAQkJCQwLDBgNDRgyIRwhMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIy"
                      "MjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjL/"
                      "wAARCAAFAAUDASIAAhEBAxEB/8QAFQABAQAAAAAAAAAAAAAAAAAAAAX/"
                      "xAAgEAABAwMFAQAAAAAAAAAAAAACAAEEAwURBxIhMkGB/"
                      "8QAFQEBAQAAAAAAAAAAAAAAAAAABAb/"
                      "xAAcEQACAgIDAAAAAAAAAAAAAAABAgADBEEFEdH/2gAMAwEAAhEDEQA/"
                      "AJ0PVMbfBjwxsrmMekFJiKU3O0WHPT3GfqIir6OLxGqUlNDZ9hVsboT/"
                      "2Q==");
}

BOOST_AUTO_TEST_CASE(snapshot_with_render_params)
{
    // move far enough away to see the background
    auto camera{std::make_unique<brayns::Camera>()};
    *camera = getCamera();
    camera->setPosition({0, 0, 50});

    // use a red background, as opposed to the default black
    auto renderingParams{std::make_unique<brayns::RenderingParameters>()};
    renderingParams->setBackgroundColor({1, 0, 0});

    brayns::SnapshotParams params;
    params.camera = std::move(camera);
    params.format = "jpg";
    params.size = {5, 5};
    params.quality = 75;
    params.name = "black_image";

    auto image =
        makeRequest<brayns::SnapshotParams,
                    brayns::ImageGenerator::ImageBase64>("snapshot", params);

    params.renderingParams = std::move(renderingParams);
    params.name = "red_image";
    auto image_with_red_background =
        makeRequest<brayns::SnapshotParams,
                    brayns::ImageGenerator::ImageBase64>("snapshot", params);

    BOOST_CHECK_NE(image.data, image_with_red_background.data);
}
#endif

BOOST_AUTO_TEST_CASE(snapshot_empty_params)
{
    BOOST_CHECK_THROW((makeRequest<brayns::SnapshotParams,
                                   brayns::ImageGenerator::ImageBase64>(
                          "snapshot", brayns::SnapshotParams())),
                      rockets::jsonrpc::response_error);
}

BOOST_AUTO_TEST_CASE(snapshot_illegal_format)
{
    brayns::SnapshotParams params;
    params.size = {5, 5};
    params.format = "";
    BOOST_CHECK_THROW(
        (makeRequest<brayns::SnapshotParams,
                     brayns::ImageGenerator::ImageBase64>("snapshot", params)),
        rockets::jsonrpc::response_error);
}