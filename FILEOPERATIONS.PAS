unit FileOperations;

{This unit contains the service functions and
 procedures for file and directory operations.}

interface
uses Windows, files, Classes, ComCTRLS, StdCtrls, SysUtils,
{ProgressDialog, }Forms, Graphics,ShlObj, GlobalVars, COntainers;

Const
{OpenFileWrite flags}
     fm_Create=1; {Create new file}
     fm_LetReWrite=2;{Let rewrite file
                    if exists - OpenFileWrite}
     fm_AskUser=4; {Ask user if something}
     fm_CreateAskRewrite=fm_Create+fm_LetRewrite+fm_AskUser;
{OpenFileRead&Write flags}
     fm_Share=8;   {Let share file}
     fm_Buffered=16;


Type

TWildCardMask=class
private
 masks:TStringList;
 Procedure SetMask(s:string);
 Function GetMask:String;
Public
 Property mask:String read GetMask Write SetMask;
 Procedure AddTrailingAsterisks;
 Function Match(s:String):boolean;
 Destructor Destroy;override;
end;


TMaskedDirectoryControl=class
 Dir:TContainerFile;
 LBDir:TListBox;
 LVDir:TListView;
 Mask:String;
 control_type:(LBox,LView);
 LastViewStyle:TViewStyle;
 Constructor CreateFromLB(L:TlistBox); {ListBox}
 Constructor CreateFromLV(L:TlistView); {ListView}
 Procedure SetDir(D:TContainerFile);
 Procedure SetMask(mask:string);
Private
 Procedure AddFile(s:string;fi:TFileInfo);
 Procedure ClearControl;
 Procedure BeginUpdate;
 Procedure EndUpdate;
end;

TFileTStream=class(TStream)
 f:TFile;
 Constructor CreateFromTFile(af:TFile);
 function Read(var Buffer; Count: Longint): Longint; override;
 function Write(const Buffer; Count: Longint): Longint; override;
 function Seek(Offset: Longint; Origin: Word): Longint; override;
end;


Function OpenFileRead(const path:TFileName;mode:word):TFile;
Function OpenFileWrite(const path:TFileName;mode:word):TFile;
Function IsContainer(const path:TFileName):boolean;
Function IsInContainer(const path:TFileName):Boolean;
Function OpenContainer(const path:TFileName):TContainerFile;
Function OpenGameContainer(const path:TFileName):TContainerFile;
Function OpenGameFile(const name:TFileName):TFile;
Function FindProjDirFile(const name:string):string;
{opens it just once and keeps it open then only returns references on it.
 Used for game data files}

Function ExtractExt(path:String):String;
Function ExtractPath(path:String):String;
Function ExtractName(path:String):String;
Procedure CopyFileData(Ffrom,Fto:TFile;size:longint);
Function CopyAllFile(const Fname,ToName:String):boolean;
Function BackupFile(const Name:String):String;
Procedure ListDirMask(const path,mask:String;sl:TStringList);
Function ChangeExt(path:String; const newExt:String):String;

Function ConcPath(const path1,path2:string):string;

Function GetCurDir:string;
Procedure SetCurDir(const dir:string);

implementation
uses Misc_utils;

var
 CopyBuf:array[0..$8000-1] of byte;
 Containers:TStringList;

Function BackupFile(Const Name:String):String;
var cext:String;
begin
 if not FileExists(name) then exit;
 cext:=ExtractExt(Name);
 Insert('~',cext,2);
 if length(cext)>4 then setLength(cext,4);
 Result:=ChangeFileExt(Name,cext);
 if FileExists(Result) then DeleteFile(Result);
 RenameFile(Name,Result);
end;

Procedure ListDirMask(const path,mask:String;sl:TStringList);
var sr:TSearchRec;
    Res:Integer;
    CurMask:array[0..128] of char;
    P,PM:Pchar;
begin
 sl.Clear;
 PM:=PChar(Mask);
Repeat
 p:=StrScan(PM,';');
 if p=nil then p:=StrEnd(PM);
 StrLCopy(CurMask,PM,P-PM);

 Res:=FindFirst(path+curMask,faAnyFile,sr);
 While Res=0 do
 begin
  if (Sr.Attr and (faVolumeID+faDirectory))=0 then sl.Add(sr.Name);
  Res:=FindNext(sr);
 end;
 FindClose(sr);
  if P^=#0 then break else PM:=P+1;
