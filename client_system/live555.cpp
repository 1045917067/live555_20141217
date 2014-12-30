/** ============================================================================
 *
 *  live555.c
 *
 *  Author     : zzx.
 *
 *  Date       : Dec 30, 2014
 *
 *  Description: 
 *  ============================================================================
 */

/*  --------------------- Include system headers ---------------------------- */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*  --------------------- Include user headers   ---------------------------- */
#include <UsageEnvironment.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <liveMedia.hh>
#include <liveMedia_version.hh>
#include <RTSPClient.hh>

#include "live555.h"

/*
 *  --------------------- Macro definition -------------------------------------
 */

/** ============================================================================
 *  @Macro:         Macro name
 *
 *  @Description:   Description of this macro.
 *  ============================================================================
 */
#define DEMUX_TRACK_NUM_MAX (8)

#define DEMUX_TS_INVALID    (0)

#  define INT64_C(c)	    c ## LL
/*
 *  --------------------- Structure definition ---------------------------------
 */

/** ----------------------------------------------------------------------------
 *  @Name:          Structure name
 *
 *  @Description:   Description of the structure.
 *
 *  @Field:         Field1 member
 *
 *  @Field          Field2 member
 *  ----------------------------------------------------------------------------
 */
typedef struct
{
    demux_t        *m_demux;
    MediaSubsession*m_sub;

    bool            m_muxed;
    bool            m_quicktime;
    bool            m_asf;
    bool            m_discard_trunc;
    unsigned int    m_codec;

    uint8_t       * m_buffer;
    unsigned int    m_offset;
    unsigned int    m_buffer_size;

    bool            m_first_frame_written;
    bool            m_rtcp_sync;
    char            m_waiting;
    int64_t         m_pts;
    float           m_npt;
    
	uint8_t       * aacCfg;

} live_track_t;

class RTSPClientVlc;

struct __demux_t
{
    demux_prm_t      m_prm;

    DEMUX_CALLBACK_FXN
                     m_fxn;

    void *           m_userdata;

    status_t         m_status;

    char             m_sdp[4096];
    bool             m_sdp_init;

    MediaSession    * m_ms;
    TaskScheduler   * m_scheduler;
    UsageEnvironment* m_env ;
    RTSPClientVlc   * m_rtsp;

    /* */
    int               m_track;
    live_track_t    * m_tracks[DEMUX_TRACK_NUM_MAX];

    /* */
    int64_t          m_pcr; /* The clock */
    float            m_npt;
    float            m_npt_length;
    float            m_npt_start;

    /* timeout thread information */
    int              m_timeout;     /* session timeout value in seconds */
    bool             m_timeout_call;/* mark to send an RTSP call to prevent server timeout */

    /* */
    bool             m_force_mcast;
    bool             m_multicast;   /* if one of the tracks is multicasted */
    bool             m_no_data;     /* if we never received any data */
    int              m_no_data_ti;  /* consecutive number of TaskInterrupt */

    char             m_event_rtsp;
    char             m_event_data;

    bool             m_get_param;   /* Does the server support GET_PARAMETER */
    bool             m_paused;      /* Are we paused? */
    bool             m_error;
    int              m_live555_ret; /* live555 callback return code */

    float            m_seek_request;/* In case we receive a seek request while paused*/
};

class RTSPClientVlc : public RTSPClient
{
public:
    static RTSPClientVlc* createNew(UsageEnvironment &env, char const *rtspURL,
            int verbosityLevel = 0, char const *applicationName = NULL,
            portNumBits tunnelOverHTTPPortNum = 0, demux_t *p_demux = NULL);

    demux_t *m_demux;

protected:

    RTSPClientVlc(UsageEnvironment &env, char const *rtspURL,
            int verbosityLevel = 0, char const *applicationName = NULL,
            portNumBits tunnelOverHTTPPortNum = 0, demux_t *p_demux = NULL);

    virtual ~RTSPClientVlc();
};

RTSPClientVlc* RTSPClientVlc::createNew(UsageEnvironment &env, char const *rtspURL,
            int verbosityLevel, char const *applicationName,
            portNumBits tunnelOverHTTPPortNum, demux_t *p_demux)
{
    return new RTSPClientVlc(env, rtspURL, verbosityLevel,
            applicationName, tunnelOverHTTPPortNum, p_demux);
}

RTSPClientVlc::RTSPClientVlc(UsageEnvironment &env, char const *rtspURL,
            int verbosityLevel, char const *applicationName,
            portNumBits tunnelOverHTTPPortNum, demux_t *p_demux)
    : RTSPClient(env, (char const *)rtspURL, (int)verbosityLevel, (char const *)applicationName, (portNumBits)tunnelOverHTTPPortNum, -1),
      m_demux(p_demux)
{
}

RTSPClientVlc::~RTSPClientVlc()
{
}

/*
 *  --------------------- Global variable definition ---------------------------
 */

/** ----------------------------------------------------------------------------
 *  @Name:          Variable name
 *
 *  @Description:   Description of the variable.
 * -----------------------------------------------------------------------------
 */

/*
 *  --------------------- Local function forward declaration -------------------
 */
static status_t Connect( demux_t *p_demux );

static status_t SessionsSetup( demux_t *p_demux );

static int Play( demux_t *p_demux );

static int Control( demux_t *p_demux, int i_query, va_list args );

static void StreamRead( void *p_private, unsigned int i_size,
                        unsigned int i_truncated_bytes, struct timeval pts,
                        unsigned int duration );

static void StreamClose( void *p_private );

static void TaskInterruptData( void *p_private );

static void TaskInterruptRTSP( void *p_private );

/*
 *  --------------------- Public function definition ---------------------------
 */
