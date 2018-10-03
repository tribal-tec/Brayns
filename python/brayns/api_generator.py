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
Provides functionality to dynamically add properties, types and functions to a given object.

The information for the API generation is based on a registry and JSON-schema of exposed properties
and RPCs.
"""

import os
import python_jsonschema_objects as pjs
import inflection

from .utils import HTTP_METHOD_PUT, SCHEMA_ENDPOINT, add_method, underscorize


def build_api(target_object, registry, schemas):
    """
    Add the API (types, properties, methods) to the given object from registry and schemas.

    :param object target_object: The target object where to add all generated properties, RPCs and
                                 types to
    :param dict registry: The registry of all endpoints which are properties and methods
    :param dict schemas: The schemas matching the endpoints from the registry
    """
    for registry_entry in registry.keys():
        if registry_entry in schemas:
            schema = schemas[registry_entry]
        elif registry_entry.endswith(SCHEMA_ENDPOINT):
            method_name = registry_entry[:-len(SCHEMA_ENDPOINT)]
            schema = schemas[method_name]

        if 'type' not in schema:
            continue

        if 'method' in schema['type']:
            _add_method(target_object, schema)
            continue
        if registry_entry.endswith(SCHEMA_ENDPOINT):
            continue

        writeable = HTTP_METHOD_PUT in registry[registry_entry]
        _try_add_property(target_object, registry_entry, schema, writeable)


def _try_add_property(target_object, registry_entry, schema, writeable):
    """
    Try to add registry_entry as a property.

    This will add a property with the name registry_entry which initializes itself with current
    value from Brayns on first access. Furthermore, it adds potential enum values as string
    constants and adds a commit() function if the property is writeable.

    :param object target_object: The target object where to add the property to
    :param str registry_entry: registry endpoint, e.g. foo[/schema]
    :param dict schema: The schema of the object behind the endpoint.
    :param bool writeable: if the property is writeable or not
    """
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
        if writeable:
            _add_commit(target_object, class_type, registry_entry)
    else:  # array
        value = class_type(())

    # add member and property to target_object
    member = '_' + underscorize(os.path.basename(registry_entry))
    setattr(target_object, member, value)
    _add_property(target_object, member, registry_entry, schema['type'])


def _add_method_with_object_arg(cls, param, method, description, is_async, is_request):
    """
    Add a method to cls where each property of the param object is a key-value argument.

    :param dict param: the parameter object from the RPC
    :param str method: the name of RPC
    :param str description: the description of RPC
    :param bool is_async: if the RPC is processed asynchronously or not
    :param bool is_request: if the RPC is a request with a response or a notification only
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

    func_name = str(underscorize(os.path.basename(method)))
    if is_request:
        if is_async:
            code = '''
                def function(self, {0}, call_async=True):
                    args = locals()
                    del args['self']
                    del args['call_async']
                    params = {{k:v for k,v in args.items() if v is not None}}
                    if call_async:
                        return self.async_request("{1}", params)
                    return self.request("{1}", params)
                '''.format(arg_list, method)
        else:
            code = '''
                def function(self, {0}, response_timeout=None):
                    args = locals()
                    del args['self']
                    params = {{k:v for k,v in args.items() if v is not None}}
                    return self.request("{1}", params)
                '''.format(arg_list, method)
    else:
        code = '''
            def function(self, {0}):
                args = locals()
                del args['self']
                return self.notify("{1}", params={{k:v for k,v in args.items()
                                                if v is not None}})
            '''.format(arg_list, method)

    d = {}
    exec(code.strip(), d)  # pylint: disable=W0122
    function = d['function']
    add_method(cls, func_name, description)(function)


def _add_method_with_array_arg(cls, param, method, description, is_async, is_request):
    """
    Add a method to cls where the argument is an array.

    :param dict param: the parameter object from the RPC
    :param str method: the name of RPC
    :param str description: the description of RPC
    :param bool is_async: if the RPC is processed asynchronously or not
    :param bool is_request: if the RPC is a request with a response or a notification only
    """
    if 'description' in param:
        description += '\n:param {0}: {1}'.format(param['name'], param['description'])
    func_name = str(underscorize(os.path.basename(method)))
    if is_request:
        if is_async:
            @add_method(cls, func_name, description)
            def function(self, array, call_async=True):  # pylint: disable=C0111,W0612
                if call_async:
                    return self.async_request(method, array)
                return self.request(method, array)
        else:
            @add_method(cls, func_name, description)
            def function(self, array, response_timeout=None):  # pylint: disable=C0111
                return self.request(method, array, response_timeout=response_timeout)
    else:
        @add_method(cls, func_name, description)
        def function(self, array):  # pylint: disable=C0111
            self.notify(method, array)


