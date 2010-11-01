#!/bin/sh

killer()
{
  sleep 2 && ps $1 > /dev/null && touch "telnet_fail" && kill -9 $1
}

wait_or_quit()
{
  wait $1
  if [ `ls telnet_fail 2> /dev/null | wc -l` = 1 ]
  then
    rm -f telnet_fail
    exit 1
  fi
}

# Remove the fail file if needed
rm -f telnet_fail

# Test that VLC handle options correctly
../vlc -I luatelnet &
VLC1=$!
sleep 1
killer $VLC1 &
echo "admin\nshutdown\n" | nc localhost 4212
wait_or_quit $VLC1

../vlc -I luatelnet --telnet-port 4312 &
VLC2=$!
sleep 1
killer $VLC2 &
echo "admin\nshutdown\n" | nc localhost 4312
wait_or_quit $VLC2

../vlc -I luatelnet --telnet-port 1234 --telnet-password bla &
VLC3=$!
sleep 1
killer $VLC3 &
echo "bla\nshutdown\n" | nc localhost 1234
wait_or_quit $VLC3

../vlc -I luatelnet --telnet-port 1234 --telnet-password one_long_password &
VLC4=$!
sleep 1
killer $VLC4 &
echo "one_long_password\nshutdown\n" | nc localhost 1234
wait_or_quit $VLC4

exit 0