#if defined(__cplusplus)
extern "C" {
#endif

status_t demux_open(demux_t **pdemux, const demux_prm_t *prm)
{
    demux_t *demux = NULL;
    status_t status = OSA_SOK;

    if (pdemux == NULL || prm == NULL) {
        return OSA_EARGS;
    }

    (*pdemux) = NULL;

    demux = (demux_t *)malloc(sizeof(demux_t));
    if (demux == NULL) {
        return OSA_EMEM;
    }
    memset(demux, 0, sizeof(demux_t));

    /* Initialize demux object using default value */
    memcpy(&demux->m_prm, prm, sizeof(demux_prm_t));

    demux->m_fxn = NULL;
    demux->m_userdata = NULL;
    demux->m_status = OSA_SOK;

    demux->m_scheduler = NULL;
    demux->m_env = NULL;
    demux->m_ms = NULL;
    demux->m_rtsp = NULL;
    demux->m_track = 0;

    demux->m_pcr = 0;
    demux->m_npt = 0.;
    demux->m_npt_start = 0.;
    demux->m_npt_length = 0.;
    demux->m_no_data = true;
    demux->m_no_data_ti = 0;
    demux->m_timeout = 0;
    demux->m_timeout_call = false;
    demux->m_multicast = false;
    demux->m_force_mcast = false;
    demux->m_get_param = false;
    demux->m_paused = false;
    demux->m_seek_request = -1;
    demux->m_error = false;
    demux->m_live555_ret = 0;

    /* Initialize live555 rtsp environments */
    if( ( demux->m_scheduler = BasicTaskScheduler::createNew() ) == NULL )
    {
        goto error;
    }

    if( !( demux->m_env = BasicUsageEnvironment::createNew(*demux->m_scheduler) ) )
    {
        goto error;
    }

    if( ( status = Connect( demux ) ) != OSA_SOK )
    {
        fprintf( stderr, "Failed to connect with %s.\n", prm->m_url);
        goto error;
    }

    if( !demux->m_sdp_init )
    {
        fprintf( stderr, "Failed to retrieve the RTSP Session Description.\n" );
        status = OSA_EFAIL;
        goto error;
    }

    if( ( status = SessionsSetup( demux ) ) != OSA_SOK )
    {
        fprintf( stderr, "Nothing to play for %s.\n", prm->m_url);
        goto error;
    }

    if( ( status = Play( demux ) ) != OSA_SOK )
        goto error;

    if( demux->m_track <= 0 )
        goto error;

    (*pdemux) = demux;

    return status;

error:
    demux_close( &demux );
    return status;
}

status_t demux_exec(demux_t *p_demux, DEMUX_CALLBACK_FXN fxn, void *ud)
{
    TaskToken      task;

    bool            b_send_pcr = true;
    int64_t         i_pcr = 0;
    int             i;

    if (p_demux == NULL || fxn == NULL) {
        return OSA_EARGS;
    }

    /* Save callback information */
    p_demux->m_fxn = fxn;
    p_demux->m_userdata = ud;

    /* Check if we need to send the server a Keep-A-Live signal */
    if( p_demux->m_timeout_call && p_demux->m_rtsp && p_demux->m_ms )
    {
        char *psz_bye = NULL;
        p_demux->m_rtsp->sendGetParameterCommand( *p_demux->m_ms, NULL, psz_bye );
        p_demux->m_timeout_call = false;
    }

    /* First warn we want to read data */
    p_demux->m_event_data = 0;
    for( i = 0; i < p_demux->m_track; i++ )
    {
        live_track_t *tk = p_demux->m_tracks[i];

        if( tk->m_waiting == 0 )
        {
            tk->m_waiting = 1;
            tk->m_sub->readSource()->getNextFrame( tk->m_buffer + tk->m_offset, tk->m_buffer_size - tk->m_offset,
                                          StreamRead, tk, StreamClose, tk );
        }
    }

    /* Create a task that will be called if we wait more than 300ms */
    task = p_demux->m_scheduler->scheduleDelayedTask( 300000, TaskInterruptData, p_demux );

    /* Do the read */
    p_demux->m_scheduler->doEventLoop( &p_demux->m_event_data );

    /* remove the task */
    p_demux->m_scheduler->unscheduleDelayedTask( task );

    /* Check for gap in pts value */
    for( i = 0; i < p_demux->m_track; i++ )
    {
        live_track_t *tk = p_demux->m_tracks[i];

        if( !tk->m_muxed && !tk->m_rtcp_sync &&
            tk->m_sub->rtpSource() && tk->m_sub->rtpSource()->hasBeenSynchronizedUsingRTCP() )
        {
            //fprintf( stderr, "tk->rtpSource->hasBeenSynchronizedUsingRTCP()" );

            tk->m_rtcp_sync = true;
            /* reset PCR */
            tk->m_pts = DEMUX_TS_INVALID;
            tk->m_npt = 0.;
            p_demux->m_pcr = 0;
            p_demux->m_npt = 0.;
            i_pcr = 0;
        }
    }

    if( p_demux->m_multicast && p_demux->m_no_data &&
        ( p_demux->m_no_data_ti > 120 ) )
    {
        /* FIXME Make this configurable
        msg_Err( p_demux, "no multicast data received in 36s, aborting" );
        return 0;
        */
    }
    else if( !p_demux->m_multicast && !p_demux->m_paused &&
              p_demux->m_no_data && ( p_demux->m_no_data_ti > 34 ) )
    {
        bool b_rtsp_tcp = false; 

        if( !b_rtsp_tcp && p_demux->m_rtsp && p_demux->m_ms )
        {
            fprintf( stderr, "no data received in 10s. Switching to TCP" );
            return OSA_EFAIL;
        }
        fprintf( stderr, "no data received in 10s, aborting" );
        return OSA_EFAIL;
    }
    else if( !p_demux->m_multicast && !p_demux->m_paused &&
             ( p_demux->m_no_data_ti > 34 ) )
    {
        /* EOF ? */
        fprintf( stderr, "no data received in 10s, eof ?\n" );
        return OSA_EEOF;
    }

    return p_demux->m_error ? OSA_EFAIL : OSA_SOK;
}

status_t demux_control(demux_t *demux, int cmd, ...)
{
    return OSA_SOK;
}

status_t demux_close(demux_t **pdemux)
{
    demux_t *p_demux = NULL;
    status_t status = OSA_SOK;

    if (pdemux == NULL || *pdemux == NULL) {
        return OSA_EARGS;
    }

    p_demux = (*pdemux);

    if( p_demux->m_rtsp && p_demux->m_ms )
    {
        p_demux->m_rtsp->sendTeardownCommand( *p_demux->m_ms, NULL );
    }

    if( p_demux->m_ms ) Medium::close( p_demux->m_ms );
    if( p_demux->m_rtsp ) RTSPClient::close( p_demux->m_rtsp );
    if( p_demux->m_env ) p_demux->m_env->reclaim();

    for( int i = 0; i < p_demux->m_track; i++ )
    {
        live_track_t *tk = p_demux->m_tracks[i];

        free( tk->m_buffer );
        free( tk );
    }

    delete p_demux->m_scheduler;

    free( p_demux );

    (*pdemux) = NULL;

    return status;
}

#if defined(__cplusplus)
}
#endif

/*
 *  --------------------- Local function definition ----------------------------
 */
static void default_live555_callback( RTSPClient* client, int result_code, char* result_string )
{
    RTSPClientVlc *client_vlc = static_cast<RTSPClientVlc *> ( client );
    demux_t *p_demux = client_vlc->m_demux;
    delete []result_string;
    p_demux->m_live555_ret = result_code;
    p_demux->m_error = p_demux->m_live555_ret != 0;
    p_demux->m_event_rtsp = 1;
}

/* return true if the RTSP command succeeded */
static bool wait_Live555_response( demux_t *p_demux, int i_timeout = 0 /* ms */ )
{
    TaskToken task;
    p_demux->m_event_rtsp = 0;
    if( i_timeout > 0 )
    {
        /* Create a task that will be called if we wait more than timeout ms */
        task = p_demux->m_scheduler->scheduleDelayedTask( i_timeout * 1000,
                                                          TaskInterruptRTSP,
                                                          p_demux );
    }
    p_demux->m_event_rtsp = 0;
    p_demux->m_error = true;
    p_demux->m_live555_ret = 0;
    p_demux->m_scheduler->doEventLoop( &p_demux->m_event_rtsp );
    //here, if b_error is true and i_live555_ret = 0 we didn't receive a response
    if( i_timeout > 0 )
    {
        /* remove the task */
        p_demux->m_scheduler->unscheduleDelayedTask( task );
    }

    return !p_demux->m_error;
}

