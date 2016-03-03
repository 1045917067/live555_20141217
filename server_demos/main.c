/** ============================================================================
 *
 *  main.c
 *
 *  Author     : zzx
 *
 *  Date       : Sep 02, 2013
 *
 *  Description: 
 *  ============================================================================
 */

/*  --------------------- Include system headers ---------------------------- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

/*  --------------------- Include user headers   ---------------------------- */
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
#define TSMUX_TMP_BUFFER_SIZE   (1024 *1024)

#define	STREAM_VIDEO

#define	STREAM_AUDIO

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
struct __stream_state_t; typedef struct __stream_state_t stream_state_t;
struct __stream_state_t
{
    int        m_buf_size;
    int        m_buf_len;
    int        m_read_len;

    int        m_cur_frm_id;

    int        m_cur_frm_addr;
    int        m_next_frm_addr;

    int        m_prev_nal_type;
    int        m_prev_vcl_nal_flag;
    int        m_curr_nal_type;
    int        m_curr_vcl_nal_flag;
};

struct __rtspsvr_object_t;
typedef struct __rtspsvr_object_t rtspsvr_object_t;
struct __rtspsvr_object_t
{
    int              m_exit;
    int              m_vcount;
    int              m_acount;
    FILE           * m_vin;
    FILE           * m_ain;

    unsigned char  * m_buf;
    stream_state_t   m_strm_state;

