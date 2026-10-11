# Third-party software and distribution notices

SED source code is licensed under the GNU General Public License version 3 in `LICENSE`.

## Qt

The desktop application uses Qt Widgets, OpenGL, OpenGLWidgets, Concurrent, and Multimedia. Qt is not vendored in this
source repository. Qt is available under commercial terms and under applicable open-source terms, primarily LGPLv3 or
GPLv3 depending on the module and build. SED itself is GPLv3; using or redistributing Qt still requires compliance with
the exact licenses supplied with the chosen Qt distribution.

Official references:

- https://doc.qt.io/qt-6/licensing.html
- https://doc.qt.io/qt-6/deployment.html
- https://doc.qt.io/qt-6/licenses-used-in-qt.html

For a Windows Qt 6 build, the CMake target invokes `windeployqt` when the selected kit provides that deployment
tool; it may copy Qt shared libraries and plugins beside `sed.exe`. Qt 5 builds and packages assembled without that
target still require an explicit deployment step. A binary release maintainer must inspect the actual staged files and
include the corresponding Qt license texts, copyright notices, third-party notices, and source/relinking information
required by that Qt distribution. Do not assume that this repository-level notice, or the package-layout test, is
sufficient for a binary package. Static Qt linking requires a separate compliance review and is not the default release
configuration.

Qt Multimedia deployments can include separately licensed backends and codecs. Include notices only for components
actually shipped, using Qt's installed license files or software bill of materials as the source of truth. The staged
package must contain the notices that correspond to the exact Qt kit and multimedia backend files it ships; the
repository notice does not substitute for those kit-specific texts.

## Windows Visual C++ runtime

Windows Release packages use official app-local Visual C++ runtime DLLs from the selected MSVC toolchain beside
`sed.exe`; do not substitute arbitrary `System32` copies. Determine the imports of `sed.exe` and its dynamically loaded
dependencies, then use the toolchain's supported Redistributable files and `Redist.txt`/REDIST list for the target
architecture and toolset. See [Microsoft's DLL redistribution guidance](https://learn.microsoft.com/en-us/cpp/windows/determining-which-dlls-to-redistribute?view=msvc-170)
and the [Visual Studio distributable-code list](https://learn.microsoft.com/en-us/visualstudio/releases/2026/redistribution).

App-local deployment is this portable package's chosen delivery strategy; Microsoft generally recommends central deployment
for update servicing. The release maintainer must verify the exact toolchain and license terms before copying files into a
package. This notice is not blanket permission to redistribute Visual Studio, Windows, or runtime files. Validate the
app-local package on a clean Windows machine.

The selected Qt kit's `sbom/` metadata for Qt Base, Qt Multimedia, and Qt SVG is a useful component inventory. A Qt
deployment may also include ICU and FFmpeg components. Use the exact kit's installed license and notice files for every
shipped component; an SBOM entry does not mean those notices have been staged, and repository documentation does not
replace them.

## Game data and trademarks

SED supports formats used by *Star Wars Jedi Knight: Dark Forces II*, *Mysteries of the Sith*, and *Indiana Jones and
the Infernal Machine*. This repository does not grant rights to redistribute game assets. Contributors and release
maintainers must not add proprietary levels, textures, models, sounds, archives, or other extracted game data.

The checkout also ships `data/master.tpl`, `data/mots.tpl`, and `data/ijim.tpl`, template catalogs containing
retail-style game definitions. Per-file provenance and rights evidence for these catalogs is not recorded in this
repository. The repository-level GPL notice covers the SED source code and does not by itself establish the catalogs'
origins or redistribution terms. Confirm the exact source and redistribution rights for each catalog before a public
release.

Game names and trademarks belong to their respective owners. Their use here identifies compatibility and does not imply
endorsement.
