unit Files;

{This unit defines abstract file and directory
 classes as well as implementation of simple
 disk file, buffered file and a text file and
 a disk directory. Little addition }

interface
uses Classes,SysUtils, StdCtrls, FileCtrl, misc_utils;

Const
{Constants for text files}
txt_bufsize=8096;
txt_maxlinesize=4096;
Type

 TFileInfo=class
  size,
  offs:longint;
  Function GetString(n:Integer):String;virtual;
 end;

TFileChangeControl=class
 Fsize:longint;
 Ftime:Integer;
 Fname:String;
 Procedure SetFile(name:String);
 Function Changed:boolean;
end;

{TFileList=class(TStringList)
private
 Procedure SetFInfo(n:Integer;fi:TFileInfo);
 Function GetFInfo(n:Integer):TFileInfo;
public
 Property FileInfo[N:Integer]:TFileInfo read GetFInfo write SetFInfo;
end;}


 TFile=class
  Function GetFullName:String;virtual;abstract;
  Function GetShortName:String;virtual;abstract;
  Function Fread(var buf;size:integer):integer;virtual;abstract;
  Function Fwrite(const buf;size:integer):integer;virtual;abstract;
  Function Fsize:Longint;virtual;abstract;
  Function Fpos:Longint;virtual;abstract;
  Procedure Fseek(Pos:longint);virtual;abstract;
  Destructor FClose;virtual;abstract;
 end;

TContainerCreator=class {Service object. Used to modify containers}
 cf:TFile;
 Constructor Create(name:String);
 Procedure PrepareHeader(newfiles:TStringList);virtual;abstract;
 Procedure AddFile(F:TFile);virtual;abstract;
 Function ValidateName(name:String):String;virtual;abstract;
 Destructor Destroy;override;
end;


TContainerFile=class
 Permanent:Boolean;
 name:String;
 Files:TStringList;
 Dirs:TStringList;
 tmpflist:TStringList;
 tmpdlist:TStringList;
 F:TFile;
 fc:TFileChangeControl;
 HasDirs:boolean;
 CurDir:String;
 Constructor CreateOpen(path:String);
 Procedure Free;
 Destructor Destroy;Override;
 Function GetColName(n:Integer):String;dynamic;
 Function GetColWidth(n:Integer):Integer;dynamic;
 Function ColumnCount:Integer;dynamic;
 Function GetFullName:String;virtual;
 Function GetShortName:String;dynamic;
 Function OpenFile(Fname:TFileName;mode:Integer):TFile;virtual;
 Function OpenFilebyFI(fi:TFileInfo):TFile;virtual;
 Function ChDir(const d:String):boolean;virtual;
 Procedure RenameFile(Fname,newname:TFileName);virtual;abstract;
 Function FileExists(const name:string):boolean; {full path}
 Function ListFiles:TStringList;virtual;
 Function ListDirs:TStringList;virtual;
 Function GetFileInfo(Fname:String):TFileInfo;virtual;
 Function AddFiles(AddList:TStringList):boolean; virtual;
 Function DeleteFiles(DelList:TStringList):boolean;virtual;abstract;
 Function AddFile(Fname:String):boolean;virtual;
 Function DeleteFile(Fname:String):boolean;
 Function GetContainerCreator(name:String):TContainerCreator;virtual;abstract; {used for adding/deleting files}
Protected
 Procedure ClearIndex;
 {Below go the methods that have to be overridden}
 Procedure Refresh;virtual;abstract;
 {Must fill the .Files filed Format:
 Strings[n] - name
 Objects[n] - TFileInfo for the file }
end;


TSubFile=class(TFile)
 offs,size:longint;
 f:TFile;
 Fname:String;
 Function GetShortName:String;Override;
 Function GetFullName:String;Override;
 Constructor CreateRead(const fn,name:String;apos,asize:Longint);
 Function Fread(var buf;size:integer):integer;override;
 Function Fsize:Longint;override;
 Function Fpos:Longint;override;
 Procedure Fseek(Pos:longint);override;
 Destructor FClose;override;
 Destructor Destroy;override;
end;

