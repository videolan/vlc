/*****************************************************************************
 * win32touch.c: touch gestures recognition
 *****************************************************************************
 * Copyright © 2013-2014 VideoLAN
 *
 * Authors: Ludovic Fauvet <etix@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "win32touch.h"

#include <vlc_actions.h>

#include <assert.h>

enum {
    GESTURE_ACTION_UNDEFINED = 0,
    GESTURE_ACTION_VOLUME,
    GESTURE_ACTION_JUMP,
    GESTURE_ACTION_BRIGHTNESS
};


struct win32_gesture_sys_t
{
    DWORD       i_type;                 /* Gesture ID */
    int         i_action;               /* GESTURE_ACTION */

    int         i_beginx;               /* Start X position */
    int         i_beginy;               /* Start Y position */
    int         i_lasty;                /* Last known Y position for PAN */
    double      f_lastzoom;             /* Last zoom factor */

    ULONGLONG   i_ullArguments;         /* Base values to compare between 2 zoom gestures */
    bool        b_2fingers;             /* Did we detect 2 fingers? */

    bool (*DecodeGestureImpl)( vlc_object_t *, struct win32_gesture_sys_t *, const GESTUREINFO* );
};


static bool DecodeGestureAction( vlc_object_t *p_this, struct win32_gesture_sys_t *p_gesture, const GESTUREINFO* p_gi )
{
    bool bHandled = false; /* Needed to release the handle */
    switch ( p_gi->dwID )
    {
        case GID_BEGIN:
            /* Set the win32_gesture_sys_t values */
            p_gesture->i_beginx      = p_gi->ptsLocation.x;
            p_gesture->i_beginy      = p_gi->ptsLocation.y;
            p_gesture->i_lasty       = p_gesture->i_beginy;
            p_gesture->b_2fingers    = false;
            break;
        case GID_END:
            if( p_gesture->i_type != 0 &&
                p_gesture->i_action == GESTURE_ACTION_JUMP )
            {
                int action_id;
                if( p_gesture->i_beginx > p_gi->ptsLocation.x )
                {
                    if( p_gesture->b_2fingers )
                        action_id = ACTIONID_JUMP_BACKWARD_MEDIUM;
                    else
                        action_id = ACTIONID_JUMP_BACKWARD_SHORT;
                }
                else
                {
                    if( p_gesture->b_2fingers )
                        action_id = ACTIONID_JUMP_FORWARD_MEDIUM;
                    else
                        action_id = ACTIONID_JUMP_FORWARD_SHORT;
                }
                var_SetInteger( vlc_object_instance(p_this), "key-action", action_id );
            }
            /* Reset the values */
            p_gesture->i_action = GESTURE_ACTION_UNDEFINED;
            p_gesture->i_type = p_gesture->i_beginx = p_gesture->i_beginy = -1;
            p_gesture->b_2fingers = false;
            break;
        case GID_PAN:
            p_gesture->i_type = GID_PAN;
            bHandled = true;

            if (p_gi->dwFlags & GF_BEGIN) {
                p_gesture->i_beginx = p_gi->ptsLocation.x;
                p_gesture->i_beginy = p_gi->ptsLocation.y;
            }

            if( (DWORD)p_gi->ullArguments > 0 )
                p_gesture->b_2fingers = true;

            if( p_gesture->i_action == GESTURE_ACTION_UNDEFINED )
            {
                if( abs( p_gesture->i_beginx - p_gi->ptsLocation.x ) +
                    abs( p_gesture->i_beginy - p_gi->ptsLocation.y ) > 50 )
                {
                    if( abs( p_gesture->i_beginx - p_gi->ptsLocation.x ) >
                        abs( p_gesture->i_beginy - p_gi->ptsLocation.y ) )
                       p_gesture->i_action =  GESTURE_ACTION_JUMP;
                    else if ( p_gesture->b_2fingers )
                       p_gesture->i_action = GESTURE_ACTION_BRIGHTNESS;
                    else
                       p_gesture->i_action =  GESTURE_ACTION_VOLUME;
                }
            }

            if( p_gesture->i_action == GESTURE_ACTION_VOLUME )
            {
                int offset = p_gesture->i_lasty - p_gi->ptsLocation.y;

                if( offset > 100)
                    var_SetInteger( vlc_object_instance(p_this), "key-action", ACTIONID_VOL_UP );
                else if( offset < -100)
                    var_SetInteger( vlc_object_instance(p_this), "key-action", ACTIONID_VOL_DOWN );
                else
                    break;

                p_gesture->i_lasty = p_gi->ptsLocation.y;
            }
            else if ( p_gesture->i_action == GESTURE_ACTION_BRIGHTNESS )
            {
                /* Currently unimplemented
                if( p_gesture->i_lasty == -1 )
                    p_gesture->i_lasty = p_gesture->i_beginy;

                if( p_gesture->i_lasty - p_gesture->i_beginy > 80 )
                {
                    var_SetInteger( vlc_object_instance(p_this), "key-action", ACTIONID_BRIGHTNESS_DOWN );
                    p_gesture->i_lasty = p_gi->ptsLocation.y;
                }
                else if ( p_gesture->i_lasty - p_gesture->i_beginy < 80 )
                {
                    var_SetInteger( vlc_object_instance(p_this), "key-action", ACTIONID_BRIGHTNESS_UP );
                    p_gesture->i_lasty = p_gi->ptsLocation.y;
                } */
            }
            break;
        case GID_TWOFINGERTAP:
            p_gesture->i_type = GID_TWOFINGERTAP;
            var_SetInteger( vlc_object_instance(p_this), "key-action", ACTIONID_PLAY_PAUSE );
            bHandled = true;
            break;
        case GID_ZOOM:
            p_gesture->i_type = GID_ZOOM;
            switch( p_gi->dwFlags )
            {
                case GF_BEGIN:
                    p_gesture->i_ullArguments = p_gi->ullArguments;
                    break;
                case GF_END:
                    {
                        double k = (double)(p_gi->ullArguments) /
                                   (double)(p_gesture->i_ullArguments);
                        if( k > 1 )
                            var_SetInteger( vlc_object_instance(p_this), "key-action",
                                    ACTIONID_TOGGLE_FULLSCREEN );
                        else
                            var_SetInteger( vlc_object_instance(p_this), "key-action",
                                    ACTIONID_LEAVE_FULLSCREEN );
                    }
                    break;
                default:
                    msg_Err( p_this, "Unmanaged dwFlag: %lx", p_gi->dwFlags );
            }
            bHandled = true;
            break;
        case WM_VSCROLL:
            bHandled = true;
            break;
        default:
            break;
    }
    return bHandled;
}


