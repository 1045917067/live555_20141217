/** ============================================================================
 *
 *  rtspserver_api.h
 *
 *  Author     : zzx.
 *
 *  Date       : Dec 30, 2014
 *
 *  Description: 
 *  ============================================================================
 */

#if !defined (__RTSPSERVER_API_H)
#define __RTSPSERVER_API_H

/*  --------------------- Include system headers ---------------------------- */

/*  --------------------- Include user headers   ---------------------------- */
#include "scallback.h"

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

/*
 *  --------------------- Data type definition ---------------------------------
 */
typedef int             status_t;

typedef unsigned int    rtsp_t;

typedef unsigned int    sms_t;

/** ----------------------------------------------------------------------------
 *  @Name:          Structure name
 *
 *  @Description:   Description of the structure.
 *
 *  @Field:         Field1 member
 *
 *  @Field:         Field2 member
 *  ----------------------------------------------------------------------------
 */
typedef status_t (*AUDIO_CALLBACK_FXN)(void *addr, unsigned size, unsigned *psz, void *ud);

typedef struct rtsp_prm_t
{
    unsigned short  m_port;
} rtsp_prm_t;

typedef struct sms_prm_t
{
    unsigned char   m_name[32];
} sms_prm_t;

typedef enum subms_type_t
{
    SUBMS_TYPE_VIDEO = 0,
    SUBMS_TYPE_AUDIO = 1,
} subms_type_t;

typedef struct subms_prm_t
{
    subms_type_t    m_av_type;
    STREAM_CALLBACK_FXN m_cbs_fxn;
    void *          m_userdata;
} subms_prm_t;

/*
 *  --------------------- Public function declaration --------------------------
 */
status_t    rtsp_create(rtsp_t *rtsp, const rtsp_prm_t *prm);

status_t    rtsp_create_sms(rtsp_t rtsp, sms_prm_t *ms_prm, sms_t *sms);

status_t    rtsp_sms_add_subms(sms_t sms, subms_prm_t *subms_prm);

status_t    rtsp_process(rtsp_t rtsp);

status_t    rtsp_delete_sms(rtsp_t rtsp, sms_t *sms);

status_t    rtsp_delete(rtsp_t *rtsp);

status_t    rtsp_server_init();

status_t    rtsp_server_deinit();

#if defined(__cplusplus)
}
#endif  /* defined(__cplusplus) */

#endif  /* if !defined (__RTSPSERVER_API_H) */
