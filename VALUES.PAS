unit values;

interface
uses Classes, SysUtils;

type
v_type=(vt_str,vt_int,vt_float,vt_flag,vt_ptr,vt_vect,vt_frame,vt_unk);
tcog_type=(ct_unk,ct_ai,ct_cog,ct_key,ct_mat,ct_msg,
           ct_3do,ct_sec,ct_wav,ct_srf,ct_tpl,ct_thg,
           ct_int,ct_float,ct_vect);
TAdjType=(at_unk,at_mat,at_cog,at_snd,at_pup,at_spr,at_par,at_3do,at_ai,at_tpl,at_frame);
{ai
cog
flex
float
int
keyframe
material
message
model
sector
sound
surface
template
thing
vector}


TValue=class
 Name:string;
 vtype:v_type;
 int:longint;
 float:double;
 s:string;
 Function AsString:String;
 Function Val(const s:string):boolean;
 Procedure Assign(v:TValue);
end;

TValues=class(TList)
 Function GetItem(n:integer):TValue;
 Procedure SetItem(n:integer;v:TValue);
 Function IndexOfName(const name:string):integer;
Property Items[n:integer]:TValue read GetItem write SetItem; default;
end;

TCOGValue=class(TValue)
  cog_type:TCog_Type;
  local:boolean;
  desc:String;
  mask:longint;
  obj:TObject;
  Function AsString:String;
  Function AsJedString:String;
  Procedure Assign(cv:TCogValue);
  Function Val(const s:string):boolean;
  Function JedVal(const s:string):boolean;
  Procedure Resolve;
end;

TCOGValues=class(TValues)
 Function GetItem(n:integer):TCOGValue;
 Procedure SetItem(n:integer;v:TCOGValue);
Property Items[n:integer]:TCOGValue read GetItem write SetItem; default;
end;

TTplValue=class(tvalue)
 atype:TAdjType;
 Procedure Assign(v:TTplValue);
 Procedure GetFrame(var x,y,z,pch,yaw,rol:double);
 Procedure SetFrame(x,y,z,pch,yaw,rol:double);
end;

TTPLValues=class(TValues)
 Function GetItem(n:integer):TTPLValue;
 Procedure SetItem(n:integer;v:TTPLValue);
Property Items[n:integer]:TTPLValue read GetItem write SetItem; default;
end;


Function GetCogType(const stype:string):TCog_type;
Function GetCogVType(const stype:string):v_type;
Function GetVTypeFromCOGType(vtype:TCOG_Type):v_type;

Function GetTplVType(const name:string):v_type;
Function GetTplType(const name:string):TAdjtype;


Procedure S2TPLVal(const s:string;v:TTPLValue);
Procedure S2CogVal(const s:string;v:TCOGValue);

Function GetCOGVal(const st:string;v:TCogValue):boolean;
Function GetJedVal(const st:string;v:TCogValue):boolean;

Function GetCogTypeName(ct:TCOG_type):String;

Procedure InitValues;

implementation
uses Misc_utils, J_Level;

var
 CogTypes,CogVtypes,tplNames,TplVtypes:TStringList;


Function GetCogTypeName(ct:TCOG_type):String;
var n:integer;
begin
 n:=CogTypes.IndexOfObject(TObject(ct));
 if n=-1 then result:='unknown'
 else Result:=CogTypes[n];
end;

Function GetCogType(const stype:string):TCog_type;
var n:integer;
begin
 n:=CogTypes.IndexOf(stype);
 if n=-1 then Result:=ct_unk
 else Result:=TCog_type(CogTypes.Objects[n]);
end;

Function GetVTypeFromCOGType(vtype:TCOG_Type):v_type;
begin
 case vtype of
  ct_unk: result:=vt_unk;
  ct_ai,ct_cog,ct_key,ct_mat,ct_msg,
  ct_3do,ct_wav,ct_tpl: result:=vt_str;
  ct_sec,ct_srf,ct_thg: result:=vt_ptr;
  ct_int: result:=vt_int;
  ct_float: result:=vt_float;
  ct_vect: result:=vt_vect;
 else result:=vt_unk;
 end;
