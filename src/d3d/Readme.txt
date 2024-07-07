DirectX 5.0 for Delphi
~~~~~~~~~~~~~~~~~~~~~~
Version 1.7c (20.4.98) written by Erik Unger

Wow, It's done :-)
The conversion of the DirectX 5.0 API for Delphi 2/3 is complete !

I haven't converted FastFile, because it's not an
essential part of DirectX.
I also havn't converted DirectShow and the
other APIs of DirectX 5.2 yet.

Delphi 2 and 3 COM syntax:
~~~~~~~~~~~~~~~~~~~~~~~~~~
All units can be used with Delphi 2 and Delphi 3.
Because Delphi 3 has a new syntax for COM-interfaces,
I have implemented a mechanism that allows you to choose
between the old Delphi 2 and the new Delphi 3 COM-syntax.
(See Delphi documentation for more information about
COM-interfaces)
If the compiler-symbol D2COM is defined in the file
COMSWITCH.INC (that all units include), then the old
syntax (which is compatible with Delphi 2 and 3) is used.
Else you need Delphi 3.x to compile the units.
You can use the AddCOM and ReleaseCOM functions of DXTools
for version-independent COM managment.

UNICODE and ANSI strings:
~~~~~~~~~~~~~~~~~~~~~~~~~
Some DirectX-objects can handle UNICODE and ANSI strings.
The names of interfaces and records that use PWideChar UNICODE-strings
end with a 'W' and their ANSI-pendants with an 'A'.
You can select UNICODE as default stringtype by defining
the compile-symbol UNICODE in the file STRINGSWITCH.INC.
If UNICODE is not defined, then ANSI is the default stringtype.
The default stringtype is used, when you use interface
and record-names that don't end with A or W.
(In some cases that's different to the original headers, that
use only 'Name' and 'NameA', where 'Name' is the UNICODE version)

Delphi look and feel typenames:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

I have modyfied the names of records, enumerated-,
and other types, so that they look like 'default Delphi' typenames.
Here is a list with representative examples:

                  | C                   | Delphi
------------------+---------------------+----------------------
Record types      | D3DCOLORVALUE       | TD3DColorValue
Pointer types     | LPD3DCOLORVALUE     | PD3DColorValue
Enumerated types  | D3DLIGHTTYPE        | TD3DLightType
Enumerated Values | D3DLIGHT_SPOT       | D3DLIGHT_SPOT *
Constants         | D3DENUMRET_OK       | D3DENUMRET_OK *
Interfaces        | LPDIRECTDRAW2       | IDirectDraw2
Procedureal types | D3DVALIDATECALLBACK | TD3DValidateCallback

* no changes

Helper unit DXTools:
~~~~~~~~~~~~~~~~~~~~
The unit DXTools gives you some useful tools that
make DirectX-coding a little bit more handy.
The procedure which you will use very often is DXCheck.
It takes a HResult value returned by a DX-call
as argument, and raises an exception with a short description
of the error if necessary. DDCheck, D3DCheck, ... will do this
only for a particular DirectX API.
The procedure InitRecord fills a record with zero, and writes
the recordsize into the first DWord.
ReleaseCOM releases Delphi 2 and 3 COM-interfaces
(and sets the pointer to nil), even when they are not initialised.
ReleaseCOMe does the same, but raises an excepten
when the interface is not initialised.
DXTools has also some matrix-functions for 3D applications:
ProjectionMatrix and ViewMatrix calculate homogenius-matrices
for your virtual camera.
With the class TStack you are able to implement a state-stack
(for vectors, matrices and other state-values) to make Direct3D 
a little bit more like OpenGL.

Copyrights and JEDI:
~~~~~~~~~~~~~~~~~~~~
I have no copyrights on the units, because the
original headers are copyright by Microsoft.

So use them like freeware :-)

I am a member of the JEDI-Project (a non-profit organisation that
adapts Microsoft API's for Delphi <http://www.delphi-jedi.org>),
therefore these units are the official JEDI-DirectX API.

Contact me!
~~~~~~~~~~~
It would be great to get some feedback from you.
Tell me what you tink about the conversion,
if there are problems and what could be done better.
Thanks in advance for your bug-reports :-)

My E-Mail address is: <h_unger@magnet.at> 

Files and Download:
~~~~~~~~~~~~~~~~~~~
You can download the file DelphiDX5.zip from my Delphi Graphics Homepage
<http://iguan.magnet.at/users/h_unger/DelphiDX5.zip>.
It contains the following files:

D3DTypes.pas     Direct3D Immediated Mode
D3DCaps.pas      Direct3D Immediated Mode
D3D.pas          Direct3D Immediated Mode
D3DRMDef.pas     Direct3D Retained Mode
D3DRMObj.pas     Direct3D Retained Mode
D3DRM.pas        Direct3D Retained Mode
D3DRMWin.pas     Direct3D Retained Mode
DDraw.pas        DirectDraw
DInput.pas       DirectInput
DPlay.pas        DirectPlay
DPLobby.pas      DirectPlay Lobby
DSetup.pas       DirectSetup
DSound.pas       DirectSound
DVP.pas          DirectVideoPlay
DXTools.pas      Helper-unit for DirectX
COMSWITCH.INC    Includefile to switch COM syntax
STRINGSWITCH.INC Includefile to switch stringtype
Readme.txt       This file
DelphiDX5.htm    HTML-Readme

