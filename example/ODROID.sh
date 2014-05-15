#!/bin/bash

flag=0
serialPort="/dev/ttyUSB0"

trap "flag=1" SIGINT SIGKILL SIGTERM

./port_open &
subppid=$!

DATA[0]="ODROID"
DATA[1]="SHOW"

sleep 0.1
echo -ne "\e[5s\e[0r" > $serialPort
sleep 0.1
echo -ne "\ec" > $serialPort
sleep 0.1

while true
do
	if [ $flag -ne 0 ] ; then
		echo -ne "\ec\e[2s\e[1r" > $serialPort
		kill $subppid
		exit
	fi
	for ((j=1; j<8; j++)); do
		echo -ne "\e[25;100f" > $serialPort
		for ((i=0; i<6; i++)); do
			echo -ne "\e[3"$j"m\e[3"$j"m${DATA[0]:$i:1}" > $serialPort
			sleep 0.02
		done
		echo -ne "\eE\e[55;150f" > $serialPort
		for ((i=0; i<4; i++)); do
			echo -ne "\e[3"$j"m\e[3"$j"m${DATA[1]:$i:1}" > $serialPort
			sleep 0.02
		done
	done
done

