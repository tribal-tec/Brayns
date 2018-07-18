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

TEST_OBJECT_SCHEMA = dict()
TEST_OBJECT_SCHEMA['title'] = 'TestObject'
TEST_OBJECT_SCHEMA['type'] = 'object'
TEST_OBJECT_SCHEMA['properties'] = dict()
TEST_OBJECT_SCHEMA['properties']['number'] = {'type':'number'}
TEST_OBJECT_SCHEMA['properties']['integer'] = {'type':'integer'}
TEST_OBJECT_SCHEMA['properties']['string'] = {'type':'string'}
TEST_OBJECT_SCHEMA['properties']['boolean'] = {'type':'boolean'}
TEST_OBJECT_SCHEMA['properties']['enum'] = {'type':'string', 'enum': [u'value_a', u'value_b']}
TEST_OBJECT_SCHEMA['properties']['enum_title'] = {'type':'string', 'title': 'my_enum', 'enum': [u'mine', u'yours']}
TEST_OBJECT_SCHEMA['properties']['enum_array'] = {'type':'array', 'items': { 'type':'string', 'enum': [u'one', u'two', u'three']}}

TEST_OBJECT = dict()
TEST_OBJECT['number'] = 0.1
TEST_OBJECT['integer'] = 5
TEST_OBJECT['string'] = 'foobar'
TEST_OBJECT['boolean'] = False
TEST_OBJECT['enum'] = 'value_b'
TEST_OBJECT['enum_title'] = 'yours'
TEST_OBJECT['enum_array'] = ['one', 'three']

VERSION_SCHEMA = dict()
VERSION_SCHEMA['title'] = 'Version'
VERSION_SCHEMA['type'] = 'object'

TEST_REGISTRY = dict()
TEST_REGISTRY['add-model/schema'] = ['GET']
TEST_REGISTRY['test-object'] = ['GET', 'PUT']
TEST_REGISTRY['test-object/schema'] = ['GET']
TEST_REGISTRY['version'] = ['GET']


def mock_http_request(method, url, command, body=None, query_params=None):
    if command == 'add-model/schema':
        return brayns.utils.Status(200, ADD_MODEL_SCHEMA)
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
