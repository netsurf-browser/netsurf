#!/bin/sh

#todo: cflib, libcurl -> ensure ssl support, force ssl support

#example usage: 
#./makelibs.sh -prefix /usr -dest /media/EXT3_DATA/nslibs/m68000 -cross -nsonly -clean -src ./
#./makelibs.sh -prefix /usr -dest /media/EXT3_DATA/nslibs/m68020 -cross -arch 68020 -nsonly -clean -src ./
#./makelibs.sh -prefix /usr -dest /media/EXT3_DATA/nslibs/m68020-60 -cross -arch 68020-60 -nsonly -clean -src ./
#./makelibs.sh -prefix /usr -dest /media/EXT3_DATA/nslibs/m5475 -cross -arch 5475 -clean

# option description:
#
#-buildroot - this option tells the script where it is located, only needen when the script isn|t located in cwd.
#-arch - specifiy architecture type (format: 68000, 68020, 5475 etc...) 
#-src - tell the tool where the sources for the ns libs are located
#-prefix - what prefix to use ( -prefix local )
#-dest - where to install result files, this should NOT point to /usr or something like that!!! Its a temporary folder. 
#-optflags 
#-with-nsfb - compile with libnsfb
#-cross - set up some cross-compiler vars
#-clean - clean source dirs before building 
#-nsonly - only build netsurf libs
#-dry - only set environment variables, echo them and then exit the script
#-release - compile from release svn tree 

buildroot=`pwd`"/"
libopensslpkg="openssl-0.9.8r"
libpngpkg="libpng-1.5.10"
libzlibpkg="zlib-1.2.5"
libldgpkg="ldg-dev-2.33"
libiconvpkg="libiconv-1.13.1"
libcurlpkg="curl-7.25.0"
libfreetypepkg="freetype-2.4.9"
libhermespkg="Hermes-1.3.3"
libjpegpkg="jpeg-8b"
libxmlpkg="libxml2-2.7.8"
libparserutils_version=0.1.1
libwapcaplet_version=0.1.1
hubbub_version=0.1.2
libnsgif_version=0.0.3
libnsbmp_version=0.0.3
libnsfb_version=0.0.2
libcss_version=0.1.2
arch="68000"
archdir=""
debugmode="1"
profileflags=""
optflags="-O2"
withnsfb="1"
prefix="/usr"
userfs=$buildroot"userfs"
patchdir=$buildroot"patches/"
builddir=$buildroot"build/"
rpmdir=$buildroot"packages/"
nssrctree=$buildroot
compiler="gcc"
cross="0"
cleanup="echo no cleaning"
ssltarget="m68k-mint"
dry="0"
nsonly="0"
release="0"

if [ "$CC" != "" ]
then
	compiler=$CC
fi

while [ "$1" != "" ]			# When there are arguments...
do					# Process the next one
  case $1				# Look at $1
  in
   -dummy)
		dummy="1"
		shift
	;;

   -p)
		profileflags="-pg"
		shift
	;;

   -buildroot)
		shift
		buildroot=$1
		userfs=$buildroot"userfs"
		patchdir=$buildroot"patches/"
		builddir=$buildroot"build/"
		rpmdir=$buildroot"packages/"
		nssrctree=$buildroot"src/"
		shift
	;;

   -arch)
		shift
		arch=$1
		shift
	;;

   -src)
		shift
		nssrctree=$1
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
	
   -dest)
		shift
		userfs=$1
		shift
	;;

   -optflags)
		shift
		optflags=$1
		shift
	;;

   -with-nsfb)
		withnsfb="1"
		shift
	;;

   -cross )
		cross="1"
		shift
	;;

   -nsonly )
	nsonly="1"
	shift
	;;

   -clean )
		cleanup="make clean && make distclean"
		shift
	;;

   -dry ) 
                dry="1"
		shift
        ;;

	*)	echo "Option [$1] not one of  [-buildroot,-arch,-src,-prefix,-dest,-optflags,-with-nsfb]";
	exit;;
  esac
done


if [ -d "$buildroot" ] 
then
	echo "Buildroot: $buildroot"
else
	echo "Invalid buildroot directory ("$buildroot") !" 
	echo "This script must know where it is located!"
	echo "Either use buildroot option or start from directory where the script is located."
	exit 0
fi

