#!/bin/sh

# where to store the processed list of warnings
WARNING_LIST=/tmp/warning-list

if [ $# -gt 1 ]; then
  if [ -f $1 ]; then
    cp $1 ${WARNING_LIST}
  else
    echo "Need a valid warning file"
    exit 1
  fi
else
  make clean 2>&1 >/dev/null
  make nsgtk 2>&1 |grep "warning:" | sort | uniq > ${WARNING_LIST}
fi

for blamefile in $(cat ${WARNING_LIST} | cut -f 1 -d ':'  | sort | uniq ); do
  if [ -f ${blamefile} ]; then
    svn blame ${blamefile} >/tmp/blame

    cat ${WARNING_LIST} | grep "^${blamefile}" >/tmp/blame-warnings

    while read warning; do
      echo ${warning}

      lineno=$(echo ${warning} | cut -f 2 -d ':' ; )

      cat /tmp/blame | head -n ${lineno} | tail -n 1

    done < /tmp/blame-warnings
    rm /tmp/blame-warnings
  else
    echo "Unable to find ${blamefile}"
  fi
done 
