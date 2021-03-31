#!/bin/bash

TURBO_DISABLED="0"
TURBO_ENABLED="1"

        perf stat -e instructions,cycles -o out.txt sleep 1.0
while :
do
        sed -i 's/,/./g' out.txt
        ipc=$(egrep "insn" out.txt | awk '{print $4}')
        #echo "ipc = $ipc"

        if [ $(echo "$ipc>0.6"| bc) -eq 0 ];
        then
                echo $TURBO_DISABLED > /sys/devices/system/cpu/cpufreq/boost

        elif [ $(echo "$ipc<=0.6"| bc) -eq 0 ];
        then
                echo $TURBO_ENABLED > /sys/devices/system/cpu/cpufreq/boost
        fi
        perf stat -e instructions,cycles -o out.txt sleep 5.0
done
