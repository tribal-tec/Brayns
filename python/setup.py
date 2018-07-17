#!/usr/bin/env python
# -*- coding: utf-8 -*-
# pylint: disable=R0801,W0122,E0602

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

"""setup.py"""
import os

from setuptools import setup
try: # for pip >= 10
    from pip._internal.req import parse_requirements
    from pip._internal.download import PipSession
except ImportError: # for pip <= 9.0.3
    from pip.req import parse_requirements
    from pip.download import PipSession
from optparse import Option

BASEDIR = os.path.dirname(os.path.abspath(__file__))


def parse_reqs(reqs_file):
    ''' parse the requirements '''
    options = Option("--workaround")
    options.skip_requirements_regex = None
    options.isolated_mode = True
    install_reqs = parse_requirements(reqs_file, options=options, session=PipSession())
    return [str(ir.req) for ir in install_reqs]


REQS = parse_reqs(os.path.join(BASEDIR, "requirements.txt"))

exec(open('brayns/version.py').read())
setup(name="brayns",
      version=VERSION,
      description="Brayns python API",
      long_description="The Brayns python package gives full access to the websocket API of the "
      "Brayns application.",
      packages=['brayns'],
      url='https://github.com/BlueBrain/Brayns.git',
      author='Blue Brain Project Visualization team',
      author_email='bbp-open-source@googlegroups.com',
      license='GNU LGPL',
      install_requires=REQS)
