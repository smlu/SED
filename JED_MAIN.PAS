unit Jed_Main;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  ComCtrls, StdCtrls, Menus, GlobalVars, J_Level, misc_utils, ExtCtrls,
  Render, OGL_render, FileOperations, lev_utils, Geometry,
  Buttons, Math, Containers, Files, U_templates,Prefab, ListRes,
  sft_render, u_multisel, jed_plugins, values, shellApi, U_copypaste,
  FileCtrl, u_undo,JED_COM,tbar_tools, u_3dos;

Const
     {Map modes}
     MM_SC=0;
     MM_SF=1;
     MM_VX=2;
     MM_TH=3;
     MM_ED=4;
     MM_LT=5;
     MM_FR=6;
     MM_Extra=10;

     {Mouse Modes}
     MM_Select=0;
     MM_Cleave=1;
     MM_Drag=2;
     MM_TranslateCam=3;
     MM_RotateCam=4;
     MM_TranslateGrid=5;
     MM_RotateGrid=6;
     MM_CreateSector=7;
     MM_RectSelect=8;

     All_layers=-100;

     {Toolbar Panel IDs}
     PI_XYZ=0;

     {ShiftTexture constants}
     st_left=0;
     st_right=1;
     st_up=2;
     st_down=3;

     {SectorChanged() constants}
     sc_vertices=1;
     sc_values=2;
     sc_all=-1;
     {ScaleObject constants}
     sc_ScaleTX=1;
     sc_scaleX=2;
     sc_scaleY=4;
     sc_scaleZ=8;
     sc_scaleGrid=16;

     ro_up=0;
     ro_down=1;
     {Cursor constants}
     crSaber=1;

     {SetThingView constants}
      cv_Dots=0;
      cv_Boxes=1;
      cv_Wireframes=2;

     {OpenProject flags}
      op_open=0;
      op_revert=1;

      {Multiselection modes}
      mm_Toggle=0;
      mm_Add=1;
      mm_Subtract=2;


type

  TExtraLine=class
   v1,v2:TVertex;
   name:string;
   Constructor Create;
   Destructor Destroy;override;
  end;

  TExtraVertex=class(TVertex)
   name:string;
  end;

  TExtraPolygon=class(TIsolatedPolygon)
   name:string;
  end;

  TOnExtraMove=procedure(sender:TObject;continued:boolean) of object;

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
    SaveJKL: TSaveDialog;
    Export1: TMenuItem;
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
    SaveJed: TSaveDialog;
    Options1: TMenuItem;
    GobProject1: TMenuItem;
    SaveGOB: TSaveDialog;
    SaveJKL1: TMenuItem;
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
    procedure Exit1Click(Sender: TObject);
    procedure OpenMenuClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure Save1Click(Sender: TObject);
    procedure FormResize(Sender: TObject);
    procedure FormKeyDown(Sender: TObject; var Key: Word;
      Shift: TShiftState);
    procedure FormMouseDown(Sender: TObject; Button: TMouseButton;
      Shift: TShiftState; X, Y: Integer);
    procedure Import1Click(Sender: TObject);
    procedure FormPaint(Sender: TObject);
    procedure Export1Click(Sender: TObject);
    procedure PlaceCogs1Click(Sender: TObject);
    procedure FormMouseUp(Sender: TObject; Button: TMouseButton;
      Shift: TShiftState; X, Y: Integer);
    procedure FormMouseMove(Sender: TObject; Shift: TShiftState; X,
      Y: Integer);
    procedure CalculateLights1Click(Sender: TObject);
    procedure ConsistencyCheck1Click(Sender: TObject);
    procedure About1Click(Sender: TObject);
    procedure BNSCClick(Sender: TObject);
    procedure ItemEditor1Click(Sender: TObject);
    procedure FormKeyUp(Sender: TObject; var Key: Word;
      Shift: TShiftState);
    procedure MUNextObjectClick(Sender: TObject);
    procedure MUPrevObjectClick(Sender: TObject);
    procedure MUSnapGridToClick(Sender: TObject);
    procedure ToolWindow1Click(Sender: TObject);
    procedure FormShow(Sender: TObject);
    procedure Options1Click(Sender: TObject);
    procedure New1Click(Sender: TObject);
    procedure FormDestroy(Sender: TObject);
    procedure GobProject1Click(Sender: TObject);
    procedure SaveJKL1Click(Sender: TObject);
    procedure Viewtogrid1Click(Sender: TObject);
    procedure Toolbar1Click(Sender: TObject);
    procedure Topics1Click(Sender: TObject);
    procedure JedTutor1Click(Sender: TObject);
    procedure N3DPreview1Click(Sender: TObject);
    procedure SaveAs1Click(Sender: TObject);
    procedure PMsgDblClick(Sender: TObject);
    procedure Messages1Click(Sender: TObject);
    procedure SaveJKLGob1Click(Sender: TObject);
    procedure GridtoView1Click(Sender: TObject);
    procedure Find1Click(Sender: TObject);
    procedure FindNext1Click(Sender: TObject);
    procedure FormCloseQuery(Sender: TObject; var CanClose: Boolean);
    procedure Levelheadereditor1Click(Sender: TObject);
    procedure WireframesClick(Sender: TObject);
    procedure NewMOTSProject1Click(Sender: TObject);
    procedure PNProjTypeDblClick(Sender: TObject);
    procedure EpisodeEditor1Click(Sender: TObject);
    procedure CheckResources1Click(Sender: TObject);
    procedure ExportSectoras3DO1Click(Sender: TObject);
    procedure ReloadTemplates1Click(Sender: TObject);
    procedure MakeaBackupCopy1Click(Sender: TObject);
    procedure Reverttosaved1Click(Sender: TObject);
    procedure SaveJKLGOBandTest1Click(Sender: TObject);
    procedure Edit1Click(Sender: TObject);
    procedure miCopyClick(Sender: TObject);
    procedure miPasteClick(Sender: TObject);
    procedure N3DPreviewtoItem1Click(Sender: TObject);
    procedure PMselClick(Sender: TObject);
    procedure miToggleClick(Sender: TObject);
    procedure TemplateCreator1Click(Sender: TObject);
    procedure SaveTimerTimer(Sender: TObject);
    procedure miUndoClick(Sender: TObject);
    procedure SnapViewToObjectClick(Sender: TObject);
    procedure JumptoObject1Click(Sender: TObject);
    procedure HideThingsClick(Sender: TObject);
    procedure HideLightsClick(Sender: TObject);
    procedure ExportSectorasShape1Click(Sender: TObject);
    procedure TutorialsonMassassiNet1Click(Sender: TObject);
    procedure CutsceneHelper1Click(Sender: TObject);
    procedure N3DOHierarchy1Click(Sender: TObject);
  public
    {Project vars}
    AskSaveAs:boolean;
    NewLevel:boolean;
    LevelFile:String;
    Changed:boolean;
    updatestuff:boolean;

    snaptoy:boolean;

    {}
    SnapToGrid:boolean;
    renderer:TRenderer;
    ListID:Integer;
    hdc,hglc:integer;
    {scale:double;}
    Cur_SC,
    Cur_SF,
    Cur_VX,
    Cur_ED,
    Cur_LT,
    Cur_TH,
    Cur_FR,
    Cur_EX:Integer;
    Map_mode:integer;
    Mouse_mode:integer;
    msel_mode:integer;
    thing_view:integer;
    { Private declarations }
    {Select vars}
    LastSelXY:TPoint;
    csel:integer;
    {Drag variables}
    dragged:boolean;
    DragOrg:T3DPoint;
    DragOrgPos:T3DPoint;
    {Cleave vars}
    CleaveOrgXY:TPoint;
    CleaveOrg:T3DPoint;
    CleaveStarted:boolean;
    SCCreateStarted:boolean;
    LastX,LastY:Integer;
    {Rect select vars}
    RectOrgX,RectOrgY:Integer;
    RectLastX,RectLastY:Integer;
    RectSelStarted:boolean;

    {Grid translate vars}
    GOrgXY:TPoint;
    GOrg:T3DPoint;
    {Cam translate}
    CamOrg:TPoint;
    {Cam Rotate vars}
    OrgX,OrgY,OrgZ:TVector;
    {Stitch variables}
    stitch_Sec:TJKSector;
    stitch_Surf:TJKSurface;

    tcube:TPolygons;

    scsel:TSCMultisel;
    sfsel:TSFMultisel;
    vxsel:TVXMultisel;
    thsel:TTHMultisel;
    ltsel:TLTMultisel;
    edsel:TEDMultisel;
    frsel:TFRMultisel;
    exsel:TTHMultiSel;

    {Old style controls vars}
     rPch,rYaw,rRol:double;

    {Extra objects - for plugins}
    ExtraObjs:TObjList;
    ExtraObjsName:string;
    OnExtraSelect:TNotifyEvent;
    OnExtraMove:TOnExtraMove;
    {Hide thing/lights bools}
    ThingsHidden,LightsHidden:boolean;

    {3DO Hierarchy}

    Procedure WMEraseBkg(var msg:TMessage); message WM_ERASEBKGND;
    Procedure WMQueryPal(var msg:TMessage); message WM_QUERYNEWPALETTE;
    Procedure WMPalCHANGED(var msg:TMessage);message WM_PALETTECHANGED;
    procedure RecentClick(Sender: TObject);
    procedure KBCommandClick(Sender: TObject);
    Procedure AddKBItem(mi:TMenuItem;const name:string;c:char;sc:TShiftState);
    Function DoSaveJKL:boolean;
    Function GobProj(const name:string):boolean;
    procedure PluginClick(Sender: TObject);
    Procedure LoadPlugins;
    Procedure Import3DO(const name:string);

    procedure ExceptHandler(Sender: TObject; E: Exception);
    Procedure SetRendfromPYR;
    Procedure GetPYR;
    Function GetTargetJKLName:string;
    Procedure UseInCog;
    Procedure GotoXYZ(x,y,z:double;force:boolean);
    Procedure CenterViewOnObject(force:boolean);
    Procedure ConnectSFs;
    Procedure ConnectSCs;
    Procedure JumpToObject;
  public
    Procedure SaveSelThingsUndo(const rname:string;change:integer);
    Procedure SaveSelLightsUndo(const rname:string;change:integer);
    Procedure SaveSelFramesUndo(const rname:string;change:integer);

    Procedure SaveSelSurfUndo(const rname:string;change,how:integer);
    Procedure SaveSelSecUndo(const rname:string;change,how:integer);
    Procedure SaveSelVertUndo(const rname:string;change:integer);
    Procedure SaveSelEdgeUndo(const rname:string;change:integer);

    Procedure NewProject;
    Procedure OpenProject(const fname:string;how:integer);
    Function SaveProject:boolean;
    Function SysSaveProject(askSave:Boolean):boolean;
    Procedure SaveJKLto(const name:string);
    Procedure SaveJEDTo(const name:string);
    Procedure SnapGridToObject;
    Function GetXYZAt(atX,atY:integer;var X,Y,Z:double):boolean;
    Function GetMousePos(var X,Y:integer):boolean;

    Procedure GetCurObjRefXYZ(var rx,ry,rz:double);
    Procedure Do_SelectAt(X,Y:integer;fore:boolean);

    Procedure Do_StartCleave(X,Y:integer;snaptovertex:boolean);
    Procedure Do_ProceedCleave(X,Y:integer;snaptovertex:boolean);
    Procedure Do_EndCleave(X,Y:integer;snaptovertex:boolean);

    Procedure Do_StartDrag(X,Y:integer);
    Procedure Do_ProceedDrag(X,Y:integer;snaptovx:boolean;snaptoaxis,yaxis:boolean);
    Procedure Do_EndDrag(X,Y:Integer);

    Procedure Do_StartTranslateCam(X,Y:integer);
    Procedure Do_TranslateCam(X,Y:integer);

    Procedure Do_StartRotateCam(X,Y:integer);
    Procedure Do_RotateCam(X,Y:integer);

    Procedure Do_StartTranslateGrid(X,Y:integer);
    Procedure Do_TranslateGrid(X,Y:integer);

    Procedure Do_StartRotateGrid(X,Y:integer);
    Procedure Do_RotateGrid(X,Y:integer);

    Procedure DO_StartCreateSC(X,Y:integer;snaptovertex:boolean);
    Procedure DO_ProceedCreateSC(X,Y:integer;snaptovertex:boolean);
    Procedure DO_EndCreateSC(X,Y:integer;snaptovertex:boolean);

    Procedure DO_StartRectSelect(X,Y:integer);
    Procedure DO_ProceedRectSelect(X,Y:integer);
    Procedure DO_EndRectSelect(X,Y:integer);


    Procedure ResetEditor(DefaultParams:boolean);
    Procedure ResettingEditor;
    Procedure SetLevelName;
    Procedure ReDraw;
    Procedure DrawThing(th:TThing);
    Procedure DrawThingAt(th:TThing;x,y,z,pch,yaw,rol:single);
    Procedure DrawSelThing(th:TThing);
    Procedure DrawFrame(fr:TTPLValue);
    Procedure DrawSelFrame(th,fr:integer);

    Procedure SetMouseMode(mm:integer);
    Procedure SetMapMode(mm:integer);
    Procedure SetCurSC(SC:integer);
    Procedure SetCurSF(SC,SF:integer);
    Procedure SetCurVX(SC,VX:integer);
    Procedure SetCurED(SC,SF,ED:integer);
    Procedure SetCurLT(LT:integer);
    Procedure SetCurTH(TH:integer);
    Procedure SetCurFR(th,FR:integer);
    Procedure SetCurEX(EX:integer);
    Procedure AddLightsAt(x,y:integer);
    Procedure AddThingsAt(x,y:integer);
    Procedure AddSectorAt(X,Y:integer);
    Procedure AddFramesAt(X,Y:integer);
    Procedure CopySectorsAt(X,Y:integer);
    Procedure NextObject;
    Procedure PreviousObject;
    Procedure NextObjectInSurface;
    Procedure PreviousObjectInSurface;
    Procedure EditObject;
    Procedure GotoSC(sc:integer);
    Procedure GotoSF(sc,sf:integer);
    Procedure GotoTH(th:Integer);
    Function LayerThing(n:integer):boolean;
    Procedure CancelMouseMode;
    Procedure Goto_Adjoin;
    Procedure SetViewToGrid;
    Procedure SetGridToView;
    Procedure ShiftTexture(how:integer);
    Procedure RotateTexture(how:integer);
    Procedure ScaleTexture(how:integer);
    {Notification procedures}
    Procedure SectorChanged(s:TJKSector);
    Procedure SectorAdded(s:TJKSector);
    Procedure SectorDeleted(s:TJKSector);
    Procedure ThingChanged(th:TJKThing);
    Procedure ThingAdded(th:TJKThing);
    Procedure ThingDeleted(th:TJKThing);

    Procedure RotateObject(angle:double; axis:integer);
    Procedure ScaleObject(sfactor:double;how:integer);
    Procedure TranslateObject(dx,dy,dz:double);
    Procedure MakeDoor;
    Procedure StartStitch;
    Procedure DoStitch;
    Procedure StraightenTexture(zero,rot90:boolean);
    Procedure SetCam(X,Y,Z,PCH,YAW,ROL:Double);
    Procedure GetCam(var X,Y,Z,PCH,YAW,ROL:Double);
    Procedure RaiseObject(how:integer);
    Function IsSelValid:boolean;
    Procedure VerifySelection;
    Procedure VerifyMultiSelection;

    Procedure SyncRecents;
    Procedure AddRecent(const s:string);
    Procedure LevelChanged;
    Procedure UpdateItemEditor;
    Function AskSave:boolean;
    Procedure SetThingView(how:integer);
    Procedure SetMSelMode(mode:integer);
    Procedure UpdateThingData(th:TJKThing);
    Procedure LoadTemplates;
    Procedure SetMotsIndicator;
    Function ShortJKLName:string;
    Procedure DO_MultiSelect;
    Procedure ClearMultiSelection;
    Procedure CleaveBy(const norm:TVector;x,y,z:double);
    Procedure CreateRenderer;
    Procedure FlipObject(how:integer);
    procedure DeleteFrame(Cur_TH,Cur_FR:integer);
    Procedure DO_SelSC(sc:integer);
    Procedure DO_SelSF(sc,sf:integer);
    Procedure DO_SelVX(sc,vx:integer);
    Procedure DO_SelED(sc,sf,ed:integer);
    Procedure DO_SelTH(th:integer);
    Procedure DO_SelFR(th,fr:integer);
    Procedure DO_SelLT(lt:integer);

    Function GetCurObjForCog(ct:TCOG_Type):TObject;
    Procedure LayerThings;

    Procedure BringThingToSurf(th,sc,sf:integer);
    Procedure BringLightToSurf(lt,sc,sf:integer);
    Procedure AddThingAtSurf;
    Procedure ClearExtraObjs;
    Function AddExtraVertex(x,y,z:double;const name:string):integer;
    Function AddExtraLine(x1,y1,z1,x2,y2,z2:double;const name:string):integer;
    Function TranslateExtras(cur_ex:integer;dx,dy,dz:double):integer;
    Procedure DeleteExtraObj(n:integer);
    Procedure AddThingAtXYZPYR(x,y,z,pch,yaw,rol:double);
    Procedure SetHideLights(hide:boolean);
    Procedure SetHideThings(hide:boolean);
  end;

var
  JedMain: TJedMain;

Procedure LoadThing3DO(th:TJKThing;force:boolean);
Procedure sysGetPYR(const x,y,z:TVector;var pch,yaw,rol:double);
Procedure LoadDLLPlugin(const dll:string);


implementation

uses Item_edit, U_CogForm, Cons_checker, FileDialogs, U_Tools,
  U_Options, Jed_about1, ProgressDialog, U_tbar, U_Preview, U_msgForm,
  Q_Sectors, Q_surfs, Q_things, U_lheader, U_Medit, u_DFI,
  u_errorform, U_tplcreate, ResourcePicker, u_cscene, u_3doform,
  graph_files;



Procedure TJedMain.WMEraseBkg(var msg:TMessage);
begin
 msg.Result:=0;
end;

Procedure TJedMain.WMQueryPal(var msg:TMessage);
begin
 msg.Result:=Renderer.HandleWMQueryPal;
end;

Procedure TJedMain.WMPalCHANGED(var msg:TMessage);
begin
 msg.Result:=Renderer.HandleWMChangePal;
end;


{$R *.DFM}

procedure TJedMain.Exit1Click(Sender: TObject);
begin
 Application.Terminate;
end;

Procedure TJedMain.NewProject;
var sec:TJKSector;
    surf:TJKSurface;
    v:TJKVertex;
    th:TJKThing;
    lt:TJedLight;
    i:integer;
begin
 for i:=0 to Level.Things.Count-1 do
 Free3DO(Level.Things[i].a3DO);

 Level.Clear;
 sec:=Level.NewSector;
 Level.Sectors.Add(sec);

 v:=sec.newVertex; v.x:=-1; v.y:=1; v.z:=0;
 v:=sec.newVertex; v.x:=-1; v.y:=-1; v.z:=0;
 v:=sec.newVertex; v.x:=1; v.y:=-1; v.z:=0;
 v:=sec.newVertex; v.x:=1; v.y:=1; v.z:=0;

 v:=sec.newVertex; v.x:=-1; v.y:=1; v.z:=1;
 v:=sec.newVertex; v.x:=1; v.y:=1; v.z:=1;
 v:=sec.newVertex; v.x:=1; v.y:=-1; v.z:=1;
 v:=sec.newVertex; v.x:=-1; v.y:=-1; v.z:=1;

 With Sec do
 begin
 surf:=NewSurface; sec.surfaces.Add(surf); surf.surfflags:=surf.surfflags or SF_Floor;
 Surf.AddVertex(vertices[0]);Surf.AddVertex(vertices[1]);Surf.AddVertex(vertices[2]);Surf.AddVertex(vertices[3]);
 surf:=NewSurface; sec.surfaces.Add(surf);
 Surf.AddVertex(vertices[4]);Surf.AddVertex(vertices[5]);Surf.AddVertex(vertices[6]);Surf.AddVertex(vertices[7]);

 surf:=NewSurface; sec.surfaces.Add(surf);
 Surf.AddVertex(vertices[2]);Surf.AddVertex(vertices[6]);Surf.AddVertex(vertices[5]);Surf.AddVertex(vertices[3]);

 surf:=NewSurface; sec.surfaces.Add(surf);
 Surf.AddVertex(vertices[1]);Surf.AddVertex(vertices[7]);Surf.AddVertex(vertices[6]);Surf.AddVertex(vertices[2]);

 surf:=NewSurface; sec.surfaces.Add(surf);
 Surf.AddVertex(vertices[0]);Surf.AddVertex(vertices[4]);Surf.AddVertex(vertices[7]);Surf.AddVertex(vertices[1]);

 surf:=NewSurface; sec.surfaces.Add(surf);
 Surf.AddVertex(vertices[3]);Surf.AddVertex(vertices[5]);Surf.AddVertex(vertices[4]);Surf.AddVertex(vertices[0]);
 end;

 th:=Level.NewThing; Level.Things.Add(th);
 With th do
 begin
  x:=0; y:=0; z:=0.1;
  name:='walkplayer';
  sec:=Level.Sectors[0];
 end;

 lt:=Level.NewLight; Level.Lights.Add(lt);
 With lt do begin x:=0; y:=0; z:=0.5; end;

 for i:=0 to sec.surfaces.count-1 do
 sec.surfaces[i].RecalcAll;

 sec.Renumber;

 Level.SetDefaultHeader;

 AskSaveAs:=true;
 NewLevel:=true;
 LevelFile:='Untitled.jed';
 Level.AddMissingLayers;
 Level.MasterCMP:='';
 level.mots:=isMots;
end;

Procedure TJedMain.OpenProject(const fname:string;how:integer);
var ext:string;
    i:integer;
begin
 for i:=0 to Level.Things.Count-1 do
  Free3DO(Level.Things[i].a3DO);
 ext:=UpperCase(ExtractExt(FName));
 ResettingEditor;
 PMsg.Caption:='';
 if ext='.JKL' then
 begin
  Level.LoadFromJKL(FName);
  NewLevel:=false;
  LevelFile:=Fname;
  AskSaveAs:=IsInContainer(Fname);
  SetLevelName;
 end
 else if ext='.JED' then
 begin
  Level.LoadFromJED(FName);
  NewLevel:=false;
  LevelFile:=Fname;
  AskSaveAs:=IsInContainer(Fname);
  SetLevelName;
 end;
  SetMots(level.Mots);
  Level.JKLPostLoad;
  AddRecent(fname);
  ResetEditor(how<>op_revert);
  Invalidate;
end;

procedure TJedMain.SaveAs1Click(Sender: TObject);
begin
 SysSaveProject(true);
end;

Function TJedMain.SaveProject:boolean;
begin
 Result:=SysSaveProject(askSaveAs);
end;

Procedure TJedMain.SaveJEDTo(const name:string);
begin
  Level.LVisString:=ToolBar.GetLVisString;
  Level.SaveToJed(name);
  AskSaveAs:=false;
  NewLevel:=false;
  Changed:=false;
  LevelFile:=name;
  AddRecent(name);
  SetLevelName;
end;

Function TJedMain.SysSaveProject(askSave:Boolean):boolean;
var fpath,fname:string;
    t:TextFile;
begin
 Result:=false;
 if not askSave then
 begin
  fname:=ChangeFileExt(LevelFile,'.jed');
  BackUpFile(fname);
 end
 else
 begin
  SaveJed.FileName:=ChangeFileExt(ExtractName(LevelFile),'.jed');
  if not SaveJed.Execute then exit;
  fname:=SaveJed.FileName;
 end;
  fpath:=ExtractFilePath(fname);
  if not FileExists(fpath+'episode.jk') then
  begin
   AssignFile(t,fpath+'episode.jk');Rewrite(t);
   WriteLn(t,Format(EpisodeJKTpl,[ChangeFileExt(ExtractName(fname),'.jkl')]));
   CloseFile(t);
  end;
  SaveJedTo(fname);
  Result:=true;
end;


procedure TJedMain.OpenMenuClick(Sender: TObject);
begin
 if Not AskSave then exit;
 With GetFileOpen do
 begin
  Filter:='All acceptable files|*.JKL;*.JED;*.GOB;*.GOO|Jed files(*.JED)|*.JED|JK Files (*goo;*.gob;*.jkl)|*.GOO;*.GOB;*.JKL';
 If Execute then OpenProject(FileName,op_open);
 end;
end;

procedure TJedMain.Reverttosaved1Click(Sender: TObject);
begin
 if AskSaveAs then begin ShowMessage('The project wasn''t saved!'); exit; end;
 if ConfirmRevert and changed then
 if MsgBox('Revert To Saved?','Warning',MB_YESNO)<>ID_YES then exit;

 OpenProject(LevelFile,op_revert);
end;


Procedure BoxToPolys(const box:TThingBox;cube:TPolygons);
begin
  With Cube,box do
  begin
   With VXList[0] do begin x:=x1; y:=y2; z:=z1; end;
   With VXList[1] do begin x:=x1; y:=y1; z:=z1; end;
   With VXList[2] do begin x:=x2; y:=y1; z:=z1; end;
   With VXList[3] do begin x:=x2; y:=y2; z:=z1; end;

   With VXList[4] do begin x:=x1; y:=y2; z:=z2; end;
   With VXList[5] do begin x:=x2; y:=y2; z:=z2; end;
   With VXList[6] do begin x:=x2; y:=y1; z:=z2; end;
   With VXList[7] do begin x:=x1; y:=y1; z:=z2; end;
end;
end;


Procedure TJedMain.DrawThing(th:TThing);
begin
 With th do DrawThingAt(th,x,y,z,pch,yaw,rol);
end;

Procedure TJedMain.DrawThingAt(th:TThing;x,y,z,pch,yaw,rol:single);
var i:integer;
begin
 Renderer.DrawVertex(X,Y,Z);
 case thing_view of
  cv_dots: exit;
  cv_boxes:
  begin
    if th.bbox.x1=th.bbox.x2 then exit;
    BoxToPolys(th.bbox,tcube);
    Renderer.DrawPolygonsAt(tcube,x,y,z,pch,yaw,rol);
  end;
  cv_wireframes:
  begin
   if th.a3Do=nil then exit;
   for i:=0 to th.a3Do.Meshes.Count-1 do
    With th.a3Do.Meshes[i] do
    Renderer.DrawPolygonsAt(Faces,x,y,z,pch,yaw,rol);
  end;
 end;
end;


Procedure TJedMain.DrawSelThing(th:TThing);
var d:Tvector;
    v1,v2:TJKVertex;
    size:double;
    i:integer;
begin
  Renderer.SetCulling(r_CULLNONE);
  DrawThing(th);
  d.dx:=0; d.dy:=0.2; d.dz:=0;
  RotateVector(d,th.pch,th.yaw,th.rol);

  v1:=TJKVertex.Create;
  v2:=TJKVertex.Create;

  v1.x:=th.x; v1.y:=th.y; v1.z:=th.z;

  v2.x:=v1.x+d.dx;
  v2.y:=v1.y+d.dy;
  v2.z:=v1.z+d.dz;
  Renderer.DrawLine(v1,v2);
  v1.free; v2.free;
end;

Procedure TJedMain.DrawSelFrame(th,fr:integer);
var d:Tvector;
    v1,v2:TJKVertex;
    size:double;
    i:integer;
    thing:TJKThing;
    x,y,z,pch,yaw,rol:double;
begin
 if fr=-1 then exit;
 thing:=Level.Things[th];
 thing.Vals[fr].GetFrame(x,y,z,pch,yaw,rol);
 Renderer.DrawVertex(x,y,z);

 Renderer.SetCulling(r_CULLNONE);
 DrawThingAt(thing,x,y,z,pch,yaw,rol);
 d.dx:=0; d.dy:=0.2; d.dz:=0;
 RotateVector(d,pch,yaw,rol);

 v1:=TJKVertex.Create;
 v2:=TJKVertex.Create;

 v1.x:=x; v1.y:=y; v1.z:=z;

 v2.x:=v1.x+d.dx;
 v2.y:=v1.y+d.dy;
 v2.z:=v1.z+d.dz;
 Renderer.DrawLine(v1,v2);
 v1.free; v2.free;
end;


Procedure TJedMain.DrawFrame(fr:TTplValue);
var x,y,z,pch,yaw,rol:double;
begin
 if fr.atype<>at_frame then exit;
 fr.GetFrame(x,y,z,pch,yaw,rol);
 Renderer.DrawVertex(x,y,z);
end;

Procedure TJedMain.ReDraw;
var s,th:integer;
    v1,v2:TJKVertex;
    sec:TJKSector;
    surf:TJKSurface;
    Thing:TJKThing;
    light:TJedLight;
    en:TVector;
    dx,dy,dz,pch,yaw,rol:double;
    bbox,cbox:TBox;
    wx,wy,sc,sf,vx,ed,fr:integer;
    obj:TObject;
begin
 if not visible then exit;
try
 With clMapBack do Renderer.SetColor(CL_Background,r,g,b);
 Renderer.BeginScene;
 With clGrid do Renderer.SetColor(CL_Front,r,g,b);
 Renderer.SetPointSize(1);
 Renderer.DrawGrid;

 if map_mode=MM_VX then
 begin
  Renderer.SetPointSize(4);
  With clVertex do Renderer.SetColor(CL_Front,r,g,b);
 for s:=0 to Level.Sectors.Count-1 do
 begin
  sec:=level.Sectors[s];
  if Toolbar.IsLayerVisible(sec.Layer) then Renderer.DrawVertices(Sec.Vertices);
 end;
  Renderer.SetPointSize(1);
 end;

 With clMapGeoBack do Renderer.SetColor(CL_Back,r,g,b);
 Renderer.SetCulling(r_CULLBACK);
 for s:=0 to Level.Sectors.Count-1 do
 begin
  Sec:=level.Sectors[s];
  if Toolbar.IsLayerVisible(sec.Layer) then Renderer.DrawPolygons(sec.surfaces);
 end;

 With clMapGeo do Renderer.SetColor(CL_Front,r,g,b);
 Renderer.SetCulling(r_CULLFRONT);
 for s:=0 to Level.Sectors.Count-1 do
 begin
  Sec:=level.Sectors[s];
  if Toolbar.IsLayerVisible(sec.Layer) then Renderer.DrawPolygons(sec.surfaces);
 end;


 Renderer.SetCulling(r_CULLNONE);
 Renderer.SetPointSize(4);

