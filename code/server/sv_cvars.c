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
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with XreaL source code; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ===========================================================================
 */

//sv_cvars.c - handles all server console variables

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sv_cvars.h"

cvar_t *sv_fps = NULL; //time rate for running non-clients
cvar_t *sv_timeout; //seconds without any message
cvar_t *sv_zombietime; //seconds to sink messages after disconnect
#if defined(INCLUDE_REMOTE_COMMANDS)
cvar_t *sv_rconPassword; //password for remote server commands
cvar_t *sv_rconLog; //log file for remote server commands
#endif
cvar_t *sv_privatePassword; // password for the privateClient slots
cvar_t *sv_hidden;
cvar_t *sv_allowDownload;
cvar_t *sv_maxclients;
cvar_t *sv_maxclientsPerIP;
cvar_t *sv_clientTLD;
cvar_t *sv_collectClientJunkInfo;
cvar_t *sv_cheats;
cvar_t *sv_privateClients; //number of clients reserved for password
cvar_t *sv_hostname;
cvar_t *sv_master[MAX_MASTER_SERVERS]; //master server ip address
cvar_t *sv_reconnectlimit; //minimum seconds between connect messages
cvar_t *sv_showloss; //report when usercmds are lost
cvar_t *sv_padPackets; //add nop bytes to messages
cvar_t *sv_killserver; //menu system can set to 1 to shut server down
cvar_t *sv_mapname;
cvar_t *sv_mapChecksum;
cvar_t *sv_referencedPakNames;
cvar_t *sv_serverid;
cvar_t *sv_minRate;
cvar_t *sv_maxRate;
cvar_t *sv_dlRate;
cvar_t *sv_novis;
cvar_t *sv_pure;
cvar_t *sv_cpuusagepublic;
cvar_t *sv_avgframetimepublic;
cvar_t *sv_warningscpu;
cvar_t *sv_warningsframetime;
cvar_t *sv_floodWait;
cvar_t *sv_floodLimit;
cvar_t *sv_floodProtect;
cvar_t *sv_pingFix;
cvar_t *sv_userInfoFloodProtect;
cvar_t *sv_showAverageBPS;
cvar_t *sv_lanForceRate; //dedicated 1 (LAN) server forces local client rates to 99999 (bug #491)
cvar_t *sv_antiDRDoS;
cvar_t *sv_antiDRDoSAffectsLan;
cvar_t *sv_dequeuePeriod;
cvar_t *sv_levelTimeReset;
cvar_t *sv_filter;
cvar_t *sv_antiWallhack;
cvar_t *sv_sendNearbyEnts;
cvar_t *sv_sendNearbyEntsRange;
cvar_t *sv_filterCommands;

#if defined(DEBUG_SV_CHALLENGE)
cvar_t  *sv_debugChallenges;
#endif

