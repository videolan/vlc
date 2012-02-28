#!/bin/sh
# Copyright (C) 2010 VideoLAN
# License: GPLv2
#----------------------------------------------------------------------------
# Traffic shaping for HTTP Live Streaming client tests.
#----------------------------------------------------------------------------
# Requires: iproute2
#----------------------------------------------------------------------------
#qdisc pfifo_fast 0: root refcnt 2 bands 3 priomap  1 2 2 2 1 2 0 0 1 1 1 1 1 1 1 1
#
TC=tc
INTF="eth0"
RATE="500kbit"
BURST="20kbit"
PEAK="520kbit"
MTU="1500"

set +e

# Shaping
function traffic_shaping() {
    ${TC} qdisc add    \
          dev ${INTF}  \
          root         \
          tbf          \
          rate ${RATE} \
          burst ${BURST} \
          latency 70ms \
          peakrate ${PEAK} \
          mtu ${MTU}
    RESULT=$?
}

# tc qdisc add dev eth2 root tbf rate 50kbit burst 2kbit latency 70ms peakrate 52kbit mtu 1500

traffic_shaping
if test "${RESULT}" != "0"; then
    exit 1
fi
exit 0
