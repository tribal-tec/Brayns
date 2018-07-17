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

from nose.tools import assert_true, assert_equal
from mock import Mock, patch
import brayns

TEST_VERSION = dict()
TEST_VERSION['major'] = 0
TEST_VERSION['minor'] = 7
TEST_VERSION['patch'] = 0
TEST_VERSION['revision'] = 12345

VERSION_SCHEMA = dict()
VERSION_SCHEMA['title'] = 'Version'
VERSION_SCHEMA['type'] = 'object'


def mock_http_request(method, url, command, body=None, query_params=None):
    if command == 'version':
        return brayns.utils.Status(200, TEST_VERSION)
    if command == 'version/schema':
        return brayns.utils.Status(200, VERSION_SCHEMA)
    if command == 'registry':
        registry = dict()
        registry['version'] = ['GET']
        return brayns.utils.Status(200, registry)
    return brayns.utils.Status(404, 'muh')


def test_brayns_init():
    with patch('brayns.utils.http_request', new=mock_http_request):
        app = brayns.Brayns('localhost:8200')
        assert_equal(app.url(), 'http://localhost:8200/')
        assert_equal(app.version.as_dict(), TEST_VERSION)
