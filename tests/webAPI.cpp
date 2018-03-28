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

#define BOOST_TEST_MODULE braynsWebAPI

#include <jsonSerialization.h>

#include <tests/paths.h>

#include "ClientServer.h"
#include <brayns/common/engine/Engine.h>
#include <brayns/common/renderer/Renderer.h>

#include <ImageGenerator.h>

#include <fstream>

BOOST_GLOBAL_FIXTURE(ClientServer);

BOOST_AUTO_TEST_CASE(reset_camera)
{
    const auto target = getCamera().getTarget();
    getCamera().setTarget({1, 2, 3});
    makeNotification("reset-camera");
    BOOST_CHECK_EQUAL(getCamera().getTarget(), target);
}

BOOST_AUTO_TEST_CASE(inspect)
{
    auto inspectResult =
        makeRequest<std::array<float, 2>, brayns::Renderer::PickResult>(
            "inspect", {{0.5, 0.5}});
    BOOST_CHECK(inspectResult.hit);
    BOOST_CHECK(inspectResult.pos.equals(
        {0.500001490116119, 0.500001490116119, 1.19209289550781e-7}));

    auto failedInspectResult =
        makeRequest<std::array<float, 2>, brayns::Renderer::PickResult>(
            "inspect", {{10, -10}});
    BOOST_CHECK(!failedInspectResult.hit);
}

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
                      "xAAfEAABBAAHAAAAAAAAAAAAAAABAAIRIQMEBxIVMUH/"
                      "xAAVAQEBAAAAAAAAAAAAAAAAAAACBf/"
                      "EABkRAAEFAAAAAAAAAAAAAAAAAAABAxETUf/"
                      "aAAwDAQACEQMRAD8AiDWoBu3gHOEky/OibMxWGKHQ9gWSbREVGhvASp/"
                      "/2Q==");
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

BOOST_AUTO_TEST_CASE(receive_binary_illegal_no_request)
{
    const std::string illegal("illegal");
    getWsClient().sendBinary(illegal.data(), illegal.size());
    process();
    // nothing to test, Brayns ignores the message and prints a warning
}

BOOST_AUTO_TEST_CASE(receive_binary_illegal_no_params)
{
    try
    {
        makeRequest<std::vector<brayns::BinaryParam>, bool>("receive-binary",
                                                            {});
    }
    catch (const rockets::jsonrpc::response_error& e)
    {
        BOOST_CHECK_EQUAL(e.code, -1731);
        BOOST_CHECK(e.data.empty());
    }
}

BOOST_AUTO_TEST_CASE(receive_binary_missing_params)
{
    brayns::BinaryParam params;
    try
    {
        makeRequest<std::vector<brayns::BinaryParam>, bool>("receive-binary",
                                                            {params});
    }
    catch (const rockets::jsonrpc::response_error& e)
    {
        BOOST_CHECK_EQUAL(e.code, -1731);
        BOOST_CHECK(e.data.empty());
    }
}

BOOST_AUTO_TEST_CASE(receive_binary_invalid_size)
{
    brayns::BinaryParam params;
    params.type = "xyz";
    params.size = 0;
    try
    {
        makeRequest<std::vector<brayns::BinaryParam>, bool>("receive-binary",
                                                            {params});
    }
    catch (const rockets::jsonrpc::response_error& e)
    {
        BOOST_CHECK_EQUAL(e.code, -1731);
        BOOST_CHECK(e.data.empty());
    }
}

BOOST_AUTO_TEST_CASE(receive_binary_unsupported_type)
{
    brayns::BinaryParam params;
    params.type = "blub";
    try
    {
        makeRequest<std::vector<brayns::BinaryParam>, bool>("receive-binary",
                                                            {params});
    }
    catch (const rockets::jsonrpc::response_error& e)
    {
        BOOST_CHECK_EQUAL(e.code, -1732);
        BOOST_REQUIRE(!e.data.empty());
        brayns::BinaryError error;
        BOOST_CHECK(from_json(error, e.data));
        BOOST_CHECK_EQUAL(error.index, 0);
        BOOST_CHECK_GT(error.supportedTypes.size(), 0);
    }
}

BOOST_AUTO_TEST_CASE(receive_binary_multiple_files_one_unsupported)
{
    std::vector<brayns::BinaryParam> params{3, {4, ""}};
    params[0].type = "xyz";
    params[1].type = "wrong";
    params[2].type = "abc";
    try
    {
        makeRequest<std::vector<brayns::BinaryParam>, bool>("receive-binary",
                                                            params);
    }
    catch (const rockets::jsonrpc::response_error& e)
    {
        BOOST_REQUIRE(!e.data.empty());
        brayns::BinaryError error;
        BOOST_CHECK(from_json(error, e.data));
        BOOST_CHECK_EQUAL(error.index, 1); // fails on the first wrong param
        BOOST_CHECK_GT(error.supportedTypes.size(), 0);
    }
}

BOOST_AUTO_TEST_CASE(receive_binary)
{
    brayns::BinaryParam params;
    params.size = [] {
        std::ifstream file(BRAYNS_TESTDATA + std::string("monkey.xyz"),
                           std::ios::binary | std::ios::ate);
        return file.tellg();
    }();
    params.type = "xyz";

    auto responseFuture =
        getJsonRpcClient().request<std::vector<brayns::BinaryParam>, bool>(
            "receive-binary", {params});

    auto asyncWait = std::async(std::launch::async, [&responseFuture] {
        while (!is_ready(responseFuture))
            process();
    });

    std::ifstream file(BRAYNS_TESTDATA + std::string("monkey.xyz"),
                       std::ios::binary);

    std::vector<char> buffer(1024, 0);

    while (file.read(buffer.data(), buffer.size()))
    {
        const std::streamsize size = file.gcount();
        getWsClient().sendBinary(buffer.data(), size);
    }

    // read & send last chunk
    const std::streamsize size = file.gcount();
    if (size != 0)
    {
        file.read(buffer.data(), size);
        getWsClient().sendBinary(buffer.data(), size);
    }

    asyncWait.get();
    BOOST_CHECK(responseFuture.get());
}

BOOST_AUTO_TEST_CASE(receive_binary_cancel)
{
    brayns::BinaryParam params;
    params.size = 42;
    params.type = "xyz";

    auto responseFuture =
        getJsonRpcClient().request<std::vector<brayns::BinaryParam>, bool>(
            "receive-binary", {params});

    auto asyncWait = std::async(std::launch::async, [&responseFuture] {
        while (!is_ready(responseFuture))
            process();
    });

    getJsonRpcClient().cancelLastRequest();

    asyncWait.get();
    BOOST_CHECK_THROW(responseFuture.get(), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(receive_binary_second_request_with_first_one_not_finished)
{
    brayns::BinaryParam params;
    params.size = 4;
    params.type = "xyz";

    auto responseFuture =
        getJsonRpcClient().request<std::vector<brayns::BinaryParam>, bool>(
            "receive-binary", {params});

    try
    {
        makeRequest<std::vector<brayns::BinaryParam>, bool>("receive-binary",
                                                            {params});
        BOOST_REQUIRE(false);
    }
    catch (const rockets::jsonrpc::response_error& e)
    {
        BOOST_CHECK_EQUAL(e.code, -1730);
    }
}
