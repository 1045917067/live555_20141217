/** ============================================================================
 *
 *  live555.h
 *
 *  Author     : zzx.
 *
 *  Date       : Dec 30, 2014
 *
 *  Description: 
 *  ============================================================================
 */

#if !defined (__LIVE555_H)
#define __LIVE555_H

/*  --------------------- Include system headers ---------------------------- */

/*  --------------------- Include user headers   ---------------------------- */
#include "osa_status.h"

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
#define VIDEO_ES    (0)

#define AUDIO_ES    (1)

/*
 *  --------------------- Data type definition ---------------------------------
 */
struct __demux_t; typedef struct __demux_t demux_t;

typedef status_t (*DEMUX_CALLBACK_FXN)(void *buf, unsigned int size, int av_type, void *ud);

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
struct __demux_prm_t;
typedef struct __demux_prm_t demux_prm_t;
struct __demux_prm_t
{
    unsigned char   m_url[128];
};

/*
 *  --------------------- Public function declaration --------------------------
 */
status_t demux_open(demux_t **pdemux, const demux_prm_t *prm);

status_t demux_exec(demux_t *pdemux, DEMUX_CALLBACK_FXN fxn, void *ud);

status_t demux_control(demux_t *pdemux, int cmd, ...);

status_t demux_close(demux_t **pdemux);

#if defined(__cplusplus)
}
#endif  /* defined(__cplusplus) */

#endif  /* if !defined (__LIVE555_H) */
