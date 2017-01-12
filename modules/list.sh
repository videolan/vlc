#! /bin/sh
# Piggy list consistency checker

LANG=C
export LANG

TEMPFILE=/tmp/vlclist.tmp.$$
LISTFILE=MODULES_LIST


rm -f $TEMPFILE
touch $TEMPFILE

echo "------------------------------------"
echo "Checking that all modules are listed"
echo "------------------------------------"

i=0

for modfile in `find . -name "Makefile.am"`
do
 for module in `awk '/^lib.*_plugin_la_SOURCES/{sub(/lib/,""); sub(/_plugin_la_SOURCES/,"",$1); print $1}' "$modfile"`
 do
  echo $module >> $TEMPFILE
  if ! grep -q " \* $module:" $LISTFILE
  then
   echo "$module exists in $modfile, but not listed"
   i=1
  fi
 done
done

if [ $i = 0 ]
then
  echo "OK"
fi

i=0

echo
echo "--------------------------------------"
echo "Checking that all listed modules exist"
echo "--------------------------------------"

for module in `awk -F'[ :]' '/ \* /{print $3}' $LISTFILE`
do
 if ! grep -wq $module $TEMPFILE
 then
  i=1
  echo "$module is listed but does not exist"
 fi
done

if [ $i = 0 ]
then
  echo "OK"
fi

echo
echo "-------------------------------"
echo "Checking for alphabetical order"
echo "-------------------------------"

grep " \* " $LISTFILE | LC_COLLATE=C LC_CTYPE=C sort -c && echo "OK"


echo ""
echo "`sort -u $TEMPFILE | wc -l` modules listed in Makefiles"

rm -f $TEMPFILE
