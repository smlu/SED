object Preview3D: TPreview3D
  Left = 448
  Top = 162
  BorderIcons = [biSystemMenu]
  BorderStyle = bsSingle
  Caption = '3D Preview'
  ClientHeight = 291
  ClientWidth = 401
  Color = clBtnFace
  DefaultMonitor = dmDesktop
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -15
  Font.Name = 'Segoe UI'
  Font.Style = []
  FormStyle = fsStayOnTop
  KeyPreview = True
  Menu = MainMenu1
  OnActivate = FormActivate
  OnAfterMonitorDpiChanged = FormAfterMonitorDpiChanged
  OnBeforeMonitorDpiChanged = FormBeforeMonitorDpiChanged
  OnClose = FormClose
  OnCreate = FormCreate
  OnDblClick = FormDblClick
  OnHide = FormHide
  OnKeyDown = FormKeyDown
  OnMouseDown = FormMouseDown
  OnMouseMove = FormMouseMove
  OnMouseUp = FormMouseUp
  OnMouseWheel = FormMouseWheel
  OnPaint = FormPaint
  OnShow = FormShow
  PixelsPerInch = 144
  TextHeight = 20
  object MainMenu1: TMainMenu
    Left = 80
    Top = 40
    object Preview: TMenuItem
      Caption = '&Preview'
      object MenuHighlightSelection: TMenuItem
        AutoCheck = True
        Caption = 'Highlight Selection'
        Checked = True
        OnClick = MenuHighlightSelectionClick
      end
      object MenuNoClipSelectionOutline: TMenuItem
        AutoCheck = True
        Caption = 'Don'#39't Clip Selection Outline'
        Hint = 'Draw selection outilne through walls'
        OnClick = MenuNoClipSelectionOutlineClick
      end
      object N3: TMenuItem
        Caption = '-'
      end
      object MenuHighlightFloor: TMenuItem
        AutoCheck = True
        Caption = 'Highlight Solid &Floor'
        OnClick = MenuHighlightFloorClick
      end
      object MenuHighlightSolidSurface: TMenuItem
        AutoCheck = True
        Caption = 'Highlight Solid &Surface'
        Hint = 'Higlight surfaces with collision flag set and impassable adjoins'
        OnClick = MenuHighlightSolidSurfaceClick
      end
      object MenuHighlightSpecialSurfaces: TMenuItem
        AutoCheck = True
        Caption = 'Highlight &Surfaces With Special Properties'
        Hint = 
          'Higlight surfaces which have special properties i.e. surfaceflag' +
          's or faceflags.\nFor IJIM world surfaces: cog linked, ledge, cli' +
          'mable, whip aim and track surface.\nIJIM Thing model which has f' +
          'ace: ledge or whip aim.\nJKDF&MOTS cog linked world surfaces. '
        OnClick = MenuHighlightSpecialSurfacesClick
      end
      object N2: TMenuItem
        Caption = '-'
      end
      object MenuRenderFog: TMenuItem
        AutoCheck = True
        Caption = 'Render &Fog'
        Checked = True
        OnClick = MenuRenderFogClick
      end
      object MenuRenderFullyLit: TMenuItem
        AutoCheck = True
        Caption = 'Render Fully &Lit'
        OnClick = MenuRenderFullyLitClick
      end
      object N1: TMenuItem
        Caption = '-'
      end
      object Reload: TMenuItem
        Caption = '&Reload'
        ShortCut = 123
        OnClick = ReloadClick
      end
      object Close1: TMenuItem
        Caption = '&Close'
        OnClick = Close1Click
      end
    end
    object Commands1: TMenuItem
      Caption = '&Commands'
      OnClick = Commands1Click
      object miControl: TMenuItem
        Caption = '&Control'
      end
      object miEdit: TMenuItem
        Caption = '&Editing'
      end
      object miTex: TMenuItem
        Caption = '&Texturing'
      end
      object SetViewcamera1: TMenuItem
        Caption = 'Set Map &View To Camera'
        OnClick = SetViewcamera1Click
      end
      object SetCameraToItem1: TMenuItem
        Caption = 'Set Camera To Item'
        ShortCut = 116
        OnClick = SetCameraToItem1Click
      end
      object MenuSetThingToCamera: TMenuItem
        Caption = 'Set Thing To Camera'
        OnClick = MenuSetThingToCameraClick
      end
    end
    object Settings1: TMenuItem
      Caption = '&Options'
      OnClick = Settings1Click
    end
  end
end
