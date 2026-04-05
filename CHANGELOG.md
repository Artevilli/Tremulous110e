Changelog
==================
Please note that this is not a complete list of every change. It will be continuously updated after 12/29/2025, and the dates will specify when each change was made. I kept a short changelog for a long time because info pages are limited to a maximum length of 1,022 characters due to them being sent by server commands. I have often shown changelogs to people I know, so if I find them, they will be listed in a category at the bottom. They will not be in perfect order, and there will be gaps between them. Additionally, changelogs were not written at the beginning of the project; they became a thing a few months in.

Changes
-----------------------
**--04/05/2026--**

* sys:<br />add some included files to vcproj

* qcommon:<br />fix some defines in qcommon header

* sys:<br />initial msvc files

* qcommon/server:<br />fixes for github actions

* qcommon:<br />drop redundant const void

* client:<br />add brackets around CG_SENDCONSOLECOMMAND syscall

* qcommon:<br />fix emit typo for x86

* github: github actions

**--04/04/2026--**

* cmake:<br />added cmake support

* qcommon:<br />ppc fpu load/store and var mapping optimizations

* qcommon:<br />added missing json file

**--04/02-2026--**

* qcommon:<br />qvm improvements

**--04/01/2026--**

* renderer:<br />correct shader time

**--03/31/2026--**

* qcommon:<br />optimize scalar loads

* qcommon/server:<br />rework free block search

* client:<br />fix and use internal ogg vorbis

**--03/28/2026--**

* server:<br />fix trace entity num

**--03/27/2026--**

* client/qcommon:<br />pull darklegion's hostname length patch

* qcommon:<br />unsigned shifting

* qcommon:<br />fix and allow library loading

**--03/26/2026--**

* qcommon:<br />avoid array bounds check for DLLs

* qcommon:<br />adapt RotatePointAroundVector() from Mesa 3D

* client/qcommon/server/sys:<br />big validity and stability patch

**--03/25/2026--**

* server:<br />redo no zero rate warnings

* server:<br />do not warn if rate is zero

* server:<br />mirror forced rate qualifications before sending warnings

**--03/24/2026--**

* qcommon:<br />specific -> generic qvm optimization

* server:<br />fix up client rates

**--03/23/2026--**

* server:<br />drop server demos

**--03/22/2026--**

* qcommon:<br />literal pool for fp immediates

**--03/20/2026--**

* qcommon:<br />more ppc optimizations

* qcommon/server:<br />attempt at fixing server status

**--03/19/2026--**

* client/renderer:<br />more atoi refactoring

**--03/18/2026--**

* qcommon:<br />basic powerpc const optimizations

**--03/16/2026--**

* server:<br />missed bounds check

* qcommon:<br />proper version string

**--03/14/2026--**

* qcommon:<br />use shared optimization framework in vm_powerpc.c

* qcommon:<br />replaced missed FS_IsBaseGame check

**--03/13/2026--**

* makefile:<br />temporarily stop using internal vorbis

**--03/12/2026--**

* scripts/qcommon:<br />fix up vm_aarch64.c and add osx scripts

* makefile/qcommon:<br />move opstack/reg optimizations to header file

**--03/11/2026--**

* qcommon:<br />change `MAX_INFO_STRING` back to 4096, allow specifying multiple basegames

* qcommon/server:<br />revert `MAX_INFO_STRING`

**--03/09/2026--**

* qcommon:<br />more cmod fixes

* qcommon:<br />reduce huffman buffer sizes

**--03/07/2026--**

* makefile:<br />drop hardening flags for performance purposes

* server:<br />properly handle ipv4/ipv6 settings when sending gamestat

**--03/06/2026--**

* server:<br />wrap attack log calls in a dedicated define to avoid crashes

**--03/05/2026--**

* client:<br />following server logic, move cgvm and uivm to clientStatic_t

* client:<br />add bounds checks to literal args

**--03/03/2026--**

makefile/qcommon:<br />remove version specific flags to run on more systems and fix fs_homepath

**--03/02/2026--**

* qcommon:<br />avoid memory leak in parse

* client:<br />workaround fix to Get New List

* qcommon:<br />fix parsing issues

* qcommon/sys:<br />code cleanup

**--03/01/2026--**

