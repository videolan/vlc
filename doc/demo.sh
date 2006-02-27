#!/bin/sh

########################################################################
# VLC demo command line generator
# $Id$
########################################################################

#TODO: change on Max OS X
VLC="./vlc --quiet --color "
CMD=""

pyschedelic()
{
  echo -e "\n- Psychedelic demo -\nconfiguration\n"
  echo -en "Please chose an input. Live camera feeds are best.\ninput? "
  read input
  echo -e "\n$VLC --sub-filter marq --marq-position 8 --marq-size 30 --marq-color 16776960 --marq-marquee \"VLC - Psychedelic video filter\" --vout-filter distort --distort-mode psychedelic $input"
}

gradient()
{
  echo -e "\n- Gradient demo -\nconfiguration\n"
  echo -en "Please chose an input. Live camera feeds are best.\ninput? "
  read input
  echo -en "Please chose a logo to display (or multiple logos according to the --logo-file syntax)\nlogo? "
  read logofile
  echo "new a broadcast enabled loop
setup a input $input
setup a output #duplicate{dst=mosaic-bridge,select=video}
control a play" > "`pwd`/demo.vlm"
  echo "VLM batch file saved to `pwd`/demo.vlm"
  echo -e "\n$VLC --sub-filter mosaic:marq:logo --mosaic-width 120 --mosaic-height 90 --mosaic-cols 1 --mosaic-rows 1 --marq-position 8 --marq-size 30 --marq-color 65280 --marq-marquee \"VLC - Gradient video filter\" --logo-file $logofile --vout-filter distort --distort-mode gradient --extraintf telnet --telnet-host localhost --vlm-conf `pwd`/demo.vlm $input"
}

mosaic()
{
  echo -e "\n- Mosaic demo -\nconfiguration\n"
  echo -en "Please chose a background input.\nbackground input? "
  read bg
  echo -en "Please chose a video to blend.\nvideo? "
  read vid
  echo "new a broadcast enabled loop
setup a input $vid
setup a output #duplicate{dst=mosaic-bridge,select=video}
control a play" > "`pwd`/demo.vlm"
  echo "VLM batch file saved to `pwd`/demo.vlm"
  echo -e "\n$VLC --sub-filter mosaic:marq --marq-marque \"VLC - mosaic\" --marq-position 6 --mosaic-width 120 --mosaic-height 90 --mosaic-rows 1 --mosaic-cols 1 --mosaic-alpha 150 --extraintf telnet --telnet-host localhost --vlm-conf `pwd`/demo.vlm $bg"
}

cat << EOF
VLC cool demos script
 1. psychedelic video filter
 2. gradient video filter
 3. mosaic
EOF

echo -n "demo number? "
read choice

case "$choice" in
 1) pyschedelic;;
 2) gradient;;
 3) mosaic;;
 *) echo "Wrong answer ... try again"; exit 1;;
esac

echo -e "\nUse the previous command to run the demo."
echo "Note: make sure that you reset your preferences before running these demos."
