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

#define BOOST_TEST_MODULE braynsRenderer

#include <jsonPropertyMap.h>

#include "ClientServer.h"

const std::string GET_RENDERER("get-renderer");
const std::string SET_RENDERER("set-renderer");
const std::string RENDERER("renderer");

const std::string GET_RENDERER_PARAMS("get-renderer-params");
const std::string SET_RENDERER_PARAMS("set-renderer-params");
const std::string RENDERER_PARAMS("renderer-params");

BOOST_GLOBAL_FIXTURE(ClientServer);

BOOST_AUTO_TEST_CASE(get_renderer)
{
    auto renderer = makeRequest<brayns::RenderingParameters>(GET_RENDERER);
    BOOST_CHECK_EQUAL(renderer.getCurrentRenderer(), "basic");
}

BOOST_AUTO_TEST_CASE(get_renderer_params)
{
    BOOST_CHECK_EQUAL(getRenderer().getCurrentType(), "basic");
    auto rendererParams = makeRequest<brayns::PropertyMap>(GET_RENDERER_PARAMS);
    BOOST_CHECK(rendererParams.getProperties().empty());
}

BOOST_AUTO_TEST_CASE(change_renderer)
{
    BOOST_CHECK_EQUAL(getRenderer().getCurrentType(), "basic");

    auto params = ClientServer::instance()
                      .getBrayns()
                      .getParametersManager()
                      .getRenderingParameters();
    params.setCurrentRenderer("scivis");
    BOOST_CHECK(
        (makeRequest<brayns::RenderingParameters, bool>(SET_RENDERER, params)));
    BOOST_CHECK_EQUAL(getRenderer().getCurrentType(), "scivis");

    brayns::PropertyMap scivisProps = getRenderer().getPropertyMap("scivis");
    auto rendererParams =
        makeRequestUpdate<brayns::PropertyMap>(GET_RENDERER_PARAMS,
                                               scivisProps);
    BOOST_CHECK(!rendererParams.getProperties().empty());
    BOOST_CHECK_EQUAL(rendererParams.getProperty<int>("aoSamples"), 1);

    rendererParams.updateProperty("aoSamples", 42);
    BOOST_CHECK((makeRequest<brayns::PropertyMap, bool>(SET_RENDERER_PARAMS,
                                                        rendererParams)));
    BOOST_CHECK_EQUAL(getRenderer().getPropertyMap("scivis").getProperty<int>(
                          "aoSamples"),
                      42);

    params.setCurrentRenderer("wrong");
    BOOST_CHECK_THROW(
        (makeRequest<brayns::RenderingParameters, bool>(SET_RENDERER, params)),
        std::runtime_error);
    BOOST_CHECK_EQUAL(getRenderer().getCurrentType(), "scivis");
}
