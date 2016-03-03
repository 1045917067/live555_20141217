#!/bin/bash
rm rtsp_daemon
#arm-arago-linux-gnueabi-gcc -o rtsp_daemon main.c -L"." -lrtsp -lstdc++ -lpthread
gcc -o rtsp_daemon main.c -L"." -lrtsp -lstdc++ -lpthread
