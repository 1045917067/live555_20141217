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
// A framed source that read frame from callback function.
// Implementation

#include "osa_status.h"
#include "H264VideoCBFramedSource.hh"

#define NALU_TYPE(c)		        ((c) & 0x1f)

#define H264VIDEO_FRAME_SIZE_MAX    (512 * 1024)

extern "C"
{
extern int h264dec_seq_parameter_set(uint8_t *buf, int size, SPS *sps_ptr);
};

H264VideoCBFramedSource* H264VideoCBFramedSource::createNew(UsageEnvironment &env, STREAM_CALLBACK_FXN fxn, void *userdata)
{
    return new H264VideoCBFramedSource(env, fxn, userdata);
}

H264VideoCBFramedSource::H264VideoCBFramedSource(UsageEnvironment &env, STREAM_CALLBACK_FXN fxn, void *userdata)
    : CBFramedSource(env, fxn, userdata),
      m_av_type(AV_TYPE_VIDEO),
      m_retval(OSA_SOK)
{
    (*m_cbs_fxn)(m_buffer, sizeof(m_buffer), NULL, 0, m_userdata);
}

H264VideoCBFramedSource::~H264VideoCBFramedSource()
{
    (*m_cbs_fxn)(m_buffer, sizeof(m_buffer), NULL, 1, m_userdata);
}

unsigned H264VideoCBFramedSource::maxFrameSize() const
{
    return (H264VIDEO_FRAME_SIZE_MAX);
}

void H264VideoCBFramedSource::doGetNextFrame()
{
    fFrameSize = 0;

    m_retval = (*m_cbs_fxn)(fTo, fMaxSize, &fFrameSize, 2, m_userdata);

    if (OSA_ISERROR(m_retval) || fFrameSize == 0) {
        handleClosure(this);
    }

    // Complete delivery to the client:
    FramedSource::afterGetting(this);
}
