unit Jed_Main;

interface

uses
    Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
    ComCtrls, StdCtrls, Menus, GlobalVars, J_Level, misc_utils, ExtCtrls,
    Render, OGL_render, FileOperations, lev_utils, Geometry, Buttons, System.Math,
    Containers, Files, U_templates, Prefab, ListRes, sft_render, u_multisel,
    jed_plugins, sed_plugins, values, shellApi, U_copypaste, FileCtrl, u_undo,
    SED_COM, JED_COM, tbar_tools, u_3dos, System.ImageList, Vcl.ImgList, Types;

Const
    { Map modes }
    MM_SC = 0;
    MM_SF = 1;
    MM_VX = 2;
    MM_TH = 3;
    MM_ED = 4;
    MM_LT = 5;
    MM_FR = 6;
    MM_Extra = 10;

    { Mouse Modes }
    MM_Select = 0;
    MM_Cleave = 1;
    MM_Drag = 2;
    MM_TranslateCam = 3;
    MM_RotateCam = 4;
    MM_TranslateGrid = 5;
    MM_RotateGrid = 6;
    MM_CreateSector = 7;
    MM_RectSelect = 8;
    MM_Ruler = 9;

    All_layers = -100;

    { Toolbar Panel IDs }
    PI_XYZ = 0;

    { ShiftTexture constants }
    st_left = 0;
    st_right = 1;
    st_up = 2;
    st_down = 3;

    { SectorChanged() constants }
    sc_vertices = 1;
    sc_values = 2;
    sc_all = -1;

    { ScaleObject constants }
    sc_ScaleTX = 1;
    sc_scaleX = 2;
    sc_scaleY = 4;
    sc_scaleZ = 8;
    sc_scaleGrid = 16;

    ro_up = 0;
    ro_down = 1;
    { Cursor constants }
    crSaber = 1;
    crEye   = 2;

    { SetThingView constants }
    cv_Dots = 0;
    cv_Boxes = 1;
    cv_Wireframes = 2;

    { OpenProject flags }
    op_open = 0;
    op_revert = 1;

    { Multiselection modes }
    mm_Toggle = 0;
    mm_Add = 1;
    mm_Subtract = 2;

type

    TExtraLine = class
        v1, v2: TVertex;
        name: string;
        Constructor Create;
        Destructor Destroy; override;
    end;

    TExtraVertex = class(TVertex)
        name: string;
    end;

    TExtraPolygon = class(TIsolatedPolygon)
        name: string;
    end;

    TOnExtraMove = procedure(sender: TObject; continued: boolean) of object;
    TSectorCallback = reference to procedure(sec: TJKSector) ;
    TSurfaceCallback = reference to procedure(sec: TJKSurface) ;

    TJedMain = class(TForm)
        MainMenu: TMainMenu;
        FileMenu: TMenuItem;
        Exit1: TMenuItem;
        OpenMenu: TMenuItem;
        New1: TMenuItem;
        RecentBar: TMenuItem;
        Help1: TMenuItem;
        Topics1: TMenuItem;
        About1: TMenuItem;
        N2: TMenuItem;
        Save1: TMenuItem;
        Import1: TMenuItem;
        SaveAs: TSaveDialog;
        Tools1: TMenuItem;
        ConsistencyCheck1: TMenuItem;
        Tbar: TPanel;
        BNSC: TSpeedButton;
        BNSF: TSpeedButton;
        BNVX: TSpeedButton;
        BNTH: TSpeedButton;
        BNLT: TSpeedButton;
        BNED: TSpeedButton;
        ToolWindow1: TMenuItem;
        PlcedCogs1: TMenuItem;
        ItemEditor1: TMenuItem;
        Commands1: TMenuItem;
        MUNextObject: TMenuItem;
        MUPrevObject: TMenuItem;
        MUSnapGridTo: TMenuItem;
        Options1: TMenuItem;
        GobProject1: TMenuItem;
        SaveGOB: TSaveDialog;
        Viewtogrid1: TMenuItem;
        Toolbar1: TMenuItem;
        JedTutor1: TMenuItem;
        N3DPreview1: TMenuItem;
        SaveAs1: TMenuItem;
        Panel1: TPanel;
        PXYZ: TPanel;
        PMsg: TPanel;
        Messages1: TMenuItem;
        LBXYZ: TLabel;
        SaveJKLGob1: TMenuItem;
        GridtoView1: TMenuItem;
        Edit1: TMenuItem;
        Find1: TMenuItem;
        FindNext1: TMenuItem;
        Levelheadereditor1: TMenuItem;
        View1: TMenuItem;
        ViewthingsAs1: TMenuItem;
        Dots: TMenuItem;
        Boxes: TMenuItem;
        Wireframes: TMenuItem;
        NewMOTSProject1: TMenuItem;
        PNProjType: TPanel;
        EpisodeEditor1: TMenuItem;
        CheckResources1: TMenuItem;
        Save3DO: TSaveDialog;
        N1: TMenuItem;
        N3: TMenuItem;
        N4: TMenuItem;
        ExportSectoras3DO1: TMenuItem;
        ReloadTemplates1: TMenuItem;
        MakeaBackupCopy1: TMenuItem;
        CalcLightOnLayers: TMenuItem;
        Reverttosaved1: TMenuItem;
        miKeyboard: TMenuItem;
        miMap: TMenuItem;
        miSel: TMenuItem;
        miTex: TMenuItem;
        miOther: TMenuItem;
        miRecovery: TMenuItem;
        BNFR: TSpeedButton;
        SaveJKLGOBandTest1: TMenuItem;
        Plugins: TMenuItem;
        miCopy: TMenuItem;
        miPaste: TMenuItem;
        N5: TMenuItem;
        N3DPreviewtoItem1: TMenuItem;
        PMsel: TPanel;
        Multiselectionmode1: TMenuItem;
        miToggle: TMenuItem;
        miAdd: TMenuItem;
        miSubtract: TMenuItem;
        TemplateCreator1: TMenuItem;
        SaveTimer: TTimer;
        miUndo: TMenuItem;
        N6: TMenuItem;
        N7: TMenuItem;
        N8: TMenuItem;
        N9: TMenuItem;
        CalcLightInSel: TMenuItem;
        SnapViewToObject: TMenuItem;
        JumptoObject1: TMenuItem;
        miGrid: TMenuItem;
        miEdit: TMenuItem;
        ImageList1: TImageList;
        HideThings: TMenuItem;
        HideLights: TMenuItem;
        ExportSectorasShape1: TMenuItem;
        TutorialsonMassassiNet1: TMenuItem;
        CutsceneHelper1: TMenuItem;
        N3DOHierarchy1: TMenuItem;
        BNEX: TSpeedButton;
        NewIJIMProject: TMenuItem;
        SetMapGeoBackfaceCulling: TMenuItem;

        procedure Exit1Click(sender: TObject);
        procedure OpenMenuClick(sender: TObject);
        procedure FormCreate(sender: TObject);
        procedure Save1Click(sender: TObject);
        procedure FormResize(sender: TObject);
        procedure FormKeyDown(sender: TObject; var Key: Word;
          Shift: TShiftState);
        procedure FormMouseDown(sender: TObject; Button: TMouseButton;
          Shift: TShiftState; X, Y: Integer);
        procedure Import1Click(sender: TObject);
        procedure FormPaint(sender: TObject);
        procedure PlaceCogs1Click(sender: TObject);
        procedure FormMouseUp(sender: TObject; Button: TMouseButton;
          Shift: TShiftState; X, Y: Integer);
        procedure FormMouseMove(sender: TObject; Shift: TShiftState;
          X, Y: Integer);
        procedure CalculateLights1Click(sender: TObject);
        procedure ConsistencyCheck1Click(sender: TObject);
        procedure About1Click(sender: TObject);
        procedure BNSCClick(sender: TObject);
        procedure ItemEditor1Click(sender: TObject);
        procedure FormKeyUp(sender: TObject; var Key: Word; Shift: TShiftState);
        procedure MUNextObjectClick(sender: TObject);
        procedure MUPrevObjectClick(sender: TObject);
        procedure MUSnapGridToClick(sender: TObject);
        procedure ToolWindow1Click(sender: TObject);
        procedure FormShow(sender: TObject);
        procedure Options1Click(sender: TObject);
        procedure New1Click(sender: TObject);
        procedure FormDestroy(sender: TObject);
        procedure GobProject1Click(sender: TObject);
        procedure SaveJKL1Click(sender: TObject);
        procedure Viewtogrid1Click(sender: TObject);
        procedure Toolbar1Click(sender: TObject);
        procedure Topics1Click(sender: TObject);
        procedure JedTutor1Click(sender: TObject);
        procedure N3DPreview1Click(sender: TObject);
        procedure SaveAs1Click(sender: TObject);
        procedure PMsgDblClick(sender: TObject);
        procedure Messages1Click(sender: TObject);
        procedure SaveJKLGob1Click(sender: TObject);
        procedure GridtoView1Click(sender: TObject);
        procedure Find1Click(sender: TObject);
        procedure FindNext1Click(sender: TObject);
        procedure FormCloseQuery(sender: TObject; var CanClose: boolean);
        procedure Levelheadereditor1Click(sender: TObject);
        procedure WireframesClick(sender: TObject);
        procedure NewMOTSProject1Click(sender: TObject);
        procedure PNProjTypeDblClick(sender: TObject);
        procedure EpisodeEditor1Click(sender: TObject);
        procedure CheckResources1Click(sender: TObject);
        procedure ExportSectoras3DO1Click(sender: TObject);
        procedure ReloadTemplates1Click(sender: TObject);
        procedure MakeaBackupCopy1Click(sender: TObject);
        procedure Reverttosaved1Click(sender: TObject);
        procedure SaveJKLGOBandTest1Click(sender: TObject);
        procedure Edit1Click(sender: TObject);
        procedure miCopyClick(sender: TObject);
        procedure miPasteClick(sender: TObject);
        procedure N3DPreviewtoItem1Click(sender: TObject);
        procedure PMselClick(sender: TObject);
        procedure miToggleClick(sender: TObject);
        procedure TemplateCreator1Click(sender: TObject);
        procedure SaveTimerTimer(sender: TObject);
        procedure miUndoClick(sender: TObject);
        procedure SnapViewToObjectClick(sender: TObject);
        procedure JumptoObject1Click(sender: TObject);
        procedure HideThingsClick(sender: TObject);
        procedure HideLightsClick(sender: TObject);
        procedure ExportSectorasShape1Click(sender: TObject);
        procedure TutorialsonMassassiNet1Click(sender: TObject);
        procedure CutsceneHelper1Click(sender: TObject);
        procedure N3DOHierarchy1Click(sender: TObject);
        procedure NewIJIMProjectClick(Sender: TObject);
        procedure SetMapGeoBackfaceCullingClick(Sender: TObject);
        procedure FormMouseWheel(Sender: TObject; Shift: TShiftState;
          WheelDelta: Integer; MousePos: TPoint; var Handled: Boolean);

        procedure OnFixSolidSurfaces(sender: TObject);
        procedure OnFixPassableAdjoins(sender: TObject);

        procedure PickThing(th: TThing; id: integer);
        procedure PickThingAt(th: TThing; x, y, z, pitch, yaw, roll: double; id: integer);

        function IsCleaveMapMode: Boolean;

        // Renderer stuff
        function ZoomIn: boolean;
        function ZoomOut: boolean;

        procedure SetRendererGeoVertexSize;
        procedure SetRendererObjVertexSize;

    public
        { Project vars }
        AskSaveAs: boolean;
        NewLevel: boolean;
        LevelFile: String;
        Changed: boolean;
        updatestuff: boolean;

        snaptoy: boolean;

        { }
        bSnapToGrid: boolean;
        renderer: TRenderer;
        ListID: Integer;
        hdc: HDC;
        hglc: HGLRC;
        
        { different mode vars }
        Cur_SC, Cur_SF, Cur_VX, Cur_ED, Cur_LT, Cur_TH, Cur_FR, Cur_EX: Integer;
        mapMode: Integer;
        bMapGeoBackfaceCull: Boolean;
        mouseMode: Integer;
        mselMode: Integer;
        thingView: Integer;
        
        { Private declarations }
        { Select vars }
        lastSelXY: TPoint;
        csel: Integer;
        
        { Drag variables }
        bDragged: boolean;
        dragOrg: TVector;
        dragRefOrg: TVector;
        
        { Cleave vars }
        cleaveOrg2: TPoint;
        cleaveOrg: TVector;
        cleaveCurPos2: TPoint;
        bCleaveStarted: boolean;
        bCreateSCStarted: boolean;        
        
        { Whip ruler vars}
        rulerOrg2: TPoint;
        rulerCurPos2: TPoint;
        rulerOrg: TVector;
        bRulerStarted: boolean;
        
        { Rect select vars }
        rectSelOrg: TPoint;
        reactSelPos: TPoint;

        bRectSelStarted: boolean;

        { Grid translate vars }
        gridTranslateOrg2: TPoint;
        gridTranslateOrg: TVector;
        
        { Cam translate }
        camTranslatePos: TPoint;
        
        { Cam Rotate vars }
        camRotOrgX, camRotOrgY, camRotOrgZ: TVector; // cam view basis vectors
        
        { Stitch variables }
        stitchSec: TJKSector;
        stitchSurf: TJKSurface;

        tcube: TPolygons;

        { Multi selection vars }
        scsel: TSCMultisel;
        sfsel: TSFMultisel;
        vxsel: TVXMultisel;
        thsel: TTHMultisel;
        ltsel: TLTMultisel;
        edsel: TEDMultisel;
        frsel: TFRMultisel;
        exsel: TTHMultisel;

        { Old style controls vars }
        rPch, rYaw, rRol: double;

        { Extra objects - for plugins }
        extraObjs: TObjList;
        extraObjsName: string;
        onExtraSelect: TNotifyEvent;
        onExtraMove: TOnExtraMove;
        
        { Hide thing/lights bools }
        bThingsHidden, bLightsHidden: boolean;

        { 3DO Hierarchy }

        Procedure WMEraseBkg(var msg: TMessage); message WM_ERASEBKGND;
        Procedure WMQueryPal(var msg: TMessage); message WM_QUERYNEWPALETTE;
        Procedure WMPalCHANGED(var msg: TMessage); message WM_PALETTECHANGED;
        procedure RecentClick(sender: TObject);
        procedure KBCommandClick(sender: TObject);
        Procedure AddKBItem(mi: TMenuItem; const name: string; c: char; sc: TShiftState); overload;
        Procedure AddKBItem(mi: TMenuItem; const name: string; onClick: TNotifyEvent); overload;

        Function DoSaveJKL: boolean;
        Function GobProj(const name: string): boolean;
        procedure PluginClick(sender: TObject);
        Procedure LoadPlugins;
        Procedure Import3DO(const name: string; const clearCurrentLevel: Boolean);

        procedure ExceptHandler(sender: TObject; E: Exception);
        Procedure SetRendFromPYR;
        Procedure GetPYR;
        Function GetTargetJKLName: string;
        Procedure UseInCog;
        Procedure GotoXYZ(X, Y, z: double; force: boolean);
        Procedure CenterViewOnObject(force: boolean);
        Procedure ConnectSFs;
        Procedure ConnectSCs;
        Procedure JumpToObject;

        Procedure SaveSelThingsUndo(const rname: string; change: Integer);
        Procedure SaveSelLightsUndo(const rname: string; change: Integer);
        Procedure SaveSelFramesUndo(const rname: string; change: Integer);

        Procedure SaveSelSurfUndo(const rname: string; change, how: Integer);
        Procedure SaveSelSecUndo(const rname: string; change, how: Integer);
        Procedure SaveSelVertUndo(const rname: string; change: Integer);
        Procedure SaveSelEdgeUndo(const rname: string; change: Integer);

        Procedure NewProject;
        Procedure OpenProject(const filename: string; how: Integer);
        Function SaveProject: boolean;
        Function SysSaveProject(askSave: boolean): boolean;
        Procedure SaveJKLto(const name: string);
        Procedure SaveToFile(const filePath: string; ext: string; setSaved: Boolean = True);
        Procedure SnapGridToObject;
        Function GetGridPosAt(atX, atY: Integer; var X, Y, z: double): boolean; // projects 2D to 3D pos at grid pos or nearest grid pos  if SnapToGrid is true
        Function GetPosAt(atX, atY: Integer; snapToGrid: Boolean; var x, y, z: double): boolean; overload; // projects 2D x,y pos to 3D pos at camera view pos or grid pos if snapToGrid is true
        Function GetPosAt(atX, atY: Integer; snapToGrid: Boolean; var pos: TVector): boolean; overload; // projects 2D x,y pos to 3D pos at camera view pos or grid pos if snapToGrid is true
        Function GetPosAt(atX, atY: Integer; var x, y, z: double): boolean; overload; // projects 2D x,y pos to 3D pos at camera view pos or grid pos if bSnapToGrid is true
        Function GetPosAt(atX, atY: Integer; var pos: TVector): boolean; overload; // projects 2D x,y pos to 3D pos at camera view pos or grid pos if bSnapToGrid is true

        Function GetSnappedPosAt(atX, atY: Integer; snapToVertex: boolean; snapToGrid: boolean; var x, y, z: double): boolean; overload; // projects 2D x,y pos to 3D pos at camera view pos. The pos can be snapped to grid or nearest word geo vertex
        Function GetSnappedPosAt(atPos: TPoint; snapToVertex: boolean; snapToGrid: boolean; var pos: TVector): boolean; overload;
        Function GetSnappedPosAt(atX, atY: Integer; snapToVertex: boolean; snapToGrid: boolean; var pos: TVector): boolean; overload;

        Function GetSnappedPosAt(atX, atY: Integer; snapToVertex: boolean; var x, y, z: double): boolean; overload; // projects 2D x,y pos to 3D pos at camera view pos. The pos can be snapped to grid (bSnapToGrid = True) or nearest word geo vertex
        Function GetSnappedPosAt(atPos: TPoint; snapToVertex: boolean; var pos: TVector): boolean; overload;
        Function GetSnappedPosAt(atX, atY: Integer; snapToVertex: boolean; var pos: TVector): boolean; overload;

        Function GetMousePos(var X, Y: Integer): boolean;

        Procedure GetCurObjRefXYZ(var rx, ry, rz: double);
        Procedure Do_SelectAt(X, Y: Integer; fore: boolean);

        Procedure Do_StartCleave(X, Y: Integer; snapToVertex: boolean);
        Procedure Do_ProceedCleave(X, Y: Integer; snapToVertex: boolean);
        Procedure Do_EndCleave(X, Y: Integer; snapToVertex: boolean);

        Procedure Do_StartRuler(X, Y: Integer; snapToVertex: boolean);
        Procedure Do_ProceedRuler(X, Y: Integer; snapToVertex: boolean);
        Procedure Do_EndRuler(X, Y: Integer; snapToVertex: boolean);

        Procedure Do_StartDrag(X, Y: Integer);
        Procedure Do_ProceedDrag(X, Y: Integer; snapToVx: boolean; snapToAxis, yaxis: boolean);
        Procedure Do_EndDrag(X, Y: Integer);

        Procedure Do_StartTranslateCam(X, Y: Integer);
        Procedure Do_TranslateCam(X, Y: Integer);

        Procedure Do_StartRotateCam(X, Y: Integer);
        Procedure Do_RotateCam(X, Y: Integer);

        Procedure Do_StartTranslateGrid(X, Y: Integer);
        Procedure Do_TranslateGrid(X, Y: Integer);

        Procedure Do_StartRotateGrid(X, Y: Integer);
        Procedure Do_RotateGrid(X, Y: Integer);

        Procedure Do_StartCreateSC(X, Y: Integer; snapToVertex: boolean);
        Procedure Do_ProceedCreateSC(X, Y: Integer; snapToVertex: boolean);
        Procedure Do_EndCreateSC(X, Y: Integer; snapToVertex: boolean);

        Procedure Do_StartRectSelect(X, Y: Integer);
        Procedure Do_ProceedRectSelect(X, Y: Integer);
        Procedure Do_EndRectSelect(X, Y: Integer);

        Procedure ResetEditor(DefaultParams: boolean);
        Procedure ResettingEditor;
        Procedure ResetCamera;
        Procedure SetLevelName;
        Procedure ReDraw;
        Procedure DrawThing(th: TThing);
        Procedure DrawThingAt(th: TThing; X, Y, z, pch, yaw, rol: single);
        Procedure DrawSelThing(th: TThing);
        Procedure DrawFrame(fr: TTPLValue);
        Procedure DrawSelFrame(th, fr: Integer);

        Procedure SetMouseMode(mm: Integer);
        Procedure SetMapMode(mm: Integer);
        Procedure EnableMapGeoBackfaceCull(enable: Boolean);
        Procedure SetCurSC(sc: Integer);
        Procedure SetCurSF(sc, SF: Integer);
        Procedure SetCurVX(sc, VX: Integer);
        Procedure SetCurED(sc, SF, ED: Integer);
        Procedure SetCurLT(LT: Integer);
        Procedure SetCurTH(th: Integer);
        Procedure SetCurFR(th, fr: Integer);
        Procedure SetCurEX(EX: Integer);
        Procedure AddLightsAt(X, Y: Integer);
        Procedure AddThingsAt(X, Y: Integer);
        Procedure AddSectorAt(X, Y: Integer);
        Procedure AddFramesAt(X, Y: Integer);
        Procedure CopySectorsAt(X, Y: Integer);
        Procedure NextObject;
        Procedure PreviousObject;
        Procedure NextObjectInSurface;
        Procedure PreviousObjectInSurface;
        Procedure EditObject;
        Procedure GotoSC(sc: Integer);
        Procedure GotoSF(sc, SF: Integer);
        Procedure GotoTH(th: Integer);
        Function LayerThing(n: Integer): boolean;
        Procedure CancelMouseMode;
        Procedure Goto_Adjoin;
        Procedure SetViewToGrid;
        Procedure SetGridToView;
        Procedure ShiftTexture(how: Integer);
        Procedure RotateTexture(how: Integer);
        Procedure ScaleTexture(how: Integer);

        { Notification procedures }
        Procedure SectorChanged(s: TJKSector);
        Procedure SectorAdded(s: TJKSector);
        Procedure SectorDeleted(s: TJKSector);
        Procedure ThingChanged(th: TJKThing);
        Procedure ThingAdded(th: TJKThing);
        Procedure ThingDeleted(th: TJKThing);
        Procedure LightAdded(lt: TSedLight);
        Procedure LightChanged(lt: TSedLight);
        Procedure LightDeleted(lt: TSedLight);

        Procedure RotateObject(angle: double; axis: Integer);
        Procedure ScaleObject(sfactor: double; how: Integer);
        Procedure TranslateObject(dx, dy, dz: double);
        Procedure MakeDoor;
        Procedure StartStitch;
        Procedure DoStitch(copyProps: Boolean);
        Procedure StraightenTexture(zero, rot90: boolean);
        Procedure SetCam(X, Y, z, pch, yaw, rol: double);
        Procedure GetCam(var X, Y, z, pch, yaw, rol: double);
        Procedure RaiseObject(how: Integer);
        Function IsSelValid: boolean;
        Procedure VerifySelection;
        Procedure VerifyMultiSelection;

        Procedure SyncRecents;
        Procedure AddRecent(const s: string);
        Procedure LevelChanged;
        Procedure UpdateItemEditor;
        Function AskSave: boolean;
        Procedure SetThingView(how: Integer);
        Procedure SetMSelMode(mode: Integer);
        Procedure UpdateSelCount;
        Procedure UpdateThingData(th: TJKThing);
        Procedure LoadTemplates;
        Procedure SetProjectTypeIndicator;
        Function ShortJKLName: string;
        Procedure DO_MultiSelect;
        Procedure ClearMultiSelection;
        Procedure CleaveBy(const norm: TVector; X, Y, z: double);
        Procedure CreateRenderer;
        Procedure FlipObject(how: Integer);
        Procedure DeleteFrame(Cur_TH, Cur_FR: Integer);
        Procedure DO_SelSC(sc: Integer);
        Procedure DO_SelSF(sc, SF: Integer);
        Procedure DO_SelVX(sc, VX: Integer);
        Procedure DO_SelED(sc, SF, ED: Integer);
        Procedure DO_SelTH(th: Integer);
        Procedure DO_SelFR(th, fr: Integer);
        Procedure DO_SelLT(LT: Integer);

        Function GetCurObjForCog(ct: TCOG_Type): TObject;
        Procedure LayerThings;

        Procedure BringThingToSurf(th, sc, SF: Integer);
        Procedure BringLightToSurf(lt, sc, SF: Integer);
        Procedure AddThingOnCurSurf;
        Procedure AddThingAtXYZPYR(X, Y, z, pch, yaw, rol: double);
        Procedure ClearExtraObjs;
        Function AddExtraVertex(X, Y, z: double; const name: string): Integer;
        Function AddExtraLine(x1, y1, z1, x2, y2, z2: double; const name: string): Integer; overload;
        Function AddExtraLine(x, y, z: double; direction: TVector; length: double; const name: string): Integer; overload;
        Function AddExtraLine(startPos: TVector; endPos: TVector; const name: string): Integer; overload;
        Function TranslateExtras(Cur_EX: Integer; dx, dy, dz: double): Integer;
        Procedure DeleteExtraObj(n: Integer);

        Procedure SetHideLights(hide: boolean);
        Procedure SetHideThings(hide: boolean);

        procedure ForSelectedSectorsDo(callback: TSectorCallback);
        procedure ForSelectedSurfacesDo(callback: TSurfaceCallback);

        procedure ResetAutoSaveTimer;
    end;

var
    JedMain: TJedMain;

Procedure LoadThing3DO(th: TJKThing; force: boolean);
Procedure SysGetPYR(const X, Y, z: TVector; var pch, yaw, rol: double);
Procedure LoadDLLPlugin(const dll: string);

implementation

uses Item_edit, U_CogForm, Cons_checker, FileDialogs, U_Tools,
    U_Options, Jed_about1, math, ProgressDialog, U_tbar, U_Preview, U_msgForm,
    Q_Sectors, Q_surfs, Q_things, U_lheader, U_Medit, u_DFI,
    u_errorform, U_tplcreate, ResourcePicker, u_cscene, u_3doform,
    graph_files;

Procedure TJedMain.WMEraseBkg(var msg: TMessage);
begin
    msg.Result := 0;
end;

Procedure TJedMain.WMQueryPal(var msg: TMessage);
begin
    msg.Result := renderer.HandleWMQueryPal;
end;

Procedure TJedMain.WMPalCHANGED(var msg: TMessage);
begin
    msg.Result := renderer.HandleWMChangePal;
end;

{$R *.DFM}

procedure TJedMain.Exit1Click(sender: TObject);
begin
    Application.Terminate;
end;

Procedure TJedMain.NewProject;
var
    sec: TJKSector;
    surf: TJKSurface;
    v: TJKVertex;
    th: TJKThing;
    LT: TSedLight;
    i: Integer;
begin
    for i := 0 to level.things.Count - 1 do
        Free3DO(level.things[i].a3DO);

    level.Clear;
    level.MasterCMP := '';
    level.kind := CurrentProject;

    sec := level.NewSector;
    level.sectors.Add(sec);

    v := sec.newVertex;
    v.X := -1;
    v.Y := 1;
    v.z := 0;

    v := sec.newVertex;
    v.X := -1;
    v.Y := -1;
    v.z := 0;

    v := sec.newVertex;
    v.X := 1;
    v.Y := -1;
    v.z := 0;

    v := sec.newVertex;
    v.X := 1;
    v.Y := 1;
    v.z := 0;

    v := sec.newVertex;
    v.X := -1;
    v.Y := 1;
    v.z := 1;

    v := sec.newVertex;
    v.X := 1;
    v.Y := 1;
    v.z := 1;

    v := sec.newVertex;
    v.X := 1;
    v.Y := -1;
    v.z := 1;

    v := sec.newVertex;
    v.X := -1;
    v.Y := -1;
    v.z := 1;

    With sec do
    begin
        surf := NewSurface;
        sec.surfaces.Add(surf);
        surf.surfflags := surf.surfflags or SF_Floor;
        surf.AddVertex(vertices[0]);
        surf.AddVertex(vertices[1]);
        surf.AddVertex(vertices[2]);
        surf.AddVertex(vertices[3]);
        surf := NewSurface;
        sec.surfaces.Add(surf);
        surf.AddVertex(vertices[4]);
        surf.AddVertex(vertices[5]);
        surf.AddVertex(vertices[6]);
        surf.AddVertex(vertices[7]);

        surf := NewSurface;
        sec.surfaces.Add(surf);
        surf.AddVertex(vertices[2]);
        surf.AddVertex(vertices[6]);
        surf.AddVertex(vertices[5]);
        surf.AddVertex(vertices[3]);

        surf := NewSurface;
        sec.surfaces.Add(surf);
        surf.AddVertex(vertices[1]);
        surf.AddVertex(vertices[7]);
        surf.AddVertex(vertices[6]);
        surf.AddVertex(vertices[2]);

        surf := NewSurface;
        sec.surfaces.Add(surf);
        surf.AddVertex(vertices[0]);
        surf.AddVertex(vertices[4]);
        surf.AddVertex(vertices[7]);
        surf.AddVertex(vertices[1]);

        surf := NewSurface;
        sec.surfaces.Add(surf);
        surf.AddVertex(vertices[3]);
        surf.AddVertex(vertices[5]);
        surf.AddVertex(vertices[4]);
        surf.AddVertex(vertices[0]);
    end;

    th := level.NewThing;
    level.things.Add(th);
    With th do
    begin
        X := 0;
        Y := 0;
        z := 0.1;
        name := 'walkplayer';
        sec := level.sectors[0];
    end;

    lt := level.NewLight;
    level.lights.Add(lt);
    With lt do
    begin
        position.x := 0;
        position.y := 0;
        position.z := 0.5;
    end;

    for i := 0 to sec.surfaces.Count - 1 do
        sec.surfaces[i].RecalcAll;

    sec.Renumber;

    level.SetDefaultHeader;

    AskSaveAs := true;
    NewLevel  := true;
    LevelFile := 'Untitled';
    Level.AddMissingLayers;
end;

Procedure TJedMain.OpenProject(const filename: string; how: Integer);
var
    ext: string;
    i: Integer;
begin
    for i := 0 to Level.Things.Count - 1 do
        Free3DO(Level.Things[i].a3DO);
    ext := UpperCase(ExtractExt(filename));
    ResettingEditor;
    PMsg.Caption := '';
    if (ext = '.JKL') or (ext = '.NDY') then
      begin
          level.LoadFromJKL(filename);
          NewLevel  := false;
          LevelFile := filename;
          AskSaveAs := IsInContainer(filename);
          SetLevelName;
      end
    else if ext = '.JED' then
      begin
          level.LoadFromJED(filename);
          NewLevel  := false;
          LevelFile := filename;
          AskSaveAs := IsInContainer(filename);
          SetLevelName;
      end;

    SetProjectType(level.kind);
    Level.JKLPostLoad;
    AddRecent(filename);
    ResetEditor(how <> op_revert);
    Invalidate;
end;

procedure TJedMain.SaveAs1Click(sender: TObject);
begin
    SysSaveProject(true);
end;

Function TJedMain.SaveProject: boolean;
begin
    Result := SysSaveProject(AskSaveAs);
end;

Procedure TJedMain.SaveToFile(const filePath: string; ext: string; setSaved: Boolean);
begin
    BackUpFile(filePath);
    ForceDirectories(ExtractFilePath(filePath));

    level.LVisString := ToolBar.GetLVisString;
    if CompareText(ext, '.jed') = 0 then
      level.SaveToJed(filePath)
    else if (CompareText(ext, '.jkl') = 0) or (CompareText(ext,'.ndy') = 0) then
      level.SaveToJKL(filePath)
    else
      begin
        PanMessageFmt(mt_error, 'SaveTo: Unkown file format with extension ''%s''', [ext]);
        exit;
      end;

    if setSaved then
    begin
      AskSaveAs := false;
      NewLevel  := false;
      Changed   := false;
      LevelFile := filePath;
      AddRecent(filePath);
      SetLevelName;
    end;
end;

Function TJedMain.SysSaveProject(askSave: boolean): boolean;
var
    fname: string;
    t: TextFile;
begin
    Result := false;

//    if IsInContainer(LevelFile) then
//    begin
//        PanMessage(mt_error, 'File is in container - try "Save As"');
//        exit;
//    end;

    if not askSave then
      begin
        fname := LevelFile
      end
    else
      begin
        SaveAs.FileName := LevelFile;
        if not SaveAs.Execute then exit;
        fname := SaveAs.FileName;
      end;

    var ext := ExtractExt(fname);
    if CompareText(ext, '.jed') = 0 then
    begin
      var fpath := ExtractFilePath(fname);
      if not FileExists(fpath + 'episode.jk') then
      begin
          AssignFile(t, fpath + 'episode.jk');
          Rewrite(t);
          WriteLn(t, Format(EpisodeJKTpl, [ChangeFileExt(ExtractName(fname),
            '.jkl')]));
          CloseFile(t);
      end;
    end;

    SaveToFile(fname, ext);
    Result := true;
end;

//Function TJedMain.SysSaveProject(askSave: boolean): boolean;
//var
//    fpath, fname: string;
//    t: TextFile;
//begin
//    Result := false;
//    if not askSave then
//    begin
//        fname := ChangeFileExt(LevelFile, '.jed');
//        BackUpFile(fname);
//    end
//    else
//    begin
//        SaveJed.FileName := ChangeFileExt(ExtractName(LevelFile), '.jed');
//        if not SaveJed.Execute then
//            exit;
//        fname := SaveJed.FileName;
//        BackUpFile(fname);
//    end;
//    fpath := ExtractFilePath(fname);
//    if not FileExists(fpath + 'episode.jk') then
//    begin
//        AssignFile(t, fpath + 'episode.jk');
//        Rewrite(t);
//        WriteLn(t, Format(EpisodeJKTpl, [ChangeFileExt(ExtractName(fname),
//          '.jkl')]));
//        CloseFile(t);
//    end;
//    SaveJEDTo(fname);
//    Result := true;
//end;

procedure TJedMain.OpenMenuClick(sender: TObject);
begin
    if Not askSave then
        exit;
    With GetFileOpen do
    begin
        Filter := 'All acceptable files|*.JKL;*.NDY;*.JED;*.GOB;*.GOO|Jed files (*.JED)|*.JED|JK Files (*goo;*.gob;*.jkl)|*.GOO;*.GOB;*.JKL|Infernal Machine Files (*.gob;*.ndy)|*.GOB;*.NDY';
        If Execute then
            OpenProject(FileName, op_open);
    end;
end;

procedure TJedMain.Reverttosaved1Click(sender: TObject);
begin
    if AskSaveAs then
    begin
        ShowMessage('The project wasn''t saved!');
        exit;
    end;
    if ConfirmRevert and Changed then
        if MsgBox('Revert To Saved?', 'Warning', MB_YESNO) <> ID_YES then
            exit;

    OpenProject(LevelFile, op_revert);
end;

Procedure BoxToPolys(const box: TThingBox; cube: TPolygons);
begin
    With cube, box do
    begin
        With VXList[0] do
        begin
            X := x1;
            Y := y2;
            z := z1;
        end;
        With VXList[1] do
        begin
            X := x1;
            Y := y1;
            z := z1;
        end;
        With VXList[2] do
        begin
            X := x2;
            Y := y1;
            z := z1;
        end;
        With VXList[3] do
        begin
            X := x2;
            Y := y2;
            z := z1;
        end;

        With VXList[4] do
        begin
            X := x1;
            Y := y2;
            z := z2;
        end;
        With VXList[5] do
        begin
            X := x2;
            Y := y2;
            z := z2;
        end;
        With VXList[6] do
        begin
            X := x2;
            Y := y1;
            z := z2;
        end;
        With VXList[7] do
        begin
            X := x1;
            Y := y1;
            z := z2;
        end;
    end;
end;

Procedure TJedMain.DrawThing(th: TThing);
begin
    With th do
        DrawThingAt(th, X, Y, z, pch, yaw, rol);
end;

Procedure TJedMain.DrawThingAt(th: TThing; x, y, z, pch, yaw, rol: single);
var
    i: Integer;
    var cpclr: TRColor;
    function highlight(clr: TRColor): TRColor;
    begin
      var hint: Integer := 152;
      Result.r := System.Math.Min(clr.r + hint, 255);
      Result.g := System.Math.Min(clr.g + hint, 255);
      Result.b := System.Math.Min(clr.b + hint, 255);
    end;
begin
    var clr := renderer.Front_clr;
    cpclr := renderer.Front_clr;
    case thingView of
        cv_Boxes:
            begin
                if th.bbox.x1 <> th.bbox.x2 then
                begin
                  BoxToPolys(th.bbox, tcube);
                  renderer.DrawPolygonsAt(tcube, x, y, z, pch, yaw, rol);
                end;
            end;
        cv_Wireframes:
            begin
                if th.a3DO <> nil then
                begin
                  cpclr := highlight(cpclr);
                  for i := 0 to th.a3DO.Meshes.Count - 1 do
                      with th.a3DO.meshes[i] do
                          renderer.DrawPolygonsAt(faces, x, y, z, pch, yaw, rol);
                end;
            end;
    end;

    renderer.SetColor(CL_FRONT, cpclr.r, cpclr.g, cpclr.b);
    renderer.DrawVertex(x, y, z);
    renderer.SetColor(CL_FRONT, clr.r, clr.g, clr.b);
