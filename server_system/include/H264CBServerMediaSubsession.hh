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
// A 'ServerMediaSubsession' object that creates new, unicast, "RTPSink"s
// on demand, from callback function.
// C++ header

#ifndef _H264CB_SERVER_MEDIA_SUBSESSION_HH
#define _H264CB_SERVER_MEDIA_SUBSESSION_HH

#ifndef _CB_SERVER_MEDIA_SUBSESSION_HH
#include "CBServerMediaSubsession.hh"
#endif

#include "scallback.h"

class H264CBServerMediaSubsession: public CBServerMediaSubsession
{
public:
    static H264CBServerMediaSubsession * createNew(UsageEnvironment &env, STREAM_CALLBACK_FXN fxn, void *userdata);

    // Used to implement "getAuxSDPLine()":
    void checkForAuxSDPLine1();
    void afterPlayingDummy1();

protected:

    H264CBServerMediaSubsession(UsageEnvironment &env, STREAM_CALLBACK_FXN fxn, void *userdata);
    virtual ~H264CBServerMediaSubsession();

    void setDoneFlag() { fDoneFlag = ~0; }

private:
    virtual char const * getAuxSDPLine(RTPSink *rtpSink, FramedSource *inputSource);

    virtual FramedSource * createNewStreamSource(unsigned int clientSessionId, unsigned int &estBitrate);
    virtual RTPSink * createNewRTPSink(Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource *inputSource);

private:
    char* fAuxSDPLine;
    char fDoneFlag; // used when setting up "fAuxSDPLine"
    RTPSink* fDummyRTPSink; // ditto

    STREAM_CALLBACK_FXN m_cbs_fxn;
    void *              m_userdata;
    av_type_t m_av_type;
};

#endif
