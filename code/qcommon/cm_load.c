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
// cmodel.c -- model loading

#include "cm_local.h"

#if defined(BSPC)

#include "../bspc/l_qfiles.h"

void
SetPlaneSignbits(cplane_t *out)
{
  qint bits;
  qint j;

  //for fast box on planeside test
  bits = 0;

  for(j = 0;j < 3;j++)
  {
    if (out->normal[j] < 0)
    {
      bits |= BIT(j);
    }
  }

  out->signbits = bits;
}
#endif //BSPC

// to allow boxes to be treated as brush models, we allocate
// some extra indexes along with those needed by the map
#define BOX_BRUSHES 1
#define BOX_SIDES 6
#define BOX_LEAFS 2
#define BOX_PLANES 12

#define LL(x) x=LittleLong(x)


clipMap_t cm;
qint c_pointcontents;
qint c_traces;
qint c_brush_traces;
qint c_patch_traces;


byte *cmod_base;

#if !defined(BSPC)
cvar_t *cm_noAreas;
cvar_t *cm_noCurves;
cvar_t *cm_playerCurveClip;
#endif

cmodel_t box_model;
cplane_t *box_planes;
cbrush_t *box_brush;



void
CM_InitBoxHull(void);
void
CM_FloodAreaConnections(void);


/*
===============================================================================

					MAP LOADING

===============================================================================
*/

/*
=================
CMod_LoadShaders
=================
*/
static void
CMod_LoadShaders(const lump_t *l)
{
  dshader_t *in;
  dshader_t *out;
  qint i;
  qint count;

  in = (void *)(cmod_base + l->fileofs);

  if (l->filelen % sizeof(*in))
  {
    Com_Error(ERR_DROP, "%s: funny lump size", __func__);
  }

  count = l->filelen / sizeof(*in);

  if (count < 1)
  {
    Com_Error(ERR_DROP, "%s: map with no shaders", __func__);
  }

  cm.shaders = Hunk_Alloc(count * sizeof(*cm.shaders), h_high);
  cm.numShaders = count;

  Com_Memcpy(cm.shaders, in, count * sizeof(*cm.shaders));

  out = cm.shaders;

  for(i = 0;i < count;i++, in++, out++)
  {
    out->contentFlags = LittleLong(out->contentFlags);
    out->surfaceFlags = LittleLong(out->surfaceFlags);
  }
}


/*
=================
CMod_LoadSubmodels
=================
*/
static void
CMod_LoadSubmodels(const lump_t *l)
{
  dmodel_t *in;
  cmodel_t *out;
  qint i;
  qint j;
  qint count;
  qint *indexes;
  unsigned firstBrush;
  unsigned numBrushes;
  unsigned firstSurface;
  unsigned numSurfaces;

  in = (void *)(cmod_base + l->fileofs);

  if (l->filelen % sizeof(*in))
  {
    Com_Error(ERR_DROP, "%s: funny lump size", __func__);
  }

  count = l->filelen / sizeof(*in);

  if (count < 1)
  {
    Com_Error(ERR_DROP, "%s: map with no models", __func__);
  }

  if (count > MAX_SUBMODELS)
  {
    Com_Error(ERR_DROP, "%s: MAX_SUBMODELS exceeded", __func__);
  }

  cm.cmodels = Hunk_Alloc(count * sizeof(*cm.cmodels), h_high);
  cm.numSubModels = count;

  for(i = 0;i < count;i++, in++)
  {
    out = &cm.cmodels[i];

    for(j = 0;j < 3;j++) // spread the mins / maxs by a pixel
    {
      out->mins[j] = LittleFloat (in->mins[j]) - 1;
      out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
    }

    if (i == 0)
    {
      continue; //world model doesn't need other info
    }

    firstBrush = LittleLong(in->firstBrush);
    numBrushes = LittleLong(in->numBrushes);

    if ((uint64_t)firstBrush + numBrushes > cm.numBrushes)
    {
      Com_Error(ERR_DROP, "%s: bad brushes", __func__);
    }

    //make a "leaf" just to hold the model's brushes and surfaces
    out->leaf.numLeafBrushes = numBrushes;
    indexes = Hunk_Alloc(numBrushes * sizeof(*indexes), h_high);
    out->leaf.firstLeafBrush = indexes - cm.leafbrushes;

    for(j = 0;j < numBrushes;j++)
    {
      indexes[j] = firstBrush + j;
    }

    firstSurface = LittleLong(in->firstSurface);
    numSurfaces = LittleLong(in->numSurfaces);

    if ((uint64_t)firstSurface + numSurfaces > cm.numSurfaces)
    {
      Com_Error(ERR_DROP, "%s: bad surfaces", __func__);
    }

    out->leaf.numLeafSurfaces = numSurfaces;
    indexes = Hunk_Alloc(numSurfaces * sizeof(*indexes), h_high);
    out->leaf.firstLeafSurface = indexes - cm.leafsurfaces;

    for(j = 0;j < numSurfaces;j++)
    {
      indexes[j] = firstSurface + j;
    }
  }
}


