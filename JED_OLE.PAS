unit JED_OLE;

interface
uses OleAuto, GlobalVars, J_Level, Classes, misc_utils,
     lev_utils, Values, Windows, u_multisel, U_undo, U_templates,
     Cons_Checker, Geometry, Menus;

type
  TOLELevel=class;

  TOleObjList=class
   idxObjs:TList;
   Objs:TList;
   Constructor Create;
   Destructor Destroy;override;
   Procedure AddObj(idxObj:TObject;oleObj:TAutoObject);
   Function GetObj(idxObj:TObject):TAutoObject;
   Procedure ReleaseObj(idxobj:TObject);
   Procedure DeleteObj(idxObj:TObject);
   Procedure DeleteAll;
  end;


  TOLESector=class(TAutoObject)
   Olelev:TOLELevel;
   sec:TJKSector;
   Constructor Create(olev:TOLELevel;sector:TJKSector);
   Function GetNVertices:Integer;
   Function GetNSurfaces:Integer;

   Function GetFlags:Longint;
   Procedure SetFlags(f:longint);
   Function GetAmbient:double; // deprecated;
   Procedure SetAmbient(amb:double); // deprecated;
   Function GetAmbientBGR:UInt32;      // SED 0.1.0
   Procedure SetAmbientBGR(amb:UInt32); // SED 0.1.0
   Function GetExtra:double; // deprecated;
  Procedure SetExtra(ex:double); // deprecated;
   Function GetExtraLightBGR:UInt32;      // SED 0.1.0
   Procedure SetExtraLightBGR(ex:UInt32); // SED 0.1.0
   Function GetCMP:string;
   Procedure SetCMP(const cmp:string);
   Function GetSound:String;
   Procedure SetSound(const snd:string);
   Function GetVol:double;
   Procedure SetVol(vol:double);
   Function GetLayer:Integer;
   Procedure SetLayer(l:integer);

   // deprecated;
   Function GetTintR:double;
   Function GetTintG:double;
   Function GetTintB:double;

   // SED 0.1.0
   Function GetTintBGR:UInt32;
   Procedure SetTintBGR(ex:UInt32);

  automated
   Function AddSurface:Integer;
   Function InsertSurface(n:integer):integer;
   Procedure DeleteSurface(n:integer);
   Function GetSurface(n:integer):Variant;

   Function AddVertex(x,y,z:double):Integer;
   Function FindVertex(x,y,z:double):Integer;
   Procedure DeleteVertex(n:integer);
   Procedure GetVertex(n:integer;var x,y,z:double);
   Procedure SetVertex(n:integer;x,y,z:double);

   Procedure Update;
   Procedure Release;

   Procedure GetTint(var r,g,b:double);
   Procedure SetTint(r,g,b:double);

   {JED 0.9}
   Function IsConvex:WordBool;

   Property NVertices:Integer read GetNVertices;
   Property NSurfaces:Integer read GetNSurfaces;

   Property Tinx_R:double read GetTintR;
   Property Tinx_G:double read GetTintG;
   Property Tinx_B:double read GetTintB;
   Property Flags:longint read GetFlags write SetFlags;
   Property Ambient:double read GetAmbient write SetAmbient; // deprecated
   Property ExtraLight:double read GetExtra write SetExtra;  // deprecated
   Property ColorMap:String read GetCMP write SetCMP;
   Property Sound:string read GetSound write SetSound;
   Property Volume:double read GetVol write SetVol;
   Property Layer:Integer read GetLayer write SetLayer;

   {SED 0.1.0}
   Property AmbientBGR:UInt32 read GetAmbientBGR write SetAmbientBGR;
   Property ExtraLightBGR:UInt32 read GetExtraLightBGR write SetExtraLightBGR;
  end;

  TOLEThing=class(TAutoObject)
   olelev:TOLELevel;
   thing:TJKThing;
   Constructor Create(olev:TOLELevel;th:TJKThing);
   Function GetNValues:Integer;
   Function GetSec:Integer;
   Procedure SetSec(n:integer);
   Function GetName:string;
   Procedure setName(const name:string);
   Function GetLayer:Integer;
   Procedure SetLayer(n:integer);

   Function GetX:double;
   Function GetY:double;
   Function GetZ:double;
   Function GetPCH:double;
   Function GetYAW:double;
   Function GetROL:double;

  automated
   Procedure Release;
   Procedure Update;

   Procedure GetXYZ(var x,y,z:double);
   Procedure SetXYZ(x,y,z:double);
   Procedure GetOrient(var pch,yaw,rol:double);
   Procedure SetOrient(pch,yaw,rol:double);

   Procedure GetValue(n:Integer;var name,val:string);
   Procedure SetValue(n:Integer;const name,val:string);
   Function GetValueName(n:Integer):String;
   Function GetValueData(n:integer):string;

   Function AddValue(const name,val:string):integer;
   Procedure InsertValue(n:Integer;const name,val:string);
   Procedure DeleteValue(n:integer);

   Property X:double read GetX;
   Property Y:double read GetY;
   Property Z:double read GetZ;
   Property PCH:double read GetPCH;
   Property YAW:double read GetYAW;
   Property ROL:double read GetROL;

   Property NValues:Integer read GetNValues;
   Property Sector:Integer read GetSec write SetSec;
   Property Template:string read GetName write setName;
   Property layer:integer read GetLayer write SetLayer;
  end;

  TOLELight=class(TAutoObject)
   olelev:TOLELevel;
   lt:TSedLight;
   Constructor Create(olev:TOLELevel;alt:TSedLight);
   Function GetRange:double;
   Procedure SetRange(d:double);
   Function GetLayer:integer;
   Procedure SetLayer(n:integer);
   Function GetFlags:Integer;
   Procedure SetFlags(f:integer);
   Function GetR:double;
   Function GetG:double;
   Function GetB:double;
   Function GetX:double;
   Function GetY:double;
   Function GetZ:double;
   Function GetInt:double;
   Function GetRGBInt:double;

  automated
   Procedure Update;
   Procedure Release;

   Procedure GetXYZ(var x,y,z:double);
   Procedure SetXYZ(x,y,z:double);
   Procedure GetRGB(var r,g,b:double);
   Procedure SetRGB(r,g,b:double);
   Procedure GetIntensity(var white,rgb:double);
   Procedure SetIntensity(white,rgb:double);

   Property R:double read GetR;
   Property G:double read GetG;
   Property B:double read GetB;
   Property X:double read GetX;
   Property Y:double read GetY;
   Property Z:double read GetZ;
   Property Intensity:double read GetInt;
   Property RGBIntensity:double read GetRGBInt;

   Property Range:double read GetRange write SetRange;
   Property Layer:integer read GetLayer write SetLayer;
   Property Flags:integer read GetFlags write SetFlags;
  end;

  TOLECOG=class(TAutoObject)
   olelev:TOLELevel;
   cog:TCOG;
   Constructor Create(olev:TOLELevel;cg:TCOG);
   Function GetNValues:Integer;
   Function GetName:string;
   Procedure setName(const name:string);
  automated
   Procedure Release;
   Procedure Update;

   Function GetValue(n:Integer):string;
   Function SetValue(n:Integer;const val:string):WordBool;

   Function GetValueName(n:Integer):String;
   Function GetValueType(n:Integer):String;

   Procedure SetValueType(n:Integer;vtype:string);
   Procedure SetValueName(n:Integer;name:string);

   Function AddValue(const name,val,vtype:string):integer;
   Procedure InsertValue(n:Integer;const name,val,vtype:string);
   Procedure DeleteValue(n:integer);

   Property NValues:Integer read GetNValues;
   Property FileName:string read GetName write setName;
  end;


  TOLESurface=class(TAutoObject)
   olelev:TOLELevel;
   surf:TJKSurface;
   Constructor Create(olev:TOLELevel;asurf:TJKSurface);

   Function GetMAT:string;
   Procedure SetMat(const mat:string);
   Function GetExtra:double; // deprecated
   Procedure SetExtra(ex:double); // deprecated
   Function GetExtraLightABGR:UInt32;
   Procedure SetExtraLightABGR(ex:UInt32);
   Function GetUScale:double;
   Function GetVScale:double;
   Procedure SetUScale(sc:double);
   Procedure SetVScale(sc:double);
   Procedure SetTXScale(sc:double);

   Function GetNVertices:Integer;
   Function GetAdjFlags:integer;
   Procedure SetAdjFlags(f:integer);
   Function GetSurfFlags:Integer;
   Procedure SetSurfFlags(f:integer);
   Function GetFaceFlags:Integer;
   Procedure SetFaceFlags(f:integer);
   Function GetGeo:integer;
   Function GetLight:integer;
   Function GetTex:integer;
   Function GetNormX:double;
   Function GetNormY:double;
   Function GetNormZ:double;

  Automated
   Procedure Release;
   Procedure Update;
   Procedure UpdateSector;

   Procedure GetNormal(var x,y,z:double);

   Procedure GetAdjoin(var sc,sf:integer);
   Procedure SetAdjoin(sc,sf:integer);
   Procedure GetFlags(var AdjoinFlags, SurfFlags, FaceFlags:Longint);
   Procedure SetFlags(AdjoinFlags, SurfFlags, FaceFlags:Longint);
   Procedure GetGeoLightTex(var geo,light,tex:integer);
   Procedure SetGeoLightTex(geo,light,tex:integer);

   {Vertices}
   Procedure InsertVertex(n:integer;nvx:integer);
   Procedure DeleteVertex(n:integer);
   Function GetVertex(n:integer):Integer;
   Procedure SetVertex(n:integer;nvx:integer);
   Procedure GetVertexUV(n:integer;var u,v:double);
   Procedure SetVertexUV(n:integer;u,v:double);
   Function GetVertexLight(n:integer):double;
   Procedure SetVertexLight(n:integer;light:double);
   Procedure GetVertexRGB(n:integer;var r,g,b:double);
   Procedure SetVertexRGB(n:integer;r,g,b:double);

   Function GetAdjoinSC:Integer;
   Function GetAdjoinSF:Integer;
   Function GetVertexU(n:integer):Double;
   Function GetVertexV(n:integer):Double;
   Function GetVertexR(n:integer):Double;
   Function GetVertexG(n:integer):Double;
   Function GetVertexB(n:integer):Double;

   {JED 0.9}

   Function IsConvex:Wordbool;
   Function IsPlanar:Wordbool;

   Property AdjoinFlags:Integer read GetAdjFlags write SetAdjFlags;
   Property SurfFlags:Integer read GetSurfFlags write SetSurfFlags;
   Property FaceFlags:Integer read GetFaceFlags write SetAdjFlags;
   Property Geo:integer read GetGeo;
   Property Light:integer read GetLight;
   Property Tex:integer read GetTex;

   Property Material:string read GetMAT write SetMat;
   Property ExtraLight:double read GetExtra write SetExtra; // deprecated
   Property TXScale:double read GetUScale write SetTXScale;
   Property UScale:double read GetUScale write SetUScale;
   Property VScale:double read GetVScale write SetVScale;
   Property NormalX:double read GetNormX;
   Property NormalY:double read GetNormY;
   Property NormalZ:double read GetNormZ;

   {JED 0.85}
   Property Nvertices:Integer read GetNVertices;

   {SED 0.1.0}
   Property ExtraLightABGR:UInt32 read GetExtraLightABGR write SetExtraLightABGR;

  end;


  TOLELevel=class(TAutoObject)
   scs,
   surfs,
   ths, lts,cogs:TOleObjList;
   constructor Create; override;
   Destructor Destroy; override;
   Procedure ClearOLEObjs;
   Function GetNSectors:integer;
   Function GetNThings:integer;
   Function GetNLights:integer;
   Function GetMasterCMP:string;
   Procedure SetMasterCMP(const cmp:String);
   Function GetNLayers:Integer;

  automated

   Function GetSector(n:integer):variant;
   Function GetSurface(sc,sf:integer):variant;
   Function GetThing(n:integer):variant;
   Function GetLight(n:integer):variant;
   Function GetCOG(n:integer):variant; {JED 0.81}

   Procedure DeleteSector(n:integer);
   Procedure DeleteThing(n:integer);
   Procedure DeleteLight(n:integer);
   Procedure DeleteCOG(n:integer); {JED 0.81}

   Function AddSector:Integer;
   Function AddThing:Integer;
   Function AddLight:Integer;
   Function AddCOG(const name:string):Integer; {JED 0.81}

   Function GetLayerName(n:integer):String;
   Function AddLayer(const name:string):integer;

   {JED 0.85}
   Property NLayers:integer read GetNLayers;

   Property NSectors:integer read GetNSectors;
   Property NThings:integer read GetNThings;
   Property NLights:integer read GetNLights;
   Property MasterCMP:string read GetMasterCMP write SetMasterCMP;
  end;

  {JED 0.85}
  TOLETemplates=class(TAutoObject)
  automated
   Procedure ClearTemplates;
   Function NTemplates:integer;
   Function GetTemplateName(n:integer):String;
   Function GetTemplateParent(n:integer):string;
   Function GetTemplateValues(n:integer):String;

   Function GetTemplateDescription(n:integer):String;
   Procedure SetTemplateDescription(n:integer;const desc:string);
   Procedure GetTemplateBBox(n:integer;var x1,y1,z1,x2,y2,z2:double);
   Function GetTemplateBBoxEx(n:integer;what:integer):double;
   Procedure SetTemplateBBox(n:integer;x1,y1,z1,x2,y2,z2:double);

   Procedure DeleteTemplate(n:integer);
   Function AddTemplate(const name,parent,values:string):integer;
   Function FindTemplate(const name:string):integer;
   Procedure LoadTemplates(const filename:string);
   Procedure SaveTemplates(const filename:string);
  end;

  TJEDApp = class(TAutoObject)
  private
   lev:TOLELevel;
   tpls:TOLETemplates;
   foreWnd:integer;
  public
   constructor Create; override;
   Destructor Destroy; override;
   Function GetMapMode:integer;
   Procedure SetMapMode(mode:integer);

   Function GetCurSC:integer;
   Procedure SetCurSC(sc:integer);
   Function GetCurTH:integer;
   Procedure SetCurTH(th:integer);
   Function GetCurLT:integer;
   Procedure SetCurLT(sc:integer);

   Function GetCurEX:integer;
   Procedure SetCurEX(ex:integer);


   Function GetLevel:Variant;
   Function GetTemplates:Variant;
   Function GetGameDir:String;
   Function GetProjectDir:String;
   Function GetJEDDir:String;

   Function GetProjectType:Word;
   Procedure SetProjectType(kind:Word);

   Function GetIsMots:wordbool;  // deprecated
   Procedure SetIsMots(mots:wordbool); // deprecated

   Function GetVersion:double;

   Procedure SaveCurApp;
   Procedure RestoreCurApp;

   Function prGetCurSF:integer;
   Function prGetCurED:integer;
   Function prGetCurVX:integer;
   Function prGetCurFR:integer;

   Function GetGridX:double;
   Function GetGridY:double;
   Function GetGridZ:double;

   Function GetCamX:double;
   Function GetCamY:double;
   Function GetCamZ:double;
   Function getIsPreviewActive:WordBool;
   Function GetLevelFile:String;

  automated
   {Functions}

   Procedure Release;

   Procedure NewLevel(mots:WordBool);
   Procedure OpenLevel(const name:string);
   Procedure SaveJED(const name:string);
   Procedure SaveJKL(const name:string);
   Procedure UpdateMap;

   Procedure GetCurSF(var sc,sf:integer);
   Procedure SetCurSF(sc,sf:integer);
   Procedure GetCurVX(var sc,vx:integer);
   Procedure SetCurVX(sc,vx:integer);
   Procedure GetCurED(var sc,sf,ed:integer);
   Procedure SetCurED(sc,sf,ed:integer);
   Procedure GetCurFR(var th,fr:integer);
   Procedure SetCurFR(th,fr:integer);

   {Picking}
    Function PickThing(const curThing:String):String;
    Function PickTemplate(const curtpl:string):string;
    Function PickCMP(const CurCMP:string):string;
    Function PickWav(CurWav:string):string;
    Function PickMAT(CurMAT:string):string;
    Function PickCOG(CurCog:string):string;
{    Function PickLayer(CurLayer:string):string;}
    Function Pick3DO(const cur3do:String):String;
    Function PickAI(const CurAI:string):string;
    Function PickKEY(const CurKEY:string):string;
    Function PickSND(const CurSND:string):string;
    Function PickPUP(const CurPUP:string):string;

    {JED 0.81}
    Function PickSPR(const CurSPR:string):string;
    Function PickPAR(const CurPAR:string):string;
    Function PickPER(const CurPER:string):string;

    Procedure ReloadTemplates;
    Procedure PanMessage(mtype:integer;const msg:string);
    Procedure SendKey(shift:integer;key:integer);
    Function ExecuteMenu(const itemref:string):WordBool;

    {JED 0.81}

    {JED 0.85}
    Function EditSectorFlags(flags:LongInt):LongInt;
    Function EditSurfaceFlags(flags:LongInt):LongInt;
    Function EditAdjoinFlags(flags:LongInt):LongInt;

    Function EditThingFlags(flags:LongInt):LongInt;
    Function EditFaceFlags(flags:LongInt):LongInt;
    Function EditLightFlags(flags:Longint):LongInt;

    Function PickGeo(geo:integer):integer;
    Function PickLightMode(lmode:integer):integer;
    Function PickTEX(tex:integer):integer;

    {Stuff}

    Procedure GetCamXYZ(var x,y,z:double);
    Procedure GetGridXYZ(var x,y,z:double);

    {JED 0.81}
    Procedure SetCamXYZ(x,y,z:double);
    Procedure SetGridXYZ(x,y,z:double);
    Procedure GetGridAxes(var xx,xy,xz,yx,yy,yz,zx,zy,zz:double);
    Function GetGridAxis(naxis,ncoord:integer):double;
    Procedure SetGridAxes(xx,xy,xz,yx,yy,yz:double);

    Procedure GetCamAxes(var xx,xy,xz,yx,yy,yz,zx,zy,zz:double);
    Function GetCamAxis(naxis,ncoord:integer):double;
    Procedure SetCamAxes(xx,xy,xz,yx,yy,yz:double);

    Procedure SetPreviewCamXYZ(x,y,z:double);
    Procedure GetPreviewCamXYZ(var x,y,z:double);
    Procedure SetPreviewCamPY(pch,yaw:double);
    Procedure GetPreviewCamPY(var pch,yaw:double);
    Function GetPreviewCamParam(what:integer):double;


    Function NMultiSelected(what:integer):integer;
    Procedure ClearMultiselection(what:integer);
    Procedure RemoveFromMultiselection(what,n:integer);

    Function GetSelectedSC(n:integer):integer;
    Function GetSelectedTH(n:integer):integer;
    Function GetSelectedLT(n:integer):integer;

    procedure GetSelectedSF(n:integer;var sc,sf:integer);
    procedure GetSelectedED(n:integer;var sc,sf,ed:integer);
    procedure GetSelectedVX(n:integer;var sc,vx:integer);
    procedure GetSelectedFR(n:integer;var th,fr:integer);

    Function GetSelectedSFop(n:integer;what:integer):integer;
    Function GetSelectedEDop(n:integer;what:integer):integer;
    Function GetSelectedVXop(n:integer;what:integer):integer;
    Function GetSelectedFRop(n:integer;what:integer):integer;

    Function SelectSC(sc:integer):integer;
    Function SelectSF(sc,sf:integer):integer;
    Function SelectED(sc,sf,ed:integer):integer;
    Function SelectVX(sc,vx:integer):integer;
    Function SelectTH(th:integer):integer;
    Function SelectFR(th,fr:integer):integer;
    Function SelectLT(lt:integer):integer;

    Function FindSelectedSC(sc:integer):integer;
    Function FindSelectedSF(sc,sf:integer):integer;
    Function FindSelectedED(sc,sf,ed:integer):integer;
    Function FindSelectedVX(sc,vx:integer):integer;
    Function FindSelectedTH(th:integer):integer;
    Function FindSelectedFR(th,fr:integer):integer;
    Function FindSelectedLT(lt:integer):integer;

    {JED 0.85}
    Procedure StartUndo(const name:string);
    Procedure SaveUndoForThing(n:integer;change:integer);
    Procedure SaveUndoForLight(n:integer;change:integer);
    Procedure SaveUndoForSector(n:integer;change:integer;whatpart:integer);
    Procedure ClearUndoBuffer;
    Procedure ApplyUndo;
    Function GetJEDSetting(const name:string):variant;
    Function IsLayerVisible(n:integer):WordBool;

    {JED 0.9}
    procedure CheckConsistencyErrors;
    procedure CheckResources;
    Function NConsistencyErrors:integer;
    Function GetConsErrorString(n:integer):String;
    Function GetConsErrorType(n:integer):integer;
    Function GetConsErrorSector(n:integer):integer;
    Function GetConsErrorSurface(n:integer):integer;
    Function GetConsErrorThing(n:integer):integer;
    Function GetConsErrorCog(n:integer):integer;

    Procedure ClearExtraObjects;
    Function AddExtraVertex(x,y,z:double):integer;
    Function AddExtraLine(x1,y1,z1,x2,y2,z2:double):Integer;
    Procedure DeleteExtraObject(n:integer);


   {Properties}
   Property IsPreviewActive:WordBool read getIsPreviewActive;
   Property Version:double read GetVersion;
   Property MapMode:integer read GetMapMode write SetMapMode;
   Property CurSC:integer read GetCurSC write SetCurSC;
   Property CurTH:integer read GetCurTH write SetCurTH;
   Property CurLT:integer read GetCurLT write SetCurLT;
   Property CurSF:integer read prGetCurSF;
   Property CurED:integer read prGetCurED;
   Property CurVX:integer read prGetCurVX;
   Property CurFR:integer read prGetCurFR;

   Property Level:Variant read GetLevel;

   Property GridX:double read GetGridX;
   Property GridY:double read GetGridY;
   Property GridZ:double read GetGridZ;
   Property CamX:double read GetCamX;
   Property CamY:double read GetCamY;
   Property CamZ:double read GetCamZ;

   Property GameDir:String read GetGameDir;
   Property ProjectDir:String read GetProjectDir;
   Property JEDDir:String read GetJedDir;
   Property IsMots:WordBool read GetIsMots write SetIsMots; // deprecated

   {JED 0.85}
   Property LevelFile:String read GetLevelFile;
   Property Templates:variant read GetTemplates;

   {JED 0.9}
   Property CurEX:integer read GetCurEX write SetCurEX;

   {SED 0.1.0}
   Property CurrentProject:Word read GetProjectType write SetProjectType;

  end;

