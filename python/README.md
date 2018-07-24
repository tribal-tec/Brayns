# Brayns

The brayns package provides a python API to the brayns application.

## Documentation

Brayns documentation is built and hosted on [readthedocs](https://readthedocs.org/).

* [latest snapshot](http://brayns.readthedocs.org/en/latest/)
* [latest release](http://brayns.readthedocs.org/en/stable/)

## Installation

It is recommended that you use [`pip`](https://pip.pypa.io/en/stable/) to install
`Brayns` into a [`virtualenv`](https://virtualenv.pypa.io/en/stable/). The following
assumes a `virtualenv` named `venv` has been set up and
activated. We will see three ways to install `Brayns`


### 1. From the Python Package Index

```
(venv)$ pip install brayns
```

### 2. From git repository

```
(venv)$ pip install git+https://github.com/BlueBrain/Brayns.git#subdirectory=python
```

### 3. From source

Clone the repository and install it:

```
(venv)$ git clone https://github.com/BlueBrain/Brayns.git
(venv)$ pip install -e ./Brayns/python
```

This installs `Brayns` into your `virtualenv` in "editable" mode. That means changes
made to the source code are seen by the installation. To install in read-only mode, omit
the `-e`.

## Get started

Simple connect:

```python
>>> from brayns import Client

>>> brayns = brayns.Client('localhost:8200')
>>> print(brayns)
Brayns version 0.7.0.c52dd4b running on http://localhost:8200/
```
