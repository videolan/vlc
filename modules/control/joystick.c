/*****************************************************************************
 * joystick.c: control vlc with a joystick
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>

#include <errno.h>

#include <fcntl.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>

#include <linux/joystick.h>

#include "audio_output.h"

/* Default values for parameters */
#define DEFAULT_MAX_SEEK        10 /* seconds */
#define DEFAULT_REPEAT          100
#define DEFAULT_WAIT            500
#define DEFAULT_DEVICE          "/dev/input/js0"
#define DEFAULT_THRESHOLD       12000  /* 0 -> 32767 */

#define DEFAULT_MAPPING \
    "{axis-0-up=forward,axis-0-down=back," \
    "axis-1-up=next,axis-1-down=prev," \
    "butt-1-down=play,butt-2-down=fullscreen}"

/* Default Actions (used if there are missing actions in the default
 * Available actions are: Next,Prev, Forward,Back,Play,Fullscreen,dummy */
#define AXIS_0_UP_ACTION        Forward
#define AXIS_0_DOWN_ACTION      Back
#define AXIS_1_UP_ACTION        Next
#define AXIS_1_DOWN_ACTION      Prev

#define BUTTON_1_PRESS_ACTION   Play
#define BUTTON_1_RELEASE_ACTION dummy
#define BUTTON_2_PRESS_ACTION   Fullscreen
#define BUTTON_2_RELEASE_ACTION dummy

/*****************************************************************************
 * intf_sys_t: description and status of interface
 *****************************************************************************/

typedef int (*action)(intf_thread_t *p_intf);

struct joy_axis_t
{
    int         b_trigered;     /* Are we in the trigger zone ? */
    int         i_value;        /* Value of movement */
    int         b_dowork;       /* Do we have to do the action ? */
    action      pf_actup;       /* Action when axis is up */
    action      pf_actdown;     /* Action when axis is down */
    mtime_t     l_time;         /* When did the axis enter the trigger
                                 * zone ? */
};

struct joy_button_t
{
    action      pf_actup;  /* What to do when button is released */
    action      pf_actdown;/* What to do when button is pressed */
};

struct intf_sys_t
{
    int                 i_fd;           /* File descriptor for joystick */
    struct timeval      timeout;        /* Select timeout */
    int                 i_threshold;    /* motion threshold */
    int                 i_wait;         /* How much to wait before repeat */
    int                 i_repeat;       /* Repeat time */
    int                 i_maxseek;      /* Maximum seek time */
    struct joy_axis_t   axes[3];        /* Axes descriptor */
    struct joy_button_t buttons[2];     /* Buttons descriptor */
    input_thread_t      *p_input;       /* Input thread (for seeking) */
    float               f_seconds;      /* How much to seek */
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/

static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );
static int  Init   ( intf_thread_t *p_intf );

static int handle_event   ( intf_thread_t *p_intf, struct js_event event );


/* Actions */
static int Next        (intf_thread_t *p_intf);
static int Prev        (intf_thread_t *p_intf);
static int Back        (intf_thread_t *p_intf);
static int Forward     (intf_thread_t *p_intf);
static int Play        (intf_thread_t *p_intf);
static int Fullscreen  (intf_thread_t *p_intf);

static int dummy       (intf_thread_t *p_intf);

/* Exported functions */
static void Run       ( intf_thread_t *p_intf );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define THRESHOLD_TEXT N_( "Motion threshold" )
#define THRESHOLD_LONGTEXT N_( \
    "Amount of joystick movement required for a movement to be " \
    "recorded (0->32767)." )

#define DEVICE_TEXT N_( "Joystick device" )
#define DEVICE_LONGTEXT N_( \
    "The joystick device (usually /dev/js0 or /dev/input/js0).")

#define REPEAT_TEXT N_( "Repeat time (ms)" )
#define REPEAT_LONGTEXT N_( \
    "Delay waited before the action is repeated if it is still " \
    "triggered, in milliseconds." )

