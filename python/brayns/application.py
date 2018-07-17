#!/usr/bin/env python
# -*- coding: utf-8 -*-
# pylint: disable=E1101,W0122
#
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

import json
import os
import threading
import python_jsonschema_objects as pjs
import websocket
import inflection

from .utils import HTTP_METHOD_GET, HTTP_METHOD_PUT, HTTP_STATUS_OK, \
    http_request, set_ws_protocol, WS_PATH


def camelcase_to_snake_case(name):
    """
    Convert CamelCase to snake_case
    :param name: CamelCase to convert
    :return: converted snake_case
    """
    import re
    s1 = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
    return re.sub('([a-z0-9])([A-Z])', r'\1_\2', s1).lower()


def _handle_param_object(param, method, description):
    """
    Create types from oneOf parameter of RPC and adds the RPC function itself.
    :param param: the parameter object
    :param method: name of RPC
    :param description: description of RPC
    :return:
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


def _add_enums(value, target):
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
            enum_class = str(camelcase_to_snake_case(enum['title'])).upper()
        enum_class += "_"
        for val in enum['enum']:
            enum_value = enum_class + inflection.parameterize(val, '_').upper()
            setattr(target, enum_value, val)


class Application(object):
    """
    The application class exposes a dynamic generated API from HTTP/websockets server exposed
    registry.
    """

    def __init__(self, url):
        """
        Create a new application instance by connecting to the given resource
        :param url: a string 'hostname:port' to connect to a running brayns instance
        """

        self._url = url + '/'

        if not self._check_version():
            raise Exception('Minimal version check failed')

        self._registry, ret_code = self._obtain_registry()
        if ret_code != HTTP_STATUS_OK:
            raise Exception('Failed to obtain registry from application')

        self._create_all_properties()

        self._ws = None
        self._ws_connected = False
        self._request_id = 0
        self._update_callback = {}
        self._ws_requests = {}
        self._setup_websocket()

    def url(self):
        """
        :return: The url of the rendering resource
        """
        return self._url

    def __str__(self):
        if self.version:
            version = '.'.join(str(x) for x in [self.version.major, self.version.minor,
                                                self.version.patch, self.version.revision])
        else:
            version = 'unknown'
        return "Application version {0} running on {1}".format(version, self.url())

    def rpc_request(self, method, params=None, response_timeout=5):
        """
        Invoke a RPC on the application.
        :param method: name of the method to invoke
        :param params: params for the method
        :param response_timeout: number of seconds to wait for the response
        :return: result or error of RPC
        """
        data = dict()
        data['jsonrpc'] = "2.0"
        data['id'] = self._request_id
        data['method'] = method
        if params:
            data['params'] = params

        result = {'done': False, 'result': None}

        def callback(payload):
            """
            The callback for the reply
            :param payload: the actual reply data
            """
            result['result'] = payload
            result['done'] = True

        self._ws_requests[self._request_id] = callback
        self._request_id += 1

        self._setup_websocket()
        self._ws.send(json.dumps(data))

        import time
        if response_timeout:
            timeout = response_timeout * 10

            while not result['done'] and timeout:
                time.sleep(0.1)
                timeout -= 1

            if 'done' not in result:
                raise Exception('Request was not answered within {0} seconds'
                                .format(response_timeout))
        else:
            while not result['done']:
                time.sleep(0.0001)

        return result['result']

    def rpc_notify(self, method, params=None):
        """
        Invoke a RPC on the application without waiting for a response.
        :param method: name of the method to invoke
        :param params: params for the method
        """
        data = dict()
        data['jsonrpc'] = "2.0"
        data['method'] = method
        if params:
            data['params'] = params

        self._setup_websocket()
        self._ws.send(json.dumps(data))

    def _check_version(self):
        """
        Check if the application version is sufficient enough.
        :return True if minimal version matches expectation, False otherwise
        """

        status = http_request(HTTP_METHOD_GET, self._url, 'version')
        if status.code != HTTP_STATUS_OK:
            print('Cannot obtain version from application')
            return False

        minimal_version = '0.5.0'
        version = '.'.join(str(x) for x in list(status.contents.values())[:3])

        import semver
        if semver.match(version, '<{0}'.format(minimal_version)):
            print('Application does not satisfy minimal required version; '
                  'needed {0}, got {1}'.format(minimal_version, version))
            return False
        return True

    def _create_all_properties(self):
        """
        Add all exposed objects and types from the application as properties to the Application.
        """

        for object_name in self._registry.keys():
            if self._handle_rpc(object_name):
                continue

            schema, ret_code = self._schema(object_name)
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
            success, value = self._create_member(class_type)
            if not success:
                continue

            is_object = schema['type'] == 'object'
            if is_object:
                self._add_enums(value, self)
                self._add_commit(class_type, object_name)

            # add member to Application
            member = '_' + os.path.basename(object_name).replace('-', '_')
            setattr(self, member, value)
            self._add_property(member, object_name, schema['type'])

    def _handle_rpc(self, object_name):
        """
        Try to handle object_name as RPC
        :param object_name: registry endpoint, e.g. v1/foo[/schema]
        :return: True if object_name was RPC, False otherwise
        """
        if not object_name.endswith('/schema'):
            return False

        method = object_name[:-len('/schema')]
        status = http_request(HTTP_METHOD_GET, self._url, method)
        schema, ret_code = self._schema(method)
        if status.code != HTTP_STATUS_OK and ret_code == HTTP_STATUS_OK:
            self._add_rpc(schema)
            return True
        return False

    def _add_rpc(self, schema):
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
                code = self._handle_param_oneof(params['oneOf'], method, description)
            # in the absence of multiple parameters support, create a function with multiple
            # parameters from object properties
            elif params['type'] == 'object':
                code = _handle_param_object(params, method, description)
            else:
                code = '''
                    def function(self, {1}, response_timeout=5):
                        """
                        {0}
                        {2}
                        """
                        return self.rpc_request("{3}", params={1},
                                                response_timeout=response_timeout)
                    '''.format(description, params['name'],
                               ":param {0}: {1}".format(params['name'], params['description']),
                               method)
        else:
            code = '''
                def function(self, response_timeout=5):
                    """
                    {0}
                    """
                    return self.rpc_request("{1}", response_timeout=response_timeout)
                '''.format(description, method)

        if code is None:
            return
        try:
            d = {}
            exec(code.strip(), d)
            function = d['function']
            function.__name__ = func_name
            setattr(self.__class__, function.__name__, function)
        except ValueError as e:
            print(e)

    def _handle_param_oneof(self, param, method, description):
        """
        Create types from oneOf parameter of RPC and adds the RPC function itself.
        :param param: the oneOf array
        :param method: name of RPC
        :param description: description of RPC
        :return:
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
            setattr(self, pretty_class_name, class_type)
            param_types.append(pretty_class_name)

            # create and add potential enums to type
            self._add_enums(class_type(), class_type)

        return '''
                            def function(self, params, response_timeout=5):
                                """
                                {0}
                                ":param: one of the params for the active type: {1}
                                """
                                return self.rpc_request("{2}", params.for_json(),
                                                        response_timeout=response_timeout)
                            '''.format(description, ', '.join(param_types), method)

    def _create_member(self, class_type, object_name=None, object_type=None):
        """
        Create a new object from the given class type and initialize it from the application state.
        :param class_type: class type of the new object
        :param object_name: name of the new object
        :param object_type: type as string of the new object
        :return: tuple(success, object)
        """

        # initialize object from application state with GET if applicable
        if object_name and HTTP_METHOD_GET in self._registry[object_name]:
            status = http_request(HTTP_METHOD_GET, self._url, object_name)
            if status.code != HTTP_STATUS_OK:
                print('Error getting data for {0}: {1}'.format(object_name, status.code))
                return False, None
            if object_type in ['array', 'object']:
                value = class_type.from_json(json.dumps(status.contents))
            else:
                value = class_type(status.contents)
        else:
            value = class_type()
        return True, value

    def _add_commit(self, class_type, object_name):
        """ Add commit() for given property """

        if HTTP_METHOD_PUT in self._registry[object_name]:
            def commit_builder(url):
                """ Wrapper for returning the property.commit() function """

                def commit(prop):
                    """ Update the property in the application """

                    return self.rpc_request('set-' + os.path.basename(url), prop.as_dict())

                return commit

            setattr(class_type, 'commit', commit_builder(object_name))

    def _add_property(self, member, object_name, property_type):
        """ Add property to Application for object """

        def getter_builder(member, object_name):
            """ Wrapper for returning the property state """

            def function(self):
                """ Returns the current state for the property """
                value = getattr(self, member)

                # Initialize on first access; updates are received via websocket
                if not value.as_dict():
                    status = http_request(HTTP_METHOD_GET, self._url, object_name)
                    if status.code == HTTP_STATUS_OK:
                        value.__init__(**status.contents)

                if property_type == 'array':
                    return value.data
                return value

            return function

        def setter_builder(member, object_name):
            """ Wrapper for updating the property state """

            def function(self, prop):
                """ Update the current state of the property locally and in the application """
                if property_type == 'object':
                    setattr(self, member, prop)
                    http_request(HTTP_METHOD_PUT, self._url, object_name, prop.serialize())
                    return

                if property_type == 'array':
                    value = getattr(self, member)
                    value.data = prop
                else:
                    setattr(self, member, prop)
                    http_request(HTTP_METHOD_PUT, self._url, object_name, json.dumps(prop))

            return function if HTTP_METHOD_PUT in self._registry[object_name] else None

        endpoint_name = os.path.basename(object_name)
        snake_case_name = endpoint_name.replace('-', '_')
        setattr(type(self), snake_case_name,
                property(fget=getter_builder(member, object_name),
                         fset=setter_builder(member, object_name),
                         doc='Access to the {0} property'.format(endpoint_name)))

    def _obtain_registry(self):
        """ Returns the registry of PUT and GET objects of the application """
        status = http_request(HTTP_METHOD_GET, self._url, 'registry')
        return status.contents, status.code

    def _schema(self, object_name):
        """ Returns the JSON schema for the given object """
        status = http_request(HTTP_METHOD_GET, self._url, object_name + '/schema')
        return status.contents, status.code

    def _setup_websocket(self):
        """
        Setups websocket with handling for binary (image) and text (all properties) messages. The
        websocket app runs in a separate thread to unblock all notebook cells.
        """

        if self._ws_connected:
            return

        def on_open(ws):
            # pylint: disable=unused-argument
            """ Websocket is open, remember this state """
            self._ws_connected = True

        def on_data(ws, data, data_type, cont):
            # pylint: disable=unused-argument
            """ Websocket received data, handle it """
            if data_type == websocket.ABNF.OPCODE_TEXT:
                data = json.loads(data)

                if self._handle_reply(data):
                    return

                prop = getattr(self, '_' + data['method'].replace('-', '_'), None)
                if prop is None:
                    return

                prop.__init__(**data['params'])
                if data['method'] in self._update_callback:
                    self._update_callback[data['method']](data=prop)
            elif data_type == websocket.ABNF.OPCODE_BINARY:
                if 'image-jpeg' in self._update_callback:
                    self._update_callback['image-jpeg'](data=data)

        def on_close(ws):
            # pylint: disable=unused-argument
            """ Websocket is closing, notify all registered callbacks to e.g. close widgets """
            self._ws_connected = False
            for f in self._update_callback.values():
                f(close=True)
            self._update_callback.clear()

        websocket.enableTrace(False)

        self._ws = websocket.WebSocketApp(set_ws_protocol(self._url) + WS_PATH,
                                          subprotocols=['rockets'], on_open=on_open,
                                          on_data=on_data, on_close=on_close)
        ws_thread = threading.Thread(target=self._ws.run_forever)
        ws_thread.daemon = True
        ws_thread.start()

        import time
        conn_timeout = 5
        while not self._ws_connected and conn_timeout:
            time.sleep(0.2)
            conn_timeout -= 1

    def _handle_reply(self, data):
        """
        Handle JSON RPC reply
        :param data: data of the reply
        :return True if a request was handled, False otherwise
        """
        if 'id' not in data:
            return False

        payload = None
        if 'result' in data:
            payload = None if data['result'] == '' or data['result'] == 'OK' or data['result']\
                           else data['result']
        elif 'error' in data:
            payload = data['error']

        if data['id'] in self._ws_requests:
            self._ws_requests[data['id']](payload)
            self._ws_requests.pop(data['id'])
        else:
            print('Invalid reply for request ' + str(data['id']))
        return True
