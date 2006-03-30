/*****************************************************************************
 * logo.c : logo video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2003-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Simon Latapie <garf@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/vout.h>

#include "vlc_filter.h"
#include "filter_common.h"
#include "vlc_image.h"
#include "vlc_osd.h"

#ifdef LoadImage
#   undef LoadImage
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );

static int  SendEvents( vlc_object_t *, char const *,
                        vlc_value_t, vlc_value_t, void * );
static int  MouseEvent( vlc_object_t *, char const *,
                        vlc_value_t , vlc_value_t , void * );
static int  Control   ( vout_thread_t *, int, va_list );

static int  CreateFilter ( vlc_object_t * );
static void DestroyFilter( vlc_object_t * );

static int LogoCallback( vlc_object_t *, char const *,
                         vlc_value_t, vlc_value_t, void * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FILE_TEXT N_("Logo filenames")
#define FILE_LONGTEXT N_("Full path of the image files to use. Format is " \
"<image>[,<delay in ms>[,<alpha>]][;<image>[,<delay>[,<alpha>]]][;...]. " \
"If you only have one file, simply enter its filename.")
#define REPEAT_TEXT N_("Logo animation # of loops")
#define REPEAT_LONGTEXT N_("Number of loops for the logo animation." \
        "1 = continuous, 0 = disabled")
#define DELAY_TEXT N_("Logo individual image time in ms")
#define DELAY_LONGTEXT N_("Individual image display time of 0 - 60000 ms.")

#define POSX_TEXT N_("X coordinate")
#define POSX_LONGTEXT N_("X coordinate of the logo. You can move the logo " \
                "by left-clicking it." )
#define POSY_TEXT N_("Y coordinate")
#define POSY_LONGTEXT N_("Y coordinate of the logo. You can move the logo " \
                "by left-clicking it." )
#define TRANS_TEXT N_("Transparency of the logo")
#define TRANS_LONGTEXT N_("Logo transparency value " \
  "(from 0 for full transparency to 255 for full opacity)." )
#define POS_TEXT N_("Logo position")
#define POS_LONGTEXT N_( \
  "Enforce the logo position on the video " \
  "(0=center, 1=left, 2=right, 4=top, 8=bottom, you can " \
  "also use combinations of these values, eg 6 = top-right).")

static int pi_pos_values[] = { 0, 1, 2, 4, 8, 5, 6, 9, 10 };
static char *ppsz_pos_descriptions[] =
{ N_("Center"), N_("Left"), N_("Right"), N_("Top"), N_("Bottom"),
  N_("Top-Left"), N_("Top-Right"), N_("Bottom-Left"), N_("Bottom-Right") };

vlc_module_begin();
    set_description( _("Logo video filter") );
    set_capability( "video filter", 0 );
    set_shortname( N_("Logo overlay") );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_SUBPIC );
    add_shortcut( "logo" );
    set_callbacks( Create, Destroy );

    add_file( "logo-file", NULL, NULL, FILE_TEXT, FILE_LONGTEXT, VLC_FALSE );
    add_integer( "logo-x", -1, NULL, POSX_TEXT, POSX_LONGTEXT, VLC_TRUE );
    add_integer( "logo-y", 0, NULL, POSY_TEXT, POSY_LONGTEXT, VLC_TRUE );
    /* default to 1000 ms per image, continuously cycle through them */
    add_integer( "logo-delay", 1000, NULL, DELAY_TEXT, DELAY_LONGTEXT, VLC_TRUE );
    add_integer( "logo-repeat", -1, NULL, REPEAT_TEXT, REPEAT_LONGTEXT, VLC_TRUE );
    add_integer_with_range( "logo-transparency", 255, 0, 255, NULL,
        TRANS_TEXT, TRANS_LONGTEXT, VLC_FALSE );
    add_integer( "logo-position", 6, NULL, POS_TEXT, POS_LONGTEXT, VLC_FALSE );
        change_integer_list( pi_pos_values, ppsz_pos_descriptions, 0 );

    /* subpicture filter submodule */
    add_submodule();
    set_capability( "sub filter", 0 );
    set_callbacks( CreateFilter, DestroyFilter );
    set_description( _("Logo sub filter") );
    add_shortcut( "logo" );