implementation
uses Jed_Main, ResourcePicker, U_CogForm, ListRes, SysUtils, U_preview,
     FlagEditor, U_SCFEdit, U_options, U_tbar, JED_COM;

Constructor TOleObjList.Create;
begin
 idxObjs:=TList.Create;
 Objs:=TList.Create;
end;

Destructor TOleObjList.Destroy;
begin
 DeleteAll;
 Objs.free;
 idxObjs.free;
end;

Procedure TOleObjList.DeleteAll;
var i:integer;
begin
 for i:=0 to Objs.count-1 do
  TAutoObject(Objs[i]).Free;
 Objs.clear;
 idxObjs.Clear;
end;

Procedure TOleObjList.AddObj(idxObj:TObject;oleObj:TAutoObject);
begin
 Objs.Add(oleObj);
 idxObjs.Add(idxObj);
end;

Function TOleObjList.GetObj(idxObj:TObject):TAutoObject;
var i:integer;
begin
 result:=nil;
 i:=idxObjs.IndexOf(idxObj);
 if i=-1 then exit;
 Result:=TAutoObject(Objs[i]);
 Result.AddRef;
end;

Procedure TOleObjList.ReleaseObj(idxobj:TObject);
var i:integer;
    au:TAutoObject;
begin
 i:=idxObjs.IndexOf(idxObj);
 if i=-1 then exit;
 au:=TAutoObject(Objs[i]);
 if au.Release=0 then
 begin
  idxObjs.Delete(i);
  Objs.Delete(i);
 end;
