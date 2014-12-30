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
// RTP sink for H.264 video (RFC 3984)
// C++ header

#ifndef _H264_VIDEO_CB_RTP_SINK_HH
#define _H264_VIDEO_CB_RTP_SINK_HH

#include "H264VideoRTPSink.hh"

class H264VideoCBRTPSink: public H264VideoRTPSink {
public:
  static H264VideoCBRTPSink* createNew(UsageEnvironment& env, Groupsock* RTPgs, unsigned char rtpPayloadFormat);

protected:
  H264VideoCBRTPSink(UsageEnvironment& env, Groupsock* RTPgs, unsigned char rtpPayloadFormat);
	// called only by createNew()

  virtual ~H264VideoCBRTPSink();

private: // redefined virtual functions:
  virtual char const* auxSDPLine();
  virtual Boolean sourceIsCompatibleWithUs(MediaSource& source);
  virtual Boolean continuePlaying();
  virtual void doSpecialFrameHandling(unsigned fragmentationOffset,
                                      unsigned char* frameStart,
                                      unsigned numBytesInFrame,
                                      struct timeval framePresentationTime,
                                      unsigned numRemainingBytes);
  virtual Boolean frameCanAppearAfterPacketStart(unsigned char const* frameStart,
						 unsigned numBytesInFrame) const;

private: // We can not access parent's private variables, so...
  char* fFmtpSDPLine;
  u_int8_t* fSPS; unsigned fSPSSize; u_int8_t* fPPS; unsigned fPPSSize;
};

#endif
