# Docker container for running Brayns as a service
# Check https://docs.docker.com/engine/userguide/eng-image/dockerfile_best-practices/#user for best practices.

# This Dockerfile leverages multi-stage builds, available since Docker 17.05
# See: https://docs.docker.com/engine/userguide/eng-image/multistage-build/#use-multi-stage-builds

# Image where Brayns is built
FROM alpine:3.7 as builder
LABEL maintainer="bbp-svc-viz@groupes.epfl.ch"
ARG DIST_PATH=/app/dist

# Install packages
RUN echo "http://dl-2.alpinelinux.org/alpine/edge/main" > /etc/apk/repositories && \
    echo "http://dl-2.alpinelinux.org/alpine/edge/community" >> /etc/apk/repositories && \
    echo "http://dl-2.alpinelinux.org/alpine/edge/testing" >> /etc/apk/repositories && \
    apk update && \
    apk add --no-cache \
    build-base \
    cmake \
    git \
    ninja \
    boost-dev \
    hdf5-dev \
    imagemagick-dev \
    libtbb-dev \
    libjpeg-turbo-dev \
    libtbb \
    wget \
    openssl-dev \
    libexecinfo \
    libexecinfo-dev \
    ca-certificates \
 && rm -rf /var/cache/apk/* /tmp/* /var/tmp/*

# Get ISPC
# https://ispc.github.io/downloads.html
ARG ISPC_VERSION=1.9.2
ARG ISPC_DIR=ispc-v${ISPC_VERSION}-linux
ARG ISPC_PATH=/app/$ISPC_DIR

RUN mkdir -p ${ISPC_PATH} \
 && wget http://netix.dl.sourceforge.net/project/ispcmirror/v${ISPC_VERSION}/${ISPC_DIR}.tar.gz \
 && tar zxvf ${ISPC_DIR}.tar.gz -C ${ISPC_PATH} --strip-components=1 \
 && rm -rf ${ISPC_PATH}/${ISPC_DIR}/examples

# Add ispc bin to the PATH
ENV PATH $PATH:${ISPC_PATH}

# Install Embree
# https://github.com/embree/embree
ARG EMBREE_VERSION=2.17.1
ARG EMBREE_SRC=/app/embree

RUN mkdir -p ${EMBREE_SRC} \
 && git clone https://github.com/embree/embree.git ${EMBREE_SRC} \
 && cd ${EMBREE_SRC} \
 && git checkout v${EMBREE_VERSION} \
 && mkdir -p build \
 && cd build \
 && CMAKE_PREFIX_PATH=${DIST_PATH} cmake .. -GNinja \
    -DEMBREE_TUTORIALS=OFF \
    -DCMAKE_INSTALL_PREFIX=${DIST_PATH} \
&& ninja install

# Install OSPRay
# https://github.com/ospray/ospray/releases
ARG OSPRAY_VERSION=1.4.3
ARG OSPRAY_SRC=/app/ospray

RUN mkdir -p ${OSPRAY_SRC} \
 && git clone https://github.com/ospray/ospray.git ${OSPRAY_SRC} \
 && cd ${OSPRAY_SRC} \
 && git checkout v${OSPRAY_VERSION} \
 && mkdir -p build \
 && cd build \
 && CMAKE_PREFIX_PATH=${DIST_PATH} cmake .. -GNinja \
    -DOSPRAY_ENABLE_APPS=OFF \
    -DCMAKE_INSTALL_PREFIX=${DIST_PATH} \
 && ninja install
 
# Install assimp
# https://github.com/assimp/assimp
ARG ASSIMP_VERSION=4.1.0
ARG ASSIMP_SRC=/app/assimp

RUN mkdir -p ${ASSIMP_SRC} \
 && git clone https://github.com/assimp/assimp.git ${ASSIMP_SRC} \
 && cd ${ASSIMP_SRC} \
 && git checkout v${ASSIMP_VERSION} \
 && mkdir -p build \
 && cd build \
 && cmake .. -GNinja \
    -DASSIMP_BUILD_ASSIMP_TOOLS=OFF \
    -DASSIMP_BUILD_TESTS=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=${DIST_PATH} \
&& ninja install

# Install libwebsockets (2.0 from Debian is not reliable)
# https://github.com/warmcat/libwebsockets/releases
ARG LWS_VERSION=2.3.0
ARG LWS_SRC=/app/libwebsockets
ARG LWS_FILE=v${LWS_VERSION}.tar.gz

RUN mkdir -p ${LWS_SRC} \
 && wget https://github.com/warmcat/libwebsockets/archive/${LWS_FILE} \
 && tar zxvf ${LWS_FILE} -C ${LWS_SRC} --strip-components=1 \
 && cd ${LWS_SRC} \
 && mkdir -p build \
 && cd build \
 && cmake .. -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLWS_STATIC_PIC=ON \
    -DLWS_WITH_SSL=OFF \
    -DLWS_WITH_ZLIB=OFF \
    -DLWS_WITH_ZIP_FOPS=OFF \
    -DLWS_WITHOUT_EXTENSIONS=ON \
    -DLWS_WITHOUT_TESTAPPS=ON \
    -DCMAKE_INSTALL_PREFIX=${DIST_PATH} \
 && ninja install \
 && rm -rf ${DIST_PATH}/lib/cmake/libwebsockets


# Set working dir and copy Brayns assets
ARG BRAYNS_SRC=/app/brayns
WORKDIR /app
ADD . ${BRAYNS_SRC}

# Install Brayns
# https://github.com/BlueBrain/Brayns
RUN cksum ${BRAYNS_SRC}/.gitsubprojects \
 && cd ${BRAYNS_SRC} \
 && git submodule update --init --recursive --remote \
 && mkdir -p build \
 && cd build \
 && PKG_CONFIG_PATH=${DIST_PATH}/lib/pkgconfig CMAKE_PREFIX_PATH=${DIST_PATH} cmake .. -GNinja \
    -DBRAYNS_BRION_ENABLED=ON \
    -DBRAYNS_NETWORKING_ENABLED=ON \
    -DCLONE_SUBPROJECTS=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=${DIST_PATH} \
    -DBUILD_PYTHON_BINDINGS=OFF \
    -DCOMMON_LIBRARY_TYPE=SHARED

RUN cd ${BRAYNS_SRC}/build && ninja mvd-tool Brayns-install \
 && rm -rf ${DIST_PATH}/include ${DIST_PATH}/share

# Final image, containing only Brayns and libraries required to run it
FROM alpine:3.7
ARG DIST_PATH=/app/dist

RUN echo "http://dl-2.alpinelinux.org/alpine/edge/main" > /etc/apk/repositories && \
    echo "http://dl-2.alpinelinux.org/alpine/edge/community" >> /etc/apk/repositories && \
    echo "http://dl-2.alpinelinux.org/alpine/edge/testing" >> /etc/apk/repositories && \
    apk update && \
    apk add --no-cache \
    boost-filesystem \
    boost-program_options \
    boost-regex \
    boost-serialization \
    boost-system \
    boost-iostreams \
    libgomp \
    hdf5 \
    imagemagick-c++ \
    libjpeg-turbo \
    libtbb \
    libexecinfo \
 && rm -rf /var/cache/apk/* /tmp/* /var/tmp/*

# The COPY command below will:
# 1. create a container based on the `builder` image (but do not start it)
#    Equivalent to the `docker create` command
# 2. create a new image layer containing the
#    /app/dist directory of this new container
#    Equivalent to the `docker copy` command.
COPY --from=builder ${DIST_PATH} ${DIST_PATH}

# Add binaries from dist to the PATH
ENV LD_LIBRARY_PATH $LD_LIBRARY_PATH:${DIST_PATH}/lib
ENV LD_LIBRARY_PATH $LD_LIBRARY_PATH:${DIST_PATH}/lib64
ENV PATH ${DIST_PATH}/bin:$PATH

# Expose a port from the container
# For more ports, use the `--expose` flag when running the container,
# see https://docs.docker.com/engine/reference/run/#expose-incoming-ports for docs.
EXPOSE 8200

# When running `docker run -ti --rm -p 8200:8200 brayns`,
# this will be the cmd that will be executed (+ the CLI options from CMD).
# To ssh into the container (or override the default entry) use:
# `docker run -ti --rm --entrypoint bash -p 8200:8200 brayns`
# See https://docs.docker.com/engine/reference/run/#entrypoint-default-command-to-execute-at-runtime
# for more docs
ENTRYPOINT ["braynsService"]
CMD ["--http-server", ":8200"]
