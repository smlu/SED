unit Preview;

interface

uses
  MMSystem,Classes, ExtCtrls, StdCtrls, Files, FileOperations, graph_files,
  Windows,Graphics, SysUtils, Misc_utils;

type
  TPreviewThread = class(TThread)
  private
    { Private declarations }
    Im:TImage;
    MM:TMemo;
    LB:TLabel;
    SBMat:TScrollBar;
    Interrupted:boolean;
    NewFile:Boolean;
    Fname:String;
    cmp_name:string;
    F:TFile;
    Pcx:TPCX;
    mat:TMAT;
    bm:TBitmap;
    ptext:PChar;
    pWav:Pointer;
    cmppal:TCMPPal;
    Msg:String;
    bmcomment:string;
    nMatCell:integer;
    nCells:integer;
    Procedure TerminateProc(Sender:TObject);
    Procedure ShowBM;
    Procedure ShowMAT;
    Procedure ShowText;
    Procedure ShowNothing;
    Procedure ExceptionMsg;
  protected
    procedure Execute; override;
  Private
    procedure SBChange(Sender: TObject);
  Public
    Constructor Create(aIM:TImage;aMM:TMemo;aLB:TLabel;aSB:TScrollBar);
    Procedure StartPreview(const Name:String);
    Procedure StopPreview;
    Procedure SetCmp(const name:string);
  end;

implementation
uses Lev_utils, J_Level, U_3doprev;
{ Important: Methods and properties of objects in VCL can only be used in a
  method called using Synchronize, for example,

      Synchronize(UpdateCaption);

  and UpdateCaption could look like,

    procedure TPreviewThread.UpdateCaption;
    begin
      Form1.Caption := 'Updated in a thread';
    end; }

{ TPreviewThread }

Procedure TPreviewThread.SetCmp(const name:string);
begin
 cmp_name:=name;
// if cmp_name='' then cmp_name:='dflt.cmp';
 LoadCMPPal(name,CmpPal);
{ GetLevelPal(Level,cmpPal);
 ApplyCmp(name,cmpPal);}
end;

Procedure TPreviewThread.ShowBM;
begin
 LB.Visible:=true;
 Im.Visible:=true;
 mm.Visible:=false;
 LB.Caption:=Format('Width %d height %d%s',[BM.Width,BM.Height,bmcomment]);
 Im.Picture.Bitmap:=bm;
 Bm.Free;
end;

Procedure TPreviewThread.ShowMAT;
begin
 LB.Visible:=true;
 Im.Visible:=true;
 mm.Visible:=false;
 SBMat.OnChange:=nil;
 SBMat.Max:=ncells-1;
 SBMat.Visible:=bmcomment<>'';
 SBMat.OnChange:=SBChange;
 LB.Caption:=Format('Width %d height %d%s',[BM.Width,BM.Height,bmcomment]);
 Im.Picture.Bitmap:=bm;
 Bm.Free;
end;


Procedure TPreviewThread.ExceptionMsg;
begin
 PanMessage(mt_Error,Msg);
end;

Procedure TPreviewThread.ShowText;
begin
 Im.Visible:=false;
 LB.Visible:=false;
 MM.Visible:=true;
 Mm.SetTextBuf(Ptext);
 StrDispose(Ptext);
 Ptext:=nil;
end;

Procedure TPreviewThread.ShowNothing;
begin
 Im.Visible:=false;
 MM.Visible:=false;
 LB.Caption:='';
end;

Constructor TPreviewThread.Create(aIM:TImage;aMM:TMemo;aLB:TLabel;aSB:TScrollBar);
begin
 IM:=aIM;
 MM:=aMM;
 LB:=aLB;
 SBMat:=aSB;
 SBMat.onChange:=nil;
 SBMat.Position:=0;
 SBMat.onChange:=SBChange;

 SetCmp('dflt.cmp');
 Inherited Create(false);
 FreeOnTerminate:=true;
 OnTerminate:=TerminateProc;
end;

