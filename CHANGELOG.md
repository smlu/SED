# Change Log
## Version 0.2.1
- Fixed the flag editor to allow simultaneous selection of water and lava surface flags, enabling the creation of 'cold water' surface types.
- Changed keyboard shortcut for View-To-Grid command in the Map Editor [PR #36](https://github.com/smlu/SED/pull/36) 
- Updated keyboard key commands in 3D Preview [PR #36](https://github.com/smlu/SED/pull/36) 

## Version 0.2.0
- Fixed division by zero in `FindUVScales` function.
- Fixed calculating yaw & roll angles in `TMatrix43.ExtractPYR` function.
- Fixed scaling color component when recoding MAT texture pixel to destination format.  
  This fixes the issue with the color component being scaled to 0xFF when recoding from 16-bit to 24-bit or 32-bit format and vice versa.  
  In essence, it affects the texture color, especially the alpha channel where solid textures with alpha channel were not fully opaque due to now fixed scaling issue.
- Replaced unit `GLunit` with `dglOpenGL` to support OpenGL up to version 4.6.
- Removed obsolete and unused code & units.

- Map Editor
  * Deprecated software renderer in favor of OpenGL renderer.
  * Renamed renderer units

- 3D Preview:
  * Implemented horizon sky rendering in 3D preview (OpenGL only) [PR #35](https://github.com/smlu/SED/pull/35)
  * Added selection no-clip option to 3D preview settings. [PR #28](https://github.com/smlu/SED/pull/28)
  * Fixed acquisition/release of OpenGL context in `TOGLPRenderer` class.  
  * Fixed thing/surface selection in 3D preview when OpenGL is used.
  * Fixed 'Snap Grid to Item' and 'Cleave by Grid' hotkeys [PR #31](https://github.com/smlu/SED/pull/31)
  * Fixed ceiling sky depth to fall behind the level geometry and thing objects.
  * Deprecated DirectX GAPI: The DirectX Graphics API is being deprecated in favor of OpenGL.  
    The DirectX GAPI will be removed in the next major release.
  * Fixed incorrect sort range when flushing cached polygon
  * Fixed transparency rendering issues by improving distance calculation for sorting transparent polygons.  
    Now using full 3D world space distance from vertex to camera position instead of comparing vertices forward (y-axis) values,  
    which ensures correct rendering of overlapping transparent objects, especially in OpenGL.
  * Fixed rendering of translucent polygons in OpenGL by enabling alpha testing.
  * Updated OpenGL preview renderer to use curCamera FOV.
  * Refactored OpenGL renderer to replace all immediate‑mode (glBegin/glEnd) draws with batched dynamic VBO/IBO updates and single glDrawElements.
  * Renamed renderer units


## Version 0.1.0
- Added support for game Indiana Jones and the Infernal Machine (IJIM)
- Ported to latest Delphi version (unicode support)
- Added indication in window title bar when level is changed (asterisk char '*' is added at the end of filepath)
- Added option to import 3DO as sector to current project
- Added support for 24/32 bit MATs
- Added support to rotate texture UVs of multiple selected surfaces
- Added command to stitch & copy surface properties (Ctrl + stitch key(s))
- Added warning messages when trying to build sector/surface and selected surfaces/edges/vertices doesn't belong to sector
- Added new struct TColorF which represents normalized RGBA color
- Added TVector, TVector2 and TMatrix classes which implements vector and matrix arithmetics
- Added live update of Item editor when item is changed in map editor / 3D preview
- Added templates merge from level on level load
- Added multi-selection count
- Added paste in place command (Ctrl+Shift+V) which pastes from clipboard objects at their copied (original) position
- Editor configuration is now stored in sed.ini and not in windows register
- Changed default behavior to require confirmation before reverting to the saved state. Removed shortcut Ctrl + R
- Fixed adjoin surface properties when undoing sector changes
- Fixed saving surface info for adjoin surface when undoing sector cleaving.
  Due to call to DeleteSector in the undo stack the adjoin parameters were reset to solid surface.
- Fixed adjoin cleaving to leave adjoin surface properties unchanged (e.g.: not resting faceflags/surfflags to 0, not setting geomode to 0 etc..)
- Fixed bug when pasting from clipboard a non-thing data as thing which resulted in locking system clipboard
- Fixed crash in extra mode when no extra object is available
- Fixed layering thing when snapping to surface (bring thing to surface)
- Fixed surface material undo after stitching surface
- Fixed saving the correct sector to undo history before adjoin is changed in `RemoveSurfRefs`
- Fixed rotating multiple objects over pivot
- Fixed sector geo undo when sector is first changed then deleted in the same undo frame.
  This fixes undoing sector join.
- Fixed surface adjoin parameters when pasting sector from clipboard
- Fixed sector(s), light(s) and thing(s) position offset when pasting from clipboard
- Fixed left and right rotating of texture UVs
- Fixed selecting new item on visible layer after current item is deleted.
- Fixed to not highlight last deleted item
- Fixed various out of bounds reads
- Fixed merged surface(s) max vertices
- Fixed calculating surface normals
- Fixed sector values change undo on texture UV scale
- Fixed update on state change (game object, sector, surface etc..)
- Fixed progress dialog to not show up when app is not active
- Fixed resetting auto-save timer
- Fixed returning success when cleaving sector succeeds
- Fixed emitting sector changed notification when adjoining surfaces (MakeAdjoinSCUP)
- Fixed thing undo when adding new thing onto current surface
- Increased undo steps to 1024
- Optimized texture straightening algorithm to perpendicular align texture UVs
- Object editing is immediately updated in the map editor / 3D preview
- Refactored code to use RGBA colors internally
- Removed Direct3DRM
- Removed requirement for game CD (JKDF2 & MOTS)
- Updated UI and enabled per monitor DPI awareness
- Updated SPrintf to allow 0 left padding in the format (max len 9). e.g. "%1.2f" will format 0.25326 as 0.25.

- 3D Preview:
  * Added support for mouse navigation
  * Added Thing rotation via Ctrl + mouse wheel
  * Added Thing translation via Shift + W/A/S/D
  * Added command to move camera to item position (F5)
  * Added command to move Thing to camera position
  * Added command to bring Thing to surface (B)
  * Added commands extrude (Shift + X) and extrude & expand (Ctrl + X)
  * Added command merge (M)
  * Added command join (J)
  * Added command to clear multi-selection (Backspace)
  * Added command to select camera sector (Alt + S)
  * Added thing baked light rendering when light radius <= 0.01 (IJIM)
  * Added fog rendering (IJIM)
  * Added option to highlight selection and optionally surfaces with special properties (solid floors, cog linked surfaces etc...)
  * Fixed showing/hiding sectors and objects visible/hidden layer
  * Fixed selection of rotated model
  * Fixed model selection when clicking on its polygon face with the double-sided face flag set.
  * Implemented loop play of sector sound while camera is in sector
  * Implemented original triangulation algorithm
  * Implemented original rendering of sector tint color (JKDF2 & MOTS)
  * Implemented original vertex color blending (missing dynamic lights)
  * Render texture UV clamping (IJIM)
  * Render ceiling sky
  * Render 3DO model at highest level of detail (LOD 0)

- Consistency Checker:
  * Highlight warning & error entries
  * Added texture geomode check for solid surface
  * Added floor collision flag check
  * Added check for track surface (IJIM)
  * Added adjoin check for visible flag to be set when surface geomode is set to not drawn or translucent faceflag is set
  * Added total issues counter
  * Fixed current selected item when check is refreshed

- Item Editor:
  * Save undo when pasting frame to item
  * Save frame undo on change
  * Live update when current item changes in map editor/3d preview
  * Fixed selecting corresponding linked COG symbol in Placed COG editor
  * Sector:
    - Added fields:
      + ambient light
      + avg. light intensity (IJIM)
      + avg. light position (IJIM)
      + avg. light falloff (IJIM)
      + thrust (IJIM)
  * Surface:
    - Select the mirror adjoin surface when double-clicking the adjoin field
    - Fixed saving sector geometry undo on UV scale change or flip faceflags set/clear
  * Vertex:
    - Added vertex coords copy/past button
  * Thing:
    - Added color pick option for `light` & `lightintensity` param (IJIM)
    - Added sector selection on double click sector field
  * Light:
    - Added button for setting light to sector point light to selected sector(s) (IJIM)

- Flags Editor:
  * Sector:
    - Auto set/clear surface flags 0x80 - aetherium and 0x20000 - water for all sector surfaces when sector flags 0x2 - underwater and 0x100 - aetherium is set/cleared.
  * Surface:
    - Added IJIM surface flags and rename existing JKDF2 flags
    - Auto set collision surface flag when floor surface flag is set, and unset floor surfflag when collision flag is unset.
  * Thing:
    - Added IJIM thingflags and rename existing JKDF flags

- Map Editor
  * Added experimental perspective camera projection to map editor (shortcut: Num 5)
  * Added undo step when calculating lighting
  * Added support for mouse navigation & zooming
  * Added support for mouse wheel to PYR rotate selected Thing object(s)
  * Added thing & frame selection by mesh wireframe
  * Cleave sector(s) from any angle without needing to align with grid
  * Fixed calculating lighting on currently selected sector/surface/edge/vertex
  * Fixed cleaving in normal mode when view is parallel to the grid
  * Fixed scaling texture when in map editor
  * Fixed snap to vertex
  * Implemented snap to XY axis (Alt) and snap to grid (Ctrl) cleaving mode
  * [OpenGL] Implemented wireframe culling
  * [OpenGL] Changed wireframe rendering to use line loop instead of triangles
  * Prevent cleaving level geometry on invisible layer.
  * Removed command to calculate lighting for sectors on non-visible layers.
    Now light data can be calculated only on sectors that are on visible layer.
  * Render dashed object(s) on hidden layer
  * Set OpenGL as default wireframe API

- Placed COG editor
  * Fixed item selection on list refresh
  * Fixed updating COG form when selected item is assigned

- Options Window:
  * Added option to store lights data and layer info in JKL/NDY file
  * Added option to change cleave line color (default green)
  * Added color selection options for 3D preview window and cleave line
  * Added options to set display size of vertex
  * Added options to invert wireframe polygon cull coloring
  * Added option to set max vertex snap distance
  * Added option to lock map zoom in/out
  * Added option to enable/disable playing of sector sound in 3D Preview
  * Added new window resolutions for 3D Preview

- Recovery menu:
  * Added command: 'Fix solid surface(s)' which auto sets collision and floor surface flags to all selected surface(s)/sector(s)
  * Added command: 'Fix passable adjoin(s)' which sets not drawn geo mode (0) to all selected surface adjoin(s) that  can be passed through.

- Tool Window:
  * Added surface(s) rotation
  * Added surface(s) scale