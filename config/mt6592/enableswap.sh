#!/bin/sh
echo 0 > /sys/block/zram0/disksize
/system/bin/tiny_mkswap /dev/block/zram0
/system/bin/tiny_swapon /dev/block/zram0

# Lenovo-sw wuzb1 2014-04-14 enable KSM
echo 100 > /sys/kernel/mm/ksm/pages_to_scan
echo 500 > /sys/kernel/mm/ksm/sleep_millisecs
echo 1 > /sys/kernel/mm/ksm/run