end;

Procedure TOleObjList.DeleteObj(idxObj:TObject);
var i:integer;
begin
 i:=idxObjs.IndexOf(idxObj);
 if i=-1 then exit;
 TAutoObject(Objs[i]).Free;
 idxObjs.Delete(i);
 Objs.Delete(i);
end;

Constructor TOLESector.Create(olev:TOLELevel;sector:TJKSector);
begin
 Inherited Create;
 Olelev:=Olev;
 sec:=sector;
end;

Function TOLESector.GetFlags:Longint;
begin
 Result:=sec.Flags;
end;

Procedure TOLESector.SetFlags(f:longint);
begin
 sec.Flags:=f;
end;

Function TOLESector.GetAmbient:double; deprecated
begin
 Result:= RgbToIntensity(sec.Ambient);
end;

Procedure TOLESector.SetAmbient(amb:double); deprecated
begin
 sec.Ambient:=IntensityToColor(amb);
 sec.Ambient.a := 0.0;
end;

Function TOLESector.GetAmbientBGR:UInt32;
begin
 Result:=EncodeBGR(sec.Ambient);
end;

Procedure TOLESector.SetAmbientBGR(amb:UInt32);
begin
 sec.ambient:=DecodeBGR(amb);
end;

Function TOLESector.GetExtra:double; deprecated
begin
  Result := RgbToIntensity(sec.ExtraLight);
end;
Procedure TOLESector.SetExtra(ex:double); deprecated
begin
  sec.ExtraLight := IntensityToColor(ex);
  sec.ExtraLight.a := 0.0;
end;

Function TOLESector.GetExtraLightBGR:UInt32;
begin
 Result := EncodeBGR(sec.ExtraLight);
end;

Procedure TOLESector.SetExtraLightBGR(ex:UInt32);
begin
 sec.ExtraLight := DecodeBGR(ex);
end;

Function TOLESector.GetCMP:string;
begin
 Result:=sec.ColorMap;
end;

Procedure TOLESector.SetCMP(const cmp:string);
begin
 sec.ColorMap:=cmp;
end;

Function TOLESector.GetSound:String;
begin
 Result:=sec.Sound;
end;

Procedure TOLESector.SetSound(const snd:string);
begin
 sec.Sound:=snd;
end;

Function TOLESector.GetVol:double;
begin
 Result:=sec.soundVolume;
end;

Procedure TOLESector.SetVol(vol:double);
begin
 sec.soundVolume:=vol;
end;

Function TOLESector.GetLayer:Integer;
begin
 Result:=sec.Layer;
end;

Procedure TOLESector.SetLayer(l:integer);
begin
 sec.Layer:=l;
end;

Function TOLESector.GetTintR:double;
begin
 Result:=sec.tint.r;
end;

Function TOLESector.GetTintG:double;
begin
 Result:=sec.tint.g;
end;

Function TOLESector.GetTintB:double;
begin
 Result:=sec.tint.b;
end;

Function TOLESector.GetTintBGR:UInt32;
begin
 Result := EncodeBGR(sec.Tint);
end;

Procedure TOLESector.SetTintBGR(ex:UInt32);
begin
 sec.Tint := DecodeBGR(ex);
end;

Procedure TOLESector.GetTint(var r,g,b:double);
begin
 r:=sec.tint.r;
 g:=sec.tint.g;
 b:=sec.tint.b;
end;

Procedure TOLESector.SetTint(r,g,b:double);
begin
 sec.tint.r:=r;
 sec.tint.g:=g;
 sec.tint.b:=b;
end;

Function TOLESector.GetNVertices:Integer;
begin
 Result:=sec.Vertices.Count;
end;

Function TOLESector.GetNSurfaces:Integer;
begin
 Result:=sec.Surfaces.Count;
end;

Function TOLESector.GetSurface(n:integer):Variant;
begin
 Result:=OleLev.GetSurface(sec.num,n);
end;

Function TOLESector.AddSurface:Integer;
var surf:TJKSurface;
begin
 surf:=sec.NewSurface;
 surf.Num:=sec.Surfaces.count;
 Result:=sec.Surfaces.Add(surf);
end;

Function TOLESector.InsertSurface(n:integer):integer;
var surf:TJKSurface;
begin
 surf:=sec.NewSurface;
 Result:=n;
 sec.Surfaces.Insert(n,surf);
 sec.Renumber;
end;

Procedure TOLESector.DeleteSurface(n:integer);
var surf:TJKSurface;
begin
 surf:=Sec.Surfaces[n];
 Lev_Utils.RemoveSurfRefs(Level,surf);
 sec.surfaces.Delete(n);
 sec.Renumber;
end;

Function TOLESector.AddVertex(x,y,z:double):Integer;
var v:TJKVertex;
begin
 v:=sec.NewVertex;
 Result:=sec.Vertices.Count-1;
 v.x:=x;
 v.y:=y;
 v.z:=z;
end;

Function TOLESector.FindVertex(x,y,z:double):Integer;
begin
 Result:=sec.FindVX(x,y,z);
end;

Procedure TOLESector.GetVertex(n:integer;var x,y,z:double);
var v:TJKVertex;
begin
 v:=sec.Vertices[n];
 x:=v.x;
 y:=v.y;
 z:=v.z;
end;

Procedure TOLESector.SetVertex(n:integer;x,y,z:double);
var v:TJKVertex;
begin
 v:=sec.Vertices[n];
 v.x:=x;
 v.y:=y;
 v.z:=z;
end;

Procedure TOLESector.DeleteVertex(n:integer);
var v:TJKVertex;
    j,i:integer;
begin
 v:=sec.Vertices[n];
 for i:=0 to sec.Surfaces.count-1 do
 With sec.Surfaces[i] do
 begin
  j:=Vertices.IndexOf(v);
  if j<>-1 then DeleteVertex(j);
 end;
 sec.Vertices.Delete(n);
 v.free;
 sec.Renumber;
end;

Procedure TOLESector.Update;
begin
 Sec.Renumber;
 JedMain.SectorChanged(sec);
end;

Function TOLESector.IsConvex:WordBool;
begin
 result:=IsSectorConvex(sec);
end;

Procedure TOLESector.Release;
begin
 olelev.scs.ReleaseObj(sec);
end;

{Surface}
Constructor TOLESurface.Create;
begin
 Inherited Create;
 olelev:=olev;
 surf:=asurf;
end;

Procedure TOLESurface.Release;
begin
 OleLev.surfs.ReleaseObj(surf);
end;

Procedure TOLESurface.Update;
begin
 surf.RecalcAll;
end;

Procedure TOLESurface.UpdateSector;
begin
 JedMain.SectorChanged(surf.Sector);
end;

Function TOLESurface.GetAdjFlags:integer;
begin
 Result:=surf.AdjoinFlags;
end;

Procedure TOLESurface.SetAdjFlags(f:integer);
begin
 surf.AdjoinFlags:=f;
end;

Function TOLESurface.GetSurfFlags:Integer;
begin
 Result:=surf.SurfFlags;
end;

Procedure TOLESurface.SetSurfFlags(f:integer);
begin
 surf.SurfFlags:=f;
end;

Function TOLESurface.GetFaceFlags:Integer;
begin
 Result:=surf.FaceFlags;
end;

Procedure TOLESurface.SetFaceFlags(f:integer);
begin
 surf.FaceFlags:=f;
end;

Function TOLESurface.GetGeo:integer;
begin
 Result:=surf.geo;