if [ "$release" = "0" ]
then
libparserutils_version=""
libwapcaplet_version=""
hubbub_version=""
libnsgif_version=""
libnsbmp_version=""
libnsfb_version=""
libcss_version=""
fi

if [ "$nssrctree" = "./" ]
then
	nssrctree=`pwd`
fi



#testarch:
archok=0
if [ "$arch" = "68000" ]
then
	archok=1
fi
if [ "$arch" = "68020" ]
then
	archok=1
fi
if [ "$arch" = "68020-60" ]
then
	archok=1
	openssltarget="m680x0-mint"
fi
if [ "$arch" = "5475" ]
then
	archok=1
	openssltarget="cf-mint"
fi

if [ "$archok" = "0" ]
then
	echo "Invalid arch:"$arch" valid: 68000,68020,68020-69,5475"
	exit 0
fi

if [ "$cross" = "1" ]
then
	echo "enabling cross compiler mode"
	export CC="m68k-atari-mint-gcc"
	export LD="m68k-atari-mint-ld"
	export AR="m68k-atari-mint-ar"
	export RANLIB="m68k-atari-mint-ranlib"
	export CPP="m68k-atari-mint-cpp"
	compiler="m68k-atari-mint-gcc"
fi


# handle arch specific settings here.

if [ "$arch" = "68000" ]
then
	archlibdir=$userfs$prefix"/lib"
	archdir=""
else
	archlibdir=$userfs$prefix"/lib/m"$arch
	archdir="m"$arch
fi

if [ "$arch" = "5475" ]
then
	machineflag="cpu="$arch
else
	machineflag=$arch
fi


echo "machine: " $machineflag
incdir="-I$userfs$prefix/include"
CFLAGS_ORG="-m$machineflag $optflags $profileflags $incdir"
LDLAGS_ORG="-m$machineflag $profileflags"
CFLAGS="$CFLAGS_ORG"
LDFLAGS="$LDFLAGS_ORG"
export CFLAGS_ORG
export LDLAGS_ORG
export CFLAGS
export LDFLAGS

echo "Build root: "$buildroot
echo "Netsurf sources: "$nssrctree
echo "Build directory: "$builddir
echo "Patches: "$patchdir
echo "Libdir: "$archlibdir
echo "Dest: "$userfs
echo "Prefix: "$prefix
echo "Compiler: "$compiler
echo "CFLAGS: "$CFLAGS
echo "PKG_CONFIG_PATH=$archlibdir/pkgconfig"
echo "PKG_CONFIG_LIBDIR=$archlibdir/pkgconfig"
echo "PKG_CONFIG_SYSROOT_DIR=$userfs"
echo "Dry: " $dry


#echo "Installing RPMs:"
#rpm -i $rpmdir""$opensslpkg

#cd $buildroot
#exit 0


# configure flags for curl, this actually saves 30kb in the lib and about 100k in the final executable 
#./configure --disable-debug --enable-optimize --disable-ldap --disable-ldaps --disable-rtsp --disable-telnet --disable-tftp --disable-pop3 --disable-imap --disable-smtp --disable-manual -- disable-sspi --target="m"$arch 

# freetype configured for winfnt, truetype, raster (not smooth)
# saves around 160kb in the lib.

if [ "$dry" = "1" ]
then
echo "export CFLAGS=$CFLAGS"
echo "export PKG_CONFIG_PATH=$archlibdir/pkgconfig"
echo "export PKG_CONFIG_LIBDIR=$archlibdir/pkgconfig"
echo "export PKG_CONFIG_SYSROOT_DIR=$userfs"
exit 0
fi

echo "creating staging directory"
mkdir "$userfs"
mkdir "$userfs$prefix"
mkdir "$userfs$prefix/include"
mkdir "$archlibdir"
mkdir "$archlibdir/pkgconfig"


export PKG_CONFIG_PATH="$archlibdir/pkgconfig"
export PKG_CONFIG_LIBDIR="$archlibdir/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="$userfs"

echo "Building Libraries..."
cd $nssrctree

#start test
if [ "$nsonly" = "0" ]
then

echo "compiling Hermes..."
cd $libhermespkg
$cleanup
./configure --disable-x86asm --disable-debug --host="m68k-atari-mint" --prefix="$userfs$prefix"
make install
cd ..

