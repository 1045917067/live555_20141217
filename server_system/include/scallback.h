/** ============================================================================
 *
 *  scallback.h
 *
 *  Author     : zzx.
 *
 *  Date       : Dec 30, 2014
 *
 *  Description: 
 *  ============================================================================
 */

#if !defined (__SCALLBACK_H)
#define __SCALLBACK_H

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
enum av_type_t
{
    AV_TYPE_VIDEO = 0,
    AV_TYPE_AUDIO = 1,
};

enum av_state_t
{
    AV_STATE_OPEN = 0,
    AV_STATE_CLOSE = 1,
    AV_STATE_PROC = 2,
};

/*
 *  --------------------- Data type definition ---------------------------------
 */

typedef status_t (*STREAM_CALLBACK_FXN)(void *buf, size_t size, size_t *psz, int av_state, void *ud);

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

/*
 *  --------------------- Public function declaration --------------------------
 */

#if defined(__cplusplus)
}
#endif  /* defined(__cplusplus) */

#endif  /* if !defined (__SCALLBACK_H) */
