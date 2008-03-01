/*****************************************************************************
 * dynamicoverlay.c : dynamic overlay plugin for vlc
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Author: SÃ¸ren BÃ¸g <avacore@videolan.org>
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <fcntl.h>
#include <vlc/vlc.h>
#include <vlc_sout.h>
#include <vlc_vout.h>

#include <vlc_filter.h>
#include <vlc_osd.h>

#include "dynamicoverlay.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Create( vlc_object_t * );
static void Destroy( vlc_object_t * );
static subpicture_t *Filter( filter_t *, mtime_t );

static int AdjustCallback( vlc_object_t *p_this, char const *psz_var,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define INPUT_TEXT N_("Input FIFO")
#define INPUT_LONGTEXT N_("FIFO which will be read for commands")

#define OUTPUT_TEXT N_("Output FIFO")
#define OUTPUT_LONGTEXT N_("FIFO which will be written to for responses")

vlc_module_begin();
    set_description( _("Dynamic video overlay") );
    set_shortname( _("Overlay" ));
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VFILTER );
    set_capability( "sub filter", 0 );

    add_file( "overlay-input", NULL, NULL, INPUT_TEXT, INPUT_LONGTEXT,
              VLC_FALSE );
    add_file( "overlay-output", NULL, NULL, OUTPUT_TEXT, OUTPUT_LONGTEXT,
              VLC_FALSE );

    add_shortcut( "overlay" );
    set_callbacks( Create, Destroy );
vlc_module_end();

static const char *ppsz_filter_options[] = {
    "input", "output", NULL
};

/*****************************************************************************
 * overlay_t: Overlay descriptor
 *****************************************************************************/

struct overlay_t
{
    int i_x, i_y;
    int i_alpha;
    vlc_bool_t b_active;

    video_format_t format;
    union {
        picture_t *p_pic;
        char *p_text;
    } data;
};
typedef struct overlay_t overlay_t;

static overlay_t *OverlayCreate( void )
{
    overlay_t *p_ovl = malloc( sizeof( overlay_t ) );
    if( p_ovl == NULL )
       return NULL;
    memset( p_ovl, 0, sizeof( overlay_t ) );

    p_ovl->i_x = p_ovl->i_y = 0;
    p_ovl->i_alpha = 0xFF;
    p_ovl->b_active = VLC_FALSE;
    vout_InitFormat( &p_ovl->format, VLC_FOURCC( '\0','\0','\0','\0') , 0, 0,
                     VOUT_ASPECT_FACTOR );
    p_ovl->data.p_text = NULL;

    return p_ovl;
}