static bool DecodeGestureProjection( vlc_object_t *p_this, struct win32_gesture_sys_t *p_gesture, const GESTUREINFO* p_gi )
{
    //vout_display_t *vd = (vout_display_t *)p_this;

    bool bHandled = false; /* Needed to release the handle */
    switch ( p_gi->dwID )
    {
        case GID_BEGIN:
            /* Set the win32_gesture_sys_t values */
            p_gesture->i_beginx      = p_gi->ptsLocation.x;
            p_gesture->i_beginy      = p_gi->ptsLocation.y;
            p_gesture->i_lasty       = p_gesture->i_beginy;
            p_gesture->b_2fingers    = false;
            break;
        case GID_END:
            if( p_gesture->i_type != 0 &&
                p_gesture->i_action == GESTURE_ACTION_JUMP )
            {
                int action_id;
                if( p_gesture->b_2fingers )
                {
                    if( p_gesture->i_beginx > p_gi->ptsLocation.x )
                        action_id = ACTIONID_JUMP_BACKWARD_SHORT;
                    else
                        action_id = ACTIONID_JUMP_FORWARD_SHORT;
                    var_SetInteger( vlc_object_instance(p_this), "key-action", action_id );
                }
            }
            /* Reset the values */
            p_gesture->i_action = GESTURE_ACTION_UNDEFINED;
            p_gesture->i_type = p_gesture->i_beginx = p_gesture->i_beginy = -1;
            p_gesture->b_2fingers = false;
            break;
        case GID_PAN:
            //vd->cfg->display.width;
            p_gesture->i_type = GID_PAN;
            bHandled = true;
            if (p_gi->dwFlags & GF_BEGIN) {
                p_gesture->i_beginx = p_gi->ptsLocation.x;
                p_gesture->i_beginy = p_gi->ptsLocation.y;
            }

            if( (DWORD)p_gi->ullArguments > 0 )
                p_gesture->b_2fingers = true;

            if( p_gesture->b_2fingers && p_gesture->i_action == GESTURE_ACTION_UNDEFINED )
            {
                if( abs( p_gesture->i_beginx - p_gi->ptsLocation.x ) +
                    abs( p_gesture->i_beginy - p_gi->ptsLocation.y ) > 50 )
                {
                    if( abs( p_gesture->i_beginx - p_gi->ptsLocation.x ) >
                        abs( p_gesture->i_beginy - p_gi->ptsLocation.y ) )
                        p_gesture->i_action =  GESTURE_ACTION_JUMP;
                    else
                        p_gesture->i_action =  GESTURE_ACTION_VOLUME;
                }
            }

            if( p_gesture->i_action == GESTURE_ACTION_VOLUME )
            {
                int offset = p_gesture->i_lasty - p_gi->ptsLocation.y;

                if( offset > 100)
                    var_SetInteger( vlc_object_instance(p_this), "key-action", ACTIONID_VOL_UP );
                else if( offset < -100)
                    var_SetInteger( vlc_object_instance(p_this), "key-action", ACTIONID_VOL_DOWN );
                else
                    break;

                p_gesture->i_lasty = p_gi->ptsLocation.y;
            }
            break;
        case GID_TWOFINGERTAP:
            p_gesture->i_type = GID_TWOFINGERTAP;
            var_SetInteger( vlc_object_instance(p_this), "key-action", ACTIONID_PLAY_PAUSE );
            bHandled = true;
            break;
        case GID_ZOOM:
            p_gesture->i_type = GID_ZOOM;
            bHandled = true;
            switch( p_gi->dwFlags )
            {
                case GF_BEGIN:
                    p_gesture->i_ullArguments = p_gi->ullArguments;
                    p_gesture->f_lastzoom = 1.0;
                    break;
                default:
                {
                    double k = (double)(p_gi->ullArguments) /
                               (double)(p_gesture->i_ullArguments);

                    if (k > p_gesture->f_lastzoom * 1.01)
                    {
                        var_SetInteger( vlc_object_instance(p_this), "key-action", ACTIONID_VIEWPOINT_FOV_IN );
                        p_gesture->f_lastzoom = k;
                    }
                    else if (k < p_gesture->f_lastzoom * 0.99)
                    {
                        var_SetInteger( vlc_object_instance(p_this), "key-action", ACTIONID_VIEWPOINT_FOV_OUT );
                        p_gesture->f_lastzoom = k;
                    }
                    break;
                }
            }
            break;
        case WM_VSCROLL:
            bHandled = true;
            break;
        default:
            break;
    }
    return bHandled;
}

