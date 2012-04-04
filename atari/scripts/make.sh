#!/bin/sh
arch="68000"
#arch="68020-60"
#arch="5475"
prefix="/usr/m68k-atari-mint/"
libdir=$prefix"lib/"
outfile="ns.prg"
release="0"

while [ "$1" != "" ]                    # When there are arguments...
do                                      # Process the next one
  case $1                               # Look at $1
  in
	-arch)
		shift
		arch=$1
		shift
	;;

	-prefix)
		shift
		prefix=$1
		shift
	;;

	-release)
		release="1"
		shift
	;;

	*)
		echo "Unknown commandline option"
	exit;;
  esac
done

libdir=$prefix"lib/"


if [ "$arch" = "68000" ]
then
echo "Default m68000 build."
else
libdir="$libdir/m$arch/"
fi

if [ "$arch" = "68020-60" ]
then
outfile="ns020.prg"
fi

if [ "$arch" = "5475" ]
then
outfile="nscf.prg"
fi


echo "compiling: " $outfile
echo



pkgconfdir="$libdir"pkgconfig
export PKG_CONFIG_PATH=$pkgconfdir
export PKG_CONFIG_LIBDIR=$pkgconfdir

#env

echo "ibdir: $libdir"
echo "pkgconfdir: $pkgconfdir"
echo "arch: $arch"


echo PKG_CONFIG_PATH="$pkgconfdir" PKG_CONFIG_LIBDIR="$pkgconfdir" AS="m68k-atari-mint-as" CC="m68k-atari-mint-gcc" LD="m68k-atari-mint-ld" AR="m68k-atari-mint-ar" RANLIB="m68k-atari-mint-ranlib" make TARGET="atari" 
PKG_CONFIG_PATH="$pkgconfdir" PKG_CONFIG_LIBDIR="$pkgconfdir" AS="m68k-atari-mint-as" CC="m68k-atari-mint-gcc" LD="m68k-atari-mint-ld" AR="m68k-atari-mint-ar" RANLIB="m68k-atari-mint-ranlib" make TARGET="atari" 
if [ "$arch" != "68000" ]
then
mv ns.prg $outfile
fi


if [ "$release" = "1" ]
then
echo Stripping $outfile ...
m68k-atari-mint-strip $outfile
fi