if (not ThingsHidden) or (map_mode=MM_TH) then
begin
 With clThing do Renderer.SetColor(cl_front,r,g,b);
 for th:=0 to Level.Things.Count-1 do
 begin
  Thing:=Level.Things[th];
  if Toolbar.IsLayerVisible(Thing.Layer) then
    DrawThing(Thing);
 end;
end;

if (not LightsHidden) or (map_mode=MM_LT) then
begin
 With clLight do Renderer.SetColor(cl_front,r,g,b);
 For th:=0 to Level.Lights.Count-1 do
 begin
  light:=Level.Lights[th];
  if Toolbar.IsLayerVisible(Light.Layer) then With light do Renderer.DrawVertex(X,Y,Z);
 end;
end;

 if map_mode=MM_FR then
 begin
  With clFrame do Renderer.SetColor(cl_front,r,g,b);
  Renderer.SetPointSize(4);
  for th:=0 to Level.Things.Count-1 do
  begin
   Thing:=Level.Things[th];
   if not Toolbar.IsLayerVisible(Thing.Layer) then continue;
   With thing do Renderer.DrawVertex(x,y,z);
   for s:=0 to thing.vals.count-1 do DrawFrame(thing.vals[s]);

  end;
 end;

 {Draw extra stuff}
 With clExtra do Renderer.SetColor(cl_Front,r,g,b);
 Renderer.SetCulling(r_CULLNONE);
 for s:=0 to extraObjs.count-1 do
 begin
  obj:=extraObjs[s];
  if Obj is TVertex then With TJKVertex(obj) do Renderer.DrawVertex(x,y,z) else
  if Obj is TExtraLine then With TExtraLine(obj) do Renderer.DrawLine(v1,v2) else
  if Obj is TIsolatedPolygon then Renderer.DrawPolygon(TPolygon(obj));
 end;


 except
  On Exception do;
 end;

  {Draw multiselection}

 case map_mode of
  MM_SC: begin
          With clMSelBack do Renderer.SetColor(CL_Back,r,g,b);
          Renderer.SetCulling(r_CULLBACK);
          for s:=0 to scsel.Count-1 do
          begin
           Sec:=level.Sectors[scsel.GetSC(s)];
           Renderer.DrawPolygons(sec.surfaces);
          end;
          With clMSel do Renderer.SetColor(CL_Front,r,g,b);
          Renderer.SetCulling(r_CULLFRONT);
          for s:=0 to scsel.Count-1 do
          begin
           Sec:=level.Sectors[scsel.GetSC(s)];
           Renderer.DrawPolygons(sec.surfaces);
          end;
         end;
  MM_SF: begin
          With clMSelBack do Renderer.SetColor(CL_Back,r,g,b);
          Renderer.SetCulling(r_CULLBACK);
          for s:=0 to sfsel.Count-1 do
          begin
           sfsel.GetSCSF(s,sc,sf);
           surf:=Level.Sectors[sc].Surfaces[sf];
           Renderer.DrawPolygon(surf);
          end;

          With clMSel do Renderer.SetColor(CL_Front,r,g,b);
          Renderer.SetCulling(r_CULLFRONT);

          for s:=0 to sfsel.Count-1 do
          begin
           sfsel.GetSCSF(s,sc,sf);
           surf:=Level.Sectors[sc].Surfaces[sf];
           Renderer.DrawPolygon(surf);
          end;
         end;
  MM_ED: begin
          With clMSelBack do Renderer.SetColor(CL_Back,r,g,b);
          Renderer.SetCulling(r_CULLBACK);
          for s:=0 to edsel.Count-1 do
          begin
           edsel.GetSCSFED(s,sc,sf,ed);
           surf:=Level.Sectors[sc].Surfaces[sf];
           With Surf do
            Renderer.DrawLine(Vertices[ed],Vertices[NextVX(ed)]);
          end;
          With clMSel do Renderer.SetColor(CL_Front,r,g,b);
          Renderer.SetCulling(r_CULLFRONT);
          for s:=0 to edsel.Count-1 do
          begin
           edsel.GetSCSFED(s,sc,sf,ed);
           surf:=Level.Sectors[sc].Surfaces[sf];
           With Surf do
            Renderer.DrawLine(Vertices[ed],Vertices[NextVX(ed)]);
          end;
         end;
  MM_VX: begin
          With clMSel do Renderer.SetColor(CL_Front,r,g,b);
          {Renderer.SetCulling(r_CULLBACK);}
          Renderer.SetPointSize(4);
          for s:=0 to vxsel.Count-1 do
          begin
           vxsel.GetSCVX(s,sc,vx);
           With Level.Sectors[sc].Vertices[vx] do
            Renderer.DrawVertex(x,y,z);
          end;
         end;
  MM_TH: begin
          With clMSel do Renderer.SetColor(CL_Front,r,g,b);
          for s:=0 to thsel.count-1 do
           DrawThing(Level.Things[thsel.GetTH(s)]);
         end;
  MM_FR: begin
          With clMSel do Renderer.SetColor(CL_Front,r,g,b);
          for s:=0 to frsel.count-1 do
          begin
           frsel.GetTHFR(s,th,fr);
           if fr=-1 then continue;
           DrawFrame(Level.Things[TH].Vals[fr]);
          end;
         end;
  MM_LT: begin
          With clMSel do Renderer.SetColor(CL_Front,r,g,b);
          for s:=0 to ltsel.count-1 do
          With Level.Lights[ltsel.getLT(s)] do
           Renderer.DrawVertex(x,y,z);
         end;
 end;

{Draw Selection}
 With clMapSel do Renderer.SetColor(CL_Front,r,g,b);

 if IsSelValid then
 case Map_mode of
  MM_SC: begin
          sec:=level.Sectors[Cur_SC];

          if scsel.FindSC(Cur_SC)=-1 then
          With clMapSelBack do Renderer.SetColor(CL_Back,r,g,b)
          else With clSelMSelBack do Renderer.SetColor(CL_Back,r,g,b);

          Renderer.SetCulling(r_CULLBACK);
          Renderer.DrawPolygons(sec.surfaces);

          if scsel.FindSC(Cur_SC)=-1 then
          With clMapSel do Renderer.SetColor(CL_Front,r,g,b)
          else With clSelMSel do Renderer.SetColor(CL_Front,r,g,b);

          Renderer.SetCulling(r_CULLFRONT);
          Renderer.DrawPolygons(sec.surfaces);

          if sec.Vertices.Count>0 then
           With sec.Vertices[0] do Renderer.DrawVertex(x,y,z);
         end;
  MM_SF: begin
          if sfsel.FindSF(Cur_SC,Cur_SF)=-1 then
          With clMapSelBack do Renderer.SetColor(CL_Back,r,g,b)
          else With clSelMSelBack do Renderer.SetColor(CL_Back,r,g,b);

          Renderer.SetCulling(r_CULLBACK);
          Renderer.DrawPolygon(level.Sectors[Cur_SC].surfaces[Cur_SF]);

          if sfsel.FindSF(Cur_SC,Cur_SF)=-1 then
          With clMapSel do Renderer.SetColor(CL_Front,r,g,b)
          else With clSelMSel do Renderer.SetColor(CL_Front,r,g,b);

          Renderer.SetCulling(r_CULLFRONT);
          Renderer.DrawPolygon(level.Sectors[Cur_SC].surfaces[Cur_SF]);

          With level.Sectors[Cur_SC].surfaces[Cur_SF].Vertices[0] do Renderer.DrawVertex(x,y,z);
{          v1:=TJKVertex.Create;
          With level.Sectors[Cur_SC].surfaces[Cur_SF] do
          With Vertices[0] do
          begin
           v1.x:=x+normal.dx*0.2;
           v1.y:=y+normal.dy*0.2;
           v1.z:=z+normal.dz*0.2;
           Renderer.DrawLine(vertices[0],v1);
          end;
          v1.free;}
         end;
  MM_ED: With Level.Sectors[Cur_SC].Surfaces[Cur_SF] do
         begin

          if edsel.FindED(Cur_SC,Cur_SF,Cur_ED)=-1 then
          With clMapSel do Renderer.SetColor(CL_Front,r,g,b)
          else With clSelMSel do Renderer.SetColor(CL_Front,r,g,b);

          v1:=Vertices[Cur_ED];
          v2:=Vertices[NextVx(Cur_ED)];
          Renderer.DrawLine(v1,v2);
          Renderer.SetPointSize(4);
          Renderer.DrawVertex(v1.x,v1.y,v1.z);
          dx:=v2.x-v1.x; dy:=v2.y-v1.y; dz:=v2.z-v1.z;
          v2:=TJKVertex.Create;
          v2.x:=v1.x+dx*0.5; v2.y:=v1.y+dy*0.5; v2.z:=v1.z+dz*0.5;
          VMult(normal.dx,normal.dy,normal.dz,dx,dy,dz,en.dx,en.dy,en.dz);
          v1:=TJKVertex.Create;
          v1.x:=v2.x+en.dx*0.5; v1.y:=v2.y+en.dy*0.5; v1.z:=v2.z+en.dz*0.5;
          Renderer.DrawLine(v2,v1);
          v1.free; v2.free;
         end;
  MM_TH: begin
          if thsel.FindTH(Cur_TH)=-1 then
          With clMapSel do Renderer.SetColor(CL_Front,r,g,b)
          else With clSelMSel do Renderer.SetColor(CL_Front,r,g,b);

          DrawSelThing(Level.Things[Cur_TH]);
         end;
  MM_FR: if Cur_FR=-1 then with Level.Things[Cur_TH] do Renderer.DrawVertex(x,y,z)
         else
         begin
          if frsel.FindFR(Cur_TH,Cur_FR)=-1 then
          With clMapSel do Renderer.SetColor(CL_Front,r,g,b)
          else With clSelMSel do Renderer.SetColor(CL_Front,r,g,b);
          DrawSelFrame(Cur_TH,Cur_FR);
         end;
  MM_VX: begin
          if vxsel.FindVX(Cur_SC,Cur_VX)=-1 then
          With clMapSel do Renderer.SetColor(CL_Front,r,g,b)
          else With clSelMSel do Renderer.SetColor(CL_Front,r,g,b);

          With level.Sectors[Cur_SC].Vertices[Cur_VX] do
          Renderer.DrawVertex(x,Y,z);
         end;
  MM_LT: begin
          if ltsel.FindLT(Cur_LT)=-1 then
          With clMapSel do Renderer.SetColor(CL_Front,r,g,b)
          else With clSelMSel do Renderer.SetColor(CL_Front,r,g,b);

          if Cur_LT<Level.Lights.Count then
          With Level.Lights[Cur_LT] do
          begin
           Renderer.DrawVertex(x,y,z);
           Renderer.DrawCircle(x,y,z,range);
          end;
         end;

  MM_Extra:begin
          With clMSel do Renderer.SetColor(CL_Front,r,g,b);
          obj:=extraObjs[Cur_EX];
          if Obj is TVertex then With TJKVertex(obj) do Renderer.DrawVertex(x,y,z) else
          if Obj is TExtraLine then With TExtraLine(obj) do Renderer.DrawLine(v1,v2) else
          if Obj is TPolygon then Renderer.DrawPolygon(TPolygon(obj));
         end;
 end;
  Renderer.EndScene;
  ValidateRect(Handle,nil);
end;

procedure TJedMain.FormCreate(Sender: TObject);
var sec:TJKSector;
    surf:TJKSurface;
    vx:TJKVertex;
    p:TPolygon;
    v:Tvertex;
    i:integer;
begin
 Application.OnException:=ExceptHandler;

 MUSnapGridTo.Caption:=MUSnapGridTo.Caption+#9'Shft+S';
// ShortCut:=ShortCut(Ord('S'),[ssShift]);
 SnapViewToObject.ShortCut:=ShortCut(Ord('S'),[ssAlt]);

 CalcLightOnLayers.ShortCut:=ShortCut(Ord('L'),[ssCtrl,ssShift]);
 CalcLightInSel.ShortCut:=ShortCut(Ord('L'),[ssCtrl,ssAlt]);

 miCopy.Caption:='&Copy'#9'Ctrl+C';
 miPaste.Caption:='&Paste'#9'Ctrl+V';
 { Renderer.CamZ:=-25;}
 level.Sectors.Add(Level.NewSector);

 scsel:=TSCMultiSel.Create;
 sfsel:=TSFMultiSel.Create;
 vxsel:=TVXMultiSel.Create;
 thsel:=TTHMultiSel.Create;
 ltsel:=TLTMultiSel.Create;
 edsel:=TEDMultiSel.Create;
 frsel:=TFRMultiSel.Create;
 exsel:=TTHMultiSel.Create;

 ExtraObjs:=TObjList.Create;


 SetWinPos(Self,MWinPos);

 if MWMaxed then WindowState:=wsMaximized;

 tcube:=TPolygons.Create;
 tcube.VXList:=Tvertices.Create;
 for i:=1 to 8 do tcube.VXList.Add(TVertex.Create);

 With TCube do
 begin
  p:=TPolygon.Create; Add(p);
  p.AddVertex(VXList[0]);p.AddVertex(VXList[1]);p.AddVertex(VXList[2]);p.AddVertex(VXList[3]);
  p:=TPolygon.Create; Add(p);
  p.AddVertex(VXList[4]);p.AddVertex(VXList[5]);p.AddVertex(VXList[6]);p.AddVertex(VXList[7]);
  p:=TPolygon.Create; Add(p);
  p.AddVertex(VXList[2]);p.AddVertex(VXList[6]);p.AddVertex(VXList[5]);p.AddVertex(VXList[3]);
  p:=TPolygon.Create; Add(p);
  p.AddVertex(VXList[1]);p.AddVertex(VXList[7]);p.AddVertex(VXList[6]);p.AddVertex(VXList[2]);
  p:=TPolygon.Create; Add(p);
  p.AddVertex(VXList[0]);p.AddVertex(VXList[4]);p.AddVertex(VXList[7]);p.AddVertex(VXList[1]);
  p:=TPolygon.Create; Add(p);
  p.AddVertex(VXList[3]);p.AddVertex(VXList[5]);p.AddVertex(VXList[4]);p.AddVertex(VXList[0]);
