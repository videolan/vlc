object PlaylistDlg: TPlaylistDlg
  Left = 162
  Top = 364
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
    OnDblClick = PlayStreamActionExecute
    OnKeyDown = ListViewPlaylistKeyDown
  end
  object MainMenuPlaylist: TMainMenu
    Left = 8
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
        Action = DeleteAllAction
      end
      object MenuDeleteSelected: TMenuItem
        Action = DeleteSelectionAction
        Caption = '&Selection'
      end
    end
    object MenuSelection: TMenuItem
      Caption = '&Selection'
      object MenuSelectionCrop: TMenuItem
        Action = CropSelectionAction
      end
      object MenuSelectionInvert: TMenuItem
        Action = InvertSelectionAction
      end
    end
  end
  object PopupMenuPlaylist: TPopupMenu
    Left = 40
    Top = 352
    object PopupPlay: TMenuItem
      Action = PlayStreamAction
    end
    object N1: TMenuItem
      Caption = '-'
    end
    object PopupInvertSelection: TMenuItem
      Action = InvertSelectionAction
      Caption = '&Invert selection'
    end
    object PopupCropSelection: TMenuItem
      Action = CropSelectionAction
      Caption = '&Crop selection'
    end
    object N2: TMenuItem
      Caption = '-'
    end
    object PopupDeleteSelected: TMenuItem
      Action = DeleteSelectionAction
      Caption = '&Delete selection'
    end
    object PopupDeleteAll: TMenuItem
      Action = DeleteAllAction
      Caption = 'Delete &all'
    end
  end
  object ActionList1: TActionList
    Left = 264
    Top = 352
    object InvertSelectionAction: TAction
      Caption = 'Invert'
      Hint = 'Invert selection'
      OnExecute = InvertSelectionActionExecute
    end
    object CropSelectionAction: TAction
      Caption = 'Crop'
      Hint = 'Crop selection'
      OnExecute = CropSelectionActionExecute
    end
    object DeleteSelectionAction: TAction
      Caption = 'Delete'
      Hint = 'Delete selection'
      OnExecute = DeleteSelectionActionExecute
    end
    object DeleteAllAction: TAction
      Caption = 'All'
      Hint = 'Delete all items'
      OnExecute = DeleteAllActionExecute
    end
    object PlayStreamAction: TAction
      Caption = 'Play'
      Hint = 'Play the selected stream'
      OnExecute = PlayStreamActionExecute
    end
  end
end
