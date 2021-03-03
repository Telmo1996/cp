#!/bin/bash

i=0

while :
do
	echo "----------------------INTENTO $i -----------------------"
	((i=i+1))
	./bank -t 3 -i 20 -a 6 -d 1	
done