def _add_method_with_oneof_arg(target_object, param, method, description, is_async, is_request):
    """
    Add a method to cls where the argument is from a oneOf array.

    Also create a type for each oneOf type and add it to target_object.

    :param object target_object: The target object where to add the oneOf types to
    :param list param: the oneOf array
    :param str method: name of RPC
    :param str description: description of RPC
    :param bool is_async: if the RPC is processed asynchronously or not
    :param bool is_request: if the RPC is a request with a response or a notification only
    """
    param_types = list()
    for o in param:
        classes = pjs.ObjectBuilder(o).build_classes()
        class_names = dir(classes)

        # find the generated class name that matches our schema root title
        class_name = None
        for c in class_names:
            if c.lower() == inflection.camelize(o['title'].replace(' ', '_')).lower():
                class_name = c
                break

        # create class name <Type><Method w/o set->, e.g. Perspective+CameraParams
        pretty_class_name = underscorize(method[4:])
        pretty_class_name = inflection.camelize(pretty_class_name)
        pretty_class_name = class_name + pretty_class_name

        # add type to target_object
        class_type = getattr(classes, class_name)
        setattr(target_object, pretty_class_name, class_type)
        param_types.append(pretty_class_name)

        # create and add potential enums to type
        _add_enums(class_type(), class_type)

    func_name = str(underscorize(os.path.basename(method)))
    cls = target_object.__class__
    if is_request:
        if is_async:
            @add_method(cls, func_name, description)
            def function(self, params, call_async=True):  # pylint: disable=C0111,W0612
                if call_async:
                    return self.async_request(method, params.for_json())
                return self.request(method, params.for_json())
        else:
            @add_method(cls, func_name, description)
            def function(self, params, response_timeout=None):  # pylint: disable=C0111
                return self.request(method, params.for_json(), response_timeout=response_timeout)
    else:
        @add_method(cls, func_name, description)
        def function(self, params):  # pylint: disable=C0111
            self.notify(method, params.for_json())


def _add_method_with_no_args(cls, method, description, is_async, is_request):
    """
    Add a method to cls which has no argument.

    :param str method: name of RPC
    :param str description: description of RPC
    :param bool is_async: if the RPC is processed asynchronously or not
    :param bool is_request: if the RPC is a request with a response or a notification only
    """
    func_name = str(underscorize(os.path.basename(method)))
    if is_request:
        if is_async:
            @add_method(cls, func_name, description)
            def function(self, call_async=True):  # pylint: disable=C0111,W0612
                if call_async:
                    return self.async_request(method)
                return self.request(method)
        else:
            @add_method(cls, func_name, description)
            def function(self, response_timeout=None):  # pylint: disable=C0111
                return self.request(method, response_timeout=response_timeout)
    else:
        @add_method(cls, func_name, description)
        def function(self):  # pylint: disable=C0111
            self.notify(method)


def _add_enums(root_object, target_object):
    """
    Look for enums in the given object to create string constants <ENUM_CLASSNAME>_<ENUM_VALUE>.

    :param dict root_object: the object which may contain enums
    :param object target_object: The target object where to add the string constants to
    """
    for i in root_object.keys():
        enum = None
        propinfo = root_object.propinfo(i)
        if 'enum' in propinfo:
            enum = propinfo
        if propinfo['type'] == 'array':
            if 'enum' in propinfo['items']:
                enum = propinfo['items']
        if not enum:
            continue

        enum_class = str(i).upper()
        if 'title' in enum:
            enum_class = str(inflection.underscore(enum['title'])).upper()
        enum_class += "_"
        for val in enum['enum']:
            enum_value = enum_class + inflection.parameterize(val, '_').upper()
            setattr(target_object, enum_value, val)


def _add_method(target_object, schema):
    """
    Add a new method to target_object from the given schema that describes an RPC.

    :param object target_object: The target object where to add the method to
    :param dict schema: schema containing name, description, params of the RPC
    :raises Exception: if the param type of the RPC does not match oneOf, object or array
    """
    method = schema['title']

    if 'params' in schema and len(schema['params']) > 1:
        print("Multiple arguments for RPC '{0}' not supported".format(method))
        return

    description = schema['description']
    is_async = schema['async']
    is_request = 'returns' in schema and schema['returns']

    if 'params' in schema and len(schema['params']) == 1:
        params = schema['params'][0]
        if 'oneOf' in params:
            _add_method_with_oneof_arg(target_object, params['oneOf'], method, description,
                                       is_async, is_request)
        # in the absence of multiple argument support, create a function with multiple arguments
        # from object properties
        elif params['type'] == 'object':
            _add_method_with_object_arg(target_object.__class__, params, method, description,
                                        is_async, is_request)
        elif params['type'] == 'array':
            _add_method_with_array_arg(target_object.__class__, params, method, description,
                                       is_async, is_request)
        else:
            raise Exception('Invalid argument type for method "{0}": '.format(method) +
                            'must be "object", "array" or "oneOf"')
    else:
        _add_method_with_no_args(target_object.__class__, method, description, is_async, is_request)


def _add_commit(rpc_client, property_type, object_name):
    """Add commit() for given property type."""
    def commit_builder(url):
        """Wrapper for returning the property.commit() function."""
        def commit(prop):
            """Update the property in the remote side."""
            return rpc_client.request('set-' + os.path.basename(url), prop.as_dict())

        return commit

    setattr(property_type, 'commit', commit_builder(object_name))


def _add_property(target_object, member, property_name, property_type):
    """Add property to target_object which initializes itself on first access."""
    def getter_builder(member, property_name):
        """Wrapper for returning the property state."""
        def function(self):
            """Returns the current state for the property."""
            value = getattr(self, member)

            # Initialize on first access; updates are received via websocket
            if property_type == 'array':
                has_value = value.data
            else:
                has_value = value.as_dict()
            if not has_value or not target_object.connected():
                new_value = target_object.request('get-'+property_name)
                if 'code' not in new_value:
                    if property_type == 'array':
                        value.__init__(new_value)
                    else:
                        value.__init__(**new_value)

            if property_type == 'array':
                return value.data
            return value

        return function

    endpoint_name = os.path.basename(property_name)
    snake_case_name = underscorize(endpoint_name)
    setattr(type(target_object), snake_case_name,
            property(fget=getter_builder(member, property_name),
                     doc='Access to the {0} property'.format(endpoint_name)))