TBufFile=class(TFile)
 f:TFile;
 pbuf:pchar;
 spos:Longint;
 bpos,bsize:word;
 bcapacity:word;
 fmode:(f_read,f_write);
 Constructor CreateRead(bf:TFile);
 Constructor CreateWrite(bf:TFile);
 Function GetFullName:String;override;
 Function GetShortName:String;override;
 Function Fread(var buf;size:integer):integer;override;
 Function Fwrite(const buf;size:integer):integer;override;
 Function Fsize:Longint;override;
 Function Fpos:Longint;override;
 Procedure Fseek(Pos:longint);override;
 Destructor FClose;override;
Private
 Procedure InitBuffer(size:word);
 Procedure ReadBuffer;
 Procedure WriteBuffer;
end;

TTextFile=Class
 f:TFile;
 bpos,bsize:word;
 buffer:array[0..txt_bufsize] of char;
 curline:Integer;
 Function GetFullName:String;
 Constructor CreateRead(bf:TFile);
 Constructor CreateWrite(bf:TFile);
 Procedure Readln(var s:String);
 Procedure Writeln(s:String);
 Procedure WritelnFmt(const fmt:String;const args:array of const);
 Function Eof:Boolean;
 Destructor FClose;
 Destructor Destroy;override;
 Function FSize:Longint;
 Function FPos:Longint;
 Property CurrentLine:Integer read curLine;
Private
 Procedure LoadBuffer;
 Procedure SeekEoln;
end;

TLECTextFile=Class(TTextFile)
 FComment:String;
 Procedure Readln(var s:String);
 Property Comment:String read FComment;
end;

{TParsedTextFile=class(TTextFile)
private
 fsp:TStringParser;
Public
 Constructor CreateRead(bf:TFile);
 Procedure ReadLine;
 Property Parsed: TStringParser read fsp;
 Destructor FClose;
end;}

TDiskDirectory=class(TContainerFile)
 Name:String;
 Constructor CreateFromPath(Dname:TfileName);
 Function GetFullName:String;override;
 Function GetShortName:String;override;
 Function OpenFile(Fname:TFileName;mode:Integer):TFile;override;
 Procedure RenameFile(Fname,newname:TFileName);override;
 Function DeleteFiles(DelList:TStringList):boolean;override;
 Function ListFiles:TStringList;override;
 Function AddFiles(AddList:TStringList):boolean;override;
end;

TDiskFile=class(TFile)
  f:File;
  Constructor CreateRead(path:TFileName);
  Constructor CreateWrite(path:TFileName);
  Function GetFullName:String;override;
  Function GetShortName:String;override;
  Function Fread(var buf;size:integer):integer;override;
  Function Fwrite(const buf;size:integer):integer;override;
  Function Fsize:Longint;override;
  Function Fpos:Longint;override;
  Procedure Fseek(Pos:longint);override;
  Destructor FClose;override;
  Destructor Destroy;override;
end;

implementation

Uses FileOperations;

{Procedure TFileList.SetFInfo(n:Integer;fi:TFileInfo);
begin
 Objects[n]:=fi;
end;

Function TFileList.GetFInfo(n:Integer):TFIleInfo;
begin
 Result:=TFileInfo(Objects[n]);
end;}


Function TFileInfo.GetString(n:Integer):String;
begin
 if n=1 then Result:=IntToStr(size) else Result:='';
end;

Constructor TDiskFile.CreateRead(path:TFileName);
begin
 Assign(f,path);
 FileMode:=0;
 Reset(f,1);
end;

Constructor TDiskFile.CreateWrite(path:TFileName);
begin
 Assign(f,path);
 ReWrite(f,1);
end;


Function TDiskFile.GetFullName:String;
begin
 Result:=TFileRec(f).name;
end;

Function TDiskFile.GetShortName:String;
begin
 Result:=ExtractFileName(GetFullName);
end;


Function TDiskFile.Fread(var buf;size:integer):integer;
begin
 BlockRead(f,buf,size,Result);
end;

Function TDiskFile.Fwrite(const buf;size:integer):integer;
begin
 BlockWrite(f,buf,size,Result);
end;

Function TDiskFile.Fsize:Longint;
begin
 Result:=FileSize(f);
end;

Function TDiskFile.Fpos:Longint;
begin
 Result:=FilePos(f);
end;

Procedure TDiskFile.Fseek(Pos:longint);
begin
 Seek(f,Pos);
end;

Destructor TDiskFile.FClose;
begin
 CloseFile(f);
end;

