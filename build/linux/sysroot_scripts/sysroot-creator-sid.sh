#!/bin/bash
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

DISTRO=debian
DIST=sid

# Keep the "experimental" repo before the "sid" repo.  There are some packages
# that are currently only available in experimental like libgtk-4-0, but if it
# were to be placed first, experimental (unreleased) versions of other packages
# like libc6 would take precedence over the sid (released) versions.  While this
# may be useful for certain kinds of development, the standard sysroots should
# continue to be shipped only with released packages.
ARCHIVE_URL="https://snapshot.debian.org/archive/debian"
ARCHIVE_TIMESTAMP=20181214T150526Z
APT_SOURCES_LIST="${ARCHIVE_URL}/${ARCHIVE_TIMESTAMP}/ experimental main
${ARCHIVE_URL}/${ARCHIVE_TIMESTAMP}/ sid main"

# gpg keyring file generated using:
#   export KEYS="518E17E1 46925553 2B90D010 C857C906 F66AEC98 8AE22BA9 1A7B6500"
#   gpg --recv-keys $KEYS
#   gpg --output ./debian-archive-sid-stable.gpg --export $KEYS
KEYRING_FILE="${SCRIPT_DIR}/debian-archive-sid-stable.gpg"

HAS_ARCH_AMD64=1
HAS_ARCH_I386=1
HAS_ARCH_ARM=1
HAS_ARCH_ARM64=1
HAS_ARCH_ARMEL=1
HAS_ARCH_MIPS=1
HAS_ARCH_MIPS64EL=1

