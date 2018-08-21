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
# HOST is set to the identifier of the toolchain doing the building
# CC is the compiler (gcc or clang)
# BUILD_NUMBER is the CI build number

#####

# set defaults - this is not retrivable from the jenkins environment
OLD_ARTIFACT_COUNT=25

################# Parameter and environment setup #####################

#identifier for this specific build
IDENTIFIER="$CC-${BUILD_NUMBER}"

# Identifier for build which will be cleaned
OLD_IDENTIFIER="$CC-$((BUILD_NUMBER - ${OLD_ARTIFACT_COUNT}))"

# default atari architecture - bletch
ATARIARCH=68020-60

# make tool
MAKE=make

# Ensure the combination of target and toolchain works and set build
#   specific parameters too
case ${TARGET} in
    "riscos")
	case ${HOST} in
	    "arm-unknown-riscos")
		;;

	    *)
		echo "Target \"${TARGET}\" cannot be built on \"${HOST})\""
		exit 1
		;;

	esac

	PKG_SRC=netsurf
	PKG_SFX=.zip
	;;

    "haiku")
	case ${HOST} in
	    "i586-pc-haiku")
		;;

	    *)
		echo "Target \"${TARGET}\" cannot be built on \"${HOST})\""
		exit 1
		;;

	esac

	PKG_SRC=netsurf_x86-3.8-1-x86_gcc2
	PKG_SFX=.hpkg
	;;


    "windows")
	case ${HOST} in
	    "i686-w64-mingw32")
		;;

	    *)
		echo "Target \"${TARGET}\" cannot be built on \"${HOST})\""
		exit 1
		;;

	esac

	PKG_SRC=netsurf-installer
	PKG_SFX=.exe
	;;


    "cocoa")
	case ${HOST} in
	    "x86_64-apple-darwin14.5.0")
		PATH=/opt/local/bin:/opt/local/sbin:${PATH}
		;;

	    "i686-apple-darwin10")
		;;

	    "powerpc-apple-darwin9")
		;;

	    *)
		echo "Target \"${TARGET}\" cannot be built on \"${HOST})\""
		exit 1
		;;

	esac

	IDENTIFIER="${HOST}-${IDENTIFIER}"
	OLD_IDENTIFIER="${HOST}-${OLD_IDENTIFIER}"
	PKG_SRC=NetSurf
	PKG_SFX=.dmg
	;;


    "amiga")
	case ${HOST} in
	    "ppc-amigaos")
		;;

	    *)
		echo "Target \"${TARGET}\" cannot be built on \"${HOST})\""
		exit 1
		;;

	esac

	PKG_SRC=NetSurf_Amiga/netsurf
	PKG_SFX=.lha
	;;


    "amigaos3")
	case ${HOST} in
	    "m68k-unknown-amigaos")
		;;

	    *)
		echo "Target \"${TARGET}\" cannot be built on \"${HOST})\""
		exit 1
		;;

	esac

	PKG_SRC=NetSurf_Amiga/netsurf
	PKG_SFX=.lha
	;;


    "atari")
	case ${HOST} in
	    "m68k-atari-mint")
		PKG_SRC=ns020
		PKG_SFX=.zip
		;;

	    "m5475-atari-mint")
		export GCCSDK_INSTALL_ENV=/opt/netsurf/m5475-atari-mint/env
		export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/m5475-atari-mint/cross/bin
		ATARIARCH=v4e
		PKG_SRC=nsv4e
		PKG_SFX=.zip
		;;

	    *)
		echo "Target \"${TARGET}\" cannot be built on \"${HOST})\""
		exit 1
		;;

	esac

	IDENTIFIER="${HOST}-${IDENTIFIER}"
	OLD_IDENTIFIER="${HOST}-${OLD_IDENTIFIER}"
	;;


    "gtk")
	case ${HOST} in
	    "x86_64-linux-gnu")
		;;

	    "arm-linux-gnueabihf")
		;;

	    "aarch64-linux-gnu")
		;;

	    amd64-unknown-openbsd*)
		MAKE=gmake
		;;

	    x86_64-unknown-freebsd*)
		MAKE=gmake
		;;

	    *)
		echo "Target \"${TARGET}\" cannot be built on \"${HOST}\""
		exit 1
		;;

	esac

	IDENTIFIER="${HOST}-${IDENTIFIER}"
	OLD_IDENTIFIER="${HOST}-${OLD_IDENTIFIER}"
	PKG_SRC=nsgtk
	PKG_SFX=
	;;


    "gtk3")
	case ${HOST} in
	    "x86_64-linux-gnu")
		;;

	    "arm-linux-gnueabihf")
		;;

	    "aarch64-linux-gnu")
		;;

	    amd64-unknown-openbsd*)
		MAKE=gmake
		;;

	    x86_64-unknown-freebsd*)
		MAKE=gmake
		;;

	    *)
		echo "Target \"${TARGET}\" cannot be built on \"${HOST}\""
		exit 1
		;;

	esac

	IDENTIFIER="${HOST}-${IDENTIFIER}"
	OLD_IDENTIFIER="${HOST}-${OLD_IDENTIFIER}"
	PKG_SRC=nsgtk3
	PKG_SFX=
	;;


    "framebuffer")
	case ${HOST} in
	    "x86_64-linux-gnu")
		;;

	    arm-linux-gnueabihf)
		;;

	    "aarch64-linux-gnu")
		;;

	    "i686-apple-darwin10")
		;;

	    "powerpc-apple-darwin9")
		;;

	    amd64-unknown-openbsd*)
		MAKE=gmake
		;;

	    x86_64-unknown-freebsd*)
		MAKE=gmake
		;;

	    "arm-unknown-riscos")
		export GCCSDK_INSTALL_ENV=/opt/netsurf/${HOST}/env
		export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${HOST}/cross/bin
		;;

	    "m68k-atari-mint")
		export GCCSDK_INSTALL_ENV=/opt/netsurf/${HOST}/env
		export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${HOST}/cross/bin
		;;

	    "m5475-atari-mint")
		ATARIARCH=v4e
		export GCCSDK_INSTALL_ENV=/opt/netsurf/${HOST}/env
		export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${HOST}/cross/bin
		;;

	    "i686-w64-mingw32")
		export GCCSDK_INSTALL_ENV=/opt/netsurf/${HOST}/env
		export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${HOST}/cross/bin
		;;

	    "ppc-amigaos")
		export GCCSDK_INSTALL_ENV=/opt/netsurf/${HOST}/env
		export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${HOST}/cross/bin
		;;

	    "m68k-unknown-amigaos")
		export GCCSDK_INSTALL_ENV=/opt/netsurf/${HOST}/env
		export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${HOST}/cross/bin
		;;

	    *)
		echo "Target \"${TARGET}\" cannot be built on \"${HOST})\""
		exit 1
		;;

	esac

	IDENTIFIER="${HOST}-${IDENTIFIER}"
	OLD_IDENTIFIER="${HOST}-${OLD_IDENTIFIER}"
	PKG_SRC=nsfb
	PKG_SFX=
	;;


    "monkey")
	# monkey target can be built anywhere
	case ${HOST} in
	    amd64-unknown-openbsd*)
		MAKE=gmake
		;;

	    x86_64-unknown-freebsd*)
		MAKE=gmake
		;;

	    "arm-unknown-riscos")
		export GCCSDK_INSTALL_ENV=/opt/netsurf/${HOST}/env
		export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${HOST}/cross/bin
		# headers and compiler combination throw these warnings
		export CFLAGS="-Wno-redundant-decls -Wno-parentheses"
                export LDFLAGS=-lcares
		;;

	    "m68k-atari-mint")
		export GCCSDK_INSTALL_ENV=/opt/netsurf/${HOST}/env
		export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${HOST}/cross/bin
		;;

	    "m5475-atari-mint")
		ATARIARCH=v4e
		export GCCSDK_INSTALL_ENV=/opt/netsurf/${HOST}/env
		export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${HOST}/cross/bin
		;;

	    "i686-w64-mingw32")
		export GCCSDK_INSTALL_ENV=/opt/netsurf/${HOST}/env
		export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${HOST}/cross/bin
		;;

	    "ppc-amigaos")
		export GCCSDK_INSTALL_ENV=/opt/netsurf/${HOST}/env
		export GCCSDK_INSTALL_CROSSBIN=/opt/netsurf/${HOST}/cross/bin
		;;

	    *)
		echo "Target \"${TARGET}\" generic build on \"${HOST})\""
		;;

	esac

	IDENTIFIER="${HOST}-${IDENTIFIER}"
	OLD_IDENTIFIER="${HOST}-${OLD_IDENTIFIER}"
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
export PREFIX=${JENKINS_HOME}/artifacts-${HOST}
export PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${PREFIX}/lib
export PATH=${PATH}:${PREFIX}/bin

