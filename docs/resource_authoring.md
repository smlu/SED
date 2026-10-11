# Resource authoring contracts

The core resource readers and text writers provide portable formats for sprite
definitions, sound classes, and COG strings. Their public models and functions
are declared in [`spr.hpp`](../src/core/include/sed/assets/spr.hpp),
[`snd.hpp`](../src/core/include/sed/assets/snd.hpp), and
[`string_table.hpp`](../src/core/include/sed/assets/string_table.hpp). The
legacy COG-string compatibility wrappers remain in
[`episode.hpp`](../src/core/include/sed/assets/episode.hpp). These APIs use
synthetic text and resource names; they do not require installed game assets.

The application exposes **Tools > Sprite Editor**, **Tools > UNI Editor**, and
**Tools > Sound Class Editor**. Each action opens or activates its resource
group without creating a file tab. Sprite, Sound Class, and UNI have separate
dockable groups with file tabs and a shared New, Open, and Save toolbar. A file tab can detach into a
same-kind resource window, redock, or join another window of that kind. Moving a
tab transfers its existing pane, including active raw edits and external
modified state. Native dock tabs sit above the docks and show one major group at
a time. Floating Sprite, Sound Class, UNI, and COG editors provide native Maximize
and Restore controls. They retain the shared New/Open/Save
toolbar, document tools, and a visible Dock return control. A whole-window drop
onto the same editor kind joins its files into the target's document tabs and
retains the dropped window's current file. A drop onto another editor kind keeps
the editors separate in the native window tab bar. These window rules also apply
to the COG Script Editor. Sprite, Sound Class, and UNI docked headers offer
Undock on right-click; their floating headers have no menu.
During a file-tab drag, a Join label marks the visible same-kind target;
other editor kinds retain native docking behavior. Cancelling or leaving the
target removes the label. Moving the last file out hides its primary group,
which Tools can reopen empty with New and Open available. An emptied detached
host is retired without closing the transferred documents.

