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
// A source object for AAC audio files in ADTS format
// Implementation

#include "ADTSAudioCBFramedSource.hh"
#include "InputFile.hh"
#include <GroupsockHelper.hh>

////////// ADTSAudioCBFramedSource //////////

#define ADTSAUDIO_FRAME_SIZE_MAX    (512 * 1024)


static unsigned const samplingFrequencyTable[16] = {
  96000, 88200, 64000, 48000,
  44100, 32000, 24000, 22050,
  16000, 12000, 11025, 8000,
  7350, 0, 0, 0
};

ADTSAudioCBFramedSource*
ADTSAudioCBFramedSource::createNew(UsageEnvironment& env, STREAM_CALLBACK_FXN fxn, void *userdata) {

	ADTSAudioFrameParser *parser = new ADTSAudioFrameParser(ADTSAUDIO_FRAME_SIZE_MAX, fxn, userdata);

  do {

    // Now, having opened the input file, read the fixed header of the first frame,
    // to get the audio stream's parameters:
    unsigned char fixedHeader[4]; // it's actually 3.5 bytes long
    unsigned len = 0;

    len = parser->parserHeader((char *)fixedHeader, sizeof fixedHeader);

    if (len < sizeof fixedHeader) {
        break;
    }

    // Check the 'syncword':
    if (!(fixedHeader[0] == 0xFF && (fixedHeader[1]&0xF0) == 0xF0)) {
      env.setResultMsg("Bad 'syncword' at start of ADTS file");
      break;
    }

    // Get and check the 'profile':
    u_int8_t profile = (fixedHeader[2]&0xC0)>>6; // 2 bits
    if (profile == 3) {
      env.setResultMsg("Bad (reserved) 'profile': 3 in first frame of ADTS file");
      break;
    }

    // Get and check the 'sampling_frequency_index':
    u_int8_t sampling_frequency_index = (fixedHeader[2]&0x3C)>>2; // 4 bits
    if (samplingFrequencyTable[sampling_frequency_index] == 0) {
      env.setResultMsg("Bad 'sampling_frequency_index' in first frame of ADTS file");
      break;
    }

    // Get and check the 'channel_configuration':
    u_int8_t channel_configuration
      = ((fixedHeader[2]&0x01)<<2)|((fixedHeader[3]&0xC0)>>6); // 3 bits

    // If we get here, the frame header was OK.
    // Reset the fid to the beginning of the file:
#if 0
#ifndef _WIN32_WCE
    rewind(fid);
#else
    SeekFile64(fid, SEEK_SET,0);
#endif
#endif
#ifdef DEBUG
    fprintf(stderr, "Read first frame: profile %d, "
	    "sampling_frequency_index %d => samplingFrequency %d, "
	    "channel_configuration %d\n",
	    profile,
	    sampling_frequency_index, samplingFrequencyTable[sampling_frequency_index],
	    channel_configuration);
#endif
    return new ADTSAudioCBFramedSource(env, fxn, userdata, profile,
				   sampling_frequency_index, channel_configuration, parser);
  } while (0);

  // An error occurred:
  delete parser;

  return NULL;

}

ADTSAudioCBFramedSource::ADTSAudioCBFramedSource(UsageEnvironment &env, STREAM_CALLBACK_FXN fxn, void *userdata, u_int8_t profile,
		u_int8_t samplingFrequencyIndex, u_int8_t channelConfiguration,ADTSAudioFrameParser *parser)
: CBFramedSource(env, fxn, userdata),
    m_parser(parser), 
	m_av_type(AV_TYPE_AUDIO),
	m_retval(OSA_SOK)
{
  fSamplingFrequency = samplingFrequencyTable[samplingFrequencyIndex];
  fNumChannels = channelConfiguration == 0 ? 2 : channelConfiguration;
  fuSecsPerFrame
    = (1024/*samples-per-frame*/*1000000) / fSamplingFrequency/*samples-per-second*/;

  // Construct the 'AudioSpecificConfig', and from it, the corresponding ASCII string:
  unsigned char audioSpecificConfig[2];
  u_int8_t const audioObjectType = profile + 1;
  audioSpecificConfig[0] = (audioObjectType<<3) | (samplingFrequencyIndex>>1);
  audioSpecificConfig[1] = (samplingFrequencyIndex<<7) | (channelConfiguration<<3);
  sprintf(fConfigStr, "%02X%02x", audioSpecificConfig[0], audioSpecificConfig[1]);

//	(*m_cbs_fxn)(m_buffer, sizeof(m_buffer), NULL, 0, m_userdata);

}