/*
=================
CMod_LoadNodes
=================
*/
static void
CMod_LoadNodes(const lump_t *l)
{
  dnode_t *in;
  cNode_t *out;
  qint i;
  qint j;
  qint count;
  unsigned child;
  unsigned num;

  in = (dnode_t *)(cmod_base + l->fileofs);

  if (l->filelen % sizeof(*in))
  {
    Com_Error(ERR_DROP, "%s: funny lump size", __func__);
  }

  count = l->filelen / sizeof(*in);

  if (count < 1)
  {
    Com_Error(ERR_DROP, "%s: map has no nodes", __func__);
  }

  cm.nodes = Hunk_Alloc(count * sizeof(*cm.nodes), h_high);
  cm.numNodes = count;

  out = cm.nodes;

  for(i = 0;i < count;i++, out++, in++)
  {
    num = LittleLong(in->planeNum);

    if (num >= cm.numPlanes)
    {
      Com_Error(ERR_DROP, "%s: bad planeNum", __func__);
    }

    out->plane = cm.planes + num;

    for(j = 0;j < 2;j++)
    {
      child = LittleLong(in->children[j]);

      if (child & 0x80000000)
      {
        if (~child >= cm.numLeafs)
        {
          Com_Error(ERR_DROP, "%s: bad leaf", __func__);
        }
      }
      else
      {
        if (child >= count)
        {
          Com_Error(ERR_DROP, "%s: bad node", __func__);
        }
      }

      out->children[j] = child;
    }
  }
}

/*
=================
CM_BoundBrush
=================
*/
void
CM_BoundBrush(cbrush_t *b)
{
  b->bounds[0][0] = -b->sides[0].plane->dist;
  b->bounds[1][0] = b->sides[1].plane->dist;

  b->bounds[0][1] = -b->sides[2].plane->dist;
  b->bounds[1][1] = b->sides[3].plane->dist;

  b->bounds[0][2] = -b->sides[4].plane->dist;
  b->bounds[1][2] = b->sides[5].plane->dist;
}


/*
=================
CMod_LoadBrushes
=================
*/
static void
CMod_LoadBrushes(const lump_t *l)
{
  dbrush_t *in;
  cbrush_t *out;
  qint i;
  qint count;
  unsigned firstSide;
  unsigned numSides;

  in = (void *)(cmod_base + l->fileofs);

  if (l->filelen % sizeof(*in))
  {
    Com_Error(ERR_DROP, "%s: funny lump size", __func__);
  }

  count = l->filelen / sizeof(*in);

  cm.brushes = Hunk_Alloc((BOX_BRUSHES + count) * sizeof(*cm.brushes), h_high);
  cm.numBrushes = count;

  out = cm.brushes;

  for(i = 0;i < count;i++, out++, in++)
  {
    firstSide = LittleLong(in->firstSide);
    numSides = LittleLong(in->numSides);

    if ((uint64_t)firstSide + numSides > cm.numBrushSides)
    {
      Com_Error(ERR_DROP, "%s: bad brushsides", __func__);
    }

    out->sides = cm.brushsides + firstSide;
    out->numsides = numSides;

    out->shaderNum = LittleLong(in->shaderNum);

    if (out->shaderNum < 0 || out->shaderNum >= cm.numShaders)
    {
      Com_Error(ERR_DROP, "%s: bad shaderNum: %i", __func__, out->shaderNum);
    }

    out->contents = cm.shaders[out->shaderNum].contentFlags;

    CM_BoundBrush(out);
  }
}