vlc_module_end();

/*****************************************************************************
 * Structure to hold the set of individual logo image names, times,
 * transparencies
 ****************************************************************************/
typedef struct
{
    char *psz_file;    /* candidate for deletion -- not needed */
    int i_delay;       /* -1 means use default delay */
    int i_alpha;       /* -1 means use default alpha */
    picture_t *p_pic;

} logo_t;

/*****************************************************************************
 * Logo list structure. Common to both the vout and sub picture filter
 ****************************************************************************/
typedef struct
{
    logo_t *p_logo;         /* the parsing's result */
    unsigned int i_count;   /* the number of logo images to be displayed */

    int i_repeat;         /* how often to repeat the images, image time in ms */
    mtime_t i_next_pic;     /* when to bring up a new logo image */

    unsigned int i_counter; /* index into the list of logo images */

    int i_delay;            /* default delay (0 - 60000 ms) */
    int i_alpha;            /* default alpha */

    char *psz_filename;     /* --logo-file string ( is it really useful
                             * to store it ? ) */

    vlc_mutex_t lock;
} logo_list_t;

/*****************************************************************************
 * LoadImage: loads the logo image into memory
 *****************************************************************************/
static picture_t *LoadImage( vlc_object_t *p_this, char *psz_filename )
{
    picture_t *p_pic;
    image_handler_t *p_image;
    video_format_t fmt_in = {0}, fmt_out = {0};

    fmt_out.i_chroma = VLC_FOURCC('Y','U','V','A');
    p_image = image_HandlerCreate( p_this );
    p_pic = image_ReadUrl( p_image, psz_filename, &fmt_in, &fmt_out );
    image_HandlerDelete( p_image );

    return p_pic;
}

/*****************************************************************************
 * LoadLogoList: loads the logo images into memory
 *****************************************************************************
 * Read the logo-file input switch, obtaining a list of images and associated
 * durations and transparencies.  Store the image(s), and times.  An image
 * without a stated time or transparency will use the logo-delay and
 * logo-transparency values.
 *****************************************************************************/
#define LoadLogoList( a, b ) __LoadLogoList( VLC_OBJECT( a ), b )
void __LoadLogoList( vlc_object_t *p_this, logo_list_t *p_logo_list )
{
    char *psz_list; /* the list: <logo>[,[<delay>[,[<alpha>]]]][;...] */
    unsigned int i;
    logo_t *p_logo;         /* the parsing's result */

    p_logo_list->i_counter = 0;
    p_logo_list->i_next_pic = 0;

    psz_list = strdup( p_logo_list->psz_filename );

    /* Count the number logos == number of ';' + 1 */
    p_logo_list->i_count = 1;
    for( i = 0; i < strlen( psz_list ); i++ )
    {
        if( psz_list[i] == ';' ) p_logo_list->i_count++;
    }

    p_logo_list->p_logo = p_logo =
        (logo_t *)malloc( p_logo_list->i_count * sizeof(logo_t) );

    /* Fill the data */
    for( i = 0; i < p_logo_list->i_count; i++ )
    {
        char *p_c;
        char *p_c2;
        p_c = strchr( psz_list, ';' );
        p_c2 = strchr( psz_list, ',' );

        p_logo[i].i_alpha = -1; /* use default settings */
        p_logo[i].i_delay = -1; /* use default settings */

        if( p_c2 && ( p_c2 < p_c || !p_c ) )
        {
            /* <logo>,<delay>[,<alpha>] type */
            if( p_c2[1] != ',' && p_c2[1] != ';' && p_c2[1] != '\0' )
                p_logo[i].i_delay = atoi( p_c2+1 );
            *p_c2 = '\0';
            if( ( p_c2 = strchr( p_c2+1, ',' ) )
                && ( p_c2 < p_c || !p_c ) && p_c2[1] != ';' && p_c2[1] != '\0' )
                p_logo[i].i_alpha = atoi( p_c2 + 1 );
        }
        else
        {
            /* <logo> type */
            if( p_c ) *p_c = '\0';
        }

        p_logo[i].psz_file = strdup( psz_list );
        p_logo[i].p_pic = LoadImage( p_this, p_logo[i].psz_file );

        if( !p_logo[i].p_pic )
        {
            msg_Warn( p_this, "Error while loading logo %s. It will be skipped",
                      p_logo[i].psz_file );
        }

        if( p_c ) psz_list = p_c + 1;
    }

    for( i = 0; i < p_logo_list->i_count; i++ )
    {
       msg_Dbg( p_this, "logo file name %s, delay %d, alpha %d",
                p_logo[i].psz_file, p_logo[i].i_delay, p_logo[i].i_alpha );
    }

    /* initialize so that on the first update it will wrap back to 0 */
    p_logo_list->i_counter = p_logo_list->i_count;
}

