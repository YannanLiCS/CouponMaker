#!/bin/bash
llc --march=msp430 -mcpu=msp430 -mhwmult=none $1 -o 1.s
msp430-gcc -mmcu=msp430f1611 -Wl,-Map=timertest.map 1.s -c -o 1.o
rm *.s
cd mspsim-master/tests
make clean
mv ../../1.o .
make
cd ..
make
java -classpath "build/:lib/jipv6.jar:lib/jfreechart-1.0.11.jar:lib/jcommon-1.0.14.jar:lib/json-simple-1.1.1.jar" se.sics.mspsim.platform.sky.SkyNode tests/cputest.firmware 