end;

Function TOLESurface.GetLight:integer;
begin
 Result:=surf.light;
end;

Function TOLESurface.GetTex:integer;
begin
 Result:=surf.tex;
end;

Function TOLESurface.GetNVertices:Integer;
begin
 Result:=surf.vertices.count;
end;

Function TOLESurface.GetAdjoinSC:Integer;
begin
 if surf.adjoin=nil then result:=-1 else result:=surf.adjoin.Sector.num;
end;

Function TOLESurface.GetAdjoinSF:Integer;
begin
 if surf.adjoin=nil then result:=-1 else result:=surf.adjoin.num;
end;

Function TOLESurface.GetVertexU(n:integer):Double;
begin
 result:=surf.Txvertices[n].u;
end;

Function TOLESurface.GetVertexV(n:integer):Double;
begin
 result:=surf.Txvertices[n].v;
end;

Function TOLESurface.GetVertexR(n:integer):Double;
begin
 result:=surf.Txvertices[n].color.r;
end;

Function TOLESurface.GetVertexG(n:integer):Double;
begin
 result:=surf.Txvertices[n].color.g;
end;

Function TOLESurface.GetVertexB(n:integer):Double;
begin
 result:=surf.Txvertices[n].color.b;
end;


Procedure TOLESurface.GetAdjoin(var sc,sf:integer);
begin
 if surf.Adjoin=nil then begin sc:=-1; sf:=-1; end
 else begin sc:=surf.adjoin.Sector.num; sf:=surf.adjoin.num; end;
end;

Procedure TOLESurface.SetAdjoin(sc,sf:integer);
begin
 if (sc<0) or (sf<0) then surf.Adjoin:=nil else
 surf.Adjoin:=Level.Sectors[sc].Surfaces[sf];
end;

Procedure TOLESurface.GetFlags(var AdjoinFlags, SurfFlags, FaceFlags:Longint);
begin
 AdjoinFlags:=surf.AdjoinFlags;
 SurfFlags:=surf.SurfFlags;
 FaceFlags:=surf.FaceFlags;
end;

Procedure TOLESurface.SetFlags(AdjoinFlags, SurfFlags, FaceFlags:Longint);
begin
 surf.AdjoinFlags:=AdjoinFlags;
 surf.SurfFlags:=SurfFlags;
 surf.FaceFlags:=FaceFlags;
end;

Procedure TOLESurface.GetGeoLightTex(var geo,light,tex:integer);
begin
 geo:=surf.geo;
 light:=surf.light;
 tex:=surf.tex;
end;

Procedure TOLESurface.SetGeoLightTex(geo,light,tex:integer);
begin
 surf.geo:=geo;
 surf.light:=light;
 surf.tex:=tex;
end;

Function TOLESurface.GetMAT:string;
begin
 Result:=surf.Material;
end;

Procedure TOLESurface.SetMat(const mat:string);
begin
 surf.Material:=mat;
end;

Function TOLESurface.GetExtra:double; deprecated
begin
 Result := RgbaToIntensity(surf.ExtraLight);
end;
Procedure TOLESurface.SetExtra(ex:double); deprecated
begin
 surf.ExtraLight:= IntensityToColor(ex);
end;

Function TOLESurface.GetExtraLightABGR:UInt32;
begin
 Result := EncodeABGR(surf.ExtraLight);
end;

Procedure TOLESurface.SetExtraLightABGR(ex:UInt32);
begin
 surf.ExtraLight := DecodeABGR(ex);
end;

Function TOLESurface.GetUScale:double;
begin
 Result:=surf.uscale;
end;

Function TOLESurface.GetVScale:double;
begin
 Result:=surf.uscale;
end;

Procedure TOLESurface.SetTXScale(sc:double);
begin
 surf.uscale:=sc;
 surf.vscale:=sc;
end;

Procedure TOLESurface.SetUScale(sc:double);
begin
 surf.uscale:=sc;
end;

Procedure TOLESurface.SetVScale(sc:double);
begin
 surf.vscale:=sc;
end;

Procedure TOLESurface.GetNormal(var x,y,z:double);
begin
 x:=surf.normal.dx;
 y:=surf.normal.dy;
 z:=surf.normal.dz;
end;

Function TOLESurface.GetNormX:double;
begin
 Result:=surf.normal.dx;
end;

Function TOLESurface.GetNormY:double;
begin
 Result:=surf.normal.dy;
end;

Function TOLESurface.GetNormZ:double;
begin
 Result:=surf.normal.dz;
end;

Procedure TOLESurface.InsertVertex(n:integer;nvx:integer);
begin
 Surf.InsertVertex(n,surf.sector.Vertices[nvx]);
end;

Procedure TOLESurface.DeleteVertex(n:integer);
begin
 Surf.DeleteVertex(n);
end;

Function TOLESurface.GetVertex(n:integer):Integer;
begin
 Result:=Surf.Vertices[n].Num;
end;

Procedure TOLESurface.SetVertex(n:integer;nvx:integer);
begin
 surf.Vertices[n]:=surf.sector.Vertices[nvx];
end;

Procedure TOLESurface.GetVertexUV(n:integer;var u,v:double);
var tv:TTXVertex;
begin
 tv:=surf.TXVertices[n];
 u:=tv.u;
 v:=tv.v;
end;

Procedure TOLESurface.SetVertexUV(n:integer;u,v:double);
var tv:TTXVertex;
begin
 tv:=surf.TXVertices[n];
 tv.u:=u;
 tv.v:=v;
end;

Function TOLESurface.GetVertexLight(n:integer):double;
var tv:TTXVertex;
begin
 tv:=surf.TXVertices[n];
 Result:=tv.color.a;
end;

Procedure TOLESurface.SetVertexLight(n:integer;light:double);
var tv:TTXVertex;
begin
 tv:=surf.TXVertices[n];
 tv.color.a:=light;
end;

Procedure TOLESurface.GetVertexRGB(n:integer;var r,g,b:double);
var tv:TTXVertex;
begin
 tv:=surf.TXVertices[n];
 r:=tv.color.r;
 g:=tv.color.g;
 b:=tv.color.b;
end;

Procedure TOLESurface.SetVertexRGB(n:integer;r,g,b:double);
var tv:TTXVertex;
begin
 tv:=surf.TXVertices[n];
 tv.color.r:=r;
 tv.color.g:=g;
 tv.color.b:=b;
end;

Function TOLESurface.IsConvex:Wordbool;
begin
 result:=IsSurfConvex(surf);
end;

Function TOLESurface.IsPlanar:Wordbool;
begin
 result:=IsSurfPlanar(surf);
end;


{Thing}

Constructor TOLEThing.Create(olev:TOLELevel;th:TJKThing);
begin
 Inherited Create;
 olelev:=olev;
 thing:=th;
end;

Function TOLEThing.GetNValues:Integer;
begin
 Result:=thing.Vals.count;
end;

Function TOLEThing.GetSec:Integer;
begin
 if thing.sec=nil then Result:=-1 else result:=thing.sec.num;
end;

Procedure TOLEThing.SetSec(n:integer);
begin
 if n=-1 then thing.sec:=nil else thing.sec:=Level.Sectors[n];
end;

Function TOLEThing.GetName:string;
begin
 Result:=thing.Name;
end;

Procedure TOLEThing.setName(const name:string);
begin
 thing.Name:=name;
end;

Function TOLEThing.GetLayer:Integer;
begin
 Result:=thing.Layer;
end;

Procedure TOLEThing.SetLayer(n:integer);
begin
 thing.Layer:=n;
end;

Procedure TOLEThing.Release;
begin
 OleLev.ths.ReleaseObj(thing);
end;

Procedure TOLEThing.Update;
begin
 JedMain.ThingChanged(thing);
end;

Function TOLEThing.GetX:double;
begin
 Result:=thing.x;
end;

Function TOLEThing.GetY:double;
begin
 Result:=thing.y;
end;

Function TOLEThing.GetZ:double;
begin
 Result:=thing.z;
end;

Function TOLEThing.GetPCH:double;
begin
 Result:=thing.pch;
end;

Function TOLEThing.GetYAW:double;
begin
 Result:=thing.yaw;
end;

Function TOLEThing.GetROL:double;
begin
 Result:=thing.rol;
end;


Procedure TOLEThing.GetXYZ(var x,y,z:double);
begin
 x:=thing.x;
 y:=thing.y;
 z:=thing.z;
end;

Procedure TOLEThing.SetXYZ(x,y,z:double);
begin
 thing.x:=x;
 thing.y:=y;
 thing.z:=z;
end;

Procedure TOLEThing.GetOrient(var pch,yaw,rol:double);
begin
 pch:=thing.pch;
 yaw:=thing.yaw;
 rol:=thing.rol;
end;

Procedure TOLEThing.SetOrient(pch,yaw,rol:double);
begin
 thing.pch:=pch;
 thing.yaw:=yaw;
 thing.rol:=rol;
end;

Function TOLEThing.GetValueName(n:Integer):String;
begin
 Result:=Thing.Vals[n].Name;
end;

Function TOLEThing.GetValueData(n:integer):string;
begin
 Result:=Thing.Vals[n].AsString;
end;


Procedure TOLEThing.GetValue(n:Integer;var name,val:string);
var v:TTPLValue;
begin
 v:=thing.Vals[n];
 name:=v.name;
 val:=v.AsString;
end;

Procedure TOLEThing.SetValue(n:Integer;const name,val:string);
var v:TTPLValue;
begin
 v:=thing.Vals[n];
 S2TPLVal(name+'='+val,v);
end;

Function TOLEThing.AddValue(const name,val:string):integer;
var v:TTPLValue;
begin
 v:=TTplValue.Create;
 S2TPLVal(name+'='+val,v);
 Result:=Thing.Vals.Add(v);
end;

Procedure TOLEThing.InsertValue(n:Integer;const name,val:string);
var v:TTPLValue;
begin
 v:=TTplValue.Create;
 S2TPLVal(name+'='+val,v);
 Thing.Vals.Insert(n,v);
end;

Procedure TOLEThing.DeleteValue(n:integer);
var v:TTPLValue;
begin
 v:=Thing.Vals[n];
 Thing.Vals.Delete(n);
 v.free;
end;

{Light}

Constructor TOLELight.Create(olev:TOLELevel;alt:TSedLight);
begin
 Inherited Create;
 Olelev:=olev;
 lt:=alt;
end;

Procedure TOLELight.Update;
begin
 JEDMain.LevelChanged;
end;

Procedure TOLELight.Release;
begin
 OleLev.lts.ReleaseObj(lt);
end;

