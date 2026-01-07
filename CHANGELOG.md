Changelog
==================
Please note that this is not a complete list of every change. It will be continuously updated after 12/29/2025, and the dates will specify when each change was made. I kept a short changelog for a long time because info pages are limited to a maximum length of 1,022 characters due to them being sent by server commands. I have often shown changelogs to people I know, so if I find them, they will be listed in a category at the bottom. They will not be in perfect order, and there will be gaps between them. Additionally, changelogs were not written at the beginning of the project; they became a thing a few months in.

Changes
-----------------------
**--01/07/2026--**

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

* tremded:<br />accompanying the increased sv_dlRate range, increase the last package acknowledge time to 12 seconds

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
