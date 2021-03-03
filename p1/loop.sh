#!/bin/bash

i=0

while :
do
	echo "----------------------INTENTO $i-----------------------"
	((i=i+1))
	./bank -t 3 -i 2 -a 12 -d 1	
	sleep 0.1
done