/*****************************************************************************
 * FreeLogoList
 *****************************************************************************/
#define FREE( a ) free(a);a=NULL;
void FreeLogoList( logo_list_t *p_logo_list )
{
    unsigned int i;
    if( p_logo_list->psz_filename ) FREE( p_logo_list->psz_filename );
    for( i = 0; i < p_logo_list->i_count; i++ )
    {
        logo_t *p_logo = &p_logo_list->p_logo[i];
        if( p_logo[i].psz_file ) FREE( p_logo[i].psz_file );
        if( p_logo[i].p_pic )
        {
            p_logo[i].p_pic->pf_release( p_logo[i].p_pic );
            p_logo[i].p_pic = NULL;
        }
    }
}
#undef FREE

/*****************************************************************************
 * vout_sys_t: logo video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Invert specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    logo_list_t *p_logo_list;

    vout_thread_t *p_vout;

    filter_t *p_blend;

    int i_width, i_height;
    int pos, posx, posy;
};

/*****************************************************************************
 * Create: allocates logo video thread output method
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vout_sys_t *p_sys;
    vlc_value_t val;
    logo_list_t *p_logo_list;

    /* Allocate structure */
    p_sys = p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return VLC_ENOMEM;
    }
    p_logo_list = p_sys->p_logo_list = malloc( sizeof( logo_list_t ) );
    if( p_logo_list == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        free( p_sys );
        return VLC_ENOMEM;
    }

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = NULL;
    p_vout->pf_render = Render;
    p_vout->pf_display = NULL;
    p_vout->pf_control = Control;

    p_logo_list->psz_filename = var_CreateGetString( p_this , "logo-file" );
    if( !p_logo_list->psz_filename || !*p_logo_list->psz_filename )
    {
        msg_Err( p_this, "logo file not specified" );
        return 0;
    }

    var_Create( p_this, "logo-position", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_this, "logo-position", &val );
    p_sys->pos = val.i_int;

    var_Create( p_this, "logo-x", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_this, "logo-x", &val );
    p_sys->posx = val.i_int;

    var_Create( p_this, "logo-y", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_this, "logo-y", &val );
    p_sys->posy = val.i_int;

    var_Create( p_this, "logo-delay", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_this, "logo-delay", &val );
    p_logo_list->i_delay = __MAX( __MIN( val.i_int, 60000 ), 0 );

    var_Create( p_this, "logo-repeat", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_this, "logo-repeat", &val );
    p_logo_list->i_repeat = val.i_int;

    var_Create(p_this, "logo-transparency", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT);
    var_Get( p_this, "logo-transparency", &val );
    p_logo_list->i_alpha = __MAX( __MIN( val.i_int, 255 ), 0 );

    LoadLogoList( p_vout, p_logo_list );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Init: initialize logo video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    picture_t *p_pic;
    int i_index;
    video_format_t fmt = {0};

    logo_list_t *p_logo_list = p_sys->p_logo_list;

    I_OUTPUTPICTURES = 0;

    /* adjust index to the next logo */
    p_logo_list->i_counter =
                        ( p_logo_list->i_counter + 1 )%p_logo_list->i_count;

    p_pic = p_logo_list->p_logo[p_logo_list->i_counter].p_pic;
    /* Initialize the output structure */
    p_vout->output.i_chroma = p_vout->render.i_chroma;
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;
    p_vout->fmt_out = p_vout->fmt_in;
    fmt = p_vout->fmt_out;

    /* Load the video blending filter */
    p_sys->p_blend = vlc_object_create( p_vout, sizeof(filter_t) );
    vlc_object_attach( p_sys->p_blend, p_vout );
    p_sys->p_blend->fmt_out.video.i_x_offset =
        p_sys->p_blend->fmt_out.video.i_y_offset = 0;
    p_sys->p_blend->fmt_in.video.i_x_offset =
        p_sys->p_blend->fmt_in.video.i_y_offset = 0;
    p_sys->p_blend->fmt_out.video.i_aspect = p_vout->render.i_aspect;
    p_sys->p_blend->fmt_out.video.i_chroma = p_vout->output.i_chroma;
    p_sys->p_blend->fmt_in.video.i_chroma = VLC_FOURCC('Y','U','V','A');
    p_sys->p_blend->fmt_in.video.i_aspect = VOUT_ASPECT_FACTOR;
    p_sys->i_width =
        p_sys->p_blend->fmt_in.video.i_width =
            p_sys->p_blend->fmt_in.video.i_visible_width =
                p_pic ? p_pic->p[Y_PLANE].i_visible_pitch : 0;
    p_sys->i_height =
        p_sys->p_blend->fmt_in.video.i_height =
            p_sys->p_blend->fmt_in.video.i_visible_height =
                p_pic ? p_pic->p[Y_PLANE].i_visible_lines : 0;
    p_sys->p_blend->fmt_out.video.i_width =
        p_sys->p_blend->fmt_out.video.i_visible_width =
           p_vout->output.i_width;
    p_sys->p_blend->fmt_out.video.i_height =
        p_sys->p_blend->fmt_out.video.i_visible_height =
            p_vout->output.i_height;

    p_sys->p_blend->p_module =
        module_Need( p_sys->p_blend, "video blending", 0, 0 );
    if( !p_sys->p_blend->p_module )
    {
        msg_Err( p_vout, "can't open blending filter, aborting" );
        vlc_object_detach( p_sys->p_blend );
        vlc_object_destroy( p_sys->p_blend );
        return VLC_EGENERIC;
    }

    if( p_sys->posx < 0 || p_sys->posy < 0 )
    {
        p_sys->posx = 0; p_sys->posy = 0;

        if( p_sys->pos & SUBPICTURE_ALIGN_BOTTOM )
        {
            p_sys->posy = p_vout->render.i_height - p_sys->i_height;
        }
        else if ( !(p_sys->pos & SUBPICTURE_ALIGN_TOP) )
        {
            p_sys->posy = p_vout->render.i_height / 2 - p_sys->i_height / 2;
        }

        if( p_sys->pos & SUBPICTURE_ALIGN_RIGHT )
        {
            p_sys->posx = p_vout->render.i_width - p_sys->i_width;
        }
        else if ( !(p_sys->pos & SUBPICTURE_ALIGN_LEFT) )
        {
            p_sys->posx = p_vout->render.i_width / 2 - p_sys->i_width / 2;
        }
    }

    /* Try to open the real video output */
    msg_Dbg( p_vout, "spawning the real video output" );

    p_sys->p_vout = vout_Create( p_vout, &fmt );

    /* Everything failed */
    if( p_sys->p_vout == NULL )
    {
        msg_Err( p_vout, "can't open vout, aborting" );
        return VLC_EGENERIC;
    }

    var_AddCallback( p_sys->p_vout, "mouse-x", MouseEvent, p_vout);
    var_AddCallback( p_sys->p_vout, "mouse-y", MouseEvent, p_vout);

    ALLOCATE_DIRECTBUFFERS( VOUT_MAX_PICTURES );
    ADD_CALLBACKS( p_sys->p_vout, SendEvents );
    ADD_PARENT_CALLBACKS( SendEventsToChild );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate logo video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    int i_index;

    /* Free the fake output buffers we allocated */
    for( i_index = I_OUTPUTPICTURES ; i_index ; )
    {
        i_index--;
        free( PP_OUTPUTPICTURE[ i_index ]->p_data_orig );
    }

    var_DelCallback( p_sys->p_vout, "mouse-x", MouseEvent, p_vout);
    var_DelCallback( p_sys->p_vout, "mouse-y", MouseEvent, p_vout);

    if( p_sys->p_vout )
    {
        DEL_CALLBACKS( p_sys->p_vout, SendEvents );
        vlc_object_detach( p_sys->p_vout );
        vout_Destroy( p_sys->p_vout );
    }

    if( p_sys->p_blend->p_module )
        module_Unneed( p_sys->p_blend, p_sys->p_blend->p_module );
    vlc_object_detach( p_sys->p_blend );
    vlc_object_destroy( p_sys->p_blend );
}