echo "compiling iconv..."
cd $libiconvpkg
$cleanup
./configure --enable-static \
       --host="m68k-atari-mint"\
        --prefix="$prefix"\
	--exec-prefix="$prefix"\
       --enable-extra-encodings\
        lt_cv_sys_max_cmd_len=65536
make install DESTDIR=$destdir PREFIX=$prefix
cd ..


cd $libzlibpkg
$cleanup
if [ "$cross" = "1" ]
then
	./configure --prefix=$prefix --static
else
	./configure --prefix=$prefix --static
fi
make
# there is an error within make instal, copy headers manually. 
install -m644 zlib.h "$userfs$prefix/include/zlib.h"
install -m644 zconf.h "$userfs$prefix/include/zconf.h"
install -m644 zutil.h "$userfs$prefix/include/zutil.h"
make install DESTDIR=$userfs PREFIX="$prefix"
cd ..

pwd
cd $libfreetypepkg
$cleanup
if [ "$cross" = "1" ]
then
./configure --prefix="$prefix" \
	--host="m68k-atari-mint" \
	CFLAGS="$CFLAGS_ORG"
make
make install DESTDIR=$userfs
else
./configure --prefix="$userfs$prefix" --host="m68k-atari-mint" --target="m$arch" CFLAGS="$CFLAGS_ORG"
make
make install DESTDIR=$userfs
fi

cd ..

cd $libxmlpkg
$cleanup
if [ "$cross" = "1" ]
then
./configure --prefix="$prefix" \
	--host="m68k-atari-mint" \
	--without-python \
	--without-threads \
	--enable-ipv6=no \
	--without-debug \
	--without-http \
	--without-ftp \
	--without-legacy \
	--without-docbook \
	--without-catalog \
	--without-regexps \
	--without-schemas \
	--without-schematron \
	--without-sax1 \
	--without-xpath \
	--without-modules \
	--without-c14n \
	--without-pattern \
	--without-push \
	--with-iconv="$archlibdir" \
	--with-zlib="$archlibdir"
make
make install DESTDIR="$userfs"
else
echo "no cross"
fi
cd ..

cd $libopensslpkg
$cleanup
if [ "$cross" = "1" ]
then
./Configure $openssltarget --prefix="$prefix" --install-prefix="$userfs"
else
./Configure $openssltarget --prefix="$prefix" --install-prefix="$userfs" 
fi
make
make rehash
make install
cd ..

# FIXME: build c-ares here, if you want to

cd $libcurlpkg
$cleanup
/configure\
 --prefix="$prefix" \
 --libdir=$prefix"/lib/$archdir"\
 --host="m68k-atari-mint"\
 --program-suffix=".ttp"\
 --with-random="/dev/urandom"\
 --enable-static\
 --enable-optimize\
 --enable-warnings\
 --enable-http\
 --enable-gopher\
 --enable-nonblocking\
 --enable-cookies\
 --disable-libtool-lock\
 --disable-verbose\
 --disable-shared\
 --disable-dependency-tracking\
 --disable-manual\
 --disable-curldebug\
 --disable-debug\
 --disable-ipv6\
 --disable-largefile\
 --disable-thread\
 --disable-threaded-resolver\
 --disable-telnet\
 --disable-tftp\
 --disable-dict\
 --disable-pop3\
 --disable-imap\
 --disable-smtp\
 --disable-ldaps\
 --disable-ldap\
 --disable-rtsp\
 --disable-sspi\
 --disable-rtsp\
 --without-polarssl\
 --without-cyassl\
 --without-nss\
 --without-libssh2\
 --without-librtmp\
 --without-libidn\
 --without-gnutls
# --with-ares="/usr/m68k-atari-mint/lib/"$archdir\
# --enable-ares

make CFLAGS="$CFLAGS_ORG"
make install DESTDIR="$userfs"
cd ..

cd $libjpegpkg
./configure --enable-static --prefix="$userfs$prefix" --host="m68k-atari-mint"
make install
cd ..

cd $libpngpkg
$cleanup
if [ "$cross" = "1" ]
then
	./configure --prefix=$prefix --host=m68k-atari-mint
else
	./configure --prefix=$prefix
fi

make
make install DESTDIR="$userfs"
cd ..

# we only need header files of ldg... 
#cd $libldgpkg
#cp ./include/ldg.h "$userfs$prefix/include"
#cd ..