bool DecodeGesture( vlc_object_t *p_this, struct win32_gesture_sys_t *p_gesture,
                    HGESTUREINFO hGestureInfo )
{
    if( !p_gesture )
        return false;

    GESTUREINFO gi;
    ZeroMemory( &gi, sizeof( GESTUREINFO ) );
    gi.cbSize = sizeof( GESTUREINFO );

    if( GetGestureInfo(hGestureInfo, &gi) )
    {
        if( p_gesture->DecodeGestureImpl(p_this, p_gesture, &gi) )
        {
            /* Close the Handle, if we handled the gesture, a contrario
             * from the doc example */
            CloseGestureInfoHandle(hGestureInfo);
            return true;
        }
    }
    else
    {
        DWORD dwErr = GetLastError();
        if( dwErr > 0 )
            msg_Err( p_this, "Could not retrieve a valid GESTUREINFO structure" );
    }

    return false;
}

struct win32_gesture_sys_t *InitGestures( HWND hwnd, bool b_isProjected )
{
    BOOL result = FALSE;
    GESTURECONFIG config = { 0, 0, 0 };
    if (b_isProjected)
    {
        //don't handle single finger pan in projected mode, it will be interpreted
        //as mouse events.
        config.dwID    = GID_PAN;
        config.dwWant  = GC_PAN;
        config.dwBlock = GC_PAN_WITH_INERTIA;
    }
    else
    {
        config.dwID    = GID_PAN;
        config.dwWant  = GC_PAN |
                         GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY |
                         GC_PAN_WITH_SINGLE_FINGER_VERTICALLY;
        config.dwBlock = GC_PAN_WITH_INERTIA;
    }

    struct win32_gesture_sys_t *p_gesture = malloc( sizeof(*p_gesture) );
    if( unlikely(!p_gesture) )
        return NULL;

    result = SetGestureConfig(
            hwnd,
            0,
            1,
            &config,
            sizeof( GESTURECONFIG )
            );
    if (result == 0)
    {
        free(p_gesture);
        return NULL;
    }
    if (b_isProjected)
        p_gesture->DecodeGestureImpl = DecodeGestureProjection;
    else
        p_gesture->DecodeGestureImpl = DecodeGestureAction;

    p_gesture->i_type     = 0;
    p_gesture->b_2fingers = false;
    p_gesture->i_action   = GESTURE_ACTION_UNDEFINED;
    p_gesture->i_beginx   = p_gesture->i_beginy = -1;
    p_gesture->i_lasty    = -1;

    return p_gesture;
}

void CloseGestures( struct win32_gesture_sys_t *p_gesture )
{
    free( p_gesture );
}