/*
=================
CMod_LoadLeafs
=================
*/
static void
CMod_LoadLeafs(const lump_t *l)
{
  qint i;
  cLeaf_t *out;
  dleaf_t *in;
  qint count;
  unsigned firstLeafBrush;
  unsigned numLeafBrushes;
  unsigned firstLeafSurface;
  unsigned numLeafSurfaces;

  in = (void *)(cmod_base + l->fileofs);

  if (l->filelen % sizeof(*in))
  {
    Com_Error(ERR_DROP, "%s: funny lump size", __func__);
  }

  count = l->filelen / sizeof(*in);

  if (count < 1)
  {
    Com_Error(ERR_DROP, "%s: map with no leafs", __func__);
  }

  cm.leafs = Hunk_Alloc((BOX_LEAFS + count) * sizeof(*cm.leafs), h_high);
  cm.numLeafs = count;

  out = cm.leafs;

  for(i = 0;i < count;i++, in++, out++)
  {
    out->cluster = LittleLong(in->cluster);

    if (out->cluster + 1U > INT_MAX - 63U)
    {
      Com_Error(ERR_DROP, "%s: bad cluster", __func__);
    }

    out->area = LittleLong(in->area);

    if (out->area + 1U > MAX_MAP_AREAS)
    {
      Com_Error(ERR_DROP, "%s: bad area", __func__);
    }

    firstLeafBrush = LittleLong(in->firstLeafBrush);
    numLeafBrushes = LittleLong(in->numLeafBrushes);

    if ((uint64_t)firstLeafBrush + numLeafBrushes > cm.numLeafBrushes)
    {
      Com_Error(ERR_DROP, "%s: bad leafbrushes", __func__);
    }

    out->firstLeafBrush = firstLeafBrush;
    out->numLeafBrushes = numLeafBrushes;

    firstLeafSurface = LittleLong(in->firstLeafSurface);
    numLeafSurfaces = LittleLong(in->numLeafSurfaces);

    if ((uint64_t)firstLeafSurface + numLeafSurfaces > cm.numLeafSurfaces)
    {
      Com_Error(ERR_DROP, "%s: bad leafsurfaces", __func__);
    }

    out->firstLeafSurface = firstLeafSurface;
    out->numLeafSurfaces = numLeafSurfaces;

    if (out->cluster >= cm.numClusters)
    {
      cm.numClusters = out->cluster + 1;
    }

    if (out->area >= cm.numAreas)
    {
      cm.numAreas = out->area + 1;
    }
  }

  cm.areas = Hunk_Alloc(cm.numAreas * sizeof(*cm.areas), h_high);
  cm.areaPortals = Hunk_Alloc(cm.numAreas * cm.numAreas * sizeof(*cm.areaPortals), h_high);
}

/*
=================
CMod_LoadPlanes
=================
*/
void
CMod_LoadPlanes(const lump_t *l)
{
  qint i;
  qint j;
  cplane_t *out;
  dplane_t *in;
  qint count;
  qint bits;

  in = (void *)(cmod_base + l->fileofs);

  if (l->filelen % sizeof(*in))
  {
    Com_Error(ERR_DROP, "%s: funny lump size", __func__);
  }

  count = l->filelen / sizeof(*in);

  if (count < 1)
  {
    Com_Error(ERR_DROP, "%s: map with no planes", __func__);
  }

  cm.planes = Hunk_Alloc((BOX_PLANES + count) * sizeof(*cm.planes), h_high);
  cm.numPlanes = count;

  out = cm.planes;

  for(i = 0;i < count;i++, in++, out++)
  {
    bits = 0;

    for(j = 0;j < 3;j++)
    {
      out->normal[j] = LittleFloat (in->normal[j]);

      if (out->normal[j] < 0)
      {
        bits |= BIT(j);
      }
    }

    out->dist = LittleFloat(in->dist);
    out->type = PlaneTypeForNormal(out->normal);
    out->signbits = bits;
  }
}

/*
=================
CMod_LoadLeafBrushes
=================
*/
static void
CMod_LoadLeafBrushes(const lump_t *l)
{
  qint i;
  qint *out;
  qint *in;
  qint count;

  in = (void *)(cmod_base + l->fileofs);

  if (l->filelen % sizeof(*in))
  {
    Com_Error(ERR_DROP, "%s: funny lump size", __func__);
  }

  count = l->filelen / sizeof(*in);

  cm.leafbrushes = Hunk_Alloc((count + BOX_BRUSHES) * sizeof(*cm.leafbrushes), h_high);
  cm.numLeafBrushes = count;

  out = cm.leafbrushes;

  for(i = 0;i < count;i++, in++, out++)
  {
    unsigned j = LittleLong(*in);

    if (j >= cm.numBrushes)
    {
      Com_Error(ERR_DROP, "%s: bad brush", __func__);
    }

    *out = j;
  }
}

/*
=================
CMod_LoadLeafSurfaces
=================
*/
static void
CMod_LoadLeafSurfaces(const lump_t *l)
{
  qint i;
  qint *out;
  qint *in;
  qint count;

  in = (void *)(cmod_base + l->fileofs);

  if (l->filelen % sizeof(*in))
  {
    Com_Error(ERR_DROP, "%s: funny lump size", __func__);
  }

  count = l->filelen / sizeof(*in);

  cm.leafsurfaces = Hunk_Alloc(count * sizeof(*cm.leafsurfaces), h_high);
  cm.numLeafSurfaces = count;

  out = cm.leafsurfaces;

  for(i = 0;i < count;i++, in++, out++)
  {
    unsigned j = LittleLong(*in);

    if (j >= cm.numSurfaces)
    {
      Com_Error(ERR_DROP, "%s: bad surface", __func__);
    }

    *out = j;
  }
}

