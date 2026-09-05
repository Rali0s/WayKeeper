FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates debootstrap dosfstools e2fsprogs gdisk mount parted \
        qemu-user-static rsync util-linux xz-utils \
    && apt-get clean \
    && find /var/lib/apt/lists -mindepth 1 -delete

RUN apt-get update \
    && apt-get install -y --no-install-recommends proot \
    && apt-get clean \
    && find /var/lib/apt/lists -mindepth 1 -delete