end;

Function GetCogVType(const stype:string):v_type;
var n:integer;
begin
 n:=CogVTypes.IndexOf(stype);
 if n=-1 then Result:=vt_unk
 else Result:=v_type(CogVTypes.Objects[n]);
end;

Function GetTplVType(const name:string):v_type;
var n:integer;
begin
 n:=TplVTypes.IndexOf(name);
 if n=-1 then Result:=vt_str
 else Result:=v_type(TplVTypes.Objects[n]);
end;

Function GetTplType(const name:string):TAdjtype;
var n:integer;
begin
 n:=tplNames.IndexOf(name);
 if n=-1 then Result:=at_unk
 else Result:=TAdjtype(TplNames.Objects[n]);
end;

Function GetCOGVal(const st:string;v:TCogValue):boolean;
var w:string;
    p,pe:integer;
begin
 Result:=false;
 p:=GetWord(st,1,w);
 if w='' then exit;
 v.vtype:=GetCogVType(w);
 v.cog_type:=GetCogType(w);
 p:=GetWord(st,p,w);

 pe:=Pos('=',w);
 if pe=0 then v.name:=w
 else begin v.name:=Copy(w,1,pe-1); v.Val(Copy(w,pe+1,length(w))); end;

 While p<Length(st) do
 begin
  p:=GetWord(st,p,w);
  if w='' then continue;
  if CompareText(w,'local')=0 then v.local:=true;
 end;

end;

Function GetJedVal(const st:string;v:TCogValue):boolean;
var pe,ps:integer;
    w:string;
begin
 ps:=pos(':',st);
 pe:=Pos('=',st);
 v.name:=Copy(st,1,ps-1);
 w:=Copy(st,ps+1,pe-ps-1);
 v.vtype:=GetCogVType(w);
 v.cog_type:=GetCogType(w);
 v.Val(Copy(st,pe+1,length(st)));
end;

Function TCOGValue.AsJedString:String;
begin
 case cog_type of
  ct_srf: begin
           if obj=nil then begin Result:='-1'; exit; end;
           With TJKSUrface(obj) do
            Result:=Format('%d %d',[Sector.num,num]);
            exit;
          end;
 end;
 Result:=AsString;
end;

Function TCOGValue.AsString:String;
begin
 case cog_type of
   ct_srf,ct_sec,ct_thg:
    if obj=nil then
    begin
     Result:='-1';
     exit;
    end;
 end;

 case cog_type of
  ct_sec: Result:=IntToStr(TJKSector(obj).num);
  ct_srf: With TJKSurface(obj) do
           Result:=IntToStr(Sector.Level.GetGlobalSFN(Sector.Num,Sector.Surfaces.IndexOf(obj)));
  ct_thg: Result:=IntToStr(TJKThing(obj).num);
 else Result:=Inherited AsString;
 end;
end;

Procedure TCOGValue.Assign(cv:TCogValue);
begin
 Name:=cv.Name;
 cog_type:=cv.cog_type;
 vtype:=cv.vtype;
 desc:=cv.desc;
 local:=cv.local;
end;

Procedure TCOGValue.Resolve;
begin
case cog_type of
 ct_sec: obj:=Level.GetSectorN(int);
 ct_srf: obj:=Level.GetSurfaceN(int);
 ct_thg: obj:=Level.GetThingN(int);
 else exit;
end;
if (obj=nil) and (int<>-1) then
 PanMessageFmt(mt_warning,'Cog parameter resolution failed: no %s num: %d',
  [GetCogTypeName(cog_type),int]);
end;

Function TCOGValue.Val(const s:string):boolean;
var d:longint;
begin
 Case cog_type of
  ct_sec,ct_srf,ct_thg: Result:=ValDword(s,Int);
 else Result:=Inherited Val(s);
 end;