/*
=================
CMod_LoadBrushSides
=================
*/
static void
CMod_LoadBrushSides(const lump_t *l)
{
  qint i;
  cbrushside_t *out;
  dbrushside_t *in;
  qint count;
  unsigned num;

  in = (dbrushside_t *)(cmod_base + l->fileofs);

  if (l->filelen % sizeof(*in))
  {
    Com_Error(ERR_DROP, "%s: funny lump size", __func__);
  }

  count = l->filelen / sizeof(*in);

  cm.brushsides = Hunk_Alloc((BOX_SIDES + count) * sizeof(*cm.brushsides), h_high);
  cm.numBrushSides = count;

  out = cm.brushsides;

  for(i = 0;i < count;i++, in++, out++)
  {
    num = LittleLong(in->planeNum);

    if (num >= cm.numPlanes)
    {
      Com_Error(ERR_DROP, "%s: bad planeNum", __func__);
    }

    out->planeNum = num;
    out->plane = &cm.planes[num];
    out->shaderNum = LittleLong(in->shaderNum);

    if (out->shaderNum < 0 || out->shaderNum >= cm.numShaders)
    {
      Com_Error(ERR_DROP, "%s: bad shaderNum: %i", __func__, out->shaderNum);
    }

    out->surfaceFlags = cm.shaders[out->shaderNum].surfaceFlags;
  }
}

#define CM_EDGE_VERTEX_EPSILON 0.1f

/*
=================
CMod_BrushEdgesAreTheSame
=================
*/
static qbool
CMod_BrushEdgesAreTheSame(const vec3_t p0, const vec3_t p1, const vec3_t q0, const vec3_t q1)
{
  if (VectorCompareEpsilon(p0, q0, CM_EDGE_VERTEX_EPSILON) && VectorCompareEpsilon(p1, q1, CM_EDGE_VERTEX_EPSILON))
  {
    return qtrue;
  }

  if (VectorCompareEpsilon(p1, q0, CM_EDGE_VERTEX_EPSILON) && VectorCompareEpsilon(p0, q1, CM_EDGE_VERTEX_EPSILON))
  {
    return qtrue;
  }

  return qfalse;
}

/*
=================
CMod_AddEdgeToBrush
=================
*/
static qbool
CMod_AddEdgeToBrush(const vec3_t p0, const vec3_t p1, cbrushedge_t *edges, qint *numEdges)
{
  qint i;

  if (!edges || !numEdges)
  {
    return qfalse;
  }

  for(i = 0;i < *numEdges;i++)
  {
    if (CMod_BrushEdgesAreTheSame(p0, p1, edges[i].p0, edges[i].p1))
    {
      return qfalse;
    }
  }

  VectorCopy(p0, edges[*numEdges].p0);
  VectorCopy(p1, edges[*numEdges].p1);
  (*numEdges)++;

  return qtrue;
}

/*
=================
CMod_CreateBrushSideWindings
=================
*/
static void
CMod_CreateBrushSideWindings(void)
{
  qint i;
  qint j;
  qint k;
  winding_t *w;
  cbrushside_t *side;
  cbrushside_t *chopSide;
  cplane_t *plane;
  cbrush_t *brush;
  cbrushedge_t *tempEdges;
  qint numEdges;
  qint edgesAlloc;
  qint totalEdgesAlloc = 0;
  qint totalEdges = 0;

  for(i = 0;i < cm.numBrushes;i++)
  {
    brush = &cm.brushes[i];
    numEdges = 0;

    //walk the list of brush sides
    for(j = 0;j < brush->numsides;j++)
    {
      //get side and plane
      side = &brush->sides[j];
      plane = side->plane;

      w = BaseWindingForPlane(plane->normal, plane->dist);

      //walk the list of brush sides
      for(k = 0;k < brush->numsides && w != NULL;k++)
      {
        chopSide = &brush->sides[k];

        if (chopSide == side)
        {
          continue;
        }

        if (chopSide->planeNum == (side->planeNum ^ 1))
        {
          continue; //back side clipaway
        }

        plane = &cm.planes[chopSide->planeNum ^ 1];
        ChopWindingInPlace(&w, plane->normal, plane->dist, 0);
      }

      if (w)
      {
        numEdges += w->numpoints;
      }

      //set side winding
      side->winding = w;
    }

    //Allocate a temporary buffer of the maximal size
    tempEdges = (cbrushedge_t *)Z_Malloc(sizeof(cbrushedge_t) * numEdges);
    brush->numEdges = 0;

    //compose the points into edges
    for(j = 0;j < brush->numsides;j++)
    {
      side = &brush->sides[j];

      if (side->winding)
      {
        for(k = 0;k < side->winding->numpoints - 1;k++)
        {
          if (brush->numEdges == numEdges)
          {
            Com_Error(ERR_FATAL, "Insufficient memory allocated for collision map edges");
          }

          CMod_AddEdgeToBrush(side->winding->p[k], side->winding->p[k + 1], tempEdges, &brush->numEdges);
        }

        FreeWinding(side->winding);
        side->winding = NULL;
      }
    }

    //Allocate a buffer of the actual size
    edgesAlloc = sizeof(cbrushedge_t) * brush->numEdges;
    totalEdgesAlloc += edgesAlloc;
    brush->edges = (cbrushedge_t *)Hunk_Alloc(edgesAlloc, h_low);

    //Copy temporary buffer to permanent buffer
    Com_Memcpy(brush->edges, tempEdges, edgesAlloc);

    //Free temporary buffer
    Z_Free(tempEdges);

    totalEdges += brush->numEdges;
  }

  Com_DPrintf("Allocated %d bytes for %d collision map edges...\n", totalEdgesAlloc, totalEdges);
}