Credits:
~~~~~~~~
I want to thank
Blake Stone for his initial DX3 conversion, which was the
base for my first steps,
David Sisson for his generic conversions with PERL,
Hiroyuki Hori whose DX5 conversion was helpful in some cases
the JEDI graphics teamand and many other guys for bug-reports and fixes.

Version Changes:
~~~~~~~~~~~~~~~~

Changes from 1.7 to 1.7c
~~~~~~~~~~~~~~~~~~~~~~~~

1. Fixed DLL-name in DPLobby.pas
2. Changed parameter of:

DirectPlayLobbyCreate
IDirectPlayLobby.GetConnectionSettings
IDirectPlay3.CreatePlayer

3. Added error-managment functions in DXTools:

function D3DErrorstring(Value: HResult) : string;
function DDrawErrorstring(Value: HResult) : string;
function DInputErrorstring(Value: HResult) : string;
function DPlayErrorstring(Value: HResult) : string;
function DSetupErrorstring(Value: HResult) : string;
function DSoundErrorstring(Value: HResult) : string;

procedure D3DCheck(Value: HResult);
procedure DDCheck(Value: HResult);
procedure DICheck(Value: HResult);
procedure DPCheck(Value: HResult);
procedure DSCheck(Value: HResult);
procedure DSetupCheck(Value: HResult);

4. Fixed TDSEnumCallback

Changes from 1.6 to 1.7
~~~~~~~~~~~~~~~~~~~~~~~

1. Added units:

DPlay.pas
DPLobby.pas
DVP.pas
DSetup.pas

3. The global c_dfDIxxxx variables in DInput are initialised with default values
2. Better UNICODE-ANSI managment

Changes from 1.5d to 1.6
~~~~~~~~~~~~~~~~~~~~~~~~

1. Added DirectInput unit

The unit is nearly complete, but there are some questions open:

Knows anybody how the global variables
  c_dfDIMouse : TDIDataFormat;
  c_dfDIKeyboard : TDIDataFormat;
  c_dfDIJoystick : TDIDataFormat;
  c_dfDIJoystick2 : TDIDataFormat;
are initialised?
In the C++header they are defined as extern constants:
  extern const DIDATAFORMAT c_dfDIMouse;
  extern const DIDATAFORMAT c_dfDIKeyboard;
  extern const DIDATAFORMAT c_dfDIJoystick;
  extern const DIDATAFORMAT c_dfDIJoystick2;
Are they imported from dinput.lib or dinput.dll? How?

The method SendDeviceData of the interface IDirectInputDevice2
is defined in the C++header, but it's not described in the DirectX-Helpfile.

    function SendDeviceData(Arg1: DWORD; var Arg2: TDIDeviceObjectData;
        var Arg3: DWORD; Arg4: DWORD) : HResult; //?

3. New includefile STRINGSWITCH.INC:

If you want to use Unicode as default stringtype,
define the compiler-symbol UNICODE in it: {$DEFINE UNICODE}

4. Added dwRGBZBitMask & dwYUVZBitMask in TDDPixelFormat
5. Added dwLinearSize in TDDSurfaceDesc

6.The new version of DDRAW.PAS allows the possibility to load the DDRAW.DLL file indirectly,
so an application can be used also when no DDRAW.DLL is available. To enable this option,
define the INDIRECT at the beginning of the DDRAW.PAS file.
When defined, a global variable DDrawLoaded is defined which is set to true if the
DDRAW.DLL has been loaded. If false the functions are not available.
When INDIRECT is defined, all functions are declared as function variables but you don't
need to change anything you source files only rebuild everything.
A last note, the DDRAW.DLL is not loaded when the application is DELPHI32.EXE and the OS
is NT (this caused an DDRAW.DLL initialization error). See the end of DDRAW.PAS.
Thanks to Josha :-)

7. The Retained-Mode example D3DBrowser uses the latest DirectX 5 units now.

Changes from 1.5 to 1.5d
~~~~~~~~~~~~~~~~~~~~~~~~
1. TRect params redefined as PRect to allow nil as parameter
2. Matrices.pas is integrated in DXTools.pas
3. Added class Stack in DXTools
4. Added CI_ and RGB_/RGBA_ functions in D3DTypes
5. Added MAKE_DSHResult function in DSound
6. Renamed TColor in DXTools to TTrueColor
7. Fixed VectorCrossProduct bug in D3DTypes
8. TDDEnumCallback uses PGUID for lpGUID parameter
9. Added D3DDEVCAPS_CANRENDERAFTERFLIP in D3DCaps
10. Fixed IDirectSoundNotify interface definition