ADTSAudioCBFramedSource::~ADTSAudioCBFramedSource() 
{
    delete m_parser;
	(*m_cbs_fxn)(m_buffer, sizeof(m_buffer), NULL, 1, m_userdata);
}

unsigned ADTSAudioCBFramedSource::maxFrameSize() const
{
    return (ADTSAUDIO_FRAME_SIZE_MAX);
}

// Note: We should change the following to use asynchronous file reading, #####
// as we now do with ByteStreamFileSource. #####
void ADTSAudioCBFramedSource::doGetNextFrame() 
{
#if 0
	fFrameSize = 0;

	m_retval = (*m_cbs_fxn)(fTo, fMaxSize, &fFrameSize, 2, m_userdata);

	if (OSA_ISERROR(m_retval) || fFrameSize == 0) {
		handleClosure(this);
	}
#else
	// Begin by reading the 7-byte fixed_variable headers:
	unsigned char headers[7];
	unsigned len = 0;

	len = m_parser->parserHeader((char *)headers, sizeof headers);

	if (len < sizeof headers) {
		// The input source has ended:
		handleClosure(this);
		return;
	}

	// Extract important fields from the headers:
	Boolean protection_absent = headers[1]&0x01;
	u_int16_t frame_length
		= ((headers[3]&0x03)<<11) | (headers[4]<<3) | ((headers[5]&0xE0)>>5);
#ifdef DEBUG
	u_int16_t syncword = (headers[0]<<4) | (headers[1]>>4);
	fprintf(stderr, "Read frame: syncword 0x%x, protection_absent %d, frame_length %d\n", syncword, protection_absent, frame_length);
	if (syncword != 0xFFF) fprintf(stderr, "WARNING: Bad syncword!\n");
#endif
	unsigned numBytesToRead
		= frame_length > sizeof headers ? frame_length - sizeof headers : 0;

	// If there's a 'crc_check' field, skip it:
	if (!protection_absent) {
		//SeekFile64(fFid, 2, SEEK_CUR);
		m_parser->skipBytes(2);
		numBytesToRead = numBytesToRead > 2 ? numBytesToRead - 2 : 0;
	}

	// Next, read the raw frame data into the buffer provided:
	if (numBytesToRead > fMaxSize) {
		fNumTruncatedBytes = numBytesToRead - fMaxSize;
		numBytesToRead = fMaxSize;
	}
	//int numBytesRead = fread(fTo, 1, numBytesToRead, fFid);
	int numBytesRead = m_parser->parserFrame((char *)fTo, numBytesToRead);
	if (numBytesRead < 0) numBytesRead = 0;
	fFrameSize = numBytesRead;
	fNumTruncatedBytes += numBytesToRead - numBytesRead;

	// Set the 'presentation time':
	if (fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0) {
		// This is the first frame, so use the current time:
		gettimeofday(&fPresentationTime, NULL);
	} else {
		// Increment by the play time of the previous frame:
		unsigned uSeconds = fPresentationTime.tv_usec + fuSecsPerFrame;
		fPresentationTime.tv_sec += uSeconds/1000000;
		fPresentationTime.tv_usec = uSeconds%1000000;
	}

	fDurationInMicroseconds = fuSecsPerFrame;
#endif

	// Complete delivery to the client:
	FramedSource::afterGetting(this);

}

ADTSAudioFrameParser::ADTSAudioFrameParser(unsigned buf_size, STREAM_CALLBACK_FXN fxn, void *userdata)
  : m_cbs_fxn(fxn),
    m_userdata(userdata),
    m_cur_frm_idx(0),
    m_next_frm_idx(0),
    m_buf_len(0),
    m_buf_size(buf_size)
{
    m_buffer = new unsigned char[m_buf_size * 2];
}

unsigned ADTSAudioFrameParser::parserHeader(char *hdr, unsigned size)
{
	int     status = 0;
	size_t  read_len = 0;

	this->m_cur_frm_idx = 0;


        if (m_cbs_fxn != NULL) {
	    status = (*m_cbs_fxn)(m_buffer, m_buf_size, &read_len, 2, m_userdata);
           
            if (status < 0) {
                return 0;
            }
            
            m_buf_len = read_len;
        }

        memcpy(hdr, m_buffer + this->m_cur_frm_idx, size);

	this->m_cur_frm_idx += size;
	
	return size;
}

unsigned ADTSAudioFrameParser::parserFrame(char *frm, unsigned size)
{
    memcpy(frm, m_buffer + m_cur_frm_idx, size);

    return size;
}

void ADTSAudioFrameParser::skipBytes(unsigned size)
{
    m_cur_frm_idx += size;
}

ADTSAudioFrameParser::~ADTSAudioFrameParser()
{
   delete[] m_buffer;
}