/*
=================
CMod_LoadEntityString
=================
*/
static void
CMod_LoadEntityString(const lump_t *l)
{
  cm.entityString = Hunk_Alloc(l->filelen + 1, h_high);
  cm.numEntityChars = l->filelen;
  Com_Memcpy(cm.entityString, cmod_base + l->fileofs, l->filelen);
  cm.entityString[l->filelen] = 0;
}

/*
=================
CMod_LoadVisibility
=================
*/
#define VIS_HEADER 8
static void
CMod_LoadVisibility(const lump_t *l)
{
  unsigned numClusters;
  unsigned clusterBytes;
  unsigned len;
  byte *buf;

  len = PAD(cm.numClusters, 64) >> 3;
  cm.novis = Hunk_Alloc(len, h_high);
  Com_Memset(cm.novis, 0xff, len);

  len = l->filelen;

  if (!len)
  {
    return;
  }

  if (len < VIS_HEADER)
  {
    Com_Error(ERR_DROP, "%s: lump too short", __func__);
  }

  buf = cmod_base + l->fileofs;
  numClusters = LittleLong(((qint *)buf)[0]);
  clusterBytes = LittleLong(((qint *)buf)[1]);

  buf += VIS_HEADER;
  len -= VIS_HEADER;

  if ((uint64_t)numClusters * clusterBytes > len)
  {
    Com_Error(ERR_DROP, "%s: lump too short", __func__);
  }

  if (numClusters < cm.numClusters)
  {
    Com_Error(ERR_DROP, "%s: bad numClusters", __func__);
  }

  if (clusterBytes < (numClusters + 7) >> 3)
  {
    Com_Error(ERR_DROP, "%s: bad clusterBytes", __func__);
  }

  cm.visibility = Hunk_Alloc(len, h_high);
  cm.numClusters = numClusters;
  cm.clusterBytes = clusterBytes;
  Com_Memcpy(cm.visibility, buf, len);
}

//==================================================================


/*
=================
CMod_LoadPatches
=================
*/
#define MAX_PATCH_VERTS 1024
static void
CMod_LoadPatches(const lump_t *surfs, const lump_t *verts)
{
  drawVert_t *dv;
  drawVert_t *dv_p;
  dsurface_t *in;
  qint count;
  qint i;
  qint j;
  unsigned firstVert;
  unsigned numVerts;
  unsigned totalVerts;
  cPatch_t *patch;
  vec3_t points[MAX_PATCH_VERTS];
  unsigned width;
  unsigned height;
  unsigned shaderNum;

  in = (void *)(cmod_base + surfs->fileofs);

  if (surfs->filelen % sizeof(*in))
  {
    Com_Error (ERR_DROP, "%s: funny lump size", __func__);
  }

  cm.numSurfaces = count = surfs->filelen / sizeof(*in);
  cm.surfaces = Hunk_Alloc(cm.numSurfaces * sizeof(cm.surfaces[0]), h_high);

  dv = (void *)(cmod_base + verts->fileofs);

  if (verts->filelen % sizeof(*dv))
  {
    Com_Error(ERR_DROP, "%s: funny lump size", __func__);
  }

  totalVerts = verts->filelen / sizeof(*dv);

  //scan through all the surfaces, but only load patches,
  //not planar faces
  for(i = 0;i < count;i++, in++)
  {
    if (LittleLong(in->surfaceType) != MST_PATCH)
    {
      continue; //ignore other surfaces
    }

    //FIXME: check for non-colliding patches

    cm.surfaces[i] = patch = Hunk_Alloc(sizeof(*patch), h_high);

    //load the full drawverts onto the stack
    width = LittleLong(in->patchWidth);
    height = LittleLong(in->patchHeight);

    if ((uint64_t)width * height > MAX_PATCH_VERTS)
    {
      Com_Error(ERR_DROP, "%s: MAX_PATCH_VERTS", __func__);
    }

    firstVert = LittleLong(in->firstVert);
    numVerts = width * height;

    if ((uint64_t)firstVert + numVerts > totalVerts)
    {
      Com_Error(ERR_DROP, "%s: bad firstVert", __func__);
    }

    dv_p = dv + firstVert;

    for(j = 0;j < numVerts;j++, dv_p++)
    {
      points[j][0] = LittleFloat(dv_p->xyz[0]);
      points[j][1] = LittleFloat(dv_p->xyz[1]);
      points[j][2] = LittleFloat(dv_p->xyz[2]);
    }

    shaderNum = LittleLong(in->shaderNum);

    if (shaderNum >= cm.numShaders)
    {
      Com_Error(ERR_DROP, "%s: bad shaderNum", __func__);
    }

    patch->contents = cm.shaders[shaderNum].contentFlags;
    patch->surfaceFlags = cm.shaders[shaderNum].surfaceFlags;

    //create the internal facet structure
    patch->pc = CM_GeneratePatchCollide(width, height, points);
  }
}

