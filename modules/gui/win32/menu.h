/*****************************************************************************
 * menu.h: prototypes for menu functions
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: menu.h,v 1.2 2002/12/13 03:52:58 videolan Exp $
 *
 * Authors: Olivier Teuliere <ipkiss@via.ecp.fr>
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

#ifndef menuH
#define menuH
//----------------------------------------------------------------------------
class TMenusGen : public TObject
{
private:
    intf_thread_t *p_intf;

    /* local pointers to main window menu items */
    TMenuItem *MenuAudio;
    TMenuItem *PopupAudio;
    TMenuItem *MenuSubtitles;
    TMenuItem *PopupSubtitles;
    TMenuItem *MenuProgram;
    TMenuItem *PopupProgram;
    TMenuItem *MenuTitle;
    TMenuItem *MenuChapter;
    TMenuItem *PopupNavigation;

    /* Language information */
    es_descriptor_t   * p_audio_es_old;
    es_descriptor_t   * p_spu_es_old;

    /* Helpful functions */
    int Item2Index( TMenuItem *Root, TMenuItem *Item );
    TMenuItem *Index2Item( TMenuItem *Root, int i_index, bool SingleColumn );
    int __fastcall Data2Title( int data );
    int __fastcall Data2Chapter( int data );
    int __fastcall Pos2Data( int title, int chapter );

    void __fastcall LangChange( TMenuItem *, TMenuItem *, TMenuItem *, int );
    void __fastcall ProgramChange( TMenuItem *, TMenuItem * );
    void __fastcall ProgramMenu( TMenuItem *, pgrm_descriptor_t *,
                                 TNotifyEvent );
    void __fastcall RadioMenu( TMenuItem *, AnsiString, int, int,
                               TNotifyEvent );
    void __fastcall LanguageMenu( TMenuItem *, es_descriptor_t *, int,
                                  TNotifyEvent );
    void __fastcall NavigationMenu( TMenuItem *, TNotifyEvent );
public:
    __fastcall TMenusGen( intf_thread_t *_p_intf );

    /* menu generation */
    void __fastcall SetupMenus();

    /* callbacks for menuitems */
    void __fastcall MenuAudioClick( TObject *Sender );
    void __fastcall PopupAudioClick( TObject *Sender );
    void __fastcall MenuSubtitleClick( TObject *Sender );
    void __fastcall PopupSubtitleClick( TObject *Sender );
    void __fastcall MenuProgramClick( TObject *Sender );
    void __fastcall PopupProgramClick( TObject *Sender );
    void __fastcall MenuTitleClick( TObject *Sender );
    void __fastcall MenuChapterClick( TObject *Sender );
    void __fastcall PopupNavigationClick( TObject *Sender );
};
//----------------------------------------------------------------------------
#endif
