# JED
Level editor for Jedi Knight game

Revision History

Version 0.1 Beta - first released version

Version 0.3 beta
- Added textured 3D Preview
- Fixed clipped label in GOB dialog
- Fixed bug cause failure to enter directory inside GOB
- Fixed CD detection - should now work fine.
- Added floating hints in Flag Editor
- Fixed cleave bug giving "uncleaveable" surfaces
- Added current X,Y,Z panel
- Fixed bug in reading COG variables
- Added "Save As..." option.
- Added Named layers
- Added texture shifting, rotating
- Message pan split to message panel and messages window.
- Loading or create a new level now clears the messages
- Fixed edge selection. Improved edge selection display
- Added texturing tools ( < > with Shift and CTRL)
- Added Stitching tools (keys ; and ')
- Added "Straighten texture" tool ( /, +Shift, +Ctrl)
- Added some optimizations to lighting calculation - should be 
  faster now.
- Fixed level display in 256 color modes.
- Added project-local master template capability (create 
  file master.tpl in your project directory and it's used
  instead of default one)
- Fixed bug in respicker 
- Placed COG editor now shows parameter type
- Added Edge cleave
- Added detection of concave surfaces and sectors in Consistency Checker
- Added detection of invalid surfaces
- Selected things now displays orientation and approximate size
- Changed main window movement controls slightly.
- Added template descriptions in ResPicker
- Added translation perpendicular to the grid ( [ and ] )
- Improved grid
- Added rotation and scaling tools

Version 0.5 beta
- Item Editor and Toolbar can now be minimized (double-click on caption)
- Added recent documents menu
- Added color configuration in Options
- Fixed bug that caused "File not found" on invoking Options
  dialog or CD detection when JK directory was specified incorrectly
- Added alternative texturing and stitching keys - for
  people with international keyboards
  (in 3D preview only 
    Ctrl+left,right arrow - rotate texture
    Shft+left,right,up,down arrow - shift texture
   in both main window and 3D preview
    Alt+Home, Ctrl+Home, Ctrl+Shift+Home - straighen texture (optionally 
                                           rotating 90 and zeroing at origin)
    Ctrl+INS; Shift+INS - select start surface; stitch)
- Added collide box calculation (should speed up MP levels)
- JED now loads and saves lights in JKLs in FB format
- The floor flag is now automatically set on created surfaces if they have 
  less than 45 degree slope
- Message Window now pops up on warnings
- Added "Grid to View" option.
- Added "Magnify at cursor" function - Shift+C
- Added Copy Sector function ( INS - instead of create cube)
- Added custom list capability to ResPicker
- "Refresh" in COG editor now refreshes COG parameters
- Added "Find" tool for sectors, surfaces and things (Ctrl+F, Ctrl+G)
- A backup copy is now created when saving (file with extension .~je)
- If you changed the project and didn't save it, you're now asked
  to do it when you close JED or open a new project.
- Fixed bug in concave surface detection
- Added "Used in cogs" list box in Item Editor window
- Changed CMP handling to match the way JK does it (so now if
  something looks wrong in JED, that means it will look wrong 
  in JK as well).
- Fixed small bug in surface cleave that lead to non-overlapping adjoins
- Added an option to show only visble layers in 3D preview (to speed 
  things up if you need it)
- Added thing wireframe view in editor window
- Added an option to show things in 3D preview
- Added things are now automatically placed at correct distance from the 
  floor.
- Added 3DO preview in Resource Picker
- Added new "create shape" function. Press K, draw a line on the grid
  to define the size and orientation of the shape. Change current shape
  settings on "Shapes" tab in ToolBar window.
- Added a editor for JKL header
- Added basic Dark Forces import (geometry, lighting, objects).
- Fixed bug in removing thing values.
- Added MOTS support
- Fixed bug in Stitching when stitching flipped surfaces
- Added "Export sector as 3DO" funtion
- Added resource checker to Consistency Checker
- Modified Sector cleave to be more correct and stable.
- Fixed a bug that caused "Access Violation at" when picking in 3D Preview.
- Added "Make door" tool
- The things in cleaved sectors are now relayered (reassigned to sectors)
- Added Episode editor
- Extude now adjusts new surfaces to remove small precision errors
- The numbers in scientific notation don't appear in the editor anymore.
- Added "Reload templates" in "Commands" menu
- Added an option to calculate lighting only on visible layers
- Added light flags (not blocked flag)
- "Extrude" size is now relative to the source surface

Version 0.51
- Fixed bug in outputting CMP list to JKL
- Fixed bug that caused a bug in creating COGSTRINGS.UNI that caused JK/MOTS to
  crash when picking a level on "Host game" screen.
- Changed MAT handling a bit (to account for problem with 01narsky.mat)
- Added warning for using multiple CMPs in the level in Resource Checker
- Added "Edit Cog" button to Placed COG editor
- Added an option to use OpenGL for 3D Preview (it works slower, but 
  loads faster. Picking might works funny. You also can't change
  adjust gamma on the fly). Could be helpful in case Direct3D gives 
  you troubles)
