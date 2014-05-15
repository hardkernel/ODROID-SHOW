#!/bin/bash

flag=0
serial="/dev/ttyUSB0"

trap "flag=1" SIGINT SIGKILL SIGTERM

./port_open &
subppid=$!

sleep 0.1
echo -ne "\ec\e[0r" > $serial
sleep 0.3

while true
do
	if [ $flag -ne 0 ] ; then
		echo -ne "\ec\e[1r" > $serial
		kill $subppid
		exit
	fi
		echo -ne "\e[0r" > $serial
		sleep 0.2
		echo -ne "\e[0;0,240;320i" > $serial
		cat penguin.raw > $serial
		sleep 0.1
		echo -ne "\e[1r" > $serial
		sleep 0.2
		echo -ne "\e[0;0,320;240i" > $serial
		cat butterfly.raw > $serial
		sleep 0.1
		echo -ne "\e[0r" > $serial
		sleep 0.2
		echo -ne "\e[0;0,240;320i" > $serial
		cat woof.raw > $serial
		sleep 0.1
		echo -ne "\ec\e[0r" > $serial
		sleep 0.3
		echo -ne "\e[40;10,220;200i" > $serial
		cat paint.raw > $serial
		sleep 0.1
		echo -ne "\ec\e[1r" > $serial
		sleep 0.3
		echo -ne "\e[10;10,190;200i" > $serial
		cat paint.raw > $serial
		sleep 0.1
done
