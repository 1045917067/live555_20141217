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
59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**********/
// Copyright (c) 1996-2015, Live Networks, Inc.  All rights reserved
// A test program that reads a H.264 Elementary Stream video file
// and streams it using RTP
// main program
//
// NOTE: For this application to work, the H.264 Elementary Stream video file *must* contain SPS and PPS NAL units,
// ideally at or near the start of the file.  These SPS and PPS NAL units are used to specify 'configuration' information
// that is set in the output stream's SDP description (by the RTSP server that is built in to this application).
// Note also that - unlike some other "*Streamer" demo applications - the resulting stream can be received only using a
// RTSP client (such as "openRTSP")

#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <strDup.hh>

UsageEnvironment* env;
#if 0
char const* inputFileName = "test.264";
#else
/*
 *  Modified by: zzx
 *
 *  Date       : Dec 29, 2014
 *
 *  Description: User can change options by command line.
 *
 */

char const* inputFileName = NULL;
char const* streamName = NULL;
#endif
H264VideoStreamFramer* videoSource;
RTPSink* videoSink;

void play(); // forward

/*
 *  Modified by: zzx
 *
 *  Date       : Dec 29, 2014
 *
 *  Description: User can change options by command line.
 *
 */

extern double glb_h264_framerate;

#define VALID_OPTIONS       ("p:f:r:n:h")

static void PrintUsage(UsageEnvironment& env, int argc, char* argv[])
{
    env << "Available options:\n"
        "  -p [listen port] : The port this server listen at.\n"
        "  -f [file name  ] : The name of the file to read from.\n"
        "  -n [stream name] : The name of this stream.\n"
        "  -r [frame rate ] : The frame rate of the H.264 stream.\n"
        "  -h               : Print this usage information.\n";
}



int main(int argc, char** argv) {
  // Begin by setting up our usage environment:
  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
  env = BasicUsageEnvironment::createNew(*scheduler);

  /*
   *  Modified by: zzx
   *
   *  Date       : Dec 29, 2014
   *
   *  Description: User can change options by command line.
   *
   */

  unsigned int    port      = 8554;
  unsigned int    frameRate = 25;

  /*
   *  Check the options.
   */
  int opt;
  while((opt = getopt(argc, argv, VALID_OPTIONS)) != -1) {
	  switch(opt)
	  {
	  case 'p':
		  /* the port */
		  port = atoi(optarg);
		  break;

	  case 'f':
		  /* the input file name */
		  inputFileName = strDup(optarg);
		  break;

	  case 'r':
		  /* the frame rate of H.264 stream */
		  frameRate = atoi(optarg);
		  break;

	  case 'n':
		  /* the stream name used by RTSP server */
		  streamName = strDup(optarg);
		  break;

	  case 'h':
		  /* Print Usage */
		  PrintUsage(*env, argc, argv);
		  return 0;

	  default:/* ? */
		  PrintUsage(*env, argc, argv);
		  return 0;
	  }
  }

  if (inputFileName == NULL || streamName == NULL) {
	  PrintUsage(*env, argc, argv);
	  return 0;
  }

  /* Set the global frame rate */
  glb_h264_framerate = (double)frameRate;



  // Create 'groupsocks' for RTP and RTCP:
  struct in_addr destinationAddress;
  destinationAddress.s_addr = chooseRandomIPv4SSMAddress(*env);
  // Note: This is a multicast address.  If you wish instead to stream
  // using unicast, then you should use the "testOnDemandRTSPServer"
  // test program - not this test program - as a model.

  const unsigned short rtpPortNum = 18888;
  const unsigned short rtcpPortNum = rtpPortNum+1;
  const unsigned char ttl = 255;

  const Port rtpPort(rtpPortNum);
  const Port rtcpPort(rtcpPortNum);

  Groupsock rtpGroupsock(*env, destinationAddress, rtpPort, ttl);
  rtpGroupsock.multicastSendOnly(); // we're a SSM source
  Groupsock rtcpGroupsock(*env, destinationAddress, rtcpPort, ttl);
  rtcpGroupsock.multicastSendOnly(); // we're a SSM source

  // Create a 'H264 Video RTP' sink from the RTP 'groupsock':
 // OutPacketBuffer::maxSize = 100000;

  /* Set the max size of output packet */
  OutPacketBuffer::maxSize = 512 * 1024;
  videoSink = H264VideoRTPSink::createNew(*env, &rtpGroupsock, 96);

  // Create (and start) a 'RTCP instance' for this RTP sink:
  const unsigned estimatedSessionBandwidth = 500; // in kbps; for RTCP b/w share
  const unsigned maxCNAMElen = 100;
  unsigned char CNAME[maxCNAMElen+1];
  gethostname((char*)CNAME, maxCNAMElen);
  CNAME[maxCNAMElen] = '\0'; // just in case
  RTCPInstance* rtcp
  = RTCPInstance::createNew(*env, &rtcpGroupsock,
			    estimatedSessionBandwidth, CNAME,
			    videoSink, NULL /* we're a server */,
			    True /* we're a SSM source */);
  // Note: This starts RTCP running automatically
#if 0
  RTSPServer* rtspServer = RTSPServer::createNew(*env, 8554);
#else
  RTSPServer* rtspServer = RTSPServer::createNew(*env, port);
#endif
  if (rtspServer == NULL) {
    *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
    exit(1);
  }
  ServerMediaSession* sms
    = ServerMediaSession::createNew(*env, streamName, inputFileName,
		   "Session streamed by \"testH264VideoStreamer2\"",
					   True /*SSM*/);
  sms->addSubsession(PassiveServerMediaSubsession::createNew(*videoSink, rtcp));
  rtspServer->addServerMediaSession(sms);

  char* url = rtspServer->rtspURL(sms);
  *env << "Play this stream using the URL \"" << url << "\"\n";
  delete[] url;

  // Start the streaming:
  *env << "Beginning streaming...\n";
  play();

  env->taskScheduler().doEventLoop(); // does not return

  return 0; // only to prevent compiler warning
}

void afterPlaying(void* /*clientData*/) {
  *env << "...done reading from file\n";
  videoSink->stopPlaying();
  Medium::close(videoSource);
  // Note that this also closes the input file that this source read from.

  // Start playing once again:
  play();
}

void play() {
  // Open the input file as a 'byte-stream file source':
  ByteStreamFileSource* fileSource
    = ByteStreamFileSource::createNew(*env, inputFileName);
  if (fileSource == NULL) {
    *env << "Unable to open file \"" << inputFileName
         << "\" as a byte-stream file source\n";
    exit(1);
  }

  FramedSource* videoES = fileSource;

  // Create a framer for the Video Elementary Stream:
  videoSource = H264VideoStreamFramer::createNew(*env, videoES);

  // Finally, start playing:
  *env << "Beginning to read from file...\n";
  videoSink->startPlaying(*videoSource, afterPlaying, videoSink);
}
