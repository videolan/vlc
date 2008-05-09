#!/bin/sh
echo "Searching $1"
for file in `find $1 -name "Modules.am"`; do
     cmake_file=`dirname $file`/CMakeLists.txt
     echo "Creating $cmake_file"
     cat $file | perl -ne 's/\\\n/ /g; print' | sed -n -e "s/^SOURCES_\([^\ ]*\)[ ]*=\(.*\)$/vlc_add_module( \1 \2 )/p" | tr '\t' ' '  | sed 's/  */ /g'  | sed 's/\$(NULL)//g' > $cmake_file
     dir=`dirname $file`
     echo "" >> $cmake_file
     for subdirfile in `find $dir -name "Modules.am"`; do
        if [ "$subdirfile" != "$file" ]; then
          subdir=`echo \`dirname $subdirfile\` | sed -e "s:$dir::" | sed -e "s:^/::"`
          echo "add_subdirectory( $subdir )" >> $cmake_file
        fi
     done
done
