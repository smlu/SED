object JedMain: TJedMain
  Left = 531
  Top = 73
  Caption = 'SED'
  ClientHeight = 822
  ClientWidth = 968
  Color = clBtnFace
  Constraints.MinHeight = 764
  Constraints.MinWidth = 827
  DefaultMonitor = dmDesktop
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -21
  Font.Name = 'Segoe UI'
  Font.Style = []
  KeyPreview = True
  Menu = MainMenu
  ShowHint = True
  OnCloseQuery = FormCloseQuery
  OnCreate = FormCreate
  OnDestroy = FormDestroy
  OnKeyDown = FormKeyDown
  OnKeyUp = FormKeyUp
  OnMouseDown = FormMouseDown
  OnMouseMove = FormMouseMove
  OnMouseUp = FormMouseUp
  OnMouseWheel = FormMouseWheel
  OnPaint = FormPaint
  OnResize = FormResize
  OnShow = FormShow
  PixelsPerInch = 144
  TextHeight = 30
  object Tbar: TPanel
    Left = 0
    Top = 0
    Width = 968
    Height = 86
    Margins.Left = 5
    Margins.Top = 5
    Margins.Right = 5
    Margins.Bottom = 5
    ParentCustomHint = False
    Align = alTop
    BevelOuter = bvNone
    BiDiMode = bdLeftToRight
    Ctl3D = True
    DoubleBuffered = False
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -23
    Font.Name = 'Segoe UI'
    Font.Style = []
    ParentBiDiMode = False
    ParentBackground = False
    ParentCtl3D = False
    ParentDoubleBuffered = False
    ParentFont = False
    ParentShowHint = False
    ShowHint = False
    TabOrder = 0
    ExplicitWidth = 958
    object BNSC: TSpeedButton
      Left = 6
      Top = 6
      Width = 72
      Height = 72
      Hint = 'Sector Mode'
      Margins.Left = 5
      Margins.Top = 5
      Margins.Right = 5
      Margins.Bottom = 5
      GroupIndex = 1
      Down = True
      Caption = 'Sector'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -18
      Font.Name = 'Segoe UI'
      Font.Style = []
      Glyph.Data = {
        76010000424D7601000000000000760000002800000020000000100000000100
        04000000000000010000130B0000130B00001000000000000000000000000000
        80000080000000808000800000008000800080800000C0C0C000808080000000
        FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF00333333333333
        3333333333333333333330000000003333338888888883333333300333333003
        3333883333338833333330303333303033338383333383833333303303333033
        0333833833338338333330330000000000338338888888888333303303333033
        3033833833338333833330330333303330338338333383338333303303333033
        3033833833338333833330000000003330338888888883338333330303333303
        3033383833333833833333300333333030333388333333838333333303333333
        0033333833333338833333330000000000333338888888888333333333333333
        3333333333333333333333333333333333333333333333333333}
      Layout = blGlyphTop
      NumGlyphs = 2
      ParentFont = False
      ParentShowHint = False
      ShowHint = True
      Spacing = 5
      OnClick = BNSCClick
    end
    object BNSF: TSpeedButton
      Left = 84
      Top = 6
      Width = 72
      Height = 72
      Hint = 'Surface Mode'
      Margins.Left = 5
      Margins.Top = 5
      Margins.Right = 5
      Margins.Bottom = 5
      GroupIndex = 1
      Caption = 'Surface'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -18
      Font.Name = 'Segoe UI'
      Font.Style = []
      Glyph.Data = {
        76010000424D7601000000000000760000002800000020000000100000000100
        04000000000000010000130B0000130B00001000000000000000000000000000
        80000080000000808000800000008000800080800000C0C0C000808080000000
        FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF00333333333333
        3333333333333333333333333333333333333333333333333333330000000000
        0333388888888888333333044444444403333877777777783333330444444444
        0333387777777778333333044444444403333877777777783333330444444444
        0333387777777778333333044444444403333877777777783333330444444444
        0333387777777778333333044444444403333877777777783333330444444444
        0333387777777778333333044444444403333877777777783333330444444444
        0333387777777778333333000000000003333888888888883333333333333333
        3333333333333333333333333333333333333333333333333333}
      Layout = blGlyphTop
      NumGlyphs = 2
      ParentFont = False
      ParentShowHint = False
      ShowHint = True
      Spacing = 5
      OnClick = BNSCClick
    end
    object BNVX: TSpeedButton
      Left = 162
      Top = 6
      Width = 72
      Height = 72
      Hint = 'Vertex Mode'
      Margins.Left = 5
      Margins.Top = 5
      Margins.Right = 5
      Margins.Bottom = 5
      GroupIndex = 1
      Caption = 'Vertex'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -18
      Font.Name = 'Segoe UI'
      Font.Style = []
      Glyph.Data = {
        76010000424D7601000000000000760000002800000020000000100000000100
        04000000000000010000130B0000130B00001000000000000000000000000000
        80000080000000808000800000008000800080800000C0C0C000808080000000
        FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF00333333333333
        33333333333333333333300FFFFFFFF003333887777777788333300333333330
        033338833333333883333F3333333333F33337333333333373333F3333333333
        F33337333333333373333F3333333333F33337333333333373333F3333333333
        F33337333333333373333F3333333333F33337333333333373333F3333333333
        F33337333333333373333F3333333333F33337333333333373333F3333333333
        F33337333333333373333F3333333333F33337333333333373333F3333333333
        F333373333333333733330033333333003333883333333388333300FFFFFFFF0
        0333388777777778833333333333333333333333333333333333}
      Layout = blGlyphTop
      NumGlyphs = 2
      ParentFont = False
      ParentShowHint = False
      ShowHint = True
      Spacing = 5
      OnClick = BNSCClick
    end
    object BNTH: TSpeedButton
      Left = 390
      Top = 6
      Width = 72
      Height = 72
      Hint = 'Thing Mode'
      Margins.Left = 5
      Margins.Top = 5
      Margins.Right = 5
      Margins.Bottom = 5
      GroupIndex = 1
      Caption = 'Thing'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -18
      Font.Name = 'Segoe UI'
      Font.Style = []
      Glyph.Data = {
        76010000424D7601000000000000760000002800000020000000100000000100
        04000000000000010000130B0000130B00001000000000000000000000000000
        80000080000000808000800000008000800080800000C0C0C000808080000000
        FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF00333333333333
        3333333333333333333333888888888833333388888888883333338FFFFFFFF8
        83333387777777788333338FFFFFFFF8F8333387777777787833339999999998
        F833331111111118783333999999999998333311111111111833338FFFFFFFF9
        99333387777777711133338FFFFFFFF899333387777777781133338888888888
        F9333388888888887133338FFFFFFFFF883333877777777788333338FFFFFFFF
        F833333877777777783333338FFFFFFFF8333333877777777833333338888888
        8833333338888888883333333333333333333333333333333333333333333333
        3333333333333333333333333333333333333333333333333333}
      Layout = blGlyphTop
      NumGlyphs = 2
      ParentFont = False
      ParentShowHint = False
      ShowHint = True
      Spacing = 5
      OnClick = BNSCClick
    end
    object BNLT: TSpeedButton
      Left = 468
      Top = 6
      Width = 72
      Height = 72
      Hint = 'Light Mode'
      Margins.Left = 5
      Margins.Top = 5
      Margins.Right = 5
      Margins.Bottom = 5
      GroupIndex = 1
      Caption = 'Light'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -18
      Font.Name = 'Segoe UI'
      Font.Style = []
      Glyph.Data = {
        76010000424D7601000000000000760000002800000020000000100000000100
        04000000000000010000130B0000130B00001000000000000000000000000000
        80000080000000808000800000008000800080800000C0C0C000808080000000
        FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF00333333003333
        3333333338833333333333333000033333333333888833333333333330000333
        333333338888333333333333300003333333333388883333333333333F33F333
        333333337337333333333333FF33FF3333333337733773333333333FF3333FF3
        33333377333377333333333F333333F333333373333337333333333F333333F3
        333333733333373333333333F3333F333333333733337333333333303FFFF303
        3333338377773833333333033333333033333833333333833333303333333333
        0333833333333338333303333333333330383333333333338333333333333333
        3333333333333333333333333333333333333333333333333333}
      Layout = blGlyphTop
      NumGlyphs = 2
      ParentFont = False
      ParentShowHint = False
      ShowHint = True
      Spacing = 5
      OnClick = BNSCClick
    end
    object BNED: TSpeedButton
      Left = 240
      Top = 6
      Width = 72
      Height = 72
      Hint = 'Edge Mode'
      Margins.Left = 5
      Margins.Top = 5
      Margins.Right = 5
      Margins.Bottom = 5
      GroupIndex = 1
      Caption = 'Edge'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -18
      Font.Name = 'Segoe UI'
      Font.Style = []
      Glyph.Data = {
        76010000424D7601000000000000760000002800000020000000100000000100
        04000000000000010000130B0000130B00001000000000000000000000000000
        80000080000000808000800000008000800080800000C0C0C000808080000000
        FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF00333333333333
        3333333333333333333333333333333333333333333333333333333333333333
        3333333333333333333333333333333333333333333333333333333333333333
        3333333333333333333333333333333333333333333333333333333333333333
        3333333333333333333330000000000000338888888888888833333333333333
        3333333333333333333333333333333333333333333333333333333333333333
        3333333333333333333333333333333333333333333333333333333333333333
        3333333333333333333333333333333333333333333333333333333333333333
        3333333333333333333333333333333333333333333333333333}
      Layout = blGlyphTop
      NumGlyphs = 2
      ParentFont = False
      ParentShowHint = False
      ShowHint = True
      Spacing = 5
      OnClick = BNSCClick
    end
    object LBXYZ: TLabel
      Left = 702
      Top = 29
      Width = 64
      Height = 31
      Margins.Left = 5
      Margins.Top = 5
      Margins.Right = 5
      Margins.Bottom = 5
      Caption = 'LBXYZ'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -23
      Font.Name = 'Segoe UI'
      Font.Style = []
      ParentFont = False
      Visible = False
    end
    object BNFR: TSpeedButton
      Left = 546
      Top = 6
      Width = 72
      Height = 72
      Hint = 'Frame Mode'
      Margins.Left = 5
      Margins.Top = 5
      Margins.Right = 5
      Margins.Bottom = 5
      GroupIndex = 1
      Caption = 'Frame'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -18
      Font.Name = 'Segoe UI'
      Font.Style = []
      Glyph.Data = {
        F6000000424DF600000000000000760000002800000010000000100000000100
        0400000000008000000000000000000000001000000010000000000000000000
        BF0000BF000000BFBF00BF000000BF00BF00BFBF0000C0C0C000808080000000
        FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF00777777777777
        7777700000000000777778080808080007777000000000008077770888888888
        08077000000000008080770FFFFFFFFF0808777077777777F080777707777777
        7F077000000000000000770FFFFFFFFF07777770FFFFFFFFF07777770FFFFFFF
        FF07777770000000000077777777777777777777777777777777}
      Layout = blGlyphTop
      ParentFont = False
      ParentShowHint = False
      ShowHint = True
      Spacing = 5
      OnClick = BNSCClick
    end
    object BNEX: TSpeedButton
      Left = 624
      Top = 6
      Width = 72
      Height = 72
      Hint = 'Extra Mode'
      Margins.Left = 5
      Margins.Top = 5
      Margins.Right = 5
      Margins.Bottom = 5
      GroupIndex = 1
      Caption = 'Extra'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -18
      Font.Name = 'Segoe UI'
      Font.Style = []
      Glyph.Data = {
        F6000000424DF600000000000000760000002800000010000000100000000100
        0400000000008000000000000000000000001000000010000000000000000000
        BF0000BF000000BFBF00BF000000BF00BF00BFBF0000C0C0C000808080000000
        FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF00777777777777
        7777700000000000777778080808080007777000000000008077770888888888
        08077000000000008080770FFFFFFFFF0808777077777777F080777707777777
        7F077000000000000000770FFFFFFFFF07777770FFFFFFFFF07777770FFFFFFF
        FF07777770000000000077777777777777777777777777777777}
      Layout = blGlyphTop
      ParentFont = False
      ParentShowHint = False
      ShowHint = True
      Spacing = 5
      OnClick = BNSCClick
    end
    object PNProjType: TPanel
      AlignWithMargins = True
      Left = 885
      Top = 5
      Width = 72
      Height = 76
      Hint = 'Change project game type'
      Margins.Left = 5
      Margins.Top = 5
      Margins.Right = 11
      Margins.Bottom = 5
      Align = alRight
      Caption = 'JK'
      Constraints.MaxHeight = 77
      Constraints.MaxWidth = 72
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -23
      Font.Name = 'Segoe UI'
      Font.Style = []
      ParentFont = False
      ParentShowHint = False
      ShowHint = True
      TabOrder = 0
      OnDblClick = PNProjTypeDblClick
      ExplicitLeft = 875
    end
  end
  object Panel1: TPanel
    Left = 0
    Top = 791
    Width = 968
    Height = 31
    Margins.Left = 5
    Margins.Top = 5
    Margins.Right = 5
    Margins.Bottom = 5
    Align = alBottom
    BevelOuter = bvLowered
    TabOrder = 1
    ExplicitTop = 789
    ExplicitWidth = 958
    object PXYZ: TPanel
      Left = 1
      Top = 1
      Width = 336
      Height = 29
      Hint = 'Shows current mouse position'
      Margins.Left = 5
      Margins.Top = 5
      Margins.Right = 5
      Margins.Bottom = 5
      ParentCustomHint = False
      Align = alLeft
      Alignment = taLeftJustify
      BevelOuter = bvLowered
      Ctl3D = True
      DoubleBuffered = False
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -20
      Font.Name = 'Segoe UI'
      Font.Style = []
      ParentBackground = False
      ParentCtl3D = False
      ParentDoubleBuffered = False
      ParentFont = False
      ParentShowHint = False
      ShowHint = True
      TabOrder = 0
    end
    object PMsg: TPanel
      Left = 373
      Top = 1
      Width = 594
      Height = 29
      Hint = 'Shows messages and warnings'
      Margins.Left = 5
      Margins.Top = 5
      Margins.Right = 5
      Margins.Bottom = 5
      Align = alClient
      BevelOuter = bvLowered
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -20
      Font.Name = 'Segoe UI'
      Font.Style = []
      ParentBackground = False
      ParentFont = False
      ParentShowHint = False
      ShowHint = True
      TabOrder = 1
      OnDblClick = PMsgDblClick
      ExplicitWidth = 584
    end
    object PMsel: TPanel
      Left = 337
      Top = 1
      Width = 36
      Height = 29
      Hint = 'Current <ultislection mode is Toggle. Click to change'
      Margins.Left = 5
      Margins.Top = 5
      Margins.Right = 5
      Margins.Bottom = 5
      Align = alLeft
      BevelOuter = bvLowered
      Caption = '*'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -20
      Font.Name = 'Segoe UI'
      Font.Style = []
      ParentBackground = False
      ParentFont = False
      ParentShowHint = False
      ShowHint = True
      TabOrder = 2
      OnClick = PMselClick
      OnDblClick = PMsgDblClick
    end
  end
  object MainMenu: TMainMenu
    Left = 72
    Top = 152
    object FileMenu: TMenuItem
      Caption = '&File'
      object New1: TMenuItem
        Caption = 'New &JK Project'
        OnClick = New1Click
      end
      object NewMOTSProject1: TMenuItem
        Caption = 'New &MOTS Project'
        OnClick = NewMOTSProject1Click
      end
      object NewIJIMProject: TMenuItem
        Caption = 'New &Infernal Project'
        Hint = 'Create new project for Indiana Jones and the Infernal Machine'
        OnClick = NewIJIMProjectClick
      end
      object N1: TMenuItem
        Caption = '-'
      end
      object OpenMenu: TMenuItem
        Caption = '&Open...'
        ShortCut = 16463
        OnClick = OpenMenuClick
      end
      object Import1: TMenuItem
        Caption = '&Import...'
        OnClick = Import1Click
      end
      object Save1: TMenuItem
        Caption = '&Save'
        ShortCut = 16467
        OnClick = Save1Click
      end
      object SaveAs1: TMenuItem
        Caption = 'Save &As...'
        OnClick = SaveAs1Click
      end
      object Reverttosaved1: TMenuItem
        Caption = '&Revert to Saved'
        OnClick = Reverttosaved1Click
      end
      object MakeaBackupCopy1: TMenuItem
        Caption = 'Make a &Backup Copy'
        ShortCut = 16450
        OnClick = MakeaBackupCopy1Click
      end
      object N4: TMenuItem
        Caption = '-'
      end
      object ExportSectoras3DO1: TMenuItem
        Caption = 'Export Sector(s) as &3DO...'
        OnClick = ExportSectoras3DO1Click
      end
      object ExportSectorasShape1: TMenuItem
        Caption = 'Export Sector as Shape'
        OnClick = ExportSectorasShape1Click
      end
      object N3: TMenuItem
        Caption = '-'
      end
      object GobProject1: TMenuItem
        Caption = '&Gob Project'
        OnClick = GobProject1Click
      end
      object SaveJKLGob1: TMenuItem
        Caption = 'Save JKL And GOB'
        OnClick = SaveJKLGob1Click
      end
      object SaveJKLGOBandTest1: TMenuItem
        Caption = 'Save JKL and &Test'
        ShortCut = 16497
        OnClick = SaveJKLGOBandTest1Click
      end
      object Exit1: TMenuItem
        Caption = '&Exit'
        OnClick = Exit1Click
      end
      object RecentBar: TMenuItem
        Caption = '-'
      end
    end
    object Edit1: TMenuItem
      Caption = '&Edit'
      OnClick = Edit1Click
      object miUndo: TMenuItem
        Caption = '&Undo'
        ShortCut = 16474
        OnClick = miUndoClick
      end
      object miCopy: TMenuItem
        Caption = '&Copy'
        OnClick = miCopyClick
      end
      object miPaste: TMenuItem
        Caption = '&Paste'
        OnClick = miPasteClick
      end
      object miPasteInPlace: TMenuItem
        Caption = '&Paste in Place'
        OnClick = miPasteInPlaceClick
      end
      object N5: TMenuItem
        Caption = '-'
      end
      object Find1: TMenuItem
        Caption = '&Find...'
        ShortCut = 16454
        OnClick = Find1Click
      end
      object FindNext1: TMenuItem
        Caption = 'Find &Next'
        ShortCut = 16455
        OnClick = FindNext1Click
      end
    end
    object View1: TMenuItem
      Caption = '&View'
      object ViewthingsAs1: TMenuItem
        Caption = '&View Things As'
        object Dots: TMenuItem
          Caption = '&Dots'
          Checked = True
          GroupIndex = 2
          RadioItem = True
          OnClick = WireframesClick
        end
        object Boxes: TMenuItem
          Caption = '&Boxes'
          GroupIndex = 2
          RadioItem = True
          OnClick = WireframesClick
        end
        object Wireframes: TMenuItem
          Caption = '&Wireframes'
          GroupIndex = 2
          RadioItem = True
          OnClick = WireframesClick
        end
      end
      object Multiselectionmode1: TMenuItem
        Caption = '&Multiselection mode'
        object miToggle: TMenuItem
          Caption = '&Toggle'
          Checked = True
          GroupIndex = 3
          RadioItem = True
          OnClick = miToggleClick
        end
        object miAdd: TMenuItem
          Caption = '&Add'
          GroupIndex = 3
          RadioItem = True
          OnClick = miToggleClick
        end
        object miSubtract: TMenuItem
          Caption = '&Subtract'
          GroupIndex = 3
          RadioItem = True
          OnClick = miToggleClick
        end
      end
      object HideThings: TMenuItem
        Caption = 'Hide &Things'
        OnClick = HideThingsClick
      end
      object HideLights: TMenuItem
        Caption = 'Hide &Lights'
        OnClick = HideLightsClick
      end
    end
    object Commands1: TMenuItem
      Caption = 'Co&mmands'
      object ReloadTemplates1: TMenuItem
        Caption = '&Reload Templates'
        OnClick = ReloadTemplates1Click
      end
      object MUNextObject: TMenuItem
        Caption = '&Next Object'
        OnClick = MUNextObjectClick
      end
      object MUPrevObject: TMenuItem
        Caption = '&Previous Object'
        OnClick = MUPrevObjectClick
      end
      object JumptoObject1: TMenuItem
        Caption = '&Jump to Object...'
        ShortCut = 16458
        OnClick = JumptoObject1Click
      end
      object MUSnapGridTo: TMenuItem
        Caption = '&Snap Grid to Object'
        OnClick = MUSnapGridToClick
      end
      object SnapViewToObject: TMenuItem
        Caption = '&Snap View to Object'
        OnClick = SnapViewToObjectClick
      end
      object SetMapGeoBackfaceCulling: TMenuItem
        Caption = 'Disable backface culling'
        OnClick = SetMapGeoBackfaceCullingClick
      end
      object Viewtogrid1: TMenuItem
        Caption = '&View to grid'
        ShortCut = 113
        OnClick = Viewtogrid1Click
      end
      object GridtoView1: TMenuItem
        Caption = '&Grid to View'
        ShortCut = 114
        OnClick = GridtoView1Click
      end
      object N3DPreviewtoItem1: TMenuItem
        Caption = '&3D Preview to Item'
        ShortCut = 116
        OnClick = N3DPreviewtoItem1Click
      end
      object miKeyboard: TMenuItem
        Caption = '&Keyboard'
        object miMap: TMenuItem
          Caption = '&Map Control'
        end
        object miGrid: TMenuItem
          Caption = '&Grid Control'
        end
        object miSel: TMenuItem
          Caption = '&Selection'
        end
        object miTex: TMenuItem
          Caption = '&Texturing'
        end
        object miRecovery: TMenuItem
          Caption = '&Recovery'
        end
        object miEdit: TMenuItem
          Caption = '&Editing'
        end
        object miOther: TMenuItem
          Caption = 'Other'
        end
      end
    end
    object Tools1: TMenuItem
      Caption = '&Tools'
      object CalcLightOnLayers: TMenuItem
        Caption = 'Calculate &Lighting'
        OnClick = CalculateLights1Click
      end
      object CalcLightInSel: TMenuItem
        Caption = '... In selected sectors'
        OnClick = CalculateLights1Click
      end
      object N6: TMenuItem
        Caption = '-'
      end
      object ConsistencyCheck1: TMenuItem
        Caption = '&Consistency Check'
        ShortCut = 121
        OnClick = ConsistencyCheck1Click
      end
      object CheckResources1: TMenuItem
        Caption = 'Check &Resources'
        ShortCut = 8313
        OnClick = CheckResources1Click
      end
      object N7: TMenuItem
        Caption = '-'
      end
      object ToolWindow1: TMenuItem
        Caption = '&Tool Window'
        ShortCut = 120
        OnClick = ToolWindow1Click
      end
      object PlcedCogs1: TMenuItem
        Caption = '&Placed Cogs'
        ShortCut = 118
        OnClick = PlaceCogs1Click
      end
      object ItemEditor1: TMenuItem
        Caption = 'Item &Editor'
        OnClick = ItemEditor1Click
      end
      object Messages1: TMenuItem
        Caption = '&Messages'
        OnClick = Messages1Click
      end
      object N3DOHierarchy1: TMenuItem
        Caption = '&3DO Hierarchy'
        OnClick = N3DOHierarchy1Click
      end
      object N8: TMenuItem
        Caption = '-'
      end
      object Options1: TMenuItem
        Caption = '&Options'
        OnClick = Options1Click
      end
      object Toolbar1: TMenuItem
        Caption = 'Map &Settings'
        ShortCut = 115
        OnClick = Toolbar1Click
      end
      object N3DPreview1: TMenuItem
        Caption = '&3D Preview'
        ShortCut = 123
        OnClick = N3DPreview1Click
      end
      object N9: TMenuItem
        Caption = '-'
      end
      object Levelheadereditor1: TMenuItem
        Caption = 'Level &Header Editor'
        ShortCut = 117
        OnClick = Levelheadereditor1Click
      end
      object EpisodeEditor1: TMenuItem
        Caption = '&Episode Editor'
        ShortCut = 119
        OnClick = EpisodeEditor1Click
      end
      object TemplateCreator1: TMenuItem
        Caption = '&Template Creator'
        OnClick = TemplateCreator1Click
      end
      object CutsceneHelper1: TMenuItem
        Caption = 'Cutscene Helper'
        Visible = False
        OnClick = CutsceneHelper1Click
      end
    end
    object Plugins: TMenuItem
      Caption = '&Plugins'
    end
    object Help1: TMenuItem
      Caption = '&Help'
      object Topics1: TMenuItem
        Caption = '&Topics'
        Visible = False
        OnClick = Topics1Click
      end
      object JedTutor1: TMenuItem
        Caption = 'Jed &Tutor'
        Visible = False
        OnClick = JedTutor1Click
      end
      object TutorialsonMassassiNet1: TMenuItem
        Caption = 'Tutorials on Massassi.Net'
        OnClick = TutorialsonMassassiNet1Click
      end
      object N2: TMenuItem
        Caption = '-'
      end
      object About1: TMenuItem
        Caption = '&About'
        OnClick = About1Click
      end
    end
  end
  object SaveGOB: TSaveDialog
    Filter = 'GOB Files|*.gob|GOO Files|*.goo'
    Options = [ofOverwritePrompt, ofHideReadOnly, ofNoChangeDir, ofPathMustExist]
    Title = 'Select a GOB to create'
    Left = 16
    Top = 104
  end
  object Save3DO: TSaveDialog
    DefaultExt = '3do'
    Filter = '3DO Files (*.3DO)|*.3DO'
    Options = [ofOverwritePrompt, ofHideReadOnly, ofNoChangeDir, ofPathMustExist]
    Title = 'Save 3DO'
    Left = 56
    Top = 104
  end
  object SaveTimer: TTimer
    Interval = 60000
    OnTimer = SaveTimerTimer
    Left = 184
    Top = 64
  end
  object ImageList1: TImageList
    Left = 328
    Top = 96
  end
  object SaveAs: TSaveDialog
    Options = [ofOverwritePrompt, ofHideReadOnly, ofPathMustExist]
    Left = 32
    Top = 200
  end
end
