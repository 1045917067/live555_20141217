/** ============================================================================
 *
 *  RTSPServerApi.cpp
 *
 *  Author     : zzx.
 *
 *  Date       : Dec 30, 2014
 *
 *  Description: 
 *  ============================================================================
 */

/*  --------------------- Include system headers ---------------------------- */
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/*  --------------------- Include user headers   ---------------------------- */
#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include <strDup.hh>

#include "H264CBServerMediaSubsession.hh"
#include "ADTSCBServerMediaSubsession.hh"
#include "MediaSystemTaskScheduler.hh"

#include "rtspserver_api.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 *  --------------------- Macro definition -------------------------------------
 */

/** ============================================================================
 *  @Macro:         Macro name
 *
 *  @Description:   Description of this macro.
 *  ============================================================================
 */
#define RTSPSERVER_MEDIASTM_MAX     (4)

#define RTSPSERVER_DEFAULT_PORT     (8554)

#define RTSPSERVER_GET_HANDLE(hdl)  ((rtspserver_handle)(hdl))

#define SMS_GET_HANDLE(hdl)         ((sms_handle)(hdl))
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
struct rtspserver_object_t;
typedef struct rtspserver_object_t * rtspserver_handle;

struct server_media_session_t
{
    unsigned int             m_used;
    unsigned int             m_idx;
    unsigned char            m_name[32];

    ServerMediaSession     * m_sms;
    rtspserver_handle        m_rtsp;
};

typedef struct server_media_session_t   server_media_session_t;
typedef struct server_media_session_t * sms_handle;

struct rtspserver_object_t
{
    rtsp_prm_t               m_prm;
    TaskScheduler          * m_schd;
    UsageEnvironment       * m_env;
    RTSPServer             * m_rtsp;
    server_media_session_t   m_svr_sms[RTSPSERVER_MEDIASTM_MAX];

    pthread_mutex_t          m_mutex;
    char                     m_exit;
};

/*
 *  --------------------- Global variable definition ---------------------------
 */

/** ----------------------------------------------------------------------------
 *  @Name:          Variable name
 *
 *  @Description:   Description of the variable.
 * -----------------------------------------------------------------------------
 */
char const * descriptionString = "Session streamed by media server";

/*
 *  --------------------- Local function forward declaration -------------------
 */
static void * __rtspserver_runstub_fxn(void *arg);

static status_t
__rtspserver_alloc_sms(rtspserver_handle hdl, sms_handle *sms);

static status_t
__rtspserver_free_sms(rtspserver_handle hdl, sms_handle sms);

static void __rtspserver_announceStream(RTSPServer *rtspServer, ServerMediaSession *sms);

/*
 *  --------------------- Public function definition ---------------------------
 */
status_t    rtsp_create(rtsp_t *rtsp, const rtsp_prm_t *prm)
{
    int i;
    rtspserver_handle hdl = NULL;
    if (rtsp == NULL || prm == NULL) {
        return -EINVAL;
    }

    hdl = (rtspserver_handle)calloc(1, sizeof(*hdl));
    if (hdl == NULL) {
        return OSA_EMEM;
    }

    for (i = 0; i < RTSPSERVER_MEDIASTM_MAX; i++) {
        memset(&hdl->m_svr_sms[i], 0, sizeof(server_media_session_t));
        hdl->m_svr_sms[i].m_used = False;
    }

    hdl->m_prm = (*prm);

    pthread_mutex_init(&hdl->m_mutex, NULL);

    /* Create Task scheduler */
    hdl->m_schd = MediaSystemTaskScheduler::createNew();

    /* Create usage env */
    hdl->m_env = BasicUsageEnvironment::createNew(*hdl->m_schd);

    /* Set the max size of output packet */
    OutPacketBuffer::maxSize = 2 * 512 * 1024;

    UserAuthenticationDatabase *authDB = NULL;
#ifdef ACCESS_CONTROL
    // To implement client access control to the RTSP server, do the following:
    authDB = new UserAuthenticationDatabase;
    authDB->addUserRecord("username1", "password1"); // replace these with real strings
    // Repeat the above with each <username>, <password> that you wish to allow
    // access to the server.
#endif

    /* Create rtsp server */
    hdl->m_rtsp = RTSPServer::createNew(*hdl->m_env, hdl->m_prm.m_port, authDB);

    if (hdl->m_rtsp == NULL) {
        fprintf(stderr, "%s\n", hdl->m_env->getResultMsg());
        delete hdl->m_schd;
        free(hdl);
        return OSA_EFAIL;
    }

    (*rtsp) = (rtsp_t)hdl;

    return OSA_SOK;
}

status_t    rtsp_create_sms(rtsp_t rtsp, sms_prm_t *ms_prm, sms_t *sms)
{
    status_t status = OSA_SOK;
    rtspserver_handle hdl = NULL;
    sms_handle sms_hdl = NULL;

    hdl = RTSPSERVER_GET_HANDLE(rtsp);

    if (hdl == NULL || ms_prm == NULL || sms == NULL) {
        return OSA_EARGS;
    }

    status = __rtspserver_alloc_sms(hdl, &sms_hdl);
    if (OSA_ISERROR(status)) {
        return status;
    }

    /* Initiailize sms prm */
    snprintf((char *)sms_hdl->m_name, sizeof(sms_hdl->m_name) - 1, "%s", ms_prm->m_name);

    /* Create server medis session */
    sms_hdl->m_sms = ServerMediaSession::createNew(*hdl->m_env, (char const *)sms_hdl->m_name,
            (char const *)sms_hdl->m_name, descriptionString);
    if (sms_hdl->m_sms == NULL) {
        __rtspserver_free_sms(hdl, sms_hdl);
        return OSA_EFAIL;
    }

    hdl->m_rtsp->addServerMediaSession(sms_hdl->m_sms);

    sms_hdl->m_rtsp = hdl;

    __rtspserver_announceStream(hdl->m_rtsp, sms_hdl->m_sms);

    (*sms) = (sms_t)sms_hdl;

    return status;
}

