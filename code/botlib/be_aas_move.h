/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

/*****************************************************************************
 * name:		be_aas_move.h
 *
 * desc:		AAS
 *
 * $Archive: /source/code/botlib/be_aas_move.h $
 *
 *****************************************************************************/

#ifdef AASINTERN
extern aas_settings_t aassettings;
#endif //AASINTERN

//movement prediction
qint AAS_PredictClientMovement(struct aas_clientmove_s *move,
							qint entnum, const vec3_t origin,
							qint presencetype, qint onground,
							const vec3_t velocity, const vec3_t cmdmove,
							qint cmdframes,
							qint maxframes, float frametime,
							qint stopevent, qint stopareanum, qint visualize);
//predict movement until bounding box is hit
qint AAS_ClientMovementHitBBox(struct aas_clientmove_s *move,
								qint entnum, const vec3_t origin,
								qint presencetype, qint onground,
								const vec3_t velocity, const vec3_t cmdmove,
								qint cmdframes,
								qint maxframes, float frametime,
								const vec3_t mins, const vec3_t maxs, qint visualize);
//returns true if on the ground at the given origin
qint AAS_OnGround(vec3_t origin, qint presencetype, qint passent);
//returns true if swimming at the given origin
qint AAS_Swimming(vec3_t origin);
//returns the jump reachability run start point
void AAS_JumpReachRunStart(struct aas_reachability_s *reach, vec3_t runstart);
//returns true if against a ladder at the given origin
qint AAS_AgainstLadder(vec3_t origin);
//rocket jump Z velocity when rocket-jumping at origin
float AAS_RocketJumpZVelocity(vec3_t origin);
//bfg jump Z velocity when bfg-jumping at origin
float AAS_BFGJumpZVelocity(vec3_t origin);
//calculates the horizontal velocity needed for a jump and returns true this velocity could be calculated
qint AAS_HorizontalVelocityForJump(float zvel, vec3_t start, vec3_t end, float *velocity);
//
void AAS_SetMovedir(vec3_t angles, vec3_t movedir);
//
qint AAS_DropToFloor(vec3_t origin, vec3_t mins, vec3_t maxs);
//
void AAS_InitSettings(void);
