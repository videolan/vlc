/*****************************************************************************
 * dvb.h : functions to control a DVB card under Linux with v4l2
 *****************************************************************************
 * Copyright (C) 1998-2005 the VideoLAN team
 *
 * Authors: Johan Bilien <jobi@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman _at_ videolan _dot_ org>
 *          Christopher Ross <chris@tebibyte.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA    02111, USA.
 *****************************************************************************/


/*****************************************************************************
 * Devices location
 *****************************************************************************/
#define DMX      "/dev/dvb/adapter%d/demux%d"
#define FRONTEND "/dev/dvb/adapter%d/frontend%d"
#define DVR      "/dev/dvb/adapter%d/dvr%d"
#define CA       "/dev/dvb/adapter%d/ca%d"

/*****************************************************************************
 * Local structures
 *****************************************************************************/
typedef struct demux_handle_t
{
    int i_pid;
    int i_handle;
    int i_type;
} demux_handle_t;

typedef struct frontend_t frontend_t;

typedef struct en50221_session_t
{
    int i_slot;
    int i_resource_id;
    void (* pf_handle)( access_t *, int, uint8_t *, int );
    void (* pf_close)( access_t *, int );
    void (* pf_manage)( access_t *, int );
    void *p_sys;
} en50221_session_t;

#define EN50221_MMI_NONE 0
#define EN50221_MMI_ENQ 1
#define EN50221_MMI_ANSW 2
#define EN50221_MMI_MENU 3
#define EN50221_MMI_MENU_ANSW 4
#define EN50221_MMI_LIST 5

typedef struct en50221_mmi_object_t
{
    int i_object_type;

    union
    {
        struct
        {
            bool b_blind;
            char *psz_text;
        } enq;

        struct
        {
            bool b_ok;
            char *psz_answ;
        } answ;

        struct
        {
            char *psz_title, *psz_subtitle, *psz_bottom;
            char **ppsz_choices;
            int i_choices;
        } menu; /* menu and list are the same */

        struct
        {
            int i_choice;
        } menu_answ;
    } u;
} en50221_mmi_object_t;

static __inline__ void en50221_MMIFree( en50221_mmi_object_t *p_object )
{
    int i;

    switch ( p_object->i_object_type )
    {
    case EN50221_MMI_ENQ:
        FREENULL( p_object->u.enq.psz_text );
        break;

    case EN50221_MMI_ANSW:
        if ( p_object->u.answ.b_ok )
        {
            FREENULL( p_object->u.answ.psz_answ );
        }
        break;

    case EN50221_MMI_MENU:
    case EN50221_MMI_LIST:
        FREENULL( p_object->u.menu.psz_title );
        FREENULL( p_object->u.menu.psz_subtitle );
        FREENULL( p_object->u.menu.psz_bottom );
        for ( i = 0; i < p_object->u.menu.i_choices; i++ )
        {
            FREENULL( p_object->u.menu.ppsz_choices[i] );
        }
        FREENULL( p_object->u.menu.ppsz_choices );
        break;

    default:
        break;
    }
}

#define MAX_DEMUX 256
#define MAX_CI_SLOTS 16
#define MAX_SESSIONS 32
#define MAX_PROGRAMS 24

struct access_sys_t
{
    int i_handle, i_frontend_handle;
    demux_handle_t p_demux_handles[MAX_DEMUX];
    frontend_t *p_frontend;
    bool b_budget_mode;

    /* CA management */
    int i_ca_handle;
    int i_ca_type;
    int i_nb_slots;
    bool pb_active_slot[MAX_CI_SLOTS];
    bool pb_tc_has_data[MAX_CI_SLOTS];
    bool pb_slot_mmi_expected[MAX_CI_SLOTS];
    bool pb_slot_mmi_undisplayed[MAX_CI_SLOTS];
    en50221_session_t p_sessions[MAX_SESSIONS];
    mtime_t i_ca_timeout, i_ca_next_event, i_frontend_timeout;
    dvbpsi_pmt_t *pp_selected_programs[MAX_PROGRAMS];
    int i_selected_programs;

    /* */
    int i_read_once;

#ifdef ENABLE_HTTPD
    /* Local HTTP server */
    httpd_host_t        *p_httpd_host;
    httpd_file_sys_t    *p_httpd_file;
    httpd_redirect_t    *p_httpd_redir;

    vlc_mutex_t         httpd_mutex;
    vlc_cond_t          httpd_cond;
    mtime_t             i_httpd_timeout;
    bool          b_request_frontend_info, b_request_mmi_info;
    char                *psz_frontend_info, *psz_mmi_info;
    char                *psz_request;
#endif
};

#define VIDEO0_TYPE     1
#define AUDIO0_TYPE     2
#define TELETEXT0_TYPE  3
#define SUBTITLE0_TYPE  4
#define PCR0_TYPE       5
#define TYPE_INTERVAL   5
#define OTHER_TYPE     21

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int  FrontendOpen( access_t * );
void FrontendPoll( access_t *p_access );
int  FrontendSet( access_t * );
void FrontendClose( access_t * );
#ifdef ENABLE_HTTPD
void FrontendStatus( access_t * );
#endif

int DMXSetFilter( access_t *, int i_pid, int * pi_fd, int i_type );
int DMXUnsetFilter( access_t *, int i_fd );

int  DVROpen( access_t * );
void DVRClose( access_t * );

int  CAMOpen( access_t * );
int  CAMPoll( access_t * );
int  CAMSet( access_t *, dvbpsi_pmt_t * );
void CAMClose( access_t * );
#ifdef ENABLE_HTTPD
void CAMStatus( access_t * );
#endif

int en50221_Init( access_t * );
int en50221_Poll( access_t * );
int en50221_SetCAPMT( access_t *, dvbpsi_pmt_t * );
int en50221_OpenMMI( access_t * p_access, int i_slot );
int en50221_CloseMMI( access_t * p_access, int i_slot );
en50221_mmi_object_t *en50221_GetMMIObject( access_t * p_access,
                                                int i_slot );
void en50221_SendMMIObject( access_t * p_access, int i_slot,
                                en50221_mmi_object_t *p_object );
void en50221_End( access_t * );

#ifdef ENABLE_HTTPD
int HTTPOpen( access_t *p_access );
void HTTPClose( access_t *p_access );
char *HTTPExtractValue( char *psz_uri, const char *psz_name,
                            char *psz_value, int i_value_max );
#endif
/*****************************************************************************
 * Hacks
 *****************************************************************************/
#define STRINGIFY( z )   UGLY_KLUDGE( z )
#define UGLY_KLUDGE( z ) #z