Until False;
end;

Function CopyAllFile(const Fname,ToName:String):boolean;
var f,f1:TFile;
begin
 Result:=true;
 f:=OpenFileRead(Fname,0);
 f1:=OpenFileWrite(ToName,0);
 CopyFileData(f,f1,f.Fsize);
 F.Fclose;
 f1.Fclose;
end;

Procedure CopyFileData(Ffrom,Fto:TFile;size:longint);
begin
While size>sizeof(CopyBuf) do
begin
 Ffrom.Fread(CopyBuf,sizeof(CopyBuf));
 Fto.FWrite(CopyBuf,sizeof(CopyBuf));
 dec(size,sizeof(CopyBuf));
end;
 Ffrom.Fread(CopyBuf,size);
 Fto.FWrite(CopyBuf,size);
end;

Function GetQuote(ps,quote:pchar):pchar;
var p,p1:pchar;
begin
 if ps^ in ['?','*'] then
  begin
   GetQuote:=ps+1;
   quote^:=ps^;
   (quote+1)^:=#0;
   exit;
  end;
 p:=StrScan(ps,'?'); if p=nil then p:=StrEnd(ps);
 p1:=StrScan(ps,'*'); if p1=nil then p1:=StrEnd(ps);
 if p>p1 then p:=p1;
 StrLCopy(quote,ps,p-ps);
 GetQuote:=p;
end;

Function WildCardMatch(mask,s:string):boolean;
var pmask,ps,p:pchar;
    quote:array[0..100] of char;