//==================================================================
#if 0
static uint32_t
CM_LumpChecksum(const lump_t *lump)
{
  return LittleLong(Com_BlockChecksum(cmod_base + lump->fileofs, lump->filelen));
}

static uint32_t
CM_Checksum(const dheader_t *header)
{
  uint32_t checksums[11];

  checksums[0] = CM_LumpChecksum(&header->lumps[LUMP_SHADERS]);
  checksums[1] = CM_LumpChecksum(&header->lumps[LUMP_LEAFS]);
  checksums[2] = CM_LumpChecksum(&header->lumps[LUMP_LEAFBRUSHES]);
  checksums[3] = CM_LumpChecksum(&header->lumps[LUMP_LEAFSURFACES]);
  checksums[4] = CM_LumpChecksum(&header->lumps[LUMP_PLANES]);
  checksums[5] = CM_LumpChecksum(&header->lumps[LUMP_BRUSHSIDES]);
  checksums[6] = CM_LumpChecksum(&header->lumps[LUMP_BRUSHES]);
  checksums[7] = CM_LumpChecksum(&header->lumps[LUMP_MODELS]);
  checksums[8] = CM_LumpChecksum(&header->lumps[LUMP_NODES]);
  checksums[9] = CM_LumpChecksum(&header->lumps[LUMP_SURFACES]);
  checksums[10] = CM_LumpChecksum(&header->lumps[LUMP_DRAWVERTS]);

  return LittleLong(Com_BlockChecksum(checksums, ARRAY_LEN(checksums) * 4));
}
#endif

static void
CM_ValidateTree_r(byte *visited, qint node)
{
  while(node >= 0)
  {
    if (visited[node])
    {
      Com_Error(ERR_DROP, "%s: cycle encountered", __func__);
    }

    visited[node] = 1;
    CM_ValidateTree_r(visited, cm.nodes[node].children[0]);
    node = cm.nodes[node].children[1];
  }
}

static void
CM_ValidateTree(void)
{
  byte *visited = Hunk_AllocateTempMemory(cm.numNodes);

  Com_Memset(visited, 0, cm.numNodes);
  CM_ValidateTree_r(visited, 0);
  Hunk_FreeTempMemory(visited);
}

/*
==================
CM_LoadMap

Loads in the map and all submodels
==================
*/
void
CM_LoadMap(const qchar *name, qbool clientload, qint *checksum)
{
  void *buf;
  qint i;
  dheader_t header;
  qint length;

  if (!name || !name[0])
  {
    Com_Error(ERR_DROP, "%s: NULL name", __func__);
  }

#if !defined(BSPC)
  cm_noAreas = Cvar_GetAndDescribe("cm_noAreas", "0", CVAR_CHEAT, "Do not use areaportals, all areas are connected.");
  cm_noCurves = Cvar_GetAndDescribe("cm_noCurves", "0", CVAR_CHEAT, "Do not collide against curves.");
  cm_playerCurveClip = Cvar_GetAndDescribe("cm_playerCurveClip", "1", CVAR_ARCHIVE_ND | CVAR_CHEAT, "Collide against player curves.");
#endif
  Com_DPrintf("%s('%s', %i)\n", __func__, name, clientload);

  if (!strcmp(cm.name, name) && clientload)
  {
    *checksum = cm.checksum;
    return;
  }

  //free old stuff
  CM_ClearMap();

#if 0
  if (!name[0])
  {
    cm.numLeafs = 1;
    cm.numClusters = 1;
    cm.numAreas = 1;
    cm.cmodels = Hunk_Alloc(sizeof(*cm.cmodels), h_high);
    *checksum = 0;
    return;
  }
#endif

  //
  //load the file
  //
#if !defined(BSPC)
  length = FS_ReadFile(name, &buf);
#else
  length = LoadQuakeFile((quakefile_t *) name, &buf);
#endif

  if (!buf)
  {
    Com_Error(ERR_DROP, "%s: couldn't load %s", __func__, name);
  }

  if (length < sizeof(dheader_t))
  {
    Com_Error(ERR_DROP, "%s: %s has truncated header", __func__, name);
  }

  *checksum = cm.checksum = LittleLong(Com_BlockChecksum(buf, length));

  header = *(dheader_t *)buf;

  for(i = 0;i < sizeof(dheader_t) / 4;i++)
  {
    ((int32_t *)&header)[i] = LittleLong (((int32_t *)&header)[i]);
  }

  if (header.version != BSP_VERSION)
  {
    Com_Error(ERR_DROP, "%s: %s has wrong version number (%i should be %i)", __func__, name, header.version, BSP_VERSION);
  }

  for(i = 0;i < HEADER_LUMPS;i++)
  {
    uint32_t ofs = header.lumps[i].fileofs;
    uint32_t len = header.lumps[i].filelen;

    if ((uint64_t)ofs + len > length)
    {
      Com_Error(ERR_DROP, "%s: %s has wrong lump[%i] size/offset", __func__, name, i);
    }
  }

  cmod_base = (byte *)buf;

  //pre-calculate some stuff
  cm.numBrushes = header.lumps[LUMP_BRUSHES].filelen / sizeof(dbrush_t);
  cm.numSurfaces = header.lumps[LUMP_SURFACES].filelen / sizeof(dsurface_t);

  //load into heap
  CMod_LoadShaders(&header.lumps[LUMP_SHADERS]);
  CMod_LoadLeafBrushes(&header.lumps[LUMP_LEAFBRUSHES]);
  CMod_LoadLeafSurfaces(&header.lumps[LUMP_LEAFSURFACES]);
  CMod_LoadPlanes(&header.lumps[LUMP_PLANES]);
  CMod_LoadBrushSides(&header.lumps[LUMP_BRUSHSIDES]);
  CMod_LoadBrushes(&header.lumps[LUMP_BRUSHES]);
  CMod_LoadSubmodels(&header.lumps[LUMP_MODELS]);
  CMod_LoadLeafs(&header.lumps[LUMP_LEAFS]);
  CMod_LoadNodes(&header.lumps[LUMP_NODES]);
  CMod_LoadEntityString(&header.lumps[LUMP_ENTITIES]);
  CMod_LoadVisibility(&header.lumps[LUMP_VISIBILITY]);
  CMod_LoadPatches(&header.lumps[LUMP_SURFACES], &header.lumps[LUMP_DRAWVERTS]);

  CMod_CreateBrushSideWindings();

  //we are NOT freeing the file, because it is cached for the ref
  FS_FreeFile(buf);

  //check for cycles so we don't overflow stack
  CM_ValidateTree();

  CM_InitBoxHull();

  CM_FloodAreaConnections();

  //allow this to be cached if it is loaded by the server
  if (!clientload)
  {
    Q_strncpyz(cm.name, name, sizeof(cm.name));
  }
}

