/*****************************************************************************
 * mainframe.h: Prototype for main window
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
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


#ifndef mainframeH
#define mainframeH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ComCtrls.hpp>
#include <Dialogs.hpp>
#include <ImgList.hpp>
#include <Menus.hpp>
#include <ToolWin.hpp>
#include <AppEvnts.hpp>
#include <ExtCtrls.hpp>
//---------------------------------------------------------------------------
class TMainFrameDlg : public TForm
{
__published:	// IDE-managed Components
    TToolBar *ToolBar1;
    TToolButton *ToolButtonFile;
    TToolButton *ToolButtonDisc;
    TToolButton *ToolButtonNet;
    TToolButton *ToolButtonSep1;
    TToolButton *ToolButtonBack;
    TToolButton *ToolButtonStop;
    TToolButton *ToolButtonPlay;
    TToolButton *ToolButtonPause;
    TToolButton *ToolButtonSlow;
    TToolButton *ToolButtonFast;
    TToolButton *ToolButtonSep2;
    TToolButton *ToolButtonPlaylist;
    TToolButton *ToolButtonPrev;
    TToolButton *ToolButtonNext;
    TMainMenu *MainMenu1;
    TMenuItem *MenuFile;
    TMenuItem *MenuOpenFile;
    TMenuItem *MenuOpenDisc;
    TMenuItem *MenuNetworkStream;
    TMenuItem *N1;
    TMenuItem *MenuExit;
    TMenuItem *MenuView;
    TMenuItem *MenuHideinterface;
    TMenuItem *MenuFullscreen;
    TMenuItem *N2;
    TMenuItem *MenuTitle;
    TMenuItem *MenuChapter;
    TMenuItem *MenuAngle;
    TMenuItem *N3;
    TMenuItem *MenuPlaylist;
    TMenuItem *MenuModules;
    TMenuItem *MenuMessages;
    TMenuItem *MenuSettings;
    TMenuItem *MenuAudio;
    TMenuItem *MenuSubtitles;
    TMenuItem *N4;
    TMenuItem *MenuPreferences;
    TMenuItem *MenuHelp;
    TMenuItem *MenuAbout;
    TOpenDialog *OpenDialog1;
    TImageList *ImageListToolbar;
    TPopupMenu *PopupMenuMain;
    TMenuItem *PopupPlay;
    TMenuItem *PopupPause;
    TMenuItem *PopupStop;
    TMenuItem *PopupBack;
    TMenuItem *PopupSlow;
    TMenuItem *PopupFast;
    TMenuItem *N5;
    TMenuItem *PopupToggleInterface;
    TMenuItem *PopupFullscreen;
    TMenuItem *N6;
    TMenuItem *PopupNext;
    TMenuItem *PopupPrev;
    TMenuItem *PopupJump;
    TMenuItem *PopupNavigation;
    TMenuItem *PopupProgram;
    TMenuItem *PopupAudio;
    TMenuItem *PopupSubtitles;
    TMenuItem *PopupFile;
    TMenuItem *PopupPlaylist;
    TMenuItem *PopupPreferences;
    TMenuItem *N7;
    TMenuItem *PopupExit;
    TToolButton *ToolButtonEject;
    TStatusBar *StatusBar;
    TGroupBox *GroupBoxFile;
    TLabel *LabelFileName;
    TGroupBox *GroupBoxNetwork;
    TEdit *EditChannel;
    TUpDown *UpDownChannel;
    TLabel *LabelChannel;
    TLabel *LabelServer;
    TGroupBox *GroupBoxDisc;
    TMenuItem *N8;
    TMenuItem *MenuEjectDisc;
    TMenuItem *MenuProgram;
    TLabel *LabelDisc;
    TLabel *LabelTitle;
    TButton *ButtonTitlePrev;
    TButton *ButtonTitleNext;
    TButton *ButtonChapterPrev;
    TButton *ButtonChapterNext;
    TLabel *LabelChapter;
    TLabel *LabelTitleCurrent;
    TLabel *LabelChapterCurrent;
    TButton *ButtonGo;
    TGroupBox *GroupBoxSlider;
    TTrackBar *TrackBar;
    TTimer *TimerManage;
    TMenuItem *PopupOpenFile;
    TMenuItem *PopupOpenDisc;
    TMenuItem *PopupNetworkStream;
    TMenuItem *PopupClose;
    TMenuItem *N9;
    void __fastcall TimerManageTimer( TObject *Sender );
    void __fastcall TrackBarChange( TObject *Sender );
    void __fastcall FormClose( TObject *Sender, TCloseAction &Action );
    void __fastcall MenuOpenFileClick( TObject *Sender );
    void __fastcall MenuOpenDiscClick( TObject *Sender );
    void __fastcall MenuNetworkStreamClick( TObject *Sender );
    void __fastcall MenuExitClick( TObject *Sender );
    void __fastcall MenuFullscreenClick( TObject *Sender );
    void __fastcall MenuPlaylistClick( TObject *Sender );
    void __fastcall MenuMessagesClick( TObject *Sender );
    void __fastcall MenuPreferencesClick( TObject *Sender );
    void __fastcall MenuAboutClick( TObject *Sender );
    void __fastcall ToolButtonFileClick( TObject *Sender );
    void __fastcall ToolButtonDiscClick( TObject *Sender );
    void __fastcall ToolButtonNetClick( TObject *Sender );
    void __fastcall ToolButtonPlaylistClick( TObject *Sender );
    void __fastcall ToolButtonBackClick( TObject *Sender );
    void __fastcall ToolButtonStopClick( TObject *Sender );
    void __fastcall ToolButtonPlayClick( TObject *Sender );
    void __fastcall ToolButtonPauseClick( TObject *Sender );
    void __fastcall ToolButtonSlowClick( TObject *Sender );
    void __fastcall ToolButtonFastClick( TObject *Sender );
    void __fastcall ToolButtonPrevClick( TObject *Sender );
    void __fastcall ToolButtonNextClick( TObject *Sender );
    void __fastcall ToolButtonEjectClick( TObject *Sender );
    void __fastcall PopupCloseClick( TObject *Sender );
    void __fastcall PopupPlayClick( TObject *Sender );
    void __fastcall PopupPauseClick( TObject *Sender );
    void __fastcall PopupStopClick( TObject *Sender );
    void __fastcall PopupBackClick( TObject *Sender );
    void __fastcall PopupSlowClick( TObject *Sender );
    void __fastcall PopupFastClick( TObject *Sender );
    void __fastcall PopupToggleInterfaceClick( TObject *Sender );
    void __fastcall PopupFullscreenClick( TObject *Sender );
    void __fastcall PopupNextClick( TObject *Sender );
    void __fastcall PopupPrevClick( TObject *Sender );
    void __fastcall PopupJumpClick( TObject *Sender );
    void __fastcall PopupPlaylistClick( TObject *Sender );
    void __fastcall PopupPreferencesClick( TObject *Sender );
    void __fastcall PopupExitClick( TObject *Sender );
    void __fastcall PopupOpenFileClick( TObject *Sender );
    void __fastcall PopupOpenDiscClick( TObject *Sender );
    void __fastcall PopupNetworkStreamClick( TObject *Sender );
    void __fastcall ButtonTitlePrevClick( TObject *Sender );
    void __fastcall ButtonTitleNextClick( TObject *Sender );
    void __fastcall ButtonChapterPrevClick( TObject *Sender );
    void __fastcall ButtonChapterNextClick( TObject *Sender );
    void __fastcall ButtonGoClick( TObject *Sender );
private:	// User declarations
public:		// User declarations
    __fastcall TMainFrameDlg( TComponent* Owner );
    void __fastcall DisplayHint( TObject *Sender );
    void __fastcall ModeManage();
    void __fastcall MenuProgramClick( TObject *Sender );
    void __fastcall MenuAudioClick( TObject *Sender );
    void __fastcall MenuSubtitleClick( TObject *Sender );
    void __fastcall MenuTitleClick( TObject *Sender );
    void __fastcall MenuChapterClick( TObject *Sender );
    void __fastcall PopupProgramClick( TObject *Sender );
    void __fastcall PopupAudioClick( TObject *Sender );
    void __fastcall PopupSubtitleClick( TObject *Sender );
    void __fastcall PopupNavigationClick( TObject *Sender );
};
//---------------------------------------------------------------------------
#endif
