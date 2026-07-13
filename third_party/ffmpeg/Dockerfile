FROM debian:bookworm-slim

ARG FFMPEG_COMMIT=a09be9b91e8e1219f297586873b0d7322b47df96
ARG NV_CODEC_HEADERS_COMMIT=15ee32753c92faddbabbff11676779618fc6db7e

RUN apt-get update && apt-get install -y --no-install-recommends \
        binutils-mingw-w64-x86-64 \
        ca-certificates \
        diffutils \
        gcc \
        gcc-mingw-w64-x86-64 \
        git \
        libc6-dev \
        make \
        nasm \
        patch \
        perl \
        pkg-config \
        xz-utils \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
RUN git init ffmpeg \
    && git -C ffmpeg remote add origin https://github.com/FFmpeg/FFmpeg.git \
    && git -C ffmpeg fetch --depth 1 origin "${FFMPEG_COMMIT}" \
    && git -C ffmpeg checkout --detach FETCH_HEAD \
    && test "$(git -C ffmpeg rev-parse HEAD)" = "${FFMPEG_COMMIT}"

RUN git init nv-codec-headers \
    && git -C nv-codec-headers remote add origin https://github.com/FFmpeg/nv-codec-headers.git \
    && git -C nv-codec-headers fetch --depth 1 origin "${NV_CODEC_HEADERS_COMMIT}" \
    && git -C nv-codec-headers checkout --detach FETCH_HEAD \
    && test "$(git -C nv-codec-headers rev-parse HEAD)" = "${NV_CODEC_HEADERS_COMMIT}" \
    && make -C nv-codec-headers PREFIX=/opt/nvcodec install

ENV PKG_CONFIG_PATH=/opt/nvcodec/lib/pkgconfig
WORKDIR /src/ffmpeg

# Keep the FFmpeg distribution surface small and reproducible: only FFmpeg's
# built-in LGPL code plus the MIT-licensed nv-codec-headers are enabled.
# No GPL, nonfree, or optional third-party codec libraries are linked.
RUN ./configure \
        --prefix=/opt/ffmpeg \
        --arch=x86_64 \
        --target-os=mingw32 \
        --cross-prefix=x86_64-w64-mingw32- \
        --pkg-config=pkg-config \
        --pkg-config-flags=--static \
        --extra-cflags=-I/opt/nvcodec/include \
        --enable-shared \
        --disable-static \
        --disable-debug \
        --disable-doc \
      --disable-programs \
      --disable-network \
      --enable-protocol=file \
      --disable-autodetect \
        --disable-pthreads \
        --enable-w32threads \
        --enable-ffnvcodec \
        --enable-nvenc \
        --enable-nvdec \
        --enable-cuvid \
        --enable-d3d11va \
        --enable-dxva2 \
        --extra-version=rtxvc-minimal \
    && make -j"$(nproc)" \
    && make install

COPY NV_CODEC_HEADERS_LICENSE.txt /tmp/NV_CODEC_HEADERS_LICENSE.txt

RUN mkdir -p /export/runtime /export/development /export/sources \
    && cp -a /opt/ffmpeg/bin/*.dll /export/runtime/ \
    && cp -a /opt/ffmpeg/include /opt/ffmpeg/lib /export/development/ \
    && cp COPYING.LGPLv2.1 /export/runtime/FFMPEG_LICENSE_LGPLv2.1.txt \
    && cp /tmp/NV_CODEC_HEADERS_LICENSE.txt /export/runtime/NV_CODEC_HEADERS_LICENSE.txt \
    && git archive --format=tar --prefix="FFmpeg-${FFMPEG_COMMIT}/" HEAD \
       | xz -T0 -9 > "/export/sources/FFmpeg-${FFMPEG_COMMIT}-source.tar.xz" \
    && git -C /src/nv-codec-headers archive --format=tar \
       --prefix="nv-codec-headers-${NV_CODEC_HEADERS_COMMIT}/" HEAD \
       | xz -T0 -9 > "/export/sources/nv-codec-headers-${NV_CODEC_HEADERS_COMMIT}-source.tar.xz" \
    && cp ffbuild/config.log /export/sources/ffmpeg-config.log

COPY README.md /export/sources/BUILD-AND-SOURCE-NOTICE.md
