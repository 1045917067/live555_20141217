/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// Copyright (c) 1996-2013 Live Networks, Inc.  All rights reserved.
// Media system Usage Environment: Do only SingleStep routine in doEventLoop function.
// C++ header

#ifndef _MEDIA_SYSTEM_TASK_SCHEDULER_HH
#define _MEDIA_SYSTEM_TASK_SCHEDULER_HH

#ifndef _BASIC_USAGE_ENVIRONMENT_HH
#include "BasicUsageEnvironment.hh"
#endif

class MediaSystemTaskScheduler: public BasicTaskScheduler
{
public:
    static MediaSystemTaskScheduler * createNew(unsigned maxSchedulerGranularity = 10000/*microseconds*/);
    // "maxSchedulerGranularity" (default value: 10 ms) specifies the maximum time that we wait (in "select()") before
    // returning to the event loop to handle non-socket or non-timer-based events, such as 'triggered events'.
    // You can change this is you wish (but only if you know what you're doing!), or set it to 0, to specify no such maximum time.
    // (You should set it to 0 only if you know that you will not be using 'event triggers'.)
    virtual ~MediaSystemTaskScheduler();

    // Our own event loop
    void doEventLoop2(char* watchVariable);

protected:

    MediaSystemTaskScheduler(unsigned maxSchedulerGranularity);
    // called only by "createNew()"
};

#endif
