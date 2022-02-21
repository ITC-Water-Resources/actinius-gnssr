#!/usr/bin/bash

device=/dev/ttyUSB0
buas=115200

stty -F $device ${baud} cs8 -ixon
cat $device
