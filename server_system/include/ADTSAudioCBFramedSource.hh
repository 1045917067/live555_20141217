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

#ifndef _H264VIDEO_CB_FRAMED_SOURCE_HH
#define _H264VIDEO_CB_FRAMED_SOURCE_HH

#include "osa_status.h"

#ifndef _CB_FRAMED_SOURCE_HH
#include "CBFramedSource.hh"
#endif


class ADTSAudioFrameParser;

class ADTSAudioCBFramedSource: public CBFramedSource
{
public:
	static ADTSAudioCBFramedSource * createNew(UsageEnvironment &env, STREAM_CALLBACK_FXN fxn, void *userdata);

	unsigned samplingFrequency() const { return fSamplingFrequency; }
	unsigned numChannels() const { return fNumChannels; }
	char const* configStr() const { return fConfigStr; }
	// returns the 'AudioSpecificConfig' for this stream (in ASCII form)

protected:  // we're the virtual function

    ADTSAudioCBFramedSource(UsageEnvironment &env, STREAM_CALLBACK_FXN fxn, void *userdata, u_int8_t profile,
		      u_int8_t samplingFrequencyIndex, u_int8_t channelConfiguration, ADTSAudioFrameParser *parser);
	// called only by createNew()

    virtual ~ADTSAudioCBFramedSource();

private:
	// size of the largest possible frame that we may serve, or 0
	// if no such maximum is known (default)
	virtual unsigned maxFrameSize() const;

	// Redefine virtual function
	virtual void doGetNextFrame();

private:
	ADTSAudioFrameParser *m_parser;
private:
	unsigned fSamplingFrequency;
	unsigned fNumChannels;
	unsigned fuSecsPerFrame;
	char fConfigStr[5];

    av_type_t       m_av_type;
    status_t        m_retval;
    unsigned char   m_buffer[128];

};
class ADTSAudioFrameParser
{
public:

      ADTSAudioFrameParser(unsigned buf_size, STREAM_CALLBACK_FXN m_cbs_fxn, void *m_userdata);

      unsigned parserHeader(char *hdr, unsigned size);

      unsigned parserFrame(char *frm, unsigned size);

      void skipBytes(unsigned size);

     ~ADTSAudioFrameParser();

private:
    STREAM_CALLBACK_FXN m_cbs_fxn;
    void *m_userdata;

    unsigned m_cur_frm_idx;
    unsigned m_next_frm_idx;

    unsigned m_buf_len;
    unsigned m_buf_size;

    unsigned char *m_buffer;
};


#endif
