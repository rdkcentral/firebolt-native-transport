FROM ubuntu:22.04

ARG SYSROOT_PATH=/opt/sysroot

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    clang-format \
    libboost-all-dev \
    curl \
    wget \
    pkg-config \
    && apt-get autoremove -y && apt-get clean -y && rm -rf /var/lib/apt/lists/*

RUN mkdir -p ${SYSROOT_PATH} /deps

ARG DEPS_GOOGLETEST_V="1.15.2"
RUN cd /deps \
    && dir="googletest-${DEPS_GOOGLETEST_V}" \
    && echo "cmd: curl -sL https://github.com/google/googletest/releases/download/v${DEPS_GOOGLETEST_V}/$dir.tar.gz | tar xzf -" \
    && curl -sL https://github.com/google/googletest/releases/download/v${DEPS_GOOGLETEST_V}/$dir.tar.gz | tar xzf - \
    && cmake -B "build/$dir" \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX:PATH="$SYSROOT_PATH/usr" \
          -DBUILD_SHARED_LIBS=ON \
          "$dir" \
    && cmake --build "build/$dir" --target install \
    && rm -rf "build/$dir" "$dir"

ARG DEPS_NLOHMANN_JSON_V="3.11.3"
RUN cd /deps \
    && dir="nlohmann-json-${DEPS_NLOHMANN_JSON_V}" \
    && git clone --depth 1 --branch "v${DEPS_NLOHMANN_JSON_V}" "https://github.com/nlohmann/json" "$dir" \
    && cmake -B "build/$dir" \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX:PATH="$SYSROOT_PATH/usr" \
          -DBUILD_SHARED_LIBS=ON \
          "$dir" \
    && cmake --build "build/$dir" --target install \
    && rm -rf "build/$dir" "$dir"

ARG DEPS_JSON_SCHEMA_VALIDATOR_V="2.3.0"
RUN cd /deps \
    && dir="json-schema-validator-${DEPS_JSON_SCHEMA_VALIDATOR_V}" \
    && curl -sL https://github.com/pboettch/json-schema-validator/archive/refs/tags/${DEPS_JSON_SCHEMA_VALIDATOR_V}.tar.gz | tar xzf - \
    && cmake -B "build/$dir" \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX:PATH="$SYSROOT_PATH/usr" \
          -DBUILD_SHARED_LIBS=ON \
          "$dir" \
    && cmake --build "build/$dir" --target install \
    && rm -rf "build/$dir" "$dir"

ARG DEPS_WEBSOCKETPP_V="0.8.2"
RUN cd /deps \
    && dir="websocketpp-${DEPS_WEBSOCKETPP_V}" \
    && curl -sL https://github.com/zaphoyd/websocketpp/archive/refs/tags/${DEPS_WEBSOCKETPP_V}.tar.gz | tar xzf - \
    && cmake -B "build/$dir" \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX:PATH="$SYSROOT_PATH/usr" \
          -DBUILD_SHARED_LIBS=ON \
          "$dir" \
    && cmake --build "build/$dir" --target install \
    && rm -rf "build/$dir" "$dir"

ENV SYSROOT_PATH=${SYSROOT_PATH}

WORKDIR /workspace

CMD ["/bin/bash"]
