#! /bin/sh

set -e
python setup.py build 

cd ..
# TODO: FIXME !!
export PYTHONPATH=$PYTHONPATH:bindings/mediacontrol-python/build/lib.linux-i686-2.3:test/build/lib.linux-i686-2.3:test/build/lib.linux-x86_64-2.3

export LD_LIBRARY_PATH=src/.libs/

python test/test.py -v 2>&1|perl  -e \
'$bold = "\033[1m";
$grey  = "\033[37m";
$green  = "\033[32m";
$blue  = "\033[34m";
$red  = "\033[31m";
$reset = "\033[0m";

# Combinations
$info   = $reset;
$ok     = $green;
$err    = $red.$bold;

while(<STDIN>)
{
     $line = $_;
     chomp $line;
     if( $line =~ s/^(\[[A-z0-9]*\]\s.*)\.\.\.\sok$/$info$1\.\.\.$ok ok/g || 
         $line =~ s/^(\[[A-z0-9]*\]\s.*)\.\.\.\sFAIL$/$info$1\.\.\.$err FAIL/g||
         $line =~ s/^(\[[A-z0-9]*\]\s.*)\.\.\.(.)*$/$info$1\.\.\.$2/g || 
         $line =~ s/^(ok)$/$ok$1/ig || $line =~ s/^FAIL$/$err FAIL/g || 
         $line =~ s/(Ran\s.*)/$info$1/g )
     {
        print $line.$reset."\n";
     }
     else
     {
        print $grey.$line."\n";
     }
}'

