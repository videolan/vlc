object PlaylistDlg: TPlaylistDlg
  Left = 447
  Top = 197
  Width = 335
  Height = 436
  Caption = 'Playlist'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  Menu = MainMenuPlaylist
  OldCreateOrder = False
  OnHide = FormHide
  OnShow = FormShow
  PixelsPerInch = 96
  TextHeight = 13
  object BitBtnOk: TBitBtn
    Left = 76
    Top = 356
    Width = 176
    Height = 25
    Anchors = [akBottom]
    Caption = 'OK'
    ModalResult = 1
    TabOrder = 0
    OnClick = BitBtnOkClick
  end
  object ListViewPlaylist: TListView
    Left = 11
    Top = 10
    Width = 305
    Height = 331
    Anchors = [akTop, akBottom]
    Columns = <
      item
        Caption = 'Filename'
        Width = 200
      end
      item
        Alignment = taCenter
        Caption = 'Duration'
        Width = 100
      end>
    HideSelection = False
    MultiSelect = True
    ReadOnly = True
    RowSelect = True
    PopupMenu = PopupMenuPlaylist
    TabOrder = 1
    ViewStyle = vsReport
    OnCustomDrawItem = ListViewPlaylistCustomDrawItem
    OnDblClick = ListViewPlaylistDblClick
    OnKeyDown = ListViewPlaylistKeyDown
  end
  object MainMenuPlaylist: TMainMenu
    Left = 24
    Top = 352
    object MenuAdd: TMenuItem
      Caption = '&Add'
      object MenuAddFile: TMenuItem
        Caption = '&File'
        OnClick = MenuAddFileClick
      end
      object MenuAddDisc: TMenuItem
        Caption = '&Disc'
        OnClick = MenuAddDiscClick
      end
      object MenuAddNet: TMenuItem
        Caption = '&Network'
        OnClick = MenuAddNetClick
      end
      object MenuAddUrl: TMenuItem
        Caption = '&Url'
        Enabled = False
        OnClick = MenuAddUrlClick
      end
    end
    object MenuDelete: TMenuItem
      Caption = '&Delete'
      object MenuDeleteAll: TMenuItem
        Caption = '&All'
        OnClick = MenuDeleteAllClick
      end
      object MenuDeleteSelected: TMenuItem
        Caption = '&Selected'
        OnClick = MenuDeleteSelectedClick
      end
    end
    object MenuSelection: TMenuItem
      Caption = '&Selection'
      object MenuSelectionCrop: TMenuItem
        Caption = '&Crop'
        OnClick = MenuSelectionCropClick
      end
      object MenuSelectionInvert: TMenuItem
        Caption = '&Invert'
        OnClick = MenuSelectionInvertClick
      end
    end
  end
  object PopupMenuPlaylist: TPopupMenu
    Left = 272
    Top = 352
    object PopupPlay: TMenuItem
      Caption = '&Play'
      OnClick = PopupPlayClick
    end
    object N1: TMenuItem
      Caption = '-'
    end
    object PopupInvertSelection: TMenuItem
      Caption = '&Invert selection'
      OnClick = PopupInvertSelectionClick
    end
    object PopupCropSelection: TMenuItem
      Caption = '&Crop selection'
      OnClick = PopupCropSelectionClick
    end
    object N2: TMenuItem
      Caption = '-'
    end
    object PopupDeleteSelected: TMenuItem
      Caption = '&Delete selected'
      OnClick = PopupDeleteSelectedClick
    end
    object PopupDeleteAll: TMenuItem
      Caption = 'Delete &all'
      OnClick = PopupDeleteAllClick
    end
  end
end
