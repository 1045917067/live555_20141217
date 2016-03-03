#!/bin/bash
rm demux_daemon
#arm-arago-linux-gnueabi-gcc -o rtsp_daemon main.c -L"." -lrtsp -lstdc++ -lpthread
gcc -o demux_daemon demux_main.c -L"." -lrtsp -lstdc++ -lpthread
