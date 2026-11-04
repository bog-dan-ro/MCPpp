# Copyright (c) 2026 BogDan Vatra
# SPDX-License-Identifier: MIT

# Pin the base image and the tested reflection compiler package.
FROM debian:testing-slim@sha256:74c29a19a857bacc7d54bf3e4f2f7a378e6af21886d14af55709d522b2d31224
ARG GCC_PACKAGE_VERSION=16.2.0-3
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       ca-certificates git cmake ninja-build "g++-16=${GCC_PACKAGE_VERSION}" \
       libboost1.92-dev libboost-json1.92-dev libboost-test1.92-dev \
       nlohmann-json3-dev libssl-dev openssl python3 python3-venv nodejs npm \
    && rm -rf /var/lib/apt/lists/*

# Boost 1.92's aggregate Asio/Beast CMake targets declare these dependencies
# even though MCP++ itself does not instantiate spawn/regex/date-time APIs.
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       libboost-date-time1.92-dev libboost-context1.92-dev libboost-regex1.92-dev \
    && rm -rf /var/lib/apt/lists/*

COPY tools/verification/package.json tools/verification/package-lock.json tools/verification/requirements.txt /opt/mcpp-verification/
RUN python3 -m venv /opt/mcpp-verification/venv \
    && /opt/mcpp-verification/venv/bin/python -m pip install --no-cache-dir -r /opt/mcpp-verification/requirements.txt \
    && npm ci --prefix /opt/mcpp-verification --ignore-scripts --no-audit --no-fund \
    && npm cache clean --force
ENV PATH="/opt/mcpp-verification/venv/bin:/opt/mcpp-verification/node_modules/.bin:${PATH}"
WORKDIR /workspace
CMD ["cmake", "-P", "tools/ci/run.cmake"]
