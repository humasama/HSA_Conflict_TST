#!/bin/bash
killall -9 mc-mapping
./mc-mapping -c 1 -i 10000000 -l 6 -r 13 -b 8 >& mc1 & 
sleep 1
./mc-mapping -c 2 -i 10000000 -l 6 -r 13 -b 8 >& mc2 &
sleep 1
./mc-mapping -c 3 -i 10000000 -l 6 -r 13 -b 8 -v >& mc3 &

