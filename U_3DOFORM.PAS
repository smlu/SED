unit u_3doform;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  ComCtrls, u_3dos,  misc_utils, StdCtrls, J_level, lev_utils, u_undo, commctrl;

type
  TUrqForm = class(TForm)
    TVNodes: TTreeView;
    BNSave: TButton;
    BNLoad: TButton;
    EBXYZ: TEdit;
    Label1: TLabel;
    EBPYR: TEdit;
    Label3: TLabel;
    EBType: TEdit;
    Label5: TLabel;
    Save3DH: TSaveDialog;
    Load3DH: TOpenDialog;
    BNEditOffsets: TButton;
    CBParent: TComboBox;
    Label2: TLabel;
    AddMissing: TButton;
    BNAddNode: TButton;
    BNDelNode: TButton;
    procedure TVNodesChange(Sender: TObject; Node: TTreeNode);
    procedure BNSaveClick(Sender: TObject);
    procedure BNLoadClick(Sender: TObject);
    procedure BNEditOffsetsClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure EBXYZChange(Sender: TObject);
    procedure EBXYZExit(Sender: TObject);
    procedure EBPYRChange(Sender: TObject);
    procedure EBTypeChange(Sender: TObject);
    procedure TVNodesEdited(Sender: TObject; Node: TTreeNode;
      var S: string);
    procedure CBParentChange(Sender: TObject);
    procedure AddMissingClick(Sender: TObject);
    procedure BNAddNodeClick(Sender: TObject);
    procedure BNDelNodeClick(Sender: TObject);
  private
    { Private declarations }
    lasthnode:ThNode;
    Procedure LoadNode(node:TTreeNode);
    procedure MoveOffset(ex:TObject;continued:boolean);
    procedure LoadMeshOffsets(reload:boolean);
    Procedure SetNodeParent(node:THNode;parent:integer);
    Procedure SelectNode(node:THNode);
  public
    { Public declarations }
   Procedure Reload;
   procedure UpdateMeshOffsets;
   Procedure GoToNode(n:integer);
  end;

var
  UrqForm: TUrqForm;


implementation

uses Jed_Main, Files, FileOperations, Graph_files;

{$R *.DFM}

Procedure TUrqForm.Reload;
var hnodes:THNodes;
    i,n:integer;
    tnodes:array[0..255] of TTreeNode; // already added nodes;
    recurdepth:integer;
    lastselected:THNode;

Function AddNode(nnode:integer):TTreeNode;
var node:THNode;
    pnode,tnode:TTreeNode;
begin
 inc(recurdepth);
 if recurdepth>50 then
 begin
  PanMessage(mt_warning,'Possible circular hierarchy! Node '+hnodes[nnode].NodeName);
  result:=NIL; dec(recurdepth); exit;

 end;

 if tnodes[nnode]<>nil then
 begin
  result:=tnodes[nnode];
  dec(recurdepth);
  exit;
 end;

 node:=hnodes[nnode];
 if (node.parent<0) or (node.parent>=hnodes.count) then pnode:=nil
 else pnode:=AddNode(node.parent);

 tnode:=TVNodes.Items.AddChildObject(pnode,node.NodeName,node);
 tnodes[nnode]:=tnode;
 result:=tnode;
 dec(recurdepth);
end;

begin
 recurdepth:=0;
 hnodes:=Level.h3donodes;
 lastselected:=NIL;
 if TVNodes.Selected<>NIL then lastselected:=THNode(TVNodes.Selected.Data);

 CBParent.Items.Clear;
 CBParent.Items.Add('None');
 for i:=0 to hnodes.count-1 do
  CBParent.Items.Add(hnodes[i].NodeName);


 FillChar(tnodes,sizeof(tnodes),0);
 TVNodes.Items.Clear;
 for i:=0 to hnodes.count-1 do AddNode(i);
 TVNodes.FullExpand;
 SelectNode(lastselected);
end;

Procedure TUrqForm.SelectNode(node:THNode);
var i:integer;
begin
 For i:=0 to TVNodes.Items.Count-1 do
  if TVNodes.Items[i].Data=node then
  begin
   TVNodes.Selected:=TVNodes.Items[i];
   break;
  end;

end;

Procedure TUrqForm.GoToNode(n:integer);
begin
 SelectNode(level.h3donodes[n]);
end;

procedure TUrqForm.TVNodesChange(Sender: TObject; Node: TTreeNode);
begin
 LoadNode(Node);
end;

procedure SetCBItem(CB:TComboBox;n:integer);
var e:TNotifyEvent;
begin
 e:=CB.OnChange;
 cb.OnChange:=NIL;
 cb.ItemIndex:=n;
 cb.OnChange:=e;
end;

