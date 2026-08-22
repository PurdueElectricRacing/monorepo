FROM ubuntu:26.04

# refresh apt
RUN apt update

# install deps
RUN apt -y install --no-install-recommends \
    ca-certificates \
    clang-format \
    cmake \
    cppcheck \
    curl \
    g++ \
    gcc \
    gcc-arm-none-eabi \
    git \
    lcov \
    libgtest-dev \
    libnewlib-arm-none-eabi \
    libssl-dev \
    libudev-dev \
    libxcb-render0-dev \
    libxcb-shape0-dev \
    libxcb-xfixes0-dev \
    libxkbcommon-dev \
    ninja-build \
    pkg-config

# setup python environment
RUN apt -y install --no-install-recommends \
    python3 \
    python3-venv

ENV VIRTUAL_ENV=/opt/venv
RUN python3 -m venv $VIRTUAL_ENV
ENV PATH="$VIRTUAL_ENV/bin:$PATH"

COPY ./requirements.txt /usr/requirements.txt
RUN python3 -m pip install --requirement /usr/requirements.txt

# setup Rust environment
ENV RUSTUP_HOME=/opt/rustup
ENV CARGO_HOME=/opt/cargo
ENV PATH="$CARGO_HOME/bin:$PATH"

RUN curl --proto '=https' --tlsv1.2 --fail --silent --show-error \
        https://sh.rustup.rs \
        --output /tmp/rustup-init.sh \
    && sh /tmp/rustup-init.sh \
        -y \
        --no-modify-path \
        --profile minimal \
        --default-toolchain stable \
    && rustup component add rustfmt \
    && rm /tmp/rustup-init.sh
