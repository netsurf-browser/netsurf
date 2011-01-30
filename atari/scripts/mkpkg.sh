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
dst=$src"atari/pkg/"
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
mkdir $dst"res"
mkdir $dst"res/icons"
mkdir $dst"res/fonts"
cp $src"ns.prg" $dst
chmod +x $dst"ns.prg"
strip $dst"ns.prg"
stack -S 1000k $dst"ns.prg"

cp $src"atari/res/" $dst -rL
cp $src"\!NetSurf/Resources/AdBlock,f79" $dst"res/adblock.css" -rL
cp $src"\!NetSurf/Resources/CSS,f79" $dst"res/default.css" -rL
cp $src"\!NetSurf/Resources/CSS,f79" $dst"res/quirks.css" -rL
cp $src"\!NetSurf/Resources/SearchEngines" $dst"res/search" -rL
cp $src"\!NetSurf/Resources/ca-bundle" $dst"res/cabundle" -rL
cp $src"\!NetSurf/Resources/en/Messages" $dst"res/messages" -rL
cp $src"\!NetSurf/Resources/Icons/content.png" $dst"res/icons/content.png" -rL
cp $src"\!NetSurf/Resources/Icons/directory.png" $dst"res/icons/dir.png" -rL

#remove uneeded files: 
rm $dst"res/netsurf.rsm"
rm $dst"res/netsurf.rsh"


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
atari_font_driver:freetype
homepage_url:file:///./res/blank
http_proxy:0
http_proxy_host:
http_proxy_port:8123
http_proxy_auth:0
http_proxy_auth_user:
http_proxy_auth_pass:
suppress_curl_debug:1
font_size:120
font_min_size:80
#font_sans:Sans
#font_serif:Serif
#font_mono:Monospace
#font_cursive:Serif
#font_fantasy:Serif
accept_language:
accept_charset:
memory_cache_size:204800
disc_cache_age:28
block_advertisements:0
minimum_gif_delay:0
send_referer:1
animate_images:1
expire_url:28
#font_default:1
ca_bundle:./res/cabundle
ca_path:./res/certs
cookie_file:./res/Cookies
cookie_jar:./res/Cookies
search_url_bar:0
search_provider:0
url_suggestion:0
window_x:0
window_y:0
window_width:0
window_height:0
window_screen_width:0
window_screen_height:0
scale:100
incremental_reflow:1
min_reflow_period:200
tree_icons_dir:./res/icons
core_select_menu:1
max_fetchers:16
max_fetchers_per_host:2
max_cached_fetch_handles:6
target_blank:1
margin_top:10
margin_bottom:10
margin_left:10
margin_right:10
export_scale:70
suppress_images:0
remove_backgrounds:0
enable_loosening:1
enable_PDF_compression:1
enable_PDF_password:0
render_resample:0
downloads_clear:0
request_overwrite:1
downloads_directory:./
url_file:./res/URLs
button_type:2
disable_popups:0
disable_plugins:0
history_age:0
hover_urls:0
focus_new:0
new_blank:0
hotlist_path:./res/Hotlist
current_theme:0
" > $dst"Choices"

cd $dst
tar cvf - ./* | gzip -c > ns.tar.gz

echo
exit 0