#define WAIT_TEXT N_( "Wait time (ms)")
#define WAIT_LONGTEXT N_(\
   "The time waited before the repeat starts, in milliseconds.")

#define SEEK_TEXT N_( "Max seek interval (seconds)")
#define SEEK_LONGTEXT N_(\
   "The maximum number of seconds that will be sought at a time." )

#define MAP_TEXT N_( "Action mapping")
#define MAP_LONGTEXT N_( "Allows you to remap the actions." )

vlc_module_begin();
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_CONTROL );
    add_integer( "motion-threshold", DEFAULT_THRESHOLD, NULL,
                     THRESHOLD_TEXT, THRESHOLD_LONGTEXT, VLC_TRUE );
    add_string( "joystick-device", DEFAULT_DEVICE, NULL,
                     DEVICE_TEXT, DEVICE_LONGTEXT, VLC_TRUE );
    add_integer ("joystick-repeat", DEFAULT_REPEAT,NULL,
                     REPEAT_TEXT, REPEAT_LONGTEXT, VLC_TRUE );
    add_integer ("joystick-wait", DEFAULT_WAIT,NULL,
                     WAIT_TEXT, WAIT_LONGTEXT, VLC_TRUE );
    add_integer ("joystick-max-seek",DEFAULT_MAX_SEEK,NULL,
                     SEEK_TEXT, SEEK_LONGTEXT, VLC_TRUE );
    add_string("joystick-mapping",DEFAULT_MAPPING,NULL,
                    MAP_TEXT,MAP_LONGTEXT, VLC_TRUE );
    set_description( _("Joystick control interface") );
    set_capability( "interface", 0 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: initialize interface
 *****************************************************************************/
static int Open ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );

    if( p_intf->p_sys == NULL )
    {
        return VLC_ENOMEM;
    }

    if( Init( p_intf ) < 0 )
    {
        msg_Err( p_intf, "cannot initialize interface" );
        free( p_intf->p_sys );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_intf, "interface initialized" );

    p_intf->pf_run = Run;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy the interface
 *****************************************************************************/
static void Close ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Destroy structure */
    if( p_intf->p_sys )
    {
        free( p_intf->p_sys );
    }
}


/*****************************************************************************
 * Run: main loop
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    int i_sel_res = 0;
    int i_read    = 0;
    int i_axis     = 0;
    struct js_event event;

    /* Main loop */
    while( !p_intf->b_die )
    {
        fd_set fds;
 
        vlc_mutex_lock( &p_intf->change_lock );

        FD_ZERO( &fds );
        FD_SET( p_intf->p_sys->i_fd, &fds );

        p_intf->p_sys->timeout.tv_sec  = 0;
        p_intf->p_sys->timeout.tv_usec = p_intf->p_sys->i_repeat;


        i_sel_res = select( p_intf->p_sys->i_fd + 1, &fds,
                            NULL, NULL, &p_intf->p_sys->timeout );

        p_intf->p_sys->p_input = (input_thread_t *)
           vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_ANYWHERE );

        if( i_sel_res == -1 && errno != EINTR )
        {
            msg_Err( p_intf, "select error: %s",strerror(errno) );
        }
        else if(i_sel_res > 0 && FD_ISSET( p_intf->p_sys->i_fd, &fds))
        {
            /* We got an event */
            memset(&event,0,sizeof(struct js_event));
            i_read = read( p_intf->p_sys->i_fd, &event,
                                    sizeof(struct js_event));
            handle_event( p_intf, event ) ;
        }
        else if(i_sel_res == 0)
        {
            /*We have no event, but check if we have an action to repeat */
            for(i_axis = 0; i_axis <= 1; i_axis++)
            {
                if( p_intf->p_sys->axes[i_axis].b_trigered &&
                    mdate()-p_intf->p_sys->axes[i_axis].l_time >
                        p_intf->p_sys->i_wait &&
                    p_intf->p_sys->axes[i_axis].i_value > 0 )
                {
                    p_intf->p_sys->axes[i_axis].pf_actup(p_intf);
                }

                if( p_intf->p_sys->axes[i_axis].b_trigered &&
                    mdate()-p_intf->p_sys->axes[i_axis].l_time >
                          p_intf->p_sys->i_wait &&
                    p_intf->p_sys->axes[i_axis].i_value < 0 )
                {
                    p_intf->p_sys->axes[i_axis].pf_actdown(p_intf);
                }
            }
        }

        if(p_intf->p_sys->p_input)
                vlc_object_release (p_intf->p_sys->p_input);

        vlc_mutex_unlock ( &p_intf->change_lock );
    }
}

