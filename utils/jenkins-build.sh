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

# NetSurf continuous integration build script for jenkins
#
# This script is executed by jenkins to build netsurf itself
#
# Usage: jenkins-build.sh
#

# TARGET is set to the frontend target to build
# label is set to the identifier of the toolchain doing the building

################# Parameter and environment setup #####################

#identifier for this specific build
IDENTIFIER="$CC-${BUILD_JS}-${BUILD_NUMBER}"

# default atari architecture - bletch
ATARIARCH=68020-60

# Ensure the combination of target and toolchain works and set build
#   specific parameters too
case ${TARGET} in
    "riscos")
	case ${label} in
	    "arm-unknown-riscos")
		ARTIFACT_TARGET=riscos
		;;

	    *)
		echo "Target \"${TARGET}\" cannot be built on \"${label})\""
		exit 1
		;;

	esac

	PKG_SRC=netsurf
	PKG_SFX=.zip
	;;


    "windows")
	case ${label} in
	    "i686-w64-mingw32")
		ARTIFACT_TARGET=windows
		;;

	    *)
		echo "Target \"${TARGET}\" cannot be built on \"${label})\""
		exit 1
		;;

	esac

	PKG_SRC=netsurf-installer
	PKG_SFX=.exe
	;;


    "cocoa")
	case ${label} in
	    "i686-apple-darwin10")
		ARTIFACT_TARGET=Darwin
		IDENTIFIER="${label}-${IDENTIFIER}"
		;;

	    "powerpc-apple-darwin9")
		ARTIFACT_TARGET=powerpc-apple-darwin9
		IDENTIFIER="${ARTIFACT_TARGET}-${IDENTIFIER}"
		;;

	    *)
		echo "Target \"${TARGET}\" cannot be built on \"${label})\""
		exit 1
		;;

	esac

	PKG_SRC=NetSurf
	PKG_SFX=.dmg
	;;


    "amiga")
	case ${label} in
	    "ppc-amigaos")
		ARTIFACT_TARGET=amiga
		;;

	    *)
		echo "Target \"${TARGET}\" cannot be built on \"${label})\""
		exit 1
		;;

	esac

	PKG_SRC=NetSurf_Amiga/netsurf
	PKG_SFX=.lha
	;;


    "atari")
	case ${label} in
	    "m68k-atari-mint")
		ARTIFACT_TARGET=m68k-atari-mint
		PKG_SRC=ns020
		PKG_SFX=.zip
		;;

	    "m5475-atari-mint")
		ARTIFACT_TARGET=m5475-atari-mint
		export GCCSDK_INSTALL_ENV=/opt/netsurf/m5475-atari-mint/env
		export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/m5475-atari-mint/cross/bin
		ATARIARCH=v4e
		PKG_SRC=nsv4e
		PKG_SFX=.zip
		;;

	    *)
		echo "Target \"${TARGET}\" cannot be built on \"${label})\""
		exit 1
		;;

	esac

	IDENTIFIER="${ARTIFACT_TARGET}-${IDENTIFIER}"
	;;


    "gtk")
	case ${label} in
	    "x86_64-linux-gnu")
		ARTIFACT_TARGET=Linux
		;;

	    *)
		echo "Target \"${TARGET}\" cannot be built on \"${label})\""
		exit 1
		;;

	esac

	PKG_SRC=nsgtk
	PKG_SFX=
	;;


    "framebuffer")
	case ${label} in
	    "x86_64-linux-gnu")
		ARTIFACT_TARGET=Linux
		;;

	    "i686-apple-darwin10")
		ARTIFACT_TARGET=Darwin
		;;

	    "powerpc-apple-darwin9")
		ARTIFACT_TARGET=powerpc-apple-darwin9
		;;

	    "arm-unknown-riscos")
		ARTIFACT_TARGET=riscos
		export GCCSDK_INSTALL_ENV=/opt/netsurf/${label}/env
		export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${label}/cross/bin
		;;

	    "m68k-atari-mint")
		ARTIFACT_TARGET=m68k-atari-mint
		export GCCSDK_INSTALL_ENV=/opt/netsurf/${label}/env
		export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${label}/cross/bin
		;;

	    "m5475-atari-mint")
		ATARIARCH=v4e
		ARTIFACT_TARGET=m5475-atari-mint
		export GCCSDK_INSTALL_ENV=/opt/netsurf/${label}/env
		export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${label}/cross/bin
		;;

	    "i686-w64-mingw32")
		ARTIFACT_TARGET=windows
		export GCCSDK_INSTALL_ENV=/opt/netsurf/${label}/env
		export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${label}/cross/bin
		;;

	    "ppc-amigaos")
		ARTIFACT_TARGET=amiga
		export GCCSDK_INSTALL_ENV=/opt/netsurf/${label}/env
		export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${label}/cross/bin
		;;

	    *)
		echo "Target \"${TARGET}\" cannot be built on \"${label})\""
		exit 1
		;;

	esac

	PKG_SRC=nsfb
	PKG_SFX=
	;;


    "monkey")
	# monkey target can be built on most of the supported architectures
	case ${label} in
	    "x86_64-linux-gnu")
		ARTIFACT_TARGET=Linux
		;;

	    "i686-apple-darwin10")
		ARTIFACT_TARGET=Darwin
		;;

	    "powerpc-apple-darwin9")
		ARTIFACT_TARGET=powerpc-apple-darwin9
		;;

	    "arm-unknown-riscos")
		ARTIFACT_TARGET=riscos
		export GCCSDK_INSTALL_ENV=/opt/netsurf/${label}/env
		export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${label}/cross/bin
		;;

	    "m68k-atari-mint")
		ARTIFACT_TARGET=m68k-atari-mint
		export GCCSDK_INSTALL_ENV=/opt/netsurf/${label}/env
		export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${label}/cross/bin
		;;

	    "m5475-atari-mint")
		ATARIARCH=v4e
		ARTIFACT_TARGET=m5475-atari-mint
		export GCCSDK_INSTALL_ENV=/opt/netsurf/${label}/env
		export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${label}/cross/bin
		;;

	    "i686-w64-mingw32")
		ARTIFACT_TARGET=windows
		export GCCSDK_INSTALL_ENV=/opt/netsurf/${label}/env
		export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${label}/cross/bin
		;;

	    "ppc-amigaos")
		ARTIFACT_TARGET=amiga
		export GCCSDK_INSTALL_ENV=/opt/netsurf/${label}/env
		export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${label}/cross/bin
		;;

	    *)
		echo "Target \"${TARGET}\" cannot be built on \"${label})\""
		exit 1
		;;

	esac

	IDENTIFIER="${label}-${IDENTIFIER}"
	PKG_SRC=nsmonkey
	PKG_SFX=
	;;

    *)
	# TARGET must be in the environment and set correctly
	echo "Unkown TARGET \"${TARGET}\""
	exit 1
	;;

