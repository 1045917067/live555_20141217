/** ============================================================================
 *
 *  osa_status.h
 *
 *  Author     : zzx
 *
 *  Date       : Dec 30, 2014
 *
 *  Description: 
 *  ============================================================================
 */

#if !defined (__OSA_STATUS_H)
#define __OSA_STATUS_H

/*  --------------------- Include system headers ---------------------------- */

/*  --------------------- Include user headers   ---------------------------- */

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

#ifdef  OSA_SOK
#undef  OSA_SOK
#endif
#define OSA_SOK         0

#ifdef  OSA_EFAIL
#undef  OSA_EFAIL
#endif

#define OSA_EFAIL       1
#define OSA_EMEM        2
#define OSA_ENOENT      3
#define OSA_EEXIST      4
#define OSA_EARGS       5
#define OSA_EINVAL      6
#define OSA_ETIMEOUT    7
#define OSA_EEOF        8
#define OSA_ERES        9
#define OSA_ECONNECT    10

#ifdef  OSA_ISERROR
#undef  OSA_ISERROR
#endif
#define OSA_ISERROR(status)     (status != OSA_SOK)


/*
 *  --------------------- Data type definition ---------------------------------
 */

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
typedef int status_t;

/*
 *  --------------------- Public function declaration --------------------------
 */

#if defined(__cplusplus)
}
#endif  /* defined(__cplusplus) */

#endif  /* if !defined (__OSA_STATUS_H) */
