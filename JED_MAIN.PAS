unit Jed_Main;

interface

uses
    Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
    ComCtrls, StdCtrls, Menus, GlobalVars, J_Level, misc_utils, ExtCtrls,
    Render, OGL_render, FileOperations, lev_utils, Geometry, Buttons, Math,
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
        CalculateLights1: TMenuItem;
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
        MIrecovery: TMenuItem;
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
        MiEdit: TMenuItem;
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

    public
        { Project vars }
        AskSaveAs: boolean;
        NewLevel: boolean;
        LevelFile: String;
        Changed: boolean;
        updatestuff: boolean;

        snaptoy: boolean;

        { }
        SnapToGrid: boolean;
        renderer: TRenderer;
        ListID: Integer;
        hdc: HDC;
        hglc: HGLRC;
        { scale:double; }
        Cur_SC, Cur_SF, Cur_VX, Cur_ED, Cur_LT, Cur_TH, Cur_FR, Cur_EX: Integer;
        Map_mode: Integer;
        MapGeoBackfaceCull: Boolean;
        Mouse_mode: Integer;
        msel_mode: Integer;
        thing_view: Integer;
        { Private declarations }
        { Select vars }
        LastSelXY: TPoint;
        csel: Integer;
        { Drag variables }
        dragged: boolean;
        DragOrg: T3DPoint;
        DragOrgPos: T3DPoint;
        { Cleave vars }
        CleaveOrgXY: TPoint;
        CleaveOrg: T3DPoint;
        CleaveStarted: boolean;
        SCCreateStarted: boolean;
        LastX, LastY: Integer;
        { Whip ruler vars}
        RulerOrgXY: TPoint;
        RulerOrg: T3DPoint;
        RulerStarted: boolean;
        { Rect select vars }
        RectOrgX, RectOrgY: Integer;
        RectLastX, RectLastY: Integer;
        RectSelStarted: boolean;

        { Grid translate vars }
        GOrgXY: TPoint;
        GOrg: T3DPoint;
        { Cam translate }
        CamOrg: TPoint;
        { Cam Rotate vars }
        OrgX, OrgY, OrgZ: TVector;
        { Stitch variables }
        stitch_Sec: TJKSector;
        stitch_Surf: TJKSurface;

        tcube: TPolygons;

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
        ExtraObjs: TObjList;
        ExtraObjsName: string;
        OnExtraSelect: TNotifyEvent;
        OnExtraMove: TOnExtraMove;
        { Hide thing/lights bools }
        ThingsHidden, LightsHidden: boolean;

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
        Procedure SetRendfromPYR;
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
        Procedure OpenProject(const fname: string; how: Integer);
        Function SaveProject: boolean;
        Function SysSaveProject(askSave: boolean): boolean;
        Procedure SaveJKLto(const name: string);
        Procedure SaveToFile(const filePath: string; ext: string; setSaved: Boolean = True);
        Procedure SnapGridToObject;
        Function GetXYZAt(atX, atY: Integer; var X, Y, z: double): boolean;
        Function GetMousePos(var X, Y: Integer): boolean;

        Procedure GetCurObjRefXYZ(var rx, ry, rz: double);
        Procedure Do_SelectAt(X, Y: Integer; fore: boolean);

        Procedure Do_StartCleave(X, Y: Integer; snaptovertex: boolean);
        Procedure Do_ProceedCleave(X, Y: Integer; snaptovertex: boolean);
        Procedure Do_EndCleave(X, Y: Integer; snaptovertex: boolean);

        Procedure Do_StartRuler(X, Y: Integer; snaptovertex: boolean);
        Procedure Do_ProceedRuler(X, Y: Integer; snaptovertex: boolean);
        Procedure Do_EndRuler(X, Y: Integer; snaptovertex: boolean);

        Procedure Do_StartDrag(X, Y: Integer);
        Procedure Do_ProceedDrag(X, Y: Integer; snaptovx: boolean;
          snaptoaxis, yaxis: boolean);
        Procedure Do_EndDrag(X, Y: Integer);

        Procedure Do_StartTranslateCam(X, Y: Integer);
        Procedure Do_TranslateCam(X, Y: Integer);

        Procedure Do_StartRotateCam(X, Y: Integer);
        Procedure Do_RotateCam(X, Y: Integer);

        Procedure Do_StartTranslateGrid(X, Y: Integer);
        Procedure Do_TranslateGrid(X, Y: Integer);

        Procedure Do_StartRotateGrid(X, Y: Integer);
        Procedure Do_RotateGrid(X, Y: Integer);

        Procedure DO_StartCreateSC(X, Y: Integer; snaptovertex: boolean);
        Procedure DO_ProceedCreateSC(X, Y: Integer; snaptovertex: boolean);
        Procedure DO_EndCreateSC(X, Y: Integer; snaptovertex: boolean);

        Procedure DO_StartRectSelect(X, Y: Integer);
        Procedure DO_ProceedRectSelect(X, Y: Integer);
        Procedure DO_EndRectSelect(X, Y: Integer);

        Procedure ResetEditor(DefaultParams: boolean);
        Procedure ResettingEditor;
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

        Procedure RotateObject(angle: double; axis: Integer);
        Procedure ScaleObject(sfactor: double; how: Integer);
        Procedure TranslateObject(dx, dy, dz: double);
        Procedure MakeDoor;
        Procedure StartStitch;
        Procedure DoStitch;
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
        Function askSave: boolean;
        Procedure SetThingView(how: Integer);
        Procedure SetMSelMode(mode: Integer);
        Procedure UpdateThingData(th: TJKThing);
        Procedure LoadTemplates;
        Procedure SetProjectTypeIndicator;
        Function ShortJKLName: string;
        Procedure DO_MultiSelect;
        Procedure ClearMultiSelection;
        Procedure CleaveBy(const norm: TVector; X, Y, z: double);
        Procedure CreateRenderer;
        Procedure FlipObject(how: Integer);
        procedure DeleteFrame(Cur_TH, Cur_FR: Integer);
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
        Procedure BringLightToSurf(LT, sc, SF: Integer);
        Procedure AddThingAtSurf;
        Procedure ClearExtraObjs;
        Function AddExtraVertex(X, Y, z: double; const name: string): Integer;
        Function AddExtraLine(x1, y1, z1, x2, y2, z2: double;
          const name: string): Integer;
        Function TranslateExtras(Cur_EX: Integer; dx, dy, dz: double): Integer;
        Procedure DeleteExtraObj(n: Integer);
        Procedure AddThingAtXYZPYR(X, Y, z, pch, yaw, rol: double);
        Procedure SetHideLights(hide: boolean);
        Procedure SetHideThings(hide: boolean);

        procedure ForSelectedSectorsDo(callback: TSectorCallback);
        procedure ForSelectedSurfacesDo(callback: TSurfaceCallback);
    end;

var
    JedMain: TJedMain;

Procedure LoadThing3DO(th: TJKThing; force: boolean);
Procedure sysGetPYR(const X, Y, z: TVector; var pch, yaw, rol: double);
Procedure LoadDLLPlugin(const dll: string);

implementation

uses Item_edit, U_CogForm, Cons_checker, FileDialogs, U_Tools,
    U_Options, Jed_about1, ProgressDialog, U_tbar, U_Preview, U_msgForm,
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
    for i := 0 to Level.Things.Count - 1 do
        Free3DO(Level.Things[i].a3DO);

    Level.Clear;
    Level.MasterCMP := '';
    Level.kind := CurrentProject;

    sec := Level.NewSector;
    Level.Sectors.Add(sec);

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

    th := Level.NewThing;
    Level.Things.Add(th);
    With th do
    begin
        X := 0;
        Y := 0;
        z := 0.1;
        name := 'walkplayer';
        sec := Level.Sectors[0];
    end;

    LT := Level.NewLight;
    Level.Lights.Add(LT);
    With LT do
    begin
        position.x := 0;
        position.y := 0;
        position.z := 0.5;
    end;

    for i := 0 to sec.surfaces.Count - 1 do
        sec.surfaces[i].RecalcAll;

    sec.Renumber;

    Level.SetDefaultHeader;

    AskSaveAs := true;
    NewLevel := true;
    LevelFile := 'Untitled';
    Level.AddMissingLayers;

end;

Procedure TJedMain.OpenProject(const fname: string; how: Integer);
var
    ext: string;
    i: Integer;
begin
    for i := 0 to Level.Things.Count - 1 do
        Free3DO(Level.Things[i].a3DO);
    ext := UpperCase(ExtractExt(fname));
    ResettingEditor;
    PMsg.Caption := '';
    if (ext = '.JKL') or (ext = '.NDY') then
    begin
        Level.LoadFromJKL(fname);
        NewLevel := false;
        LevelFile := fname;
        AskSaveAs := IsInContainer(fname);
        SetLevelName;
    end
    else if ext = '.JED' then
    begin
        Level.LoadFromJED(fname);
        NewLevel := false;
        LevelFile := fname;
        AskSaveAs := IsInContainer(fname);
        SetLevelName;
    end;
    SetProjectType(Level.kind);
    Level.JKLPostLoad;
    AddRecent(fname);
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

    Level.LVisString := ToolBar.GetLVisString;
    if CompareText(ext, '.jed') = 0 then
      begin
        Level.SaveToJed(filePath);
      end
    else if (CompareText(ext, '.jkl') = 0) or (CompareText(ext,'.ndy') = 0) then
      Level.SaveToJKL(filePath)
    else
      begin
        PanMessageFmt(mt_error, 'SaveTo: Unkown file format with extension ''%s''', [ext]);
        exit;
      end;

    if setSaved then
    begin
      AskSaveAs := false;
      NewLevel := false;
      Changed := false;
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

Procedure TJedMain.DrawThingAt(th: TThing; X, Y, z, pch, yaw, rol: single);
var
    i: Integer;
    var cpclr: TRColor;
    function highlight(clr: TRColor): TRColor;
    begin
      var hint: Integer := 152;
      Result.r := Math.Min(clr.r + hint, 255);
      Result.g := Math.Min(clr.g + hint, 255);
      Result.b := Math.Min(clr.b + hint, 255);
    end;
begin
    var clr := renderer.Front_clr;
    cpclr := renderer.Front_clr;
    case thing_view of
        cv_Boxes:
            begin
                if th.bbox.x1 <> th.bbox.x2 then
                begin
                  BoxToPolys(th.bbox, tcube);
                  renderer.DrawPolygonsAt(tcube, X, Y, z, pch, yaw, rol);
                end;
            end;
        cv_Wireframes:
            begin
                if th.a3DO <> nil then
                begin
                  cpclr := highlight(cpclr);
                  for i := 0 to th.a3DO.Meshes.Count - 1 do
                      With th.a3DO.Meshes[i] do
                          renderer.DrawPolygonsAt(Faces, X, Y, z, pch, yaw, rol);
                end;
            end;
    end;

    renderer.SetColor(CL_Front, cpclr.r, cpclr.g, cpclr.b);
    renderer.DrawVertex(X, Y, z);
    renderer.SetColor(CL_Front, clr.r, clr.g, clr.b);
end;

Procedure TJedMain.DrawSelThing(th: TThing);
var
    d: TVector;
    v1, v2: TJKVertex;
    size: double;
    i: Integer;
begin
    renderer.SetCulling(r_CULLNONE);
    DrawThing(th);
    d.dx := 0;
    d.dy := 0.2;
    d.dz := 0;
    RotateVector(d, th.pch, th.yaw, th.rol);

    v1 := TJKVertex.Create;
    v2 := TJKVertex.Create;

    v1.X := th.X;
    v1.Y := th.Y;
    v1.z := th.z;

    v2.X := v1.X + d.dx;
    v2.Y := v1.Y + d.dy;
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
    if fr = -1 then
        exit;
    thing := Level.Things[th];
    thing.Vals[fr].GetFrame(X, Y, z, pch, yaw, rol);
    renderer.DrawVertex(X, Y, z);

    renderer.SetCulling(r_CULLNONE);
    DrawThingAt(thing, X, Y, z, pch, yaw, rol);
    d.dx := 0;
    d.dy := 0.2;
    d.dz := 0;
    RotateVector(d, pch, yaw, rol);

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
    X, Y, z, pch, yaw, rol: double;
begin
    if fr.atype <> at_frame then
        exit;
    fr.GetFrame(X, Y, z, pch, yaw, rol);
    renderer.DrawVertex(X, Y, z);
end;