/*****************************************************************************
 * Destroy: destroy logo video thread output method
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    vout_sys_t *p_sys = p_vout->p_sys;

    DEL_PARENT_CALLBACKS( SendEventsToChild );

    FreeLogoList( p_sys->p_logo_list );
    free( p_sys->p_logo_list );

    free( p_sys );
}

/*****************************************************************************
 * Render: render the logo onto the video
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_inpic )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    picture_t *p_outpic;
    picture_t *p_pic;
    logo_list_t *p_logo_list;
    logo_t * p_logo;

    p_logo_list = p_sys->p_logo_list;

    if( p_logo_list->i_next_pic < p_inpic->date )
    {
        /* It's time to use a new logo */
        p_logo_list->i_counter =
                        ( p_logo_list->i_counter + 1 )%p_logo_list->i_count;
        p_logo = &p_logo_list->p_logo[p_sys->p_logo_list->i_counter];
        p_pic = p_logo->p_pic;
        p_logo_list->i_next_pic = p_inpic->date + ( p_logo->i_delay != -1 ?
                              p_logo->i_delay : p_logo_list->i_delay ) * 1000;
        if( p_pic )
        {

            p_sys->i_width =
                p_sys->p_blend->fmt_in.video.i_width =
                    p_sys->p_blend->fmt_in.video.i_visible_width =
                        p_pic->p[Y_PLANE].i_visible_pitch;
            p_sys->i_height =
                p_sys->p_blend->fmt_in.video.i_height =
                    p_sys->p_blend->fmt_in.video.i_visible_height =
                        p_pic->p[Y_PLANE].i_visible_lines;

            /* Just in case the new image would overflow the vout */
            if( (unsigned int)(p_sys->posy + p_sys->i_height)
                                                > p_vout->render.i_height
             || (unsigned int)(p_sys->posx + p_sys->i_width)
                                                > p_vout->render.i_width
             || p_sys->pos )
            {
                if( p_sys->pos & SUBPICTURE_ALIGN_BOTTOM )
                {
                    p_sys->posy = p_vout->render.i_height - p_sys->i_height;
                }
                else if ( !(p_sys->pos & SUBPICTURE_ALIGN_TOP) )
                {
                    p_sys->posy = p_vout->render.i_height/2 - p_sys->i_height/2;
                }

                if( p_sys->pos & SUBPICTURE_ALIGN_RIGHT )
                {
                    p_sys->posx = p_vout->render.i_width - p_sys->i_width;
                }
                else if ( !(p_sys->pos & SUBPICTURE_ALIGN_LEFT) )
                {
                    p_sys->posx = p_vout->render.i_width/2 - p_sys->i_width/2;
                }
            }
        }

    }
    else
    {
        p_logo = &p_logo_list->p_logo[p_sys->p_logo_list->i_counter];
        p_pic = p_logo->p_pic;
    }

    /* This is a new frame. Get a structure from the video_output. */
    while( !(p_outpic = vout_CreatePicture( p_sys->p_vout, 0, 0, 0 )) )
    {
        if( p_vout->b_die || p_vout->b_error ) return;
        msleep( VOUT_OUTMEM_SLEEP );
    }

    vout_CopyPicture( p_vout, p_outpic, p_inpic );
    vout_DatePicture( p_sys->p_vout, p_outpic, p_inpic->date );

    if( p_pic )
    p_sys->p_blend->pf_video_blend( p_sys->p_blend, p_outpic, p_outpic,
                                    p_pic, p_sys->posx, p_sys->posy,
                                    p_logo->i_alpha != -1 ? p_logo->i_alpha
                                    : p_logo_list->i_alpha );

    vout_DisplayPicture( p_sys->p_vout, p_outpic );
}

