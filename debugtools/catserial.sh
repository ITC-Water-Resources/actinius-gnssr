#!/usr/bin/bash

device=/dev/ttyUSB1
baud=115200

stty -F $device ${baud} cs8 -ixon
cat $device
