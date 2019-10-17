/*****************************************************************************
 * smb_common.h: common strings used by SMB and DSM modules
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
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

#define SMB_USER_TEXT N_("Username")
#define SMB_USER_LONGTEXT N_("Username that will be used for the connection, " \
        "if no username is set in the URL.")
#define SMB_PASS_TEXT N_("Password")
#define SMB_PASS_LONGTEXT N_("Password that will be used for the connection, " \
        "if no username or password are set in URL.")
#define SMB_DOMAIN_TEXT N_("SMB domain")
#define SMB_DOMAIN_LONGTEXT N_("Domain/Workgroup that " \
        "will be used for the connection.")

#define SMB1_LOGIN_DIALOG_TITLE N_( "SMBv1 authentication required" )

#define SMB_LOGIN_DIALOG_TITLE N_( "SMB authentication required" )
#define SMB_LOGIN_DIALOG_TEXT N_( "The computer (%s) you are trying to connect "   \
    "to requires authentication.\nPlease provide a username (ideally a "  \
    "domain name using the format DOMAIN;username) and a password." )