- EWEB now works in the level.
- "N" and "P" in 3D Preview now cycle through player starts
- Added "Revert to saved" option - reloads the level from file keeping
  your current editing settings
- Fixed a bug giving "Range check error" if you set "Extra light" on
  surface or sector in a way so that the vertex instensity becomes 
  negative.
- Fixed bug in saving ceiling and horizon sky offsets to .JED files
- Added "TX Scale" field in surface properties. Lets you stretch/shrink
  the texture on the surface.  You can also use Alt+< and Alt+> to scale
  the texture up/down. You also change a step of it on "Texturing" tab
  in "Map Settings" window.
- Added alternative keys for "move perpendicular to the grid" (
  that is "[" and "]" keys). Now you can also do it with Ctrl+Up/down 
  arrow. For people with non-US keyboards.
- Fixed several errors in MOTS templates and added custom template lists
  for JK and MOTS
- Modified adjoining operations a bit so that now the surfaces that
  overlap, but have different number of surfaces are treated as 
  overlapping (it affects adjoining, unadjoining, cleaving adjoined
  surfaces/sectors, merging sectors and consistency checker).
- Changing CMP in the list in Resource Picker now changes the palette
  for a MAT, as it was in Jed 0.3. If you select a CMP different than
  Level's "Master" CMP, the floating hint will say that.

Version 0.8
- Fixed bug with JED showing the orientation of things incorrectly in 3D
  preview and main window.
- Fixed bug with JED not adding placed cogs parameters to the resource 
  lists in JKL
- Added "Planarize Surface" function - "Ctrl+P"
- Changed map controls yet again. Not it's final at last!
- Added multiselection
- Changed JKL and JED saving so that precision errors and texture 
  warping should not appear anymore or at least be less common
- Added recovery tools (delete surface, build surface and sector)
- Elimintated dependence on OpenGL. Now you can run JED without OpenGL.
  However, JED can still take advantage of OpenGL if you have it.
- Added basic 3D Studio .ASC import
- Added "Save JKL, GOB and Test" function - use at your own risk
- Added Frame Mode (Shift+T)
- Layer visibility info is now saved in .JED file
- Improved sector and surface merging to handle sectors/surfaces having
  more than one common edge/surface, pairs of adjoined surfaces, etc.
- Improved surface and sector cleave to handle certain situation better.
- Added 3DO import. Notice that doesn't make JED a complete 3DO editor.
  People asked for it, so here it is. It's only useful only if you know
  what you're doing.
- JED now extracts JK and MOTS directories from registry when you 
  first run it.
- Improved the speed of 3D preview loading a bit (didn't time it, but it
  feels like something in vicinity of 20%).
- If you cleave a surface, the texturing now retained. Also found and 
  fixed a bug that might be responsible for "wiggly" textures.
