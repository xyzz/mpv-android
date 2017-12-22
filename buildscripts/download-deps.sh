#!/bin/bash -e

source ./version.sh

[ -z "$TRAVIS" ] && TRAVIS=0

mkdir -p deps && cd deps

# nettle
mkdir nettle
cd nettle
wget https://ftp.gnu.org/gnu/nettle/nettle-$v_nettle.tar.gz -O - | \
	tar -xz -f - --strip-components=1
cd ..

# gnutls
mkdir gnutls
cd gnutls
wget ftp://ftp.gnutls.org/gcrypt/gnutls/v${v_gnutls%.*}/gnutls-$v_gnutls.tar.xz -O - | \
	tar -xJ -f - --strip-components=1
cd ..

# ffmpeg
git clone https://github.com/FFmpeg/FFmpeg ffmpeg
if [ $TRAVIS -eq 1 ]; then
	pushd ffmpeg
	git checkout $v_travis_ffmpeg
	popd
fi

# freetype2
git clone git://git.sv.nongnu.org/freetype/freetype2.git -b VER-$v_freetype

# fribidi
mkdir fribidi
cd fribidi
wget https://download.videolan.org/contrib/fribidi/fribidi-$v_fribidi.tar.bz2 -O - | \
	tar -xj -f - --strip-components=1
cd ..

# libass
git clone https://github.com/libass/libass -b $v_libass

# lua
mkdir lua
cd lua
wget http://www.lua.org/ftp/lua-$v_lua.tar.gz -O - | \
	tar -xz -f - --strip-components=1
cd ..

# mpv
if [ $TRAVIS -eq 0 ]; then
	git clone https://github.com/mpv-player/mpv
fi

cd ..