Function TOLELight.GetRange:double;
begin
 Result:=lt.Range;
end;

Procedure TOLELight.SetRange(d:double);
begin
 lt.range:=d;
end;

Function TOLELight.GetLayer:integer;
begin
 Result:=lt.layer;
end;

Procedure TOLELight.SetLayer(n:integer);
begin
 lt.layer:=n;
end;

Function TOLELight.GetFlags:Integer;
begin
 Result:=lt.Flags;
end;

Procedure TOLELight.SetFlags(f:integer);
begin
 lt.flags:=f;
end;

Function TOLELight.GetR:double;
begin
 Result:=lt.color.r;
end;

Function TOLELight.GetG:double;
begin
 Result:=lt.color.g;
end;

Function TOLELight.GetB:double;
begin
 Result:=lt.color.b;
end;

Function TOLELight.GetX:double;
begin
 Result:=lt.position.x;
end;

Function TOLELight.GetY:double;
begin
 Result:=lt.position.y;
end;

Function TOLELight.GetZ:double;
begin
 Result:=lt.position.z;
end;

Function TOLELight.GetInt:double;
begin
 Result:=lt.Intensity;
end;

Function TOLELight.GetRGBInt:double;
begin
 Result:=lt.intensity;
end;


Procedure TOLELight.GetXYZ(var x,y,z:double);
begin
 x:=lt.position.x;
 y:=lt.position.y;
 z:=lt.position.z;
end;

Procedure TOLELight.SetXYZ(x,y,z:double);
begin
 lt.position.x:=x;
 lt.position.y:=y;
 lt.position.z:=z;
end;

Procedure TOLELight.GetRGB(var r,g,b:double);
begin
 r:=lt.color.r;
 g:=lt.color.g;
 b:=lt.color.b;
end;

Procedure TOLELight.SetRGB(r,g,b:double);
begin
 lt.color.r:=r;
 lt.color.g:=g;
 lt.color.b:=b;
end;

Procedure TOLELight.GetIntensity(var white,rgb:double);
begin
 white:=Lt.Intensity;
 rgb:=Lt.Intensity;
end;

Procedure TOLELight.SetIntensity(white,rgb:double);
begin
 Lt.Intensity:=white;
 Lt.Intensity:=rgb;
end;

{COG}
Constructor TOLECOG.Create(olev:TOLELevel;cg:TCOG);
begin
 Inherited Create;
 olelev:=olev;
 cog:=cg;
end;

Function TOLECOG.GetNValues:Integer;
begin
 result:=cog.Vals.count;
end;

Function TOLECOG.GetName:string;
begin
 result:=cog.Name;
end;

Procedure TOLECOG.setName(const name:string);
begin
 cog.Name:=name;
end;

Procedure TOLECOG.Release;
begin
 OleLev.cogs.ReleaseObj(cog);
end;

Procedure TOLECOG.Update;
var n:integer;
begin
 n:=Level.Cogs.IndexOf(Cog);
 if n<>-1 then CogForm.UpdateCog(n);
end;


Function TOLECOG.GetValue(n:Integer):string;
begin
 Result:=cog.Vals[n].AsString;
end;

Function TOLECOG.SetValue(n:Integer;const val:string):WordBool;
begin
 Result:=cog.Vals[n].JedVal(val);
end;

Function TOLECOG.GetValueName(n:Integer):String;
begin
 result:=cog.Vals[n].name;
end;

Function TOLECOG.GetValueType(n:Integer):String;
begin
 Result:=GetCogTypeName(cog.Vals[n].cog_type);
end;

Procedure TOLECOG.SetValueName(n:Integer;name:string);
begin
 Cog.Vals[n].Name:=name;
end;

Procedure TOLECOG.SetValueType(n:Integer;vtype:string);
begin
 Cog.Vals[n].cog_type:=GetCOGType(vtype);
end;


Function TOLECOG.AddValue(const name,val,vtype:string):integer;
var v:TCOGValue;
begin
 v:=TCogValue.Create;
 v.name:=name;
 v.vtype:=GetCogVType(vtype);
 v.cog_type:=GetCogType(vtype);
 v.Val(val);
 result:=cog.Vals.Add(v);
end;

Procedure TOLECOG.InsertValue(n:Integer;const name,val,vtype:string);
var v:TCOGValue;
begin
 v:=TCogValue.Create;
 v.name:=name;
 v.vtype:=GetCogVType(vtype);
 v.cog_type:=GetCogType(vtype);
 v.Val(val);
 cog.Vals.Insert(n,v);
end;

Procedure TOLECOG.DeleteValue(n:integer);
var v:TCOGValue;
begin
 v:=Cog.Vals[n];
 cog.Vals.Delete(n);
 v.free;
end;

{Level}

constructor TOLELevel.Create;
begin
 Inherited Create;
 scs:=TOLEObjList.Create;
 surfs:=TOleObjList.Create;
 ths:=TOLEObjList.Create;
 lts:=TOLEObjList.Create;
 cogs:=TOLEObjList.Create;
end;

Destructor TOLELevel.Destroy;
begin
 scs.Free;
 surfs.Free;
 ths.Free;
 lts.Free;
 cogs.free;
 Inherited Destroy;
end;

Procedure TOLELevel.ClearOLEObjs;
begin
 if self=nil then exit;
 scs.DeleteAll;
 surfs.DeleteAll;
 ths.DeleteAll;
 lts.DeleteAll;
 cogs.DeleteAll;
end;

Function TOLELevel.GetMasterCMP:string;
begin
 Result:=Level.MasterCMP;
end;

Procedure TOLELevel.SetMasterCMP(const cmp:String);
begin
 Level.MasterCMP:=cmp;
end;

Function TOLELevel.GetLayerName(n:integer):String;
begin
 Result:=Level.GetLayerName(n);
end;

Function TOLELevel.AddLayer(const name:string):integer;
begin
 Result:=Level.AddLayer(name);
end;

Function TOLELevel.GetNSectors:integer;
begin
 Result:=Level.Sectors.Count;
end;

Function TOLELevel.GetNThings:integer;
begin
 Result:=Level.Things.Count;
end;

Function TOLELevel.GetNLights:integer;
begin
 Result:=Level.Lights.Count;
end;

Function TOLELevel.GetNLayers:Integer;
begin
 Result:=Level.Layers.Count;
end;

Function TOLELevel.GetSector(n:integer):variant;
var sc:TOLESector;
    sec:TJKSector;
begin
 sec:=Level.Sectors[n];

 sc:=TOLESector(scs.GetObj(sec));
 if sc=nil then
 begin
  sc:=TOLESector.Create(self,sec);
  scs.AddObj(sec,sc);
 end;

 Result:=sc.OleObject;
end;

Function TOLELevel.GetSurface(sc,sf:integer):variant;
var osf:TOLESurface;
    surf:TJKSurface;
begin
 surf:=Level.Sectors[sc].Surfaces[sf];

 osf:=TOLESurface(surfs.GetObj(surf));
 if osf=nil then
 begin
  osf:=TOLESurface.Create(self,surf);
  surfs.AddObj(surf,osf);
 end;

 Result:=osf.OleObject;
end;

Function TOLELevel.GetThing(n:integer):variant;
var th:TOLEThing;
    thing:TJKThing;
begin
 thing:=Level.Things[n];

 th:=TOLEThing(ths.GetObj(thing));
 if th=nil then
 begin
  th:=TOLEThing.Create(self,thing);
  ths.AddObj(thing,th);
 end;

 Result:=th.OleObject;
end;

Function TOLELevel.GetLight(n:integer):variant;
var lt:TOLELight;
    light:TSedLight;
begin
 Light:=Level.Lights[n];

 lt:=TOLELight(lts.GetObj(light));
 if lt=nil then
 begin
  lt:=TOLELight.Create(self,light);
  lts.AddObj(light,lt);
 end;

 Result:=lt.OleObject;
end;

Function TOLELevel.GetCOG(n:integer):variant;
var cg:TOLECOG;
    cog:TCOG;
begin
 cog:=Level.Cogs[n];

 cg:=TOLECOG(cogs.GetObj(cog));
 if cg=nil then
 begin
  cg:=TOLECOG.Create(self,cog);
  cogs.AddObj(cog,cg);
 end;

 Result:=cg.OleObject;
end;


Procedure TOLELevel.DeleteSector(n:integer);
var i:integer;
    sec:TJKSector;
begin
 sec:=Level.Sectors[n];
 for i:=0 to sec.surfaces.count-1 do
  surfs.DeleteObj(sec.surfaces[i]);

 scs.DeleteObj(sec);
 Lev_Utils.DeleteSector(Level,n);
end;

Procedure TOLELevel.DeleteThing(n:integer);
begin
 ths.DeleteObj(Level.Things[n]);
 Lev_Utils.DeleteThing(Level,n);
end;

Procedure TOLELevel.DeleteLight(n:integer);
begin
 lts.DeleteObj(Level.Lights[n]);
 Lev_Utils.DeleteLight(Level,n);
end;

Procedure TOLELevel.DeleteCOG(n:integer);
var cog:TCOG;
begin
 Lev_utils.DeleteCOG(level,n);
end;


Function TOLELevel.AddSector:Integer;
var sec:TJKSector;
begin
 sec:=Level.NewSector;
 Result:=Level.Sectors.Add(sec);
 Level.RenumSecs;
 JedMain.SectorAdded(sec);
end;

Function TOLELevel.AddThing:Integer;
var th:TJKThing;
begin
 th:=Level.NewThing;
 Result:=Level.Things.Add(th);
 Level.RenumThings;
 JedMain.ThingAdded(th);
end;

Function TOLELevel.AddLight:Integer;
var lt:TSedLight;
begin
 lt:=Level.NewLight;
 Result:=Level.lights.Add(lt);
 JedMain.LevelChanged;
end;

Function TOLELevel.AddCOG(const name:string):Integer;
var cg:TCOG;
    cf:TCogFile;
    i:integer;
    cv:TCOGValue;
begin
 cg:=TCOG.Create;
 cg.Name:=name;
 
 cf:=TCogFile.Create;
 cf.LoadNoLocals(Name);

 for i:=0 to cf.count-1 do
 begin
  cv:=TCOGValue.Create;
  cg.Vals.Add(cv);
  cv.Assign(cf[i]);
 end;

 cf.Free;

 Result:=Level.Cogs.Add(cg);
 CogForm.RefreshList;
 JedMain.LevelChanged;
end;


{TOLETemplates}

Procedure TOLETemplates.ClearTemplates;
begin
 Templates.Clear;
end;

Function TOLETemplates.NTemplates:integer;
begin
 Result:=Templates.count;
