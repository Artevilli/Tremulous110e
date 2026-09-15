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
 * name:		be_aas_sample.h
 *
 * desc:		AAS
 *
 * $Archive: /source/code/botlib/be_aas_sample.h $
 *
 *****************************************************************************/

#ifdef AASINTERN
void AAS_InitAASLinkHeap(void);
void AAS_InitAASLinkedEntities(void);
void AAS_FreeAASLinkHeap(void);
void AAS_FreeAASLinkedEntities(void);
#if 0
aas_face_t *AAS_AreaGroundFace(qint areanum, vec3_t point);
#endif
aas_face_t *AAS_TraceEndFace(aas_trace_t *trace);
aas_plane_t *AAS_PlaneFromNum(qint planenum);
aas_link_t *AAS_AASLinkEntity(vec3_t absmins, vec3_t absmaxs, qint entnum);
aas_link_t *AAS_LinkEntityClientBBox(vec3_t absmins, vec3_t absmaxs, qint entnum, qint presencetype);
qbool AAS_PointInsideFace(qint facenum, vec3_t point, float epsilon);
void AAS_UnlinkFromAreas(aas_link_t *areas);
#endif //AASINTERN

//returns the mins and maxs of the bounding box for the given presence type
void AAS_PresenceTypeBoundingBox(qint presencetype, vec3_t mins, vec3_t maxs);
//returns the cluster the area is in (negative portal number if the area is a portal)
qint AAS_AreaCluster(qint areanum);
//returns the presence type(s) of the area
qint AAS_AreaPresenceType(qint areanum);
//returns the presence type(s) at the given point
qint AAS_PointPresenceType(vec3_t point);
//returns the result of the trace of a client bbox
aas_trace_t AAS_TraceClientBBox(vec3_t start, vec3_t end, qint presencetype, qint passent);
//stores the areas the trace went through and returns the number of passed areas
qint AAS_TraceAreas(vec3_t start, vec3_t end, qint *areas, vec3_t *points, qint maxareas);
//returns the areas the bounding box is in
qint AAS_BBoxAreas(vec3_t absmins, vec3_t absmaxs, qint *areas, qint maxareas);
//return area information
qint AAS_AreaInfo( qint areanum, aas_areainfo_t *info );
//returns the area the point is in
qint AAS_PointAreaNum(vec3_t point);
//
qint AAS_PointReachabilityAreaIndex( vec3_t point );
#if 0
//returns the plane the given face is in
void AAS_FacePlane(qint facenum, vec3_t normal, float *dist);
#endif


