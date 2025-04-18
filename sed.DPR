program sed;

uses
  Forms,
  Classes,
  GlobalVars in 'src\GlobalVars.pas',
  U_OPTIONS in 'src\U_OPTIONS.pas' {Options},
  JED_MAIN in 'src\JED_MAIN.PAS' {JedMain},
  tmp_load3do in 'src\tmp_load3do.pas',
  RENDER in 'src\RENDER.PAS',
  J_level in 'src\J_level.pas',
  Files in 'src\Files.pas',
  Tools in 'src\Tools.pas',
  GEOMETRY in 'src\GEOMETRY.PAS',
  ProgressDialog in 'src\ProgressDialog.pas' {Progress},
  lev_utils in 'src\lev_utils.pas',
  Containers in 'src\Containers.pas',
  FileOperations in 'src\FileOperations.pas',
  FileDialogs in 'src\FileDialogs.pas' {GetFileOpen},
  UNIT1 in 'src\UNIT1.PAS' {Form1},
  values in 'src\values.pas',
  u_templates in 'src\u_templates.pas',
  ListRes in 'src\ListRes.pas',
  U_CogForm in 'src\U_CogForm.pas' {CogForm},
  D3D_PRender in 'src\D3D_PRender.pas',
  D3d in 'src\d3d\D3d.pas',
  D3dcaps in 'src\d3d\D3dcaps.pas',
  D3dtypes in 'src\d3d\D3dtypes.pas',
  Ddraw in 'src\d3d\Ddraw.pas',
  Item_edit in 'src\Item_edit.pas' {ItemEdit},
  FieldEdit in 'src\FieldEdit.pas',
  misc_utils in 'src\misc_utils.pas',
  Cog_utils in 'src\Cog_utils.pas',
  ResourcePicker in 'src\ResourcePicker.pas' {ResPicker},
  Preview in 'src\Preview.pas',
  graph_files in 'src\graph_files.pas',
  Images in 'src\Images.pas',
  Cons_checker in 'src\Cons_checker.pas' {Consistency},
  FlagEditor in 'src\FlagEditor.pas' {FlagEdit},
  U_Tools in 'src\U_Tools.pas' {ToolForm},
  Jed_about1 in 'src\Jed_about1.pas' {Jed_about},
  U_tbar in 'src\U_tbar.pas' {Toolbar},
  U_PREVIEW in 'src\U_PREVIEW.PAS' {Preview3D},
  U_msgForm in 'src\U_msgForm.pas' {MsgForm},
  U_SCFEdit in 'src\U_SCFEdit.pas' {SCFieldPicker},
  JDH_JDL in 'src\JDH_JDL.PAS',
  Q_Utils in 'src\Q_Utils.pas',
  Q_Sectors in 'src\Q_Sectors.pas' {FindSectors},
  Q_surfs in 'src\Q_surfs.pas' {FindSurfs},
  Q_things in 'src\Q_things.pas' {FindThings},
  Prefab in 'src\Prefab.pas',
  U_Medit in 'src\U_Medit.pas' {EpisodeEdit},
  u_3dos in 'src\u_3dos.pas',
  U_lheader in 'src\U_lheader.pas' {LHEdit},
  U_3doprev in 'src\U_3doprev.pas' {Preview3DO},
  u_StrEdit in 'src\u_StrEdit.pas' {StrEdit},
  u_coggen in 'src\u_coggen.pas' {CogGen},
  U_DFI in 'src\U_DFI.PAS' {DFImport},
  OGL_PRender in 'src\OGL_PRender.pas',
  sft_render in 'src\sft_render.pas',
  u_tpledit in 'src\u_tpledit.pas' {TplEdit},
  sft_persp_render in 'src\sft_persp_render.pas',
  u_undo in 'src\u_undo.pas',
  u_multisel in 'src\u_multisel.pas',
  u_copypaste in 'src\u_copypaste.pas',
  u_errorform in 'src\u_errorform.pas' {ErrForm},
  U_tplcreate in 'src\U_tplcreate.pas' {TPLCreator},
  PRender in 'src\PRender.pas',
  d3d_NPRender in 'src\d3d_NPRender.pas',
  DXTools in 'src\d3d\DXTools.pas',
  DPlay in 'src\d3d\DPlay.pas',
  DInput in 'src\d3d\DInput.pas',
  DSetup in 'src\d3d\DSetup.pas',
  DSound in 'src\d3d\DSound.pas',
  SED_COM in 'src\SED_COM.pas',
  sed_plugins in 'src\sed_plugins.pas',
  tbar_tools in 'src\tbar_tools.pas',
  u_cscene in 'src\u_cscene.pas' {KeyForm},
  u_pjkey in 'src\u_pjkey.pas',
  u_pj3dos in 'src\u_pj3dos.pas',
  pjgeometry in 'src\pjgeometry.pas',
  u_3doform in 'src\u_3doform.pas' {UrqForm},
  JED_COM in 'src\JED_COM.PAS',
  JED_PLUGINS in 'src\JED_PLUGINS.PAS',
  math in 'src\math.pas',
  RenderUtils in 'src\RenderUtils.pas',
  IndyWV in 'src\IndyWV.pas',
  D3DUtils in 'src\d3d\D3DUtils.pas',
  dglOpenGL in 'src\dglOpenGL.pas',
  GLContext in 'src\GLContext.pas';

{$R *.RES}
{$R SEDstatic.res}

begin
  Application.Initialize;
  Application.CreateForm(TJedMain, JedMain);
  Application.CreateForm(TForm1, Form1);
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
  Application.CreateForm(TJed_about, Jed_about);
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
  Application.CreateForm(TTplEdit, TplEdit);
  Application.CreateForm(TErrForm, ErrForm);
  Application.CreateForm(TTPLCreator, TPLCreator);
  Application.CreateForm(TKeyForm, KeyForm);
  Application.CreateForm(TUrqForm, UrqForm);
  Application.ShowHint := True;
  Application.Run;
end.
