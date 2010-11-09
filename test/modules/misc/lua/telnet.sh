#!/bin/sh

TELNET_FAIL="telnet_fail"
PORT=`echo "$$+1024" | bc`

killer()
{
  sleep 2 && ps $1 > /dev/null && touch $TELNET_FAIL && kill -9 $1
}

wait_or_quit()
{
  wait $1
  if [ `ls $TELNET_FAIL 2> /dev/null | wc -l` = 1 ]
  then
    rm -f $TELNET_FAIL
    exit 1
  fi
}

# Remove the fail file if needed
rm -f $TELNET_FAIL

# Test that VLC handle options correctly
../vlc -I luatelnet --telnet-port $PORT &
VLC1=$!
sleep 1
killer $VLC1 &
echo "admin\nshutdown\n" | nc localhost $PORT
wait_or_quit $VLC1

../vlc -I luatelnet --telnet-port $PORT --telnet-password bla &
VLC2=$!
sleep 1
killer $VLC2 &
echo "bla\nshutdown\n" | nc localhost $PORT
wait_or_quit $VLC2

../vlc -I luatelnet --telnet-port $PORT --telnet-password one_long_password &
VLC3=$!
sleep 1
killer $VLC3 &
echo "one_long_password\nshutdown\n" | nc localhost $PORT
wait_or_quit $VLC3

../vlc -I luatelnet --telnet-port $PORT --telnet-password "" &
VLC4=$!
sleep 1
killer $VLC4 &
echo "\nshutdown\n" | nc localhost $PORT
wait_or_quit $VLC4

exit 0
