#! /bin/sh
# Piggy list consistency checker

LANG=C
export LANG

TEMPFILE=/tmp/vlclist.tmp.$$
LISTFILE=LIST
LISTFILE2=/tmp/vlclist2.tmp.$$
LISTFILE3=/tmp/vlclist3.tmp.$$


rm -f $TEMPFILE
touch $TEMPFILE

echo "------------------------------------"
echo "Checking that all modules are listed"
echo "------------------------------------"

i=0

for modfile in `find . -name "Modules.am"`
do
 for module in `grep "SOURCES_" $modfile|awk '{print $1}'|awk 'BEGIN {FS="SOURCES_"};{print $2}'`
 do
  echo $module >> $TEMPFILE
  if [ `grep " \* $module:" $LISTFILE |wc -l` = 0 ]
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

for module in `grep " \* " $LISTFILE|awk '{print $2}'|sed s,':',,g `
do
 if [ `grep $module $TEMPFILE|wc -l` = 0 ]
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

rm -f $LISTFILE2
touch $LISTFILE2
rm -f $LISTFILE3
touch $LISTFILE3

grep " \* " $LISTFILE  >> $LISTFILE2

sort -n $LISTFILE2 >> $LISTFILE3

i=`diff $LISTFILE2 $LISTFILE3|wc -l`
diff -u $LISTFILE2 $LISTFILE3

if [ $i = 0 ]
then 
  echo "OK"
fi


echo ""
echo "`cat $TEMPFILE| wc -l` modules listed in Modules.am files"

rm -f $TEMPFILE
rm -f $LISTFILE2
rm -f $LISTFILE3