end;

Function TOLETemplates.GetTemplateName(n:integer):String;
begin
 Result:=Templates[n].name;
end;

Function TOLETemplates.GetTemplateParent(n:integer):string;
begin
 Result:=Templates[n].Parent;
end;

Function TOLETemplates.GetTemplateValues(n:integer):String;
begin
 Result:=Templates[n].ValsAsString;
end;

Function TOLETemplates.GetTemplateDescription(n:integer):String;
begin
 Result:=Templates[n].Desc;
end;

Procedure TOLETemplates.SetTemplateDescription(n:integer;const desc:string);
begin
 Templates[n].Desc:=desc;
end;

Procedure TOLETemplates.GetTemplateBBox(n:integer;var x1,y1,z1,x2,y2,z2:double);
var tpl:TTemplate;
begin
 tpl:=Templates[n];
 x1:=tpl.bbox.x1;
 x2:=tpl.bbox.x2;
 y1:=tpl.bbox.y1;
 y2:=tpl.bbox.y2;
 z1:=tpl.bbox.z1;
 z2:=tpl.bbox.z2;
end;

Function TOLETemplates.GetTemplateBBoxEx(n:integer;what:integer):double;
var tpl:TTemplate;
begin
 tpl:=Templates[n];
 case what of
  1: Result:=tpl.bbox.x1;
  2: Result:=tpl.bbox.y1;
  3: Result:=tpl.bbox.z1;
  4: Result:=tpl.bbox.x2;
  5: Result:=tpl.bbox.y2;
  6: Result:=tpl.bbox.z2;
  else  Result:=0;
 end;
end;

Procedure TOLETemplates.SetTemplateBBox(n:integer;x1,y1,z1,x2,y2,z2:double);
var tpl:TTemplate;
begin
 tpl:=Templates[n];
 tpl.bbox.x1:=x1;
 tpl.bbox.x2:=x2;
 tpl.bbox.y1:=y1;
 tpl.bbox.y2:=y2;
 tpl.bbox.z1:=z1;
 tpl.bbox.z2:=z2;
end;

Procedure TOLETemplates.DeleteTemplate(n:integer);
begin
 Templates.DeleteTemplate(n);
end;

Function TOLETemplates.AddTemplate(const name,parent,values:string):Integer;
begin
 Result:=Templates.AddFromString(
 Concat(name,' ',parent,' ',values));
end;

Function TOLETemplates.FindTemplate(const name:string):integer;
begin
 Result:=Templates.IndexOfName(name);
end;

Procedure TOLETemplates.LoadTemplates(const filename:string);
begin
 Templates.LoadFromFile(filename);
end;

Procedure TOLETemplates.SaveTemplates(const filename:string);
begin
 Templates.SaveToFile(filename);
end;


constructor TJEDApp.Create;
begin
 inherited Create;
end;

Destructor TJEDApp.Destroy;
begin
 lev.Free;
 tpls.free;
 Inherited destroy;
end;

Procedure TJEDApp.Release;
begin
 Inherited Release;
end;

Function TJEDApp.GetVersion:double;
begin
 ValDouble(SedVerNum,Result);
end;


Function TJEDApp.GetIsMots:WordBool; deprecated
begin
 Result:= (GlobalVars.CurrentProject = TProjectType.MOTS);
end;

Procedure TJEDApp.SetIsMots(mots:WordBool); deprecated
 var kind: TProjectType;
begin
 kind := TProjectType.JKDF2;
 if mots then kind := TProjectType.MOTS;
 U_Options.SetProjectType(kind);
end;

Function TJEDApp.GetProjectType:Word;
begin
 Result:=Word(GlobalVars.CurrentProject);
end;

Procedure TJEDApp.SetProjectType(kind:Word);
begin
 U_Options.SetProjectType(TProjectType(kind));
end;

Function TJEDApp.GetLevel:Variant;
begin
 if lev=nil then lev:=TOLELevel.Create;
 Result:=Lev.OleObject;
end;

Function TJEDApp.GetTemplates:Variant;
begin
 if tpls=nil then tpls:=TOLETemplates.Create;
 Result:=tpls.OleObject;
end;


Procedure TJEDApp.UpdateMap;
begin
 JedMain.Invalidate;
end;

Function TJEDApp.GetMapMode:integer;
begin
 result:=JedMain.Map_Mode;
end;

Procedure TJEDApp.SetMapMode(mode:integer);
begin
 JedMain.SetMapMode(Mode);
end;

Function TJEDApp.GetCurSC:integer;
begin
 Result:=JedMain.Cur_SC;
end;

Procedure TJEDApp.SetCurSC(sc:integer);
begin
 JedMain.SetCurSC(sc);
end;

Function TJEDApp.GetCurTH:integer;
begin
 Result:=JedMain.Cur_TH;
end;

Function TJEDApp.prGetCurSF:integer;
begin
 Result:=JedMain.Cur_SF;
end;

Function TJEDApp.prGetCurED:integer;
begin
 Result:=JedMain.Cur_ED;
end;

Function TJEDApp.prGetCurVX:integer;
begin
 Result:=JedMain.Cur_VX;
end;

Function TJEDApp.prGetCurFR:integer;
begin
 Result:=JedMain.Cur_FR;
end;

Procedure TJEDApp.SetCurTH(th:integer);
begin
 JedMain.SetCurTH(th);
end;

Function TJEDApp.GetCurLT:integer;
begin
 Result:=JedMain.Cur_LT;
end;

Procedure TJEDApp.SetCurLT(sc:integer);
begin
 JedMain.SetCurLT(sc);
end;

Procedure TJEDApp.GetCurSF(var sc,sf:integer);
begin
 sc:=JedMain.Cur_SC;
 sf:=JedMain.Cur_SF;
end;

Procedure TJEDApp.SetCurSF(sc,sf:integer);
begin
 JedMain.SetCurSF(sc,sf);
end;

Procedure TJEDApp.GetCurVX(var sc,vx:integer);
begin
 sc:=JedMain.Cur_SC;
 vx:=JedMain.Cur_VX;
end;

Procedure TJEDApp.SetCurVX(sc,vx:integer);
begin
 JedMain.SetCurVX(sc,vx);
end;

Procedure TJEDApp.GetCurED(var sc,sf,ed:integer);
begin
 sc:=JedMain.Cur_SC;
 sf:=JedMain.Cur_SF;
 ed:=JedMain.Cur_ED;
end;

Procedure TJEDApp.SetCurED(sc,sf,ed:integer);
begin
 JedMain.SetCurED(sc,sf,ed);
end;

Procedure TJEDApp.GetCurFR(var th,fr:integer);
begin
 th:=JedMain.Cur_TH;
 fr:=JedMain.Cur_FR;
end;

Procedure TJEDApp.SetCurFR(th,fr:integer);
begin
 JedMain.SetCurFR(th,fr);
end;


Procedure TJEDApp.NewLevel(mots:WordBool);
begin
 Lev.ClearOLEObjs;
 case mots of
  true: JedMain.New1.Click;
  false: JedMain.NewMotsProject1.Click;
 end;
end;

Procedure TJEDApp.OpenLevel(const name:string);
begin
 Lev.ClearOLEObjs;
 JedMain.OpenProject(name,op_open);
end;

Procedure TJEDApp.SaveJED(const name:string);
begin
 JedMain.SaveToFile(name, 'jed');
end;

Procedure TJEDApp.SaveJKL(const name:string);
begin
 JedMain.SaveJKLto(name);
end;

Function TJEDApp.GetGameDir:String;
begin
 result:=GlobalVars.GameDir;
end;

Function TJEDApp.GetProjectDir:String;
begin
 Result:=GlobalVars.ProjectDir;
end;

Function TJEDApp.GetJEDDir:String;
begin
 Result:=GlobalVars.BaseDir;
end;

Function TJEDApp.GetLevelFile:String;
begin
 Result:=JedMain.LevelFile;
end;

{Picking}

Procedure TJEDApp.SaveCurApp;
begin
 ForeWnd:=GetForeGroundWindow;
 SetForeGroundWindow(JedMain.Handle);
end;

Procedure TJEDApp.RestoreCurApp;
begin
 SetForeGroundWindow(ForeWnd);
end;

Function TJEDApp.PickThing(const curThing:String):String;
begin
 SaveCurApp;
 Result:=ResPicker.PickThing(CurThing);
 RestoreCurApp;
end;

Function TJEDApp.PickTemplate(const curtpl:string):string;
begin
 SaveCurApp;
 Result:=ResPicker.PickTemplate(CurTpl);
 RestoreCurApp;
end;

Function TJEDApp.PickCMP(const CurCMP:string):string;
begin
 SaveCurApp;
 Result:=ResPicker.PickCMP(CurCMP);
 RestoreCurApp;
end;

Function TJEDApp.PickWav(CurWav:string):string;
begin
 SaveCurApp;
 Result:=ResPicker.PickSecSound(CurWav);
 RestoreCurApp;
end;

Function TJEDApp.PickMAT(CurMAT:string):string;
begin
 SaveCurApp;
 Result:=ResPicker.PickMAT(CurMat);
 RestoreCurApp;
end;

Function TJEDApp.PickCOG(CurCog:string):string;
begin
 SaveCurApp;
 Result:=ResPicker.PickCOG(CurCOG);
 RestoreCurApp;
end;

Function TJEDApp.Pick3DO(const cur3do:String):String;
begin
 SaveCurApp;
 Result:=ResPicker.Pick3DO(Cur3do);
 RestoreCurApp;
end;

Function TJEDApp.PickAI(const CurAI:string):string;
begin
 SaveCurApp;
 Result:=ResPicker.PickAI(CurAI);
 RestoreCurApp;
end;

Function TJEDApp.PickKEY(const CurKEY:string):string;
begin
 SaveCurApp;
 Result:=ResPicker.PickKEY(CurKEY);
 RestoreCurApp;
end;

Function TJEDApp.PickSND(const CurSND:string):string;
begin
 SaveCurApp;
 Result:=ResPicker.PickSND(CurSND);
 RestoreCurApp;
end;

Function TJEDApp.PickPUP(const CurPUP:string):string;
begin
 SaveCurApp;
 Result:=ResPicker.PickPUP(CurPUP);
 RestoreCurApp;
end;

Function TJEDApp.PickSPR(const CurSPR:string):string;
begin
 SaveCurApp;
 Result:=ResPicker.PickSPR(CurSPR);
 RestoreCurApp;
end;

Function TJEDApp.PickPAR(const CurPAR:string):string;
begin
 SaveCurApp;
 Result:=ResPicker.PickPAR(CurPAR);
 RestoreCurApp;