/*****************************************************************************
 * SendEvents: forward mouse and keyboard events to the parent p_vout
 *****************************************************************************/
static int SendEvents( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    var_Set( (vlc_object_t *)p_data, psz_var, newval );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * MouseEvent: callback for mouse events
 *****************************************************************************/
static int MouseEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t*)p_data;
    vout_sys_t *p_sys = p_vout->p_sys;
    vlc_value_t valb;
    int i_delta;

    var_Get( p_vout->p_sys->p_vout, "mouse-button-down", &valb );

    i_delta = newval.i_int - oldval.i_int;

    if( (valb.i_int & 0x1) == 0 )
    {
        return VLC_SUCCESS;
    }

    if( psz_var[6] == 'x' )
    {
        vlc_value_t valy;
        var_Get( p_vout->p_sys->p_vout, "mouse-y", &valy );
        if( newval.i_int >= (int)p_sys->posx &&
            valy.i_int >= (int)p_sys->posy &&
            newval.i_int <= (int)(p_sys->posx + p_sys->i_width) &&
            valy.i_int <= (int)(p_sys->posy + p_sys->i_height) )
        {
            p_sys->posx = __MIN( __MAX( p_sys->posx + i_delta, 0 ),
                          p_vout->output.i_width - p_sys->i_width );
        }
    }
    else if( psz_var[6] == 'y' )
    {
        vlc_value_t valx;
        var_Get( p_vout->p_sys->p_vout, "mouse-x", &valx );
        if( valx.i_int >= (int)p_sys->posx &&
            newval.i_int >= (int)p_sys->posy &&
            valx.i_int <= (int)(p_sys->posx + p_sys->i_width) &&
            newval.i_int <= (int)(p_sys->posy + p_sys->i_height) )
        {
            p_sys->posy = __MIN( __MAX( p_sys->posy + i_delta, 0 ),
                          p_vout->output.i_height - p_sys->i_height );
        }
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Control: control facility for the vout (forwards to child vout)
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    return vout_vaControl( p_vout->p_sys->p_vout, i_query, args );
}