- Added rectangular multiselection (hold down Alt and draw a rectangle)
- Added "Send 3D Preview to item" tool - sends 3D preview camera to
  item location (invoked by F5). Doesn't work in Surface, Edge and 
  Vertex modes, but does work in sector mode. Also, in 3D Preview you
  can send main view to camera position by using 
  "Commands"|"Set View to Camera".
- Updated "Find XXX" tool to allow adding/subtracting found items to/from
  multiselection
- Fixed bug in refreshing COG parameters when you click "Refresh" in 
  Placed Cogs window.
- Now you can also double-click on COG parameters in Placed COG editor
  to perform appropriate actions depending on parameter type. Here they
  are:
  sound,model,template,mat, ai, cog - invokes appropriate picker.
  thing,sector,surface - selects them in the editor
- Added Copy/Paste
- Improved stitching to handle some more complicated cases.
- Added clean unexpected error reporting to facilitate bug reports and
  allow to pinpoint the place where the error occured.
- The colormap of sectors created by "K" is now copied from a currently
  selected sector.
- Copied sectors now retain texture alignment.
- Fixed a bug in reading Cogstrings.Uni that gave "Access Violation" when
  invoking Episode Editor
- Added "Swap Grid Axis" function (Shift+G)
- Added two buttons to rotate the 3DO preview in Resource Picker
- 3DO Preview now shows highest quality models, not the lowest that
  are displayed in 3D Preview.
- Now you can do some basic editing operations (raising/lowering objects
  deleting thing, extruding surfaces, etc) from 3D preview.
- Added "Texture flipped" surface flag. For flag to take effect you will
  need to straighten the texture on this surface
- Added basic Template Creator
- Fixed bug in importing .LEV, .3DO and .ASC. If you imported them in 
  MOTS mode, the level was marked as JK level, but the editor functioned
  in MOTS mode.
- Added an option to fall back to old map rotation controls.
- Added a bunch of OLE interface functions

Version 0.81
- Added an option to double-buffer the software wireframe renderer 
  (removes that darn blinking).
- When you insert sectors/things/lights if you inserted just one it's
  no longer multiselected.
- "Snap grid to item" now works in frame mode.
- You can now customize 3D Preview movement step.
- Added an option to scale the textures when the geometry is scaled.
- Fixed a bug in sector pasting that didn't release the clipboard after
  pasting is done.
- Fixed a bug in thing copying/pasting
- Added a confirmation before testing level
- JED now accepts JK directory tree in project directory. I.e. - you can
  put, say, .COG files in cog\ subdirectory
- Fixed bug in loading JKLs with things out of sectors giving "Sector 
  index out of bounds -1" error.
- Fixed a bug with 3D Preview not updating the "eternal emptiness"
  areas.
- Fixed bug in GOBbbing/GOOing that put jkstrings.uni,credits.uni, etc.
  to ui\ directory instead of misc\
- Changed "Save JKL, GOB and test" to "Save JKL and Test" - GOBBing is no longer 
  necessary. To test project without Saving a JKL go to project directory and start
  the file "Test_ProjectName.bat" - it will be created the first time to test the 
  level.
- Returned map rotation with R+Mouse and also fixed a bug in it. Works
  both in new and old control mode. The difference is subtle, but there is
  a difference.
- Added "Copy camera as frame" function - "F" in 3D Preview. Copies 
  the current camera position and orientation to clipboard in 
  frame format - i.e. "(x/y/z:pch/yaw/rol)".
- Picking a template in Template creator now sets the "parent" field
  as well as "template" field.
- Shift+Mouse Click now multiselects the item under cursor.
- Now if you have a thing selected in frame mode, you can move it.
  Unlike moving in thing mode, in this case the thing frames are
  not affected.
- "txscale" field of surface is slit in two - "U scale" and "V scale" 
   letting you stretch/shrink texture independently in both
   dimensions.
- Now when you change flipped flag of the surface or Uscale/Vscale,
  the surface is automatically retextured (i.e. - you don't need to
  "straighten it"). 
- When you load a .JKL or a .JED file created by older version of JED
  JED automatically sets "Texture flipped" flag for textures whose 
  textures are flipped.