end;

Procedure TJedMain.DrawSelThing(th: TThing);
var
    d: TVector;
    v1, v2: TJKVertex;
    size: double;
    i: Integer;
begin
    renderer.SetCulling(R_CULLNONE);
    DrawThing(th);
    d.dx := 0;
    d.dy := 0.2;
    d.dz := 0;
    RotateVector(d, th.pch, th.yaw, th.rol);

    if not ToolBar.IsLayerVisible(th.layer) then
      exit;

    v1 := TJKVertex.Create;
    v2 := TJKVertex.Create;

    v1.x := th.x;
    v1.y := th.y;
    v1.z := th.z;

    v2.x := v1.X + d.dx;
    v2.y := v1.Y + d.dy;
    v2.z := v1.z + d.dz;
    renderer.DrawLine(v1, v2);
    v1.free;
    v2.free;
end;

Procedure TJedMain.DrawSelFrame(th, fr: Integer);
var
    d: TVector;
    v1, v2: TJKVertex;
    size: double;
    i: Integer;
    thing: TJKThing;
    X, Y, z, pch, yaw, rol: double;
begin
    if not CheckFrameIndex(level, th, fr) or (fr = -1) then
        exit;

    thing := level.things[th];
    thing.vals[fr].GetFrame(X, Y, z, pch, yaw, rol);
    renderer.DrawVertex(X, Y, z);

    renderer.SetCulling(R_CULLNONE);
    DrawThingAt(thing, X, Y, z, pch, yaw, rol);
    d.dx := 0;
    d.dy := 0.2;
    d.dz := 0;
    RotateVector(d, pch, yaw, rol);

    if not ToolBar.IsLayerVisible(thing.layer) then
      exit;

    v1 := TJKVertex.Create;
    v2 := TJKVertex.Create;

    v1.X := X;
    v1.Y := Y;
    v1.z := z;

    v2.X := v1.X + d.dx;
    v2.Y := v1.Y + d.dy;
    v2.z := v1.z + d.dz;
    renderer.DrawLine(v1, v2);
    v1.free;
    v2.free;
end;

Procedure TJedMain.DrawFrame(fr: TTPLValue);
var
    x, y, z, pch, yaw, rol: double;
begin
    if fr.atype <> at_frame then
        exit;
    fr.GetFrame(x, y, z, pch, yaw, rol);
    renderer.DrawVertex(x, y, z);
end;

Procedure TJedMain.ReDraw;
type
  TRenderMode = (RM_VERTEX = 0, RM_POLYGON);
var
    s, th: Integer;
    v1, v2: TJKVertex;
    sec: TJKSector;
    surf: TJKSurface;
    thing: TJKThing;
    light: TSedLight;
    en: TVector;
    dx, dy, dz, pch, yaw, rol: double;
    bbox, cbox: TBox;
    wx, wy, sc, SF, VX, ED, fr: Integer;
    obj: TObject;
    ps: double;

    procedure SetCullingAndDrawColor(cull: integer; color: TJedColor);
    begin
      var cullmode := IfThen(WF_InvertCulling, IfThen(cull = R_CULLFRONT, R_CULLBACK, R_CULLFRONT), cull);
      var what := IfThen(cullmode = R_CULLFRONT, CL_BACK, CL_FRONT);
      renderer.SetColor(what, color.r, color.g, color.b);
      renderer.SetCulling(cullmode);
    end;

    procedure RenderWorldGeometry(mode: TRenderMode);
    begin
      SetRendererGeoVertexSize;
      for var s := 0 to level.sectors.Count - 1 do
        begin
          sec := level.sectors[s];
          if ToolBar.IsLayerVisible(sec.layer) then
            begin
              case mode of
                RM_VERTEX:  renderer.DrawVertices(sec.vertices);
                RM_POLYGON: renderer.DrawPolygons(sec.surfaces);
              end;
            end;
        end;
    end;

    procedure SetLineDrawModeForLayer(const layer: Integer);
    begin
      if ToolBar.IsLayerVisible(layer) then
        renderer.SetLineDrawMode(RdSolidLIne)
      else
        renderer.SetLineDrawMode(RdDashedLine);
    end;

begin
    if not visible then
        exit;
    try
        With clMapBack do
            renderer.SetColor(CL_BACKGROUND, r, g, b);

        renderer.BeginScene;

        With clGrid do
            renderer.SetColor(CL_FRONT, r, g, b);

        ps := 1;// * DpiScale(1, Screen.PixelsPerInch);
        renderer.pointSize := ps;
        renderer.DrawGrid;

        if mapMode = MM_VX then
        begin
            With clVertex do
                renderer.SetColor(CL_FRONT, r, g, b);
            RenderWorldGeometry(RM_VERTEX);
        end;

        // Draw level geometry

        // Render backfaces of level geometry
        if not bMapGeoBackfaceCull then
          begin
            SetCullingAndDrawColor(R_CULLFRONT, clMapGeoBack);
            RenderWorldGeometry(RM_POLYGON);
          end;

        // Render frontfaces of level geometry
        SetCullingAndDrawColor(R_CULLBACK, clMapGeo);
        RenderWorldGeometry(RM_POLYGON);

        // Render objects
        renderer.SetCulling(R_CULLNONE);
        SetRendererObjVertexSize;

        if (not bThingsHidden) or (mapMode = MM_TH) then
        begin
            With clThing do
                renderer.SetColor(CL_FRONT, r, g, b);
            for th := 0 to level.things.Count - 1 do
            begin
                thing := level.things[th];
                if ToolBar.IsLayerVisible(thing.layer) then
                    DrawThing(thing);
            end;
        end;

        if (not bLightsHidden) or (mapMode = MM_LT) then
        begin
            with clLight do
                renderer.SetColor(CL_FRONT, r, g, b);

            for th := 0 to level.lights.Count - 1 do
            begin
                light := level.lights[th];
                if ToolBar.IsLayerVisible(light.Layer) then
                    with light do
                        renderer.DrawVertex(position.x, position.y, position.z);
            end;
        end;

        if mapMode = MM_FR then
        begin
            With clFrame do
                renderer.SetColor(CL_FRONT, r, g, b);

            for th := 0 to level.things.Count - 1 do
            begin
                thing := level.things[th];
                if not ToolBar.IsLayerVisible(thing.layer) then
                    continue;
                with thing do
                    renderer.DrawVertex(X, Y, z);
                for s := 0 to thing.vals.Count - 1 do
                    DrawFrame(thing.vals[s]);
            end;
        end;

        // Draw extra stuff
        SetRendererGeoVertexSize;
        With clExtra do
            renderer.SetColor(CL_FRONT, r, g, b);
        for s := 0 to extraObjs.Count - 1 do
          begin
              obj := extraObjs[s];
              if obj is TVertex then
                  With TJKVertex(obj) do
                      renderer.DrawVertex(X, Y, z)
              else if obj is TExtraLine then
                  With TExtraLine(obj) do
                      renderer.DrawLine(v1, v2)
              else if obj is TIsolatedPolygon then
                  renderer.DrawPolygon(TPolygon(obj));
          end;

    except
        On Exception do;
    end;

    // Draw multiselection
    case mapMode of
        MM_SC:
            begin
                SetCullingAndDrawColor(R_CULLFRONT, clMSelBack);
                for s := 0 to scsel.Count - 1 do
                begin
                    sec := level.sectors[scsel.GetSC(s)];
                    SetLineDrawModeForLayer(sec.layer);
                    renderer.DrawPolygons(sec.surfaces);
                end;

                SetCullingAndDrawColor(R_CULLBACK, clMSel);
                for s := 0 to scsel.Count - 1 do
                begin
                    sec := level.sectors[scsel.GetSC(s)];
                    SetLineDrawModeForLayer(sec.layer);
                    renderer.DrawPolygons(sec.surfaces);
                end;
            end;
        MM_SF:
            begin
                SetCullingAndDrawColor(R_CULLFRONT, clMSelBack);
                for s := 0 to sfsel.Count - 1 do
                begin
                    sfsel.GetSCSF(s, sc, SF);
                    sec := level.sectors[sc];
                    surf := sec.surfaces[SF];
                    SetLineDrawModeForLayer(sec.layer);
                    renderer.DrawPolygon(surf);
                end;

                SetCullingAndDrawColor(R_CULLBACK, clMSel);

                for s := 0 to sfsel.Count - 1 do
                begin
                    sfsel.GetSCSF(s, sc, SF);
                    sec := level.sectors[sc];
                    surf := sec.surfaces[SF];
                    SetLineDrawModeForLayer(sec.layer);
                    renderer.DrawPolygon(surf);
                end;
            end;
        MM_ED:
            begin
                SetCullingAndDrawColor(R_CULLFRONT, clMSelBack);
                for s := 0 to edsel.Count - 1 do
                begin
                    edsel.GetSCSFED(s, sc, SF, ED);
                    sec := level.sectors[sc];
                    surf := sec.surfaces[SF];
                    SetLineDrawModeForLayer(sec.layer);
                    with surf do
                        renderer.DrawLine(vertices[ED], vertices[NextVX(ED)]);
                end;

                SetCullingAndDrawColor(R_CULLBACK, clMSel);
                for s := 0 to edsel.Count - 1 do
                begin
                    edsel.GetSCSFED(s, sc, SF, ED);
                    sec := level.sectors[sc];
                    surf := sec.surfaces[SF];
                    SetLineDrawModeForLayer(sec.layer);
                    with surf do
                        renderer.DrawLine(vertices[ED], vertices[NextVX(ED)]);
                end;
            end;
        MM_VX:
            begin
                with clMSel do
                    renderer.SetColor(CL_FRONT, r, g, b);
                SetRendererGeoVertexSize;
                for s := 0 to vxsel.Count - 1 do
                begin
                    vxsel.GetSCVX(s, sc, vx);
                    with level.sectors[sc].vertices[vx] do
                        renderer.DrawVertex(x, y, z);
                end;
            end;
        MM_TH:
            begin
                with clMSel do
                    renderer.SetColor(CL_FRONT, r, g, b);
                SetRendererObjVertexSize;
                for s := 0 to thsel.Count - 1 do
                  begin
                    thing := level.things[thsel.GetTH(s)];
                    SetLineDrawModeForLayer(thing.layer);
                    DrawThing(thing);
                  end;
            end;
        MM_FR:
            begin
                with clMSel do
                    renderer.SetColor(CL_FRONT, r, g, b);
                SetRendererObjVertexSize;
                for s := 0 to frsel.Count - 1 do
                begin
                    frsel.GetTHFR(s, th, fr);
                    SetLineDrawModeForLayer(level.things[th].layer);
                    DrawSelFrame(th, fr);
                end;
            end;
        MM_LT:
            begin
                with clMSel do
                  renderer.SetColor(CL_FRONT, r, g, b);
                SetRendererObjVertexSize;
                for s := 0 to ltsel.Count - 1 do
                  With level.lights[ltsel.GetLT(s)] do
                      renderer.DrawVertex(position.x, position.y, position.z);
            end;
    end;

    // Draw current selected object
    With clMapSel do
        renderer.SetColor(CL_FRONT, r, g, b);

    if IsSelValid then
        case mapMode of
            MM_SC:
                begin
                    sec := level.sectors[Cur_SC];
                    if scsel.FindSC(Cur_SC) = -1 then
                      SetCullingAndDrawColor(R_CULLFRONT, clMapSelBack)
                    else
                      SetCullingAndDrawColor(R_CULLFRONT, clSelMSelBack);

                    SetLineDrawModeForLayer(sec.layer);
                    renderer.DrawPolygons(sec.surfaces);

                    if scsel.FindSC(Cur_SC) = -1 then
                      SetCullingAndDrawColor(R_CULLBACK, clMapSel)
                    else
                      SetCullingAndDrawColor(R_CULLBACK, clSelMSel);

                    renderer.DrawPolygons(sec.surfaces);

                    if sec.vertices.Count > 0 then
                        with sec.vertices[0] do
                            renderer.DrawVertex(X, Y, z);
                end;
            MM_SF:
                begin
                    sec := level.sectors[Cur_SC];
                    surf := sec.surfaces[Cur_SF];
                    if sfsel.FindSF(Cur_SC, Cur_SF) = -1 then
                      SetCullingAndDrawColor(R_CULLFRONT, clMapSelBack)
                    else
                      SetCullingAndDrawColor(R_CULLFRONT, clSelMSelBack);

                    SetLineDrawModeForLayer(sec.layer);
                    renderer.DrawPolygon(surf);

                    if sfsel.FindSF(Cur_SC, Cur_SF) = -1 then
                      SetCullingAndDrawColor(R_CULLBACK, clMapSel)
                    else
                      SetCullingAndDrawColor(R_CULLBACK, clSelMSel);

                    renderer.DrawPolygon(surf);

                    // draw surf first vert
                    with surf.vertices[0] do
                        renderer.DrawVertex(x, y, z);

                    // draw surf normal
//                    var v0: TVector;
//                    CalcSurfCenter(surf, v0.x, v0.y, v0.z);
//                    Renderer.DrawLine(v0, surf.normal, 0.2);
                end;
            MM_ED:
                with level.sectors[Cur_SC].surfaces[Cur_SF] do
                begin

                    if edsel.FindED(Cur_SC, Cur_SF, Cur_ED) = -1 then
                        with clMapSel do
                            renderer.SetColor(CL_FRONT, r, g, b)
                    else
                        with clSelMSel do
                            renderer.SetColor(CL_FRONT, r, g, b);

                    v1 := vertices[Cur_ED];
                    v2 := vertices[NextVX(Cur_ED)];
                    SetLineDrawModeForLayer(level.sectors[Cur_SC].layer);
                    renderer.DrawLine(v1, v2);

                    SetRendererGeoVertexSize;
                    renderer.DrawVertex(v1.x, v1.y, v1.z);

                    dx := v2.x - v1.x;
                    dy := v2.y - v1.y;
                    dz := v2.z - v1.z;

                    v2 := TJKVertex.Create;
                    v2.x := v1.x + dx * 0.5;
                    v2.y := v1.y + dy * 0.5;
                    v2.z := v1.z + dz * 0.5;

                    en := normal.Cross(dx, dy, dz);
//                    VMult(normal.dx, normal.dy, normal.dz, dx, dy, dz, en.dx,
//                      en.dy, en.dz);

                    v1 := TJKVertex.Create;
                    v1.X := v2.x + en.dx * 0.5;
                    v1.Y := v2.y + en.dy * 0.5;
                    v1.z := v2.z + en.dz * 0.5;
                    renderer.DrawLine(v2, v1);
                    v1.free;
                    v2.free;
                end;
            MM_TH:
                begin
                    if thsel.FindTH(Cur_TH) = -1 then
                        with clMapSel do
                            renderer.SetColor(CL_FRONT, r, g, b)
                    else
                        with clSelMSel do
                            renderer.SetColor(CL_FRONT, r, g, b);

                    thing := level.things[Cur_TH];
                    SetLineDrawModeForLayer(thing.layer);
                    DrawSelThing(thing);
                end;
            MM_FR:
                if Cur_FR = -1 then
                    with level.things[Cur_TH] do
                        renderer.DrawVertex(X, Y, z)
                else
                begin
                    if frsel.FindFR(Cur_TH, Cur_FR) = -1 then
                        with clMapSel do
                            renderer.SetColor(CL_FRONT, r, g, b)
                    else
                        with clSelMSel do
                            renderer.SetColor(CL_FRONT, r, g, b);

                    SetLineDrawModeForLayer(level.things[Cur_TH].layer);
                    DrawSelFrame(Cur_TH, Cur_FR);
                end;
            MM_VX:
                begin
                    if vxsel.FindVX(Cur_SC, Cur_VX) = -1 then
                        with clMapSel do
                            renderer.SetColor(CL_FRONT, r, g, b)
                    else
                        with clSelMSel do
                            renderer.SetColor(CL_FRONT, r, g, b);

                    with level.sectors[Cur_SC].vertices[Cur_VX] do
                        renderer.DrawVertex(X, Y, z);
                end;
            MM_LT:
                begin
                    if ltsel.FindLT(Cur_LT) = -1 then
                        with clMapSel do
                            renderer.SetColor(CL_FRONT, r, g, b)
                    else
                        with clSelMSel do
                            renderer.SetColor(CL_FRONT, r, g, b);

                    if CheckLightIndex(level, Cur_LT) then
                        with level.lights[Cur_LT] do
                        begin
                            renderer.DrawVertex(position.x, position.y, position.z);
                            renderer.DrawCircle(position.x, position.y, position.z, range);
                        end;
                end;

            MM_Extra:
                begin
                    with clMSel do
                        renderer.SetColor(CL_FRONT, r, g, b);
                    SetRendererGeoVertexSize;
                    obj := ExtraObjs[Cur_EX];
                    if obj is TVertex then
                        with TJKVertex(obj) do
                            renderer.DrawVertex(X, Y, z)
                    else if obj is TExtraLine then
                        with TExtraLine(obj) do
                            renderer.DrawLine(v1, v2)
                    else if obj is TPolygon then
                        renderer.DrawPolygon(TPolygon(obj));
                end;
        end;

    // DEBUG
//    with renderer do
//    begin
//      renderer.SetColor(CL_FRONT, 255, 0, 0);
//      renderer.DrawLineAt(GridX, GridY, GridZ, gnormal, 10);
//
//      renderer.SetColor(CL_FRONT, 255, 255, 0);
//      renderer.DrawLineAt(GridX, GridY, GridZ, gynormal, 10);
//
//      renderer.SetColor(CL_FRONT, 0, 0, 255);
//      renderer.DrawLineAt(GridX, GridY, GridZ, gxnormal, 10);
//
//      renderer.SetColor(CL_FRONT,0, 255, 255);
//      renderer.DrawLineAt(-CamX, -CamY, -CamZ, yv, 10);
//
//      renderer.SetColor(CL_FRONT,255, 0, 255);
//      renderer.DrawLineAt(-CamX, -CamY, -CamZ, xv, 10);
//
//      renderer.SetColor(CL_FRONT,255, 255, 255);
//      var zvc := zv;
//      //zvc.x := zvc.x + 1;
//      renderer.DrawLineAt(-CamX, -CamY, -CamZ, zvc, 10);
//    end;
   // DEBUG END

    renderer.SetLineDrawMode(RdSolidLine); // Make sure solid line is drawn beyond this point
    renderer.EndScene;
    ValidateRect(Handle, nil);
end;

procedure TJedMain.FormCreate(sender: TObject);
var
    sec: TJKSector;
    surf: TJKSurface;
    VX: TJKVertex;
    p: TPolygon;
    v: TVertex;
    i: Integer;
