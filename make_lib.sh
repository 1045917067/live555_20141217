#!/bin/sh

export CROSS_COMPILE=arm-arago-linux-gnueabi-

make clean

make

ar -cr librtsp.a BasicUsageEnvironment/*.o groupsock/*.o liveMedia/*.o \
UsageEnvironment/*.o server_system/*.o client_system/*.o

