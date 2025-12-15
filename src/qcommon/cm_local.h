/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2006 Tim Angus

This file is part of Tremulous.

Tremulous is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremulous is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#pragma once

#include "q_shared.h"
#include "qcommon.h"
#include "cm_polylib.h"

#define	MAX_SUBMODELS			256
#define	BOX_MODEL_HANDLE		255
#define CAPSULE_MODEL_HANDLE	254


typedef struct {
	cplane_t	*plane;
	qint			children[2];		// negative numbers are leafs
} cNode_t;

typedef struct {
	qint			cluster;
	qint			area;

	qint			firstLeafBrush;
	qint			numLeafBrushes;

	qint			firstLeafSurface;
	qint			numLeafSurfaces;
} cLeaf_t;

typedef struct cmodel_s {
	vec3_t		mins, maxs;
	cLeaf_t		leaf;			// submodels don't reference the main tree
} cmodel_t;

typedef struct cbrushedge_s
{
	vec3_t	p0;
	vec3_t	p1;
} cbrushedge_t;

typedef struct {
	cplane_t			*plane;
	qint						planeNum;
	qint						surfaceFlags;
	qint						shaderNum;
	winding_t			*winding;
} cbrushside_t;

typedef struct {
	qint			shaderNum;		// the shader that determined the contents
	qint			contents;
	vec3_t		bounds[2];
	qint			numsides;
	cbrushside_t	*sides;
	qint			checkcount;		// to avoid repeated testings
	qbool	collided; // marker for optimisation
	cbrushedge_t	*edges;
	qint						numEdges;
} cbrush_t;


typedef struct {
	qint			checkcount;				// to avoid repeated testings
	qint			surfaceFlags;
	qint			contents;
	struct patchCollide_s	*pc;
} cPatch_t;


typedef struct {
	qint			floodnum;
	qint			floodvalid;
} cArea_t;

typedef struct {
	qchar		name[MAX_QPATH];

	qint			numShaders;
	dshader_t	*shaders;

	qint			numBrushSides;
	cbrushside_t *brushsides;

	qint			numPlanes;
	cplane_t	*planes;

	qint			numNodes;
	cNode_t		*nodes;

	qint			numLeafs;
	cLeaf_t		*leafs;

	qint			numLeafBrushes;
	qint			*leafbrushes;

	qint			numLeafSurfaces;
	qint			*leafsurfaces;

	qint			numSubModels;
	cmodel_t	*cmodels;

	qint			numBrushes;
	cbrush_t	*brushes;

	qint			numClusters;
	qint			clusterBytes;
	byte		*visibility;
	qbool	vised;			// if false, visibility is just a single cluster of ffs

	qint			numEntityChars;
	qchar		*entityString;

	qint			numAreas;
	cArea_t		*areas;
	qint			*areaPortals;	// [ numAreas*numAreas ] reference counts

	qint			numSurfaces;
	cPatch_t	**surfaces;			// non-patches will be NULL

	qint			floodvalid;
	qint			checkcount;					// incremented on each trace
} clipMap_t;


// keep 1/8 unit away to keep the position valid before network snapping
// and to avoid various numeric issues
#define	SURFACE_CLIP_EPSILON	0.125f

extern	clipMap_t	cm;
extern	qint			c_pointcontents;
extern	uint64_t			c_traces, c_brush_traces, c_patch_traces;
extern	cvar_t		*cm_noAreas;
extern	cvar_t		*cm_noCurves;
extern	cvar_t		*cm_playerCurveClip;

// cm_test.c

typedef struct
{
	float		startRadius;
	float		endRadius;
} biSphere_t;

// Used for oriented capsule collision detection
typedef struct
{
	float		radius;
	float		halfheight;
	vec3_t		offset;
} sphere_t;

typedef struct {
	traceType_t	type;
	vec3_t			start;
	vec3_t			end;
	vec3_t			size[2];	// size of the box being swept through the model
	vec3_t			offsets[8];	// [signbits][x] = either size[0][x] or size[1][x]
	float				maxOffset;	// longest corner length from origin
	vec3_t			extents;	// greatest of abs(size[0]) and abs(size[1])
	vec3_t			bounds[2];	// enclosing box of start and end surrounding by size
	vec3_t			modelOrigin;// origin of the model tracing through
	qint					contents;	// ored contents of the model tracing through
	qbool		isPoint;	// optimized case
	trace_t			trace;		// returned from trace call
	sphere_t		sphere;		// sphere for oriendted capsule collision
	biSphere_t	biSphere;
	qbool		testLateralCollision; // whether or not to test for lateral collision
} traceWork_t;

typedef struct leafList_s {
	qint		count;
	qint		maxcount;
	qbool	overflowed;
	qint		*list;
	vec3_t	bounds[2];
	qint		lastLeaf;		// for overflows where each leaf can't be stored individually
	void	(*storeLeafs)( struct leafList_s *ll, qint nodenum );
} leafList_t;


qint CM_BoxBrushes( const vec3_t mins, const vec3_t maxs, cbrush_t **list, qint listsize );

void CM_StoreLeafs( leafList_t *ll, qint nodenum );
void CM_StoreBrushes( leafList_t *ll, qint nodenum );

void CM_BoxLeafnums_r( leafList_t *ll, qint nodenum );

cmodel_t	*CM_ClipHandleToModel( clipHandle_t handle );
qbool CM_BoundsIntersect( const vec3_t mins, const vec3_t maxs, const vec3_t mins2, const vec3_t maxs2 );
qbool CM_BoundsIntersectPoint( const vec3_t mins, const vec3_t maxs, const vec3_t point );

// cm_patch.c

struct patchCollide_s	*CM_GeneratePatchCollide( qint width, qint height, vec3_t *points );
void CM_TraceThroughPatchCollide( traceWork_t *tw, const struct patchCollide_s *pc );
qbool CM_PositionTestInPatchCollide( traceWork_t *tw, const struct patchCollide_s *pc );
void CM_ClearLevelPatches( void );