end;

Function TCOGValue.JedVal(const s:string):boolean;
var i,p:integer;w:string;
begin
 Result:=false;
 case cog_type of
  ct_srf: begin
           p:=GetWord(s,1,w);
           Result:=ValInt(w,i);
           if not result then exit;
           if i<0 then begin obj:=nil; exit; end;
           GetWord(s,p,w);
           Result:=ValInt(w,p);
           if not result then exit;
           try
            obj:=Level.Sectors[i].Surfaces[p];
           except
            On Exception do Result:=false;
           end;
          end;
  ct_sec,ct_thg:
  begin
   Result:=ValInt(s,I);
   if not Result then exit;
   if i<0 then begin obj:=nil; exit; end;
   case cog_type of
    ct_sec: begin
             if i<Level.Sectors.Count then obj:=Level.Sectors[i]
             else Result:=false;
            end;
    ct_thg: begin
             if i<Level.Things.Count then obj:=Level.Things[i]
             else Result:=false;
            end;
   end;
  end;
 else Result:=Val(s);
 end;
end;

Procedure TTplValue.Assign(v:TTplValue);
begin
 Inherited Assign(v);
 atype:=v.atype;
end;

Procedure TTplValue.GetFrame(var x,y,z,pch,yaw,rol:double);
var vs:string;
    i:integer;
begin
 x:=0; y:=0; z:=0; pch:=0; yaw:=0; rol:=0;
 vs:=s;
 for i:=1 to length(vs) do if vs[i] in ['/',':','(',')'] then vs[i]:=' ';
 SScanf(vs,'%f %f %f %f %f %f',[@x,@y,@z,@pch,@yaw,@rol]);
end;

Procedure TTplValue.SetFrame(x,y,z,pch,yaw,rol:double);
begin
 if CompareText(name,'frame')<>0 then exit;
 s:=Sprintf('(%.6f/%.6f/%.6f:%.6f/%.6f/%.6f)',
                    [x,y,z,pch,yaw,rol]);
end;

Function TValue.AsString:String;
begin
 case vtype of
  vt_str,vt_unk:result:=s;
  vt_vect,vt_frame: Result:=s;
  vt_int,vt_ptr:Result:=IntToStr(int);
  vt_float: Result:=FloatToStr(float);
  vt_flag: Result:=Format('0x%x',[int]);
 else Result:='';
 end;
end;

Function TValue.Val(const s:string):boolean;
begin
 Result:=true;
 case vtype of
  vt_str,vt_unk:self.s:=s;
  vt_vect,vt_frame: self.s:=s;
  vt_int,vt_ptr:Result:=ValDword(s,int);
  vt_float: Result:=ValDouble(s,float);
  vt_flag: Result:=ValHex(s,int);
 else Result:=false;
 end;
end;

Procedure TValue.Assign;
begin
 Name:=v.Name;
 vtype:=v.vtype;
 int:=v.int;
 float:=v.float;
 s:=v.s;
end;


Function TValues.GetItem(n:integer):TValue;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Value Index is out of bounds: %d',[n]);
 Result:=TValue(List[n]);
end;

Procedure TValues.SetItem(n:integer;v:TValue);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Value Index is out of bounds: %d',[n]);
 List[n]:=v;
end;

Function TValues.IndexOfName(const name:string):integer;
var i:integer;
begin
 Result:=-1;
 for i:=0 to count-1 do
 begin
  if compareText(name,Items[i].Name)<>0 then continue;
  result:=i;
  break;
 end;
end;


Function TCOGValues.GetItem(n:integer):TCOGValue;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Value Index is out of bounds: %d',[n]);
 Result:=TCOGValue(List[n]);
end;

Procedure TCOGValues.SetItem(n:integer;v:TCOGValue);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Value Index is out of bounds: %d',[n]);
 List[n]:=v;
end;


Function TTPLValues.GetItem(n:integer):TTPLValue;
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Value Index is out of bounds: %d',[n]);
 Result:=TTPLValue(List[n]);