* client/qcommon/sys:<br />code cleanup

**--02/28/2026--**

* qcommon:<br />various fixes

* renderer:<br />use `GL_ARB_texture_border_clamp` unconditionally if available

* renderer:<br />dont use vbo or ppl without arb shaders

* renderer:<br />dont use merged lightmaps without `GL_ARB_texture_border_clamp`

* qcommon:<br />fix defines

**--02/27/2026--**

* qcommon:<br />fix variables

* qcommon/client:<br />cleanup

* qcommon/sys:<br />add an icon, tiny code cleanup

* qcommon:<br />fix separator replacement

* qcommon:<br />fix pure checksums

**--02/26/2026--**

* client:<br />fix syscalls

* code:<br />drop qvm code

* readme:<br />rename repository (`z-tremded-enhanced` -> `Tremulous11e`) and update readme

**--02/25/2026--**

* makefile/server:<br />drop mysql support

* renderer:<br />fix typos

**--02/19/2026--**

* server:<br />don't resolve master every time gamestat is sent

* server:<br />more constants

**--02/18/2026--**

* server:<br />disable webconsole

**--02/16/2026--**

* server:<br />enable webconsole

* server:<br />make webconsole optional

* server:<br />remove some unused server vars

* qcommon:<br />fix ppc variables

* qcommon:<br />fix opStack address assigning in vm_powerpc.c

* makefile/server:<br />webconsole

**--02/15/2026--**

* qcommon:<br />finalize steampath

**--02/14/2026--**

* client:<br />more misc fixes

* client:<br />misc fixes

* qcommon/client/renderer:<br />buggy driver prints

* client:<br />tiny fix to server browser name length

**--02/13/2026--**

* qcommon:<br />fix cvar flags

**--02/12/2026--**

* qcommon/server:<br />temporarily drop syscallCount

* qcommon/server:<br />syscallCount optimization

* qcommon:<br />use divss instead of divps

* server:<br />dont resolve master every new map

* renderer:<br />null pointer dereference fix

* qcommon/server:<br />syscall overflow detection

* qcommon/client/server:<br />undo some custom values

**--02/10/2026--**

* qcommon:<br />add isa jit optimizations to `vm_powerpc.c`

* makefile:<br />fix `RENDERER_DEFAULT`

* makefile/qcommon:<br />ppc64 support

* makefile/qcommon:<br />add powerpc support

* makefile:<br />ppc64le support

* qcommon:<br />remove debug code

* qcommon:<br />revert last qvm change and use movss instead of movd

* makefile/null:<br />remove unused null directory

**--02/09/2026--**

* client:<br />format header consistency

**--02/08/2026--**

* client:<br />fix compiler warnings when built as debug

**--02/07/2026--**

* qcommon:<br />only use cvtss2si on SSE1-only CPUs

* makefile:<br />more runtime protection flags

* renderers:<br />fix dlight regression

**--02/06/2026--**

* makefile:<br />stack protection

**--02/05/2026--**

* client:<br />avoid sharing syscalls when calling from cgame

**--02/04/2026--**

* client:<br />compatibility calls

* qcommon:<br />cleanup `FS_LoadLibrary()`

**--02/03/2026--**

* qcommon:<br />return full length of read file rather than just that it exists

* client:<br />small update

**--02/02/2026--**

* makefile/client/renderer/qcommon/server:<br />client compiles

* readme:<br />fixed typo

**--02/01/2026--**

* makefile/client/qcommon:<br />client progress

* client/qcommon/renderer/sys:<br />fix variable types

**--01/31/2026--**

* readme:<br />updated readme

* qcommon/sys:<br />include commented out renderer header in `unix_main.c`

* qcommon/server:<br />time for a brand new, alternate method of multiprotocol

* makefile/client/sys:<br />initial replacement of client

**--01/30/2026--**

* makefile/src:<br />src -> code

* libs/asm/renderer:<br />initial replacement of core system utilities

**--01/28/2026--**

* qcommon:<br />fix remaining macos defines

* qcommon:<br />formatting

**--01/27/2026--**

* qcommon:<br />revert "fix an uninitialized variable in CM_TransposeGrid()"

* qcommon:<br />disable alternative float casting

* qcommon:<br />fix an uninitialized variable in CM_TransposeGrid()

