#!/bin/sh
#
# NetSurf Library, tool and browser support script
#
# Usage: source env.sh
# TARGET_ABI sets the target for library builds
# TARGET_WORKSPACE is the workspace directory to keep the sandboxes
#
# This script allows NetSurf and its libraries to be built without
#   requiring installation into a system.
#
# Copyright 2013 Vincent Sanders <vince@netsurf-browser.org>
# Released under the MIT Licence

# parameters
if [ "x${TARGET_ABI}" = "x" ]; then
    TARGET_ABI=$(uname -s)
fi

if [ "x${TARGET_WORKSPACE}" = "x" ]; then
    TARGET_WORKSPACE=${HOME}/dev-netsurf/workspace
fi

if [ "x${USE_CPUS}" = "x" ]; then
    NCPUS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || getconf NPROCESSORS_ONLN 2>/dev/null)
    NCPUS="${NCPUS:-1}"
    NCPUS=$((NCPUS * 2))
    USE_CPUS="-j${NCPUS}"
fi

# setup environment
echo "TARGET_ABI=${TARGET_ABI}"
echo "TARGET_WORKSPACE=${TARGET_WORKSPACE}"
echo "USE_CPUS=${USE_CPUS}"

export PREFIX=${TARGET_WORKSPACE}/inst-${TARGET_ABI}
export PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH}::
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${PREFIX}/lib
export PATH=${PATH}:${PREFIX}/bin

NS_GIT="git://git.netsurf-browser.org"
# internal libraries all frontends require (order is important)
NS_INTERNAL_LIBS="buildsystem libwapcaplet libparserutils libhubbub libdom libcss libnsgif libnsbmp libsvgtiny librosprite"
# internal libraries only required by some frontends
NS_FRONTEND_LIBS="libnsfb"
# internal libraries required for the risc os target abi
NS_RISCOS_LIBS="librufl libpencil"
# tools required to build the browser
NS_TOOLS="nsgenbind"
# The browser itself
NS_BROWSER="netsurf"

# deb packages
NS_DEV_DEB="build-essential pkg-config git gperf"
NS_TOOL_DEB="flex bison libhtml-parser-perl"
NS_GTK_DEB="libgtk2.0-dev libcurl3-dev libpng-dev librsvg2-dev libjpeg-dev libmozjs185-dev"

#add target specific libraries
if [ "x${TARGET_ABI}" = "xriscos" ]; then
     NS_FRONTEND_LIBS="${NS_FRONTEND_LIBS} ${NS_RISCOS_LIBS}"
fi

# apt get commandline to install necessary dev packages
ns-apt-get-install()
{
    sudo apt-get install $(echo ${NS_DEV_DEB} ${NS_TOOL_DEB} ${NS_GTK_DEB})
}

# git pull in all repos parameters are passed to git pull
ns-pull()
{
    for REPO in ${NS_INTERNAL_LIBS} ${NS_FRONTEND_LIBS} ${NS_TOOLS} ${NS_BROWSER} ; do 
	echo -n "     GIT: Pulling ${REPO}: "
	if [ -f ${TARGET_WORKSPACE}/${REPO}/.git/config ]; then
	    (cd ${TARGET_WORKSPACE}/${REPO} && git pull $*; )
	else
	    echo "Repository not present"	    
	fi
    done
}

# clone all repositories
ns-clone()
{
    mkdir -p ${TARGET_WORKSPACE}
    for REPO in $(echo ${NS_INTERNAL_LIBS} ${NS_FRONTEND_LIBS} ${NS_RISCOS_LIBS} ${NS_TOOLS} ${NS_BROWSER}) ; do 
	echo -n "     GIT: Cloning ${REPO}: "
	if [ -f ${TARGET_WORKSPACE}/${REPO}/.git/config ]; then
	    echo "Repository already present"
	else
	    (cd ${TARGET_WORKSPACE} && git clone ${NS_GIT}/${REPO}.git; )
	fi
    done

    # put current env.sh in place in workspace
    if [ ! -f "${TARGET_WORKSPACE}/env.sh" -a -f ${TARGET_WORKSPACE}/${NS_BROWSER}/Docs/env.sh ];then
	cp ${TARGET_WORKSPACE}/${NS_BROWSER}/Docs/env.sh ${TARGET_WORKSPACE}/env.sh
    fi
}

# issues a make command to all libraries
ns-make-libs()
{
    for REPO in $(echo ${NS_INTERNAL_LIBS} ${NS_FRONTEND_LIBS} ${NS_TOOLS}); do 
	echo "    MAKE: make -C ${REPO} $USE_CPUS $*"
        make -C ${TARGET_WORKSPACE}/${REPO} TARGET=${TARGET_ABI} $USE_CPUS $*
    done
}

# issues a make command to all libraries
ns-make-libnsfb()
{
    echo "    MAKE: make -C libnsfb $USE_CPUS $*"
    make -C ${TARGET_WORKSPACE}/libnsfb TARGET=${TARGET_ABI} $USE_CPUS $*
}

# pulls all repos and makes and installs the libraries and tools
ns-pull-install()
{
    ns-pull $*

    ns-make-libs install
}

# Passes appropriate flags to make
ns-make()
{
    make $USE_CPUS "$@"
}