esac

# setup environment
export PREFIX=${JENKINS_HOME}/artifacts-${ARTIFACT_TARGET}
export PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${PREFIX}/lib
export PATH=${PATH}:${PREFIX}/bin

# configure ccache for clang
if [ "${CC}" = "clang" ];then
    export CCACHE_CPP2=yes
    export CC="clang -Qunused-arguments"
fi

# convert javascript parameters
if [ "${BUILD_JS}" = "json" ];then
    case ${TARGET} in
	"riscos")
	    BUILD_MOZJS=NO
	    BUILD_JS=YES
	    ;;
	*)
	    BUILD_MOZJS=YES
	    BUILD_JS=NO
	;;

    esac

else
    BUILD_JS=NO
    BUILD_MOZJS=NO
fi




########### Build from source ##################

# Clean first
make NETSURF_USE_JS=${BUILD_JS} NETSURF_USE_MOZJS=${BUILD_MOZJS} clean

# Do the Build
make -k NETSURF_USE_JS=${BUILD_JS} NETSURF_USE_MOZJS=${BUILD_MOZJS} CI_BUILD=${BUILD_NUMBER} ATARIARCH=${ATARIARCH} Q=





############ Package artifact construction ################

#destination for package artifacts
DESTDIR=/srv/ci.netsurf-browser.org/html/builds/${TARGET}/

# build the package file
make -k NETSURF_USE_JS=${BUILD_JS} NETSURF_USE_MOZJS=${BUILD_MOZJS} CI_BUILD=${BUILD_NUMBER} ATARIARCH=${ATARIARCH} package Q=

if [ ! -f "${PKG_SRC}${PKG_SFX}" ]; then
    # unable to find package file
    exit 1
fi



############ Package artifact deployment ################

# copy the file into the output - always use scp as it works local or remote
scp "${PKG_SRC}${PKG_SFX}" netsurf@ci.netsurf-browser.org:${DESTDIR}/NetSurf-${IDENTIFIER}${PKG_SFX}

# remove the package file
rm -f "${PKG_SRC}${PKG_SFX}"

# setup latest link
ssh netsurf@ci.netsurf-browser.org "rm -f ${DESTDIR}/LATEST && echo "NetSurf-${IDENTIFIER}${PKG_SFX}" > ${DESTDIR}/LATEST"
