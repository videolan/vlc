#include <vlc/control.h>
#include <vlc/intf.h>

mediacontrol_Instance* mediacontrol_new( char** args, mediacontrol_Exception *exception )
{
    exception->code = mediacontrol_InternalException;
    exception->message = strdup( "The mediacontrol extension was compiled for plugin use only." );
    return NULL;
};

void
mediacontrol_exit( mediacontrol_Instance *self )
{
    vlc_mutex_lock( &self->p_intf->change_lock );
    self->p_intf->b_die = 1;
    vlc_mutex_unlock( &self->p_intf->change_lock );
}