**--01/26/2026--**

* qcommon:<br />fix more cppcheck problems

* qcommon:<br />fix aarch64 bit shifting

* makefile:<br />enable `-D_FORTIFY_SOURCE` at level 3

* qcommon/server:<br />fix a few problems reported by cppcheck

* qcommon:<br />do not assign a local auto-variable address to a function parameter

* qcommon:<br />remove colors in favor of new ones from CPMA/CNQ3

**--01/24/2026--**

* server:<br />remove problematic `SV_IsValidClientSnapshot()`

* server:<br />potentially fix client command spam on world entering

* server:<br />small cleanup

**--01/23/2026--**

* server:<br />fix missing locations ucmd

* server:<br />flag `sv_padPackets` as `CVAR_DEVELOPER`

**--01/22/2026--**

* qcommon/renderer:<br />added `CVAR_DEVELOPER`

**--01/21/2026--**

* makefile/qcommon/client:<br />initial rewrite of client key handling

**--01/20/2026--**

* server:<br />reimplement avoiding sending full snapshots to loading clients

**--01/18/2026--**

* server:<br />encompass rate dropping in sv_protect

* server:<br />reimplement minimal netbuf allocation

* server:<br />properly null bytes in drdos checking

**--01/17/2026--**

* server:<br />readd sv_protect logic

* server:<br />add more rate limiting

**--01/16/2026--**

* server:<br />revert redundant "make rate decreasing follow a dynamic sv_dlRate"

* qcommon:<br />reduce `MAX_RELIABLE_COMMANDS` back down to 128

* server:<br />fix SV_SendClientMessages()

* server:<br />make rate decreasing follow a dynamic sv_dlRate

* server:<br />fix download rate decreasing when sv_dlRate is set lower than 5000

* server:<br />massive code cleanup

**--01/14/2026--**

* client:<br />begin the rewrite

**--01/13/2026--**

* server/qcommon:<br />network debugging

* server:<br />partially revert `server snapshot cleanup`

* qcommon:<br />add an option for alternative float casting in virtual machines

* server/qcommon/renderer/public:<br />initial support for engine extensions

* server:<br />clean up unoptimized client downloads

* server:<br />fix clientCommand spam when entering the world

**--01/12/2026--**

* server:<br />server snapshot cleanup

* server:<br />various fixes

* qcommon:<br />various fixes

* qcommon:<br />avoid using ansicolor for now

* sys:<br />fix bad color code ordering

* qcommon/server:<br />better handling of dedicated server commands, implement `com_viewlog` for Windows, avoid empty map names in `SV_Map_f()` code cleanup

* server:<br />remove redundant array initializer

* server:<br />temporary serverside fix to VoIP support

**--01/11/2026--**

* server:<br />fix preprocessor define for dynamic sv_fps values

* server:<br />code cleanup

* makefile/sys:<br />initial rewrite of windows sys

**--01/10/2026--**

* makefile/sys/qcommon/server:<br />initial restructuring and rewrite of sys handling, better rcon handling

**--01/09/2026--**

* server:<br />completely strip sv_minRebootDelayMins

* server:<br />improve SV_IntegerOverflowShutDown()

* sys:<br />improve Sys_Sleep()

* qcommon:<br />fixup some net_ip cvar handling

* server:<br />more sv_init cleanup

* server:<br />clean up attack log prints

* makefile/qcommon:<br />rebase to Quake3e, add support for new vms

**--01/08/2026--**

* qcommon:<br />fix bad debug if statement

* makefile/qcommon:<br />removed unused vms

* qcommon/server:<br />undo OpenWolf's modified va function

**--01/07/2026--**

* server:<br />more sv_game.c fixes

* qcommon/server:<br />use `SV_GameError` and `SV_GamePrint`, code formatting

* qcommon/server:<br />move `Q_strstrip` to `q_shared.c`, cleanup `Cmd_Args_Sanitize`

* server:<br />clean up userinfo flood protection

* server:<br />better client handling in `SV_UserMove` and `SV_ClientThink`

* qcommon:<br />code cleanup

**--01/06/2026--**

* qcommon:<br />allow VMs to specify serverinfo vars

* qcommon:<br />fix patch collide generation functions and code cleanup

* qcommon:<br />allow VMs to specify systeminfo vars