/*****************************************************************************
 * SendEventsToChild: forward events to the child/children vout
 *****************************************************************************/
static int SendEventsToChild( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    var_Set( p_vout->p_sys->p_vout, psz_var, newval );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * filter_sys_t: logo filter descriptor
 *****************************************************************************/
struct filter_sys_t
{
    logo_list_t *p_logo_list;

    int pos, posx, posy;

    vlc_bool_t b_absolute;
    mtime_t i_last_date;

    /* On the fly control variable */
    vlc_bool_t b_need_update;
};

static subpicture_t *Filter( filter_t *, mtime_t );

/*****************************************************************************
 * CreateFilter: allocates logo video filter
 *****************************************************************************/
static int CreateFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;
    vlc_object_t *p_input;
    logo_list_t *p_logo_list;

    /* Allocate structure */
    p_sys = p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_sys == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }
    p_logo_list = p_sys->p_logo_list = malloc( sizeof( logo_list_t ) );
    if( p_logo_list == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        free( p_sys );
        return VLC_ENOMEM;
    }

    /* Hook used for callback variables */
    p_input = vlc_object_find( p_this, VLC_OBJECT_INPUT, FIND_PARENT );
    if( !p_input )
    {
        free( p_sys );
        free( p_logo_list );
        return VLC_ENOOBJ;
    }

    p_logo_list->psz_filename =
        var_CreateGetString( p_input->p_libvlc , "logo-file" );
    if( !p_logo_list->psz_filename || !*p_logo_list->psz_filename )
    {
        msg_Err( p_this, "logo file not specified" );
        vlc_object_release( p_input );
        //if( p_sys->psz_filename ) free( p_sys->psz_filename );
        free( p_sys );
        free( p_logo_list );
        return VLC_EGENERIC;
    }

    p_sys->posx = var_CreateGetInteger( p_input->p_libvlc , "logo-x" );
    p_sys->posy = var_CreateGetInteger( p_input->p_libvlc , "logo-y" );
    p_sys->pos = var_CreateGetInteger( p_input->p_libvlc , "logo-position" );
    p_logo_list->i_alpha = __MAX( __MIN( var_CreateGetInteger(
                           p_input->p_libvlc, "logo-transparency"), 255 ), 0 );
    p_logo_list->i_delay =
                    var_CreateGetInteger( p_input->p_libvlc , "logo-delay" );
    p_logo_list->i_repeat =
                    var_CreateGetInteger( p_input->p_libvlc , "logo-repeat" );

    var_AddCallback( p_input->p_libvlc, "logo-file", LogoCallback, p_sys );
    var_AddCallback( p_input->p_libvlc, "logo-x", LogoCallback, p_sys );
    var_AddCallback( p_input->p_libvlc, "logo-y", LogoCallback, p_sys );
    var_AddCallback( p_input->p_libvlc, "logo-position", LogoCallback, p_sys );
    var_AddCallback( p_input->p_libvlc, "logo-transparency", LogoCallback, p_sys );
    var_AddCallback( p_input->p_libvlc, "logo-repeat", LogoCallback, p_sys );
    vlc_object_release( p_input );

    vlc_mutex_init( p_filter, &p_logo_list->lock );
    vlc_mutex_lock( &p_logo_list->lock );

    LoadLogoList( p_this, p_logo_list );

    vlc_mutex_unlock( &p_logo_list->lock );

    /* Misc init */
    p_filter->pf_sub_filter = Filter;
    p_sys->b_need_update = VLC_TRUE;

    p_sys->i_last_date = 0;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DestroyFilter: destroy logo video filter
 *****************************************************************************/
