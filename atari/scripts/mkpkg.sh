#!/bin/bash

# this is an small build script to create an package for nsgem
# invoke: mkpkg.sh [-s,-d,-8,-fonts]
#
# Parameters:
#
# -8
# Description: The package will be build for 8.3 filesystems
#              This also defines the -fonts parameter
#
# -fonts
# Description: The package will include the DejaVu fonts package
#              ( 8.3 compatible names )
#
# -fpath
# Description: Set path to dejavu Fonts
#
# -s (srcpath)
# Description: use it like: -s "path to netsurf root" to configure from which
# 	            directory the package files are taken.
#              The Path must have trailing slash!
#
# -d (dstpath)
# Description: use it like: -d "path to dir where the package will be placed"
#              to configure the output path of this script.
#              The path mus have trailing slash!
#

# config variable, set default values
src="/f/netsurf/netsurf/"
dst=$src"atari/nspkg/"
shortfs=0
inc_short_fonts=0
font_src="/usr/share/fonts/truetype/ttf-dejavu/"
framebuffer=0

while [ "$1" != "" ]			# When there are arguments...
do					# Process the next one
  case $1				# Look at $1
  in
   -8)
		shortfs="1"
		shift
	;;

   -fonts)
    	inc_short_fonts="1"
		shift
	;;

	-fpath)
		shift
		font_src=$1
		shift
	;;

	-d)
		shift
		dst=$1
		shift
	;;

	-s)
		shift
		src=$1
		shift
	;;

	*)	echo "Option [$1] not one of  [-8,-fonts,-d,-s,-fpath]";
	exit;;

  esac
done

echo "Building from: "$src
echo "Building in: "$dst
echo "Building for short fs: "$shortfs

if [ "$shortfs" = "1" ]
then
		inc_short_fonts=1
fi

if [ -d "$font_src" ]
then
	echo "Found fonts in $font_src"
else
	echo "Error: TTF Fonts not found ($font_src)!"
	exit 0
fi

set -o verbose
rm $dst -r
mkdir $dst
mkdir $dst"download"
mkdir $dst"res"
mkdir $dst"res/icons"
mkdir $dst"res/fonts"
touch $dst"cookies"
cp $src"atari/doc" $dst -R
cp $src"ns.prg" $dst
chmod +x $dst"ns.prg"
m68k-atari-mint-strip $dst"ns.prg"
m68k-atari-mint-stack -S 256k $dst"ns.prg"

cp $src"atari/res/" $dst -rL
cp $src"!NetSurf/Resources/AdBlock,f79" $dst"res/adblock.css" -rL
cp $src"!NetSurf/Resources/CSS,f79" $dst"res/default.css" -rL
cp $src"!NetSurf/Resources/Quirks,f79" $dst"res/quirks.css" -rL
cp $src"!NetSurf/Resources/internal.css,f79" $dst"res/internal.css" -rL
cp $src"!NetSurf/Resources/SearchEngines" $dst"res/search" -rL
cp $src"!NetSurf/Resources/ca-bundle" $dst"res/cabundle" -rL
cp $src"!NetSurf/Resources/en/Messages" $dst"res/messages" -rL
cp $src"!NetSurf/Resources/Icons/content.png" $dst"res/icons/content.png" -rL
cp $src"!NetSurf/Resources/Icons/directory.png" $dst"res/icons/dir.png" -rL

#remove uneeded files:
rm $dst"res/netsurf.rsm"
rm $dst"res/netsurf.rsh"
rm $dst"res/.svn" -r
rm $dst"res/icons/.svn" -r
rm $dst"res/fonts/.svn" -r
rm $dst"doc/.svn" -r
rm $dst"download/.svn" -r

if [ "$inc_short_fonts" = "1" ]
then
	cp $font_src"DejaVuSans.ttf" $dst"res/fonts/ss.ttf"
	cp $font_src"DejaVuSans-Bold.ttf" $dst"res/fonts/ssb.ttf"
	cp $font_src"DejaVuSans-Oblique.ttf" $dst"res/fonts/ssi.ttf"
	cp $font_src"DejaVuSans-BoldOblique.ttf" $dst"res/fonts/ssib.ttf"
	cp $font_src"DejaVuSansMono.ttf" $dst"res/fonts/mono.ttf"
	cp $font_src"DejaVuSansMono-Bold.ttf" $dst"res/fonts/monob.ttf"
	cp $font_src"DejaVuSansMono-Oblique.ttf" $dst"res/fonts/cursive.ttf"
	cp $font_src"DejaVuSerif.ttf" $dst"res/fonts/s.ttf"
	cp $font_src"DejaVuSerif-Bold.ttf" $dst"res/fonts/sb.ttf"
	cp $font_src"DejaVuSerifCondensed-Bold.ttf" $dst"res/fonts/fantasy.ttf"
fi

#create an simple startup script:
if [ "$framebuffer" = "1" ]
then
echo "NETSURFRES=./res/
export NETSURFRES
./nsfb.prg -v file:///f/" > $dst"ns.sh"
chmod +x $dst"ns.sh"
fi

echo "
atari_screen_driver:vdi
# select font driver, available values: freetype, internal
atari_font_driver:freetype
atari_transparency:1
atari_realtime_move:1
# uncomment the following to show source within editor:
#atari_editor:/path/to/editor.app

# url to start netsurf with ( and new windows )
homepage_url:file://./res/blank

#configure css font settings:
font_size:130
font_min_size:120

# 2.5 MB Cache as default:
memory_cache_size:2048512

# this actually hides advertisements, it still generates network traffic:
block_advertisements:0

#network configuration:
send_referer:1
http_proxy:0
http_proxy_host:
http_proxy_port:8123
http_proxy_auth:0
http_proxy_auth_user:
http_proxy_auth_pass:
suppress_curl_debug:1

# animation configuration ( GIF ):
minimum_gif_delay:50
animate_images:1

# path configuration
ca_bundle:./res/cabundle
ca_path:./res/certs
cookie_file:./res/cookies
url_file:./res/url.db
tree_icons_path:./res/icons
downloads_directory:./download
hotlist_path:./res/hotlist

# enable reflow for interactive content and during fetch:
incremental_reflow:1
# reformat time during fetch:
min_reflow_period:500
core_select_menu:1
max_fetchers:3
max_fetchers_per_host:2
max_cached_fetch_handles:5

# uncomment to configure GUI colors
#gui_colour_bg_1
#gui_colour_fg_1
#gui_colour_fg_2

# allow target=_blank (link opens in new window):
target_blank:1

# options nowhere used currently follow:

# suppres images when exporting pages (to PDF):
# suppress_images:0
# turn off backgrounds for printed content:
#remove_backgrounds:0
# smooth resizing:
#render_resample:1
# enable loosening for printed content:
#enable_loosening:1
# configure disc cache ( currently not implemented )
#expire_url:28
#hover_urls:1
" > $dst"Choices"

cd $dst
tar cvf - ./* | gzip -9 -c > netsurf.tar.gz
zip netsurf.zip -9 -r ./ -x netsurf.tar.gz

echo
exit 0