begin
    Application.OnException := ExceptHandler;

    MUSnapGridTo.Caption := MUSnapGridTo.Caption + #9'Shft+S';
    // ShortCut:=ShortCut(Ord('S'),[ssShift]);
    SnapViewToObject.ShortCut := ShortCut(Ord('S'), [ssAlt]);

    CalcLightOnLayers.ShortCut := ShortCut(Ord('L'), [ssCtrl]);
    CalcLightInSel.ShortCut    := ShortCut(Ord('L'), [ssCtrl, ssAlt]);

    miCopy.Caption := '&Copy'#9'Ctrl+C';
    miPaste.Caption := '&Paste'#9'Ctrl+V';
    { Renderer.CamZ:=-25; }
    Level.Sectors.Add(Level.NewSector);

    scsel := TSCMultisel.Create;
    sfsel := TSFMultisel.Create;
    vxsel := TVXMultisel.Create;
    thsel := TTHMultisel.Create;
    ltsel := TLTMultisel.Create;
    edsel := TEDMultisel.Create;
    frsel := TFRMultisel.Create;
    exsel := TTHMultisel.Create;

    ExtraObjs := TObjList.Create;

    SetWinPos(Self, MWinPos);

    if MWMaxed then
        WindowState := wsMaximized;

    EnableMapGeoBackfaceCull(false);
    tcube := TPolygons.Create;
    tcube.VXList := Tvertices.Create;
    for i := 1 to 8 do
        tcube.VXList.Add(TVertex.Create);

    With tcube do
    begin
        p := TPolygon.Create;
        Add(p);
        p.AddVertex(VXList[0]);
        p.AddVertex(VXList[1]);
        p.AddVertex(VXList[2]);
        p.AddVertex(VXList[3]);
        p := TPolygon.Create;
        Add(p);

        p.AddVertex(VXList[4]);
        p.AddVertex(VXList[5]);
        p.AddVertex(VXList[6]);
        p.AddVertex(VXList[7]);
        p := TPolygon.Create;
        Add(p);

        p.AddVertex(VXList[2]);
        p.AddVertex(VXList[6]);
        p.AddVertex(VXList[5]);
        p.AddVertex(VXList[3]);
        p := TPolygon.Create;
        Add(p);

        p.AddVertex(VXList[1]);
        p.AddVertex(VXList[7]);
        p.AddVertex(VXList[6]);
        p.AddVertex(VXList[2]);
        p := TPolygon.Create;
        Add(p);

        p.AddVertex(VXList[0]);
        p.AddVertex(VXList[4]);
        p.AddVertex(VXList[7]);
        p.AddVertex(VXList[1]);
        p := TPolygon.Create;
        Add(p);

        p.AddVertex(VXList[3]);
        p.AddVertex(VXList[5]);
        p.AddVertex(VXList[4]);
        p.AddVertex(VXList[0]);
    end;
    Screen.Cursors[crSaber] := LoadCursor(HInstance, PChar('SABER'));
    Screen.Cursors[crEye]   := LoadCursor(HInstance, PChar('EYE'));

    AddKBItem(miMap, 'Top view', '1', []);
    AddKBItem(miMap, 'Front view', '2', []);
    AddKBItem(miMap, 'Left view', '3', []);
    AddKBItem(miMap, 'Bottom view', '4', []);
    AddKBItem(miMap, 'Back view', '5', []);
    AddKBItem(miMap, 'Right view', '6', []);

    AddKBItem(miMap, 'Move'#9'Shift+Middle Mouse Button', #0, []);
    AddKBItem(miMap, 'Move left', Char(VK_RIGHT), []);
    AddKBItem(miMap, 'Move right', Char(VK_LEFT), []);
    AddKBItem(miMap, 'Move Down', Char(VK_UP), []);
    AddKBItem(miMap, 'Move Up', Char(VK_Down), []);
    AddKBItem(miMap, 'Move forward', Char(VK_NEXT), []);
    AddKBItem(miMap, 'Move back', Char(VK_PRIOR), []);

    AddKBItem(miMap, 'Rotate'#9'Middle Mouse Button', #0, []);
    AddKBItem(miMap, 'Rotate in X direction'#9'Ctrl+Middle Mouse Button', #0, []);
    AddKBItem(miMap, 'Rotate in Y direction'#9'Ctrl+Shift+Middle Mouse Button', #0, []);
    AddKBItem(miMap, 'Rotate right', Char(VK_RIGHT), [ssShift]);
    AddKBItem(miMap, 'Rotate left', Char(VK_LEFT), [ssShift]);
    AddKBItem(miMap, 'Rotate Up', Char(VK_UP), [ssShift]);
    AddKBItem(miMap, 'Rotate Down', Char(VK_DOWN), [ssShift]);
    AddKBItem(miMap, 'Rotate ...', Char(VK_NEXT), [ssShift]);
    AddKBItem(miMap, 'Rotate ...', Char(VK_PRIOR), [ssShift]);
    AddKBItem(miMap, 'Zoom at cursor'#9'Mouse Wheel', #0, []);
    AddKBItem(miMap, 'Zoom at cursor', 'C', [ssShift]);
    AddKBItem(miMap, 'Zoom in', Char(VK_ADD), []);
    AddKBItem(miMap, 'Zoom out', Char(VK_SUBTRACT), []);
    AddKBItem(miMap, 'View To Grid', Char(VK_MULTIPLY), []);

    AddKBItem(miGrid, 'Top view&&grid', '1', [ssShift]);
    AddKBItem(miGrid, 'Front view&&grid', '2', [ssShift]);
    AddKBItem(miGrid, 'Left view&&grid', '3', [ssShift]);
    AddKBItem(miGrid, 'Bottom view&&grid', '4', [ssShift]);
    AddKBItem(miGrid, 'Back view&&grid', '5', [ssShift]);
    AddKBItem(miGrid, 'Right view&&grid', '6', [ssShift]);

    AddKBItem(miGrid, 'Snap grid to item', 'S', [ssShift]);
    AddKBItem(miGrid, 'Snap grid to item', 'W', [ssShift]);
    AddKBItem(miGrid, 'Move Grid'#9'G+Mouse', #0, []);
    AddKBItem(miGrid, 'Swap Grid axis', 'G', [ssShift]);

    AddKBItem(miSel, 'Next item', 'N', []);
    AddKBItem(miSel, 'Prev item', 'P', []);
    AddKBItem(miSel, 'Next edge in surface', 'N', [ssShift]);
    AddKBItem(miSel, 'Prev edge in surface', 'P', [ssShift]);
    AddKBItem(miSel, 'Goto Adjoin', 'F', [ssShift]);
    AddKBItem(miSel, 'Clear multiselection', Chr(VK_BACK), []);
    AddKBItem(miSel, 'Multselect item', ' ', []);
    AddKBItem(miSel, 'Rectangle Multselect'#9'Alt+Mouse', #0, []);

    AddKBItem(miTex, 'Scroll up', #188, [ssShift]);
    AddKBItem(miTex, 'Scroll down', #190, [ssShift]);

    AddKBItem(miTex, 'Scroll left', #188, []);
    AddKBItem(miTex, 'Scroll right', #190, []);

    AddKBItem(miTex, 'Start stitch', Char(VK_INSERT), [ssCtrl]);
    AddKBItem(miTex, 'Start stitch', #186, []);

    AddKBItem(miTex, 'Stitch', Char(VK_INSERT), [ssShift]);
    AddKBItem(miTex, 'Stitch', #222, []);
    AddKBItem(miTex, 'Stitch && Copy Surface Properties', Char(VK_INSERT), [ssShift, ssCtrl]);
    AddKBItem(miTex, 'Stitch && Copy Surface Properties', #222, [ssCtrl]);

    AddKBItem(miTex, 'Straighten Texture', Char(VK_HOME), [ssAlt]);
    AddKBItem(miTex, 'Straighten Texture', #191, []);
    AddKBItem(miTex, 'Scale texture down', #188, [ssAlt]);
    AddKBItem(miTex, 'Scale texture Up', #190, [ssAlt]);
    AddKBItem(miTex, 'Straighten/zero/rotate90 texture', #191,
      [ssShift, ssCtrl]);
    AddKBItem(miTex, 'Straighten/zero/rotate90 texture', char(VK_HOME),
      [ssShift, ssCtrl]);
    AddKBItem(miTex, 'Straighten/zero texture', #191, [ssCtrl]);
    AddKBItem(miTex, 'Straighten/zero texture', Char(VK_HOME), [ssCtrl]);
    AddKBItem(miTex, 'Rotate texture left', #188, [ssCtrl]);
    AddKBItem(miTex, 'Rotate texture right', #190, [ssCtrl]);

    AddKBItem(miRecovery, 'Build surface/sector', 'B', [ssAlt]);
    AddKBItem(miRecovery, 'Delete surface', Char(VK_DELETE), [ssAlt]);
    AddKBItem(miRecovery, 'Invert surface', 'I', [ssAlt]);
    AddKBItem(miRecovery, 'Planarize surface', 'P', [ssCtrl]);
    AddKBItem(miRecovery, 'Fix solid surface(s)', OnFixSolidSurfaces);
    AddKBItem(miRecovery, 'Fix passable adjoin(s)', OnFixPassableAdjoins);

    AddKBItem(miEdit, 'Move item'#9'Ctrl+Left Mouse Button', #0, []);
    AddKBItem(miEdit, 'Start cleave', 'C', []);
    AddKBItem(miEdit, 'Cleave by grid', 'C', [ssAlt]);
    AddKBItem(miEdit, 'Assign Thing to Sector', 'A', [ssShift]);
    AddKBItem(miEdit, 'Unadjoin', 'A', [ssAlt]);
    AddKBItem(miEdit, 'Adjoin', 'A', []);
    AddKBItem(miEdit, 'Join sectors/surfaces', 'J', []);
    AddKBItem(miEdit, 'Rotate Thing Pitch'#9'Ctrl+Shift+Mouse Wheel', #0, []);
    AddKBItem(miEdit, 'Rotate Thing Yaw'#9'Ctrl+Mouse Wheel', #0, []);
    AddKBItem(miEdit, 'Rotate Thing Roll'#9'Ctrl+Alt+Mouse Wheel', #0, []);
    AddKBItem(miEdit, 'Raise item', Char(VK_UP), [ssCtrl]);
    AddKBItem(miEdit, 'Lower item', Char(VK_Down), [ssCtrl]);
    AddKBItem(miEdit, 'Raise item', #219, []);
    AddKBItem(miEdit, 'Lower item', #221, []);
    AddKBItem(miEdit, 'Item Editor', Char(VK_RETURN), []);
    AddKBItem(miEdit, 'Merge', 'M', []);
    AddKBItem(miEdit, 'Duplicate item', Char(VK_INSERT), []);
    AddKBItem(miEdit, 'Delete item', Char(VK_DELETE), []);

    AddKBItem(miEdit, 'Create sector', 'K', []);

    AddKBItem(miEdit, 'Extrude surface', 'X', []);
    AddKBItem(miEdit, 'Extrude surface by', 'X', [ssShift]);
    AddKBItem(miEdit, 'Extrude && Expand surface by', 'X', [ssCtrl]);
    AddKBItem(miEdit, 'Use item in COG', 'U', []);
    AddKBItem(miEdit, 'Bring thing/light to surface', 'B', []);

    AddKBItem(miOther, 'Cancel cleave/etc', Char(VK_ESCAPE), []);
    AddKBItem(miOther, 'Sector mode', 'S', []);
    AddKBItem(miOther, 'Surface mode', 'F', []);
    AddKBItem(miOther, 'Edge mode', 'E', []);
    AddKBItem(miOther, 'Vertex mode', 'V', []);
    AddKBItem(miOther, 'Thing mode', 'T', []);
    AddKBItem(miOther, 'Thing frame mode', 'T', [ssShift]);
    AddKBItem(miOther, 'Light mode', 'L', []);
    AddKBItem(miOther, 'Toggle Snap To X/Y', 'Z', [ssCtrl, ssAlt]);

end;

procedure TJedMain.Save1Click(sender: TObject);
begin
    SaveProject;
end;

procedure TJedMain.FormResize(sender: TObject);
begin
    if renderer <> nil then
        renderer.SetViewPort(0, 0, ClientWidth, ClientHeight);
        ReDraw;
end;


procedure TJedMain.OnFixSolidSurfaces(sender: TObject);
begin
    const MakeSolid = procedure (surf: TJKSurface)
      begin
          if surf.Adjoin = nil then
            begin
                surf.geo       := integer(TGeoMode.Texture);
                surf.SurfFlags := surf.SurfFlags or SF_Collision;
                surf.CheckIfFloor;
            end;
      end;

    case mapMode of
        MM_SC:
          ForSelectedSectorsDo(procedure(sec: TJKSector)
            begin
                for var surf in sec.surfaces do
                  MakeSolid(surf);
                SectorChanged(sec);
            end
          );
        MM_SF:
          ForSelectedSurfacesDo(procedure(surf: TJKSurface)
            begin
                MakeSolid(surf);
                SectorChanged(surf.Sector);
            end
          );
    end;
end;

procedure TJedMain.OnFixPassableAdjoins(sender: TObject);
begin
    const MakePassable = procedure (surf: TJKSurface)
      begin
          if surf.Adjoin <> nil then
            begin
                surf.geo         := integer(TGeoMode.NotDrawn);
                surf.AdjoinFlags := (SAF_Visible or SAF_Move)
                  or (surf.AdjoinFlags and not (SAF_NoAIMove or SAF_NoPlayerMove));
            end;
      end;

    case mapMode of
        MM_SC:
          ForSelectedSectorsDo(procedure(sec: TJKSector)
            begin
                for var surf in sec.surfaces do
                  MakePassable(surf);
                SectorChanged(sec);
            end
          );
        MM_SF:
          ForSelectedSurfacesDo(procedure(surf: TJKSurface)
            begin
                MakePassable(surf);
                SectorChanged(surf.Sector);
            end
          );
    end;
end;

Procedure RotAxisAngle(var X, Y: TVector; ang: double);
var
    sina, cosa: double;
    nx, ny: TVector;
begin
    cosa := cOS(ang / 180 * PI);
    sina := SIN(ang / 180 * PI);

    { cosa:=cos(ang/180*pi);
      sina:=sin(ang/180*pi); }

    nx.dx := cosa * X.dx + sina * Y.dx;
    nx.dy := cosa * X.dy + sina * Y.dy;
    nx.dz := cosa * X.dz + sina * Y.dz;

    ny.dx := -sina * X.dx + cosa * Y.dx;
    ny.dy := -sina * X.dy + cosa * Y.dy;
    ny.dz := -sina * X.dz + cosa * Y.dz;

    X := nx;
    Y := ny;

end;

Procedure RotAxis(var X, Y: TVector; dir: Integer);
begin
    Case dir of
        1:
            RotAxisAngle(X, Y, 5);
        -1:
            RotAxisAngle(X, Y, -5);
    end;
end;

procedure TJedMain.FormKeyDown(sender: TObject; var Key: Word;
  Shift: TShiftState);
var
    th: TJKThing;
    i, a, nth, nfr, X, Y: Integer;
    n, xn: TVector;
    dx, dy, dz: double;
    nx, ny, nz: TVector;
    surf, surf1: TJKSurface;
    sec, sec1: TJKSector;
    mx: TMat3x3;
    nsc, nsf: Integer;
    nsel: Integer;
    s: string;
begin
    if (Key = VK_MENU) or (Key = VK_LMENU) then
        exit;

    if Key = VK_NUMPAD5 then
      begin
        // Experimental
        renderer.perspective := not renderer.perspective;
        invalidate;
        exit;
      end;

    if Shift = [ssCtrl, ssAlt] then
        case Key of
            Ord('Z'):
                snaptoy := not snaptoy;
        end;

    if Shift = [ssShift] then
    begin
        case Key of
            Ord('1') .. Ord('6'):
                if MapRot = MR_old then
                begin
                    with renderer do
                        Case char(Key) of
                            '1':
                                begin
                                    rPch := 0;
                                    rYaw := 0;
                                    rRol := 0;
                                    SetGridNormal(0, 0, 1);
                                    SetGridXNormal(1, 0, 0);
                                end;
                            '2':
                                begin
                                    rPch := -90;
                                    rYaw := 180;
                                    rRol := 0;
                                    SetGridNormal(0, 1, 0);
                                    SetGridXNormal(-1, 0, 0);
                                end;
                            '3':
                                begin
                                    rPch := 90;
                                    rYaw := 0;
                                    rRol := 90;
                                    SetGridNormal(1, 0, 0);
                                    SetGridXNormal(0, 1, 0);
                                end;
                            '4':
                                begin
                                    rPch := 180;
                                    rYaw := 0;
                                    rRol := 0;
                                    SetGridNormal(0, 0, -1);
                                    SetGridXNormal(-1, 0, 0);
                                end;
                            '5':
                                begin
                                    rPch := 90;
                                    rYaw := 0;
                                    rRol := 0;
                                    SetGridNormal(0, -1, 0);
                                    SetGridXNormal(1, 0, 0);
                                end;
                            '6':
                                begin
                                    rPch := 0;
                                    rYaw :=-90;
                                    rRol :=-90;
                                    SetGridNormal(-1, 0, 0);
                                    SetGridXNormal(0, -1, 0);
                                end;
                        end;
                    SetRendfromPYR;
                    Invalidate;
                end
                else
                    With renderer do
                        Case char(Key) of
                            '1':
                                begin
                                    SetZ(0, 0, 1);
                                    SetX(1, 0, 0);
                                    SetGridNormal(0, 0, 1);
                                    SetGridXNormal(1, 0, 0);
                                end;
                            '2':
                                begin
                                    SetZ(0, 1, 0);
                                    SetX(-1, 0, 0);
                                    SetGridNormal(0, 1, 0);
                                    SetGridXNormal(-1, 0, 0);
                                end;
                            '3':
                                begin
                                    SetZ(1, 0, 0);
                                    SetX(0, 1, 0);
                                    SetGridNormal(1, 0, 0);
                                    SetGridXNormal(0, 1, 0);
                                end;
                            '4':
                                begin
                                    SetZ(0, 0, -1);
                                    SetX(-1, 0, 0);
                                    SetGridNormal(0, 0, -1);
                                    SetGridXNormal(-1, 0, 0);
                                end;
                            '5':
                                begin
                                    SetZ(0, -1, 0);
                                    SetX(1, 0, 0);
                                    SetGridNormal(0, -1, 0);
                                    SetGridXNormal(1, 0, 0);
                                end;
                            '6':
                                begin
                                    SetZ(-1, 0, 0);
                                    SetX(0, -1, 0);
                                    SetGridNormal(-1, 0, 0);
                                    SetGridXNormal(0, -1, 0);
                                end;
                        end;
//            Ord('C'):   // Zoom in and mouse pos
//                if GetMousePos(X, Y) then
//                    With renderer do
//                    begin
//                        ny := renderer.zv;
//                        if not GetXYZonPlaneAt(X, Y, ny, CamX, CamY, CamZ, dx,
//                          dy, dz) then
//                            exit;
//                        CamX := -dx;
//                        CamY := -dy;
//                        CamZ := -dz;
//                        ZoomIn;
//                    end;
            { if GetXYZAt(X,Y,dx,dy,dz) then
              with Renderer do
              begin
              CamX:=-dX;
              CamY:=-dy;
              CamZ:=-dz;
              Self.scale:=Self.scale/1.5;
              Renderer.Scale:=Self.scale;
              end; }
            Ord('N'):
                NextObjectInSurface;
            Ord('P'):
                PreviousObjectInSurface;
            Ord('T'):
                SetMapMode(MM_FR);
            Ord('G'):
                begin
                    nx := renderer.gynormal;
                    nz := renderer.gxnormal;
                    With nz do
                        renderer.SetGridNormal(dx, dy, dz);
                    With nx do
                        renderer.SetGridXNormal(dx, dy, dz);
                    Invalidate;
                    exit;
                end;
            Ord('X'):
                if mapMode = MM_SF then
                begin
                    surf := Level.Sectors[Cur_SC].surfaces[Cur_SF];
                    if surf.adjoin <> nil then
                        exit;
                    s := DoubleToStr(surf.ExtrudeSize);
                    if not InputQuery('Extrude', 'By:', s) then
                        exit;
                    if (Not ValDouble(s, dx)) or (dx = 0) then
                    begin
                        ShowMessage('Invalid Value: ' + s);
                        exit;
                    end;

                    StartUndoRec('Extrude Surface');

                    ExtrudeSurface(surf, dx);
                    SetCurSF(Cur_SC, Cur_SF);
                end;
            VK_RIGHT,
            VK_LEFT,
            VK_UP,
            VK_DOWN,
            VK_NEXT,
            VK_PRIOR:
                if MapRot = MR_old then
                  begin
                      case Key of
                          VK_RIGHT:
                              rYaw := rYaw - 5;
                          VK_LEFT:
                              rYaw := rYaw + 5;
                          VK_UP:
                              rPch := rPch - 5;
                          VK_DOWN:
                              rPch := rPch + 5;
                          VK_PRIOR:
                              rRol := rRol - 5;
                          VK_NEXT:
                              rRol := rRol + 5;
                      end;
                      SetRendFromPYR;
                      //with renderer do
                        //SetGridNormal(zv.dx, zv.dy, zv.dz); // New code, fixes cleaving
                      Invalidate;
                  end
                else
                  with renderer do
                  begin
                      { SetVec(nx,1,0,0);
                        SetVec(ny,0,1,0);
                        SetVec(nz,0,0,1);
                        RotatePoint(0,0,0,nx.dx,nx.dy,nx.dz,PCH,ny.dx,ny.dy,ny.dz);
                        RotatePoint(0,0,0,nx.dx,nx.dy,nx.dz,PCH,ny.dz,nz.dy,nz.dz);

                        RotatePoint(0,0,0,ny.dx,ny.dy,ny.dz,YAW,nx.dx,nx.dy,nx.dz);
                        RotatePoint(0,0,0,ny.dx,ny.dy,ny.dz,YAW,nz.dx,nz.dy,nz.dz);

                        RotatePoint(0,0,0,nz.dy,nz.dy,nz.dz,ROL,nx.dx,nx.dy,nx.dz);
                        RotatePoint(0,0,0,nz.dy,nz.dy,nz.dz,ROL,ny.dx,ny.dy,ny.dz); }
                      With renderer do
                          case Key of
                              VK_RIGHT:
                                  RotAxis(zv, xv, -1);
                              VK_LEFT:
                                  RotAxis(zv, xv, 1);
                              VK_UP:
                                  RotAxis(yv, zv, -1);
                              VK_Down:
                                  RotAxis(yv, zv, 1);
                              VK_PRIOR:
                                  RotAxis(xv, yv, -1);
                              VK_NEXT:
                                  RotAxis(xv, yv, 1);
                          end;
                      With renderer do
                      begin
                          with zv do
                              SetZ(dx, dy, dz);
                          with xv do
                              SetX(dx, dy, dz);
                      end;
                      Invalidate;
                      exit;
                  end;
            VK_INSERT:
                DoStitch(ssCtrl in shift);
            188:
                begin
                    ShiftTexture(st_up);
                    exit;
                end; { < }
            190:
                begin
                    ShiftTexture(st_down);
                    exit;
                end; { > }
            191:
                begin
                    StraightenTexture(false, true);
                    exit;
                end; { / }
            Ord('A'):
                if mapMode = MM_TH then
                    LayerThings;
            Ord('F'):
                Goto_Adjoin;
            Ord('S'), Ord('W'):
                SnapGridToObject;
        else
            exit;
        end;
        Key := 0;
        Invalidate;
        exit;
    end;

    if Shift = [ssAlt] then
    begin
        n := renderer.gnormal;
        xn := renderer.gxnormal;

        case Key of
            Ord('A'):
                case mapMode of
                    MM_SC:
                        begin
                            i := scsel.AddSC(Cur_SC);
                            if scsel.Count = 2 then
                              begin
                                StartUndoRec('Unadjoin Sectors');
                                if UnAdjoinSectors(level.sectors[scsel.GetSC(0)],
                                  level.sectors[scsel.GetSC(1)]) then
                                    PanMessage(mt_info, 'Sectors unadjoined');
                              end
                              else
                                ShowMessage('You must have 2 sectors selected');
                            scsel.DeleteN(i);
                        end;

                    MM_SF:
                        begin
                            X := sfsel.AddSF(Cur_SC, Cur_SF);
                            nsel := 0;
                            StartUndoRec('Unadjoin Surfaces');
                            for i := 0 to sfsel.Count - 1 do
                              begin
                                sfsel.GetSCSF(i, nsc, nsf);
                                if UnAdjoin(level.sectors[nsc].surfaces[nsf]) then
                                  Inc(nsel);
                              end;
                            PanMessageFmt(mt_info,
                              '%d adjoins removed', [nsel]);
                            SetCurSF(Cur_SC, Cur_SF);
                            sfsel.DeleteN(X);
                        end;
                end;
            Ord('B'):
                case mapMode of
                    MM_SF:
                        begin
                            i := sfsel.AddSF(Cur_SC, Cur_SF);
                            sec := BuildSector(level, sfsel);
                            if sec = nil then
                                sfsel.DeleteN(i)
                            else
                            begin
                                sfsel.Clear;
                                SetMapMode(MM_SC);
                                SetCurSC(sec.num);
                            end;
                        end;
                    MM_VX:
                        begin
                            i := vxsel.AddVX(Cur_SC, Cur_VX);
                            surf := BuildSurface(level, vxsel);
                            if surf = nil then
                                vxsel.DeleteN(i)
                            else
                            begin
                                vxsel.Clear;
                                SetMapMode(MM_SF);
                                SetCurSF(surf.sector.num, surf.num);
                            end;
                        end;
                end;
            Ord('C'):
                { Cleave by grid plane }
                With renderer do
                    CleaveBy(gnormal, GridX, GridY, GridZ);
            Ord('I'):
                if mapMode = MM_SF then
                begin
                    i := sfsel.AddSF(Cur_SC, Cur_SF);
                    FlipSurfaces(Level, sfsel);
                    sfsel.DeleteN(i);
                end;
            { VK_RIGHT : begin n.dx:=Cos(Arccos(n.dx)+0.3); xn.dx:=Cos(Arccos(xn.dx)+0.3); end;
              VK_LEFT: begin n.dx:=Cos(Arccos(n.dx)-12/pi); xn.dx:=Cos(Arccos(xn.dx)-12/pi); end;
              VK_UP: With Renderer do PCH:=PCH+10;
              VK_DOWN: With Renderer do PCH:=PCH-10;
              VK_NEXT: With Renderer do ROL:=ROL+10;
              VK_PRIOR: With Renderer do ROL:=ROL-10; }
             VK_INSERT:
              Case mapMode of
                MM_SC: if GetMousePos(X,Y) then AddSectorAt(X,Y);
              end;
            VK_HOME:
                begin
                    StraightenTexture(false, false);
                    exit;
                end;
            VK_DELETE:
                if mapMode = MM_SF then
                    DeleteSurface(level, Cur_SC, Cur_SF);
            188: { < }
                begin
                    ScaleTexture(st_down);
                    exit;
                end;
            190: { > }
                begin
                    ScaleTexture(st_up);
                    exit;
                end;
        end;

        renderer.SetGridNormal(n.dx, n.dy, n.dz);
        renderer.SetGridXNormal(xn.dx, xn.dy, xn.dz);
        Invalidate;
        exit;
    end;

    if Shift = [ssShift, ssCtrl] then
    begin
        case Key of
            191, VK_HOME:
                StraightenTexture(true, true);
            VK_INSERT:
                DoStitch(True);
        end;
        exit;
    end;

    if Shift = [ssCtrl] then
    begin
        case Key of
            VK_INSERT:
                begin
                    StartStitch;
                    exit;
                end;
            191, VK_HOME:
                begin
                    StraightenTexture(true, false);
                    exit;
                end;
            188: { < }
                begin
                    RotateTexture(st_left);
                    exit;
                end;
            190: { > }
                begin
                    RotateTexture(st_right);
                    exit;
                end;
            222: { ' }
            begin
                DoStitch(True);
                exit;
            end;
            VK_UP:
                RaiseObject(ro_up);
            VK_Down:
                RaiseObject(ro_down);
            Ord('C'):
                miCopyClick(nil);
            Ord('V'):
                miPasteClick(nil);
            Ord('P'):
                if mapMode = MM_SF then
                  begin
                      StartUndoRec('Flatten Surface');
                      if FlattenSurface(level.sectors[Cur_SC].surfaces[Cur_SF])
                        then
                            PanMessage(mt_info, 'Surface planarized')
                        else
                            PanMessage(mt_info, 'Surface wasn''t fully planarized');
                      Invalidate;
                  end;
            Ord('X'):
                if mapMode = MM_SF then
                begin
                    surf := level.sectors[Cur_SC].surfaces[Cur_SF];
                    if surf.adjoin <> nil then
                        exit;
                    s := DoubleToStr(surf.ExtrudeSize);
                    if not InputQuery('Extrude && Expand', 'By:', s) then
                        exit;
                    if (Not ValDouble(s, dx)) or (dx = 0) then
                    begin
                        ShowMessage('Invalid Value: ' + s);
                        exit;
                    end;

                    StartUndoRec('Extrude && Expand Surface');
                    ExtrudeAndExpandSurface(surf, dx);
                    SetCurSF(Cur_SC, Cur_SF);
                end;
        end;
        exit;
    end;

    case Key of
        VK_BACK:
            ClearMultiSelection;
        VK_ESCAPE:
            CancelMouseMode;
        Ord('1') .. Ord('6'):
            if MapRot = MR_old then
            begin
                case Chr(Key) of
                    '1':
                        begin
                            rPch := 0;
                            rYaw := 0;
                            rRol := 0;
                        end;
                    '2':
                        begin
                            rPch := -90;
                            rYaw := 180;
                            rRol := 0;
                        end;
                    '3':
                        begin
                            rPch := 90;
                            rYaw := 0;
                            rRol := 90;
                        end;
                    '4':
                        begin
                            rPch := 0;
                            rYaw := 180;
                            rRol := 0;
                        end;
                    '5':
                        begin
                            rPch := 90;
                            rYaw := 0;
                            rRol := 0;
                        end;
                    '6':
                        begin
                            rPch := 0;
                            rYaw := -90;
                            rRol := -90;
                        end;
                end;
                SetRendfromPYR;
//                with renderer do
//                  SetGridNormal(zv.dx, zv.dy, zv.dz); // New code, fixes cleaving
                Invalidate;
            end
            else
                With renderer do
                    case Chr(Key) of
                        '1':
                            begin
                                SetZ(0, 0, 1);
                                SetX(1, 0, 0);
                            end;
                        '2':
                            begin
                                SetZ(0, 1, 0);
                                SetX(-1, 0, 0);
                            end;
                        '3':
                            begin
                                SetZ(1, 0, 0);
                                SetX(0, 1, 0);
                            end;
                        '4':
                            begin
                                SetZ(0, 0, -1);
                                SetX(-1, 0, 0);
                            end;
                        '5':
                            begin
                                SetZ(0, -1, 0);
                                SetX(1, 0, 0);
                            end;
                        '6':
                            begin
                                SetZ(-1, 0, 0);
                                SetX(0, -1, 0);
                            end;
                    end;
        VK_ADD:
            begin
                ZoomIn;
            end;
        VK_SUBTRACT:
            begin
                ZoomOut;
            end;
        VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_NEXT, VK_PRIOR:
            With renderer do
            begin
                { CreateRotMatrix(mx,PCH,YAW,ROL); }

                nx := renderer.xv.Scaled(IfThen(renderer.perspective,  0.2, DpiScale));
                ny := renderer.yv.Scaled(IfThen(renderer.perspective,  0.2, DpiScale));
                nz := renderer.zv.Scaled(IfThen(renderer.perspective, -0.2, DpiScale));
                case Key of
                    VK_LEFT:
                        begin
                            CamX := CamX + nx.dx;
                            CamY := CamY + nx.dy;
                            CamZ := CamZ + nx.dz;
                        end;
                    VK_RIGHT:
                        begin
                            CamX := CamX - nx.dx;
                            CamY := CamY - nx.dy;
                            CamZ := CamZ - nx.dz;
                        end;
                    VK_DOWN:
                        begin
                            CamX := CamX + ny.dx;
                            CamY := CamY + ny.dy;
                            CamZ := CamZ + ny.dz;
                        end;
                    VK_UP:
                        begin
                            CamX := CamX - ny.dx;
                            CamY := CamY - ny.dy;
                            CamZ := CamZ - ny.dz;
                        end;
                    VK_NEXT:
                        begin
                            CamX := CamX + nz.dx;
                            CamY := CamY + nz.dy;
                            CamZ := CamZ + nz.dz;
                        end;
                    VK_PRIOR:
                        begin
                            CamX := CamX - nz.dx;
                            CamY := CamY - nz.dy;
                            CamZ := CamZ - nz.dz;
                        end;
                end;
            end;
        VK_RETURN:
            begin
                if IsSelValid then
                    ItemEdit.Show;
                exit;
            end;
        Ord('A'):
            Case mapMode of
                MM_SC:
                    begin
                        i := scsel.AddSC(Cur_SC);
                        if scsel.Count = 2 then
                        begin
                            StartUndoRec('Adjoin Sectors');
                            if AdjoinSectors(level.sectors[scsel.GetSC(0)],
                              Level.Sectors[scsel.GetSC(1)]) then
                                PanMessage(mt_info, 'Sectors adjoined');
                        end
                        else
                            ShowMessage('You must have 2 sectors selected');
                        scsel.DeleteN(i);
                    end;

                MM_SF:
                    begin
                        X := sfsel.AddSF(Cur_SC, Cur_SF);
                        nsel := 0;
                        StartUndoRec('Adjoin Surfaces');
                        for i := 0 to sfsel.Count - 1 do
                        begin
                            sfsel.GetSCSF(i, nsc, nsf);
                            if MakeAdjoin(level.sectors[nsc].surfaces[nsf]) then
                                Inc(nsel);
                        end;
                        PanMessageFmt(mt_info, '%d adjoins formed', [nsel]);
                        SetCurSF(Cur_SC, Cur_SF);
                        sfsel.DeleteN(X);
                    end;
                MM_TH:
                    LayerThings;
            end;
        Ord('B'):
            case mapMode of
                MM_TH:
                    BringThingToSurf(Cur_TH, Cur_SC, Cur_SF);
                MM_LT:
                    BringLightToSurf(Cur_LT, Cur_SC, Cur_SF);
            end;
        VK_MULTIPLY:
            SetViewToGrid;
        Ord('E'):
            SetMapMode(MM_ED);
        Ord('X'):
            if mapMode = MM_SF then
            begin
                surf := level.sectors[Cur_SC].surfaces[Cur_SF];
                StartUndoRec('Extrude Surface');
                ExtrudeSurface(surf, surf.ExtrudeSize);
                SetCurSF(Cur_SC, Cur_SF);
            end;
        Ord('S'): // set sector map mode
            SetMapMode(MM_SC);
        Ord('C'):
            if IsCleaveMapMode then
              SetMouseMode(MM_Cleave);
//        Ord('W'):
//            SetMouseMode(MM_Ruler);
        Ord('F'):
            SetMapMode(MM_SF);
        Ord('J'):
            case mapMode of
                MM_SC:
                    ConnectSCs;
                MM_SF:
                    ConnectSFs;
            end;
        Ord('K'):
            SetMouseMode(MM_CreateSector);
        Ord('G'):
            if GetMousePos(X, Y) then
            begin
                SetMouseMode(MM_TranslateGrid);
                Do_StartTranslateGrid(X, Y);
            end;
        VK_Space:
            DO_MultiSelect;
        { if GetMousePos(X,Y) then begin SetMouseMode(MM_TranslateCam); Do_StartTranslateCam(X,Y); end; }
        Ord('R'):
            if GetMousePos(X, Y) then
                Do_StartRotateCam(X, Y);
        Ord('T'):
            SetMapMode(MM_TH);
        Ord('V'):
            SetMapMode(MM_VX);
        Ord('L'):
            SetMapMode(MM_LT);
        Ord('N'):
            NextObject;
        Ord('P'):
            PreviousObject;
        Ord('U'):
            UseInCog;
//        Ord('Q'):
//            LoadDLLPlugin('h:\rr\mv\mv.dll');
        Ord('M'):
            begin
                case mapMode of
                    MM_SC:
                        begin
                            i := scsel.AddSC(Cur_SC);
                            scsel.sort;
                            nsel := 0;
                            StartUndoRec('Merge Sectors');

                            while scsel.Count > 1 do
                            begin
                                sec := level.sectors[scsel.GetSC(scsel.Count - 1)];
                                for i := 0 to scsel.Count - 2 do
                                begin
                                    var msec := level.sectors[scsel.GetSC(i)];
                                    if not ToolBar.IsLayerVisible(msec.Layer) then continue;  // Don't allow modification of sectors on invisible layer
                                    if MergeSectors(msec, sec) <> nil then
                                    begin
                                        Inc(nsel);
                                        break;
                                    end;
                                end;
                                scsel.DeleteN(scsel.Count - 1);
                            end;

                            SetCurSC(scsel.GetSC(0));
                            scsel.Clear;
                            PanMessageFmt(mt_info, '%d sectors merged', [nsel]);
                        end;
                    MM_SF:
                        begin
                            i := sfsel.AddSF(Cur_SC, Cur_SF);
                            case sfsel.Count of
                                1:
                                    begin
                                        surf := level.sectors[Cur_SC].surfaces[Cur_SF];
                                        if surf.adjoin = nil then
                                          exit;

                                        StartUndoRec('Merge Sectors');
                                        sec := MergeSectors(surf.sector, surf.adjoin.sector);
                                        if sec = nil then
                                          PanMessage(mt_info, 'Sectors weren''t merged')
                                        else
                                          begin
                                            SetCurSF(sec.num, 0);
                                            sfsel.Clear;
                                          end;
                                    end;
                            else
                                begin
                                    sfsel.sort;
                                    nsel := 0;
                                    StartUndoRec('Merge Surfaces');

                                    while sfsel.Count > 1 do
                                      begin
                                          sfsel.GetSCSF(sfsel.Count - 1, nsc, nsf);
                                          if CheckSurfaceIndex(level, nsc, nsf) then
                                            begin
                                              surf := level.sectors[nsc].surfaces[nsf];
                                              for i := 0 to sfsel.Count - 2 do
                                                begin
                                                  sfsel.GetSCSF(i, nsc, nsf);
                                                  if not CheckSurfaceIndex(level, nsc, nsf) then
                                                    continue;

                                                  var msurf := level.sectors[nsc].surfaces[nsf];
                                                  if MergeSurfaces(msurf, surf) <> nil then
                                                  begin
                                                    Inc(nsel);
                                                    break;
                                                  end;
                                                end;
                                            end;
                                          sfsel.DeleteN(sfsel.Count - 1);
                                      end;

                                    sfsel.GetSCSF(0, nsc, nsf);
                                    surf := level.sectors[nsc].surfaces[nsf];
                                    sfsel.Clear;
                                    SetCurSF(surf.sector.num, surf.num);
                                    PanMessageFmt(mt_info,'%d surfaces merged', [nsel]);
                                end;
                                { else ShowMessage('You should either select one surface to merge sector or two surfaces to merge surfaces'); }
                            end;
                            sfsel.DeleteN(i);
                        end;
                    MM_VX:
                        begin
                            PanMessageFmt(mt_info, '%d vertices merged', [MergeVertices(level, vxsel)]);
                            vxsel.Clear;
                        end;
                    MM_ED:
                        begin
                            surf := level.sectors[Cur_SC].surfaces[Cur_SF];
                            StartUndoRec('Merge Surfaces');
                            if MergeSurfacesAt(surf, Cur_ED) then
                              edsel.Clear;
                        end;
                end;
                SetMapMode(mapMode);
            end;
        VK_INSERT:
            Begin
                Case mapMode of
                    MM_SC:
                        if GetMousePos(X, Y) then
                            CopySectorsAt(X, Y);
                    MM_LT:
                        if GetMousePos(X, Y) then
                            AddLightsAt(X, Y);
                    MM_TH:
                        if GetMousePos(X, Y) then
                            AddThingsAt(X, Y);
                    MM_FR:
                        if GetMousePos(X, Y) then
                            AddFramesAt(X, Y);
                end;
            end;
        VK_DELETE:
            begin
                case mapMode of
                    MM_SC:
                        begin
                            if (scsel.Count + IfThen(scsel.FindSC(Cur_SC) <> -1,  0, 1)) >= level.sectors.Count then
                                exit;

                            scsel.AddSC(Cur_SC);
                            scsel.sort;

                            StartUndoRec('Delete Sector(s)');
                            for i := scsel.Count - 1 downto 0 do
                              begin
                                var secn := scsel.GetSC(i);
                                if ToolBar.IsLayerVisible(level.sectors[secn].Layer) then
                                  DeleteSector(level, secn);
                              end;
                            { updated in procedure }
                            scsel.Clear;
                        end;
                    MM_TH:
                        begin
                            if (thsel.Count + IfThen(thsel.FindTH(Cur_TH) <> -1,  0, 1)) >= level.things.Count then
                                exit;

                            thsel.AddTH(Cur_TH);
                            thsel.sort;

                            StartUndoRec('Delete Thing(s)');
                            for i := thsel.Count - 1 downto 0 do
                              begin
                                  nth := thsel.GetTH(i);
                                  th := level.things[nth];
                                  if ToolBar.IsLayerVisible(th.Layer) then
                                    begin
                                      Free3DO(th.a3DO);
                                      DeleteThing(level, nth);
                                    end;
                              end;
                            thsel.Clear;
                        end;
                    MM_FR:
                        begin
                            frsel.AddFR(Cur_TH, Cur_FR);
                            frsel.sort;
                            { SaveSelFramesUndo('Delete frame(s)',ch_deleted); }
                            StartUndoRec('Delete Frame(s)');
                            for i := frsel.Count - 1 downto 0 do
                            begin
                                frsel.GetTHFR(i, nth, nfr);
                                if ToolBar.IsLayerVisible(level.things[nth].Layer) then
                                  DeleteFrame(nth, nfr);
                            end;
                            frsel.Clear;
                        end;
                    MM_LT:
                        begin
                          if (ltsel.Count + IfThen(ltsel.FindLT(Cur_LT) <> -1,  0, 1)) >= level.lights.Count then
                                  exit;

                            ltsel.AddLT(Cur_LT);
                            ltsel.sort;
                            { SaveSelLightsUndo('Delete light(s)',ch_deleted); }
                            StartUndoRec('Delete Light(s)');
                            for i := ltsel.Count - 1 downto 0 do
                              begin
                                var nlt := ltsel.getLT(i);
                                if ToolBar.IsLayerVisible(level.lights[nlt].Layer) then
                                  DeleteLight(level, nlt);
                              end;
                            ltsel.Clear;
                        end;
                    MM_VX:
                        begin
                            vxsel.AddVX(Cur_SC, Cur_VX);
                            vxsel.sort;
                            if vxsel.Count = 1 then
                                StartUndoRec('Delete Vertex')
                            else
                                StartUndoRec('Delete Vertices');

                            a := 0;
                            for i := vxsel.Count - 1 downto 0 do
                            begin
                                vxsel.GetSCVX(i, X, Y);
                                if ToolBar.IsLayerVisible(level.sectors[X].Layer) then
                                  if DeleteVertex(level.sectors[X], Y,
                                    vxsel.Count <> 1) then
                                      Inc(a);
                            end;
                            PanMessageFmt(mt_info, '%d vertices deleted', [a]);
                            vxsel.Clear;

                        end;
                end;
                SetMapMode(mapMode);
            end;
        { ; } 186:
            begin
                StartStitch;
                exit;
            end;
        { ' } 222:
            begin
                DoStitch(ssCtrl in shift);
                exit;
            end;
        { / } 191:
            begin
                StraightenTexture(false, false);
                exit;
            end;
        { < } 188:
            begin
                ShiftTexture(st_left);
                exit;
            end;
        { > } 190:
            begin
                ShiftTexture(st_right);
                exit;
            end;
        { [ } 219:
            RaiseObject(ro_up);
        { ] } 221:
            RaiseObject(ro_down);

    else
        exit;
    end;
    Key := 0;
    Invalidate;
end;

{ Mouse operations }

Procedure TJedMain.Do_SelectAt(X, Y: Integer; fore: boolean);
Var
    s, SF, ED, th, VX, fr: Integer;
    v1, v2: TJKVertex;
    sec: TJKSector;
    thing: TJKThing;
    light: TSedLight;
    obj: TObject;
    fx, fy, fz, pch, yaw, rol: double;
begin
    if (LastSelXY.X = X) and (LastSelXY.X = X) and (renderer.Selected.Count <> 0)
    then
    begin
        if fore then
            if csel >= renderer.Selected.Count - 1 then
                csel := 0
            else
                Inc(csel);
        if not fore then
            if csel = 0 then
                csel := renderer.Selected.Count - 1
            else
                dec(csel);
    end
    else
        csel := -1;

    if csel = -1 then
    begin
        renderer.BeginPick(X, Y);
        Case mapMode of
            MM_SC:
                for s := 0 to level.sectors.Count - 1 do
                begin
                    sec := level.sectors[s];
                    if ToolBar.IsLayerVisible(sec.layer) then
                        renderer.PickPolygons(sec.surfaces, s);
                end;
            MM_SF:
                for s := 0 to level.sectors.Count - 1 do
                begin
                    sec := level.sectors[s];
                    if ToolBar.IsLayerVisible(sec.layer) then
                        for SF := 0 to sec.surfaces.Count - 1 do
                            renderer.PickPolygon(sec.surfaces[SF],
                              s + SF * 65536);
                end;
            MM_ED:
                for s := 0 to level.sectors.Count - 1 do
                begin
                    sec := level.sectors[s];
                    if ToolBar.IsLayerVisible(sec.layer) then
                        for SF := 0 to sec.surfaces.Count - 1 do
                            With sec.surfaces[SF] do
                                for ED := 0 to vertices.Count - 1 do
                                begin
                                    v1 := vertices[ED];
                                    v2 := vertices[NextVX(ED)];
                                    renderer.PickLine(v1, v2,
                                      s * 65536 + ED * 1024 + SF);
                                end;
                end;
            MM_TH:
                begin
                    SetRendererObjVertexSize;
                    for th := 0 to level.things.Count - 1 do
                    begin
                        thing := level.things[th];
                        if ToolBar.IsLayerVisible(thing.layer) then
                          PickThing(thing, th);
                    end;
                end;
            MM_FR:
                begin
                    SetRendererObjVertexSize;
                    for th := 0 to level.things.Count - 1 do
                    begin
                        thing := level.things[th];
                        if not ToolBar.IsLayerVisible(thing.layer) then
                            continue;

                        with thing Do
                            renderer.PickVertex(x, y, z, th * 65536 + 65535);
                        for fr := 0 to thing.vals.Count - 1 do
                            with thing.vals[fr] do
                            begin
                                if atype <> at_frame then
                                    continue;

                                GetFrame(fx, fy, fz, pch, yaw, rol);
                                if frsel.FindFR(th, fr) <> -1 then
                                   PickThingAt(thing, fx, fy, fz, pch, yaw, rol, th * 65536 + fr)
                                else
                                  renderer.PickVertex(fx, fy, fz, th * 65536 + fr);
                            end;
                    end;
                end;
            MM_VX:
                begin
                    SetRendererGeoVertexSize;
                    for s := 0 to level.sectors.Count - 1 do
                    begin
                        sec := level.sectors[s];
                        if ToolBar.IsLayerVisible(sec.layer) then
                            With sec do
                                for VX := 0 to vertices.Count - 1 do
                                    With vertices[VX] do
                                        renderer.PickVertex(X, Y, z,
                                          s + VX * 65536);
                    end;
                end;
            MM_LT:
                begin
                    SetRendererObjVertexSize;
                    for th := 0 to level.lights.Count - 1 do
                    begin
                        light := level.lights[th];
                        if ToolBar.IsLayerVisible(light.layer) then
                            With light do
                                renderer.PickVertex(position.x, position.y, position.z, th);
                    end;
                end;
            MM_Extra:
                begin
                    SetRendererGeoVertexSize;
                    for s := 0 to ExtraObjs.Count - 1 do
                    begin
                        obj := ExtraObjs[s];
                        if obj is TVertex then
                            With TJKVertex(obj) do
                                renderer.PickVertex(X, Y, z, s)
                        else if obj is TExtraLine then
                            With TExtraLine(obj) do
                                renderer.PickLine(v1, v2, s)
                        else if obj is TPolygon then
                            renderer.PickPolygon(TPolygon(obj), s);
                    end;
                end;
        end;
        renderer.EndPick;
        if renderer.Selected.Count <> 0 then
            csel := 0;
    end;

    { Lb.Caption:=IntToStr(Renderer.Selected.Count); }
    if (renderer.Selected.Count <> 0) then
    begin
        case mapMode of
            MM_SC:
                SetCurSC(renderer.Selected[csel]);
            MM_SF:
                begin
                    Cur_SC := renderer.Selected[csel] mod 65536;
                    Cur_SF := renderer.Selected[csel] div 65536;
                    SetCurSF(Cur_SC, Cur_SF);
                end;
            MM_ED:
                begin
                    VX := renderer.Selected[csel];
                    s  := VX div 65536;
                    SF := VX mod 1024;
                    ED := (VX mod 65536) div 1024;
                    SetCurED(s, SF, ED);
                end;
            MM_TH:
                SetCurTH(renderer.Selected[csel]);
            MM_FR:
                begin
                    Cur_FR := renderer.Selected[csel] mod 65536;
                    Cur_TH := renderer.Selected[csel] div 65536;
                    if Cur_FR = 65535 then
                        Cur_FR := -1;
                    SetCurFR(Cur_TH, Cur_FR);
                end;
            MM_VX:
                begin
                    Cur_SC := renderer.Selected[csel] mod 65536;
                    Cur_VX := renderer.Selected[csel] div 65536;
                    SetCurVX(Cur_SC, Cur_VX);
                end;
            MM_LT:
                SetCurLT(renderer.Selected[csel]);
            MM_Extra:
                SetCurEX(renderer.Selected[csel]);
        end;
        Invalidate;
    end;

    LastSelXY.X := X;
    LastSelXY.Y := Y;
end;

Procedure TJedMain.PickThing(th: TThing; id: integer);
begin
    PickThingAt(th, th.x, th.y, th.z, th.pch, th.yaw, th.rol, id);
end;

procedure TJedMain.PickThingAt(th: TThing; x, y, z, pitch, yaw, roll: double; id: integer);
begin
  renderer.PickVertex(x,y,z, id);
  case thingView of
        cv_Boxes:
            begin
              if th.bbox.x1 <> th.bbox.x2 then
                begin
                  BoxToPolys(th.bbox, tcube);
                  renderer.PickPolygonsAt(tcube, x, y, z, pitch, yaw, roll, id);
                end
            end;
        cv_Wireframes:
            begin
                if th.a3DO <> nil then
                  begin
                    for var i := 0 to th.a3DO.Meshes.Count - 1 do
                      renderer.PickPolygonsAt(th.a3DO.meshes[i].faces, x, y, z, pitch, yaw, roll, id);
                  end;
            end;
    end;
end;

function TJedMain.IsCleaveMapMode: Boolean;
begin
  Result := (mapMode = MM_SC) or (mapMode = MM_SF) or (mapMode = MM_ED);
end;

{ Cleave mouse functions }

Procedure TJedMain.Do_StartCleave(X, Y: Integer; snapToVertex: boolean);
begin
    if not IsCleaveMapMode then
    begin
      CancelMouseMode;
      exit;
    end;

    if bCleaveStarted then
      ReDraw;
        
    With cleaveOrg2 do
      if not GetMousePos(X, Y) then
        exit;

    var stg := bSnapToGrid or IsKeyPressed(VK_CONTROL); // sets snap to grid when control key is presse
    if not GetSnappedPosAt(cleaveOrg2, snapToVertex, stg, cleaveOrg) then
      exit;

    With cleaveOrg do
      renderer.ProjectPoint(x, y, z, cleaveOrg2.X, cleaveOrg2.Y);

    cleaveCurPos2 := cleaveOrg2;   
    SetMouseMode(MM_Cleave);
    bCleaveStarted := true;
end;

Procedure TJedMain.Do_ProceedCleave(X, Y: Integer; snapToVertex: boolean);
var
    fx, fy, fz: double;
begin
    if not bCleaveStarted then
      exit;

    Canvas.Pen.mode := pmXor;
    Canvas.Pen.Color := clCleaveLine.col;//clRed;
    Canvas.MoveTo(cleaveOrg2.X, cleaveOrg2.Y);
    Canvas.LineTo(cleaveCurPos2.x, cleaveCurPos2.y);
    Canvas.MoveTo(cleaveOrg2.X, cleaveOrg2.Y);

    if IsKeyPressed(VK_MENU) then // alt key (snap to XY axis)
      SnapPointToAxis(X,Y, cleaveOrg2, X, Y)
    else
      begin
        // Get projected point
        var stg := bSnapToGrid or IsKeyPressed(VK_CONTROL); // sets snap to grid when control key is pressed
        if not GetSnappedPosAt(X, Y, snapToVertex, stg, fx, fy, fz) then
          exit;
        renderer.ProjectPoint(fx, fy, fz, X, Y)
      end;

    Canvas.LineTo(X, Y);
    cleaveCurPos2 := TPoint.Create(X, Y);
end;

Procedure TJedMain.Do_EndCleave(X, Y: Integer; snapToVertex: boolean);
var
    endPos: TVector;
begin
    try
        if not bCleaveStarted then
            exit;

        var snapToGrid := bSnapToGrid or IsKeyPressed(VK_CONTROL); // sets snap to grid when control key is pressed
        if IsKeyPressed(VK_MENU) then // alt key (snap to XY axis)
          begin
            snapToGrid := False;
            snapToVertex := False;
            SnapPointToAxis(X,Y, cleaveOrg2, X, Y);
          end;

        if not GetSnappedPosAt(X, Y, snapToVertex, snapToGrid, endPos) then
            exit;

        if endPos = cleaveOrg then
          exit;

        var normal := renderer.gnormal;
        if not snapToGrid then
          normal := renderer.GetCameraForward;

        var cnormal := normal.Cross(endPos - cleaveOrg).Normalized;
        CleaveBy(cnormal, cleaveOrg.x, cleaveOrg.y, cleaveOrg.z);
    finally
        SetMouseMode(MM_Select);
        Invalidate;
        bCleaveStarted := false;
    end;
end;


{ Whip Ruler mouse functions }

Procedure TJedMain.Do_StartRuler(X, Y: Integer; snaptovertex: boolean);
begin
    if bRulerStarted then
      ReDraw;
      
    With rulerOrg2 do
      if not GetMousePos(Integer(X), Integer(Y)) then
        exit;

   With cleaveOrg2 do
      if not GetMousePos(X, Y) then
        exit;

    if not GetSnappedPosAt(rulerOrg2, snaptovertex, rulerOrg) then
      exit;
    
    With rulerOrg do
      renderer.ProjectPoint(X, Y, z, Integer(rulerOrg2.X), Integer(rulerOrg2.Y));
      
    rulerCurPos2 := rulerOrg2;
    SetMouseMode(MM_Ruler);
    bRulerStarted := true;
end;

Procedure TJedMain.Do_ProceedRuler(X, Y: Integer; snaptovertex: boolean);
var
    fx, fy, fz: double;
begin
    if not bRulerStarted then
        exit;
    Canvas.Pen.mode  := pmXor;
    Canvas.Pen.Color := clCleaveLine.col;
    Canvas.MoveTo(rulerOrg2.X, rulerOrg2.Y);
    Canvas.LineTo(rulerCurPos2.x, rulerCurPos2.y);
    Canvas.MoveTo(rulerOrg2.X, rulerOrg2.Y);

    if not GetSnappedPosAt(X, Y, snaptovertex, fx, fy, fz) then
      exit;

    renderer.ProjectPoint(fx, fy, fz, X, Y);

    Canvas.LineTo(X, Y);
    rulerCurPos2 := TPoint.Create(X, y);
end;

Procedure TJedMain.Do_EndRuler(X, Y: Integer; snaptovertex: boolean);
var
    x1, y1, z1, x2, y2, z2: double;
    cnormal: TVector;
    cleaved: boolean;
    i: Integer;
begin
    try
        if not bRulerStarted then
            exit;
    finally
        SetMouseMode(MM_Select);
        Invalidate;
        bRulerStarted := false;
    end;
end;

Procedure TJedMain.Do_StartRectSelect(X, Y: Integer);
begin
    bRectSelStarted := true;
    rectSelOrg.X := X;
    rectSelOrg.Y := Y;
    reactSelPos := rectSelOrg;
    SetMouseMode(MM_RectSelect);
end;

Procedure TJedMain.Do_ProceedRectSelect(X, Y: Integer);
begin
    Canvas.Pen.mode := pmXor;
    Canvas.Pen.Color := clRed;
    Canvas.Brush.Style := bsClear;
    Canvas.Rectangle(rectSelOrg.X, rectSelOrg.Y, reactSelPos.X, reactSelPos.Y);
    Canvas.Rectangle(rectSelOrg.X, rectSelOrg.Y, X, Y);
    reactSelPos.X := X;
    reactSelPos.Y := Y;
end;

Procedure TJedMain.Do_EndRectSelect(X, Y: Integer);
var
    i, SF, VX, ED, fr: Integer;
    ax, ay, az, pch, yaw, rol: double;
    allin: boolean;
begin
    renderer.BeginRectPick(rectSelOrg.X, rectSelOrg.Y, X, Y);
    case mapMode of
        MM_SC:
            begin
                for i := 0 to level.sectors.Count - 1 do
                    with level.sectors[i] do
                        if ToolBar.IsLayerVisible(layer) then
                        begin
                            allin := true;
                            for VX := 0 to vertices.Count - 1 do
                                with vertices[VX] do
                                    if not renderer.IsVertexInRect(X, Y, z) then
                                    begin
                                        allin := false;
                                        break;
                                    end;
                            if allin then
                                DO_SelSC(i);
                        end;
            end;
        MM_SF:
            begin
                for i := 0 to level.sectors.Count - 1 do
                    with level.sectors[i] do
                        if ToolBar.IsLayerVisible(layer) then
                            For SF := 0 to surfaces.Count - 1 do
                                if renderer.IsPolygonInRect(surfaces[SF]) then
                                    DO_SelSF(i, SF);

            end;
        MM_ED:
            begin
                for i := 0 to level.sectors.Count - 1 do
                    with level.sectors[i] do
                        if ToolBar.IsLayerVisible(layer) then
                            For SF := 0 to surfaces.Count - 1 do
                                with surfaces[SF] do
                                    for ED := 0 to vertices.Count - 1 do
                                        if renderer.IsLineInRect(vertices[ED],
                                        vertices[NextVX(ED)]) then
                                        DO_SelED(i, SF, ED);
            end;
        MM_VX:
            begin
                for i := 0 to level.sectors.Count - 1 do
                    with level.sectors[i] do
                        if ToolBar.IsLayerVisible(layer) then
                            for VX := 0 to vertices.Count - 1 do
                                with vertices[VX] do
                                    if renderer.IsVertexInRect(X, Y, z) then
                                        DO_SelVX(i, VX);
            end;
        MM_TH:
            begin
                for i := 0 to level.things.Count - 1 do
                    with level.things[i] do
                        if ToolBar.IsLayerVisible(layer) then
                            if renderer.IsVertexInRect(X, Y, z) then
                                DO_SelTH(i);
            end;
        MM_LT:
            begin
                for i := 0 to level.lights.Count - 1 do
                    with level.lights[i] do
                        if ToolBar.IsLayerVisible(layer) then
                            if renderer.IsVertexInRect(position.x, position.y, position.z) then
                                DO_SelLT(i);
            end;
        MM_FR:
            begin
                for i := 0 to level.things.Count - 1 do
                    with level.things[i] do
                        if ToolBar.IsLayerVisible(layer) then
                            for fr := 0 to Vals.Count - 1 do
                                with Vals[fr] do
                                begin
                                    if atype <> at_frame then
                                        continue;
                                    GetFrame(ax, ay, az, pch, yaw, rol);
                                    if renderer.IsVertexInRect(ax, ay, az) then
                                        DO_SelFR(i, fr);
                                end;
            end;
    end;
    renderer.EndRectPick;

    SetMouseMode(MM_Select);
    Invalidate;
end;

{ Mouse Drag functions }

Procedure TJedMain.Do_StartDrag(X, Y: Integer);
var
    fx, fy, fz: double;
    n, i: Integer;
    th, fr: Integer;
begin
    if not GetPosAt(X, Y, dragOrg) then
      exit;

    SetMouseMode(MM_Drag);
    with dragRefOrg do
        GetCurObjRefXYZ(x, y, z);

    case mapMode of
        MM_SC:
            StartUndoRec('Drag Sector(s)');
        MM_SF:
            StartUndoRec('Drag Surface(s)');
        MM_ED:
            StartUndoRec('Drag Edge(s)');
        MM_VX:
            StartUndoRec('Drag Vertices');
        MM_LT:
            StartUndoRec('Drag Light(s)');
        MM_TH:
            StartUndoRec('Drag Thing(s)');
        MM_FR:
            StartUndoRec('Drag Frame(s)');
        MM_Extra:
            if Assigned(OnExtraMove) then
                OnExtraMove(extraObjs[Cur_EX], false);
    end;
end;

Procedure TJedMain.GetCurObjRefXYZ(var rx, ry, rz: double);
var
    pch, yaw, rol: double;
begin
    rx := 0;
    ry := 0;
    rz := 0;
    case mapMode of
        MM_SC:
            begin
                var sec := level.GetSectorN(Cur_SC);
                if sec = nil then
                  exit;

                With sec.vertices[0] do
                begin
                    rx := x;
                    ry := y;
                    rz := z;
                end;
            end;
        MM_SF:
            begin
                var surf := level.GetSectorSurfaceN(Cur_SC, Cur_SF);
                if surf = nil then
                  exit;
                With surf.vertices[0] do
                begin
                    rx := x;
                    ry := y;
                    rz := z;
                end;
            end;
        MM_VX:
            begin
                var vx := level.GetSectorVertexN(Cur_SC, Cur_VX);
                if vx = nil then
                  exit;
                With vx do
                begin
                    rx := x;
                    ry := y;
                    rz := z;
                end;
            end;
        MM_ED:
            begin
                var surf := level.GetSectorSurfaceN(Cur_SC, Cur_SF);
                if (surf = nil) or not CheckSurfaceEdgeIndex(surf, Cur_ED) then
                  exit;

                With surf do
                begin
                    var vx := vertices[Cur_ED];
                    With vx do
                    begin
                        rx := x;
                        ry := y;
                        rz := z;
                    end;
                end;
            end;
        MM_TH:
            begin
                var th := level.GetThingN(Cur_TH);
                if th = nil then
                  exit;
                With th do
                  begin
                      rx := x;
                      ry := y;
                      rz := z;
                  end;
            end;
        MM_FR:
            begin
                var th := level.GetThingN(Cur_TH);
                if th = nil then
                  exit;

                if Cur_FR = -1 then
                  begin
                      With th do
                      begin
                          rx := x;
                          ry := y;
                          rz := z;
                      end;
                  end
                else
                  begin
                      var vl := th.vals[Cur_FR];
                      vl.GetFrame(rx, ry, rz, pch, yaw, rol);
                  end;
            end;
        MM_LT:
            begin
                var lt := level.GetLightN(Cur_LT);
                With lt do
                  begin
                      rx := position.x;
                      ry := position.y;
                      rz := position.z;
                  end;
            end;
        MM_Extra:
            begin
                if (Cur_EX < 0) or (extraObjs.Count = 0) then
                  exit;

                var ext := extraObjs[Cur_EX];
                if ext is TVertex then
                    With TVertex(ext) do
                    begin
                        rx := x;
                        ry := y;
                        rz := z;
                    end;
                if ext is TExtraLine then
                    With TExtraLine(ext).v1 do
                    begin
                        rx := x;
                        ry := y;
                        rz := z;
                    end;
                if ext is TPolygon then
                    With TPolygon(ext).vertices[0] do
                    begin
                        rx := x;
                        ry := y;
                        rz := z;
                    end;
            end;
    end;
end;

Procedure TJedMain.Do_ProceedDrag(X, Y: Integer; snapToVx: boolean; snapToAxis, yaxis: boolean);
    procedure GetSnapToGridVector(pos: TVector; var svec: Tvector);
      var
         pline, gpos: TVector;
    begin
        { Find distance to plane - signed }
        with renderer do
          begin
              var dot := gnormal.Dot(gridX, gridY, gridZ);

              PlaneLineXn(gnormal, dot, pos.x, pos.y, pos.z,
                pos.x + gnormal.dx, pos.y + gnormal.dy, pos.z + gnormal.dz,
                pline.x, pline.y, pline.z);

              renderer.GetNearestGrid(pline.x, pline.y, pline.z, gpos.x, gpos.y, gpos.z);
              svec := gpos - pline + svec;
          end;
    end;

    function GetDragVector(curRefPos: TVector; var dvec: Tvector) : boolean;
      var curDragPos: TVector;
    begin
        Result := false;
        if not GetPosAt(X, Y, curDragPos) then
          exit;

        dvec := curDragPos - dragOrg; // default delta for no snap
        Result := true;

        if snapToAxis then
            with renderer do
            begin
                if yaxis then
                    dvec := gynormal * gynormal.Dot(dvec)
                else
                    dvec := gxnormal * gxnormal.Dot(dvec);

//                if bSnapToGrid then
//                    GetSnapToGridVector(curRefPos + dvec, dvec);
//                exit;
            end;

        if snapToVx then
           begin
              var vxpos: TVector;
              if not GetNearestWorldVertexToLine(level, curRefPos + dvec, renderer.GetCameraForward, VertSnapMaxDist, vxpos) then
                exit;
              dvec := vxpos - curRefPos;
            end
        else if bSnapToGrid then
          GetSnapToGridVector(curRefPos + dvec, dvec);
    end;

Var
    curRefPos, tvec: TVector;
begin
    if not GetDragVector(dragRefOrg, tvec) then
      exit;

    GetCurObjRefXYZ(curRefPos.x, curRefPos.y, curRefPos.z);
    tvec :=  dragRefOrg - curRefPos + tvec;
    case mapMode of
        MM_SC:
            TranslateSectors(level, scsel, Cur_SC, tvec.x, tvec.y, tvec.z);
        MM_SF:
            TranslateSurfaces(Level, sfsel, Cur_SC, Cur_SF, tvec.x, tvec.y, tvec.z);
        MM_VX:
            TranslateVertices(Level, vxsel, Cur_SC, Cur_VX, tvec.x, tvec.y, tvec.z);
        MM_ED:
            TranslateEdges(level, edsel, Cur_SC, Cur_SF, Cur_ED, tvec.x, tvec.y, tvec.z);
        MM_TH:
            TranslateThings(level, thsel, Cur_TH, tvec.x, tvec.y, tvec.z, MoveFrames);
        MM_FR:
            if Cur_FR = -1 then
                TranslateThingKeepFrame(level, Cur_TH, tvec.x, tvec.y, tvec.z)
            else
                TranslateFrames(level, frsel, Cur_TH, Cur_FR, tvec.x, tvec.y, tvec.z);
        MM_LT:
            TranslateLights(level, ltsel, Cur_LT, tvec.x, tvec.y, tvec.z);
        MM_Extra:
            TranslateExtras(Cur_EX, tvec.x, tvec.y, tvec.z);
    end;

    Invalidate;
end;

Procedure TJedMain.Do_EndDrag(X, Y: Integer);
begin
    { Case mapMode of
      mm_th: LayerThing(Cur_TH);
      end; }
    SetMouseMode(MM_Select);
end;

Procedure TJedMain.Do_StartTranslateCam(X, Y: Integer);
begin
    camTranslatePos.X := X;
    camTranslatePos.Y := Y;
    SetMouseMode(MM_TranslateCam);
end;

Procedure TJedMain.Do_TranslateCam(X, Y: Integer);
var
    nx, ny, nz, nx1, ny1, nz1: double;
begin
    if (camTranslatePos.X = X) and (camTranslatePos.Y = Y) then exit;
    With renderer do
    begin
        if not GetXYZonPlaneAt(X, Y, renderer.zv, CamX, CamY, CamZ, nx, ny, nz)
          then exit;

        if not GetXYZonPlaneAt(camTranslatePos.X, camTranslatePos.Y, renderer.zv, CamX, CamY, CamZ, nx1, ny1, nz1)
          then exit;

        const s = Min(renderer.DpiScale * 10000.0, 1.0);
        CamX := CamX + (nx - nx1)*s;
        CamY := CamY + (ny - ny1)*s;
        CamZ := CamZ + (nz - nz1)*s;
    end;

    { nx:=Renderer.xv;
      ny:=Renderer.yv;
      nz:=Renderer.zv;

      ny.dx:=0; ny.dy:=scale*dy; ny.dz:=0;
      RotateVector(ny,PCH,YAW,ROL);
      nx.dx:=scale*dx; nx.dy:=0; nx.dz:=0;
      RotateVector(nx,PCH,YAW,ROL);
      nz.dx:=0; nz.dy:=0; nz.dz:=scale*dx;
      RotateVector(nz,PCH,YAW,ROL);
      With Renderer do
      begin CamX:=CamX+nz.dx; CamY:=CamY+nz.dy; CamZ:=CamZ+nz.dz; end; }

    Do_StartTranslateCam(X, Y);
    Invalidate;
end;

Procedure TJedMain.Do_StartRotateCam(X, Y: Integer);
begin
    if mouseMode = MM_RotateCam then
        exit;
    gridTranslateOrg2.X := X;
    gridTranslateOrg2.Y := Y;
    With gridTranslateOrg do
    begin
        x := rPch;
        y := rYaw;
        z := rRol;
    end;
    camRotOrgX := renderer.xv;
    camRotOrgY := renderer.yv;
    camRotOrgZ := renderer.zv;


//      var nx, ny, nz: double;
//      with renderer do
//        begin
//            if not GetXYZonPlaneAt(X, Y, renderer.zv, CamX, CamY, CamZ, nx, ny, nz)
//        then exit;
//
//
//        CamX :=  nx;
//        CamY :=  ny;
//        CamZ :=  nz;
//        end;

    SetMouseMode(MM_RotateCam);
end;

Procedure TJedMain.Do_RotateCam(X, Y: Integer);
var
    dx, dy: Integer;
    nx, ny, nz: TVector;
begin
    dx := X - gridTranslateOrg2.X;
    dy := Y - gridTranslateOrg2.Y;
//      var cx, cy, cz: double;
//     With renderer do
//    begin
//        var nx1, ny1, nz1, nx2, ny2, nz2: double;
//        if not GetXYZonPlaneAt(X, Y, renderer.zv, CamX, CamY, CamZ, nx1, ny1, nz1)
//        then exit;
//
//        if not GetXYZonPlaneAt(GOrgXY.X, GOrgXY.Y, renderer.zv, CamX, CamY, CamZ, nx2, ny2, nz2)
//        then exit;
//
//        cx := nx2;
//        cy := ny2;
//        cz := nz2;
//        CamX := CamX+ nx2;
//        CamY := CamY+ ny2;
//        CamZ := CamZ +nz2;
//    end;

    if MapRot = MR_old then
      begin
        const s = Min(renderer.DpiScale * 100.0, 1.0);
        rRol := gridTranslateOrg.z - dx * s;
        rPch := gridTranslateOrg.x - dy * s;
  //      if rRol > 360 then
  //        rRol := rRol - 360
  //      else if rRol < 0 then
  //        rRol := rRol + 360;
  //      if rPch > 360 then
  //        rPch := rPch - 360
  //      else if rPch < 0 then
  //        rPch := rPch + 360;

       // PanMessageFmt(mt_Info, 'pch: %f rol: %f', [rPch, rRol]);
        SetRendFromPYR;
      end
    else
      begin
          nx := camRotOrgX;
          ny := camRotOrgY;
          nz := camRotOrgZ;

          RotAxisAngle(nx, ny, dx); { l/r }
          RotAxisAngle(nz, ny, dy); { u/d }

          With nz do
              renderer.SetZ(dx, dy, dz);
          With nx do
              renderer.SetX(dx, dy, dz);
  //        With ny do
  //            renderer.SetY(dx, dy, dz);

      end;

//   with renderer do
//          SetGridNormal(zv.dx, zv.dy, zv.dz); // New code, fixes cleaving
    Invalidate;
end;

Procedure TJedMain.Do_StartTranslateGrid(X, Y: Integer);
begin
    gridTranslateOrg2.X := X;
    gridTranslateOrg2.Y := Y;
    With gridTranslateOrg do
    begin
        x := renderer.gridX;
        y := renderer.gridY;
        z := renderer.gridZ;
    end;
end;

Procedure TJedMain.Do_TranslateGrid(X, Y: Integer);
var
    pnorm: TVector;
    a, nx, ny, nz: double;
begin
    With renderer do
    begin
        pnorm := zv;
        if not GetXYZonPlaneAt(X, Y, pnorm, CamX, CamY, CamZ, nx, ny, nz) then
            exit;
        try
            a := 1 / GridMoveStep;
            nx := Int(nx * a) / a;
            ny := Int(ny * a) / a;
            nz := Int(nz * a) / a;
        except
            on Exception do;
        end;
        GridX := nx;
        GridY := ny;
        GridZ := nz;
        Invalidate;
    end;
end;

Procedure TJedMain.Do_StartRotateGrid(X, Y: Integer);
begin
    gridTranslateOrg2.X := X;
    gridTranslateOrg2.Y := Y;
    With gridTranslateOrg do
    begin
        x := renderer.gridX;
        y := renderer.gridY;
        z := renderer.gridZ;
    end;
end;

Procedure TJedMain.Do_RotateGrid(X, Y: Integer);
var
    dx, dy: double;
begin
    dx := X - gridTranslateOrg2.X;
    dy := Y - gridTranslateOrg2.Y;

end;

Procedure TJedMain.Do_StartCreateSC(X, Y: Integer; snapToVertex: boolean);
begin
    if bCreateSCStarted then
        ReDraw;
        
    With cleaveOrg2 do
        if not GetMousePos(Integer(X), Integer(Y)) then
            exit;

    if not GetSnappedPosAt(cleaveOrg2, snapToVertex, cleaveOrg) then
        exit;

    With cleaveOrg do
        renderer.ProjectPoint(x, y, z, Integer(cleaveOrg2.X), Integer(cleaveOrg2.Y));

    cleaveCurPos2 := cleaveOrg2;
    SetMouseMode(MM_CreateSector);
    bCreateSCStarted := true;
end;

Procedure TJedMain.Do_ProceedCreateSC(X, Y: Integer; snapToVertex: boolean);
var
    fx, fy, fz: double;
begin
    if not bCreateSCStarted then
        exit;

    Canvas.Pen.mode := pmXor;
    Canvas.Pen.Color := clRed;
    Canvas.MoveTo(cleaveOrg2.X, cleaveOrg2.Y);
    Canvas.LineTo(cleaveCurPos2.X, cleaveCurPos2.Y);
    Canvas.MoveTo(cleaveOrg2.X, cleaveOrg2.Y);

    if not GetSnappedPosAt(X, Y, snapToVertex, fx, fy, fz) then
        exit;

    renderer.ProjectPoint(fx, fy, fz, X, Y);
    Canvas.LineTo(X, Y);
    cleaveCurPos2 := TPoint.Create(X, Y);
end;

Procedure TJedMain.Do_EndCreateSC(X, Y: Integer; snapToVertex: boolean);
var
    endPos, nx, ny, nz: TVector;
    vl: double;
    sc, nsc: TJKSector;
    box: TBox;
    sec: TJKSector;
begin
    try
        if not bCreateSCStarted then
            exit;

        if not GetSnappedPosAt(X, Y, snapToVertex, endPos) then
            exit;

        nx := endPos - cleaveOrg;
        vl := nx.Normalize;
        sc := ToolBar.GetNewShape;

        if vl < 0.1 then
          begin
            vl := 1;
            FindBBox(sc, box);
            vl := box.x2 - box.x1;

            nx := renderer.gxnormal;
            if not bSnapToGrid then
              nx := renderer.GetCameraRight;
          end;

        nx.Scale(vl);

        nz := renderer.gnormal;
        if not bSnapToGrid then
          nz := -renderer.GetCameraForward;
        nz.Scale(vl);

        ny := nz.Cross(nx).Normalized;
        ny.Scale(vl);

        // Recalculate nx in case angled nx (snap to vertex, etc...)
        // otherwise it could result in in curved sector
        nx := ny.Cross(nz).Normalized;
        nx.Scale(vl);

        sec := level.sectors[Cur_SC];
        nsc := level.NewSector;
        DuplicateSector(sc, nsc, 0, 0, 0, nx, ny, nz, cleaveOrg.x, cleaveOrg.y, cleaveOrg.z);
        nsc.Assign(sec);
        level.sectors.Add(nsc);
        level.RenumSecs;

        StartUndoRec('Create Sector');
        SaveSecUndo(nsc, ch_added, sc_both);

        SectorAdded(nsc);
        SetCurSC(level.sectors.Count - 1);
    finally
        SetMouseMode(MM_Select);
        bCreateSCStarted := false;
        Invalidate;
    end;
end;

procedure TJedMain.FormMouseDown(sender: TObject; Button: TMouseButton;
  Shift: TShiftState; X, Y: Integer);
var
    t: TJKThing;
begin
    case Button of
      TMouseButton.mbLeft:
        case mouseMode of
            MM_Select:
                ;
            MM_Cleave:
                Do_StartCleave(X, Y, ssShift in Shift);
            MM_CreateSector:
                DO_StartCreateSC(X, Y, ssShift in Shift);
            MM_Ruler:
                Do_StartRuler(X, Y, ssShift in Shift);
        end;
      TMouseButton.mbRight: ;
      TMouseButton.mbMiddle:
        if GetMousePos(X, Y) then
          begin
            if [ssShift, ssMiddle] = Shift then
              Do_StartTranslateCam(X, Y)
            else
              Do_StartRotateCam(X, Y);
          end;
    end;
end;

procedure TJedMain.FormMouseUp(sender: TObject; Button: TMouseButton;
  Shift: TShiftState; X, Y: Integer);
begin
    case Button of
        TMouseButton.mbLeft:
            Case mouseMode of
                MM_Select:
                    begin
                        Do_SelectAt(X, Y, ssAlt in Shift);
                        if ssShift in Shift then
                            DO_MultiSelect;
                    end;
                MM_CreateSector:
                    DO_EndCreateSC(X, Y, ssShift in Shift);
                MM_Drag:
                    Do_EndDrag(X, Y);
                MM_Cleave:
                    Do_EndCleave(X, Y, ssShift in Shift);
                MM_Ruler:
                    Do_EndRuler(X, Y, ssShift in Shift);
                MM_RectSelect:
                    DO_EndRectSelect(X, Y);
            end;
        TMouseButton.mbRight: ;
        TMouseButton.mbMiddle: SetMouseMode(MM_Select);
    end;
end;

procedure TJedMain.FormMouseWheel(Sender: TObject; Shift: TShiftState;
  WheelDelta: Integer; MousePos: TPoint; var Handled: Boolean);
  var dx, dy, dz: double;
begin
  if WheelDelta = 0 then exit;

  if Shift <> [] then
    begin
      if ((mapMode = MM_TH) or (mapMode = MM_FR)) and (ssCtrl in Shift) then
      begin
          dx := 0;
          dy := 0;
          dz := 0;

          var axis := -1;
          const angle = Round(WheelDelta * 0.008);
          if Shift = [ssCtrl] then
            axis := rt_z
          else if Shift = [ssShift, ssCtrl] then
            axis := rt_x
          else if Shift = [ssAlt, ssCtrl] then
            axis := rt_y;

          if axis <> -1 then
            begin
                RotateObject(angle, axis);
            end;
      end;
    end
  else   // Zoom Map
    begin
        MousePos := ScreenToClient(MousePos);
        With renderer do
          begin
              var ny := renderer.zv;
              if not GetXYZonPlaneAt(MousePos.X, MousePos.Y, ny, CamX, CamY, CamZ, dx, dy, dz) then
                  exit;

              var zoomed := false;
              if WheelDelta > 0 then
                zoomed := ZoomIn
              else
                zoomed := ZoomOut;
              if not zoomed then
                exit;

              CamX := dx;
              CamY := dy;
              CamZ := dz;

              GetXYZonPlaneAt(MousePos.X, MousePos.Y, ny, CamX, CamY, CamZ, dx, dy, dz);
              CamX := dx;
              CamY := dy;
              CamZ := dz;

              Invalidate;
        end;
    end;
end;

procedure TJedMain.FormMouseMove(sender: TObject; Shift: TShiftState; X, Y: Integer);
var
    msX, msY: Integer;
    gX, gY, gZ: double;
begin
    Case mouseMode of
        MM_Select:
            begin
                if (ssLeft in Shift) and (ssCtrl in Shift) then
                    Do_StartDrag(X, Y)
                else if (ssLeft in Shift) and (ssAlt in Shift) then
                    DO_StartRectSelect(X, Y);
                snaptoy := false;
            end;
        MM_Drag:
            Do_ProceedDrag(X, Y, ssShift in Shift, ssAlt in Shift, snaptoy);
        MM_Cleave:
            Do_ProceedCleave(X, Y, ssShift in Shift);
        MM_CreateSector:
            DO_ProceedCreateSC(X, Y, ssShift in Shift);
        MM_Ruler:
            Do_ProceedRuler(X, Y, ssShift in Shift);
        MM_TranslateGrid:
            Do_TranslateGrid(X, Y);
        MM_TranslateCam:
            Do_TranslateCam(X, Y);
        MM_RotateCam:
          begin
            if (ssShift in Shift) and (ssCtrl in Shift) then
              X := gridTranslateOrg2.X
            else if ssCtrl in Shift then
              Y := gridTranslateOrg2.Y;
            Do_RotateCam(X, Y);
          end;
        MM_RectSelect:
            DO_ProceedRectSelect(X, Y);
    end;

    //if GetMousePos(msX, msY) and renderer.GetCameraAt(X, Y, gX, gY, gZ) then  // <--- TODO: use this at some point
    if GetMousePos(msX, msY) and renderer.GetGridAt(msX, msY, gX, gY, gZ) then
        PXYZ.Caption := Format('X,Y,Z: %2.4f, %2.4f, %2.4f', [gX, gY, gZ]);

end;

Procedure TJedMain.Import3DO(const name: string; const clearCurrentLevel: Boolean);
var
    a3DO: T3DO;
    mesh: T3DOMesh;
    v3: TVertex;
    v: TJKVertex;
    sec: TJKSector;
    surf: TJKSurface;
    face: T3DOFace;
    tv: TTXVertex;
    hnode: THNode;
begin
    var fileVersion : double := 0;
    a3DO := T3DO.CreateFrom3DO(name, 0, fileVersion);
    var fixUVs := IsClose(fileVersion, 2.3);
    if clearCurrentLevel then Level.Clear;
    try
        for var i := 0 to a3DO.Meshes.Count - 1 do
        begin
            mesh := a3DO.Meshes[i];
            sec := Level.NewSector;
            //sec.Flags := sec.Flags or SECF_3DO;
            sec.Layer := Level.AddLayer(mesh.name);
            Level.Sectors.Add(sec);
            for var j := 0 to mesh.vertices.Count - 1 do
            begin
                v3 := mesh.vertices[j];
                v3.num := j;
                v := sec.newVertex;
                v.X := v3.X;
                v.Y := v3.Y;
                v.z := v3.z;
            end;

            for var j := 0 to mesh.Faces.Count - 1 do
            begin
                face := mesh.Faces[j];
                surf := sec.NewSurface;
                sec.surfaces.Add(surf);
                With surf do
                begin
                    adjoin         := nil;
                    Material       := a3DO.GetMat(face.imat);
                    geo            := face.geo;
                    light          := face.light;
                    tex            := face.tex;
                    FaceFlags      := face.faceflags;
                    ExtraLight     := face.extraLight;
                    normal         := face.normal;
                end;

                var matSize := TSize.Create(1,1);
                if fixUVs and (not surf.Material.IsEmpty) then
                  try
                    matSize := surf.GetMaterialSize;
                  except on E:Exception do PanMessage(mt_warning,
                    Format('Cannot load %s for mesh %d face %d: %s' + sLineBreak
                      + '  Imported surface will have invalid UV coords!',
                      [surf.Material, i, j, e.message]));
                  end;

                for var k := 0 to face.vertices.Count - 1 do
                begin
                    surf.AddVertex(sec.vertices[face.vertices[k].num]);
                    var uvtx := surf.TXVertices[k];
                    uvtx.Assign(face.TXVertices[k]);
                    uvtx.u     := uvtx.u * matSize.Width;
                    uvtx.v     := uvtx.v * matSize.Height;
                end;
//                for k := n - 1 downto 0 do
//                begin
//                    surf.AddVertex(sec.vertices[face.vertices[k].num]);
//                    surf.TXVertices[n - k - 1].Assign(face.TXVertices[k]);
//                end;
                surf.Recalc;
                FindUVScales(surf);
                if CompareValue(surf.normal.z, 0.6, CloseEnough) = GreaterThanValue then // If less LE 40�
                  surf.SurfFlags := surf.SurfFlags or SF_Floor;
            end;
        end;

        for var i := 0 to a3DO.hnodes.Count - 1 do
        begin
            hnode := Level.New3DONode;
            hnode.Assign(a3DO.hnodes[i]);
            Level.h3donodes.Add(hnode);
            hnode.pivotx := 0;
            hnode.pivoty := 0;
            hnode.pivotz := 0;
        end;

    finally
        if clearCurrentLevel then
          begin
            ClearUndoBuffer;
            Level.Things.Add(Level.NewThing);
        end;
        Level.RenumSecs;
        for var i := 0 to level.sectors.Count - 1 do
            Level.Sectors[i].Renumber;

        if sec <> nil then SetCurSC(sec.num);
        a3DO.free;
    end;
end;

procedure TJedMain.Import1Click(sender: TObject);
var
    th: TJKThing;
    i, nsec: Integer;
    Scale: double;
    s: string;
    a3DO: T3DO;
begin
    if not askSave then
        exit;

    With GetFileOpen do
    begin
        Filter := 'All Importable files|*.lev;*.gob;*.asc;*.3do|DF files(*.LEV;*.GOB)|*.LEV;*.GOB|3D Studio ASC|*.asc|JK 3DOs|*.3do;*.gob';
        If Execute then
        begin
            ResettingEditor;

            s := LowerCase(ExtractFileExt(FileName));
            if s = '.3do' then
              begin
                { a3DO:=T3DO.CreateFrom3DO(FileName,0);
                  a3Do.SaveToFile('i:\a.3do');
                  a3Do.Free; }
                var clearCurrent := MsgBox('Do you want to import 3DO sector(s) to the current project?', 'Info', MB_YESNO) <> ID_YES;
                Import3DO(FileName, clearCurrent);
                if not clearCurrent then
                  begin
                    LevelChanged;
                    Preview3D.ReloadLevel;
                    SyncRecents;
                    exit;
                  end;
              end
            else if s = '.asc' then
              begin
                  level.ImportAsc(FileName);
              end
            else
              begin
                  Scale := 40;
                  s := '40';
                  if DFImport.ShowModal <> mrOK then
                      exit;
                  Scale := DFImport.VScale.AsFloat;
                  level.ImportLev(FileName, Scale, DFImport.RGTX.ItemIndex);
              end;

            PMsg.Caption := '';
            LevelFile := 'Untitled';
            NewLevel := true;
            ResetEditor(true);
            level.kind := CurrentProject;
            for i := 0 to level.things.Count - 1 do
            begin
                th := level.things[i];
                th.z := th.z - th.bbox.z1;
                nsec := FindSectorForThing(th);
                if nsec <> -1 then
                    th.sec := level.sectors[nsec];
                if th.sec = nil then
                    th.Layer := level.AddLayer('Orphan things')
                else
                begin
                    s := level.GetLayerName(th.sec.Layer);
                    s := 'Things' + Copy(s, 6, length(s));
                    th.Layer := level.AddLayer(s);
                end;
            end;
        end;
        ToolBar.LoadLayers;
    end;
end;

procedure TJedMain.FormPaint(sender: TObject);
begin
    VerifySelection;
    VerifyMultiSelection;
    ReDraw;
    if updatestuff then
    begin
        EditObject;
        updatestuff := false;
    end;
end;

procedure TJedMain.SetMapGeoBackfaceCullingClick(Sender: TObject);
begin
    EnableMapGeoBackfaceCull(not bMapGeoBackfaceCull);
end;

Procedure TJedMain.SetMapMode(mm: Integer);
var
    oldmode: Integer;

    procedure ToSCMsel(mode: Integer);
    var
        i: Integer;
        sc, a, b: Integer;
    begin
        case mode of
            MM_SF:
                begin
                    scsel.Clear;
                    for i := 0 to sfsel.Count - 1 do
                    begin
                        sfsel.GetSCSF(i, sc, a);
                        scsel.AddSC(sc);
                    end;
                end;
            MM_ED:
                begin
                    scsel.Clear;
                    for i := 0 to edsel.Count - 1 do
                    begin
                        edsel.GetSCSFED(i, sc, a, b);
                        scsel.AddSC(sc);
                    end;
                end;
            MM_VX:
                begin
                    scsel.Clear;
                    for i := 0 to vxsel.Count - 1 do
                    begin
                        vxsel.GetSCVX(i, sc, a);
                        scsel.AddSC(sc);
                    end;
                end;
        end;
    end;

    procedure ToSFMsel(mode: Integer);
    var
        i: Integer;
        sc, SF, b: Integer;
        sec: TJKSector;
    begin
        case mode of
            MM_SC:
                begin
                    sfsel.Clear;
                    for i := 0 to scsel.Count - 1 do
                    begin
                        sc := scsel.GetSC(i);
                        sec := level.sectors[sc];
                        for SF := 0 to sec.surfaces.Count - 1 do
                            sfsel.AddSF(sc, SF);
                    end;
                end;
            MM_ED:
                begin
                    sfsel.Clear;
                    for i := 0 to edsel.Count - 1 do
                    begin
                        edsel.GetSCSFED(i, sc, SF, b);
                        sfsel.AddSF(sc, SF);
                    end;
                end;
            MM_VX:
                begin
                    sfsel.Clear;
                end;
        end;
    end;

    procedure ToEDMsel(mode: Integer);
    var
        i: Integer;
        sc, SF, ED: Integer;
        sec: TJKSector;
    begin
        case mode of
            MM_SC:
                begin
                    edsel.Clear;
                    for i := 0 to scsel.Count - 1 do
                    begin
                        sc := scsel.GetSC(i);
                        sec := Level.Sectors[sc];
                        for SF := 0 to sec.surfaces.Count - 1 do
                            With sec.surfaces[SF] do
                                for ED := 0 to vertices.Count - 1 do
                                    edsel.addED(sc, SF, ED);
                    end;
                end;
            MM_SF:
                begin
                    edsel.Clear;
                    for i := 0 to sfsel.Count - 1 do
                    begin
                        sfsel.GetSCSF(i, sc, SF);
                        with level.sectors[sc].surfaces[SF] do
                            for ED := 0 to vertices.Count - 1 do
                                edsel.addED(sc, SF, ED);
                    end;
                end;
            MM_VX:
                begin
                    edsel.Clear;
                end;
        end;
    end;

    procedure ToVXMsel(mode: Integer);
    var
        i: Integer;
        sc, SF, ED, VX: Integer;
        sec: TJKSector;
        surf: TJKSurface;
    begin
        case mode of
            MM_SC:
                begin
                    vxsel.Clear;
                    for i := 0 to scsel.Count - 1 do
                    begin
                        sc := scsel.GetSC(i);
                        sec := Level.Sectors[sc];
                        for VX := 0 to sec.vertices.Count - 1 do
                            vxsel.AddVX(sc, VX);
                    end;
                end;
            MM_SF:
                begin
                    vxsel.Clear;
                    for i := 0 to sfsel.Count - 1 do
                    begin
                        sfsel.GetSCSF(i, sc, SF);
                        with level.sectors[sc].surfaces[SF] do
                            for VX := 0 to vertices.Count - 1 do
                                vxsel.AddVX(sc, vertices[VX].num);
                    end;
                end;
            MM_ED:
                begin
                    vxsel.Clear;
                    for i := 0 to edsel.Count - 1 do
                    begin
                        edsel.GetSCSFED(i, sc, SF, ED);
                        surf := level.sectors[sc].surfaces[SF];
                        vxsel.AddVX(sc, surf.vertices[ED].num);
                        vxsel.AddVX(sc, surf.vertices[surf.NextVX(ED)].num);
                    end;
                end;
        end;
    end;

    procedure ToFRMsel(mode: Integer);
    var
        i: Integer;
        th, fr: Integer;
        thing: TJKThing;
    begin
        case mode of
            MM_TH:
                begin
                    frsel.Clear;
                    for i := 0 to thsel.Count - 1 do
                    begin
                        th := thsel.GetTH(i);
                        thing := Level.Things[th];
                        for fr := 0 to thing.Vals.Count - 1 do
                            if thing.Vals[fr].atype = at_frame then
                                frsel.AddFR(th, fr);
                    end;
                end;
        end;
    end;

    procedure ToTHMsel(mode: Integer);
    var
        i: Integer;
        th, a, b: Integer;
    begin
        case mode of
            MM_FR:
                begin
                    thsel.Clear;
                    for i := 0 to frsel.Count - 1 do
                    begin
                        frsel.GetTHFR(i, th, a);
                        thsel.AddTH(th);
                    end;
                end;
        end;
    end;

begin
    oldmode := mapMode;
    mapMode := mm;
    Case mm of
        MM_SC:
            begin
                ToSCMsel(oldmode);
                SetCurSC(Cur_SC);
            end;
        MM_SF:
            begin
                ToSFMsel(oldmode);
                SetCurSF(Cur_SC, Cur_SF);
            end;
        MM_VX:
            begin
                ToVXMsel(oldmode);
                SetCurVX(Cur_SC, Cur_VX);
            end;
        MM_TH:
            begin
                ToTHMsel(oldmode);
                SetCurTH(Cur_TH);
            end;
        MM_FR:
            begin
                ToFRMsel(oldmode);
                SetCurFR(Cur_TH, Cur_FR);
            end;
        MM_ED:
            begin
                ToEDMsel(oldmode);
                SetCurED(Cur_SC, Cur_SF, Cur_ED);
            end;
        MM_LT:
            SetCurLT(Cur_LT);
        MM_Extra:
            SetCurEX(Cur_EX);
    else
        begin
            ItemEdit.hide;
            exit;
        end;
    end;

    BNSC.Down := mm = MM_SC;
    BNSF.Down := mm = MM_SF;
    BNVX.Down := mm = MM_VX;
    BNED.Down := mm = MM_ED;
    BNTH.Down := mm = MM_TH;
    BNFR.Down := mm = MM_FR;
    BNLT.Down := mm = MM_LT;
    Invalidate;
end;

Procedure TJedMain.EnableMapGeoBackfaceCull(enable: Boolean);
begin
  bMapGeoBackfaceCull := enable;
  SetMapGeoBackfaceCulling.Caption := 'Disable Backface Cull';
  SetMapGeoBackfaceCulling.Hint := 'Disable map geometry back-face culling';
  if not bMapGeoBackfaceCull then
    begin
      SetMapGeoBackfaceCulling.Caption := 'Enable Backface Cull';
      SetMapGeoBackfaceCulling.Hint := 'Enable map geometry back-face culling'
    end;

  if renderer <> nil then ReDraw;
end;

Procedure TJedMain.SetCurSC(sc: Integer);
begin
    Cur_SC := sc;
    if Cur_SC >= level.sectors.Count then
        Cur_SC := 0;
    if Cur_SC < 0 then
        Cur_SC := level.sectors.Count - 1;
    EditObject;
end;

Procedure TJedMain.SetCurSF(sc, SF: Integer);
begin
    Cur_SC := sc;
    Cur_SF := SF;
    if sc >= level.sectors.Count then
        Cur_SC := 0;
    if Cur_SC < 0 then
        Cur_SC := level.sectors.Count - 1;

    with level.sectors[Cur_SC] do
    begin
        if Cur_SF < 0 then
            Cur_SF := surfaces.Count - 1;
        if Cur_SF >= surfaces.Count then
            Cur_SF := 0;
    end;
    EditObject;
end;

Procedure TJedMain.SetCurVX(sc, VX: Integer);
begin
    Cur_SC := sc;
    Cur_VX := VX;
    if sc >= level.sectors.Count then
        Cur_SC := 0;
    if sc < 0 then
        Cur_SC := level.sectors.Count - 1;

    with level.sectors[Cur_SC] do
    begin
        if VX < 0 then
            Cur_VX := vertices.Count - 1;
        if VX >= vertices.Count then
            Cur_VX := 0;
    end;
    EditObject;
end;

Procedure TJedMain.SetCurED(sc, SF, ED: Integer);
begin
    Cur_SC := sc;
    Cur_SF := SF;
    Cur_ED := ED;
    if sc >= level.sectors.Count then
        Cur_SC := 0;
    if sc < 0 then
        Cur_SC := level.sectors.Count - 1;

    with level.sectors[Cur_SC] do
    begin
        if SF < 0 then
            Cur_SF := surfaces.Count - 1;
        if SF >= surfaces.Count then
            Cur_SF := 0;
    end;

    with level.sectors[Cur_SC].surfaces[Cur_SF] do
    begin
        if vertices.Count = 0 then
            exit;
        if ED = -2 then
            Cur_ED := vertices.Count - 1
        else if ED < 0 then
            SetCurED(Cur_SC, Cur_SF - 1, -2);
        if ED >= vertices.Count then
            SetCurED(Cur_SC, Cur_SF + 1, 0);;
    end;

    EditObject;
end;

Procedure TJedMain.SetCurTH(th: Integer);
begin
    Cur_TH := th;
    if th >= level.things.Count then
        Cur_TH := 0;
    if th < 0 then
        Cur_TH := level.things.Count - 1;
    EditObject;
end;

Procedure TJedMain.SetCurFR(th, fr: Integer);
var
    thing: TJKThing;
begin
    Cur_TH := th;
    if th >= level.things.Count then
        Cur_TH := 0;
    if th < 0 then
        Cur_TH := level.things.Count - 1;
    thing := level.things[Cur_TH];

    if fr < -1 then
        Cur_FR := thing.vals.Count - 1;
    if fr >= thing.vals.Count then
        Cur_FR := -1;

    EditObject;
end;

Procedure TJedMain.SetCurLT(LT: Integer);
begin
    Cur_LT := LT;
    if LT >= level.lights.Count then
        Cur_LT := 0;
    if LT < 0 then
        Cur_LT := level.lights.Count - 1;
    EditObject;
end;

Procedure TJedMain.SetCurEX(EX: Integer);
begin
    Cur_EX := EX;
    if EX >= extraObjs.Count then
        Cur_EX := 0;
    if EX < 0 then
        Cur_EX := extraObjs.Count - 1;
    if Assigned(OnExtraSelect) then
        OnExtraSelect(extraObjs[Cur_EX]);
    EditObject;
end;

Procedure TJedMain.SaveJKLto(const name: string);
begin
    level.SaveToJKL(name);
end;

Function TJedMain.GetTargetJKLName: string;
begin
    var ext := '.jkl';
    var dir := 'jkl\';
    if CurrentProject = IJIM then
      begin
        ext := '.ndy';
        dir := 'ndy\';
      end;

    if CompareText(ExtractFileExt(LevelFile), ext) = 0 then
      begin
          Result := LevelFile;
          exit;
      end;

      Result := ProjectDir + dir + ChangeFileExt
        (ExtractFileName(LevelFile), ext);
end;

procedure TJedMain.SaveJKL1Click(sender: TObject);
begin
    DoSaveJKL;
end;

Function TJedMain.DoSaveJKL: boolean;
var
    s: string;
begin
    Result := false;
    var ext := '.jkl';
    if CurrentProject = IJIM then ext := '.ndy';

    if CompareText(ExtractFileExt(LevelFile), ext) <> 0 then
      begin
          LevelFile := ChangeFileExt(LevelFile, ext);
      end;

    Result := SysSaveProject(AskSaveAs);
//    Result := false;
//    if IsInContainer(LevelFile) then
//    begin
//        PanMessage(mt_error, 'File is in container - try "SaveJKL to"');
//        exit;
//    end;
//    if NewLevel then
//    begin
//        PanMessage(mt_error, 'Project was never saved');
//        exit;
//    end;
//
//    var ext := '.jkl';
//    if CurrentProject = IJIM then ext := '.ndy';
//    s := GetTargetJKLName;
//    ForceDirectories(ExtractFilePath(s));
//    DeleteFile(ProjectDir + ChangeFileExt(ExtractFileName(LevelFile), ext));
//    Level.SaveToJKL(s);
//    Result := true;
end;

Procedure TJedMain.ResettingEditor;
begin
    MsgForm.msgs.Clear;
end;

Procedure TJedMain.ResetCamera;
begin
  if renderer = nil then
    exit;

    with renderer do
      begin
        camX := 0;
        camY := 0;
        camZ := 0;

        rPch := 0;
        rYaw := 0;
        rRol := 0;
        scale := 4;

        SetRendFromPYR;
      end;
end;

Procedure TJedMain.ResetEditor;
var
    i: Integer;
begin
  ClearUndoBuffer;
  ClearExtraObjs;
  UrqForm.Reload;

  if DefaultParams then
  begin
    with renderer do
    begin
      SetZ(0, 0, 1);
      SetX(1, 0, 0);

      GridX := 0;
      GridY := 0;
      GridZ := 0;
      SetGridNormal(0, 0, 1);
      SetGridXNormal(1, 0, 0);

      perspective := false;
      ResetCamera;
    end;

    Cur_SC := IfThen(Level.sectors.Count = 0, -1, 0);
    Cur_SF := IfThen(Level.sectors.Count = 0, -1, 0);
    Cur_TH := IfThen(Level.things.Count  = 0, -1, 0);
    Cur_VX := IfThen(Level.sectors.Count = 0, -1, 0);
    Cur_LT := IfThen(Level.lights.Count  = 0, -1, 0);

    ToolBar.CBGridStep.Text := DoubleToStr(DefGridStep);
    ToolBar.CBGridLine.Text := DoubleToStr(DefGridLine);
    ToolBar.CBGridDot.Text  := DoubleToStr(DefGridDot);
    ToolBar.EnableSnapToGrid(DefSnapToGrid);
    ToolBar.CBGridSize.Text := DoubleToStr(DefGridSize);
  end;

  Preview3D.Reset; // call before SetMapMode
  LoadTemplates; // load before calling SetThingView to load correct templates for current ptoject

  if DefaultParams then
    begin
      SetMapMode(MM_SC);
      ToolBar.SetLVis(level.lvisString);
      SetThingView(defThingView);
      scsel.Clear;
      sfsel.Clear;
      edsel.Clear;
      vxsel.Clear;
      thsel.Clear;
      ltsel.Clear;
      SetMSelMode(DefMSelMode);
    end
  else
    begin
      ToolBar.RefreshLayers(false);
      SetMapMode(mapMode);
      VerifyMultiSelection;
    end;

  Changed := false;
  SetLevelName;
  CogForm.RefreshList;

  LoadPlugins;

  Preview3D.ReloadLevel;
  SyncRecents;

  SetProjectTypeIndicator;

  for i := 0 to level.things.Count - 1 do
    UpdateThingData(level.things[i]);

  ResetAutoSaveTimer;
  SetHideLights(false);
  SetHideThings(false);
end;

Procedure TJedMain.SetLevelName;
var
    l: Integer;
begin
    var title := Format('SED - %s*', [LevelFile]);
    if not changed then
      SetLength(title, Length(title) - 1); // remove '*' fom title
    Caption := title;

    if AskSaveAs then
        ProjectDir := ''
    else
        ProjectDir := ExtractFilePath(LevelFile);

    if CompareText(ExtractFileExt(LevelFile), '.jkl') = 0 then
      begin
          l := Length(ProjectDir);
          if CompareText(Copy(ProjectDir, l - 3, 4), 'jkl\') = 0 then
              SetLength(ProjectDir, l - 4);
      end
    else if CompareText(ExtractFileExt(LevelFile), '.ndy') = 0 then
      begin
          l := Length(ProjectDir);
          if CompareText(Copy(ProjectDir, l - 3, 4), 'ndy\') = 0 then
              SetLength(ProjectDir, l - 4);
      end;

    if ProjectDir <> '' then
        SetCurDir(ProjectDir);
end;

procedure TJedMain.PlaceCogs1Click(sender: TObject);
begin
    CogForm.Show;
end;

Procedure TJedMain.AddLightsAt(X, Y: Integer);
var
    lx, ly, lz: double;
    lt, nlt: TSedLight;
    nlts, i: Integer;
begin
    If not GetGridPosAt(X, Y, lx, ly, lz) then
        exit;

    StartUndoRec('Add Light(s)');

    if level.lights.Count = 0 then
    begin
        lt := level.NewLight;
        lt.position.x := lx;
        lt.position.y := ly;
        lt.position.z := lz;
        level.lights.Add(lt);
        LightAdded(lt);
        SaveLightUndo(lt, ch_added);
        exit;
    end;

    with level.lights[Cur_LT] do
    begin
        lx := lx - position.x;
        ly := ly - position.y;
        lz := lz - position.z;
    end;

    ltsel.AddLT(Cur_LT);

    nlts := level.lights.Count;

    for i := 0 to ltsel.Count - 1 do
    begin
        lt := level.Lights[ltsel.getLT(i)];
        nlt := level.NewLight;
        nlt.Assign(lt);
        nlt.position.x := lt.position.x + lx;
        nlt.position.y := lt.position.y + ly;
        nlt.position.z := lt.position.z + lz;
        level.lights.Add(nlt);
        LightAdded(lt);
        SaveLightUndo(nlt, ch_added);
    end;

    ltsel.Clear;
    for i := nlts to level.lights.Count - 1 do
        ltsel.AddLT(i);

    SetCurLT(nlts);
    if ltsel.Count = 1 then
        ltsel.Clear;

    Invalidate;
end;

Procedure TJedMain.AddThingsAt(X, Y: Integer);
var
    tx, ty, tz, thx, thy, thz, pch, yaw, rol: double;
    d: double;
    oldth, th: TJKThing;
    i, j, nths: Integer;
begin
    If not GetGridPosAt(X, Y, tx, ty, tz) then
        exit;

    StartUndoRec('Add Thing(s)');

    { Copy data from currently selcted thing }
    if level.things.Count = 0 then
    begin
        th := level.NewThing;
        level.things.Add(th);
        SaveThingUndo(th, ch_added);
        ThingAdded(th);
        LayerThing(0);
        SaveThingUndo(th, ch_added);
        { SaveSelThingsUndo('Add thing',ch_added); }
        exit;
    end;

    with level.things[Cur_TH] do
    begin
        tx := tx - X;
        ty := ty - Y;
        tz := tz - z;
    end;
    thsel.AddTH(Cur_TH);

    if NewOnFloor and (thsel.Count = 1) then
        with level.things[Cur_TH] do
        begin
            tx := tx - bbox.x1;
            ty := ty - bbox.y1;
            tz := tz - bbox.z1;
        end;

    nths := level.things.Count;

    for i := 0 to thsel.Count - 1 do
    begin
        oldth := level.things[thsel.GetTH(i)];
        th := level.NewThing;
        th.Assign(oldth);
        th.x := tx + oldth.x;
        th.y := ty + oldth.y;
        th.z := tz + oldth.z;

        for j := 0 to th.vals.Count - 1 do
            with th.vals[j] do
            begin
                if atype <> at_frame then
                    continue;
                GetFrame(thx, thy, thz, pch, yaw, rol);
                SetFrame(thx + tx, thy + ty, thz + tz, pch, yaw, rol);
            end;
        th.num := level.things.Count;
        level.things.Add(th);
        ThingAdded(th); // must be called before LayerThing
        LayerThing(th.num);
        SaveThingUndo(th, ch_added);
    end;

    thsel.Clear;

    for i := nths to level.things.Count - 1 do
        thsel.AddTH(i);

    SetCurTH(nths);
    if thsel.Count = 1 then
        thsel.Clear;
    { SaveSelThingsUndo('Add thing(s)',ch_added); }
    Invalidate;
end;

Procedure TJedMain.AddSectorAt;
var
    tx, ty, tz: double;
    en: TVector;
    nx, ny, nz: Tvector;
begin
    If not GetGridPosAt(X, Y, tx, ty, tz) then
        exit;

    en := renderer.gxnormal;
    en.dx := en.dx * 2;
    en.dy := en.dy * 2;
    en.dz := en.dz * 2;

    CreateCube(level, tx, ty, tz, renderer.gnormal, en);
    SectorAdded(level.sectors.Last);
    SetCurSC(level.sectors.Count - 1);

    StartUndoRec('Add Sector');
    SaveSecUndo(level.sectors.Last, ch_added, sc_both);

    Invalidate;
end;

Procedure TJedMain.CopySectorsAt(X, Y: Integer);
var
    dx, dy, dz: double;
    i, j, fnew: Integer;
    sec, newsc: TJKSector;
    surf: TJKSurface;
    nx, ny, nz: Tvector;
begin
    If not GetGridPosAt(X, Y, dx, dy, dz) then
        exit;
    sec := level.sectors[Cur_SC];

    WIth sec.vertices[0] do
    begin
        dx := dx - X;
        dy := dy - Y;
        dz := dz - z;
    end;

    scsel.AddSC(Cur_SC);
    fnew := level.sectors.Count;

    StartUndoRec('Copy Sector(s)');

    nx.SetCoords(1, 0, 0);
    ny.SetCoords(0, 1, 0);
    nz.SetCoords(0, 0, 1);
    for i := 0 to scsel.Count - 1 do
    begin
        sec := level.sectors[scsel.GetSC(i)];
        newsc := level.NewSector;
        with renderer, sec.vertices[0] do
            DuplicateSector(sec, newsc, X, Y, z, nx, ny, nz, dx, dy, dz);
        level.sectors.Add(newsc);
        SaveSecUndo(newsc, ch_added, sc_both);
    end;

    level.RenumSecs;
    scsel.Clear;

    for i := fnew to level.sectors.Count - 1 do
    begin
        sec := level.sectors[i];
        for j := 0 to sec.surfaces.Count - 1 do
        begin
            surf := sec.surfaces[j];
            if surf.nadj = 1 then
                MakeAdjoinSCUP(surf, fnew);
        end;
        SectorAdded(sec);
        scsel.AddSC(i);
    end;

    SetCurSC(fnew);
    if scsel.Count = 1 then
        scsel.Clear;
    Invalidate;
end;

procedure TJedMain.CalculateLights1Click(sender: TObject);
var
    sms, ems: longint;
    s: string;
    sec: TJKSector;
    scs: TSCMultisel;
    i: Integer;
    sc, SF, a: Integer;

    procedure AddSector(sec: TJKSector);
    begin
      if ToolBar.IsLayerVisible(sec.layer) then
      begin
        SaveSecUndo(sec, ch_changed, sc_geo);
        scs.AddSC(sec.num);
      end;
    end;

    procedure AddSectorIndex(sc: Integer);
    begin
      AddSector(level.sectors[sc]);
    end;
begin
    scs := TSCMultisel.Create;
    StartUndoRec('Calculate lighting');

    try
      if sender = CalcLightInSel then
        begin
          case mapMode of
            MM_SC:
              begin
                var sci := scsel.AddSC(Cur_SC);
                for i := 0 to scsel.Count - 1 do
                  AddSectorIndex(scsel.GetSC(i));
                scsel.DeleteN(sci);
              end;
            MM_SF:
              begin
                var sfi := sfsel.AddSF(Cur_SC, Cur_SF);
                for i := 0 to sfsel.Count - 1 do
                begin
                  sfsel.GetSCSF(i, sc, SF);
                  AddSectorIndex(sc);
                end;
                sfsel.DeleteN(sfi);
              end;
            MM_ED:
              begin
                var edi := edsel.AddED(Cur_SC, Cur_SF, Cur_ED);
                for i := 0 to edsel.Count - 1 do
                begin
                  edsel.GetSCSFED(i, sc, SF, a);
                  AddSectorIndex(sc);
                end;
                edsel.DeleteN(edi);
              end;
            MM_VX:
              begin
                var vxi := vxsel.AddVX(Cur_SC, Cur_VX);
                for i := 0 to vxsel.Count - 1 do
                begin
                  vxsel.GetSCVX(i, sc, a);
                  AddSectorIndex(sc);
                end;
                vxsel.DeleteN(vxi);
              end;
          else
            begin
              ShowMessage('You must be in sector, surface, edge or vertex mode to use this option');
              exit;
            end;
          end;
        end
      else  // select sectors which are on visible layer
        begin
          for i := 0 to level.sectors.Count - 1 do
            AddSectorIndex(i);
        end;

      //ClearUndoBuffer;
      sms := GetMSecs;

      if NewLightCalc then
        CalcLightingNew(Level, scs)
      else
        CalcLighting(Level, scs);

      ems := GetMSecs;
      sms := SubMSecs(sms, ems);
      PanMessage(mt_info, Format('%d lights processed in %s',
        [Level.Lights.Count, StrMSecs(sms)]));

      if level.sectors.Count = scs.Count then
        Preview3D.ReloadLevel
      else
        for i := 0 to scs.Count - 1 do
          SectorChanged(level.sectors[scs.GetSC(i)]);

      LevelChanged;
    finally
      scs.Free;
    end;
end;

procedure TJedMain.ConsistencyCheck1Click(sender: TObject);
begin
    Consistency.Check;
end;

procedure TJedMain.CheckResources1Click(sender: TObject);
begin
    Consistency.CheckResources;
end;

procedure TJedMain.About1Click(sender: TObject);
begin
    Jed_About.ShowModal;
end;

procedure TJedMain.BNSCClick(sender: TObject);
begin
    if sender = BNSC then
        SetMapMode(MM_SC)
    else if sender = BNSF then
        SetMapMode(MM_SF)
    else if sender = BNED then
        SetMapMode(MM_ED)
    else if sender = BNVX then
        SetMapMode(MM_VX)
    else if sender = BNTH then
        SetMapMode(MM_TH)
    else if sender = BNLT then
        SetMapMode(MM_LT)
    else if sender = BNEX then
        SetMapMode(MM_Extra)
    else if sender = BNFR then
        SetMapMode(MM_FR);
end;

Procedure TJedMain.NextObject;
var
    i, n, fi: Integer;
    th: TJKThing;
begin
    case mapMode of
        MM_SC:
            begin
                fi := Cur_SC;
                n := fi;
                repeat
                    if n >= level.sectors.Count - 1 then
                        n := 0
                    else
                        Inc(n);
                    if n = fi then
                        exit;
                until ToolBar.IsLayerVisible(level.sectors[n].Layer);

                SetCurSC(n);
            end;
        MM_SF:
            SetCurSF(Cur_SC, Cur_SF + 1);
        MM_VX:
            SetCurVX(Cur_SC, Cur_VX + 1);
        MM_ED:
            SetCurED(Cur_SC, Cur_SF, Cur_ED + 1);
        MM_TH:
            begin
                fi := Cur_TH;
                n := fi;
                repeat
                    if n >= level.things.Count - 1 then
                        n := 0
                    else
                        Inc(n);
                    if n = fi then
                        exit;
                until ToolBar.IsLayerVisible(level.things[n].Layer);
                SetCurTH(n);
            end;
        MM_FR:
            begin
                th := level.things[Cur_TH];
                Cur_FR := th.NextFrame(Cur_FR);
                SetCurFR(Cur_TH, Cur_FR);
            end;
        MM_LT:
            begin
                fi := Cur_LT;
                n := fi;
                repeat
                    if n >= level.lights.Count - 1 then
                        n := 0
                    else
                        Inc(n);
                    if n = fi then
                        exit;
                until ToolBar.IsLayerVisible(level.lights[n].Layer);
                SetCurLT(n);
            end;
        MM_Extra:
            SetCurLT(Cur_EX + 1);
    end;
    Invalidate;
end;

Procedure TJedMain.PreviousObject;
var
    i, fi, n: Integer;
    th: TJKThing;
begin
    case mapMode of
        MM_SC:
            begin
              n := GetPreviousSector(level, Cur_SC);
              if n < 0 then
                exit;
              SetCurSC(n);
            end;
        MM_SF:
            SetCurSF(Cur_SC, Cur_SF - 1);
        MM_VX:
            SetCurVX(Cur_SC, Cur_VX - 1);
        MM_ED:
            SetCurED(Cur_SC, Cur_SF, Cur_ED - 1);
        MM_TH:
            begin
                n := GetPreviousThing(level, Cur_TH);
                if n < 0 then
                  exit;
                SetCurTH(n);
            end;
        MM_FR:
            begin
                th := level.things[Cur_TH];
                if Cur_FR = -1 then
                    Cur_FR := th.Vals.Count - 1
                else
                    Cur_FR := th.PrevFrame(Cur_FR);
                SetCurFR(Cur_TH, Cur_FR);
            end;
        MM_LT:
            begin
                n := GetPreviousLight(level, Cur_LT);
                if n < 0 then
                  exit;
                SetCurLT(n);
            end;
        MM_Extra:
            SetCurEX(Cur_EX - 1);
    end;
    Invalidate;
end;

Procedure TJedMain.NextObjectInSurface;
var
    surf: TJKSurface;
begin
    case mapMode of
        MM_ED:
            begin
                surf := Level.Sectors[Cur_SC].surfaces[Cur_SF];
                if Cur_ED >= surf.vertices.Count - 1 then
                    SetCurED(Cur_SC, Cur_SF, 0)
                else
                    SetCurED(Cur_SC, Cur_SF, Cur_ED + 1);
            end;

    end;
end;

Procedure TJedMain.PreviousObjectInSurface;
var
    surf: TJKSurface;
begin
    case mapMode of
        MM_ED:
            begin
                surf := level.sectors[Cur_SC].surfaces[Cur_SF];
                if Cur_ED <= 0 then
                    SetCurED(Cur_SC, Cur_SF, surf.vertices.Count - 1)
                else
                    SetCurED(Cur_SC, Cur_SF, Cur_ED - 1);
            end;

    end;
end;

Function TJedMain.GetGridPosAt(atX, atY: Integer; var X, Y, z: double): boolean;
begin
    Result := false;
    Result := renderer.GetGridAt(atX, atY, X, Y, z);
    if not Result then
        exit;
    if bSnapToGrid then
        renderer.GetNearestGrid(X, Y, z, X, Y, z);
end;

Function TJedMain.GetPosAt(atX, atY: Integer; snapToGrid: Boolean; var x, y, z: double): boolean;
begin
  if snapToGrid then
    Result := renderer.GetGridAt(atX, atY, x, y, z)
  else
    Result := renderer.GetCameraAt(atX, atY, x, y, z);
end;

Function TJedMain.GetPosAt(atX, atY: Integer;  snapToGrid: Boolean; var pos: TVector): boolean;
begin
  Result := GetPosAt(atX, atY, snapToGrid, pos.x, pos.y, pos.z);
end;

Function TJedMain.GetPosAt(atX, atY: Integer; var x, y, z: double): boolean;
begin
  Result := GetPosAt(atX, atY, bSnapToGrid, x, y, z);
end;

Function TJedMain.GetPosAt(atX, atY: Integer; var pos: TVector): boolean;
begin
  Result := GetPosAt(atX, atY, bSnapToGrid, pos.x, pos.y, pos.z);
end;

Function TJedMain.GetSnappedPosAt(atX, atY: Integer; snapToVertex: boolean; snapToGrid: boolean; var x, y, z: double): boolean;
begin
  Result := GetPosAt(atX, atY, snapToGrid, x, y, z);
  if not Result then
    exit;

  if snapToVertex then
    GetNearestWorldVertexToLine(level, x, y, z, renderer.GetCameraForward, VertSnapMaxDist, x, y, z)
  else if snapToGrid then
    renderer.GetNearestGrid(x, y, z, x, y, z);
end;

Function TJedMain.GetSnappedPosAt(atPos: TPoint; snapToVertex: boolean; snapToGrid: boolean; var pos: TVector): boolean;
begin
  Result := GetSnappedPosAt(atPos.X, atPos.Y, snapToVertex, snapToGrid, pos.x, pos.y, pos.z);
end;

Function TJedMain.GetSnappedPosAt(atX, atY: Integer; snapToVertex: boolean; snapToGrid: boolean; var pos: TVector): boolean;
begin
  Result := GetSnappedPosAt(atX, atY, snapToVertex, snapToGrid, pos.x, pos.y, pos.z);
end;

Function TJedMain.GetSnappedPosAt(atX, atY: Integer; snapToVertex: boolean; var x, y, z: double): boolean;
begin
  Result := GetSnappedPosAt(atX, atY, snapToVertex, bSnapToGrid, x, y, z);
end;

Function TJedMain.GetSnappedPosAt(atPos: TPoint; snapToVertex: boolean; var pos: TVector): boolean;
begin
  Result := GetSnappedPosAt(atPos.X, atPos.Y, snapToVertex, pos.x, pos.y, pos.z);
end;

Function TJedMain.GetSnappedPosAt(atX, atY: Integer; snapToVertex: boolean; var pos: TVector): boolean;
begin
  Result := GetSnappedPosAt(atX, atY, snapToVertex, pos.x, pos.y, pos.z);
end;

Function TJedMain.GetMousePos(var X, Y: Integer): boolean;
var
    pt: TPoint;
begin
    Result := false;
    GetCursorPos(pt);
    pt := ScreenToClient(pt);
    X := pt.X;
    Y := pt.Y;
    if not PtInRect(ClientRect, pt) then
        exit;
    Result := true;
end;

Procedure TJedMain.SetMouseMode(mm: Integer);
begin
    Case mm of
        MM_Select:
            CurSor := crDefault;
        MM_Cleave:
            CurSor := crUpArrow; //crSaber;
        MM_Drag:
            CurSor := crDrag;
        MM_TranslateCam:
            CurSor := crSize;
        MM_RotateCam:
            CurSor := crDefault;
        MM_TranslateGrid:
            CurSor := crSize;
        MM_RotateGrid:
            CurSor := crDefault;
        MM_CreateSector:
            CurSor := crDefault;
        MM_RectSelect:
            CurSor := crDefault;
        MM_Ruler:
            CurSor := crDefault;
    else
        exit;
    end;
    mouseMode := mm;
    Screen.Cursor := CurSor;
end;

Procedure TJedMain.EditObject;
begin
    if not IsSelValid then
        exit;
    case mapMode of
        MM_SC:
          begin
            ItemEdit.LoadSector(Level.Sectors[Cur_SC]);
            Preview3D.Repaint; // draws selection wf in 3D preview
          end;
        MM_SF:
          begin
            ItemEdit.LoadSurface(Level.Sectors[Cur_SC].surfaces[Cur_SF]);
            Preview3D.Repaint; // draws selection wf in 3D preview
          end;
        MM_VX:
            ItemEdit.LoadVertex(Level.Sectors[Cur_SC].vertices[Cur_VX]);
        MM_TH:
          begin
            ItemEdit.LoadThing(Level.Things[Cur_TH]);
            Preview3D.Repaint; // draws selection wf in 3D preview
          end;
        MM_LT:
            ItemEdit.LoadLight(Cur_LT);
        MM_ED:
            ItemEdit.LoadEdge(Level.Sectors[Cur_SC].surfaces[Cur_SF], Cur_ED);
        MM_FR:
            ItemEdit.LoadFrame(Cur_TH, Cur_FR);
        MM_Extra:
            ItemEdit.LoadExtra(Cur_EX);
    end;
end;

Procedure TJedMain.GotoXYZ(X, Y, z: double; force: boolean);
begin
    if not force then
    begin
        renderer.BeginRectPick(0, 0, ClientWidth, Clientheight);
        force := not renderer.IsVertexInRect(X, Y, z);
    end;

    if not force then
        exit;
    renderer.CamX := -X;
    renderer.CamY := -Y;
    renderer.CamZ := -z;
    Invalidate;
end;

Procedure TJedMain.CenterViewOnObject;
var
    cx, cy, cz, a: double;
begin
    case mapMode of
        MM_SC:
            with level.sectors[Cur_SC].vertices[0] do
            begin
                cx := X;
                cy := Y;
                cz := z;
            end;
        MM_SF:
            with level.sectors[Cur_SC].surfaces[Cur_SF].vertices[0] do
            begin
                cx := X;
                cy := Y;
                cz := z;
            end;
        MM_ED:
            with level.sectors[Cur_SC].surfaces[Cur_SF].vertices[Cur_ED] do
            begin
                cx := X;
                cy := Y;
                cz := z;
            end;
        MM_VX:
            with level.sectors[Cur_SC].vertices[Cur_VX] do
            begin
                cx := X;
                cy := Y;
                cz := z;
            end;
        MM_TH:
            with level.things[Cur_TH] do
            begin
                cx := X;
                cy := Y;
                cz := z;
            end;
        MM_FR:
            if Cur_FR <> -1 then
                with level.things[Cur_TH].vals[Cur_FR] do
                begin
                    GetFrame(cx, cy, cz, a, a, a);
                end
            else
                With level.things[Cur_TH] do
                begin
                    cx := X;
                    cy := Y;
                    cz := z;
                end;
        MM_LT:
            with level.lights[Cur_LT] do
            begin
                cx := position.x;
                cy := position.y;
                cz := position.z;
            end;
    else
        exit;
    end;

    GotoXYZ(cx, cy, cz, force);
end;

Procedure TJedMain.GotoSC(sc: Integer);
begin
    SetCurSC(sc);
    SetMapMode(MM_SC);
    CenterViewOnObject(false);
end;

Procedure TJedMain.GotoSF(sc, SF: Integer);
begin
    SetCurSF(sc, SF);
    SetMapMode(MM_SF);
    CenterViewOnObject(false);
end;

Procedure TJedMain.GotoTH(th: Integer);
begin
    SetCurTH(th);
    SetMapMode(MM_TH);
    CenterViewOnObject(false);
end;

procedure TJedMain.ItemEditor1Click(sender: TObject);
begin
    ItemEdit.Show;
end;

procedure TJedMain.FormKeyUp(sender: TObject; var Key: Word;
  Shift: TShiftState);
begin
    case Key of
        Ord('G'):
            SetMouseMode(MM_Select);
        Ord('R'):
            SetMouseMode(MM_Select);
        VK_Space:
            SetMouseMode(MM_Select);
    end;
end;

procedure TJedMain.MUNextObjectClick(sender: TObject);
begin
    NextObject;
end;

procedure TJedMain.MUPrevObjectClick(sender: TObject);
begin
    PreviousObject;
end;

Procedure TJedMain.SnapGridToObject;
var
    rx, ry, rz, p, Y, r: double;
    norm: TVector;
    av, xnorm: TVector;
    v1, v2: TJKVertex;
    sc, VX, i: Integer;
begin
    norm  := renderer.gnormal;
    xnorm := renderer.gxnormal;
    case mapMode of
        MM_SC:
            begin
                With level.sectors[Cur_SC].vertices[0] do
                begin
                    rx := X;
                    ry := Y;
                    rz := z;
                end;
            end;
        MM_SF:
            With level.sectors[Cur_SC].surfaces[Cur_SF] do
            begin
                v1 := vertices[0];
                v2 := vertices[1];
                with v1 do
                begin
                    rx := X;
                    ry := Y;
                    rz := z;
                end;
                norm := normal;
                xnorm.dx := v2.X - v1.X;
                xnorm.dy := v2.Y - v1.Y;
                xnorm.dz := v2.z - v1.z;
            end;
        MM_ED:
            With level.sectors[Cur_SC].surfaces[Cur_SF] do
            begin
                v1 := vertices[Cur_ED];
                v2 := vertices[NextVX(Cur_ED)];
                with v1 do
                  begin
                      rx := x;
                      ry := y;
                      rz := z;
                  end;
                norm := normal;
                xnorm.dx := v2.X - v1.X;
                xnorm.dy := v2.Y - v1.Y;
                xnorm.dz := v2.z - v1.z;
            end;
        MM_VX:
            begin
                i := vxsel.AddVX(Cur_SC, Cur_VX);
                if vxsel.Count = 3 then
                begin
                    vxsel.GetSCVX(0, sc, VX);
                    v1 := level.sectors[sc].vertices[VX];

                    vxsel.GetSCVX(1, sc, VX);
                    v2 := level.sectors[sc].vertices[VX];
                    xnorm.SetCoords(v2.x - v1.x, v2.y - v1.y, v2.z - v1.z);

                    vxsel.GetSCVX(2, sc, VX);
                    v1 := level.sectors[sc].vertices[VX];
                    av.SetCoords(v1.X - v2.X, v1.Y - v2.Y, v1.z - v2.z);

                    norm := xnorm.Cross(av).Normalized;
//                    VMult(xnorm.dx, xnorm.dy, xnorm.dz, av.dx, av.dy, av.dz,
//                      norm.dx, norm.dy, norm.dz);
                end;

                vxsel.DeleteN(i);
                With level.sectors[Cur_SC].vertices[Cur_VX] do
                  begin
                      rx := x;
                      ry := y;
                      rz := z;
                  end;
            end;
        MM_TH:
            with level.things[Cur_TH] do
            begin
                rx := x;
                ry := y;
                rz := z;
            end;
        MM_FR:
            if Cur_FR = -1 then
                with level.things[Cur_TH] do
                begin
                    rx := x;
                    ry := y;
                    rz := z;
                end
            else
                with level.things[Cur_TH].vals[Cur_FR] do
                begin
                    GetFrame(rx, ry, rz, p, Y, r);
                end;
        MM_LT:
            with level.lights[Cur_LT] do
            begin
                rx := position.x;
                ry := position.y;
                rz := position.z;
            end;
    else
        exit;
    end;
    With renderer do
      begin
          gridX := rx;
          gridY := ry;
          gridZ := rz;
          With norm do
              SetGridNormal(dx, dy, dz);
          With xnorm do
              SetGridXNormal(dx, dy, dz);
          Invalidate;
      end;
end;

procedure TJedMain.MUSnapGridToClick(sender: TObject);
begin
    SnapGridToObject;
end;

Procedure TJedMain.LayerThings;
var
    n, a, i: Integer;
begin
    if mapMode <> MM_TH then
        exit;
    n := 0;
    a := thsel.AddTH(Cur_TH);
    for i := 0 to thsel.Count - 1 do
        if LayerThing(thsel.GetTH(i)) then
            Inc(n);
    thsel.DeleteN(a);
    PanMessageFmt(mt_info, '%d thing(s) layered', [n]);
end;

Function TJedMain.LayerThing(n: Integer): boolean;
var
    nsec: Integer;
    thing: TJKThing;
begin
    Result := false;
    thing := level.things[n];
    nsec := FindSectorForThing(thing);

    if nsec = -1 then
        thing.sec := nil
    else
        thing.sec := Level.Sectors[nsec];
    Result := nsec <> -1;
    ThingChanged(thing);
end;

procedure TJedMain.ToolWindow1Click(sender: TObject);
begin
    ToolForm.Show;
end;

procedure TJedMain.FormShow(sender: TObject);
begin
    CreateRenderer;

    NewProject;
    if paramstr(1) <> '' then
        OpenProject(paramstr(1), op_open);

    ResetEditor(true);
    ToolBar.SetDefaults;
end;

procedure TJedMain.Options1Click(sender: TObject);
begin
    With Options do
    begin
        SetOptions(nil);
        if IsVarChanged(WireframeAPI) or IsVarChanged(WF_DoubleBuf)  then
            CreateRenderer;
    end;

    if MapRot = MR_old then
        GetPYR;
    Invalidate;
end;

procedure TJedMain.CancelMouseMode;
begin
    case mouseMode of
        MM_Cleave:
            begin
                Invalidate;
                bCleaveStarted := false;
            end;
        MM_CreateSector:
            begin
                Invalidate;
                bCreateSCStarted := false;
            end;
        MM_RectSelect:
            begin
                Invalidate;
                bRectSelStarted := false;
            end;
        MM_Ruler:
            begin
                Invalidate;
                bRulerStarted := false;
            end;
    end;
    SetMouseMode(MM_Select);
end;

procedure TJedMain.New1Click(sender: TObject);
begin
    if Not askSave then
        exit;
    ResettingEditor;

    PMsg.Caption := '';
    SetProjectType(TProjectType.JKDF2);
    NewProject;
    ResetEditor(true);
end;

procedure TJedMain.NewMOTSProject1Click(sender: TObject);
begin
    if Not askSave then
        exit;
    ResettingEditor;
    PMsg.Caption := '';
    SetProjectType(TProjectType.MOTS);
    NewProject;
    ResetEditor(true);
end;

procedure TJedMain.NewIJIMProjectClick(sender: TObject);
begin
    if Not askSave then
        exit;
    ResettingEditor;
    PMsg.Caption := '';
    SetProjectType(TProjectType.IJIM);
    NewProject;
    ResetEditor(true);
end;


procedure TJedMain.FormDestroy(sender: TObject);
var
    i: Integer;
begin
    MWMaxed := WindowState = wsMaximized;
    if not MWMaxed then
        GetWinPos(Self, MWinPos);
    for i := 0 to Level.Things.Count - 1 do
        Free3DO(Level.Things[i].a3DO);
    renderer.free;
    renderer := nil;
end;

Procedure TJedMain.Goto_Adjoin;
var
    SF: TJKSurface;
begin
    SF := Level.Sectors[Cur_SC].surfaces[Cur_SF];
    if SF.adjoin = nil then
        exit;

    SetCurSF(SF.adjoin.sector.num, SF.adjoin.num);
    Invalidate;
end;

procedure TJedMain.GobProject1Click(sender: TObject);
var
    gobname: string;
    idir, s, ext: string;
begin
    ext := UpperCase(ExtractFileExt(LevelFile));
    { if Ext='.JED' then Idir:=ProjectDir else iDir:=ExtractFilePath(LevelFile); }
    gobname := ExtractFilePath(SaveGOB.FileName);
    if gobname = '' then
        if CurrentProject = IJIM then
          gobname := GameDir + 'Resource\'
        else
          gobname := GameDir + 'EPISODE\';

    case CurrentProject of
      JKDF2,IJIM:
        begin
            SaveGOB.FileName := gobname +
              ChangeFileExt(ExtractFileName(LevelFile), '.gob');
            SaveGOB.FilterIndex := 1;
            SaveGOB.DefaultExt := 'gob';
        end;
      MOTS:
        begin
            SaveGOB.FileName := gobname +
              ChangeFileExt(ExtractFileName(LevelFile), '.goo');
            SaveGOB.FilterIndex := 2;
            SaveGOB.DefaultExt := 'goo';
        end
    end;

    if not SaveGOB.Execute then
        exit;
    GobProj(SaveGOB.FileName);
end;

Function TJedMain.GobProj(const name: string): boolean;
// TODO: implement for IJIM
var
    gob: TGOB2Creator;

    slist, flist, ls: TStringList;
    i: Integer;
    f: TFile;
    njkls: Integer;
    s, ext: string;

    checklist: TStringList;
    extlist: TStringList;

    Function IfIjimAnd(expr: bool): bool;
    begin
      Result := (CurrentProject = IJIM) and expr;
    end;

    Function IfNotIjimAnd(expr: bool): bool;
    begin
      Result := (CurrentProject <> IJIM) and expr;
    end;

    Procedure InitSmartGOB;
    var
        i: Integer;
        id: TITEMSDATFile;
        md: TMODELSDATFile;

    begin
        checklist := TStringList.Create;
        extlist := TStringList.Create;
        checklist.sorted := true;
        checklist.duplicates := dupignore;

        With extlist do
        begin
            sorted := true;
            Add('.wav');
            Add('.mat');
            Add('.ai');
            Add('.3do');
            Add('.spr');
            Add('.key');
            Add('.pup');
            Add('.snd');
            Add('.cog');
        end;

        for i := 0 to slist.Count - 1 do
            if IfNotIjimAnd(CompareText(ExtractFileExt(slist[i]), '.jkl') = 0) or
               IfIjimAnd(CompareText(ExtractFileExt(slist[i]), '.ndy') = 0) then
                LoadJKLLists(ProjectDir + slist[i], checklist);

        id := TITEMSDATFile.Create;
        id.Load('items.dat');
        checklist.AddStrings(id.cogs);
        id.free;

        if CurrentProject <> IJIM then
            begin
                md := TMODELSDATFile.Create;
                md.Load('models.dat');
                checklist.AddStrings(md.snds);
                checklist.AddStrings(md.a3dos);
                md.free;
            end;
    end;

    Procedure CheckFlist(slist, flist: TStringList);
    var
        i: Integer;
        s: string;
    begin
        for i := slist.Count - 1 downto 0 do
        begin
            s := ExtractFileName(slist[i]);
            if extlist.IndexOf(ExtractFileExt(s)) = -1 then
                continue;
            if checklist.IndexOf(s) = -1 then
            begin
                slist.Delete(i);
                flist.Delete(i);
            end;
        end;
    end;

    Procedure DoneSmartGOB;
    begin
        checklist.free;
        extlist.free;
    end;

    Procedure AddFiles(const subdir, mask: string);
    var
        i, n: Integer;
    begin
        ListDirMask(ProjectDir + subdir, mask, ls);
        for i := 0 to ls.Count - 1 do
        begin
            n := flist.IndexOf(subdir + ls[i]);
            if n <> -1 then
            begin
                PanMessageFmt(mt_warning,
                  'Duplicate files in project directory: %s and %s',
                  [slist[n], subdir + ls[i]]);
                slist[n] := subdir + ls[i];
            end
            else
            begin
                flist.Add(subdir + ls[i]);
                slist.Add(subdir + ls[i]);
            end;

            if IfIjimAnd(mask = '*.ndy') or IfNotIjimAnd(mask = '*.jkl') then
              Inc(njkls);
        end;
    end;

begin
    Result := false;
    if NewLevel then
    begin
        PanMessage(mt_error, 'Project was never saved');
        exit;
    end;
    if IsInContainer(LevelFile) then
    begin
        PanMessage(mt_error, 'Level is in container - can''t gob');
        exit;
    end;

    gob := TGOB2Creator.Create(name);
    flist := TStringList.Create;
    slist := TStringList.Create;
    ls := TStringList.Create;
    njkls := 0;
    try
        ListDirMask(ProjectDir, '*.*', ls);
        for i := 0 to ls.Count - 1 do
        begin
            s := ls[i];
            ext := UpperCase(ExtractFileExt(s));
            if IfNotIjimAnd(ext = '.JK') then
                flist.Add(s)
            else if IfIjimAnd(ext = '.NDY') or IfNotIjimAnd(ext = '.JKL') then
            begin
                if CurrentProject <> IJIM then
                  flist.Add('jkl\' + s) 
                else                     
                  flist.Add('ndy\' + s);
                Inc(njkls);
            end
            else if ext = '.COG' then
                flist.Add('cog\' + s)
            else if ext = '.3DO' then
                flist.Add('3do\' + s)
            else if ext = '.MAT' then
                flist.Add('mat\' + s)
            else if ext = '.DAT' then
                flist.Add('misc\' + s)
            else if ext = '.UNI' then
              begin
                  var sfn := ExtractFileName(s);
                  if IfNotIjimAnd(CompareText(sfn, 'cogstrings.uni') = 0) or
                    (CompareText(sfn, 'sithstrings.uni') = 0) or
                     IfIjimAnd(CompareText(sfn, 'jonesstrings.uni') = 0) or
                     IfIjimAnd(CompareText(sfn, 'voicestrings.uni') = 0)  then
                      flist.Add('misc\' + s)
                  else if CurrentProject <> IJIM then
                      flist.Add('ui\' + s);
              end
            else if ext = '.KEY' then
                flist.Add('3do\key\' + s)
            else if (ext = '.AI') or (ext = '.AI0') or (ext = '.AI2') then
                flist.Add('misc\ai\' + s)
            else if IfNotIjimAnd(ext = '.CMP') then
                flist.Add('misc\cmp\' + s)
            else if ext = '.PAR' then
                flist.Add('misc\par\' + s)
            else if ext = '.PER' then
                flist.Add('misc\per\' + s)
            else if ext = '.PUP' then
                flist.Add('misc\pup\' + s)
            else if ext = '.SND' then
                flist.Add('misc\snd\' + s)
            else if ext = '.SPR' then
                flist.Add('misc\spr\' + s)
            else if ext = '.WAV' then
                flist.Add('sound\' + s)
            else if IfNotIjimAnd(ext = '.BM') then
                flist.Add('ui\bm\' + s)
            else if IfNotIjimAnd(ext = '.SFT') then
                flist.Add('ui\sft\' + s)
            else if IfNotIjimAnd(ext = '.SMK') then
                flist.Add('video\' + s)
            else if IfNotIjimAnd(ext = '.SAN') then
                flist.Add('video\' + s);
        end;

        for i := 0 to flist.Count - 1 do
            slist.Add(ExtractFileName(flist[i]));

        AddFiles('cog\', '*.cog');
        if CurrentProject = IJIM then
          AddFiles('ndy\', '*.ndy')
        else
          AddFiles('jkl\', '*.jkl');
        AddFiles('3do\', '*.3do');
        AddFiles('mat\', '*.mat');
        if CurrentProject <> IJIM then
          AddFiles('3do\mat\', '*.mat');
        AddFiles('misc\', '*.uni');
        if CurrentProject <> IJIM then
          AddFiles('ui\', '*.uni');
        AddFiles('misc\', '*.dat');
        AddFiles('3do\key\', '*.key');
        AddFiles('misc\ai\', '*.ai;*.ai2;*.ai0');
        if CurrentProject <> IJIM then
          AddFiles('misc\cmp\', '*.cmp');
        AddFiles('misc\par\', '*.par');
        AddFiles('misc\per\', '*.per');
        AddFiles('misc\pup\', '*.pup');
        AddFiles('misc\snd\', '*.snd');
        AddFiles('misc\spr\', '*.spr');
        AddFiles('sound\', '*.wav');
        if CurrentProject <> IJIM then
        begin
          AddFiles('voice\', '*.wav');
          AddFiles('voiceuu\', '*.wav');
          AddFiles('ui\bm\', '*.bm');
          AddFiles('ui\sft\', '*.sft');
        end;

        if njkls = 0 then
          begin
              var extn := 'JKL';
              if CurrentProject = IJIM then extn := 'NDY';              
              PanMessageFmt(mt_warning, 'No %s files found in project directory!', [extn]);
          end;

        if GOBSmart then
            InitSmartGOB;
        if GOBSmart then
            CheckFlist(slist, flist);

        Progress.Reset(flist.Count);

        gob.PrepareHeader(flist);
        for i := 0 to flist.Count - 1 do
        begin
            Progress.Step;
            f := OpenFileRead(ProjectDir + slist[i], 0);
            gob.AddFile(f);
            f.Fclose;
        end;

    finally
        if GOBSmart then
            DoneSmartGOB;
        gob.free;
        ls.free;
        flist.free;
        slist.free;
        Progress.hide;
    end;
    Result := true;
end;

procedure TJedMain.Viewtogrid1Click(sender: TObject);
begin
    SetViewToGrid;
end;

procedure TJedMain.GridtoView1Click(sender: TObject);
begin
    SetGridToView;
end;

Procedure TJedMain.SetGridToView;
var
    X, z: TVector;
begin
    X.SetCoords(1, 0, 0);
    z.SetCoords(0, -1, 0);

    X := renderer.xv;
    z := renderer.zv;

    renderer.SetGridNormal(z.dx, z.dy, z.dz);
    renderer.SetGridXNormal(X.dx, X.dy, X.dz);
    Invalidate;
end;

Procedure TJedMain.SetViewToGrid;
begin

    With renderer do
    begin
        With gnormal do
            SetZ(dx, dy, dz);
        With gxnormal do
            SetX(dx, dy, dz);

        CamX := -GridX;
        CamY := -GridY;
        CamZ := -GridZ;
        Invalidate;
    end;

    if MapRot = MR_old then
    begin
        GetPYR;
        SetRendfromPYR;
    end;

end;

procedure TJedMain.Toolbar1Click(sender: TObject);
begin
    ToolBar.Show;
end;

procedure TJedMain.Topics1Click(sender: TObject);
begin
    //Application.HelpFile := BaseDir + 'Jedhelp.hlp';
    //Application.HelpContext(10);
end;

procedure TJedMain.JedTutor1Click(sender: TObject);
begin
    //Application.HelpFile := BaseDir + 'Jedtutor.hlp';
    //Application.HelpContext(10);
end;

procedure TJedMain.N3DPreview1Click(sender: TObject);
begin
    Preview3D.ShowPreview;
end;

Procedure TJedMain.ScaleTexture(how: Integer);
var
    du, dv: double;
    surf: TJKSurface;
    i, n, sc, SF: Integer;
    u, v: TVector;

    Procedure ScaleSurf(surf: TJKSurface);
    begin
        if not ToolBar.IsLayerVisible(surf.sector.layer) then  // prevent editing of locked surface
          exit;

        SaveSecUndo(surf.sector, ch_changed, sc_geo);
        case how of
            st_up:
                begin
                    surf.uscale := surf.uscale * TxScaleStep;
                    surf.VScale := surf.VScale * TxScaleStep;
                end;
            st_down:
                begin
                    surf.uscale := surf.uscale / TxScaleStep;
                    surf.VScale := surf.VScale / TxScaleStep;
                end;
        else
            exit;
        end;
        CalcUVNormals(surf, u, v);
        ArrangeTexture(surf, 0, u, v);
        SectorChanged(surf.sector);
    end;

begin
    if mapMode <> MM_SF then
        SetMapMode(MM_SF);

    if not Preview3D.IsActive then
        exit;

    StartUndoRec('Scale Texture(s)');
    n := sfsel.AddSF(Cur_SC, Cur_SF);
    for i := 0 to sfsel.Count - 1 do
    begin
        sfsel.GetSCSF(i, sc, SF);
        ScaleSurf(level.sectors[sc].surfaces[SF]);
    end;
    sfsel.DeleteN(n);
end;

Procedure TJedMain.ShiftTexture(how: Integer);
var
    du, dv: double;
    surf: TJKSurface;
    i, sc, SF, n: Integer;

    procedure ShiftSurf(surf: TJKSurface);
    var
        i: Integer;
    begin
        if not ToolBar.IsLayerVisible(surf.sector.layer) then // prevent editing of locked sector
          exit;

        SaveSecUndo(surf.sector, ch_changed, sc_geo);
        for i := 0 to surf.txVertices.Count - 1 do
            With surf.txVertices[i] do
            begin
                u := u + du;
                v := v + dv;
            end;
        SectorChanged(surf.sector);
    end;

begin
    if mapMode <> MM_SF then
        SetMapMode(MM_SF);
    if not Preview3D.IsActive then
        exit;
    case how of
        st_left:
            begin
                du := TXStep;
                dv := 0;
            end;
        st_right:
            begin
                du := -TXStep;
                dv := 0;
            end;
        st_up:
            begin
                du := 0;
                dv := -TXStep;
            end;
        st_down:
            begin
                du := 0;
                dv := TXStep;
            end;
    else
        exit;
    end;

    StartUndoRec('Shift Texture(s)');
    n := sfsel.AddSF(Cur_SC, Cur_SF);
    for i := 0 to sfsel.Count - 1 do
    begin
        sfsel.GetSCSF(i, sc, SF);
        ShiftSurf(Level.Sectors[sc].surfaces[SF]);
    end;
    sfsel.DeleteN(n);
end;

Procedure TJedMain.RotateTexture(how: Integer);
var
    sc, sf: Integer;
    cosa, sina: double;

    procedure RotateUV(surf: TJKSurface; sina, cosa: double);
    var
        fv: Integer;
        du, dv, xx,yy,zz: double;
        nu, nv, u, v: TVector;

    begin
        if not ToolBar.IsLayerVisible(surf.sector.layer) then // Prevent editing of locked surface
          exit;

        SaveSecUndo(surf.sector, ch_changed, sc_geo);

        fv := 0;
        if (surf.sector.num = Cur_SC) and (surf.num = Cur_SF) then
          case mapMode of
            MM_ED:
              fv := Cur_ED;
            MM_SF:
              fv := 0;
          end;

        CalcUVNormals(surf, u, v);

        nv.dx := -sina * u.dx + cosa * v.dx;
        nv.dy := -sina * u.dy + cosa * v.dy;
        nv.dz := -sina * u.dz + cosa * v.dz;

        nu.dx := cosa * u.dx + sina * v.dx;
        nu.dy := cosa * u.dy + sina * v.dy;
        nu.dz := cosa * u.dz + sina * v.dz;

        UpdateSurfUVData(surf, nu, nv);
        ArrangeTexture(surf, fv, nu, nv);
//        var refsurf := level.sectors[Cur_SC].surfaces[Cur_SF];
//        with refsurf.vertices[fv], refsurf.txvertices[fv] do
//         ArrangeTextureBy(surf, nu, nv, x, y, z, u, v);

        SectorChanged(surf.sector);
    end;
begin
    if not Preview3D.IsActive then
        exit;

    if (mapMode <> MM_SF) and (mapMode <> MM_ED) then
        SetMapMode(MM_SF);

    case how of
        st_left:
            begin
                cosa := Cos(TXRotStep * PI / 180);
                sina := Sin(TXRotStep * PI / 180);
            end;
        st_right:
            begin
                cosa := Cos(-TXRotStep * PI / 180);
                sina := Sin(-TXRotStep * PI / 180);
            end;
    end;

    StartUndoRec('Rotate Texture(s)');
    var n := sfsel.AddSF(Cur_SC, Cur_SF);
    for var i := 0 to sfsel.Count - 1 do
    begin
        sfsel.GetSCSF(i, sc, sf);
        RotateUV(level.sectors[sc].surfaces[sf], sina, cosa);
    end;
    sfsel.DeleteN(n);
end;

procedure TJedMain.PMsgDblClick(sender: TObject);
begin
    PMsg.Caption := '';
end;

procedure TJedMain.Messages1Click(sender: TObject);
begin
    MsgForm.Show;
end;

Procedure TJedMain.SectorChanged(s: TJKSector);
begin
    Preview3D.UpdateSector(s);
    if ((mapMode = MM_SC) or (mapMode = MM_SF) or (mapMode = MM_ED) or (mapMode = MM_VX))
      and (s.num = Cur_SC) then
      UpdateItemEditor;

    LevelChanged;
end;

Procedure TJedMain.SectorAdded(s: TJKSector);
begin
    Preview3D.AddSector(s);
    LevelChanged;
end;

Procedure TJedMain.SectorDeleted(s: TJKSector);
begin
    Preview3D.DeleteSector(s);
    LevelChanged;
end;

Procedure TJedMain.ThingChanged(th: TJKThing);
begin
    UpdateThingData(th);
    Preview3D.UpdateThing(th);
    if ((mapMode = MM_TH) or (mapMode = MM_FR)) and (th.num = Cur_TH) then
      UpdateItemEditor;
    LevelChanged;
end;

Procedure TJedMain.ThingAdded(th: TJKThing);
begin
    UpdateThingData(th);
    Preview3D.AddThing(th);
    LevelChanged;
end;

Procedure TJedMain.ThingDeleted(th: TJKThing);
begin
    Preview3D.DeleteThing(th);
    LevelChanged;
end;

Procedure TJedMain.LightAdded(lt: TSedLight);
begin
    LevelChanged;
end;

Procedure TJedMain.LightChanged(lt: TSedLight);
begin
    if (mapMode = MM_LT) and (lt = level.lights[Cur_LT]) then
      UpdateItemEditor;
    LevelChanged;
end;
Procedure TJedMain.LightDeleted(lt: TSedLight);
begin
    JedMain.LevelChanged;
end;

Procedure TJedMain.RotateObject(angle: double; axis: Integer);
var
    i: Integer;
    vec: TVector;
    cx, cy, cz, a: double;
    sec: TJKSector;
    th: TJKThing;
    LT: TSedLight;
begin

    case axis of
        rt_x:
            vec.SetCoords(1, 0, 0);
        rt_y:
            vec.SetCoords(0, 1, 0);
        rt_z:
            vec.SetCoords(0, 0, 1);
        rt_grid:
            begin
                vec := renderer.gnormal;
                cx := renderer.GridX;
                cy := renderer.GridY;
                cz := renderer.GridZ;
            end;
    else
        exit;
    end;

    case mapMode of
        MM_SC:
            begin
                sec := level.sectors[Cur_SC];
                if axis <> rt_grid then
                    CalcSecCenter(sec, cx, cy, cz);
                StartUndoRec('Rotate Sector(s)');
                i := scsel.AddSC(Cur_SC);
                RotateSectors(Level, scsel, vec, cx, cy, cz, angle);
                scsel.DeleteN(i);
            end;
        MM_SF:
            begin
                StartUndoRec('Rotate Surface(s)');
                if axis <> rt_grid then
                    CalcSurfCenter(level.sectors[Cur_SC].surfaces[Cur_SF], cx, cy, cz);
                i := sfsel.AddSF(Cur_SC, Cur_SF);
                RotateSurfaces(Level, sfsel, vec, cx, cy, cz, angle);
                sfsel.DeleteN(i);
            end;
        MM_TH:
            begin
                th := level.things[Cur_TH];
                if axis <> rt_grid then
                begin
                    cx := th.X;
                    cy := th.Y;
                    cz := th.z;
                end;

                StartUndoRec('Rotate Thing(s)');
                i := thsel.AddTH(Cur_TH);
                RotateThings(Level, thsel, vec, cx, cy, cz, angle, True);
                thsel.DeleteN(i);
            end;
        MM_LT:
            begin
                LT := level.lights[Cur_LT];
                if axis <> rt_grid then
                begin
                    cx := LT.position.x;
                    cy := LT.position.y;
                    cz := LT.position.z;
                end;

                StartUndoRec('Rotate Light(s)');
                i := ltsel.AddLT(Cur_LT);
                RotateLights(level, ltsel, vec, cx, cy, cz, angle);
                ltsel.DeleteN(i);
            end;
        MM_FR:
            begin
                if axis <> rt_grid then
                begin
                    if Cur_FR < 0 then
                        with level.things[Cur_TH] do
                        begin
                            cx := X;
                            cy := Y;
                            cz := z;
                        end
                    else
                        level.things[Cur_TH].vals[Cur_FR].GetFrame(cx, cy,
                          cz, a, a, a);
                end;

                StartUndoRec('Rotate Frame(s)');
                i := frsel.AddFR(Cur_TH, Cur_FR);
                RotateFrames(Level, frsel, vec, cx, cy, cz, angle, True);
                frsel.DeleteN(i);
            end;
    end;
    Invalidate;
end;

Procedure TJedMain.FlipObject(how: Integer);
var
    cx, cy, cz, a: double;
    i: Integer;
    vec: TVector;
begin
    case mapMode of
        MM_SC:
            begin
                StartUndoRec('Flip Sector(s)');
                i := scsel.AddSC(Cur_SC);
                if how = rt_grid then
                    FlipSectorsOverPlane(level, scsel, renderer.gnormal,
                      renderer.GridX, renderer.GridY, renderer.GridZ)
                else
                begin
                    CalcSecCenter(level.Sectors[Cur_SC], cx, cy, cz);
                    FlipSectors(level, scsel, cx, cy, cz, how);
                end;
                scsel.DeleteN(i);
            end;
        MM_TH:
            begin
                StartUndoRec('Flip Thing(s)');
                { SaveSelThingsUndo('Change thing(s)',ch_changed); }
                i := thsel.AddTH(Cur_TH);

                if how = rt_grid then
                    FlipThingsOverPlane(level, thsel, renderer.gnormal,
                      renderer.GridX, renderer.GridY, renderer.GridZ)
                else
                    with level.things[Cur_TH] do
                        FlipThings(level, thsel, X, Y, z, how);
                thsel.DeleteN(i);
            end;
        MM_LT:
            begin
                StartUndoRec('Flip Light(s)');
                { SaveSelThingsUndo('Change thing(s)',ch_changed); }
                i := ltsel.AddLT(Cur_LT);

                with level.lights[Cur_LT] do
                begin
                    cx := position.x;
                    cy := position.y;
                    cz := position.z;
                end;

                case how of
                    rt_x:
                        vec.SetCoords(1, 0, 0);
                    rt_y:
                        vec.SetCoords(0, 1, 0);
                    rt_z:
                        vec.SetCoords(0, 0, 1);
                    rt_grid:
                        begin
                            vec := renderer.gnormal;
                            cx := renderer.gridX;
                            cy := renderer.gridY;
                            cz := renderer.gridZ;
                        end;
                end;
                FlipLightsOverPlane(level, ltsel, vec, cx, cy, cz);
                ltsel.DeleteN(i);
            end;
        MM_FR:
            begin
                StartUndoRec('Flip Frame(s)');
                { SaveSelThingsUndo('Change thing(s)',ch_changed); }
                i := frsel.AddFR(Cur_TH, Cur_FR);

                if Cur_FR < 0 then
                    with level.things[Cur_TH] do
                    begin
                        cx := X;
                        cy := Y;
                        cz := z;
                    end
                else
                    level.things[Cur_TH].vals[Cur_FR].GetFrame(cx, cy,
                      cz, a, a, a);

                case how of
                    rt_x:
                        vec.SetCoords(1, 0, 0);
                    rt_y:
                        vec.SetCoords(0, 1, 0);
                    rt_z:
                        vec.SetCoords(0, 0, 1);
                    rt_grid:
                        begin
                            vec := renderer.gnormal;
                            cx := renderer.gridX;
                            cy := renderer.gridY;
                            cz := renderer.gridZ;
                        end;
                end;
                FlipFramesOverPlane(level, frsel, vec, cx, cy, cz);
                frsel.DeleteN(i);
            end;
    end;
    Invalidate;
end;

Procedure TJedMain.ScaleObject(sfactor: double; how: Integer);
var
    sd: TScaleData;
    a: double;
    i: Integer;
begin
    sd.sfactor := sfactor;
    if (how and sc_scaleX <> 0) then
        sd.how := scale_x
    else if (how and sc_scaleY <> 0) then
        sd.how := scale_y
    else if (how and sc_scaleZ <> 0) then
        sd.how := scale_z
    else if (how and sc_scaleGrid <> 0) then
      begin
          sd.how := scale_vec;
          sd.vec := renderer.gnormal;
      end
    else
        sd.how := scale_XYZ;

    case mapMode of
        MM_SC:
            begin
                StartUndoRec('Scale Sector(s)');
                CalcSecCenter(level.sectors[Cur_SC], sd.cx, sd.cy, sd.cz);
                i := scsel.AddSC(Cur_SC);
                ScaleSectors(level, scsel, sd, how and sc_ScaleTX <> 0);
                scsel.DeleteN(i);
            end;
        MM_SF:
            begin
                StartUndoRec('Scale Surface(s)');
                CalcSurfCenter(level.sectors[Cur_SC].surfaces[Cur_SF], sd.cx, sd.cy, sd.cz);
                i := sfsel.AddSF(Cur_SC, Cur_SF);
                ScaleSurfaces(level, sfsel, sd, how and sc_ScaleTX <> 0);
                sfsel.DeleteN(i);
            end;
        MM_TH:
            begin
                StartUndoRec('Scale Thing(s)');
                i := thsel.AddTH(Cur_TH);
                with level.things[Cur_TH] do
                begin
                    sd.cx := X;
                    sd.cy := Y;
                    sd.cz := z;
                end;
                ScaleThings(level, thsel, sd);
                thsel.DeleteN(i);
            end;
        MM_LT:
            begin
                StartUndoRec('Scale Light(s)');
                i := ltsel.AddLT(Cur_LT);
                with level.lights[Cur_LT] do
                begin
                    sd.cx := position.x;
                    sd.cy := position.y;
                    sd.cz := position.z;
                end;
                ScaleLights(level, ltsel, sd);
                ltsel.DeleteN(i);
            end;
        MM_FR:
            begin
                StartUndoRec('Scale Light(s)');
                i := frsel.AddFR(Cur_TH, Cur_FR);
                if Cur_FR = -1 then
                    with level.things[Cur_TH] do
                    begin
                        sd.cx := X;
                        sd.cy := Y;
                        sd.cz := z;
                    end
                else
                    level.things[Cur_TH].vals[Cur_FR].GetFrame(sd.cx, sd.cy,
                      sd.cz, a, a, a);
                ScaleFrames(level, frsel, sd);
                frsel.DeleteN(i);
            end;
    end;
    Invalidate;
end;

Procedure TJedMain.TranslateObject(dx, dy, dz: double);
var

    i: Integer;
begin
    case mapMode of
        MM_SC:
            begin
                StartUndoRec('Translate Sector(s)');
                TranslateSectors(level, scsel, Cur_SC, dx, dy, dz);
            end;
        MM_SF:
            begin
                StartUndoRec('Translate Surface(s)');
                TranslateSurfaces(level, sfsel, Cur_SC, Cur_SF, dx, dy, dz);
            end;
    end;
    Invalidate;
end;

Procedure TJedMain.StartStitch;
begin
    if mapMode <> MM_SF then
        exit;
    stitchSec  := level.sectors[Cur_SC];
    stitchSurf := level.sectors[Cur_SC].surfaces[Cur_SF];
end;

Procedure TJedMain.DoStitch(copyProps: Boolean);
var
    ssurf, esurf: TJKSurface;
begin
    if mapMode <> MM_SF then
        exit;
    if stitchSec = nil then
    begin
        PanMessage(mt_error, 'No start surface was set for stitching');
        exit;
    end;

    if (level.sectors.IndexOf(stitchSec) < 0) or
       (stitchSec.surfaces.IndexOf(stitchSurf) < 0) then
    begin
        PanMessage(mt_error, 'The stitching start surface was deleted');
        exit;
    end;

    ssurf := stitchSurf;
    esurf := level.sectors[Cur_SC].surfaces[Cur_SF];
    if not ToolBar.IsLayerVisible(esurf.sector.layer) then
      exit;

    StartUndoRec('Stitch Surfaces');
    SaveSecUndo(esurf.sector, ch_changed, sc_val);

    if (ssurf.material <> '')  then
      esurf.material := ssurf.material;

    if copyProps or IsKeyPressed(VK_CONTROL) then
    begin
      esurf.surfflags   := ssurf.surfflags;
      esurf.faceflags   := ssurf.faceflags;
      esurf.geo         := ssurf.geo;
      esurf.light       := ssurf.light;
      esurf.tex         := ssurf.tex;
      esurf.extraLight  := ssurf.extraLight;
      esurf.adjoinflags := ssurf.adjoinflags;
    end;

    StitchSurfaces(ssurf, esurf);
    SectorChanged(esurf.sector);
    StartStitch;
end;

procedure GetPerpendicularUVNormals(const normal: TVector; var axis1, axis2: TVector);
begin

  if normal.y < 0 then
    axis1 := TVector.left
  else
    axis1 := TVector.right;

  axis1 := axis1 - normal * axis1.Dot(normal);

  if (Abs(axis1.x) < 0.001) and (Abs(axis1.y) < 0.001) and (Abs(axis1.z) < 0.001) then
  begin
    if normal.x < 0 then
      axis1 := TVector.forward
    else
      axis1 := TVector.back;

    axis1 := axis1 - normal * axis1.Dot(normal);
  end;

  axis1.Normalize;

  // Calculate axis2 as the cross product of the normal and axis1
  axis2 := normal.Cross(axis1);
  axis2.Normalize;
end;

Procedure TJedMain.StraightenTexture(zero, rot90: boolean);
var
    u, v: TVector;
    surf: TJKSurface;

    Procedure StrEd(surf: TJKSurface; ED: Integer);
    begin
        if not ToolBar.IsLayerVisible(surf.sector.layer) then
          exit;

        SaveSecUndo(surf.sector, ch_changed, sc_geo);

        if rot90 then
        begin
            CalcDefaultUVNormals(surf, ED, v, u);
            v.dx := -v.dx;
            v.dy := -v.dy;
            v.dz := -v.dz;
            UpdateSurfUVData(surf, u, v);
        end
        else
            CalcDefaultUVNormals(surf, ED, u, v);

        if zero then
            with surf.txVertices[ED] do
            begin
                u := 0;
                v := 0;
            end;
        ArrangeTexture(surf, ED, u, v);
        SectorChanged(surf.sector);
    end;

    Procedure StrSurf(surf: TJKSurface);
    begin
      if not ToolBar.IsLayerVisible(surf.sector.layer) then
        exit;

      SaveSecUndo(surf.sector, ch_changed, sc_geo);

      if rot90 then
        begin
            CalcUVNormals(surf, v, u);
            v.dx := -v.dx;
            v.dy := -v.dy;
            v.dz := -v.dz;
            UpdateSurfUVData(surf, u, v);
        end
      else
        begin
          GetPerpendicularUVNormals(surf.normal, u, v);
        end;

      if zero then
        with surf.txVertices[0] do
        begin
            u := 0;
            v := 0;
        end;

      ArrangeTexture(surf, 0, u, v);
      SectorChanged(surf.sector);
    end;

var
    i, sc, SF, ED, n: Integer;

begin
    case mapMode of
        MM_SF:
            begin
                StartUndoRec('Straighten Surface(s)');
                n := sfsel.AddSF(Cur_SC, Cur_SF);
                for i := 0 to sfsel.Count - 1 do
                begin
                    sfsel.GetSCSF(i, sc, SF);
                    StrSurf(level.sectors[sc].surfaces[SF]);
                end;
                sfsel.DeleteN(n);
            end;
        MM_ED:
            begin
                StartUndoRec('Straighten Surface(s)');
                n := edsel.addED(Cur_SC, Cur_SF, Cur_ED);
                for i := 0 to edsel.Count - 1 do
                begin
                    edsel.GetSCSFED(i, sc, SF, ED);
                    StrEd(level.sectors[sc].surfaces[SF], ED);
                end;
                edsel.DeleteN(n);
            end;
    end;
end;

Procedure TJedMain.GetCam(var X, Y, z, pch, yaw, rol: double);
begin
    X := -renderer.CamX;
    Y := -renderer.CamY;
    z := -renderer.CamZ;
    if MapRot <> MR_old then
        GetPYR;
    pch := rPch;
    yaw := rYaw;
    rol := rRol;
end;

Procedure TJedMain.SetCam(X, Y, z, pch, yaw, rol: double);
var
    zv, xv: TVector;
begin
    renderer.CamX := -X;
    renderer.CamY := -Y;
    renderer.CamZ := -z;

    { SetVec(xv,1,0,0);
      SetVec(zv,0,0,1);
      RotateVector(xv,pch,yaw,rol);
      RotateVector(zv,pch,yaw,rol);

      Renderer.SetZ(zv.dx,zv.dy,zv.dz);
      Renderer.SetX(xv.dx,xv.dy,xv.dz); }

    rPch := pch;
    rYaw := yaw;
    rRol := rol;
    SetRendFromPYR;

    Invalidate;
end;

Procedure TJedMain.RaiseObject(how: Integer);
var
    dx, dy, dz: double;
    th: TJKThing;
    vl: TTPLValue;
begin

    With renderer do
      if how = ro_up then
        begin
            dx := gnormal.dx * perpStep;
            dy := gnormal.dy * perpStep;
            dz := gnormal.dz * perpStep;
        end
      else
        begin
            dx := -gnormal.dx * perpStep;
            dy := -gnormal.dy * perpStep;
            dz := -gnormal.dz * perpStep;
        end;

    case mapMode of
        MM_SC:
            begin
                StartUndoRec('Translate Sector(s)');
                TranslateSectors(level, scsel, Cur_SC, dx, dy, dz);
            end;
        MM_SF:
            begin
                StartUndoRec('Translate Surface(s)');
                TranslateSurfaces(level, sfsel, Cur_SC, Cur_SF, dx, dy, dz);
            end;
        { TranslateSurface(Level.Sectors[Cur_SC].Surfaces[Cur_SF],
          dx,dy,dz); }
        MM_VX:
            begin
                StartUndoRec('Translate Vertices');
                TranslateVertices(level, vxsel, Cur_SC, Cur_VX, dx, dy, dz);
            end;

        MM_ED:
            begin
                StartUndoRec('Translate Edge(s)');
                TranslateEdges(level, edsel, Cur_SC, Cur_SF, Cur_ED,
                  dx, dy, dz);
            end;
        MM_TH:
            begin
                StartUndoRec('Translate Thing(s)');
                { SaveSelThingsUndo('Translate thing(s)',ch_changed); }
                TranslateThings(level, thsel, Cur_TH, dx, dy, dz, MoveFrames);
            end;
        MM_FR:
            begin
                StartUndoRec('Translate Frame(s)');
                { SaveSelFramesUndo('Translate frame(s)',ch_changed); }
                if Cur_FR = -1 then
                    TranslateThingKeepFrame(level, Cur_TH, dx, dy, dz)
                else
                    TranslateFrames(level, frsel, Cur_TH, Cur_FR, dy, dy, dz);
            end;
        MM_LT:
            begin
                { SaveSelLightsUndo('Translate thing(s)',ch_changed); }
                StartUndoRec('Translate Light(s)');
                TranslateLights(level, ltsel, Cur_LT, dx, dy, dz);
            end;
    else
        exit;
    end;
    Invalidate;
end;

Procedure TJedMain.VerifyMultiSelection;
var
    i, sc, SF, ED, VX, fr, n: Integer;
begin
    case mapMode of
        MM_SC:
            for i := scsel.Count - 1 downto 0 do
                if not CheckSectorIndex(level, scsel.GetSC(i)) then
                    scsel.DeleteN(i);
        MM_SF:
            for i := sfsel.Count - 1 downto 0 do
            begin
                sfsel.GetSCSF(i, sc, SF);
                if not CheckSurfaceIndex(level, sc, SF) then
                    sfsel.DeleteN(i);
            end;
        MM_ED:
            for i := edsel.Count - 1 downto 0 do
            begin
                edsel.GetSCSFED(i, sc, SF, ED);
                if not CheckEdgeIndex(level, sc, SF, ED) then
                    edsel.DeleteN(i);
            end;
        MM_VX:
            for i := vxsel.Count - 1 downto 0 do
            begin
                vxsel.GetSCVX(i, sc, VX);
                if not CheckVertexIndex(level, sc, VX) then
                    vxsel.DeleteN(i);
            end;
        MM_TH:
            for i := thsel.Count - 1 downto 0 do
                if not CheckThingIndex(level, thsel.GetTH(i)) then
                    thsel.DeleteN(i);
        MM_FR:
            for i := frsel.Count - 1 downto 0 do
            begin
                frsel.GetTHFR(i, n, fr);
                if not CheckFrameIndex(level, n, fr) then
                    frsel.DeleteN(i);
            end;
        MM_LT:
            for i := ltsel.Count - 1 downto 0 do
                if not CheckLightIndex(level, ltsel.getLT(i)) then
                    ltsel.DeleteN(i);
    end;
    UpdateSelCount;
end;

Procedure TJedMain.VerifySelection;
begin
    if not IsSelValid then
        SetMapMode(mapMode);
end;

Function TJedMain.IsSelValid: boolean;
begin
    case mapMode of
        MM_SC:
            Result := CheckSectorIndex(level, Cur_SC);
        MM_SF:
            Result := CheckSurfaceIndex(level, Cur_SC, Cur_SF);
        MM_ED:
            Result := CheckEdgeIndex(level, Cur_SC, Cur_SF, Cur_ED);
        MM_VX:
            Result := CheckVertexIndex(level, Cur_SC, Cur_VX);
        MM_TH:
            Result := CheckThingIndex(level, Cur_TH);
        MM_FR:
            Result := CheckFrameIndex(level, Cur_TH, Cur_FR);
        MM_LT:
            Result := CheckLightIndex(level, Cur_LT);
        MM_Extra:
            Result := (Cur_EX >= 0) and (Cur_EX < extraObjs.Count);
    else
        Result := false;
    end;
end;

procedure TJedMain.RecentClick(sender: TObject);
begin
    if Not askSave then
        exit;
    OpenProject((sender as TMenuItem).hint, op_open);
end;

Procedure TJedMain.SyncRecents;
var
    im: Integer;

    Procedure AddItem(const s: string);
    var
        mi: TMenuItem;
        dn, fn: string;
    begin
        if s = '' then
            exit;
        mi := TMenuItem.Create(FileMenu);
        if length(s) > 32 then
        begin
            dn := ExtractPath(s);
            fn := ExtractName(s);
            dn := Copy(dn, 1, 28 - length(fn));
            mi.Caption := dn + '...' + '\' + fn;
        end
        else
            mi.Caption := s;
        mi.hint := s;
        mi.OnClick := RecentClick;
        FileMenu.Add(mi);
    end;

begin
    im := RecentBar.MenuIndex;
    While FileMenu.Count > im + 1 do
        FileMenu.Delete(im + 1);
    AddItem(Recent1);
    AddItem(Recent2);
    AddItem(Recent3);
    AddItem(Recent4);
end;

Procedure TJedMain.AddRecent(const s: string);
var
    sl: TStringList;
    i: Integer;
begin
    sl := TStringList.Create;
    sl.Add(Recent1);
    sl.Add(Recent2);
    sl.Add(Recent3);
    sl.Add(Recent4);
    i := sl.IndexOf(s);

    if (i <> -1) then
        sl.Delete(i);
    sl.Insert(0, s);

    Recent1 := sl[0];
    Recent2 := sl[1];
    Recent3 := sl[2];
    Recent4 := sl[3];

    sl.free;

    SyncRecents;
end;

procedure TJedMain.SaveJKLGob1Click(sender: TObject);
begin
    if DoSaveJKL then
        GobProject1.Click;
end;

procedure TJedMain.SaveJKLGOBandTest1Click(sender: TObject);
var
    gobname, batname, ext: string;
    t: TextFile;
    pdir: array [0 .. 255] of char;
begin
    If MsgBox('You''re about to test your level. Proceed?', 'Warning', MB_YESNO)
      = idNo then
        exit;

    if DoSaveJKL then
    begin
        batname := ProjectDir + 'Test_' +
          ChangeFileExt(ExtractFileName(LevelFile), '.bat');
        if not FileExists(batname) then
        begin
            if CurrentProject = MOTS then
                ext := 'jkm.exe'
            else
                ext := 'jk.exe';

            if Pos(' ', ProjectDir) = 0 then
                StrLCopy(pdir, PChar(ProjectDir), Length(pdir) - 1)
            else
                GetShortPathName(PChar(ProjectDir), pdir, Length(pdir) - 1);

            AssignFile(t, batname);
            Rewrite(t);
            WriteLn(t, ExtractFileDrive(GameDir));
            WriteLn(t, 'cd "', GameDir, '"');
            WriteLn(t, ext,
              ' -devmode -dispstats -debug log -displayconfig -path ' + pdir);
            CloseFile(t);
        end;

        if ShellExecute(Handle, nil, PChar(batname), nil, PChar(GameDir),
          SW_SHOW) < 32 then
            PanMessage(mt_error, 'Couldn''t start JK/MOTS');

    end;
end;

procedure TJedMain.Find1Click(sender: TObject);
begin
    case mapMode of
        MM_SC:
            if FindSectors.Find then
                Invalidate;
        MM_SF:
            if FindSurfs.Find then
                Invalidate;
        MM_TH:
            if FindThings.Find then
                Invalidate;
    end;
end;

procedure TJedMain.FindNext1Click(sender: TObject);
begin
    case mapMode of
        MM_SC:
            FindSectors.FindNext(Cur_SC);
        MM_SF:
            FindSurfs.FindNext(Cur_SC, Cur_SF);
        MM_TH:
            FindThings.FindNext(Cur_TH);
    end;
end;

Procedure TJedMain.UpdateItemEditor;
begin
    if ItemEdit.Visible then
      updatestuff := true;
end;

Procedure TJedMain.LevelChanged;
begin
    Changed := true;
    SetLevelName;
    Invalidate;
end;

Function TJedMain.askSave: boolean;
begin
    Result := true;
    if not Changed then
        exit;
    Case MsgBox('The project was changed. Save?', 'SED', MB_YESNOCANCEL) of
        ID_YES:
            Result := SaveProject;
        ID_NO:
            ;
        ID_CANCEL:
            Result := false;
    end;
end;

procedure TJedMain.FormCloseQuery(sender: TObject; var CanClose: boolean);
begin
    CanClose := askSave;
end;

procedure TJedMain.Levelheadereditor1Click(sender: TObject);
begin
    if not LHEdit.EditHeader then
        exit;
    Preview3D.ReloadLevel;
    LevelChanged;
end;

Procedure TJedMain.SetThingView(how: Integer);
var
    i: Integer;
begin
    thingView := how;
    case how of
        cv_Dots:
            Dots.Checked := true;
        cv_Boxes:
            Boxes.Checked := true;
        cv_Wireframes:
            begin
                Wireframes.Checked := true;
                Progress.Reset(level.things.Count);
                Progress.msg := 'Loading 3DOs...';
                for i := 0 to level.things.Count - 1 do
                begin
                    UpdateThingData(level.things[i]);
                    Progress.Step;
                end;
                Progress.hide;
            end;
    end;
    Invalidate;
end;

Procedure LoadThing3DO(th: TJKThing; force: boolean);
var
    tval: TTPLValue;
    itpl: Integer;
    tpl: TTemplate;
    a3DO: T3DO;
begin
    if not force and (th.a3DO <> nil) then
        exit;

    itpl := Templates.IndexOfName(th.name);
    if itpl = -1 then
    begin
        Free3DO(th.a3DO);
        exit;
    end;
    tval := Templates.GetTPLField(th.name, 'model3d');
    if tval = nil then
        a3DO := nil
    else
        a3DO := Load3DO(tval.AsString);
    Free3DO(th.a3DO);
    th.a3DO := a3DO;
    if a3DO <> nil then
        a3DO.GetBBOX(th.bbox);
end;

Procedure TJedMain.UpdateThingData(th: TJKThing);
var
    itpl: Integer;
    tpl: TTemplate;
begin
    case thingView of
        cv_Dots, cv_Boxes:
            begin
                itpl := Templates.IndexOfName(th.name);
                if itpl = -1 then
                    FillChar(th.bbox, sizeof(th.bbox), 0)
                else
                begin
                    { Assign BBOX }
                    th.bbox := Templates[itpl].bbox;
                end;
                if Preview3D.IsActive and P3DThings then
                    LoadThing3DO(th, true);
            end;
        cv_Wireframes:
            LoadThing3DO(th, true);
    end;
end;

procedure TJedMain.WireframesClick(sender: TObject);
begin
    if sender = Dots then
        SetThingView(cv_Dots)
    else if sender = Boxes then
        SetThingView(cv_Boxes)
    else if sender = Wireframes then
        SetThingView(cv_Wireframes);
end;

procedure TJedMain.LoadTemplates;
var
    tplname: string;
begin
    case CurrentProject of
      JKDF2: tplname := 'master.tpl';
      MOTS: tplname := 'mots.tpl';
      IJIM: tplname := 'ijim.tpl';
    end;

    if (ProjectDir = '') or (not FileExists(ProjectDir + tplname)) then
      begin
        if FileExists(BaseDir + DataDir + '\' + tplname) then
          Templates.LoadFromFile(BaseDir + DataDir + '\' + tplname)
        else
           PanMessage(mt_warning,
            Format('Cannot load %s. File not found!',[tplname]));
      end
    else
        Templates.LoadFromFile(ProjectDir + tplname);

    templates.Merge(level.templates);
end;

Procedure TJedMain.SetProjectTypeIndicator;
begin
  GobProject1.Visible := true;
  SaveJKLGob1.Visible := false;
  //SaveJKLGob1.Caption := 'Save JKL And GOB';
  EpisodeEditor1.Visible := true;
  EpisodeEditor1.Enabled := true;
  SaveJKLGOBandTest1.Visible := true;

  SaveAs.DefaultExt := 'jkl';
  SaveAs.FileName   := '1.jkl';
  SaveAs.Filter     := 'JKL|*.jkl|JED file (*.jed)|*.jed';

  case CurrentProject of
    JKDF2:
      begin
        PNProjType.Caption := 'JK';
        PNProjType.hint :=
          'Current project is JK project (double-click to change)';
      end;
    MOTS:
      begin
        PNProjType.Caption := 'MOTS';
        PNProjType.hint :=
          'Current project is MOTS project (double-click to change)';
      end;
    else
      begin
        PNProjType.Caption := 'IJIM';
        PNProjType.hint :=
          'Current project is Infernal Machine project (double-click to change)';
        //GobProject1.Visible := false;
        //SaveJKLGob1.Visible := false;
        //SaveJKLGob1.Caption := 'Save NDY And GOB';
        EpisodeEditor1.Visible := false;
        EpisodeEditor1.Enabled := false;
        SaveJKLGOBandTest1.Visible := false;
        SaveAs.DefaultExt := 'ndy';
        SaveAs.FileName   := '1.ndy';
        SaveAs.Filter     := 'Infernal Machine|*.ndy';
      end;
  end;
end;

procedure TJedMain.PNProjTypeDblClick(sender: TObject);
  var kind: TProjectType;
begin
    if CurrentProject = IJIM then
      begin
        if MsgBox('Infernal project is not fully compatible with JKDF2 & MOTS!'#10#10'Continue anyway?', 'Warning', MB_YESNO) <> ID_YES then
          exit;
        kind := TProjectType.JKDF2;
      end
    else if CurrentProject = JKDF2 then
      kind := MOTS
    else if CurrentProject = MOTS then
      kind := IJIM;

    SetProjectType(kind);
    SetProjectTypeIndicator;
    LoadTemplates;
    EditObject;
end;

procedure TJedMain.EpisodeEditor1Click(sender: TObject);
begin
    EpisodeEdit.EditEpisode;
end;

Procedure TJedMain.MakeDoor;
var
    th: TJKThing;
var
    cg: TCOG;
    cf: TCOGFile;
    v: TCOgValue;
    tv: TTPLValue;
    i: Integer;
    nz: TVector;
    d: double;

    Procedure AddTHValue(const name, vs: string);
    begin
        tv := TTPLValue.Create;
        th.vals.Add(tv);
        tv.name := name;
        tv.vtype := GetTPLVType(tv.name);
        tv.atype := GetTPLType(tv.name);
        tv.Val(vs);
    end;

begin
    if mapMode <> MM_TH then
    begin
        ShowMessage('You must be in Thing mode');
        exit;
    end;
    th := level.things[Cur_TH];

    i := th.Vals.IndexOfName('numframes');
    if i <> -1 then
    begin
        ShowMessage('The thing appears to be a door already');
        exit;
    end;

    d := th.bbox.z2 - th.bbox.z1;
    if IsClose(d, 0) then
    begin
        ShowMessage('The thing doesn''t have correct bounding box');
        exit;
    end;

    StartUndoRec('Make Door');
    SaveThingUndo(th, ch_changed);

    for i := 0 to th.vals.Count - 1 do
        th.vals[i].Free;
    th.vals.Clear;

    AddTHValue('thingflags', '0x408');
    AddTHValue('numframes', '2');
    AddTHValue('frame', Sprintf('(%1.6f/%1.6f/%1.6f:%1.6f/%1.6f/%1.6f)',
      [th.X, th.Y, th.z, th.pch, th.yaw, th.rol]));

    nz.SetCoords(0, 0, d);
    RotateVector(nz, th.pch, th.yaw, th.rol);
    AddTHValue('frame', Sprintf('(%1.6f/%1.6f/%1.6f:%1.6f/%1.6f/%1.6f)',
      [th.X + nz.dx, th.Y + nz.dy, th.z + nz.dz, th.pch, th.yaw, th.rol]));

    cf := TCOGFile.Create;
    cg := TCOG.Create;
    cg.name := '00_door.cog';
    level.cogs.Add(cg);
    cf.LoadNoLocals(cg.name);
    for i := 0 to cf.Count - 1 do
    begin
        v := TCOgValue.Create;
        v.Assign(cf[i]);
        cg.vals.Add(v);
        if i = 0 then
            v.Val(IntToStr(th.num))
        else
            v.Val(cf[i].AsString);
        v.Resolve;
    end;
    cf.Free;
    CogForm.RefreshList;
end;

procedure TJedMain.ExportSectoras3DO1Click(sender: TObject);
var
    a3DO: T3DO;
    fname: string;
    mesh: T3DOMesh;
    cx, cy, cz, fileVersion: double;

    Function AddMat(const mat: string): Integer;
    var
        i: Integer;
    begin
        i := a3DO.Mats.IndexOf(mat);
        if i <> -1 then
            Result := i
        else
            Result := a3DO.Mats.Add(mat);
    end;

    Procedure AddSecToMesh(mesh: T3DOMesh; sec: TJKSector);
    var
        face: T3DOFace;
        n, i, j: Integer;
        v: TVertex;
        jv: TJKVertex;
        jtv: TTXVertex;
    begin
        { Add Vertices }
        for i := 0 to sec.vertices.Count - 1 do
            With sec.vertices[i] do
            begin
                { v:=TVertex.Create;
                  v.x:=x-cx;
                  v.y:=y-cy;
                  v.z:=z-cz; }
                mark := mesh.AddVertex(X - cx, Y - cy, z - cz);
            end;

        { Add Faces }
        for i := 0 to sec.surfaces.Count - 1 do
            With sec.surfaces[i] do
            begin
                if adjoin <> nil then
                    continue;

                face            := T3DOFace.Create;
                face.imat       := AddMat(Material);
                face.faceflags  := FaceFlags;
                face.geo        := geo;
                face.light      := light;
                face.tex        := tex;
                face.extraLight := ExtraLight;

                var matSize := TSizeF.Create(1,1);
                if (not Material.IsEmpty) and IsClose(fileVersion, 2.3) then
                  try
                    var matSizeI := GetMaterialSize;
                    matSize.Width  := 1 / matSizeI.Width;
                    matSize.Height := 1 / matSizeI.Height;
                  except on E:Exception do PanMessage(mt_warning,
                    Format('Cannot load %s for mesh %d face %d: %s' + sLineBreak
                      + '  Exported surface will have invalid UV coords!',
                      [Material, i, j, e.message]));
                  end;

                //for j := vertices.Count - 1 downto 0 do
                for j:=0 to vertices.Count - 1 do
                begin
                    jv := vertices[j];
                    jtv := TXVertices[j];
                    n := mesh.AddVertex(jv.X - cx, jv.Y - cy, jv.z - cz, True);
                    n := face.AddVertex(mesh.vertices[jv.mark]);
                    With face.TXVertices[n] do
                    begin
                        u     := jtv.u * matSize.Width;
                        v     := jtv.v * matSize.Height;
                        //color := jtv.color;
                    end;
                end;
                mesh.Faces.Add(face);
            end;
    end;

var
    i, j, n: Integer;
    csec, sec: TJKSector;
    nsel: TSCMultisel;
    hnode: THNode;
    ax, ay, az: double;

begin
    if mapMode <> MM_SC then
        exit;
    if not Save3DO.Execute then
        exit;
    fname := Save3DO.FileName;
    sec := Level.Sectors[Cur_SC];
    a3DO := T3DO.CreateNew;

    fileVersion := 2.1;
    if CurrentProject = IJIM then fileVersion := 2.3;

    cx := 0;
    cy := 0;
    cz := 0;
    if Level.h3donodes.Count = 0 then
        FindCenter(sec, cx, cy, cz);

    nsel := TSCMultisel.Create;

    For i := 0 to scsel.Count - 1 do
        nsel.AddSC(scsel.GetSC(i));

    nsel.AddSC(Cur_SC);

    While nsel.Count > 0 do
    begin
        sec := Level.Sectors[nsel.GetSC(0)];
        mesh := a3DO.NewMesh;
        a3DO.Meshes.Add(mesh);
        mesh.name := Level.GetLayerName(sec.layer);

        for i := nsel.Count - 1 downto 0 do
        begin
            csec := Level.Sectors[nsel.GetSC(i)];
            if csec.layer <> sec.layer then
                continue;
            csec.Renumber;
            AddSecToMesh(mesh, csec);
            nsel.DeleteN(i);
        end;

    end;

    // Build Hierarrchy;
    if Level.h3donodes.Count = 0 then // No hierarchy defined. Create default
    begin
        hnode := THNode.Create;
        a3DO.hnodes.Add(hnode);
        if a3DO.Meshes.Count = 1 then
        begin
            hnode.nodename := a3DO.Meshes[0].name;
            hnode.nmesh := 0;
        end
        else
        begin
            hnode.nodename := '$$DUMMY';
            for i := 0 to a3DO.Meshes.Count - 1 do
            begin
                hnode := THNode.Create;
                a3DO.hnodes.Add(hnode);
                hnode.nodename := a3DO.Meshes[i].name;
                hnode.nmesh    := i;
                hnode.parent   := 0;
            end;
        end;
    end;

    if Level.h3donodes.Count <> 0 then
    begin
        for i := 0 to Level.h3donodes.Count - 1 do
            with Level.h3donodes[i] do
            begin
                hnode := THNode.Create;
                hnode.Assign(Level.h3donodes[i]);
                a3DO.hnodes.Add(hnode);
            end;
        // set nmesh for all nodes and add nodes for undefined meshes
        for i := 0 to a3DO.Meshes.Count - 1 do
        begin
            n := 0;
            for j := 0 to a3DO.hnodes.Count - 1 do
            begin
                if CompareText(a3DO.hnodes[j].nodename, a3DO.Meshes[i].name) = 0
                then
                begin
                    Inc(n);
                    a3DO.hnodes[j].nmesh := i;
                end;
            end;
            if n = 0 then // no nodes for this mesh
            begin
                hnode := THNode.Create;
                hnode.parent := 0;
                hnode.nmesh := i;
                hnode.nodename := a3DO.Meshes[i].name;
                a3DO.hnodes.Add(hnode);
            end;
        end;

        // Check that there are no nodes pointing to non-existing meshes
        for i := 0 to a3DO.hnodes.Count - 1 do
        begin
            hnode := a3DO.hnodes[i];
            n := 0;
            for j := 0 to a3DO.Meshes.Count - 1 do
                if CompareText(hnode.nodename, a3DO.Meshes[j].name) = 0 then
                    Inc(n);
            if n = 0 then
                hnode.nmesh := -1;
        end;

    end;

    a3DO.SaveToFile(fname, fileVersion);
    a3DO.free;
    nsel.free;

end;

procedure TJedMain.ReloadTemplates1Click(sender: TObject);
var
    X, Y, z: TVector;
    p, ya, r: double;
    ap, ay, ar: double;
    nx, ny, nz: TVector;
begin
    { p:=10;
      ya:=20;
      r:=30;
      repeat
      SetVec(x,1,0,0);
      SetVec(y,0,1,0);
      SetVec(z,0,0,1);
      RotateVector(x,p,ya,r);
      RotateVector(y,p,ya,r);
      RotateVector(z,p,ya,r);
      GetJKPYR(x,y,z,ap,ay,ar);

      SetVec(nx,1,0,0);
      SetVec(ny,0,1,0);
      SetVec(nz,0,0,1);
      RotateVector(nx,ap,ay,ar);
      RotateVector(ny,ap,ay,ar);
      RotateVector(nz,ap,ay,ar);

      until p=-1; }
    LoadTemplates;
end;

Function TJedMain.ShortJKLName: string;
begin
    Result := ChangeFileExt(ExtractFileName(LevelFile), '.jkl');
end;

procedure TJedMain.MakeaBackupCopy1Click(sender: TObject);
var
    s: string;
    n: Integer;
begin
    if ProjectDir = '' then
    begin
        ShowMessage('Save the level first');
        exit;
    end;
{$I-}
    MkDir(ProjectDir + 'backup');
    if ioresult <> 0 then;
{$I+}
    n := 0;

    While n < 100 do
    begin
        s := Format('%s%s%-.2d%s', [ExtractFilePath(LevelFile) + 'backup\',
          ChangeFileExt(ExtractFileName(LevelFile), ''), n,
          ExtractFileExt(LevelFile)]);
        if not FileExists(s) then
            break;
        Inc(n);
    end;

    if n > 99 then
        PanMessage(mt_error, 'You have 100 backup copies! Remove some')
    else
    begin
        CopyFile(PChar(LevelFile), PChar(s), false);
        PanMessage(mt_info, 'Made a backup copy ' + s);
    end;
end;

Procedure TJedMain.DO_MultiSelect;
begin
    case mapMode of
        MM_SC:
            DO_SelSC(Cur_SC);
        MM_SF:
            DO_SelSF(Cur_SC, Cur_SF);
        MM_ED:
            DO_SelED(Cur_SC, Cur_SF, Cur_ED);
        MM_VX:
            DO_SelVX(Cur_SC, Cur_VX);
        MM_TH:
            DO_SelTH(Cur_TH);
        MM_FR:
            DO_SelFR(Cur_TH, Cur_FR);
        MM_LT:
            DO_SelLT(Cur_LT);
    else
        exit;
    end;
    Invalidate;
end;

Procedure TJedMain.ClearMultiSelection;
begin
    case mapMode of
        MM_SC:
            scsel.Clear;
        MM_SF:
            sfsel.Clear;
        MM_ED:
            edsel.Clear;
        MM_VX:
            vxsel.Clear;
        MM_TH:
            thsel.Clear;
        MM_FR:
            frsel.Clear;
        MM_LT:
            ltsel.Clear;
    end;

    UpdateSelCount;
    if (mapMode = MM_SC) or (mapMode = MM_SF) or (mapMode = MM_TH) then
      Preview3D.Repaint;
end;

Procedure TJedMain.CleaveBy(const norm: TVector; X, Y, z: double);
var
    i: Integer;
    nsc, nsf, ned: Integer;
begin
    case mapMode of
        MM_SC:
            begin
                StartUndoRec('Cleave Sector(s)');
                scsel.AddSC(Cur_SC);
                for i := 0 to scsel.Count - 1 do
                  begin
                    var sec := level.sectors[scsel.GetSC(i)];
                    if ToolBar.IsLayerVisible(sec.layer) then
                      CleaveSector(sec, norm, X, Y, z);
                  end;
                { updated in procedure }
                scsel.Clear;
            end;
        MM_SF:
            begin
                StartUndoRec('Cleave Surface(s)');
                sfsel.AddSF(Cur_SC, Cur_SF);
                for i := 0 to sfsel.Count - 1 do
                  begin
                      sfsel.GetSCSF(i, nsc, nsf);
                      var sec := level.sectors[nsc];
                      if ToolBar.IsLayerVisible(sec.layer) then
                        CleaveSurface(sec.surfaces[nsf], norm, X, Y, z);
                  end;
                sfsel.Clear;
            end;
        MM_ED:
            begin
                StartUndoRec('Cleave Edge(s)');
                edsel.AddED(Cur_SC, Cur_SF, Cur_ED);
                for i := 0 to edsel.Count - 1 do
                  begin
                    edsel.GetSCSFED(i, nsc, nsf, ned);
                    var sec := level.sectors[nsc];
                    if ToolBar.IsLayerVisible(sec.Layer) then
                      CleaveEdge(sec.surfaces[nsf], ned, norm, X, Y, z);
                  end;
                edsel.Clear;
            end;
    end;
    UpdateSelCount;
end;

Procedure TJedMain.AddKBItem(mi: TMenuItem; const name: string; c: char; sc: TShiftState);
var
    nmi: TMenuItem;
    ext: string;
begin
    nmi := TMenuItem.Create(mi);
    nmi.OnClick := KBCommandClick;
    case c of
        #0:
            ext := '';
        char(VK_ADD):
            ext := #9'+';
        char(VK_SUBTRACT):
            ext := #9'-';
        char(VK_MULTIPLY):
            ext := #9'*';
    else
        ext := #9 + ShortCutToText(ShortCut(Ord(c), sc));
    end;

    nmi.Caption := name + ext;
    nmi.Tag := ShortCut(Ord(c), sc);
    mi.Add(nmi);
end;

Procedure TJedMain.AddKBItem(mi: TMenuItem; const name: string; onClick: TNotifyEvent);
begin
    var nmi := TMenuItem.Create(mi);
    nmi.OnClick := onClick;
    nmi.Caption := name;
    mi.Add(nmi);
end;

Procedure TJedMain.KBCommandClick(sender: TObject);
var
    Key: Word;
    sc: TShiftState;
begin
    with (sender as TMenuItem) do
    begin
        ShortCutToKey(Tag, Key, sc);
        FormKeyDown(Self, Key, sc);
    end;
end;

Procedure TJedMain.CreateRenderer;
var
    X, z, gX, gZ: TVector;
    scale, cmx, cmy, cmz, grx, gry, grz: double;
    grdot, grline, grsize, grstep: double;
begin
    if renderer = nil then
      begin
          X.SetCoords(1, 0, 0);
          z.SetCoords(0, 0, 1);
          gX.SetCoords(1, 0, 0);
          gZ.SetCoords(0, 0, 1);
          scale  := 4;
          cmx    := 0;
          cmy    := 0;
          cmz    := 0;
          grx    := 0;
          gry    := 0;
          grz    := 0;
          grdot  := 0.2;
          grstep := 0.2;
          grline := 1;
      end
    else
      begin
          X  := renderer.xv;
          z  := renderer.zv;
          gX := renderer.gxnormal;
          gZ := renderer.gnormal;

          scale  := renderer.scale;
          cmx    := renderer.CamX;
          cmy    := renderer.CamY;
          cmz    := renderer.CamZ;

          grx    := renderer.GridX;
          gry    := renderer.GridY;
          grz    := renderer.GridZ;

          grdot  := renderer.GridDot;
          grline := renderer.GridLine;
          grstep := renderer.GridStep;

          renderer.Free;
          renderer := nil;
      end;
    try
        EnableMapGeoBackfaceCull(false);
        SetMapGeoBackfaceCulling.Visible := false;
        case wireframeAPI of
            WF_Software:
                renderer := TSFTRenderer.Create(Self.Handle);
            WF_OpenGL:
              begin
                renderer := TOGLRenderer.Create(Self.Handle);
//                SetMapGeoBackfaceCulling.Visible := False; // backface culling doesn't work at the moment
              end
        else
            renderer := TSFTRenderer.Create(Self.Handle);
        end;
    except
        On Exception do
        begin
            PanMessage(mt_warning,
              'Couldn''t create a requested wireframe renderer. Reverted to software');
            renderer := TSFTRenderer.Create(Self.Handle);
        end;
    end;

    renderer.SetZ(z.dx, z.dy, z.dz);
    renderer.SetX(X.dx, X.dy, X.dz);
    renderer.SetGridNormal(gZ.dx, gZ.dy, gZ.dz);
    renderer.SetGridXNormal(gX.dx, gX.dy, gX.dz);
    renderer.scale := scale;
    with renderer do
    begin
        CamX := cmx;
        CamY := cmy;
        CamZ := cmz;

        GridX := grx;
        GridY := gry;
        GridZ := grz;

        GridDot  := grdot;
        GridLine := grline;
        GridStep := grstep;
    end;
end;

Procedure LoadDLLPlugin(const dll: string);
    Procedure CallLoad(hmod: HMODULE );
    var
        sedpl:    TSEDPluginLoad;
        sedplstd: TSEDPluginLoadStdCall;
        jedpl:    TJEDPluginLoad;
        jedplstd: TJEDPluginLoadStdCall;
    begin
        try
            sedplstd := GetProcAddress(hmod, 'SEDPluginLoadStdCall');
            if Assigned(sedplstd) then
            begin
                if not sedplstd(GetSedCOM) then
                    PanMessageFmt(mt_warning, 'Plug-in %s returned error while executing!', [dll]);
                exit;
            end;

            sedpl := GetProcAddress(hmod, 'SEDPluginLoad');
            if Assigned(sedpl) then
            begin
                if not sedpl(GetSedCOM) then
                    PanMessageFmt(mt_warning, 'Plug-in %s returned error while executing!', [dll]);
                exit;
            end;

            {Fallback to lagacy JED plugin}
            jedplstd := GetProcAddress(hmod, 'JEDPluginLoadStdCall');
            //lfstd:=GetProcAddress(hmod,'@JEDPluginLoadStdcall$qqsp4IJED');
            if Assigned(jedplstd) then
            begin
                if not jedplstd(GetJEDCOM) then
                    PanMessageFmt(mt_warning, 'Plug-in %s returned error while executing!', [dll]);
                exit;
            end;

            jedpl := GetProcAddress(hmod, 'JEDPluginLoad');
            if not Assigned(jedpl) then
              begin
                  PanMessageFmt(mt_warning, 'The DLL %s is not a SED or JED plug-in: No load function found!', [dll]);
                  exit;
              end;

            if not jedpl(GetJEDCOM) then
                PanMessageFmt(mt_warning, 'Plug-in %s returned error while executing!', [dll]);
        Except
            On E: Exception do
                PanMessageFmt(mt_warning, 'Exception raised by %s: %s', [dll, E.message]);
        end;
    end;

begin
    var hlib := GetModuleHandle(PChar(dll));
    if hlib <> 0 then
    begin
        CallLoad(hlib);
        exit;
    end;

    hlib := LoadLibrary(PChar(dll));
    if hlib = 0 then
    begin
        PanMessage(mt_error, 'Couldn''t load plugin ' + dll);
        exit;
    end;

    CallLoad(hlib);
end;

procedure TJedMain.PluginClick(sender: TObject);
var
    fn: string;
begin
    fn := (sender as TMenuItem).hint;
    if CompareText(ExtractFileExt(fn), '.dll') = 0 then
        LoadDLLPlugin(fn)
    else
    begin
        if ShellExecute(Handle, nil, PChar(fn), '/JED', nil, SW_SHOW) < 32 then
            PanMessage(mt_error, 'Couldn''t start plugin: ' +
              SysErrorMessage(GetLastError));
    end;
end;

procedure TJedMain.LoadPlugins;
var
    i: Integer;

    Procedure LoadDetails(mi: TMenuItem; const fname: string);
    var
        t: TextFile;
        s, v: string;
        pe: Integer;
    begin
        if Not FileExists(fname) then
            exit;
        try
            AssignFile(t, fname);
            Reset(t);
        except
            on Exception do
                exit;
        end;

        try
            try
                While Not eof(t) do
                begin
                    Readln(t, s);
                    pe := Pos('=', s);
                    if pe = 0 then
                        continue;
                    v := LowerCase(Copy(s, 1, pe - 1));
                    s := Copy(s, pe + 1, length(s));
                    if v = 'name' then
                        mi.Caption := s
                    else if v = 'key' then
                        mi.ShortCut := TextToShortCut(s);
                end;

            finally
                CloseFile(t);
            end;
        except
            on Exception do;
        end;
    end;

    Procedure LoadDir(const dir: string; mi: TMenuItem);
    var
        sr: TSearchRec;
        res: Integer;
        ni: TMenuItem;
        s: string;
    begin
        res := FindFirst(dir + '*.*', faAnyFile, sr);
        while res = 0 do
        begin
            if (sr.attr and faDirectory <> 0) and (sr.name <> '.') and
              (sr.name <> '..') then
            begin
                ni := TMenuItem.Create(mi);
                mi.Add(ni);
                ni.Caption := sr.name;
                LoadDir(dir + sr.name + '\', ni);
            end;

            s := LowerCase(ExtractFileExt(sr.name));
            if (sr.attr and (faDirectory or faVolumeID) = 0) and
              ((s = '.exe') or (s = '.js') or (s = '.vbs') or (s = '.lnk') or
              (s = '.bat') or (s = '.dll')) then
            begin

                ni := TMenuItem.Create(mi);

                s := sr.name;
                if CompareText(ExtractFileExt(s), '.lnk') = 0 then
                    s := ChangeFileExt(s, '');

                ni.Caption := s;
                ni.hint := dir + sr.name;
                ni.OnClick := PluginClick;

                s := sr.name;
                if CompareText(ExtractFileExt(s), '.lnk') = 0 then
                    s := ChangeFileExt(s, '');
                LoadDetails(ni, dir + s + '.dsc');

                mi.Add(ni);

            end;
            res := FindNext(sr);
        end;
        FindClose(sr);
    end;

begin
    for i := Plugins.Count - 1 downto 0 do
        Plugins.Delete(i);

    LoadDir(BaseDir + 'plugins\', Plugins);

end;

procedure TJedMain.DeleteFrame(Cur_TH, Cur_FR: Integer);
var
    th: TJKThing;
    av, vl: TTPLValue;
    nv, cv, i: Integer;
begin
    if Cur_FR = -1 then
        exit;

    th := level.things[Cur_TH];
    if Cur_FR >= th.vals.Count then
        exit;

    vl := th.vals[Cur_FR];
    if CompareText(vl.name, 'frame') <> 0 then
        exit;

    SaveThingUndo(th, ch_changed);

    vl.Free;
    th.vals.Delete(Cur_FR);
    ThingChanged(th);

    if Cur_FR >= th.vals.Count then
        av := th.vals[th.vals.Count - 1]
    else
        av := th.vals[Cur_FR];

    nv := th.NFrames;
    i := th.vals.IndexOfName('numframes');

    if nv = 0 then
    begin
        if i <> -1 then
        begin
            th.vals[i].Free;
            th.vals.Delete(i);
            ThingChanged(th);
        end;
        exit;
    end;

    if i = -1 then
        th.InsertValue(0, 'numframes', IntToStr(nv))
    else
        th.vals[i].Val(IntToStr(nv));

    nv := th.vals.IndexOf(av);
    Cur_FR := -1;

    i := th.NextFrame(-1);
    while i <> -1 do
    begin
        Cur_FR := i + 1;
        i := th.NextFrame(i);
    end;

    SetCurFR(Cur_TH, Cur_FR);
end;

Procedure TJedMain.AddFramesAt(X, Y: Integer);
var
    fx, fy, fz: double;
    nv, i, fr: Integer;
    th: TJKThing;
    vl: TTPLValue;
begin
    If not GetGridPosAt(X, Y, fx, fy, fz) then
        exit;

    th := level.things[Cur_TH];

    StartUndoRec('Add Frame');
    SaveThingUndo(th, ch_changed);

    if Cur_FR = -1 then
        vl := th.AddValue('frame', Sprintf('(%.6f/%.6f/%.6f:%.6f/%.6f/%.6f)',
          [fx, fy, fz, th.pch, th.yaw, th.rol]))
    else
        vl := th.InsertValue(Cur_FR + 1, 'frame',
          Sprintf('(%.6f/%.6f/%.6f:%.6f/%.6f/%.6f)', [fx, fy, fz, th.pch,
          th.yaw, th.rol]));

    nv := th.NFrames;
    i := th.vals.IndexOfName('numframes');
    if i = -1 then
        th.InsertValue(0, 'numframes', IntToStr(nv))
    else
        th.vals[i].Val(IntToStr(nv));

    ThingChanged(th);
    Cur_FR := th.vals.IndexOf(vl);
    SetCurFR(Cur_TH, Cur_FR);
end;

procedure TJedMain.Edit1Click(sender: TObject);
var
    s: string;
begin
    s := GetUndoRecName;
    miUndo.Caption := '&Undo ' + s;
    miUndo.Enabled := s <> '';

    case mapMode of
        MM_SC:
            begin
                miPaste.Enabled := CanPasteSectors;
                miCopy.Enabled := true;
            end;
        MM_TH:
            begin
                miPaste.Enabled := CanPasteThings;
                miCopy.Enabled := true;
            end;
        MM_LT:
            begin
                miPaste.Enabled := CanPasteLights;
                miCopy.Enabled := true;
            end;
    else
        begin
            miPaste.Enabled := false;
            miCopy.Enabled := false;
        end;
    end;
end;

procedure TJedMain.miCopyClick(sender: TObject);
begin
    case mapMode of
        MM_SC:
            CopySectors(Level, scsel, Cur_SC);
        MM_LT:
            CopyLights(Level, ltsel, Cur_LT);
        MM_TH:
            CopyThings(Level, thsel, Cur_TH);
    end;
end;

procedure TJedMain.miPasteClick(sender: TObject);
var
    i, j, n: Integer;
    rx, ry, rz: double;
    X, Y: Integer;
    Layer: Integer;
    sec, nsc: TJKSector;
    surf: TJKSurface;
begin
    if sender <> nil then
      begin
          rx := renderer.CamX;
          ry := renderer.CamY;
          rz := renderer.CamZ;
      end
    else
      begin
          if not GetMousePos(X, Y) then
              exit;
          if not GetGridPosAt(X, Y, rx, ry, rz) then
              exit;
      end;

    case mapMode of
        MM_SC:
            begin
                if not CanPasteSectors then
                  exit;

                sec := level.sectors[Cur_SC];
                n := PasteSectors(level, rx, ry, rz);
                if n = -1 then
                    exit;

                StartUndoRec('Paste Sector(s)');

                SetCurSC(n);
                scsel.Clear;
                for i := n to level.sectors.Count - 1 do
                begin
                    nsc := level.sectors[i];
                    nsc.Layer := sec.Layer;
                    nsc.ColorMap := sec.ColorMap;

                    SaveSecUndo(nsc, ch_added, sc_both);

                    for j := 0 to nsc.surfaces.Count - 1 do
                    begin
                        surf := nsc.surfaces[j];
                        if High(surf.mark) <> $FFFF then
                            MakeAdjoinSCUP(surf, n);
                    end;

                    scsel.AddSC(i);
                    SectorAdded(Level.sectors[i]);
                end;

                Invalidate;
            end;
        MM_LT:
            begin
                if not CanPasteLights then
                  exit;

                layer := 0;
                if level.lights.Count > 0 then
                    layer := level.lights[Cur_LT].layer;

                StartUndoRec('Paste Light(s)');

                n := PasteLights(level, rx, ry, rz);
                if n = -1 then
                    exit;

                SetCurLT(n);
                ltsel.Clear;
                for i := n to level.lights.Count - 1 do
                begin
                    SaveLightUndo(level.lights[i], ch_added);
                    level.lights[i].layer := layer;
                    ltsel.AddLT(i);
                end;
                { SaveSelLightsUndo('Paste light(s)',ch_added); }
                LevelChanged;
                Invalidate;
            end;
        MM_TH:
            begin
                if not CanPasteThings then
                  exit;

                layer := 0;
                if level.things.Count > 0 then
                    layer := level.things[Cur_TH].layer;

                StartUndoRec('Paste Thing(s)');

                n := PasteThings(level, rx, ry, rz);
                if n = -1 then
                    exit;

                SetCurTH(n);
                thsel.Clear;
                for i := n to level.things.Count - 1 do
                begin
                    level.things[i].layer := layer;
                    SaveThingUndo(level.things[i], ch_added);
                    LayerThing(i);
                    ThingAdded(level.things[i]);
                    thsel.AddTH(i);
                end;
                { SaveSelThingsUndo('paste Thing(s)',ch_added); }
                Invalidate;
            end;
    end;

    UpdateSelCount;
end;

procedure TJedMain.N3DPreviewtoItem1Click(sender: TObject);
var
    cx, cy, cz, cpch, cyaw, a: double;
begin
    cpch := 0;
    cyaw := 0;
    case mapMode of
        MM_SC:
            FindCenter(level.sectors[Cur_SC], cx, cy, cz);
        { MM_VX: with Level.Sectors[Cur_SC].Vertices[Cur_VX] do begin cx:=x; cy:=y; cz:=z; end; }
        MM_TH:
            with level.things[Cur_TH] do
            begin
                cx := X;
                cy := Y;
                cz := z;
                cpch := pch;
                cyaw := yaw;
            end;
        MM_LT:
            with level.lights[Cur_LT] do
            begin
                cx := position.x;
                cy := position.y;
                cz := position.z;
            end;
        MM_FR:
            if Cur_FR = -1 then
                with level.lights[Cur_LT] do
                begin
                    cx := position.x;
                    cy := position.y;
                    cz := position.z;
                end
            else
                level.things[Cur_TH].Vals[Cur_FR].GetFrame(cx, cy, cz,
                  cpch, cyaw, a);
    end;
    Preview3D.SetCam(cx, cy, cz, cpch, cyaw);
end;

Procedure TJedMain.SetMSelMode(mode: Integer);
var
    mname: string;
begin
    case mode of
        mm_Toggle:
            begin
                mname := 'Toggle';
                miToggle.Checked := true;
                PMsel.Caption := '*';
            end;
        mm_Add:
            begin
                mname := 'Add';
                miAdd.Checked := true;
                PMsel.Caption := '+';
            end;
        mm_Subtract:
            begin
                mname := 'Subtract';
                miSubtract.Checked := true;
                PMsel.Caption := '-';
            end;
    else
        exit;
    end;
    mselMode := mode;
    PMsel.hint := 'Current multiselction mode is ' + mname + '. Click to change';
    UpdateSelCount;
end;

procedure ResizePanelToCaption(APanel: TPanel);
var
  CaptionWidth, CaptionHeight: Integer;
begin
  with TControlCanvas.Create do
  try
    Control := APanel;
    CaptionWidth := TextWidth(APanel.Caption) + 40;
  finally
    Free;
  end;
  APanel.Width := CaptionWidth;
end;

Procedure TJedMain.UpdateSelCount;
begin
    case mselMode of
        mm_Toggle:
            begin
                PMsel.Caption := '*';
            end;
        mm_Add:
            begin
                PMsel.Caption := '+';
            end;
        mm_Subtract:
            begin
                PMsel.Caption := '-';
            end;
    end;

    var count := 1;
    case mapMode of
        MM_SC:
          begin
            count := scsel.Count;
            if scsel.FindSC(Cur_SC) = -1 then
              count := count + 1;
          end;
        MM_SF:
          begin
            count := sfsel.Count;
            if sfsel.FindSF(Cur_SC, Cur_SF) = -1 then
              count := count + 1;
          end;
        MM_ED:
          begin
            count := edsel.Count;
            if edsel.FindED(Cur_SC, Cur_SF, Cur_ED) = -1 then
              count := count + 1;
          end;
        MM_VX:
          begin
            count := vxsel.Count;
            if vxsel.FindVX(Cur_SC, Cur_VX) = -1 then
              count := count + 1;
          end;
        MM_TH:
          begin
            count := thsel.Count;
            if thsel.FindTH(Cur_TH) = -1 then
              count := count + 1;
          end;
        MM_FR:
          begin
            count := frsel.Count;
            if frsel.FindFR(Cur_TH, Cur_FR) = -1 then
              count := count + 1;
          end;
        MM_LT:
          begin
            count := ltsel.Count;
            if ltsel.FindLT(Cur_LT) = -1 then
              count := count + 1;
          end;
        MM_Extra:
          begin
            count := exsel.Count;
            if exsel.FindTH(Cur_EX) = -1 then
              count := count + 1;
          end;
    else
        exit;
    end;

    if count > 1 then
    begin
      PMsel.Caption := Format('%s (%d)', [PMsel.Caption, count]);
    end;
    ResizePanelToCaption(PMsel);
end;

procedure TJedMain.PMselClick(sender: TObject);
begin
    case mselMode of
        mm_Toggle:
            SetMSelMode(mm_Add);
        mm_Add:
            SetMSelMode(mm_Subtract);
        mm_Subtract:
            SetMSelMode(mm_Toggle);
    else
        SetMSelMode(mm_Toggle);
    end;
end;

procedure TJedMain.miToggleClick(sender: TObject);
begin
    if sender = miToggle then
        SetMSelMode(mm_Toggle);
    if sender = miAdd then
        SetMSelMode(mm_Add);
    if sender = miSubtract then
        SetMSelMode(mm_Subtract);
end;

Procedure TJedMain.DO_SelSC(sc: Integer);
var
    i: Integer;
begin
    case mselMode of
        mm_Toggle:
            begin
                i := scsel.AddSC(sc);
                scsel.DeleteN(i xor Nbit);
            end;
        mm_Add:
            scsel.AddSC(sc);
        mm_Subtract:
            begin
                i := scsel.FindSC(sc);
                if i <> -1 then
                    scsel.DeleteN(i);
            end;
    end;
    UpdateSelCount;
end;

Procedure TJedMain.DO_SelSF(sc, SF: Integer);
var
    i: Integer;
begin
    case mselMode of
        mm_Toggle:
            begin
                i := sfsel.AddSF(sc, SF);
                sfsel.DeleteN(i xor Nbit);
            end;
        mm_Add:
            sfsel.AddSF(sc, SF);
        mm_Subtract:
            begin
                i := sfsel.FindSF(sc, SF);
                if i <> -1 then
                    sfsel.DeleteN(i);
            end;
    end;
    UpdateSelCount;
end;

Procedure TJedMain.DO_SelVX(sc, VX: Integer);
var
    i: Integer;
begin
    case mselMode of
        mm_Toggle:
            begin
                i := vxsel.AddVX(sc, VX);
                vxsel.DeleteN(i xor Nbit);
            end;
        mm_Add:
            vxsel.AddVX(sc, VX);
        mm_Subtract:
            begin
                i := vxsel.FindVX(sc, VX);
                if i <> -1 then
                    vxsel.DeleteN(i);
            end;
    end;
    UpdateSelCount;
end;

Procedure TJedMain.DO_SelED(sc, SF, ED: Integer);
var
    i: Integer;
begin
    case mselMode of
        mm_Toggle:
            begin
                i := edsel.addED(sc, SF, ED);
                edsel.DeleteN(i xor Nbit);
            end;
        mm_Add:
            edsel.addED(sc, SF, ED);
        mm_Subtract:
            begin
                i := edsel.FindED(sc, SF, ED);
                if i <> -1 then
                    edsel.DeleteN(i);
            end;
    end;
    UpdateSelCount;
end;

Procedure TJedMain.DO_SelTH(th: Integer);
var
    i: Integer;
begin
    case mselMode of
        mm_Toggle:
            begin
                i := thsel.AddTH(th);
                thsel.DeleteN(i xor Nbit);
            end;
        mm_Add:
            thsel.AddTH(th);
        mm_Subtract:
            begin
                i := thsel.FindTH(th);
                if i <> -1 then
                    thsel.DeleteN(i);
            end;
    end;
    UpdateSelCount;
end;

Procedure TJedMain.DO_SelFR(th, fr: Integer);
var
    i: Integer;
begin
    if fr < 0 then
        exit;
    case mselMode of
        mm_Toggle:
            begin
                i := frsel.AddFR(th, fr);
                frsel.DeleteN(i xor Nbit);
            end;
        mm_Add:
            frsel.AddFR(th, fr);
        mm_Subtract:
            begin
                i := frsel.FindFR(th, fr);
                if i <> -1 then
                    frsel.DeleteN(i);
            end;
    end;
    UpdateSelCount;
end;

Procedure TJedMain.DO_SelLT(LT: Integer);
var
    i: Integer;
begin
    case mselMode of
        mm_Toggle:
            begin
                i := ltsel.AddLT(LT);
                ltsel.DeleteN(i xor Nbit);
            end;
        mm_Add:
            ltsel.AddLT(LT);
        mm_Subtract:
            begin
                i := ltsel.FindLT(LT);
                if i <> -1 then
                    ltsel.DeleteN(i);
            end;
    end;
    UpdateSelCount;
end;

procedure TJedMain.ExceptHandler(sender: TObject; E: Exception);
begin
    ErrForm.ReportError(E, ExceptAddr);
end;

procedure TJedMain.TemplateCreator1Click(sender: TObject);
begin
    LoadTemplates;
    TPLCreator.CreateTemplate;
end;

Procedure TJedMain.SetRendFromPYR;
var
    X, Y, z: TVector;
begin
    X.SetCoords(1, 0, 0);
    Y.SetCoords(0, 1, 0);
    z.SetCoords(0, 0, 1);

    { PCH,ROL,YAW }
    RotateVector(X, rPch, 0, 0);
    RotateVector(X, 0, rRol, 0);
    RotateVector(X, 0, 0, rYaw);
    RotateVector(Y, rPch, 0, 0);
    RotateVector(Y, 0, rRol, 0);
    RotateVector(Y, 0, 0, rYaw);
    RotateVector(z, rPch, 0, 0);
    RotateVector(z, 0, rRol, 0);
    RotateVector(z, 0, 0, rYaw);


    //var mat: TMat3x3s;
    //CreateRotMatrix(mat, rPch, rYaw, rRol);

    { So PCH - x , YAW - y, ROL - z }
    renderer.SetZ(z.dx, z.dy, z.dz);
    renderer.SetX(X.dx, X.dy, X.dz);
    renderer.SetY(Y.dx, Y.dy, Y.dz);

end;

{ Old one - for ROL, YAW , PCH order }
(* Procedure GetPY(const x,y,z:TVector;var pch,yaw:double);
  {Assumes ROL,YAW,PCH order - reversed!
  PCH - x, YAW - y, ROL - z}
  var l:double;
  begin
  l:=sqrt(sqr(z.dy)+sqr(z.dz));
  if l=0 then PCH:=0 else PCH:=ArcCos(z.dz/l)/pi*180;
  if z.dy<0 then Pch:=pch+180;

  if l=0 then Yaw:=90 else
  begin
  yaw:=Smult(0,z.dy,z.dz,z.dx,z.dy,z.dz);
  yaw:=ArcCos(yaw/l)/pi*180;
  end;

  if SMult(z.dx,z.dy,z.dz,1,0,0)<0 then yaw:=yaw+180;

  end; *)

Procedure SysGetPYR(const X, Y, z: TVector; var pch, yaw, rol: double);
{ Assumes PCH,ROL, YAW
  PCH - x, YAW - y, ROL - z }
var
    l: double;
    nx, nz: TVector;
begin

    l := sqrt(sqr(X.dx) + sqr(X.dz));

    if l = 0 then
        yaw := 0
    else
    begin
        yaw := -ArcSin(X.dz / l) / PI * 180;
    end;

    if X.dx < 0 then
        yaw := 180 - yaw;
    if yaw < 0 then
        yaw := 360 + yaw;

    nx := X;
    RotateVector(nx, 0, 0, -yaw);

    rol := ArcCos(nx.dx) / PI * 180;
    if nx.dy < 0 then
        rol := 360 - rol;

    nz := z;
    RotateVector(nz, 0, 0, -yaw);
    RotateVector(nz, 0, -rol, 0);

    pch := ArcCos(nz.dz) / PI * 180;
    if nz.dy > 0 then
        pch := 360 - pch;

end;

Procedure TJedMain.GetPYR;
begin
    With renderer do
        SysGetPYR(xv, yv, zv, rPch, rYaw, rRol);
end;


procedure TJedMain.SaveTimerTimer(sender: TObject);
var
    fname: string;
begin
    if ProjectDir = '' then
        exit;

    var ext := ExtractFileExt(LevelFile);
    if ((CompareText(ext, '.jed') <> 0) and
       (CompareText(ext, '.jkl') <> 0)  and
       (CompareText(ext, '.ndy') <> 0)) or IsInContainer(LevelFile) then
        exit;

    ForceDirectories(ProjectDir + 'autosave');
    fname := ProjectDir + 'autosave\' + ExtractFileName(LevelFile);

    SaveToFile(fname, ext, (*setSaved:=*)False);
    PanMessage(mt_info, 'Project autosaved to ' + fname);
end;

Procedure TJedMain.UseInCog;
var
    ctype: TCOG_Type;
    obj: TObject;
    icog, ival: Integer;
begin
    case mapMode of
        MM_SC:
            begin
                ctype := ct_sec;
                obj := level.sectors[Cur_SC];
            end;
        MM_SF:
            begin
                ctype := ct_srf;
                obj := level.sectors[Cur_SC].surfaces[Cur_SF];
            end;
        MM_TH:
            begin
                ctype := ct_thg;
                obj := level.things[Cur_TH];
            end;
    else
        exit;
    end;

    icog := -1;
    ival := -1;

    if ResPicker.PickCOGVal(icog, ival, ctype) then
    begin
        level.cogs[icog].vals[ival].obj := obj;
        CogForm.UpdateCOG(icog);
        PanMessageFmt(mt_info, 'Item is assigned to value %s in cog %s',
          [Level.cogs[icog].Vals[ival].name, level.cogs[icog].name]);
    end;

end;

Function TJedMain.GetCurObjForCog(ct: TCOG_Type): TObject;
begin
    Result := nil;
    case mapMode of
        MM_SC:
            begin
                case ct of
                    ct_sec:
                        Result := Level.Sectors[Cur_SC];
                else
                    ShowMessage('SED map is not in the appropriate mode');
                end;
            end;
        MM_SF:
            begin
                case ct of
                    ct_srf:
                        Result := Level.Sectors[Cur_SC].surfaces[Cur_SF];
                    ct_sec:
                        Result := Level.Sectors[Cur_SC];
                else
                    ShowMessage('SED map is not in the appropriate mode');
                end;
            end;
        MM_TH:
            begin
                case ct of
                    ct_thg:
                        Result := Level.Things[Cur_TH];
                else
                    ShowMessage('SED map is not in the appropriate mode');
                end;
            end;
    else
        ShowMessage('SED map is not in the appropriate mode');
    end;
end;

procedure TJedMain.miUndoClick(sender: TObject);
begin
    ApplyUndo;
    VerifyMultiSelection;
    UpdateItemEditor;
end;

Procedure TJedMain.SaveSelThingsUndo(const rname: string; change: Integer);
var
    i, n: Integer;
begin
    n := thsel.AddTH(Cur_TH);
    StartUndoRec(rname);
    for i := 0 to thsel.Count - 1 do
        SaveThingUndo(Level.Things[thsel.GetTH(i)], change);
    thsel.DeleteN(n);
end;

Procedure TJedMain.SaveSelLightsUndo(const rname: string; change: Integer);
var
    i, n: Integer;
begin
    n := ltsel.AddLT(Cur_LT);
    StartUndoRec(rname);
    for i := 0 to ltsel.Count - 1 do
        SaveLightUndo(Level.Lights[ltsel.getLT(i)], change);
    ltsel.DeleteN(n);
end;

Procedure TJedMain.SaveSelFramesUndo(const rname: string; change: Integer);
var
    i, n: Integer;
    th, fr: Integer;
begin
    n := frsel.AddFR(Cur_TH, Cur_FR);
    StartUndoRec(rname);
    for i := 0 to frsel.Count - 1 do
    begin
        frsel.GetTHFR(i, th, fr);
        SaveThingUndo(Level.Things[th], change);
    end;
    frsel.DeleteN(n);
end;

Procedure TJedMain.SaveSelSurfUndo(const rname: string; change, how: Integer);
var
    i, n: Integer;
    sc, SF: Integer;
begin
    n := sfsel.AddSF(Cur_SC, Cur_SF);
    StartUndoRec(rname);
    for i := 0 to sfsel.Count - 1 do
    begin
        sfsel.GetSCSF(i, sc, SF);
        SaveSecUndo(level.sectors[sc], change, how);
    end;
    sfsel.DeleteN(n);
end;

Procedure TJedMain.SaveSelEdgeUndo(const rname: string; change: Integer);
var
    i, n: Integer;
    sc, SF: Integer;
begin
    n := edsel.addED(Cur_SC, Cur_SF, Cur_ED);
    StartUndoRec(rname);
    for i := 0 to edsel.Count - 1 do
    begin
        edsel.GetSCSFED(i, sc, SF, SF);
        SaveSecUndo(level.sectors[sc], change, sc_geo);
    end;
    edsel.DeleteN(n);
end;

Procedure TJedMain.SaveSelSecUndo(const rname: string; change, how: Integer);
var
    i, n: Integer;
begin
    n := scsel.AddSC(Cur_SC);
    StartUndoRec(rname);
    for i := 0 to scsel.Count - 1 do
    begin
        SaveSecUndo(Level.Sectors[scsel.GetSC(i)], change, how);
    end;
    scsel.DeleteN(n);
end;

Procedure TJedMain.SaveSelVertUndo(const rname: string; change: Integer);
var
    i, n: Integer;
    sc, VX: Integer;
begin
    n := vxsel.AddVX(Cur_SC, Cur_VX);
    StartUndoRec(rname);
    for i := 0 to vxsel.Count - 1 do
    begin
        vxsel.GetSCVX(i, sc, VX);
        SaveSecUndo(Level.Sectors[sc], change, sc_geo);
    end;
    vxsel.DeleteN(n);
end;

procedure TJedMain.SnapViewToObjectClick(sender: TObject);
begin
    CenterViewOnObject(true);
end;

Procedure TJedMain.ConnectSFs;
var
    a: Integer;
    sc1, sc2, sf1, sf2: Integer;
begin
    if mapMode <> MM_SF then
        exit;
    a := sfsel.AddSF(Cur_SC, Cur_SF);
    if sfsel.Count = 2 then
    begin
        StartUndoRec('Connect Surfaces');
        sfsel.GetSCSF(0, sc1, sf1);
        sfsel.GetSCSF(1, sc2, sf2);
        if ConnectSurfaces(level.sectors[sc1].surfaces[sf1],
          level.sectors[sc2].surfaces[sf2]) then
            PanMessage(mt_info, 'The surfaces successfully connected');
        UpdateItemEditor;
    end;
    sfsel.DeleteN(a);
end;

Procedure TJedMain.ConnectSCs;
var
    a: Integer;
begin
    if mapMode <> MM_SC then
        exit;
    a := scsel.AddSC(Cur_SC);
    if scsel.Count = 2 then
    begin
        StartUndoRec('Connect Sectors');
        if ConnectSectors(level.sectors[scsel.GetSC(0)],
          level.sectors[scsel.GetSC(1)]) then
            PanMessage(mt_info, 'The sectors successfully connected');
        UpdateItemEditor;
    end;
    scsel.DeleteN(a);
end;

Procedure TJedMain.BringThingToSurf(th, sc, SF: Integer);
begin
    if not CheckSurfaceIndex(level, sc, sf) or not CheckThingIndex(level, th) then
      exit;

    StartUndoRec('Bring Thing to Surface');

    const surf = level.sectors[sc].surfaces[SF];
    var thing := level.things[th];

    SaveThingUndo(thing, ch_changed);
    GetThingPoseOnSurface(thing, surf, thing.x, thing.y, thing.z, thing.pch, thing.yaw, thing.rol);

    LayerThing(thing.num);
    ThingChanged(thing);
end;

Procedure TJedMain.BringLightToSurf(lt, sc, SF: Integer);
var
    surf: TJKSurface;
    light: TSedLight;
    x, y, z, l: double;
begin
    StartUndoRec('Bring Light to Surface');
    surf := level.sectors[sc].surfaces[SF];
    CalcSurfCenter(surf, X, Y, z);
    X := X + surf.normal.dx * 0.001;
    Y := Y + surf.normal.dy * 0.001;
    z := z + surf.normal.dz * 0.001;
    light := level.lights[lt];
    SaveLightUndo(light, ch_changed);
    light.position.x := x;
    light.position.y := y;
    light.position.z := z;
    LightChanged(light);
end;

Procedure TJedMain.AddThingOnCurSurf;
begin
    if not CheckSurfaceIndex(level, Cur_SC, Cur_SF) then  // just sanity check
      exit;

    StartUndoRec('Add Thing');

    { Copy data from currently selcted thing }
    var thing := level.NewThing;
    if Cur_TH < level.things.Count then
        thing.Assign(level.things[Cur_TH]);

    var n := level.things.Add(thing);
    ThingAdded(thing);
    level.RenumThings;

    const surf = level.sectors[Cur_SC].surfaces[Cur_SF];
    GetThingPoseOnSurface(thing, surf, thing.x, thing.y, thing.z, thing.pch, thing.yaw, thing.rol);

    LayerThing(n);
    SaveThingUndo(thing, ch_added);
    SetCurTH(n);
end;

Procedure TJedMain.AddThingAtXYZPYR(X, Y, z, pch, yaw, rol: double);
var
    th: TJKThing;
    n: Integer;
begin
    StartUndoRec('Add Thing');

    { Copy data from currently selcted thing }
    th := level.NewThing;
    if Cur_TH < level.things.Count then
        th.Assign(level.things[Cur_TH]);
    n := level.things.Add(th);
    ThingAdded(th);
    level.RenumThings;

    th.x := X;
    th.y := Y;
    th.z := z;
    th.pch := pch;
    th.yaw := yaw;
    th.rol := rol;

    LayerThing(n);
    SaveThingUndo(th, ch_added);
    SetCurTH(n);
end;

Procedure TJedMain.JumpToObject;
var
    s: string;
    n, SF, ED: Integer;
    surf: TJKSurface;
begin
    case mapMode of
        MM_SC:
            s := Format('%d', [Cur_SC]);
        MM_SF:
            s := Format('%d %d', [Cur_SC, Cur_SF]);
        MM_ED:
            s := Format('%d %d %d', [Cur_SC, Cur_SF, Cur_ED]);
        MM_VX:
            s := Format('%d %d', [Cur_SC, Cur_VX]);
        MM_TH:
            s := Format('%d', [Cur_TH]);
        MM_FR:
            s := Format('%d %d', [Cur_TH, Cur_FR]);
        MM_LT:
            s := Format('%d', [Cur_LT]);
    else
        exit;
    end;
    if not InputQuery('Jump to', 'Jump to:', s) then
        exit;
    case mapMode of
        MM_SC:
            begin
                ValInt(s, n);
                SetCurSC(n);
            end;
        MM_SF:
            begin
                if GetWordN(s, 2) = '' then
                begin
                    ValInt(s, n);
                    surf := Level.GetSurfaceN(n);
                    if surf = nil then
                        ShowMessage('No surface with absolute number ' +
                          IntToStr(n))
                    else
                        SetCurSF(surf.sector.num, surf.num);
                end
                else
                begin
                    SScanf(s, '%d %d', [@n, @SF]);
                    SetCurSF(n, SF);
                end;
            end;
        MM_ED:
            begin
                SScanf(s, '%d %d %d', [@n, @SF, @ED]);
                SetCurED(n, SF, ED);
            end;
        MM_VX:
            begin
                SScanf(s, '%d %d', [@n, @SF]);
                SetCurVX(n, SF);
            end;
        MM_TH:
            begin
                ValInt(s, n);
                SetCurTH(n);
            end;
        MM_FR:
            begin
                SScanf(s, '%d %d', [@n, @SF]);
                SetCurFR(n, SF);
            end;
        MM_LT:
            begin
                ValInt(s, n);
                SetCurLT(n);
            end;
    else
        exit;
    end;
end;

procedure TJedMain.JumptoObject1Click(sender: TObject);
begin
    JumpToObject;
end;

Constructor TExtraLine.Create;
begin
    v1 := TVertex.Create;
    v2 := TVertex.Create;
end;

Destructor TExtraLine.Destroy;
begin
    v1.free;
    v2.free;
end;

Procedure TJedMain.ClearExtraObjs;
var
    i, j: Integer;
begin
    for i := 0 to extraObjs.Count - 1 do
        extraObjs[i].free;
    extraObjs.Clear;
    Cur_EX := -1;
    ExtraObjsName := '';
end;

Function TJedMain.AddExtraVertex(X, Y, z: double; const name: string): Integer;
var
    v: TExtraVertex;
begin
    v := TExtraVertex.Create;
    v.X := X;
    v.Y := Y;
    v.z := z;
    v.name := name;
    Result := extraObjs.Add(v);
end;

Function TJedMain.AddExtraLine(x1, y1, z1, x2, y2, z2: double; const name: string): Integer;
var
    i: Integer;
    l: TExtraLine;
begin
    l := TExtraLine.Create;
    l.v1.X := x1;
    l.v1.Y := y1;
    l.v1.z := z1;
    l.v2.X := x2;
    l.v2.Y := y2;
    l.v2.z := z2;
    l.name := name;
    Result := extraObjs.Add(l);
end;

Function TJedMain.AddExtraLine(x, y, z: double; direction: TVector; length: double; const name: string): Integer;
begin
   Result := AddExtraLine(x, y, z, x + direction.x * length, y + direction.y * length, z + direction.z * length, name);
end;

Function TJedMain.AddExtraLine(startPos: TVector; endPos: TVector; const name: string): Integer;
begin
  Result := AddExtraLine(startPos.x, startPos.y, startPos.z, endPos.x, endPos.y, endPos.z, name);
end;

Procedure TJedMain.DeleteExtraObj(n: Integer);
begin
    extraObjs[n].free;
    extraObjs.Delete(n);
    if Cur_EX >= extraObjs.Count then
        Cur_EX := extraObjs.Count - 1;
    extraObjsName := '';
end;

Function TJedMain.TranslateExtras(Cur_EX: Integer; dx, dy, dz: double): Integer;
var
    j: Integer;
    EX: TObject;
begin
    if (Cur_EX < -1) or (extraObjs.Count = 0) then
      exit;

    EX := extraObjs[Cur_EX];
    if EX is TVertex then
        With TVertex(EX) do
        begin
            X := X + dx;
            Y := Y + dy;
            z := z + dz;
        end;
    if EX is TExtraLine then
        With TExtraLine(EX) do
        begin
            v1.X := v1.X + dx;
            v1.Y := v1.Y + dy;
            v1.z := v1.z + dz;
            v2.X := v2.X + dx;
            v2.Y := v2.Y + dy;
            v2.z := v2.z + dz;
        end;
    if EX is TPolygon then
        With TPolygon(EX) do
            for j := 0 to vertices.Count - 1 do
                with vertices[j] do
                begin
                    X := X + dx;
                    Y := Y + dy;
                    z := z + dz;
                end;
    if Assigned(OnExtraMove) then
        OnExtraMove(EX, true);
end;

Procedure TJedMain.SetHideLights(hide: boolean);
begin
    bLightsHidden := hide;
    if bLightsHidden then
        HideLights.Caption := 'Show &Lights'
    else
        HideLights.Caption := 'Hide &Lights';
    Invalidate;
end;

Procedure TJedMain.SetHideThings(hide: boolean);
begin
    bThingsHidden := hide;
    if bThingsHidden then
        HideThings.Caption := 'Show &Things'
    else
        HideThings.Caption := 'Hide &Things';
    Invalidate;
end;

procedure TJedMain.HideThingsClick(sender: TObject);
begin
    SetHideThings(not bThingsHidden);
end;

procedure TJedMain.HideLightsClick(sender: TObject);
begin
    SetHideLights(not bLightsHidden);
end;

procedure TJedMain.ExportSectorasShape1Click(sender: TObject);
var
    s: string;
begin
    s := '';
    if not InputQuery('New Shape Name', 'Name:', s) then
        exit;
    s := trim(s);
    if s = '' then
        exit;
    AddPrefab(s, Level.Sectors[Cur_SC]);
    SavePrefabs(BaseDir + DataDir + '\shapes.tpl');
    ToolBar.UpdatePrefabs;
end;

procedure TJedMain.TutorialsonMassassiNet1Click(sender: TObject);
var
    err: Integer;
begin
    err := ShellExecute(Handle, Nil, 'http://www.massassi.net/basics/', nil,
      nil, SW_SHOWNORMAL);
    if err <= 32 then
        ShowMessage('Couldn''t go to http://www.massassi.net/basics/: ' +
          SysErrorMessage(err));
end;

procedure TJedMain.CutsceneHelper1Click(sender: TObject);
begin
    KeyForm.ShowKeyDialog;
end;

procedure TJedMain.N3DOHierarchy1Click(sender: TObject);
begin
    UrqForm.Reload;
    UrqForm.Show;
end;

procedure TJedMain.ForSelectedSectorsDo(callback: TSectorCallback);
begin
    if mapMode <> MM_SC then exit;
    var curi := scsel.AddSC(Cur_SC);
    for var i := 0 to scsel.Count - 1 do
        begin
            var sec := Level.Sectors[scsel.GetSC(i)];
            callback(sec);
        end;
    scsel.DeleteN(curi);
end;

procedure TJedMain.ForSelectedSurfacesDo(callback: TSurfaceCallback);
begin
    if mapMode <> MM_SF then exit;
    var curi := sfsel.AddSF(Cur_SC, Cur_SF);
    for var i := 0 to sfsel.Count - 1 do
    begin
        var sci := -1;
        var sfi := -1;
        sfsel.GetSCSF(i, sci, sfi);
        var surf := Level.Sectors[sci].surfaces[sfi];
        callback(surf);
    end;
    sfsel.DeleteN(curi);
end;

function TJedMain.ZoomIn: boolean;
begin
  Result := false;
  if (renderer = nil) or renderer.perspective then
    exit;

  renderer.scale := renderer.scale * 1.5;
  if ZoomLocked then
    renderer.scale := Min(renderer.scale, ZoomMax);
  Result := true;
end;

function TJedMain.ZoomOut: boolean;
begin
  Result := false;
  if (renderer = nil) or renderer.perspective then
    exit;

  renderer.scale := renderer.scale / 1.5;
  if ZoomLocked then
    renderer.scale := Max(renderer.scale, ZoomMin);
  Result := true;
end;

procedure TJedMain.SetRendererGeoVertexSize;
begin
  renderer.pointSize := WF_GeoVertSize;
end;

procedure TJedMain.SetRendererObjVertexSize;
begin
  renderer.pointSize := WF_ObjVertSize;
end;

procedure TJedMain.ResetAutoSaveTimer;
begin
  SaveTimer.Enabled  := False;
  SaveTimer.Interval := SaveInterval * 60 * 1000;
  SaveTimer.Enabled  := AutoSave;
end;

end.