Changes from 1.4 to 1.5
~~~~~~~~~~~~~~~~~~~~~~~

1. Added DirectSound Unit
2. Delphi3 native COM should work now

Changes from 1.3d to 1.4
~~~~~~~~~~~~~~~~~~~~~~~~

New versions of DD and D3D headers:
1. Delphi 3 native COM-syntax support
2. More Pascal-like syntax
3. Many bugs fixed

Changes from 1.3c to 1.3d:
~~~~~~~~~~~~~~~~~~~~~~~~~~

1. Added constant DDLOCK_NOSYSLOCK
2. Fixed params of Blt, BltFast and Flip (Surface2 & 3)

Changes from 1.3a to 1.3c:
~~~~~~~~~~~~~~~~~~~~~~~~~~

1. DXTools.DXErrorString reports D3D 5 errors
2. New procedure InitDXRecord in DXTools
3. New TDisplaymode & TDisplaymodes classes in DXTools
4. TD3DTexture should work now
5. Fixed IDirectDrawSurface2 & 3 interfaces

6. New Versions of DPlay.pas and DPLobby.pas from David Sisson

Changes from 1.3 to 1.3a:
~~~~~~~~~~~~~~~~~~~~~~~~~

1. Fixed result.z of the vectorfunctions
2. Fixed IDirect3DMaterial2.GetHandle
3. New perl-generated versions of DSound, DPLobby and DPlay

Changes from 1.2c to 1.3:
~~~~~~~~~~~~~~~~~~~~~~~~~
1. Added David Sissions automatic
   generated units:

DSetup.pas
DSound.pas
DPlay.pas
DPLobby.pas
DInput.pas
DVP.pas
FastFile.pas


Changes from 1.1c to 1.2c:
~~~~~~~~~~~~~~~~~~~~~~~~~~
1. Added DXTools unit (alpha).
2. GetTransform and other Bugs fixed

Changes from 1.0ß to 1.1c:
~~~~~~~~~~~~~~~~~~~~~~~~~~
1. Added 'matrices' unit !

2. Fixed D3DRMMATRIX4D bug

3. Added functions for vector-manipulation (in d3dtypes)
that are defined with the D3D_OVERLOADS symbol in C++

The names are different from the C++ versions,
because Delphi doesn't support operator-overloading:

    // Addition and subtraction
  function VectorAdd(v1, v2: D3DVECTOR) : D3DVECTOR;
  function VectorSub(v1, v2: D3DVECTOR) : D3DVECTOR;
    // Scalar multiplication and division
  function VectorMulS(v: D3DVECTOR; s: D3DVALUE) : D3DVECTOR;
  function VectorDivS(v: D3DVECTOR; s: D3DVALUE) : D3DVECTOR;
    // Memberwise multiplication and division
  function VectorMul(v1, v2: D3DVECTOR) : D3DVECTOR;
  function VectorDiv(v1, v2: D3DVECTOR) : D3DVECTOR;
    // Vector dominance
  function VectorSmaller(v1, v2: D3DVECTOR) : boolean;
  function VectorSmallerEquel(v1, v2: D3DVECTOR) : boolean;
    // Bitwise equality
  function VectorEquel(v1, v2: D3DVECTOR) : boolean;
    // Length-related functions
  function VectorSquareMagnitude(v: D3DVECTOR) : D3DVALUE;
  function VectorMagnitude(v: D3DVECTOR) : D3DVALUE;
    // Returns vector with same direction and unit length
  function VectorNormalize(v: D3DVECTOR) : D3DVECTOR;
    // Return min/max component of the input vector
  function VectorMin(v: D3DVECTOR) : D3DVALUE;
  function VectorMax(v: D3DVECTOR) : D3DVALUE;
    // Return memberwise min/max of input vectors
  function VectorMinimize(v1, v2: D3DVECTOR) : D3DVECTOR;
  function VectorMaximize(v1, v2: D3DVECTOR) : D3DVECTOR;
    // Dot and cross product
  function VectorDotProduct(v1, v2: D3DVECTOR) : D3DVALUE;
  function VectorCrossProduct(v1, v2: D3DVECTOR) : D3DVECTOR;


Have fun,
         Erik Unger.











Legal Disclaimer
~~~~~~~~~~~~~~~~

ANY USE BY YOU OF THE SOFTWARE IS AT YOUR OWN RISK. THE SOFTWARE
IS PROVIDED FOR USE "AS IS" WITHOUT WARRANTY OF ANY KIND. TO THE
MAXIMUM EXTENT PERMITTED BY LAW, THE AUTHOR (ERIK UNGER)
DISCLAIMS ALL WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
INCLUDING, WITHOUT LIMITATION, IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. THE AUTHOR (ERIK UNGER) IS NOT OBLIGATED
TO PROVIDE ANY UPDATES TO THE SOFTWARE.
