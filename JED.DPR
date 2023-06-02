program Jed;

uses
  Forms,
  Jed_Main in 'Jed_Main.pas' {JedMain},
  tmp_load3do in 'tmp_load3do.pas',
  Render in 'Render.pas',
  J_level in 'J_level.pas',
  OGL_render in 'OGL_render.pas',
  Files in 'Files.pas',
  Tools in 'Tools.pas',
  GlobalVars in 'GlobalVars.pas',
  Geometry in 'Geometry.pas',
  ProgressDialog in 'ProgressDialog.pas' {Progress},
  lev_utils in 'lev_utils.pas',
  Containers in 'Containers.pas',
  FileOperations in 'FileOperations.pas',
  FileDialogs in 'FileDialogs.pas' {GetFileOpen},
  Unit1 in 'Unit1.pas' {Form1},
  values in 'values.pas',
  u_templates in 'u_templates.pas',
  ListRes in 'ListRes.pas',
  U_Options in 'U_Options.pas' {Options},
  U_CogForm in 'U_CogForm.pas' {CogForm},
  D3D_PRender in 'D3D_PRender.pas',
  d3d in 'd3d\D3d.pas',
  d3dcaps in 'd3d\D3dcaps.pas',
  d3drm in 'd3d\D3drm.pas',
  d3drmdef in 'd3d\D3drmdef.pas',
  d3drmobj in 'd3d\D3drmobj.pas',
  d3dtypes in 'd3d\D3dtypes.pas',
  ddraw in 'd3d\Ddraw.pas',
  d3drmwin in 'd3d\D3Drmwin.pas',
  Item_edit in 'Item_edit.pas' {ItemEdit},
  FieldEdit in 'FieldEdit.pas',
  misc_utils in 'misc_utils.pas',
  Cog_utils in 'Cog_utils.pas',
  ResourcePicker in 'ResourcePicker.pas' {ResPicker},
  Preview in 'Preview.pas',
  graph_files in 'graph_files.pas',
  Images in 'Images.pas',
  Cons_checker in 'Cons_checker.pas' {Consistency},
  FlagEditor in 'FlagEditor.pas' {FlagEdit},
  U_Tools in 'U_Tools.pas' {ToolForm},
  Jed_about1 in 'Jed_about1.pas' {Jed_about},
  U_tbar in 'U_tbar.pas' {Toolbar},
  U_Preview in 'U_Preview.pas' {Preview3D},
  U_msgForm in 'U_msgForm.pas' {MsgForm},
  U_SCFEdit in 'U_SCFEdit.pas' {SCFieldPicker},
  jdh_jdl in 'jdh_jdl.pas',
  Q_Utils in 'Q_Utils.pas',
  Q_Sectors in 'Q_Sectors.pas' {FindSectors},
  Q_surfs in 'Q_surfs.pas' {FindSurfs},
  Q_things in 'Q_things.pas' {FindThings},
  Prefab in 'Prefab.pas',
  U_Medit in 'U_Medit.pas' {EpisodeEdit},
  u_3dos in 'u_3dos.pas',
  U_lheader in 'U_lheader.pas' {LHEdit},
  U_3doprev in 'U_3doprev.pas' {Preview3DO},
  u_StrEdit in 'u_StrEdit.pas' {StrEdit},
  u_coggen in 'u_coggen.pas' {CogGen},
  u_DFI in 'u_DFI.pas' {DFImport},
  OGL_PRender in 'OGL_PRender.pas',
  sft_render in 'sft_render.pas',
  u_tpledit in 'u_tpledit.pas' {TplEdit},
  sft_persp_render in 'sft_persp_render.pas',
  u_undo in 'u_undo.pas',
  u_multisel in 'u_multisel.pas',
  GLunit in 'GLunit.pas',
  JED_OLE in 'JED_OLE.pas',
  u_copypaste in 'u_copypaste.pas',
  u_errorform in 'u_errorform.pas' {ErrForm},
  U_tplcreate in 'U_tplcreate.pas' {TPLCreator},
  PRender in 'PRender.pas',
  d3d_NPRender in 'd3d_NPRender.pas',
  DXTools in 'd3d\DXTools.pas',
  DPlay in 'd3d\DPlay.pas',
  DInput in 'd3d\DInput.pas',
  DSetup in 'd3d\DSetup.pas',
  DSound in 'd3d\DSound.pas',
  JED_COM in 'JED_COM.pas',
  jed_plugins in 'jed_plugins.pas',
  tbar_tools in 'tbar_tools.pas',
  u_cscene in 'u_cscene.pas' {KeyForm},
  u_pjkey in 'u_pjkey.pas',
  u_pj3dos in 'u_pj3dos.pas',
  pjGeometry in 'pjgeometry.pas',
  u_3doform in 'u_3doform.pas' {UrqForm};

{$R *.RES}
{$R Ver_info.res}
begin
  Application.Initialize;
  Application.CreateForm(TJedMain, JedMain);
  Application.CreateForm(TForm1, Form1);
  Application.CreateForm(TProgress, Progress);
  Application.CreateForm(TMsgForm, MsgForm);
  Application.CreateForm(TGetFileOpen, GetFileOpen);
  Application.CreateForm(TOptions, Options);
  Application.CreateForm(TCogForm, CogForm);
  Application.CreateForm(TItemEdit, ItemEdit);
  Application.CreateForm(TResPicker, ResPicker);
  Application.CreateForm(TConsistency, Consistency);
  Application.CreateForm(TFlagEdit, FlagEdit);
  Application.CreateForm(TToolForm, ToolForm);
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
  Application.Run;
end.