cd windom
cd src
rm ../lib/gcc/libwindom.a
$cleanup
export M68K_ATARI_MINT_CFLAGS="$CFLAGS"
echo $M68K_ATARI_MINT_CFLAGS
echo "dest:"  $userfs$prefix
if [ "$cross" = "1" ]
then
make cross
cp ../lib/gcc/libwindom.a "$userfs$prefix/lib"
cp ../include/* "$userfs$prefix/include/" -R
else
make -f gcc.mak
cp ./lib/gcc/libwindom.a "$userfs$prefix/lib"
cp ./include/* "$userfs$prefix/include/" -R
fi
cd ../..

fi # END OF TEST 

# set TARGET, so that make clean and build use the same directory.
export TARGET="freemint"

echo "compiling libparserutils..."
cd libparserutils/$libparserutils_version
export CFLAGS="$CFLAGS_ORG -DWITH_ICONV_FILTER"
$cleanup
if [ "$cross" = "1" ]
then
	make TARGET="freemint"
	make install DESTDIR="$userfs" PREFIX="$prefix" TARGET="freemint"
else
#	make install DESTDIR="$userfs" PREFIX="$prefix"
	make install DESTDIR="$archlibdir" PREFIX="$prefix"
fi
cd $nssrctree
export CFLAGS="$CFLAGS_ORG"


echo "compiling libwapcaplet..."
cd libwapcaplet/$libwapcaplet_version || exit 1
$cleanup
if [ "$cross" = "1" ]
then
	make install DESTDIR=$userfs PREFIX=$prefix TARGET="freemint"
	#make install DESTDIR="$userfs/$archdir" TARGET="freemint" 
	#PREFIX=$prefix TARGET="freemint"
else
	make install DESTDIR=$userfs PREFIX=$prefix
	#make install DESTDIR="$archlibdir" PREFIX=$prefix
fi
cd $nssrctree

cd libcss/$libcss_version
$cleanup
if [ "$cross" = "1" ]
then
	make install DESTDIR=$userfs PREFIX=$prefix TARGET="freemint"
#	make install DESTDIR="$archlibdir" TARGET="freemint" 
#PREFIX=$prefix TARGET="freemint"
else
	make install DESTDIR=$userfs PREFIX=$prefix
#	make install DESTDIR="$archlibdir" PREFIX=$prefix
fi
cd $nssrctree

echo "compiling hubbub..." 
pwd
cd hubbub/$hubbub_version || exit 1
$cleanup
if [ "$cross" = "1" ]
then
	make install DESTDIR=$userfs PREFIX=$prefix TARGET="freemint"
else
	make install DESTDIR=$userfs PREFIX=$prefix
fi
cd $nssrctree


echo "compiling libnsgif..." 
cd libnsgif/$libnsgif_version || exit 1
$cleanup
if [ "$cross" = "1" ]
then
	make install DESTDIR=$userfs PREFIX=$prefix TARGET="freemint"
else
	make install DESTDIR=$userfs PREFIX=$prefix
fi
cd $nssrctree

echo "compiling libnsbmp..." 
cd libnsbmp/$libnsbmp_version || exit 1
$cleanup
if [ "$cross" = "1" ]
then
	make install DESTDIR=$userfs PREFIX=$prefix TARGET="freemint"
else
	make install DESTDIR=$userfs PREFIX=$prefix
fi
cd $nssrctree


if [ "$withnsfb" = "1" ]
then
	echo "compiling libnsfb..."
	cd libnsfb/$libnsfb_version || exit 1
	$cleanup
	if [ "$cross" = "1" ]
	then
		make install DESTDIR=$userfs PREFIX=$prefix TARGET="freemint"
	else
		make install DESTDIR=$userfs PREFIX=$prefix
	fi
	cd $nssrctree
else
	echo "libnsfb skipped"
fi


if [ "$arch" = "68000" ]
then
	echo "No library relocation needed!"
else
	echo "Relocation Libraries to :"
	echo $archlibdir
	srclibdir=$userfs$prefix"/lib/"
	srcpkgdir=$userfs$prefix"/lib/pkgconfig"
	mkdir $archlibdir
	rm $archlib/*.a 
	mv $srclibdir*.a  $archlibdir/ -v
	mv $srcpkgdir $archlibdir -v
fi

echo "please add symlink to libxml2/libxml within /usr/m68k-atari-mint/include"
echo "please add symlink to freetype2/freetype within /usr/m68k-atari-mint/include"
exit 0
