object Options: TOptions
  Left = 328
  Top = 160
  BorderStyle = bsDialog
  Caption = 'Options'
  ClientHeight = 750
  ClientWidth = 816
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -20
  Font.Name = 'Segoe UI'
  Font.Style = []
  Position = poScreenCenter
  OnCreate = FormCreate
  PixelsPerInch = 144
  TextHeight = 28
  object Label5: TLabel
    Left = 449
    Top = 53
    Width = 273
    Height = 30
    Margins.Left = 5
    Margins.Top = 5
    Margins.Right = 5
    Margins.Bottom = 5
    AutoSize = False
    Caption = '&Jed Knight CD'
  end
  object Label6: TLabel
    Left = 455
    Top = 126
    Width = 267
    Height = 29
    Margins.Left = 5
    Margins.Top = 5
    Margins.Right = 5
    Margins.Bottom = 5
    AutoSize = False
    Caption = '&Jedi Knight  Directory'
  end
  object Label7: TLabel
    Left = 47
    Top = 90
    Width = 267
    Height = 29
    Margins.Left = 5
    Margins.Top = 5
    Margins.Right = 5
    Margins.Bottom = 5
    AutoSize = False
    Caption = '&Jedi Knight  Directory'
  end
  object Label8: TLabel
    Left = 41
    Top = 17
    Width = 273
    Height = 30
    Margins.Left = 5
    Margins.Top = 5
    Margins.Right = 5
    Margins.Bottom = 5
    AutoSize = False
    Caption = '&Jed Knight CD'
  end
  object Pages: TPageControl
    Left = 0
    Top = 0
    Width = 816
    Height = 677
    Margins.Left = 5
    Margins.Top = 5
    Margins.Right = 5
    Margins.Bottom = 5
    ActivePage = PPreview
    Align = alClient
    TabOrder = 0
    ExplicitWidth = 806
    ExplicitHeight = 675
    object JK: TTabSheet
      HelpContext = 290
      Margins.Left = 5
      Margins.Top = 5
      Margins.Right = 5
      Margins.Bottom = 5
      Caption = 'Game'
      object GBMOTS: TGroupBox
        Left = 5
        Top = 173
        Width = 787
        Height = 121
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        Caption = 'Mysteries of the Sith'
        TabOrder = 0
        object Label9: TLabel
          Left = 21
          Top = 41
          Width = 267
          Height = 28
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          AutoSize = False
          Caption = 'Install &Directory'
        end
        object MotsDirEdit: TEdit
          Left = 21
          Top = 69
          Width = 629
          Height = 36
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          TabOrder = 0
        end
        object BtnBrowseMotsDir: TButton
          Left = 669
          Top = 66
          Width = 113
          Height = 39
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          Caption = 'Browse'
          TabOrder = 1
          OnClick = BtnBrowseMotsDirClick
        end
      end
      object GBJKDF2: TGroupBox
        Left = 5
        Top = 35
        Width = 787
        Height = 127
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        Caption = 'Dark Forces II'
        TabOrder = 1
        object Label1: TLabel
          Left = 23
          Top = 41
          Width = 267
          Height = 28
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          AutoSize = False
          Caption = 'Install &Directory'
        end
        object JkDirEdit: TEdit
          Left = 23
          Top = 69
          Width = 627
          Height = 36
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          TabOrder = 0
        end
        object BtnBrowseJkDir: TButton
          Left = 669
          Top = 66
          Width = 113
          Height = 39
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          Caption = 'Browse'
          TabOrder = 1
          OnClick = BtnBrowseJkDirClick
        end
      end
      object GBIJIM: TGroupBox
        Left = 5
        Top = 305
        Width = 787
        Height = 121
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        Caption = 'Indiana Jones and the Infernal Machine'
        TabOrder = 2
        object Label13: TLabel
          Left = 23
          Top = 41
          Width = 270
          Height = 28
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          AutoSize = False
          Caption = 'Install &Directory'
        end
        object Label14: TLabel
          Left = 24
          Top = 123
          Width = 269
          Height = 29
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          AutoSize = False
          Caption = 'Exe Path'
          Visible = False
        end
        object IJIMDirEdit: TEdit
          Left = 23
          Top = 69
          Width = 627
          Height = 36
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          TabOrder = 0
        end
        object BtnBrowseIJIMExe: TButton
          Left = 608
          Top = 149
          Width = 112
          Height = 39
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          Caption = 'Browse'
          TabOrder = 1
          Visible = False
          OnClick = BtnBrowseIJIMExeClick
        end
        object IJIMExeEdit: TEdit
          Left = 24
          Top = 152
          Width = 551
          Height = 36
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          TabOrder = 2
          Visible = False
        end
        object BtnBrowseIJIMDir: TButton
          Left = 669
          Top = 68
          Width = 113
          Height = 37
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          Caption = 'Browse'
          TabOrder = 3
          OnClick = BtnBrowseIJIMDirClick
        end
      end
    end
    object PPreview: TTabSheet
      HelpContext = 300
      Margins.Left = 5
      Margins.Top = 5
      Margins.Right = 5
      Margins.Bottom = 5
      Caption = '3D Preview'
      object Label3: TLabel
        Left = 14
        Top = 116
        Width = 168
        Height = 30
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        AutoSize = False
        Caption = 'Direct3D device:'
      end
      object Label4: TLabel
        Left = 398
        Top = 116
        Width = 144
        Height = 30
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        AutoSize = False
        Caption = 'Description'
      end
      object LBGamma: TLabel
        Left = 419
        Top = 242
        Width = 144
        Height = 24
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        AutoSize = False
        Caption = 'Gamma'
      end
      object LBDevices: TListBox
        Left = 14
        Top = 146
        Width = 360
        Height = 84
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        ItemHeight = 28
        TabOrder = 0
        OnClick = LBDevicesClick
      end
      object RGWSize: TRadioGroup
        Left = 29
        Top = 239
        Width = 360
        Height = 217
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        Caption = 'Window Size'
        ItemIndex = 0
        Items.Strings = (
          '800x600'
          '1024x768'
          '1280x960'
          '1440x900'
          '1600x1024'
          '1920x1200')
        TabOrder = 1
      end
      object MMDevDesc: TMemo
        Left = 398
        Top = 146
        Width = 372
        Height = 84
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        Enabled = False
        TabOrder = 2
      end
      object CBP3DOnTop: TCheckBox
        Left = 599
        Top = 275
        Width = 162
        Height = 30
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        Caption = '&Stays on top'
        TabOrder = 3
      end
      object CBFullLight: TCheckBox
        Left = 419
        Top = 396
        Width = 144
        Height = 30
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        Caption = '&Fully Lit'
        TabOrder = 4
      end
      object EBGamma: TEdit
        Left = 419
        Top = 270
        Width = 156
        Height = 36
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        TabOrder = 5
        Text = '1'
      end
      object CB3DLayers: TCheckBox
        Left = 419
        Top = 323
        Width = 312
        Height = 30
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        Caption = '&Show only visible layers'
        TabOrder = 6
      end
      object CBShowThings: TCheckBox
        Left = 419
        Top = 359
        Width = 312
        Height = 30
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        Caption = '&Show things'
        Checked = True
        State = cbChecked
        TabOrder = 7
      end
      object CBColored: TCheckBox
        Left = 419
        Top = 434
        Width = 312
        Height = 30
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        Caption = '&Enable colored lighting'
        Checked = True
        State = cbChecked
        TabOrder = 8
      end
      object RGAPI: TRadioGroup
        Left = 224
        Top = 14
        Width = 324
        Height = 99
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        Caption = 'Graphics API'
        ItemIndex = 0
        Items.Strings = (
          'Direct3D (deprecated)'
          'OpenGL')
        TabOrder = 9
      end
      object cbPlaySecSnd: TCheckBox
        Left = 419
        Top = 473
        Width = 231
        Height = 27
        Hint = 'Plays sector sound when camera is within sector'
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        Caption = 'Play sector sound'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 10
      end
      object CBNoClipSelectionOutline: TCheckBox
        Left = 419
        Top = 509
        Width = 292
        Height = 25
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        Caption = 'Don'#39't clip selection outline'
        TabOrder = 11
      end
    end
    object PGEnv: TTabSheet
      HelpContext = 310
      Margins.Left = 5
      Margins.Top = 5
      Margins.Right = 5
      Margins.Bottom = 5
      Caption = 'Environment'
      object SColor: TShape
        Left = 320
        Top = 48
        Width = 96
        Height = 98
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        Pen.Width = 2
      end
      object Label11: TLabel
        Left = 96
        Top = 14
        Width = 170
        Height = 24
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        AutoSize = False
        Caption = 'Colors'
      end
      object Label12: TLabel
        Left = 320
        Top = 543
        Width = 63
        Height = 30
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        AutoSize = False
        Caption = 'Mins'
      end
      object Label15: TLabel
        Left = 320
        Top = 587
        Width = 63
        Height = 30
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        AutoSize = False
        Caption = 'Steps'
      end
      object LBColors: TListBox
        Left = 24
        Top = 48
        Width = 285
        Height = 464
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        ItemHeight = 28
        TabOrder = 0
        OnClick = LBColorsClick
        OnDblClick = LBColorsDblClick
      end
      object BNEditColor: TButton
        Left = 320
        Top = 155
        Width = 96
        Height = 37
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        Caption = '&Edit'
        TabOrder = 1
        OnClick = BNEditColorClick
      end
      object EBSaveInt: TEdit
        Left = 215
        Top = 537
        Width = 75
        Height = 36
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        Alignment = taRightJustify
        Enabled = False
        ReadOnly = True
        TabOrder = 2
        Text = '1'
      end
      object CBAutoSave: TCheckBox
        Left = 24
        Top = 543
        Width = 182
        Height = 30
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        Caption = '&Autosave every'
        TabOrder = 3
        OnClick = CBAutoSaveClick
      end
      object UDSaveInt: TUpDown
        Left = 290
        Top = 537
        Width = 25
        Height = 38
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        Associate = EBSaveInt
        Min = 1
        Max = 60
        Position = 1
        TabOrder = 4
      end
      object CBUndo: TCheckBox
        Left = 24
        Top = 585
        Width = 170
        Height = 30
        Hint = 'Enable undo'
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        Caption = 'Undo enabled'
        Checked = True
        ParentShowHint = False
        ShowHint = True
        State = cbChecked
        TabOrder = 5
        OnClick = CBUndoClick
      end
      object EBUndoStackSize: TEdit
        Left = 215
        Top = 584
        Width = 73
        Height = 36
        Hint = 'Max number of undo steps'
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        Alignment = taRightJustify
        NumbersOnly = True
        ParentShowHint = False
        ReadOnly = True
        ShowHint = True
        TabOrder = 6
        Text = '1'
      end
      object UDUndoStackSize: TUpDown
        Left = 288
        Top = 584
        Width = 26
        Height = 37
        Hint = 'Number of undo steps available'
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        Associate = EBUndoStackSize
        Min = 1
        Max = 1024
        ParentShowHint = False
        Position = 1
        ShowHint = True
        TabOrder = 7
        Thousands = False
      end
      object gbRenderOptions: TGroupBox
        Left = 432
        Top = 36
        Width = 359
        Height = 314
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        Caption = 'Render Options'
        TabOrder = 8
        object ebGeoVertSize: TLabeledEdit
          Left = 276
          Top = 216
          Width = 48
          Height = 36
          Hint = 
            'Enter the size of world geometry vertices displayed in the edito' +
            'r.'
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          Alignment = taRightJustify
          EditLabel.Width = 249
          EditLabel.Height = 36
          EditLabel.Hint = 
            'Adjust the size of world geometry vertices displayed in the edit' +
            'or.'
          EditLabel.Margins.Left = 12
          EditLabel.Margins.Top = 12
          EditLabel.Margins.Right = 12
          EditLabel.Margins.Bottom = 12
          EditLabel.Caption = 'Geometry vertex display size'
          EditLabel.ParentShowHint = False
          EditLabel.ShowHint = True
          LabelPosition = lpLeft
          LabelSpacing = 5
          NumbersOnly = True
          ParentShowHint = False
          ShowHint = True
          TabOrder = 0
          Text = '5'
        end
        object cbInvrtCull: TCheckBox
          Left = 14
          Top = 152
          Width = 325
          Height = 27
          Hint = 'Reverses the backface culling render order for world geometry.'
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          Caption = 'Inverted backface culling'
          ParentShowHint = False
          ShowHint = True
          TabOrder = 1
        end
        object CBDbuf: TCheckBox
          Left = 14
          Top = 180
          Width = 325
          Height = 26
          Hint = 'Removes blinking on redraws'
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          Caption = 'Double buffered (software mode)'
          Checked = True
          DoubleBuffered = False
          ParentDoubleBuffered = False
          ParentShowHint = False
          ShowHint = True
          State = cbChecked
          TabOrder = 2
        end
        object ebObjVertSize: TLabeledEdit
          Left = 276
          Top = 261
          Width = 48
          Height = 36
          Hint = 'Enter the size of object vertex displayed in the editor.'
          Margins.Left = 8
          Margins.Top = 8
          Margins.Right = 8
          Margins.Bottom = 8
          Alignment = taRightJustify
          EditLabel.Width = 220
          EditLabel.Height = 36
          EditLabel.Hint = 'Adjust the size of object vertex displayed in the editor.'
          EditLabel.Margins.Left = 18
          EditLabel.Margins.Top = 18
          EditLabel.Margins.Right = 18
          EditLabel.Margins.Bottom = 18
          EditLabel.Caption = 'Object vertex display size'
          EditLabel.ParentShowHint = False
          EditLabel.ShowHint = True
          LabelPosition = lpLeft
          LabelSpacing = 25
          NumbersOnly = True
          ParentShowHint = False
          ShowHint = True
          TabOrder = 3
          Text = '5'
        end
        object RGWireframe: TRadioGroup
          Left = 17
          Top = 35
          Width = 139
          Height = 109
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          Caption = 'Graphics API'
          ItemIndex = 0
          Items.Strings = (
            'Software'
            'OpenGL')
          TabOrder = 4
        end
        object udGeoVertSize: TUpDown
          Left = 324
          Top = 216
          Width = 26
          Height = 38
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          Associate = ebGeoVertSize
          Position = 5
          TabOrder = 5
        end
      end
      object udObjVertSize: TUpDown
        Left = 758
        Top = 297
        Width = 25
        Height = 36
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        TabOrder = 9
      end
      object gbMapEdit: TGroupBox
        Left = 432
        Top = 359
        Width = 359
        Height = 226
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        Caption = 'Map Edit'
        TabOrder = 10
        object ebVertSnapMaxDist: TLabeledEdit
          Left = 272
          Top = 33
          Width = 52
          Height = 36
          Hint = 
            'Enter the maximum distance for vertex snapping (object dragging,' +
            ' cleaving). Smaller values ensure more precise snapping.'
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          Alignment = taRightJustify
          EditLabel.Width = 218
          EditLabel.Height = 36
          EditLabel.Hint = 
            'The maximum distance for vertex snapping (object dragging, cleav' +
            'ing).'
          EditLabel.Margins.Left = 12
          EditLabel.Margins.Top = 12
          EditLabel.Margins.Right = 12
          EditLabel.Margins.Bottom = 12
          EditLabel.Caption = 'Max vertex snap distance'
          EditLabel.ParentShowHint = False
          EditLabel.ShowHint = True
          LabelPosition = lpLeft
          LabelSpacing = 20
          ParentShowHint = False
          ShowHint = True
          TabOrder = 0
          Text = '0.2'
        end
        object RGMapRot: TRadioGroup
          Left = 20
          Top = 86
          Width = 139
          Height = 120
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          Caption = 'Map rotation'
          ItemIndex = 0
          Items.Strings = (
            'New Style'
            'Old Style')
          TabOrder = 1
        end
        object gbLockZoom: TGroupBox
          Left = 182
          Top = 86
          Width = 156
          Height = 120
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          Caption = 'Lock Zoom'
          TabOrder = 2
          object cbLockZoom: TCheckBox
            Left = 110
            Top = 2
            Width = 147
            Height = 27
            Hint = 'Locks view zoom to min/max values.'
            Margins.Left = 5
            Margins.Top = 5
            Margins.Right = 5
            Margins.Bottom = 5
            ParentShowHint = False
            ShowHint = True
            TabOrder = 0
            OnClick = cbLockZoomClick
          end
          object ebZoomMin: TLabeledEdit
            Left = 75
            Top = 32
            Width = 74
            Height = 36
            Hint = 'Min zoom-out value. '
            Margins.Left = 5
            Margins.Top = 5
            Margins.Right = 5
            Margins.Bottom = 5
            Alignment = taRightJustify
            EditLabel.Width = 34
            EditLabel.Height = 36
            EditLabel.Hint = 'Min zoom-out value. '
            EditLabel.Margins.Left = 12
            EditLabel.Margins.Top = 12
            EditLabel.Margins.Right = 12
            EditLabel.Margins.Bottom = 12
            EditLabel.Caption = 'Min'
            EditLabel.ParentShowHint = False
            EditLabel.ShowHint = True
            Enabled = False
            LabelPosition = lpLeft
            LabelSpacing = 20
            ParentShowHint = False
            ShowHint = True
            TabOrder = 1
            Text = '1.0'
          end
          object ebZoomMax: TLabeledEdit
            Left = 75
            Top = 77
            Width = 74
            Height = 36
            Hint = 'Max zoom-in value. '
            Margins.Left = 5
            Margins.Top = 5
            Margins.Right = 5
            Margins.Bottom = 5
            Alignment = taRightJustify
            EditLabel.Width = 37
            EditLabel.Height = 36
            EditLabel.Hint = 'Max zoom-in value. '
            EditLabel.Margins.Left = 12
            EditLabel.Margins.Top = 12
            EditLabel.Margins.Right = 12
            EditLabel.Margins.Bottom = 12
            EditLabel.Caption = 'Max'
            EditLabel.ParentShowHint = False
            EditLabel.ShowHint = True
            Enabled = False
            LabelPosition = lpLeft
            LabelSpacing = 20
            ParentShowHint = False
            ShowHint = True
            TabOrder = 2
            Text = '5000.0'
          end
        end
      end
    end
    object PToolbar: TTabSheet
      Margins.Left = 5
      Margins.Top = 5
      Margins.Right = 5
      Margins.Bottom = 5
      Caption = 'Toolbar'
      TabVisible = False
    end
    object Miscoptions: TTabSheet
      HelpContext = 320
      Margins.Left = 5
      Margins.Top = 5
      Margins.Right = 5
      Margins.Bottom = 5
      Caption = 'Misc. Options'
      object ScrollBox1: TScrollBox
        Left = 8
        Top = 23
        Width = 780
        Height = 633
        Margins.Left = 5
        Margins.Top = 5
        Margins.Right = 5
        Margins.Bottom = 5
        BorderStyle = bsNone
        Padding.Bottom = 5550
        TabOrder = 0
        object CBThingsOnFloor: TCheckBox
          Left = 24
          Top = 12
          Width = 734
          Height = 30
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          Alignment = taLeftJustify
          Caption = 'Put new thing at half height from the grid'
          ParentShowHint = False
          ShowHint = True
          TabOrder = 0
        end
        object CBMoveFrames: TCheckBox
          Left = 24
          Top = 41
          Width = 734
          Height = 30
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          Alignment = taLeftJustify
          Caption = 'Move thing frames when thing moves'
          ParentShowHint = False
          ShowHint = True
          TabOrder = 1
        end
        object CBGobSmart: TCheckBox
          Left = 24
          Top = 71
          Width = 734
          Height = 30
          Hint = 'Put only used files in a GOB'
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          Alignment = taLeftJustify
          Caption = 'Smart GOBbing'
          ParentShowHint = False
          ShowHint = True
          TabOrder = 2
        end
        object CBCheckOverlaps: TCheckBox
          Left = 24
          Top = 99
          Width = 734
          Height = 30
          Hint = 
            'Slows down consistency check. Overlapping sectors is not afatal ' +
            'error'
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          Alignment = taLeftJustify
          Caption = 'Check for overlapping sectors in Consistency Checker'
          ParentShowHint = False
          ShowHint = True
          TabOrder = 3
        end
        object CBNewLightCalc: TCheckBox
          Left = 24
          Top = 128
          Width = 734
          Height = 30
          Hint = 'Faster, but may produce different result'
          Margins.Left = 11
          Margins.Top = 11
          Margins.Right = 11
          Margins.Bottom = 11
          Alignment = taLeftJustify
          Caption = 'Use new method of lighting calculation'
          ParentShowHint = False
          ShowHint = True
          TabOrder = 4
        end
        object CBConfRevert: TCheckBox
          Left = 24
          Top = 158
          Width = 734
          Height = 30
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          Alignment = taLeftJustify
          Caption = 'Confirm revert to saved'
          ParentShowHint = False
          ShowHint = True
          TabOrder = 5
        end
        object CBSaveEditorLights: TCheckBox
          Left = 24
          Top = 186
          Width = 734
          Height = 30
          Hint = 'Adds custom section '#39'Lights'#39' to JKL/NDY file'
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          Alignment = taLeftJustify
          Caption = 'Export editor lights  [JKL/NDY]'
          ParentShowHint = False
          ShowHint = True
          TabOrder = 6
        end
        object CBSaveEditorLayers: TCheckBox
          Left = 24
          Top = 215
          Width = 734
          Height = 30
          Hint = 'Adds custom section '#39'Layers'#39' to JKL/NDY file'
          Margins.Left = 5
          Margins.Top = 5
          Margins.Right = 5
          Margins.Bottom = 5
          Alignment = taLeftJustify
          Caption = 'Export editor layers [JKL/NDY]'
          ParentShowHint = False
          ShowHint = True
          TabOrder = 7
        end
      end
    end
  end
  object Panel1: TPanel
    Left = 0
    Top = 677
    Width = 816
    Height = 73
    Margins.Left = 5
    Margins.Top = 5
    Margins.Right = 5
    Margins.Bottom = 5
    Align = alBottom
    TabOrder = 1
    ExplicitTop = 675
    ExplicitWidth = 806
    object SBHelp: TSpeedButton
      Left = 749
      Top = 23
      Width = 45
      Height = 39
      Hint = 'Help'
      Margins.Left = 5
      Margins.Top = 5
      Margins.Right = 5
      Margins.Bottom = 5
      Enabled = False
      Glyph.Data = {
        76010000424D7601000000000000760000002800000020000000100000000100
        0400000000000001000000000000000000001000000010000000000000000000
        800000800000008080008000000080008000808000007F7F7F00BFBFBF000000
        FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF00333333333333
        3333333333FFFFF3333333333F797F3333333333F737373FF333333BFB999BFB
        33333337737773773F3333BFBF797FBFB33333733337333373F33BFBFBFBFBFB
        FB3337F33333F33337F33FBFBFB9BFBFBF3337333337F333373FFBFBFBF97BFB
        FBF37F333337FF33337FBFBFBFB99FBFBFB37F3333377FF3337FFBFBFBFB99FB
        FBF37F33333377FF337FBFBF77BF799FBFB37F333FF3377F337FFBFB99FB799B
        FBF373F377F3377F33733FBF997F799FBF3337F377FFF77337F33BFBF99999FB
        FB33373F37777733373333BFBF999FBFB3333373FF77733F7333333BFBFBFBFB
        3333333773FFFF77333333333FBFBF3333333333377777333333}
      NumGlyphs = 2
      ParentShowHint = False
      ShowHint = True
      Visible = False
      OnClick = SBHelpClick
    end
    object BNOK: TButton
      Left = 414
      Top = 17
      Width = 140
      Height = 45
      Margins.Left = 5
      Margins.Top = 5
      Margins.Right = 5
      Margins.Bottom = 5
      Caption = '&OK'
      Default = True
      TabOrder = 0
      OnClick = BNOKClick
    end
    object BNCancel: TButton
      Left = 582
      Top = 17
      Width = 140
      Height = 45
      Margins.Left = 5
      Margins.Top = 5
      Margins.Right = 5
      Margins.Bottom = 5
      Cancel = True
      Caption = '&Cancel'
      ModalResult = 2
      TabOrder = 1
    end
  end
  object ColorDlg: TColorDialog
    CustomColors.Strings = (
      'ColorA=0'
      'ColorB=FFFFFF'
      'ColorC=FF'
      'ColorD=FF00'
      'ColorE=FF0000'
      'ColorF=FFFF'
      'ColorG=FFFF00'
      'ColorH=FF00FF'
      'ColorI=0'
      'ColorJ=0'
      'ColorK=0'
      'ColorL=0'
      'ColorM=0'
      'ColorN=0'
      'ColorO=0'
      'ColorP=0')
    Left = 704
    Top = 3
  end
end