/*****************************************************************************
 * InitThread: Initialize the interface
 *****************************************************************************/
static int Init( intf_thread_t * p_intf )
{
    char *psz_device;
    char *psz_parse;
    char *psz_eof;  /* end of field */

    psz_device = config_GetPsz( p_intf, "joystick-device");

    if( !psz_device ) /* strange... */
    {
        psz_device = strdup( DEFAULT_DEVICE );
    }

    p_intf->p_sys->i_fd = open( psz_device, O_RDONLY|O_NONBLOCK );

    if( p_intf->p_sys->i_fd == -1 )
    {
        msg_Warn( p_intf, "unable to open %s for reading: %s",
                          psz_device, strerror(errno) );
        return VLC_EGENERIC;
    }

    p_intf->p_sys->i_repeat = 1000 * config_GetInt( p_intf, "joystick-repeat");
    p_intf->p_sys->i_wait = 1000 * config_GetInt( p_intf, "joystick-wait");

    p_intf->p_sys->i_threshold = config_GetInt( p_intf, "motion-threshold" );
    if(p_intf->p_sys->i_threshold > 32767 || p_intf->p_sys->i_threshold < 0 )
        p_intf->p_sys->i_threshold = DEFAULT_THRESHOLD;

    p_intf->p_sys->i_maxseek = config_GetInt( p_intf, "joystick-max-seek" );

    psz_parse = config_GetPsz( p_intf, "joystick-mapping" ) ;

    if( ! psz_parse)
    {
        msg_Warn (p_intf,"invalid mapping. aborting" );
        return VLC_EGENERIC;
    }

    if( !strlen( psz_parse ) )
    {
        msg_Warn( p_intf, "invalid mapping, aborting" );
        return VLC_EGENERIC;
    }

    p_intf->p_sys->axes[0].pf_actup  = AXIS_0_UP_ACTION;
    p_intf->p_sys->axes[0].pf_actdown  = AXIS_0_DOWN_ACTION;
    p_intf->p_sys->axes[1].pf_actup  = AXIS_1_UP_ACTION;
    p_intf->p_sys->axes[1].pf_actdown  = AXIS_1_DOWN_ACTION;

    p_intf->p_sys->buttons[0].pf_actdown = BUTTON_1_PRESS_ACTION;
    p_intf->p_sys->buttons[0].pf_actup   = BUTTON_1_RELEASE_ACTION;
    p_intf->p_sys->buttons[1].pf_actdown = BUTTON_2_PRESS_ACTION;
    p_intf->p_sys->buttons[1].pf_actup   = BUTTON_2_RELEASE_ACTION;

/* Macro to parse the command line */
#define PARSE(name,function)                                                  \
    if(!strncmp( psz_parse, name, strlen( name ) ) )                          \
    {                                                                         \
        psz_parse += strlen( name );                                          \
        psz_eof = strchr( psz_parse, ',' );                                   \
        if( !psz_eof)                                                         \
            psz_eof = strchr( psz_parse, '}' );                               \
        if( !psz_eof)                                                         \
            psz_eof = psz_parse + strlen(psz_parse);                          \
        if( psz_eof )                                                         \
        {                                                                     \
            *psz_eof = '\0' ;                                                 \
        }                                                                     \
        msg_Dbg(p_intf,"%s -> %s", name,psz_parse) ;                          \
        if(!strcasecmp( psz_parse, "play" ) ) function = Play;                \
        if(!strcasecmp( psz_parse, "next" ) ) function = Next;                \
        if(!strcasecmp( psz_parse, "prev" ) ) function = Prev;                \
        if(!strcasecmp( psz_parse, "fullscreen" ) ) function = Fullscreen;    \
        if(!strcasecmp( psz_parse, "forward" ) ) function = Forward;          \
        if(!strcasecmp( psz_parse, "back" ) ) function = Back;                \
        psz_parse = psz_eof;                                                  \
        psz_parse ++;                                                         \
        continue;                                                             \
    }                                                                         \

    for( ; *psz_parse ; psz_parse++ )
    {
        PARSE("axis-0-up=",   p_intf->p_sys->axes[0].pf_actup );
        PARSE("axis-0-down=", p_intf->p_sys->axes[0].pf_actdown );
        PARSE("axis-1-up=",   p_intf->p_sys->axes[1].pf_actup );
        PARSE("axis-1-down=", p_intf->p_sys->axes[1].pf_actdown );

        PARSE("butt-1-up=",   p_intf->p_sys->buttons[0].pf_actup );
        PARSE("butt-1-down=", p_intf->p_sys->buttons[0].pf_actdown );
        PARSE("butt-2-up=",   p_intf->p_sys->buttons[1].pf_actup );
        PARSE("butt-2-down=", p_intf->p_sys->buttons[1].pf_actdown );
    }

    p_intf->p_sys->axes[0].b_trigered = VLC_FALSE;
    p_intf->p_sys->axes[0].l_time = 0;

    p_intf->p_sys->axes[1].b_trigered = VLC_FALSE;
    p_intf->p_sys->axes[1].l_time = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * handle_event : parse a joystick event and takes the appropriate action    *
 *****************************************************************************/
static int handle_event ( intf_thread_t *p_intf, struct js_event event)
{
    unsigned int i_axis;

    if( event.type == JS_EVENT_AXIS )
    {
        /* Third axis is supposed to behave in a different way: it is a
         * throttle, and will set a value, without triggering anything */
        if( event.number == 2 &&
            /* Try to avoid Parkinson joysticks */
            abs(event.value - p_intf->p_sys->axes[2].i_value) > 200 )
        {
            p_intf->p_sys->axes[2].i_value = event.value;
            msg_Dbg( p_intf, "updating volume" );
            /* This way, the volume is between 0 and 1024 */
            aout_VolumeSet( p_intf, (32767-event.value)/64 );
            return 0;
        }

        p_intf->p_sys->axes[event.number].b_dowork = VLC_FALSE;
        p_intf->p_sys->axes[event.number].i_value  = event.value;

        if( abs(event.value) > p_intf->p_sys->i_threshold &&
             p_intf->p_sys->axes[event.number].b_trigered == VLC_FALSE )
        {
            /* The axis entered the trigger zone. Start the event */
            p_intf->p_sys->axes[event.number].b_trigered = VLC_TRUE;
            p_intf->p_sys->axes[event.number].b_dowork   = VLC_TRUE;
            p_intf->p_sys->axes[event.number].l_time     = mdate();
        }
        else if( abs(event.value) > p_intf->p_sys->i_threshold &&
                 p_intf->p_sys->axes[event.number].b_trigered == VLC_TRUE )
        {
            /* The axis moved but remained in the trigger zone
             * Do nothing at this time */
        }
        else if( abs(event.value) < p_intf->p_sys->i_threshold )
        {
            /* The axis is not in the trigger zone */
            p_intf->p_sys->axes[event.number].b_trigered = VLC_FALSE;
        }

        /* Special for seeking */
        p_intf->p_sys->f_seconds = 1 +
            (abs(event.value) - p_intf->p_sys->i_threshold) *
            (p_intf->p_sys->i_maxseek - 1 ) /
            (32767 - p_intf->p_sys->i_threshold);


        /* Handle the first two axes. */
        for(i_axis = 0; i_axis <= 1; i_axis ++)
        {
            if(p_intf->p_sys->axes[i_axis].b_dowork == VLC_TRUE)
            {
                if( p_intf->p_sys->axes[i_axis].i_value
                              > p_intf->p_sys->i_threshold )
                {
                    msg_Dbg(p_intf,"up for axis %i\n",i_axis);
                    p_intf->p_sys->axes[i_axis].pf_actup(p_intf);
                }
                else if( p_intf->p_sys->axes[i_axis].i_value
                                < -p_intf->p_sys->i_threshold )
                {
                    msg_Dbg(p_intf,"down for axis %i\n",i_axis);
                    p_intf->p_sys->axes[i_axis].pf_actdown(p_intf);
                }

            }
        }
    }
    else if( event.type == JS_EVENT_BUTTON )
    {
        msg_Dbg( p_intf, "button %i %s", event.number,
                         event.value ? "pressed" : "released" );
        if( event.number > 1 )
            return 0; /* Only trigger 2 buttons */

        if( event.value == 1 ) /* Button pressed */
        {
            if( p_intf->p_sys->buttons[event.number].pf_actdown )
                p_intf->p_sys->buttons[event.number].pf_actdown( p_intf );
        }
        else /* Button released */
        {
            if( p_intf->p_sys->buttons[event.number].pf_actup )
                p_intf->p_sys->buttons[event.number].pf_actup( p_intf );
        }
    }
    return 0;
}

/****************************************************************************
 * The actions
 ****************************************************************************/

/* Go to next item in the playlist */
static int Next( intf_thread_t *p_intf )
{
    playlist_t *p_playlist;

    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return VLC_ENOOBJ;
    }

    playlist_Next( p_playlist );
    vlc_object_release( p_playlist );
    return VLC_SUCCESS;
}