static void continueAfterDESCRIBE( RTSPClient* client, int result_code,
                                   char* result_string )
{
    RTSPClientVlc *client_vlc = static_cast<RTSPClientVlc *> ( client );
    demux_t *p_demux = client_vlc->m_demux;
    p_demux->m_live555_ret = result_code;
    if ( result_code == 0 )
    {
        char* sdpDescription = result_string;
        if( sdpDescription )
        {
            strncpy(p_demux->m_sdp, sdpDescription, sizeof(p_demux->m_sdp) - 1);
            p_demux->m_error = false;
            p_demux->m_sdp_init = true;
        }
    }
    else
        p_demux->m_error = true;
    delete[] result_string;
    p_demux->m_event_rtsp = 1;
}

static void continueAfterOPTIONS( RTSPClient* client, int result_code,
                                  char* result_string )
{
    RTSPClientVlc *client_vlc = static_cast<RTSPClientVlc *> (client);
    demux_t *p_demux = client_vlc->m_demux;
    p_demux->m_live555_ret = result_code;
    if ( result_code != 0 )
    {
        p_demux->m_error = true;
        p_demux->m_event_rtsp = 1;
    }
    else
    {
        p_demux->m_get_param = result_string != NULL && strstr( result_string, "GET_PARAMETER" ) != NULL;
        client->sendDescribeCommand( continueAfterDESCRIBE );
    }
    delete[] result_string;
}

/*****************************************************************************
 * Connect: connects to the RTSP server to setup the session DESCRIBE
 *****************************************************************************/
static status_t Connect( demux_t *p_demux )
{
    Authenticator authenticator;
    portNumBits  i_http_port  = 0;
    int  i_ret        = OSA_SOK;
    const int i_timeout = 2000;

    char const * psz_url = (char const *)p_demux->m_prm.m_url;

createnew:

    i_http_port = 0;

    p_demux->m_rtsp = RTSPClientVlc::createNew( *p_demux->m_env, psz_url,
                                        0, "LibVLC", i_http_port, p_demux );
    if( !p_demux->m_rtsp )
    {
        //msg_Err( p_demux, "RTSPClient::createNew failed (%s)", p_sys->env->getResultMsg() );
        i_ret = OSA_EFAIL;
        goto bailout;
    }

#if 0
    /* Kasenna enables KeepAlive by analysing the User-Agent string.
     * Appending _KA to the string should be enough to enable this feature,
     * however, there is a bug where the _KA doesn't get parsed from the
     * default User-Agent as created by VLC/Live555 code. This is probably due
     * to spaces in the string or the string being too long. Here we override
     * the default string with a more compact version.
     */
    if( var_InheritBool( p_demux, "rtsp-kasenna" ))
    {
        p_sys->rtsp->setUserAgentString( "VLC_MEDIA_PLAYER_KA" );
    }
#endif

describe:

    //authenticator.setUsernameAndPassword( psz_user, psz_pwd );

    p_demux->m_rtsp->sendOptionsCommand( &continueAfterOPTIONS, NULL);

    if( !wait_Live555_response( p_demux, i_timeout ) )
    {
        int i_code = p_demux->m_live555_ret;

        if( i_code == 0 )
        {
            //fprintf( stderr, "connection timeout.\n" );
            i_ret = OSA_ETIMEOUT;
        }
        else
        {
            //fprintf( stderr, "connection error %d.\n", i_code );
            i_ret = OSA_ECONNECT;
        }

        if( p_demux->m_rtsp ) RTSPClient::close( p_demux->m_rtsp );
        p_demux->m_rtsp = NULL;
    }

bailout:
    return i_ret;
}

/*****************************************************************************
 * SessionsSetup: prepares the subsessions and does the SETUP
 *****************************************************************************/
