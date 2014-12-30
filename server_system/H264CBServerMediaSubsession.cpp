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
// on demand, from a H264 video callback.
// Implementation

#include "H264CBServerMediaSubsession.hh"
#include "H264VideoCBFramedSource.hh"
#include "H264VideoRTPSink.hh"
#include "H264VideoStreamFramer.hh"

H264CBServerMediaSubsession *
H264CBServerMediaSubsession::createNew(UsageEnvironment &env, STREAM_CALLBACK_FXN fxn, void *userdata)
{
  return new H264CBServerMediaSubsession(env, fxn, userdata);
}

H264CBServerMediaSubsession::H264CBServerMediaSubsession(UsageEnvironment& env, STREAM_CALLBACK_FXN fxn, void *userdata)
    : CBServerMediaSubsession(env, fxn, userdata),
      fAuxSDPLine(NULL),
      fDoneFlag(0),
      fDummyRTPSink(NULL),
      m_cbs_fxn(fxn),
      m_userdata(userdata),
      m_av_type(AV_TYPE_VIDEO)
{
}

H264CBServerMediaSubsession::~H264CBServerMediaSubsession()
{
    delete[] fAuxSDPLine;
}

static void afterPlayingDummy(void *clientData)
{
    H264CBServerMediaSubsession* subsess = (H264CBServerMediaSubsession*)clientData;
    subsess->afterPlayingDummy1();
}

void H264CBServerMediaSubsession::afterPlayingDummy1()
{
    // Unschedule any pending 'checking' task:
    envir().taskScheduler().unscheduleDelayedTask(nextTask());
    // Signal the event loop that we're done:
    setDoneFlag();
}

static void checkForAuxSDPLine(void *clientData)
{
    H264CBServerMediaSubsession* subsess = (H264CBServerMediaSubsession*)clientData;
    subsess->checkForAuxSDPLine1();
}

void H264CBServerMediaSubsession::checkForAuxSDPLine1()
{
    char const* dasl;

    if (fAuxSDPLine != NULL) {
        // Signal the event loop that we're done:
        setDoneFlag();
    } else if (fDummyRTPSink != NULL && (dasl = fDummyRTPSink->auxSDPLine()) != NULL) {
        fAuxSDPLine = strDup(dasl);
        fDummyRTPSink = NULL;

        // Signal the event loop that we're done:
        setDoneFlag();
    } else if (!fDoneFlag) {
        // try again after a brief delay:
        int uSecsToDelay = 100000; // 100 ms
        nextTask() = envir().taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)checkForAuxSDPLine, this);
    }
}

char const* H264CBServerMediaSubsession::getAuxSDPLine(RTPSink* rtpSink, FramedSource* inputSource)
{
    if (fAuxSDPLine != NULL) return fAuxSDPLine; // it's already been set up (for a previous client)

    if (fDummyRTPSink == NULL) {
        // we're not already setting it up for another, concurrent stream
        // Note: For H264 video files, the 'config' information ("profile-level-id" and "sprop-parameter-sets") isn't known
        // until we start reading the file.  This means that "rtpSink"s "auxSDPLine()" will be NULL initially,
        // and we need to start reading data from our file until this changes.
        fDummyRTPSink = rtpSink;

        // Start reading the file:
        fDummyRTPSink->startPlaying(*inputSource, afterPlayingDummy, this);

        // Check whether the sink's 'auxSDPLine()' is ready:
        checkForAuxSDPLine(this);
    }

    envir().taskScheduler().doEventLoop(&fDoneFlag);

    return fAuxSDPLine;
}

FramedSource* H264CBServerMediaSubsession::createNewStreamSource(unsigned /*clientSessionId*/, unsigned &estBitrate)
{
    estBitrate = 8000; // kbps, estimate

    // Create the video source:
    H264VideoCBFramedSource *framedSource = H264VideoCBFramedSource::createNew(envir(), m_cbs_fxn, m_userdata);

    if (framedSource == NULL) {
        return NULL;
    }

    // Create a framer for the Video Elementary Stream:
    return H264VideoStreamFramer::createNew(envir(), framedSource);
}

RTPSink* H264CBServerMediaSubsession
::createNewRTPSink(Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource* /*inputSource*/)
{
  return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
}