/*
==================
CM_ClearMap
==================
*/
void
CM_ClearMap(void)
{
  Com_Memset(&cm, 0, sizeof(cm));
  CM_ClearLevelPatches();
}

/*
==================
CM_ClipHandleToModel
==================
*/
cmodel_t *
CM_ClipHandleToModel(clipHandle_t handle)
{
  if (handle < 0)
  {
    Com_Error(ERR_DROP, "CM_ClipHandleToModel: bad handle %i", handle);
  }

  if (handle < cm.numSubModels)
  {
    return &cm.cmodels[handle];
  }

  if (handle == BOX_MODEL_HANDLE)
  {
    return &box_model;
  }

  if (handle < MAX_SUBMODELS)
  {
    Com_Error(ERR_DROP, "CM_ClipHandleToModel: bad handle %i < %i < %i", cm.numSubModels, handle, MAX_SUBMODELS);
  }

  Com_Error(ERR_DROP, "CM_ClipHandleToModel: bad handle %i", handle + MAX_SUBMODELS);

  return NULL;

}

/*
==================
CM_InlineModel
==================
*/
clipHandle_t
CM_InlineModel(qint index)
{
  if (index < 0 || index >= cm.numSubModels)
  {
    Com_Error(ERR_DROP, "CM_InlineModel: bad number");
  }

  return index;
}

qint
CM_NumClusters(void)
{
  return cm.numClusters;
}

qint
CM_NumInlineModels(void)
{
  return cm.numSubModels;
}

qchar *
CM_EntityString(void)
{
  return cm.entityString;
}

qint
CM_LeafCluster(qint leafnum)
{
  if (leafnum < 0 || leafnum >= cm.numLeafs)
  {
    Com_Error(ERR_DROP, "CM_LeafCluster: bad number");
  }

  return cm.leafs[leafnum].cluster;
}

qint
CM_LeafArea(qint leafnum)
{
  if (leafnum < 0 || leafnum >= cm.numLeafs)
  {
    Com_Error(ERR_DROP, "CM_LeafArea: bad number");
  }

  return cm.leafs[leafnum].area;
}

//=======================================================================


