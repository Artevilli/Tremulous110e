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

#pragma once

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

//enabling rcon
#define INCLUDE_REMOTE_COMMANDS

//switching between the first and second version of stateless challenges
//#define STATELESS_CHALLENGES_VERSION_ONE

//improve udp download rate control for better performance, especially on slower or less stable connections
//supports larger downloads
#define UDP_DOWNLOAD_OPTIMIZE

//fix client ping calculation to more accurately reflect packet loss, enabled by sv_pingFix, also force minimum ping for humans to 1
#define INCLUDE_SV_PINGFIX

//stateless challenges v2 does not support debugging yet
#if defined(STATELESS_CHALLENGES_VERSION_ONE)
//challenge debugging
#define DEBUG_SV_CHALLENGE //enable for com_dprintf debugging output
#endif

#define CPU_USAGE_WARNING 70
#define FRAME_TIME_WARNING 30

extern cvar_t *sv_fps; //time rate for running non-clients
extern cvar_t *sv_timeout; //seconds without any message
extern cvar_t *sv_zombietime; //seconds to sink messages after disconnect
#if defined(INCLUDE_REMOTE_COMMANDS)
extern cvar_t *sv_rconPassword; //password for remote server commands
extern cvar_t *sv_rconLog; //log file for remote server commands
#endif
extern cvar_t *sv_privatePassword; // password for the privateClient slots
extern cvar_t *sv_hidden;
extern cvar_t *sv_allowDownload;
extern cvar_t *sv_maxclients;
extern cvar_t *sv_maxclientsPerIP;
extern cvar_t *sv_clientTLD;
extern cvar_t *sv_collectClientJunkInfo;
extern cvar_t *sv_cheats;
extern cvar_t *sv_privateClients; //number of clients reserved for password
extern cvar_t *sv_hostname;
extern cvar_t *sv_master[MAX_MASTER_SERVERS]; //master server ip address
extern cvar_t *sv_reconnectlimit; //minimum seconds between connect messages
extern cvar_t *sv_showloss; //report when usercmds are lost
extern cvar_t *sv_padPackets; //add nop bytes to messages
extern cvar_t *sv_killserver; //menu system can set to 1 to shut server down
extern cvar_t *sv_mapname;
extern cvar_t *sv_mapChecksum;
extern cvar_t *sv_referencedPakNames;
extern cvar_t *sv_serverid;
extern cvar_t *sv_minRate;
extern cvar_t *sv_maxRate;
extern cvar_t *sv_maxOOBRate;
extern cvar_t *sv_maxOOBRateIP;
extern cvar_t *sv_dlRate;
extern cvar_t *sv_minSnaps;
extern cvar_t *sv_minRebootDelayMins;
extern cvar_t *sv_novis;
extern cvar_t *sv_pure;
extern cvar_t *sv_cpuusagepublic;
extern cvar_t *sv_avgframetimepublic;
extern cvar_t *sv_warningscpu;
extern cvar_t *sv_warningsframetime;
extern cvar_t *sv_floodWait;
extern cvar_t *sv_floodLimit;
extern cvar_t *sv_floodProtect;
#if defined(INCLUDE_SV_PINGFIX)
extern cvar_t *sv_pingFix;
#endif
extern cvar_t *sv_userInfoFloodProtect;
extern cvar_t *sv_showAverageBPS;
extern cvar_t *sv_lanForceRate; //dedicated 1 (LAN) server forces local client rates to 99999 (bug #491)
extern cvar_t *sv_protect; //attack protection, 0 unpretected, 1 xreal, 2 openwolf, 4 print to console
extern cvar_t *sv_protectLog; //name
extern cvar_t *sv_protectLogInterval; //frequency of writing logs
extern cvar_t *sv_owolfAffectsLan;
extern cvar_t *sv_dequeuePeriod;
extern cvar_t *sv_levelTimeReset;
extern cvar_t *sv_filter;
extern cvar_t *sv_antiWallhack;
extern cvar_t *sv_sendNearbyEnts;
extern cvar_t *sv_sendNearbyEntsRange;
extern cvar_t *sv_filterCommands;

#if defined(DEBUG_SV_CHALLENGE)
extern cvar_t *sv_debugChallenges;
#endif
