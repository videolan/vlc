#define __VLC__
#include <mediacontrol_internal.h>
#include <vlc/mediacontrol.h>

mediacontrol_Instance* mediacontrol_new( char** args, mediacontrol_Exception *exception )
{
    mediacontrol_Instance* retval;
    vlc_object_t *p_vlc;
    int p_vlc_id;
    char **ppsz_argv;
    int i_count = 0;
    int i_index;
    char **p_tmp;

    if( args )
    {
        for ( p_tmp = args ; *p_tmp != NULL ; p_tmp++ )
            i_count++;
    }

    ppsz_argv = malloc( i_count + 2 );
    if( ! ppsz_argv )
    {
        exception->code = mediacontrol_InternalException;
        exception->message = "Out of memory";
        return NULL;
    }
    ppsz_argv[0] = "vlc";
    for ( i_index = 0; i_index < i_count; i_index++ )
    {
        ppsz_argv[i_index + 1] = strdup( args[i_index] );
        if( ! ppsz_argv[i_index + 1] )
        {
            exception->code = mediacontrol_InternalException;
            exception->message = "Out of memory";
            return NULL;
        }
    }

    ppsz_argv[i_count + 1] = NULL;

    p_vlc_id = VLC_Create();

    if( p_vlc_id < 0 )
    {
        exception->code = mediacontrol_InternalException;
        exception->message = strdup( "Unable to create VLC" );
        return NULL;        
    }

    p_vlc = ( vlc_object_t* )vlc_current_object( p_vlc_id );
  
    if( ! p_vlc )
    {
        exception->code = mediacontrol_InternalException;
        exception->message = strdup( "Unable to find VLC object" );
        return NULL;
    }
    retval = ( mediacontrol_Instance* )malloc( sizeof( mediacontrol_Instance ) );
    if( ! retval )
    {
        exception->code = mediacontrol_InternalException;
        exception->message = strdup( "Out of memory" );
        return NULL;
    }

    if( VLC_Init( p_vlc_id, i_count + 1, ppsz_argv ) != VLC_SUCCESS )
    {
        exception->code = mediacontrol_InternalException;
        exception->message = strdup( "Cannot initialize VLC" );
        return NULL;
    }

    retval->p_vlc = p_vlc;
    retval->vlc_object_id = p_vlc_id;

    /* We can keep references on these, which should not change. Is it true ? */
    retval->p_playlist = vlc_object_find( p_vlc,
                                         VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    retval->p_intf = vlc_object_find( p_vlc, VLC_OBJECT_INTF, FIND_ANYWHERE );

    if( ! retval->p_playlist || ! retval->p_intf )
    {
        exception->code = mediacontrol_InternalException;
        exception->message = strdup( "No available interface" );
        return NULL;
    }

    
    return retval;  
};

void
mediacontrol_exit( mediacontrol_Instance *self )
{
  
    vlc_object_release( (vlc_object_t* )self->p_playlist );
    vlc_object_release( (vlc_object_t* )self->p_intf );
    vlc_object_release( (vlc_object_t*)self->p_vlc );
  
    VLC_CleanUp( self->vlc_object_id );
    VLC_Destroy( self->vlc_object_id );
}
