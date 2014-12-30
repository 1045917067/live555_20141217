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
// "liveMedia"
// Copyright (c) 1996-2013 Live Networks, Inc.  All rights reserved.
// A source that get frame from callback.
// C++ header

#ifndef _CB_FRAMED_SOURCE_HH
#define _CB_FRAMED_SOURCE_HH

#ifndef _FRAMED_SOURCE_HH
#include "FramedSource.hh"
#endif

#include "scallback.h"

class CBFramedSource: public FramedSource
{
protected:  // we're the virtual function
    CBFramedSource(UsageEnvironment &env, STREAM_CALLBACK_FXN fxn, void *userdata);

    virtual ~CBFramedSource();

protected:

    STREAM_CALLBACK_FXN m_cbs_fxn;
    void *              m_userdata;
};

#endif
