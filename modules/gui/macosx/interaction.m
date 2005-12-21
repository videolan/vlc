/*****************************************************************************
 * interaction.h: Mac OS X interaction dialogs
 *****************************************************************************
 * Copyright (C) 2001-2005 the VideoLAN team
 * $Id: vout.h 13803 2005-12-18 18:54:28Z bigben $
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
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

#include "intf.h"
#import <interaction.h>

/*****************************************************************************
 * VLCInteractionList implementation
 *****************************************************************************/
@implementation VLCInteractionList

-(id)init
{
    [super init];
    o_interaction_list = [[NSMutableArray alloc] initWithCapacity:1];
    [[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(newInteractionEvent:)
        name: @"VLCNewInteractionEventNotification"
        object:self];

    return self;
}

-(void)newInteractionEvent: (NSNotification *)o_notification
{
    VLCInteraction *o_interaction;
    NSValue *o_value = [[o_notification userInfo] objectForKey:@"VLCDialogPointer"];
    interaction_dialog_t *p_dialog = [o_value pointerValue];

    switch( p_dialog->i_action )
    {
    case INTERACT_NEW:
        [self addInteraction: p_dialog];
        break;
    case INTERACT_UPDATE:
        o_interaction = (VLCInteraction *)p_dialog->p_private;
        [o_interaction updateDialog];
        break;
    case INTERACT_HIDE:
        o_interaction = (VLCInteraction *)p_dialog->p_private;
        [o_interaction hideDialog];
        break;
    case INTERACT_DESTROY:
        o_interaction = (VLCInteraction *)p_dialog->p_private;
        [o_interaction destroyDialog];
        [self removeInteraction:o_interaction];
        p_dialog->i_status = DESTROYED_DIALOG;
        break;
    }
}

-(void)addInteraction: (interaction_dialog_t *)p_dialog
{

    VLCInteraction *o_interaction = [[VLCInteraction alloc] initDialog: p_dialog];
    
    p_dialog->p_private = (void *)o_interaction;
    [o_interaction_list addObject:[o_interaction autorelease]];
    [o_interaction runDialog];
}

-(void)removeInteraction: (VLCInteraction *)o_interaction
{
    [o_interaction_list removeObject:o_interaction];
}

-(void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [o_interaction_list removeAllObjects];
    [o_interaction_list release];
    [super dealloc];
}

@end

/*****************************************************************************
 * VLCInteraction implementation
 *****************************************************************************/
@implementation VLCInteraction

-(id)initDialog: (interaction_dialog_t *)_p_dialog
{
    [super init];
    p_dialog = _p_dialog;
    return self;
}

-(void)runDialog
{
    int i = 0;
    id o_window = NULL;
    if( !p_dialog )
        NSLog( @"serious issue" );

    NSString *o_title = [NSString stringWithUTF8String:p_dialog->psz_title ? p_dialog->psz_title : "title"];
    NSString *o_description = [NSString stringWithUTF8String:p_dialog->psz_description ? p_dialog->psz_description : "desc"];
    
    vout_thread_t *p_vout = vlc_object_find( VLCIntf, VLC_OBJECT_VOUT, FIND_ANYWHERE );
    if( p_vout != NULL )
    {
        NSEnumerator * o_enum = [[NSApp orderedWindows] objectEnumerator];

        while( ( o_window = [o_enum nextObject] ) )
        {
            if( [[o_window className] isEqualToString: @"VLCWindow"] )
            {
                vlc_object_release( (vlc_object_t *)p_vout );
                break;
            }
        }
        vlc_object_release( (vlc_object_t *)p_vout );
    }
    else
    {
        o_window = [NSApp mainWindow];
    }
    
    NSLog( @"Title: %@", o_title );
    NSLog( @"Description: %@", o_description );
    if( p_dialog->i_id == DIALOG_ERRORS )
    {
        for( i = 0; i < p_dialog->i_widgets; i++ )
        {
            NSLog( @"Error: %@", [NSString stringWithUTF8String: p_dialog->pp_widgets[i]->psz_text] );
        }
    }
    else
    {
        for( i = 0; i < p_dialog->i_widgets; i++ )
        {
            NSLog( @"widget: %@", [NSString stringWithUTF8String: p_dialog->pp_widgets[i]->psz_text] );
        }
        if( p_dialog->i_flags & DIALOG_OK_CANCEL )
        {
            NSBeginInformationalAlertSheet( o_title, @"OK" , @"Cancel", nil, o_window, self,
                @selector(sheetDidEnd: returnCode: contextInfo:), NULL, nil, o_description );
        }
        else if( p_dialog->i_flags & DIALOG_YES_NO_CANCEL )
        {
            NSBeginInformationalAlertSheet( o_title, @"Yes" , @"No", @"Cancel", o_window, self,
                @selector(sheetDidEnd: returnCode: contextInfo:), NULL, nil, o_description );
        }
        else
            NSLog( @"not implemented yet" );
    }
}

- (void)sheetDidEnd:(NSWindow *)o_sheet returnCode:(int)i_return
    contextInfo:(void *)o_context
{
    vlc_mutex_lock( &p_dialog->p_interaction->object_lock );
    if( i_return == NSAlertDefaultReturn )
    {
        p_dialog->i_return = DIALOG_OK_YES;
    }
    else if( i_return == NSAlertAlternateReturn && ( p_dialog->i_flags & DIALOG_OK_CANCEL ) )
    {
        p_dialog->i_return = DIALOG_CANCELLED;
    }
    else if( i_return == NSAlertAlternateReturn )
    {
        p_dialog->i_return = DIALOG_NO;
    }
    else if( i_return == NSAlertOtherReturn )
    {
        p_dialog->i_return = DIALOG_CANCELLED;
    }
    p_dialog->i_status = ANSWERED_DIALOG;
    vlc_mutex_unlock( &p_dialog->p_interaction->object_lock );
}

-(void)updateDialog
{
    NSLog( @"update event" );
}

-(void)hideDialog
{
    NSLog( @"hide event" );
}

-(void)destroyDialog
{
    NSLog( @"destroy event" );
}

-(void)dealloc
{
    [super dealloc];
}

@end