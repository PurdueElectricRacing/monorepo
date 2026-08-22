FROM ubuntu:26.04

# refresh apt
RUN apt update

# install deps
RUN apt -y install --no-install-recommends \
    clang-format \
    cmake \
    cppcheck \
    g++ \
    gcc \
    gcc-arm-none-eabi \
    git \
    lcov \
    libgtest-dev \
    libnewlib-arm-none-eabi \
    ninja-build

# setup python environment
RUN apt -y install --no-install-recommends \
    python3 \
    python3-venv

ENV VIRTUAL_ENV=/opt/venv
RUN python3 -m venv $VIRTUAL_ENV
ENV PATH="$VIRTUAL_ENV/bin:$PATH"

COPY ./requirements.txt /usr/requirements.txt
RUN python3 -m pip install --requirement /usr/requirements.txt
