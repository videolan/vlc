object SoutDlg: TSoutDlg
  Left = 500
  Top = 325
  Width = 394
  Height = 244
  Caption = 'Stream output'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  PixelsPerInch = 96
  TextHeight = 13
  object GroupBoxStreamOut: TGroupBox
    Left = 8
    Top = 8
    Width = 369
    Height = 161
    Anchors = [akLeft, akTop, akRight]
    Caption = 'Stream output MRL (Media Resource Locator)'
    TabOrder = 0
    object LabelPort: TLabel
      Left = 264
      Top = 98
      Width = 22
      Height = 13
      Anchors = [akTop, akRight]
      Caption = 'Port:'
      Enabled = False
    end
    object LabelAddress: TLabel
      Left = 96
      Top = 98
      Width = 41
      Height = 13
      Caption = 'Address:'
      Enabled = False
    end
    object EditMrl: TEdit
      Left = 16
      Top = 24
      Width = 337
      Height = 21
      Anchors = [akLeft, akTop, akRight]
      TabOrder = 0
      Text = 'file/ts://'
    end
    object PanelAccess: TPanel
      Left = 16
      Top = 56
      Width = 73
      Height = 97
      TabOrder = 1
      object RadioButtonFile: TRadioButton
        Left = 8
        Top = 8
        Width = 49
        Height = 17
        Caption = 'File'
        Checked = True
        TabOrder = 0
        TabStop = True
        OnClick = RadioButtonAccessClick
      end
      object RadioButtonUDP: TRadioButton
        Left = 8
        Top = 40
        Width = 49
        Height = 17
        Caption = 'UDP'
        TabOrder = 1
        OnClick = RadioButtonAccessClick
      end
      object RadioButtonRTP: TRadioButton
        Left = 8
        Top = 72
        Width = 49
        Height = 17
        Caption = 'RTP'
        TabOrder = 2
        OnClick = RadioButtonAccessClick
      end
    end
    object ButtonBrowse: TButton
      Left = 278
      Top = 60
      Width = 75
      Height = 25
      Anchors = [akTop, akRight]
      Caption = 'Browse...'
      TabOrder = 2
      OnClick = ButtonBrowseClick
    end
    object EditFile: TEdit
      Left = 96
      Top = 62
      Width = 177
      Height = 21
      Anchors = [akLeft, akTop, akRight]
      TabOrder = 3
      OnChange = CustomEditChange
    end
    object SpinEditPort: TCSpinEdit
      Left = 296
      Top = 93
      Width = 57
      Height = 22
      TabStop = True
      Anchors = [akTop, akRight]
      Enabled = False
      MaxValue = 100000
      ParentColor = False
      TabOrder = 4
      Value = 1234
      OnChange = CustomEditChange
      OnClick = CustomEditChange
    end
    object EditAddress: TEdit
      Left = 144
      Top = 94
      Width = 113
      Height = 21
      Anchors = [akLeft, akTop, akRight]
      Enabled = False
      TabOrder = 5
      Text = '239.239.0.1'
      OnChange = CustomEditChange
    end
    object PanelMux: TPanel
      Left = 184
      Top = 124
      Width = 89
      Height = 25
      Anchors = [akLeft, akTop, akRight]
      TabOrder = 6
      object RadioButtonPS: TRadioButton
        Left = 3
        Top = 4
        Width = 41
        Height = 17
        Anchors = [akTop]
        Caption = 'PS'
        TabOrder = 0
        OnClick = RadioButtonMuxClick
      end
      object RadioButtonTS: TRadioButton
        Left = 40
        Top = 4
        Width = 41
        Height = 17
        Anchors = [akTop]
        Caption = 'TS'
        Checked = True
        TabOrder = 1
        TabStop = True
        OnClick = RadioButtonMuxClick
      end
    end
  end
  object BitBtnOK: TBitBtn
    Left = 31
    Top = 184
    Width = 138
    Height = 25
    TabOrder = 1
    OnClick = BitBtnOKClick
    Kind = bkOK
  end
  object BitBtnCancel: TBitBtn
    Left = 216
    Top = 184
    Width = 136
    Height = 25
    TabOrder = 2
    Kind = bkCancel
  end
  object OpenDialog1: TOpenDialog
    Left = 120
    Top = 128
  end
end