# configure ccache for clang
if [ "${CC}" = "clang" ];then
    export CCACHE_CPP2=yes
    export CC="clang -Qunused-arguments"
fi

########### Use distcc if present ######

DISTCC=distcc
PARALLEL=1
HAVE_DISTCC=$(${DISTCC} --version >/dev/null 2>&1 && echo "true" || echo "false")
if [ ${HAVE_DISTCC} = "true" ];then
    PARALLEL=$(${DISTCC} -j)
    export PATH=/usr/lib/distcc:${PATH}
    export DISTCC_DIR=${JENKINS_HOME}
fi


########### Prepare a Makefile.config ##################

rm -f Makefile.config
cat > Makefile.config <<EOF
override NETSURF_LOG_LEVEL := DEBUG
EOF

########### Additional environment info ########

uname -a


########### Build from source ##################

# Clean first
${MAKE} clean

# Do the Build
${MAKE} -j ${PARALLEL} -k CI_BUILD=${BUILD_NUMBER} ATARIARCH=${ATARIARCH} Q=


############ Package artifact construction ################

# build the package file
${MAKE} -k CI_BUILD=${BUILD_NUMBER} ATARIARCH=${ATARIARCH} PACKAGER="NetSurf Developers <support@netsurf-browser.org>" Q= package

if [ ! -f "${PKG_SRC}${PKG_SFX}" ]; then
    # unable to find package file
    exit 1
fi


############ Package artifact deployment ################

#destination for package artifacts
DESTDIR=/srv/ci.netsurf-browser.org/html/builds/${TARGET}/

NEW_ARTIFACT_TARGET="NetSurf-${IDENTIFIER}${PKG_SFX}"

# copy the file into the output - always use scp as it works local or remote
scp "${PKG_SRC}${PKG_SFX}" netsurf@ci.netsurf-browser.org:${DESTDIR}/${NEW_ARTIFACT_TARGET}

# remove the local package file artifact
rm -f "${PKG_SRC}${PKG_SFX}"

# setup latest link
ssh netsurf@ci.netsurf-browser.org "rm -f ${DESTDIR}/LATEST && echo "${NEW_ARTIFACT_TARGET}" > ${DESTDIR}/LATEST"



############ Package artifact cleanup ################

OLD_ARTIFACT_TARGET="NetSurf-${OLD_IDENTIFIER}${PKG_SFX}"

ssh netsurf@ci.netsurf-browser.org "rm -f ${DESTDIR}/${OLD_ARTIFACT_TARGET}"
