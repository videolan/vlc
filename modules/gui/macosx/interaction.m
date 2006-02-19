/*****************************************************************************
 * interaction.h: Mac OS X interaction dialogs
 *****************************************************************************
 * Copyright (C) 2005-2006 the VideoLAN team
 * $Id: vout.h 13803 2005-12-18 18:54:28Z bigben $
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
 *          Felix KŸhne <fkuehne at videolan dot org>
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

#include "intf.h"
#import "interaction.h"

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
    p_intf = VLCIntf;
    [super init];
    p_dialog = _p_dialog;
    return self;
}

-(void)runDialog
{
    int i = 0;
    id o_window = NULL;
    if( !p_dialog )
        msg_Err( p_intf, "serious issue (p_dialog == nil)" );

    if( !nib_interact_loaded )
        nib_interact_loaded = [NSBundle loadNibNamed:@"Interaction" owner:self];

    NSString *o_title = [NSString stringWithUTF8String:p_dialog->psz_title ? p_dialog->psz_title : "title"];
    NSString *o_description = [NSString stringWithUTF8String:p_dialog->psz_description ? p_dialog->psz_description : ""];
    
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
    
    msg_Dbg( p_intf, "Title: %s", [o_title UTF8String] );
    msg_Dbg( p_intf, "Description: %s", [o_description UTF8String] );
    if( p_dialog->i_id == DIALOG_ERRORS )
    {
        for( i = 0; i < p_dialog->i_widgets; i++ )
        {
            msg_Err( p_intf, "Error: %s", p_dialog->pp_widgets[i]->psz_text );
        }
    }
    else
    {
        for( i = 0; i < p_dialog->i_widgets; i++ )
        {
            msg_Dbg( p_intf, "widget: %s", p_dialog->pp_widgets[i]->psz_text );
            o_description = [o_description stringByAppendingString: \
                [NSString stringWithUTF8String: \
                    p_dialog->pp_widgets[i]->psz_text]];
        }
        if( p_dialog->i_flags & DIALOG_OK_CANCEL )
        {
            NSBeginInformationalAlertSheet( o_title, @"OK" , @"Cancel", nil, \
                o_window, self,@selector(sheetDidEnd: returnCode: contextInfo:),\
                NULL, nil, o_description );
        }
        else if( p_dialog->i_flags & DIALOG_YES_NO_CANCEL )
        {
            NSBeginInformationalAlertSheet( o_title, @"Yes", @"Cancel", @"No", \
                o_window, self,@selector(sheetDidEnd: returnCode: contextInfo:),\
                NULL, nil, o_description );
        }
        else if( p_dialog->i_type & WIDGET_PROGRESS )
        {
            [o_prog_title setStringValue: o_title];
            [o_prog_description setStringValue: o_description];
            [o_prog_bar setUsesThreadedAnimation: YES];
            [o_prog_bar setDoubleValue: 0];
            [NSApp beginSheet: o_prog_win modalForWindow: o_window \
                modalDelegate: self didEndSelector: \
                nil \
                contextInfo: nil];
            [o_prog_win makeKeyWindow];
        }
        else
            msg_Warn( p_intf, "requested dialog type not implemented yet" );
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
    int i = 0;
    for( i = 0 ; i< p_dialog->i_widgets; i++ )
    {
        /*msg_Dbg( p_intf, "update event, current value %i for index %i",
        (int)(p_dialog->pp_widgets[i]->val.f_float), i);*/
        if( p_dialog->i_type & WIDGET_PROGRESS )
            [o_prog_bar setDoubleValue: \
                (double)(p_dialog->pp_widgets[i]->val.f_float)];
    }
}

-(void)hideDialog
{
    msg_Dbg( p_intf, "hide event" );
    if( p_dialog->i_type & WIDGET_PROGRESS )
    {
        [NSApp endSheet: o_prog_win];
        [o_prog_win close];
    }
}

-(void)destroyDialog
{
    msg_Dbg( p_intf, "destroy event" );
}

- (IBAction)cancelAndClose:(id)sender
{
    /* tell the core that the dialog was cancelled */
    vlc_mutex_lock( &p_dialog->p_interaction->object_lock );
    p_dialog->i_return = DIALOG_CANCELLED;
    p_dialog->i_status = ANSWERED_DIALOG;
    vlc_mutex_unlock( &p_dialog->p_interaction->object_lock );
    msg_Dbg( p_intf, "dialog cancelled" );
}

@end