Procedure TUrqForm.LoadNode(node:TTreeNode);
var hnode:THNode;
begin
 if node=nil then exit;
 hnode:=THNode(Node.Data);
 lasthNode:=hNode;
 SetCBItem(CBParent,hnode.parent+1);
 SetEditText(EBXYZ,Sprintf('%.6f %.6f %.6f',[hnode.x,hnode.y,hnode.z]));
 SetEditText(EBPYR,Sprintf('%.6f %.6f %.6f',[hnode.pch,hnode.yaw,hnode.rol]));
 SetEditText(EBType,IntToStr(hnode.ntype));
end;

procedure TUrqForm.UpdateMeshOffsets;
begin
 if JedMain.ExtraObjsName='Mesh Offset' then
 begin
  LoadMeshOffsets(false);
  JedMain.Invalidate;
 end;
end;

procedure TUrqForm.BNSaveClick(Sender: TObject);
var t:textfile;
    hnodes:THNodes;
    i:integer;
begin
 if not Save3DH.Execute then exit;
 AssignFile(t,Save3DH.FileName);
 rewrite(t);

 hnodes:=level.h3donodes;

 writeln(t,'SECTION: HIERARCHYDEF');
 Writeln(t,'HIERARCHY NODES ',hnodes.count);
 for i:=0 to hnodes.count-1 do
 With hnodes[i] do
 begin
  Writeln(t,HNodeToString(hnodes[i],i));
 end;

 closefile(t);
end;

procedure TUrqForm.BNLoadClick(Sender: TObject);
var t:textfile;
    hnodes:THNodes;
    hnode:THNode;
    p,i,n:integer;
    s,w:string;

