#!/bin/sh

check_pkgconfig() {
	if ! which pkg-config > /dev/null 
	then
		echo "Error: install pkg-config (and make sure its in your path)" 1>&2
		exit 1
	fi
}

CFLAGS=()
LDFLAGS=()
OPTIONS=()

add_cflags() {
	CFLAGS=("${CFLAGS[@]}" "$@")
}

add_ldflags() {
	LDFLAGS=("${LDFLAGS[@]}" "$@")
}

package() {
    if ! pkg-config $1
    then
     	return 1
    else
     	add_cflags `pkg-config --cflags $1`
     	add_ldflags `pkg-config --libs $1`
	
     	return 0
	fi
}


check_required() {
	if ! package $1
	then
		echo "Error: package '$1' is required" 1>&2
		exit 1
	fi
	return 0
}

check_required_tool() {
  if ! $1 --version > /dev/null
  then
    echo "Error: package '$2' is required" 1>&2
    exit 1
  fi
  
  add_cflags `$1 --cflags`
  add_ldflags `$1 --libs`

  return 0
}

check_optional() {
	if package $2
	then
		add_cflags -D$3
		OPTIONS=("${OPTIONS[@]}" "$1")
		return 0
	else
     	return 1
	fi
}

help() {
  echo "options:"
  echo "   --with-jpeg=<prefix>       Use libjpeg found at <prefix>"
  echo "   --with-mng=<prefiy>        Use libmng found at <prefix>"
  echo "" 
 exit 0
}


parse_cmdline() {
  while test -n "$1" ; do
    case "$1" in
      --help|-h)
	  	echo "configure script for cocoa netsurf"
        help
        exit 0
      ;;
      
      --with-*=*)
        name=`expr "$1" : '--with-\(.*\)=.*'`
        value=`expr "$1" : '--with-.*=\(.*\)'`
        eval "USE_$name='$value'"
      ;;
      
      *)
        echo "Error: invalid argument '$1'" 1>&2
        help 1>&2
        exit 1
      ;;
      
    esac
    shift
  done
}

manual_config() {
  var="USE_$1"
  PREFIX=${!var}
	if test -n "$PREFIX" ; then
		OPTIONS=("${OPTIONS[@]}" "$1")
		add_cflags -D$2 "-I$PREFIX/include"
		add_ldflags "-L$PREFIX/lib" "-l$3"
	fi
}

parse_cmdline "$@"

check_pkgconfig

# Required libraries
check_required libhubbub
check_required libcss
check_required libparserutils
check_required libwapcaplet
check_required libcurl
check_required openssl
check_required_tool xml2-config libxml2

# Optional libraries
check_optional gif libnsgif WITH_GIF 
check_optional bmp libnsbmp WITH_BMP
check_optional rsvg librsvg-2.0 WITH_RSVG
check_optional svgtiny libsvgtiny WITH_NS_SVG
check_optional rosprite librosprite WITH_NSSPRITE


# Optional libraries witout pkg-config information
manual_config jpeg WITH_JPEG jpeg
manual_config mng WITH_MNG mng

# OS X provides libpng in /usr/X11
add_cflags -DWITH_PNG -I/usr/X11/include
add_ldflags -L/usr/X11/lib -lpng

# OS X provides libiconv
add_ldflags -liconv

##
#	Generate config file

cat << EOF > local.xcconfig
// Local configuration generated on `hostname` at `date`
// Activated options: ${OPTIONS[@]}

LOCAL_CONFIG_CFLAGS=${CFLAGS[@]}
LOCAL_CONFIG_LDFLAGS=${LDFLAGS[@]}
EOF
