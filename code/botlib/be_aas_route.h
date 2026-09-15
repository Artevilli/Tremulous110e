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
 * name:		be_aas_route.h
 *
 * desc:		AAS
 *
 * $Archive: /source/code/botlib/be_aas_route.h $
 *
 *****************************************************************************/

#ifdef AASINTERN
//initialize the AAS routing
void AAS_InitRouting(void);
//free the AAS routing caches
void AAS_FreeRoutingCaches(void);
//returns the travel time from start to end in the given area
unsigned short qint AAS_AreaTravelTime(qint areanum, vec3_t start, vec3_t end);
//
void AAS_CreateAllRoutingCache(void);
void AAS_WriteRouteCache(void);
//
void AAS_RoutingInfo(void);
#endif //AASINTERN

//returns the travel flag for the given travel type
qint AAS_TravelFlagForType(qint traveltype);
//return the travel flag(s) for traveling through this area
qint AAS_AreaContentsTravelFlags(qint areanum);
//returns the index of the next reachability for the given area
qint AAS_NextAreaReachability(qint areanum, qint reachnum);
//returns the reachability with the given index
void AAS_ReachabilityFromNum(qint num, struct aas_reachability_s *reach);
//returns a random goal area and goal origin
qint AAS_RandomGoalArea(qint areanum, qint travelflags, qint *goalareanum, vec3_t goalorigin);
//enable or disable an area for routing
qint AAS_EnableRoutingArea(qint areanum, qint enable);
//returns the travel time within the given area from start to end
unsigned short qint AAS_AreaTravelTime(qint areanum, vec3_t start, vec3_t end);
//returns the travel time from the area to the goal area using the given travel flags
qint AAS_AreaTravelTimeToGoalArea(qint areanum, vec3_t origin, qint goalareanum, qint travelflags);
//predict a route up to a stop event
qint AAS_PredictRoute(struct aas_predictroute_s *route, qint areanum, vec3_t origin,
							qint goalareanum, qint travelflags, qint maxareas, qint maxtime,
							qint stopevent, qint stopcontents, qint stoptfl, qint stopareanum);


