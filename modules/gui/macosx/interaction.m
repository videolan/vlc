/*****************************************************************************
 * interaction.h: Mac OS X interaction dialogs
 *****************************************************************************
 * Copyright (C) 2005-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
 *          Felix Kühne <fkuehne at videolan dot org>
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

#import "intf.h"
#import "interaction.h"
#import "misc.h"

/* for the icons in our custom error panel */
#import <ApplicationServices/ApplicationServices.h>

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

    o_error_panel = [[VLCErrorInteractionPanel alloc] init];

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

-(id)getErrorPanel
{
    return o_error_panel;
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
    id o_window = NULL;
    if( !p_dialog )
        msg_Err( p_intf, "no available interaction framework" );

    if( !nib_interact_loaded )
    {
        nib_interact_loaded = [NSBundle loadNibNamed:@"Interaction" owner:self];
        [o_prog_cancel_btn setTitle: _NS("Cancel")];
        [o_prog_bar setUsesThreadedAnimation: YES];
        [o_auth_login_txt setStringValue: _NS("Login:")];
        [o_auth_pw_txt setStringValue: _NS("Password:")];
        [o_auth_cancel_btn setTitle: _NS("Cancel")];
        [o_auth_ok_btn setTitle: _NS("OK")];
        [o_input_ok_btn setTitle: _NS("OK")];
        [o_input_cancel_btn setTitle: _NS("Cancel")];
        o_mainIntfPgbar = [[VLCMain sharedInstance] getMainIntfPgbar];
    }

    NSString *o_title = [NSString stringWithUTF8String:p_dialog->psz_title ? p_dialog->psz_title : _("Error")];
    NSString *o_description = [NSString stringWithUTF8String:p_dialog->psz_description ? p_dialog->psz_description : ""];
    NSString *o_defaultButton = p_dialog->psz_default_button ? [NSString stringWithUTF8String:p_dialog->psz_default_button] : nil;
    NSString *o_alternateButton = p_dialog->psz_alternate_button ? [NSString stringWithUTF8String:p_dialog->psz_alternate_button] : nil;
    NSString *o_otherButton = p_dialog->psz_other_button ? [NSString stringWithUTF8String:p_dialog->psz_other_button] : nil;

    vout_thread_t *p_vout = vlc_object_find( VLCIntf, VLC_OBJECT_VOUT, FIND_ANYWHERE );
    if( p_vout != NULL )
    {
        NSEnumerator * o_enum = [[NSApp orderedWindows] objectEnumerator];

        while( ( o_window = [o_enum nextObject] ) )
        {
            if( [[o_window className] isEqualToString: @"VLCVoutWindow"] )
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

#if 0
    msg_Dbg( p_intf, "Title: %s", [o_title UTF8String] );
    msg_Dbg( p_intf, "Description: %s", [o_description UTF8String] );
    msg_Dbg( p_intf, "Delivered flag: %i", p_dialog->i_flags );
#endif

    if( p_dialog->i_flags & DIALOG_BLOCKING_ERROR )
    {
        msg_Dbg( p_intf, "error panel requested" );
        NSBeginInformationalAlertSheet( o_title, _NS("OK"), nil, nil,
            o_window, self, @selector(sheetDidEnd: returnCode: contextInfo:),
            NULL, nil, o_description );
    }
    else if( p_dialog->i_flags & DIALOG_NONBLOCKING_ERROR )
    {
        msg_Dbg( p_intf, "addition to non-blocking error panel received" );
        [[[[VLCMain sharedInstance] getInteractionList] getErrorPanel]
        addError: o_title withMsg: o_description];
    }
    else if( p_dialog->i_flags & DIALOG_WARNING )
    {
        msg_Dbg( p_intf, "addition to non-blocking warning panel received" );
        [[[[VLCMain sharedInstance] getInteractionList] getErrorPanel]
            addWarning: o_title withMsg: o_description];
    }
    else if( p_dialog->i_flags & DIALOG_YES_NO_CANCEL )
    {
        msg_Dbg( p_intf, "yes-no-cancel-dialog requested" );
        NSBeginInformationalAlertSheet( o_title, o_defaultButton,
            o_alternateButton, o_otherButton, o_window, self,
            @selector(sheetDidEnd: returnCode: contextInfo:), NULL, nil,
            o_description );
    }
    else if( p_dialog->i_flags & DIALOG_LOGIN_PW_OK_CANCEL )
    {
        msg_Dbg( p_intf, "dialog for login and pw requested" );
        [o_auth_title setStringValue: o_title];
        [o_auth_description setStringValue: o_description];
        [o_auth_login_fld setStringValue: @""];
        [o_auth_pw_fld setStringValue: @""];
        [NSApp beginSheet: o_auth_win modalForWindow: o_window
            modalDelegate: self didEndSelector: nil contextInfo: nil];
        [o_auth_win makeKeyWindow];
    }
    else if( p_dialog->i_flags & DIALOG_USER_PROGRESS )
    {
        msg_Dbg( p_intf, "user progress dialog requested" );
        [o_prog_title setStringValue: o_title];
        [o_prog_description setStringValue: o_description];
        [o_prog_bar setDoubleValue: (double)p_dialog->val.f_float];
        if( p_dialog->i_timeToGo < 1 )
            [o_prog_timeToGo setStringValue: @""];
        else
            [o_prog_timeToGo setStringValue: [NSString stringWithFormat:
                _NS("Remaining time: %i seconds"), p_dialog->i_timeToGo]];
        [NSApp beginSheet: o_prog_win modalForWindow: o_window
            modalDelegate: self didEndSelector: nil contextInfo: nil];
        [o_prog_win makeKeyWindow];
    }
    else if( p_dialog->i_flags & DIALOG_PSZ_INPUT_OK_CANCEL )
    {
        msg_Dbg( p_intf, "text input from user requested" );
        [o_input_title setStringValue: o_title];
        [o_input_description setStringValue: o_description];
        [o_input_fld setStringValue: @""];
        [NSApp beginSheet: o_input_win modalForWindow: o_window
            modalDelegate: self didEndSelector: nil contextInfo: nil];
        [o_input_win makeKeyWindow];
    }
    else if( p_dialog->i_flags & DIALOG_INTF_PROGRESS )
    {
        msg_Dbg( p_intf, "progress-bar in main intf requested" );
        [[VLCMain sharedInstance] setScrollField: o_description stopAfter: -1];
        [o_mainIntfPgbar setDoubleValue: (double)p_dialog->val.f_float];
        [o_mainIntfPgbar setHidden: NO];
        [[[VLCMain sharedInstance] getControllerWindow] makeKeyWindow];
        [o_mainIntfPgbar setIndeterminate: NO];
    }
    else
        msg_Err( p_intf, "requested dialog type unknown (%i)", p_dialog->i_flags );
}

- (void)sheetDidEnd:(NSWindow *)o_sheet returnCode:(int)i_return
    contextInfo:(void *)o_context
{
    vlc_object_lock( (vlc_object_t *)(p_dialog->p_interaction) );
    if( i_return == NSAlertDefaultReturn )
    {
        p_dialog->i_return = DIALOG_OK_YES;
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
    vlc_object_unlock( (vlc_object_t *)(p_dialog->p_interaction) );
}

-(void)updateDialog
{
    if( p_dialog->i_flags & DIALOG_USER_PROGRESS )
    {
        [o_prog_description setStringValue:
            [NSString stringWithUTF8String: p_dialog->psz_description]];
        [o_prog_bar setDoubleValue: (double)p_dialog->val.f_float];

        if( [o_prog_bar doubleValue] == 100.0 )
        {
            /* we are done, let's hide */
            [self hideDialog];
        }

        if( p_dialog->i_timeToGo < 1 )
            [o_prog_timeToGo setStringValue: @""];
        else
            [o_prog_timeToGo setStringValue: [NSString stringWithFormat:
                    _NS("Remaining time: %i seconds"), p_dialog->i_timeToGo]];

        return;
    }
    if( p_dialog->i_flags & DIALOG_INTF_PROGRESS )
    {
        [[VLCMain sharedInstance] setScrollField:
            [NSString stringWithUTF8String: p_dialog->psz_description]
            stopAfter: -1];
        [o_mainIntfPgbar setDoubleValue: (double)p_dialog->val.f_float];

        if( [o_mainIntfPgbar doubleValue] == 100.0 )
        {
            /* we are done, let's hide */
            [self hideDialog];
        }
        return;
    }
}

-(void)hideDialog
{
    msg_Dbg( p_intf, "hide event %p", self );
    if( p_dialog->i_flags & DIALOG_USER_PROGRESS )
    {
        if([o_prog_win isVisible])
        {
            [NSApp endSheet: o_prog_win];
            [o_prog_win close];
        }
    }
    if( p_dialog->i_flags & DIALOG_LOGIN_PW_OK_CANCEL )
    {
        if([o_auth_win isVisible])
        {
            [NSApp endSheet: o_auth_win];
            [o_auth_win close];
        }
    }
    if( p_dialog->i_flags & DIALOG_PSZ_INPUT_OK_CANCEL )
    {
        if([o_input_win isVisible])
        {
            [NSApp endSheet: o_input_win];
            [o_input_win close];
        }
    }
    if( p_dialog->i_flags & DIALOG_INTF_PROGRESS )
    {
        [o_mainIntfPgbar setIndeterminate: YES];
        [o_mainIntfPgbar setHidden: YES];
        [[VLCMain sharedInstance] resetScrollField];
    }
}

-(void)destroyDialog
{
    msg_Dbg( p_intf, "destroy event" );
    if( o_mainIntfPgbar )
        [o_mainIntfPgbar release];
}

- (IBAction)cancelAndClose:(id)sender
{
    /* tell the core that the dialog was cancelled in a yes/no-style dialogue */
    vlc_object_lock( (vlc_object_t *)(p_dialog->p_interaction) );
    p_dialog->i_return = DIALOG_CANCELLED;
    p_dialog->i_status = ANSWERED_DIALOG;
    vlc_object_unlock( (vlc_object_t *)(p_dialog->p_interaction) );
    msg_Dbg( p_intf, "dialog cancelled" );
}

- (IBAction)cancelDialog:(id)sender
{
    /* tell core that the user wishes to cancel the dialogue
     * Use this function if cancelling is optionally like in the progress-dialogue */
    vlc_object_lock( (vlc_object_t *)(p_dialog->p_interaction) );
    p_dialog->b_cancelled = true;
    vlc_object_unlock( (vlc_object_t *)(p_dialog->p_interaction) );
    msg_Dbg( p_intf, "cancelling dialog, will close it later on" );
}

- (IBAction)okayAndClose:(id)sender
{
    msg_Dbg( p_intf, "running okayAndClose" );
    vlc_object_lock( (vlc_object_t *)(p_dialog->p_interaction) );
    if( p_dialog->i_flags == DIALOG_LOGIN_PW_OK_CANCEL )
    {
        p_dialog->psz_returned[0] = strdup( [[o_auth_login_fld stringValue] UTF8String] );
        p_dialog->psz_returned[1] = strdup( [[o_auth_pw_fld stringValue] UTF8String] );
    }
    else if( p_dialog->i_flags == DIALOG_PSZ_INPUT_OK_CANCEL )
        p_dialog->psz_returned[0] = strdup( [[o_input_fld stringValue] UTF8String] );
    p_dialog->i_return = DIALOG_OK_YES;
    p_dialog->i_status = ANSWERED_DIALOG;
    vlc_object_unlock( (vlc_object_t *)(p_dialog->p_interaction) );
    msg_Dbg( p_intf, "dialog acknowledged" );
}

@end

/*****************************************************************************
 * VLCErrorInteractionPanel implementation
 *****************************************************************************/
@implementation VLCErrorInteractionPanel
-(id)init
{
    [super init];

    /* load the nib */
    nib_interact_errpanel_loaded = [NSBundle loadNibNamed:@"InteractionErrorPanel" owner:self];

    /* init strings */
    [o_window setTitle: _NS("Errors and Warnings")];
    [o_cleanup_button setTitle: _NS("Clean up")];
    [o_messages_btn setTitle: _NS("Show Details")];

    /* init data sources */
    o_errors = [[NSMutableArray alloc] init];
    o_icons = [[NSMutableArray alloc] init];

    return self;
}

-(void)dealloc
{
    [o_errors release];
    [o_icons release];
    [super dealloc];
}

-(void)showPanel
{
    [o_window makeKeyAndOrderFront: self];
}

-(void)addError: (NSString *)o_error withMsg:(NSString *)o_msg
{
    /* format our string as desired */
    NSMutableAttributedString * ourError;
    ourError = [[NSMutableAttributedString alloc] initWithString:
        [NSString stringWithFormat:@"%@\n%@", o_error, o_msg]
        attributes:
        [NSDictionary dictionaryWithObject: [NSFont systemFontOfSize:11] forKey: NSFontAttributeName]];
    [ourError
        addAttribute: NSFontAttributeName
        value: [NSFont boldSystemFontOfSize:11]
        range: NSMakeRange( 0, [o_error length])];
    [o_errors addObject: ourError];
    [ourError release];

    [o_icons addObject: [NSImage imageWithErrorIcon]];

    [o_error_table reloadData];
}

-(void)addWarning: (NSString *)o_warning withMsg:(NSString *)o_msg
{
    /* format our string as desired */
    NSMutableAttributedString * ourWarning;
    ourWarning = [[NSMutableAttributedString alloc] initWithString:
        [NSString stringWithFormat:@"%@\n%@", o_warning, o_msg]
        attributes:
        [NSDictionary dictionaryWithObject: [NSFont systemFontOfSize:11] forKey: NSFontAttributeName]];
    [ourWarning
        addAttribute: NSFontAttributeName
        value: [NSFont boldSystemFontOfSize:11]
        range: NSMakeRange( 0, [o_warning length])];
    [o_errors addObject: ourWarning];
    [ourWarning release];

    [o_icons addObject: [NSImage imageWithWarningIcon]];
 
    [o_error_table reloadData];
}

-(IBAction)cleanupTable:(id)sender
{
    [o_errors removeAllObjects];
    [o_icons removeAllObjects];
    [o_error_table reloadData];
}

-(IBAction)showMessages:(id)sender
{
    [[VLCMain sharedInstance] showMessagesPanel: sender];
}

/*----------------------------------------------------------------------------
 * data source methods
 *---------------------------------------------------------------------------*/
- (NSInteger)numberOfRowsInTableView:(NSTableView *)theDataTable
{
    return [o_errors count];
}

- (id)tableView:(NSTableView *)theDataTable objectValueForTableColumn:
    (NSTableColumn *)theTableColumn row: (NSInteger)row
{
    if( [[theTableColumn identifier] isEqualToString: @"error_msg"] )
        return [o_errors objectAtIndex: row];

    if( [[theTableColumn identifier] isEqualToString: @"icon"] )
        return [o_icons objectAtIndex: row];

    return @"unknown identifier";
}

@end
