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

#define BOOST_TEST_MODULE braynsReceiveBinary

#include <jsonSerialization.h>

#include <tests/paths.h>

#include "ClientServer.h"

#include <fstream>

BOOST_GLOBAL_FIXTURE(ClientServer);

BOOST_AUTO_TEST_CASE(illegal_no_request)
{
    const std::string illegal("illegal");
    getWsClient().sendBinary(illegal.data(), illegal.size());
    process();
    // nothing to test, Brayns ignores the message and prints a warning
}

BOOST_AUTO_TEST_CASE(illegal_no_params)
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

BOOST_AUTO_TEST_CASE(missing_params)
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

BOOST_AUTO_TEST_CASE(invalid_size)
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

BOOST_AUTO_TEST_CASE(unsupported_type)
{
    brayns::BinaryParam params;
    params.type = "blub";
    params.size = 4;
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

BOOST_AUTO_TEST_CASE(multiple_files_one_unsupported)
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

BOOST_AUTO_TEST_CASE(xyz)
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

BOOST_AUTO_TEST_CASE(broken_xyz)
{
    brayns::BinaryParam params;
    params.size = [] {
        std::ifstream file(BRAYNS_TESTDATA + std::string("broken.xyz"),
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

    std::ifstream file(BRAYNS_TESTDATA + std::string("broken.xyz"),
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
    try
    {
        responseFuture.get();
    }
    catch (const rockets::jsonrpc::response_error& e)
    {
        BOOST_CHECK_EQUAL(e.code, -1734);
        BOOST_CHECK_EQUAL(e.what(),
                          "Invalid content in line 1: 2.500000 3.437500");
    }
}

BOOST_AUTO_TEST_CASE(cancel)
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
        responseFuture.get();
    });

    getJsonRpcClient().cancelLastRequest();

    BOOST_CHECK_THROW(asyncWait.get(), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(send_wrong_number_of_bytes)
{
    brayns::BinaryParam params;
    params.size = 4;
    params.type = "xyz";

    auto responseFuture =
        getJsonRpcClient().request<std::vector<brayns::BinaryParam>, bool>(
            "receive-binary", {params});

    auto asyncWait = std::async(std::launch::async, [&responseFuture] {
        while (!is_ready(responseFuture))
            process();
        responseFuture.get();
    });

    const std::string wrong("not_four_bytes");
    getWsClient().sendBinary(wrong.data(), wrong.size());

    try
    {
        asyncWait.get();
        BOOST_REQUIRE(false);
    }
    catch (const rockets::jsonrpc::response_error& e)
    {
        BOOST_CHECK_EQUAL(e.code, -1733);
    }
}

BOOST_AUTO_TEST_CASE(cancel_while_loading)
{
    brayns::BinaryParam params;
    params.size = 4;
    params.type = "forever";

    auto responseFuture =
        getJsonRpcClient().request<std::vector<brayns::BinaryParam>, bool>(
            "receive-binary", {params});

    auto asyncWait = std::async(std::launch::async, [&responseFuture] {
        while (!is_ready(responseFuture))
            process();
        responseFuture.get();
    });

    const std::string fourBytes("four");
    getWsClient().sendBinary(fourBytes.data(), fourBytes.size());

    getJsonRpcClient().cancelLastRequest();

    BOOST_CHECK_THROW(asyncWait.get(), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(close_client_while_pending_request)
{
}

BOOST_AUTO_TEST_CASE(multiple_xyz)
{
}

BOOST_AUTO_TEST_CASE(obj)
{
}

BOOST_AUTO_TEST_CASE(second_request_with_first_one_not_finished)
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
