/** ============================================================================
 *
 *  tsmux_main.c
 *
 *  Author     : zzx
 *
 *  Date       : Sep 17, 2013
 *
 *  Description: 
 *  ============================================================================
 */

/*  --------------------- Include system headers ---------------------------- */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <time.h>
#include <sys/time.h>

#include <pthread.h>

/*  --------------------- Include user headers   ---------------------------- */
#include "live555.h"

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
#define TSMUX_TMP_BUFFER_SIZE   (1024 *1024)

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

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
struct __demux_object_t;
typedef struct __demux_object_t demux_object_t;
struct __demux_object_t
{
    demux_prm_t     m_prm;
    int             m_exit;
    FILE          * m_vout;
    FILE          * m_aout;

    demux_t       * m_demux;
};
#if 0
struct __rtspclient_object_t;
typedef struct __rtspclient_object_t rtspclient_object_t;
struct __rtspclient_object_t
{
    unsigned char * m_ves;
    unsigned char * m_aes;
    unsigned int    m_ves_size;
    unsigned int    m_aes_size;

    RingBufferHandle_t
                    m_video_que;
    RingBufferHandle_t
                    m_audio_que;
};
#endif
/*
 *  --------------------- Global variable definition ---------------------------
 */

/** ----------------------------------------------------------------------------
 *  @Name:          Variable name
 *
 *  @Description:   Description of the variable.
 * -----------------------------------------------------------------------------
 */
static demux_object_t   glb_demux_obj;

/*
 *  --------------------- Local function forward declaration -------------------
 */

/** ============================================================================
 *
 *  @Function:	    Local function forward declaration.
 *
 *  @Description:   //	函数功能、性能等的描述
 *
 *  @Calls:	        //	被本函数调用的函数清单
 *
 *  @Called By:	    //	调用本函数的函数清单
 *
 *  @Table Accessed://	被访问的表（此项仅对于牵扯到数据库操作的程序）
 *
 *  @Table Updated: //	被修改的表（此项仅对于牵扯到数据库操作的程序）
 *
 *  @Input:	        //	对输入参数的说明
 *
 *  @Output:	    //	对输出参数的说明
 *
 *  @Return:	    //	函数返回值的说明
 *
 *  @Enter          //  Precondition
 *
 *  @Leave          //  Postcondition
 *
 *  @Others:	    //	其它说明
 *
 *  ============================================================================
 */
static status_t __demux_callback_fxn(void *buf, unsigned int size, int av_type, void *ud);

/*
 *  --------------------- Public function definition ---------------------------
 */

/** ============================================================================
 *
 *  @Function:	    Public function definition.
 *
 *  @Description:   //	函数功能、性能等的描述
 *
 *  @Calls:	        //	被本函数调用的函数清单
 *
 *  @Called By:	    //	调用本函数的函数清单
 *
 *  @Table Accessed://	被访问的表（此项仅对于牵扯到数据库操作的程序）
 *
 *  @Table Updated: //	被修改的表（此项仅对于牵扯到数据库操作的程序）
 *
 *  @Input:	        //	对输入参数的说明
 *
 *  @Output:	    //	对输出参数的说明
 *
 *  @Return:	    //	函数返回值的说明
 *
 *  @Enter          //  Precondition
 *
 *  @Leave          //  Postcondition
 *
 *  @Others:	    //	其它说明
 *
 *  ============================================================================
 */
int main(int argc, char *argv[])
{
    int           status = 0;
    static int    count  = 0;
    demux_object_t * pobj = &glb_demux_obj;

    snprintf(pobj->m_prm.m_url, sizeof(pobj->m_prm.m_url) - 1, "%s", argv[1]);

    pobj->m_vout = fopen("v.es", "wb+");

    status = demux_open(&pobj->m_demux, &pobj->m_prm);

    while (1) {

        status = demux_exec(pobj->m_demux, __demux_callback_fxn, (void *)pobj);

        if (status != 0) {
            fprintf(stderr, "demux: Failed to demux data, exit...\n");
            break;
        }

        //usleep(15000);
    }

    fclose(pobj->m_vout);

    status = demux_close(&pobj->m_demux);

    return 0;
}

/*
 *  --------------------- Local function definition ----------------------------
 */

/** ============================================================================
 *
 *  @Function:	    Local function definition.
 *
 *  @Description:   //	函数功能、性能等的描述
 *
 *  ============================================================================
 */
static status_t __demux_callback_fxn(void *buf, unsigned int size, int av_type, void *ud)
{
    demux_object_t *pobj = (demux_object_t *)ud;
    FILE * out = NULL;

	if (av_type == VIDEO_ES) {
		static FILE *vfp = NULL;
		if(vfp == NULL)
			vfp = fopen("vout.h264","wb+");
		fwrite(buf,size,1,vfp);
		out = pobj->m_vout;
	} else {
		static FILE *afp = NULL;
		if(afp == NULL)
			afp = fopen("aout.aac","wb+");
		
		fwrite(buf,size,1,afp);

        out = pobj->m_aout;
    }

    if (size > 0 && out) {
        fwrite(buf, size, 1, out);
    }

    return 0;
}

#if defined(__cplusplus)
}
#endif