end;

Procedure TTPLValues.SetItem(n:integer;v:TTPLValue);
begin
 if (n<0) or (n>=count) then raise EListError.CreateFmt('Value Index is out of bounds: %d',[n]);
 List[n]:=v;
end;


Procedure S2TPLVal(const s:string;v:TTPLValue);
var p:integer;
begin
 p:=Pos('=',s);
 v.Name:=Copy(s,1,p-1);
 v.vtype:=GetTPLVtype(v.name);
 v.atype:=GetTPLtype(v.name);
 v.s:=Copy(s,p+1,length(s));
end;

Procedure S2CogVal(const s:string;v:TCOGValue);
var p:integer;
begin
 p:=Pos('=',s);
 v.Name:=Copy(s,1,p-1);
 v.vtype:=vt_str;
 v.s:=Copy(s,p+1,length(s));
end;

Procedure InitValues;
begin
 CogTypes:=TStringList.Create;
 CogTypes.Sorted:=true;
 With CogTypes Do
 begin
  AddObject('ai',TObject(ct_ai));
  AddObject('cog',TObject(ct_cog));
  AddObject('keyframe',TObject(ct_key));
  AddObject('material',TObject(ct_mat));
  AddObject('message',TObject(ct_msg));
  AddObject('model',TObject(ct_3do));
  AddObject('sector',TObject(ct_sec));
  AddObject('sound',TObject(ct_wav));
  AddObject('surface',TObject(ct_srf));
  AddObject('template',TObject(ct_tpl));
  AddObject('thing',TObject(ct_thg));
  AddObject('int',Tobject(ct_int));
  AddObject('float',Tobject(ct_float));
  AddObject('flex',Tobject(ct_float));
  AddObject('vector',Tobject(ct_vect));
 end;

 CogVTypes:=TStringList.Create;
 CogVTypes.Sorted:=true;
 With CogVTypes do
 begin
  AddObject('flex',TObject(vt_float));
  AddObject('float',TObject(vt_float));
  AddObject('vector',TObject(vt_vect));
  AddObject('int',TObject(vt_int));

  AddObject('ai',TObject(vt_str));
  AddObject('cog',TObject(vt_int));
  AddObject('keyframe',TObject(vt_str));
  AddObject('material',TObject(vt_str));
  AddObject('message',TObject(vt_str));
  AddObject('model',TObject(vt_str));
  AddObject('sector',TObject(vt_ptr));
  AddObject('sound',TObject(vt_str));
  AddObject('surface',TObject(vt_ptr));
  AddObject('template',TObject(vt_str));
  AddObject('thing',TObject(vt_ptr));
 end;

 tplNames:=TStringList.Create;
 tplNames.Sorted:=true;
 With tplNames do
 begin
 AddObject('material',TObject(at_mat));
 AddObject('cog',TObject(at_cog));
 AddObject('soundclass',TObject(at_snd));
 AddObject('puppet',TObject(at_pup));
 AddObject('sprite',TObject(at_spr));
 AddObject('particle',TObject(at_par));
 AddObject('model3d',TObject(at_3do));
 AddObject('aiclass',TObject(at_ai));

 AddObject('creatething',TObject(at_tpl));
 AddObject('explode',TObject(at_tpl));
 AddObject('fleshhit',TObject(at_tpl));
 AddObject('weapon',TObject(at_tpl));
 AddObject('weapon2',TObject(at_tpl));
 AddObject('debris',TObject(at_tpl));
 AddObject('trailthing',TObject(at_tpl));
 AddObject('frame',TObject(at_frame));
 end;

 TplVtypes:=TStringList.Create;
 TplVtypes.Sorted:=true;
 With TplVtypes do
 begin
  AddObject('frame',TObject(vt_frame));
 end;
end;

Initialization

Finalization
begin
 CogTypes.Free;
 CogVTypes.Free;
 TplNames.Free;
end;

end.