void
SV_InitCvars(void)
{
  qint index;

  //serverinfo vars
  Cvar_Get("dmflags", "0", CVAR_ARCHIVE | CVAR_SERVERINFO);
  Cvar_Get("timelimit", "0", CVAR_SERVERINFO);
  Cvar_Get ("sv_keywords", "", CVAR_SERVERINFO);
  //Cvar_Get("protocol", va("%i", PROTOCOL_VERSION), CVAR_SERVERINFO | CVAR_ROM);
  sv_mapname = Cvar_GetAndDescribe("mapname", "nomap", CVAR_SERVERINFO | CVAR_ROM, "Display the name of the current map being used on the server.");
  sv_privateClients = Cvar_GetAndDescribe("sv_privateClients", "0", CVAR_SERVERINFO, "The number of spots out of sv_maxclients reserved for players with the server password set by sv_privatePassword, also the number of bot slots for ^1Z^7.");
  Cvar_CheckRange(sv_privateClients, "0", va("%i", MAX_CLIENTS - 1), CV_INTEGER);
  sv_hostname = Cvar_GetAndDescribe("sv_hostname", "noname", CVAR_SERVERINFO | CVAR_ARCHIVE, "Sets the name of the server.");
  sv_maxclients = Cvar_GetAndDescribe("sv_maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH, "Maximum number of people allowed to join the server.");
  Cvar_CheckRange(sv_maxclients, "1", XSTRING(MAX_CLIENTS), CV_INTEGER);
  sv_maxclientsPerIP = Cvar_GetAndDescribe("sv_maxclientsPerIP", "3", CVAR_ARCHIVE, "Limits the number of simultaneous connections from the same IP address.");
  Cvar_CheckRange(sv_maxclientsPerIP, "1", NULL, CV_INTEGER);
  sv_clientTLD = Cvar_GetAndDescribe("sv_clientTLD", "0", CVAR_ARCHIVE_ND, "Client country detection code.");
  Cvar_CheckRange(sv_clientTLD, NULL, NULL, CV_INTEGER);
  sv_minRate = Cvar_GetAndDescribe("sv_minRate", "0", CVAR_ARCHIVE_ND | CVAR_SERVERINFO, "Minimum server bandwidth (in bit per second) a client can use.");
  Cvar_CheckRange(sv_minRate, "0", "100000", CV_INTEGER);
  sv_maxRate = Cvar_GetAndDescribe("sv_maxRate", "0", CVAR_ARCHIVE_ND | CVAR_SERVERINFO, "Maximum server bandwidth (in bit per second) a client can use.");
  Cvar_CheckRange(sv_maxRate, "0", "100000", CV_INTEGER);
  sv_dlRate = Cvar_GetAndDescribe("sv_dlRate", "100", CVAR_ARCHIVE | CVAR_SERVERINFO, "Bandwidth allotted to PK3 file downloads via UDP, in kbyte/s.");
  Cvar_CheckRange(sv_dlRate, "0", "500", CV_INTEGER);
  sv_novis = Cvar_GetAndDescribe("sv_novis", "0", CVAR_ARCHIVE, "Toggle whether or not to skip the pvs check when transmitting entities.");
  sv_cpuusagepublic = Cvar_GetAndDescribe("sv_cpuusagepublic", "0", CVAR_ARCHIVE, "Toggle whether or not to publicly display server cpu usage in getinfo responses.");
  sv_avgframetimepublic = Cvar_GetAndDescribe("sv_avgframetimepublic", "0", CVAR_ARCHIVE, "Toggle whether or not to publicly display the average frame response time in getinfo responses.");
  sv_warningscpu = Cvar_GetAndDescribe("sv_warningscpu", "70", CVAR_ARCHIVE, va("Sets the desired percentage value before cpu usage warnings begin appearing.\nNOTE: Falls back to %i if not set!", CPU_USAGE_WARNING));
  Cvar_CheckRange(sv_warningscpu, "0", "100", CV_INTEGER);
  sv_warningsframetime = Cvar_GetAndDescribe("sv_warningsframetime", "30", CVAR_ARCHIVE, va("Sets the desired value the average frame time has to go over before warnings begin appearing.\nNOTE: Falls back to %i if not set!", FRAME_TIME_WARNING));
  Cvar_CheckRange(sv_warningsframetime, "0", "100", CV_INTEGER);
  sv_floodWait = Cvar_GetAndDescribe("sv_floodWait", "500", CVAR_ARCHIVE, "Time in milliseconds that a client has to wait before sending another client command.");
  sv_floodLimit = Cvar_GetAndDescribe("sv_floodLimit", "8", CVAR_ARCHIVE, "The number of client commands a client is allowed to send before flood protection triggers.");
  sv_floodProtect = Cvar_GetAndDescribe("sv_floodProtect", "1", CVAR_ARCHIVE | CVAR_SERVERINFO, "Toggle server flood protection to keep players from bringing the server down.");
  sv_pingFix = Cvar_GetAndDescribe("sv_pingFix", "1", CVAR_ARCHIVE, "Fix client ping calculation to more accurately reflect packet loss and force minimum ping for humans to 1");
  sv_userInfoFloodProtect = Cvar_GetAndDescribe("sv_userInfoFloodProtect", "1", CVAR_ARCHIVE | CVAR_SERVERINFO, "Prevents users from flooding the server with userinfo changes by delaying the next change for 5 seconds.");
  sv_showAverageBPS = Cvar_GetAndDescribe("sv_showAverageBPS", "0", 0, "BSP Network debugging");
  sv_collectClientJunkInfo = Cvar_GetAndDescribe("sv_collectClientJunkInfo", "0", CVAR_ARCHIVE, "If this is set, prints if message readcount isn't equal to the message cursize.");

  //systeminfo
  sv_cheats = Cvar_GetAndDescribe("sv_cheats", "1", CVAR_SYSTEMINFO | CVAR_ROM, "Unmodifiable cvar used for certain functions to act differently if the server allows cheats. If you want to turn cheats on, look at devmap.");
  sv_serverid = Cvar_Get("sv_serverid", "0", CVAR_SYSTEMINFO | CVAR_ROM);
  sv_pure = Cvar_GetAndDescribe("sv_pure", "1", CVAR_SYSTEMINFO | CVAR_LATCH, "Requires clients to only get data from pk3 files the server is using.");
  Cvar_Get("sv_paks", "", CVAR_SYSTEMINFO | CVAR_ROM);
  Cvar_Get("sv_pakNames", "", CVAR_SYSTEMINFO | CVAR_ROM);
  Cvar_Get("sv_referencedPaks", "", CVAR_SYSTEMINFO | CVAR_ROM);
  sv_referencedPakNames = Cvar_GetAndDescribe("sv_referencedPakNames", "", CVAR_SYSTEMINFO | CVAR_ROM, "Variable holds a list of all the pk3 files the server loaded data from.");
  //server vars
#if defined(INCLUDE_REMOTE_COMMANDS)
  sv_rconPassword = Cvar_GetAndDescribe("rconPassword", "", CVAR_TEMP, "Password for remote server commands.");
  sv_rconLog = Cvar_GetAndDescribe("sv_rconLog", "", CVAR_ARCHIVE, "Name for the file which stores logs of all rcon commands, double quote to disable.");
#endif
  sv_privatePassword = Cvar_Get("sv_privatePassword", "", CVAR_TEMP);
#if defined(USE_JAVA) || defined(USE_BULLET)
  sv_fps = Cvar_GetAndDescribe("sv_fps", "60", CVAR_TEMP, "Set the max frames per second the server sends the client.");
#else
  sv_fps = Cvar_GetAndDescribe("sv_fps", "20", CVAR_TEMP, "Set the max frames per second the server sends the client.");
#endif
  Cvar_CheckRange(sv_fps, "10", "125", CV_INTEGER);
  sv_timeout = Cvar_GetAndDescribe("sv_timeout", "200", CVAR_TEMP, "Seconds without any message before automatic client disconnect.");
  Cvar_CheckRange(sv_timeout, "4", NULL, CV_INTEGER);
  sv_zombietime = Cvar_GetAndDescribe("sv_zombietime", "2", CVAR_TEMP, "Seconds to sink messages after disconnect.");
  Cvar_CheckRange(sv_zombietime, "1", NULL, CV_INTEGER);
  sv_allowDownload = Cvar_GetAndDescribe("sv_allowDownload", "0", CVAR_SERVERINFO, "Toggle the ability for clients to download files maps etc. from server.");
  sv_hidden = Cvar_GetAndDescribe("sv_hidden", "0", CVAR_ARCHIVE, "Hide the server from queries and from master servers.");
  Cvar_GetAndDescribe("sv_dlURL", "", CVAR_SERVERINFO | CVAR_ARCHIVE, "Disconnects clients and redirects them to download paks from this URL instead of the server. When the download finishes, the client will automatically be reconnected.\nNOTE: if the URL does not have the correct paks, is missing some, or the checksum is mismatched, clients will get dropped!");
  Cvar_Get("sv_wwwDownload", "1", CVAR_SYSTEMINFO | CVAR_ARCHIVE);
  Cvar_Get("sv_wwwBaseURL", "", CVAR_SYSTEMINFO | CVAR_ARCHIVE);
  //moved to Com_Init()
  //sv_master[0] = Cvar_Get("sv_master1", MASTER_SERVER_NAME, CVAR_ARCHIVE_ND | CVAR_PROTECTED);

  //master servers
  for(index = 0;index < MAX_MASTER_SERVERS;index++)
  {
    sv_master[index] = Cvar_Get(va("sv_master%d", index + 1), "", CVAR_ARCHIVE_ND | CVAR_PROTECTED);
  }

  sv_reconnectlimit = Cvar_GetAndDescribe("sv_reconnectlimit", "3", 0, "Number of seconds a disconnected client should wait before next reconnect.");
#if defined(STATELESS_CHALLENGES_VERSION_ONE)
  Cvar_CheckRange(sv_reconnectlimit, "0", "6", CV_INTEGER);
#else
  Cvar_CheckRange(sv_reconnectlimit, "0", "12", CV_INTEGER);
#endif
  sv_showloss = Cvar_Get("sv_showloss", "0", 0);
  sv_padPackets = Cvar_GetAndDescribe("sv_padPackets", "0", CVAR_DEVELOPER, "Adds padding bytes to network packets for rate debugging.");
  sv_killserver = Cvar_GetAndDescribe("sv_killserver", "0", 0, "Internal flag to manage server state.");
  sv_mapChecksum = Cvar_GetAndDescribe("sv_mapChecksum", "", CVAR_ROM, "Allows check for client server map to match.");
  sv_lanForceRate = Cvar_GetAndDescribe("sv_lanForceRate", "1", CVAR_ARCHIVE_ND, "Forces LAN clients to the maximum rate instead of accepting client setting.");
  sv_antiDRDoS = Cvar_GetAndDescribe("sv_antiDRDoS", "0", CVAR_ARCHIVE, "Uses stricter temporary bans to protect against distributed reflected denial-of-service (DRDoS) attacks, in addition to built-in rate limiting.");
  sv_antiDRDoSAffectsLan = Cvar_GetAndDescribe("sv_antiDRDoSAffectsLan", "0", CVAR_ARCHIVE, "Sets whether sv_antiDRDoS guards against LAN clients or not.");
  sv_dequeuePeriod = Cvar_Get("sv_dequeuePeriod", "500", CVAR_ARCHIVE);
  sv_levelTimeReset = Cvar_GetAndDescribe("sv_levelTimeReset", "0", CVAR_ARCHIVE_ND, "Toggle whether or not to reset leveltime after a new map loads.");
  sv_filter = Cvar_GetAndDescribe("sv_filter", "filter.txt", CVAR_ARCHIVE, "Cvar that point on filter file, if it is \"\" then filtering will be disabled.");
  sv_antiWallhack = Cvar_GetAndDescribe("sv_antiWallhack", "0", CVAR_ARCHIVE, "Enables serverside wallhack protection\n0 - disabled\n1: players only\n2: items/structures only\n3: all");
  sv_sendNearbyEnts = Cvar_GetAndDescribe("sv_sendNearbyEnts", "0", CVAR_ARCHIVE, "Toggle whether or not to send nearby entities regardless of pvs or anti wallhack.\nNOTE: range is specified by sv_sendNearbyEntsRange.");
  sv_sendNearbyEntsRange = Cvar_GetAndDescribe("sv_sendNearbyEntsRange", "1500", CVAR_ARCHIVE, "Specifies the range at which entities that fail the pvs check are sent to the client.");
  Cvar_CheckRange(sv_sendNearbyEntsRange, "0", NULL, CV_INTEGER);
  sv_filterCommands = Cvar_GetAndDescribe("sv_filterCommands", "1", CVAR_ARCHIVE, "Toggles whether or not to filter excessive client commands.\n0: only strip essentials\n1: strip \\n and \\r\n2: also strip ;");

#if defined(DEBUG_SV_CHALLENGE)
  sv_debugChallenges = Cvar_GetAndDescribe("sv_debugChallenges", "0", CVAR_ARCHIVE, "Toggles whether or not to print debug messages for serverside stateless challenge generation.");
#endif
}