- "Export as 3DO" and "Import as 3DO" now transfer transparency 
  information on surfaces.
- Added "Set as default" button on Map Settings window to save 
  current map settings (Grid, various steps thing view mode and 
  multiselection mode) and use them as default ones.
- Added an "autosave" option.
- Added some flag descriptions
- Added "Use in COG" function. Select sector, surface or thing in
  the editor and press "U".
- Added "Get selected item" in Placed COG editor that gets the value
  of the currently selected item in the editor into COG's value slot.
- Fixed bug appearing sometimes when adding strings in string editor.
- Added an option to set grid size. Be careful with that - large 
  values can make grid redraw VERY SLOWLY!
- Adjoin/Unadjoin now works on surface multiselections
- Items that are selected AND multiselected are now displayed
  in different color (orange by default) than either selected or
  multiselected.
- JED now asks to save  changes (if they were made) before importing.
- Fixed a bug in "View to Grid" function. I can't believe nobody 
  ever reported it!
- Added "Paste" button on Item Editor that lets you paste X,Y,Z 
  and PCH,YAW,ROL values to current thing/frame copied to clipboard 
  by "Copy As Frame" operation. Or copied to clipboard in any other
  way in standard JK frame format - "(X/Y/Z:PCH/YAW/ROL)" or vector
  format "(X/Y/Z)".
- Fixed a bug in losing grid settings when switching renderers.
- Fixed a bug giving "Index out of bounds" errors on "Revert To Saved"
- Corrected COG parameter handling of type "COG". 
- Added "Toggle Fully lit" menu item - a shortcut to the same thing 
  in the Options
- "Flip" tool now works on things as well.
- Fixed a bug in displaying objects that don't have a 3DO associated
  in 3D preview - they are shown as white 1x1 squares at object 
  location in Direct3D renderer. In OpenGL renderer they aren't 
  shown.
- Modified OLE scripting registration so that JED plugins would only
  work while JED is running (before that launching a plugin while
  JED isn't running would launch JED without giving any control to 
  the user). So now "CreateObject("Jed.App")" will fail when JED 
  isn't running.
- Expanded Plugin interface a bit - you can now supply a description
  file with your plug-in that lets you specify the menu caption and 
  the keyboard shortcut to use (The description file should named 
  plugin_file_name.dsc. I.e if plug-in is Myplugin.exe, the 
  description will be Myplugin.exe.dsc - that's right, two 
  extensions). See JKtree plug-in included with JED.
- Fixed a few bugs that could lead to invalid multiselections.

OLE Scripting:
- Fixed bug in CurLT property
- Added .PickSPR(), .PickPAR(), .PickPER() and .ReloadTemplates methods
  to TJedApp object.
- Picking functions (.PickXXX) now pop up the resource picker in 
  front and return the focus to the calling application when done.
- Added .uscale and .vscale surface properties. Old "txscale" 
  property is still supported, when you read it it returns uscale
  and when you write it it set both uscale and vscale.
- Fixed a bug in TOLESector.GetVertex()
- Added .GetNormal() function and .NormalX, .NormalY and .NormalZ
  properties to TOLESurface to retrieve surface's normal. Note - you
  can't set surface's normal, JED calculates it from surface's data.
- Added TOLECOG interface to access level's placed COGs. Also added
  methods .AddCOG(), .GetCOG() and .DeleteCOG() to TOLELevel.
- Added .PanMessage() method - to add messages to JED's message pan.
- Added .Update and .Release to TOLELight object.
- Added .CurSF, .CurED, .CurVX and .CurFR properties.
- Added .SendKey() and .ExecuteMenu() methods that let you simulate
  pressing a key or picking a menu in JED.
- Added a set of methods to get/set position/orientation of grid
  and camera
- Added a set of methods to get/set position/orientation of 3D 
  Preview camera.
- Added a bunch of OLE functions to access multiselection

Version 0.85
- Fixed a bug giving access violation at times when merging and 
  deleting sectors when 3D Preview is active.