/*
===================
CM_InitBoxHull

Set up the planes and nodes so that the six floats of a bounding box
can just be stored out and get a proper clipping hull structure.
===================
*/
void
CM_InitBoxHull(void)
{
  qint i;
  qint side;
  cplane_t *p;
  cbrushside_t *s;

  box_planes = &cm.planes[cm.numPlanes];

  box_brush = &cm.brushes[cm.numBrushes];
  box_brush->numsides = 6;
  box_brush->sides = cm.brushsides + cm.numBrushSides;
  box_brush->contents = CONTENTS_BODY;
  box_brush->edges = (cbrushedge_t *)Hunk_Alloc(sizeof(cbrushedge_t) * 12, h_low);
  box_brush->numEdges = 12;

  box_model.leaf.numLeafBrushes = 1;
  //box_model.leaf.firstLeafBrush = cm.numBrushes;
  box_model.leaf.firstLeafBrush = cm.numLeafBrushes;
  cm.leafbrushes[cm.numLeafBrushes] = cm.numBrushes;

  for(i = 0;i < 6;i++)
  {
    side = i&1;

    //brush sides
    s = &cm.brushsides[cm.numBrushSides+i];
    s->plane = cm.planes + (cm.numPlanes+i*2+side);
    s->surfaceFlags = 0;

    //planes
    p = &box_planes[i*2];
    p->type = i >> 1;
    p->signbits = 0;
    VectorClear(p->normal);
    p->normal[i >> 1] = 1;

    p = &box_planes[i * 2 + 1];
    p->type = 3 + (i >> 1);
    p->signbits = 0;
    VectorClear(p->normal);
    p->normal[i >> 1] = -1;

    SetPlaneSignbits(p);
  }
}

/*
===================
CM_TempBoxModel

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
Capsules are handled differently though.
===================
*/
clipHandle_t
CM_TempBoxModel(const vec3_t mins, const vec3_t maxs, qint capsule)
{
  VectorCopy(mins, box_model.mins);
  VectorCopy(maxs, box_model.maxs);

  if (capsule)
  {
    return CAPSULE_MODEL_HANDLE;
  }

  box_planes[0].dist = maxs[0];
  box_planes[1].dist = -maxs[0];
  box_planes[2].dist = mins[0];
  box_planes[3].dist = -mins[0];
  box_planes[4].dist = maxs[1];
  box_planes[5].dist = -maxs[1];
  box_planes[6].dist = mins[1];
  box_planes[7].dist = -mins[1];
  box_planes[8].dist = maxs[2];
  box_planes[9].dist = -maxs[2];
  box_planes[10].dist = mins[2];
  box_planes[11].dist = -mins[2];

  //First side
  VectorSet(box_brush->edges[0].p0, mins[0], mins[1], mins[2]);
  VectorSet(box_brush->edges[0].p1, mins[0], maxs[1], mins[2]);
  VectorSet(box_brush->edges[1].p0, mins[0], maxs[1], mins[2]);
  VectorSet(box_brush->edges[1].p1, mins[0], maxs[1], maxs[2]);
  VectorSet(box_brush->edges[2].p0, mins[0], maxs[1], maxs[2]);
  VectorSet(box_brush->edges[2].p1, mins[0], mins[1], maxs[2]);
  VectorSet(box_brush->edges[3].p0, mins[0], mins[1], maxs[2]);
  VectorSet(box_brush->edges[3].p1, mins[0], mins[1], mins[2]);

  //Opposite side
  VectorSet(box_brush->edges[4].p0, maxs[0], mins[1], mins[2]);
  VectorSet(box_brush->edges[4].p1, maxs[0], maxs[1], mins[2]);
  VectorSet(box_brush->edges[5].p0, maxs[0], maxs[1], mins[2]);
  VectorSet(box_brush->edges[5].p1, maxs[0], maxs[1], maxs[2]);
  VectorSet(box_brush->edges[6].p0, maxs[0], maxs[1], maxs[2]);
  VectorSet(box_brush->edges[6].p1, maxs[0], mins[1], maxs[2]);
  VectorSet(box_brush->edges[7].p0, maxs[0], mins[1], maxs[2]);
  VectorSet(box_brush->edges[7].p1, maxs[0], mins[1], mins[2]);

  //Connecting edges
  VectorSet(box_brush->edges[8].p0, mins[0], mins[1], mins[2]);
  VectorSet(box_brush->edges[8].p1, maxs[0], mins[1], mins[2]);
  VectorSet(box_brush->edges[9].p0, mins[0], maxs[1], mins[2]);
  VectorSet(box_brush->edges[9].p1, maxs[0], maxs[1], mins[2]);
  VectorSet(box_brush->edges[10].p0, mins[0], maxs[1], maxs[2]);
  VectorSet(box_brush->edges[10].p1, maxs[0], maxs[1], maxs[2]);
  VectorSet(box_brush->edges[11].p0, mins[0], mins[1], maxs[2]);
  VectorSet(box_brush->edges[11].p1, maxs[0], mins[1], maxs[2]);

  VectorCopy(mins, box_brush->bounds[0]);
  VectorCopy(maxs, box_brush->bounds[1]);

  return BOX_MODEL_HANDLE;
}

/*
===================
CM_ModelBounds
===================
*/
void
CM_ModelBounds(clipHandle_t model, vec3_t mins, vec3_t maxs)
{
  cmodel_t *cmod;

  cmod = CM_ClipHandleToModel(model);
  VectorCopy(cmod->mins, mins);
  VectorCopy(cmod->maxs, maxs);
}
