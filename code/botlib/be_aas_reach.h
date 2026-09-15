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
 * name:		be_aas_reach.h
 *
 * desc:		AAS
 *
 * $Archive: /source/code/botlib/be_aas_reach.h $
 *
 *****************************************************************************/

#ifdef AASINTERN
//initialize calculating the reachabilities
void AAS_InitReachability(void);
//continue calculating the reachabilities
qint AAS_ContinueInitReachability(float time);
#if 0
qint AAS_BestReachableLinkArea(aas_link_t *areas);
#endif
#endif //AASINTERN

//returns true if the are has reachabilities to other areas
qint AAS_AreaReachability(qint areanum);
//returns the best reachable area and goal origin for a bounding box at the given origin
qint AAS_BestReachableArea(vec3_t origin, vec3_t mins, vec3_t maxs, vec3_t goalorigin);
//returns the best jumppad area from which the bbox at origin is reachable
qint AAS_BestReachableFromJumpPadArea(vec3_t origin, vec3_t mins, vec3_t maxs);
//returns the next reachability using the given model
qint AAS_NextModelReachability(qint num, qint modelnum);
//returns the total area of the ground faces of the given area
float AAS_AreaGroundFaceArea(qint areanum);
//returns true if the area is crouch only
qint AAS_AreaCrouch(qint areanum);
//returns true if a player can swim in this area
qint AAS_AreaSwim(qint areanum);
//returns true if the area is filled with a liquid
qint AAS_AreaLiquid(qint areanum);
//returns true if the area contains lava
qint AAS_AreaLava(qint areanum);
//returns true if the area contains slime
qint AAS_AreaSlime(qint areanum);
//returns true if the area has one or more ground faces
qint AAS_AreaGrounded(qint areanum);
//returns true if the area is a jump pad
qint AAS_AreaJumpPad(qint areanum);
//returns true if the area is donotenter
qint AAS_AreaDoNotEnter(qint areanum);