- Fixed a bug of not being able to change map background color
  when using OpenGL renderer.
- You can now have subdirectories in "Plugins" directory which 
  will appear as submenus.
- JED now does not require palettized texture support to use
  OpenGL for 3D preview. However, if your OpenGL implementation
  doesn't support palettized textures (like Riva OpenGL ICD),
  significatly larger amout of memory will be used by 3D Preview.
- Added "Extrude by" function (Shift+X) that lets you specify
  explicitly by how much to extrude.
- Fixed a bug in gobbing .BM files from ui\bm\ subdirectory.
- Added Undo. Enabled by default. Go to Options|Environment to
  disable it. It can go up to 4 steps back.
- Added sa new tab in Options - "Misc. Options" containing some 
  settings that alter minor, yet important details in JED's editing 
  operations. Adjust them to your liking.
  Here are the current options:

  An option to either move or keep frames when you move a thing.
  An option to enabled/disable undo
  An Options to GOB "smart" - i.e. only put files that are used
  in any of project's JKLs, items.dat or models.dat. Be careful
  as enabling it slows down GOBBibg quite a bit and may actually
  miss some things.

- Added two scrollbars to rotate and tilt 3DO view and the old
  left/right buttons are removed.
- Added "Grid Move step" setting in Map Settings, that controls
  how the grid moves with G+Mouse. For istance setting it to
  0.5 will make grid take only X,Y,Z positions with X,Y and Z
  which are multiples of 0.5: 0.5, 1, 1.5, etc.
- Fixed a bug in 3D Preview thing selection in OpenGL mode
- "Add Value", "Remove value" buttons on Item Editor now work
  on multiselections. Be careful doing that though.
- JED will now look for Res1hi.gob/JKMsndLO.goo files on CD if they
  are not found on HD. This lets you perform the following trick:
  Specify incorrect JK/MOTS directory, JED will still work fetching files
  from CD, so if you have full of JK/MOTS install, you can run JK/MOTS
  and JED simulteneously without conflicts in file access (JK/MOTS 
  will use files from HD, JED - from CD).
- Surfaces with light mode 0 (fully lit) are now displayed as such
  in 3D Preview
- The values for edge starting/ending vertex now show vertex index in
  the sector.
- Surface merging is improved to handle some complex cases.
- Fixed a bug in edge cleaving.
- Added a "make a backup copy" option. Makes a copy of your
  current .JED file in "backup" subdirectory of your project 
  directory with a name "filenameXX.jed", where XX is 2 digit
  number increasing each time you make a backup.

OLE
- Added .EditXXXFlags functions to invoke flag editor dialogs.
- Added .PickGeo/Tex/lightmode functions to invoke flag editor 
  dialogs to invoke the corresponding dialogs.
- Added TOLESurface.Nvertices property. Can't believe something 
  like this was left out for 2 version!
- Added .NLayers property to TOLELevel.
- Added .LevelFile property to TJedApp
- Added a set of procedures to manipulate Undo.
- Added .GetJEDSetting property to retrieve JED's settings. Just 
  in case.
- Added a set of OLE functions to access templates (as a new object
  TOLETemplates).

Version 0.9
- Consistency checker now doesn't report "Sector not convex"
  error for sectors with "Show as 3DO" flag set - i.e. the
  sectors that are to be exported as 3DOs, not part of level
  geometry.
- Redone 3D Preview low level code.
- Added overlapping sector detection in consistency checker.
  Disabled by default (as it slows things down), you can
  enable it in "Options|Misc. Options".
