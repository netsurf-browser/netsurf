#!/bin/sh
# call: compile-xib.sh [xib file] [language] [(optional output nib file)]
DIR=`dirname "$1"`
XIB=`basename -s .xib "$1"`

STRINGS_FILE="$DIR/$2.lproj/$XIB.xib.strings"
TRANSLATE=""
if [ -f $STRINGS_FILE ] 
then
	TRANSLATE="--strings-file $STRINGS_FILE"
fi

OUTPUT="$2.$XIB.nib"

if [ "x$3" != "x" ]
then
	OUTPUT="$3"
fi

exec /usr/bin/ibtool $TRANSLATE --compile $OUTPUT $1