Procedure GetNextLine(var s:String);
var cmt_pos:word; {Position of #}
begin
 s:='';
 Repeat
  if eof(t) then break;
  Readln(t,s);
  cmt_pos:=Pos('#',s);
  if cmt_pos<>0 then SetLength(s,cmt_pos-1);
  s:=UpperCase(Trim(s));
 Until s<>'';
end; {GetNextLine}

begin
 if not Load3DH.Execute then exit;
 ClearNodesUndo;
 AssignFile(t,Load3DH.FileName);
 Reset(t);
 hnodes:=level.h3donodes;
 for i:=0 to hnodes.count-1 do hnodes[i].Free;
 hnodes.Clear;

 while not eof(t) do
 begin
  GetNextLine(s);
  p:=GetWord(s,1,w);
  if w='SECTION:' then
  begin
   p:=GetWord(s,p,w);
   if w='HIERARCHYDEF' then break;
  end;
 end;

 if not eof(t) then
 begin
  GetNextLine(s);
  SScanf(s,'HIERARCHY NODES %d',[@n]);
  for i:=0 to n-1 do
  begin
   GetNextLine(s);
   hnode:=level.New3DONode;
   StringToHNode(s,hnode);
   HNodes.Add(Hnode);
   hnode.pivotx:=0;
   hnode.pivoty:=0;
   hnode.pivotz:=0;
 end;

 CloseFile(t);

 Reload;
end;

end;

procedure TUrqForm.MoveOffset(ex:TObject;continued:boolean);
var hnodes:THNodes;
    evx:TExtraVertex;
    ax,ay,az:double;
    cur_ex:integer;
    i:integer;
begin
 if not (ex is TExtraVertex) then exit;
  if not JedMain.IsSelValid then exit;
 if not continued then StartUndoRec('Node XYZ change');
 cur_ex:=JedMain.Cur_EX;
 SaveNodeUndo(level.h3donodes[cur_ex],ch_changed);

 evx:=TExtraVertex(ex);
 With level.h3donodes[Cur_EX] do
 begin
  ax:=evx.x; ay:=evx.y; az:=evx.z;
  if parent>=0 then UnOffsetByNode(level,parent,ax,ay,az);
  x:=ax;
  y:=ay;
  z:=az;
 end;
 UpdateMeshOffsets;
 JedMain.Cur_Ex:=cur_ex;
// JedMain.SetCurEx(cur_ex);
end;

procedure TUrqForm.LoadMeshOffsets(reload:boolean);
var hnodes:THNodes;
    hnode:THNode;
    i:integer;
    ax,ay,az:double;
begin
 if reload then JedMain.ClearExtraObjs;
 JedMain.ExtraObjsName:='Mesh Offset';
 hnodes:=level.h3donodes;
 for i:=0 to hnodes.count-1 do
 begin
  hnode:=hnodes[i];
  ax:=hnode.x; ay:=hnode.y; az:=hnode.z;
  if hnode.parent>=0 then OffsetByNode(level,hnode.parent,ax,ay,az);
  if reload then JedMain.AddExtraVertex(ax,ay,az,hnode.nodename)
  else with TEXtraVertex(JedMain.ExtraObjs[i]) do
       begin
        x:=ax;
        y:=ay;
        z:=az;
       end;

 end;
end;

procedure TUrqForm.BNEditOffsetsClick(Sender: TObject);
begin
 LoadMeshOffsets(true);
 Jedmain.OnExtraMove:=MoveOffset;
 JedMain.SetMapMode(MM_EXTRA);
 JedMain.SetFocus;
end;

procedure TUrqForm.FormCreate(Sender: TObject);
begin
 ClientHeight:=BNSave.Top+BNSave.Height+TVNodes.Top;
 ClientWidth:=EBXYZ.Left+EBXYZ.Width+TVNodes.Left;
end;

procedure TUrqForm.EBXYZChange(Sender: TObject);
begin
 if LastHNode=nil then exit;
 StartUndoRec('Node XYZ change');
 SaveNodeUndo(LastHNode,ch_changed);
 ValVector(EBXYZ.Text,LastHNode.X,LastHNode.Y,LastHNode.Z);
 UpdateMeshOffsets;
end;

procedure TUrqForm.EBXYZExit(Sender: TObject);
begin
 LoadNode(TVNodes.Selected);
end;

procedure TUrqForm.EBPYRChange(Sender: TObject);
begin
 if LastHNode=nil then exit;
 StartUndoRec('Node PYR change');
 SaveNodeUndo(LastHNode,ch_changed);
 ValVector(EBPYR.Text,LastHNode.Pch,LastHNode.Yaw,LastHNode.Rol);
 UpdateMeshOffsets;
end;

procedure TUrqForm.EBTypeChange(Sender: TObject);
begin
 if LastHNode=nil then exit;
 StartUndoRec('Node change');
 SaveNodeUndo(LastHNode,ch_changed);
 ValDword(EBType.Text,LastHNode.ntype);
end;

procedure TUrqForm.TVNodesEdited(Sender: TObject; Node: TTreeNode;
  var S: string);
var hnode:THNode;
begin
 if node=nil then exit;
 StartUndoRec('Node name change');
 SaveNodeUndo(LastHNode,ch_changed);
 hnode:=THNode(Node.Data);
 hnode.nodename:=s;
end;

procedure TUrqForm.CBParentChange(Sender: TObject);
begin
 if LastHNode=nil then exit;
 StartUndoRec('Node change');
 SaveNodeUndo(LastHNode,ch_changed);
 SetNodeParent(LastHNode,CBParent.ItemIndex-1);
end;

Procedure TUrqForm.SetNodeParent(node:THNode;parent:integer);
var i:integer;
begin
 node.parent:=parent;
 Reload;
end;

procedure TUrqForm.AddMissingClick(Sender: TObject);
var i,j:integer;
    hnode:THnode;
    s:string;
    found:boolean;
begin
 StartUndoRec('Add Nodes');

 if level.h3donodes.count=0 then
 begin
  hnode:=level.New3DONode;
  hnode.NodeName:='$$DUMMY';
  level.h3donodes.Add(hnode);
  SaveNodeUndo(hnode,ch_added);
 end;

 for i:=0 to level.layers.count-1 do
 begin
  s:=level.layers[i];
  found:=false;
   for j:=0 to level.h3donodes.count-1 do
    if CompareText(s,level.h3donodes[j].NodeName)=0 then
    begin found:=true; break; end;

  if not found then
  begin
   hnode:=level.New3DONode;
   hnode.NodeName:=s;
   hnode.parent:=0;
   level.h3donodes.Add(hnode);
   SaveNodeUndo(hnode,ch_added);
  end;

 end;

 Reload;
end;

procedure TUrqForm.BNAddNodeClick(Sender: TObject);
var hnode:THNode;
begin
 StartUndoRec('Add Node');
 hnode:=level.New3DONode;
 hnode.NodeName:='NewNode';
 level.h3donodes.Add(hnode);
 SaveNodeUndo(hnode,ch_added);
 Reload;
 SelectNode(hnode);
 TVNodes.SetFocus;
 TreeView_EditLabel(TVNodes.Handle,TreeView_GetSelection(TVNodes.Handle));
end;

procedure TUrqForm.BNDelNodeClick(Sender: TObject);
var hnode:THNode;
    i,n:integer;
begin
 ClearNodesUndo;
 n:=-1;
 For i:=0 to level.h3donodes.count-1 do
 begin
  if level.h3donodes[i]=LastHNode then begin n:=i; break; end;
 end;
 if n=-1 then exit;

 For i:=0 to level.h3donodes.count-1 do
 begin
  hnode:=level.h3donodes[i];
  if hnode.Parent>=n then
  begin
   if hnode.Parent=n then hnode.Parent:=-1;
   if hnode.Parent>n then Dec(hnode.Parent);
  end;
 end;

 level.h3donodes.Delete(n);
 hnode:=LastHNode;
 Reload;
 HNode.Free;
end;

end.