# Sysroot packages: these are the packages needed to build chrome.
# NOTE: When DEBIAN_PACKAGES is modified, the packagelist files must be updated
# by running this script in GeneratePackageList mode.
DEBIAN_PACKAGES="\
  comerr-dev
  krb5-multidev
  libappindicator-dev
  libappindicator1
  libappindicator3-1
  libappindicator3-dev
  libasound2
  libasound2-dev
  libasyncns0
  libatk-bridge2.0-0
  libatk-bridge2.0-dev
  libatk1.0-0
  libatk1.0-dev
  libatomic1
  libatspi2.0-0
  libatspi2.0-dev
  libattr1
  libaudit1
  libavahi-client3
  libavahi-common3
  libblkid1
  libbluetooth-dev
  libbluetooth3
  libbrlapi-dev
  libbrlapi0.6
  libbsd0
  libc6
  libc6-dev
  libcairo-gobject2
  libcairo-script-interpreter2
  libcairo2
  libcairo2-dev
  libcap-dev
  libcap-ng0
  libcap2
  libcolord2
  libcom-err2
  libcups2
  libcups2-dev
  libcupsimage2
  libcupsimage2-dev
  libdatrie1
  libdb5.3
  libdbus-1-3
  libdbus-1-dev
  libdbus-glib-1-2
  libdbusmenu-glib-dev
  libdbusmenu-glib4
  libdbusmenu-gtk3-4
  libdbusmenu-gtk4
  libdrm-amdgpu1
  libdrm-dev
  libdrm-nouveau2
  libdrm-radeon1
  libdrm2
  libegl1
  libegl1-mesa
  libegl1-mesa-dev
  libelf-dev
  libelf1
  libepoxy-dev
  libepoxy0
  libevent-2.1-6
  libexpat1
  libexpat1-dev
  libffi-dev
  libffi6
  libffi7
  libflac-dev
  libflac8
  libfontconfig1
  libfontconfig1-dev
  libfreetype6
  libfreetype6-dev
  libfribidi-dev
  libfribidi0
  libgbm-dev
  libgbm1
  libgcc-6-dev
  libgcc1
  libgcrypt20
  libgcrypt20-dev
  libgdk-pixbuf2.0-0
  libgdk-pixbuf2.0-dev
  libgl1
  libgl1-mesa-dev
  libgl1-mesa-glx
  libglapi-mesa
  libgles1
  libgles2
  libglib2.0-0
  libglib2.0-dev
  libglvnd-dev
  libglvnd0
  libglx0
  libgmp10
  libgnome-keyring-dev
  libgnome-keyring0
  libgnutls-dane0
  libgnutls-openssl27
  libgnutls28-dev
  libgnutls30
  libgnutlsxx28
  libgomp1
  libgpg-error-dev
  libgpg-error0
  libgraphene-1.0-0
  libgraphene-1.0-dev
  libgraphite2-3
  libgraphite2-dev
  libgssapi-krb5-2
  libgssrpc4
  libgtk-3-0
  libgtk-3-dev
  libgtk-4-0
  libgtk-4-dev
  libgtk2.0-0
  libharfbuzz-dev
  libharfbuzz-gobject0
  libharfbuzz-icu0
  libharfbuzz0b
  libhogweed4
  libice6
  libicu-le-hb0
  libicu63
  libicu60
  libidl-2-0
  libidn11
  libidn2-0
  libindicator3-7
  libindicator7
  libjbig0
  libjpeg62-turbo
  libjpeg62-turbo-dev
  libjson-glib-1.0-0
  libjsoncpp-dev
  libjsoncpp1
  libk5crypto3
  libkadm5clnt-mit11
  libkadm5srv-mit11
  libkdb5-9
  libkeyutils1
  libkrb5-3
  libkrb5-dev
  libkrb5support0
  liblcms2-2
  libltdl7
  liblz4-1
  liblzma5
  liblzo2-2
  libminizip-dev
  libminizip1
  libmount1
  libnettle6
  libnspr4
  libnspr4-dev
  libnss-db
  libnss3
  libnss3-dev
  libogg-dev
  libogg0
  libopengl0
  libopus-dev
  libopus0
  liborbit-2-0
  liborbit2
  libp11-kit0
  libpam0g
  libpam0g-dev
  libpango-1.0-0
  libpango1.0-dev
  libpangocairo-1.0-0
  libpangoft2-1.0-0
  libpangox-1.0-0
  libpangoxft-1.0-0
  libpci-dev
  libpci3
  libpciaccess0
  libpcre16-3
  libpcre3
  libpcre3-dev
  libpcre32-3
  libpcrecpp0v5
  libpipewire-0.2-1
  libpipewire-0.2-dev
  libpixman-1-0
  libpixman-1-dev
  libpng-dev
  libpng16-16
  libpsl5
  libpthread-stubs0-dev
  libpulse-dev
  libpulse-mainloop-glib0
  libpulse0
  libre2-4
  libre2-dev
  librest-0.7-0
  libselinux1
  libsm6
  libsnappy-dev
  libsnappy1v5
  libsndfile1
  libsoup-gnome2.4-1
  libsoup2.4-1
  libspa-lib-0.1-dev
  libspeechd-dev
  libspeechd2
  libsqlite3-0
  libssl-dev
  libssl1.0.2
  libssl1.1
  libstdc++-6-dev
  libstdc++6
  libsystemd0
  libtasn1-6
  libthai0
  libtiff5
  libudev-dev
  libudev1
  libunbound8
  libunistring2
  libuuid1
  libva-dev
  libva-drm2
  libva-glx2
  libva-wayland2
  libva-x11-2
  libva2
  libvorbis0a
  libvorbisenc2
  libvpx-dev
  libvpx5
  libvulkan1
  libwayland-client0
  libwayland-cursor0
  libwayland-dev
  libwayland-egl1
  libwayland-egl1-mesa
  libwayland-server0
  libwebp-dev
  libwebp6
  libwebpdemux2
  libwebpmux3
  libwrap0
  libx11-6
  libx11-dev
  libx11-xcb-dev
  libx11-xcb1
  libxau-dev
  libxau6
  libxcb-dri2-0
  libxcb-dri2-0-dev
  libxcb-dri3-0
  libxcb-glx0
  libxcb-glx0-dev
  libxcb-present0
  libxcb-render0
  libxcb-render0-dev
  libxcb-shm0
  libxcb-shm0-dev
  libxcb-sync1
  libxcb-xfixes0
  libxcb1
  libxcb1-dev
  libxcomposite-dev
  libxcomposite1
  libxcursor-dev
  libxcursor1
  libxdamage-dev
  libxdamage1
  libxdmcp-dev
  libxdmcp6
  libxext-dev
  libxext6
  libxfixes-dev
  libxfixes3
  libxft2
  libxi-dev
  libxi6
  libxinerama-dev
  libxinerama1
  libxkbcommon-dev
  libxkbcommon0
  libxml2
  libxml2-dev
  libxrandr-dev
  libxrandr2
  libxrender-dev
  libxrender1
  libxshmfence1
  libxslt1-dev
  libxslt1.1
  libxss-dev
  libxss1
  libxt-dev
  libxt6
  libxtst-dev
  libxtst6
  libxxf86vm-dev
  libxxf86vm1
  libzstd1
  linux-libc-dev
  mesa-common-dev
  shared-mime-info
  speech-dispatcher
  uuid-dev
  wayland-protocols
  x11proto-composite-dev
  x11proto-damage-dev
  x11proto-dev
  x11proto-fixes-dev
  x11proto-input-dev
  x11proto-kb-dev
  x11proto-randr-dev
  x11proto-record-dev
  x11proto-render-dev
  x11proto-scrnsaver-dev
  x11proto-xext-dev
  x11proto-xinerama-dev
  zlib1g
  zlib1g-dev
"

DEBIAN_PACKAGES_AMD64="
  liblsan0
  libtsan0
"

DEBIAN_PACKAGES_X86="
  libasan3
  libcilkrts5
  libdrm-intel1
  libitm1
  libmpx2
  libquadmath0
  libubsan0
"

DEBIAN_PACKAGES_ARM="
  libasan3
  libdrm-etnaviv1
  libdrm-exynos1
  libdrm-freedreno1
  libdrm-omap1
  libdrm-tegra0
  libubsan0
"

DEBIAN_PACKAGES_ARM64="
  libasan3
  libdrm-etnaviv1
  libdrm-freedreno1
  libdrm-tegra0
  libgmp10
  libitm1
  libthai0
  libubsan0
"

DEBIAN_PACKAGES_ARMEL="
  libasan3
  libdrm-exynos1
  libdrm-freedreno1
  libdrm-omap1
  libdrm-tegra0
  libubsan0
"

. "${SCRIPT_DIR}/sysroot-creator.sh"
