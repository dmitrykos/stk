FROM ubuntu:18.04

# Use minimal possible dependencies (https://ubuntu.com/blog/we-reduced-our-docker-images-by-60-with-no-install-recommends)
RUN apt-get update && apt-get install --no-install-recommends --yes\
    wget xorg

# Download QEMU and unpack
RUN wget https://github.com/xpack-dev-tools/qemu-arm-xpack/releases/download/v7.1.0-1/xpack-qemu-arm-7.1.0-1-linux-x64.tar.gz &&\
    tar xvf xpack-qemu-arm-7.1.0-1-linux-x64.tar.gz

# Cleanup
RUN rm -f xpack-qemu-arm-7.1.0-1-linux-x64.tar.gz &&\
    apt-get clean &&\
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

ENV PATH="${PATH}:xpack-qemu-arm-7.1.0-1/bin"
