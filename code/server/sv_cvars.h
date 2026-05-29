/*
 ===========================================================================
 Copyright (C) 1998 Steve Yeager
 Copyright (C) 2006 Cheyenne Spring Barnes
 Copyright (C) 2008 Robert Beckebans <trebor_7@users.sourceforge.net>

 This file is part of XreaL source code.

 XreaL source code is free software; you can redistribute it
 and/or modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation; either version 2 of the License,
 or (at your option) any later version.

 XreaL source code is distributed in the hope that it will be
 useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with XreaL source code; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 ===========================================================================
 */

//sv_cvars.h - handles all server console variables

#if defined(EXTERN_SV_CVAR)
#define SV_CVAR(cvar, cvarName, defaultString, cvarFlags, description) extern cvar_t *cvar;
#endif

#if defined(DECLARE_SV_CVAR)
#define SV_CVAR(cvar, cvarName, defaultString, cvarFlags, description) cvar_t *cvar;
#endif

#if defined(SV_CVAR_LIST)
#define SV_CVAR(cvar, cvarName, defaultString, cvarFlags, description) cvar = Cvar_GetAndDescribe(cvarName, defaultString, cvarFlags, description);
#endif

#if defined(SV_CVAR_LIST)
#define SV_CVAR_RANGE(vmCvar, minimum, maximum, valueType) Cvar_CheckRange(vmCvar, minimum, maximum, valueType);
#else
#define SV_CVAR_RANGE(vmCvar, minimum, maximum, valueType)
#endif