- Fixed a bug in undo not undoing sector deletion completely
  (surfaces adjoined to the deleted sectors weren't restored).
- Fixed a bug of undo information not being saved for
  some F9-Tools operations.
- Fixed a bug that made small surfaces color-filled when 
  using texturing tools.
- Redone lighting calculation (sped up about 4 times). Kept
  an option to revert to the old lighting calculation for 
  compatibility.
- You can now add/subtract things and surfaces to/from 
  multiselection in 3D Preview by Shift+Mouse Click. It is
  an equivalent of selecting this objects and pressing space.
  I.e. - it is affected by "Multiselection mode" setting.
- Added an option to rotate/flip around Grid in F9 Tools.
- Added a "Snap View to Object" (Alt+S) option - centers 
  the view on the selected object.
- When you click on surface/thing in 3D preview it, the main
  view is no longer centered on the selected object. Use 
  "Snap view to Object" if you want to center it.
- Fixed a bug causing "Invalid pointer operation" when 
  performing operation on things with large (10 and more)
  number of frames.
- Added "Join" function - position two surfaces back to 
  back, select them both and press "J" - they will be 
  cleaved and their common area will become a pair of
  adjoined surfaces.
- Added "Extrude And Expand" feature - Ctrl+X.
- Added "Bring thing/light To Surface" function. Select a surface,
  then switch to thing mode, select a thing ( or to light mode
  and select a light) and press "B". The thing/light will be
  brought to the middle of the surface and offset to the 
  appropriate distance from it (0.01 units for lights and
  half of thing's bounding box for things).
- Added "Insert thing at surface" function to 3D Preview.
  Select a surface in 3D preview and press "I" - a thing
  will be placed at the middle of it.
- Added and option to confirm "Revert to Saved" function.
- JED now prints a warning when you form non-convex or
  non planar surfaces and non-convex sectors by merging.
- Added "Layer" field in "Find Surface" dialog.
- 3DO export and 3DO import are changed slightly:
  Export:
  - The meshes are now named by the layer names.
  - Sectors in the same layer are merged into one mesh.
  - Adjoined surfaces are not exported.
  Import:
  - Each mesh is imported to a layer named like the mesh.
  - "Preview as 3DO" flag is set on imported sectors.
- Added "Prep" button in F9 Tools rotation dialog. The way it
  works is you select two surfaces (multiselect one and select
  another) and press this button, JED calculates the rotation
  parameters for a rotation that would place these two 
  surfaces in one plane. After this preparation you only
  need to select the sectors to rotate and press "Rotate".
- Added "Jump to Object" function that lets you jump to a 
  specific obejct (sector, surface,etc) by its number. You
  can also jump to surfaces by their absolute number (their
  number in JKL) - you just need to enter one number instead
  of two.

OLE Scripting:
- Added .IsConvex function to TOLESector. Returns true if 
  sector is convex and false otherwise.
- Added .IsConvex and .IsPlanar functions to TOLESurface. 
  Returns true if  sector is convex/planar and false 
  otherwise.
- Added a bunch of OLE functions to retrieve consistency
  errors.
- Added function to display some additional data in JED 
  window.

Version 0.91
- Template creator now allows you to replace existing 
  templates (be careful when doing that).
- Changed 3DO preview slightly so that its failure to 
  initialize doesn't disable template/3DO pickers.
- Fixed bug causing 3D Preview go black when you look 
  straight up or straight down.
- "Multiple" now appears after width/height info
   for multiple MAT files in Resource Picker.
- Fixed a bug giving "not enough memory for Z buffer" error
  after several reloadings of 3D Preview.
- Fixed a bug displaying wrong textures in 3D Preview
  when using OpenGL.
- Fixed a bug that caused thing orienation to display 
  incorrectly in 3D Preview (this bug was fixed before, 
  but somehow crawled back in).
- "Bring thing to surface" now changes PCH,YAW and ROL of the
  thing so that it "stands" on slanted surfaces.
- Added an option to make Placed COGs window stay on top.
- Changed Keyboard menu a bit to fit 640x480 screen.
- Added DLL plug-in interface (old OLE still works).

Version 0.92
- When you click "Edit COG" in Placed COG editor on a COG
  that's inside a GOB file, JED now offers you to extract
  it to project directory to edit it.
- When you preview a multiple MAT in Resource Picker, a
  scrollbar will now appear that will let you scroll through
  all cells in the file.
- Now you can use rotate,flip and scale F9 tools on things, 
  lights and frames as well as sectors.
- By pressing Shift+I in 3D Preview you can now insert a 
  thing at camera position.
- "Join" function now works on sectors as well as surfaces.
- Fixed an omission in DLL plug-in interface that made it
  impossible to make DLLs using some programming languages.
- Added a lot of DLL functionality
- Template Creator now reloads template file before editing,
  making sure that you don't lose any changes you've made
  indepenedently to the .TPL file.
- Added "Duplicate" button to Placed COG editor that lets
  you place another copy of the currently selected COG. 
  Useful for doors and such. Also, Placed cog editor screen
  updates are sped up somewhat.
- When you change a template in Template Creator all the 
  things that use it are now updated.
- Fixed a bug causing plug-ins written in Delphi 3.0 cause
  access violation on exit from JED.
- The plugin interface unit is now the same for Delphi 2.0
  and 3.0

Version 0.93
- Fixed a bug in extracting .COG from GOB/GOOs when you try to
  edit a .COG that's in the game's GOB/GOO files.
- F9 Scale function is enhanced to allow scaling along X,Y,Z or
  Grid Z axis individually, not just in all directions.
- Holding Alt while cycling through possible items under cursor (
  i.e. - when you click the left mouse button several times without 
  moving the mouse) now cycles backwards.
- You can now change the CMP used as palette when you view a 3DO in 
  Resource Picker.
- JED now shows a circle designating radius for a selected light.
- Fixed a bug in .SurfaceUpdate() DLL interface function.
- Fixed a bug of not being able to enter capital S in layer name
- JED now invokes an "Add COG" dialog when you click on a COG's variable
  of type "COG" that equals -1.
- Added an option to hide things/lights when not in thing/light mode.
- Holding "Ctrl" and "Alt" while dragging objects now limits its transformation
  to one grid axis. Use "Ctrl+Alt+Z" (press Z while dragging) to toggle between
  snapping to X and Y grid axis.
- You can now manipulate shapes (prefabs) - import them from sectors and
  delete them.
- The last open file in the recent file list now moves on top of the list
- Pressing + and - to adjust gamma in 3D Preview in OpenGL and Direct3D IM mode 
  doesn't display warning message about having to reload 3D preview, instead
  reloads it for you. Note that it can take some time.
- Added "Blocks Light" adjoin flag that tells JED to treat the adjoin as an opaque
  surface in lighting claculation.
- Fixed bug in .GetLight() function of DLL interface.
- Fixed bug in .PickResource() plugin function

Version 0.94
- Fixed bug that caused not being able to multiselect frames by holding Alt and
  dragging a rectangle over them.
- Fixed a bug that created invalid adjoins when you cleave an adjoined surface and then
  undo it.
- Fixed bug in undoing stitching.
- You can now delete vertices of adjoined surfaces. But watch out - JED won't delete
  the corresponding vertex of the adjoined surface for you so you might end up creating
  an invalid adjoin.
- Fixed a bug that sometimes caused windows to appear outside of screen if you start
  JED after reducing screen resolution.
- Eliminated a number of memory leaks
- Now when you delete a COG in "Placed COG editor" the COG references to other COGs are 
  updated.
- Added "vertex merge" function - select duplicate vertices (vertices that are on the 
  same spot) in vertex mode and press "M". You can't, of course, merge vertices of
  different sectors. JED does it for you anyway when you save to JKL.
- Fixed a bug in edge cleaving that lead to incorrect undo (and possibly other things).
- Now when you duplicate a COG in placed COG editor the cog values are also copied.
- Vertex deleting now works on the multiselection.
- Fixed a problem with textures jumping all over the place when trying to rotate them on
  some surfaces.

Version 0.95
- Fixed an access violation that sometimes happened on exit from JED.
- Added Hierarchy support for 3DO export/import (including a Hierarchy editor).
- Texture U and V scale are now properly calculated when importing 3DOs (
  which means you can now easily retexture imported 3DOs).
- Added support for 16 bit MATs.
