/*****************************************************************************
 * en50221.h:
 *****************************************************************************
 * Copyright (C) 1998-2010 the VideoLAN team
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

typedef struct cam cam_t;

typedef struct en50221_session_t
{
    unsigned i_slot;
    int i_resource_id;
    void (* pf_handle)( cam_t *, int, uint8_t *, int );
    void (* pf_close)( cam_t *, int );
    void (* pf_manage)( cam_t *, int );
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

static inline void en50221_MMIFree( en50221_mmi_object_t *p_object )
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
            free( p_object->u.menu.ppsz_choices[i] );
        }
        FREENULL( p_object->u.menu.ppsz_choices );
        break;

    default:
        break;
    }
}

/* CA management */
#define MAX_CI_SLOTS 16
#define MAX_SESSIONS 32
#define MAX_PROGRAMS 24

struct cam
{
    vlc_object_t *obj;
    int i_ca_type;
    int fd;
    mtime_t i_timeout, i_next_event;

    unsigned i_nb_slots;
    bool pb_active_slot[MAX_CI_SLOTS];
    bool pb_tc_has_data[MAX_CI_SLOTS];
    bool pb_slot_mmi_expected[MAX_CI_SLOTS];
    bool pb_slot_mmi_undisplayed[MAX_CI_SLOTS];
    en50221_session_t p_sessions[MAX_SESSIONS];

    dvbpsi_pmt_t *pp_selected_programs[MAX_PROGRAMS];
    int i_selected_programs;
};

int en50221_Init( cam_t * );
int en50221_Poll( cam_t * );
int en50221_SetCAPMT( cam_t *, dvbpsi_pmt_t * );
int en50221_OpenMMI( cam_t *, unsigned i_slot );
int en50221_CloseMMI( cam_t *, unsigned i_slot );
en50221_mmi_object_t *en50221_GetMMIObject( cam_t *, unsigned i_slot );
void en50221_SendMMIObject( cam_t *, unsigned i_slot, en50221_mmi_object_t * );
void en50221_End( cam_t * );

char *dvbsi_to_utf8( const char *psz_instring, size_t i_length );