static void DestroyFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;
    vlc_object_t *p_input;

    vlc_mutex_destroy( &p_sys->p_logo_list->lock );
    FreeLogoList( p_sys->p_logo_list );
    free( p_sys->p_logo_list );
    free( p_sys );

    /* Delete the logo variables from INPUT */
    p_input = vlc_object_find( p_this, VLC_OBJECT_INPUT, FIND_PARENT );
    if( !p_input ) return;

    var_Destroy( p_input->p_libvlc , "logo-file" );
    var_Destroy( p_input->p_libvlc , "logo-x" );
    var_Destroy( p_input->p_libvlc , "logo-y" );
    var_Destroy( p_input->p_libvlc , "logo-delay" );
    var_Destroy( p_input->p_libvlc , "logo-repeat" );
    var_Destroy( p_input->p_libvlc , "logo-position" );
    var_Destroy( p_input->p_libvlc , "logo-transparency" );
    vlc_object_release( p_input );
}

/*****************************************************************************
 * Filter: the whole thing
 *****************************************************************************
 * This function outputs subpictures at regular time intervals.
 *****************************************************************************/
static subpicture_t *Filter( filter_t *p_filter, mtime_t date )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    logo_list_t *p_logo_list = p_sys->p_logo_list;
    subpicture_t *p_spu;
    subpicture_region_t *p_region;
    video_format_t fmt;
    picture_t *p_pic;
    logo_t *p_logo;

    vlc_mutex_lock( &p_logo_list->lock );
    /* Basic test:  b_need_update occurs on a dynamic change,
                    & i_next_pic is the general timer, when to
                    look at updating the logo image */

    if( ( ( !p_sys->b_need_update ) && ( p_logo_list->i_next_pic > date ) )
        || !p_logo_list->i_repeat )
    {
        vlc_mutex_unlock( &p_logo_list->lock );
        return 0;
    }
    /* prior code tested on && p_sys->i_last_date +5000000 > date ) return 0; */

    /* adjust index to the next logo */
    p_logo_list->i_counter =
                        ( p_logo_list->i_counter + 1 )%p_logo_list->i_count;

    p_logo = &p_logo_list->p_logo[p_logo_list->i_counter];
    p_pic = p_logo->p_pic;

    /* Allocate the subpicture internal data. */
    p_spu = p_filter->pf_sub_buffer_new( p_filter );
    if( !p_spu )
    {
        vlc_mutex_unlock( &p_logo_list->lock );
        return NULL;
    }

    p_spu->b_absolute = p_sys->b_absolute;
    p_spu->i_start = p_sys->i_last_date = date;
    p_spu->i_stop = 0;
    p_spu->b_ephemer = VLC_TRUE;

    p_sys->b_need_update = VLC_FALSE;
    p_logo_list->i_next_pic = date +
    ( p_logo->i_delay != -1 ? p_logo->i_delay : p_logo_list->i_delay ) * 1000;

    if( p_logo_list->i_repeat != -1
        && p_logo_list->i_counter == 0 )
    {
        p_logo_list->i_repeat--;
        if( p_logo_list->i_repeat == 0 )
        {
            vlc_mutex_unlock( &p_logo_list->lock );
            return p_spu;
        }
    }

    if( !p_pic || !p_logo->i_alpha
        || ( p_logo->i_alpha == -1 && !p_logo_list->i_alpha ) )
    {
        /* Send an empty subpicture to clear the display */
        vlc_mutex_unlock( &p_logo_list->lock );
        return p_spu;
    }

    /* Create new SPU region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_FOURCC('Y','U','V','A');
    fmt.i_aspect = VOUT_ASPECT_FACTOR;
    fmt.i_sar_num = fmt.i_sar_den = 1;
    fmt.i_width = fmt.i_visible_width = p_pic->p[Y_PLANE].i_visible_pitch;
    fmt.i_height = fmt.i_visible_height = p_pic->p[Y_PLANE].i_visible_lines;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    p_region = p_spu->pf_create_region( VLC_OBJECT(p_filter), &fmt );
    if( !p_region )
    {
        msg_Err( p_filter, "cannot allocate SPU region" );
        p_filter->pf_sub_buffer_del( p_filter, p_spu );
        vlc_mutex_unlock( &p_logo_list->lock );
        return NULL;
    }

    vout_CopyPicture( p_filter, &p_region->picture, p_pic );
    vlc_mutex_unlock( &p_logo_list->lock );

    /*  where to locate the logo: */
    if( p_sys->posx < 0 || p_sys->posy < 0 )
    {   /* set to one of the 9 relative locations */
        p_spu->i_flags = p_sys->pos;
        p_spu->i_x = 0;
        p_spu->i_y = 0;
        p_spu->b_absolute = VLC_FALSE;
    }
    else
    {   /*  set to an absolute xy, referenced to upper left corner */
        p_spu->i_flags = OSD_ALIGN_LEFT | OSD_ALIGN_TOP;
        p_spu->i_x = p_sys->posx;
        p_spu->i_y = p_sys->posy;
        p_spu->b_absolute = VLC_TRUE;
    }

    p_spu->p_region = p_region;

    p_spu->i_alpha = ( p_logo->i_alpha != -1 ?
                       p_logo->i_alpha : p_logo_list->i_alpha );

    return p_spu;
}

