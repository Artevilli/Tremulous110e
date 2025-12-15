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

#include "qfiles.h"


void		CM_LoadMap( const qchar *name, qbool clientload, qint *checksum);
void		CM_ClearMap( void );
clipHandle_t CM_InlineModel( qint index );		// 0 = world, 1 + are bmodels
clipHandle_t CM_TempBoxModel( const vec3_t mins, const vec3_t maxs, qint capsule );

void		CM_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs );

qint			CM_NumClusters (void);
qint			CM_NumInlineModels( void );
qchar		*CM_EntityString (void);

// returns an ORed contents mask
qint			CM_PointContents( const vec3_t p, clipHandle_t model );
qint			CM_TransformedPointContents( const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles );

void		CM_BoxTrace ( trace_t *results, const vec3_t start, const vec3_t end,
						  vec3_t mins, vec3_t maxs,
						  clipHandle_t model, qint brushmask, traceType_t type );
void		CM_TransformedBoxTrace( trace_t *results, const vec3_t start, const vec3_t end,
						  vec3_t mins, vec3_t maxs,
						  clipHandle_t model, qint brushmask,
						  const vec3_t origin, const vec3_t angles, traceType_t type );
void		CM_BiSphereTrace( trace_t *results, const vec3_t start,
							const vec3_t end, float startRad, float endRad,
							clipHandle_t model, qint mask );
void		CM_TransformedBiSphereTrace( trace_t *results, const vec3_t start,
							const vec3_t end, float startRad, float endRad,
							clipHandle_t model, qint mask,
							const vec3_t origin );

byte		*CM_ClusterPVS (qint cluster);

qint			CM_PointLeafnum( const vec3_t p );

// only returns non-solid leafs
// overflow if return listsize and if *lastLeaf != list[listsize-1]
qint			CM_BoxLeafnums( const vec3_t mins, const vec3_t maxs, qint *list,
		 					qint listsize, qint *lastLeaf );

qint			CM_LeafCluster (qint leafnum);
qint			CM_LeafArea (qint leafnum);

void		CM_AdjustAreaPortalState( qint area1, qint area2, qbool open );
qbool	CM_AreasConnected( qint area1, qint area2 );

qint			CM_WriteAreaBits( byte *buffer, qint area );

// cm_patch.c
void CM_DrawDebugSurface( void (*drawPoly)(qint color, qint numPoints, float *points) );
