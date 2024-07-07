unit Preview;

interface

uses
  MMSystem, Classes, ExtCtrls, StdCtrls, Files, FileOperations, graph_files,
  Windows, Graphics, SysUtils, Misc_utils, GlobalVars, pngimage, Images;

type
  TPreviewThread = class(TThread)
  private
    { Private declarations }
    Im: TImage;
    MM: TMemo;
    LB: TLabel;
    SBMat: TScrollBar;
    Interrupted: boolean;
    NewFile: boolean;
    Fname: String;
    cmp_name: string;
    F: TFile;
    Pcx: TPCX;
    mat: TMAT;
    bm: TBitmap;
    ptext: PAnsiChar;
    //pWav: Pointer;
    bSndPlay: Boolean;
    cmppal: TCMPPal;
    Msg: String;
    bmcomment: string;
    nMatCell: integer;
    nCells: integer;
    Procedure TerminateProc(Sender: TObject);
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
    Constructor Create(aIM: TImage; aMM: TMemo; aLB: TLabel; aSB: TScrollBar);
    Procedure StartPreview(const Name: String);
    Procedure StopPreview;
    Procedure SetCmp(const Name: string);
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

Procedure TPreviewThread.SetCmp(const Name: string);
begin
  cmp_name := name;
  // if cmp_name='' then cmp_name:='dflt.cmp';
  LoadCMPPal(name, cmppal);
  { GetLevelPal(Level,cmpPal);
    ApplyCmp(name,cmpPal); }
end;

Procedure TPreviewThread.ShowBM;
begin
  LB.Visible := true;
  Im.Visible := true;
  MM.Visible := false;
  LB.Caption := Format('Width %d height %d%s',
    [bm.Width, bm.Height, bmcomment]);
  Im.Picture.Bitmap := bm;
  bm.Free;
end;

Procedure TPreviewThread.ShowMAT;
begin
  LB.Visible := true;
  Im.Visible := true;
  MM.Visible := false;
  SBMat.OnChange := nil;
  SBMat.Max := nCells - 1;
  SBMat.Visible := bmcomment <> '';
  SBMat.OnChange := SBChange;
  LB.Caption := Format('Width %d height %d%s',
    [bm.Width, bm.Height, bmcomment]);
  //Im.Proportional := true;

  Im.Picture.Assign(bm);
  IM.Refresh;
  IM.Repaint;
  bm.Free;
end;

Procedure TPreviewThread.ExceptionMsg;
begin
  PanMessage(mt_Error, Msg);
end;

Procedure TPreviewThread.ShowText;
begin
  Im.Visible := false;
  LB.Visible := false;
  MM.Visible := true;
  MM.SetTextBuf(PWideChar(string(ptext))); // TODO: Find better conversion
  StrDispose(ptext);
  ptext := nil;
end;

Procedure TPreviewThread.ShowNothing;
begin
  Im.Visible := false;
  MM.Visible := false;
  LB.Caption := '';
end;

Constructor TPreviewThread.Create(aIM: TImage; aMM: TMemo; aLB: TLabel;
  aSB: TScrollBar);
begin
  Im := aIM;
  MM := aMM;
  LB := aLB;
  SBMat := aSB;
  SBMat.OnChange := nil;
  SBMat.Position := 0;
  SBMat.OnChange := SBChange;

  if GlobalVars.CurrentProject <> IJIM then
    SetCmp('dflt.cmp');
  Inherited Create(false);
  FreeOnTerminate := False;
  OnTerminate := TerminateProc;
end;

Procedure TPreviewThread.StartPreview(const Name: String);
begin
  Fname := Name;
  NewFile := true;
  nMatCell := SBMat.Position;
  Resume;
end;

Procedure TPreviewThread.StopPreview;
begin
end;

Function GetANIMFromITM(const ITM: String): String;
var
  t: TLecTextFile;
  s, w: String;
  p: integer;
begin
  Result := '';
  t := TLecTextFile.CreateRead(OpenGameFile(ITM));
  While not t.eof do
  begin
    t.Readln(s);
    p := GetWord(s, 1, w);
    if w <> 'ANIM' then
      Continue;
    GetWord(s, p, Result);
    break;
  end;
  t.FClose;
end;

procedure TPreviewThread.Execute;
var
  ext, s: string;
  size: integer;
label loopend;
begin
  { Place thread code here }
  ShowNothing;
  // Suspend;
  Repeat
    if not NewFile then
      goto loopend;

    NewFile := false;
    bmcomment := '';
    ext := UpperCase(ExtractExt(Fname));
    try
      if ext = '.3DO' then
        begin
          Set3DOCMP(cmp_name);
          View3DO(Fname);
          Continue;
        end;

      if (ext = '.MAT') then
        begin
          F := OpenGameFile(Fname);
          if F = nil then
          begin
            SynChronize(ShowNothing);
            Raise Exception.Create(Fname + ' not found');
          end;

          mat := TMAT.Create(F, nMatCell);
          mat.SetPal(cmppal);
          bm := mat.LoadBitmap(-1, -1);
          if mat.isAnimated then
            bmcomment := ', Multiple';
          nCells := mat.celCount;
          mat.Free;

          SynChronize(ShowMAT);
        end
      else if (ext = '.PCX') then
        begin
          F := OpenGameFile(Fname);
          if F = nil then
          begin
            SynChronize(ShowNothing);
            Raise Exception.Create(Fname + ' not found');
          end;

          Pcx := TPCX.Create(F);
          bm := Pcx.LoadBitmap(-1, -1);
          Pcx.Free;
          SynChronize(ShowBM);
        end
      else if (ext = '.COG') or (ext = '.PAR') or (ext = '.SPR') or
        (ext = '.AI') or (ext = '.AI0') or (ext = '.AI2') or (ext = '.SND') or
        (ext = '.PUP') then
        begin
          F := OpenGameFile(Fname);
          if F = nil then
          begin
            SynChronize(ShowNothing);
            Raise Exception.Create(Fname + ' not found');
          end;
          size := F.Fsize;
          if size > 4000 then
            size := 4000;
          ptext := AnsiStrAlloc(size + 1);
          F.Fread(ptext^, size);
          (ptext + size)^ := #0;
          F.FClose;
          SynChronize(ShowText);
        end
      else if ext = '.WAV' then
        begin
          try
            appSoundPlayer.Play(Fname);
            appSoundPlayer.volume := 1;
            bSndPlay := True;

          finally
            SynChronize(ShowNothing);
          end;
        end
      else
        SynChronize(ShowNothing);
    except
      on E: Exception do
        begin
          Msg := 'Exception in thread: ' + E.Message;
          SynChronize(ExceptionMsg);
        end
      else
        begin
          Msg := 'Exception in thread: ' + ExceptObject.ClassName;
          SynChronize(ExceptionMsg);
        end;
    end;

  loopend:
    if not NewFile then
      Suspend;
  Until Terminated;
end;

Procedure TPreviewThread.TerminateProc;
begin
  SBMat.OnChange := nil;
  if bSndPlay then
  begin
    bSndPlay := False;
    appSoundPlayer.Stop;
  end;

//  if pWav <> nil then
//  begin
//    PlaySound(Nil, 0, SND_NODEFAULT);
//    FreeMem(pWav);
//  end;
end;

procedure TPreviewThread.SBChange(Sender: TObject);
begin
  nMatCell := SBMat.Position;
  NewFile := true;
  Resume;
end;

end.
