#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Copyright (c) 2016-2018, Blue Brain Project
#                          Raphael Dumusc <raphael.dumusc@epfl.ch>
#                          Daniel Nachbaur <daniel.nachbaur@epfl.ch>
#                          Cyrille Favreau <cyrille.favreau@epfl.ch>
#
# This file is part of Brayns <https://github.com/BlueBrain/Brayns>
#
# This library is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License version 3.0 as published
# by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
# details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
# All rights reserved. Do not distribute without further notice.

from nose.tools import assert_true, assert_equal, raises
from mock import Mock, patch
import brayns

TEST_VERSION = dict()
TEST_VERSION['major'] = 0
TEST_VERSION['minor'] = 7
TEST_VERSION['patch'] = 0
TEST_VERSION['revision'] = 12345

ADD_MODEL_SCHEMA = dict()
ADD_MODEL_SCHEMA['title'] = 'add-model'
ADD_MODEL_SCHEMA['description'] = 'Add model from remote path; returns model descriptor on success'
ADD_MODEL_SCHEMA['type'] = 'method'
ADD_MODEL_SCHEMA['returns'] = dict()
ADD_MODEL_SCHEMA['returns']['anyOf'] = list()
ADD_MODEL_SCHEMA['returns']['anyOf'].append({'type': 'null'})
ADD_MODEL_SCHEMA['returns']['anyOf'].append({'type': 'object'})
ADD_MODEL_SCHEMA_PARAM = dict()
ADD_MODEL_SCHEMA_PARAM['type'] = 'object'
ADD_MODEL_SCHEMA_PARAM['name'] = 'model_param'
ADD_MODEL_SCHEMA_PARAM['description'] = 'bla'
ADD_MODEL_SCHEMA_PARAM['properties'] = dict()
ADD_MODEL_SCHEMA_PARAM['properties']['visible'] = {'type':'boolean'}
ADD_MODEL_SCHEMA_PARAM['properties']['name'] = {'type':'string'}
ADD_MODEL_SCHEMA_PARAM['required'] = ['name']
ADD_MODEL_SCHEMA['params'] = list()
ADD_MODEL_SCHEMA['params'].append(ADD_MODEL_SCHEMA_PARAM)

ANIMATION_PARAMETERS_SCHEMA = dict()
ANIMATION_PARAMETERS_SCHEMA['title'] = 'AnimationParameters'
ANIMATION_PARAMETERS_SCHEMA['type'] = 'object'
ANIMATION_PARAMETERS_SCHEMA['properties'] = dict()
ANIMATION_PARAMETERS_SCHEMA['properties']['dt'] = {'type':'number'}
ANIMATION_PARAMETERS_SCHEMA['properties']['current'] = {'type':'integer'}
ANIMATION_PARAMETERS_SCHEMA['properties']['unit'] = {'type':'string'}

TEST_ANIMATION_PARAMS = dict()
TEST_ANIMATION_PARAMS['dt'] = 0.1
TEST_ANIMATION_PARAMS['current'] = 5
TEST_ANIMATION_PARAMS['unit'] = 'ms'

VERSION_SCHEMA = dict()
VERSION_SCHEMA['title'] = 'Version'
VERSION_SCHEMA['type'] = 'object'

TEST_REGISTRY = dict()
TEST_REGISTRY['add-model/schema'] = ['GET']
TEST_REGISTRY['animation-parameters'] = ['GET', 'PUT']
TEST_REGISTRY['animation-parameters/schema'] = ['GET']
TEST_REGISTRY['version'] = ['GET']


def mock_http_request(method, url, command, body=None, query_params=None):
    if command == 'add-model/schema':
        return brayns.utils.Status(200, ADD_MODEL_SCHEMA)
    if command == 'animation-parameters/schema':
        return brayns.utils.Status(200, ANIMATION_PARAMETERS_SCHEMA)
    if command == 'animation-parameters':
        return brayns.utils.Status(200, TEST_ANIMATION_PARAMS)
    if command == 'version':
        return brayns.utils.Status(200, TEST_VERSION)
    if command == 'version/schema':
        return brayns.utils.Status(200, VERSION_SCHEMA)
    if command == 'registry':
        return brayns.utils.Status(200, TEST_REGISTRY)
    return brayns.utils.Status(404, None)


def mock_http_request_wrong_version(method, url, command, body=None, query_params=None):
    if command == 'version':
        import copy
        version = copy.deepcopy(TEST_VERSION)
        version['minor'] = 3
        return brayns.utils.Status(200, version)


def mock_http_request_no_registry(method, url, command, body=None, query_params=None):
    if command == 'version':
        return brayns.utils.Status(200, TEST_VERSION)
    if command == 'registry':
        return brayns.utils.Status(404, None)


def mock_rpc_request(self, method, params=None, response_timeout=5):
    return True


def test_init():
    with patch('brayns.utils.http_request', new=mock_http_request):
        app = brayns.Brayns('localhost:8200')
        assert_equal(app.url(), 'http://localhost:8200/')
        assert_equal(app.version.as_dict(), TEST_VERSION)
        assert_equal(str(app), 'Application version 0.7.0.12345 running on http://localhost:8200/')


@raises(Exception)
def test_init_wrong_version():
    with patch('brayns.utils.http_request', new=mock_http_request_wrong_version):
        brayns.Brayns('localhost:8200')


@raises(Exception)
def test_init_no_registry():
    with patch('brayns.utils.http_request', new=mock_http_request_no_registry):
        brayns.Brayns('localhost:8200')


def test_object_generation():
    with patch('brayns.utils.http_request', new=mock_http_request):
        app = brayns.Brayns('localhost:8200')
        assert_equal(app.animation_parameters.current, TEST_ANIMATION_PARAMS['current'])
        assert_equal(app.animation_parameters.dt, TEST_ANIMATION_PARAMS['dt'])
        assert_equal(app.animation_parameters.unit, TEST_ANIMATION_PARAMS['unit'])

def test_method_generation():
    with patch('brayns.utils.http_request', new=mock_http_request), \
            patch('brayns.Application.rpc_request', new=mock_rpc_request):
        app = brayns.Brayns('localhost:8200')
        import inspect
        assert_equal(inspect.getdoc(app.add_model), ADD_MODEL_SCHEMA['description'])
        assert_true(app.add_model(visible=False, name='foo'))

if __name__ == '__main__':
    import nose
    nose.run(defaultTest=__name__)
