object PlaylistDlg: TPlaylistDlg
  Left = 433
  Top = 308
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
    Tag = 3
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
    Tag = 3
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
    OnDblClick = PlayStreamActionExecute
    OnKeyDown = ListViewPlaylistKeyDown
  end
  object MainMenuPlaylist: TMainMenu
    Left = 8
    Top = 8
    object MenuFile: TMenuItem
      Caption = '&File'
      object MenuFileOpen: TMenuItem
        Caption = '&Open...'
        OnClick = MenuFileOpenClick
      end
      object MenuFileSave: TMenuItem
        Caption = '&Save...'
        OnClick = MenuFileSaveClick
      end
      object N3: TMenuItem
        Caption = '-'
      end
      object MenuFileClose: TMenuItem
        Caption = '&Close'
        OnClick = MenuFileCloseClick
      end
    end
    object MenuAdd: TMenuItem
      Tag = 3
      Caption = '&Add'
      object MenuAddFile: TMenuItem
        Tag = 3
        Caption = '&File...'
        OnClick = MenuAddFileClick
      end
      object MenuAddDisc: TMenuItem
        Tag = 3
        Caption = '&Disc...'
        OnClick = MenuAddDiscClick
      end
      object MenuAddNet: TMenuItem
        Tag = 3
        Caption = '&Network...'
        OnClick = MenuAddNetClick
      end
      object MenuAddUrl: TMenuItem
        Tag = 3
        Caption = '&Url'
        Enabled = False
        OnClick = MenuAddUrlClick
      end
    end
    object MenuDelete: TMenuItem
      Tag = 3
      Caption = '&Delete'
      object MenuDeleteAll: TMenuItem
        Tag = 3
        Action = DeleteAllAction
      end
      object MenuDeleteSelected: TMenuItem
        Tag = 3
        Action = DeleteSelectionAction
        Caption = '&Selection'
      end
    end
    object MenuSelection: TMenuItem
      Tag = 3
      Caption = '&Selection'
      object MenuSelectionCrop: TMenuItem
        Tag = 3
        Action = CropSelectionAction
      end
      object MenuSelectionInvert: TMenuItem
        Tag = 3
        Action = InvertSelectionAction
      end
    end
  end
  object PopupMenuPlaylist: TPopupMenu
    Left = 40
    Top = 8
    object PopupPlay: TMenuItem
      Tag = 3
      Action = PlayStreamAction
    end
    object N1: TMenuItem
      Caption = '-'
    end
    object PopupInvertSelection: TMenuItem
      Tag = 3
      Action = InvertSelectionAction
      Caption = '&Invert selection'
    end
    object PopupCropSelection: TMenuItem
      Tag = 3
      Action = CropSelectionAction
      Caption = '&Crop selection'
    end
    object N2: TMenuItem
      Caption = '-'
    end
    object PopupDeleteSelected: TMenuItem
      Tag = 3
      Action = DeleteSelectionAction
      Caption = '&Delete selection'
    end
    object PopupDeleteAll: TMenuItem
      Tag = 3
      Action = DeleteAllAction
      Caption = 'Delete &all'
    end
  end
  object ActionList1: TActionList
    Left = 72
    Top = 8
    object InvertSelectionAction: TAction
      Tag = 3
      Caption = 'Invert'
      Hint = 'Invert selection'
      OnExecute = InvertSelectionActionExecute
    end
    object CropSelectionAction: TAction
      Tag = 3
      Caption = 'Crop'
      Hint = 'Crop selection'
      OnExecute = CropSelectionActionExecute
    end
    object DeleteSelectionAction: TAction
      Tag = 3
      Caption = 'Delete'
      Hint = 'Delete selection'
      OnExecute = DeleteSelectionActionExecute
    end
    object DeleteAllAction: TAction
      Tag = 3
      Caption = 'All'
      Hint = 'Delete all items'
      OnExecute = DeleteAllActionExecute
    end
    object PlayStreamAction: TAction
      Tag = 3
      Caption = 'Play'
      Hint = 'Play the selected stream'
      OnExecute = PlayStreamActionExecute
    end
  end
  object PlaylistOpenDlg: TOpenDialog
    Filter = 'All files(*.*)|*.*'
    Options = [ofHideReadOnly, ofPathMustExist, ofFileMustExist, ofEnableSizing]
    Left = 136
    Top = 8
  end
  object PlaylistSaveDlg: TSaveDialog
    Filter = 'All files(*.*)|*.*'
    Options = [ofOverwritePrompt, ofHideReadOnly, ofPathMustExist, ofEnableSizing]
    Left = 104
    Top = 8
  end
end
