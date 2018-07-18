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

from nose.tools import assert_true, assert_false, assert_equal, raises
from mock import Mock, patch
import brayns

TEST_VERSION = {
    'major': 0,
    'minor': 7,
    'patch': 0,
    'revision': 12345
}

TEST_RPC_ONE_PARAMETER = {
    'title': 'test-rpc',
    'description': 'Pass on parameter to brayns',
    'type': 'method',
    'returns': {
        'anyOf': [{
                'type': 'null'
            }, {
                'type': 'object'
            }
        ]
    },
    'params': [{
        'type': 'object',
        'name': 'model_param',
        'description': 'bla',
        'properties': {
            'doit': {
                'type':'boolean'
            },
            'name': {
                'type: string'
            }
        },
        'required': ['name']
    }]
}

TEST_RPC_INVALID = {
    'title': 'test-rpc',
    'type': 'object'
}

TEST_RPC_TWO_PARAMETERS = {
    'title': 'test-rpc-two-params',
    'description': 'Pass on two parameters to brayns',
    'type': 'method',
    'params': [{
        'type': 'object',
        'name': 'param_one',
        'properties': {}
    }, {
        'type': 'object',
        'name': 'param_two',
        'properties': {}
    }]
}

TEST_OBJECT_SCHEMA = {
    'title': 'TestObject',
    'type': 'object',
    'properties': {
        'number:': {'type': 'number'},
        'integer:': {'type': 'integer'},
        'string:': {'type': 'string'},
        'boolean:': {'type': 'boolean'},
        'enum': {
            'type': 'string',
            'enum': [u'value_a', u'value_b']
        },
        'enum_title': {
            'type': 'string',
            'title': 'my_enum',
            'enum': [u'mine', u'yours']
        },
        'enum_array': {
            'type': 'array',
            'items': {
                'type':'string',
                'enum': [u'one', u'two', u'three']
            }
        }
    }
}

TEST_OBJECT = {
    'number': 0.1,
    'integer': 5,
    'string': 'foobar',
    'boolean': False,
    'enum': 'value_b',
    'enum_title': 'yours',
    'enum_array': ['one', 'three']
}

VERSION_SCHEMA = {
    'title': 'Version',
    'type': 'object'
}

TEST_REGISTRY = {
    'test-rpc/schema': ['GET'],
    'test-rpc-invalid/schema': ['GET'],
    'test-rpc-two-params/schema': ['GET'],
    'test-object': ['GET', 'PUT'],
    'test-object/schema': ['GET'],
    'version': ['GET']
}

def mock_http_request(method, url, command, body=None, query_params=None):
    if command == 'test-rpc/schema':
        return brayns.utils.Status(200, TEST_RPC_ONE_PARAMETER)
    if command == 'test-rpc-invalid/schema':
        return brayns.utils.Status(200, TEST_RPC_INVALID)
    if command == 'test-rpc-two-params/schema':
        return brayns.utils.Status(200, TEST_RPC_TWO_PARAMETERS)
    if command == 'test-object/schema':
        return brayns.utils.Status(200, TEST_OBJECT_SCHEMA)
    if command == 'test-object':
        return brayns.utils.Status(200, TEST_OBJECT)
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


def mock_http_request_no_version(method, url, command, body=None, query_params=None):
    return brayns.utils.Status(404, None)


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
def test_init_no_version():
    with patch('brayns.utils.http_request', new=mock_http_request_no_version):
        brayns.Brayns('localhost:8200')


@raises(Exception)
def test_init_wrong_version():
    with patch('brayns.utils.http_request', new=mock_http_request_wrong_version):
        brayns.Brayns('localhost:8200')


@raises(Exception)
def test_init_no_registry():
    with patch('brayns.utils.http_request', new=mock_http_request_no_registry):
        brayns.Brayns('localhost:8200')


def test_object_properties():
    with patch('brayns.utils.http_request', new=mock_http_request):
        app = brayns.Brayns('localhost:8200')
        assert_equal(app.test_object.integer, TEST_OBJECT['integer'])
        assert_equal(app.test_object.number, TEST_OBJECT['number'])
        assert_equal(app.test_object.string, TEST_OBJECT['string'])
        assert_equal(app.test_object.boolean, TEST_OBJECT['boolean'])


def test_object_properties_enum():
    with patch('brayns.utils.http_request', new=mock_http_request):
        app = brayns.Brayns('localhost:8200')
        assert_equal(app.test_object.enum, TEST_OBJECT['enum'])
        assert_true(hasattr(app, 'ENUM_VALUE_A'))
        assert_true(hasattr(app, 'ENUM_VALUE_B'))


def test_object_properties_enum_with_title():
    with patch('brayns.utils.http_request', new=mock_http_request):
        app = brayns.Brayns('localhost:8200')
        assert_equal(app.test_object.enum_title, TEST_OBJECT['enum_title'])
        assert_true(hasattr(app, 'MY_ENUM_MINE'))
        assert_true(hasattr(app, 'MY_ENUM_YOURS'))


def test_object_properties_enum_array():
    with patch('brayns.utils.http_request', new=mock_http_request):
        app = brayns.Brayns('localhost:8200')
        assert_equal(app.test_object.enum_array, TEST_OBJECT['enum_array'])

        assert_true(hasattr(app, 'ENUM_ARRAY_ONE'))
        assert_true(hasattr(app, 'ENUM_ARRAY_TWO'))
        assert_true(hasattr(app, 'ENUM_ARRAY_THREE'))


def test_rpc_one_parameter():
    with patch('brayns.utils.http_request', new=mock_http_request), \
            patch('brayns.Application.rpc_request', new=mock_rpc_request):
        app = brayns.Brayns('localhost:8200')
        import inspect
        assert_equal(inspect.getdoc(app.test_rpc), TEST_RPC_ONE_PARAMETER['description'])
        assert_true(app.test_rpc(doit=False, name='foo'))


def test_rpc_two_parameters():
    with patch('brayns.utils.http_request', new=mock_http_request), \
         patch('brayns.Application.rpc_request', new=mock_rpc_request):
        app = brayns.Brayns('localhost:8200')
        assert_false(hasattr(app, 'test-rpc-two-params'))


def test_rpc_invalid():
    with patch('brayns.utils.http_request', new=mock_http_request), \
            patch('brayns.Application.rpc_request', new=mock_rpc_request):
        app = brayns.Brayns('localhost:8200')
        assert_false(hasattr(app, 'test-rpc-invalid'))

if __name__ == '__main__':
    import nose
    nose.run(defaultTest=__name__)
