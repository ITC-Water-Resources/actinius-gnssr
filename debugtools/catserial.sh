#!/usr/bin/bash

device=/dev/ttyUSB0

#figure out USB device
found=0
for i in `seq 0 4`
do
	device=/dev/ttyUSB${i}
	if [ -e "$device" ]
	then
		found=1
		break
	fi
done

if  [ ! $found ] 
then
	echo Did not find a an appropriate USB serial device at /dev/ttyUSB0... 
else
	echo using ${device}
fi




baud=115200

stty -F $device ${baud} cs8 -ixon
cat $device
