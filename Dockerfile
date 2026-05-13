FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
        nasm gcc gcc-multilib binutils make dosfstools \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /os

CMD ["make", "all"]