SV_CVAR(sv_mapname, "mapname", "nomap", CVAR_SERVERINFO | CVAR_ROM, "Display the name of the current map being used on the server.")
SV_CVAR(sv_privateClients, "sv_privateClients", "0", CVAR_SERVERINFO, "The number of spots, out of sv_maxclients, reserved for players with the server password (sv_privatePassword).")
SV_CVAR_RANGE(sv_privateClients, "0", va("%i", MAX_CLIENTS - 1), CV_INTEGER)
SV_CVAR(sv_hostname, "sv_hostname", "noname", CVAR_SERVERINFO | CVAR_ARCHIVE, "Sets the name of the server.")
SV_CVAR(sv_maxclients, "sv_maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH, "Maximum number of people allowed to join the server.")
SV_CVAR_RANGE(sv_maxclients, "1", XSTRING(MAX_CLIENTS), CV_INTEGER)

SV_CVAR(sv_maxclientsPerIP, "sv_maxclientsPerIP", "3", CVAR_ARCHIVE, "Limits the number of simultaneous connections from the same IP address.")
SV_CVAR_RANGE(sv_maxclientsPerIP, "1", NULL, CV_INTEGER)

SV_CVAR(sv_clientTLD, "sv_clientTLD", "0", CVAR_ARCHIVE_ND, "Client country detection code.")
SV_CVAR_RANGE(sv_clientTLD, NULL, NULL, CV_INTEGER)

SV_CVAR(sv_minRate, "sv_minRate", "0", CVAR_ARCHIVE_ND | CVAR_SERVERINFO, "Minimum server bandwidth (in bit per second) a client can use.")
SV_CVAR_RANGE(sv_minRate, "0", "100000", CV_INTEGER)
SV_CVAR(sv_maxRate, "sv_maxRate", "0", CVAR_ARCHIVE_ND | CVAR_SERVERINFO, "Maximum server bandwidth (in bit per second) a client can use.")
SV_CVAR_RANGE(sv_maxRate, "0", "100000", CV_INTEGER)
SV_CVAR(sv_dlRate, "sv_dlRate", "100", CVAR_ARCHIVE | CVAR_SERVERINFO, "Bandwidth allotted to PK3 file downloads via UDP, in kbyte/s.")
SV_CVAR_RANGE(sv_dlRate, "0", "500", CV_INTEGER)
SV_CVAR(sv_floodWait, "sv_floodWait", "500", CVAR_ARCHIVE, "Time in milliseconds that a client has to wait before sending another client command.")
SV_CVAR(sv_floodLimit, "sv_floodLimit", "8", CVAR_ARCHIVE, "The number of client commands a client is allowed to send before flood protection triggers.")
SV_CVAR(sv_floodProtect, "sv_floodProtect", "1", CVAR_ARCHIVE | CVAR_SERVERINFO, "Toggle server flood protection to keep players from bringing the server down.")

SV_CVAR(sv_novis, "sv_novis", "0", CVAR_ARCHIVE, "Toggle whether or not to skip the pvs check when transmitting entities.")
SV_CVAR(sv_pingFix, "sv_pingFix", "1", CVAR_ARCHIVE, "Fix client ping calculation to more accurately reflect packet loss and force minimum ping for humans to 1")
SV_CVAR(sv_showAverageBPS, "sv_showAverageBPS", "0", 0, "BSP Network debugging")

//systeminfo
SV_CVAR(sv_cheats, "sv_cheats", "1", CVAR_SYSTEMINFO | CVAR_ROM, "Unmodifiable cvar used for certain functions to act differently if the server allows cheats. If you want to turn cheats on, look at devmap.")
SV_CVAR(sv_serverid, "sv_serverid", "0", CVAR_SYSTEMINFO | CVAR_ROM, "")
SV_CVAR(sv_pure, "sv_pure", "1", CVAR_SYSTEMINFO | CVAR_LATCH, "Requires clients to only get data from pk3 files the server is using.")

SV_CVAR(sv_referencedPakNames, "sv_referencedPakNames", "", CVAR_SYSTEMINFO | CVAR_ROM, "Variable holds a list of all the pk3 files the server loaded data from.")

//server vars
#if defined(INCLUDE_REMOTE_COMMANDS)
SV_CVAR(sv_rconPassword, "rconPassword", "", CVAR_TEMP, "Password for remote server commands.")
SV_CVAR(sv_rconLog, "sv_rconLog", "", CVAR_ARCHIVE, "Name for the file which stores logs of all rcon commands, double quote to disable.")
#endif
SV_CVAR(sv_privatePassword, "sv_privatePassword", "", CVAR_TEMP, "")
#if defined(USE_JAVA) || defined(USE_BULLET)
SV_CVAR(sv_fps, "sv_fps", "60", CVAR_TEMP, "Set the max frames per second the server sends the client.")
#else
SV_CVAR(sv_fps, "sv_fps", "20", CVAR_TEMP, "Set the max frames per second the server sends the client.")
#endif
SV_CVAR_RANGE(sv_fps, "10", "125", CV_INTEGER)
SV_CVAR(sv_timeout, "sv_timeout", "200", CVAR_TEMP, "Seconds without any message before automatic client disconnect.")
SV_CVAR_RANGE(sv_timeout, "4", NULL, CV_INTEGER)
SV_CVAR(sv_zombietime, "sv_zombietime", "2", CVAR_TEMP, "Seconds to sink messages after disconnect.")
SV_CVAR_RANGE(sv_zombietime, "1", NULL, CV_INTEGER)

SV_CVAR(sv_allowDownload, "sv_allowDownload", "0", CVAR_SERVERINFO, "Toggle the ability for clients to download files maps etc. from server.")

SV_CVAR(sv_hidden, "sv_hidden", "0", CVAR_ARCHIVE, "Hide the server from queries and from master servers.")

SV_CVAR(sv_reconnectlimit, "sv_reconnectlimit", "3", 0, "Number of seconds a disconnected client should wait before next reconnect.")
#if defined(STATELESS_CHALLENGES_VERSION_ONE)
SV_CVAR_RANGE(sv_reconnectlimit, "0", "6", CV_INTEGER)
#else
SV_CVAR_RANGE(sv_reconnectlimit, "0", "12", CV_INTEGER)
#endif

SV_CVAR(sv_padPackets, "sv_padPackets", "0", CVAR_DEVELOPER, "Adds padding bytes to network packets for rate debugging.")
SV_CVAR(sv_killserver, "sv_killserver", "0", 0, "Internal flag to manage server state.")
SV_CVAR(sv_mapChecksum, "sv_mapChecksum", "", CVAR_ROM, "Allows check for client server map to match.")
SV_CVAR(sv_lanForceRate, "sv_lanForceRate", "1", CVAR_ARCHIVE_ND, "Forces LAN clients to the maximum rate instead of accepting client setting.")

SV_CVAR(sv_antiDRDoS, "sv_antiDRDoS", "0", CVAR_ARCHIVE, "Uses stricter temporary bans to protect against distributed reflected denial-of-service (DRDoS) attacks, in addition to built-in rate limiting.")
SV_CVAR(sv_antiDRDoSAffectsLan, "sv_antiDRDoSAffectsLan", "0", CVAR_ARCHIVE, "Sets whether sv_antiDRDoS guards against LAN clients or not.")

SV_CVAR(sv_levelTimeReset, "sv_levelTimeReset", "0", CVAR_ARCHIVE_ND, "Toggle whether or not to reset leveltime after a new map loads.")
SV_CVAR(sv_filter, "sv_filter", "filter.txt", CVAR_ARCHIVE, "Cvar that point on filter file, if it is \"\" then filtering will be disabled.")

SV_CVAR(sv_antiWallhack, "sv_antiWallhack", "0", CVAR_ARCHIVE, "Enables serverside wallhack protection\n0: disabled\n1: players only\n2: items/structures only\n3: all")

SV_CVAR(sv_sendNearbyEnts, "sv_sendNearbyEnts", "0", CVAR_ARCHIVE, "Toggle whether or not to send nearby entities regardless of pvs or anti wallhack.\nNOTE: range is specified by sv_sendNearbyEntsRange.")
SV_CVAR(sv_sendNearbyEntsRange, "sv_sendNearbyEntsRange", "1500", CVAR_ARCHIVE, "Specifies the range at which entities that fail the pvs check are sent to the client.")
SV_CVAR_RANGE(sv_sendNearbyEntsRange, "0", NULL, CV_INTEGER)

SV_CVAR(sv_filterCommands, "sv_filterCommands", "1", CVAR_ARCHIVE, "Toggles whether or not to filter excessive client commands.\n0: only strip essentials\n1: strip \\n and \\r\n2: also strip ;")

#if defined(DEBUG_SV_CHALLENGE)
SV_CVAR(sv_debugChallenges, "sv_debugChallenges", "0", CVAR_ARCHIVE, "Toggles whether or not to print debug messages for serverside stateless challenge generation.")
#endif

#undef SV_CVAR

#if defined(SV_CVAR_RANGE)
#undef SV_CVAR_RANGE
#endif