end;
Screen.Cursors[crSaber]:=LoadCursor(HInstance,PChar('SABER'));

 AddKBItem(miMap,'Top view','1',[]);
 AddKBItem(miMap,'Front view','2',[]);
 AddKBItem(miMap,'Left view','3',[]);
 AddKBItem(miMap,'Bottom view','4',[]);
 AddKBItem(miMap,'Back view','5',[]);
 AddKBItem(miMap,'Right view','6',[]);

 AddKBItem(miMap,'Move left',char(VK_RIGHT),[]);
 AddKBItem(miMap,'Move right',char(VK_LEFT),[]);
 AddKBItem(miMap,'Move Down',char(VK_UP),[]);
 AddKBItem(miMap,'Move Up',char(VK_Down),[]);
 AddKBItem(miMap,'Move forth',char(VK_NEXT),[]);
 AddKBItem(miMap,'Move back',char(VK_PRIOR),[]);

 AddKBItem(miMap,'Rotate right',char(VK_RIGHT),[ssShift]);
 AddKBItem(miMap,'Rotate left',char(VK_LEFT),[ssShift]);
 AddKBItem(miMap,'Rotate Up',char(VK_UP),[ssShift]);
 AddKBItem(miMap,'Rotate Down',char(VK_Down),[ssShift]);
 AddKBItem(miMap,'Rotate ...',char(VK_NEXT),[ssShift]);
 AddKBItem(miMap,'Rotate ...',char(VK_PRIOR),[ssShift]);
 AddKBItem(miMap,'Zoom at cursor','C',[ssShift]);
 AddKBItem(MiMap,'Zoom in',Chr(VK_ADD),[]);
 AddKBItem(MiMap,'Zoom out',Chr(VK_SUBTRACT),[]);
 AddKBItem(MiMap,'View To Grid',Chr(VK_MULTIPLY),[]);

 AddKBItem(miGrid,'Top view&&grid','1',[ssShift]);
 AddKBItem(miGrid,'Front view&&grid','2',[ssShift]);
 AddKBItem(miGrid,'Left view&&grid','3',[ssShift]);
 AddKBItem(miGrid,'Bottom view&&grid','4',[ssShift]);
 AddKBItem(miGrid,'Back view&&grid','5',[ssShift]);
 AddKBItem(miGrid,'Right view&&grid','6',[ssShift]);

 AddKBItem(miGrid,'Snap grid to item','S',[ssShift]);
 AddKBItem(miGrid,'Snap grid to item','W',[ssShift]);
 AddKBItem(MiGrid,'Move Grid'#9'G+Mouse',#0,[]);
 AddKBItem(MiGrid,'Swap Grid axis','G',[ssShift]);

 AddKBItem(MiSel,'Next item','N',[]);
 AddKBItem(MiSel,'Prev item','P',[]);
 AddKBItem(MiSel,'Next edge in surface','N',[ssShift]);
 AddKBItem(MiSel,'Prev edge in surface','P',[ssShift]);
 AddKBItem(MiSel,'Goto Adjoin','F',[ssShift]);
 AddKBItem(MiSel,'Clear multiselection',Chr(VK_BACK),[]);
 AddKBItem(MiSel,'Multselect item',' ',[]);
 AddKBItem(MiSel,'Rectangle Multselect'#9'Alt+Mouse',#0,[]);

 AddKBItem(MiTex,'Scroll up',#188,[ssShift]);
 AddKBItem(MiTex,'Scroll down',#190,[ssShift]);

 AddKBItem(MiTex,'Scroll left',#188,[]);
 AddKBItem(MiTex,'Scroll right',#190,[]);

 AddKBItem(MiTex,'Start stitch',Char(VK_INSERT),[ssCtrl]);
 AddKBItem(MiTex,'Start stitch',#186,[]);

 AddKBItem(MiTex,'Stitch',char(VK_INSERT),[ssShift]);
 AddKBItem(MiTex,'Stitch',#222,[]);

 AddKBItem(MiTex,'Straighten Texture',char(VK_HOME),[ssAlt]);
 AddKBItem(MiTex,'Straighten Texture',#191,[]);
 AddKBItem(MiTex,'Scale texture down',#188,[ssAlt]);
 AddKBItem(MiTex,'Scale texture Up',#190,[ssAlt]);
 AddKBItem(MiTex,'Straighten/zero/rotate90 texture',#191,[ssShift,ssCtrl]);
 AddKBItem(MiTex,'Straighten/zero/rotate90 texture',Char(VK_HOME),[ssShift,ssCtrl]);
 AddKBItem(MiTex,'Straighten/zero texture',#191,[ssCtrl]);
 AddKBItem(MiTex,'Straighten/zero texture',Char(VK_HOME),[ssCtrl]);
 AddKBItem(MiTex,'Rotate texture left',#188,[ssCtrl]);
 AddKBItem(MiTex,'Rotate texture right',#190,[ssCtrl]);


 AddKBItem(miRecovery,'Build surface/sector','B',[ssAlt]);
 AddKBItem(miRecovery,'Delete surface',Char(VK_DELETE),[ssAlt]);
 AddKBItem(miRecovery,'Invert surface','I',[ssAlt]);
 AddKBItem(miRecovery,'Planarize surface','P',[ssCtrl]);

 AddKBItem(miEdit,'Move item'#9'Ctrl+Mouse',#0,[]);
 AddKBItem(miEdit,'Start cleave','C',[]);
 AddKBItem(miEdit,'Cleave by grid','C',[ssAlt]);
 AddKBItem(miEdit,'Assign Thing to Sector','A',[ssShift]);
 AddKBItem(miEdit,'Unadjoin','A',[ssAlt]);
 AddKBItem(miEdit,'Adjoin','A',[]);
 AddKBItem(miEdit,'Join surfaces','J',[]);

 AddKBItem(miEdit,'Raise item',char(VK_UP),[ssCtrl]);
 AddKBItem(miEdit,'Lower item',char(VK_Down),[ssCtrl]);
 AddKBItem(miEdit,'Raise item',#219,[]);
 AddKBItem(miEdit,'Lower item',#221,[]);
 AddKBItem(miEdit,'Item Editor',Chr(VK_return),[]);
 AddKBItem(miEdit,'Merge','M',[]);
 AddKBItem(miEdit,'Duplicate item',Char(VK_INSERT),[]);
 AddKBItem(miEdit,'Delete item',Char(VK_DELETE),[]);

 AddKBItem(miEdit,'Create sector','K',[]);

 AddKBItem(miEdit,'Extrude surface','X',[]);
 AddKBItem(miEdit,'Extrude surface by','X',[ssShift]);
 AddKBItem(miEdit,'Extrude&&Expand surface by','X',[ssCtrl]);
 AddKBItem(miEdit,'Use item in COG','U',[]);
 AddKBItem(miEdit,'Bring thing/light to surface','B',[]);

 AddKBItem(miOther,'Cancel cleave/etc',Chr(VK_ESCAPE),[]);
 AddKBItem(miOther,'Sector mode','S',[]);
 AddKBItem(miOther,'Surface mode','F',[]);
 AddKBItem(miOther,'Edge mode','E',[]);
 AddKBItem(miOther,'Vertex mode','V',[]);
 AddKBItem(miOther,'Thing mode','T',[]);
 AddKBItem(miOther,'Thing frame mode','T',[ssShift]);
 AddKBItem(miOther,'Light mode','L',[]);
 AddKBItem(miOther,'Toggle Snap To X/Y','Z',[ssCtrl,ssAlt]);

end;

procedure TJedMain.Save1Click(Sender: TObject);
begin
 SaveProject;
end;

procedure TJedMain.FormResize(Sender: TObject);
begin
 if renderer<>nil then Renderer.SetViewPort(0,0,ClientWidth,Clientheight);
end;


Procedure RotAxisAngle(var x,y:TVector;ang:double);
var sina,cosa:double;
    nx,ny:TVector;
begin
 cosa:=cOS(ang/180*PI);
 sina:=SIN(ang/180*PI);

{ cosa:=cos(ang/180*pi);
 sina:=sin(ang/180*pi);}

 nx.dx:=cosa*x.dx+sina*y.dx;
 nx.dy:=cosa*x.dy+sina*y.dy;
 nx.dz:=cosa*x.dz+sina*y.dz;

 ny.dx:=-sina*x.dx+cosa*y.dx;
 ny.dy:=-sina*x.dy+cosa*y.dy;
 ny.dz:=-sina*x.dz+cosa*y.dz;

 x:=nx;
 y:=ny;

end;

Procedure RotAxis(var x,y:TVector;dir:integer);
begin
 Case dir of
  1: RotAxisAngle(x,y,5);
 -1: RotAxisAngle(x,y,-5);
 end;
end;

procedure TJedMain.FormKeyDown(Sender: TObject; var Key: Word;
  Shift: TShiftState);
var  th:TJKThing;
     i,a,nth,nfr,x,y:integer;
     n,xn:TVector;
     dx,dy,dz:double;
     nx,ny,nz:TVector;
     surf,surf1:TJKSurface;
     sec,sec1:TJKSector;
     mx:TMat3x3;
     nsc,nsf:integer;
     nsel:integer;
     s:string;
begin
 if (key=VK_MENU) or (key=VK_LMENU) then exit;

 if Shift=[ssCtrl,ssAlt] then
 case key of
  ord('Z'): snaptoy:=not snaptoy; 
 end;

 if Shift=[ssShift] then
 begin
 case key of
  Ord('1')..Ord('6'):
  if MapRot=MR_old then
  begin
   With Renderer do
   Case Char(key) of
    '1': begin rPCH:=0; rYaw:=0; rRol:=0; SetGridNormal(0,0,1); SetGridXNormal(1,0,0); end;
    '2': begin rPCH:=-90; rYaw:=180; rRol:=0; SetGridNormal(0,1,0); SetGridXNormal(-1,0,0); end;
    '3': begin rPCH:=90; rYaw:=0; rRol:=90; SetGridNormal(1,0,0); SetGridXNormal(0,1,0); end;
    '4': begin rPCH:=0; rYaw:=180; rRol:=0; SetGridNormal(0,0,-1); SetGridXNormal(-1,0,0); end;
    '5': begin rPCH:=90; rYaw:=0; rRol:=0; SetGridNormal(0,-1,0); SetGridXNormal(1,0,0); end;
    '6': begin rPCH:=0; rYaw:=-90; rRol:=-90; SetGridNormal(-1,0,0); SetGridXNormal(0,-1,0); end;
   end;
   SetRendFromPYR;
   Invalidate;
  end
  else
  With Renderer do
  Case char(key) of
   '1': begin SetZ(0,0,1); SetX(1,0,0); SetGridNormal(0,0,1); SetGridXNormal(1,0,0); end;
   '2': begin SetZ(0,1,0); SetX(-1,0,0); SetGridNormal(0,1,0); SetGridXNormal(-1,0,0); end;
   '3': begin SetZ(1,0,0); SetX(0,1,0); SetGridNormal(1,0,0); SetGridXNormal(0,1,0); end;
   '4': begin SetZ(0,0,-1); SetX(-1,0,0); SetGridNormal(0,0,-1); SetGridXNormal(-1,0,0); end;
   '5': begin SetZ(0,-1,0); SetX(1,0,0); SetGridNormal(0,-1,0); SetGridXNormal(1,0,0); end;
   '6': begin SetZ(-1,0,0); SetX(0,-1,0); SetGridNormal(-1,0,0); SetGridXNormal(0,-1,0); end;
  end;
  Ord('C'): if GetMousePos(X,Y) then
            With Renderer do
            begin
              ny:=Renderer.zv;
              if not GetXYZonPlaneAt(X,Y,ny,CamX,CamY,CamZ,dx,dy,dz) then exit;
              CamX:=-dX;
              CamY:=-dy;
              CamZ:=-dz;
              Renderer.Scale:=Renderer.scale/1.5;
            end;
            { if GetXYZAt(X,Y,dx,dy,dz) then
             with Renderer do
             begin
              CamX:=-dX;
              CamY:=-dy;
              CamZ:=-dz;
              Self.scale:=Self.scale/1.5;
              Renderer.Scale:=Self.scale;
             end;}
  Ord('N'): NextObjectInSurface;
  Ord('P'): PreviousObjectInSurface;
  Ord('T'): SetMapMode(MM_FR);
  Ord('G'): begin
             nx:=Renderer.gynormal;
             nz:=Renderer.gxnormal;
             With nz do Renderer.SetGridnormal(dx,dy,dz);
             With nx do Renderer.SetGridXnormal(dx,dy,dz);
             invalidate;
             exit;
            end;
  Ord('X'): if map_mode=MM_SF then
            begin
             surf:=Level.Sectors[cur_sc].Surfaces[cur_sf];
             if surf.adjoin<>nil then exit;
             s:=DoubleToStr(surf.ExtrudeSize);
             if not InputQuery('Extrude','By:',s) then exit;
             if (Not ValDouble(s,dx)) or (dx=0) then
             begin ShowMessage('Invalid Value: '+s); exit; end;

             StartUndoRec('Extrude surface');

             ExtrudeSurface(surf,dx);
             SetCurSF(Cur_SC,Cur_SF);
            end;
  VK_RIGHT,VK_LEFT,VK_UP,VK_DOWN,VK_NEXT,VK_PRIOR:
  if MapRot=MR_OLD then
  begin

   case key of
    VK_RIGHT: rYAW:=rYAW-5;
    VK_LEFT: rYAW:=rYAW+5;
    VK_UP: rPCH:=rPCH-5;
    VK_DOWN: rPCH:=rPCH+5;
    VK_PRIOR: rROL:=rROL-5;
    VK_NEXT: rROL:=rROL+5;
   end;
   SetRendFromPYR;
   Invalidate;
  end
  else
  With Renderer do
  begin
  { SetVec(nx,1,0,0);
   SetVec(ny,0,1,0);
   SetVec(nz,0,0,1);
   RotatePoint(0,0,0,nx.dx,nx.dy,nx.dz,PCH,ny.dx,ny.dy,ny.dz);
   RotatePoint(0,0,0,nx.dx,nx.dy,nx.dz,PCH,ny.dz,nz.dy,nz.dz);

   RotatePoint(0,0,0,ny.dx,ny.dy,ny.dz,YAW,nx.dx,nx.dy,nx.dz);
   RotatePoint(0,0,0,ny.dx,ny.dy,ny.dz,YAW,nz.dx,nz.dy,nz.dz);

   RotatePoint(0,0,0,nz.dy,nz.dy,nz.dz,ROL,nx.dx,nx.dy,nx.dz);
   RotatePoint(0,0,0,nz.dy,nz.dy,nz.dz,ROL,ny.dx,ny.dy,ny.dz);}
  With Renderer do
  case key of
   VK_RIGHT: RotAxis(zv,xv,-1);
   VK_LEFT: RotAxis(zv,xv,1);
   VK_UP: RotAxis(yv,zv,-1);
   VK_DOWN: RotAxis(yv,zv,1);
   VK_PRIOR: RotAxis(xv,yv,-1);
   VK_NEXT: RotAxis(xv,yv,1);
  end;
   With Renderer do
   begin
    with zv do SetZ(dx,dy,dz);
    with xv do SetX(dx,dy,dz);
   end;
  Invalidate;
  exit;
  end;
  VK_INSERT: DoStitch;
  188: begin ShiftTexture(st_up); exit; end;{<}
  190: begin ShiftTexture(st_down); exit; end; {>}
  191: begin StraightenTexture(false,true); exit; end;{/}
  Ord('A'): if Map_mode=MM_TH then LayerThings;
  Ord('F'): Goto_Adjoin;
  Ord('S'),Ord('W'): SnapGridToObject;
  else exit;
 end;
  Key:=0;
  Invalidate;
  exit;
 end;

 if shift=[ssAlt] then
 begin
  n:=Renderer.gnormal;
  xn:=Renderer.gxnormal;

 case key of
  Ord('A'): case map_mode of
            MM_SC: begin
                    i:=scsel.AddSC(Cur_SC);
                    if scsel.count=2 then
                    begin
                     StartUndoRec('Unadjoin sectors');
                     if UnAdjoinSectors(Level.Sectors[scsel.GetSC(0)],Level.Sectors[scsel.GetSC(1)]) then
                     PanMessage(mt_info,'Sectors unadjoined');
                    end else showMessage('You must have 2 sectors selected');
                     scsel.DeleteN(i);
                   end;

            MM_SF:begin
                   x:=sfsel.AddSF(cur_SC,cur_SF);
                   nsel:=0;
                   StartUndoRec('Unadjoin surfaces');
                   for i:=0 to sfsel.count-1 do
                   begin
                    sfsel.GetSCSF(i,nsc,nsf);
                    if UnAdjoin(Level.Sectors[nsc].Surfaces[nsf]) then Inc(nsel);
                   end;
                  PanMessageFmt(mt_info,'%d adjoins removed',[nsel]);
                  SetCurSF(Cur_SC,Cur_SF);
                  sfsel.DeleteN(x);
                end;
           end;
  Ord('B'): case map_mode of
             MM_SF:begin
                    i:=sfsel.AddSF(cur_SC,cur_SF);
                    sec:=BuildSector(level,sfsel);
                    if sec=nil then sfsel.DeleteN(i) else
                    begin
                     sfsel.clear;
                     SetMapMode(MM_SC);
                     SetCurSC(sec.num);
                   end;
                  end;
             MM_VX:begin
                    i:=vxsel.AddVX(cur_SC,cur_VX);
                    surf:=BuildSurface(level,vxsel);
                    if surf=nil then vxsel.DeleteN(i) else
                    begin
                     vxsel.clear;
                     SetMapMode(MM_SF);
                     SetCurSF(surf.sector.num,surf.num);
                   end;
                  end;
            end;
   Ord('C'): With Renderer do CleaveBy(gnormal,GridX,GridY,GridZ); {Cleave by grid plane}
   Ord('I'): if map_mode=MM_SF then
             begin
              i:=sfsel.AddSF(Cur_SC,Cur_SF);
              FlipSurfaces(Level,sfsel);
              sfsel.DeleteN(i);
             end;
{  VK_RIGHT : begin n.dx:=Cos(Arccos(n.dx)+0.3); xn.dx:=Cos(Arccos(xn.dx)+0.3); end;
  VK_LEFT: begin n.dx:=Cos(Arccos(n.dx)-12/pi); xn.dx:=Cos(Arccos(xn.dx)-12/pi); end;
  VK_UP: With Renderer do PCH:=PCH+10;
  VK_DOWN: With Renderer do PCH:=PCH-10;
  VK_NEXT: With Renderer do ROL:=ROL+10;
  VK_PRIOR: With Renderer do ROL:=ROL-10;}
{  VK_INSERT: Case map_mode of
               MM_SC: if GetMousePos(X,Y) then AddSectorAt(X,Y);
              end;}
  VK_HOME: begin StraightenTexture(false,false); exit; end;
  VK_DELETE: if Map_mode=MM_SF then DeleteSurface(Level,cur_SC,cur_SF);
  188: begin ScaleTexture(st_down); exit; end; {<}
  190: begin RotateTexture(st_up); exit; end; {>}

 end;

  Renderer.SetGridNormal(n.dx,n.dy,n.dz);
  Renderer.SetGridXNormal(xn.dx,xn.dy,xn.dz);
  Invalidate;
  exit;
 end;

 if shift=[ssShift,ssCtrl] then
 begin
 case key of
  191,VK_HOME: StraightenTexture(true,true);
 end;
  exit;
 end;

 if shift=[ssCtrl] then
 begin
 case key of
  VK_INSERT: begin StartStitch; exit; end;
  191,VK_HOME: begin StraightenTexture(true,false); exit; end;
  188: begin RotateTexture(st_left);  exit; end; {<}
  190: begin RotateTexture(st_right); exit; end; {>}
  VK_UP: RaiseObject(ro_up);
  VK_DOWN: RaiseObject(ro_down);
  Ord('C'): miCopyClick(nil);
  Ord('V'): miPasteClick(nil);
  Ord('P'): if map_mode=MM_SF then
           begin
            StartUndoRec('Flatten surface');
             if FlattenSurface(Level.Sectors[Cur_SC].Surfaces[Cur_SF]) then
             PanMessage(mt_info,'Surface planarized')
             else PanMessage(mt_info,'Surface wasn''t fully planarized');
             Invalidate;
            end;
  Ord('X'): if map_mode=MM_SF then
            begin
             surf:=Level.Sectors[cur_sc].Surfaces[cur_sf];
             if surf.adjoin<>nil then exit;
             s:=DoubleToStr(surf.ExtrudeSize);
             if not InputQuery('Extrude&&Expand','By:',s) then exit;
             if (Not ValDouble(s,dx)) or (dx=0) then
             begin ShowMessage('Invalid Value: '+s); exit; end;

             StartUndoRec('Extrude&&Expand surface');

             ExtrudeNExpandSurface(surf,dx);
             SetCurSF(Cur_SC,Cur_SF);
            end;
 end;
 exit;
 end;

 case key of
  VK_BACK: ClearMultiSelection;
  VK_ESCAPE: CancelMouseMode;
  Ord('1')..Ord('6'):
   if MapRot=MR_Old then
   begin
    case Chr(Key) of
     '1': begin rPCH:=0; rYaw:=0; rRol:=0; end;
     '2': begin rPCH:=-90; rYaw:=180; rRol:=0; end;
     '3': begin rPCH:=90; rYaw:=0; rRol:=90; end;
     '4': begin rPCH:=0; rYaw:=180; rRol:=0; end;
     '5': begin rPCH:=90; rYaw:=0; rRol:=0; end;
     '6': begin rPCH:=0; rYaw:=-90; rRol:=-90; end;
    end;
    SetRendFromPYR;
    Invalidate;
   end
   else
   With Renderer do
   case Chr(Key) of
   '1': begin SetZ(0,0,1); SetX(1,0,0); end;
   '2': begin SetZ(0,1,0); SetX(-1,0,0); end;
   '3': begin SetZ(1,0,0); SetX(0,1,0); end;
   '4': begin SetZ(0,0,-1); SetX(-1,0,0); end;
   '5': begin SetZ(0,-1,0); SetX(1,0,0); end;
   '6': begin SetZ(-1,0,0); SetX(0,-1,0); end;
   end;
  VK_ADD: begin Renderer.scale:=Renderer.scale/1.5; end;
  VK_SUBTRACT: begin Renderer.scale:=Renderer.scale*1.5; end;
  VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,VK_NEXT,VK_PRIOR:
  With Renderer do
  begin
   {CreateRotMatrix(mx,PCH,YAW,ROL);}

   With Renderer do SetVec(nx,xv.dx*scale,xv.dy*scale,xv.dz*scale);
   With Renderer do SetVec(ny,yv.dx*scale,yv.dy*scale,yv.dz*scale);
   With Renderer do SetVec(nz,zv.dx*scale,zv.dy*scale,zv.dz*scale);

  case key of
   VK_LEFT: begin CamX:=CamX+nx.dx; CamY:=CamY+nx.dy; CamZ:=CamZ+nx.dz; end;
   VK_RIGHT: begin CamX:=CamX-nx.dx; CamY:=CamY-nx.dy; CamZ:=CamZ-nx.dz; end;
   VK_DOWN: begin CamX:=CamX+ny.dx; CamY:=CamY+ny.dy; CamZ:=CamZ+ny.dz; end;
   VK_UP: begin CamX:=CamX-ny.dx; CamY:=CamY-ny.dy; CamZ:=CamZ-ny.dz; end;
   VK_NEXT : begin CamX:=CamX+nz.dx; CamY:=CamY+nz.dy; CamZ:=CamZ+nz.dz; end;
   VK_PRIOR: begin CamX:=CamX-nz.dx; CamY:=CamY-nz.dy; CamZ:=CamZ-nz.dz; end;
  end;
  end;
  VK_RETURN: begin if IsSelvalid then ItemEdit.Show; exit; end;
  Ord('A'): Case map_mode of
             MM_SC: begin
                     i:=scsel.AddSC(CUr_SC);
                     if scsel.count=2 then
                     begin
                      StartUndoRec('Adjoin Sectors');
                      if AdjoinSectors(Level.Sectors[scsel.GetSC(0)],Level.Sectors[scsel.GetSC(1)]) then
                      PanMessage(mt_info,'Sectors adjoined');
                     end else showMessage('You must have 2 sectors selected');
                     scsel.DeleteN(i);
                    end;

              MM_SF:
                begin
                   x:=sfsel.AddSF(cur_SC,cur_SF);
                   nsel:=0;
                   StartUndoRec('Adjoin surfaces');
                   for i:=0 to sfsel.count-1 do
                   begin
                    sfsel.GetSCSF(i,nsc,nsf);
                    if MakeAdjoin(Level.Sectors[nsc].Surfaces[nsf]) then Inc(nsel);
                   end;
                  PanMessageFmt(mt_info,'%d adjoins formed',[nsel]);
                  SetCurSF(Cur_SC,Cur_SF);
                  sfsel.DeleteN(x);
                end;
              MM_TH: LayerThings;
             end;
  Ord('B'): case map_mode of
             MM_TH: BringThingToSurf(cur_TH,cur_SC,Cur_SF);
             MM_LT: BringLightToSurf(cur_LT,cur_SC,Cur_SF);
            end;
  VK_MULTIPLY: SetViewToGrid;
  Ord('E'): SetMapMode(MM_ED);
  Ord('X'): if map_mode=MM_SF then
            begin
             surf:=Level.Sectors[cur_sc].Surfaces[cur_sf];
             StartUndoRec('Extrude surface');
             ExtrudeSurface(surf,surf.ExtrudeSize);
             SetCurSF(Cur_SC,Cur_SF);
            end;
  Ord('S'): SetMapMode(MM_SC);
  Ord('C'): SetMouseMode(MM_CLEAVE);
  Ord('F'): SetMapMode(MM_SF);
  Ord('J'): case map_mode of
             MM_SC: ConnectSCs;
             MM_SF: ConnectSFs;
            end;
  Ord('K'): SetMouseMode(MM_CreateSector);
  Ord('G'): if GetMousePos(X,Y) then begin SetMouseMode(MM_TranslateGrid); Do_StartTranslateGrid(X,Y); end;
  VK_Space: DO_MultiSelect;
  {if GetMousePos(X,Y) then begin SetMouseMode(MM_TranslateCam); Do_StartTranslateCam(X,Y); end;}
  Ord('R'): if GetMousePos(X,Y) then Do_StartRotateCam(X,Y);
  Ord('T'): SetMapMode(MM_TH);
  Ord('V'): SetMapMode(MM_VX);
  Ord('L'): SetMapMode(MM_LT);
  Ord('N'): NextObject;
  Ord('P'): PreviousObject;
  Ord('U'): UseInCog;
  Ord('Q'): LoadDLLPlugin('h:\rr\mv\mv.dll');
  Ord('M'): begin
            case Map_mode of
             MM_SC: begin
                     i:=scsel.AddSC(Cur_SC);
                     scsel.sort;
                     nsel:=0;
                     StartUndoRec('Merge sectors');

                     while scsel.count>1 do
                     begin
                      sec:=Level.Sectors[scsel.GetSC(scsel.count-1)];
                      for i:=0 to scsel.count-2 do
                      begin
                       if MergeSectors(Level.Sectors[scsel.GetSC(i)],sec)<>nil
                       then begin inc(nsel); break; end;
                      end;
                      scsel.DeleteN(scsel.count-1);
                     end;

                      SetCurSC(scsel.GetSC(0));
                      scsel.clear;
                      PanMessageFmt(mt_info,'%d sectors merged',[nsel]);
                    end;
             MM_SF: begin
                     i:=sfsel.AddSF(Cur_SC,Cur_SF);
                     case sfsel.count of
                      1: begin
                          surf:=Level.Sectors[Cur_SC].Surfaces[Cur_SF];
                          if surf.adjoin=nil then exit;
                          StartUndoRec('Merge sectors');
                          sec:=MergeSectors(surf.sector,surf.adjoin.sector);
                          if Sec=nil then PanMessage(mt_info,'Sectors weren''t merged') else
                          begin
                           setCurSF(sec.Num,0);
                           sfsel.clear;
                          end;
                         end;
                     else begin
                           sfsel.sort;
                           nsel:=0;
                           StartUndoRec('Merge surfaces');

                           while sfsel.count>1 do
                           begin
                            sfsel.GetSCSF(sfsel.count-1,nsc,nsf);
                            surf:=Level.Sectors[nsc].Surfaces[nsf];
                            for i:=0 to sfsel.count-2 do
                            begin
                             sfsel.GetSCSF(i,nsc,nsf);
                             if MergeSurfaces(Level.Sectors[nsc].Surfaces[nsf],
                                              surf)<>nil then begin inc(nsel); break; end;

                            end;
                            sfsel.deleteN(sfsel.count-1);
                           end;

                           sfsel.GetSCSF(0,nsc,nsf);
                           surf:=Level.Sectors[nsc].Surfaces[nsf];
                           sfsel.clear;
                           SetCurSF(surf.sector.num,surf.num);
                           PanMessageFmt(mt_info,'%d surfaces merged',[nsel]);

                         end;
                     {else ShowMessage('You should either select one surface to merge sector or two surfaces to merge surfaces');}
                     end;
                     sfsel.DeleteN(i);
                    end;
             MM_VX: begin
                     PanMessageFmt(mt_info,'%d vertices merged',[MergeVertices(Level,vxsel)]);
                     vxsel.clear;
                    end;
             MM_ED: begin
                      surf:=Level.Sectors[Cur_SC].Surfaces[Cur_SF];
                      StartUndoRec('Merge surfaces');
                      if MergeSurfacesAt(surf,Cur_ED) then
                      edsel.clear;
                    end;
            end;
            SetMapMode(map_mode);
            end;
  VK_INSERT: Begin
              Case map_mode of
               MM_SC: if GetMousePos(X,Y) then CopySectorsAt(X,Y);
               MM_LT: if GetMousePos(X,Y) then AddLightsAt(X,Y);
               MM_TH: if GetMousePos(X,Y) then AddThingsAt(X,Y);
               MM_FR: if GetMousePos(X,Y) then AddFramesAt(X,Y);
              end;
             end;
  VK_DELETE: begin
             case Map_mode of
              MM_SC:
              begin
               {SaveSelSecUndo('Delete Sector(s)',ch_deleted,sc_both);}
               scsel.AddSC(Cur_SC);
               scsel.sort;
               if scsel.count>=Level.Sectors.count then exit;

               StartUndoRec('Delete Sector(s)');

               for i:=scsel.count-1 downto 0 do
                 DeleteSector(Level,scsel.GetSC(i)); {updated in procedure}
               scsel.clear;
              end;
              MM_TH: begin
                      thsel.AddTH(Cur_TH);
                      thsel.sort;
                      if thsel.count>=Level.Things.count then exit;
                      StartUndoRec('Delete Thing(s)');
                      for i:=thsel.count-1 downto 0 do
                      begin
                       Free3DO(Level.Things[thsel.getTH(i)].a3DO);
                       DeleteThing(Level,thsel.getTH(i));
                      end;
                      thsel.Clear;
                     end;
              MM_FR: begin
                      frsel.AddFR(Cur_TH,Cur_FR);
                      frsel.sort;
                      {SaveSelFramesUndo('Delete frame(s)',ch_deleted);}
                      StartUndoRec('Delete Frame(s)');
                      for i:=frsel.count-1 downto 0 do
                      begin
                       frsel.GetTHFR(i,nth,nfr);
                       DeleteFrame(nth,nFR);
                      end;
                      frsel.clear;
                     end;
              MM_LT: begin
                      ltsel.AddLT(Cur_LT);
                      ltsel.sort;
                      if ltsel.count>=Level.Lights.count then exit;
                      {SaveSelLightsUndo('Delete light(s)',ch_deleted);}
                      StartUndoRec('Delete Light(s)');
                      for i:=ltsel.count-1 downto 0 do
                       DeleteLight(Level,ltsel.getLT(i));
                      ltsel.clear;
                     end;
              MM_VX: begin
                      vxsel.AddVX(Cur_SC,Cur_VX);
                      vxsel.sort;
                      if vxsel.count=1 then StartUndoRec('Delete Vertex')
                      else StartUndoRec('Delete Vertices');
                      a:=0;
                      for i:=vxsel.count-1 downto 0 do
                      begin
                       vxsel.GetSCVX(i,x,y);
                       if DeleteVertex(Level.Sectors[x],y,vxsel.count<>1) then inc(a);
                      end;
                      PanMessageFmt(mt_info,'%d vertices deleted',[a]);
                      vxsel.clear;
                     end;
             end;
              SetmapMode(map_mode);
             end;
  {;}  186: begin StartStitch; exit; end;
  {'}  222: begin DoStitch;  exit; end;
  {/}  191: begin StraightenTexture(false,false);  exit; end;
  {<}  188: begin ShiftTexture(st_left);  exit; end;
  {>}  190: begin ShiftTexture(st_right);  exit; end;
  {[}  219: RaiseObject(ro_up);
  {]}  221: RaiseObject(ro_down);

  else exit;
 end;
 Key:=0;
 Invalidate;
end;

{Mouse operations}

Procedure TJedMain.Do_SelectAt(x,Y:integer;fore:boolean);
Var s,sf,ed,th,vx,fr:integer;
    v1,v2:TJKVertex;
    sec:TJKSector;
    Thing:TJKThing;
    light:TJedLight;
    obj:TObject;
    fx,fy,fz,pch,yaw,rol:double;
begin
 if (LastSelXY.X=X) and (LastSelXY.X=X) and (Renderer.Selected.Count<>0) then
 begin
  if fore then if cSel>=Renderer.Selected.Count-1 then csel:=0 else inc(csel);
  if not fore then if cSel=0 then csel:=Renderer.Selected.Count-1 else dec(csel);
 end
 else csel:=-1;
if csel=-1 then
begin
 Renderer.BeginPick(X,Y);
 Renderer.SetPointSize(4);
 Case Map_mode of
 MM_SC: for s:=0 to Level.Sectors.Count-1 do
        begin
         Sec:=level.Sectors[s];
         if Toolbar.IsLayerVisible(sec.layer) then
         Renderer.PickPolygons(sec.surfaces,s);
        end;
 MM_SF: for s:=0 to Level.Sectors.Count-1 do
        begin
         sec:=level.Sectors[s];
         if Toolbar.IsLayerVisible(sec.Layer) then
         for sf:=0 to Sec.Surfaces.Count-1 do
          Renderer.PickPolygon(sec.surfaces[sf],s+sf*65536);
        end;
 MM_ED: for s:=0 to Level.Sectors.Count-1 do
        begin
         sec:=level.Sectors[s];
         if Toolbar.IsLayerVisible(sec.Layer) then
         for sf:=0 to Sec.Surfaces.Count-1 do
          With Sec.Surfaces[sf] do
          for ed:=0 to Vertices.Count-1 do
          begin
           v1:=Vertices[ED];
           v2:=Vertices[NextVx(ED)];
           Renderer.PickLine(v1,v2,s*65536+ed*1024+sf);
          end;
        end;
 MM_TH: For th:=0 to Level.Things.Count-1 do
        begin
         thing:=Level.Things[th];
         if Toolbar.ISLayerVisible(thing.Layer) then
         With Thing do Renderer.PickVertex(x,y,z,th);
        end;
 MM_FR: For th:=0 to Level.Things.Count-1 do
        begin
         thing:=Level.Things[th];
         if not Toolbar.ISLayerVisible(thing.Layer) then continue;
         with Thing Do Renderer.PickVertex(x,y,z,th*65536+65535);
         for fr:=0 to thing.vals.count-1 do
         with thing.vals[fr] do
         begin
          if atype<>at_frame then continue;
          getFrame(fx,fy,fz,pch,yaw,rol);
          Renderer.PickVertex(fx,fy,fz,th*65536+fr);
         end;
        end;
 MM_VX: for s:=0 to Level.Sectors.Count-1 do
        begin
         sec:=level.Sectors[s];
         if Toolbar.IsLayerVisible(Sec.Layer) then
         With Sec do
         for vx:=0 to Vertices.Count-1 do
         With Vertices[vx] do
          Renderer.PickVertex(x,y,z,s+vx*65536);
        end;
 MM_LT: for th:=0 to Level.Lights.Count-1 do
        begin
         Light:=Level.Lights[th];
         If Toolbar.IsLayerVisible(Light.Layer) then
         With Light do
          Renderer.PickVertex(x,y,z,th);
        end;
MM_Extra: for s:=0 to ExtraObjs.Count-1 do
         begin
          obj:=extraObjs[s];
          if Obj is TVertex then With TJKVertex(obj) do Renderer.PickVertex(x,y,z,s) else
          if Obj is TExtraLine then With TExtraLine(obj) do Renderer.PickLine(v1,v2,s) else
          if Obj is TPolygon then Renderer.PickPolygon(TPolygon(obj),s);
         end;

end;
 Renderer.EndPick;
 if Renderer.Selected.Count<>0 then
  csel:=0;
end;
 {Lb.Caption:=IntToStr(Renderer.Selected.Count);}
 if (Renderer.Selected.Count<>0) then
 begin
  case map_mode of
  MM_SC: SetCurSC(Renderer.Selected[csel]);
  MM_SF: begin
          Cur_SC:=Renderer.Selected[csel] mod 65536;
          Cur_SF:=Renderer.Selected[csel] div 65536;
          SetCurSF(Cur_SC,Cur_SF);
         end;
  MM_ED: begin
          vx:=Renderer.Selected[csel];
          s:=vx div 65536;
          sf:=vx mod 1024;
          ed:=(vx mod 65536) div 1024;
          SetCurED(s,sf,ed);
         end;
  MM_TH: SetCurTH(Renderer.Selected[csel]);
  MM_FR: begin
          Cur_FR:=Renderer.Selected[csel] mod 65536;
          Cur_TH:=Renderer.Selected[csel] div 65536;
          if Cur_FR=65535 then Cur_FR:=-1;
          SetCurFR(Cur_TH,Cur_FR);
         end;
  MM_VX: begin
          Cur_SC:=Renderer.Selected[csel] mod 65536;
          Cur_VX:=Renderer.Selected[csel] div 65536;
          SetCurVX(Cur_SC,Cur_VX);
         end;
  MM_LT: SetCurLT(Renderer.Selected[csel]);
  MM_Extra: SetCurEX(Renderer.Selected[csel]);
 end;
  Invalidate;
 end;

 LastSelXY.X:=X;
 LastSelXY.Y:=Y;

end;

{Cleave mouse functions}

Procedure TJedMain.Do_StartCleave(X,Y:integer;snaptovertex:boolean);
begin
 if CleaveStarted then Redraw;
 With CleaveOrgXY do if not GetMousePos(Integer(X),Integer(Y)) then exit;
 WIth CleaveOrg do
 begin
  if not Renderer.GetGridAt(CleaveOrgXY.X,CleaveOrgXY.Y,X,Y,Z) then exit;
  if snaptoVertex then GetNearestVX(Level,x,y,z,x,y,z,0.2)
  else if SnapToGrid then Renderer.GetNearestGrid(x,y,z,x,y,z);
 end;
 With CleaveOrg do
  Renderer.ProjectPoint(x,y,z,Integer(CleaveOrgXY.X),Integer(CleaveOrgXY.Y));
 LastX:=CleaveOrgXY.X;
 LastY:=CleaveOrgXY.Y;
 SetMouseMode(MM_CLeave);
 CleaveStarted:=true;
end;

Procedure TJedMain.Do_ProceedCleave(X,Y:integer;snaptovertex:boolean);
var fx,fy,fz:double;
begin
 if not CleaveStarted then exit;
 Canvas.Pen.Mode:=pmXor;
 Canvas.Pen.Color:=clRed;
 Canvas.MoveTo(CleaveOrgXY.X,CleaveOrgXY.Y);
 Canvas.LineTo(LastX,LastY);
 Canvas.MoveTo(CleaveOrgXY.X,CleaveOrgXY.Y);

 if not Renderer.GetGridAt(X,Y,fX,fY,fZ) then exit;
 if snaptoVertex then GetNearestVX(Level,fx,fy,fz,fx,fy,fz,0.2)
 else if SnapToGrid then Renderer.GetNearestGrid(fx,fy,fz,fx,fy,fz);

 Renderer.ProjectPoint(fx,fy,fz,X,Y);
 Canvas.LineTo(X,Y);
 LastX:=X; LastY:=Y;
end;

Procedure TJedMain.Do_EndCleave(X,Y:integer;snaptovertex:boolean);
var
    x1,y1,z1,x2,y2,z2:double;
    cnormal:TVector;
    cleaved:boolean;
    i:integer;
begin
try
 if not CleaveStarted then exit;

 if not Renderer.GetGridAt(X,Y,X2,Y2,Z2) then exit;
 if snaptoVertex then GetNearestVX(Level,x2,y2,z2,x2,y2,z2,0.2)
 else if SnapToGrid then Renderer.GetNearestGrid(x2,y2,z2,x2,y2,z2);

 With CleaveOrg do begin X1:=X; Y1:=Y; Z1:=Z; end;
 If IsClose(x2,x1) and IsClose(y2,y1) and IsClose(z2,z1) Then exit;
 With Renderer do
 VMult(Gnormal.dx,Gnormal.dy,Gnormal.dz,X2-X1,Y2-Y1,Z2-Z1,cnormal.dX,cNormal.dY,cNormal.dZ);
 Normalize(cnormal);
 cleaved:=false;
 CleaveBy(cnormal,x1,y1,z1);
finally
 SetMouseMode(MM_SELECT);
 Invalidate;
 CleaveStarted:=false;
end;
end;

Procedure TJedMain.DO_StartRectSelect(X,Y:integer);
begin
 RectSelStarted:=true;
 RectOrgX:=X;
 RectOrgY:=Y;
 RectLastX:=RectOrgX;
 RectLastY:=RectOrgY;
 SetMouseMode(MM_RectSelect);
end;

Procedure TJedMain.DO_ProceedRectSelect(X,Y:integer);
begin
 Canvas.Pen.Mode:=pmXor;
 Canvas.Pen.Color:=clRed;
 Canvas.Brush.Style:=bsClear;
 Canvas.Rectangle(RectOrgX,RectOrgY,RectLastX,RectLastY);
 Canvas.Rectangle(RectOrgX,RectOrgY,X,Y);
 RectLastX:=X;
 RectLastY:=Y;
end;

Procedure TJedMain.DO_EndRectSelect(X,Y:integer);
var i,sf,vx,ed,fr:integer;
    ax,ay,az,pch,yaw,rol:double;
    allin:boolean;
begin
 Renderer.BeginRectPick(RectOrgX,RectOrgY,X,Y);
 case Map_mode of
  MM_SC: begin
          for i:=0 to level.Sectors.count-1 do
          With Level.Sectors[i] do
          if ToolBar.IsLayerVisible(Layer) then
          begin
           allin:=true;
           for vx:=0 to vertices.count-1 do
           With Vertices[vx] do
            if not Renderer.IsVertexInRect(X,y,z) then
            begin
             allin:=false;
             break;
            end;
           if allin then DO_SelSC(i);
          end;
         end;
  MM_SF: begin
          for i:=0 to level.Sectors.count-1 do
          With Level.Sectors[i] do
          if ToolBar.IsLayerVisible(Layer) then
          For sf:=0 to surfaces.count-1 do
           if Renderer.IsPolygonInRect(Surfaces[sf]) then DO_SelSF(i,sf);

         end;
  MM_ED: begin
          for i:=0 to level.Sectors.count-1 do
          With Level.Sectors[i] do
          if ToolBar.IsLayerVisible(Layer) then
          For sf:=0 to surfaces.count-1 do
          With surfaces[sf] do
           for ed:=0 to vertices.count-1 do
           if Renderer.IsLineInRect(Vertices[ed],Vertices[NextVX(ed)]) then
             DO_SelED(i,sf,ed);
         end;
  MM_VX: begin
          for i:=0 to level.Sectors.count-1 do
          With Level.Sectors[i] do
          if ToolBar.IsLayerVisible(Layer) then
          for vx:=0 to vertices.count-1 do
          With Vertices[vx] do
          if Renderer.IsVertexInRect(X,y,z) then DO_SelVX(i,vx);
         end;
  MM_TH: begin
          for i:=0 to Level.Things.count-1 do
          With Level.Things[i] do
          if ToolBar.IsLayerVisible(Layer) then
          if Renderer.IsVertexInRect(X,y,z) then DO_SelTH(i);
         end;
  MM_LT: begin
          for i:=0 to Level.Lights.count-1 do
          With Level.Lights[i] do
          if ToolBar.IsLayerVisible(Layer) then
          if Renderer.IsVertexInRect(X,y,z) then DO_SelLT(i);
         end;
  MM_FR: begin
          for i:=0 to Level.Things.count-1 do
          With Level.Things[i] do
          if ToolBar.IsLayerVisible(Layer) then
          for fr:=0 to Vals.count-1 do
          With Vals[fr] do
          begin
           if atype<>at_frame then continue;
           GetFrame(ax,ay,az,pch,yaw,rol);
           if Renderer.IsVertexInRect(aX,ay,az) then DO_SelFR(i,fr);
          end;
         end;
 end;
 Renderer.EndRectPick;

 SetMouseMode(MM_Select);
 Invalidate;
end;

{Mouse Drag functions}

Procedure TJedMain.Do_StartDrag(X,Y:integer);
var fx,fy,fz:double;
    n,i:integer;
    th,fr:integer;
begin
 if not Renderer.GetGridAt(X,Y,fx,fy,fz) then exit;
  SetMouseMode(MM_DRAG);
  DragOrg.x:=fx;
  DragOrg.y:=fy;
  DragOrg.z:=fz;

  with DragOrgPos do GetCurObjRefXYZ(x,y,z);

 case map_mode of
  MM_SC: StartUndoRec('Drag sector(s)');
  MM_SF: StartUndoRec('Drag surface(s)');
  MM_ED: StartUndoRec('Drag edge(s)');
  MM_VX: StartUndoRec('Drag vertices');

  MM_LT: StartUndoRec('Drag light(s)');
  MM_TH: StartUndoRec('Drag thing(s)');
  MM_FR: StartUndoRec('Drag frame(s)');
  MM_Extra: if Assigned(OnExtraMove) then OnExtraMove(ExtraObjs[Cur_EX],false);
 end;
end;

Procedure TJedMain.GetCurObjRefXYZ(var rx,ry,rz:double);
var
    sec:TJKSector;
    surf:TJKSurface;
    vx:TJKVertex;
    th:TJKThing;
    lt:TJedLight;
    vl:TTPLValue;
    ex:TObject;
    pch,yaw,rol:double;
begin
 case map_mode of
  MM_SC: begin
          sec:=Level.Sectors[Cur_SC];
          With Sec.Vertices[0] do begin rx:=x; ry:=y; rz:=z; end;
         end;
  MM_SF: begin
          Surf:=Level.Sectors[Cur_SC].Surfaces[Cur_SF];
          With Surf.Vertices[0] do begin rx:=x; ry:=y; rz:=z; end;
         end;
  MM_VX: begin
          vx:=Level.Sectors[Cur_SC].Vertices[Cur_VX];
          With vx do begin rx:=x; ry:=y; rz:=z; end;
         end;
  MM_ED: With Level.Sectors[Cur_SC].Surfaces[Cur_SF] do
         begin
          vx:=Vertices[Cur_ED];
          With vx do begin rx:=x; ry:=y; rz:=z; end;
         end;
  MM_TH: begin
          th:=Level.Things[Cur_TH];
          With th do begin rx:=x; ry:=y; rz:=z; end;
         end;
  MM_FR: begin
          if Cur_FR=-1 then
          begin
           th:=Level.Things[Cur_TH];
           With th do begin rx:=x; ry:=y; rz:=z; end;
          end
          else
          begin
           vl:=Level.Things[Cur_TH].Vals[Cur_FR];
           vl.GetFrame(rx,ry,rz,pch,yaw,rol);
          end;
         end;
  MM_LT: begin
          lt:=Level.Lights[Cur_LT];
          With lt do begin rx:=x; ry:=y; rz:=z; end;
         end;
  MM_Extra: begin
          ex:=ExtraObjs[Cur_EX];
          if ex is TVertex then With TVertex(ex) do begin rx:=x; ry:=y; rz:=z; end;
          if ex is TExtraLine then With TExtraLine(ex).v1 do begin rx:=x; ry:=y; rz:=z; end;
          if ex is TPolygon then With TPolygon(ex).vertices[0] do begin rx:=x; ry:=y; rz:=z; end;
         end;
  else begin rx:=0; ry:=0; rz:=0; end;
 end;
end;

Procedure TJedMain.Do_ProceedDrag(X,Y:integer;snaptovx:boolean;snaptoaxis,yaxis:boolean);
const
     dragcloseenough=0.2;
var fx,fy,fz,
    dx,dy,dz:double;

Procedure GetDXYZ(rx,ry,rz:double;var dx,dy,dz:double);
var ax,ay,az,d:double;
    arx,ary,arz:double;
begin
 With DragOrg do begin dx:=fx-x; dy:=fy-y; dz:=fz-z; end;

 if snaptoaxis then
 with Renderer do
 begin
  arx:=gxnormal.dx*dx+gxnormal.dy*dy+gxnormal.dz*dz;
  ary:=gynormal.dx*dx+gynormal.dy*dy+gynormal.dz*dz;

  if yaxis then
  begin
    dx:=gynormal.dx*ary;
    dy:=gynormal.dy*ary;
    dz:=gynormal.dz*ary;
  end else
  begin
    dx:=gxnormal.dx*arx;
    dy:=gxnormal.dy*arx;
    dz:=gxnormal.dz*arx;
  end;

  if SnapToGrid then
  with Renderer do
  begin
   d:=gnormal.dx*GridX+gnormal.dy*GridY+gnormal.dz*GridZ;
   PlaneLineXn(gnormal,D,rx+dx,ry+dy,rz+dz,rx+dx+gnormal.dx,ry+dy+gnormal.dy,rz+dz+gnormal.dz,
               arx,ary,arz);
   Renderer.GetNearestGrid(arx,ary,arz,ax,ay,az);
   dx:=ax-arx+dx; dy:=ay-ary+dy; dz:=az-arz+dz;
  end;

  exit;

 end;


 case snaptovx of
  false: case SnapToGrid of
          false: exit;
          true: begin
                 {Find distance to plane - signed}
                 With renderer do
                 begin
                  {a:=SMult(gnormal.dX,gnormal.dy,gnormal.dz,rx+dx-gridX,ry+dy-GridY,rz+dz-GridZ);}
                  d:=gnormal.dx*GridX+gnormal.dy*GridY+gnormal.dz*GridZ;
                  PlaneLineXn(gnormal,D,rx+dx,ry+dy,rz+dz,rx+dx+gnormal.dx,ry+dy+gnormal.dy,rz+dz+gnormal.dz,
                    arx,ary,arz);
                  Renderer.GetNearestGrid(arx,ary,arz,ax,ay,az);
                  dx:=ax-arx+dx; dy:=ay-ary+dy; dz:=az-arz+dz;
                end;
                end;
         end;
  true: begin
         if not GetNearestVX(Level,rx+dx,ry+dy,rz+dz,ax,ay,az,dragcloseenough) then exit;
         dx:=ax-rx; dy:=ay-ry; dz:=az-rz;
        end;
 end;
end;

Var
    i:integer;
    rx,ry,rz:double;

begin
 if not Renderer.GetGridAt(X,Y,fx,fy,fz) then exit;

// GetCurObjRefXYZ(rx,ry,rz);

 With DragOrgPos do begin rx:=x; ry:=y; rz:=z; end;

 GetDXYZ(rx,ry,rz,dx,dy,dz);

 GetCurObjRefXYZ(rx,ry,rz);

 dx:=DragOrgPos.x-rx+dx;
 dy:=DragOrgPos.y-ry+dy;
 dz:=DragOrgPos.z-rz+dz;


 case map_mode of
  MM_SC: TranslateSectors(level,scsel,Cur_SC,dx,dy,dz);
  MM_SF: TranslateSurfaces(Level,sfsel,Cur_SC,Cur_SF,dx,dy,dz);
  MM_VX: TranslateVertices(level,vxsel,cur_SC,Cur_VX,dx,dy,dz);
  MM_ED: TranslateEdges(level,edsel,Cur_SC,Cur_SF,Cur_ED,dx,dy,dz);
  MM_TH: TranslateThings(level,thsel,Cur_TH,dx,dy,dz,MoveFrames);
  MM_FR: if Cur_FR=-1 then TranslateThingKeepFrame(Level,Cur_TH,dx,dy,dz)
         else TranslateFrames(level,frsel,Cur_TH,Cur_FR,dx,dy,dz);
  MM_LT: TranslateLights(level,ltsel,Cur_LT,dx,dy,dz);
  MM_EXTRA: TranslateExtras(Cur_EX,dx,dy,dz);
 end;

//  With DragOrg do
//  begin
//   x:=x+dx;
//   y:=y+dy;
//   z:=z+dz;
//  end;

 Invalidate;
end;

Procedure TJedMain.Do_EndDrag(X,Y:Integer);
begin
 {Case Map_Mode of
  mm_th: LayerThing(Cur_TH);
 end;}
 SetMouseMode(MM_SELECT);
end;


Procedure TJedMain.Do_StartTranslateCam(X,Y:integer);
begin
 CamOrg.X:=X;
 CamOrg.Y:=Y;
end;

Procedure TJedMain.Do_TranslateCam(X,Y:integer);
var
 nx,ny,nz:double;
begin
 With Renderer do
 begin
  if not GetXYZonPlaneAt(X,Y,Renderer.zv,CamX,CamY,CamZ,nx,ny,nz) then exit;

  CamX:=nx;
  CamY:=nY;
  CamZ:=nZ;
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
   begin CamX:=CamX+nz.dx; CamY:=CamY+nz.dy; CamZ:=CamZ+nz.dz; end;}

  Do_StartTranslateCam(X,Y);
  Invalidate;
end;

Procedure TJedMain.Do_StartRotateCam(X,Y:integer);
begin
 if mouse_mode=MM_RotateCam then exit;
 GOrgXY.X:=X; GOrgXY.Y:=Y;
 With GOrg do
 begin
  X:=rPCH;
  Y:=rYAW;
  Z:=rROL;
 end;
 OrgX:=Renderer.xv;
 OrgY:=Renderer.yv;
 OrgZ:=Renderer.zv;

 SetMouseMode(MM_RotateCam);
end;

Procedure TJedMain.Do_RotateCam(X,Y:integer);
var dx,dy:integer;
    nx,ny,nz:Tvector;
begin
 dx:=x-GOrgXY.X;
 dy:=y-GOrgXY.y;

if MapRot=MR_old then
begin
 with GOrg do
 begin
  rROL:=Z+dX;
  rPCH:=X+dY;
  SetRendFromPYR;
 end;
end else
begin
 nx:=OrgX;
 ny:=OrgY;
 nz:=OrgZ;

 RotAxisAngle(nx,ny,dx); {l/r}
 RotAxisAngle(ny,nz,dy); {u/d}

 With nz do Renderer.SetZ(dx,dy,dz);
 With nx do Renderer.SetX(dx,dy,dz);

end;

 Invalidate;
end;

Procedure TJedMain.Do_StartTranslateGrid(X,Y:integer);
begin
 GOrgXY.X:=X; GOrgXY.Y:=Y;
 With GOrg,Renderer do
 begin
  X:=GridX;
  Y:=GridY;
  Z:=GridZ;
 end;
end;

Procedure TJedMain.Do_TranslateGrid(X,Y:integer);
var pnorm:TVector;
    a,nx,nY,nZ:double;
begin
 With Renderer do
 begin
  pnorm:=zv;
  if not GetXYZonPlaneAt(X,Y,pnorm,CamX,CamY,CamZ,nx,ny,nz) then exit;

try
 a:=1/GridMoveStep;
 nx:=Int(nx*a)/a;
 ny:=Int(ny*a)/a;
 nz:=Int(nz*a)/a;
except
 on Exception do;
end;
  GridX:=nx;
  GridY:=ny;
  GridZ:=nz;
  Invalidate;
 end;
end;

Procedure TJedMain.Do_StartRotateGrid(X,Y:integer);
begin
 GOrgXY.X:=X; GOrgXY.Y:=Y;
 With GOrg,Renderer do
 begin
  X:=GridX;
  Y:=GridY;
  Z:=GridZ;
 end;
end;

Procedure TJedMain.Do_RotateGrid(X,Y:integer);
var dx,dy:double;
begin
 dx:=X-GOrgXY.X;
 dy:=Y-GOrgXY.Y;

end;

Procedure TJedMain.DO_StartCreateSC(X,Y:integer;snaptovertex:boolean);
begin
 if SCCreateStarted then Redraw;
 With CleaveOrgXY do if not GetMousePos(Integer(X),Integer(Y)) then exit;
 WIth CleaveOrg do
 begin
  if not Renderer.GetGridAt(CleaveOrgXY.X,CleaveOrgXY.Y,X,Y,Z) then exit;
  if snaptoVertex then GetNearestVX(Level,x,y,z,x,y,z,0.2)
  else if SnapToGrid then Renderer.GetNearestGrid(x,y,z,x,y,z);
 end;
 With CleaveOrg do
  Renderer.ProjectPoint(x,y,z,Integer(CleaveOrgXY.X),Integer(CleaveOrgXY.Y));
 LastX:=CleaveOrgXY.X;
 LastY:=CleaveOrgXY.Y;
 SetMouseMode(MM_CreateSector);
 SCCreateStarted:=true;
end;

Procedure TJedMain.DO_ProceedCreateSC(X,Y:integer;snaptovertex:boolean);
var fx,fy,fz:double;
begin
 if not SCCreateStarted then exit;
 Canvas.Pen.Mode:=pmXor;
 Canvas.Pen.Color:=clRed;
 Canvas.MoveTo(CleaveOrgXY.X,CleaveOrgXY.Y);
 Canvas.LineTo(LastX,LastY);
 Canvas.MoveTo(CleaveOrgXY.X,CleaveOrgXY.Y);

 if not Renderer.GetGridAt(X,Y,fX,fY,fZ) then exit;
 if snaptoVertex then GetNearestVX(Level,fx,fy,fz,fx,fy,fz,0.2)
 else if SnapToGrid then Renderer.GetNearestGrid(fx,fy,fz,fx,fy,fz);

 Renderer.ProjectPoint(fx,fy,fz,X,Y);
 Canvas.LineTo(X,Y);
 LastX:=X; LastY:=Y;
end;

Procedure TJedMain.DO_EndCreateSC(X,Y:integer;snaptovertex:boolean);
var
    x1,y1,z1,x2,y2,z2:double;
    nx,ny,nz:TVector;
    vl:double;
    sc,nsc:TJKSector;
    box:TBox;
    sec:TJKSector;
begin
try
 if not SCCreateStarted then exit;

 if not Renderer.GetGridAt(X,Y,X2,Y2,Z2) then exit;
 if snaptoVertex then GetNearestVX(Level,x2,y2,z2,x2,y2,z2,0.2)
 else if SnapToGrid then Renderer.GetNearestGrid(x2,y2,z2,x2,y2,z2);

 With CleaveOrg do begin X1:=X; Y1:=Y; Z1:=Z; end;
 nx.dx:=x2-x1; nx.dy:=y2-y1; nx.dz:=z2-z1;

 vl:=Vlen(nx);
 sc:=ToolBar.GetNewShape;

 if vl<0.1 then nx:=Renderer.gxnormal
 else
 begin
  FindBBox(sc,box);
  vl:=box.x2-box.x1;
  SetVec(nx,nx.dx/vl,nx.dy/vl,nx.dz/vl);
 end;

 vl:=Vlen(nx);

 nz:=Renderer.gnormal;
 SetVec(nz,nz.dx*vl,nz.dy*vl,nz.dz*vl);

 VMult(nz.dx,nz.dy,nz.dz,nx.dx,nx.dy,nx.dz,ny.dX,ny.dY,ny.dZ);

 Normalize(ny);
 SetVec(ny,ny.dx*vl,ny.dy*vl,ny.dz*vl);

 sec:=Level.Sectors[Cur_SC];
 nsc:=Level.NewSector;
 DuplicateSector(sc,nsc,0,0,0,nx,ny,nz,x1,y1,z1);
 nsc.Assign(sec);
 Level.Sectors.Add(nsc);
 Level.RenumSecs;

 StartUndoRec('Create sector');
 SaveSecUndo(nsc,ch_added,sc_both);

 SectorAdded(nsc);
 SetCurSC(level.sectors.count-1);

finally
 SetMouseMode(MM_SELECT);
 SCCreateStarted:=false;
 Invalidate;
end;
end;


procedure TJedMain.FormMouseDown(Sender: TObject; Button: TMouseButton;
  Shift: TShiftState; X, Y: Integer);
var
    t:TJKThing;
begin
case Mouse_mode of
 MM_SELECT:;
 MM_Cleave: DO_StartCleave(X,Y,ssShift in Shift);
 MM_CreateSector: DO_StartCreateSC(X,Y,ssShift in Shift);
end;
end;

procedure TJedMain.FormMouseUp(Sender: TObject; Button: TMouseButton;
  Shift: TShiftState; X, Y: Integer);
begin
 Case Mouse_mode of
  MM_SELECT: begin
              Do_SelectAt(X,Y,ssAlt in Shift);
              if ssShift in Shift then DO_MultiSelect;
             end;
  MM_CreateSector: DO_EndCreateSC(X,Y,ssShift in Shift);
  MM_DRAG: DO_EndDrag(X,Y);
  MM_Cleave: DO_EndCleave(X,Y,ssShift in Shift);
  MM_RectSelect: DO_EndRectSelect(X,Y);
 end;
end;

procedure TJedMain.FormMouseMove(Sender: TObject; Shift: TShiftState; X,
  Y: Integer);
var msX,msY:integer;
    gX,gY,gZ:double;
begin
 Case Mouse_Mode of
  MM_SELECT: begin
              if (ssLeft in shift) and (ssCtrl in shift) then
              DO_StartDrag(X,Y) else
             if (ssLeft in shift) and (ssAlt in shift) then
              Do_StartRectSelect(X,Y);
              snaptoy:=false;
             end;
  MM_DRAG: Do_ProceedDrag(X,Y,ssShift in shift,ssAlt in shift,snaptoy);
  MM_Cleave: Do_ProceedCleave(X,Y,ssShift in Shift);
  MM_CreateSector: DO_ProceedCreateSC(X,Y,ssShift in Shift);
  MM_TranslateGrid: Do_TranslateGrid(X,Y);
  MM_TranslateCam: Do_TranslateCam(X,Y);
  MM_RotateCam: Do_RotateCam(X,Y);
  MM_RectSelect: DO_ProceedRectSelect(X,Y);
 end;

  if GetMousePos(MsX,MsY) and Renderer.GetGridAt(MsX,MsY,GX,gY,gZ) then
  PXYZ.Caption:=Format('X,Y,Z: %2.4f, %2.4f, %2.4f',[gX,gY,gZ]);

end;

Procedure TJedMain.Import3DO(const name:string);
var a3DO:T3DO;
    n,i,j,k:integer;
    mesh:T3DOMesh;
    v3:TVertex;
    v:TJKVertex;
    sec:TJKSector;
    surf:TJKSurface;
    face:T3DOFace;
    tv:TTXVertex;
    hnode:THNode;
begin
 a3DO:=T3DO.CreateFrom3DO(name,0);
 Level.Clear;
try
 for i:=0 to a3DO.Meshes.count-1 do
 begin
  mesh:=a3DO.Meshes[i];
  sec:=Level.NewSector;
  sec.Flags:=sec.Flags or SF_3DO;
  sec.layer:=Level.AddLayer(Mesh.Name);
  Level.Sectors.Add(sec);
  for j:=0 to Mesh.Vertices.count-1 do
  begin
   v3:=Mesh.Vertices[j];
   v3.num:=j;
   v:=sec.NewVertex;
   v.x:=v3.x;
   v.y:=v3.y;
   v.z:=v3.z;
  end;

  for j:=0 to mesh.faces.count-1 do
  begin
   face:=Mesh.Faces[j];
   surf:=sec.NewSurface;
   sec.surfaces.Add(surf);
   With surf do
   begin
    Adjoin:=nil;
    Material:=a3DO.GetMat(face.imat);
    geo:=face.geo;
    light:=face.light;
    tex:=face.tex;
    if face.ftype and 2<>0 then surf.FaceFlags:=surf.FaceFlags or FF_Transluent;
    extraLight:=face.extra_l;
   end;

  n:=face.vertices.count;
  for k:=n-1 downto 0 do
  begin
   surf.AddVertex(sec.vertices[face.vertices[k].num]);
   surf.TXVertices[n-k-1].Assign(face.TXVertices[k]);
  end;
  surf.Recalc;
  FindUVScales(surf);
 end;
end;

for i:=0 to a3DO.hnodes.count-1 do
begin
 hnode:=level.New3DONode;
 hnode.Assign(a3DO.hnodes[i]);
 level.h3donodes.Add(hnode);
 hnode.pivotx:=0;
 hnode.pivoty:=0;
 hnode.pivotz:=0;
end;

finally
 ClearUndoBuffer;
 Level.Things.Add(Level.NewThing);
 Level.RenumSecs;
 for i:=0 to level.sectors.count-1 do Level.Sectors[i].Renumber;
 a3DO.Free;
end;
end;

procedure TJedMain.Import1Click(Sender: TObject);
var th:TJKThing;
    I,nsec:integer;
    scale:double;
    s:string;
    a3do:T3DO;
begin
 if not AskSave then exit;

 With GetFileOpen do
 begin
  Filter:='All Importable files|*.lev;*.gob;*.asc;*.3do|DF files(*.LEV;*.GOB)|*.LEV;*.GOB|3D Studio ASC|*.asc|JK 3DOs|*.3do;*.gob';
 If Execute then
 begin
  ResettingEditor;

  s:=LowerCase(ExtractFileExt(FileName));
 if s='.3do' then
 begin
{  a3DO:=T3DO.CreateFrom3DO(FileName,0);
  a3Do.SaveToFile('i:\a.3do');
  a3Do.Free;}

  Import3DO(FileName);
 end else
 if s='.asc' then
 begin
   Level.ImportAsc(FileName);
 end
 else
 begin
  scale:=40;
  s:='40';
  if DFImport.ShowModal<>mrOK then exit;
  scale:=DFImport.VScale.AsFloat;
  Level.ImportLev(FileName,scale,DFImport.RGTX.ItemIndex);
 end;

  PMsg.Caption:='';
  LevelFile:='Untitled.jed';
  newLevel:=true;
  ResetEditor(true);
  Level.Mots:=IsMots;
  for i:=0 to Level.Things.count-1 do
  begin
   th:=Level.Things[i];
   th.z:=th.z-th.bbox.z1;
   nsec:=FindSectorForThing(th);
   if nsec<>-1 then
    th.Sec:=Level.Sectors[nsec];
   if th.sec=nil then th.layer:=Level.AddLayer('Orphan things')
   else
   begin
    s:=Level.GetLayerName(th.sec.layer);
    s:='Things'+Copy(s,6,length(s));
    th.layer:=Level.AddLayer(s);
   end;
  end;
 end;
 ToolBar.LoadLayers;
end;
end;

procedure TJedMain.FormPaint(Sender: TObject);
begin
 VerifySelection;
 VerifyMultiselection;
 Redraw;
 if updatestuff then
 begin
  EditObject;
  UpdateStuff:=false;
 end;
end;

Procedure TJedMain.SetMapMode(mm:integer);
var oldmode:integer;

procedure ToSCMsel(mode:integer);
var i:integer;
    sc,a,b:integer;
begin
 case mode of
  MM_SF: begin
          scsel.clear;
          for i:=0 to sfsel.count-1 do
          begin
           sfsel.getSCSF(i,sc,a);
           scsel.AddSC(sc);
          end;
         end;
  MM_ED: begin
          scsel.clear;
          for i:=0 to edsel.count-1 do
          begin
           edsel.getSCSFED(i,sc,a,b);
           scsel.AddSC(sc);
          end;
         end;
  MM_VX: begin
          scsel.clear;
          for i:=0 to vxsel.count-1 do
          begin
           vxsel.getSCVX(i,sc,a);
           scsel.AddSC(sc);
          end;
         end;
 end;
end;

procedure ToSFMsel(mode:integer);
var i:integer;
    sc,sf,b:integer;
    sec:TJKSector;
begin
 case mode of
  MM_SC: begin
          sfsel.clear;
          for i:=0 to scsel.count-1 do
          begin
           sc:=scsel.GetSC(i);
           sec:=level.Sectors[sc];
           for sf:=0 to sec.surfaces.count-1 do
            sfsel.AddSF(sc,sf);
          end;
         end;
  MM_ED: begin
          sfsel.clear;
          for i:=0 to edsel.count-1 do
          begin
           edsel.getSCSFED(i,sc,sf,b);
           sfsel.AddSF(sc,sf);
          end;
         end;
  MM_VX: begin
          sfsel.clear;
         end;
 end;
end;

procedure ToEDMsel(mode:integer);
var i:integer;
    sc,sf,ed:integer;
    sec:TJKSector;
begin
 case mode of
  MM_SC: begin
          edsel.clear;
          for i:=0 to scsel.count-1 do
          begin
           sc:=scsel.GetSC(i);
           sec:=level.Sectors[sc];
           for sf:=0 to sec.surfaces.count-1 do
           With sec.surfaces[sf] do
           for ed:=0 to vertices.count-1 do
            edsel.addED(sc,sf,ed);
          end;
         end;
  MM_SF: begin
          edsel.clear;
          for i:=0 to sfsel.count-1 do
          begin
           sfsel.getSCSF(i,sc,sf);
           With level.Sectors[sc].Surfaces[sf] do
           for ed:=0 to vertices.count-1 do
            edsel.AddED(sc,sf,ed);
          end;
         end;
  MM_VX: begin
          edsel.clear;
         end;
 end;
end;

procedure ToVXMsel(mode:integer);
var i:integer;
    sc,sf,ed,vx:integer;
    sec:TJKSector;
    surf:TJKSurface;
begin
 case mode of
  MM_SC: begin
          vxsel.clear;
          for i:=0 to scsel.count-1 do
          begin
           sc:=scsel.GetSC(i);
           sec:=level.Sectors[sc];
           for vx:=0 to sec.vertices.count-1 do
            vxsel.addVX(sc,vx);
          end;
         end;
  MM_SF: begin
          vxsel.clear;
          for i:=0 to sfsel.count-1 do
          begin
           sfsel.getSCSF(i,sc,sf);
           With level.Sectors[sc].surfaces[sf] do
           for vx:=0 to vertices.count-1 do
            vxsel.AddVX(sc,vertices[vx].num);
          end;
         end;
  MM_ED: begin
          vxsel.clear;
          for i:=0 to edsel.count-1 do
          begin
           edsel.getSCSFED(i,sc,sf,ed);
           surf:=level.Sectors[sc].surfaces[sf];
           vxsel.AddVX(sc,surf.vertices[ed].num);
           vxsel.AddVX(sc,surf.vertices[surf.nextVX(ed)].num);
          end;
         end;
 end;
end;

procedure ToFRMsel(mode:integer);
var i:integer;
    th,fr:integer;
    thing:TJKThing;
begin
 case mode of
  MM_TH: begin
          frsel.clear;
          for i:=0 to thsel.count-1 do
          begin
           th:=thsel.GetTH(i);
           thing:=level.Things[th];
           for fr:=0 to thing.vals.count-1 do
           if thing.Vals[fr].atype=at_frame then
            frsel.addFR(th,fr);
          end; 
         end;
 end;
end;

procedure ToTHMsel(mode:integer);
var i:integer;
    th,a,b:integer;
begin
 case mode of
  MM_FR: begin
          thsel.clear;
          for i:=0 to frsel.count-1 do
          begin
           frsel.getTHFR(i,th,a);
           thsel.AddTH(th);
          end;
         end;
 end;
end;


begin
 oldmode:=map_mode;
 map_mode:=mm;
 Case mm  of
  MM_SC: begin ToSCMsel(oldmode); SetCurSC(Cur_SC); end;
  MM_SF: begin ToSFMSel(oldmode); SetCurSF(Cur_SC,Cur_SF); end;
  MM_VX: begin ToVXMSel(oldmode); SetCurVX(Cur_SC,Cur_VX); end;
  MM_TH: begin ToTHMSel(oldmode); SetCurTH(Cur_TH); end;
  MM_FR: begin ToFRMSel(oldmode); SetCurFR(Cur_TH,Cur_FR); end;
  MM_ED: begin ToEDMSel(oldmode); SetCurED(Cur_SC,Cur_SF,Cur_ED); end;
  MM_LT: SetCurLT(Cur_LT);
  MM_Extra: SetCurEX(Cur_EX);
  else begin ItemEdit.Hide; exit; end;
 end;
 BNSC.Down:=mm=MM_SC;
 BNSF.Down:=mm=MM_SF;
 BNVX.Down:=mm=MM_VX;
 BNED.Down:=mm=MM_ED;
 BNTH.Down:=mm=MM_TH;
 BNFR.Down:=mm=MM_FR;
 BNLT.Down:=mm=MM_LT;
 Invalidate;
end;

Procedure TJedMain.SetCurSC(SC:integer);
begin
 Cur_SC:=SC;
 if Cur_SC>=Level.Sectors.Count then Cur_SC:=0;
 if Cur_SC<0 then Cur_SC:=Level.Sectors.Count-1;
 EditObject;
end;

Procedure TJedMain.SetCurSF(SC,SF:integer);
begin
 Cur_SC:=SC;
 Cur_SF:=SF;
 if SC>=Level.Sectors.Count then Cur_SC:=0;
 if Cur_SC<0 then Cur_SC:=Level.Sectors.Count-1;

 With Level.Sectors[CUR_SC] do
 begin
  if Cur_SF<0 then Cur_SF:=Surfaces.Count-1;
  if Cur_SF>=Surfaces.Count then Cur_SF:=0;
 end;
 EditObject;
end;

Procedure TJedMain.SetCurVX(SC,VX:integer);
begin
 Cur_SC:=SC; Cur_VX:=VX;
 if SC>=Level.Sectors.Count then Cur_SC:=0;
 if SC<0 then Cur_SC:=Level.Sectors.Count-1;

 With Level.Sectors[CUR_SC] do
 begin
  if VX<0 then Cur_VX:=Vertices.Count-1;
  if VX>=Vertices.Count then Cur_VX:=0;
 end;
 EditObject;
end;

Procedure TJedMain.SetCurED(SC,SF,ED:integer);
begin
 Cur_SC:=SC; Cur_SF:=SF; Cur_ED:=ED;
 if SC>=Level.Sectors.Count then Cur_SC:=0;
 if SC<0 then Cur_SC:=Level.Sectors.Count-1;

 With Level.Sectors[CUR_SC] do
 begin
  if SF<0 then Cur_SF:=Surfaces.Count-1;
  if SF>=Surfaces.Count then Cur_SF:=0;
 end;

 With Level.Sectors[Cur_SC].Surfaces[CUR_SF] do
 begin
  if Vertices.Count=0 then exit;
  if ED=-2 then Cur_ED:=Vertices.Count-1
  else if ED<0 then SetCurED(Cur_SC,Cur_SF-1,-2);
  if ED>=Vertices.Count then SetCurED(Cur_SC,Cur_SF+1,0);;
 end;

 EditObject;
end;


Procedure TJedMain.SetCurTH(TH:integer);
begin
 Cur_TH:=TH;
 if TH>=Level.Things.Count then Cur_TH:=0;
 if TH<0 then Cur_TH:=Level.Things.Count-1;
 EditObject;
end;

Procedure TJedMain.SetCurFR(th,FR:integer);
var thing:TJKThing;
begin
 Cur_TH:=TH;
 if TH>=Level.Things.Count then Cur_TH:=0;
 if TH<0 then Cur_TH:=Level.Things.Count-1;
 thing:=Level.Things[Cur_TH];

 if FR<-1 then Cur_FR:=thing.vals.count-1;
 if FR>=thing.vals.count then Cur_FR:=-1;

 EditObject;
end;

Procedure TJedMain.SetCurLT(LT:integer);
begin
 Cur_LT:=LT;
 if LT>=Level.Lights.Count then Cur_LT:=0;
 if LT<0 then Cur_LT:=Level.Lights.Count-1;
 EditObject;
end;

Procedure TJedMain.SetCurEX(EX:integer);
begin
 Cur_EX:=EX;
 if EX>=ExtraObjs.Count then Cur_EX:=0;
 if EX<0 then Cur_EX:=ExtraObjs.Count-1;
 if Assigned(OnExtraSelect) then OnExtraSelect(ExtraObjs[Cur_EX]);
 EditObject;
end;


Procedure TJedMain.SaveJKLto(const name:string);
begin
 Level.SaveToJKL(name);
end;

procedure TJedMain.Export1Click(Sender: TObject);
begin
 SaveJKL.FileName:=ChangeFileExt(ExtractName(LevelFile),'.jkl');
 if not SaveJKL.Execute then exit;
 Level.SaveToJKL(SaveJKL.FileName);
end;

Function TJedMain.GetTargetJKLName:string;
begin
 if CompareText(ExtractFileExt(LevelFile),'.jkl')=0 then
  begin result:=LevelFile; exit; end;
  
 Result:=ProjectDir+'jkl\'+ChangeFileExt(ExtractFileName(LevelFile),'.jkl');
end;

procedure TJedMain.SaveJKL1Click(Sender: TObject);
begin
 DoSaveJKL;
end;

Function TJedMain.DoSaveJKL:boolean;
var s:string;
begin
 result:=false;
 if IsInContainer(LevelFile) then
 begin
  PanMessage(mt_error,'File is in container - try "SaveJKL to"');
  exit;
 end;
 if NewLevel then
 begin
  PanMessage(mt_error,'Project was never saved');
  exit;
 end;
 s:=GetTargetJKLName;
 ForceDirectories(ExtractFilePath(s));
 DeleteFile(ProjectDir+ChangeFileExt(ExtractFileName(LevelFile),'.jkl'));
 Level.SaveToJKL(s);
 result:=true;
end;

Procedure TJedMain.ResettingEditor;
begin
 MsgForm.msgs.Clear;
end;

Procedure TJedMain.ResetEditor;
var i:integer;
begin

ClearUndoBuffer;
ClearExtraObjs;
UrqForm.Reload;

if DefaultParams then
begin
 With Renderer do
 begin
  CamX:=0;
  CamY:=0;
  CamZ:=0;
  SetZ(0,0,1);
  SetX(1,0,0);

  rPCH:=0;
  rYAW:=0;
  rROL:=0;

  GridX:=0;
  GridY:=0;
  GridZ:=0;
  SetGridNormal(0,0,1);
  SetGridXNormal(1,0,0);
  scale:=1;
 end;

 Cur_SC:=0;
 Cur_SF:=0;
 Cur_TH:=0;
 Cur_VX:=0;
 Cur_LT:=0;
 Toolbar.CBGridStep.Text:=DoubleToStr(DefGridStep);
 Toolbar.CBGridLine.Text:=DoubleToStr(DefGridLine);
 Toolbar.CBGridDot.Text:=DoubleToStr(DefGridDot);
 Toolbar.CBGridSize.Text:=DoubleToStr(DefGridSize);
 Toolbar.CBGrid.Checked:=true;
end;

 if DefaultParams then
 begin
  SetMapMode(MM_SC);
  ToolBar.SetLVis(Level.LVisString);
  SetThingView(defThingView);
  SetMSelMode(DefMSelMode);
  scsel.clear;
  sfsel.clear;
  edsel.clear;
  vxsel.clear;
  thsel.clear;
  ltsel.clear;
 end
 else
 begin
  Toolbar.RefreshLayers(false);
  SetMapMode(Map_mode);
  VerifyMultiSelection;
 end;

 SetLevelName;
 CogForm.RefreshList;

 LoadTemplates;

 LoadPlugins;

 changed:=false;
 Preview3D.ReloadLevel;
 SyncRecents;

 SetMotsIndicator;

 For i:=0 to Level.Things.count-1 do
  UpdateThingData(level.Things[i]);

  SaveTimer.Enabled:=AutoSave;
  SaveTimer.Interval:=SaveInterval*60*1000;
  SetHideLights(false);
  SetHideThings(false);
end;

Procedure TJedMain.SetLevelName;
var l:integer;
begin
 Caption:=Format('Jed - %s',[LevelFile]);
 if AskSaveAs then ProjectDir:='' else ProjectDir:=ExtractFilePath(LevelFile);

 if CompareText(ExtractFileExt(LevelFile),'.jkl')=0 then
 begin
  l:=length(ProjectDir);
  if CompareText(Copy(ProjectDir,l-3,4),'jkl\')=0 then
  SetLength(ProjectDir,l-4);
 end;

 if ProjectDir<>'' then SetCurDir(ProjectDir);
end;

procedure TJedMain.PlaceCogs1Click(Sender: TObject);
begin
 CogForm.Show;
end;

Procedure TJedMain.AddLightsAt(X,Y:integer);
var lx,ly,lz:double;
    lt,nlt:TJedLight;
    nlts,i:integer;
begin
 If not GetXYZAt(X,Y,lx,ly,lz) then exit;


 StartUndoRec('Add light(s)');

 if Level.Lights.count=0 then
 begin
  lt:=Level.NewLight;
  lt.x:=lx; lt.y:=ly; lt.z:=lz;
  Level.Lights.Add(lt);
  SaveLightUndo(lt,ch_added);
{  SaveSelLightsUndo('Add light',ch_added);}
  LevelChanged;
  exit;
 end;

 with level.Lights[Cur_LT] do begin lx:=lx-x; ly:=ly-y; lz:=lz-z; end;

 ltsel.AddLT(Cur_LT);

 nlts:=level.Lights.count;

 for i:=0 to ltsel.count-1 do
 begin
  lt:=Level.Lights[ltsel.getLT(i)];
  nlt:=Level.NewLight;
  nlt.Assign(lt);
  nlt.x:=lt.x+lx;
  nlt.y:=lt.y+ly;
  nlt.z:=lt.z+lz;
  Level.Lights.Add(nlt);
  SaveLightUndo(nlt,ch_added);
end;

 ltsel.clear;
 for i:=nlts to level.Lights.count-1 do
  ltsel.AddLT(i);

 SetCurLT(nlts);
 if ltsel.count=1 then ltsel.clear;

{ SaveSelLightsUndo('Add light(s)',ch_added);}

 LevelChanged;
 Invalidate;
end;

Procedure TJedMain.AddThingsAt(x,y:integer);
var tx,ty,tz,thx,thy,thz,pch,yaw,rol:double;
    d:double;
    oldth,th:TJKThing;
    i,j,nths:integer;
begin
 If not GetXYZAt(X,Y,tx,ty,tz) then exit;

 StartUndoRec('Add thing(s)');

 {Copy data from currently selcted thing}
 if level.Things.count=0 then
 begin
  th:=Level.NewThing;
  Level.Things.Add(th);
  SaveThingUndo(th,ch_added);
  ThingAdded(th);
  LayerThing(0);
  SaveThingUndo(th,ch_added);
{  SaveSelThingsUndo('Add thing',ch_added);}
  exit;
 end;

 With Level.Things[Cur_TH] do begin tx:=tx-x; ty:=ty-y; tz:=tz-z; end;
 thsel.AddTH(Cur_TH);

 if NewOnFloor and (thsel.count=1) then
 With Level.Things[Cur_TH] do
 begin
  tx:=tx-bbox.x1;
  ty:=ty-bbox.y1;
  tz:=tz-bbox.z1;
 end;


 nths:=Level.Things.count;

 for i:=0 to thsel.count-1 do
 begin
  oldth:=Level.Things[thsel.GetTH(i)];
  th:=Level.NewThing;
  th.Assign(oldth);
  th.x:=tx+oldth.x;
  th.y:=ty+oldth.y;
  th.z:=tz+oldth.z;

  for j:=0 to th.vals.count-1 do
  With th.Vals[j] do
  begin
   if atype<>at_frame then continue;
   GetFrame(thx,thy,thz,pch,yaw,rol);
   SetFrame(thx+tx,thy+ty,thz+tz,pch,yaw,rol);
  end;
  Th.num:=Level.Things.Count;
  Level.Things.Add(th);
  LayerThing(Th.Num);
  ThingAdded(th);
  SaveThingUndo(th,ch_added);
 end;

 thsel.Clear;

 for i:=nths to level.Things.count-1 do
     thsel.AddTH(i);
 SetCurTH(nths);
 if thsel.count=1 then thsel.clear;
{ SaveSelThingsUndo('Add thing(s)',ch_added);}
 Invalidate;
end;

Procedure TJedMain.AddSectorAt;
var tx,ty,tz:double;
    en:TVector;
begin
 If not GetXYZAt(X,Y,tx,ty,tz) then exit;
 en:=Renderer.gxnormal;
 en.dx:=en.dx*2; en.dy:=en.dy*2; en.dz:=en.dz*2;

 CreateCube(Level,tx,ty,tz,Renderer.gnormal,en);
 SetCurSC(Level.Sectors.Count-1);
 SectorAdded(Level.Sectors[Level.Sectors.Count-1]);

 StartUndoRec('Add sector');
 SaveSecUndo(Level.Sectors[Level.Sectors.Count-1],ch_added,sc_both);

 Invalidate;
end;

Procedure TJedMain.CopySectorsAt(X,Y:integer);
var dx,dy,dz:double;
    i,j,fnew:integer;
    sec,newsc:TJKSector;
    surf:TJKSurface;
begin
 If not GetXYZAt(X,Y,dx,dy,dz) then exit;
 sec:=Level.Sectors[Cur_SC];

 WIth Sec.Vertices[0] do begin dx:=dx-x; dy:=dy-y; dz:=dz-z; end;


 scsel.AddSC(Cur_SC);

 fnew:=level.Sectors.count;

 StartUndoRec('Copy sector(s)');

 for i:=0 to scsel.count-1 do
 begin
  sec:=Level.Sectors[scsel.getSC(i)];
  newsc:=Level.NewSector;
  With Renderer,Sec.Vertices[0] do
  DuplicateSector(sec,newsc,x,y,z,gxnormal,gynormal,gnormal,dx,dy,dz);
  Level.Sectors.Add(newSc);
  SaveSecUndo(newSC,ch_added,sc_both);
 end;

 Level.RenumSecs;
 scsel.clear;

 for i:=fnew to level.sectors.count-1 do
 begin
  sec:=Level.Sectors[i];
  for j:=0 to sec.surfaces.count-1 do
  begin
   surf:=sec.surfaces[j];
   if surf.nadj=1 then MakeAdjoinSCUP(surf,fnew);
  end;
  SectorAdded(sec);
  scsel.addSC(i);
 end;

 SetCurSC(fnew);
 if scsel.count=1 then scsel.clear;
 Invalidate;
end;


procedure TJedMain.CalculateLights1Click(Sender: TObject);
var sms,ems:longint;
    s:string;
    sec:TJKSector;
    scs:TSCMultiSel;
    i:integer;
    sc,sf,a:integer;
begin
 scs:=TSCMultiSel.Create;

try

 if sender=CalcLightOnLayers then
 begin
  for i:=0 to Level.Sectors.count-1 do
   if ToolBar.IsLayerVisible(Level.Sectors[i].layer) then scs.addSC(i);
 end
 else if sender=CalcLightInSel then
 begin
  case map_mode of
   MM_SC: begin
           for i:=0 to scsel.count-1 do
            scs.AddSC(scsel.GetSC(i));
          end;
   MM_SF: begin
           for i:=0 to sfsel.count-1 do
           begin
            sfsel.getSCSF(i,sc,sf);
            scs.addSC(sc);
           end;
          end;
   MM_ED: begin
           for i:=0 to edsel.count-1 do
           begin
            edsel.getSCSFED(i,sc,sf,a);
            scs.addSC(sc);
           end;
          end;
   MM_VX: begin
           for i:=0 to vxsel.count-1 do
           begin
            vxsel.getSCVX(i,sc,a);
            scs.addSC(sc);
           end;
          end;
  else begin
        ShowMessage('You must be in sector,surface,edge or vertex mode to use this option');
        exit;
       end;
  end;
 end
 else
 begin
  for i:=0 to level.sectors.count-1 do
   scs.addSC(i);
 end;


 ClearUndoBuffer;
 sms:=GetMSecs;

 if NewLightCalc then CalcLightingNew(Level,scs)
 else CalcLighting(Level,scs);

 ems:=GetMSecs;
 sms:=SubMSecs(sms,ems);
 PanMessage(mt_info,Format('%d lights processed in %s',[Level.Lights.Count,StrMSecs(sms)]));

 if Level.sectors.count=scs.count then Preview3D.ReloadLevel
 else
 for i:=0 to scs.count-1 do
  SectorChanged(level.Sectors[scs.getSC(i)]);

 LevelChanged;

finally
 scs.free;
end;
end;

procedure TJedMain.ConsistencyCheck1Click(Sender: TObject);
begin
 Consistency.Check;
end;

procedure TJedMain.CheckResources1Click(Sender: TObject);
begin
 Consistency.CheckResources;
end;

procedure TJedMain.About1Click(Sender: TObject);
begin
 Jed_About.ShowModal;
end;

procedure TJedMain.BNSCClick(Sender: TObject);
begin
 if Sender=BNSC then SetMapMode(MM_SC)
 else if Sender=BNSF then SetMapMode(MM_SF)
 else if Sender=BNED then SetMapMode(MM_ED)
 else if Sender=BNVX then SetMapMode(MM_VX)
 else if Sender=BNTH then SetMapMode(MM_TH)
 else if Sender=BNLT then SetMapMode(MM_LT)
 else if Sender=BNEX then SetMapMode(MM_Extra)
 else if Sender=BNFR then SetMapMode(MM_FR);
end;

Procedure TJedMain.NextObject;
var i,n,fi:integer;
    th:TJKThing;
begin
 case Map_mode of
 MM_SC: begin
         fi:=Cur_Sc;
         n:=fi;
         repeat
          if n>=Level.Sectors.Count-1 then n:=0 else inc(n);
          if n=fi then exit;
         until ToolBar.IsLayerVisible(Level.Sectors[n].Layer);

         SetCurSC(n);
        end;
 MM_SF: SetCurSF(Cur_SC,Cur_SF+1);
 MM_VX: SetCurVX(Cur_SC,Cur_VX+1);
 MM_ED: SetCurED(Cur_SC,Cur_SF,Cur_ED+1);
 MM_TH: begin
         fi:=Cur_TH;
         n:=fi;
         repeat
          if n>=Level.Things.Count-1 then n:=0 else inc(n);
          if n=fi then exit;
         until ToolBar.IsLayerVisible(Level.Things[n].Layer);
         SetCurTH(n);
        end;
 MM_FR: begin
         th:=Level.Things[Cur_TH];
         Cur_FR:=th.NextFrame(Cur_FR);
         SetCurFR(Cur_TH,Cur_FR);
        end;
 MM_LT: begin
         fi:=Cur_LT;
         n:=fi;
         repeat
          if n>=Level.Lights.Count-1 then n:=0 else inc(n);
          if n=fi then exit;
         until ToolBar.IsLayerVisible(Level.Lights[n].Layer);
         SetCurLT(n);
        end;
 MM_Extra: SetCurLT(Cur_EX+1);
 end;
 Invalidate;
end;

Procedure TJedMain.PreviousObject;
var i,fi,n:integer;
    th:TJKThing;
begin
 case Map_mode of
 MM_SC: begin
         fi:=Cur_Sc;
         n:=fi;
         repeat
          if n<1 then n:=Level.Sectors.Count-1 else dec(n);
          if n=fi then exit;
         until ToolBar.IsLayerVisible(Level.Sectors[n].Layer);
         SetCurSC(n);
        end;
 MM_SF: SetCurSF(Cur_SC,Cur_SF-1);
 MM_VX: SetCurVX(Cur_SC,Cur_VX-1);
 MM_ED: SetCurED(Cur_SC,Cur_SF,Cur_ED-1);
 MM_TH: begin
         fi:=Cur_TH;
         n:=fi;
         repeat
          if n<1 then n:=Level.Things.Count-1 else dec(n);
          if n=fi then exit;
         until ToolBar.IsLayerVisible(Level.Things[n].Layer);
         SetCurTH(n);
        end;
 MM_FR: begin
         th:=Level.Things[Cur_TH];
         if Cur_FR=-1 then Cur_FR:=th.Vals.count-1 else
         Cur_FR:=th.PrevFrame(Cur_FR);
         SetCurFR(Cur_TH,Cur_FR);
        end;
 MM_LT: begin
         fi:=Cur_LT;
         n:=fi;
         repeat
          if n<1 then n:=level.Lights.count-1 else dec(n);
          if n=fi then exit;
         until ToolBar.IsLayerVisible(Level.Lights[n].Layer);
         SetCurLT(n);
        end;
 MM_Extra: SetCurEX(Cur_EX-1);
 end;
 Invalidate;
end;

Procedure TJedMain.NextObjectInSurface;
var surf:TJKSurface;
begin
case map_mode of
 MM_ED:  begin
          surf:=Level.Sectors[Cur_SC].Surfaces[Cur_SF];
          if Cur_ED>=surf.vertices.count-1 then SetCurED(Cur_SC,Cur_SF,0)
          else SetCurED(Cur_SC,Cur_SF,Cur_ED+1);
         end;

end;
end;

Procedure TJedMain.PreviousObjectInSurface;
var surf:TJKSurface;
begin
case map_mode of
 MM_ED:  begin
          surf:=Level.Sectors[Cur_SC].Surfaces[Cur_SF];
          if Cur_ED<=0 then SetCurED(Cur_SC,Cur_SF,surf.vertices.count-1)
          else SetCurED(Cur_SC,Cur_SF,Cur_ED-1);
         end;

end;
end;


Function TJedMain.GetXYZAt(atX,atY:integer;var X,Y,Z:double):boolean;
begin
 Result:=false;
 Result:=Renderer.GetGridAt(atx,aty,x,y,z);
 if not result then exit;
 if SnapToGrid then Renderer.GetNearestGrid(x,y,z,x,y,z);
end;


Function TJedMain.GetMousePos(var X,Y:integer):boolean;
var    pt:TPoint;
begin
 result:=false;
 GetCursorPos(Pt);
 pt := ScreenToClient(pt);
 X:=pt.X; Y:=pt.Y;
 if not PtInRect(ClientRect,pt) then exit;
 Result:=true;
end;

Procedure TJedMain.SetMouseMode(mm:integer);
begin
 Case mm of
  MM_SELECT: CurSor:=crDefault;
  MM_Cleave: Cursor:=crSaber;
  MM_Drag: Cursor:=crDrag;
  MM_TranslateCam: Cursor:=crSize;
  MM_RotateCam: Cursor:=crDefault;
  MM_TranslateGrid: Cursor:=crSize;
  MM_RotateGrid: Cursor:=crDefault;
  MM_CreateSector: CurSor:=crDefault;
  MM_RectSelect:CurSor:=crDefault;
 else exit;
 end;
 mouse_mode:=mm;
end;

Procedure TJedMain.EditObject;
begin
 if not IsSelValid then exit;
 case Map_mode of
  MM_SC: ItemEdit.LoadSector(Level.Sectors[Cur_SC]);
  MM_SF: ItemEdit.LoadSurface(Level.Sectors[Cur_SC].Surfaces[Cur_SF]);
  MM_VX: ItemEdit.LoadVertex(Level.Sectors[Cur_SC].Vertices[Cur_VX]);
  MM_TH: ItemEdit.LoadThing(Level.Things[Cur_TH]);
  MM_LT: ItemEdit.LoadLight(Cur_LT);
  MM_ED: ItemEdit.LoadEdge(Level.Sectors[Cur_SC].Surfaces[Cur_SF],Cur_ED);
  MM_FR: ItemEdit.LoadFrame(Cur_TH,Cur_FR);
  MM_EXTRA: ItemEdit.LoadExtra(Cur_EX);
 end;
end;

Procedure TJedMain.GotoXYZ(x,y,z:double;force:boolean);
begin
 if not force then
 begin
  Renderer.BeginRectPick(0,0,ClientWidth,ClientHeight);
  force:=not Renderer.IsVertexInRect(X,Y,Z);
 end;

 if not force then exit;
 Renderer.camX:=-X;
 Renderer.CamY:=-Y;
 Renderer.CamZ:=-Z;
 Invalidate;
end;

Procedure TJedMain.CenterViewOnObject;
var cx,cy,cz,a:double;
begin
 case map_mode of
  MM_SC: With Level.Sectors[Cur_SC].vertices[0] do
         begin
          cx:=x;
          cy:=y;
          cz:=z;
         end;
  MM_SF: With Level.Sectors[Cur_SC].Surfaces[Cur_SF].Vertices[0] do
         begin
          cx:=x;
          cy:=y;
          cz:=z;
         end;
 MM_ED: With Level.Sectors[Cur_SC].Surfaces[Cur_SF].Vertices[Cur_ED] do
         begin
          cx:=x;
          cy:=y;
          cz:=z;
         end;
 MM_VX: With Level.Sectors[Cur_SC].Vertices[Cur_VX] do
         begin
          cx:=x;
          cy:=y;
          cz:=z;
         end;
 MM_TH: With Level.Things[Cur_TH] do
        begin
         cx:=x;
         cy:=y;
         cz:=z;
        end;
 MM_FR: if cur_FR<>-1 then
        With Level.Things[Cur_TH].Vals[Cur_FR] do
        begin
         GetFrame(cx,cy,cz,a,a,a);
        end else
        With Level.Things[Cur_TH] do
        begin
         cx:=x;
         cy:=y;
         cz:=z;
        end;
 MM_LT: With Level.Lights[Cur_LT] do
        begin
         cx:=x;
         cy:=y;
         cz:=z;
        end;
 else exit;
end;

 GotoXYZ(cx,cy,cz,force);
end;

Procedure TJedMain.GotoSC(sc:integer);
begin
 SetCurSC(SC);
 SetMapMode(MM_SC);
 CenterViewOnObject(false);
end;

Procedure TJedMain.GotoSF(sc,sf:integer);
begin
 SetCurSF(Sc,Sf);
 SetMapMode(MM_SF);
 CenterViewOnObject(false);
end;

Procedure TJedMain.GotoTH(th:Integer);
begin
 SetCurTH(TH);
 SetMapMode(MM_TH);
 CenterViewOnObject(false);
end;


procedure TJedMain.ItemEditor1Click(Sender: TObject);
begin
 ItemEdit.Show;
end;

procedure TJedMain.FormKeyUp(Sender: TObject; var Key: Word;
  Shift: TShiftState);
begin
 case key of
  Ord('G'): SetMouseMode(MM_SELECT);
  Ord('R'): SetMouseMode(MM_SELECT);
  VK_Space: SetMouseMode(MM_SELECT);
 end;
end;

procedure TJedMain.MUNextObjectClick(Sender: TObject);
begin
 NextObject;
end;

procedure TJedMain.MUPrevObjectClick(Sender: TObject);
begin
 PreviousObject;
end;

Procedure TJedMain.SnapGridToObject;
var rx,ry,rz,p,y,r:double;
    norm:TVector;
    av,xnorm:TVector;
    v1,v2:TJKVertex;
    sc,vx,i:integer;
begin
 norm:=Renderer.gnormal;
 xnorm:=Renderer.gxnormal;
 case map_mode of
  MM_SC: begin
          With Level.Sectors[Cur_SC].Vertices[0] do begin rx:=x; ry:=y; rz:=z; end;
         end;
  MM_SF: With Level.Sectors[Cur_SC].Surfaces[Cur_SF] do
         begin
          v1:=Vertices[0];
          v2:=Vertices[1];
          with v1 do begin rx:=x; ry:=y; rz:=z; end;
          norm:=normal;
          xnorm.dx:=v2.x-v1.x;
          xnorm.dy:=v2.y-v1.y;
          xnorm.dz:=v2.z-v1.z;
         end;
  MM_ED: With Level.Sectors[Cur_SC].Surfaces[Cur_SF] do
         begin
          v1:=Vertices[Cur_ED];
          v2:=Vertices[NextVX(Cur_ED)];
          with v1 do begin rx:=x; ry:=y; rz:=z; end;
          norm:=normal;
          xnorm.dx:=v2.x-v1.x;
          xnorm.dy:=v2.y-v1.y;
          xnorm.dz:=v2.z-v1.z;
         end;
  MM_VX: begin
          i:=vxsel.AddVX(Cur_SC,Cur_VX);
          if vxsel.count=3 then
          begin
           vxsel.GetSCVX(0,sc,vx);
           v1:=Level.Sectors[sc].Vertices[vx];
           vxsel.GetSCVX(1,sc,vx);
           v2:=Level.Sectors[sc].Vertices[vx];
           SetVec(xnorm,v2.x-v1.x,v2.y-v1.y,v2.z-v1.z);
           vxsel.GetSCVX(2,sc,vx);
           v1:=Level.Sectors[sc].Vertices[vx];
           SetVec(av,v1.x-v2.x,v1.y-v2.y,v1.z-v2.z);
           VMult(xnorm.dx,xnorm.dy,xnorm.dz,
                 av.dx,av.dy,av.dz,
                 norm.dx,norm.dy,norm.dz);
          end;
          vxsel.DeleteN(i);
          With Level.Sectors[Cur_SC].Vertices[Cur_VX] do begin rx:=x; ry:=y; rz:=z; end;
         end;
  MM_TH: With Level.Things[Cur_TH] do begin rx:=x; ry:=y; rz:=z; end;
  MM_FR: if Cur_FR=-1 then With Level.Things[Cur_TH] do begin rx:=x; ry:=y; rz:=z; end
         else with Level.Things[Cur_TH].Vals[Cur_FR] do
         begin
          GetFrame(rx,ry,rz,p,y,r);
         end;
  MM_LT: With Level.Lights[Cur_LT] do begin rx:=x; ry:=y; rz:=z; end;
  else exit;
 end;
 With Renderer do
 begin
  GridX:=rx;
  GridY:=ry;
  GridZ:=rz;
  With norm do SetGridNormal(dx,dy,dz);
  With xnorm do SetGridXNormal(dx,dy,dz);
  Invalidate;
 end;
end;

procedure TJedMain.MUSnapGridToClick(Sender: TObject);
begin
 SnapGridToObject;
end;

Procedure TJedMain.LayerThings;
var n,a,i:integer;
begin
 if Map_Mode<>MM_TH then exit;
 n:=0;
 a:=thsel.AddTH(Cur_TH);
 for i:=0 to thsel.count-1 do
 if LayerThing(thsel.getTH(i)) then inc(n);
 thsel.DeleteN(a);
 PanMessageFmt(mt_info,'%d thing(s) layered',[n]);
end;

Function TJedMain.LayerThing(n:integer):boolean;
var nsec:integer;
    thing:TJKThing;
begin
 result:=false;
 thing:=Level.Things[n];
 nsec:=FindSectorForThing(Thing);

 if nsec=-1 then thing.sec:=nil  else thing.Sec:=Level.Sectors[nsec];
 result:=nsec<>-1;
 ThingChanged(thing);
end;

procedure TJedMain.ToolWindow1Click(Sender: TObject);
begin
 ToolForm.Show;
end;

procedure TJedMain.FormShow(Sender: TObject);
begin
 CreateRenderer;

 NewProject;
 if paramstr(1)<>'' then OpenProject(Paramstr(1),op_open);

 ResetEditor(true);
 ToolBar.SetDefaults;
end;

procedure TJedMain.Options1Click(Sender: TObject);
begin
 With Options do
 begin
  SetOptions(nil);
  if IsVarChanged(wireframeAPI) or IsVarChanged(WF_DoubleBuf) then CreateRenderer;
 end;

 if MapRot=MR_Old then GetPYR;
 Invalidate;
end;

procedure TJedMain.CancelMouseMode;
begin
 case Mouse_mode of
  MM_Cleave: begin Invalidate; cleavestarted:=false; end;
  MM_RectSelect: begin Invalidate; RectSelStarted:=false; end;
 end;
 SetMouseMode(MM_SELECT);
end;

procedure TJedMain.New1Click(Sender: TObject);
begin
 if Not AskSave then exit;
 ResettingEditor;

 PMsg.Caption:='';
 SetMots(false);
 NewProject;
 ResetEditor(true);
end;

procedure TJedMain.NewMOTSProject1Click(Sender: TObject);
begin
 if Not AskSave then exit;
 ResettingEditor;
 PMsg.Caption:='';
 SetMots(true);
 NewProject;
 ResetEditor(true);
end;

procedure TJedMain.FormDestroy(Sender: TObject);
var i:integer;
begin
 MWMaxed:=WindowState=wsMaximized;
 if not MWMaxed then GetWinPos(Self,MWinPos);
 for i:=0 to Level.Things.Count-1 do Free3DO(Level.Things[i].a3DO);
 Renderer.Free;
 Renderer:=nil;
end;

Procedure TJedMain.Goto_Adjoin;
var sf:TJKSurface;
begin
 sf:=Level.Sectors[Cur_SC].Surfaces[Cur_SF];
 if sf.adjoin=nil then exit;

 SetCurSF(sf.Adjoin.Sector.num,sf.Adjoin.num);
 Invalidate;
end;

procedure TJedMain.GobProject1Click(Sender: TObject);
var gobname:string;
    idir,s,ext:string;
begin
 ext:=UpperCase(ExtractFileExt(LevelFile));
{ if Ext='.JED' then Idir:=ProjectDir else iDir:=ExtractFilePath(LevelFile);}
 gobname:=ExtractFilePath(SaveGob.FileName);
 if gobname='' then gobname:=GameDir+'EPISODE\';

 if isMots then
 begin
  SaveGob.FileName:=gobname+ChangeFileExt(ExtractFileName(LevelFile),'.goo');
  SaveGob.FilterIndex:=2;
  SaveGob.DefaultExt:='goo';
 end
 else
 begin
  SaveGob.FileName:=gobname+ChangeFileExt(ExtractFileName(LevelFile),'.gob');
  SaveGob.FilterIndex:=1;
  SaveGob.DefaultExt:='gob';
 end;

 if not SaveGob.Execute then exit;
 GobProj(SaveGob.FileName);
end;

Function TJedMain.GobProj(const name:string):boolean;
var
    gob:TGOB2Creator;

    slist,flist,ls:TStringList;
    i:integer;
    f:TFile;
    njkls:integer;
    s,ext:string;

    checklist:TStringList;
    extlist:TSTringList;

Procedure InitSmartGOB;
var i:integer;
    id:TITEMSDATFile;
    md:TMODELSDATFile;

begin
 checkList:=TStringList.Create;
 extlist:=TStringList.Create;
 checklist.sorted:=true;
 checklist.duplicates:=dupignore;

 With extlist do
 begin
  Sorted:=true;
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

 for i:=0 to slist.count-1 do
 if CompareText(ExtractFileExt(slist[i]),'.jkl')=0 then
  LoadJKLLists(ProjectDir+slist[i],checklist);

 id:=TITEMSDATFile.Create;
 id.Load('items.dat');
 checklist.AddStrings(id.cogs);
 id.Free;

 md:=TMODELSDATFile.Create;
 md.Load('models.dat');
 checklist.AddStrings(md.snds);
 checklist.AddStrings(md.a3dos);
 md.free;

end;

Procedure CheckFlist(slist,flist:TStringList);
var i:integer;
    s:string;
begin
 for i:=slist.count-1 downto 0 do
 begin
  s:=extractFileName(slist[i]);
  if extlist.IndexOf(ExtractFileExt(s))=-1 then continue;
  if checklist.IndexOf(s)=-1 then
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

Procedure AddFiles(const subdir,mask:string);
var i,n:integer;
begin
 ListDirMask(ProjectDir+subdir,mask,ls);
 for i:=0 to ls.count-1 do
 begin
  n:=flist.IndexOf(subdir+ls[i]);
  if n<>-1 then
  begin
   PanMessageFmt(mt_warning,'Duplicate files in project directory: %s and %s',
                 [slist[n],subdir+ls[i]]);
   slist[n]:=subdir+ls[i];
  end
  else
  begin
   flist.Add(subdir+ls[i]);
   slist.Add(subdir+ls[i]);
  end;

  if mask='*.jkl' then Inc(njkls);
 end;
end;

begin
 result:=false;
 if NewLevel then
 begin
  PanMessage(mt_error,'Project was never saved');
  exit;
 end;
 if IsInContainer(LevelFile) then
 begin
  PanMessage(mt_error,'Level is in container - can''t gob');
  exit;
 end;

 gob:=TGOB2Creator.Create(name);
 flist:=TStringList.Create;
 slist:=TStringList.Create;
 ls:=TStringList.Create;
 njkls:=0;
 try
  ListDirMask(ProjectDir,'*.*',ls);
  for i:=0 to ls.count-1 do
  begin
    s:=ls[i];
    ext:=UpperCase(ExtractFileExt(s));
   if ext='.JK' then flist.Add(s)
   else if ext='.JKL' then begin flist.Add('jkl\'+s); inc(njkls); end
   else if ext='.COG' then flist.Add('cog\'+s)
   else if ext='.3DO' then flist.Add('3do\'+s)
   else if ext='.MAT' then flist.Add('mat\'+s)
   else if ext='.DAT' then flist.Add('misc\'+s)
   else if ext='.UNI' then
   begin
    if (CompareText(ExtractFileName(s),'cogstrings.uni')=0) or (CompareText(ExtractFileName(s),'sithstrings.uni')=0)
    then flist.Add('misc\'+s) else flist.Add('ui\'+s);
   end
   else if ext='.KEY' then flist.Add('3do\key\'+s)
   else if (ext='.AI') or (ext='.AI0') or (ext='.AI2') then flist.Add('misc\ai\'+s)
   else if ext='.CMP' then flist.Add('misc\cmp\'+s)
   else if ext='.PAR' then flist.Add('misc\par\'+s)
   else if ext='.PER' then flist.Add('misc\per\'+s)
   else if ext='.PUP' then flist.Add('misc\pup\'+s)
   else if ext='.SND' then flist.Add('misc\snd\'+s)
   else if ext='.SPR' then flist.Add('misc\spr\'+s)
   else if ext='.WAV' then flist.Add('sound\'+s)
   else if ext='.BM' then flist.Add('ui\bm\'+s)
   else if ext='.SFT' then flist.Add('ui\sft\'+s)
   else if ext='.SMK' then flist.Add('video\'+s)
   else if ext='.SAN' then flist.Add('video\'+s);
  end;

  for i:=0 to Flist.count-1 do
    slist.Add(ExtractFileName(Flist[i]));


  AddFiles('cog\','*.cog');
  AddFiles('jkl\','*.jkl');
  AddFiles('3do\','*.3do');
  AddFiles('3do\mat\','*.mat');
  AddFiles('mat\','*.mat');
  AddFiles('misc\','*.uni');
  AddFiles('ui\','*.uni');
  AddFiles('misc\','*.dat');
  AddFiles('3do\key\','*.key');
  AddFiles('misc\ai\','*.ai;*.ai2;*.ai0');
  AddFiles('misc\cmp\','*.cmp');
  AddFiles('misc\par\','*.par');
  AddFiles('misc\per\','*.per');
  AddFiles('misc\pup\','*.pup');
  AddFiles('misc\snd\','*.snd');
  AddFiles('misc\spr\','*.spr');
  AddFiles('sound\','*.wav');
  AddFiles('voice\','*.wav');
  AddFiles('voiceuu\','*.wav');
  AddFiles('ui\bm\','*.bm');
  AddFiles('ui\sft\','*.sft');

  if njkls=0 then
   PanMessage(mt_warning,'No JKL files found in project directory!');

  if GOBSmart then InitSmartGob;
  if GOBSmart then CheckFlist(slist,flist);


  Progress.Reset(flist.count);

  gob.PrepareHeader(flist);
  for i:=0 to flist.count-1 do
  begin
   Progress.Step;
   f:=OpenFileRead(ProjectDir+sList[i],0);
   gob.AddFile(f);
   f.Fclose;
  end;

 finally
  if GOBSmart then DoneSmartGOB;
  gob.Free;
  ls.free;
  flist.free;
  slist.free;
  Progress.hide;
 end;
 result:=true;
end;


procedure TJedMain.Viewtogrid1Click(Sender: TObject);
begin
 SetViewToGrid;
end;

procedure TJedMain.GridtoView1Click(Sender: TObject);
begin
 SetGridToView;
end;

Procedure TJedMain.SetGridToView;
var x,z:TVector;
begin
 SetVec(x,1,0,0);
 SetVec(z,0,-1,0);

 x:=Renderer.xv;
 z:=Renderer.zv;

 Renderer.SetGridNormal(z.dx,z.dy,z.dz);
 Renderer.SetGridXNormal(x.dx,x.dy,x.dz);
 Invalidate;
end;


Procedure TJedMain.SetViewToGrid;
begin

With Renderer do
begin
 With gnormal do SetZ(dx,dy,dz);
 With gxnormal do SetX(dx,dy,dz);

 CamX:=-GridX;
 camY:=-GridY;
 CamZ:=-GridZ;
 Invalidate;
end;

if MapRot=MR_old then
begin
 GetPYR;
 SetRendFromPYR;
end;

end;

procedure TJedMain.Toolbar1Click(Sender: TObject);
begin
 Toolbar.Show;
end;

procedure TJedMain.Topics1Click(Sender: TObject);
begin
 Application.HelpFile:=BaseDir+'Jedhelp.hlp';
 Application.HelpContext(10);
end;

procedure TJedMain.JedTutor1Click(Sender: TObject);
begin
 Application.HelpFile:=BaseDir+'Jedtutor.hlp';
 Application.HelpContext(10);
end;

procedure TJedMain.N3DPreview1Click(Sender: TObject);
begin
 Preview3D.ShowPreview;
end;

Procedure TJedMain.ScaleTexture(how:integer);
var du,dv:double;
    surf:TJKSurface;
    i,n,sc,sf:integer;
    u,v:TVector;

Procedure ScaleSurf(surf:TJKSurface);
begin
 SaveSecUndo(surf.sector,ch_changed,sc_geo);
 case how of
  st_up: begin
          surf.uscale:=surf.uscale*TxScaleStep;
          surf.vscale:=surf.vscale*TxScaleStep;
         end;
  st_down: begin
            surf.uscale:=surf.uscale/TxScaleStep;
            surf.vscale:=surf.vscale/TxScaleStep;
           end;
  else exit;
 end;
 CalcUVNormals(surf,u,v);
 ArrangeTexture(surf,0,u,v);
 SectorChanged(surf.sector);
end;

begin
 if map_mode<>MM_SF then SetMapMode(MM_SF);
 if not Preview3D.IsActive then exit;

 StartUndoRec('Scale texture');

 n:=sfsel.AddSF(Cur_SC,Cur_SF);

 for i:=0 to sfsel.count-1 do
 begin
  sfsel.getSCSF(i,sc,sf);
  scaleSurf(Level.Sectors[sc].Surfaces[sf]);
 end;

 sfsel.DeleteN(n);

end;


Procedure TJedMain.ShiftTexture(how:integer);
var du,dv:double;
    surf:TJKSurface;
    i,sc,sf,n:integer;

procedure ShiftSurf(surf:TJKSurface);
var i:integer;
begin
 SaveSecUndo(surf.sector,ch_changed,sc_geo);

 for i:=0 to surf.TXVertices.count-1 do
 With surf.TXVertices[i] do
 begin
  u:=u+du;
  v:=v+dv;
 end;
 SectorChanged(surf.sector);
end;

begin
 if map_mode<>MM_SF then SetMapMode(MM_SF);
 if not Preview3D.IsActive then exit;
 case how of
  st_left: begin du:=TXStep; dv:=0; end;
  st_right: begin du:=-TXStep; dv:=0; end;
  st_up: begin du:=0; dv:=-TXStep; end;
  st_down: begin du:=0; dv:=TXStep; end;
  else exit;
 end;

 StartUndoRec('Shift texture');

 n:=sfsel.AddSF(Cur_SC,Cur_SF);

 for i:=0 to sfsel.count-1 do
 begin
  sfsel.GetSCSF(i,sc,sf);
  ShiftSurf(Level.Sectors[sc].Surfaces[sf]);
 end;

 sfsel.DeleteN(n);
end;

Procedure TJedMain.RotateTexture(how:integer);
var nu,nv,u,v:tvector;
    surf:TJKSurface;
    du,dv,cosa,sina:double;
    fv:integer;

begin
 if not Preview3D.IsActive then exit;

 fv:=0;
 case map_mode of
  MM_ED: fv:=Cur_ED;
  MM_SF: fv:=0;
  else SetMapMode(MM_SF);
 end;

 case how of
  st_left: begin cosa:=cos(-TXRotStep*pi/180); sina:=sin(-TXRotStep*pi/180); end;
  st_right: begin cosa:=cos(TXRotStep*pi/180); sina:=sin(TXRotStep*pi/180); end;
 end;

 StartUndoRec('Rotate texture');

 surf:=Level.Sectors[cur_SC].Surfaces[Cur_SF];

 SaveSecUndo(surf.sector,ch_changed,sc_geo);


 CalcUVNormals(surf,u,v);

  nv.dx:=-sina*u.dx+cosa*v.dx;
  nv.dy:=-sina*u.dy+cosa*v.dy;
  nv.dz:=-sina*u.dz+cosa*v.dz;

  nu.dx:=cosa*u.dx+sina*v.dx;
  nu.dy:=cosa*u.dy+sina*v.dy;
  nu.dz:=cosa*u.dz+sina*v.dz;

  UpdateSurfUVData(surf,nu,nv);
  ArrangeTexture(surf,fv,nu,nv);
  SectorChanged(surf.sector);
end;

procedure TJedMain.PMsgDblClick(Sender: TObject);
begin
 PMsg.caption:='';
end;

procedure TJedMain.Messages1Click(Sender: TObject);
begin
 MsgForm.Show;
end;

Procedure TJedMain.SectorChanged(s:TJKSector);
begin
 Preview3D.UpdateSector(s);
 LevelChanged;
end;

Procedure TJedMain.SectorAdded(s:TJKSector);
begin
 Preview3D.AddSector(s);
 LevelChanged;
end;

Procedure TJedMain.SectorDeleted(s:TJKSector);
begin
 Preview3D.DeleteSector(s);
 LevelChanged;
end;

Procedure TJedMain.ThingChanged(th:TJKThing);
begin
 UpdateThingData(th);
 Preview3D.UpdateThing(th);
 LevelChanged;
end;

Procedure TJedMain.ThingAdded(th:TJKThing);
begin
 UpdateThingData(th);
 Preview3D.AddThing(th);
 LevelChanged;
end;

Procedure TJedMain.ThingDeleted(th:TJKThing);
begin
 Preview3D.DeleteThing(th);
 LevelChanged;
end;

Procedure TJedMain.RotateObject(angle:double; axis:integer);
var i:integer;
    vec:TVector;
    cx,cy,cz,a:double;
    sec:TJKSector;
    th:TJKThing;
    lt:TJEDLight;
begin

 case axis of
  rt_x: SetVec(vec,1,0,0);
  rt_y: SetVec(vec,0,1,0);
  rt_z: SetVec(vec,0,0,1);
  rt_grid: begin vec:=Renderer.Gnormal; cx:=Renderer.GridX; cy:=Renderer.GridY; cz:=Renderer.GridZ; end;
  else exit;
 end;

 case map_mode of
  MM_SC: begin
          sec:=Level.Sectors[Cur_SC];
          if axis<>rt_grid then CalcSecCenter(sec,cx,cy,cz);
          StartUndoRec('Rotate sector(s)');
          i:=scsel.AddSC(Cur_SC);
          RotateSectors(level,scsel,vec,cx,cy,cz,angle);
          scsel.DeleteN(i);
         end;
  MM_TH: begin
          th:=Level.Things[Cur_TH];
          if axis<>rt_grid then begin cx:=th.x; cy:=th.y; cz:=th.z; end;

          StartUndoRec('Rotate thing(s)');
          i:=thsel.AddTH(Cur_TH);
          RotateThings(level,thsel,vec,cx,cy,cz,angle);
          thsel.DeleteN(i);
         end;
  MM_LT: begin
          lt:=Level.Lights[Cur_LT];
          if axis<>rt_grid then begin cx:=lt.x; cy:=lt.y; cz:=lt.z; end;

          StartUndoRec('Rotate light(s)');
          i:=ltsel.AddLT(Cur_LT);
          RotateLights(level,ltsel,vec,cx,cy,cz,angle);
          ltsel.DeleteN(i);
         end;
  MM_FR: begin
          if axis<>rt_grid then
          begin
           if Cur_FR<0 then with Level.Things[Cur_TH] do begin cx:=x; cy:=y; cz:=z; end
           else Level.Things[Cur_TH].Vals[Cur_FR].GetFrame(cx,cy,cz,a,a,a);
          end;

          StartUndoRec('Rotate frame(s)');
          i:=frsel.AddFR(Cur_TH,Cur_FR);
          RotateFrames(level,frsel,vec,cx,cy,cz,angle);
          frsel.DeleteN(i);
         end;
 end;
Invalidate;
end;

Procedure TJedMain.FlipObject(how:integer);
var cx,cy,cz,a:double;
    i:integer;
    vec:TVector;
begin
 case map_mode of
  MM_SC: begin
          StartUndorec('Flip sector(s)');
          i:=scsel.AddSC(Cur_SC);
          if how=rt_grid then FlipSectorsOverPlane(level,scsel,Renderer.gnormal,Renderer.GridX,Renderer.Gridy,Renderer.GridZ)
          else
          begin
           CalcSecCenter(level.sectors[Cur_SC],cx,cy,cz);
           StartUndoRec('Flip Sectors');
           FlipSectors(level,scsel,cx,cy,cz,how);
          end;
          scsel.DeleteN(i);
         end;
  MM_TH: begin
          StartUndorec('Flip thing(s)');
          {SaveSelThingsUndo('Change thing(s)',ch_changed);}
          i:=thsel.AddTH(Cur_TH);

          if how=rt_grid then FlipThingsOverPlane(level,thsel,Renderer.gnormal,Renderer.GridX,Renderer.Gridy,Renderer.GridZ)
          else With Level.Things[Cur_TH] do FlipThings(level,thsel,x,y,z,how);
          thsel.DeleteN(i);
         end;
  MM_LT: begin
          StartUndorec('Flip light(s)');
          {SaveSelThingsUndo('Change thing(s)',ch_changed);}
          i:=ltsel.AddLT(Cur_LT);

          With Level.lights[Cur_LT] do
          begin
           cx:=x; cy:=y; cz:=z;
          end;

          case how of
           rt_x: SetVec(vec,1,0,0);
           rt_y: SetVec(vec,0,1,0);
           rt_z: SetVec(vec,0,0,1);
           rt_grid: begin
                     vec:=Renderer.gnormal;
                     cx:=Renderer.GridX;
                     cy:=Renderer.GridY;
                     cz:=Renderer.GridZ;
                    end;
          end;
          FlipLightsOverPlane(level,ltsel,vec,cx,cy,cz);
          ltsel.DeleteN(i);
         end;
  MM_FR: begin
          StartUndorec('Flip frame(s)');
          {SaveSelThingsUndo('Change thing(s)',ch_changed);}
          i:=frsel.AddFR(Cur_TH,Cur_FR);

          if Cur_FR<0 then With Level.Things[Cur_TH] do
          begin
           cx:=x; cy:=y; cz:=z;
          end else Level.Things[Cur_TH].Vals[Cur_FR].GetFrame(cx,cy,cz,a,a,a);

          case how of
           rt_x: SetVec(vec,1,0,0);
           rt_y: SetVec(vec,0,1,0);
           rt_z: SetVec(vec,0,0,1);
           rt_grid: begin
                     vec:=Renderer.gnormal;
                     cx:=Renderer.GridX;
                     cy:=Renderer.GridY;
                     cz:=Renderer.GridZ;
                    end;
          end;
          FlipFramesOverPlane(level,frsel,vec,cx,cy,cz);
          frsel.DeleteN(i);
         end;
 end;
Invalidate;
end;


Procedure TJedMain.ScaleObject(sfactor:double;how:integer);
var sd:TScaleData;
    a:double;
    i:integer;
begin
 sd.sfactor:=sfactor;
 if (how and sc_ScaleX<>0) then sd.how:=scale_x else
 if (how and sc_ScaleY<>0) then sd.how:=scale_y else
 if (how and sc_ScaleZ<>0) then sd.how:=scale_z else
 if (how and sc_ScaleGrid<>0) then
 begin
  sd.how:=scale_vec;
  sd.vec:=Renderer.GNormal;
 end
 else sd.how:=scale_XYZ;

 case map_mode of
  MM_SC: begin
          StartUndoRec('Scale sector(s)');
          CalcSecCenter(Level.Sectors[Cur_SC],sd.cx,sd.cy,sd.cz);
          i:=scsel.AddSC(cur_SC);
          ScaleSectors(level,scsel,sd,how and sc_ScaleTX<>0);
          scsel.DeleteN(i);
         end;
  MM_TH: begin
          StartUndoRec('Scale thing(s)');
          i:=thsel.AddTH(Cur_TH);
          With Level.Things[Cur_TH] do
          begin
           sd.cx:=x; sd.cy:=y; sd.cz:=z;
          end;
          ScaleThings(level,thsel,sd);
          thsel.DeleteN(i);
         end;
  MM_LT: begin
          StartUndoRec('Scale light(s)');
          i:=ltsel.AddLT(Cur_LT);
          With Level.Lights[Cur_LT] do
          begin
           sd.cx:=x; sd.cy:=y; sd.cz:=z;
          end;
          ScaleLights(level,ltsel,sd);
          ltsel.DeleteN(i);
         end;
  MM_FR: begin
          StartUndoRec('Scale light(s)');
          i:=frsel.AddFR(Cur_TH,Cur_FR);
          if Cur_FR=-1 then With Level.Things[Cur_TH] do begin sd.cx:=x; sd.cy:=y; sd.cz:=z; end
          else Level.Things[Cur_TH].Vals[Cur_FR].GetFrame(sd.cx,sd.cy,sd.cz,a,a,a);
          ScaleFrames(level,frsel,sd);
          frsel.DeleteN(i);
         end;

 end;
Invalidate;
end;

Procedure TJedMain.TranslateObject(dx,dy,dz:double);
var sec:TJKSector;
    surf:TJKSurface;
    i:integer;
begin
case Map_Mode of
 MM_SC: begin
         StartUndorec('Translate sectors');
         sec:=Level.Sectors[Cur_SC];
         TranslateSectors(level,scsel,Cur_SC,dx,dy,dz);
        end;
 MM_SF: begin
         StartUndorec('Translate surfaces');
         surf:=Level.Sectors[Cur_SC].Surfaces[Cur_SF];
         TranslateSurfaces(Level,sfsel,Cur_SC,Cur_SF,dx,dy,dz);
        end;
end;
Invalidate;
end;

Procedure TJedMain.StartStitch;
begin
if map_mode<>MM_SF then exit;
 stitch_Sec:=Level.Sectors[Cur_SC];
 stitch_Surf:=Level.Sectors[Cur_SC].Surfaces[Cur_SF];
end;

Procedure TJedMain.DoStitch;
var ssurf,esurf:TJKSurface;
begin
if map_mode<>MM_SF then exit;
if stitch_sec=nil then
begin
 PanMessage(mt_error,'No start surface was set for stitching');
 exit;
end;

if (Level.Sectors.IndexOf(stitch_sec)<0) or
 (stitch_sec.Surfaces.IndexOf(stitch_surf)<0) then
 begin
  PanMessage(mt_error,'The stitching start surface was deleted');
  exit;
 end;

 ssurf:=stitch_surf; esurf:=Level.Sectors[Cur_SC].Surfaces[Cur_SF];


 StartUndoRec('Stitch surfaces');
 StitchSurfaces(ssurf,esurf);
 if ssurf.Material<>'' then esurf.Material:=ssurf.Material;



 SectorChanged(esurf.sector);
 StartStitch;
end;

Procedure TJedMain.StraightenTexture(zero,rot90:boolean);
var u,v:tvector;
    surf:TJKSurface;


Procedure StrEd(surf:TJKSurface;ed:integer);
begin
 SaveSecUndo(surf.sector,ch_changed,sc_geo);

  if rot90 then
  begin
   CalcDefaultUVNormals(surf,ED,v,u);
   v.dx:=-v.dx;
   v.dy:=-v.dy;
   v.dz:=-v.dz;
   UpdateSurfUVData(surf,u,v);
  end else CalcDefaultUVNormals(surf,ED,u,v);

  if zero then with surf.TXvertices[ED] do
  begin
   u:=0;
   v:=0;
  end;
  ArrangeTexture(surf,ED,u,v);
  SectorChanged(surf.sector);
end;

Procedure StrSurf(surf:TJKSurface);
begin
 SaveSecUndo(surf.sector,ch_changed,sc_geo);

 if rot90 then
 begin
  CalcUVNormals(surf,v,u);
  v.dx:=-v.dx;
  v.dy:=-v.dy;
  v.dz:=-v.dz;
  UpdateSurfUVData(surf,u,v);
 end else CalcUVNormals(surf,u,v);
 if zero then
 With surf.txvertices[0] do begin u:=0; v:=0; end;
 ArrangeTexture(surf,0,u,v);
 SectorChanged(surf.sector);
end;

Var
    i,sc,sf,ed,n:integer;

begin
 case map_mode of
  MM_SF: begin
          StartUndoRec('Straighten surfaces');
          n:=sfsel.AddSF(Cur_SC,Cur_SF);
          for i:=0 to sfsel.count-1 do
          begin
           sfsel.GetSCSF(i,sc,sf);
           strSurf(Level.Sectors[sc].Surfaces[SF]);
          end;
          sfsel.DeleteN(n);
         end;
  MM_ED: begin
          StartUndoRec('Straighten surfaces');
          n:=edsel.AddED(Cur_SC,Cur_SF,Cur_ED);
          for i:=0 to edsel.count-1 do
          begin
           edsel.GetSCSFED(i,sc,sf,ed);
           strEd(Level.Sectors[sc].Surfaces[SF],ed);
          end;
          edsel.DeleteN(n);
        end;
  end;
end;

Procedure TJedMain.GetCam(var X,Y,Z,PCH,YAW,ROL:Double);
begin
 x:=-Renderer.CamX;
 y:=-Renderer.Camy;
 z:=-Renderer.CamZ;
 if maprot<>MR_OLD then GETPYR;
 pch:=rPCH;
 yaw:=rYAW;
 rol:=rROL;
end;

Procedure TJedMain.SetCam(X,Y,Z,PCH,YAW,ROL:Double);
var zv,xv:TVector;
begin
 Renderer.CamX:=-x;
 Renderer.CamY:=-Y;
 Renderer.CamZ:=-Z;

{ SetVec(xv,1,0,0);
 SetVec(zv,0,0,1);
 RotateVector(xv,pch,yaw,rol);
 RotateVector(zv,pch,yaw,rol);

 Renderer.SetZ(zv.dx,zv.dy,zv.dz);
 Renderer.SetX(xv.dx,xv.dy,xv.dz);}

 rPch:=PCH;
 rYaw:=Yaw;
 rRol:=Rol;
 SetRendFromPYR;

 Invalidate;
end;

Procedure TJedMain.RaiseObject(how:integer);
var dx,dy,dz:double;
    th:TJKThing;
    vl:TTPLValue;
begin

 With Renderer do
 if how=ro_up then begin dx:=gnormal.dx*perpStep; dy:=gnormal.dy*perpStep; dz:=gnormal.dz*perpStep; end
 else begin dx:=-gnormal.dx*perpStep; dy:=-gnormal.dy*perpStep; dz:=-gnormal.dz*perpStep; end;

 case Map_mode of
  MM_SC: begin
          StartUndoRec('Translate Sector(s)');
          TranslateSectors(level,scsel,Cur_SC,dx,dy,dz);
         end;
  MM_SF: begin
          StartUndoRec('Translate surface(s)');
          TranslateSurfaces(Level,sfsel,Cur_SC,Cur_SF,dx,dy,dz);
         end;
    {TranslateSurface(Level.Sectors[Cur_SC].Surfaces[Cur_SF],
           dx,dy,dz);}
  MM_VX: begin
          StartUndoRec('Translate vertices');
          TranslateVertices(level,vxsel,cur_SC,Cur_VX,dx,dy,dz);
         end;

  MM_ED: begin
          StartUndoRec('Translate Edge(s)');
          TranslateEdges(level,edsel,Cur_SC,Cur_SF,Cur_ED,dx,dy,dz);
         end;
  MM_TH: begin
          StartUndoRec('Translate thing(s)');
          {SaveSelThingsUndo('Translate thing(s)',ch_changed);}
          TranslateThings(level,thsel,Cur_TH,dx,dy,dz,MoveFrames);
         end;
  MM_FR: begin
          StartUndoRec('Translate frame(s)');
          {SaveSelFramesUndo('Translate frame(s)',ch_changed);}
          if Cur_FR=-1 then TranslateThingKeepFrame(Level,Cur_TH,dx,dy,dz)
          else TranslateFrames(level,frsel,Cur_TH,Cur_FR,dy,dy,dz);
         end;
  MM_LT: begin
          {SaveSelLightsUndo('Translate thing(s)',ch_changed);}
          StartUndoRec('Translate light(s)');
          TranslateLights(level,ltsel,Cur_LT,dx,dy,dz);
         end;
  else exit;
 end;
 Invalidate;
end;

Function IsSCValid(sc:integer):boolean;
begin
 result:=(SC>=0) and (SC<Level.Sectors.count);
end;

Function IsSFValid(sc,sf:integer):boolean;
begin
 result:=false;
 if (SC<0) or (SC>=Level.Sectors.count) then exit;
 Result:=(SF>=0) and (SF<Level.Sectors[SC].Surfaces.count);
end;

Function IsEDValid(sc,sf,ed:integer):boolean;
begin
 result:=false;
 if (SC<0) or (SC>=Level.Sectors.count) then exit;
 if (SF<0) or (SF>=Level.Sectors[SC].Surfaces.count) then exit;
 Result:=(ED>=0) and (ED<Level.Sectors[SC].Surfaces[SF].Vertices.count);
end;

Function IsVXValid(sc,vx:integer):boolean;
begin
 result:=false;
 if (SC<0) or (SC>=Level.Sectors.count) then exit;
 Result:=(VX>=0) and (VX<Level.Sectors[SC].Vertices.count);
end;

Function IsTHValid(th:integer):boolean;
begin
 result:=(TH>=0) and (TH<Level.Things.count);
end;

Function IsLTValid(lt:integer):boolean;
begin
 result:=(LT>=0) and (LT<Level.Lights.count);
end;

Function IsFRValid(th,fr:integer):boolean;
begin
 result:=false;
 if (TH<0) or (TH>=Level.Things.count) then exit;
 Result:=(FR>=-1) and (FR<Level.Things[TH].Vals.count);
end;

Procedure TJedMain.VerifyMultiSelection;
var i,sc,sf,ed,vx,fr,n:integer;
begin
 case map_mode of
  MM_SC: For i:=scsel.count-1 downto 0 do
         if not IsSCValid(scsel.getSC(i)) then scsel.DeleteN(i);
  MM_SF: For i:=sfsel.count-1 downto 0 do
         begin
          sfsel.GetSCSF(i,sc,sf);
          if not IsSFValid(sc,sf) then sfsel.DeleteN(i);
         end;
  MM_ED: For i:=edsel.count-1 downto 0 do
         begin
          edsel.GetSCSFED(i,sc,sf,ed);
          if not IsEDValid(sc,sf,ed) then edsel.DeleteN(i);
         end;
  MM_VX: For i:=vxsel.count-1 downto 0 do
         begin
          vxsel.GetSCVX(i,sc,vx);
          if not IsVXValid(sc,vx) then vxsel.DeleteN(i);
         end;
  MM_TH: For i:=thsel.count-1 downto 0 do
         if not IsTHValid(thsel.getTH(i)) then thsel.DeleteN(i);
  MM_FR: For i:=frsel.count-1 downto 0 do
         begin
          frsel.GetTHFR(i,n,fr);
          if not IsFRValid(n,fr) then frsel.DeleteN(i);
         end;
  MM_LT: For i:=ltsel.count-1 downto 0 do
         if not IsLTValid(ltsel.getLT(i)) then ltsel.DeleteN(i);
 end;
end;

Procedure TJedMain.VerifySelection;
begin
 if not IsSelValid then SetMapMode(map_mode);
end;

Function TJedMain.IsSelValid:boolean;
begin
 case map_mode of
  MM_SC: result:=(Cur_SC>=0) and (Cur_SC<Level.Sectors.count);
  MM_SF: begin
          result:=false;
          if (Cur_SC<0) or (Cur_SC>=Level.Sectors.count) then exit;
          Result:=(Cur_SF>=0) and (Cur_SF<Level.Sectors[Cur_SC].Surfaces.count);
         end;
  MM_ED: begin
          result:=false;
          if (Cur_SC<0) or (Cur_SC>=Level.Sectors.count) then exit;
          if (Cur_SF<0) or (Cur_SF>=Level.Sectors[Cur_SC].Surfaces.count) then exit;
          Result:=(Cur_ED>=0) and (Cur_ED<Level.Sectors[Cur_SC].Surfaces[Cur_SF].Vertices.count);
         end;
  MM_VX: begin
          result:=false;
          if (Cur_SC<0) or (Cur_SC>=Level.Sectors.count) then exit;
          Result:=(Cur_VX>=0) and (Cur_VX<Level.Sectors[Cur_SC].Vertices.count);
         end;
  MM_TH: result:=(Cur_TH>=0) and (Cur_TH<Level.Things.count);
  MM_FR: begin
          result:=false;
          if (Cur_TH<0) or (Cur_TH>=Level.Things.count) then exit;
          Result:=(Cur_FR>=-1) and (Cur_FR<Level.Things[Cur_TH].Vals.count);
         end;
  MM_LT: result:=(Cur_LT>=0) and (Cur_LT<Level.Lights.count);
  MM_Extra: result:=(Cur_EX>=0) and (Cur_EX<ExtraObjs.count);
 else result:=false;
 end;
end;

procedure TJedMain.RecentClick(Sender: TObject);
begin
 if Not AskSave then exit;
 OpenProject((Sender as TMenuItem).hint,op_open);
end;

Procedure TJedMain.SyncRecents;
var im:integer;

Procedure AddItem(const s:string);
var mi:TMenuItem;
    dn,fn:string;
begin
 if s='' then exit;
 mi:=TMenuItem.Create(FileMenu);
 if length(s)>32 then
 begin
  dn:=ExtractPath(s);
  fn:=ExtractName(s);
  dn:=copy(dn,1,28-length(fn));
  mi.caption:=dn+'...'+'\'+fn;
 end
 else mi.Caption:=s;
 mi.hint:=s;
 mi.OnClick:=RecentClick;
 FileMenu.Add(mi);
end;

begin
 im:=RecentBar.MenuIndex;
 While FileMenu.Count>im+1 do FileMenu.Delete(im+1);
 AddItem(Recent1);
 AddItem(Recent2);
 AddItem(Recent3);
 AddItem(Recent4);
end;

Procedure TJedMain.AddRecent(const s:string);
var sl:TStringList;
    i:integer;
begin
 sl:=TStringList.Create;
 sl.Add(Recent1);
 sl.Add(Recent2);
 sl.Add(Recent3);
 sl.Add(Recent4);
 i:=sl.IndexOf(s);

 if (i<>-1) then sl.Delete(i);
 sl.Insert(0,s);

 Recent1:=sl[0];
 Recent2:=sl[1];
 Recent3:=sl[2];
 Recent4:=sl[3];

 sl.Free;

 SyncRecents;
end;

procedure TJedMain.SaveJKLGob1Click(Sender: TObject);
begin
 if DoSaveJKL then GobProject1.Click;
end;

procedure TJedMain.SaveJKLGOBandTest1Click(Sender: TObject);
var gobname,batname,ext:string;
    t:TextFile;
    pdir:array[0..255] of char;
begin
 If MsgBox('You''re about to test your level. Proceed?','Warning',mb_YesNo)=idNo then exit;

 if DoSaveJKL then
 begin
   batname:=ProjectDir+'Test_'+ChangeFileExt(ExtractFileName(LevelFile),'.bat');
   if not FileExists(batname) then
   begin
    if IsMots then ext:='jkm.exe' else ext:='jk.exe';

    if Pos(' ',ProjectDir)=0 then StrLCopy(pdir,Pchar(ProjectDir),sizeof(pdir))
    else GetShortPathName(pchar(ProjectDir),pdir,sizeof(pdir));

    AssignFile(t,batname); Rewrite(t);
    Writeln(t,ExtractFileDrive(GameDir));
    Writeln(t,'cd "',GameDir,'"');
    Writeln(t,ext,' -devmode -dispstats -debug log -displayconfig -path '+Pdir);
    CloseFile(t);
   end;

  if ShellExecute(Handle,nil,Pchar(batname),nil,Pchar(GameDir),SW_SHOW)<32 then
   PanMessage(mt_error,'Couldn''t start JK/MOTS');

 end;
end;



procedure TJedMain.Find1Click(Sender: TObject);
begin
 case map_mode of
  MM_SC: if FindSectors.Find then invalidate;
  MM_SF: if FindSurfs.Find then Invalidate;
  MM_TH: if FindThings.Find then Invalidate;
 end;
end;

procedure TJedMain.FindNext1Click(Sender: TObject);
begin
 case map_mode of
  MM_SC: FindSectors.FindNext(Cur_SC);
  MM_SF: FindSurfs.FindNext(Cur_SC,Cur_SF);
  MM_TH: FindThings.FindNext(cur_TH);
 end;
end;

Procedure TJedMain.UpdateItemEditor;
begin
 updatestuff:=true;
end;

Procedure TJedMain.LevelChanged;
begin
 changed:=true;
 Invalidate;
end;

Function TJedMain.AskSave:boolean;
begin
 Result:=true;
 if not changed then exit;
 Case MsgBox('The project was changed. Save?','JED',MB_YESNOCANCEL) of
  ID_YES: Result:=SaveProject;
  ID_NO:;
  ID_CANCEL: result:=false;
 end;
end;

procedure TJedMain.FormCloseQuery(Sender: TObject; var CanClose: Boolean);
begin
 CanClose:=AskSave;
end;

procedure TJedMain.Levelheadereditor1Click(Sender: TObject);
begin
 if not LHEdit.EditHeader then exit;
 Preview3D.ReloadLevel;
 LevelChanged;
end;

Procedure TJedMain.SetThingView(how:integer);
var i:integer;
begin
 thing_view:=how;
 case how of
  cv_Dots: Dots.Checked:=true;
  cv_Boxes: Boxes.Checked:=true;
  cv_WireFrames:
   begin
    WireFrames.Checked:=true;
    Progress.Reset(Level.Things.Count);
    Progress.Msg:='Loading 3DOs...';
    for i:=0 to Level.Things.Count-1 do
    begin
     UpdateThingData(Level.Things[i]);
     Progress.Step;
    end;
    Progress.Hide;
   end;
 end;
 Invalidate;
end;

Procedure LoadThing3DO(th:TJKThing;force:boolean);
var
    tval:TTPLValue;
    itpl:integer;
    tpl:TTemplate;
    a3DO:T3DO;
begin
 if not force and (th.a3Do<>nil) then exit;
 itpl:=Templates.IndexOfName(th.name);
 if itpl=-1 then
 begin
  Free3DO(th.a3do);
  exit;
 end;
 tval:=Templates.GetTPLField(th.name,'model3d');
 if tval=nil then a3Do:=nil else a3Do:=Load3DO(tval.AsString);
 Free3DO(th.a3DO);
 th.a3DO:=a3DO;
 if a3DO<>nil then a3DO.GetBBOX(th.bbox);
end;

Procedure TJedMain.UpdateThingData(th:TJKThing);
var
    itpl:integer;
    tpl:TTemplate;
begin
 case thing_view of
 cv_dots,cv_boxes:
  begin
   itpl:=Templates.IndexOfName(th.name);
   if itpl=-1 then FillChar(th.Bbox,sizeof(th.Bbox),0) else
   begin
    {Assign BBOX}
    th.bbox:=Templates[itpl].Bbox;
   end;
   if Preview3D.IsActive and P3DThings then LoadThing3DO(th,true);
  end;
 cv_wireframes: LoadThing3DO(th,true);
 end;
end;

procedure TJedMain.WireframesClick(Sender: TObject);
begin
 if Sender=Dots then SetThingView(cv_Dots)
 else if Sender=Boxes then SetThingView(cv_Boxes)
 else if Sender=Wireframes then SetThingView(cv_Wireframes);
end;

procedure TJedMain.LoadTemplates;
var tplname:string;
begin
 if IsMots then tplName:='mots.tpl' else tplName:='master.tpl';
 if (ProjectDir='') or (not FileExists(ProjectDir+tplname)) then
  templates.LoadFromFile(BaseDir+'jeddata\'+tplname) else
  templates.LoadFromFile(ProjectDir+tplname);
end;

Procedure TJedMain.SetMotsIndicator;
begin
if IsMots then
 begin
  PNProjType.Caption:='MOTS';
  PNProjType.Hint:='Current project is MOTS project (double-click to change)';
 end else
 begin
  PNProjType.Caption:='JK';
  PNProjType.Hint:='Current project is JK project (double-click to change)';
 end;
end;

procedure TJedMain.PNProjTypeDblClick(Sender: TObject);
begin
 SetMots(not isMots);
 SetMotsIndicator;
 LoadTemplates;
end;

procedure TJedMain.EpisodeEditor1Click(Sender: TObject);
begin
 EpisodeEdit.EditEpisode;
end;

Procedure TJedMain.MakeDoor;
var th:TJKThing;
var cg:TCOG;
    cf:TCOGFile;
    v:TCOgValue;
    tv:TTplValue;
    i:integer;
    nz:tvector;
    d:double;

Procedure AddTHValue(const name,vs:string);
begin
 tv:=TTPLValue.Create;
 th.Vals.Add(tv);
 tv.name:=name;
 tv.vtype:=GetTPLVType(tv.name);
 tv.atype:=GetTPLType(tv.name);
 tv.Val(vs);
end;

begin
 if map_mode<>MM_TH then
 begin
  ShowMessage('You must be in Thing mode');
  exit;
 end;
 th:=Level.Things[Cur_TH];

 i:=th.Vals.IndexOfName('numframes');
 if i<>-1 then
 begin
  ShowMessage('The thing appears to be a door already');
  exit;
 end;

 d:=th.bbox.z2-th.bbox.z1;
 if IsClose(d,0) then
 begin
  ShowMessage('The thing doesn''t have correct bounding box');
  exit;
 end;

 StartUndoRec('Make door');
 SaveThingUndo(th,ch_changed);

 for i:=0 to th.vals.count-1 do th.vals[i].free;
 th.Vals.clear;

 AddTHValue('thingflags','0x408');
 AddTHValue('numframes','2');
 AddTHValue('frame',Sprintf('(%.6f/%.6f/%.6f:%.6f/%.6f/%.6f)',
                    [th.x,th.y,th.z,th.pch,th.yaw,th.rol]));
 SetVec(nz,0,0,d);
 RotateVector(nz,th.pch,th.yaw,th.rol);
 AddTHValue('frame',Sprintf('(%.6f/%.6f/%.6f:%.6f/%.6f/%.6f)',
                    [th.x+nz.dx,th.y+nz.dy,th.z+nz.dz,th.pch,th.yaw,th.rol]));


 cf:=TCogFile.Create;
 cg:=TCog.Create;
 cg.Name:='00_door.cog';
 Level.Cogs.Add(cg);
 cf.LoadNoLocals(cg.Name);
 for i:=0 to cf.count-1 do
 begin
  v:=TCOGvalue.Create;
  v.Assign(cf[i]);
  cg.Vals.Add(v);
  if i=0 then v.Val(IntToStr(th.Num)) else
   v.Val(cf[i].AsString);
  v.Resolve;
 end;
 cf.free;
 CogForm.RefreshList;
end;

procedure TJedMain.ExportSectoras3DO1Click(Sender: TObject);
var a3DO:T3DO;
    fname:string;
    mesh:T3DOMesh;
    cx,cy,cz:double;

Function AddMat(const mat:string):integer;
var i:integer;
begin
 i:=a3Do.Mats.IndexOf(mat);
 if i<>-1 then Result:=i
 else Result:=a3Do.Mats.Add(mat);
end;

Procedure AddSecToMesh(mesh:T3DOMesh;sec:TJKSector);
var
    face:T3DOFace;
    n,i,j:integer;
    v:TVertex;
    jv:TJKVertex;
    jtv:TTXVertex;
begin
 {Add Vertices}
 for i:=0 to sec.vertices.count-1 do
 With sec.Vertices[i] do
 begin
{  v:=TVertex.Create;
  v.x:=x-cx;
  v.y:=y-cy;
  v.z:=z-cz;}
  mark:=Mesh.AddVertex(x-cx,y-cy,z-cz);
 end;

 {Add Faces}
 for i:=0 to sec.Surfaces.count-1 do
 With sec.Surfaces[i] do
 begin
  if adjoin<>nil then continue;

  face:=T3DOface.Create;
  face.imat:=AddMat(material);
  face.ftype:=0;
  if (faceFlags and FF_Transluent<>0) then face.ftype:=2;
  face.geo:=geo;
  face.light:=light;
  face.tex:=tex;
  face.extra_l:=extraLight;
  for j:=vertices.count-1 downto 0 do
  begin
   jv:=vertices[j];
   jtv:=TXvertices[j];
   n:=face.AddVertex(mesh.vertices[jv.mark]);
   With face.TXVertices[n] do
   begin
    u:=jtv.u;
    v:=jtv.v;
   end;
  end;
  Mesh.Faces.Add(face);
 end;
end;

var i,j,n:integer;
    csec,sec:TJKSector;
    nsel:TSCMultiSel;
    hnode:THNode;
    ax,ay,az:double;

begin
 if Map_mode<>MM_SC then exit;
 if not Save3DO.Execute then exit;
 fname:=Save3DO.FileName;
 sec:=Level.Sectors[Cur_SC];
 a3DO:=T3Do.CreateNew;

 cx:=0; cy:=0; cz:=0;
 if Level.h3donodes.count=0 then FindCenter(sec,cx,cy,cz);


 nsel:=TSCMultiSel.Create;

 For i:=0 to scsel.count-1 do
  nsel.AddSC(scsel.GetSC(i));

 nsel.AddSC(Cur_SC);

 While nsel.count>0 do
 begin
  sec:=Level.Sectors[nsel.getSC(0)];
  mesh:=a3Do.NewMesh; a3DO.Meshes.Add(mesh);
  mesh.Name:=Level.GetLayerName(sec.layer);

 for i:=nsel.count-1 downto 0 do
 begin
  csec:=Level.Sectors[nsel.getSC(i)];
  if csec.layer<>sec.layer then continue;
  csec.Renumber;
  AddSecToMesh(Mesh,cSec);
  nsel.DeleteN(i);
 end;

end;

// Build Hierarrchy;
if Level.h3donodes.count=0 then // No hierarchy defined. Create default
begin
 hnode:=THNode.Create;
 a3DO.hnodes.Add(hnode);
 if a3DO.meshes.count=1 then begin hnode.nodename:=a3DO.meshes[0].Name; hnode.nmesh:=0; end
 else
 begin
  hnode.nodename:='$$DUMMY';
  for i:=0 to a3DO.meshes.count-1 do
  begin
   hnode:=THNode.Create;
   a3DO.hnodes.Add(hnode);
   hnode.nodename:=a3DO.meshes[i].name;
   hnode.nmesh:=i;
   hnode.parent:=0;
  end;
 end;
end;

if Level.h3donodes.count<>0 then
begin
 for i:=0 to level.h3donodes.count-1 do
 with level.h3donodes[i] do
 begin
  hnode:=THNode.Create;
  hnode.Assign(level.h3donodes[i]);
  a3DO.hnodes.Add(hnode);
 end;
 // set nmesh for all nodes and add nodes for undefined meshes
 for i:=0 to a3DO.meshes.count-1 do
 begin
  n:=0;
  for j:=0 to a3DO.hnodes.count-1 do
  begin
   if CompareText(a3DO.hnodes[j].nodename,a3DO.meshes[i].Name)=0 then
   begin
    inc(n);
    a3DO.hnodes[j].nmesh:=i;
   end;
  end;
  if n=0 then // no nodes for this mesh
  begin
   hnode:=THNode.Create;
   hnode.parent:=0;
   hnode.nmesh:=i;
   hnode.nodename:=a3DO.meshes[i].Name;
   a3Do.hnodes.Add(hnode);
  end;
 end;

 // Check that there are no nodes pointing to non-existing meshes
 for i:=0 to a3DO.hnodes.count-1 do
 begin
  hnode:=a3Do.hnodes[i];
  n:=0;
  for j:=0 to a3DO.meshes.count-1 do
   if CompareText(hnode.nodename,a3DO.meshes[j].Name)=0 then inc(n);
  if n=0 then hnode.nmesh:=-1; 
 end;

end;

a3Do.SaveToFile(fname);
a3Do.Free;
nsel.free;

end;

procedure TJedMain.ReloadTemplates1Click(Sender: TObject);
var x,y,z:tvector;
    p,ya,r:double;
    ap,ay,ar:double;
    nx,ny,nz:Tvector;
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

until p=-1;}
 LoadTemplates;
end;

Function TJedMain.ShortJKLName:string;
begin
 Result:=ChangeFileExt(ExtractFileName(LevelFile),'.jkl');
end;


procedure TJedMain.MakeaBackupCopy1Click(Sender: TObject);
var s:string;
    n:integer;
begin
 if ProjectDir='' then
 begin
  ShowMessage('Save the level first');
  exit;
 end;
 {$i-}
  MkDir(ProjectDir+'backup');
  if ioresult<>0 then;
 {$i+}
 n:=0;

 While n<100 do
 begin
  s:=Format('%s%s%-.2d%s',[ExtractFilePath(LevelFile)+'backup\',
                          ChangeFileExt(ExtractFileName(LevelFile),'')
                          ,n,ExtractFileExt(LevelFile)]);
  if not FileExists(s) then break;
  inc(n);
 end;

 if n>99 then PanMessage(mt_error,'You have 100 backup copies! Remove some')
 else
 begin
  CopyFile(Pchar(LevelFile),Pchar(s),false);
  PanMessage(mt_info,'Made a backup copy '+s);
 end;
end;

Procedure TJedMain.DO_MultiSelect;
begin
 case map_mode of
  MM_SC: DO_SelSC(Cur_SC);
  MM_SF: DO_SelSF(Cur_SC,Cur_SF);
  MM_ED: DO_SelED(Cur_SC,Cur_SF,Cur_ED);
  MM_VX: DO_SelVX(Cur_SC,Cur_VX);
  MM_TH: DO_SelTH(Cur_TH);
  MM_FR: DO_SelFR(Cur_TH,Cur_FR);
  MM_LT: DO_SelLT(Cur_LT);
 else exit;
 end;
 Invalidate;
end;

Procedure TJedMain.ClearMultiSelection;
begin
 case map_mode of
  MM_SC: scsel.clear;
  MM_SF: sfsel.clear;
  MM_ED: edsel.clear;
  MM_VX: vxsel.clear;
  MM_TH: thsel.clear;
  MM_FR: frsel.clear;
  MM_LT: ltsel.clear;
 end;
end;

Procedure TJedMain.CleaveBy(const norm:TVector;x,y,z:double);
var i:integer;
    nsc,nsf:integer;
begin
 case Map_mode of
  MM_SC:  begin
           StartUndoRec('Cleave sector(s)');
           scsel.AddSC(Cur_SC);
           for i:=0 to scsel.count-1 do
            {Cleaved:=}CleaveSector(Level.Sectors[scsel.getSC(i)],norm,x,y,z); {updated in procedure}
           scsel.Clear;
          end;
  MM_SF:  begin
           StartUndoRec('Cleave surface(s)');
           sfsel.AddSF(Cur_SC,Cur_SF);
           for i:=0 to sfsel.count-1 do
           begin
            sfsel.GetSCSF(i,nsc,nsf);
           {Cleaved:=}CleaveSurface(Level.Sectors[nsc].Surfaces[nsf],
                 norm,x,y,z);
           end;
           sfsel.clear;
          end;
  MM_ED: begin
          StartUndorec('Cleave Edge'); 
        {CLeaved:=}CleaveEdge(Level.Sectors[Cur_SC].Surfaces[Cur_SF],
                   Cur_ED,norm,x,y,z);
         end;
 end;

end;

Procedure TJedMain.AddKBItem(mi:TMenuItem;const name:string;c:char;sc:TShiftState);
var nmi:TMenuItem;
    ext:string;
begin
 nmi:=TMenuItem.Create(mi);
 nmi.OnClick:=KBCommandClick;
 case c of
  #0: ext:='';
  Char(VK_ADD):ext:=#9'+';
  Char(VK_SUBTRACT):ext:=#9'-';
  Char(VK_MULTIPLY):ext:=#9'*';
 else ext:=#9+ShortCutToText(ShortCut(ord(c),sc));
 end;

 nmi.Caption:=name+ext;
 nmi.Tag:=ShortCut(ord(c),sc);
 mi.Add(nmi);
end;

procedure TJedMain.KBCommandClick(Sender: TObject);
var key:word;
    sc:TShiftState;
begin
 with (sender as TMenuItem) do
 begin
  ShortCutToKey(Tag,key,sc);
  FormKeyDown(Self, Key, sc);
 end;
end;

Procedure TJedMain.CreateRenderer;
var x,z,gx,gz:TVector;
    scale,cmx,cmy,cmz,grx,gry,grz:double;
    grdot,grline,grsize,grstep:double;
begin
 if Renderer=nil then
 begin
  SetVec(x,1,0,0);
  SetVec(z,0,0,1);
  SetVec(gx,1,0,0);
  SetVec(gz,0,0,1);
  scale:=1;
  cmx:=0;
  cmy:=0;
  cmz:=0;
  grx:=0;gry:=0;grz:=0;
end else
begin
  x:=Renderer.xv;
  z:=Renderer.zv;
  gx:=Renderer.gxnormal;
  gz:=Renderer.gnormal;
  scale:=Renderer.Scale;
  cmx:=Renderer.CamX;
  cmy:=Renderer.CamY;
  cmz:=Renderer.CamZ;
  grx:=Renderer.GridX;
  gry:=Renderer.GridY;
  grz:=Renderer.GridZ;
  grdot:=Renderer.GridDot;
  grline:=Renderer.GridLine;
  grstep:=Renderer.GridStep;

  Renderer.free;
end;
try
 case WireFrameAPI of
  WF_Software: Renderer:=TSFTRenderer.Create(self.handle);
  WF_OpenGL: Renderer:=TOGLRenderer.Create(self.handle);
 else Renderer:=TSFTRenderer.Create(self.handle);
 end;
except
 On Exception do
 begin
  PanMessage(mt_warning,'Couldn''t create a requested wireframe renderer. Reverted to software');
  Renderer:=TSFTRenderer.Create(self.handle);
 end;
end;

Renderer.SetZ(z.dx,z.dy,z.dz);
Renderer.SetX(x.dx,x.dy,x.dz);
Renderer.SetGridNormal(gz.dx,gz.dy,gz.dz);
Renderer.SetGridXNormal(gx.dx,gx.dy,gx.dz);
Renderer.Scale:=scale;
With Renderer do
begin
 CamX:=cmx;
 CamY:=cmy;
 CamZ:=cmz;
 GridX:=grx;
 GridY:=gry;
 GridZ:=grz;
 GridDot:=grdot;
 GridLine:=grline;
 GridStep:=grstep;
end;
end;

Procedure LoadDLLPlugin(const dll:string);
var hlib:Integer;

Procedure CallLoad(hmod:Integer);
var
    lf:TJEDPluginLoad;
    lfstd:TJEDPluginLoadStdCall;
begin
try
 lfstd:=GetProcAddress(hlib,'JEDPluginLoadStdCall');
{ lfstd:=GetProcAddress(hlib,'@JEDPluginLoadStdcall$qqsp4IJED');}


  if Assigned(lfstd) then
  begin
   if not lfstd(getJEDCOM) then PanMessage(mt_warning,'Plug-in returned a error in initialization '+dll);
   exit;
  end;

 lf:=GetProcAddress(hlib,'JEDPluginLoad');
  if not Assigned(lf) then
  begin
   PanMessage(mt_warning,'The DLL '+dll+' is not a JED plug-in: no JEDPluginLoadStdCall or JEDPluginLoad function');
   exit;
  end;
  if not lf(getJEDCOM) then PanMessage(mt_warning,'Plug-in returned a error in initialization '+dll);
 Except
  On e:Exception do PanMessage(mt_warning,'Exception raised by '+dll+' '+e.message);
 end;
 end;

begin
 hlib:=GetModuleHandle(pchar(dll));
 if hlib<>0 then begin CallLoad(hlib); exit; end;

 hlib:=LoadLibrary(pchar(dll));
 if hlib=0 then begin PanMessage(mt_error,'Couldn''t load plugin '+dll); exit; end;

 CallLoad(hlib);
end;

procedure TJedMain.PluginClick(Sender: TObject);
var fn:string;
begin
 fn:=(Sender as TMenuItem).hint;
 if CompareText(ExtractFileExt(fn),'.dll')=0 then LoadDLLPlugin(fn) else
 begin
  if ShellExecute(Handle,nil,Pchar(fn),'/JED',nil,SW_SHOW)<32 then
   PanMessage(mt_error,'Couldn''t start plugin: '+SysErrorMessage(GetLastError));
 end;
end;

procedure TJedMain.LoadPlugins;
var    i:integer;

Procedure LoadDetails(mi:TmenuItem;const fname:string);
var t:textfile;
    s,v:string;
    pe:integer;
begin
 if Not FileExists(fname) then exit;
 try
  AssignFile(t,fname);
  Reset(t);
 except
  on Exception do exit;
 end;

 try
  try
   While Not eof(t) do
   begin
    Readln(t,s);
    pe:=Pos('=',s);
    if pe=0 then continue;
    v:=LowerCase(Copy(s,1,pe-1));
    s:=Copy(s,pe+1,length(s));
    if v='name' then mi.Caption:=s
    else if v='key' then mi.ShortCut:=TextToShortCut(s);
   end;

  finally
   closeFile(t);
  end;
 except
  on Exception do;
 end;
end;

Procedure LoadDir(const dir:string;mi:TMenuItem);
var sr:TSearchRec;
    res:integer;
    ni:TMenuItem;
    s:string;
begin
 res:=FindFirst(dir+'*.*',faAnyFile,sr);
 while res=0 do
 begin
  if (sr.attr and faDirectory<>0) and (sr.name<>'.') and (sr.name<>'..') then
  begin
   ni:=TmenuItem.Create(mi);
   mi.Add(ni);
   ni.Caption:=sr.name;
   LoadDir(dir+sr.name+'\',ni);
  end;

  s:=LowerCase(ExtractFileExt(sr.name));
  if (sr.attr and (faDirectory or faVolumeID)=0) and
     ((s='.exe') or (s='.js') or (s='.vbs') or (s='.lnk') or (s='.bat') or (s='.dll')) then
  begin

   ni:=TmenuItem.Create(mi);

   s:=sr.name;
   if CompareText(ExtractFileExt(s),'.lnk')=0 then s:=ChangeFileExt(s,'');

  ni.Caption:=s;
  ni.Hint:=dir+sr.name;
  ni.OnClick:=PluginClick;

  s:=sr.name;
  if CompareText(ExtractFileExt(s),'.lnk')=0 then s:=ChangeFileExt(s,'');
  LoadDetails(ni,dir+s+'.dsc');

  mi.Add(ni);
  
  end;
  res:=FindNext(sr);
 end;
 FindClose(sr);
end;

begin
 for i:=Plugins.Count-1 downto 0 do Plugins.Delete(i);

 LoadDir(BaseDir+'plugins\',Plugins);

end;

procedure TJedMain.DeleteFrame(Cur_TH,Cur_FR:integer);
var th:TJKThing;
    av,vl:TTPLValue;
    nv,cv,i:integer;
begin
 if Cur_FR=-1 then exit;
 th:=Level.Things[Cur_TH];
 if Cur_FR>=th.Vals.count then exit;
 vl:=th.vals[Cur_FR];
 if CompareText(vl.Name,'frame')<>0 then exit;

 SaveThingUndo(th,ch_changed);

 vl.free;
 th.vals.Delete(Cur_FR);

 if Cur_FR>=th.Vals.count then av:=th.vals[th.Vals.count-1]
 else av:=th.vals[Cur_FR];

 nv:=th.NFrames;
 i:=th.vals.IndexOfName('numframes');

 if nv=0 then
 begin
  if i<>-1 then
  begin
   th.Vals[i].Free;
   th.Vals.Delete(i);
  end;
  exit;
 end;

 if i=-1 then th.InsertValue(0,'numframes',IntToStr(nv)) else
 th.Vals[i].Val(IntToStr(nv));

 nv:=th.Vals.IndexOf(av);

 Cur_FR:=-1;

 i:=th.NextFrame(-1);

 While i<>-1 do
 begin
  Cur_FR:=i+1;
  i:=th.NextFrame(i);
 end;

 SetCurFR(Cur_TH,Cur_FR);

end;

Procedure TJedMain.AddFramesAt(X,Y:integer);
var fx,fy,fz:double;
    nv,i,fr:integer;
    th:TJKThing;
    vl:TTplValue;
begin
 If not GetXYZAt(X,Y,fx,fy,fz) then exit;

 th:=Level.Things[Cur_TH];

 StartUndoRec('Add frame');
 SaveThingUndo(th,ch_changed);

 if Cur_FR=-1 then
  vl:=th.AddValue('frame',Sprintf('(%.6f/%.6f/%.6f:%.6f/%.6f/%.6f)',
                    [fx,fy,fz,th.pch,th.yaw,th.rol]))
 else vl:=th.InsertValue(Cur_FR+1,'frame',Sprintf('(%.6f/%.6f/%.6f:%.6f/%.6f/%.6f)',
                    [fx,fy,fz,th.pch,th.yaw,th.rol]));

 nv:=th.NFrames;
 i:=th.vals.IndexOfName('numframes');
 if i=-1 then th.InsertValue(0,'numframes',IntToStr(nv)) else
 th.Vals[i].Val(IntToStr(nv));

 Cur_FR:=th.Vals.IndexOf(vl);

 SetCurFR(Cur_TH,Cur_FR);

end;

procedure TJedMain.Edit1Click(Sender: TObject);
var s:string;
begin
 s:=GetUndoRecName;
 miUndo.Caption:='&Undo '+s;
 miUndo.Enabled:=s<>'';

 case map_mode of
  MM_SC: begin
          miPaste.Enabled:=CanPasteSectors;
          miCopy.Enabled:=true;
         end;
  MM_TH: begin
          miPaste.Enabled:=CanPasteThings;
          miCopy.Enabled:=true;
         end;
  MM_LT: begin
          miPaste.Enabled:=CanPasteLights;
          miCopy.Enabled:=true;
         end;
  else
   begin
    miPaste.Enabled:=false;
    miCopy.Enabled:=false;
   end;
 end;
end;

procedure TJedMain.miCopyClick(Sender: TObject);
begin
 case Map_mode of
  MM_SC: CopySectors(Level,scsel,Cur_SC);
  MM_LT: CopyLights(Level,ltsel,Cur_LT);
  MM_TH: CopyThings(Level,thsel,Cur_TH);
 end;
end;

procedure TJedMain.miPasteClick(Sender: TObject);
var i,j,n:integer;
    rx,ry,rz:double;
    x,y:integer;
    layer:integer;
    sec,nsc:TJKSector;
    surf:TJKSurface;
begin
 if Sender<>nil then
 begin
  rx:=Renderer.CamX;
  ry:=Renderer.CamY;
  rz:=Renderer.CamZ;
 end else
 begin
  if not GetMousePos(X,Y) then exit;
  if not GetXYZAt(X,Y,rX,rY,rZ) then exit;
 end;

 case Map_mode of
  MM_SC: begin
          sec:=Level.Sectors[Cur_SC];
          n:=PasteSectors(Level,rx,ry,rz);
          if n=-1 then exit;

          StartUndoRec('Paste sector(s)');

          SetCurSC(n);
          scsel.clear;
          for i:=n to Level.sectors.count-1 do
          begin
           nsc:=Level.Sectors[i];
           nsc.Layer:=Sec.Layer;
           nsc.ColorMap:=Sec.ColorMap;

           SaveSecUndo(nsc,ch_added,sc_both);

           for j:=0 to nsc.surfaces.count-1 do
           begin
            surf:=nsc.surfaces[j];
            if High(surf.mark)<>$FFFF then MakeAdjoinSCUP(surf,n);
           end;

           scsel.AddSC(i);
           SectorAdded(Level.Sectors[i]);
          end;
          Invalidate;

         end;
  MM_LT: begin
          layer:=0;
          if level.lights.count>0 then layer:=level.lights[Cur_LT].Layer;

          StartUndoRec('Paste light(s)');

          n:=PasteLights(Level,rx,ry,rz);
          if n=-1 then exit;
          SetCurLT(n);
          ltsel.clear;
          for i:=n to Level.Lights.count-1 do
          begin
           saveLightUndo(Level.Lights[i],ch_added);
           Level.Lights[i].Layer:=Layer;
           ltsel.AddLT(i);
          end;
          {SaveSelLightsUndo('Paste light(s)',ch_added);}
          LevelChanged;
          Invalidate;
         end;
  MM_TH: begin
          layer:=0;
          if level.things.count>0 then layer:=level.Things[Cur_TH].Layer;

          StartUndoRec('Paste things');

          n:=PasteThings(Level,rx,ry,rz);
          if n=-1 then exit;
          SetCurTH(n);
          thsel.clear;
          for i:=n to Level.Things.count-1 do
          begin
           Level.Things[i].Layer:=Layer;
           SaveThingUndo(Level.Things[i],ch_added);
           LayerThing(i);
           ThingAdded(Level.Things[i]);
           thsel.AddTH(i);
          end;
          {SaveSelThingsUndo('paste Thing(s)',ch_added);}
          Invalidate;
         end;
 end;
end;

procedure TJedMain.N3DPreviewtoItem1Click(Sender: TObject);
var cx,cy,cz,cpch,cyaw,a:double;
begin
 cpch:=0; cyaw:=0;
 case Map_mode of
  MM_SC: FindCenter(Level.Sectors[Cur_SC],cx,cy,cz);
  {MM_VX: with Level.Sectors[Cur_SC].Vertices[Cur_VX] do begin cx:=x; cy:=y; cz:=z; end;}
  MM_TH: with Level.Things[Cur_TH] do begin cx:=x; cy:=y; cz:=z; cpch:=pch; cyaw:=yaw; end;
  MM_LT: with Level.Lights[Cur_LT] do begin cx:=x; cy:=y; cz:=z; end;
  MM_FR: if Cur_FR=-1 then with Level.Lights[Cur_LT] do begin cx:=x; cy:=y; cz:=z; end
         else Level.Things[Cur_TH].Vals[Cur_FR].GetFrame(cx,cy,cz,cpch,cyaw,a);
 end;
 Preview3D.SetCam(cX,cY,cZ,cpch,cyaw);
end;

Procedure TJedMain.SetMSelMode(mode:integer);
var mname:string;
begin
 case mode of
  mm_Toggle:
   begin
    mname:='Toggle';
    miToggle.Checked:=true;
    pMsel.Caption:='*';
   end;
  mm_Add:
   begin
    mname:='Add';
    miAdd.Checked:=true;
    pMsel.Caption:='+';
   end;
  mm_Subtract:
   begin
    mname:='Subtract';
    miSubtract.Checked:=true;
    pMsel.Caption:='-';
   end;
  else exit;
 end;
 msel_mode:=mode;
 PMsel.Hint:='Current multiselction mode is '+mname+' Click to change';
end;

procedure TJedMain.PMselClick(Sender: TObject);
begin
 case msel_mode of
  mm_Toggle: SetMselMode(mm_Add);
  mm_Add: SetMselMode(mm_Subtract);
  mm_Subtract: SetMselMode(mm_Toggle);
 else SetMselMode(mm_Toggle);
 end;
end;

procedure TJedMain.miToggleClick(Sender: TObject);
begin
 if Sender=miToggle then SetMselMode(mm_Toggle);
 if Sender=miAdd then SetMselMode(mm_Add);
 if Sender=miSubtract then SetMselMode(mm_Subtract);
end;

Procedure TJedMain.DO_SelSC(sc:integer);
var i:integer;
begin
 case msel_mode of
  mm_toggle: begin i:=scsel.AddSC(sc);  scsel.DeleteN(i xor Nbit); end;
  mm_add: scsel.AddSC(sc);
  mm_subtract: begin i:=scsel.FindSC(sc); if i<>-1 then scsel.DeleteN(i); end;
 end;
end;

Procedure TJedMain.DO_SelSF(sc,sf:integer);
var i:integer;
begin
 case msel_mode of
  mm_toggle: begin i:=sfsel.AddSF(sc,sf);  sfsel.DeleteN(i xor Nbit); end;
  mm_add: sfsel.AddSF(sc,sf);
  mm_subtract: begin i:=sfsel.FindSF(sc,sf); if i<>-1 then sfsel.DeleteN(i); end;
 end;
end;

Procedure TJedMain.DO_SelVX(sc,vx:integer);
var i:integer;
begin
 case msel_mode of
  mm_toggle: begin i:=vxsel.AddVX(sc,vx);  vxsel.DeleteN(i xor Nbit); end;
  mm_add: vxsel.AddVX(sc,vx);
  mm_subtract: begin i:=vxsel.FindVX(sc,vx); if i<>-1 then vxsel.DeleteN(i); end;
 end;
end;

Procedure TJedMain.DO_SelED(sc,sf,ed:integer);
var i:integer;
begin
 case msel_mode of
  mm_toggle: begin i:=edsel.AddED(sc,sf,ed);  edsel.DeleteN(i xor Nbit); end;
  mm_add: edsel.AddED(sc,sf,ed);
  mm_subtract: begin i:=edsel.FindED(sc,sf,ed); if i<>-1 then edsel.DeleteN(i); end;
 end;
end;

Procedure TJedMain.DO_SelTH(th:integer);
var i:integer;
begin
 case msel_mode of
  mm_toggle: begin i:=thsel.AddTH(th);  thsel.DeleteN(i xor Nbit); end;
  mm_add: thsel.AddTH(th);
  mm_subtract: begin i:=thsel.FindTH(th); if i<>-1 then thsel.DeleteN(i); end;
 end;
end;

Procedure TJedMain.DO_SelFR(th,fr:integer);
var i:integer;
begin
 if fr<0 then exit;
 case msel_mode of
  mm_toggle: begin i:=frsel.AddFR(th,fr);  frsel.DeleteN(i xor Nbit); end;
  mm_add: frsel.AddFR(th,fr);
  mm_subtract: begin i:=frsel.FindFR(th,fr); if i<>-1 then frsel.DeleteN(i); end;
 end;
end;

Procedure TJedMain.DO_SelLT(lt:integer);
var i:integer;
begin
 case msel_mode of
  mm_toggle: begin i:=ltsel.AddLT(lt);  ltsel.DeleteN(i xor Nbit); end;
  mm_add: ltsel.AddLT(lt);
  mm_subtract: begin i:=ltsel.FindLT(lt); if i<>-1 then ltsel.DeleteN(i); end;
 end;
end;

procedure TJedMain.ExceptHandler(Sender: TObject; E: Exception);
begin
 ErrForm.ReportError(E,ExceptAddr);
end;


procedure TJedMain.TemplateCreator1Click(Sender: TObject);
begin
 LoadTemplates;
 TPLCreator.CreateTemplate;
end;

Procedure TJedMain.SetRendfromPYR;
var x,y,z:Tvector;
begin
 SetVec(x,1,0,0);
 SetVec(y,0,1,0);
 SetVec(z,0,0,1);
 {PCH,ROL,YAW}
 RotateVector(x,rPCH,0,0); RotateVector(x,0,rROL,0); RotateVector(x,0,0,rYAW);
 RotateVector(y,rPCH,0,0); RotateVector(y,0,rROL,0); RotateVector(y,0,0,rYAW);
 RotateVector(z,rPCH,0,0); RotateVector(z,0,rROL,0); RotateVector(z,0,0,rYAW);
 {So PCH - x , YAW - y, ROL - z}
 Renderer.SetZ(z.dx,z.dy,z.dz);
 Renderer.SetX(x.dx,x.dy,x.dz);
end;

{Old one - for ROL, YAW , PCH order}
(*Procedure GetPY(const x,y,z:TVector;var pch,yaw:double);
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

end;*)

Procedure sysGetPYR(const x,y,z:TVector;var pch,yaw,rol:double);
{Assumes PCH,ROL, YAW
 PCH - x, YAW - y, ROL - z}
var l:double;
    nx,nz:Tvector;
begin

 l:=sqrt(sqr(x.dx)+sqr(x.dz));

 if l=0 then Yaw:=0 else
 begin
  yaw:=-ArcSin(x.dz/l)/pi*180;
 end;

 if x.dx<0 then YAW:=180-YAW;
 if YAW<0 then Yaw:=360+YAW;

 nx:=x;
 RotateVector(nx,0,0,-Yaw);

 Rol:=ArcCos(nx.dx)/pi*180;
 if nx.dy<0 then Rol:=360-rol;

 nz:=z;
 RotateVector(nz,0,0,-Yaw);
 RotateVector(nz,0,-ROL,0);

 PCH:=ArcCos(nz.dz)/pi*180;
 if nz.dy>0 then PCH:=360-PCH;

end;

Procedure TJedMain.GetPYR;
begin
 With Renderer do sysGetPYR(xv,yv,zv,rpch,ryaw,rrol);
end;

procedure TJedMain.SaveTimerTimer(Sender: TObject);
var fname:string;
begin
 if ProjectDir='' then exit;
 If CompareText(ExtractFileExt(LevelFile),'.jed')<>0 then exit;

 ForceDirectories(ProjectDir+'autosave');
 fname:=ProjectDir+'autosave\'+ExtractFileName(LevelFile);
 Level.SaveToJed(fname);
 PanMessage(mt_info,'Project autosaved to '+fname);
end;

Procedure TJedMain.UseInCog;
var ctype:TCOG_Type;
    obj:TObject;
    icog,ival:integer;
begin
 case map_mode of
  MM_SC: begin ctype:=ct_sec; obj:=Level.Sectors[Cur_SC]; end;
  MM_SF: begin ctype:=ct_srf; obj:=Level.Sectors[Cur_SC].Surfaces[CUr_SF]; end;
  MM_TH: begin ctype:=ct_thg; obj:=Level.Things[Cur_TH]; end;
 else exit;
 end;

 icog:=-1;
 ival:=-1;

 if ResPicker.PickCOGVal(icog,ival,ctype) then
 begin
  Level.Cogs[icog].Vals[ival].obj:=obj;
  CogForm.UpdateCOG(icog);
  PanMessageFmt(mt_info,'Item is assigned to value %s in cog %s',
  [Level.Cogs[icog].Vals[ival].name,Level.Cogs[icog].name]);
 end;

end;

Function TJedMain.GetCurObjForCog(ct:TCOG_Type):TObject;
begin
 Result:=nil;
 case map_mode of
  MM_SC: begin
          case ct of
           ct_sec: Result:=Level.Sectors[Cur_SC];
          else ShowMessage('Jed map is not in the appropriate mode');
          end;
         end;
  MM_SF: begin
          case ct of
           ct_srf: Result:=Level.Sectors[Cur_SC].Surfaces[Cur_SF];
           ct_sec: Result:=Level.Sectors[Cur_SC];
          else ShowMessage('Jed map is not in the appropriate mode');
          end;
         end;
  MM_TH: begin
          case ct of
           ct_thg: Result:=Level.Things[Cur_TH];
          else ShowMessage('Jed map is not in the appropriate mode');
          end;
         end;
  else ShowMessage('Jed map is not in the appropriate mode');
 end;
end;


procedure TJedMain.miUndoClick(Sender: TObject);
begin
 ApplyUndo;
 VerifyMultiselection;
 UpdateItemEditor;
end;

Procedure TJedMain.SaveSelThingsUndo(const rname:string;change:integer);
var i,n:integer;
begin
 n:=thsel.AddTH(Cur_TH);
 StartUndoRec(rname);
 for i:=0 to thsel.count-1 do
 SaveThingUndo(Level.Things[thsel.GetTH(i)],change);
 thsel.DeleteN(n);
end;


Procedure TJedMain.SaveSelLightsUndo(const rname:string;change:integer);
var i,n:integer;
begin
 n:=ltsel.AddLT(Cur_LT);
 StartUndoRec(rname);
 for i:=0 to ltsel.count-1 do
 SaveLightUndo(Level.Lights[ltsel.GetLT(i)],change);
 ltsel.DeleteN(n);
end;

Procedure TJedMain.SaveSelFramesUndo(const rname:string;change:integer);
var i,n:integer;
    th,fr:integer;
begin
 n:=frsel.AddFR(Cur_TH,Cur_FR);
 StartUndoRec(rname);
for i:=0 to frsel.count-1 do
 begin
  frsel.GetTHFR(i,th,fr);
  SaveThingUndo(Level.Things[th],change);
 end;
 frsel.DeleteN(n);
end;

Procedure TJedMain.SaveSelSurfUndo(const rname:string;change,how:integer);
var i,n:integer;
    sc,sf:integer;
begin
 n:=sfsel.AddSF(Cur_SC,Cur_SF);
 StartUndoRec(rname);
for i:=0 to sfsel.count-1 do
 begin
  sfsel.GetSCSF(i,sc,sf);
  SaveSecUndo(Level.Sectors[sc],change,how);
 end;
 sfsel.DeleteN(n);
end;

Procedure TJedMain.SaveSelEdgeUndo(const rname:string;change:integer);
var i,n:integer;
    sc,sf:integer;
begin
 n:=edsel.AddED(Cur_SC,Cur_SF,Cur_ED);
 StartUndoRec(rname);
 for i:=0 to edsel.count-1 do
 begin
  edsel.GetSCSFED(i,sc,sf,sf);
  SaveSecUndo(Level.Sectors[sc],change,sc_geo);
 end;
 edsel.DeleteN(n);
end;


Procedure TJedMain.SaveSelSecUndo(const rname:string;change,how:integer);
var i,n:integer;
begin
 n:=scsel.AddSC(Cur_SC);
 StartUndoRec(rname);
for i:=0 to scsel.count-1 do
 begin
  SaveSecUndo(Level.Sectors[scsel.GetSC(i)],change,how);
 end;
 scsel.DeleteN(n);
end;

Procedure TJedMain.SaveSelVertUndo(const rname:string;change:integer);
var i,n:integer;
    sc,vx:integer;
begin
 n:=vxsel.AddVX(Cur_SC,Cur_VX);
 StartUndoRec(rname);
for i:=0 to vxsel.count-1 do
 begin
  vxsel.GetSCVX(i,sc,vx);
  SaveSecUndo(Level.Sectors[sc],change,sc_geo);
 end;
 vxsel.DeleteN(n);
end;


procedure TJedMain.SnapViewToObjectClick(Sender: TObject);
begin
 CenterViewOnObject(true);
end;

Procedure TJedMain.ConnectSFs;
var a:integer;
    sc1,sc2,sf1,sf2:integer;
begin
 if map_mode<>MM_SF then exit;
 a:=sfsel.AddSF(Cur_SC,CUR_SF);
 if sfsel.count=2 then
 begin
  StartUndoRec('Connect surfaces');
  sfsel.GetSCSF(0,sc1,sf1);
  sfsel.GetSCSF(1,sc2,sf2);
  if ConnectSurfaces(Level.Sectors[sc1].Surfaces[sf1],
                     Level.Sectors[sc2].Surfaces[sf2]) then
  PanMessage(mt_info,'The surfaces successfully connected');
  UpdateItemEditor;
 end;
 sfsel.DeleteN(a);
end;

Procedure TJedMain.ConnectSCs;
var a:integer;
begin
 if map_mode<>MM_SC then exit;
 a:=scsel.AddSC(Cur_SC);
 if scsel.count=2 then
 begin
  StartUndoRec('Connect sectors');
  if ConnectSectors(Level.Sectors[scsel.GetSC(0)],
                    Level.Sectors[scsel.GetSC(1)]) then
  PanMessage(mt_info,'The sectors successfully connected');
  UpdateItemEditor;
 end;
 scsel.DeleteN(a);
end;


Procedure TJedMain.BringThingToSurf(th,sc,sf:integer);
var surf:TJKSurface;
    thing:TJKThing;
    x,y,z,l:double;
    xv,yv,zv:Tvector;
    v1,v2:TJKvertex;
begin
 StartUndorec('Bring thing to Surface');
 surf:=Level.Sectors[sc].Surfaces[sf];
 CalcSurfCenter(surf,x,y,z);
 thing:=Level.Things[th];

 SaveThingUndo(thing,ch_Changed);

 l:=-thing.bbox.z1;
 x:=x+surf.normal.dx*l;
 y:=y+surf.normal.dy*l;
 z:=z+surf.normal.dz*l;
 thing.x:=x;
 thing.y:=y;
 thing.z:=z;

 v1:=Surf.Vertices[0];
 v2:=Surf.Vertices[1];
 zv:=surf.normal;
 SetVec(yv,v2.x-v1.x,v2.y-v1.y,v2.z-v1.z);
 normalize(yv);
 Vmult(yv.dx,yv.dy,yv.dz,
       zv.dx,zv.dy,zv.dz,
       xv.dx,xv.dy,xv.dz);

 GetJKPYR(xv,yv,zv,thing.pch,thing.yaw,thing.rol);

 ThingChanged(thing);
end;

Procedure TJedMain.BringLightToSurf(lt,sc,sf:integer);
var surf:TJKSurface;
    light:TJEDLight;
    x,y,z,l:double;
begin
 StartUndorec('Bring light to Surface');
 surf:=Level.Sectors[sc].Surfaces[sf];
 CalcSurfCenter(surf,x,y,z);
 x:=x+surf.normal.dx*0.001;
 y:=y+surf.normal.dy*0.001;
 z:=z+surf.normal.dz*0.001;
 light:=Level.Lights[lt];
 SaveLightUndo(light,ch_Changed);
 light.x:=x;
 light.y:=y;
 light.z:=z;
 LevelChanged;
end;

Procedure TJedMain.AddThingAtSurf;
var th:TJKThing;
    n:integer;
begin
 StartUndoRec('Add thing(s)');

 {Copy data from currently selcted thing}
  th:=Level.NewThing;
  if Cur_TH<Level.Things.count then th.Assign(Level.Things[Cur_TH]);
  n:=Level.Things.Add(th);
  ThingAdded(th);
  Level.RenumThings;

  BringThingToSurf(n,Cur_SC,Cur_SF);

  LayerThing(n);
  SaveThingUndo(th,ch_added);
  SetCurTH(n);

end;

Procedure TJedMain.AddThingAtXYZPYR(x,y,z,pch,yaw,rol:double);
var th:TJKThing;
    n:integer;
begin
 StartUndoRec('Add thing(s)');

 {Copy data from currently selcted thing}
  th:=Level.NewThing;
  if Cur_TH<Level.Things.count then th.Assign(Level.Things[Cur_TH]);
  n:=Level.Things.Add(th);
  ThingAdded(th);
  Level.RenumThings;

  th.x:=x; th.y:=y; th.z:=z;
  th.pch:=pch; th.yaw:=yaw; th.rol:=rol;

  LayerThing(n);
  SaveThingUndo(th,ch_added);
  SetCurTH(n);
end;


Procedure TJedMain.JumpToObject;
var s:string;
    n,sf,ed:Integer;
    surf:TJKSurface;
begin
case map_mode of
 MM_SC: s:=Format('%d',[Cur_SC]);
 MM_SF: s:=Format('%d %d',[Cur_SC,Cur_SF]);
 MM_ED: s:=Format('%d %d %d',[Cur_SC,Cur_SF,Cur_ED]);
 MM_VX: s:=Format('%d %d',[Cur_SC,Cur_VX]);
 MM_TH: s:=Format('%d',[Cur_TH]);
 MM_FR: s:=Format('%d %d',[Cur_TH,Cur_FR]);
 MM_LT: s:=Format('%d',[Cur_LT]);
 else exit;
end;
if not InputQuery('Jump to','Jump to:',s) then exit;
case map_mode of
 MM_SC: begin
         ValInt(s,n);
         SetCurSC(n);
        end;
 MM_SF: begin
         if GetWordN(s,2)='' then
         begin
          ValInt(s,n);
          surf:=Level.GetSurfaceN(n);
          if surf=nil then ShowMessage('No surface with absolute number '+IntToStr(n))
          else SetCurSF(surf.sector.num,surf.num);
         end else
         begin
          SScanf(s,'%d %d',[@n,@sf]);
          SetCurSF(n,sf);
         end;
        end;
 MM_ED: begin
          SScanf(s,'%d %d %d',[@n,@sf,@ed]);
          SetCurED(n,sf,ed);
        end;
 MM_VX: begin
          SScanf(s,'%d %d',[@n,@sf]);
          SetCurVX(n,sf);
        end;
 MM_TH: begin
          ValInt(s,n);
          SetCurTH(n);
        end;
 MM_FR: begin
          SScanf(s,'%d %d',[@n,@sf]);
          SetCurFR(n,sf);
        end;
 MM_LT: begin
          ValInt(s,n);
          SetCurLT(n);
        end;
 else exit;
end;
end;

procedure TJedMain.JumptoObject1Click(Sender: TObject);
begin
JumpToObject;
end;

Constructor TExtraLine.Create;
begin
 v1:=TVertex.Create;
 v2:=TVertex.Create;
end;

Destructor TExtraLine.Destroy;
begin
 v1.free;
 v2.free;
end;

Procedure TJedMain.ClearExtraObjs;
var i,j:integer;
begin
 for i:=0 to ExtraObjs.count-1 do ExtraObjs[i].Free;
 ExtraObjs.Clear;
 Cur_EX:=-1;
 ExtraObjsName:='';
end;

Function TJedMain.AddExtraVertex(x,y,z:double;const name:string):Integer;
var
    v:TExtraVertex;
begin
 v:=TExtraVertex.Create;
 v.x:=x;
 v.y:=y;
 v.z:=z;
 v.name:=name;
 result:=ExtraObjs.Add(v);
end;

Function TJedMain.AddExtraLine(x1,y1,z1,x2,y2,z2:double;const name:string):integer;
var i:integer;
    l:TExtraLine;
begin
 l:=TExtraLine.Create;
 l.v1.x:=x1;
 l.v1.y:=y1;
 l.v1.z:=z1;
 l.v2.x:=x2;
 l.v2.y:=y2;
 l.v2.z:=z2;
 l.name:=name;
 Result:=ExtraObjs.Add(l);
end;

Procedure TJedMain.DeleteExtraObj(n:integer);
begin
 ExtraObjs[n].Free;
 ExtraObjs.Delete(n);
 if Cur_EX>=ExtraObjs.Count then Cur_EX:=ExtraObjs.Count-1;
 ExtraObjsName:='';
end;

Function TJedMain.TranslateExtras(cur_ex:integer;dx,dy,dz:double):integer;
var j:integer;
    ex:TObject;
begin
  ex:=ExtraObjs[cur_ex];
  if ex is TVertex then With TVertex(ex) do begin x:=x+dx;y:=y+dy; z:=z+dz; end;
  if ex is TExtraLine then With TExtraLine(ex) do
  begin
   v1.x:=v1.x+dx;
   v1.y:=v1.y+dy;
   v1.z:=v1.z+dz;
   v2.x:=v2.x+dx;
   v2.y:=v2.y+dy;
   v2.z:=v2.z+dz;
  end;
  if ex is TPolygon then With TPolygon(ex) do
  for j:=0 to Vertices.count-1 do
  with Vertices[j] do begin x:=x+dx;y:=y+dy; z:=z+dz; end;
  if Assigned(OnExtraMove) then OnExtraMove(ex,true);
end;

Procedure TJedMain.SetHideLights(hide:boolean);
begin
 LightsHidden:=hide;
 if LightsHidden then HideLights.Caption:='Show &Lights'
 else HideLights.Caption:='Hide &Lights';
 Invalidate;
end;

Procedure TJedMain.SetHideThings(hide:boolean);
begin
 ThingsHidden:=hide;
 if ThingsHidden then HideThings.Caption:='Show &Things'
 else HideThings.Caption:='Hide &Things';
 Invalidate;
end;


procedure TJedMain.HideThingsClick(Sender: TObject);
begin
 SetHideThings(not ThingsHidden);
end;

procedure TJedMain.HideLightsClick(Sender: TObject);
begin
 SetHideLights(not LightsHidden);
end;

procedure TJedMain.ExportSectorasShape1Click(Sender: TObject);
var s:string;
begin
 s:='';
 if not InputQuery('New Shape Name','Name:',s) then exit;
 s:=trim(s);
 if s='' then exit;
 AddPrefab(s,Level.Sectors[Cur_SC]);
 SavePrefabs(BaseDir+'JedData\shapes.tpl');
 ToolBar.UpdatePrefabs;
end;

procedure TJedMain.TutorialsonMassassiNet1Click(Sender: TObject);
var err:integer;
begin
 err:=ShellExecute(Handle,Nil,'http://www.massassi.net/basics/',nil,nil,SW_SHOWNORMAL);
 if err<=32 then ShowMessage('Couldn''t go to http://www.massassi.net/basics/: '+SysErrorMessage(err));
end;

procedure TJedMain.CutsceneHelper1Click(Sender: TObject);
begin
 KeyForm.ShowKeyDialog;
end;

procedure TJedMain.N3DOHierarchy1Click(Sender: TObject);
begin
 UrqForm.Reload;
 UrqForm.Show;
end;

end.
