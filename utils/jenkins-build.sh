#!/bin/bash
#
# Copyright © 2013 Vincent Sanders <vince@netsurf-browser.org>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
#   * The above copyright notice and this permission notice shall be included in
#     all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# NetSurf continuius integration build script for jenkins
#
# This script is executed by jenkins to build netsurf itself
#
# Usage: jenkins-build.sh
#


# TARGET must be in the environment and set correctly
if [ "x${TARGET}" = "x" ];then
    echo "TARGET unset"
    exit 1
fi

#identifier for this specific build
IDENTIFIER="$CC-${BUILD_JS}-${BUILD_NUMBER}"

# default atari architecture - bletch
ATARIARCH=68020-60

if [ "${TARGET}" = "riscos" ];then

    ARTIFACT_TARGET=riscos
    PKG_SRC=netsurf
    PKG_SFX=.zip

elif [ "${TARGET}" = "windows" ];then

    ARTIFACT_TARGET=windows
    PKG_SRC=netsurf-installer
    PKG_SFX=.exe

elif [ "${TARGET}" = "cocoa" ];then

    if [ "${label}" = "i686-apple-darwin10" ]; then
        ARTIFACT_TARGET=Darwin
        IDENTIFIER="${label}-${IDENTIFIER}"

    elif [ "${label}" = "powerpc-apple-darwin9" ]; then
        ARTIFACT_TARGET=powerpc-apple-darwin9
        IDENTIFIER="${ARTIFACT_TARGET}-${IDENTIFIER}"

    else
        echo "Bad cocoa label"
        exit 1

    fi

    PKG_SRC=NetSurf
    PKG_SFX=.dmg

elif [ "${TARGET}" = "amiga" ];then

    ARTIFACT_TARGET=amiga
    PKG_SRC=NetSurf_Amiga/netsurf
    PKG_SFX=.lha


elif [ "${TARGET}" = "atari" ];then

    if [ "${label}" = "m68k-atari-mint" ]; then
        ARTIFACT_TARGET=m68k-atari-mint
        IDENTIFIER="${ARTIFACT_TARGET}-${IDENTIFIER}"    
        PKG_SRC=ns020
        PKG_SFX=.zip

    elif [ "${label}" = "m5475-atari-mint" ]; then
        ARTIFACT_TARGET=m5475-atari-mint
        export GCCSDK_INSTALL_ENV=/opt/netsurf/m5475-atari-mint/env
        export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/m5475-atari-mint/cross/bin
        ATARIARCH=v4e
        IDENTIFIER="${ARTIFACT_TARGET}-${IDENTIFIER}"
        PKG_SRC=nsv4e
        PKG_SFX=.zip

    else

        echo "Bad atari label"
        exit 1

    fi    

elif [ "${TARGET}" = "gtk" ];then

    ARTIFACT_TARGET=Linux
    PKG_SRC=nsgtk
    PKG_SFX=

elif [ "${TARGET}" = "framebuffer" ];then


    ARTIFACT_TARGET=Linux
    PKG_SRC=nsfb
    PKG_SFX=

elif [ "${TARGET}" = "monkey" ];then

    if [ "${label}" = "linux" ]; then
        ARTIFACT_TARGET=Linux

    elif [ "${label}" = "i686-apple-darwin10" ]; then
        ARTIFACT_TARGET=Darwin

    elif [ "${label}" = "powerpc-apple-darwin9" ]; then
        ARTIFACT_TARGET=powerpc-apple-darwin9

    elif [ "${label}" = "arm-unknown-riscos" ]; then
        ARTIFACT_TARGET=riscos
        export GCCSDK_INSTALL_ENV=/opt/netsurf/${label}/env
        export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${label}/cross/bin

    elif [ "${label}" = "m68k-atari-mint" ]; then
        ARTIFACT_TARGET=m68k-atari-mint
        export GCCSDK_INSTALL_ENV=/opt/netsurf/${label}/env
        export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${label}/cross/bin

    elif [ "${label}" = "m5475-atari-mint" ]; then
        ATARIARCH=v4e
        ARTIFACT_TARGET=m5475-atari-mint
        export GCCSDK_INSTALL_ENV=/opt/netsurf/${label}/env
        export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${label}/cross/bin

    elif [ "${label}" = "i686-w64-mingw32" ]; then
        ARTIFACT_TARGET=windows
        export GCCSDK_INSTALL_ENV=/opt/netsurf/${label}/env
        export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${label}/cross/bin

    elif [ "${label}" = "ppc-amigaos" ]; then
	ARTIFACT_TARGET=amiga
        export GCCSDK_INSTALL_ENV=/opt/netsurf/${label}/env
        export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${label}/cross/bin

    else
        echo "Bad monkey label"
        exit 1
    fi

    IDENTIFIER="${label}-${IDENTIFIER}"
    PKG_SRC=nsmonkey
    PKG_SFX=

else
    
    # unkown target
    exit 1

fi

########### Build from source ##################

# setup environment
export PREFIX=${JENKINS_HOME}/artifacts-${ARTIFACT_TARGET}
export PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${PREFIX}/lib
export PATH=${PATH}:${PREFIX}/bin

# disable ccache for clang
if [ "${CC}" = "clang" ];then
export CCACHE_CPP2=yes
export CC="clang -Qunused-arguments"
fi

# convert javascript parameters
if [ "${BUILD_JS}" = "json" ];then
        BUILD_MOZJS=NO
        BUILD_JS=YES
else
    BUILD_JS=NO
    BUILD_MOZJS=NO
fi

make NETSURF_USE_JS=${BUILD_JS} NETSURF_USE_MOZJS=${BUILD_MOZJS} clean

# Do the Build
make -k NETSURF_USE_JS=${BUILD_JS} NETSURF_USE_MOZJS=${BUILD_MOZJS} CI_BUILD=${BUILD_NUMBER} ATARIARCH=${ATARIARCH} Q=

############ Package artifact construction and deployment ################

#destination for package artifacts
DESTDIR=/srv/ci.netsurf-browser.org/html/builds/${TARGET}/

# build the package file
make -k NETSURF_USE_JS=${BUILD_JS} NETSURF_USE_MOZJS=${BUILD_MOZJS} CI_BUILD=${BUILD_NUMBER} ATARIARCH=${ATARIARCH} package Q=

if [ ! -f "${PKG_SRC}${PKG_SFX}" ]; then
    # unable to find package file
    exit 1
fi

# copy the file into the output - always use scp as it works local or remote
scp "${PKG_SRC}${PKG_SFX}" netsurf@ci.netsurf-browser.org:${DESTDIR}/NetSurf-${IDENTIFIER}${PKG_SFX}

# remove the package file 
rm -f "${PKG_SRC}${PKG_SFX}"

# setup latest link
ssh netsurf@ci.netsurf-browser.org "rm -f ${DESTDIR}/LATEST && echo "NetSurf-${IDENTIFIER}${PKG_SFX}" > ${DESTDIR}/LATEST"