static int OverlayDestroy( overlay_t *p_ovl )
{
    if( p_ovl->data.p_text != NULL )
        free( p_ovl->data.p_text );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * list_t: Command queue
 *****************************************************************************/

struct list_t
{
    overlay_t **pp_head, **pp_tail;
};
typedef struct list_t list_t;

static int ListInit( list_t *p_list )
{
    p_list->pp_head = malloc( 16 * sizeof( overlay_t * ) );
    if( p_list->pp_head == NULL )
        return VLC_ENOMEM;

    p_list->pp_tail = p_list->pp_head + 16;
    memset( p_list->pp_head, 0, 16 * sizeof( overlay_t * ) );

    return VLC_SUCCESS;
}

static int ListDestroy( list_t *p_list )
{
    for( overlay_t **pp_cur = p_list->pp_head;
         pp_cur < p_list->pp_tail;
         ++pp_cur )
    {
        if( *pp_cur != NULL )
        {
            OverlayDestroy( *pp_cur );
            free( *pp_cur );
        }
    }
    free( p_list->pp_head );

    return VLC_SUCCESS;
}

static ssize_t ListAdd( list_t *p_list, overlay_t *p_new )
{
    /* Find an available slot */
    for( overlay_t **pp_cur = p_list->pp_head;
         pp_cur < p_list->pp_tail;
         ++pp_cur )
    {
        if( *pp_cur == NULL )
        {
            *pp_cur = p_new;
            return pp_cur - p_list->pp_head;
        }
    }

    /* Have to expand */
    size_t i_size = p_list->pp_tail - p_list->pp_head;
    size_t i_newsize = i_size * 2;
    p_list->pp_head = realloc( p_list->pp_head,
                               i_newsize * sizeof( overlay_t * ) );
    if( p_list->pp_head == NULL )
        return VLC_ENOMEM;

    p_list->pp_tail = p_list->pp_head + i_newsize;
    memset( p_list->pp_head + i_size, 0, i_size * sizeof( overlay_t * ) );
    p_list->pp_head[i_size] = p_new;
    return i_size;
}

static int ListRemove( list_t *p_list, size_t i_idx )
{
    int ret;

    if( ( i_idx >= (size_t)( p_list->pp_tail - p_list->pp_head ) ) ||
        ( p_list->pp_head[i_idx] == NULL ) )
    {
        return VLC_EGENERIC;
    }

    ret = OverlayDestroy( p_list->pp_head[i_idx] );
    free( p_list->pp_head[i_idx] );
    p_list->pp_head[i_idx] = NULL;

    return ret;
}

static overlay_t *ListGet( list_t *p_list, size_t i_idx )
{
    if( ( i_idx >= (size_t)( p_list->pp_tail - p_list->pp_head ) ) ||
        ( p_list->pp_head[i_idx] == NULL ) )
    {
        return NULL;
    }
    return p_list->pp_head[i_idx];
}

static overlay_t *ListWalk( list_t *p_list )
{
    static overlay_t **pp_cur = NULL;

    if( pp_cur == NULL )
        pp_cur = p_list->pp_head;
    else
        pp_cur = pp_cur + 1;

    for( ; pp_cur < p_list->pp_tail; ++pp_cur )
    {
        if( ( *pp_cur != NULL ) &&
            ( (*pp_cur)->b_active == VLC_TRUE )&&
            ( (*pp_cur)->format.i_chroma != VLC_FOURCC( '\0','\0','\0','\0') ) )
        {
            return *pp_cur;
        }
    }
    pp_cur = NULL;
    return NULL;
}

/*****************************************************************************
 * filter_sys_t: adjust filter method descriptor
 *****************************************************************************/
struct filter_sys_t
{
    buffer_t input, output;

    int i_inputfd, i_outputfd;

    char *psz_inputfile, *psz_outputfile;

    vlc_bool_t b_updated, b_atomic;
    queue_t atomic, pending, processed;
    list_t overlays;
};

/*****************************************************************************
 * Command functions
 *****************************************************************************/

#define SKIP \
    { \
        const char *psz_temp = psz_command; \
        while( isspace( *psz_temp ) ) { \
            ++psz_temp; \
        } \
        if( psz_temp == psz_command ) { \
            return VLC_EGENERIC; \
        } \
        psz_command = psz_temp; \
    }
#define INT( name ) \
    SKIP \
    { \
        char *psz_temp; \
        p_myparams->name = strtol( psz_command, &psz_temp, 10 ); \
        if( psz_temp == psz_command ) \
        { \
            return VLC_EGENERIC; \
        } \
        psz_command = psz_temp; \
    }
#define CHARS( name, count ) \
    SKIP \
    { \
        if( psz_end - psz_command < count ) \
        { \
            return VLC_EGENERIC; \
        } \
        memcpy( p_myparams->name, psz_command, count ); \
        psz_command += count; \
    }
#define COMMAND( name, param, ret, atomic, code ) \
static int Command##name##Parse( const char *psz_command, \
                                 const char *psz_end, \
                                 commandparams_t *p_params ) \
{ \
    struct commandparams##name##_t *p_myparams = &p_params->name; \
    param \
    return VLC_SUCCESS; \
}
#include "dynamicoverlay_commands.h"
#undef COMMAND
#undef SKIP
#undef INT
#undef CHARS

#define COMMAND( name, param, ret, atomic, code ) \
static int Command##name##Exec( filter_t *p_filter, \
                                const commandparams_t *p_gparams, \
                                commandresults_t *p_gresults, \
                                filter_sys_t *p_sys ) \
{ \
    const struct commandparams##name##_t *p_params = &p_gparams->name; \
    struct commandresults##name##_t *p_results = &p_gresults->name; \
    code \
}
#include "dynamicoverlay_commands.h"
#undef COMMAND

