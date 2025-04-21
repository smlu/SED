program sed;

uses
  Forms,
  Classes,
  GlobalVars in 'src\GlobalVars.pas',
  U_OPTIONS in 'src\U_OPTIONS.pas' {Options},
  JED_MAIN in 'src\JED_MAIN.PAS' {JedMain},
  Render in 'src\Render.pas',
  J_level in 'src\J_level.pas',
  Files in 'src\Files.pas',
  TOOLS in 'src\TOOLS.PAS',
  GEOMETRY in 'src\GEOMETRY.PAS',
  ProgressDialog in 'src\ProgressDialog.pas' {Progress},
  lev_utils in 'src\lev_utils.pas',
  CONTAINERS in 'src\CONTAINERS.PAS',
  FILEOPERATIONS in 'src\FILEOPERATIONS.PAS',
  FILEDIALOGS in 'src\FILEDIALOGS.PAS' {GetFileOpen},
  values in 'src\values.pas',
  U_TEMPLATES in 'src\U_TEMPLATES.PAS',
  ListRes in 'src\ListRes.pas',
  U_COGFORM in 'src\U_COGFORM.PAS' {CogForm},
  D3d in 'src\d3d\D3d.pas',
  D3dcaps in 'src\d3d\D3dcaps.pas',
  D3dtypes in 'src\d3d\D3dtypes.pas',
  Ddraw in 'src\d3d\Ddraw.pas',
  Item_edit in 'src\Item_edit.pas' {ItemEdit},
  FIELDEDIT in 'src\FIELDEDIT.PAS',
  misc_utils in 'src\misc_utils.pas',
  ResourcePicker in 'src\ResourcePicker.pas' {ResPicker},
  PREVIEW in 'src\PREVIEW.PAS',
  graph_files in 'src\graph_files.pas',
  IMAGES in 'src\IMAGES.PAS',
  Cons_checker in 'src\Cons_checker.pas' {Consistency},
  FlagEditor in 'src\FlagEditor.pas' {FlagEdit},
  U_TOOLS in 'src\U_TOOLS.PAS' {ToolForm},
  SED_About in 'src\SED_About.PAS' {SedAbout},
  U_tbar in 'src\U_tbar.pas' {Toolbar},
  U_PREVIEW in 'src\U_PREVIEW.PAS' {Preview3D},
  U_MSGFORM in 'src\U_MSGFORM.PAS' {MsgForm},
  U_SCFEDIT in 'src\U_SCFEDIT.PAS' {SCFieldPicker},
  JDH_JDL in 'src\JDH_JDL.PAS',
  Q_Utils in 'src\Q_Utils.pas',
  Q_Sectors in 'src\Q_Sectors.pas' {FindSectors},
  Q_surfs in 'src\Q_surfs.pas' {FindSurfs},
  Q_things in 'src\Q_things.pas' {FindThings},
  PREFAB in 'src\PREFAB.PAS',
  U_MEDIT in 'src\U_MEDIT.PAS' {EpisodeEdit},
  U_3DOS in 'src\U_3DOS.PAS',
  U_LHEADER in 'src\U_LHEADER.PAS' {LHEdit},
  U_3doprev in 'src\U_3doprev.pas' {Preview3DO},
  U_STREDIT in 'src\U_STREDIT.PAS' {StrEdit},
  U_COGGEN in 'src\U_COGGEN.PAS' {CogGen},
  U_DFI in 'src\U_DFI.PAS' {DFImport},
  PRenderGL in 'src\PRenderGL.pas',
  RenderSW in 'src\RenderSW.pas',
  u_undo in 'src\u_undo.pas',
  u_multisel in 'src\u_multisel.pas',
  u_copypaste in 'src\u_copypaste.pas',
  u_errorform in 'src\u_errorform.pas' {ErrForm},
  U_TPLCREATE in 'src\U_TPLCREATE.PAS' {TPLCreator},
  PRender in 'src\PRender.pas',
  PRenderDX in 'src\PRenderDX.pas',
  DXTools in 'src\d3d\DXTools.pas',
  DPlay in 'src\d3d\DPlay.pas',
  DInput in 'src\d3d\DInput.pas',
  DSetup in 'src\d3d\DSetup.pas',
  DSound in 'src\d3d\DSound.pas',
  SED_COM in 'src\SED_COM.pas',
  sed_plugins in 'src\sed_plugins.pas',
  TBAR_TOOLS in 'src\TBAR_TOOLS.PAS',
  U_CSCENE in 'src\U_CSCENE.PAS' {KeyForm},
  U_PJKEY in 'src\U_PJKEY.PAS',
  U_PJ3DOS in 'src\U_PJ3DOS.PAS',
  U_3DOFORM in 'src\U_3DOFORM.PAS' {UrqForm},
  JED_COM in 'src\JED_COM.PAS',
  JED_PLUGINS in 'src\JED_PLUGINS.PAS',
  math in 'src\math.pas',
  RenderUtils in 'src\RenderUtils.pas',
  IndyWV in 'src\IndyWV.pas',
  D3DUtils in 'src\d3d\D3DUtils.pas',
  dglOpenGL in 'src\dglOpenGL.pas',
  GLContext in 'src\GLContext.pas',
  RenderGL in 'src\RenderGL.pas';

{$R *.RES}
{$R SEDstatic.res}

begin
  Application.Initialize;
  Application.CreateForm(TJedMain, JedMain);
  Application.CreateForm(TProgress, Progress);
  Application.CreateForm(TMsgForm, MsgForm);
  Application.CreateForm(TGetFileOpen, GetFileOpen);
  Application.CreateForm(TToolForm, ToolForm);
  Application.CreateForm(TOptions, Options);
  Application.CreateForm(TCogForm, CogForm);
  Application.CreateForm(TItemEdit, ItemEdit);
  Application.CreateForm(TResPicker, ResPicker);
  Application.CreateForm(TConsistency, Consistency);
  Application.CreateForm(TFlagEdit, FlagEdit);
  Application.CreateForm(TSedAbout, sedAbout);
  Application.CreateForm(TToolbar, Toolbar);
  Application.CreateForm(TPreview3D, Preview3D);
  Application.CreateForm(TSCFieldPicker, SCFieldPicker);
  Application.CreateForm(TFindSectors, FindSectors);
  Application.CreateForm(TFindSurfs, FindSurfs);
  Application.CreateForm(TFindThings, FindThings);
  Application.CreateForm(TEpisodeEdit, EpisodeEdit);
  Application.CreateForm(TLHEdit, LHEdit);
  Application.CreateForm(TStrEdit, StrEdit);
  Application.CreateForm(TCogGen, CogGen);
  Application.CreateForm(TDFImport, DFImport);
  Application.CreateForm(TErrForm, ErrForm);
  Application.CreateForm(TTPLCreator, TPLCreator);
  Application.CreateForm(TKeyForm, KeyForm);
  Application.CreateForm(TUrqForm, UrqForm);
  Application.ShowHint := True;
  Application.Run;
end.