status_t    rtsp_sms_add_subms(sms_t sms, subms_prm_t *subms_prm)
{
    status_t status = OSA_SOK;
    sms_handle hdl = NULL;

    hdl = SMS_GET_HANDLE(sms);

    if (hdl == NULL || subms_prm == NULL) {
        return OSA_EARGS;
    }

    if (subms_prm->m_av_type == SUBMS_TYPE_VIDEO) {

        hdl->m_sms->addSubsession(H264CBServerMediaSubsession
                ::createNew(*(hdl->m_rtsp->m_env), subms_prm->m_cbs_fxn, subms_prm->m_userdata));
    }
#if 1
    else if (subms_prm->m_av_type == SUBMS_TYPE_AUDIO) {
        hdl->m_sms->addSubsession(ADTSCBServerMediaSubsession
                ::createNew(*(hdl->m_rtsp->m_env), subms_prm->m_cbs_fxn, subms_prm->m_userdata));
    } 
#endif
    else {
        status = OSA_EINVAL;
    }

    return status;
}

status_t    rtsp_process(rtsp_t rtsp)
{
    rtspserver_handle hdl = NULL;
    MediaSystemTaskScheduler *msTskScheduler = NULL;

    hdl = RTSPSERVER_GET_HANDLE(rtsp);
    if (hdl == NULL) {
        return OSA_EARGS;
    }

#if 1
    msTskScheduler = dynamic_cast<MediaSystemTaskScheduler *>(&hdl->m_env->taskScheduler());
    msTskScheduler->doEventLoop2(NULL);
#else
    hdl->m_env->taskScheduler().doEventLoop(NULL);
#endif
    
    return OSA_SOK;
}

status_t    rtsp_delete_sms(rtsp_t rtsp, sms_t *sms)
{
    status_t status = OSA_SOK;
    rtspserver_handle hdl = NULL;
    sms_handle sms_hdl = NULL;

    hdl = RTSPSERVER_GET_HANDLE(rtsp);
    sms_hdl = SMS_GET_HANDLE(*sms);
    if (hdl == NULL || sms_hdl == NULL) {
        return OSA_EARGS;
    }

    /* Remove sms */
    hdl->m_rtsp->removeServerMediaSession(sms_hdl->m_sms);
    sms_hdl->m_sms = NULL;

    __rtspserver_free_sms(hdl, sms_hdl);

    (*sms) = 0;

    return status;
}

status_t    rtsp_delete(rtsp_t *rtsp)
{
    status_t status = OSA_SOK;
    rtspserver_handle hdl = NULL;

    hdl = RTSPSERVER_GET_HANDLE(*rtsp);
    if (hdl == NULL) {
        return OSA_EARGS;
    }

    pthread_mutex_destroy(&hdl->m_mutex);
    /* Delete server media session */
#if 0
    delete hdl->m_sms;
    hdl->m_sms = NULL;
    delete hdl->m_rtsp;
    hdl->m_rtsp = NULL;
    delete hdl->m_env;
    hdl->m_env = NULL;
#endif
    hdl->m_env->reclaim(); hdl->m_env = NULL;

    delete hdl->m_schd;
    hdl->m_schd = NULL;

    free(hdl);

    (*rtsp) = 0;

    return status;
}

/*
 *  --------------------- Local function definition ----------------------------
 */
static void * __rtspserver_runstub_fxn(void *arg)
{
    rtspserver_handle hdl = (rtspserver_handle)arg;

    if (hdl == NULL) {
        fprintf(stderr, "Invalid arguments.\n");
        return NULL;
    }

    /* Do the event loop */
    while (!hdl->m_exit) {
        hdl->m_env->taskScheduler().doEventLoop(NULL);
    }

    return ((void *)hdl);
}

static status_t
__rtspserver_alloc_sms(rtspserver_handle hdl, sms_handle *sms)
{
    int i;
    status_t status = OSA_ENOENT;
    sms_handle sms_hdl = NULL;

    (*sms) = NULL;

    pthread_mutex_lock(&hdl->m_mutex);

    for (i = 0; i < RTSPSERVER_MEDIASTM_MAX; i++) {
        sms_hdl = &hdl->m_svr_sms[i];

        if (!sms_hdl->m_used) {
            sms_hdl->m_used = True;
            sms_hdl->m_idx  = i;
            (*sms) = sms_hdl;
            status = OSA_SOK;
            break;
        }
    }

    pthread_mutex_unlock(&hdl->m_mutex);

    return status;
}

static status_t
__rtspserver_free_sms(rtspserver_handle hdl, sms_handle sms)
{
    status_t status = OSA_SOK;

    pthread_mutex_lock(&hdl->m_mutex);

    sms->m_used = False;

    pthread_mutex_unlock(&hdl->m_mutex);

    return status;
}

static void __rtspserver_announceStream(RTSPServer *rtspServer, ServerMediaSession *sms)
{
  char* url = rtspServer->rtspURL(sms);
  UsageEnvironment& env = rtspServer->envir();
  env << "Play this stream using the URL \"" << url << "\"\n";
  delete[] url;
}

#if defined(__cplusplus)
}
#endif