#define INT( name ) \
    { \
        int ret = BufferPrintf( p_output, " %d", p_myresults->name ); \
        if( ret != VLC_SUCCESS ) { \
            return ret; \
        } \
    }
#define CHARS( name, count ) \
    { \
        int ret = BufferAdd( p_output, p_myresults->name, count ); \
        if( ret != VLC_SUCCESS ) { \
            return ret; \
        } \
    }
#define COMMAND( name, param, ret, atomic, code ) \
static int Command##name##Unparse( const commandresults_t *p_results, \
                           buffer_t *p_output ) \
{ \
    const struct commandresults##name##_t *p_myresults = &p_results->name; \
    ret \
    return VLC_SUCCESS; \
}
#include "dynamicoverlay_commands.h"
#undef COMMAND
#undef INT
#undef CHARS

static commanddesc_t p_commands[] =
{
#define COMMAND( name, param, ret, atomic, code ) \
{ #name, atomic, Command##name##Parse, Command##name##Exec, Command##name##Unparse },
#include "dynamicoverlay_commands.h"
#undef COMMAND
};

#define NUMCOMMANDS (sizeof(p_commands)/sizeof(commanddesc_t))

/*****************************************************************************
 * Create: allocates adjust video thread output method
 *****************************************************************************
 * This function allocates and initializes a adjust vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    /* Allocate structure */
    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }
    p_sys = p_filter->p_sys;

    BufferInit( &p_sys->input );
    BufferInit( &p_sys->output );
    QueueInit( &p_sys->atomic );
    QueueInit( &p_sys->pending );
    QueueInit( &p_sys->processed );
    ListInit( &p_sys->overlays );

    p_sys->i_inputfd = -1;
    p_sys->i_outputfd = -1;
    p_sys->b_updated = VLC_TRUE;
    p_sys->b_atomic = VLC_FALSE;

    p_filter->pf_sub_filter = Filter;

    config_ChainParse( p_filter, "overlay-", ppsz_filter_options,
                       p_filter->p_cfg );

    p_sys->psz_inputfile = var_CreateGetStringCommand( p_filter,
                                                       "overlay-input" );
    p_sys->psz_outputfile = var_CreateGetStringCommand( p_filter,
                                                        "overlay-output" );

    var_AddCallback( p_filter, "overlay-input", AdjustCallback, p_sys );
    var_AddCallback( p_filter, "overlay-output", AdjustCallback, p_sys );

    msg_Dbg( p_filter, "%d commands are available:", NUMCOMMANDS );
    for( size_t i_index = 0; i_index < NUMCOMMANDS; ++i_index )
        msg_Dbg( p_filter, "    %s", p_commands[i_index].psz_command );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Destroy: destroy adjust video thread output method
 *****************************************************************************
 * Terminate an output method created by adjustCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    BufferDestroy( &p_filter->p_sys->input );
    BufferDestroy( &p_filter->p_sys->output );
    QueueDestroy( &p_filter->p_sys->atomic );
    QueueDestroy( &p_filter->p_sys->pending );
    QueueDestroy( &p_filter->p_sys->processed );
    ListDestroy( &p_filter->p_sys->overlays );

    free( p_filter->p_sys->psz_inputfile );
    free( p_filter->p_sys->psz_outputfile );
    free( p_filter->p_sys );
}

/*****************************************************************************
 * Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to adjust modified image,
 * waits until it is displayed and switch the two rendering buffers, preparing
 * next frame.
 *****************************************************************************/