/* Go to previous item in the playlist */
static int Prev( intf_thread_t *p_intf )
{
    playlist_t *p_playlist;

    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return VLC_ENOOBJ;
    }

    playlist_Prev( p_playlist );
    vlc_object_release( p_playlist );
    return VLC_SUCCESS;
}

/* Seek forward */
static int Forward( intf_thread_t *p_intf )
{
    if( p_intf->p_sys->p_input )
    {
        msg_Dbg( p_intf,"seeking %f seconds",p_intf->p_sys->f_seconds );
        var_SetTime( p_intf->p_sys->p_input, "time-offset",
                     (int64_t)p_intf->p_sys->f_seconds * I64C(1000000) );
        return VLC_SUCCESS;
    }

    return VLC_ENOOBJ;
}

/* Seek backwards */
static int Back( intf_thread_t *p_intf )
{
    if( p_intf->p_sys->p_input )
    {
        msg_Dbg( p_intf,"seeking -%f seconds", p_intf->p_sys->f_seconds );

        var_SetTime( p_intf->p_sys->p_input, "time-offset",
                     -(int64_t)p_intf->p_sys->f_seconds * I64C(1000000) );
        return VLC_SUCCESS;
    }

    return VLC_ENOOBJ;
}

/* Toggle Play/Pause */
static int Play( intf_thread_t *p_intf )
{
    if( p_intf->p_sys->p_input )
    {
        var_SetInteger( p_intf->p_sys->p_input, "state", PAUSE_S );
        return VLC_SUCCESS;
    }

    return VLC_ENOOBJ;
}

/* Toggle fullscreen mode */
static int Fullscreen( intf_thread_t *p_intf )
{
    vout_thread_t * p_vout;

    p_vout = vlc_object_find(p_intf, VLC_OBJECT_VOUT, FIND_ANYWHERE );
    if( p_vout )
    {
        p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
        vlc_object_release(p_vout);
    }
    return VLC_SUCCESS;
}

/* dummy event. Use it if you don't wan't anything to happen */
static int dummy( intf_thread_t *p_intf )
{
    return VLC_SUCCESS;
}