/*****************************************************************************
 * Callback to update params on the fly
 *****************************************************************************/
static int LogoCallback( vlc_object_t *p_this, char const *psz_var,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    filter_sys_t *p_sys = (filter_sys_t *)p_data;
    logo_list_t *p_logo_list = p_sys->p_logo_list;

    if( !strncmp( psz_var, "logo-file", 6 ) )
    {
        vlc_mutex_lock( &p_logo_list->lock );
        FreeLogoList( p_logo_list );
        p_logo_list->psz_filename = strdup( newval.psz_string );
        LoadLogoList( p_this, p_logo_list );
        vlc_mutex_unlock( &p_logo_list->lock );
        p_sys->b_need_update = VLC_TRUE;
    }
    else if ( !strncmp( psz_var, "logo-x", 6 ) )
    {
        p_sys->posx = newval.i_int;
    }
    else if ( !strncmp( psz_var, "logo-y", 6 ) )
    {
        p_sys->posy = newval.i_int;
    }
    else if ( !strncmp( psz_var, "logo-position", 12 ) )
    {
        p_sys->pos = newval.i_int;
    }
    else if ( !strncmp( psz_var, "logo-transparency", 9 ) )
    {
        vlc_mutex_lock( &p_logo_list->lock );
        p_logo_list->i_alpha = __MAX( __MIN( newval.i_int, 255 ), 0 );
        vlc_mutex_unlock( &p_logo_list->lock );
    }
    else if ( !strncmp( psz_var, "logo-repeat", 11 ) )
    {
        vlc_mutex_lock( &p_logo_list->lock );
        p_logo_list->i_repeat = newval.i_int;
        vlc_mutex_unlock( &p_logo_list->lock );
    }
    p_sys->b_need_update = VLC_TRUE;
    return VLC_SUCCESS;
}