static status_t SessionsSetup( demux_t *p_demux )
{
    MediaSubsessionIterator *iter   = NULL;
    MediaSubsession         *sub    = NULL;

    bool           b_rtsp_tcp = false;
    int            i_client_port = -1;
    int            i_return = OSA_SOK;
    unsigned int   i_buffer = 0;
    unsigned const thresh = 200000; /* RTP reorder threshold .2 second (default .1) */

    /* TODO: */
    //i_client_port = var_InheritInteger( p_demux, "rtp-client-port" );

    /* Create the session from the SDP */
    if( !( p_demux->m_ms = MediaSession::createNew( *p_demux->m_env, p_demux->m_sdp ) ) )
    {
        fprintf( stderr, "Could not create the RTSP Session: %s", p_demux->m_env->getResultMsg() );
        return OSA_EFAIL;
    }

    /* Initialise each media subsession */
    iter = new MediaSubsessionIterator( *p_demux->m_ms );
    while( ( sub = iter->next() ) != NULL )
    {
        Boolean bInit;
        live_track_t *tk;

        /* Value taken from mplayer */
        if( !strcmp( sub->mediumName(), "audio" ) )
            i_buffer = 100000;
        else if( !strcmp( sub->mediumName(), "video" ) )
            i_buffer = 2000000;
        else if( !strcmp( sub->mediumName(), "text" ) )
            ;
        else continue;

        if( i_client_port != -1 )
        {
            sub->setClientPortNum( i_client_port );
            i_client_port += 2;
        }

        if( !strcmp( sub->codecName(), "X-ASF-PF" ) )
            bInit = sub->initiate( 0 );
        else
            bInit = sub->initiate();

        if( !bInit )
        {
            fprintf( stderr, "RTP subsession '%s/%s' failed (%s)",
                      sub->mediumName(), sub->codecName(),
                      p_demux->m_env->getResultMsg() );
        }
        else
        {
            if( sub->rtpSource() != NULL )
            {
                int fd = sub->rtpSource()->RTPgs()->socketNum();

                /* Increase the buffer size */
                if( i_buffer > 0 )
                    increaseReceiveBufferTo( *p_demux->m_env, fd, i_buffer );

                /* Increase the RTP reorder timebuffer just a bit */
                sub->rtpSource()->setPacketReorderingThresholdTime(thresh);
            }
            //fprintf( stderr, "RTP subsession '%s/%s'", sub->mediumName(), sub->codecName() );

            /* Issue the SETUP */
            if( p_demux->m_rtsp )
            {
                p_demux->m_rtsp->sendSetupCommand( *sub, default_live555_callback, false,
                                                   b_rtsp_tcp,
                                                 ( p_demux->m_force_mcast && !b_rtsp_tcp ) );
                if( !wait_Live555_response( p_demux ) )
                {
                    /* if we get an unsupported transport error, toggle TCP
                     * use and try again */
                    if( p_demux->m_live555_ret == 461 )
                        p_demux->m_rtsp->sendSetupCommand( *sub, default_live555_callback, false,
                                                       !b_rtsp_tcp, false );
                    if( p_demux->m_live555_ret != 461 || !wait_Live555_response( p_demux ) )
                    {
                        fprintf( stderr, "SETUP of'%s/%s' failed %s",
                                 sub->mediumName(), sub->codecName(),
                                 p_demux->m_env->getResultMsg() );
                        continue;
                    }
                }
            }

            /* Check if we will receive data from this subsession for
             * this track */
            if( sub->readSource() == NULL ) continue;
            if( !p_demux->m_multicast )
            {
                /* We need different rollover behaviour for multicast */
                p_demux->m_multicast = IsMulticastAddress( sub->connectionEndpointAddress() );
            }

            tk = (live_track_t *)malloc( sizeof( live_track_t ) );
            if( !tk )
            {
                delete iter;
                return OSA_EMEM;
            }
            tk->m_demux     = p_demux;
            tk->m_sub       = sub;
            tk->m_quicktime = false;
            tk->m_muxed     = false;
            tk->m_discard_trunc = false;
            tk->m_waiting   = 0;
            tk->m_first_frame_written = false;
            tk->m_rtcp_sync = false;
            tk->m_pts       = DEMUX_TS_INVALID;
            tk->m_npt       = 0.;
            
            int buf_size    = 65536;
            if (!strcmp( sub->mediumName(), "video" )) {
                buf_size    = 1 * 1024 * 1024;
            }
            tk->m_buffer_size = buf_size;
            tk->m_buffer    = (uint8_t *)malloc( buf_size );
            if( !tk->m_buffer )
            {
                free( tk );
                delete iter;
                return OSA_EMEM;
            }

            /* Value taken from mplayer */
            if( !strcmp( sub->mediumName(), "audio" ) )
            {
#if 0
                es_format_Init( &tk->fmt, AUDIO_ES, VLC_FOURCC('u','n','d','f') );
                tk->fmt.audio.i_channels = sub->numChannels();
                tk->fmt.audio.i_rate = sub->rtpTimestampFrequency();

                if( !strcmp( sub->codecName(), "MPA" ) ||
                    !strcmp( sub->codecName(), "MPA-ROBUST" ) ||
                    !strcmp( sub->codecName(), "X-MP3-DRAFT-00" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_MPGA;
                    tk->fmt.audio.i_rate = 0;
                }
                else if( !strcmp( sub->codecName(), "AC3" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_A52;
                    tk->fmt.audio.i_rate = 0;
                }
                else if( !strcmp( sub->codecName(), "L16" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_S16B;
                    tk->fmt.audio.i_bitspersample = 16;
                }
                else if( !strcmp( sub->codecName(), "L20" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_S20B;
                    tk->fmt.audio.i_bitspersample = 20;
                }
                else if( !strcmp( sub->codecName(), "L24" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_S24B;
                    tk->fmt.audio.i_bitspersample = 24;
                }
                else if( !strcmp( sub->codecName(), "L8" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_U8;
                    tk->fmt.audio.i_bitspersample = 8;
                }
                else if( !strcmp( sub->codecName(), "DAT12" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_DAT12;
                    tk->fmt.audio.i_bitspersample = 12;
                }
                else if( !strcmp( sub->codecName(), "PCMU" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_MULAW;
                    tk->fmt.audio.i_bitspersample = 8;
                }
                else if( !strcmp( sub->codecName(), "PCMA" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_ALAW;
                    tk->fmt.audio.i_bitspersample = 8;
                }
                else if( !strncmp( sub->codecName(), "G726", 4 ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_ADPCM_G726;
                    tk->fmt.audio.i_rate = 8000;
                    tk->fmt.audio.i_channels = 1;
                    if( !strcmp( sub->codecName()+5, "40" ) )
                        tk->fmt.i_bitrate = 40000;
                    else if( !strcmp( sub->codecName()+5, "32" ) )
                        tk->fmt.i_bitrate = 32000;
                    else if( !strcmp( sub->codecName()+5, "24" ) )
                        tk->fmt.i_bitrate = 24000;
                    else if( !strcmp( sub->codecName()+5, "16" ) )
                        tk->fmt.i_bitrate = 16000;
                }
                else if( !strcmp( sub->codecName(), "AMR" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_AMR_NB;
                }
                else if( !strcmp( sub->codecName(), "AMR-WB" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_AMR_WB;
                }
                else if( !strcmp( sub->codecName(), "MP4A-LATM" ) )
                {
                    unsigned int i_extra;
                    uint8_t      *p_extra;

                    tk->fmt.i_codec = VLC_CODEC_MP4A;

                    if( ( p_extra = parseStreamMuxConfigStr( sub->fmtp_config(),
                                                             i_extra ) ) )
                    {
                        tk->fmt.i_extra = i_extra;
                        tk->fmt.p_extra = xmalloc( i_extra );
                        memcpy( tk->fmt.p_extra, p_extra, i_extra );
                        delete[] p_extra;
                    }
                    /* Because the "faad" decoder does not handle the LATM
                     * data length field at the start of each returned LATM
                     * frame, tell the RTP source to omit it. */
                    ((MPEG4LATMAudioRTPSource*)sub->rtpSource())->omitLATMDataLengthField();
                }
#endif
                if( !strcmp( sub->codecName(), "PCMU" ) )
                {
                    tk->m_codec = AUDIO_ES;
                    tk->m_offset = 0;
                }
                if( !strcmp( sub->codecName(), "MPEG4-GENERIC" ) )
                {
                    unsigned int i_extra;
                    uint8_t      *p_extra;

                    tk->m_codec = AUDIO_ES;

                    tk->m_offset = 7;
                    tk->aacCfg = parseGeneralConfigStr( sub->fmtp_config(),i_extra);
#if 0
                    if( ( p_extra = parseGeneralConfigStr( sub->fmtp_config(),
                                                           i_extra ) ) )
                    {
                        tk->fmt.i_extra = i_extra;
                        tk->fmt.p_extra = xmalloc( i_extra );
                        memcpy( tk->fmt.p_extra, p_extra, i_extra );
                        delete[] p_extra;
                    }
#endif
                } else {

                    tk->m_codec = AUDIO_ES;

                    tk->m_offset = 0;
                }
#if 0
                else if( !strcmp( sub->codecName(), "X-ASF-PF" ) )
                {
                    tk->b_asf = true;
                    if( p_sys->p_out_asf == NULL )
                        p_sys->p_out_asf = stream_DemuxNew( p_demux, "asf",
                                                            p_demux->out );
                }
                else if( !strcmp( sub->codecName(), "X-QT" ) ||
                         !strcmp( sub->codecName(), "X-QUICKTIME" ) )
                {
                    tk->b_quicktime = true;
                }
                else if( !strcmp( sub->codecName(), "SPEEX" ) )
                {
                    tk->fmt.i_codec = VLC_FOURCC( 's', 'p', 'x', 'r' );
                    if ( sub->rtpTimestampFrequency() )
                        tk->fmt.audio.i_rate = sub->rtpTimestampFrequency();
                    else
                    {
                        msg_Warn( p_demux,"Using 8kHz as default sample rate." );
                        tk->fmt.audio.i_rate = 8000;
                    }
                }
#endif
            }
            else if( !strcmp( sub->mediumName(), "video" ) )
            {
#if 0
                es_format_Init( &tk->fmt, VIDEO_ES, VLC_FOURCC('u','n','d','f') );
                if( !strcmp( sub->codecName(), "MPV" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_MPGV;
                    tk->fmt.b_packetized = false;
                }
                else if( !strcmp( sub->codecName(), "H263" ) ||
                         !strcmp( sub->codecName(), "H263-1998" ) ||
                         !strcmp( sub->codecName(), "H263-2000" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_H263;
                }
                else if( !strcmp( sub->codecName(), "H261" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_H261;
                }
                else 
#endif
                if( !strcmp( sub->codecName(), "H264" ) )
                {
                    unsigned int i_extra = 0;
                    uint8_t      *p_extra = NULL;

                    tk->m_codec = VIDEO_ES;

                    tk->m_offset = 4;
#if 0
                    if((p_extra=parseH264ConfigStr( sub->fmtp_spropparametersets(),
                                                    i_extra ) ) )
                    {
                        tk->fmt.i_extra = i_extra;
                        tk->fmt.p_extra = xmalloc( i_extra );
                        memcpy( tk->fmt.p_extra, p_extra, i_extra );

                        delete[] p_extra;
                    }
#endif
                }
                else if( !strcmp( sub->codecName(), "JPEG" ) )
                {
                    //tk->m_codec = VLC_CODEC_MJPG;
                    tk->m_codec = VIDEO_ES;

                    tk->m_offset = 0;
                } else {
                    tk->m_codec = VIDEO_ES;

                    tk->m_offset = 0;
                }
#if 0
                else if( !strcmp( sub->codecName(), "MP4V-ES" ) )
                {
                    unsigned int i_extra;
                    uint8_t      *p_extra;

                    tk->fmt.i_codec = VLC_CODEC_MP4V;

                    if( ( p_extra = parseGeneralConfigStr( sub->fmtp_config(),
                                                           i_extra ) ) )
                    {
                        tk->fmt.i_extra = i_extra;
                        tk->fmt.p_extra = xmalloc( i_extra );
                        memcpy( tk->fmt.p_extra, p_extra, i_extra );
                        delete[] p_extra;
                    }
                }
                else if( !strcmp( sub->codecName(), "X-QT" ) ||
                         !strcmp( sub->codecName(), "X-QUICKTIME" ) ||
                         !strcmp( sub->codecName(), "X-QDM" ) ||
                         !strcmp( sub->codecName(), "X-SV3V-ES" )  ||
                         !strcmp( sub->codecName(), "X-SORENSONVIDEO" ) )
                {
                    tk->b_quicktime = true;
                }
                else if( !strcmp( sub->codecName(), "MP2T" ) )
                {
                    tk->b_muxed = true;
                    tk->p_out_muxed = stream_DemuxNew( p_demux, "ts", p_demux->out );
                }
                else if( !strcmp( sub->codecName(), "MP2P" ) ||
                         !strcmp( sub->codecName(), "MP1S" ) )
                {
                    tk->b_muxed = true;
                    tk->p_out_muxed = stream_DemuxNew( p_demux, "ps",
                                                       p_demux->out );
                }
                else if( !strcmp( sub->codecName(), "X-ASF-PF" ) )
                {
                    tk->b_asf = true;
                    if( p_sys->p_out_asf == NULL )
                        p_sys->p_out_asf = stream_DemuxNew( p_demux, "asf",
                                                            p_demux->out );;
                }
                else if( !strcmp( sub->codecName(), "DV" ) )
                {
                    tk->b_muxed = true;
                    tk->b_discard_trunc = true;
                    tk->p_out_muxed = stream_DemuxNew( p_demux, "rawdv",
                                                       p_demux->out );
                }
#endif
            }
            else if( !strcmp( sub->mediumName(), "text" ) )
            {
#if 0
                es_format_Init( &tk->fmt, SPU_ES, VLC_FOURCC('u','n','d','f') );

                if( !strcmp( sub->codecName(), "T140" ) )
                {
                    tk->fmt.i_codec = VLC_CODEC_ITU_T140;
                }
#endif
            }

#if 0
            if( !tk->b_quicktime && !tk->b_muxed && !tk->b_asf )
            {
                tk->p_es = es_out_Add( p_demux->out, &tk->fmt );
            }
#endif

            if( sub->rtcpInstance() != NULL )
            {
                sub->rtcpInstance()->setByeHandler( StreamClose, tk );
            }

#if 0
            if( tk->m_es || tk->m_quicktime || ( tk->m_muxed && tk->m_out_muxed ) ||
                ( tk->m_asf && p_demux->m_out_asf ) )
            {
                /* Append */
#if 0
                p_sys->track = (live_track_t**)xrealloc( p_sys->track,
                            sizeof( live_track_t ) * ( p_sys->i_track + 1 ) );
#endif
            }
            else
            {
                /* BUG ??? */
                fprintf( stderr, "unusable RTSP track. this should not happen" );
#if 0
                es_format_Clean( &tk->fmt );
#endif
                free( tk );
            }
#endif

            p_demux->m_tracks[p_demux->m_track++] = tk;
        }
    }

    delete iter;

    if( p_demux->m_track <= 0 ) i_return = OSA_EFAIL;

    /* Retrieve the starttime if possible */
    p_demux->m_npt_start = p_demux->m_ms->playStartTime();

    /* Retrieve the duration if possible */
    p_demux->m_npt_length = p_demux->m_ms->playEndTime();

    /* */
    //fprintf( stderr, "setup start: %f stop:%f", p_demux->m_npt_start, p_demux->m_npt_length );

    /* */
    p_demux->m_no_data = true;
    p_demux->m_no_data_ti = 0;

    return i_return;
}

/*****************************************************************************
 * Play: starts the actual playback of the stream
 *****************************************************************************/
static int Play( demux_t *p_demux )
{
    if( p_demux->m_rtsp )
    {
        /* The PLAY */
        p_demux->m_rtsp->sendPlayCommand( *p_demux->m_ms, default_live555_callback, p_demux->m_npt_start, -1, 1 );

        if( !wait_Live555_response(p_demux) )
        {
            fprintf( stderr, "RTSP PLAY failed %s", p_demux->m_env->getResultMsg() );
            return OSA_EFAIL;
        }

        /* Retrieve the timeout value and set up a timeout prevention thread */
        p_demux->m_timeout = p_demux->m_rtsp->sessionTimeoutParameter();
        if( p_demux->m_timeout <= 0 )
            p_demux->m_timeout = 60; /* default value from RFC2326 */

#if 0
        /* start timeout-thread only if GET_PARAMETER is supported by the server */
        if( !p_sys->p_timeout && p_sys->b_get_param )
        {
            msg_Dbg( p_demux, "We have a timeout of %d seconds",  p_sys->i_timeout );
            p_sys->p_timeout = (timeout_thread_t *)malloc( sizeof(timeout_thread_t) );
            if( p_sys->p_timeout )
            {
                memset( p_sys->p_timeout, 0, sizeof(timeout_thread_t) );
                p_sys->p_timeout->p_sys = p_demux->p_sys; /* lol, object recursion :D */
                if( vlc_clone( &p_sys->p_timeout->handle,  TimeoutPrevention,
                               p_sys->p_timeout, VLC_THREAD_PRIORITY_LOW ) )
                {
                    msg_Err( p_demux, "cannot spawn liveMedia timeout thread" );
                    free( p_sys->p_timeout );
                    p_sys->p_timeout = NULL;
                }
                else
                    msg_Dbg( p_demux, "spawned timeout thread" );
            }
            else
                msg_Err( p_demux, "cannot spawn liveMedia timeout thread" );
        }
#endif
    }

    p_demux->m_pcr = 0;

    /* Retrieve the starttime if possible */
    p_demux->m_npt_start = p_demux->m_ms->playStartTime();
    if( p_demux->m_ms->playEndTime() > 0 )
        p_demux->m_npt_length = p_demux->m_ms->playEndTime();

    //fprintf( stderr, "play start: %f stop:%f", p_demux->m_npt_start, p_demux->m_npt_length );

    return OSA_SOK;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
#if 0
    int64_t *pi64, i64;
    double  *pf, f;
    bool *pb, *pb2;
    int *pi_int;

    switch( i_query )
    {
        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->i_npt > 0 )
            {
                *pi64 = (int64_t)(p_sys->i_npt * 1000000.);
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->i_npt_length > 0 )
            {
                *pi64 = (int64_t)((double)p_sys->i_npt_length * 1000000.0);
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_GET_POSITION:
            pf = (double*)va_arg( args, double* );
            if( (p_sys->i_npt_length > 0) && (p_sys->i_npt > 0) )
            {
                *pf = ( (double)p_sys->i_npt / (double)p_sys->i_npt_length );
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        case DEMUX_SET_POSITION:
        case DEMUX_SET_TIME:
            if( p_sys->rtsp && (p_sys->i_npt_length > 0) )
            {
                int i;
                float time;

                if( (i_query == DEMUX_SET_TIME) && (p_sys->i_npt > 0) )
                {
                    i64 = (int64_t)va_arg( args, int64_t );
                    time = (float)((double)i64 / (double)1000000.0); /* in second */
                }
                else if( i_query == DEMUX_SET_TIME )
                    return VLC_EGENERIC;
                else
                {
                    f = (double)va_arg( args, double );
                    time = f * (double)p_sys->i_npt_length;   /* in second */
                }

                if( p_sys->b_paused )
                {
                    p_sys->f_seek_request = time;
                    return VLC_SUCCESS;
                }

                p_sys->rtsp->sendPauseCommand( *p_sys->ms, default_live555_callback );

                if( !wait_Live555_response( p_demux ) )
                {
                    msg_Err( p_demux, "PAUSE before seek failed %s",
                        p_sys->env->getResultMsg() );
                    return VLC_EGENERIC;
                }

                p_sys->rtsp->sendPlayCommand( *p_sys->ms, default_live555_callback, time, -1, 1 );

                if( !wait_Live555_response( p_demux ) )
                {
                    msg_Err( p_demux, "seek PLAY failed %s",
                        p_sys->env->getResultMsg() );
                    return VLC_EGENERIC;
                }
                p_sys->i_pcr = 0;

                /* Retrieve RTP-Info values */
                for( i = 0; i < p_sys->i_track; i++ )
                {
                    p_sys->track[i]->b_rtcp_sync = false;
                    p_sys->track[i]->i_pts = VLC_TS_INVALID;
                }

                /* Retrieve the starttime if possible */
                p_sys->i_npt = p_sys->i_npt_start = p_sys->ms->playStartTime();

                /* Retrieve the duration if possible */
                if( p_sys->ms->playEndTime() > 0 )
                    p_sys->i_npt_length = p_sys->ms->playEndTime();

                msg_Dbg( p_demux, "seek start: %f stop:%f", p_sys->i_npt_start, p_sys->i_npt_length );
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;

        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
            pb = (bool*)va_arg( args, bool * );
            if( p_sys->rtsp && p_sys->i_npt_length > 0 )
                /* Not always true, but will be handled in SET_PAUSE_STATE */
                *pb = true;
            else
                *pb = false;
            return VLC_SUCCESS;

        case DEMUX_CAN_CONTROL_PACE:
            pb = (bool*)va_arg( args, bool * );

#if 1       /* Disable for now until we have a clock synchro algo
             * which works with something else than MPEG over UDP */
            *pb = false;
#else
            *pb = true;
#endif
            return VLC_SUCCESS;

        case DEMUX_CAN_CONTROL_RATE:
            pb = (bool*)va_arg( args, bool * );
            pb2 = (bool*)va_arg( args, bool * );

            *pb = (p_sys->rtsp != NULL) &&
                    (p_sys->i_npt_length > 0) &&
                    ( !var_GetBool( p_demux, "rtsp-kasenna" ) ||
                      !var_GetBool( p_demux, "rtsp-wmserver" ) );
            *pb2 = false;
            return VLC_SUCCESS;

        case DEMUX_SET_RATE:
        {
            double f_scale, f_old_scale;

            if( !p_sys->rtsp || (p_sys->i_npt_length <= 0) ||
                var_GetBool( p_demux, "rtsp-kasenna" ) ||
                var_GetBool( p_demux, "rtsp-wmserver" ) )
                return VLC_EGENERIC;

            /* According to RFC 2326 p56 chapter 12.35 a RTSP server that
             * supports Scale:
             *
             * "[...] should try to approximate the viewing rate, but
             *  may restrict the range of scale values that it supports.
             *  The response MUST contain the actual scale value chosen
             *  by the server."
             *
             * Scale = 1 indicates normal play
             * Scale > 1 indicates fast forward
             * Scale < 1 && Scale > 0 indicates slow motion
             * Scale < 0 value indicates rewind
             */

            pi_int = (int*)va_arg( args, int * );
            f_scale = (double)INPUT_RATE_DEFAULT / (*pi_int);
            f_old_scale = p_sys->ms->scale();

            /* Passing -1 for the start and end time will mean liveMedia won't
             * create a Range: section for the RTSP message. The server should
             * pick up from the current position */
            p_sys->rtsp->sendPlayCommand( *p_sys->ms, default_live555_callback, -1, -1, f_scale );

            if( !wait_Live555_response( p_demux ) )
            {
                msg_Err( p_demux, "PLAY with Scale %0.2f failed %s", f_scale,
                        p_sys->env->getResultMsg() );
                return VLC_EGENERIC;
            }

            if( p_sys->ms->scale() == f_old_scale )
            {
                msg_Err( p_demux, "no scale change using old Scale %0.2f",
                          p_sys->ms->scale() );
                return VLC_EGENERIC;
            }

            /* ReSync the stream */
            p_sys->i_npt_start = 0;
            p_sys->i_pcr = 0;
            p_sys->i_npt = 0.0;

            *pi_int = (int)( INPUT_RATE_DEFAULT / p_sys->ms->scale() );
            msg_Dbg( p_demux, "PLAY with new Scale %0.2f (%d)", p_sys->ms->scale(), (*pi_int) );
            return VLC_SUCCESS;
        }

        case DEMUX_SET_PAUSE_STATE:
        {
            bool b_pause = (bool)va_arg( args, int );
            if( p_sys->rtsp == NULL )
                return VLC_EGENERIC;

            if( b_pause == p_sys->b_paused )
                return VLC_SUCCESS;
            if( b_pause )
                p_sys->rtsp->sendPauseCommand( *p_sys->ms, default_live555_callback );
            else
                p_sys->rtsp->sendPlayCommand( *p_sys->ms, default_live555_callback, p_sys->f_seek_request,
                                              -1.0f, p_sys->ms->scale() );

            if( !wait_Live555_response( p_demux ) )
            {
                msg_Err( p_demux, "PLAY or PAUSE failed %s", p_sys->env->getResultMsg() );
                return VLC_EGENERIC;
            }
            p_sys->f_seek_request = -1;
            p_sys->b_paused = b_pause;

            /* When we Pause, we'll need the TimeoutPrevention thread to
             * handle sending the "Keep Alive" message to the server.
             * Unfortunately Live555 isn't thread safe and so can't
             * do this normally while the main Demux thread is handling
             * a live stream. We end up with the Timeout thread blocking
             * waiting for a response from the server. So when we PAUSE
             * we set a flag that the TimeoutPrevention function will check
             * and if it's set, it will trigger the GET_PARAMETER message */
            if( p_sys->b_paused && p_sys->p_timeout != NULL )
                p_sys->p_timeout->b_handle_keep_alive = true;
            else if( !p_sys->b_paused && p_sys->p_timeout != NULL )
                p_sys->p_timeout->b_handle_keep_alive = false;

            if( !p_sys->b_paused )
            {
                for( int i = 0; i < p_sys->i_track; i++ )
                {
                    live_track_t *tk = p_sys->track[i];
                    tk->b_rtcp_sync = false;
                    tk->i_pts = VLC_TS_INVALID;
                    p_sys->i_pcr = 0;
                    es_out_Control( p_demux->out, ES_OUT_RESET_PCR );
                }
            }

            /* Reset data received counter */
            p_sys->i_no_data_ti = 0;

            /* Retrieve the starttime if possible */
            p_sys->i_npt_start = p_sys->ms->playStartTime();

            /* Retrieve the duration if possible */
            if( p_sys->ms->playEndTime() )
                p_sys->i_npt_length = p_sys->ms->playEndTime();

            msg_Dbg( p_demux, "pause start: %f stop:%f", p_sys->i_npt_start, p_sys->i_npt_length );
            return VLC_SUCCESS;
        }
        case DEMUX_GET_TITLE_INFO:
        case DEMUX_SET_TITLE:
        case DEMUX_SET_SEEKPOINT:
            return VLC_EGENERIC;

        case DEMUX_GET_PTS_DELAY:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = INT64_C(1000)
                  * var_InheritInteger( p_demux, "network-caching" );
            return VLC_SUCCESS;

        default:
            return VLC_EGENERIC;
    }
#endif

    return OSA_SOK;
}

/*****************************************************************************
 *
 *****************************************************************************/
static void StreamRead( void *p_private, unsigned int i_size,
                        unsigned int i_truncated_bytes, struct timeval pts,
                        unsigned int duration )
{
    live_track_t   *tk = (live_track_t*)p_private;
    demux_t        *p_demux = tk->m_demux;

    //msg_Dbg( p_demux, "pts: %d", pts.tv_sec );

    int64_t i_pts = (int64_t)pts.tv_sec * INT64_C(1000000) + (int64_t)pts.tv_usec;

    /* XXX Beurk beurk beurk Avoid having negative value XXX */
    i_pts &= INT64_C(0x00ffffffffffffff);

    /* Retrieve NPT for this pts */
    tk->m_npt = tk->m_sub->getNormalPlayTime(pts);

#if 0
    if( tk->m_quicktime && tk->m_es == NULL )
    {
        QuickTimeGenericRTPSource *qtRTPSource =
            (QuickTimeGenericRTPSource*)tk->sub->rtpSource();
        QuickTimeGenericRTPSource::QTState &qtState = qtRTPSource->qtState;
        uint8_t *sdAtom = (uint8_t*)&qtState.sdAtom[4];

        /* Get codec informations from the quicktime atoms :
         * http://developer.apple.com/quicktime/icefloe/dispatch026.html */
        if( tk->fmt.i_cat == VIDEO_ES ) {
            if( qtState.sdAtomSize < 16 + 32 )
            {
                /* invalid */
                p_sys->event_data = 0xff;
                tk->waiting = 0;
                return;
            }
            tk->fmt.i_codec = VLC_FOURCC(sdAtom[0],sdAtom[1],sdAtom[2],sdAtom[3]);
            tk->fmt.video.i_width  = (sdAtom[28] << 8) | sdAtom[29];
            tk->fmt.video.i_height = (sdAtom[30] << 8) | sdAtom[31];

            if( tk->fmt.i_codec == VLC_FOURCC('a', 'v', 'c', '1') )
            {
                uint8_t *pos = (uint8_t*)qtRTPSource->qtState.sdAtom + 86;
                uint8_t *endpos = (uint8_t*)qtRTPSource->qtState.sdAtom
                                  + qtRTPSource->qtState.sdAtomSize;
                while (pos+8 < endpos) {
                    unsigned int atomLength = pos[0]<<24 | pos[1]<<16 | pos[2]<<8 | pos[3];
                    if( atomLength == 0 || atomLength > (unsigned int)(endpos-pos)) break;
                    if( memcmp(pos+4, "avcC", 4) == 0 &&
                        atomLength > 8 &&
                        atomLength <= INT_MAX )
                    {
                        tk->fmt.i_extra = atomLength-8;
                        tk->fmt.p_extra = xmalloc( tk->fmt.i_extra );
                        memcpy(tk->fmt.p_extra, pos+8, atomLength-8);
                        break;
                    }
                    pos += atomLength;
                }
            }
            else
            {
                tk->fmt.i_extra        = qtState.sdAtomSize - 16;
                tk->fmt.p_extra        = xmalloc( tk->fmt.i_extra );
                memcpy( tk->fmt.p_extra, &sdAtom[12], tk->fmt.i_extra );
            }
        }
        else {
            if( qtState.sdAtomSize < 24 )
            {
                /* invalid */
                p_sys->event_data = 0xff;
                tk->waiting = 0;
                return;
            }
            tk->fmt.i_codec = VLC_FOURCC(sdAtom[0],sdAtom[1],sdAtom[2],sdAtom[3]);
            tk->fmt.audio.i_bitspersample = (sdAtom[22] << 8) | sdAtom[23];
        }
        tk->p_es = es_out_Add( p_demux->out, &tk->fmt );
    }
#endif

#if 0
    fprintf( stderr, "StreamRead size=%d pts=%lld\n",
             i_size,
             pts.tv_sec * 1000000LL + pts.tv_usec );
#endif

    /* grow buffer if it looks like buffer is too small, but don't eat
     * up all the memory on strange streams */
    if( i_truncated_bytes > 0 )
    {
        if( tk->m_buffer_size < 2000000 )
        {
            void *p_tmp;
            fprintf( stderr, "lost %d bytes.\n", i_truncated_bytes );
            fprintf( stderr, "increasing buffer size to %d.\n", tk->m_buffer_size * 2 );
            p_tmp = realloc( tk->m_buffer, tk->m_buffer_size * 2 );
            if( p_tmp == NULL )
            {
                fprintf( stderr, "realloc failed.\n" );
            }
            else
            {
                tk->m_buffer = (uint8_t *)p_tmp;
                tk->m_buffer_size *= 2;
            }
        }

        if( tk->m_discard_trunc )
        {
            p_demux->m_event_data = 0xff;
            tk->m_waiting = 0;
            return;
        }
    }

    assert( i_size <= tk->m_buffer_size );

#if 0
    if( tk->fmt.i_codec == VLC_CODEC_AMR_NB ||
        tk->fmt.i_codec == VLC_CODEC_AMR_WB )
    {
        AMRAudioSource *amrSource = (AMRAudioSource*)tk->sub->readSource();

        p_block = block_New( p_demux, i_size + 1 );
        p_block->p_buffer[0] = amrSource->lastFrameHeader();
        memcpy( p_block->p_buffer + 1, tk->p_buffer, i_size );
    }
    else if( tk->fmt.i_codec == VLC_CODEC_H261 )
    {
        H261VideoRTPSource *h261Source = (H261VideoRTPSource*)tk->sub->rtpSource();
        uint32_t header = h261Source->lastSpecialHeader();
        p_block = block_New( p_demux, i_size + 4 );
        memcpy( p_block->p_buffer, &header, 4 );
        memcpy( p_block->p_buffer + 4, tk->p_buffer, i_size );

        if( tk->sub->rtpSource()->curPacketMarkerBit() )
            p_block->i_flags |= BLOCK_FLAG_END_OF_FRAME;
    }
#endif
    if( tk->m_codec == VIDEO_ES )
    {
        if( (tk->m_buffer[4] & 0x1f) >= 24 )
            fprintf( stderr, "unsupported NAL type for H264" );

#if 0
        /* Normal NAL type */
        p_block = block_New( p_demux, i_size + 4 );
        p_block->p_buffer[0] = 0x00;
        p_block->p_buffer[1] = 0x00;
        p_block->p_buffer[2] = 0x00;
        p_block->p_buffer[3] = 0x01;
        memcpy( &p_block->p_buffer[4], tk->p_buffer, i_size );
#endif

        /* TODO: Call the callback function to process data */
        if (!tk->m_first_frame_written)
        {

            tk->m_buffer[0] = 0x00;
            tk->m_buffer[1] = 0x00;
            tk->m_buffer[2] = 0x00;
            tk->m_buffer[3] = 0x01;

#if 0
            unsigned numSPropRecords;
            SPropRecord* sPropRecords = parseSPropParameterSets(tk->m_sub->fmtp_spropparametersets(), numSPropRecords);
            for (unsigned i = 0; i < numSPropRecords; ++i)
            {
                (*p_demux->m_fxn)(tk->m_buffer, 4, tk->m_codec, p_demux->m_userdata);
                (*p_demux->m_fxn)(sPropRecords[i].sPropBytes, sPropRecords[i].sPropLength, tk->m_codec, p_demux->m_userdata);
            }
            delete[] sPropRecords;
#endif

            tk->m_first_frame_written = true;
        }
    }
#if 0
    else if( tk->b_asf )
    {
        p_block = StreamParseAsf( p_demux, tk,
                                  tk->sub->rtpSource()->curPacketMarkerBit(),
                                  tk->p_buffer, i_size );
    }
    else
    {
        p_block = block_New( p_demux, i_size );
        memcpy( p_block->p_buffer, tk->p_buffer, i_size );
    }
#endif
	else if( tk->m_codec == AUDIO_ES )
	{
		u_int8_t audio_object_type = 0;
		u_int8_t sampling_frequency_index = 0;
		u_int8_t channel_configuration = 0;
		u_int8_t hdr[7];
		unsigned char hexConfig[10];


		hexConfig[0] = tk->aacCfg[0];
		hexConfig[1] = tk->aacCfg[1];
		hexConfig[2] = 0;
		
		audio_object_type = (hexConfig[0] & 0xF8) >> 3;
		sampling_frequency_index = ((hexConfig[0] & 0x03) << 1) | ((hexConfig[1] & 0x80) >> 7);
		channel_configuration = (hexConfig[1] & 0x78) >> 3;

		hdr[0] = 0xFF;
		hdr[1] = (0x0F << 4) | 0x01;

		hdr[2] = ((audio_object_type - 1) << 6) | (sampling_frequency_index << 2) | channel_configuration >> 2;
		hdr[3] = (channel_configuration << 6 );
		u_int16_t aac_frm_size = i_size + 7;//ADTS_HEADER_SIZE;

		hdr[3] = hdr[3] | (aac_frm_size >> 11);
		hdr[4] = (aac_frm_size >> 3);
		hdr[5] = (aac_frm_size << 5);
		hdr[6] = (i_size/1024);

		memcpy(tk->m_buffer,hdr,7);

	}

    if( p_demux->m_pcr < i_pts )
    {
        p_demux->m_pcr = i_pts;
    }

    /* Update our global npt value */
    if( tk->m_npt > 0 && tk->m_npt > p_demux->m_npt &&
        ( tk->m_npt < p_demux->m_npt_length || p_demux->m_npt_length <= 0 ) )
        p_demux->m_npt = tk->m_npt;

#if 0
    if( p_block )
    {
        if( !tk->b_muxed && !tk->b_asf )
        {
            if( i_pts != tk->i_pts )
                p_block->i_pts = VLC_TS_0 + i_pts;
            /*FIXME: for h264 you should check that packetization-mode=1 in sdp-file */
            p_block->i_dts = ( tk->fmt.i_codec == VLC_CODEC_MPGV ) ? VLC_TS_INVALID : (VLC_TS_0 + i_pts);
        }

        if( tk->b_muxed )
            stream_DemuxSend( tk->p_out_muxed, p_block );
        else if( tk->b_asf )
            stream_DemuxSend( p_sys->p_out_asf, p_block );
        else
            es_out_Send( p_demux->out, tk->p_es, p_block );
    }
#endif

    /* Call the callback function to process data */
    (*p_demux->m_fxn)(tk->m_buffer, i_size + tk->m_offset, tk->m_codec, p_demux->m_userdata);

    /* warn that's ok */
    p_demux->m_event_data = 0xff;

    /* we have read data */
    tk->m_waiting = 0;
    p_demux->m_no_data = false;
    p_demux->m_no_data_ti = 0;

    if( i_pts > 0 && !tk->m_muxed )
    {
        tk->m_pts = i_pts;
    }
}

/*****************************************************************************
 *
 *****************************************************************************/
static void StreamClose( void *p_private )
{
    live_track_t   *tk = (live_track_t*)p_private;
    demux_t        *p_demux = tk->m_demux;

    //fprintf( stderr, "StreamClose.\n" );

    p_demux->m_event_rtsp = 0xff;
    p_demux->m_event_data = 0xff;
    p_demux->m_error = true;
}


/*****************************************************************************
 *
 *****************************************************************************/
static void TaskInterruptRTSP( void *p_private )
{
    demux_t *p_demux = (demux_t *)p_private;

    /* Avoid lock */
    p_demux->m_event_rtsp = 0xff;
}

static void TaskInterruptData( void *p_private )
{
    demux_t *p_demux = (demux_t *)p_private;

    p_demux->m_no_data_ti++;

    /* Avoid lock */
    p_demux->m_event_data = 0xff;
}


