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

"""
The application class exposes a dynamic generated API from HTTP/websockets server exposed registry.
"""

import os
import python_jsonschema_objects as pjs
import inflection

from .utils import HTTP_METHOD_GET, HTTP_METHOD_PUT, HTTP_STATUS_OK

from . import utils


def _create_rpc_object_parameter(param, method, description):
    """
    Create an RPC where each property of the param object is a key-value argument.
    :param param: the parameter object from the RPC
    :param method: the name of RPC
    :param description: the description of RPC
    :return: the code of the function
    """

    required = param['required'] if 'required' in param else list()
    optional = list()
    for s in param['properties'].keys():
        if s not in required:
            optional.append(s + '=None')
    arg_list = ', '.join(required)
    if optional and required:
        arg_list += ', '
    arg_list += ', '.join(filter(None, optional))
    return '''
        def function(self, {0}, response_timeout=5):
            """
            {1}
            """
            args = locals()
            del args['self']
            del args['response_timeout']
            return self.rpc_request("{2}", params={{k:v for k,v in args.items()
                                             if v is not None}},
                                             response_timeout=response_timeout)
        '''.format(arg_list, description, method)


def _create_rpc_array_parameter(name, method, description):
    """
    Create an RPC where the parameter is an array argument.
    :param name: the name of the array argument
    :param method: the name of RPC
    :param description: the description of RPC
    :return: the code of the function
    """
    return '''
        def function(self, {1}, response_timeout=5):
            """
            {0}
            {2}
            """
            return self.rpc_request("{3}", params={1},
                                    response_timeout=response_timeout)
        '''.format(description, name, ":param {0}: {1}".format(name, description), method)


def _add_enums(value, target_object):
    """
    Look for enums in the given object to create string constants <ENUM_CLASSNAME>_<ENUM_VALUE>
    """

    for i in value.keys():
        enum = None
        if 'enum' in value.propinfo(i):
            enum = value.propinfo(i)
        if value.propinfo(i)['type'] == 'array':
            if 'enum' in value.propinfo(i)['items']:
                enum = value.propinfo(i)['items']
        if not enum:
            continue

        enum_class = str(i).upper()
        if 'title' in enum:
            enum_class = str(inflection.underscore(enum['title'])).upper()
        enum_class += "_"
        for val in enum['enum']:
            enum_value = enum_class + inflection.parameterize(val, '_').upper()
            setattr(target_object, enum_value, val)


def create_all_properties(target_object, url):
    """
    Add all exposed objects and types from the application as properties to the Application.
    """

    registry = _obtain_registry(url)

    for object_name in registry.keys():
        if _handle_rpc(target_object, url, object_name):
            continue

        schema, ret_code = _obtain_schema(url, object_name)
        if ret_code != HTTP_STATUS_OK:
            continue

        classes = pjs.ObjectBuilder(schema).build_classes()
        class_names = dir(classes)

        # find the generated class name that matches our schema root title
        for c in class_names:
            if c.lower() == schema['title'].lower():
                class_name = c
                break

        class_type = getattr(classes, class_name)

        is_object = schema['type'] == 'object'
        if is_object:
            value = class_type()
            _add_enums(value, target_object)
            if HTTP_METHOD_PUT in registry[object_name]:
                _add_commit(target_object, class_type, object_name)
        else:  # array
            value = class_type(())

        # add member to Application
        member = '_' + os.path.basename(object_name).replace('-', '_')
        setattr(target_object, member, value)
        _add_property(target_object, member, object_name, schema['type'])


def _handle_rpc(target_object, url, object_name):
    """
    Try to handle object_name as RPC
    :param object_name: registry endpoint, e.g. v1/foo[/schema]
    :return: True if object_name was RPC, False otherwise
    """
    if not object_name.endswith('/schema'):
        return False

    method = object_name[:-len('/schema')]
    status = utils.http_request(HTTP_METHOD_GET, url, method)
    schema, ret_code = _obtain_schema(url, method)
    if status.code != HTTP_STATUS_OK and ret_code == HTTP_STATUS_OK:
        _add_rpc(target_object, schema)
        return True
    return False


