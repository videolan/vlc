#!/bin/bash
ares-setup-device --add lgtv -i '{"host":"192.168.0.109","port":"9922","username":"prisoner","privateKey":"/home/gabor/.ssh/lgtv_plain","description":"LG TV"}'
ares-setup-device --default lgtv
ares-setup-device --list
