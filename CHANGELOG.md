Changelog
==================
Please note that this is not a complete list of every change. It will be continuously updated after 12/29/2025, and the dates will specify when each change was made. I kept a short changelog for a long time because info pages are limited to a maximum length of 1,022 characters due to them being sent by server commands. I have often shown changelogs to people I know, so if I find them, they will be listed in a category at the bottom. They will not be in perfect order, and there will be gaps between them. Additionally, changelogs were not written at the beginning of the project; they became a thing a few months in.

Changes
-----------------------
**--12/31/2025--**

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