Destructor TDiskFile.Destroy;
begin
 CloseFile(f);
end;

{TTextFile methods}

Destructor TTextFile.FClose;
begin
 f.FClose;
end;

Destructor TTextFile.Destroy;
begin
 f.Fclose;
end;

Function TTextFile.GetFullName:String;
begin
 Result:=F.GetFullName;
end;

Constructor TTextFile.CreateRead(bf:TFile);
begin
 f:=bf;
 f.Fseek(0);
 LoadBuffer;
end;

Function TTextFile.FSize:Longint;
begin
 Result:=f.Fsize;
end;

Function TTextFile.FPos:Longint;
begin
 Result:=f.Fpos+bsize-bpos;
end;


Constructor TTextFile.CreateWrite(bf:TFile);
begin
 f:=bf;
end;

Procedure TTextFile.SeekEoln;
var ps:Pchar;
begin
 ps:=StrScan(@buffer[bpos],#10);
 if ps<>nil then {EoLn found}
 begin
  bpos:=ps-@buffer+1;
  if bpos=bsize then LoadBuffer;
 end
 else {Eoln not found}
 begin
  Repeat
   LoadBuffer;
   if eof then exit;
   ps:=StrScan(buffer,#10);
  Until(ps<>nil);
  bpos:=ps-@buffer;
  SeekEoln;
 end;
end;

Procedure TTextFile.Readln(var s:String);
var ps,pend:Pchar;
    tmp:array[0..txt_maxlinesize-1] of char;
    ssize:word;
    l:word;
begin
 s:='';
 if bpos=bsize then LoadBuffer;
 if eof then exit;

 Inc(CurLine);

 ps:=buffer+bpos;
 pend:=StrScan(ps,#10);
 if pend<>nil then
 begin
  ssize:=pend-ps;
  if ssize>txt_maxlinesize then ssize:=txt_maxlinesize; {Limit string size to 255 chars}
  s:=StrLCopy(tmp,ps,ssize);
  l:=Length(s);
  if l<>0 then if s[l]=#13 then SetLength(s,l-1);
  inc(bpos,ssize);
  SeekEoln;
 end else
 begin
  ssize:=bsize-bpos;
  if ssize>txt_maxlinesize then ssize:=txt_maxlinesize;
  s:=StrLCopy(tmp,ps,ssize); {copy the tail of the buffer}
  LoadBuffer;
  if eof then exit;
  pend:=StrScan(buffer,#10);
  if pend=nil then ssize:=bsize else ssize:=pend-@buffer;
  if ssize+length(s)>txt_maxlinesize then ssize:=txt_maxlinesize-length(s);
  s:=ConCat(s,StrLCopy(tmp,buffer,ssize));
  inc(bpos,ssize);
  l:=Length(s);
  if l<>0 then if s[l]=#10 then SetLength(s,l-1);
  l:=Length(s);
  if l<>0 then if s[l]=#13 then SetLength(s,l-1);
  SeekEoln;
 end;
end;

Procedure TTextFile.Writeln;
const
     eol:array[0..0] of char=#10;
begin
 f.FWrite(s[1],length(s));
 f.FWrite(eol,sizeof(eol));
end;

Procedure TTextFile.WritelnFmt(const fmt:String;const args:array of const);
begin
 Writeln(Format(fmt,args));
end;

Function TTextFile.Eof:Boolean;
begin
 result:=bsize=0;
end;

Procedure TTextFile.LoadBuffer;
var bytes:longint;
begin
 bytes:=f.fsize-f.fpos;
 if bytes>txt_bufsize then bytes:=txt_bufsize;
 f.FRead(buffer,bytes);
 bpos:=0;
 bsize:=bytes;
 buffer[bsize]:=#0;
end;

{TLECTextFile}
Procedure TLECTextFile.Readln(var s:String);
var p:Integer;
begin
 Inherited Readln(s);
 p:=Pos('#',s);
 if p=0 then FComment:='' else
 begin
  FComment:=Copy(s,p,length(s)-p);
  SetLength(s,p-1);
 end;
end;

{TParsedTextFile}

{Constructor TParsedTextFile.CreateRead(bf:TFile);
begin
 fsp:=TStringParser.Create;
 Inherited CreateRead(bf);
end;

Procedure TParsedTextFile.ReadLine;
var s:String;p:Integer;
begin
 Readln(s);
 p:=pos('#',s); if p<>0 then SetLength(s,p-1);
 fsp.ParseString(s);
end;

Destructor TParsedTextFile.FClose;
begin
 fsp.free;
  Inherited;
end;}


{TBufFile methods}

Procedure TBufFile.InitBuffer(size:word);
begin
 bcapacity:=size;
 GetMem(pbuf,bcapacity);
end;

Procedure TBufFile.ReadBuffer;
var bytes:integer;
begin
 Bytes:=f.fsize-spos;
 if bytes>bcapacity then bytes:=bcapacity;
 bsize:=bytes;
 f.Fread(pbuf^,bsize);
 Inc(spos,bsize);
end;

Procedure TBufFile.WriteBuffer;
begin
 f.Fwrite(pbuf^,bpos);
 bpos:=0;
end;

Constructor TBufFile.CreateRead(bf:TFile);
begin
 InitBuffer(2048);
 f:=bf;
 ReadBuffer;
 fmode:=f_read;
end;

Constructor TBufFile.CreateWrite(bf:TFile);
begin
 fmode:=f_write;
end;

Function TBufFile.GetFullName:String;
begin
 result:=f.GetFullName;
end;

Function TBufFile.GetShortName:String;
begin
 Result:=f.GetShortName;
end;

Function TBufFile.Fread(var buf;size:integer):integer;
var bleft:integer;
begin
 if bpos+size<bsize then
 begin
  move((pbuf+bpos)^,buf,size);
  Inc(bpos,size);
 end
 else
 begin
  bleft:=bsize-bpos;
  Move((pbuf+bpos)^,buf,bleft);
  Fread((pchar(@buf)+bleft)^,size-bleft);
  Inc(spos,bpos+size);
  bpos:=0;bsize:=0;
 end;
end;

Function TBufFile.Fwrite(const buf;size:integer):integer;
begin
 if bpos+size<bcapacity then
 begin
  Move(buf,(pbuf+bpos)^,size);
  Inc(bpos,size);
 end
 else
 begin
  WriteBuffer;
  f.FWrite(buf,size);
  inc(spos,size);
 end;
end;

Function TBufFile.Fsize:Longint;
begin
 Result:=f.Fsize;
end;

Function TBufFile.Fpos:Longint;
begin
 Result:=spos+bpos;
end;

Procedure TBufFile.Fseek(Pos:longint);
begin
 if fmode=f_write then WriteBuffer;
 f.Fseek(pos);
 spos:=pos;
 if fmode=f_read then ReadBuffer;
end;

Destructor TBufFile.FClose;
begin
 if fmode=f_write then WriteBuffer;
 f.FClose;
 FreeMem(pbuf);
end;

Constructor TDiskDirectory.CreateFromPath(Dname:TfileName);
begin
 if (Dname<>'') and (Dname[length(Dname)]<>'\') and (Dname[Length(Dname)]<>':')
 then Name:=Dname+'\' else Name:=Dname; 
 If DirectoryExists(Dname) then
 else MkDir(Dname);
end;

Function TDiskDirectory.OpenFile(Fname:TFileName;mode:Integer):TFile;
begin
 Result:=TDiskFile.CreateRead(name+Fname);
end;

Procedure TDiskDirectory.RenameFile(Fname,newname:TFileName);
begin
 SysUtils.RenameFile(Name+Fname,Name+NewName);
end;

Function TDiskDirectory.DeleteFiles(DelList:TStringList):boolean;
var i:integer;
begin
 for i:=0 to files.count-1 do
  SysUtils.DeleteFile(files[i]);
  Result:=true;
end;

Function TDiskDirectory.GetShortName:String;
begin
 Result:=name;
end;

Function TDiskDirectory.GetFullName:String;
begin
 Result:=Name;
end;

Function TDiskDirectory.ListFiles:TStringList;
var sr:TSearchRec;
    Files:TStringList;
    res:integer;
    Fi:TFileInfo;
    i:integer;
begin
 if files<>nil then
 begin
  for i:=0 to files.count-1 do files.objects[i].free;
  Files.Free;
 end;

 Files:=TStringList.Create;
 Res:=FindFirst(Name+'*.*',faHidden+faSysFile+faArchive+faReadOnly,sr);
 While res=0 do
 begin
  Fi:=TFileInfo.Create;
  Fi.Size:=sr.size;
  Files.AddObject(sr.name,fi);
  res:=FindNext(sr);
 end;
 FindClose(sr);
 Result:=Files;
end;

Function TDiskDirectory.AddFiles(AddList:TStringList):boolean;
begin
end;

Constructor TContainerCreator.Create(name:String);
begin
 cf:=OpenFileWrite(name,fm_Create or fm_LetReWrite);
end;

Destructor TContainerCreator.Destroy;
begin
 if cf<>nil then cf.FClose;
end;

Procedure TContainerFile.Free;
begin
 if not Permanent then Destroy;
end;

Function TContainerFile.GetColWidth(n:Integer):Integer;
begin
 case n of
  0: Result:=13;
  1: Result:=10;
 else Result:=10;
 end;
end;

Function TContainerFile.GetColName(n:Integer):String;
begin
 case n of
  0: Result:='Name';
  1: Result:='Size';
 else Result:='';
 end;
end;

Function TContainerFile.ColumnCount:Integer;
begin
 Result:=2;
end;

Function TContainerFile.OpenFile(Fname:TFileName;mode:Integer):TFile;
var i:integer;
begin
 if fc.Changed then Refresh;
 if mode<>0 then raise Exception.Create('Cannot write file inside container: '+Fname);
 i:=Files.IndexOf(Fname);
 if i=-1 then raise Exception.Create('File not found: '+Fname);
 With TFileInfo(Files.Objects[i]) do
  Result:=TSubFile.CreateRead(Name,Fname,offs,size);
end;

Function TContainerFile.OpenFilebyFI;
var i:integer;
begin
 if fc.Changed then raise Exception.Create('File has be changed! reload.');
 i:=Files.IndexOfObject(fi);
 if i=-1 then raise Exception.Create('OpenFileByFI: Cannot find file' );
 Result:=TSubFile.CreateRead(Name,Files[i],fi.offs,fi.size);
end;

Function GetDirAt(const s:string;p:integer):string;
var sp:pchar;
begin
 sp:=StrScan(@s[p],'\');
 if sp=nil then Begin Result:=StrPas(@s[p]); exit; end;
 Result:=Copy(s,p,sp-@s[p]);
end;

Function IsLastinPath(const Path:string;p:integer):boolean;
begin
 Result:=StrScan(@Path[p],'\')=nil;
end;

Function TContainerFile.ListDirs:TStringList;
var i:integer;
    dname,dir:string;
begin
 if fc.Changed then Refresh;
 tmpdlist.clear;
 dir:=CurDir;
 if dir<>'' then dir:=dir+'\';
 for i:=0 to Dirs.Count-1 do
 begin
  dname:=Dirs[i];
  if CompareText(Copy(Dname,1,length(Dir)),Dir)<>0 then continue;
  if not IsLastInPath(Dname,length(dir)+1) then continue;
  tmpdlist.Add(GetDirAt(Dname,length(dir)+1));
 end;
 Result:=tmpdlist;
end;

Function TContainerFile.ChDir(const d:String):boolean;
var i:integer;
begin
 Result:=false;
 if (d<>'') and (Dirs.IndexOf(d)=-1) then exit;
 CurDir:=d;
 Result:=true;
end;

Function TContainerFile.DeleteFile(Fname:String):boolean;
var sl:TStringList;
begin
 sl:=TStringList.Create;
 sl.Add(name);
 DeleteFiles(sl);
 sl.Free;
end;

Function TContainerFile.AddFile(Fname:String):boolean;
var sl:TStringList;
begin
 sl:=TStringList.Create;
 sl.Add(Fname);
 AddFiles(sl);
 sl.Free;
end;

Constructor TContainerFile.CreateOpen;
begin
 Name:=Path;
 Files:=TStringList.Create;
 Dirs:=TStringList.Create;
 tmpflist:=TStringList.Create;
 tmpdlist:=TStringList.Create;
 fc:=TFileChangeControl.Create;
 fc.SetFile(name);
 Refresh;
end;

Destructor TContainerFile.Destroy;
var i:integer;
begin
 for i:=0 to Files.count-1 do Files.objects[i].Free;
 for i:=0 to Dirs.count-1 do Dirs.objects[i].Free;
 if tmpflist<>nil then tmpflist.free;
 if tmpdlist<>nil then tmpdlist.free;
 fc.Free;
 Files.Free;
 Dirs.Free;
end;

Function TContainerFile.ListFiles:TStringList;
var i:integer;
    dir:string;
    fname:string;
begin
 if fc.Changed then Refresh;
 tmpflist.clear;
 dir:=CurDir;
 if dir<>'' then dir:=dir+'\';
 for i:=0 to files.Count-1 do
 begin
  fname:=files[i];
  if CompareText(Copy(fname,1,length(Dir)),Dir)<>0 then continue;
  if not IsLastInPath(fname,length(dir)+1) then continue;
  tmpflist.AddObject(GetDirAt(Fname,length(dir)+1),files.Objects[i]);
 end;
 Result:=tmpflist;
end;

Function TContainerFile.FileExists(const name:string):boolean;
begin
 Result:=Files.IndexOf(name)<>-1;
end;


Procedure TContainerFile.ClearIndex;
var i:integer;
begin
 for i:=0 to files.count-1 do files.objects[i].free;
 files.clear;
end;

Function TContainerFile.GetFileInfo(Fname:String):TFileInfo;
var i:integer;
begin
 i:=Files.IndexOf(Fname);
 if i=-1 then result:=nil
 else Result:=TFileInfo(Files.Objects[i]);
end;

Function TContainerFile.GetFullName:String;
begin
 Result:=name;
end;

Function TContainerFile.GetShortName:String;
begin
 Result:=ExtractFileName(GetFullName);
end;

Function TContainerFile.AddFiles(AddList:TStringList):boolean;
var CCreator:TContainerCreator;
    newName:String;
    newFiles:TStringList;
    cName,fName:String;
    i,n:Integer;
    f:TFile;
begin
 NewName:=ChangeFileExt(name,'.tmp');
 CCreator:=GetContainerCreator(NewName);
 newFiles:=TStringList.Create;
 NewFiles.Assign(Files);
 for i:=0 to AddList.Count-1 do
 begin
  fName:=AddList[i];
  n:=Files.IndexOf(ExtractName(fName));
  if n=-1 then NewFiles.Add(fName)
  else begin NewFiles[n]:=fName; NewFiles.Objects[n]:=nil; end;
 end;
CCreator.PrepareHeader(NewFiles);
For i:=0 to NewFiles.Count-1 do
begin
 if NewFiles.Objects[i]=nil then {From Container}
  f:=OpenFile(NewFiles[i],0)
 else {Outside file}
  f:=OpenFileRead(NewFiles[i],0);
 CCreator.AddFile(f);
 f.Fclose;
end;
 newFiles.Free;
 CCreator.Free;
 BackUpFile(Name);
 RenameFile(NewName,Name);
 Refresh;
end;


Constructor TSubFile.CreateRead(const fn,name:String;apos,asize:Longint);
begin
 FName:=fn+'>'+Name;
 f:=OpenFileRead(fn,0);
 offs:=apos;
 size:=asize;
 f.Fseek(offs);
end;

Function TSubFile.GetFullName:String;
begin
 Result:=Fname;
end;


Function TSubFile.GetShortName:String;
begin
 Result:=ExtractName(Fname);
end;

Function TSubFile.Fread(var buf;size:integer):integer;
begin
 Result:=f.FRead(buf,size);
end;

Function TSubFile.Fsize:Longint;
begin
 Result:=size;
end;

Function TSubFile.Fpos:Longint;
begin
 Result:=f.Fpos-offs;
end;

Procedure TSubFile.Fseek(Pos:longint);
begin
 F.FSeek(offs+pos);
end;

Destructor TSubFile.FClose;
begin
 f.Fclose;
end;

Destructor TSubFile.Destroy;
begin
 f.Fclose;
end;


Procedure TFileChangeControl.SetFile(name:String);
var sr:TSearchRec;
begin
 FName:=name;
 FindFirst(Fname,faAnyFile,sr);
 Ftime:=sr.time;
 Fsize:=sr.size;
 FindClose(sr);
end;

Function TFileChangeControl.Changed:boolean;
var sr:TSearchRec;
begin
 FindFirst(Fname,faAnyFile,sr);
 result:=(Ftime<>sr.time) or (Fsize<>sr.size);
 FindClose(sr);
end;

end.
