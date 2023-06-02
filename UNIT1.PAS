unit Unit1;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, u_templates, u_3DOs, globalvars, graph_files, lev_utils;

type
  TForm1 = class(TForm)
    Button1: TButton;
    Button2: TButton;
    Button3: TButton;
    Button4: TButton;
    procedure Button1Click(Sender: TObject);
    procedure Button2Click(Sender: TObject);
    procedure Button3Click(Sender: TObject);
    procedure Button4Click(Sender: TObject);
  private
    { Private declarations }
  public
    { Public declarations }
  end;

var
  Form1: TForm1;

implementation

uses FileDialogs, Files, FileOperations, misc_utils, U_Options,
  ProgressDialog, J_Level, Jed_main;

{$R *.DFM}

procedure TForm1.Button1Click(Sender: TObject);
var Cnt:TContainerFile;
    levs:TStringList;
    i:integer;
    t:TTextFile;
    s,w:string;

Procedure ProcessGob(const gobName:string);
var i:integer;
begin
 cnt:=OpenContainer(gobName);
 cnt.ChDir('jkl');
 levs:=Cnt.ListFiles;
 for i:=0 to levs.count-1 do
 begin
  t:=TTextFile.CreateRead(Cnt.OpenFileByFI(TFileInfo(levs.Objects[i])));
  s:='';
  While (not t.Eof) and (s<>'Section: TEMPLATES') do t.Readln(s);
  if t.eof then Exception.Create('');
  s:='';
  While (not t.Eof) and (Pos('World ',s)=0) do t.Readln(s);
  if t.eof then Exception.Create('');

  While true do
  begin
   if t.eof then break;
   t.Readln(s);
   if s='end' then break;
   RemoveComment(s);
   getWord(s,1,w);
   if w='' then continue;
   Templates.AddFromSTring(s);
  end;
  t.Fclose;
 end;
 Cnt.Free;
end;

begin
 Templates.Clear;
 ProcessGob(sp_gob);
 ProcessGob(mp1_gob);
 ProcessGob(mp2_gob);
 ProcessGob(mp3_gob);

 Templates.SaveToFile('d:\1.tpl');

end;

procedure TForm1.Button2Click(Sender: TObject);
var
    th:TJKThing;
    i:integer;
begin
 Progress.Reset(templates.count);
 th:=TJKThing.Create;
 for i:=0 to Templates.count-1 do
 With Templates[i] do
 begin
  th.name:=name;
  FillChar(th.bbox,sizeof(th.bbox),0);
  LoadThing3DO(th,true);
  bbox:=th.bbox;
  Free3DO(th.a3DO);
  Progress.Step;
 end;
 Progress.hide;
 templates.SaveToFile('d:\a.tpl');
end;

procedure TForm1.Button3Click(Sender: TObject);
var Cnt:TContainerFile;
    levs:TStringList;
    i:integer;
    t:TTextFile;
    s,w:string;
    dups,tplNames,tpls:TStringList;


Procedure ProcessGob(const gobName:string);
var i,n:integer;
begin
 cnt:=OpenContainer(gobName);
 cnt.ChDir('jkl');
 levs:=Cnt.ListFiles;
 for i:=0 to levs.count-1 do
 begin
  t:=TTextFile.CreateRead(Cnt.OpenFileByFI(TFileInfo(levs.Objects[i])));
  s:='';
  While (not t.Eof) and (s<>'Section: TEMPLATES') do t.Readln(s);
  if t.eof then Exception.Create('');
  s:='';
  While (not t.Eof) and (Pos('World ',s)=0) do t.Readln(s);
  if t.eof then Exception.Create('');

  While true do
  begin
   if t.eof then break;
   t.Readln(s);
   if s='end' then break;
   RemoveComment(s);
   s:=Trim(s);
   getWord(s,1,w);
   if w='' then continue;
   n:=Tplnames.IndexOf(w);
   if n=-1 then TplNames.AddObject(w,TObject(Tpls.Add(s))) else
   begin
    n:=Integer(TplNames.Objects[n]);
    if CompareText(s,tpls[n])<>0 then Dups.Add(s);
   end;
  end;
  t.Fclose;
 end;
 Cnt.Free;
end;

begin
 tpls:=TStringList.Create;
 dups:=TStringList.Create;
 tplnames:=TStringList.Create;
 tplnames.Sorted:=true;

 ProcessGob(sp_gob);
 ProcessGob(mp1_gob);
 ProcessGob(mp2_gob);
 ProcessGob(mp3_gob);

end;

procedure TForm1.Button4Click(Sender: TObject);
var sf,sf1:TJKSurface;
begin
 sf:=Level.Sectors[0].Surfaces[5];
 sf1:=sf.adjoin;
 DO_Surf_Overlap(sf,sf1);
end;

end.