end;

Function TJEDApp.PickPER(const CurPER:string):string;
begin
 SaveCurApp;
 Result:=ResPicker.PickPER(CurPER);
 RestoreCurApp;
end;

Function TJEDApp.EditSectorFlags(flags:LongInt):LongInt;
begin
 SaveCurApp;
 Result:=FlagEdit.EditSectorFlags(flags);
 RestoreCurApp;
end;

Function TJEDApp.EditSurfaceFlags(flags:LongInt):LongInt;
begin
 SaveCurApp;
 Result:=FlagEdit.EditSurfaceFlags(flags);
 RestoreCurApp;
end;

Function TJEDApp.EditAdjoinFlags(flags:LongInt):LongInt;
begin
 SaveCurApp;
 Result:=FlagEdit.EditAdjoinFlags(flags);
 RestoreCurApp;
end;

Function TJEDApp.EditThingFlags(flags:LongInt):LongInt;
begin
 SaveCurApp;
 Result:=FlagEdit.EditThingFlags(flags);
 RestoreCurApp;
end;

Function TJEDApp.EditFaceFlags(flags:LongInt):LongInt;
begin
 SaveCurApp;
 Result:=FlagEdit.EditFaceFlags(flags);
 RestoreCurApp;
end;

Function TJEDApp.EditLightFlags(flags:Longint):LongInt;
begin
 SaveCurApp;
 Result:=FlagEdit.EditLightFlags(flags);
 RestoreCurApp;
end;

Function TJEDApp.PickGeo(geo:integer):integer;
begin
 SaveCurApp;
 Result:=SCFieldPicker.PickGeo(geo);
 RestoreCurApp;
end;

Function TJEDApp.PickLightMode(lmode:integer):integer;
begin
 SaveCurApp;
 Result:=SCFieldPicker.PickLightMode(lmode);
 RestoreCurApp;
end;

Function TJEDApp.PickTEX(tex:integer):integer;
begin
 SaveCurApp;
 Result:=SCFieldPicker.PickTEX(tex);
 RestoreCurApp;
end;

Procedure TJEDApp.ReloadTemplates;
begin
 JedMain.LoadTemplates;
end;

Procedure TJEDApp.PanMessage(mtype:integer;const msg:string);
begin
 case mtype of
  0: misc_utils.PanMessage(mt_info,msg);
  1: misc_utils.PanMessage(mt_warning,msg);
  2: misc_utils.PanMessage(mt_error,msg);
 end;
end;

Procedure TJEDApp.SendKey(shift:integer;key:integer);
var st:TShiftState;
    akey:word;
begin
 st:=[];
 if (shift and 1<>0) then st:=st+[ssCtrl];
 if (shift and 2<>0) then st:=st+[ssShift];
 if (shift and 4<>0) then st:=st+[ssAlt];
 akey:=key;
 JedMain.FormKeyDown(JedMain,akey,st);
end;

Function TJEDApp.ExecuteMenu(const itemref:string):WordBool;
var nmi,mi:TMenuItem;
    cname:string;
    p,cpos:integer;

Function GetNextItem(var s:string;pos:integer):integer;
begin
 Result:=pos;
 While result<length(ItemRef) do
  if ItemRef[result]<>'\' then inc(result) else break;

 if result=length(itemRef) then  s:=Copy(itemref,pos,Length(ItemRef))
 else s:=Copy(itemref,pos,result-pos);

 inc(result);

end;