def _add_rpc(target_object, schema):
    """
    Add a new function from the given schema that describes an RPC.
    :param schema: schema containing name, description, params of the RPC
    """
    if schema['type'] != 'method':
        return

    method = schema['title']
    func_name = str(os.path.basename(method).replace('-', '_'))

    if 'params' in schema and len(schema['params']) > 1:
        print("Multiple parameters for RPC '{0}' not supported".format(method))
        return

    description = schema['description']
    if 'params' in schema and len(schema['params']) == 1:
        params = schema['params'][0]
        if 'oneOf' in params:
            code = _handle_param_oneof(target_object, params['oneOf'], method, description)
        # in the absence of multiple parameters support, create a function with multiple
        # parameters from object properties
        elif params['type'] == 'object':
            code = _create_rpc_object_parameter(params, method, description)
        elif params['type'] == 'array':
            code = _create_rpc_array_parameter(params['name'], method, description)
        else:
            raise Exception('Invalid parameter type for method "{0}":'.format(method) +
                            ' must be "object", "array" or "oneOf"')
    else:
        code = '''
            def function(self, response_timeout=5):
                """
                {0}
                """
                return self.rpc_request("{1}", response_timeout=response_timeout)
            '''.format(description, method)

    d = {}
    exec(code.strip(), d)  # pylint: disable=W0122
    function = d['function']
    function.__name__ = func_name
    setattr(target_object.__class__, function.__name__, function)


def _handle_param_oneof(target_object, param, method, description):
    """
    Create an RPC where the parameter is from the oneOf array and create a type for each oneOf
    type.
    :param param: the oneOf array
    :param method: name of RPC
    :param description: description of RPC
    :return: the code of the function
    """

    param_types = list()
    for o in param:
        classes = pjs.ObjectBuilder(o).build_classes()
        class_names = dir(classes)

        # find the generated class name that matches our schema root title
        for c in class_names:
            if c == inflection.camelize(o['title']):
                class_name = c
                break

        # create class name <Type><Method w/o set->, e.g. Perspective+CameraParams
        pretty_class_name = method[4:].replace("-", "_")
        pretty_class_name = inflection.camelize(pretty_class_name)
        pretty_class_name = class_name + pretty_class_name

        # add type to application
        class_type = getattr(classes, class_name)
        setattr(target_object, pretty_class_name, class_type)
        param_types.append(pretty_class_name)

        # create and add potential enums to type
        _add_enums(class_type(), class_type)

    return '''
        def function(self, params, response_timeout=5):
            """
            {0}
            ":param: one of the params for the active type: {1}
            """
            return self.rpc_request("{2}", params.for_json(),
                                    response_timeout=response_timeout)
        '''.format(description, ', '.join(param_types), method)


def _add_commit(target_object, class_type, object_name):
    """ Add commit() for given property """

    def commit_builder(url):
        """ Wrapper for returning the property.commit() function """

        def commit(prop):
            """ Update the property in the application """

            return target_object.rpc_request('set-' + os.path.basename(url), prop.as_dict())

        return commit

    setattr(class_type, 'commit', commit_builder(object_name))


def _add_property(target_object, member, object_name, property_type):
    """ Add property to Application for object """

    def getter_builder(member, object_name):
        """ Wrapper for returning the property state """

        def function(self):
            """ Returns the current state for the property """
            value = getattr(self, member)

            # Initialize on first access; updates are received via websocket
            if property_type == 'array':
                has_value = value.data
            else:
                has_value = value.as_dict()
            if not has_value:
                status = utils.http_request(HTTP_METHOD_GET, self.url(), object_name)
                if status.code == HTTP_STATUS_OK:
                    if property_type == 'array':
                        value.__init__(status.contents)
                    else:
                        value.__init__(**status.contents)

            if property_type == 'array':
                return value.data
            return value

        return function

    endpoint_name = os.path.basename(object_name)
    snake_case_name = endpoint_name.replace('-', '_')
    setattr(type(target_object), snake_case_name,
            property(fget=getter_builder(member, object_name),
                     doc='Access to the {0} property'.format(endpoint_name)))


def _obtain_registry(url):
    """ Obtain the registry of exposed objects and RPCs from brayns """
    status = utils.http_request(HTTP_METHOD_GET, url, 'registry')
    if status.code != HTTP_STATUS_OK:
        raise Exception('Failed to obtain registry from Brayns')
    return status.contents


def _obtain_schema(url, object_name):
    """ Returns the JSON schema for the given object """
    status = utils.http_request(HTTP_METHOD_GET, url, object_name + '/schema')
    return status.contents, status.code