Procedure TPreviewThread.StartPreview(const Name:String);
begin
 Fname:=Name;
 NewFile:=true;
 nMatCell:=SBMat.Position;
 Resume;
end;

Procedure TPreviewThread.StopPreview;
begin
end;

Function GetANIMFromITM(const ITM:String):String;
var t:TLecTextFile;s,w:String;p:integer;
begin
 Result:='';
 t:=TLecTextFile.CreateRead(OpenGameFile(ITM));
 While not t.eof do
 begin
  T.Readln(s);
  p:=GetWord(s,1,w);
  if w<>'ANIM' then Continue;
  GetWord(s,p,Result);
  break;
 end;
 t.FClose;
end;


procedure TPreviewThread.Execute;
var ext,s:string;
    size:integer;
label loopend;
begin
  { Place thread code here }
  ShowNothing;
//  Suspend;
Repeat
 if not newfile then goto loopend;
 NewFile:=false;
 bmcomment:='';
 ext:=UpperCase(ExtractExt(Fname));
Try
 if ext='.3DO' then
 begin
  Set3DOCMP(cmp_name);
  View3DO(fname);
  continue;
 end;
 if (ext='.MAT') then
 begin
  f:=OpenGameFile(Fname);
  if f=nil then begin SynChronize(ShowNothing); Raise Exception.Create(Fname+' not found'); end;

  mat:=TMat.Create(f,nMatCell);
  Mat.SetPal(cmpPal);
  bm:=MAT.LoadBitmap(-1,-1);
  if Mat.isAnimated then bmComment:=', Multiple';
  nCells:=MAT.mh.NumOfTextures;
  MAT.Free;

  Synchronize(ShowMAT);
 end
 else if (ext='.PCX') then
 begin
  f:=OpenGameFile(Fname);
  if f=nil then begin SynChronize(ShowNothing); Raise Exception.Create(Fname+' not found'); end;

  Pcx:=TPCX.Create(f);
  bm:=Pcx.LoadBitmap(-1,-1);
  Pcx.Free;
  Synchronize(ShowBM);
 end
 else if (ext='.COG') or (ext='.PAR') or (ext='.SPR') or
         (ext='.AI') or (ext='.AI0') or (ext='.AI2') or
         (ext='.SND') or (ext='.PUP') then
 begin
  f:=OpenGameFile(Fname);
  if f=nil then begin SynChronize(ShowNothing); Raise Exception.Create(Fname+' not found'); end;
  size:=F.Fsize;
  if size>4000 then size:=4000;
  PText:=StrAlloc(size+1);
  F.Fread(Ptext^,size);
  (Ptext+size)^:=#0;
  F.FClose;
  Synchronize(ShowText);
 end
 else
 if ext='.WAV' then
 begin
  if PWav<>nil then
  begin
   PlaySound(Nil,0,SND_NODEFAULT);
   FreeMem(PWav);
   PWav:=nil;
  end;
  f:=OpenGameFile(Fname);
  if f=nil then begin SynChronize(ShowNothing); Raise Exception.Create(Fname+' not found'); end;
  GetMem(PWav,F.Fsize);
  F.Fread(PWav^,F.Fsize);
  F.FClose;
  PlaySound(PWav,0,SND_NODEFAULT or SND_MEMORY or SND_ASYNC);
  Synchronize(ShowNothing);
 end
 else Synchronize(ShowNothing);
Except
 on E:Exception do begin Msg:='Exception in thread: '+E.Message; Synchronize(ExceptionMsg); end
else begin Msg:='Exception in thread: '+ExceptObject.ClassName; Synchronize(ExceptionMsg); end;
end;
loopend:
if not NewFile then Suspend;
Until Terminated;
end;

Procedure TPreviewThread.TerminateProc;
begin
 SBMat.onChange:=nil;
 if PWav<>nil then
 begin
  PlaySound(Nil,0,SND_NODEFAULT);
  FreeMem(PWav);
 end;
end;

procedure TPreviewThread.SBChange(Sender: TObject);
begin
 nMatCell:=SBMat.Position;
 NewFile:=true;
 Resume;
end;

end.
