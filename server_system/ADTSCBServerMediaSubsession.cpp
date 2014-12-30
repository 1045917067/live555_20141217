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
// on demand, from an AAC audio file in ADTS format
// Implementation

#include "ADTSCBServerMediaSubsession.hh"
#include "ADTSAudioCBFramedSource.hh"
#include "MPEG4GenericRTPSink.hh"

ADTSCBServerMediaSubsession*
ADTSCBServerMediaSubsession::createNew(UsageEnvironment& env, STREAM_CALLBACK_FXN fxn, void *userdata)
{
  return new ADTSCBServerMediaSubsession(env, fxn, userdata);
}

ADTSCBServerMediaSubsession
::ADTSCBServerMediaSubsession(UsageEnvironment& env, STREAM_CALLBACK_FXN fxn, void *userdata)
	: CBServerMediaSubsession(env, fxn, userdata),
	fAuxSDPLine(NULL),
	fDoneFlag(0),
	fDummyRTPSink(NULL),
	m_cbs_fxn(fxn),
	m_userdata(userdata),
	m_av_type(AV_TYPE_AUDIO)
{
}

ADTSCBServerMediaSubsession
::~ADTSCBServerMediaSubsession() {
}

FramedSource* ADTSCBServerMediaSubsession
::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) {
  estBitrate = 96; // kbps, estimate

  // Create the audio source:
  return ADTSAudioCBFramedSource::createNew(envir(), m_cbs_fxn, m_userdata);

}

RTPSink* ADTSCBServerMediaSubsession
::createNewRTPSink(Groupsock* rtpGroupsock,
		   unsigned char rtpPayloadTypeIfDynamic,
		   FramedSource* inputSource) {
  ADTSAudioCBFramedSource* adtsSource = (ADTSAudioCBFramedSource*)inputSource;
  return MPEG4GenericRTPSink::createNew(envir(), rtpGroupsock,
		  rtpPayloadTypeIfDynamic,
		  adtsSource->samplingFrequency(),
		  "audio", "AAC-hbr", adtsSource->configStr(),
		  adtsSource->numChannels());
}