var i:integer;s:string;
begin
 Result:=false;
 mi:=nil;
 cpos:=getNextItem(cname,1);
 With Jedmain.Menu do
 for i:=0 to 6 do {Number of top menu items!}
 begin
  s:=Items[i].Caption;
  While pos('&',s)<>0 do Delete(s,pos('&',s),1);

  if CompareText(s,cname)=0 then
  begin
   mi:=Items[i];
   break;
  end;
 end;

 if mi=nil then exit;

 while cpos<length(itemRef) do
 begin
  cpos:=GetNextItem(cname,cpos);
  nmi:=nil;

  for i:=0 to mi.Count-1 do
  begin
   s:=mi.Items[i].Caption;
   if Pos(#9,s)<>0 then SetLength(s,Pos(#9,s)-1);

   While pos('&',s)<>0 do Delete(s,pos('&',s),1);

   if CompareText(s,cname)=0 then
   begin
    nmi:=mi.Items[i];
    break;
   end;
  end;

  if nmi=nil then exit;
  mi:=nmi;

 end;

 mi.Click;
 Result:=true;

end;

Function TJEDApp.GetGridX:double;
begin
 result:=Jedmain.Renderer.GridX;
end;

Function TJEDApp.GetGridY:double;
begin
 result:=Jedmain.Renderer.GridY;
end;

Function TJEDApp.GetGridZ:double;
begin
 result:=Jedmain.Renderer.GridZ;
end;

Function TJEDApp.GetCamX:double;
begin
 result:=Jedmain.Renderer.CamX;
end;

Function TJEDApp.GetCamY:double;
begin
 result:=Jedmain.Renderer.CamY;
end;

Function TJEDApp.GetCamZ:double;
begin
 result:=Jedmain.Renderer.CamZ;
end;


Procedure TJEDApp.GetCamXYZ(var x,y,z:double);
begin
 With JEDMain.Renderer do
 begin
  x:=CamX;
  y:=CamY;
  z:=CamZ;
 end;
end;

Procedure TJEDApp.GetGridXYZ(var x,y,z:double);
begin
 With JEDMain.Renderer do
 begin
  x:=GridX;
  y:=GridY;
  z:=GridZ;
 end;
end;


Procedure TJEDApp.SetCamXYZ(x,y,z:double);
begin
 With Jedmain do
 begin
  Renderer.CamX:=x;
  Renderer.CamY:=y;
  Renderer.CamZ:=z;
  Invalidate;
 end;
end;

Procedure TJEDApp.SetGridXYZ(x,y,z:double);
begin
 With Jedmain do
 begin
  Renderer.GridX:=x;
  Renderer.GridY:=y;
  Renderer.GridZ:=z;
  Invalidate;
 end;
end;

Procedure TJEDApp.GetGridAxes(var xx,xy,xz,yx,yy,yz,zx,zy,zz:double);
begin
 With JedMain.Renderer do
 begin
  xx:=gxnormal.dx;
  xy:=gxnormal.dy;
  xz:=gxnormal.dz;
  yx:=gynormal.dx;
  yy:=gynormal.dy;
  yz:=gynormal.dz;
  zx:=gnormal.dx;
  zy:=gnormal.dy;
  zz:=gnormal.dz;
 end;
end;

Function TJEDApp.GetGridAxis(naxis,ncoord:integer):double;
begin
 Result:=0;
 With JedMain.Renderer do
 case naxis of
  1: case ncoord of
      1: Result:=gxnormal.dx;
      2: Result:=gxnormal.dy;
      3: Result:=gxnormal.dz;
     end;
  2: case ncoord of
      1: Result:=gynormal.dx;
      2: Result:=gynormal.dy;
      3: Result:=gynormal.dz;
     end;
  3: case ncoord of
      1: Result:=gnormal.dx;
      2: Result:=gnormal.dy;
      3: Result:=gnormal.dz;
     end;
  end;
end;

Procedure TJEDApp.SetGridAxes(xx,xy,xz,yx,yy,yz:double);
var z:Tvector;
begin
 VMult(xx,xy,xz,yx,yy,yz,
       z.dx,z.dy,z.dz);
 if VLen(z)=0 then exit;
 With JedMain.Renderer do
 begin
  SetZ(z.dx,z.dy,z.dz);
  SetX(xx,xy,xz);
 end;
  JedMain.Invalidate;
end;


Procedure TJEDApp.GetCamAxes(var xx,xy,xz,yx,yy,yz,zx,zy,zz:double);
begin
 With JedMain.Renderer do
 begin
  xx:=xv.dx;
  xy:=xv.dy;
  xz:=xv.dz;
  yx:=yv.dx;
  yy:=yv.dy;
  yz:=yv.dz;
  zx:=zv.dx;
  zy:=zv.dy;
  zz:=zv.dz;
 end;
end;

Function TJEDApp.GetCamAxis(naxis,ncoord:integer):double;
begin
 Result:=0;
 With JedMain.Renderer do
 case naxis of
  1: case ncoord of
      1: Result:=xv.dx;
      2: Result:=xv.dy;
      3: Result:=xv.dz;
     end;
  2: case ncoord of
      1: Result:=yv.dx;
      2: Result:=yv.dy;
      3: Result:=yv.dz;
     end;
  3: case ncoord of
      1: Result:=zv.dx;
      2: Result:=zv.dy;
      3: Result:=zv.dz;
     end;
  end;
end;

Procedure TJEDApp.SetCamAxes(xx,xy,xz,yx,yy,yz:double);
var z:Tvector;
begin
 VMult(xx,xy,xz,yx,yy,yz,
       z.dx,z.dy,z.dz);
 if VLen(z)=0 then exit;
 With JedMain.Renderer do
 begin
  SetZ(z.dx,z.dy,z.dz);
  SetX(xx,xy,xz);
 end;
  JedMain.GetPYR;
  JedMain.Invalidate;
end;


Function TJEDApp.getIsPreviewActive:WordBool;
begin
 Result:=Preview3D.IsActive;
end;

Procedure TJEDApp.SetPreviewCamXYZ(x,y,z:double);
var ax,ay,az,ap,aya:double;
begin
 Preview3D.GetCam(ax,ay,az,ap,aya);
 Preview3D.SetCam(x,y,z,ap,aya);
end;

Procedure TJEDApp.GetPreviewCamXYZ(var x,y,z:double);
var ap,aya:double;
begin
 Preview3D.GetCam(x,y,z,ap,aya);
end;

Procedure TJEDApp.SetPreviewCamPY(pch,yaw:double);
var ax,ay,az,ap,aya:double;
begin
 Preview3D.GetCam(ax,ay,az,ap,aya);
 Preview3D.SetCam(ax,ay,az,pch,yaw);
end;

Procedure TJEDApp.GetPreviewCamPY(var pch,yaw:double);
var ax,ay,az:double;
begin
 Preview3D.GetCam(ax,ay,az,pch,yaw);
end;

Function TJEDApp.GetPreviewCamParam(what:integer):double;
var ax,ay,az,ap,aya:double;
begin
 Preview3D.GetCam(ax,ay,az,ap,aya);
 case what of
  1: result:=ax;
  2: result:=ay;
  3: result:=az;
  4: result:=ap;
  5: result:=aya;
 else result:=0;
 end;
end;


Function TJEDApp.NMultiSelected(what:integer):integer;
begin
 With JedMain do
 case what of
  MM_SC: result:=scsel.count;
  MM_SF: result:=sfsel.count;
  MM_ED: result:=edsel.count;
  MM_VX: result:=vxsel.count;
  MM_TH: result:=thsel.count;
  MM_FR: result:=frsel.count;
  MM_LT: result:=ltsel.count;
 end;
end;

Procedure TJEDApp.ClearMultiselection(what:integer);
begin
 With JedMain do
 case what of
  MM_SC: scsel.clear;
  MM_SF: sfsel.clear;
  MM_ED: edsel.clear;
  MM_VX: vxsel.clear;
  MM_TH: thsel.clear;
  MM_FR: frsel.clear;
  MM_LT: ltsel.clear;
 end;
 JedMain.Invalidate;
end;


Procedure TJEDApp.RemoveFromMultiselection(what,n:integer);
begin
 With JedMain do
 case what of
  MM_SC: scsel.DeleteN(n);
  MM_SF: sfsel.DeleteN(n);
  MM_ED: edsel.DeleteN(n);
  MM_VX: vxsel.DeleteN(n);
  MM_TH: thsel.DeleteN(n);
  MM_FR: frsel.DeleteN(n);
  MM_LT: ltsel.DeleteN(n);
 end;
 JedMain.Invalidate;
end;

Function TJEDApp.GetSelectedSC(n:integer):integer;
begin
 Result:=JedMain.scsel.GetSC(n);
end;

Function TJEDApp.GetSelectedTH(n:integer):integer;
begin
 Result:=JedMain.thsel.GetTH(n);
end;

Function TJEDApp.GetSelectedLT(n:integer):integer;
begin
 Result:=JedMain.ltsel.GetLT(n);
end;

procedure TJEDApp.GetSelectedSF(n:integer;var sc,sf:integer);
begin
 JedMain.sfsel.GetSCSF(n,sc,sf);
end;

procedure TJEDApp.GetSelectedED(n:integer;var sc,sf,ed:integer);
begin
 JedMain.edsel.GetSCSFED(n,sc,sf,ed);
end;

procedure TJEDApp.GetSelectedVX(n:integer;var sc,vx:integer);
begin
 JedMain.vxsel.GetSCVX(n,sc,vx);
end;

procedure TJEDApp.GetSelectedFR(n:integer;var th,fr:integer);
begin
 JedMain.frsel.GetTHFR(n,th,fr);
end;

Function TJEDApp.GetSelectedSFop(n:integer;what:integer):integer;
var sc,sf:integer;
begin
 JedMain.sfsel.GetSCSF(n,sc,sf);
 case what of
  1: result:=sc;
  2: result:=sf;
 else result:=0;
 end;
end;

Function TJEDApp.GetSelectedEDop(n:integer;what:integer):integer;
var sc,sf,ed:integer;
begin
 JedMain.edsel.GetSCSFED(n,sc,sf,ed);
 case what of
  1: result:=sc;
  2: result:=sf;
  3: result:=ed;
 else result:=0;
 end;
end;

Function TJEDApp.GetSelectedVXop(n:integer;what:integer):integer;
var sc,vx:integer;
begin
 JedMain.vxsel.GetSCVX(n,sc,vx);
 case what of
  1: result:=sc;
  2: result:=vx;
 else result:=0;
 end;
end;

Function TJEDApp.GetSelectedFRop(n:integer;what:integer):integer;
var th,fr:integer;
begin
 JedMain.frsel.GetTHFR(n,th,fr);
 case what of
  1: result:=th;
  2: result:=fr;
 else result:=0;
 end;
end;

Function TJEDApp.SelectSC(sc:integer):integer;
begin
 Result:=Nmask and JedMain.scsel.AddSC(sc);
end;

Function TJEDApp.SelectSF(sc,sf:integer):integer;
begin
 Result:=Nmask and JedMain.sfsel.AddSF(sc,sf);
end;

Function TJEDApp.SelectED(sc,sf,ed:integer):integer;
begin
 Result:=Nmask and JedMain.edsel.AddED(sc,sf,ed);
end;

Function TJEDApp.SelectVX(sc,vx:integer):integer;
begin
 Result:=Nmask and JedMain.vxsel.AddVX(sc,vx);
end;

Function TJEDApp.SelectTH(th:integer):integer;
begin
 Result:=Nmask and JedMain.thsel.AddTH(th);
end;

Function TJEDApp.SelectFR(th,fr:integer):integer;
begin
 Result:=Nmask and JedMain.frsel.AddFR(th,fr);
end;

Function TJEDApp.SelectLT(lt:integer):integer;
begin
 Result:=Nmask and JedMain.ltsel.AddLT(lt);
end;

Function TJEDApp.FindSelectedSC(sc:integer):integer;
begin
 Result:=JedMain.scsel.FindSC(sc);
end;

Function TJEDApp.FindSelectedSF(sc,sf:integer):integer;
begin
 Result:=JedMain.sfsel.FindSF(sc,sf);
end;

Function TJEDApp.FindSelectedED(sc,sf,ed:integer):integer;
begin
 Result:=JedMain.edsel.FindED(sc,sf,ed);
end;

Function TJEDApp.FindSelectedVX(sc,vx:integer):integer;
begin
 Result:=JedMain.vxsel.FindVX(sc,vx);
end;

Function TJEDApp.FindSelectedTH(th:integer):integer;
begin
 Result:=JedMain.thsel.FindTH(th);
end;

Function TJEDApp.FindSelectedFR(th,fr:integer):integer;
begin
 Result:=JedMain.frsel.FindFR(th,fr);
end;

Function TJEDApp.FindSelectedLT(lt:integer):integer;
begin
 Result:=JedMain.ltsel.FindLT(lt);
end;

{0.85}
Procedure TJEDApp.StartUndo(const name:string);
begin
 StartUndoRec(name);
end;

Procedure TJEDApp.SaveUndoForThing(n:integer;change:integer);
begin
 case change of
  0: SaveThingUndo(J_level.Level.Things[n],ch_changed);
  1: SaveThingUndo(J_level.Level.Things[n],ch_added);
  2: SaveThingUndo(J_level.Level.Things[n],ch_deleted);
 end;
end;

Procedure TJEDApp.SaveUndoForLight(n:integer;change:integer);
begin
 case change of
  0: SaveLightUndo(J_level.Level.Lights[n],ch_changed);
  1: SaveLightUndo(J_level.Level.Lights[n],ch_added);
  2: SaveLightUndo(J_level.Level.Lights[n],ch_deleted);
 end;
end;

Procedure TJEDApp.SaveUndoForSector(n:integer;change:integer;whatpart:integer);
var how:integer;
begin
how:=12;
if whatPart=1 then how:=8;
if whatpart=2 then how:=4;
if whatpart=3 then how:=12;

 case change of
  0: SaveSecUndo(J_level.Level.Sectors[n],ch_changed,how);
  1: SaveSecUndo(J_level.Level.Sectors[n],ch_added,how);
  2: SaveSecUndo(J_level.Level.Sectors[n],ch_deleted,how);
 end;
end;

Procedure TJEDApp.ClearUndoBuffer;
begin
 u_undo.ClearUndoBuffer;
end;

Procedure TJEDApp.ApplyUndo;
begin
 u_undo.ApplyUndo;
end;

Function TJEDApp.GetJEDSetting(const name:string):variant;
begin
 Result:=GetSetting(name);
end;

Function TJEDApp.IsLayerVisible(n:integer):WordBool;
begin
 Result:=ToolBar.IsLayerVisible(n);
end;

procedure TJEDApp.CheckConsistencyErrors;
begin
 Consistency.Check;
 Consistency.Hide;
end;

procedure TJEDApp.CheckResources;
begin
 Consistency.CheckResources;
 Consistency.Hide;
end;

Function TJEDApp.NConsistencyErrors:integer;
begin
 result:=Consistency.NErrors;
end;

Function TJEDApp.GetConsErrorString(n:integer):String;
begin
 result:=Consistency.ErrorText(n);
end;

Function TJEDApp.GetConsErrorType(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 obj:=Consistency.ErrorObject(n);
 if obj=nil then exit;
 result:=integer(obj.itype);
end;

Function TJEDApp.GetConsErrorSector(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 obj:=Consistency.ErrorObject(n);
 if obj.itype=itSector then Result:=TJKSector(obj.item).num
 else if obj.itype=itSurface then Result:=TJKSurface(obj.item).sector.num;
end;

Function TJEDApp.GetConsErrorSurface(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 obj:=Consistency.ErrorObject(n);
 if obj.itype=itSurface then Result:=TJKSurface(obj.item).num;
end;

Function TJEDApp.GetConsErrorThing(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 obj:=Consistency.ErrorObject(n);
 if obj.itype=itThing then Result:=TJKThing(obj.item).num;
end;

Function TJEDApp.GetConsErrorCog(n:integer):integer;
var obj:TConsistencyError;
begin
 result:=-1;
 obj:=Consistency.ErrorObject(n);
 if obj.itype=itCog then Result:=J_Level.Level.COGS.IndexOf(obj.item);
end;

Function TJEDApp.GetCurEX:integer;
begin
 result:=JedMain.Cur_EX;
end;

Procedure TJEDApp.SetCurEX(ex:integer);
begin
 With JedMain do
 begin
  if (ex<0) or (ex>=ExtraObjs.Count) then Cur_Ex:=-1
  else Cur_EX:=ex;
 end;
end;

Procedure TJEDApp.ClearExtraObjects;
begin
 JedMain.ClearExtraObjs;
end;

Function TJEDApp.AddExtraVertex(x,y,z:double):integer;
begin
 Result:=JedMain.AddExtraVertex(x,y,z,'');
end;

Function TJEDApp.AddExtraLine(x1,y1,z1,x2,y2,z2:double):Integer;
begin
 Result:=JedMain.AddExtraLine(x1,y1,z1,x2,y2,z2,'');
end;

Procedure TJEDApp.DeleteExtraObject(n:integer);
begin
 JedMain.DeleteExtraObj(n);
end;

procedure RegisterJEDAuto;
const
  AutoClassInfo: TAutoClassInfo = (
    AutoClass: TJEDApp;
    ProgID: 'SED.App';
    ClassID: '{6E6FA130-F620-11D1-988C-000000000000}';
    Description: 'SED';
    Instancing: acMultiInstance);
begin
  Automation.RegisterClass(AutoClassInfo);
end;

initialization
  RegisterJEDAuto;
Finalization
begin
 Automation.UpdateRegistry(false);
end;

end.