Procedure TJedMain.ReDraw;
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
begin
    if not visible then
        exit;
    try
        With clMapBack do
            renderer.SetColor(CL_Background, r, g, b);

        renderer.BeginScene;

        With clGrid do
            renderer.SetColor(CL_Front, r, g, b);

        ps := 1;// * Screen.PixelsPerInch / 96;
        renderer.SetPointSize(ps);
        renderer.DrawGrid;

        if Map_mode = MM_VX then
        begin
            renderer.SetPointSize(4);
            With clVertex do
                renderer.SetColor(CL_Front, r, g, b);
            for s := 0 to Level.Sectors.Count - 1 do
            begin
                sec := Level.Sectors[s];
                if ToolBar.IsLayerVisible(sec.Layer) then
                    renderer.DrawVertices(sec.vertices);
            end;
            renderer.SetPointSize(ps);
        end;

        With clMapGeoBack do
          begin
            if MapGeoBackfaceCull then
              renderer.SetColor(CL_Front, clMapGeo.r, clMapGeo.g, clMapGeo.b)
            else
              renderer.SetColor(CL_Back, r, g, b);
          end;

        renderer.SetCulling(r_CULLBACK);
        for s := 0 to Level.Sectors.Count - 1 do
        begin
            sec := Level.Sectors[s];
            if ToolBar.IsLayerVisible(sec.Layer) then
                renderer.DrawPolygons(sec.surfaces);
        end;

        if not MapGeoBackfaceCull then
          begin
          With clMapGeo do
              renderer.SetColor(CL_Front, r, g, b);
          renderer.SetCulling(r_CULLFRONT);
          for s := 0 to Level.Sectors.Count - 1 do
          begin
              sec := Level.Sectors[s];
              if ToolBar.IsLayerVisible(sec.Layer) then
                  renderer.DrawPolygons(sec.surfaces);
          end;
        end;

        renderer.SetCulling(r_CULLNONE);
        renderer.SetPointSize(4);

        if (not ThingsHidden) or (Map_mode = MM_TH) then
        begin
            With clThing do
                renderer.SetColor(CL_Front, r, g, b);

            for th := 0 to Level.Things.Count - 1 do
            begin
                thing := Level.Things[th];
                if ToolBar.IsLayerVisible(thing.Layer) then
                    DrawThing(thing);
            end;
        end;

        if (not LightsHidden) or (Map_mode = MM_LT) then
        begin
            With clLight do
                renderer.SetColor(CL_Front, r, g, b);

            For th := 0 to Level.Lights.Count - 1 do
            begin
                light := Level.Lights[th];
                if ToolBar.IsLayerVisible(light.Layer) then
                    With light do
                        renderer.DrawVertex(position.x, position.y, position.z);
            end;
        end;

        if Map_mode = MM_FR then
        begin
            With clFrame do
                renderer.SetColor(CL_Front, r, g, b);

            renderer.SetPointSize(4);
            for th := 0 to Level.Things.Count - 1 do
            begin
                thing := Level.Things[th];
                if not ToolBar.IsLayerVisible(thing.Layer) then
                    continue;
                With thing do
                    renderer.DrawVertex(X, Y, z);
                for s := 0 to thing.Vals.Count - 1 do
                    DrawFrame(thing.Vals[s]);

            end;
        end;

        { Draw extra stuff }
        With clExtra do
            renderer.SetColor(CL_Front, r, g, b);
        renderer.SetCulling(r_CULLNONE);
        for s := 0 to ExtraObjs.Count - 1 do
        begin
            obj := ExtraObjs[s];
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

    { Draw multiselection }

    case Map_mode of
        MM_SC:
            begin
                With clMSelBack do
                    renderer.SetColor(CL_Back, r, g, b);
                renderer.SetCulling(r_CULLBACK);
                for s := 0 to scsel.Count - 1 do
                begin
                    sec := Level.Sectors[scsel.GetSC(s)];
                    renderer.DrawPolygons(sec.surfaces);
                end;
                With clMSel do
                    renderer.SetColor(CL_Front, r, g, b);
                renderer.SetCulling(r_CULLFRONT);
                for s := 0 to scsel.Count - 1 do
                begin
                    sec := Level.Sectors[scsel.GetSC(s)];
                    renderer.DrawPolygons(sec.surfaces);
                end;
            end;
        MM_SF:
            begin
                With clMSelBack do
                    renderer.SetColor(CL_Back, r, g, b);
                renderer.SetCulling(r_CULLBACK);
                for s := 0 to sfsel.Count - 1 do
                begin
                    sfsel.GetSCSF(s, sc, SF);
                    surf := Level.Sectors[sc].surfaces[SF];
                    renderer.DrawPolygon(surf);
                end;

                With clMSel do
                    renderer.SetColor(CL_Front, r, g, b);
                renderer.SetCulling(r_CULLFRONT);

                for s := 0 to sfsel.Count - 1 do
                begin
                    sfsel.GetSCSF(s, sc, SF);
                    surf := Level.Sectors[sc].surfaces[SF];
                    renderer.DrawPolygon(surf);
                end;
            end;
        MM_ED:
            begin
                With clMSelBack do
                    renderer.SetColor(CL_Back, r, g, b);
                renderer.SetCulling(r_CULLBACK);
                for s := 0 to edsel.Count - 1 do
                begin
                    edsel.GetSCSFED(s, sc, SF, ED);
                    surf := Level.Sectors[sc].surfaces[SF];
                    With surf do
                        renderer.DrawLine(vertices[ED], vertices[NextVX(ED)]);
                end;
                With clMSel do
                    renderer.SetColor(CL_Front, r, g, b);
                renderer.SetCulling(r_CULLFRONT);
                for s := 0 to edsel.Count - 1 do
                begin
                    edsel.GetSCSFED(s, sc, SF, ED);
                    surf := Level.Sectors[sc].surfaces[SF];
                    With surf do
                        renderer.DrawLine(vertices[ED], vertices[NextVX(ED)]);
                end;
            end;
        MM_VX:
            begin
                With clMSel do
                    renderer.SetColor(CL_Front, r, g, b);
                { Renderer.SetCulling(r_CULLBACK); }
                renderer.SetPointSize(4);
                for s := 0 to vxsel.Count - 1 do
                begin
                    vxsel.GetSCVX(s, sc, VX);
                    With Level.Sectors[sc].vertices[VX] do
                        renderer.DrawVertex(X, Y, z);
                end;
            end;
        MM_TH:
            begin
                With clMSel do
                    renderer.SetColor(CL_Front, r, g, b);
                for s := 0 to thsel.Count - 1 do
                    DrawThing(Level.Things[thsel.GetTH(s)]);
            end;
        MM_FR:
            begin
                With clMSel do
                    renderer.SetColor(CL_Front, r, g, b);
                for s := 0 to frsel.Count - 1 do
                begin
                    frsel.GetTHFR(s, th, fr);
                    if fr = -1 then
                        continue;
                    DrawFrame(Level.Things[th].Vals[fr]);
                end;
            end;
        MM_LT:
            begin
                With clMSel do
                  renderer.SetColor(CL_Front, r, g, b);

                for s := 0 to ltsel.Count - 1 do
                  With Level.Lights[ltsel.getLT(s)] do
                    begin
                        renderer.DrawVertex(position.x, position.y, position.z);
                    end;
            end;
    end;

    { Draw Selection }
    With clMapSel do
        renderer.SetColor(CL_Front, r, g, b);

    if IsSelValid then
        case Map_mode of
            MM_SC:
                begin
                    sec := Level.Sectors[Cur_SC];

                    if scsel.FindSC(Cur_SC) = -1 then
                        With clMapSelBack do
                            renderer.SetColor(CL_Back, r, g, b)
                    else
                        With clSelMSelBack do
                            renderer.SetColor(CL_Back, r, g, b);

                    renderer.SetCulling(r_CULLBACK);
                    renderer.DrawPolygons(sec.surfaces);

                    if scsel.FindSC(Cur_SC) = -1 then
                        With clMapSel do
                            renderer.SetColor(CL_Front, r, g, b)
                    else
                        With clSelMSel do
                            renderer.SetColor(CL_Front, r, g, b);

                    renderer.SetCulling(r_CULLFRONT);
                    renderer.DrawPolygons(sec.surfaces);

                    if sec.vertices.Count > 0 then
                        With sec.vertices[0] do
                            renderer.DrawVertex(X, Y, z);
                end;
            MM_SF:
                begin
                    if sfsel.FindSF(Cur_SC, Cur_SF) = -1 then
                        With clMapSelBack do
                            renderer.SetColor(CL_Back, r, g, b)
                    else
                        With clSelMSelBack do
                            renderer.SetColor(CL_Back, r, g, b);

                    renderer.SetCulling(r_CULLBACK);
                    renderer.DrawPolygon(Level.Sectors[Cur_SC].surfaces
                      [Cur_SF]);

                    if sfsel.FindSF(Cur_SC, Cur_SF) = -1 then
                        With clMapSel do
                            renderer.SetColor(CL_Front, r, g, b)
                    else
                        With clSelMSel do
                            renderer.SetColor(CL_Front, r, g, b);

                    renderer.SetCulling(r_CULLFRONT);
                    renderer.DrawPolygon(Level.Sectors[Cur_SC].surfaces
                      [Cur_SF]);

                    With Level.Sectors[Cur_SC].surfaces[Cur_SF].vertices[0] do
                        renderer.DrawVertex(X, Y, z);
                    { v1:=TJKVertex.Create;
                      With level.Sectors[Cur_SC].surfaces[Cur_SF] do
                      With Vertices[0] do
                      begin
                      v1.x:=x+normal.dx*0.2;
                      v1.y:=y+normal.dy*0.2;
                      v1.z:=z+normal.dz*0.2;
                      Renderer.DrawLine(vertices[0],v1);
                      end;
                      v1.free; }
                end;
            MM_ED:
                With Level.Sectors[Cur_SC].surfaces[Cur_SF] do
                begin

                    if edsel.FindED(Cur_SC, Cur_SF, Cur_ED) = -1 then
                        With clMapSel do
                            renderer.SetColor(CL_Front, r, g, b)
                    else
                        With clSelMSel do
                            renderer.SetColor(CL_Front, r, g, b);

                    v1 := vertices[Cur_ED];
                    v2 := vertices[NextVX(Cur_ED)];
                    renderer.DrawLine(v1, v2);
                    renderer.SetPointSize(4);
                    renderer.DrawVertex(v1.X, v1.Y, v1.z);
                    dx := v2.X - v1.X;
                    dy := v2.Y - v1.Y;
                    dz := v2.z - v1.z;
                    v2 := TJKVertex.Create;
                    v2.X := v1.X + dx * 0.5;
                    v2.Y := v1.Y + dy * 0.5;
                    v2.z := v1.z + dz * 0.5;
                    VMult(normal.dx, normal.dy, normal.dz, dx, dy, dz, en.dx,
                      en.dy, en.dz);
                    v1 := TJKVertex.Create;
                    v1.X := v2.X + en.dx * 0.5;
                    v1.Y := v2.Y + en.dy * 0.5;
                    v1.z := v2.z + en.dz * 0.5;
                    renderer.DrawLine(v2, v1);
                    v1.free;
                    v2.free;
                end;
            MM_TH:
                begin
                    if thsel.FindTH(Cur_TH) = -1 then
                        With clMapSel do
                            renderer.SetColor(CL_Front, r, g, b)
                    else
                        With clSelMSel do
                            renderer.SetColor(CL_Front, r, g, b);

                    DrawSelThing(Level.Things[Cur_TH]);
                end;
            MM_FR:
                if Cur_FR = -1 then
                    with Level.Things[Cur_TH] do
                        renderer.DrawVertex(X, Y, z)
                else
                begin
                    if frsel.FindFR(Cur_TH, Cur_FR) = -1 then
                        With clMapSel do
                            renderer.SetColor(CL_Front, r, g, b)
                    else
                        With clSelMSel do
                            renderer.SetColor(CL_Front, r, g, b);
                    DrawSelFrame(Cur_TH, Cur_FR);
                end;
            MM_VX:
                begin
                    if vxsel.FindVX(Cur_SC, Cur_VX) = -1 then
                        With clMapSel do
                            renderer.SetColor(CL_Front, r, g, b)
                    else
                        With clSelMSel do
                            renderer.SetColor(CL_Front, r, g, b);

                    With Level.Sectors[Cur_SC].vertices[Cur_VX] do
                        renderer.DrawVertex(X, Y, z);
                end;
            MM_LT:
                begin
                    if ltsel.FindLT(Cur_LT) = -1 then
                        With clMapSel do
                            renderer.SetColor(CL_Front, r, g, b)
                    else
                        With clSelMSel do
                            renderer.SetColor(CL_Front, r, g, b);

                    if Cur_LT < Level.Lights.Count then
                        With Level.Lights[Cur_LT] do
                        begin
                            renderer.DrawVertex(position.x, position.y, position.z);
                            renderer.DrawCircle(position.x, position.y, position.z, range);
                        end;
                end;

            MM_Extra:
                begin
                    With clMSel do
                        renderer.SetColor(CL_Front, r, g, b);
                    obj := ExtraObjs[Cur_EX];
                    if obj is TVertex then
                        With TJKVertex(obj) do
                            renderer.DrawVertex(X, Y, z)
                    else if obj is TExtraLine then
                        With TExtraLine(obj) do
                            renderer.DrawLine(v1, v2)
                    else if obj is TPolygon then
                        renderer.DrawPolygon(TPolygon(obj));
                end;
        end;
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

    CalcLightOnLayers.ShortCut := ShortCut(Ord('L'), [ssCtrl, ssShift]);
    CalcLightInSel.ShortCut := ShortCut(Ord('L'), [ssCtrl, ssAlt]);

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

    EnableMapGeoBackfaceCull(true);
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
    AddKBItem(miMap, 'Move left', char(VK_RIGHT), []);
    AddKBItem(miMap, 'Move right', char(VK_LEFT), []);
    AddKBItem(miMap, 'Move Down', char(VK_UP), []);
    AddKBItem(miMap, 'Move Up', char(VK_Down), []);
    AddKBItem(miMap, 'Move forth', char(VK_NEXT), []);
    AddKBItem(miMap, 'Move back', char(VK_PRIOR), []);

    AddKBItem(miMap, 'Rotate'#9'Middle Mouse Button', #0, []);
    AddKBItem(miMap, 'Rotate in X direction'#9'Ctrl+Middle Mouse Button', #0, []);
    AddKBItem(miMap, 'Rotate in Y direction'#9'Ctrl+Shift+Middle Mouse Button', #0, []);
    AddKBItem(miMap, 'Rotate right', char(VK_RIGHT), [ssShift]);
    AddKBItem(miMap, 'Rotate left', char(VK_LEFT), [ssShift]);
    AddKBItem(miMap, 'Rotate Up', char(VK_UP), [ssShift]);
    AddKBItem(miMap, 'Rotate Down', char(VK_Down), [ssShift]);
    AddKBItem(miMap, 'Rotate ...', char(VK_NEXT), [ssShift]);
    AddKBItem(miMap, 'Rotate ...', char(VK_PRIOR), [ssShift]);
    AddKBItem(miMap, 'Zoom at cursor'#9'Mouse Wheel', #0, []);
    AddKBItem(miMap, 'Zoom at cursor', 'C', [ssShift]);
    AddKBItem(miMap, 'Zoom in', Chr(VK_ADD), []);
    AddKBItem(miMap, 'Zoom out', Chr(VK_SUBTRACT), []);
    AddKBItem(miMap, 'View To Grid', Chr(VK_MULTIPLY), []);

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

    AddKBItem(miTex, 'Start stitch', char(VK_INSERT), [ssCtrl]);
    AddKBItem(miTex, 'Start stitch', #186, []);

    AddKBItem(miTex, 'Stitch', char(VK_INSERT), [ssShift]);
    AddKBItem(miTex, 'Stitch', #222, []);

    AddKBItem(miTex, 'Straighten Texture', char(VK_HOME), [ssAlt]);
    AddKBItem(miTex, 'Straighten Texture', #191, []);
    AddKBItem(miTex, 'Scale texture down', #188, [ssAlt]);
    AddKBItem(miTex, 'Scale texture Up', #190, [ssAlt]);
    AddKBItem(miTex, 'Straighten/zero/rotate90 texture', #191,
      [ssShift, ssCtrl]);
    AddKBItem(miTex, 'Straighten/zero/rotate90 texture', char(VK_HOME),
      [ssShift, ssCtrl]);
    AddKBItem(miTex, 'Straighten/zero texture', #191, [ssCtrl]);
    AddKBItem(miTex, 'Straighten/zero texture', char(VK_HOME), [ssCtrl]);
    AddKBItem(miTex, 'Rotate texture left', #188, [ssCtrl]);
    AddKBItem(miTex, 'Rotate texture right', #190, [ssCtrl]);

    AddKBItem(MIrecovery, 'Build surface/sector', 'B', [ssAlt]);
    AddKBItem(MIrecovery, 'Delete surface', char(VK_DELETE), [ssAlt]);
    AddKBItem(MIrecovery, 'Invert surface', 'I', [ssAlt]);
    AddKBItem(MIrecovery, 'Planarize surface', 'P', [ssCtrl]);
    AddKBItem(MIrecovery, 'Fix solid surface(s)', OnFixSolidSurfaces);
    AddKBItem(MIrecovery, 'Fix passable adjoin(s)', OnFixPassableAdjoins);

    AddKBItem(MiEdit, 'Move item'#9'Ctrl+Left Mouse Button', #0, []);
    AddKBItem(MiEdit, 'Start cleave', 'C', []);
    AddKBItem(MiEdit, 'Cleave by grid', 'C', [ssAlt]);
    AddKBItem(MiEdit, 'Assign Thing to Sector', 'A', [ssShift]);
    AddKBItem(MiEdit, 'Unadjoin', 'A', [ssAlt]);
    AddKBItem(MiEdit, 'Adjoin', 'A', []);
    AddKBItem(MiEdit, 'Join surfaces', 'J', []);
    AddKBItem(MiEdit, 'Rotate Thing Pitch'#9'Ctrl+Shift+Mouse Wheel', #0, []);
    AddKBItem(MiEdit, 'Rotate Thing Yaw'#9'Ctrl+Mouse Wheel', #0, []);
    AddKBItem(MiEdit, 'Rotate Thing Roll'#9'Ctrl+Alt+Mouse Wheel', #0, []);
    AddKBItem(MiEdit, 'Raise item', char(VK_UP), [ssCtrl]);
    AddKBItem(MiEdit, 'Lower item', char(VK_Down), [ssCtrl]);
    AddKBItem(MiEdit, 'Raise item', #219, []);
    AddKBItem(MiEdit, 'Lower item', #221, []);
    AddKBItem(MiEdit, 'Item Editor', Chr(VK_return), []);
    AddKBItem(MiEdit, 'Merge', 'M', []);
    AddKBItem(MiEdit, 'Duplicate item', char(VK_INSERT), []);
    AddKBItem(MiEdit, 'Delete item', char(VK_DELETE), []);

    AddKBItem(MiEdit, 'Create sector', 'K', []);

    AddKBItem(MiEdit, 'Extrude surface', 'X', []);
    AddKBItem(MiEdit, 'Extrude surface by', 'X', [ssShift]);
    AddKBItem(MiEdit, 'Extrude && Expand surface by', 'X', [ssCtrl]);
    AddKBItem(MiEdit, 'Use item in COG', 'U', []);
    AddKBItem(MiEdit, 'Bring thing/light to surface', 'B', []);

    AddKBItem(miOther, 'Cancel cleave/etc', Chr(VK_ESCAPE), []);
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
        renderer.SetViewPort(0, 0, ClientWidth, Clientheight);
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

    case Map_mode of
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

    case Map_mode of
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
                    With renderer do
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
//                        renderer.Scale := renderer.Scale / 1.5;
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
                if Map_mode = MM_SF then
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

                    StartUndoRec('Extrude surface');

                    ExtrudeSurface(surf, dx);
                    SetCurSF(Cur_SC, Cur_SF);
                end;
            VK_RIGHT,
            VK_LEFT,
            VK_UP,
            VK_Down,
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
                        VK_Down:
                            rPch := rPch + 5;
                        VK_PRIOR:
                            rRol := rRol - 5;
                        VK_NEXT:
                            rRol := rRol + 5;
                    end;
                    SetRendfromPYR;
                    with renderer do
                      SetGridNormal(zv.dx, zv.dy, zv.dz); // New code, fixes cleaving
                    Invalidate;
                end
                else
                    With renderer do
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
                DoStitch;
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
                if Map_mode = MM_TH then
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
                case Map_mode of
                    MM_SC:
                        begin
                            i := scsel.AddSC(Cur_SC);
                            if scsel.Count = 2 then
                              begin
                                StartUndoRec('Unadjoin sectors');
                                if UnAdjoinSectors(Level.Sectors[scsel.GetSC(0)],
                                  Level.Sectors[scsel.GetSC(1)]) then
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
                            StartUndoRec('Unadjoin surfaces');
                            for i := 0 to sfsel.Count - 1 do
                              begin
                                sfsel.GetSCSF(i, nsc, nsf);
                                if UnAdjoin(Level.Sectors[nsc].surfaces[nsf]) then
                                  Inc(nsel);
                              end;
                            PanMessageFmt(mt_info,
                              '%d adjoins removed', [nsel]);
                            SetCurSF(Cur_SC, Cur_SF);
                            sfsel.DeleteN(X);
                        end;
                end;
            Ord('B'):
                case Map_mode of
                    MM_SF:
                        begin
                            i := sfsel.AddSF(Cur_SC, Cur_SF);
                            sec := BuildSector(Level, sfsel);
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
                            surf := BuildSurface(Level, vxsel);
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
                With renderer do
                    CleaveBy(gnormal, GridX, GridY, GridZ);
            { Cleave by grid plane }
            Ord('I'):
                if Map_mode = MM_SF then
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
            { VK_INSERT: Case map_mode of
              MM_SC: if GetMousePos(X,Y) then AddSectorAt(X,Y);
              end; }
            VK_HOME:
                begin
                    StraightenTexture(false, false);
                    exit;
                end;
            VK_DELETE:
                if Map_mode = MM_SF then
                    DeleteSurface(Level, Cur_SC, Cur_SF);
            188:
                begin
                    ScaleTexture(st_down);
                    exit;
                end; { < }
            190:
                begin
                    ScaleTexture(st_up);
                    exit;
                end; { > }

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
            188:
                begin
                    RotateTexture(st_left);
                    exit;
                end; { < }
            190:
                begin
                    RotateTexture(st_right);
                    exit;
                end; { > }
            VK_UP:
                RaiseObject(ro_up);
            VK_Down:
                RaiseObject(ro_down);
            Ord('C'):
                miCopyClick(nil);
            Ord('V'):
                miPasteClick(nil);
            Ord('P'):
                if Map_mode = MM_SF then
                begin
                    StartUndoRec('Flatten surface');
                    if FlattenSurface(Level.Sectors[Cur_SC].surfaces[Cur_SF])
                    then
                        PanMessage(mt_info, 'Surface planarized')
                    else
                        PanMessage(mt_info, 'Surface wasn''t fully planarized');
                    Invalidate;
                end;
            Ord('X'):
                if Map_mode = MM_SF then
                begin
                    surf := Level.Sectors[Cur_SC].surfaces[Cur_SF];
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

                    StartUndoRec('Extrude && Expand surface');

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
                with renderer do
                  SetGridNormal(zv.dx, zv.dy, zv.dz); // New code, fixes cleaving
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
                renderer.Scale := renderer.Scale / 1.5;
            end;
        VK_SUBTRACT:
            begin
                renderer.Scale := Math.Min(renderer.Scale * 1.5, 3.5);  // max zoom out
            end;
        VK_UP, VK_Down, VK_LEFT, VK_RIGHT, VK_NEXT, VK_PRIOR:
            With renderer do
            begin
                { CreateRotMatrix(mx,PCH,YAW,ROL); }

                With renderer do
                    nx.SetCoords(xv.dx * DpiScale, xv.dy * DpiScale, xv.dz * DpiScale);
                With renderer do
                    ny.SetCoords(yv.dx * DpiScale, yv.dy * DpiScale, yv.dz * DpiScale);
                With renderer do
                    nz.SetCoords(zv.dx * DpiScale, zv.dy * DpiScale, zv.dz * DpiScale);

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
                    VK_Down:
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
        VK_return:
            begin
                if IsSelValid then
                    ItemEdit.Show;
                exit;
            end;
        Ord('A'):
            Case Map_mode of
                MM_SC:
                    begin
                        i := scsel.AddSC(Cur_SC);
                        if scsel.Count = 2 then
                        begin
                            StartUndoRec('Adjoin Sectors');
                            if AdjoinSectors(Level.Sectors[scsel.GetSC(0)],
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
                        StartUndoRec('Adjoin surfaces');
                        for i := 0 to sfsel.Count - 1 do
                        begin
                            sfsel.GetSCSF(i, nsc, nsf);
                            if MakeAdjoin(Level.Sectors[nsc].surfaces[nsf]) then
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
            case Map_mode of
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
            if Map_mode = MM_SF then
            begin
                surf := Level.Sectors[Cur_SC].surfaces[Cur_SF];
                StartUndoRec('Extrude surface');
                ExtrudeSurface(surf, surf.ExtrudeSize);
                SetCurSF(Cur_SC, Cur_SF);
            end;
        Ord('S'):
            SetMapMode(MM_SC);
        Ord('C'):
            SetMouseMode(MM_Cleave);
//        Ord('W'):
//            SetMouseMode(MM_Ruler);
        Ord('F'):
            SetMapMode(MM_SF);
        Ord('J'):
            case Map_mode of
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
                case Map_mode of
                    MM_SC:
                        begin
                            i := scsel.AddSC(Cur_SC);
                            scsel.sort;
                            nsel := 0;
                            StartUndoRec('Merge sectors');

                            while scsel.Count > 1 do
                            begin
                                sec := Level.Sectors[scsel.GetSC(scsel.Count - 1)];
                                for i := 0 to scsel.Count - 2 do
                                begin
                                    if MergeSectors(Level.Sectors[scsel.GetSC(i)
                                      ], sec) <> nil then
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
                                        surf := Level.Sectors[Cur_SC].surfaces[Cur_SF];
                                        if surf.adjoin = nil then
                                          exit;
                                        StartUndoRec('Merge sectors');
                                        sec := MergeSectors(surf.sector,
                                        surf.adjoin.sector);
                                        if sec = nil then
                                          PanMessage(mt_info,
                                        'Sectors weren''t merged')
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
                                    StartUndoRec('Merge surfaces');

                                    while sfsel.Count > 1 do
                                    begin
                                        sfsel.GetSCSF(sfsel.Count - 1,
                                        nsc, nsf);
                                        surf := Level.Sectors[nsc]
                                        .surfaces[nsf];
                                        for i := 0 to sfsel.Count - 2 do
                                        begin
                                        sfsel.GetSCSF(i, nsc, nsf);
                                        if MergeSurfaces
                                        (Level.Sectors[nsc].surfaces[nsf], surf)
                                        <> nil then
                                        begin
                                        Inc(nsel);
                                        break;
                                        end;

                                        end;
                                        sfsel.DeleteN(sfsel.Count - 1);
                                    end;

                                    sfsel.GetSCSF(0, nsc, nsf);
                                    surf := Level.Sectors[nsc].surfaces[nsf];
                                    sfsel.Clear;
                                    SetCurSF(surf.sector.num, surf.num);
                                    PanMessageFmt(mt_info,
                                      '%d surfaces merged', [nsel]);

                                end;
                                { else ShowMessage('You should either select one surface to merge sector or two surfaces to merge surfaces'); }
                            end;
                            sfsel.DeleteN(i);
                        end;
                    MM_VX:
                        begin
                            PanMessageFmt(mt_info, '%d vertices merged',
                              [MergeVertices(Level, vxsel)]);
                            vxsel.Clear;
                        end;
                    MM_ED:
                        begin
                            surf := Level.Sectors[Cur_SC].surfaces[Cur_SF];
                            StartUndoRec('Merge surfaces');
                            if MergeSurfacesAt(surf, Cur_ED) then
                                edsel.Clear;
                        end;
                end;
                SetMapMode(Map_mode);
            end;
        VK_INSERT:
            Begin
                Case Map_mode of
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
                case Map_mode of
                    MM_SC:
                        begin
                            { SaveSelSecUndo('Delete Sector(s)',ch_deleted,sc_both); }
                            scsel.AddSC(Cur_SC);
                            scsel.sort;
                            if scsel.Count >= Level.Sectors.Count then
                                exit;

                            StartUndoRec('Delete Sector(s)');

                            for i := scsel.Count - 1 downto 0 do
                                DeleteSector(Level, scsel.GetSC(i));
                            { updated in procedure }
                            scsel.Clear;
                        end;
                    MM_TH:
                        begin
                            thsel.AddTH(Cur_TH);
                            thsel.sort;
                            if thsel.Count >= Level.Things.Count then
                                exit;
                            StartUndoRec('Delete Thing(s)');
                            for i := thsel.Count - 1 downto 0 do
                            begin
                                Free3DO(Level.Things[thsel.GetTH(i)].a3DO);
                                DeleteThing(Level, thsel.GetTH(i));
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
                                DeleteFrame(nth, nfr);
                            end;
                            frsel.Clear;
                        end;
                    MM_LT:
                        begin
                            ltsel.AddLT(Cur_LT);
                            ltsel.sort;
                            if ltsel.Count >= Level.Lights.Count then
                                exit;
                            { SaveSelLightsUndo('Delete light(s)',ch_deleted); }
                            StartUndoRec('Delete Light(s)');
                            for i := ltsel.Count - 1 downto 0 do
                                DeleteLight(Level, ltsel.getLT(i));
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
                                if DeleteVertex(Level.Sectors[X], Y,
                                  vxsel.Count <> 1) then
                                    Inc(a);
                            end;
                            PanMessageFmt(mt_info, '%d vertices deleted', [a]);
                            vxsel.Clear;
                        end;
                end;
                SetMapMode(Map_mode);
            end;
        { ; } 186:
            begin
                StartStitch;
                exit;
            end;
        { ' } 222:
            begin
                DoStitch;
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
        renderer.SetPointSize(4);
        Case Map_mode of
            MM_SC:
                for s := 0 to Level.Sectors.Count - 1 do
                begin
                    sec := Level.Sectors[s];
                    if ToolBar.IsLayerVisible(sec.Layer) then
                        renderer.PickPolygons(sec.surfaces, s);
                end;
            MM_SF:
                for s := 0 to Level.Sectors.Count - 1 do
                begin
                    sec := Level.Sectors[s];
                    if ToolBar.IsLayerVisible(sec.Layer) then
                        for SF := 0 to sec.surfaces.Count - 1 do
                            renderer.PickPolygon(sec.surfaces[SF],
                              s + SF * 65536);
                end;
            MM_ED:
                for s := 0 to Level.Sectors.Count - 1 do
                begin
                    sec := Level.Sectors[s];
                    if ToolBar.IsLayerVisible(sec.Layer) then
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
                For th := 0 to Level.Things.Count - 1 do
                begin
                    thing := Level.Things[th];
                    if ToolBar.IsLayerVisible(thing.Layer) then
                        With thing do
                            renderer.PickVertex(X, Y, z, th);
                end;
            MM_FR:
                For th := 0 to Level.Things.Count - 1 do
                begin
                    thing := Level.Things[th];
                    if not ToolBar.IsLayerVisible(thing.Layer) then
                        continue;
                    with thing Do
                        renderer.PickVertex(X, Y, z, th * 65536 + 65535);
                    for fr := 0 to thing.Vals.Count - 1 do
                        with thing.Vals[fr] do
                        begin
                            if atype <> at_frame then
                                continue;
                            GetFrame(fx, fy, fz, pch, yaw, rol);
                            renderer.PickVertex(fx, fy, fz, th * 65536 + fr);
                        end;
                end;
            MM_VX:
                for s := 0 to Level.Sectors.Count - 1 do
                begin
                    sec := Level.Sectors[s];
                    if ToolBar.IsLayerVisible(sec.Layer) then
                        With sec do
                            for VX := 0 to vertices.Count - 1 do
                                With vertices[VX] do
                                    renderer.PickVertex(X, Y, z,
                                      s + VX * 65536);
                end;
            MM_LT:
                for th := 0 to Level.Lights.Count - 1 do
                begin
                    light := Level.Lights[th];
                    if ToolBar.IsLayerVisible(light.Layer) then
                        With light do
                            renderer.PickVertex(position.x, position.y, position.z, th);
                end;
            MM_Extra:
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
        renderer.EndPick;
        if renderer.Selected.Count <> 0 then
            csel := 0;
    end;
    { Lb.Caption:=IntToStr(Renderer.Selected.Count); }
    if (renderer.Selected.Count <> 0) then
    begin
        case Map_mode of
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

{ Cleave mouse functions }

Procedure TJedMain.Do_StartCleave(X, Y: Integer; snaptovertex: boolean);
begin
    if CleaveStarted then
        ReDraw;
    With CleaveOrgXY do
        if not GetMousePos(Integer(X), Integer(Y)) then
            exit;
    WIth CleaveOrg do
    begin
        if not renderer.GetGridAt(CleaveOrgXY.X, CleaveOrgXY.Y, X, Y, z) then
            exit;
        if snaptovertex then
            GetNearestVX(Level, X, Y, z, X, Y, z, 0.2)
        else if SnapToGrid then
            renderer.GetNearestGrid(X, Y, z, X, Y, z);
    end;
    With CleaveOrg do
        renderer.ProjectPoint(X, Y, z, Integer(CleaveOrgXY.X),
          Integer(CleaveOrgXY.Y));
    LastX := CleaveOrgXY.X;
    LastY := CleaveOrgXY.Y;
    SetMouseMode(MM_Cleave);
    CleaveStarted := true;
end;

Procedure TJedMain.Do_ProceedCleave(X, Y: Integer; snaptovertex: boolean);
var
    fx, fy, fz: double;
begin
    if not CleaveStarted then
        exit;
    Canvas.Pen.mode := pmXor;
    Canvas.Pen.Color := clRed;
    Canvas.MoveTo(CleaveOrgXY.X, CleaveOrgXY.Y);
    Canvas.LineTo(LastX, LastY);
    Canvas.MoveTo(CleaveOrgXY.X, CleaveOrgXY.Y);

    if not renderer.GetGridAt(X, Y, fx, fy, fz) then
        exit;
    if snaptovertex then
        GetNearestVX(Level, fx, fy, fz, fx, fy, fz, 0.2)
    else if SnapToGrid then
        renderer.GetNearestGrid(fx, fy, fz, fx, fy, fz);

    renderer.ProjectPoint(fx, fy, fz, X, Y);
    Canvas.LineTo(X, Y);
    LastX := X;
    LastY := Y;
end;

Procedure TJedMain.Do_EndCleave(X, Y: Integer; snaptovertex: boolean);
var
    x1, y1, z1, x2, y2, z2: double;
    cnormal: TVector;
    cleaved: boolean;
    i: Integer;
begin
    try
        if not CleaveStarted then
            exit;

        if not renderer.GetGridAt(X, Y, x2, y2, z2) then
            exit;
        if snaptovertex then
            GetNearestVX(Level, x2, y2, z2, x2, y2, z2, 0.2)
        else if SnapToGrid then
            renderer.GetNearestGrid(x2, y2, z2, x2, y2, z2);

        With CleaveOrg do
        begin
            x1 := X;
            y1 := Y;
            z1 := z;
        end;
        If IsClose(x2, x1) and IsClose(y2, y1) and IsClose(z2, z1) Then
            exit;
        With renderer do
            VMult(gnormal.dx, gnormal.dy, gnormal.dz, x2 - x1, y2 - y1, z2 - z1,
              cnormal.dx, cnormal.dy, cnormal.dz);
        Normalize(cnormal);
        cleaved := false;
        CleaveBy(cnormal, x1, y1, z1);
    finally
        SetMouseMode(MM_Select);
        Invalidate;
        CleaveStarted := false;
    end;
end;


{ Whip Ruler mouse functions }

Procedure TJedMain.Do_StartRuler(X, Y: Integer; snaptovertex: boolean);
begin
    if RulerStarted then
        ReDraw;
    With RulerOrgXY do
        if not GetMousePos(Integer(X), Integer(Y)) then
            exit;
    WIth RulerOrg do
    begin
        if not renderer.GetGridAt(RulerOrgXY.X, RulerOrgXY.Y, X, Y, z) then
            exit;
        if snaptovertex then
            GetNearestVX(Level, X, Y, z, X, Y, z, 0.2)
        else if SnapToGrid then
            renderer.GetNearestGrid(X, Y, z, X, Y, z);
    end;
    With RulerOrg do
        renderer.ProjectPoint(X, Y, z, Integer(RulerOrgXY.X),
          Integer(RulerOrgXY.Y));
    LastX := RulerOrgXY.X;
    LastY := RulerOrgXY.Y;
    SetMouseMode(MM_Ruler);
    RulerStarted := true;
end;

Procedure TJedMain.Do_ProceedRuler(X, Y: Integer; snaptovertex: boolean);
var
    fx, fy, fz: double;
begin
    if not RulerStarted then
        exit;
    Canvas.Pen.mode  := pmXor;
    Canvas.Pen.Color := clRed;
    Canvas.MoveTo(RulerOrgXY.X, RulerOrgXY.Y);
    Canvas.LineTo(LastX, LastY);
    Canvas.MoveTo(RulerOrgXY.X, RulerOrgXY.Y);

    if not renderer.GetGridAt(X, Y, fx, fy, fz) then
        exit;
    if snaptovertex then
        GetNearestVX(Level, fx, fy, fz, fx, fy, fz, 0.2)
    else if SnapToGrid then
        renderer.GetNearestGrid(fx, fy, fz, fx, fy, fz);


    renderer.ProjectPoint(fx, fy, fz, X, Y);

    Canvas.LineTo(X, Y);
    LastX := X;
    LastY := Y;
end;

Procedure TJedMain.Do_EndRuler(X, Y: Integer; snaptovertex: boolean);
var
    x1, y1, z1, x2, y2, z2: double;
    cnormal: TVector;
    cleaved: boolean;
    i: Integer;
begin
    try
        if not RulerStarted then
            exit;
    finally
        SetMouseMode(MM_Select);
        Invalidate;
        RulerStarted := false;
    end;
end;

Procedure TJedMain.DO_StartRectSelect(X, Y: Integer);
begin
    RectSelStarted := true;
    RectOrgX := X;
    RectOrgY := Y;
    RectLastX := RectOrgX;
    RectLastY := RectOrgY;
    SetMouseMode(MM_RectSelect);
end;

Procedure TJedMain.DO_ProceedRectSelect(X, Y: Integer);
begin
    Canvas.Pen.mode := pmXor;
    Canvas.Pen.Color := clRed;
    Canvas.Brush.Style := bsClear;
    Canvas.Rectangle(RectOrgX, RectOrgY, RectLastX, RectLastY);
    Canvas.Rectangle(RectOrgX, RectOrgY, X, Y);
    RectLastX := X;
    RectLastY := Y;
end;

Procedure TJedMain.DO_EndRectSelect(X, Y: Integer);
var
    i, SF, VX, ED, fr: Integer;
    ax, ay, az, pch, yaw, rol: double;
    allin: boolean;
begin
    renderer.BeginRectPick(RectOrgX, RectOrgY, X, Y);
    case Map_mode of
        MM_SC:
            begin
                for i := 0 to Level.Sectors.Count - 1 do
                    With Level.Sectors[i] do
                        if ToolBar.IsLayerVisible(Layer) then
                        begin
                            allin := true;
                            for VX := 0 to vertices.Count - 1 do
                                With vertices[VX] do
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
                for i := 0 to Level.Sectors.Count - 1 do
                    With Level.Sectors[i] do
                        if ToolBar.IsLayerVisible(Layer) then
                            For SF := 0 to surfaces.Count - 1 do
                                if renderer.IsPolygonInRect(surfaces[SF]) then
                                    DO_SelSF(i, SF);

            end;
        MM_ED:
            begin
                for i := 0 to Level.Sectors.Count - 1 do
                    With Level.Sectors[i] do
                        if ToolBar.IsLayerVisible(Layer) then
                            For SF := 0 to surfaces.Count - 1 do
                                With surfaces[SF] do
                                    for ED := 0 to vertices.Count - 1 do
                                        if renderer.IsLineInRect(vertices[ED],
                                        vertices[NextVX(ED)]) then
                                        DO_SelED(i, SF, ED);
            end;
        MM_VX:
            begin
                for i := 0 to Level.Sectors.Count - 1 do
                    With Level.Sectors[i] do
                        if ToolBar.IsLayerVisible(Layer) then
                            for VX := 0 to vertices.Count - 1 do
                                With vertices[VX] do
                                    if renderer.IsVertexInRect(X, Y, z) then
                                        DO_SelVX(i, VX);
            end;
        MM_TH:
            begin
                for i := 0 to Level.Things.Count - 1 do
                    With Level.Things[i] do
                        if ToolBar.IsLayerVisible(Layer) then
                            if renderer.IsVertexInRect(X, Y, z) then
                                DO_SelTH(i);
            end;
        MM_LT:
            begin
                for i := 0 to Level.Lights.Count - 1 do
                    With Level.Lights[i] do
                        if ToolBar.IsLayerVisible(Layer) then
                            if renderer.IsVertexInRect(position.x, position.y, position.z) then
                                DO_SelLT(i);
            end;
        MM_FR:
            begin
                for i := 0 to Level.Things.Count - 1 do
                    With Level.Things[i] do
                        if ToolBar.IsLayerVisible(Layer) then
                            for fr := 0 to Vals.Count - 1 do
                                With Vals[fr] do
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
    if not renderer.GetGridAt(X, Y, fx, fy, fz) then
        exit;

    SetMouseMode(MM_Drag);
    DragOrg.X := fx;
    DragOrg.Y := fy;
    DragOrg.z := fz;

    with DragOrgPos do
        GetCurObjRefXYZ(X, Y, z);

    case Map_mode of
        MM_SC:
            StartUndoRec('Drag sector(s)');
        MM_SF:
            StartUndoRec('Drag surface(s)');
        MM_ED:
            StartUndoRec('Drag edge(s)');
        MM_VX:
            StartUndoRec('Drag vertices');

        MM_LT:
            StartUndoRec('Drag light(s)');
        MM_TH:
            StartUndoRec('Drag thing(s)');
        MM_FR:
            StartUndoRec('Drag frame(s)');
        MM_Extra:
            if Assigned(OnExtraMove) then
                OnExtraMove(ExtraObjs[Cur_EX], false);
    end;
end;

Procedure TJedMain.GetCurObjRefXYZ(var rx, ry, rz: double);
var
    sec: TJKSector;
    surf: TJKSurface;
    VX: TJKVertex;
    th: TJKThing;
    LT: TSedLight;
    vl: TTPLValue;
    EX: TObject;
    pch, yaw, rol: double;
begin
    case Map_mode of
        MM_SC:
            begin
                sec := Level.Sectors[Cur_SC];
                With sec.vertices[0] do
                begin
                    rx := X;
                    ry := Y;
                    rz := z;
                end;
            end;
        MM_SF:
            begin
                surf := Level.Sectors[Cur_SC].surfaces[Cur_SF];
                With surf.vertices[0] do
                begin
                    rx := X;
                    ry := Y;
                    rz := z;
                end;
            end;
        MM_VX:
            begin
                VX := Level.Sectors[Cur_SC].vertices[Cur_VX];
                With VX do
                begin
                    rx := X;
                    ry := Y;
                    rz := z;
                end;
            end;
        MM_ED:
            With Level.Sectors[Cur_SC].surfaces[Cur_SF] do
            begin
                VX := vertices[Cur_ED];
                With VX do
                begin
                    rx := X;
                    ry := Y;
                    rz := z;
                end;
            end;
        MM_TH:
            begin
                th := Level.Things[Cur_TH];
                With th do
                begin
                    rx := X;
                    ry := Y;
                    rz := z;
                end;
            end;
        MM_FR:
            begin
                if Cur_FR = -1 then
                begin
                    th := Level.Things[Cur_TH];
                    With th do
                    begin
                        rx := X;
                        ry := Y;
                        rz := z;
                    end;
                end
                else
                begin
                    vl := Level.Things[Cur_TH].Vals[Cur_FR];
                    vl.GetFrame(rx, ry, rz, pch, yaw, rol);
                end;
            end;
        MM_LT:
            begin
                LT := Level.Lights[Cur_LT];
                With LT do
                  begin
                      rx := position.x;
                      ry := position.y;
                      rz := position.z;
                  end;
            end;
        MM_Extra:
            begin
                if (Cur_EX < -1) or (ExtraObjs.Count = 0) then
                  begin
                    rx := 0;
                    ry := 0;
                    rz := 0;
                    exit;
                  end;

                EX := ExtraObjs[Cur_EX];
                if EX is TVertex then
                    With TVertex(EX) do
                    begin
                        rx := X;
                        ry := Y;
                        rz := z;
                    end;
                if EX is TExtraLine then
                    With TExtraLine(EX).v1 do
                    begin
                        rx := X;
                        ry := Y;
                        rz := z;
                    end;
                if EX is TPolygon then
                    With TPolygon(EX).vertices[0] do
                    begin
                        rx := X;
                        ry := Y;
                        rz := z;
                    end;
            end;
    else
        begin
            rx := 0;
            ry := 0;
            rz := 0;
        end;
    end;
end;

Procedure TJedMain.Do_ProceedDrag(X, Y: Integer; snaptovx: boolean;
  snaptoaxis, yaxis: boolean);
const
    dragcloseenough = 0.2;
var
    fx, fy, fz, dx, dy, dz: double;

    Procedure GetDXYZ(rx, ry, rz: double; var dx, dy, dz: double);
    var
        ax, ay, az, d: double;
        arx, ary, arz: double;
    begin
        With DragOrg do
        begin
            dx := fx - X;
            dy := fy - Y;
            dz := fz - z;
        end;

        if snaptoaxis then
            with renderer do
            begin
                arx := gxnormal.dx * dx + gxnormal.dy * dy + gxnormal.dz * dz;
                ary := gynormal.dx * dx + gynormal.dy * dy + gynormal.dz * dz;

                if yaxis then
                begin
                    dx := gynormal.dx * ary;
                    dy := gynormal.dy * ary;
                    dz := gynormal.dz * ary;
                end
                else
                begin
                    dx := gxnormal.dx * arx;
                    dy := gxnormal.dy * arx;
                    dz := gxnormal.dz * arx;
                end;

                if SnapToGrid then
                    with renderer do
                    begin
                        d := gnormal.dx * GridX + gnormal.dy * GridY +
                          gnormal.dz * GridZ;
                        PlaneLineXn(gnormal, d, rx + dx, ry + dy, rz + dz,
                          rx + dx + gnormal.dx, ry + dy + gnormal.dy,
                          rz + dz + gnormal.dz, arx, ary, arz);
                        renderer.GetNearestGrid(arx, ary, arz, ax, ay, az);
                        dx := ax - arx + dx;
                        dy := ay - ary + dy;
                        dz := az - arz + dz;
                    end;

                exit;

            end;

        case snaptovx of
            false:
                case SnapToGrid of
                    false:
                        exit;
                    true:
                        begin
                            { Find distance to plane - signed }
                            With renderer do
                            begin
                                { a:=SMult(gnormal.dX,gnormal.dy,gnormal.dz,rx+dx-gridX,ry+dy-GridY,rz+dz-GridZ); }
                                d := gnormal.dx * GridX + gnormal.dy * GridY +
                                  gnormal.dz * GridZ;
                                PlaneLineXn(gnormal, d, rx + dx, ry + dy,
                                  rz + dz, rx + dx + gnormal.dx,
                                  ry + dy + gnormal.dy, rz + dz + gnormal.dz,
                                  arx, ary, arz);
                                renderer.GetNearestGrid(arx, ary, arz,
                                  ax, ay, az);
                                dx := ax - arx + dx;
                                dy := ay - ary + dy;
                                dz := az - arz + dz;
                            end;
                        end;
                end;
            true:
                begin
                    if not GetNearestVX(Level, rx + dx, ry + dy, rz + dz, ax,
                      ay, az, dragcloseenough) then
                        exit;
                    dx := ax - rx;
                    dy := ay - ry;
                    dz := az - rz;
                end;
        end;
    end;

Var
    i: Integer;
    rx, ry, rz: double;

begin
    if not renderer.GetGridAt(X, Y, fx, fy, fz) then
        exit;

    // GetCurObjRefXYZ(rx,ry,rz);

    With DragOrgPos do
    begin
        rx := X;
        ry := Y;
        rz := z;
    end;

    GetDXYZ(rx, ry, rz, dx, dy, dz);

    GetCurObjRefXYZ(rx, ry, rz);

    dx := DragOrgPos.X - rx + dx;
    dy := DragOrgPos.Y - ry + dy;
    dz := DragOrgPos.z - rz + dz;

    case Map_mode of
        MM_SC:
            TranslateSectors(Level, scsel, Cur_SC, dx, dy, dz);
        MM_SF:
            TranslateSurfaces(Level, sfsel, Cur_SC, Cur_SF, dx, dy, dz);
        MM_VX:
            TranslateVertices(Level, vxsel, Cur_SC, Cur_VX, dx, dy, dz);
        MM_ED:
            TranslateEdges(Level, edsel, Cur_SC, Cur_SF, Cur_ED, dx, dy, dz);
        MM_TH:
            TranslateThings(Level, thsel, Cur_TH, dx, dy, dz, MoveFrames);
        MM_FR:
            if Cur_FR = -1 then
                TranslateThingKeepFrame(Level, Cur_TH, dx, dy, dz)
            else
                TranslateFrames(Level, frsel, Cur_TH, Cur_FR, dx, dy, dz);
        MM_LT:
            TranslateLights(Level, ltsel, Cur_LT, dx, dy, dz);
        MM_Extra:
            TranslateExtras(Cur_EX, dx, dy, dz);
    end;

    // With DragOrg do
    // begin
    // x:=x+dx;
    // y:=y+dy;
    // z:=z+dz;
    // end;

    Invalidate;
end;

Procedure TJedMain.Do_EndDrag(X, Y: Integer);
begin
    { Case Map_Mode of
      mm_th: LayerThing(Cur_TH);
      end; }
    SetMouseMode(MM_Select);
end;

Procedure TJedMain.Do_StartTranslateCam(X, Y: Integer);
begin
    CamOrg.X := X;
    CamOrg.Y := Y;
    SetMouseMode(MM_TranslateCam);
end;

Procedure TJedMain.Do_TranslateCam(X, Y: Integer);
var
    nx, ny, nz, nx1, ny1, nz1: double;
begin
    if (CamOrg.X = X) and (CamOrg.Y = Y) then exit;
    With renderer do
    begin
        if not GetXYZonPlaneAt(X, Y, renderer.zv, CamX, CamY, CamZ, nx, ny, nz)
        then exit;

        if not GetXYZonPlaneAt(CamOrg.X, CamOrg.Y, renderer.zv, CamX, CamY, CamZ, nx1, ny1, nz1)
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
    if Mouse_mode = MM_RotateCam then
        exit;
    GOrgXY.X := X;
    GOrgXY.Y := Y;
    With GOrg do
    begin
        X := rPch;
        Y := rYaw;
        z := rRol;
    end;
    OrgX := renderer.xv;
    OrgY := renderer.yv;
    OrgZ := renderer.zv;


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
    dx := X - GOrgXY.X;
    dy := Y - GOrgXY.Y;
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
        with GOrg do
        begin
            const s = Min(renderer.DpiScale * 100.0, 1.0);
            rRol := z - dx * s;
            rPch := X - dy * s;
//            if rRol > 360 then
//              rRol := rRol - 360
//            else if rRol < 0 then
//              rRol := rRol + 360;
//            if rPch > 360 then
//              rPch := rPch - 360
//            else if rPch < 0 then
//              rPch := rPch + 360;

           // PanMessageFmt(mt_Info, 'pch: %f rol: %f', [rPch, rRol]);
            SetRendfromPYR;
        end;
    end
    else
    begin
        nx := OrgX;
        ny := OrgY;
        nz := OrgZ;

        RotAxisAngle(nx, ny, dx); { l/r }
        RotAxisAngle(nz, ny, dy); { u/d }

        With nz do
            renderer.SetZ(dx, dy, dz);
        With nx do
            renderer.SetX(dx, dy, dz);
//        With ny do
//            renderer.SetY(dx, dy, dz);

    end;

    with renderer do
        SetGridNormal(zv.dx, zv.dy, zv.dz); // New code, fixes cleaving
    Invalidate;
end;

Procedure TJedMain.Do_StartTranslateGrid(X, Y: Integer);
begin
    GOrgXY.X := X;
    GOrgXY.Y := Y;
    With GOrg, renderer do
    begin
        X := GridX;
        Y := GridY;
        z := GridZ;
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
    GOrgXY.X := X;
    GOrgXY.Y := Y;
    With GOrg, renderer do
    begin
        X := GridX;
        Y := GridY;
        z := GridZ;
    end;
end;

Procedure TJedMain.Do_RotateGrid(X, Y: Integer);
var
    dx, dy: double;
begin
    dx := X - GOrgXY.X;
    dy := Y - GOrgXY.Y;

end;

Procedure TJedMain.DO_StartCreateSC(X, Y: Integer; snaptovertex: boolean);
begin
    if SCCreateStarted then
        ReDraw;
    With CleaveOrgXY do
        if not GetMousePos(Integer(X), Integer(Y)) then
            exit;
    WIth CleaveOrg do
    begin
        if not renderer.GetGridAt(CleaveOrgXY.X, CleaveOrgXY.Y, X, Y, z) then
            exit;
        if snaptovertex then
            GetNearestVX(Level, X, Y, z, X, Y, z, 0.2)
        else if SnapToGrid then
            renderer.GetNearestGrid(X, Y, z, X, Y, z);
    end;
    With CleaveOrg do
        renderer.ProjectPoint(X, Y, z, Integer(CleaveOrgXY.X),
          Integer(CleaveOrgXY.Y));
    LastX := CleaveOrgXY.X;
    LastY := CleaveOrgXY.Y;
    SetMouseMode(MM_CreateSector);
    SCCreateStarted := true;
end;

Procedure TJedMain.DO_ProceedCreateSC(X, Y: Integer; snaptovertex: boolean);
var
    fx, fy, fz: double;
begin
    if not SCCreateStarted then
        exit;
    Canvas.Pen.mode := pmXor;
    Canvas.Pen.Color := clRed;
    Canvas.MoveTo(CleaveOrgXY.X, CleaveOrgXY.Y);
    Canvas.LineTo(LastX, LastY);
    Canvas.MoveTo(CleaveOrgXY.X, CleaveOrgXY.Y);

    if not renderer.GetGridAt(X, Y, fx, fy, fz) then
        exit;
    if snaptovertex then
        GetNearestVX(Level, fx, fy, fz, fx, fy, fz, 0.2)
    else if SnapToGrid then
        renderer.GetNearestGrid(fx, fy, fz, fx, fy, fz);

    renderer.ProjectPoint(fx, fy, fz, X, Y);
    Canvas.LineTo(X, Y);
    LastX := X;
    LastY := Y;
end;

Procedure TJedMain.DO_EndCreateSC(X, Y: Integer; snaptovertex: boolean);
var
    x1, y1, z1, x2, y2, z2: double;
    nx, ny, nz: TVector;
    vl: double;
    sc, nsc: TJKSector;
    box: TBox;
    sec: TJKSector;
begin
    try
        if not SCCreateStarted then
            exit;

        if not renderer.GetGridAt(X, Y, x2, y2, z2) then
            exit;
        if snaptovertex then
            GetNearestVX(Level, x2, y2, z2, x2, y2, z2, 0.2)
        else if SnapToGrid then
            renderer.GetNearestGrid(x2, y2, z2, x2, y2, z2);

        With CleaveOrg do
        begin
            x1 := X;
            y1 := Y;
            z1 := z;
        end;
        nx.dx := x2 - x1;
        nx.dy := y2 - y1;
        nx.dz := z2 - z1;

        vl := Vlen(nx);
        sc := ToolBar.GetNewShape;

        if vl < 0.1 then
            nx := renderer.gxnormal
        else
        begin
            FindBBox(sc, box);
            vl := box.x2 - box.x1;
            nx.SetCoords(nx.dx / vl, nx.dy / vl, nx.dz / vl);
        end;

        vl := Vlen(nx);

        nz := renderer.gnormal;
        nz.SetCoords(nz.dx * vl, nz.dy * vl, nz.dz * vl);

        VMult(nz.dx, nz.dy, nz.dz, nx.dx, nx.dy, nx.dz, ny.dx, ny.dy, ny.dz);

        Normalize(ny);
        ny.SetCoords(ny.dx * vl, ny.dy * vl, ny.dz * vl);

        sec := Level.Sectors[Cur_SC];
        nsc := Level.NewSector;
        DuplicateSector(sc, nsc, 0, 0, 0, nx, ny, nz, x1, y1, z1);
        nsc.Assign(sec);
        Level.Sectors.Add(nsc);
        Level.RenumSecs;

        StartUndoRec('Create sector');
        SaveSecUndo(nsc, ch_added, sc_both);

        SectorAdded(nsc);
        SetCurSC(Level.Sectors.Count - 1);

    finally
        SetMouseMode(MM_Select);
        SCCreateStarted := false;
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
        case Mouse_mode of
            MM_Select:
                ;
            MM_Cleave:
                Do_StartCleave(X, Y, ssShift in Shift);
            MM_CreateSector:
                DO_StartCreateSC(X, Y, ssShift in Shift);
            mm_Ruler:
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
            Case Mouse_mode of
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
      if ((Map_mode = MM_TH) or (Map_mode = MM_FR)) and (ssCtrl in Shift) then
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
                //Invalidate;
            end;
      end;
    end
  else   // Zoom Map
    begin
        MousePos := ScreenToClient(MousePos);
        With renderer do
          begin
              var ny := renderer.zv;
              if not GetXYZonPlaneAt(MousePos.X, MousePos.Y, ny, CamX, CamY, CamZ,
                dx, dy, dz) then
                  exit;
              CamX := dx;
              CamY := dy;
              CamZ := dz;

              if WheelDelta > 0 then
                renderer.Scale := renderer.Scale / 1.5
              else
                renderer.Scale := Math.Min(renderer.Scale * 1.5, 3.5);  // max zoom out

              GetXYZonPlaneAt(MousePos.X, MousePos.Y, ny, CamX, CamY, CamZ, dx, dy, dz);
              CamX := dx;
              CamY := dy;
              CamZ := dz;

              Invalidate;
        end;
    end;
end;

procedure TJedMain.FormMouseMove(sender: TObject; Shift: TShiftState;
  X, Y: Integer);
var
    msX, msY: Integer;
    gX, gY, gZ: double;
begin
    Case Mouse_mode of
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
              X := GOrgXY.X
            else if ssCtrl in Shift then
              Y := GOrgXY.Y;
            Do_RotateCam(X, Y);
          end;
        MM_RectSelect:
            DO_ProceedRectSelect(X, Y);
    end;

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
        for var i := 0 to Level.Sectors.Count - 1 do
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
                Level.ImportAsc(FileName);
            end
            else
            begin
                Scale := 40;
                s := '40';
                if DFImport.ShowModal <> mrOK then
                    exit;
                Scale := DFImport.VScale.AsFloat;
                Level.ImportLev(FileName, Scale, DFImport.RGTX.ItemIndex);
            end;

            PMsg.Caption := '';
            LevelFile := 'Untitled';
            NewLevel := true;
            ResetEditor(true);
            Level.kind := CurrentProject;
            for i := 0 to Level.Things.Count - 1 do
            begin
                th := Level.Things[i];
                th.z := th.z - th.bbox.z1;
                nsec := FindSectorForThing(th);
                if nsec <> -1 then
                    th.sec := Level.Sectors[nsec];
                if th.sec = nil then
                    th.Layer := Level.AddLayer('Orphan things')
                else
                begin
                    s := Level.GetLayerName(th.sec.Layer);
                    s := 'Things' + Copy(s, 6, length(s));
                    th.Layer := Level.AddLayer(s);
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
    EnableMapGeoBackfaceCull(not MapGeoBackfaceCull);
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
                        sec := Level.Sectors[sc];
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
                        With Level.Sectors[sc].surfaces[SF] do
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
                        With Level.Sectors[sc].surfaces[SF] do
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
                        surf := Level.Sectors[sc].surfaces[SF];
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
    oldmode := Map_mode;
    Map_mode := mm;
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
  MapGeoBackfaceCull := enable;
  SetMapGeoBackfaceCulling.Caption := 'Disable Backface Cull';
  SetMapGeoBackfaceCulling.Hint := 'Disable map geometry back-face culling';
  if not MapGeoBackfaceCull then
    begin
      SetMapGeoBackfaceCulling.Caption := 'Enable Backface Cull';
      SetMapGeoBackfaceCulling.Hint := 'Enable map geometry back-face culling'
    end;

  if renderer <> nil then ReDraw;
end;

Procedure TJedMain.SetCurSC(sc: Integer);
begin
    Cur_SC := sc;
    if Cur_SC >= Level.Sectors.Count then
        Cur_SC := 0;
    if Cur_SC < 0 then
        Cur_SC := Level.Sectors.Count - 1;
    EditObject;
end;

Procedure TJedMain.SetCurSF(sc, SF: Integer);
begin
    Cur_SC := sc;
    Cur_SF := SF;
    if sc >= Level.Sectors.Count then
        Cur_SC := 0;
    if Cur_SC < 0 then
        Cur_SC := Level.Sectors.Count - 1;

    With Level.Sectors[Cur_SC] do
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
    if sc >= Level.Sectors.Count then
        Cur_SC := 0;
    if sc < 0 then
        Cur_SC := Level.Sectors.Count - 1;

    With Level.Sectors[Cur_SC] do
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
    if sc >= Level.Sectors.Count then
        Cur_SC := 0;
    if sc < 0 then
        Cur_SC := Level.Sectors.Count - 1;

    With Level.Sectors[Cur_SC] do
    begin
        if SF < 0 then
            Cur_SF := surfaces.Count - 1;
        if SF >= surfaces.Count then
            Cur_SF := 0;
    end;

    With Level.Sectors[Cur_SC].surfaces[Cur_SF] do
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
    if th >= Level.Things.Count then
        Cur_TH := 0;
    if th < 0 then
        Cur_TH := Level.Things.Count - 1;
    EditObject;
end;

Procedure TJedMain.SetCurFR(th, fr: Integer);
var
    thing: TJKThing;
begin
    Cur_TH := th;
    if th >= Level.Things.Count then
        Cur_TH := 0;
    if th < 0 then
        Cur_TH := Level.Things.Count - 1;
    thing := Level.Things[Cur_TH];

    if fr < -1 then
        Cur_FR := thing.Vals.Count - 1;
    if fr >= thing.Vals.Count then
        Cur_FR := -1;

    EditObject;
end;

Procedure TJedMain.SetCurLT(LT: Integer);
begin
    Cur_LT := LT;
    if LT >= Level.Lights.Count then
        Cur_LT := 0;
    if LT < 0 then
        Cur_LT := Level.Lights.Count - 1;
    EditObject;
end;

Procedure TJedMain.SetCurEX(EX: Integer);
begin
    Cur_EX := EX;
    if EX >= ExtraObjs.Count then
        Cur_EX := 0;
    if EX < 0 then
        Cur_EX := ExtraObjs.Count - 1;
    if Assigned(OnExtraSelect) then
        OnExtraSelect(ExtraObjs[Cur_EX]);
    EditObject;
end;

Procedure TJedMain.SaveJKLto(const name: string);
begin
    Level.SaveToJKL(name);
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

Procedure TJedMain.ResetEditor;
var
    i: Integer;
begin

    ClearUndoBuffer;
    ClearExtraObjs;
    UrqForm.Reload;

    if DefaultParams then
    begin
        With renderer do
        begin
            CamX := 0;
            CamY := 0;
            CamZ := 0;
            SetZ(0, 0, 1);
            SetX(1, 0, 0);

            rPch := 0;
            rYaw := 0;
            rRol := 0;

            GridX := 0;
            GridY := 0;
            GridZ := 0;
            SetGridNormal(0, 0, 1);
            SetGridXNormal(1, 0, 0);
            Scale := 0.5;
        end;

        Cur_SC := 0;
        Cur_SF := 0;
        Cur_TH := 0;
        Cur_VX := 0;
        Cur_LT := 0;
        ToolBar.CBGridStep.Text := DoubleToStr(DefGridStep);
        ToolBar.CBGridLine.Text := DoubleToStr(DefGridLine);
        ToolBar.CBGridDot.Text := DoubleToStr(DefGridDot);
        ToolBar.EnableSnapToGrid(DefSnapToGrid);
        ToolBar.CBGridSize.Text := DoubleToStr(DefGridSize);
    end;

    Preview3D.Reset; // call before SetMapMode
    LoadTemplates; // load before calling SetThingView to load correct templates for current ptoject

    if DefaultParams then
      begin
          SetMapMode(MM_SC);
          ToolBar.SetLVis(Level.LVisString);
          SetThingView(defThingView);
          SetMSelMode(DefMSelMode);
          scsel.Clear;
          sfsel.Clear;
          edsel.Clear;
          vxsel.Clear;
          thsel.Clear;
          ltsel.Clear;
      end
    else
      begin
          ToolBar.RefreshLayers(false);
          SetMapMode(Map_mode);
          VerifyMultiSelection;
      end;

    Changed := false;
    SetLevelName;
    CogForm.RefreshList;

    LoadPlugins;

    Preview3D.ReloadLevel;
    SyncRecents;

    SetProjectTypeIndicator;

    For i := 0 to Level.Things.Count - 1 do
        UpdateThingData(Level.Things[i]);

    SaveTimer.Enabled := AutoSave;
    SaveTimer.Interval := SaveInterval * 60 * 1000;
    SetHideLights(false);
    SetHideThings(false);
end;

Procedure TJedMain.SetLevelName;
var
    l: Integer;
begin
    var title := Format('SED - %s*', [LevelFile]);
    if not changed then
      SetLength(title, Length(title)-1); // remove '*'
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
    LT, nlt: TSedLight;
    nlts, i: Integer;
begin
    If not GetXYZAt(X, Y, lx, ly, lz) then
        exit;

    StartUndoRec('Add light(s)');

    if Level.Lights.Count = 0 then
    begin
        LT := Level.NewLight;
        LT.position.x := lx;
        LT.position.y := ly;
        LT.position.z := lz;
        Level.Lights.Add(LT);
        SaveLightUndo(LT, ch_added);
        { SaveSelLightsUndo('Add light',ch_added); }
        LevelChanged;
        exit;
    end;

    with Level.Lights[Cur_LT] do
    begin
        lx := lx - position.x;
        ly := ly - position.y;
        lz := lz - position.z;
    end;

    ltsel.AddLT(Cur_LT);

    nlts := Level.Lights.Count;

    for i := 0 to ltsel.Count - 1 do
    begin
        LT := Level.Lights[ltsel.getLT(i)];
        nlt := Level.NewLight;
        nlt.Assign(LT);
        nlt.position.x := LT.position.x + lx;
        nlt.position.y := LT.position.y + ly;
        nlt.position.z := LT.position.z + lz;
        Level.Lights.Add(nlt);
        SaveLightUndo(nlt, ch_added);
    end;

    ltsel.Clear;
    for i := nlts to Level.Lights.Count - 1 do
        ltsel.AddLT(i);

    SetCurLT(nlts);
    if ltsel.Count = 1 then
        ltsel.Clear;

    { SaveSelLightsUndo('Add light(s)',ch_added); }

    LevelChanged;
    Invalidate;
end;

Procedure TJedMain.AddThingsAt(X, Y: Integer);
var
    tx, ty, tz, thx, thy, thz, pch, yaw, rol: double;
    d: double;
    oldth, th: TJKThing;
    i, j, nths: Integer;
begin
    If not GetXYZAt(X, Y, tx, ty, tz) then
        exit;

    StartUndoRec('Add thing(s)');

    { Copy data from currently selcted thing }
    if Level.Things.Count = 0 then
    begin
        th := Level.NewThing;
        Level.Things.Add(th);
        SaveThingUndo(th, ch_added);
        ThingAdded(th);
        LayerThing(0);
        SaveThingUndo(th, ch_added);
        { SaveSelThingsUndo('Add thing',ch_added); }
        exit;
    end;

    With Level.Things[Cur_TH] do
    begin
        tx := tx - X;
        ty := ty - Y;
        tz := tz - z;
    end;
    thsel.AddTH(Cur_TH);

    if NewOnFloor and (thsel.Count = 1) then
        With Level.Things[Cur_TH] do
        begin
            tx := tx - bbox.x1;
            ty := ty - bbox.y1;
            tz := tz - bbox.z1;
        end;

    nths := Level.Things.Count;

    for i := 0 to thsel.Count - 1 do
    begin
        oldth := Level.Things[thsel.GetTH(i)];
        th := Level.NewThing;
        th.Assign(oldth);
        th.X := tx + oldth.X;
        th.Y := ty + oldth.Y;
        th.z := tz + oldth.z;

        for j := 0 to th.Vals.Count - 1 do
            With th.Vals[j] do
            begin
                if atype <> at_frame then
                    continue;
                GetFrame(thx, thy, thz, pch, yaw, rol);
                SetFrame(thx + tx, thy + ty, thz + tz, pch, yaw, rol);
            end;
        th.num := Level.Things.Count;
        Level.Things.Add(th);
        LayerThing(th.num);
        ThingAdded(th);
        SaveThingUndo(th, ch_added);
    end;

    thsel.Clear;

    for i := nths to Level.Things.Count - 1 do
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
begin
    If not GetXYZAt(X, Y, tx, ty, tz) then
        exit;
    en := renderer.gxnormal;
    en.dx := en.dx * 2;
    en.dy := en.dy * 2;
    en.dz := en.dz * 2;

    CreateCube(Level, tx, ty, tz, renderer.gnormal, en);
    SetCurSC(Level.Sectors.Count - 1);
    SectorAdded(Level.Sectors[Level.Sectors.Count - 1]);

    StartUndoRec('Add sector');
    SaveSecUndo(Level.Sectors[Level.Sectors.Count - 1], ch_added, sc_both);

    Invalidate;
end;

Procedure TJedMain.CopySectorsAt(X, Y: Integer);
var
    dx, dy, dz: double;
    i, j, fnew: Integer;
    sec, newsc: TJKSector;
    surf: TJKSurface;
begin
    If not GetXYZAt(X, Y, dx, dy, dz) then
        exit;
    sec := Level.Sectors[Cur_SC];

    WIth sec.vertices[0] do
    begin
        dx := dx - X;
        dy := dy - Y;
        dz := dz - z;
    end;

    scsel.AddSC(Cur_SC);

    fnew := Level.Sectors.Count;

    StartUndoRec('Copy sector(s)');

    for i := 0 to scsel.Count - 1 do
    begin
        sec := Level.Sectors[scsel.GetSC(i)];
        newsc := Level.NewSector;
        With renderer, sec.vertices[0] do
            DuplicateSector(sec, newsc, X, Y, z, gxnormal, gynormal, gnormal,
            //DuplicateSector(sec, newsc, X, Y, z, xv, yv, zv,   // <-- fixes new sector shape but is randomly rotated
              dx, dy, dz);
        Level.Sectors.Add(newsc);
        SaveSecUndo(newsc, ch_added, sc_both);
    end;

    Level.RenumSecs;
    scsel.Clear;

    for i := fnew to Level.Sectors.Count - 1 do
    begin
        sec := Level.Sectors[i];
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
begin
    scs := TSCMultisel.Create;

    try

        if sender = CalcLightOnLayers then
        begin
            for i := 0 to Level.Sectors.Count - 1 do
                if ToolBar.IsLayerVisible(Level.Sectors[i].Layer) then
                    scs.AddSC(i);
        end
        else if sender = CalcLightInSel then
        begin
            case Map_mode of
                MM_SC:
                    begin
                        for i := 0 to scsel.Count - 1 do
                            scs.AddSC(scsel.GetSC(i));
                    end;
                MM_SF:
                    begin
                        for i := 0 to sfsel.Count - 1 do
                        begin
                            sfsel.GetSCSF(i, sc, SF);
                            scs.AddSC(sc);
                        end;
                    end;
                MM_ED:
                    begin
                        for i := 0 to edsel.Count - 1 do
                        begin
                            edsel.GetSCSFED(i, sc, SF, a);
                            scs.AddSC(sc);
                        end;
                    end;
                MM_VX:
                    begin
                        for i := 0 to vxsel.Count - 1 do
                        begin
                            vxsel.GetSCVX(i, sc, a);
                            scs.AddSC(sc);
                        end;
                    end;
            else
                begin
                    ShowMessage
                      ('You must be in sector,surface,edge or vertex mode to use this option');
                    exit;
                end;
            end;
        end
        else
        begin
            for i := 0 to Level.Sectors.Count - 1 do
                scs.AddSC(i);
        end;

        ClearUndoBuffer;
        sms := GetMSecs;

        if NewLightCalc then
            CalcLightingNew(Level, scs)
        else
            CalcLighting(Level, scs);

        ems := GetMSecs;
        sms := SubMSecs(sms, ems);
        PanMessage(mt_info, Format('%d lights processed in %s',
          [Level.Lights.Count, StrMSecs(sms)]));

        if Level.Sectors.Count = scs.Count then
            Preview3D.ReloadLevel
        else
            for i := 0 to scs.Count - 1 do
                SectorChanged(Level.Sectors[scs.GetSC(i)]);

        LevelChanged;

    finally
        scs.free;
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
    case Map_mode of
        MM_SC:
            begin
                fi := Cur_SC;
                n := fi;
                repeat
                    if n >= Level.Sectors.Count - 1 then
                        n := 0
                    else
                        Inc(n);
                    if n = fi then
                        exit;
                until ToolBar.IsLayerVisible(Level.Sectors[n].Layer);

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
                    if n >= Level.Things.Count - 1 then
                        n := 0
                    else
                        Inc(n);
                    if n = fi then
                        exit;
                until ToolBar.IsLayerVisible(Level.Things[n].Layer);
                SetCurTH(n);
            end;
        MM_FR:
            begin
                th := Level.Things[Cur_TH];
                Cur_FR := th.NextFrame(Cur_FR);
                SetCurFR(Cur_TH, Cur_FR);
            end;
        MM_LT:
            begin
                fi := Cur_LT;
                n := fi;
                repeat
                    if n >= Level.Lights.Count - 1 then
                        n := 0
                    else
                        Inc(n);
                    if n = fi then
                        exit;
                until ToolBar.IsLayerVisible(Level.Lights[n].Layer);
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
    case Map_mode of
        MM_SC:
            begin
                fi := Cur_SC;
                n := fi;
                repeat
                    if n < 1 then
                        n := Level.Sectors.Count - 1
                    else
                        dec(n);
                    if n = fi then
                        exit;
                until ToolBar.IsLayerVisible(Level.Sectors[n].Layer);
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
                fi := Cur_TH;
                n := fi;
                repeat
                    if n < 1 then
                        n := Level.Things.Count - 1
                    else
                        dec(n);
                    if n = fi then
                        exit;
                until ToolBar.IsLayerVisible(Level.Things[n].Layer);
                SetCurTH(n);
            end;
        MM_FR:
            begin
                th := Level.Things[Cur_TH];
                if Cur_FR = -1 then
                    Cur_FR := th.Vals.Count - 1
                else
                    Cur_FR := th.PrevFrame(Cur_FR);
                SetCurFR(Cur_TH, Cur_FR);
            end;
        MM_LT:
            begin
                fi := Cur_LT;
                n := fi;
                repeat
                    if n < 1 then
                        n := Level.Lights.Count - 1
                    else
                        dec(n);
                    if n = fi then
                        exit;
                until ToolBar.IsLayerVisible(Level.Lights[n].Layer);
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
    case Map_mode of
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
    case Map_mode of
        MM_ED:
            begin
                surf := Level.Sectors[Cur_SC].surfaces[Cur_SF];
                if Cur_ED <= 0 then
                    SetCurED(Cur_SC, Cur_SF, surf.vertices.Count - 1)
                else
                    SetCurED(Cur_SC, Cur_SF, Cur_ED - 1);
            end;

    end;
end;

Function TJedMain.GetXYZAt(atX, atY: Integer; var X, Y, z: double): boolean;
begin
    Result := false;
    Result := renderer.GetGridAt(atX, atY, X, Y, z);
    if not Result then
        exit;
    if SnapToGrid then
        renderer.GetNearestGrid(X, Y, z, X, Y, z);
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
    Mouse_mode := mm;
    Screen.Cursor := CurSor;
end;

Procedure TJedMain.EditObject;
begin
    if not IsSelValid then
        exit;
    case Map_mode of
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
    case Map_mode of
        MM_SC:
            With Level.Sectors[Cur_SC].vertices[0] do
            begin
                cx := X;
                cy := Y;
                cz := z;
            end;
        MM_SF:
            With Level.Sectors[Cur_SC].surfaces[Cur_SF].vertices[0] do
            begin
                cx := X;
                cy := Y;
                cz := z;
            end;
        MM_ED:
            With Level.Sectors[Cur_SC].surfaces[Cur_SF].vertices[Cur_ED] do
            begin
                cx := X;
                cy := Y;
                cz := z;
            end;
        MM_VX:
            With Level.Sectors[Cur_SC].vertices[Cur_VX] do
            begin
                cx := X;
                cy := Y;
                cz := z;
            end;
        MM_TH:
            With Level.Things[Cur_TH] do
            begin
                cx := X;
                cy := Y;
                cz := z;
            end;
        MM_FR:
            if Cur_FR <> -1 then
                With Level.Things[Cur_TH].Vals[Cur_FR] do
                begin
                    GetFrame(cx, cy, cz, a, a, a);
                end
            else
                With Level.Things[Cur_TH] do
                begin
                    cx := X;
                    cy := Y;
                    cz := z;
                end;
        MM_LT:
            With Level.Lights[Cur_LT] do
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
    norm := renderer.gnormal;
    xnorm := renderer.gxnormal;
    case Map_mode of
        MM_SC:
            begin
                With Level.Sectors[Cur_SC].vertices[0] do
                begin
                    rx := X;
                    ry := Y;
                    rz := z;
                end;
            end;
        MM_SF:
            With Level.Sectors[Cur_SC].surfaces[Cur_SF] do
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
            With Level.Sectors[Cur_SC].surfaces[Cur_SF] do
            begin
                v1 := vertices[Cur_ED];
                v2 := vertices[NextVX(Cur_ED)];
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
        MM_VX:
            begin
                i := vxsel.AddVX(Cur_SC, Cur_VX);
                if vxsel.Count = 3 then
                begin
                    vxsel.GetSCVX(0, sc, VX);
                    v1 := Level.Sectors[sc].vertices[VX];
                    vxsel.GetSCVX(1, sc, VX);
                    v2 := Level.Sectors[sc].vertices[VX];
                    xnorm.SetCoords(v2.X - v1.X, v2.Y - v1.Y, v2.z - v1.z);
                    vxsel.GetSCVX(2, sc, VX);
                    v1 := Level.Sectors[sc].vertices[VX];

                    av.SetCoords(v1.X - v2.X, v1.Y - v2.Y, v1.z - v2.z);
                    VMult(xnorm.dx, xnorm.dy, xnorm.dz, av.dx, av.dy, av.dz,
                      norm.dx, norm.dy, norm.dz);
                end;
                vxsel.DeleteN(i);
                With Level.Sectors[Cur_SC].vertices[Cur_VX] do
                begin
                    rx := X;
                    ry := Y;
                    rz := z;
                end;
            end;
        MM_TH:
            With Level.Things[Cur_TH] do
            begin
                rx := X;
                ry := Y;
                rz := z;
            end;
        MM_FR:
            if Cur_FR = -1 then
                With Level.Things[Cur_TH] do
                begin
                    rx := X;
                    ry := Y;
                    rz := z;
                end
            else
                with Level.Things[Cur_TH].Vals[Cur_FR] do
                begin
                    GetFrame(rx, ry, rz, p, Y, r);
                end;
        MM_LT:
            With Level.Lights[Cur_LT] do
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
        GridX := rx;
        GridY := ry;
        GridZ := rz;
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
    if Map_mode <> MM_TH then
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
    thing := Level.Things[n];
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
        if IsVarChanged(wireframeAPI) or IsVarChanged(WF_DoubleBuf) then
            CreateRenderer;
    end;

    if MapRot = MR_old then
        GetPYR;
    Invalidate;
end;

procedure TJedMain.CancelMouseMode;
begin
    case Mouse_mode of
        MM_Cleave:
            begin
                Invalidate;
                CleaveStarted := false;
            end;
        MM_RectSelect:
            begin
                Invalidate;
                RectSelStarted := false;
            end;
        MM_Ruler:
            begin
                Invalidate;
                RulerStarted := false;
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
    if Map_mode <> MM_SF then
        SetMapMode(MM_SF);
    if not Preview3D.IsActive then
        exit;

    StartUndoRec('Scale texture');

    n := sfsel.AddSF(Cur_SC, Cur_SF);

    for i := 0 to sfsel.Count - 1 do
    begin
        sfsel.GetSCSF(i, sc, SF);
        ScaleSurf(Level.Sectors[sc].surfaces[SF]);
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
        SaveSecUndo(surf.sector, ch_changed, sc_geo);

        for i := 0 to surf.TXVertices.Count - 1 do
            With surf.TXVertices[i] do
            begin
                u := u + du;
                v := v + dv;
            end;
        SectorChanged(surf.sector);
    end;

begin
    if Map_mode <> MM_SF then
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

    StartUndoRec('Shift texture');

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
        du, dv: double;
        nu, nv, u, v: TVector;

    begin
        SaveSecUndo(surf.sector, ch_changed, sc_geo);

        fv := 0;
        if (surf.sector.num = Cur_SC) and (surf.num = Cur_SF) then
          case Map_mode of
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
        SectorChanged(surf.sector);
    end;
begin
    if not Preview3D.IsActive then
        exit;

    if (Map_mode <> MM_SF) and (Map_mode <> MM_ED) then
        SetMapMode(MM_SF);

    case how of
        st_left:
            begin
                cosa := cOS(TXRotStep * PI / 180);
                sina := SIN(TXRotStep * PI / 180);
            end;
        st_right:
            begin
                cosa := cOS(-TXRotStep * PI / 180);
                sina := SIN(-TXRotStep * PI / 180);
            end;
    end;

    StartUndoRec('Rotate texture');
    var n := sfsel.AddSF(Cur_SC, Cur_SF);

    for var i := 0 to sfsel.Count - 1 do
    begin
        sfsel.GetSCSF(i, sc, sf);
        RotateUV(Level.Sectors[sc].surfaces[sf], sina, cosa);
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

    case Map_mode of
        MM_SC:
            begin
                sec := Level.Sectors[Cur_SC];
                if axis <> rt_grid then
                    CalcSecCenter(sec, cx, cy, cz);
                StartUndoRec('Rotate sector(s)');
                i := scsel.AddSC(Cur_SC);
                RotateSectors(Level, scsel, vec, cx, cy, cz, angle);
                scsel.DeleteN(i);
            end;
        MM_SF:
            begin
                StartUndoRec('Rotate surface(s)');
                if axis <> rt_grid then
                    CalcSurfCenter(level.sectors[Cur_SC].surfaces[Cur_SF], cx, cy, cz);
                i := sfsel.AddSF(Cur_SC, Cur_SF);
                RotateSurfaces(Level, sfsel, vec, cx, cy, cz, angle);
                sfsel.DeleteN(i);
            end;
        MM_TH:
            begin
                th := Level.Things[Cur_TH];
                if axis <> rt_grid then
                begin
                    cx := th.X;
                    cy := th.Y;
                    cz := th.z;
                end;

                StartUndoRec('Rotate thing(s)');
                i := thsel.AddTH(Cur_TH);
                RotateThings(Level, thsel, vec, cx, cy, cz, angle, True);
                thsel.DeleteN(i);
            end;
        MM_LT:
            begin
                LT := Level.Lights[Cur_LT];
                if axis <> rt_grid then
                begin
                    cx := LT.position.x;
                    cy := LT.position.y;
                    cz := LT.position.z;
                end;

                StartUndoRec('Rotate light(s)');
                i := ltsel.AddLT(Cur_LT);
                RotateLights(Level, ltsel, vec, cx, cy, cz, angle);
                ltsel.DeleteN(i);
            end;
        MM_FR:
            begin
                if axis <> rt_grid then
                begin
                    if Cur_FR < 0 then
                        with Level.Things[Cur_TH] do
                        begin
                            cx := X;
                            cy := Y;
                            cz := z;
                        end
                    else
                        Level.Things[Cur_TH].Vals[Cur_FR].GetFrame(cx, cy,
                          cz, a, a, a);
                end;

                StartUndoRec('Rotate frame(s)');
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
    case Map_mode of
        MM_SC:
            begin
                StartUndoRec('Flip sector(s)');
                i := scsel.AddSC(Cur_SC);
                if how = rt_grid then
                    FlipSectorsOverPlane(Level, scsel, renderer.gnormal,
                      renderer.GridX, renderer.GridY, renderer.GridZ)
                else
                begin
                    CalcSecCenter(Level.Sectors[Cur_SC], cx, cy, cz);
                    StartUndoRec('Flip Sectors');
                    FlipSectors(Level, scsel, cx, cy, cz, how);
                end;
                scsel.DeleteN(i);
            end;
        MM_TH:
            begin
                StartUndoRec('Flip thing(s)');
                { SaveSelThingsUndo('Change thing(s)',ch_changed); }
                i := thsel.AddTH(Cur_TH);

                if how = rt_grid then
                    FlipThingsOverPlane(Level, thsel, renderer.gnormal,
                      renderer.GridX, renderer.GridY, renderer.GridZ)
                else
                    With Level.Things[Cur_TH] do
                        FlipThings(Level, thsel, X, Y, z, how);
                thsel.DeleteN(i);
            end;
        MM_LT:
            begin
                StartUndoRec('Flip light(s)');
                { SaveSelThingsUndo('Change thing(s)',ch_changed); }
                i := ltsel.AddLT(Cur_LT);

                With Level.Lights[Cur_LT] do
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
                            cx := renderer.GridX;
                            cy := renderer.GridY;
                            cz := renderer.GridZ;
                        end;
                end;
                FlipLightsOverPlane(Level, ltsel, vec, cx, cy, cz);
                ltsel.DeleteN(i);
            end;
        MM_FR:
            begin
                StartUndoRec('Flip frame(s)');
                { SaveSelThingsUndo('Change thing(s)',ch_changed); }
                i := frsel.AddFR(Cur_TH, Cur_FR);

                if Cur_FR < 0 then
                    With Level.Things[Cur_TH] do
                    begin
                        cx := X;
                        cy := Y;
                        cz := z;
                    end
                else
                    Level.Things[Cur_TH].Vals[Cur_FR].GetFrame(cx, cy,
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
                            cx := renderer.GridX;
                            cy := renderer.GridY;
                            cz := renderer.GridZ;
                        end;
                end;
                FlipFramesOverPlane(Level, frsel, vec, cx, cy, cz);
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

    case Map_mode of
        MM_SC:
            begin
                StartUndoRec('Scale sector(s)');
                CalcSecCenter(level.Sectors[Cur_SC], sd.cx, sd.cy, sd.cz);
                i := scsel.AddSC(Cur_SC);
                ScaleSectors(Level, scsel, sd, how and sc_ScaleTX <> 0);
                scsel.DeleteN(i);
            end;
        MM_SF:
            begin
                StartUndoRec('Scale surface(s)');
                CalcSurfCenter(level.sectors[Cur_SC].surfaces[Cur_SF], sd.cx, sd.cy, sd.cz);
                i := sfsel.AddSF(Cur_SC, Cur_SF);
                ScaleSurfaces(Level, sfsel, sd, how and sc_ScaleTX <> 0);
                sfsel.DeleteN(i);
            end;
        MM_TH:
            begin
                StartUndoRec('Scale thing(s)');
                i := thsel.AddTH(Cur_TH);
                With Level.Things[Cur_TH] do
                begin
                    sd.cx := X;
                    sd.cy := Y;
                    sd.cz := z;
                end;
                ScaleThings(Level, thsel, sd);
                thsel.DeleteN(i);
            end;
        MM_LT:
            begin
                StartUndoRec('Scale light(s)');
                i := ltsel.AddLT(Cur_LT);
                With Level.Lights[Cur_LT] do
                begin
                    sd.cx := position.x;
                    sd.cy := position.y;
                    sd.cz := position.z;
                end;
                ScaleLights(Level, ltsel, sd);
                ltsel.DeleteN(i);
            end;
        MM_FR:
            begin
                StartUndoRec('Scale light(s)');
                i := frsel.AddFR(Cur_TH, Cur_FR);
                if Cur_FR = -1 then
                    With Level.Things[Cur_TH] do
                    begin
                        sd.cx := X;
                        sd.cy := Y;
                        sd.cz := z;
                    end
                else
                    Level.Things[Cur_TH].Vals[Cur_FR].GetFrame(sd.cx, sd.cy,
                      sd.cz, a, a, a);
                ScaleFrames(Level, frsel, sd);
                frsel.DeleteN(i);
            end;

    end;
    Invalidate;
end;

Procedure TJedMain.TranslateObject(dx, dy, dz: double);
var
    sec: TJKSector;
    surf: TJKSurface;
    i: Integer;
begin
    case Map_mode of
        MM_SC:
            begin
                StartUndoRec('Translate sectors');
                sec := Level.Sectors[Cur_SC];
                TranslateSectors(Level, scsel, Cur_SC, dx, dy, dz);
            end;
        MM_SF:
            begin
                StartUndoRec('Translate surfaces');
                surf := Level.Sectors[Cur_SC].surfaces[Cur_SF];
                TranslateSurfaces(Level, sfsel, Cur_SC, Cur_SF, dx, dy, dz);
            end;
    end;
    Invalidate;
end;

Procedure TJedMain.StartStitch;
begin
    if Map_mode <> MM_SF then
        exit;
    stitch_Sec := Level.Sectors[Cur_SC];
    stitch_Surf := Level.Sectors[Cur_SC].surfaces[Cur_SF];
end;

Procedure TJedMain.DoStitch;
var
    ssurf, esurf: TJKSurface;
begin
    if Map_mode <> MM_SF then
        exit;
    if stitch_Sec = nil then
    begin
        PanMessage(mt_error, 'No start surface was set for stitching');
        exit;
    end;

    if (Level.Sectors.IndexOf(stitch_Sec) < 0) or
      (stitch_Sec.surfaces.IndexOf(stitch_Surf) < 0) then
    begin
        PanMessage(mt_error, 'The stitching start surface was deleted');
        exit;
    end;

    ssurf := stitch_Surf;
    esurf := Level.Sectors[Cur_SC].surfaces[Cur_SF];

    StartUndoRec('Stitch surfaces');
    if ssurf.Material <> '' then
        esurf.Material := ssurf.Material;
    StitchSurfaces(ssurf, esurf);

    SectorChanged(esurf.sector);
    StartStitch;
end;

Procedure TJedMain.StraightenTexture(zero, rot90: boolean);
var
    u, v: TVector;
    surf: TJKSurface;

    Procedure StrEd(surf: TJKSurface; ED: Integer);
    begin
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
            with surf.TXVertices[ED] do
            begin
                u := 0;
                v := 0;
            end;
        ArrangeTexture(surf, ED, u, v);
        SectorChanged(surf.sector);
    end;

    Procedure StrSurf(surf: TJKSurface);
    begin
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
            CalcUVNormals(surf, u, v);
        if zero then
            With surf.TXVertices[0] do
            begin
                u := 0;
                v := 0;
            end;
        ArrangeTexture(surf, 0, u, v);
        SectorChanged(surf.sector);
    end;

Var
    i, sc, SF, ED, n: Integer;

begin
    case Map_mode of
        MM_SF:
            begin
                StartUndoRec('Straighten surfaces');
                n := sfsel.AddSF(Cur_SC, Cur_SF);
                for i := 0 to sfsel.Count - 1 do
                begin
                    sfsel.GetSCSF(i, sc, SF);
                    StrSurf(Level.Sectors[sc].surfaces[SF]);
                end;
                sfsel.DeleteN(n);
            end;
        MM_ED:
            begin
                StartUndoRec('Straighten surfaces');
                n := edsel.addED(Cur_SC, Cur_SF, Cur_ED);
                for i := 0 to edsel.Count - 1 do
                begin
                    edsel.GetSCSFED(i, sc, SF, ED);
                    StrEd(Level.Sectors[sc].surfaces[SF], ED);
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
    SetRendfromPYR;

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

    case Map_mode of
        MM_SC:
            begin
                StartUndoRec('Translate Sector(s)');
                TranslateSectors(Level, scsel, Cur_SC, dx, dy, dz);
            end;
        MM_SF:
            begin
                StartUndoRec('Translate surface(s)');
                TranslateSurfaces(Level, sfsel, Cur_SC, Cur_SF, dx, dy, dz);
            end;
        { TranslateSurface(Level.Sectors[Cur_SC].Surfaces[Cur_SF],
          dx,dy,dz); }
        MM_VX:
            begin
                StartUndoRec('Translate vertices');
                TranslateVertices(Level, vxsel, Cur_SC, Cur_VX, dx, dy, dz);
            end;

        MM_ED:
            begin
                StartUndoRec('Translate Edge(s)');
                TranslateEdges(Level, edsel, Cur_SC, Cur_SF, Cur_ED,
                  dx, dy, dz);
            end;
        MM_TH:
            begin
                StartUndoRec('Translate thing(s)');
                { SaveSelThingsUndo('Translate thing(s)',ch_changed); }
                TranslateThings(Level, thsel, Cur_TH, dx, dy, dz, MoveFrames);
            end;
        MM_FR:
            begin
                StartUndoRec('Translate frame(s)');
                { SaveSelFramesUndo('Translate frame(s)',ch_changed); }
                if Cur_FR = -1 then
                    TranslateThingKeepFrame(Level, Cur_TH, dx, dy, dz)
                else
                    TranslateFrames(Level, frsel, Cur_TH, Cur_FR, dy, dy, dz);
            end;
        MM_LT:
            begin
                { SaveSelLightsUndo('Translate thing(s)',ch_changed); }
                StartUndoRec('Translate light(s)');
                TranslateLights(Level, ltsel, Cur_LT, dx, dy, dz);
            end;
    else
        exit;
    end;
    Invalidate;
end;

Function IsSCValid(sc: Integer): boolean;
begin
    Result := (sc >= 0) and (sc < Level.Sectors.Count);
end;

Function IsSFValid(sc, SF: Integer): boolean;
begin
    Result := false;
    if (sc < 0) or (sc >= Level.Sectors.Count) then
        exit;
    Result := (SF >= 0) and (SF < Level.Sectors[sc].surfaces.Count);
end;

Function IsEDValid(sc, SF, ED: Integer): boolean;
begin
    Result := false;
    if (sc < 0) or (sc >= Level.Sectors.Count) then
        exit;
    if (SF < 0) or (SF >= Level.Sectors[sc].surfaces.Count) then
        exit;
    Result := (ED >= 0) and
      (ED < Level.Sectors[sc].surfaces[SF].vertices.Count);
end;

Function IsVXValid(sc, VX: Integer): boolean;
begin
    Result := false;
    if (sc < 0) or (sc >= Level.Sectors.Count) then
        exit;
    Result := (VX >= 0) and (VX < Level.Sectors[sc].vertices.Count);
end;

Function IsTHValid(th: Integer): boolean;
begin
    Result := (th >= 0) and (th < Level.Things.Count);
end;

Function IsLTValid(LT: Integer): boolean;
begin
    Result := (LT >= 0) and (LT < Level.Lights.Count);
end;

Function IsFRValid(th, fr: Integer): boolean;
begin
    Result := false;
    if (th < 0) or (th >= Level.Things.Count) then
        exit;
    Result := (fr >= -1) and (fr < Level.Things[th].Vals.Count);
end;

Procedure TJedMain.VerifyMultiSelection;
var
    i, sc, SF, ED, VX, fr, n: Integer;
begin
    case Map_mode of
        MM_SC:
            For i := scsel.Count - 1 downto 0 do
                if not IsSCValid(scsel.GetSC(i)) then
                    scsel.DeleteN(i);
        MM_SF:
            For i := sfsel.Count - 1 downto 0 do
            begin
                sfsel.GetSCSF(i, sc, SF);
                if not IsSFValid(sc, SF) then
                    sfsel.DeleteN(i);
            end;
        MM_ED:
            For i := edsel.Count - 1 downto 0 do
            begin
                edsel.GetSCSFED(i, sc, SF, ED);
                if not IsEDValid(sc, SF, ED) then
                    edsel.DeleteN(i);
            end;
        MM_VX:
            For i := vxsel.Count - 1 downto 0 do
            begin
                vxsel.GetSCVX(i, sc, VX);
                if not IsVXValid(sc, VX) then
                    vxsel.DeleteN(i);
            end;
        MM_TH:
            For i := thsel.Count - 1 downto 0 do
                if not IsTHValid(thsel.GetTH(i)) then
                    thsel.DeleteN(i);
        MM_FR:
            For i := frsel.Count - 1 downto 0 do
            begin
                frsel.GetTHFR(i, n, fr);
                if not IsFRValid(n, fr) then
                    frsel.DeleteN(i);
            end;
        MM_LT:
            For i := ltsel.Count - 1 downto 0 do
                if not IsLTValid(ltsel.getLT(i)) then
                    ltsel.DeleteN(i);
    end;
end;

Procedure TJedMain.VerifySelection;
begin
    if not IsSelValid then
        SetMapMode(Map_mode);
end;

Function TJedMain.IsSelValid: boolean;
begin
    case Map_mode of
        MM_SC:
            Result := (Cur_SC >= 0) and (Cur_SC < Level.Sectors.Count);
        MM_SF:
            begin
                Result := false;
                if (Cur_SC < 0) or (Cur_SC >= Level.Sectors.Count) then
                    exit;
                Result := (Cur_SF >= 0) and
                  (Cur_SF < Level.Sectors[Cur_SC].surfaces.Count);
            end;
        MM_ED:
            begin
                Result := false;
                if (Cur_SC < 0) or (Cur_SC >= Level.Sectors.Count) then
                    exit;
                if (Cur_SF < 0) or
                  (Cur_SF >= Level.Sectors[Cur_SC].surfaces.Count) then
                    exit;
                Result := (Cur_ED >= 0) and
                  (Cur_ED < Level.Sectors[Cur_SC].surfaces[Cur_SF]
                  .vertices.Count);
            end;
        MM_VX:
            begin
                Result := false;
                if (Cur_SC < 0) or (Cur_SC >= Level.Sectors.Count) then
                    exit;
                Result := (Cur_VX >= 0) and
                  (Cur_VX < Level.Sectors[Cur_SC].vertices.Count);
            end;
        MM_TH:
            Result := (Cur_TH >= 0) and (Cur_TH < Level.Things.Count);
        MM_FR:
            begin
                Result := false;
                if (Cur_TH < 0) or (Cur_TH >= Level.Things.Count) then
                    exit;
                Result := (Cur_FR >= -1) and
                  (Cur_FR < Level.Things[Cur_TH].Vals.Count);
            end;
        MM_LT:
            Result := (Cur_LT >= 0) and (Cur_LT < Level.Lights.Count);
        MM_Extra:
            Result := (Cur_EX >= 0) and (Cur_EX < ExtraObjs.Count);
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
    case Map_mode of
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
    case Map_mode of
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
    thing_view := how;
    case how of
        cv_Dots:
            Dots.Checked := true;
        cv_Boxes:
            Boxes.Checked := true;
        cv_Wireframes:
            begin
                Wireframes.Checked := true;
                Progress.Reset(Level.Things.Count);
                Progress.msg := 'Loading 3DOs...';
                for i := 0 to Level.Things.Count - 1 do
                begin
                    UpdateThingData(Level.Things[i]);
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
    case thing_view of
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

    Templates.Merge(Level.templates);
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
        th.Vals.Add(tv);
        tv.name := name;
        tv.vtype := GetTPLVType(tv.name);
        tv.atype := GetTPLType(tv.name);
        tv.Val(vs);
    end;

begin
    if Map_mode <> MM_TH then
    begin
        ShowMessage('You must be in Thing mode');
        exit;
    end;
    th := Level.Things[Cur_TH];

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

    StartUndoRec('Make door');
    SaveThingUndo(th, ch_changed);

    for i := 0 to th.Vals.Count - 1 do
        th.Vals[i].free;
    th.Vals.Clear;

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
    Level.cogs.Add(cg);
    cf.LoadNoLocals(cg.name);
    for i := 0 to cf.Count - 1 do
    begin
        v := TCOgValue.Create;
        v.Assign(cf[i]);
        cg.Vals.Add(v);
        if i = 0 then
            v.Val(IntToStr(th.num))
        else
            v.Val(cf[i].AsString);
        v.Resolve;
    end;
    cf.free;
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
    if Map_mode <> MM_SC then
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
        mesh.name := Level.GetLayerName(sec.Layer);

        for i := nsel.Count - 1 downto 0 do
        begin
            csec := Level.Sectors[nsel.GetSC(i)];
            if csec.Layer <> sec.Layer then
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
    case Map_mode of
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
    case Map_mode of
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
    if (Map_mode = MM_SC) or (Map_mode = MM_SF) or (Map_mode = MM_TH) then
      Preview3D.Repaint;
end;

Procedure TJedMain.CleaveBy(const norm: TVector; X, Y, z: double);
var
    i: Integer;
    nsc, nsf: Integer;
begin
    case Map_mode of
        MM_SC:
            begin
                StartUndoRec('Cleave sector(s)');
                scsel.AddSC(Cur_SC);
                for i := 0 to scsel.Count - 1 do
                    { Cleaved:= }
                    CleaveSector(Level.Sectors[scsel.GetSC(i)], norm, X, Y, z);
                { updated in procedure }
                scsel.Clear;
            end;
        MM_SF:
            begin
                StartUndoRec('Cleave surface(s)');
                sfsel.AddSF(Cur_SC, Cur_SF);
                for i := 0 to sfsel.Count - 1 do
                begin
                    sfsel.GetSCSF(i, nsc, nsf);
                    { Cleaved:= }
                    CleaveSurface(Level.Sectors[nsc].surfaces[nsf],
                      norm, X, Y, z);
                end;
                sfsel.Clear;
            end;
        MM_ED:
            begin
                StartUndoRec('Cleave Edge');
                { CLeaved:= }
                CleaveEdge(Level.Sectors[Cur_SC].surfaces[Cur_SF], Cur_ED,
                  norm, X, Y, z);
            end;
    end;

end;

Procedure TJedMain.AddKBItem(mi: TMenuItem; const name: string; c: char;
  sc: TShiftState);
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
    Scale, cmx, cmy, cmz, grx, gry, grz: double;
    grdot, grline, grsize, grstep: double;
begin
    if renderer = nil then
      begin
          X.SetCoords(1, 0, 0);
          z.SetCoords(0, 0, 1);
          gX.SetCoords(1, 0, 0);
          gZ.SetCoords(0, 0, 1);
          Scale := 1;
          cmx := 0;
          cmy := 0;
          cmz := 0;
          grx := 0;
          gry := 0;
          grz := 0;
          grdot :=0.2;
          grstep := 0.2;
          grline := 1;
      end
    else
      begin
          X := renderer.xv;
          z := renderer.zv;
          gX := renderer.gxnormal;
          gZ := renderer.gnormal;
          Scale := renderer.Scale;
          cmx := renderer.CamX;
          cmy := renderer.CamY;
          cmz := renderer.CamZ;
          grx := renderer.GridX;
          gry := renderer.GridY;
          grz := renderer.GridZ;
          grdot := renderer.GridDot;
          grline := renderer.GridLine;
          grstep := renderer.GridStep;
          renderer.free;
          renderer := nil;
      end;
    try
        EnableMapGeoBackfaceCull(True);
        SetMapGeoBackfaceCulling.Visible := True;
        case wireframeAPI of
            WF_Software:
                renderer := TSFTRenderer.Create(Self.Handle);
            WF_OpenGL:
              begin
                renderer := TOGLRenderer.Create(Self.Handle);
                SetMapGeoBackfaceCulling.Visible := False; // backface culling doesn't work at the moment
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
    renderer.Scale := Scale;
    With renderer do
    begin
        CamX := cmx;
        CamY := cmy;
        CamZ := cmz;
        GridX := grx;
        GridY := gry;
        GridZ := grz;
        GridDot := grdot;
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
                if not sedplstd(GetSEDCOM) then
                    PanMessageFmt(mt_warning, 'Plug-in %s returned error while executing!', [dll]);
                exit;
            end;

            sedpl := GetProcAddress(hmod, 'SEDPluginLoad');
            if Assigned(sedpl) then
            begin
                if not sedpl(GetSEDCOM) then
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
    th := Level.Things[Cur_TH];
    if Cur_FR >= th.Vals.Count then
        exit;
    vl := th.Vals[Cur_FR];
    if CompareText(vl.name, 'frame') <> 0 then
        exit;

    SaveThingUndo(th, ch_changed);

    vl.free;
    th.Vals.Delete(Cur_FR);

    if Cur_FR >= th.Vals.Count then
        av := th.Vals[th.Vals.Count - 1]
    else
        av := th.Vals[Cur_FR];

    nv := th.NFrames;
    i := th.Vals.IndexOfName('numframes');

    if nv = 0 then
    begin
        if i <> -1 then
        begin
            th.Vals[i].free;
            th.Vals.Delete(i);
        end;
        exit;
    end;

    if i = -1 then
        th.InsertValue(0, 'numframes', IntToStr(nv))
    else
        th.Vals[i].Val(IntToStr(nv));

    nv := th.Vals.IndexOf(av);

    Cur_FR := -1;

    i := th.NextFrame(-1);

    While i <> -1 do
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
    If not GetXYZAt(X, Y, fx, fy, fz) then
        exit;

    th := Level.Things[Cur_TH];

    StartUndoRec('Add frame');
    SaveThingUndo(th, ch_changed);

    if Cur_FR = -1 then
        vl := th.AddValue('frame', Sprintf('(%.6f/%.6f/%.6f:%.6f/%.6f/%.6f)',
          [fx, fy, fz, th.pch, th.yaw, th.rol]))
    else
        vl := th.InsertValue(Cur_FR + 1, 'frame',
          Sprintf('(%.6f/%.6f/%.6f:%.6f/%.6f/%.6f)', [fx, fy, fz, th.pch,
          th.yaw, th.rol]));

    nv := th.NFrames;
    i := th.Vals.IndexOfName('numframes');
    if i = -1 then
        th.InsertValue(0, 'numframes', IntToStr(nv))
    else
        th.Vals[i].Val(IntToStr(nv));

    Cur_FR := th.Vals.IndexOf(vl);

    SetCurFR(Cur_TH, Cur_FR);

end;

procedure TJedMain.Edit1Click(sender: TObject);
var
    s: string;
begin
    s := GetUndoRecName;
    miUndo.Caption := '&Undo ' + s;
    miUndo.Enabled := s <> '';

    case Map_mode of
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
    case Map_mode of
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
        if not GetXYZAt(X, Y, rx, ry, rz) then
            exit;
    end;

    case Map_mode of
        MM_SC:
            begin
                if not CanPasteSectors then
                  exit;

                sec := Level.Sectors[Cur_SC];
                n := PasteSectors(Level, rx, ry, rz);
                if n = -1 then
                    exit;

                StartUndoRec('Paste sector(s)');

                SetCurSC(n);
                scsel.Clear;
                for i := n to Level.Sectors.Count - 1 do
                begin
                    nsc := Level.Sectors[i];
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
                    SectorAdded(Level.Sectors[i]);
                end;
                Invalidate;

            end;
        MM_LT:
            begin
                if not CanPasteLights then
                  exit;

                Layer := 0;
                if Level.Lights.Count > 0 then
                    Layer := Level.Lights[Cur_LT].Layer;

                StartUndoRec('Paste light(s)');

                n := PasteLights(Level, rx, ry, rz);
                if n = -1 then
                    exit;
                SetCurLT(n);
                ltsel.Clear;
                for i := n to Level.Lights.Count - 1 do
                begin
                    SaveLightUndo(Level.Lights[i], ch_added);
                    Level.Lights[i].Layer := Layer;
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

                Layer := 0;
                if Level.Things.Count > 0 then
                    Layer := Level.Things[Cur_TH].Layer;

                StartUndoRec('Paste things');

                n := PasteThings(Level, rx, ry, rz);
                if n = -1 then
                    exit;
                SetCurTH(n);
                thsel.Clear;
                for i := n to Level.Things.Count - 1 do
                begin
                    Level.Things[i].Layer := Layer;
                    SaveThingUndo(Level.Things[i], ch_added);
                    LayerThing(i);
                    ThingAdded(Level.Things[i]);
                    thsel.AddTH(i);
                end;
                { SaveSelThingsUndo('paste Thing(s)',ch_added); }
                Invalidate;
            end;
    end;
end;

procedure TJedMain.N3DPreviewtoItem1Click(sender: TObject);
var
    cx, cy, cz, cpch, cyaw, a: double;
begin
    cpch := 0;
    cyaw := 0;
    case Map_mode of
        MM_SC:
            FindCenter(Level.Sectors[Cur_SC], cx, cy, cz);
        { MM_VX: with Level.Sectors[Cur_SC].Vertices[Cur_VX] do begin cx:=x; cy:=y; cz:=z; end; }
        MM_TH:
            with Level.Things[Cur_TH] do
            begin
                cx := X;
                cy := Y;
                cz := z;
                cpch := pch;
                cyaw := yaw;
            end;
        MM_LT:
            with Level.Lights[Cur_LT] do
            begin
                cx := position.x;
                cy := position.y;
                cz := position.z;
            end;
        MM_FR:
            if Cur_FR = -1 then
                with Level.Lights[Cur_LT] do
                begin
                    cx := position.x;
                    cy := position.y;
                    cz := position.z;
                end
            else
                Level.Things[Cur_TH].Vals[Cur_FR].GetFrame(cx, cy, cz,
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
    msel_mode := mode;
    PMsel.hint := 'Current multiselction mode is ' + mname + ' Click to change';
end;

procedure TJedMain.PMselClick(sender: TObject);
begin
    case msel_mode of
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
    case msel_mode of
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
end;

Procedure TJedMain.DO_SelSF(sc, SF: Integer);
var
    i: Integer;
begin
    case msel_mode of
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
end;

Procedure TJedMain.DO_SelVX(sc, VX: Integer);
var
    i: Integer;
begin
    case msel_mode of
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
end;

Procedure TJedMain.DO_SelED(sc, SF, ED: Integer);
var
    i: Integer;
begin
    case msel_mode of
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
end;

Procedure TJedMain.DO_SelTH(th: Integer);
var
    i: Integer;
begin
    case msel_mode of
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
end;

Procedure TJedMain.DO_SelFR(th, fr: Integer);
var
    i: Integer;
begin
    if fr < 0 then
        exit;
    case msel_mode of
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
end;

Procedure TJedMain.DO_SelLT(LT: Integer);
var
    i: Integer;
begin
    case msel_mode of
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

Procedure TJedMain.SetRendfromPYR;
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
    //renderer.SetY(Y.dx, Y.dy, Y.dz);

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

Procedure sysGetPYR(const X, Y, z: TVector; var pch, yaw, rol: double);
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
        sysGetPYR(xv, yv, zv, rPch, rYaw, rRol);
end;


procedure TJedMain.SaveTimerTimer(sender: TObject);
var
    fname: string;
begin
    if ProjectDir = '' then
        exit;

    var ext := ExtractFileExt(LevelFile);
    If ((CompareText(ext, '.jed') <> 0)
    and (CompareText(ext, '.jkl') <> 0)
    and (CompareText(ext, '.ndy') <> 0)) or IsInContainer(LevelFile) then
        exit;

    ForceDirectories(ProjectDir + 'autosave');
    fname := ProjectDir + 'autosave\' + ExtractFileName(LevelFile);
    //Level.SaveToJed(fname);
    SaveToFile(fname, ext, (*setSaved:=*)False);

    PanMessage(mt_info, 'Project autosaved to ' + fname);
end;

Procedure TJedMain.UseInCog;
var
    ctype: TCOG_Type;
    obj: TObject;
    icog, ival: Integer;
begin
    case Map_mode of
        MM_SC:
            begin
                ctype := ct_sec;
                obj := Level.Sectors[Cur_SC];
            end;
        MM_SF:
            begin
                ctype := ct_srf;
                obj := Level.Sectors[Cur_SC].surfaces[Cur_SF];
            end;
        MM_TH:
            begin
                ctype := ct_thg;
                obj := Level.Things[Cur_TH];
            end;
    else
        exit;
    end;

    icog := -1;
    ival := -1;

    if ResPicker.PickCOGVal(icog, ival, ctype) then
    begin
        Level.cogs[icog].Vals[ival].obj := obj;
        CogForm.UpdateCOG(icog);
        PanMessageFmt(mt_info, 'Item is assigned to value %s in cog %s',
          [Level.cogs[icog].Vals[ival].name, Level.cogs[icog].name]);
    end;

end;

Function TJedMain.GetCurObjForCog(ct: TCOG_Type): TObject;
begin
    Result := nil;
    case Map_mode of
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
        SaveSecUndo(Level.Sectors[sc], change, how);
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
        SaveSecUndo(Level.Sectors[sc], change, sc_geo);
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
    if Map_mode <> MM_SF then
        exit;
    a := sfsel.AddSF(Cur_SC, Cur_SF);
    if sfsel.Count = 2 then
    begin
        StartUndoRec('Connect surfaces');
        sfsel.GetSCSF(0, sc1, sf1);
        sfsel.GetSCSF(1, sc2, sf2);
        if ConnectSurfaces(Level.Sectors[sc1].surfaces[sf1],
          Level.Sectors[sc2].surfaces[sf2]) then
            PanMessage(mt_info, 'The surfaces successfully connected');
        UpdateItemEditor;
    end;
    sfsel.DeleteN(a);
end;

Procedure TJedMain.ConnectSCs;
var
    a: Integer;
begin
    if Map_mode <> MM_SC then
        exit;
    a := scsel.AddSC(Cur_SC);
    if scsel.Count = 2 then
    begin
        StartUndoRec('Connect sectors');
        if ConnectSectors(Level.Sectors[scsel.GetSC(0)],
          Level.Sectors[scsel.GetSC(1)]) then
            PanMessage(mt_info, 'The sectors successfully connected');
        UpdateItemEditor;
    end;
    scsel.DeleteN(a);
end;

Procedure TJedMain.BringThingToSurf(th, sc, SF: Integer);
var
    surf: TJKSurface;
    thing: TJKThing;
    X, Y, z, l: double;
    xv, yv, zv: TVector;
    v1, v2: TJKVertex;
begin
    if (sc < 0) or (th < 0) or (sf < 0) then
      exit;

    StartUndoRec('Bring thing to Surface');
    surf := Level.Sectors[sc].surfaces[SF];
    CalcSurfCenter(surf, X, Y, z);
    thing := Level.Things[th];

    SaveThingUndo(thing, ch_changed);

    l := -thing.bbox.z1;
    X := X + surf.normal.dx * l;
    Y := Y + surf.normal.dy * l;
    z := z + surf.normal.dz * l;
    thing.X := X;
    thing.Y := Y;
    thing.z := z;

    v1 := surf.vertices[0];
    v2 := surf.vertices[1];
    zv := surf.normal;

    yv.SetCoords(v2.X - v1.X, v2.Y - v1.Y, v2.z - v1.z);
    Normalize(yv);
    VMult(yv.dx, yv.dy, yv.dz, zv.dx, zv.dy, zv.dz, xv.dx, xv.dy, xv.dz);

    GetJKPYR(xv, yv, zv, thing.pch, thing.yaw, thing.rol);

    ThingChanged(thing);
end;

Procedure TJedMain.BringLightToSurf(LT, sc, SF: Integer);
var
    surf: TJKSurface;
    light: TSedLight;
    x, y, z, l: double;
begin
    StartUndoRec('Bring light to Surface');
    surf := Level.Sectors[sc].surfaces[SF];
    CalcSurfCenter(surf, X, Y, z);
    X := X + surf.normal.dx * 0.001;
    Y := Y + surf.normal.dy * 0.001;
    z := z + surf.normal.dz * 0.001;
    light := Level.Lights[LT];
    SaveLightUndo(light, ch_changed);
    light.position.x := x;
    light.position.y := y;
    light.position.z := z;
    LevelChanged;
end;

Procedure TJedMain.AddThingAtSurf;
var
    th: TJKThing;
    n: Integer;
begin
    StartUndoRec('Add thing(s)');

    { Copy data from currently selcted thing }
    th := Level.NewThing;
    if Cur_TH < Level.Things.Count then
        th.Assign(Level.Things[Cur_TH]);
    n := Level.Things.Add(th);
    ThingAdded(th);
    Level.RenumThings;

    BringThingToSurf(n, Cur_SC, Cur_SF);

    LayerThing(n);
    SaveThingUndo(th, ch_added);
    SetCurTH(n);

end;

Procedure TJedMain.AddThingAtXYZPYR(X, Y, z, pch, yaw, rol: double);
var
    th: TJKThing;
    n: Integer;
begin
    StartUndoRec('Add thing(s)');

    { Copy data from currently selcted thing }
    th := Level.NewThing;
    if Cur_TH < Level.Things.Count then
        th.Assign(Level.Things[Cur_TH]);
    n := Level.Things.Add(th);
    ThingAdded(th);
    Level.RenumThings;

    th.X := X;
    th.Y := Y;
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
    case Map_mode of
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
    case Map_mode of
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
    for i := 0 to ExtraObjs.Count - 1 do
        ExtraObjs[i].free;
    ExtraObjs.Clear;
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
    Result := ExtraObjs.Add(v);
end;

Function TJedMain.AddExtraLine(x1, y1, z1, x2, y2, z2: double;
  const name: string): Integer;
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
    Result := ExtraObjs.Add(l);
end;

Procedure TJedMain.DeleteExtraObj(n: Integer);
begin
    ExtraObjs[n].free;
    ExtraObjs.Delete(n);
    if Cur_EX >= ExtraObjs.Count then
        Cur_EX := ExtraObjs.Count - 1;
    ExtraObjsName := '';
end;

Function TJedMain.TranslateExtras(Cur_EX: Integer; dx, dy, dz: double): Integer;
var
    j: Integer;
    EX: TObject;
begin
    if (Cur_EX < -1) or (ExtraObjs.Count = 0) then
      exit;

    EX := ExtraObjs[Cur_EX];
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
    LightsHidden := hide;
    if LightsHidden then
        HideLights.Caption := 'Show &Lights'
    else
        HideLights.Caption := 'Hide &Lights';
    Invalidate;
end;

Procedure TJedMain.SetHideThings(hide: boolean);
begin
    ThingsHidden := hide;
    if ThingsHidden then
        HideThings.Caption := 'Show &Things'
    else
        HideThings.Caption := 'Hide &Things';
    Invalidate;
end;

procedure TJedMain.HideThingsClick(sender: TObject);
begin
    SetHideThings(not ThingsHidden);
end;

procedure TJedMain.HideLightsClick(sender: TObject);
begin
    SetHideLights(not LightsHidden);
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
    if Map_mode <> MM_SC then exit;
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
    if Map_mode <> MM_SF then exit;
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

end.

