#!/bin/bash

TURBO_DISABLED="0"
TURBO_ENABLED="1"
ACTUAL_STATE=$(</sys/devices/system/cpu/cpufreq/boost)
        perf stat -e instructions,cycles -o out.txt sleep 1.0
while :
do
        sed -i 's/,/./g' out.txt
        ipc=$(egrep "insn" out.txt | awk '{print $4}')
        #echo "ipc = $ipc"
        
        if [ $(echo "$ipc>$IPC_TARGET"| bc) -eq 0 ] && [ $ACTUAL_STATE -eq $TURBO_ENABLED ] ;
        then
                ACTUAL_STATE=$TURBO_DISABLED    
                echo $TURBO_DISABLED > /sys/devices/system/cpu/cpufreq/boost

        elif [ $(echo "$ipc<=$IPC_TARGET"| bc) -eq 0 ] && [ $ACTUAL_STATE -eq $TURBO_DISABLED ];
        then
                ACTUAL_STATE=$TURBO_ENABLED
                echo $TURBO_ENABLED > /sys/devices/system/cpu/cpufreq/boost
        fi
        perf stat -e instructions,cycles -o out.txt sleep $TIME_TARGET
done
