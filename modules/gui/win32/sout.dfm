object SoutDlg: TSoutDlg
  Tag = 3
  Left = 454
  Top = 369
  Width = 392
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
    Tag = 3
    Left = 8
    Top = 8
    Width = 368
    Height = 168
    Anchors = [akLeft, akTop, akRight]
    Caption = 'Stream output MRL (Media Resource Locator)'
    TabOrder = 0
    object LabelPort: TLabel
      Tag = 3
      Left = 263
      Top = 98
      Width = 22
      Height = 13
      Anchors = [akTop, akRight]
      Caption = 'Port:'
      Enabled = False
    end
    object LabelAddress: TLabel
      Tag = 3
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
      Width = 336
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
        Tag = 3
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
        Tag = 3
        Left = 8
        Top = 40
        Width = 49
        Height = 17
        Caption = 'UDP'
        TabOrder = 1
        OnClick = RadioButtonAccessClick
      end
      object RadioButtonRTP: TRadioButton
        Tag = 3
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
      Tag = 3
      Left = 277
      Top = 60
      Width = 75
      Height = 25
      Anchors = [akTop, akRight]
      Caption = 'Browse...'
      TabOrder = 3
      OnClick = ButtonBrowseClick
    end
    object EditFile: TEdit
      Left = 96
      Top = 62
      Width = 176
      Height = 21
      Anchors = [akLeft, akTop, akRight]
      TabOrder = 2
      OnChange = CustomEditChange
    end
    object SpinEditPort: TCSpinEdit
      Left = 295
      Top = 93
      Width = 57
      Height = 22
      TabStop = True
      Anchors = [akTop, akRight]
      Enabled = False
      MaxValue = 100000
      ParentColor = False
      TabOrder = 5
      Value = 1234
      OnChange = CustomEditChange
      OnClick = CustomEditChange
    end
    object EditAddress: TEdit
      Left = 144
      Top = 94
      Width = 112
      Height = 21
      Anchors = [akLeft, akTop, akRight]
      Enabled = False
      TabOrder = 4
      Text = '239.239.0.1'
      OnChange = CustomEditChange
    end
    object PanelMux: TPanel
      Left = 184
      Top = 124
      Width = 88
      Height = 25
      Anchors = [akLeft, akTop, akRight]
      TabOrder = 6
      object RadioButtonPS: TRadioButton
        Tag = 3
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
        Tag = 3
        Left = 39
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
  object ButtonOK: TButton
    Tag = 3
    Left = 218
    Top = 184
    Width = 75
    Height = 23
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 1
    OnClick = ButtonOKClick
  end
  object ButtonCancel: TButton
    Tag = 3
    Left = 301
    Top = 184
    Width = 75
    Height = 23
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 2
  end
  object OpenDialog1: TOpenDialog
    Left = 120
    Top = 128
  end
end