begin
{ mask[length(mask)+1]:=#0;
 s[length(s)+1]:=#0;}
 result:=false;
 pmask:=@mask[1];
 ps:=@s[1];
While Pmask^<>#0 do
begin
 pmask:=GetQuote(pmask,quote);
 case Quote[0] of
  '?': if ps^<>#0 then inc(ps);
  '*': begin
	p:=GetQuote(pmask,quote);
	if quote[0] in ['*','?'] then continue;
        if Quote[0]=#0 then begin ps:=StrEnd(ps); continue; end;
	pmask:=p;
	p:=StrPos(ps,quote);
	if p=nil then exit;
	ps:=p+StrLen(quote);
       end;
  else if StrLComp(ps,quote,StrLen(quote))=0 then inc(ps,StrLen(quote)) else exit;
 end;
end;
if ps^=#0 then result:=true;
end;

Function ParseMasks(m:string):TStringList;{ mask -> masks string list. ie to handle "*.txt;*.asc" type masks}
var p,ps:Pchar;
    s:array[0..255] of char;
    Msk:TStringList;
begin
 msk:=TStringList.Create;
 if m='' then
 begin
  Msk.Add('');
  Result:=msk;
  exit;
 end;
 ps:=@m[1];
 Repeat
  p:=StrScan(ps,';');
  if p=nil then p:=StrEnd(ps);
  StrLCopy(s,Ps,p-ps);
  Msk.Add(UpperCase(s));
  ps:=p; if ps^=';' then inc(ps);
 Until PS^=#0;
 Result:=msk;
end;


Procedure TWildCardMask.SetMask(s:string);
begin
 if masks<>nil then begin masks.free; Masks:=nil; end;
 masks:=ParseMasks(s);
end;

Destructor TWildCardMask.Destroy;
begin
 Masks.free;
end;

Function TWildCardMask.GetMask:String;
var i:Integer;
begin
 Result:='';
 for i:=0 to masks.count-1 do Result:=Concat(Result,masks[i]);
end;

Procedure TWildCardMask.AddTrailingAsterisks;
var i:integer;s:string;
begin
 for i:=0 to masks.count-1 do
 begin
  s:=masks[i];
  if s='' then s:='*'
  else if s[length(s)]<>'*' then s:=s+'*';
  masks[i]:=s; 
 end;
end;

Function TWildCardMask.Match(s:String):boolean;
var i:integer;
begin
  s:=UpperCase(s);
  Result:=false;
  for i:=0 to masks.count-1 do
  begin
   Result:=Result or WildCardMatch(masks.Strings[i],s);
   if Result then break;
  end;
end;


Type
    ct_type=(ct_unknown,ct_gob,ct_gob2,ct_goo,ct_wad,ct_lab,ct_lfd,ct_notfound);

Function WhatContainer(Path:String):ct_type;
var ext:String;
    buf:array[1..4] of char;
    f:TFile;
begin
 Result:=ct_unknown;
 if not FileExists(path) then begin Result:=ct_notfound; exit; end;

 Ext:=UpperCase(ExtractFileExt(path));
 if ext='.WAD' then result:=ct_wad
 else if ext='.GOB' then result:=ct_gob
 else if ext='.LAB' then result:=ct_lab
 else if ext='.LFD' then result:=ct_lfd
 else if ext='.GOO' then result:=ct_goo;
 if result=ct_gob then
 begin
  Try
   f:=OpenFileRead(Path,0);
   f.Fread(buf,4);
   if buf='GOB ' then result:=ct_gob2;
   f.Fclose;
  except
   on Exception do Result:=ct_unknown;
  end;

 end;
end;

Function IsInContainer(const path:TFileName):Boolean;
begin
 Result:=Pos('>',path)<>0;
end;

Function IsContainer(Const path:TFileName):boolean;
begin
 Result:=WhatContainer(path)<>ct_unknown;
end;

Function OpenContainer(Const path:TFileName):TContainerFile;
begin
 Case WhatContainer(Path) of
  ct_gob: Result:=TGOBDirectory.CreateOpen(path);
{  ct_wad: Result:=TWADDirectory.CreateOpen(path);
  ct_lab: Result:=TLABDirectory.CreateOpen(path);
  ct_lfd: Result:=TFLDDirectory.CreateOpen(path);}
  ct_gob2: Result:=TGOB2Directory.CreateOpen(path);
  ct_goo: Result:=TGOB2Directory.CreateOpen(path);
  ct_notfound: Raise Exception.Create(Path+' not found');
 else Raise Exception.Create(Path+' is not a container');
 end;
end;


Function OpenFileRead(Const path:TFileName;mode:word):TFile;
var ps,i:Integer;
    Fname,ContName:String;
    cf:TContainerFile;
begin
 result:=nil;
 ps:=Pos('>',path);
 if ps=0 then Result:=TDiskFile.CreateRead(path)
 else
 begin
  ContName:=Copy(path,1,ps-1);
  fName:=Copy(path,ps+1,length(path)-ps);
  i:=Containers.IndexOf(ContName);
  if i<>-1 then cf:=TContainerFile(Containers.Objects[i])
  else
   begin
    if Containers.Count>10 then
     for i:=0 to Containers.Count-1 do
     With Containers.Objects[i] as TContainerFile do
      if not Permanent then begin Free; Containers.Delete(i); break; end;
    cf:=OpenContainer(ContName);
    Containers.AddObject(ContName,cf);
   end;
  Result:=cf.OpenFile(Fname,0);
 end;
end;

Function FindProjDirFile(const name:string):string;
var ext:string;

Function Check(const dir:string;var res:string):boolean;
begin
 result:=false;
 if FileExists(ProjectDir+dir+name) then
 begin
  result:=true;
  res:=ProjectDir+dir+name;
 end;
end;

begin
 ext:=LowerCase(ExtractFileExt(name));

 if Check('',Result) then exit;

 if ext='.uni' then
 begin
  if Check('misc\',Result) then exit;
  if Check('ui\',Result) then exit;
 end;

end;

Function OpenGameContainer(const path:string):TContainerFile;
var i:Integer;
begin
 i:=Containers.IndexOf(Path);
  if i<>-1 then result:=TContainerFile(Containers.Objects[i])
  else
   begin
    Result:=OpenContainer(Path);
    Containers.AddObject(Path,Result);
   end;
 Result.Permanent:=true;
end;

Function OpenGameFile(const name:TFileName):TFile;
var cf:TContainerFile;ext:String;

function IsInGob(const GobName,FName:String):Boolean;
begin
 if GobName='' then begin result:=false; exit; end;
Try
 cf:=OpenGameContainer(GobName);
 Result:=cf.FileExists(Fname);
except
 On Exception do Result:=false;
end;
end;

Function TryFile(const name:string):TFile;
begin
 result:=nil;
 if FileExists(ProjectDir+Name) then
  Result:=OpenFileRead(ProjectDir+Name,0);
end;

begin
 Result:=nil;
 if FileExists(ProjectDir+Name) then
 begin
  Result:=OpenFileRead(ProjectDir+Name,0);
  exit;
 end;
 ext:=UpperCase(ExtractExt(name));

 if ext='.WAV' then
 begin
  Result:=TryFile('sound\'+Name);
  if result<>nil then exit;
  Result:=TryFile('voice\'+Name);
  if result<>nil then exit;
  Result:=TryFile('voiceuu\'+Name);
  if result<>nil then exit;

  if IsInGOB(Res1_Gob,'sound\'+Name) then begin Result:=OpenFileRead(Res1_gob+'>sound\'+name,0); exit; end;
  if IsInGOB(Res1_Gob,'voice\'+Name) then begin Result:=OpenFileRead(Res1_gob+'>voice\'+name,0); exit; end;
  if IsMots then
   if IsInGOB(Res2_Gob,'voiceuu\'+Name) then begin Result:=OpenFileRead(Res2_gob+'>voiceuu\'+name,0); exit; end;

 end;


 if (ext='.MAT') then
 begin
  Result:=TryFile('mat\'+Name);
  if result<>nil then exit;
  Result:=TryFile('3do\mat\'+Name);
  if result<>nil then exit;

  if IsInGOB(Res2_Gob,'mat\'+Name) then begin Result:=OpenFileRead(Res2_gob+'>mat\'+name,0); exit; end;
  if IsInGOB(Res2_Gob,'3do\mat\'+Name) then begin Result:=OpenFileRead(Res2_gob+'>3do\mat\'+name,0); exit; end;
 end;

 if (ext='.DAT') then
 begin
  Result:=TryFile('misc\'+Name);
  if result<>nil then exit;
  if IsInGOB(Res2_Gob,'misc\'+Name) then begin Result:=OpenFileRead(Res2_gob+'>misc\'+name,0); exit; end;
 end;

 if (ext='.3DO') then
 begin
  Result:=TryFile('3do\'+Name);
  if result<>nil then exit;
  if IsInGOB(Res2_Gob,'3do\'+Name) then begin Result:=OpenFileRead(Res2_gob+'>3do\'+name,0); exit; end;
 end;

 if (ext='.KEY') then
 begin
  Result:=TryFile('\3do\key\'+Name);
  if result<>nil then exit;
  if IsInGOB(Res2_Gob,'3do\key\'+Name) then begin Result:=OpenFileRead(Res2_gob+'>3do\key\'+name,0); exit; end;
 end;

 if (ext='.AI') or (ext='.AI0') or (ext='.AI2') then
 begin
  Result:=TryFile('misc\ai\'+Name);
  if result<>nil then exit;
  if IsInGOB(Res2_Gob,'misc\ai\'+Name) then begin Result:=OpenFileRead(Res2_gob+'>misc\ai\'+name,0); exit; end;
 end;

 if (ext='.CMP') then
 begin
  Result:=TryFile('misc\cmp\'+Name);
  if result<>nil then exit;
  if IsInGOB(Res2_Gob,'misc\cmp\'+Name) then begin Result:=OpenFileRead(Res2_gob+'>misc\cmp\'+name,0); exit; end;
 end;

 if (ext='.PAR') then
 begin
  Result:=TryFile('misc\par'+Name);
  if result<>nil then exit;
  if IsInGOB(Res2_Gob,'misc\par\'+Name) then begin Result:=OpenFileRead(Res2_gob+'>misc\par\'+name,0); exit; end;
 end;

 if (ext='.PER') then
 begin
  Result:=TryFile('misc\per\'+Name);
  if result<>nil then exit;
  if IsInGOB(Res2_Gob,'misc\per\'+Name) then begin Result:=OpenFileRead(Res2_gob+'>misc\per\'+name,0); exit; end;
 end;

 if (ext='.PUP') then
 begin
  Result:=TryFile('misc\pup\'+Name);
  if result<>nil then exit;
  if IsInGOB(Res2_Gob,'misc\pup\'+Name) then begin Result:=OpenFileRead(Res2_gob+'>misc\pup\'+name,0); exit; end;
 end;

 if (ext='.SND') then
 begin
  Result:=TryFile('misc\snd\'+Name);
  if result<>nil then exit;
  if IsInGOB(Res2_Gob,'misc\snd\'+Name) then begin Result:=OpenFileRead(Res2_gob+'>misc\snd\'+name,0); exit; end;
 end;

 if (ext='.SPR') then
 begin
  Result:=TryFile('misc\spr\'+Name);
  if result<>nil then exit;
  if IsInGOB(Res2_Gob,'misc\spr\'+Name) then begin Result:=OpenFileRead(Res2_gob+'>misc\spr\'+name,0); exit; end;
 end;

 if (ext='.COG') then
 begin
  Result:=TryFile('cog\'+Name);
  if result<>nil then exit;

  if IsInGOB(Res2_Gob,'cog\'+Name) then begin Result:=OpenFileRead(Res2_gob+'>cog\'+name,0); exit; end;
  if IsInGOB(sp_gob,'cog\'+Name) then begin Result:=OpenFileRead(sp_gob+'>cog\'+name,0); exit; end;
  if IsInGOB(mp1_gob,'cog\'+Name) then begin Result:=OpenFileRead(mp1_gob+'>cog\'+name,0); exit; end;
  if IsInGOB(mp2_gob,'cog\'+Name) then begin Result:=OpenFileRead(mp2_gob+'>cog\'+name,0); exit; end;
  if IsInGOB(mp3_gob,'cog\'+Name) then begin Result:=OpenFileRead(mp3_gob+'>cog\'+name,0); exit; end;

 end;
 PanMessage(mt_warning,'Can''t find file anywhere: '+Name);
 Raise Exception.Create('Can''t find file anywhere: '+Name);
end;

Function OpenFileWrite(Const path:TFileName;mode:word):TFile;
begin
 Result:=TDiskFile.CreateWrite(path);
end;

Procedure TMaskedDirectoryControl.ClearControl;
begin
Case Control_type of
 LBox:begin
        LbDir.Items.BeginUpdate;
        LbDir.Items.Clear;
        LbDir.Items.EndUpdate;
      end;  
 LView:begin
        LVDir.Items.BeginUpdate;
        LVdir.Items.Clear;
        LVDir.Items.EndUpdate;
       end;
end;
end;

Procedure TMaskedDirectoryControl.SetDir(D:TContainerFile);
var i:integer;
    lc:TListColumn;
    FontWidth:Integer;
Function LineWidth(n:Integer):Integer;
begin
 Result:=n*Fontwidth;
end;

begin
 Dir:=d;
 if d=nil then exit;
 case control_type of
  LBox: FontWidth:=LBDir.Font.size;
  Lview: FontWidth:=LVDir.Font.size;
 end;

Case control_type of
LBox:begin
      LBDir.Columns:=lBDir.Width div LineWidth(Dir.GetColWidth(0));
     end;
LView:begin
       LVDir.Columns.Clear;
       for i:=0 to Dir.ColumnCount-1 do
       begin
        lc:=LVDir.Columns.Add;
        lc.Caption:=Dir.GetColName(i);
        lc.Width:=LineWidth(Dir.GetColWidth(i));
       end;
      end;
end;
end;

Constructor TMaskedDirectoryControl.CreateFromLB(L:TlistBox);
begin
 LBDir:=l;
 control_type:=LBox;
 Mask:='*.*';
end;

Constructor TMaskedDirectoryControl.CreateFromLV(L:TlistView);
begin
 LVDir:=l;
 control_type:=LView;
 Mask:='*.*'
end;

Procedure TMaskedDirectoryControl.BeginUpdate;
begin
 Case Control_type of
  LBox: begin
         LBDir.Sorted:=false;
         LBDir.Items.BeginUpdate;
        end;
  LView: begin
          LastViewStyle:=LVDir.ViewStyle;
          LVDir.ViewStyle:=vsReport;
          LVDir.Items.BeginUpdate;
         end;
 end;
end;

Procedure TMaskedDirectoryControl.EndUpdate;
begin
 Case Control_type of
  LBox: begin
         LBDir.Items.EndUpdate;
        end;
  LView: begin
          LVDir.ViewStyle:=LastViewStyle;
          LVDir.Items.EndUpdate;
         end;
 end;
end;


procedure TMaskedDirectoryControl.AddFile;
var LI:TListItem;
begin
 Case Control_type of
  LBox: if fi=nil then LBDir.Items.AddObject('['+s+']',fi)
           else LBDir.Items.AddObject(s,fi);
  LView: With LVDir.Items do
         begin
          Li:=Add;
          Li.Caption:=s;
          Li.SubItems.Add(IntToStr(fi.Size));
          if fi=nil then Li.ImageIndex:=1 else Li.ImageIndex:=0;
          Li.Data:=fi;
         end;
 end;
end;


Procedure TMaskedDirectoryControl.SetMask(mask:string);
var Ts:TStringList;
    i:integer;
    Matcher:TWildCardMask;
begin
 if Dir=nil then exit;
 ClearControl;
 Matcher:=TWildCardMask.Create;
 Matcher.Mask:=Mask;

 BeginUpdate;
{ Progress.Reset(ts.count);}
 ts:=Dir.ListDirs;
  for i:=0 to ts.count-1 do AddFile(ts[i],nil);

 ts:=Dir.ListFiles;
 for i:=0 to ts.count-1 do
 begin
   if Matcher.Match(ts[i]) then AddFile(ts[i],TFileInfo(ts.Objects[i]));
{   Progress.Step;}
 end;
{ Progress.Hide;}
 EndUpdate;
 Matcher.free;
end;

Function ExtractExt(path:String):String;
var p:integer;
begin
 p:=Pos('>',path);
 if p<>0 then path[p]:='\';
 Result:=ExtractFileExt(path);
end;

Function ExtractPath(path:String):String;
var p:integer;
begin
 p:=Pos('>',path);
 if p<>0 then path[p]:='\';
 Result:=ExtractFilePath(Path);
 if p<>0 then Result[p]:='>';
end;

Function ExtractName(path:String):String;
var p:integer;
begin
 p:=Pos('>',path);
 if p<>0 then path[p]:='\';
 Result:=ExtractFileName(Path);
end;

Function ChangeExt(path:String;const newExt:String):String;
var p:integer;
begin
 p:=Pos('>',path);
 if p<>0 then path[p]:='\';
 Result:=ChangeFileExt(Path,newExt);
 if p<>0 then Result[p]:='>';
end;

Constructor TFileTStream.CreateFromTFile(af:TFile);
begin
 f:=af;
end;

function TFileTStream.Read(var Buffer; Count: Longint): Longint;
begin
 Result:=F.Fread(Buffer,Count);
end;

function TFileTStream.Write(const Buffer; Count: Longint): Longint;
begin
 Result:=F.FWrite(Buffer,Count);
end;

function TFileTStream.Seek(Offset: Longint; Origin: Word): Longint;
begin
 Result:=0;
 Case ORigin of
  soFromBeginning: begin F.Fseek(Offset); Result:=Offset; end;
    soFromCurrent: begin F.FSeek(F.FPos+Offset); Result:=F.Fpos; end;
        soFromEnd: begin F.FSeek(F.Fsize+Offset); Result:=F.FPos; end;
 end;
end;

Function ConcPath(const path1,path2:string):string;
begin
 if Path1='' then begin Result:=path2; exit; end;
 if not (Path1[Length(Path1)] in ['\',':']) then Result:=Path1+'\'+Path2
 else Result:=Path1+Path2;
end;

Function GetCurDir:string;
begin
 GetDir(0,Result);
end;

Procedure SetCurDir(const dir:string);
begin
try
 ChDir(dir);
except
 on Exception do;
end;
end;

Procedure FreeContainers;
var i:integer;
begin
 for i:=0 to Containers.Count-1 do Containers.Objects[i].Free;
 Containers.Free;
end;


Initialization
begin
 Containers:=TStringList.Create;
end;

Finalization
FreeContainers;

end.