    unsigned char  * m_v_pes;
    unsigned char  * m_a_pes;
    unsigned int     m_v_es_size;
    unsigned int     m_a_es_size;
    unsigned char  * m_pts;
    unsigned int     m_ts_size;
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
static int glb_daemon_exit = 0;
static int glb_strm_restart = 0;

static rtspsvr_object_t glb_rtsp_obj;

static pthread_mutex_t glb_vmutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t glb_amutex = PTHREAD_MUTEX_INITIALIZER;

/*
 *  --------------------- Local function forward declaration -------------------
 */
static void rtsp_daemon_signal_handler(int signum);

static void rtsp_daemon_alarm_handler(int signum);

static void
__rtspsvr_stream_state_reset(stream_state_t *pst)
{
    pst->m_buf_len           = 0;
    pst->m_cur_frm_id        = 0;
    pst->m_cur_frm_addr      = 0;
    pst->m_next_frm_addr     = 0;

    pst->m_prev_nal_type     = 9;
    pst->m_prev_vcl_nal_flag = 0;
    pst->m_curr_nal_type     = 9;
    pst->m_curr_vcl_nal_flag = 0;
}

static int __rtspsvr_init(rtspsvr_object_t *pobj)
{
    status_t status = OSA_SOK;

    memset(pobj, 0, sizeof(*pobj));

    pobj->m_vcount = 0;
    pobj->m_acount = 0;

    pobj->m_vin = fopen("vin.h264", "rb");
    if (pobj->m_vin == NULL) {
        return -1;
    }
#if 	0//defined(STREAM_AUDIO)
    pobj->m_ain = fopen("ain.aac", "rb");
    if (pobj->m_ain == NULL) {
        fclose(pobj->m_ain);
        return -1;
    }
#endif
#if 0

    pobj->m_fout = fopen("out.ts", "wb+");
    if (pobj->m_fout == NULL) {
        fclose(pobj->m_vin);
        fclose(pobj->m_ain);
        return -EINVAL;
    }
#endif

    __rtspsvr_stream_state_reset(&pobj->m_strm_state);

    pobj->m_buf = (unsigned char *)malloc(sizeof(unsigned char) * TSMUX_TMP_BUFFER_SIZE * 4);

    if (pobj->m_buf == NULL) {
        fclose(pobj->m_vin);
        //fclose(pobj->m_ain);
        //fclose(pobj->m_fout);
        return -1;
    }

    pobj->m_v_pes = pobj->m_buf + TSMUX_TMP_BUFFER_SIZE;
    pobj->m_v_es_size = 0;

    pobj->m_a_pes = pobj->m_v_pes + TSMUX_TMP_BUFFER_SIZE;
    pobj->m_a_es_size = 0;

    pobj->m_pts = pobj->m_a_pes + TSMUX_TMP_BUFFER_SIZE;
    pobj->m_ts_size = 0;

    pobj->m_strm_state.m_buf_size = TSMUX_TMP_BUFFER_SIZE;
    pobj->m_strm_state.m_read_len = TSMUX_TMP_BUFFER_SIZE / 8;

    return 0;
}

static int __rtspsvr_get_video_frame(rtspsvr_object_t *tsmux)
{
    int status = 0;
    int retval = 0;
    size_t readSize = 0;

    stream_state_t *pstrm_st = &tsmux->m_strm_state;

    unsigned int first_mb_in_slice_flag = 0;
    unsigned int this_nalu_end_au       = FALSE;

    tsmux->m_v_es_size = 0;

    while (1) {

        if (pstrm_st->m_next_frm_addr < pstrm_st->m_buf_len - 6) {

            if ( tsmux->m_buf[pstrm_st->m_next_frm_addr + 0] == 0x00 &&
                 tsmux->m_buf[pstrm_st->m_next_frm_addr + 1] == 0x00 &&
                 tsmux->m_buf[pstrm_st->m_next_frm_addr + 2] == 0x00 &&
                 tsmux->m_buf[pstrm_st->m_next_frm_addr + 3] == 0x01 ) {

                pstrm_st->m_curr_nal_type = tsmux->m_buf[pstrm_st->m_next_frm_addr + 4] & 0x1F;
                first_mb_in_slice_flag    = tsmux->m_buf[pstrm_st->m_next_frm_addr + 5] & 0x80;

                pstrm_st->m_curr_vcl_nal_flag = ((pstrm_st->m_curr_nal_type) > 0 && (pstrm_st->m_curr_nal_type <= 5));

                if (pstrm_st->m_curr_vcl_nal_flag 
                       && pstrm_st->m_prev_vcl_nal_flag
                       && first_mb_in_slice_flag) {
                    this_nalu_end_au = TRUE;
                }

                if (pstrm_st->m_prev_vcl_nal_flag && !pstrm_st->m_curr_vcl_nal_flag) {
                    this_nalu_end_au = TRUE;
                }

                pstrm_st->m_prev_nal_type     = pstrm_st->m_curr_nal_type;
                pstrm_st->m_prev_vcl_nal_flag = pstrm_st->m_curr_vcl_nal_flag;

                if (this_nalu_end_au) {
                    break;
                }
            }

            pstrm_st->m_next_frm_addr++;

        } else {
            if (pstrm_st->m_buf_len + pstrm_st->m_read_len > pstrm_st->m_buf_size) {
                pstrm_st->m_buf_len = pstrm_st->m_buf_len - pstrm_st->m_cur_frm_addr;
                memmove(tsmux->m_buf, tsmux->m_buf + pstrm_st->m_cur_frm_addr, pstrm_st->m_buf_len);
                pstrm_st->m_next_frm_addr = pstrm_st->m_next_frm_addr - pstrm_st->m_cur_frm_addr;
                pstrm_st->m_cur_frm_addr = 0;
            }

            readSize = fread(tsmux->m_buf + pstrm_st->m_buf_len, 1, pstrm_st->m_read_len, tsmux->m_vin);
            if (readSize < pstrm_st->m_read_len) {
                rewind(tsmux->m_vin);
            }

            pstrm_st->m_buf_len += readSize;
        }
    } 

    if (this_nalu_end_au) {
        tsmux->m_v_es_size = pstrm_st->m_next_frm_addr - pstrm_st->m_cur_frm_addr;
        memcpy(tsmux->m_v_pes, tsmux->m_buf + pstrm_st->m_cur_frm_addr, tsmux->m_v_es_size);

        pstrm_st->m_cur_frm_addr   = pstrm_st->m_next_frm_addr;
        pstrm_st->m_next_frm_addr += 4;
    }

    return status;
}
static int __rtspsvr_get_audio_frame(rtspsvr_object_t *tsmux)
{
    int status = 0;
    int retval = 0;
    size_t readSize = 0;

	unsigned char   crc_check;
    unsigned char   aac_headers[7] = { 0 };
    unsigned int    frame_size;
    unsigned int    bytes_to_read;
    unsigned short  syncword;

    static FILE * fp = NULL;

    (readSize) = 0;

    if (fp == NULL) {
        fp = fopen("ain.aac", "rb");
        if (fp == NULL) {
            return -1;
        }
    }

    if (fread(aac_headers, 1, sizeof(aac_headers), fp) < sizeof(aac_headers)
            || feof(fp) || ferror(fp)) {
        fclose(fp);
        fp = NULL;
        return -1;
    }

#define DEBUG
#ifdef  DEBUG
    int i;
    for (i = 0; i < 7; i++)
        fprintf(stderr, "0x%02x ", aac_headers[i]);
    fprintf(stderr, "\n");

    unsigned char profile = (aac_headers[2] & 0xC0) >> 6;
    unsigned char sampling_frequency_index = (aac_headers[2] & 0x3C) >> 2;
    unsigned char channel_configuration =
        ((aac_headers[2] & 0x01) << 2) | ((aac_headers[3] & 0xC0) >> 6);

    unsigned char audio_config[2];
    unsigned char audio_object_type = profile + 1;
    audio_config[0] = (audio_object_type << 3) | (sampling_frequency_index >> 1);
    audio_config[1] = (sampling_frequency_index << 7) | (channel_configuration << 3);
    fprintf(stderr, "AAC config Str: %02x%02x\n", audio_config[0], audio_config[1]);
#endif

    crc_check  = aac_headers[1] & 0x01;
    frame_size = ((aac_headers[3] & 0x03) << 11) 
        | (aac_headers[4] << 3) | ((aac_headers[5] & 0xE0) >> 5);

#ifdef  DEBUG
    syncword = (aac_headers[0] << 4) | (aac_headers[1] >> 4);
    fprintf(stderr, "AAC frame header info:\n"
            "syncword  : 0x%x\n"
            "crc_check : %d\n"
            "frame size: %d\n"
            "profile   : %d\n"
            "sam index : %d\n"
            "chl config: %d\n",
            syncword, crc_check, frame_size, 
            profile, sampling_frequency_index, channel_configuration);
#endif

#if 0
    bytes_to_read = 
        frame_size > sizeof(aac_headers) ? frame_size - sizeof(aac_headers) : 0;

    // If there's a crc_check field, skip it:
    if (!crc_check) {
        fseek(fp, 2, SEEK_CUR);
        bytes_to_read = bytes_to_read > 2 ? bytes_to_read - 2 : 0;
    }


	memcpy(tsmux->m_a_pes, aac_headers,sizeof(aac_headers));
    (tsmux->m_a_es_size) = fread(tsmux->m_a_pes+sizeof(aac_headers), 1, bytes_to_read, fp) + sizeof(aac_headers);
#else
	
	fseek(fp, -sizeof(aac_headers), SEEK_CUR);
	(tsmux->m_a_es_size) = fread(tsmux->m_a_pes, 1, frame_size, fp);
#endif



    return status;
}
static void __rtspsvr_deinit(rtspsvr_object_t *pobj)
{
    if (pobj->m_vin != NULL) {
        fclose(pobj->m_vin);
    }
#if 0
    if (pobj->m_ain != NULL) {
        fclose(pobj->m_ain);
    }
    if (pobj->m_fout != NULL) {
        fclose(pobj->m_fout);
    }
#endif
    if (pobj->m_buf != NULL) {
        free(pobj->m_buf);
    }
}

status_t __h264video_source_fxn(void *buf, size_t size, size_t *psz, int av_state, void *ud)
{
    rtspsvr_object_t *pobj = (rtspsvr_object_t *)ud;

    //fprintf(stderr, "__h264video_source_fxn: Enter.\n");
    if (av_state == 1) {

//        fprintf(stderr, "Close stream.\n");
        pthread_mutex_lock(&glb_vmutex);

        pobj->m_vcount--;

        if (pobj->m_vcount == 0) {
            pobj->m_v_es_size = 0;
            __rtspsvr_stream_state_reset(&pobj->m_strm_state);
            rewind(pobj->m_vin);
        }

        pthread_mutex_unlock(&glb_vmutex);

        return OSA_SOK;
    }

    if (av_state == 0) {

//        fprintf(stderr, "Open stream.\n");
        pthread_mutex_lock(&glb_vmutex);

        pobj->m_vcount++;

        pthread_mutex_unlock(&glb_vmutex);

        return OSA_SOK;
    }

    __rtspsvr_get_video_frame(pobj);

    if (pobj->m_v_es_size > size) {
        fprintf(stderr, "Lost Frame data size = %d.\n", pobj->m_v_es_size - size);
        pobj->m_v_es_size = size;
    }

    memcpy(buf, pobj->m_v_pes, pobj->m_v_es_size);

    (*psz) = pobj->m_v_es_size;

    //fprintf(stderr, "__h264video_source_fxn: Leave, size=%d.\n", pobj->m_v_es_size);

    return (OSA_SOK);
}

status_t __adts_audio_source_fxn(void *buf, size_t size, size_t *psz, int av_state, void *ud)
{
    rtspsvr_object_t *pobj = (rtspsvr_object_t *)ud;
#if 1
    //fprintf(stderr, "__h264video_source_fxn: Enter.\n");
    if (av_state == 1) {

        fprintf(stderr, "Close stream.\n");
        pthread_mutex_lock(&glb_amutex);

        pobj->m_acount--;

        if (pobj->m_acount == 0) {
            pobj->m_a_es_size = 0;
            __rtspsvr_stream_state_reset(&pobj->m_strm_state);
            rewind(pobj->m_ain);
        }

        pthread_mutex_unlock(&glb_amutex);

        return OSA_SOK;
    }

    if (av_state == 0) {

        fprintf(stderr, "Open stream.\n");
        pthread_mutex_lock(&glb_amutex);

        pobj->m_acount++;

        pthread_mutex_unlock(&glb_amutex);

        return OSA_SOK;
    }
#endif
    __rtspsvr_get_audio_frame(pobj);

    if (pobj->m_a_es_size > size) {
        fprintf(stderr, "Lost Frame data size = %d.\n", pobj->m_a_es_size - size);
        pobj->m_a_es_size = size;
    }

    memcpy(buf, pobj->m_a_pes, pobj->m_a_es_size);

    (*psz) = pobj->m_a_es_size;

    fprintf(stderr, "__adts_audio_source_fxn: Leave, size=%d.\n", pobj->m_a_es_size);

    return (OSA_SOK);
}

/*
 *  --------------------- Public function definition ---------------------------
 */
int main(int argc, char *argv[])
{
    status_t status = 0;
    rtsp_t rtsp = 0;
    sms_t  sms;
    rtsp_prm_t rtsp_prm;
    sms_prm_t sms_prm;

    subms_prm_t video_prm;
    subms_prm_t audio_prm;

    rtspsvr_object_t *pobj = &glb_rtsp_obj;

    __rtspsvr_init(&glb_rtsp_obj);

    rtsp_prm.m_port = atoi(argv[1]);
    status = rtsp_create(&rtsp, &rtsp_prm);
    if (OSA_ISERROR(status)) {
        fprintf(stderr, "Failed to create RTSP Server.\n");
        exit(-1);
    }

    snprintf(sms_prm.m_name, sizeof(sms_prm.m_name) - 1, "%s", "avstream");
    status = rtsp_create_sms(rtsp, &sms_prm, &sms);
    if (OSA_ISERROR(status)) {
        rtsp_delete(&rtsp);
        exit(-1);
    }

#if	defined(STREAM_VIDEO)
    video_prm.m_av_type  = SUBMS_TYPE_VIDEO;
    video_prm.m_cbs_fxn  = __h264video_source_fxn;
    video_prm.m_userdata = (void *)pobj;
    status = rtsp_sms_add_subms(sms, &video_prm);
    if (status < 0) {
        fprintf(stderr, "Failed to add video stream.\n");
        exit(-1);
    }
#endif	// if defined STREAM_VIDEO


#if 	defined(STREAM_AUDIO)
    audio_prm.m_av_type  = SUBMS_TYPE_AUDIO;
    audio_prm.m_cbs_fxn  = __adts_audio_source_fxn;
    audio_prm.m_userdata = (void *)pobj;
    status = rtsp_sms_add_subms(sms, &audio_prm);
    if (status < 0) {
        fprintf(stderr, "Failed to add audio stream.\n");
        exit(-1);
    }
#endif	// if defined STREAM_AUDIO

    signal(SIGINT , rtsp_daemon_signal_handler);
    signal(SIGALRM, rtsp_daemon_alarm_handler);

    //alarm(10);

    glb_daemon_exit = 0;

    while (!glb_daemon_exit) {
        status = rtsp_process(rtsp);

        if (glb_strm_restart) {
            status = rtsp_delete_sms(rtsp, &sms);
            if (OSA_ISERROR(status)) {
                fprintf(stderr, "Failed to delete server media session, status = %d.\n", status);
                rtsp_delete(&rtsp);
                exit(-1);
            }

            status = rtsp_delete(&rtsp);
            if (OSA_ISERROR(status)) {
                fprintf(stderr, "Failed to delete rtsp server, status = %d.\n", status);
                exit(-1);
            }

            sleep(1);

            status = rtsp_create(&rtsp, &rtsp_prm);
            if (OSA_ISERROR(status)) {
                fprintf(stderr, "Failed to create RTSP Server.\n");
                sleep(10);
                exit(-1);
            }

            status = rtsp_create_sms(rtsp, &sms_prm, &sms);
            if (OSA_ISERROR(status)) {
                fprintf(stderr, "Failed to create server media session, status=%d.\n", status);
                rtsp_delete(&rtsp);
                exit(-1);
            }

#if	defined(STREAM_VIDEO)
            video_prm.m_av_type  = SUBMS_TYPE_VIDEO;
            video_prm.m_cbs_fxn  = __h264video_source_fxn;
            video_prm.m_userdata = (void *)pobj;
            status = rtsp_sms_add_subms(sms, &video_prm);
            if (OSA_ISERROR(status)) {
                fprintf(stderr, "Failed to add video stream.\n");
                exit(-1);
            }
#endif	// if defined STREAM_VIDEO

            glb_strm_restart = 0;
            //alarm(10);
        }
    }

    status |= rtsp_delete_sms(rtsp, &sms);

    status |= rtsp_delete(&rtsp);

    __rtspsvr_deinit(&glb_rtsp_obj);

    return 0;
}

/*
 *  --------------------- Local function definition ----------------------------
 */
static void rtsp_daemon_signal_handler(int signum)
{
    if (signum == SIGINT) {
        fprintf(stderr, "Signal SIGINT caught, rtsp daemon exit....\n");
        //fprintf(stderr, "Signal SIGALRM caught, rtsp stream restart....\n");
        glb_daemon_exit = 1;
        //glb_strm_restart = 1;
    } else {
        fprintf(stderr, "Invalid signal %d caught.\n", signum);
    }
}

static void rtsp_daemon_alarm_handler(int signum)
{
    if (signum == SIGALRM) {
        fprintf(stderr, "Signal SIGALRM caught, rtsp stream restart....\n");
        glb_strm_restart = 1;
    } else {
        fprintf(stderr, "Invalid signal %d caught.\n", signum);
    }
}

#if defined(__cplusplus)
}
#endif