static subpicture_t *Filter( filter_t *p_filter, mtime_t date )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    /* We might need to open these at any time. */
    if( p_sys->i_inputfd == -1 )
    {
        p_sys->i_inputfd = open( p_sys->psz_inputfile, O_RDONLY | O_NONBLOCK );
        if( p_sys->i_inputfd == -1 )
        {
            msg_Warn( p_filter, "Failed to grab input file: %s (%s)",
                      p_sys->psz_inputfile, strerror( errno ) );
        }
        else
        {
            msg_Info( p_filter, "Grabbed input file: %s",
                      p_sys->psz_inputfile );
        }
    }

    if( p_sys->i_outputfd == -1 )
    {
        p_sys->i_outputfd = open( p_sys->psz_outputfile,
                                  O_WRONLY | O_NONBLOCK );
        if( p_sys->i_outputfd == -1 )
        {
            if( errno != ENXIO )
            {
                msg_Warn( p_filter, "Failed to grab output file: %s (%s)",
                          p_sys->psz_outputfile, strerror( errno ) );
            }
        }
        else
        {
            msg_Info( p_filter, "Grabbed output file: %s",
                      p_sys->psz_outputfile );
        }
    }

    /* Read any waiting commands */
    if( p_sys->i_inputfd != -1 )
    {
        char p_buffer[1024];
        ssize_t i_len = read( p_sys->i_inputfd, p_buffer, 1024 );
        if( i_len == -1 )
        {
            /* We hit an error */
            if( errno != EAGAIN )
            {
                msg_Warn( p_filter, "Error on input file: %s",
                          strerror( errno ) );
                close( p_sys->i_inputfd );
                p_sys->i_inputfd = -1;
            }
        }
        else if( i_len == 0 )
        {
            /* We hit the end-of-file */
        }
        else
        {
            BufferAdd( &p_sys->input, p_buffer, i_len );
        }
    }

    /* Parse any complete commands */
    char *p_end, *p_cmd;
    while( ( p_end = memchr( p_sys->input.p_begin, '\n',
                            p_sys->input.i_length ) ) )
    {
        *p_end = '\0';
        p_cmd = p_sys->input.p_begin;

        commanddesc_t *p_lower = p_commands;
        commanddesc_t *p_upper = p_commands + NUMCOMMANDS;
        size_t i_index = 0;
        while( 1 )
        {
            if( ( p_cmd[i_index] == '\0' ) ||
                isspace( p_cmd[i_index] ) )
            {
                break;
            }
            commanddesc_t *p_cur = p_lower;
            while( ( p_cur < p_upper ) &&
                   ( p_cur->psz_command[i_index] != p_cmd[i_index] ) )
            {
                ++p_cur;
            }
            p_lower = p_cur;
            while( ( p_cur < p_upper ) &&
                   ( p_cur->psz_command[i_index] == p_cmd[i_index] ) )
            {
                ++p_cur;
            }
            p_upper = p_cur;
            ++i_index;
        }
        if( p_lower >= p_upper )
        {
            /* No matching command */
            p_cmd[i_index] = '\0';
            msg_Err( p_filter, "Got invalid command: %s", p_cmd );
            BufferPrintf( &p_sys->output, "FAILURE: %d Invalid Command\n", VLC_EGENERIC );
        }
        else if ( p_lower + 1 < p_upper )
        {
            /* Command is not a unique prefix of a command */
            p_cmd[i_index] = '\0';
            msg_Err( p_filter, "Got ambiguous command: %s", p_cmd );
            msg_Err( p_filter, "Possible completions are:" );
            for( ; p_lower < p_upper; ++p_lower )
            {
                msg_Err( p_filter, "    %s", p_lower->psz_command );
            }
            BufferPrintf( &p_sys->output, "FAILURE: %d Invalid Command\n", VLC_EGENERIC );
        }
        else
        {
            /* Command is valid */
            command_t *p_command = malloc( sizeof( command_t ) );
            if( !p_command )
                return NULL;

            p_command->p_command = p_lower;
            p_command->p_command->pf_parser( p_cmd + i_index, p_end,
                                             &p_command->params );

            if( ( p_command->p_command->b_atomic == VLC_TRUE ) &&
                ( p_sys->b_atomic == VLC_TRUE ) )
                QueueEnqueue( &p_sys->atomic, p_command );
            else
                QueueEnqueue( &p_sys->pending, p_command );

            p_cmd[i_index] = '\0';
            msg_Dbg( p_filter, "Got valid command: %s", p_cmd );
        }

        BufferDel( &p_sys->input, p_end - p_sys->input.p_begin + 1 );
    }

    /* Process any pending commands */
    command_t *p_command = NULL;
    while( (p_command = QueueDequeue( &p_sys->pending )) )
    {
        p_command->i_status =
            p_command->p_command->pf_execute( p_filter, &p_command->params,
                                              &p_command->results, p_sys );
        QueueEnqueue( &p_sys->processed, p_command );
    }

    /* Output any processed commands */
    while( (p_command = QueueDequeue( &p_sys->processed )) )
    {
        if( p_command->i_status == VLC_SUCCESS )
        {
            const char *psz_success = "SUCCESS:";
            const char *psz_nl = "\n";
            BufferAdd( &p_sys->output, psz_success, 8 );
            p_command->p_command->pf_unparser( &p_command->results,
                                               &p_sys->output );
            BufferAdd( &p_sys->output, psz_nl, 1 );
        }
        else
        {
            BufferPrintf( &p_sys->output, "FAILURE: %d\n",
                          p_command->i_status );
        }
    }

    /* Try emptying the output buffer */
    if( p_sys->i_outputfd != -1 )
    {
        ssize_t i_len = write( p_sys->i_outputfd, p_sys->output.p_begin,
                              p_sys->output.i_length );
        if( i_len == -1 )
        {
            /* We hit an error */
            if( errno != EAGAIN )
            {
                msg_Warn( p_filter, "Error on output file: %s",
                          strerror( errno ) );
                close( p_sys->i_outputfd );
                p_sys->i_outputfd = -1;
            }
        }
        else
        {
            BufferDel( &p_sys->output, i_len );
        }
    }

    if( p_sys->b_updated == VLC_FALSE )
        return NULL;

    subpicture_t *p_spu = NULL;
    overlay_t *p_overlay = NULL;

    p_spu = p_filter->pf_sub_buffer_new( p_filter );
    if( !p_spu )
    {
        msg_Err( p_filter, "cannot allocate subpicture" );
        return NULL;
    }

    p_spu->i_flags = OSD_ALIGN_LEFT | OSD_ALIGN_TOP;
    p_spu->i_x = 0;
    p_spu->i_y = 0;
    p_spu->b_absolute = VLC_TRUE;
    p_spu->i_start = date;
    p_spu->i_stop = 0;
    p_spu->b_ephemer = VLC_TRUE;

    subpicture_region_t **pp_region = &p_spu->p_region;
    while( (p_overlay = ListWalk( &p_sys->overlays )) )
    {
        msg_Dbg( p_filter, "Displaying overlay: %4.4s, %d, %d, %d",
                 &p_overlay->format.i_chroma, p_overlay->i_x, p_overlay->i_y,
                 p_overlay->i_alpha );
        if( p_overlay->format.i_chroma == VLC_FOURCC('T','E','X','T') )
        {
            *pp_region = p_spu->pf_create_region( p_filter,
                                                  &p_overlay->format );
            if( !*pp_region )
            {
                msg_Err( p_filter, "cannot allocate subpicture region" );
                continue;
            }
            (*pp_region)->psz_text = strdup( p_overlay->data.p_text );
        }
        else
        {
            picture_t clone;
            if( vout_AllocatePicture( p_filter, &clone,
                                      p_overlay->format.i_chroma,
                                      p_overlay->format.i_width,
                                      p_overlay->format.i_height,
                                      p_overlay->format.i_aspect ) )
            {
                msg_Err( p_filter, "cannot allocate picture" );
                continue;
            }
            vout_CopyPicture( p_filter, &clone, p_overlay->data.p_pic );
            *pp_region = p_spu->pf_make_region( p_filter, &p_overlay->format,
                                                &clone );
            if( !*pp_region )
            {
                msg_Err( p_filter, "cannot allocate subpicture region" );
                continue;
            }
        }
        (*pp_region)->i_x = p_overlay->i_x;
        (*pp_region)->i_y = p_overlay->i_y;
        (*pp_region)->i_align = OSD_ALIGN_LEFT | OSD_ALIGN_TOP;
        (*pp_region)->i_alpha = p_overlay->i_alpha;
        pp_region = &(*pp_region)->p_next;
    }

    p_sys->b_updated = VLC_FALSE;
    return p_spu;
}

static int AdjustCallback( vlc_object_t *p_this, char const *psz_var,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data )
{
    filter_sys_t *p_sys = (filter_sys_t *)p_data;
    VLC_UNUSED(p_this); VLC_UNUSED(psz_var); VLC_UNUSED(oldval); VLC_UNUSED(newval);
    return VLC_EGENERIC;
}