Group New creates a clean named in-memory draft; Open uses the active project's
resource picker and opens its selection in another tab. Neither operation
replaces the current draft or creates a file or directory. A resource selected
from a mounted archive is read without extraction. Its ordinary Save writes a
loose override at the project's resource destination and briefly reports the
saved path. Empty and all-clean groups disable Save. If another file of the
same kind is modified, the Save button remains enabled for its dropdown; its
plain Save action writes nothing for a clean current file. The dropdown offers
Save All Sprite, Save All Sound Class, or Save All UNI.
It saves modified files of that kind across primary and detached windows,
stopping on the first cancellation or failure and leaving later drafts dirty.
It preserves the current selection.
Save validates the authored format and replaces the destination atomically.
Closing or replacing a draft resolves unsaved edits with Save/Discard/Cancel;
the shared question identifies the current draft filename or loose source path,
uses `Save changes to <name> before closing?`, defaults to Save, and maps
Escape to Cancel. Its title names the editor: **Unsaved sprite**, **Unsaved
sound class**, **Unsaved string table**, or **Unsaved COG script**. If the owner
or authored document changes while the question is open, the pending close or
replacement stops without resolving the newer buffer.
File tabs show the filename. Hover shows the full loose path, or
`<archive basename>: <case-preserved entry path>` for an archive source, such as
`test.gob: misc/snd/Foo.SND`. The actual archive extension is retained. This
recorded origin survives tab movement and native Save/recovery without requiring
the archive to remain mounted. A successful loose Save changes the hover to its
full destination path; cancellation or write failure keeps the archive origin.
A native SED save stores open resource drafts in EDIT, but does not write loose
resource files or clear their external modified state. See the
[level designer workflow](user_guide.md#author-loose-resource-files) for controls.

| Action | Result |
| --- | --- |
| **Tools > Sprite Editor / Sound Class Editor / UNI Editor** | Opens or activates the corresponding group; it creates no file tab. |
| Group **New** | Creates one clean named draft tab in memory. No resource file or project folder is written. |
| Group **Open** | Opens the active project's resource picker and adds the selected project resource as a tab. Loose roots and mounted archives are read into memory; an archive entry is never extracted. If the normalized source already has an open tab, SED activates that buffer instead. |
| Group **Save** or focused **Ctrl + S** | Validates and atomically writes the external loose resource. A new draft asks for a destination; an archive-backed source writes its basename to the configured project resource directory and reports the saved path. |
| Focused **Ctrl + Shift + S** | Opens Save As to select a destination, then validates and atomically writes the external loose resource. |
| Group **Save All** | Saves only modified files of that kind, including detached windows, using the same validation and atomic replacement as Save. It is disabled when all files of that kind are clean. |
| Native **File > Save** | Saves the SED project and its optional EDIT resource drafts. It does not write the external resource file or clear that tab's modified marker. |
| Document **Undo / Redo** | Moves through that file's authored history. Focused text fields retain native text Undo; the document buttons operate on the whole file. |
| Drag a resource tab | Transfers the existing page and its local history between docked, floating, and other windows of the same resource kind; it does not duplicate the source buffer. |

When a resource pane owns focus, **Ctrl + S** routes to that pane's external
Save, and **Ctrl + Shift + S** opens its Save As flow. With the viewport or
another level control focused, **Ctrl + S** saves the native SED project and
its EDIT workspace. The same tab-level Save/Discard/Cancel prompt applies when
closing the page or replacing its document.

SPR, Sound Class, UNI, and COG each keep their own Undo/Redo timeline. Document
buttons and the main Edit actions use the active file; Ctrl+Z and Ctrl+Shift+Z
respect an active text editor's native Undo before document commands. Native
EDIT and recovery store the current timeline position, its redo branch, and
the external saved checkpoint with the raw draft. Restoring that workspace
retains those commands without replaying audio. External Save marks the
current position clean without discarding Undo; Undo/Redo back to that saved
position clears the file's modified marker. Manually editing values back to
their saved spelling creates another authored history position and leaves the
external file marked modified. Successful New, Open, and Reload start the
replacement document's baseline, while a cancelled or failed replacement
retains the existing timeline. Presentation-only empty Sound modes remain
undoable without dirtying the external SND.

Native capture preserves authored values and pending raw text. Each pane may also store an optional
`editorPresentation` V1 map: line-edit cursor/anchor positions; UNI current cell, selected ranges, and horizontal and
vertical scroll offsets; and SND current item, column, scroll offsets, and manual widths when explicitly sized. SND
items use stable source-row ordinals or exact mode bytes rather than tree positions. Caret/anchor state for a live UNI or
SND cell editor is captured without committing or moving focus, then restored when ordinary editing starts. Restore does
not reopen a live delegate. Presentation is excluded from the authored-content projection and resource Undo history;
changing it alone does not dirty a resource or arm recovery. Raw drafts and local Undo/Redo remain independently
preserved. A failed loose-file Reload leaves the existing draft and presentation intact; successful replacement clears
stale deferred cell carets along with the old document state. Undo/Redo and accepted data edits likewise invalidate
deferred carets for obsolete cells. Successful Sound Class Save also clears them when it normalizes a raw numeric
spelling without changing the accepted value. Cursor and anchor use UTF-16 positions and restore forward or reverse
selection directly instead of counting grapheme steps, including in supplementary and combining text. COG separately
retains its source cursor/selection and scrollbar positions. See the
[editor-state inventory](editor_state.md) for workspace, preference, and transient-state boundaries.

For a project that already has a saved path, first-time loose-file destinations
are suggested under its current directory: `misc/spr/<name>.spr`,
`misc/snd/<name>.snd`, and `misc/cogstrings.uni` for classic projects or
`misc/jonesstrings.uni` for IJIM. For an Untitled project, the Save As chooser
starts from the configured loose-resource root, falling back to the first
existing non-Episode resource root, and suggests the corresponding `misc/`
resource location. The project remains Untitled until its own Save, and no
folder or file is written until an explicit resource Save succeeds. The chooser
keeps the destination selectable; New and Open do not create project folders
or extract archive entries. Ordinary Save of a mounted resource uses that
same project location automatically, retaining the original basename. Save As
still opens a chooser. An existing destination requires explicit replacement
confirmation, and another open buffer's destination ownership remains
protected. The override directory is created only after validation and
confirmation; write failure retains the archive identity and raw draft.

Ordinary Save of an already opened loose SPR, Sound Class, or UNI file writes back to that exact source path,
including a configured resource root outside the active project. It retains destination ownership, validation, and
atomic replacement. New drafts, archive overrides, and Save As retain their project-destination rules; this source-path
exception does not authorize a different external destination.

Project-resource Open resolves one winning source through the active resource
locator and retains that source identity: a normalized loose-file path when the
winner is a loose resource, or the selected project resource name for an archive
entry. The picker's Storage metadata shows `Storage: <absolute loose path>` or
`Storage: <archive basename>: <case-preserved entry path>`, retaining GOB or GOO
extensions without adding a Filesystem or archive-type prefix. It appears inside
text/audio metadata and below image or rendered previews, and reports Unavailable when
that selection cannot resolve. Project data identifies a resource supplied
directly by the current project. The line updates independently of preview loading.
In views using loose-copy font emphasis, bold names indicate a matching loose
copy exists in configured roots; a selected archive can still be the preview
source. Template entries identify project template-database data instead of an
individual `.tpl` file. See the
[Resource Picker controls](user_guide.md#101-resource-categories-and-previews).
If the winner is missing, malformed, or over budget, Open reports the
error instead of silently substituting a shadowed source. Resource input is
admitted up to 16 MiB. Loose capture reads through EOF within that limit and probes for excess bytes; a stale file-size
hint cannot turn a growing file into an accepted prefix. Open into a new buffer parses and hashes the same immutable
byte snapshot,
so its external-change baseline always describes the bytes that populated the
pane. Archive hover metadata is cached independently of normalized ownership,
authored values, and document history. Optional `archiveTooltip` in native pane
and COG records is bounded to 32768 characters, rejects NUL, and is allowed only
for an archive source; legacy records without it retain their existing hover
fallback. COG retains its working-copy disk path separately from that origin.
Open loose SPR, Sound Class, UNI, and COG files share an asynchronous
source-and-folder watcher. External edits, atomic replacement, or deletion
offer named Reload/Keep choices even for a clean buffer. Offers wait for mouse
gestures and other dialogs and appear one at a time. Keep acknowledges that disk
revision without changing the current draft or history. A successful own Save
accepts its bytes quietly, and Save As rebinds the watch to the new source.
Reload after an external change reads the replacement before updating the pane.
SPR, Sound Class, and UNI also validate its parsed content. Missing, unreadable,
oversized, or malformed resource input reports an error and leaves the existing
raw draft and history intact. Readable COG source remains editable even with
syntax diagnostics. COG's source context menu and configured Reload shortcut
retain intentional reload; its document toolbar has no Reload button. Reopening an already owned winning loose COG
source or selected archive working-copy tab activates its live buffer without rereading source bytes; its edits and
history remain authoritative until Reload. An existing working-copy file without a live tab does not bypass admission.
Archive origin is captured before extraction changes resource candidates. COG Open into a new buffer, manual Reload,
the reread after a discard confirmation, and watcher Reload all use the same bounded 16 MiB source reader. The reader
checks the initial size, reads at most the limit plus one byte, and rejects unread or excess source bytes before installing a
tab or replacing text. Archive COG Open performs bounded winning-source admission before text copies or working-copy
extraction. Oversized winners and read failures are reported without extracting or substituting a shadowed source;
`.cog` suffix fallback is attempted only after the original resource name is missing. A source that grows beyond the
limit while a confirmation is open leaves the exact existing buffer, document history, cursor, and selection unchanged.
Ordinary loose COG Save synchronously rereads and hashes the current source against its accepted exact-byte SHA-256
baseline before format-on-save or another buffer mutation. Drift or an unknown source baseline offers Reload
from Disk / Overwrite Disk / Cancel (default and Escape). Reload rereads the latest source under the same 16 MiB
limit; Cancel preserves the exact buffer/history/caret and disk. Watcher Keep acknowledges its notice without rebasing
this Save baseline. Only successful Reload or committed Save advances the accepted hash, which can persist in the
optional COG `acceptedSourceHash` EDIT field. Failed or cancelled writes do not accept an external revision.

See [External file changes](user_guide.md#external-file-changes) for the designer
workflow.

## SPR

`SprFile` stores the material token, integer type and rendering modes, width and
height, RGBA extra light, and XYZ offset. The existing reader keeps its legacy
profile inference: a record with exactly 14 fields uses the IJIM RGBA layout;
otherwise, records with at least 11 fields use the classic scalar-light layout
and ignore extra trailing fields. Blank and comment-only lines are skipped.
The reader rejects an unsafe material token or one longer than 255 bytes before
returning a resource. A rejected Open or Reload leaves the existing editor draft,
source identity, and history intact. This input admission limit is separate from
the narrower project-specific external writer limits below.

Writers take an explicit `SprProfile`. The `classic` profile emits 11 fields
with one extra-light intensity; the `ijim` profile emits 14 fields with RGBA.
Both serialize every offset. Classic output is rejected if RGB channels differ
or alpha is not 1, because that layout cannot preserve non-scalar RGBA fields.
Unknown profile values, non-finite floats, and unsafe material tokens are
rejected. Portable raw input retains safe tokens up to 255 bytes. A safe token
is non-empty and contains no whitespace, control characters, comment markers,
field separators, or quotes. Single directory slash or backslash characters
are retained. External engine output limits material names to 31 bytes
for JKDF2/MotS and 63 bytes for IJIM. A longer imported or native draft remains
editable, but external Save fails before replacing any destination bytes.
Output uses locale-independent numbers with enough precision to
round-trip `float` values and ends with one newline.

Example classic record:

```text
sprite.mat 2 3.25 4.5 4 3 0 0.125 8.25 -9.5 10.75
```

The pane uses the active project's profile without a profile selector. Scalar
fields use scrub controls with compact step arrows; width and height edits are
nonnegative, and XYZ offsets are stacked vertically. Opening an unsupported
source value keeps the original value and identifies it explicitly instead of
silently converting it to an offered option. Untouched source floats retain
their full precision. An active numeric text edit, including empty or invalid
intermediate text, is retained separately from the canonical float in EDIT and
recovery. Capturing state does not commit or reformat that editor. Native text
Undo removes a pending edit when it returns to the original display; moving the
cursor or focus then preserves the original float even when a tiny value rounds
to zero on screen. Explicitly typing zero remains an authored edit. Successful
New, Open, or Reload clears the previous source's numeric draft; cancellation
or failed input leaves it intact. Cancelling the IJIM extra-light color picker
leaves all four source channels unchanged. Double-clicking Material opens the
project MAT picker.

| Project | Sprite type behavior | Named light modes |
| --- | --- | --- |
| JKDF2 / MotS | Accepted type tokens 0, 1, and 2 draw a camera-facing billboard; the authored token is retained. | Fully lit (0), Not lit (1), Diffuse (2), Gouraud (3) |
| IJIM | Type 0 is camera-facing; type 2 uses the Thing orientation. The renderer has no draw branch for type 1. | None (0), Lit (1), Diffuse (2), Gouraud (3) |

The isolated preview has no ambient illumination, so lit modes show the authored
extra light. Mode 0 renders at full brightness and ignores extra-light RGB or
scalar intensity; IJIM alpha still controls opacity. IJIM offers and preserves
Gouraud (3), but the original Sprite renderer caps effective lighting at Diffuse
(2), also respecting the global lighting mode; it does not draw a separate
Gouraud gradient. See the [IJIM Sprite lighting clamp](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/rdroid/Primitives/rdSprite.c#L298-L320).
Both profiles expose authored **Geometry mode** choices: **Not drawn**,
**Vertices**, **Wireframe**, **Solid**, and **Textured**. They store values 0–4
respectively; the combo displays names without numeric suffixes. The isolated
preview follows this authored value, showing no card, corner vertices, the four
wire edges, Solid MAT color, or Textured MAT pixels. Changing it is a resource
edit covered by Undo/Redo, native drafts, and external SPR Save. The regular IJIM
viewport retains the engine's global geometry path rather than using the isolated
preview override. IJIM hides the ignored texture-mode control while retaining
its source token. The IJIM loader skips texture mode. Classic texture choices
identify Affine (0), Perspective (1), and legacy or unknown values, and preserve
arbitrary imported integers. Tooltips explain
those engine limits.

Both loaders accept type tokens 0 through 2, but loader admission does not imply
that every token has a rendering implementation. XYZ offsets follow camera
right, depth, and up, so Y changes depth in a straight-on view. IJIM applies
those camera-space offsets to type 0; its type-2 geometry ignores offsets, and
the pane hides its XYZ labels and fields while retaining the source offsets.
Unsupported imported type or mode integers remain preserved by an unchanged
Save; applicable controls identify their current unsupported values. For
animated MAT files, the 0–99 FPS slider controls playback; 0 pauses, Pause holds the current cel,
Play resumes at the selected rate, and Stop returns to cel zero while paused
without changing that rate. These controls are hidden when the MAT has no
animation. Double-clicking either the Material field or rendered preview opens
the same MAT picker. Bounds is enabled by default unless saved presentation
disables it. Bounds shows an envelope that contains the card throughout camera
orbit, rather than an engine collision box. Recenter fits the same Bounds geometry the preview draws in a
straight-on view without changing authored offsets. Dimension and offset scrubs
publish intermediate geometry on a coalesced 33 ms cadence without resetting
orbit or pan or rebuilding the isolated preview Level. Framing grows as needed.
If the retained camera depth range cannot contain the result, the preview
reports the limitation and keeps the source values and previous camera. Zero
dimensions remain valid native drafts; external Save requires positive width
and height. Bounds, FPS,
splitter size, and full camera framing are stored as an
optional pane presentation map in native EDIT/recovery state, never in the SPR
record or authored dirty state. The current animation cel is not stored; after
restore, playback starts at the first available MAT cel without reparsing the
material or rebuilding the preview Level each frame. In-place numeric updates
reuse the MAT/CMP cel cache, including mounted archive resources. They retain
the paused cel or running playback clock while updating geometry. Explicit
resource-change notifications invalidate those dependencies.

## SND sound classes

`SndFile` remains the existing dependency-only view. Structured sound-class
authoring uses separate `SoundClassFile` and `SoundClassEntry` models so ordered
alternatives, unknown mode names, and all playback values remain available to an
editor.

Pass `SoundClassProfile::classic` or `SoundClassProfile::ijim` when loading.
`soundClassModeNames(profile)` exposes the matching engine mode names for editor
suggestions. Recognition is case-sensitive. The tables follow the
[OpenJKDF2 classic mode table](https://github.com/shinyquagsire23/OpenJKDF2/blob/02297602cb289b113df27be33b5a7dddbe3d91c1/src/World/sithSoundClass.c)
and the
[OpenJones3D IJIM mode table](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/sith/World/sithSoundClass.c).
Unknown mode names are retained as source rows; they do not update the inherited
flags used by subsequent recognized rows.

| Project | Offered engine slots | Audited examples | Reserved names |
| --- | --- | --- | --- |
| JKDF2 / MotS | 1–95 (`MAX` is 96) | `create` 1, `moving` 5, `surprise` 87 | `reserved1`–`reserved8` are 88–95 |
| IJIM | 1–140 (`MAX` is 141) | `moving` 5, `lwalksnow` 26, `surprise` 122, `stand2walk` 131, `treadaet` 140 | `reserved1`–`reserved8` are 123–130 |

Slot 0 is the sentinel and is omitted from suggestions. The named reserved modes
remain valid table entries: IJIM's AI movement code directly uses `reserved1`,
`reserved2`, and `reserved3`, and generic playback admits the remaining in-range
slots. These are mode indices, not alternative counts. Recognized repeated
modes append linked alternatives without a per-mode runtime count cap. A
recognized `none` row is allocated and counts; a missing WAV can be skipped.
The badge tooltip reports only `Sound mode: N`. COG `PlaySoundClass` takes a
Thing and this integer engine slot; it does not take a WAV name or alternative
index. See the [classic COG playback function](https://github.com/shinyquagsire23/OpenJKDF2/blob/02297602cb289b113df27be33b5a7dddbe3d91c1/src/Cog/sithCogFunctionSound.c#L285-L304)
and [IJIM COG playback function](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/sith/Cog/sithCogFunctionSound.c#L299-L320).

Unknown names remain exact SED source rows. IJIM rejects them before allocating
an alternative. The audited OpenJKDF2 loader uses an unsigned mode index and
fails to reject a missing lookup's zero, so an unknown name can enter its
sentinel slot. SED preserves the imported literal without reproducing that
anomaly. Its portable retention contract is separate from engine admission;
see the [classic loader](https://github.com/shinyquagsire23/OpenJKDF2/blob/02297602cb289b113df27be33b5a7dddbe3d91c1/src/World/sithSoundClass.c)
and [IJIM loader](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/sith/World/sithSoundClass.c#L403).
The [IJIM AI movement callers](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/sith/AI/sithAIMove.c#L880)
show why named reserved modes are retained.

Each row contains a mode, WAV token, 32-bit playback flags, minimum radius,
maximum radius, and maximum volume. Classic SED defaults omitted flags to
`0x40` independently for every row. IJIM carries the flags from the prior
recognized mode row, starting at `0x40`; an unknown mode is retained but does
not change that inherited value. Both profiles reset radii and volume for every
row to `0.5`, `2.5`, and `1`. The structured reader retains `none` as a valid
no-audio token. It does not resolve WAV dependencies; the legacy `SndFile` API
remains a separate, unchanged dependency parser.

Sound-class writers accept `SoundClassProfile`; an omitted argument keeps the
classic default for existing callers. The pane always passes the active
project's profile. Output emits all six fields, including explicit flags, as
one row per alternative.
External WAV names are limited to 31 bytes for JKDF2/MotS and 121 bytes for IJIM;
portable raw input and native drafts can retain longer safe tokens up to 255
bytes. Overlong external names fail before replacing the destination and leave
the current raw draft, source identity, and modified state intact.
Alternatives may repeat a mode; there is no separate runtime per-mode count
limit. The editor fills the available width with one tree of
animated mode groups, initially collapsed. Groups appear in engine-index order
without reordering source records; unknown imported modes follow in their
original first-appearance order. Each larger spanned title retains the exact
mode name, with its engine index in a separate badge on the left. An unknown
source mode has an unnumbered badge and explanatory hover help. A single left
click toggles the header; a second click during its transition reverses the
requested direction. Expand All and Collapse All sit above search. They
batch expansion without animation and publish one presentation change; a single
header toggle retains animation. An expanded group has a slightly lighter gray
entry canvas and its own column headings rather than a global header. Dragging
a heading boundary resizes that column consistently across all groups.
The resource column is labelled Sound and the volume heading is Volume. Flags,
Min, and Max headings and values are centered; Min and Max are the minimum and
maximum sound radii. Hover help retains their meaning and world-distance units.
Rows have no selection highlight. Keyboard navigation, field editing, and
native text Undo still use the current item.

The Add sound mode button sits above search. Its searchable popup offers only
engine table modes absent from all current groups, including named reserved
modes. The popup opens near the cursor without minimize or maximize controls.
Adding a mode clears the main mode search, then expands, focuses, and scrolls
to a zero-entry group. It creates no placeholder WAV and makes no external file
change. The group's
centered, spanned Add Entry row appends a real alternative without replacing existing
entry items, retaining the scroll position near that new entry. Zero-entry
groups never produce external SND records.
Native presentation retains them in optional `soundEmptyModes`: a sorted unique
array of exact engine mode names, bounded to 95 classic or 140 IJIM names and
excluding modes already present in authored rows. Other resource kinds reject
that field; New, Open, and Reload clear it. It is excluded from the authored
content projection. Search filters titles
and temporarily expands matches, including after row changes or Undo, while
retaining the user's collapse choices. Collapse identities are stored in sorted
order so unrelated structural changes do not reorder presentation state.
Serialization preserves stable source-row identity and the original alternative
order even when modes are interleaved. Double-clicking Sound opens the project
picker; double-clicking Flags opens a popup anchored at the mouse. Search updates
filtering immediately and coalesces its host notification after Qt finishes the
text transaction, allowing a host to safely destroy the pane. Cancel, owner
destruction, or a stale modal snapshot leaves the current authored rows intact.

A hovered alternative reveals compact green Play and light-blue Loop icons
on its left and a red remove icon on its right. The playing alternative keeps
its Stop and Loop controls when the pointer leaves or hovers another row;
Remove remains a hover control. Reserved positions keep row geometry stable.
Five shared buttons serve the tree rather than creating widgets for every row.
The buttons retain 22-logical-pixel hit areas with 13-logical-pixel icons rendered
at 2x density; the Loop glyph and its check state stay visible in hover and
playing controls. Moving from the row onto
one of those buttons preserves the hovered source row and its removal target.
Preview looping is separate from authored play flags and survives EDIT restore
in the optional `soundPreviewLoops` vector. Play toggles
Stop for the pane's running preview. A lease-scoped status timer runs only while
that preview is active and clears its persistent controls when playback ends
or another owner replaces it. A finished one-shot releases preview
priority so ambient playback can resume; hiding or destroying a pane stops only
its own playback. The shared audio channel authorizes an exact feature and pane
lease, so a stale pane cannot stop or adjust a replacement preview.
Genuine volume edits adjust the pane's current playing entry, including loops,
through that same lease. They do not change the global picker preview volume
or restart playback; an inactive or replaced pane only edits its own raw draft.
Undo and Redo of a volume change adjust an existing voice through its current
lease and do not start or replay a stopped preview.

Entry add, remove, cell edits, and adding empty mode groups use local Undo/Redo
with exact raw text and source order. The Sound tree's body context menu offers
Undo and Redo with Ctrl+Z and Ctrl+Shift+Z guidance; the same shortcuts operate
on pane history when the tree owns focus. Text fields retain native text Undo
while editing. Empty-mode history affects native presentation without making
an otherwise clean external file dirty. A completed numeric scrub is one
history gesture. The portable text reader admits at most 4096 rows per resource
as an editor safety limit, not an engine alternative limit. Flags use eight
uppercase hexadecimal digits, and floating point values use locale-independent,
round-trip precision. Numeric values must
be finite; radii must be nonnegative and ordered (`minRadius <= maxRadius`), and
maximum volume must be nonnegative. There is no artificial upper bound on
maximum volume. Raw mode and WAV tokens follow the same 255-byte safe-token
rules as raw SPR material names. Structured input is bounded to 4096-byte lines,
255-byte tokens, and 4096 entries.

Every volume cell displays a slider beside its value. Painting inactive sliders
avoids one persistent widget per alternative; an active drag uses one editor
and suppresses the inactive slider underneath it. The value and slider stay
inside the final Volume column. Default fitting keeps the short metadata
columns compact and first gives Sound 65% of the width beyond the Sound/Volume
minima. It then transfers 40% of the resulting Volume width to Sound, capped so
Volume never drops below its 230-pixel minimum. Manual heading resizing remains
available and keeps its requested widths.
Slider and value painting stays inside its column even after resizing; the
inactive canvas and active editor reserve the same Remove slot inside the
visible Volume cell.
Painted and active sliders share the same track and value geometry, including
narrow cells. The slider and value are vertically centered with clear margins;
the complete filled and unfilled track uses the same rendering in both states.
The visible control area reserves Remove's position when the remaining space
can hold the ordinary value, slider, and margins. The whole pane's minimum
width fits its compact columns, native frame, scrollbar, and margins, retaining
a visible Remove control as the editor narrows. The six column minima are
`80/160/100/56/56/230` logical pixels for Play/Loop, Sound, Flags, Min,
Max, and Volume. Initial widths are `80/220/100/64/64/230`; default fitting then
applies the Sound/Volume allocation above without crossing a column minimum.
The stable pane minimum reserves the styled vertical scrollbar even while
it is hidden, so changing row count or height does not change that constraint.
Manual column enlargement can still use horizontal scrolling. If that scroll
leaves too little of Volume visible, Remove hides; the slider and value stay
inside their column without covering an adjacent field. Longer imported
values may elide in the inactive label, with their complete raw text available
in hover help and the editor.
Explicit slider edits author values from 0 to 1. An imported higher value
remains unchanged until explicitly edited, and the format writer retains its
existing nonnegative contract. Radius help uses SED's distance-display
convention, `0.1` engine world unit = `1 m`; both mixers compare the authored
radii to raw world-position distance. This display conversion is defined in
[`cleave_measurement_utils.hpp`](../src/app/src/render/cleave/cleave_measurement_utils.hpp).
Numeric cells display the shortest decimal that
round-trips to the stored float, so a float authored as `0.01` displays `0.01`
without losing precision. Active invalid cells remain raw EDIT drafts and block
external Save. The flag popup uses the active engine's authored flags. IJIM
exposes the twelve `SoundPlayFlag` bits from `0x001` through `0x800`. JKDF2/MotS
also expose the loader's `0x4000` Precache flag and label no-panning and
mutual-exclusion bits according to classic behavior. Runtime-only channel state
is not offered. Unknown bits survive Apply, Cancel, Undo, and unchanged Save.
See the
[classic flag definitions](https://github.com/shinyquagsire23/OpenJKDF2/blob/02297602cb289b113df27be33b5a7dddbe3d91c1/src/types_enums.h)
and [IJIM authored flag definitions](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/sound/types.h#L35).
Declared flags are not a promise that every engine branch implements their effect.

The WAV picker's All view includes the preferred loose directory, every
configured resource root, and mounted archives. It discovers WAVs in standard
sound directories and directly in a root or archive. Project-local category
pairs still override shared category files independently of asset discovery.
The default package has no IJIM template or WAV category pairs. When an IJIM
template category pair is unavailable, templates remain available through All
and Thing placement uses type-based filters; the Template Editor hides its
category pane. When a WAV category pair is unavailable, the picker builds a
Sounds > Uncategorized category from discovered WAVs. Categories contain names,
so a catalog can remain populated while All is empty when no matching assets
are mounted or discoverable. JKDF2/MotS WAV categories are generated from
discovered sounds when no project-local or mounted pair exists.
Opening the picker, populating its initial selection, or changing categories
only updates metadata. Playback starts on explicit Play or an actual user WAV
selection change. Loop changes affect an already playing selected WAV and do
not start a silent picker.

Example:

```text
moving step.wav 0x0000000B 0.25 8.5 0.75
moving none 0x0000000B 0.5 2.5 1
```

The second row is a distinct ordered alternative and explicitly retains the
effective flags of the first row. Classic initializes flags for each allocated
row; IJIM initializes inherited flags before reading rows and checks recognition
before applying an optional flags column. Both give each allocated row the
radius and volume defaults above; see the
[OpenJones3D sound-class loader](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/sith/World/sithSoundClass.c).

## UNI COG strings

The generic `StringTableFile` API in [`string_table.hpp`](../src/core/include/sed/assets/string_table.hpp)
is used by resource panes for both the classic `cogstrings.uni` profile and the
IJIM `jonesstrings.uni` profile. It retains row order, key spelling, numeric
field, and raw game bytes. The UI converts bytes through Latin-1 so each game
byte round-trips without UTF-8 replacement; Unicode characters that cannot be
represented by one game byte are rejected on external Save and remain in the
EDIT draft. Both generic profiles use exact case-sensitive key identity, so
`CaseKey` and `casekey` are distinct while an exact duplicate is rejected.
IJIM's value field is bounded by the first and last quote, so retail embedded
quotes, backslashes, and `#` bytes inside the message are retained. Classic authored message text
continues to reject embedded quotes.

All resource panes use the active project's engine profile. The UNI pane hides
the unused legacy number column in every project without discarding the field.
Its row search only hides nonmatching rows. When a nonempty query is cleared,
the existing selected cell is scrolled to the center without changing the
current index, its column, edit buffers, history, or authored metadata; other
query transitions and empty-query refreshes do not reposition the table.
The engine reads it as signed decimal opaque metadata;
string lookup returns the message text, so the editor assigns it no additional
meaning. The separate legacy
`CogStringsFile` API in `episode.hpp` remains the
compatibility wrapper used by existing episode/COG-string consumers. Its keys
are case-insensitive for lookup, set, erase, duplicate detection, and merge;
the writer sorts canonical uppercase keys. The generic resource-pane API does
not route through that canonicalizing wrapper, so authored table order and key
case are not rewritten as a side effect of opening or saving another profile.

Both generic profiles require an `MSGS` capacity, quoted keys, a signed numeric
legacy field, quoted text, and `END`; extra message rows beyond the declared
capacity and duplicate keys are rejected. Retail tables may declare a larger
capacity than their live row count. Native drafts retain that admitted capacity;
external writers for both profiles emit the live message count in `MSGS`.
Signed legacy values, including the full 32-bit endpoints, remain exact in raw,
native, and external records. Full-line comments are recognized without
truncating `#` characters inside a quoted value.

Generic table admission is bounded independently of the SED EDIT envelope: the
declared capacity and live row count are each limited to 4096, key/text fields
to 255 game bytes, and scanned physical lines to 4096 bytes. The format-specific
record body limit is 253 bytes for classic and 273 bytes for IJIM, excluding the
line ending. A rejected or intermediate invalid cell remains in the in-memory
EDIT draft; it is never truncated to fit these authored-file limits.

`PUP` authoring is deferred; its current loader remains available for resource
discovery.

## Runtime references

The classic and IJIM SPR layouts retain the fields consumed by the engine
sprite implementation, including width, extra light, and offset; see
the [classic sprite renderer](https://github.com/shinyquagsire23/OpenJKDF2/blob/02297602cb289b113df27be33b5a7dddbe3d91c1/src/Primitives/rdSprite.c)
and [IJIM sprite renderer](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/rdroid/Primitives/rdSprite.c).
The SND mode tables, inheritance, and authored flags follow the engine
references linked above. Classic Precache is consumed by the
[classic sound-class loader](https://github.com/shinyquagsire23/OpenJKDF2/blob/02297602cb289b113df27be33b5a7dddbe3d91c1/src/World/sithSoundClass.c);
IJIM distinguishes authored playback flags from runtime channel state in its
[sound types](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/sound/types.h).
The [IJIM string-table loader](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/std/General/stdStrTable.c)
reads the signed number and keeps it separate from string lookup.
The generic UNI contract is implemented by
[`string_table.cpp`](../src/core/src/assets/string_table.cpp); the existing
`CogStringsFile` API remains a separate compatibility wrapper in
`episode.cpp`.