* server:<br />added client detection country code (needs external database) and fixed status formatting

* qcommon:<br />added `CV_FSPATH` and `CV_MAX`

* qcommon/server:<br />group cvars for better tracking

* qcommon:<br />buffer and sock fixes to netchan

**--01/05/2026--**

* qcommon:<br />actually use CVAR_NORESTART

* qcommon:<br />fix overlapping strings in parse

* qcommon:<br />add CVAR_MODIFIED to properly flag any modified cvar

* server:<br />better handling of sv_protect flags

* qcommon:<br />validate loaded bsp header

* qcommon:<br />cm code cleanup

* qcommon:<br />fix incorrect leaf brushes causing an engine crash

* qcommon/server:<br />allow overriding sv_master from command line

* qcommon/server:<br />added experimental CVAR_NODEFAULT flag

* qcommon:<br />fixes and optimizations to cmd.c

* qcommon:<br />md4.c constants

**--01/04/2026--**

* qcommon:<br />add cvar_trim to trim all unused cvars

* qcommon:<br />add varfunc to change specified cvars in different manners

* qcommon/server:<br />private flag for VM modules

* qcommon:<br />alphabetical cvar sorting

* qcommon/server:<br />introduction of UBIT()

* server:<br />proper utilization of gvm->forceDataMask

**--01/03/2026--**

* qcommon:<br />use `#if defined()` instead of `#ifdef`

* qcommon/server:<br />IPv6 is now optional

**--01/02/2026--**

* qcommon/server:<br />`FRAGMENT_BIT` memory fix and fix bitvector overflows in `SV_AddEntitiesVisibleFromPoint`

* server:<br />accompanying the increased sv_dlRate range, increase the last package acknowledge time to 12 seconds

* qcommon:<br />flag `com_affinityMask` and `vm_rtChecks` as `CVAR_SERVERINFO` for transparency 

* qcommon:<br />fix a memory problem in unzip.c

* qcommon:<br />some more formatting

* qcommon:<br />remove accidental ;

* qcommon/server:<br />formatting

* qcommon:<br />copyright

* server:<br />increase sv_dlRate's max range again

* qcommon/server:<br />fix bad syscalls placement

* makefile/qcommon/server/sys:<br />drop x86_64 vm and merge into x86, optimize and harden vm security, vms are now on par speed wise to native libraries

**--12/31/2025--**

* qcommon:<br />large cleanup to q_platform.h, fixed errors in affinity masking

* qcommon/sys:<br />affinity masking

* qcommon:<br />format VM_CallCompiled()

* qcommon:<br />add range check to VM_CallCompiled and make the memory volatile to further ensure segfaulting will not happen on newer GCC versions

**--12/30/2025--**

* server:<br />reduced the range check for sv_dlRate

* qcommon:<br />fixed some misc and memory problems

* qcommon:<br />fixed a few problems with the new VM interpreter, better security

**--12/29/2025--**

* qcommon:<br />misc fixes to a few extension functions in shared code

* qcommon/sys:<br />various fixes and optimizations to both common.c and sys, implementation of per-platform newline handling, cleanup

* qcommon:<br />properly shut down com_journalDataFile, code cleanup

* qcommon/sys:<br />cleaner and more optimized Com_Frame() including reduced CPU usage if not dedicated

* server:<br />slightly less hacky SV_SetConfigstring

* qcommon:<br />increased default com_zoneMegs to 12

**--prior to 12/29/2025--**

* readme:<br />added info

* qcommon/server:<br />use more instances of Cvar_SetIntegerValue to avoid potential rounding errors

* qcommon/server:<br />fixed 32-bit calling convention

* qcommon/server:<br />using explicit argument count for each VM_Call() which is more clear than implicit static *vmMainArgs tables

* server:<br />the 'unreferenced' check for valid pk3 downloads no longer exists

* server:<br />removed a redundant check in SV_ExecuteClientMessage, moved outgoing sequence incrementing to post client qport sending

* qcommon/server:<br />properly null out some qchars

* qcommon:<br />fixed a bad issue where cached pk3s did not have their cache references reset

* qcommon:<br />many optimizations and fixes to msg.c

* qcommon:<br />drop the original VM_PrepareInterpreter as it is no longer used
