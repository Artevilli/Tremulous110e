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


#ifndef TR_LOCAL_H
#define TR_LOCAL_H

#include "../qcommon/q_shared.h"
#include "../qcommon/qfiles.h"
#include "../qcommon/qcommon.h"
#include "../renderercommon/tr_public.h"
#include "tr_common.h"
#include "tr_extratypes.h"
#include "tr_extramath.h"
#include "tr_fbo.h"
#include "tr_postprocess.h"
#include "../renderer/iqm.h"
#include "qgl.h"

#define GL_INDEX_TYPE		GL_UNSIGNED_INT
typedef unsigned qint glIndex_t;

#define BUFFER_OFFSET(i) ((qchar *)NULL + (i))

#define	REFENTITYNUM_BITS	11	// can't be increased without changing drawsurf bit packing
#define	REFENTITYNUM_MASK	((1<<REFENTITYNUM_BITS) - 1)
// the last N-bit number (2^REFENTITYNUM_BITS - 1) is reserved for the special world refentity,
//  and this is reflected by the value of MAX_REFENTITIES (which therefore is not a power-of-2)
#define	MAX_REFENTITIES		((1<<REFENTITYNUM_BITS) - 1)
#define	REFENTITYNUM_WORLD	((1<<REFENTITYNUM_BITS) - 1)
// 14 bits
// can't be increased without changing bit packing for drawsurfs
// see QSORT_SHADERNUM_SHIFT
#define SHADERNUM_BITS	14
#define MAX_SHADERS		(1<<SHADERNUM_BITS)

#define	MAX_FBOS      64
#define MAX_VISCOUNTS 5
#define MAX_VAOS      4096

#define MAX_CALC_PSHADOWS    64
#define MAX_DRAWN_PSHADOWS    16 // do not increase past 32, because bit flags are used on surfaces
#define PSHADOW_MAP_SIZE      512

typedef struct cubemap_s {
	qchar name[MAX_QPATH];
	vec3_t origin;
	float parallaxRadius;
	image_t *image;
} cubemap_t;

typedef struct dlight_s {
	vec3_t	origin;
	vec3_t	color;				// range from 0.0 to 1.0, should be color normalized
	float	radius;

	vec3_t	transformed;		// origin in local coordinate system
	qint		additive;			// texture detail is lost tho when the lightmap is dark
} dlight_t;


// a trRefEntity_t has all the information passed in by
// the client game, as well as some locally derived info
typedef struct {
	refEntity_t	e;

	float		axisLength;		// compensate for non-normalized axis

	qbool	needDlights;	// true for bmodels that touch a dlight
	qbool	lightingCalculated;
	qbool	mirrored;		// mirrored matrix, needs reversed culling
	vec3_t		lightDir;		// normalized direction towards light, in world space
	vec3_t      modelLightDir;  // normalized direction towards light, in model space
	vec3_t		ambientLight;	// color normalized to 0-255
	qint			ambientLightInt;	// 32 bit rgba packed
	vec3_t		directedLight;
	qbool	intShaderTime;
} trRefEntity_t;


typedef struct {
	vec3_t		origin;			// in world coordinates
	vec3_t		axis[3];		// orientation in world
	vec3_t		viewOrigin;		// viewParms->or.origin in local coordinates
	float		modelMatrix[16];
	float		transformMatrix[16];
} orientationr_t;

// Ensure this is >= the ATTR_INDEX_COUNT enum below
#define VAO_MAX_ATTRIBS 16

typedef enum
{
	VAO_USAGE_STATIC,
	VAO_USAGE_DYNAMIC
} vaoUsage_t;

typedef struct vaoAttrib_s
{
	uint32_t enabled;
	uint32_t count;
	uint32_t type;
	uint32_t normalized;
	uint32_t stride;
	uint32_t offset;
}
vaoAttrib_t;

typedef struct vao_s
{
	qchar            name[MAX_QPATH];

	uint32_t        vao;

	uint32_t        vertexesVBO;
	qint             vertexesSize;	// amount of memory data allocated for all vertices in bytes
	vaoAttrib_t     attribs[VAO_MAX_ATTRIBS];

	uint32_t        frameSize;      // bytes to skip per frame when doing vertex animation

	uint32_t        indexesIBO;
	qint             indexesSize;	// amount of memory data allocated for all triangles in bytes
} vao_t;

//===============================================================================

typedef enum {
	SS_BAD,
	SS_PORTAL,			// mirrors, portals, viewscreens
	SS_ENVIRONMENT,		// sky box
	SS_OPAQUE,			// opaque

	SS_DECAL,			// scorch marks, etc.
	SS_SEE_THROUGH,		// ladders, grates, grills that may have small blended edges
						// in addition to alpha test
	SS_BANNER,

	SS_FOG,

	SS_UNDERWATER,		// for items that should be drawn in front of the water plane

	SS_BLEND0,			// regular transparency and filters
	SS_BLEND1,			// generally only used for additive type effects
	SS_BLEND2,
	SS_BLEND3,

	SS_BLEND6,
	SS_STENCIL_SHADOW,
	SS_ALMOST_NEAREST,	// gun smoke puffs

	SS_NEAREST			// blood blobs
} shaderSort_t;


#define MAX_SHADER_STAGES 8

typedef enum {
	GF_NONE,

	GF_SIN,
	GF_SQUARE,
	GF_TRIANGLE,
	GF_SAWTOOTH, 
	GF_INVERSE_SAWTOOTH, 

	GF_NOISE

} genFunc_t;


typedef enum {
	DEFORM_NONE,
	DEFORM_WAVE,
	DEFORM_NORMALS,
	DEFORM_BULGE,
	DEFORM_MOVE,
	DEFORM_PROJECTION_SHADOW,
	DEFORM_AUTOSPRITE,
	DEFORM_AUTOSPRITE2,
	DEFORM_TEXT0,
	DEFORM_TEXT1,
	DEFORM_TEXT2,
	DEFORM_TEXT3,
	DEFORM_TEXT4,
	DEFORM_TEXT5,
	DEFORM_TEXT6,
	DEFORM_TEXT7
} deform_t;

// deformVertexes types that can be handled by the GPU
typedef enum
{
	// do not edit: same as genFunc_t

	DGEN_NONE,
	DGEN_WAVE_SIN,
	DGEN_WAVE_SQUARE,
	DGEN_WAVE_TRIANGLE,
	DGEN_WAVE_SAWTOOTH,
	DGEN_WAVE_INVERSE_SAWTOOTH,
	DGEN_WAVE_NOISE,

	// do not edit until this line

	DGEN_BULGE,
	DGEN_MOVE
} deformGen_t;

typedef enum {
	AGEN_IDENTITY,
	AGEN_SKIP,
	AGEN_ENTITY,
	AGEN_ONE_MINUS_ENTITY,
	AGEN_VERTEX,
	AGEN_ONE_MINUS_VERTEX,
	AGEN_LIGHTING_SPECULAR,
	AGEN_WAVEFORM,
	AGEN_PORTAL,
	AGEN_CONST,
} alphaGen_t;

typedef enum {
	CGEN_BAD,
	CGEN_IDENTITY_LIGHTING,	// tr.identityLight
	CGEN_IDENTITY,			// always (1,1,1,1)
	CGEN_ENTITY,			// grabbed from entity's modulate field
	CGEN_ONE_MINUS_ENTITY,	// grabbed from 1 - entity.modulate
	CGEN_EXACT_VERTEX,		// tess.vertexColors
	CGEN_VERTEX,			// tess.vertexColors * tr.identityLight
	CGEN_EXACT_VERTEX_LIT,	// like CGEN_EXACT_VERTEX but takes a light direction from the lightgrid
	CGEN_VERTEX_LIT,		// like CGEN_VERTEX but takes a light direction from the lightgrid
	CGEN_ONE_MINUS_VERTEX,
	CGEN_WAVEFORM,			// programmatically generated
	CGEN_LIGHTING_DIFFUSE,
	CGEN_FOG,				// standard fog
	CGEN_CONST				// fixed color
} colorGen_t;

typedef enum {
	TCGEN_BAD,
	TCGEN_IDENTITY,			// clear to 0,0
	TCGEN_LIGHTMAP,
	TCGEN_TEXTURE,
	TCGEN_ENVIRONMENT_MAPPED,
	TCGEN_ENVIRONMENT_MAPPED_FP, // with correct first-person mapping
	TCGEN_FOG,
	TCGEN_VECTOR			// S and T from world coordinates
} texCoordGen_t;

typedef enum {
	ACFF_NONE,
	ACFF_MODULATE_RGB,
	ACFF_MODULATE_RGBA,
	ACFF_MODULATE_ALPHA
} acff_t;

typedef struct {
	float base;
	float amplitude;
	float phase;
	float frequency;

	genFunc_t	func;
} waveForm_t;

#define TR_MAX_TEXMODS 4

typedef enum {
	TMOD_NONE,
	TMOD_TRANSFORM,
	TMOD_TURBULENT,
	TMOD_SCROLL,
	TMOD_SCALE,
	TMOD_STRETCH,
	TMOD_ROTATE,
	TMOD_ENTITY_TRANSLATE
} texMod_t;

#define	MAX_SHADER_DEFORMS	3
typedef struct {
	deform_t	deformation;			// vertex coordinate modification type

	vec3_t		moveVector;
	waveForm_t	deformationWave;
	float		deformationSpread;

	float		bulgeWidth;
	float		bulgeHeight;
	float		bulgeSpeed;
} deformStage_t;


typedef struct {
	texMod_t		type;

	// used for TMOD_TURBULENT and TMOD_STRETCH
	waveForm_t		wave;

	// used for TMOD_TRANSFORM
	float			matrix[2][2];		// s' = s * m[0][0] + t * m[1][0] + trans[0]
	float			translate[2];		// t' = s * m[0][1] + t * m[0][1] + trans[1]

	// used for TMOD_SCALE
	float			scale[2];			// s *= scale[0]
	                                    // t *= scale[1]

	// used for TMOD_SCROLL
	float			scroll[2];			// s' = s + scroll[0] * time
										// t' = t + scroll[1] * time

	// + = clockwise
	// - = counterclockwise
	float			rotateSpeed;

} texModInfo_t;


#define MAX_IMAGE_ANIMATIONS		24
#define MAX_IMAGE_ANIMATIONS_VQ3	8

typedef struct {
	image_t			*image[MAX_IMAGE_ANIMATIONS];
	qint				numImageAnimations;
	double			imageAnimationSpeed;

	texCoordGen_t	tcGen;
	vec3_t			tcGenVectors[2];

	qint				numTexMods;
	texModInfo_t	*texMods;

	qint				videoMapHandle;
	qbool		isLightmap;
	qbool		isVideoMap;
} textureBundle_t;


enum
{
	TB_COLORMAP    = 0,
	TB_DIFFUSEMAP  = 0,
	TB_LIGHTMAP    = 1,
	TB_LEVELSMAP   = 1,
	TB_SHADOWMAP3  = 1,
	TB_NORMALMAP   = 2,
	TB_DELUXEMAP   = 3,
	TB_SHADOWMAP2  = 3,
	TB_SPECULARMAP = 4,
	TB_SHADOWMAP   = 5,
	TB_CUBEMAP     = 6,
	TB_SHADOWMAP4  = 6,
	NUM_TEXTURE_BUNDLES = 7
};

typedef enum
{
	// material shader stage types
	ST_COLORMAP = 0,			// vanilla Q3A style shader treatening
	ST_DIFFUSEMAP = 0,          // treat color and diffusemap the same
	ST_NORMALMAP,
	ST_NORMALPARALLAXMAP,
	ST_SPECULARMAP,
	ST_GLSL
} stageType_t;

typedef struct {
	qbool		active;
	
	textureBundle_t	bundle[NUM_TEXTURE_BUNDLES];

	waveForm_t		rgbWave;
	colorGen_t		rgbGen;

	waveForm_t		alphaWave;
	alphaGen_t		alphaGen;

	byte			constantColor[4];			// for CGEN_CONST and AGEN_CONST

	unsigned		stateBits;					// GLS_xxxx mask

	acff_t			adjustColorsForFog;

	qbool		isDetail;

	stageType_t     type;
	struct shaderProgram_s *glslShaderGroup;
	qint glslShaderIndex;

	vec4_t normalScale;
	vec4_t specularScale;

} shaderStage_t;

struct shaderCommands_s;

typedef enum {
	CT_FRONT_SIDED,
	CT_BACK_SIDED,
	CT_TWO_SIDED
} cullType_t;

typedef enum {
	FP_NONE,		// surface is translucent and will just be adjusted properly
	FP_EQUAL,		// surface is opaque but possibly alpha tested
	FP_LE			// surface is translucent, but still needs a fog pass (fog surface)
} fogPass_t;

typedef struct {
	float		cloudHeight;
	image_t		*outerbox[6], *innerbox[6];
} skyParms_t;

typedef struct {
	vec3_t	color;
	float	depthForOpaque;
} fogParms_t;

typedef struct shader_s {
	qchar		name[MAX_QPATH];		// game path, including extension
	qint			lightmapSearchIndex;	// for a shader to match, both name and lightmapIndex must match
	qint			lightmapIndex;			// for rendering

	qint			index;					// this shader == tr.shaders[index]
	qint			sortedIndex;			// this shader == tr.sortedShaders[sortedIndex]

	float		sort;					// lower numbered shaders draw before higher numbered

	qbool	defaultShader;			// we want to return index 0 if the shader failed to
										// load for some reason, but R_FindShader should
										// still keep a name allocated for it, so if
										// something calls RE_RegisterShader again with
										// the same name, we don't try looking for it again

	qbool	explicitlyDefined;		// found in a .shader file

	qint			surfaceFlags;			// if explicitlyDefined, this will have SURF_* flags
	qint			contentFlags;

	qbool	entityMergable;			// merge across entites optimizable (smoke, blood)

	qbool	isSky;
	skyParms_t	sky;
	fogParms_t	fogParms;

	float		portalRange;			// distance to fog out at
	qbool	isPortal;

	cullType_t	cullType;				// CT_FRONT_SIDED, CT_BACK_SIDED, or CT_TWO_SIDED
	qbool	polygonOffset;			// set for decals and other items that must be offset 
	qbool	noMipMaps;				// for console fonts, 2D elements, etc.
	qbool	noPicMip;				// for images that must always be full resolution

	fogPass_t	fogPass;				// draw a blended pass, possibly with depth test equals

	qint         vertexAttribs;          // not all shaders will need all data to be gathered

	qint			numDeforms;
	deformStage_t	deforms[MAX_SHADER_DEFORMS];

	qint			numUnfoggedPasses;
	shaderStage_t	*stages[MAX_SHADER_STAGES];		

	qint	lightingStage;

	void		(*optimalStageIteratorFunc)( void );

	double	clampTime;                                  // time this shader is clamped to - set to double for frameloss fix -EC-
	double	timeOffset;                                 // current time offset for this shader - set to double for frameloss fix -EC-

  struct shader_s *remappedShader;                  // current shader this one is remapped too

	struct	shader_s	*next;
} shader_t;

enum
{
	ATTR_INDEX_POSITION       = 0,
	ATTR_INDEX_TEXCOORD       = 1,
	ATTR_INDEX_LIGHTCOORD     = 2,
	ATTR_INDEX_TANGENT        = 3,
	ATTR_INDEX_NORMAL         = 4,
	ATTR_INDEX_COLOR          = 5,
	ATTR_INDEX_PAINTCOLOR     = 6,
	ATTR_INDEX_LIGHTDIRECTION = 7,
	ATTR_INDEX_BONE_INDEXES   = 8,
	ATTR_INDEX_BONE_WEIGHTS   = 9,

	// GPU vertex animations
	ATTR_INDEX_POSITION2      = 10,
	ATTR_INDEX_TANGENT2       = 11,
	ATTR_INDEX_NORMAL2        = 12,
	
	ATTR_INDEX_COUNT          = 13
};

enum
{
	ATTR_POSITION =       1 << ATTR_INDEX_POSITION,
	ATTR_TEXCOORD =       1 << ATTR_INDEX_TEXCOORD,
	ATTR_LIGHTCOORD =     1 << ATTR_INDEX_LIGHTCOORD,
	ATTR_TANGENT =        1 << ATTR_INDEX_TANGENT,
	ATTR_NORMAL =         1 << ATTR_INDEX_NORMAL,
	ATTR_COLOR =          1 << ATTR_INDEX_COLOR,
	ATTR_PAINTCOLOR =     1 << ATTR_INDEX_PAINTCOLOR,
	ATTR_LIGHTDIRECTION = 1 << ATTR_INDEX_LIGHTDIRECTION,
	ATTR_BONE_INDEXES =   1 << ATTR_INDEX_BONE_INDEXES,
	ATTR_BONE_WEIGHTS =   1 << ATTR_INDEX_BONE_WEIGHTS,

	// for .md3 interpolation
	ATTR_POSITION2 =      1 << ATTR_INDEX_POSITION2,
	ATTR_TANGENT2 =       1 << ATTR_INDEX_TANGENT2,
	ATTR_NORMAL2 =        1 << ATTR_INDEX_NORMAL2,

	ATTR_DEFAULT = ATTR_POSITION,
	ATTR_BITS =	ATTR_POSITION |
				ATTR_TEXCOORD |
				ATTR_LIGHTCOORD |
				ATTR_TANGENT |
				ATTR_NORMAL |
				ATTR_COLOR |
				ATTR_PAINTCOLOR |
				ATTR_LIGHTDIRECTION |
				ATTR_BONE_INDEXES |
				ATTR_BONE_WEIGHTS |
				ATTR_POSITION2 |
				ATTR_TANGENT2 |
				ATTR_NORMAL2
};

enum
{
	GENERICDEF_USE_DEFORM_VERTEXES  = 0x0001,
	GENERICDEF_USE_TCGEN_AND_TCMOD  = 0x0002,
	GENERICDEF_USE_VERTEX_ANIMATION = 0x0004,
	GENERICDEF_USE_FOG              = 0x0008,
	GENERICDEF_USE_RGBAGEN          = 0x0010,
	GENERICDEF_USE_BONE_ANIMATION   = 0x0020,
	GENERICDEF_ALL                  = 0x003F,
	GENERICDEF_COUNT                = 0x0040,
};

enum
{
	FOGDEF_USE_DEFORM_VERTEXES  = 0x0001,
	FOGDEF_USE_VERTEX_ANIMATION = 0x0002,
	FOGDEF_USE_BONE_ANIMATION   = 0x0004,
	FOGDEF_ALL                  = 0x0007,
	FOGDEF_COUNT                = 0x0008,
};

enum
{
	DLIGHTDEF_USE_DEFORM_VERTEXES  = 0x0001,
	DLIGHTDEF_ALL                  = 0x0001,
	DLIGHTDEF_COUNT                = 0x0002,
};

enum
{
	LIGHTDEF_USE_LIGHTMAP        = 0x0001,
	LIGHTDEF_USE_LIGHT_VECTOR    = 0x0002,
	LIGHTDEF_USE_LIGHT_VERTEX    = 0x0003,
	LIGHTDEF_LIGHTTYPE_MASK      = 0x0003,
	LIGHTDEF_ENTITY_VERTEX_ANIMATION = 0x0004,
	LIGHTDEF_USE_TCGEN_AND_TCMOD = 0x0008,
	LIGHTDEF_USE_PARALLAXMAP     = 0x0010,
	LIGHTDEF_USE_SHADOWMAP       = 0x0020,
	LIGHTDEF_ENTITY_BONE_ANIMATION = 0x0040,
	LIGHTDEF_ALL                 = 0x007F,
	LIGHTDEF_COUNT               = 0x0080
};

enum
{
	SHADOWMAPDEF_USE_VERTEX_ANIMATION = 0x0001,
	SHADOWMAPDEF_USE_BONE_ANIMATION   = 0x0002,
	SHADOWMAPDEF_ALL                  = 0x0003,
	SHADOWMAPDEF_COUNT                = 0x0004
};

enum
{
	GLSL_INT,
	GLSL_FLOAT,
	GLSL_FLOAT5,
	GLSL_VEC2,
	GLSL_VEC3,
	GLSL_VEC4,
	GLSL_MAT16,
	GLSL_MAT16_BONEMATRIX
};

typedef enum
{
	UNIFORM_DIFFUSEMAP = 0,
	UNIFORM_LIGHTMAP,
	UNIFORM_NORMALMAP,
	UNIFORM_DELUXEMAP,
	UNIFORM_SPECULARMAP,

	UNIFORM_TEXTUREMAP,
	UNIFORM_LEVELSMAP,
	UNIFORM_CUBEMAP,

	UNIFORM_SCREENIMAGEMAP,
	UNIFORM_SCREENDEPTHMAP,

	UNIFORM_SHADOWMAP,
	UNIFORM_SHADOWMAP2,
	UNIFORM_SHADOWMAP3,
	UNIFORM_SHADOWMAP4,

	UNIFORM_SHADOWMVP,
	UNIFORM_SHADOWMVP2,
	UNIFORM_SHADOWMVP3,
	UNIFORM_SHADOWMVP4,

	UNIFORM_ENABLETEXTURES,

	UNIFORM_DIFFUSETEXMATRIX,
	UNIFORM_DIFFUSETEXOFFTURB,

	UNIFORM_TCGEN0,
	UNIFORM_TCGEN0VECTOR0,
	UNIFORM_TCGEN0VECTOR1,

	UNIFORM_DEFORMGEN,
	UNIFORM_DEFORMPARAMS,

	UNIFORM_COLORGEN,
	UNIFORM_ALPHAGEN,
	UNIFORM_COLOR,
	UNIFORM_BASECOLOR,
	UNIFORM_VERTCOLOR,

	UNIFORM_DLIGHTINFO,
	UNIFORM_LIGHTFORWARD,
	UNIFORM_LIGHTUP,
	UNIFORM_LIGHTRIGHT,
	UNIFORM_LIGHTORIGIN,
	UNIFORM_MODELLIGHTDIR,
	UNIFORM_LIGHTRADIUS,
	UNIFORM_AMBIENTLIGHT,
	UNIFORM_DIRECTEDLIGHT,

	UNIFORM_PORTALRANGE,

	UNIFORM_FOGDISTANCE,
	UNIFORM_FOGDEPTH,
	UNIFORM_FOGEYET,
	UNIFORM_FOGCOLORMASK,

	UNIFORM_MODELMATRIX,
	UNIFORM_MODELVIEWPROJECTIONMATRIX,

	UNIFORM_TIME,
	UNIFORM_VERTEXLERP,
	UNIFORM_NORMALSCALE,
	UNIFORM_SPECULARSCALE,

	UNIFORM_VIEWINFO, // znear, zfar, width/2, height/2
	UNIFORM_VIEWORIGIN,
	UNIFORM_LOCALVIEWORIGIN,
	UNIFORM_VIEWFORWARD,
	UNIFORM_VIEWLEFT,
	UNIFORM_VIEWUP,

	UNIFORM_INVTEXRES,
	UNIFORM_AUTOEXPOSUREMINMAX,
	UNIFORM_TONEMINAVGMAXLINEAR,

	UNIFORM_PRIMARYLIGHTORIGIN,
	UNIFORM_PRIMARYLIGHTCOLOR,
	UNIFORM_PRIMARYLIGHTAMBIENT,
	UNIFORM_PRIMARYLIGHTRADIUS,

	UNIFORM_CUBEMAPINFO,

	UNIFORM_ALPHATEST,

	UNIFORM_BONEMATRIX,

	UNIFORM_COUNT
} uniform_t;

// shaderProgram_t represents a pair of one
// GLSL vertex and one GLSL fragment shader
typedef struct shaderProgram_s
{
	qchar            name[MAX_QPATH];

	GLuint          program;
	GLuint          vertexShader;
	GLuint          fragmentShader;
	uint32_t        attribs;	// vertex array attributes

	// uniform parameters
	GLint uniforms[UNIFORM_COUNT];
	short uniformBufferOffsets[UNIFORM_COUNT]; // max 32767/64=511 uniforms
	qchar  *uniformBuffer;
} shaderProgram_t;

// trRefdef_t holds everything that comes in refdef_t,
// as well as the locally generated scene information
typedef struct {
	qint			x, y, width, height;
	float		fov_x, fov_y;
	vec3_t		vieworg;
	vec3_t		viewaxis[3];		// transformation matrix

	stereoFrame_t	stereoFrame;

	qint			time;				// time in milliseconds for shader effects and other time dependent rendering issues
	qint			rdflags;			// RDF_NOWORLDMODEL, etc

	// 1 bits will prevent the associated area from rendering at all
	byte		areamask[MAX_MAP_AREA_BYTES];
	qbool	areamaskModified;	// qtrue if areamask changed since last scene

	double		floatTime;			// tr.refdef.time / 1000.0

	float		blurFactor;

	// text messages for deform text shaders
	qchar		text[MAX_RENDER_STRINGS][MAX_RENDER_STRING_LENGTH];

	qint			num_entities;
	trRefEntity_t	*entities;

	qint			num_dlights;
	struct dlight_s	*dlights;

	qint			numPolys;
	struct srfPoly_s	*polys;

	qint			numDrawSurfs;
	struct drawSurf_s	*drawSurfs;

	unsigned qint dlightMask;
	qint         num_pshadows;
	struct pshadow_s *pshadows;

	float       sunShadowMvp[4][16];
	float       sunDir[4];
	float       sunCol[4];
	float       sunAmbCol[4];

	float       autoExposureMinMax[2];
	float       toneMinAvgMaxLinear[3];
} trRefdef_t;


//=================================================================================

// max surfaces per-skin
// This is an arbitrary limit. Vanilla Q3 only supported 32 surfaces in skins but failed to
// enforce the maximum limit when reading skin files. It was possile to use more than 32
// surfaces which accessed out of bounds memory past end of skin->surfaces hunk block.
#define MAX_SKIN_SURFACES	256

// skins allow models to be retextured without modifying the model file
typedef struct {
	qchar		name[MAX_QPATH];
	shader_t	*shader;
} skinSurface_t;

typedef struct skin_s {
	qchar		name[MAX_QPATH];		// game path, including extension
	qint			numSurfaces;
	skinSurface_t	*surfaces;			// dynamically allocated array of surfaces
} skin_t;


typedef struct {
	qint			originalBrushNumber;
	vec3_t		bounds[2];

	unsigned	colorInt;				// in packed byte format
	float		tcScale;				// texture coordinate vector scales
	fogParms_t	parms;

	// for clipping distance in fog when outside
	qbool	hasSurface;
	float		surface[4];
} fog_t;

typedef enum {
	VPF_NONE            = 0x00,
	VPF_NOVIEWMODEL     = 0x01,
	VPF_SHADOWMAP       = 0x02,
	VPF_DEPTHSHADOW     = 0x04,
	VPF_DEPTHCLAMP      = 0x08,
	VPF_ORTHOGRAPHIC    = 0x10,
	VPF_USESUNLIGHT     = 0x20,
	VPF_FARPLANEFRUSTUM = 0x40,
	VPF_NOCUBEMAPS      = 0x80
} viewParmFlags_t;

typedef struct {
	orientationr_t	or;
	orientationr_t	world;
	vec3_t		pvsOrigin;			// may be different than or.origin for portals
	qbool	isPortal;			// true if this view is through a portal
	qbool	isMirror;			// the portal is a mirror, invert the face culling
	viewParmFlags_t flags;
	qint			frameSceneNum;		// copied from tr.frameSceneNum
	qint			frameCount;			// copied from tr.frameCount
	cplane_t	portalPlane;		// clip anything behind this if mirroring
	qint			viewportX, viewportY, viewportWidth, viewportHeight;
	FBO_t		*targetFbo;
	qint         targetFboLayer;
	qint         targetFboCubemapIndex;
	float		fovX, fovY;
	float		projectionMatrix[16];
	cplane_t	frustum[5];
	vec3_t		visBounds[2];
	float		zFar;
	float       zNear;
	stereoFrame_t	stereoFrame;
} viewParms_t;


/*
==============================================================================

SURFACES

==============================================================================
*/

// any changes in surfaceType must be mirrored in rb_surfaceTable[]
typedef enum {
	SF_BAD,
	SF_SKIP,				// ignore
	SF_FACE,
	SF_GRID,
	SF_TRIANGLES,
	SF_POLY,
	SF_MDV,
	SF_MDR,
	SF_IQM,
	SF_FLARE,
	SF_ENTITY,				// beams, rails, lightning, etc that can be determined by entity
	SF_VAO_MDVMESH,
	SF_VAO_IQM,

	SF_NUM_SURFACE_TYPES,
	SF_MAX = 0x7fffffff			// ensures that sizeof( surfaceType_t ) == sizeof( qint )
} surfaceType_t;

typedef struct drawSurf_s {
	unsigned qint		sort;			// bit combination for fast compares
	qint                 cubemapIndex;
	surfaceType_t		*surface;		// any of surface*_t
} drawSurf_t;

#define	MAX_FACE_POINTS		64

#define	MAX_PATCH_SIZE		32			// max dimensions of a patch mesh in map file
#define	MAX_GRID_SIZE		65			// max dimensions of a grid mesh in memory

// when cgame directly specifies a polygon, it becomes a srfPoly_t
// as soon as it is called
typedef struct srfPoly_s {
	surfaceType_t	surfaceType;
	qhandle_t		hShader;
	qint				fogIndex;
	qint				numVerts;
	polyVert_t		*verts;
} srfPoly_t;


typedef struct srfFlare_s {
	surfaceType_t	surfaceType;
	vec3_t			origin;
	vec3_t			normal;
	vec3_t			color;
} srfFlare_t;

typedef struct
{
	vec3_t          xyz;
	vec2_t          st;
	vec2_t          lightmap;
	int16_t         normal[4];
	int16_t         tangent[4];
	int16_t         lightdir[4];
	uint16_t        color[4];

#if DEBUG_OPTIMIZEVERTICES
	unsigned qint    id;
#endif
} srfVert_t;

#define srfVert_t_cleared(x) srfVert_t (x) = {{0, 0, 0}, {0, 0}, {0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}

// srfBspSurface_t covers SF_GRID, SF_TRIANGLES, and SF_POLY
typedef struct srfBspSurface_s
{
	surfaceType_t   surfaceType;

	// dynamic lighting information
	qint				dlightBits;
	qint             pshadowBits;

	// culling information
	vec3_t			cullBounds[2];
	vec3_t			cullOrigin;
	float			cullRadius;
	cplane_t        cullPlane;

	// indexes
	qint             numIndexes;
	glIndex_t      *indexes;

	// vertexes
	qint             numVerts;
	srfVert_t      *verts;
	
	// SF_GRID specific variables after here

	// lod information, which may be different
	// than the culling information to allow for
	// groups of curves that LOD as a unit
	vec3_t			lodOrigin;
	float			lodRadius;
	qint				lodFixed;
	qint				lodStitched;

	// vertexes
	qint				width, height;
	float			*widthLodError;
	float			*heightLodError;
} srfBspSurface_t;

typedef struct {
	vec3_t translate;
	quat_t rotate;
	vec3_t scale;
} iqmTransform_t;

// inter-quake-model
typedef struct {
	qint		num_vertexes;
	qint		num_triangles;
	qint		num_frames;
	qint		num_surfaces;
	qint		num_joints;
	qint		num_poses;
	struct srfIQModel_s	*surfaces;

	qint		*triangles;

	// vertex arrays
	float		*positions;
	float		*texcoords;
	float		*normals;
	float		*tangents;
	byte		*colors;
	qint		*influences; // [num_vertexes] indexes into influenceBlendVertexes

	// unique list of vertex blend indexes/weights for faster CPU vertex skinning
	byte		*influenceBlendIndexes; // [num_influences]
	union {
		float	*f;
		byte	*b;
	} influenceBlendWeights; // [num_influences]

	// depending upon the exporter, blend indices and weights might be qint/float
	// as opposed to the recommended byte/byte, for example Noesis exports
	// qint/float whereas the official IQM tool exports byte/byte
	qint		blendWeightsType; // IQM_UBYTE or IQM_FLOAT

	qchar		*jointNames;
	qint		*jointParents;
	float		*bindJoints; // [num_joints * 12]
	float		*invBindJoints; // [num_joints * 12]
	iqmTransform_t	*poses; // [num_frames * num_poses]
	float		*bounds;

	qint		numVaoSurfaces;
	struct srfVaoIQModel_s	*vaoSurfaces;
} iqmData_t;

// inter-quake-model surface
typedef struct srfIQModel_s {
	surfaceType_t	surfaceType;
	qchar		name[MAX_QPATH];
	shader_t	*shader;
	iqmData_t	*data;
	qint		first_vertex, num_vertexes;
	qint		first_triangle, num_triangles;
	qint		first_influence, num_influences;
} srfIQModel_t;

typedef struct srfVaoIQModel_s
{
	surfaceType_t   surfaceType;

	iqmData_t *iqmData;
	struct srfIQModel_s *iqmSurface;

	// backEnd stats
	qint             numIndexes;
	qint             numVerts;

	// static render data
	vao_t          *vao;
} srfVaoIQModel_t;

typedef struct srfVaoMdvMesh_s
{
	surfaceType_t   surfaceType;

	struct mdvModel_s *mdvModel;
	struct mdvSurface_s *mdvSurface;

	// backEnd stats
	qint             numIndexes;
	qint             numVerts;

	// static render data
	vao_t          *vao;
} srfVaoMdvMesh_t;

extern	void (*rb_surfaceTable[SF_NUM_SURFACE_TYPES])(void *);

/*
==============================================================================

SHADOWS

==============================================================================
*/

typedef struct pshadow_s
{
	float sort;
	
	qint    numEntities;
	qint    entityNums[8];
	vec3_t entityOrigins[8];
	float  entityRadiuses[8];

	float viewRadius;
	vec3_t viewOrigin;

	vec3_t lightViewAxis[3];
	vec3_t lightOrigin;
	float  lightRadius;
	cplane_t cullPlane;
} pshadow_t;


/*
==============================================================================

BRUSH MODELS

==============================================================================
*/


//
// in memory representation
//

#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2

#define CULLINFO_NONE   0
#define CULLINFO_BOX    1
#define CULLINFO_SPHERE 2
#define CULLINFO_PLANE  4

typedef struct cullinfo_s {
	qint             type;
	vec3_t          bounds[2];
	vec3_t			localOrigin;
	float			radius;
	cplane_t        plane;
} cullinfo_t;

typedef struct msurface_s {
	//qint					viewCount;		// if == tr.viewCount, already added
	struct shader_s		*shader;
	qint					fogIndex;
	qint                 cubemapIndex;
	cullinfo_t          cullinfo;

	surfaceType_t		*data;			// any of srf*_t
} msurface_t;


typedef struct mnode_s {
	// common with leaf and node
	qint			contents;		// -1 for nodes, to differentiate from leafs
	qint             visCounts[MAX_VISCOUNTS];	// node needs to be traversed if current
	vec3_t		mins, maxs;		// for bounding box culling
	struct mnode_s	*parent;

	// node specific
	cplane_t	*plane;
	struct mnode_s	*children[2];	

	// leaf specific
	qint			cluster;
	qint			area;

	qint         firstmarksurface;
	qint			nummarksurfaces;
} mnode_t;

typedef struct {
	vec3_t		bounds[2];		// for culling
	qint	        firstSurface;
	qint			numSurfaces;
} bmodel_t;

typedef struct {
	qchar		name[MAX_QPATH];		// ie: maps/tim_dm2.bsp
	qchar		baseName[MAX_QPATH];	// ie: tim_dm2

	qint			dataSize;

	qint			numShaders;
	dshader_t	*shaders;

	qint			numBModels;
	bmodel_t	*bmodels;

	qint			numplanes;
	cplane_t	*planes;

	qint			numnodes;		// includes leafs
	qint			numDecisionNodes;
	mnode_t		*nodes;

	qint         numWorldSurfaces;

	qint			numsurfaces;
	msurface_t	*surfaces;
	qint         *surfacesViewCount;
	qint         *surfacesDlightBits;
	qint			*surfacesPshadowBits;

	qint			nummarksurfaces;
	qint         *marksurfaces;

	qint			numfogs;
	fog_t		*fogs;

	vec3_t		lightGridOrigin;
	vec3_t		lightGridSize;
	vec3_t		lightGridInverseSize;
	qint			lightGridBounds[3];
	byte		*lightGridData;
	uint16_t	*lightGrid16;


	qint			numClusters;
	qint			clusterBytes;
	const byte	*vis;			// may be passed in by CM_LoadMap to save space

	qchar		*entityString;
	const qchar		*entityParsePoint;
} world_t;


/*
==============================================================================
MDV MODELS - meta format for vertex animation models like .md2, .md3, .mdc
==============================================================================
*/
typedef struct
{
	float           bounds[2][3];
	float           localOrigin[3];
	float           radius;
} mdvFrame_t;

typedef struct
{
	float           origin[3];
	float           axis[3][3];
} mdvTag_t;

typedef struct
{
	qchar            name[MAX_QPATH];	// tag name
} mdvTagName_t;

typedef struct
{
	vec3_t          xyz;
	int16_t         normal[4];
	int16_t         tangent[4];
} mdvVertex_t;

typedef struct
{
	float           st[2];
} mdvSt_t;

typedef struct mdvSurface_s
{
	surfaceType_t   surfaceType;

	qchar            name[MAX_QPATH];	// polyset name

	qint             numShaderIndexes;
	qint				*shaderIndexes;

	qint             numVerts;
	mdvVertex_t    *verts;
	mdvSt_t        *st;

	qint             numIndexes;
	glIndex_t      *indexes;

	struct mdvModel_s *model;
} mdvSurface_t;

typedef struct mdvModel_s
{
	qint             numFrames;
	mdvFrame_t     *frames;

	qint             numTags;
	mdvTag_t       *tags;
	mdvTagName_t   *tagNames;

	qint             numSurfaces;
	mdvSurface_t   *surfaces;

	qint             numVaoSurfaces;
	srfVaoMdvMesh_t  *vaoSurfaces;

	qint             numSkins;
} mdvModel_t;


//======================================================================

typedef enum {
	MOD_BAD,
	MOD_BRUSH,
	MOD_MESH,
	MOD_MDR,
	MOD_IQM
} modtype_t;

typedef struct model_s {
	qchar		name[MAX_QPATH];
	modtype_t	type;
	qint			index;		// model = tr.models[model->index]

	qint			dataSize;	// just for listing purposes
	bmodel_t	*bmodel;		// only if type == MOD_BRUSH
	mdvModel_t	*mdv[MD3_MAX_LODS];	// only if type == MOD_MESH
	void	*modelData;			// only if type == (MOD_MDR | MOD_IQM)

	qint			 numLods;
} model_t;


#define	MAX_MOD_KNOWN	1024

void		R_ModelInit (void);
model_t		*R_GetModelByHandle( qhandle_t hModel );
qint			R_LerpTag( orientation_t *tag, qhandle_t handle, qint startFrame, qint endFrame, 
					 float frac, const qchar *tagName );
void		R_ModelBounds( qhandle_t handle, vec3_t mins, vec3_t maxs );

void		R_Modellist_f (void);

//====================================================

#define	MAX_DRAWIMAGES			2048
#define	MAX_SKINS				1024


#define	MAX_DRAWSURFS			0x10000
#define	DRAWSURF_MASK			(MAX_DRAWSURFS-1)

/*

the drawsurf sort data is packed into a single 32 bit value so it can be
compared quickly during the qsorting process

the bits are allocated as follows:

0 - 1	: dlightmap index
//2		: used to be clipped flag REMOVED - 03.21.00 rad
2 - 6	: fog index
11 - 20	: entity index
21 - 31	: sorted shader index

	TTimo - 1.32
0-1   : dlightmap index
2-6   : fog index
7-16  : entity index
17-30 : sorted shader index

    SmileTheory - for pshadows
17-31 : sorted shader index
7-16  : entity index
2-6   : fog index
1     : pshadow flag
0     : dlight flag
*/
#define	QSORT_FOGNUM_SHIFT	2
#define	QSORT_REFENTITYNUM_SHIFT	7
#define	QSORT_SHADERNUM_SHIFT	(QSORT_REFENTITYNUM_SHIFT+REFENTITYNUM_BITS)
#if (QSORT_SHADERNUM_SHIFT+SHADERNUM_BITS) > 32
	#error "Need to update sorting, too many bits."
#endif
#define QSORT_PSHADOW_SHIFT     1

extern	qint			gl_filter_min, gl_filter_max;

/*
** performanceCounters_t
*/
typedef struct {
	qint		c_sphere_cull_patch_in, c_sphere_cull_patch_clip, c_sphere_cull_patch_out;
	qint		c_box_cull_patch_in, c_box_cull_patch_clip, c_box_cull_patch_out;
	qint		c_sphere_cull_md3_in, c_sphere_cull_md3_clip, c_sphere_cull_md3_out;
	qint		c_box_cull_md3_in, c_box_cull_md3_clip, c_box_cull_md3_out;

	qint		c_leafs;
	qint		c_dlightSurfaces;
	qint		c_dlightSurfacesCulled;
} frontEndCounters_t;

#define	FOG_TABLE_SIZE		256
#define FUNCTABLE_SIZE		1024
#define FUNCTABLE_SIZE2		10
#define FUNCTABLE_MASK		(FUNCTABLE_SIZE-1)


// the renderer front end should never modify glstate_t
typedef struct {
	qbool	finishCalled;
	qint			texEnv[2];
	qint			faceCulling;
	qint         faceCullFront;
	uint32_t    glStateBits;
	uint32_t    storedGlState;
	float           vertexAttribsInterpolation;
	qbool        vertexAnimation;
	qint             boneAnimation; // number of bones
	mat4_t          boneMatrix[IQM_MAX_JOINTS];
	uint32_t        vertexAttribsEnabled;  // global if no VAOs, tess only otherwise
	FBO_t          *currentFBO;
	vao_t          *currentVao;
	mat4_t        modelview;
	mat4_t        projection;
	mat4_t		modelviewProjection;
} glstate_t;

typedef enum {
	MI_NONE,
	MI_NVX,
	MI_ATI
} memInfo_t;

typedef enum {
	TCR_NONE = 0x0000,
	TCR_RGTC = 0x0001,
	TCR_BPTC = 0x0002,
} textureCompressionRef_t;

// We can't change glConfig_t without breaking DLL/vms compatibility, so
// store extensions we have here.
typedef struct {
	qbool    intelGraphics;

	qbool	occlusionQuery;

	qint glslMajorVersion;
	qint glslMinorVersion;
	qint glslMaxAnimatedBones;

	memInfo_t   memInfo;

	qbool framebufferObject;
	qint maxRenderbufferSize;
	qint maxColorAttachments;

	qbool textureFloat;
	textureCompressionRef_t textureCompression;
	qbool swizzleNormalmap;
	
	qbool framebufferMultisample;
	qbool framebufferBlit;

	qbool depthClamp;
	qbool seamlessCubeMap;

	qbool vertexArrayObject;
	qbool directStateAccess;
} glRefConfig_t;


typedef struct {
	qint		c_surfaces, c_shaders, c_vertexes, c_indexes, c_totalIndexes;
	qint     c_surfBatches;
	float	c_overDraw;
	
	qint		c_vaoBinds;
	qint		c_vaoVertexes;
	qint		c_vaoIndexes;

	qint     c_staticVaoDraws;
	qint     c_dynamicVaoDraws;

	qint		c_dlightVertexes;
	qint		c_dlightIndexes;

	qint		c_flareAdds;
	qint		c_flareTests;
	qint		c_flareRenders;

	qint     c_glslShaderBinds;
	qint     c_genericDraws;
	qint     c_lightallDraws;
	qint     c_fogDraws;
	qint     c_dlightDraws;

	qint		msec;			// total msec for backend run
} backEndCounters_t;

// all state modified by the back end is separated
// from the front end state
typedef struct {
	trRefdef_t	refdef;
	viewParms_t	viewParms;
	orientationr_t	or;
	backEndCounters_t	pc;
	qbool	isHyperspace;
	const trRefEntity_t *currentEntity;
	qbool	skyRenderedThisView;	// flag for drawing sun

	qbool	projection2D;	// if qtrue, drawstretchpic doesn't need to change modes
	byte		color2D[4];
	qbool	vertexes2D;		// shader needs to be finished
	trRefEntity_t	entity2D;	// currentEntity will point at this when doing 2D rendering

	FBO_t *last2DFBO;
	qbool    colorMask[4];
	qbool    framePostProcessed;
	qbool    depthFill;
} backEndState_t;

/*
** trGlobals_t 
**
** Most renderer globals are defined here.
** backend functions should never modify any of these fields,
** but may read fields that aren't dynamically modified
** by the frontend.
*/
typedef struct {
	qbool				registered;		// cleared at shutdown, set at beginRegistration

	qint						visIndex;
	qint						visClusters[MAX_VISCOUNTS];
	qint						visCounts[MAX_VISCOUNTS];	// incremented every time a new vis cluster is entered

	qint						frameCount;		// incremented every frame
	qint						sceneCount;		// incremented every scene
	qint						viewCount;		// incremented every view (twice a scene if portaled)
											// and every R_MarkFragments call

	qint						frameSceneNum;	// zeroed at RE_BeginFrame

	qbool				worldMapLoaded;
	qbool				worldDeluxeMapping;
	vec2_t                  autoExposureMinMax;
	vec3_t                  toneMinAvgMaxLevel;
	world_t					*world;

	const byte				*externalVisData;	// from RE_SetWorldVisData, shared with CM_Load

	image_t					*defaultImage;
	image_t					*scratchImage[ MAX_VIDEO_HANDLES ];
	image_t					*fogImage;
	image_t					*dlightImage;	// inverse-quare highlight for projective adding
	image_t					*flareImage;
	image_t					*whiteImage;			// full of 0xff
	image_t					*identityLightImage;	// full of tr.identityLightByte

	image_t                 *shadowCubemaps[MAX_DLIGHTS];
	

	image_t					*renderImage;
	image_t					*sunRaysImage;
	image_t					*renderDepthImage;
	image_t					*pshadowMaps[MAX_DRAWN_PSHADOWS];
	image_t					*screenScratchImage;
	image_t					*textureScratchImage[2];
	image_t                 *quarterImage[2];
	image_t					*calcLevelsImage;
	image_t					*targetLevelsImage;
	image_t					*fixedLevelsImage;
	image_t					*sunShadowDepthImage[4];
	image_t                 *screenShadowImage;
	image_t                 *screenSsaoImage;
	image_t					*hdrDepthImage;
	image_t                 *renderCubeImage;
	
	image_t					*textureDepthImage;

	FBO_t					*renderFbo;
	FBO_t					*msaaResolveFbo;
	FBO_t					*sunRaysFbo;
	FBO_t					*depthFbo;
	FBO_t					*pshadowFbos[MAX_DRAWN_PSHADOWS];
	FBO_t					*screenScratchFbo;
	FBO_t					*textureScratchFbo[2];
	FBO_t                   *quarterFbo[2];
	FBO_t					*calcLevelsFbo;
	FBO_t					*targetLevelsFbo;
	FBO_t					*sunShadowFbo[4];
	FBO_t					*screenShadowFbo;
	FBO_t					*screenSsaoFbo;
	FBO_t					*hdrDepthFbo;
	FBO_t                   *renderCubeFbo;

	shader_t				*defaultShader;
	shader_t				*shadowShader;
	shader_t				*projectionShadowShader;

	shader_t				*flareShader;
	shader_t				*sunShader;
	shader_t				*sunFlareShader;

	qint						numLightmaps;
	qint						lightmapSize;
	image_t					**lightmaps;
	image_t					**deluxemaps;

	qint						fatLightmapCols;
	qint						fatLightmapRows;

	qint                     numCubemaps;
	cubemap_t               *cubemaps;

	trRefEntity_t			*currentEntity;
	trRefEntity_t			worldEntity;		// point currentEntity at this when rendering world
	qint						currentEntityNum;
	qint						shiftedEntityNum;	// currentEntityNum << QSORT_REFENTITYNUM_SHIFT
	model_t					*currentModel;

	//
	// GPU shader programs
	//
	shaderProgram_t genericShader[GENERICDEF_COUNT];
	shaderProgram_t textureColorShader;
	shaderProgram_t fogShader[FOGDEF_COUNT];
	shaderProgram_t dlightShader[DLIGHTDEF_COUNT];
	shaderProgram_t lightallShader[LIGHTDEF_COUNT];
	shaderProgram_t shadowmapShader[SHADOWMAPDEF_COUNT];
	shaderProgram_t pshadowShader;
	shaderProgram_t down4xShader;
	shaderProgram_t bokehShader;
	shaderProgram_t tonemapShader;
	shaderProgram_t calclevels4xShader[2];
	shaderProgram_t shadowmaskShader;
	shaderProgram_t ssaoShader;
	shaderProgram_t depthBlurShader[4];
	shaderProgram_t testcubeShader;


	// -----------------------------------------

	viewParms_t				viewParms;

	float					identityLight;		// 1.0 / ( 1 << overbrightBits )
	qint						identityLightByte;	// identityLight * 255
	qint						overbrightBits;		// r_overbrightBits->integer, but set to 0 if no hw gamma

	orientationr_t			or;					// for current entity

	trRefdef_t				refdef;

	qint						viewCluster;

	float                   sunShadowScale;

	qbool                sunShadows;
	vec3_t					sunLight;			// from the sky shader for this level
	vec3_t					sunDirection;
	vec3_t                  lastCascadeSunDirection;
	float                   lastCascadeSunMvp[16];

	frontEndCounters_t		pc;
	qint						frontEndMsec;		// not in pc due to clearing issue

	//
	// put large tables at the end, so most elements will be
	// within the +/32K indexed range on risc processors
	//
	model_t					*models[MAX_MOD_KNOWN];
	qint						numModels;

	qint						numImages;
	image_t					*images[MAX_DRAWIMAGES];

	qint						numFBOs;
	FBO_t					*fbos[MAX_FBOS];

	qint						numVaos;
	vao_t					*vaos[MAX_VAOS];

	// shader indexes from other modules will be looked up in tr.shaders[]
	// shader indexes from drawsurfs will be looked up in sortedShaders[]
	// lower indexed sortedShaders must be rendered first (opaque surfaces before translucent)
	qint						numShaders;
	shader_t				*shaders[MAX_SHADERS];
	shader_t				*sortedShaders[MAX_SHADERS];

	qint						numSkins;
	skin_t					*skins[MAX_SKINS];

	GLuint					sunFlareQuery[2];
	qint						sunFlareQueryIndex;
	qbool				sunFlareQueryActive[2];

	float					sinTable[FUNCTABLE_SIZE];
	float					squareTable[FUNCTABLE_SIZE];
	float					triangleTable[FUNCTABLE_SIZE];
	float					sawToothTable[FUNCTABLE_SIZE];
	float					inverseSawToothTable[FUNCTABLE_SIZE];
	float					fogTable[FOG_TABLE_SIZE];
	qbool				vertexLightingAllowed;
} trGlobals_t;

extern backEndState_t	backEnd;
extern trGlobals_t	tr;
extern glstate_t	glState;		// outside of TR since it shouldn't be cleared during ref re-init
extern glRefConfig_t glRefConfig;

//
// cvars
//
extern cvar_t	*r_flareSize;
extern cvar_t	*r_flareFade;
// coefficient for the flare intensity falloff function.
#define FLARE_STDCOEFF "150"
extern cvar_t	*r_flareCoeff;

extern cvar_t	*r_railWidth;
extern cvar_t	*r_railCoreWidth;
extern cvar_t	*r_railSegmentLength;

extern cvar_t	*r_ignore;				// used for debugging anything

extern cvar_t	*r_znear;				// near Z clip plane
extern cvar_t	*r_zproj;				// z distance of projection plane
extern cvar_t	*r_stereoSeparation;			// separation of cameras for stereo rendering

extern cvar_t	*r_measureOverdraw;		// enables stencil buffer overdraw measurement

extern cvar_t	*r_lodbias;				// push/pull LOD transitions
extern cvar_t	*r_lodscale;

extern cvar_t	*r_teleporterFlash;		// teleport hyperspace visual

extern cvar_t	*r_fastsky;				// controls whether sky should be cleared or drawn
extern cvar_t	*r_drawSun;				// controls drawing of sun quad
extern cvar_t	*r_dynamiclight;		// dynamic lights enabled/disabled
extern cvar_t	*r_dlightBacks;			// dlight non-facing surfaces for continuity

extern	cvar_t	*r_norefresh;			// bypasses the ref rendering
extern	cvar_t	*r_drawentities;		// disable/enable entity rendering
extern	cvar_t	*r_drawworld;			// disable/enable world rendering
extern	cvar_t	*r_speeds;				// various levels of information display
extern  cvar_t	*r_detailTextures;		// enables/disables detail texturing stages
extern	cvar_t	*r_novis;				// disable/enable usage of PVS
extern	cvar_t	*r_nocull;
extern	cvar_t	*r_facePlaneCull;		// enables culling of planar surfaces with back side test
extern	cvar_t	*r_nocurves;
extern	cvar_t	*r_showcluster;

extern cvar_t	*r_gamma;

extern  cvar_t  *r_ext_framebuffer_object;
extern  cvar_t  *r_ext_texture_float;
extern  cvar_t  *r_ext_framebuffer_multisample;
extern  cvar_t  *r_arb_seamless_cube_map;
extern  cvar_t  *r_arb_vertex_array_object;
extern  cvar_t  *r_ext_direct_state_access;

extern	cvar_t	*r_nobind;						// turns off binding to appropriate textures
extern	cvar_t	*r_singleShader;				// make most world faces use default shader
extern	cvar_t	*r_roundImagesDown;
extern	cvar_t	*r_colorMipLevels;				// development aid to see texture mip usage
extern	cvar_t	*r_picmip;						// controls picmip values
extern	cvar_t	*r_finish;
extern	cvar_t	*r_textureMode;
extern	cvar_t	*r_offsetFactor;
extern	cvar_t	*r_offsetUnits;

extern	cvar_t	*r_fullbright;					// avoid lightmap pass
extern	cvar_t	*r_lightmap;					// render lightmaps only
extern	cvar_t	*r_vertexLight;					// vertex lighting mode for better performance

extern	cvar_t	*r_showtris;					// enables wireframe rendering of the world
extern	cvar_t	*r_showsky;						// forces sky in front of all surfaces
extern	cvar_t	*r_shownormals;					// draws wireframe normals
extern	cvar_t	*r_clear;						// force screen clear every frame

extern	cvar_t	*r_shadows;						// controls shadows: 0 = none, 1 = blur, 2 = stencil, 3 = black planar projection
extern	cvar_t	*r_flares;						// light flares

extern	cvar_t	*r_intensity;

extern	cvar_t	*r_lockpvs;
extern	cvar_t	*r_noportals;
extern	cvar_t	*r_portalOnly;

extern	cvar_t	*r_subdivisions;
extern	cvar_t	*r_lodCurveError;
extern	cvar_t	*r_skipBackEnd;

extern	cvar_t	*r_anaglyphMode;

extern  cvar_t  *r_externalGLSL;

extern  cvar_t  *r_hdr;
extern  cvar_t  *r_floatLightmap;
extern  cvar_t  *r_postProcess;

extern  cvar_t  *r_toneMap;
extern  cvar_t  *r_forceToneMap;
extern  cvar_t  *r_forceToneMapMin;
extern  cvar_t  *r_forceToneMapAvg;
extern  cvar_t  *r_forceToneMapMax;

extern  cvar_t  *r_autoExposure;
extern  cvar_t  *r_forceAutoExposure;
extern  cvar_t  *r_forceAutoExposureMin;
extern  cvar_t  *r_forceAutoExposureMax;

extern  cvar_t  *r_cameraExposure;

extern  cvar_t  *r_depthPrepass;
extern  cvar_t  *r_ssao;

extern  cvar_t  *r_normalMapping;
extern  cvar_t  *r_specularMapping;
extern  cvar_t  *r_deluxeMapping;
extern  cvar_t  *r_parallaxMapping;
extern  cvar_t  *r_parallaxMapOffset;
extern  cvar_t  *r_parallaxMapShadows;
extern  cvar_t  *r_cubeMapping;
extern  cvar_t  *r_cubemapSize;
extern  cvar_t  *r_deluxeSpecular;
extern  cvar_t  *r_pbr;
extern  cvar_t  *r_baseNormalX;
extern  cvar_t  *r_baseNormalY;
extern  cvar_t  *r_baseParallax;
extern  cvar_t  *r_baseSpecular;
extern  cvar_t  *r_baseGloss;
extern  cvar_t  *r_glossType;
extern  cvar_t  *r_dlightMode;
extern  cvar_t  *r_pshadowDist;
extern  cvar_t  *r_mergeLightmaps;
extern  cvar_t  *r_imageUpsample;
extern  cvar_t  *r_imageUpsampleMaxSize;
extern  cvar_t  *r_imageUpsampleType;
extern  cvar_t  *r_genNormalMaps;
extern  cvar_t  *r_forceSun;
extern  cvar_t  *r_forceSunLightScale;
extern  cvar_t  *r_forceSunAmbientScale;
extern  cvar_t  *r_sunlightMode;
extern  cvar_t  *r_drawSunRays;
extern  cvar_t  *r_sunShadows;
extern  cvar_t  *r_shadowFilter;
extern  cvar_t  *r_shadowBlur;
extern  cvar_t  *r_shadowMapSize;
extern  cvar_t  *r_shadowCascadeZNear;
extern  cvar_t  *r_shadowCascadeZFar;
extern  cvar_t  *r_shadowCascadeZBias;
extern  cvar_t  *r_ignoreDstAlpha;

extern	cvar_t	*r_greyscale;

extern	cvar_t	*r_ignoreGLErrors;

extern	cvar_t	*r_overBrightBits;
extern	cvar_t	*r_mapOverBrightBits;

extern	cvar_t	*r_debugSurface;
extern	cvar_t	*r_simpleMipMaps;

extern	cvar_t	*r_showImages;
extern	cvar_t	*r_debugSort;

extern	cvar_t	*r_printShaders;

extern cvar_t	*r_marksOnTriangleMeshes;

//====================================================================

static ID_INLINE qbool ShaderRequiresCPUDeforms(const shader_t * shader)
{
	if(shader->numDeforms)
	{
		const deformStage_t *ds = &shader->deforms[0];

		if (shader->numDeforms > 1)
			return qtrue;

		switch (ds->deformation)
		{
			case DEFORM_WAVE:
			case DEFORM_BULGE:
				// need CPU deforms at high level-times to avoid floating point precision loss
				return ( backEnd.refdef.floatTime != (float)backEnd.refdef.floatTime );

			default:
				return qtrue;
		}
	}

	return qfalse;
}

//====================================================================

void R_SwapBuffers( qint );

void R_RenderView( const viewParms_t *parms );
void R_RenderDlightCubemaps(const refdef_t *fd);
void R_RenderPshadowMaps(const refdef_t *fd);
void R_RenderSunShadowMaps(const refdef_t *fd, qint level);
void R_RenderCubemapSide( qint cubemapIndex, qint cubemapSide, qbool subscene );

void R_AddMD3Surfaces( trRefEntity_t *e );
void R_AddNullModelSurfaces( trRefEntity_t *e );
void R_AddBeamSurfaces( trRefEntity_t *e );
void R_AddRailSurfaces( trRefEntity_t *e, qbool isUnderwater );
void R_AddLightningBoltSurfaces( trRefEntity_t *e );

void R_AddPolygonSurfaces( void );

void R_DecomposeSort( unsigned sort, qint *entityNum, shader_t **shader, 
					 qint *fogNum, qint *dlightMap, qint *pshadowMap );

void R_AddDrawSurf( surfaceType_t *surface, shader_t *shader, 
				   qint fogIndex, qint dlightMap, qint pshadowMap, qint cubemap );

void R_CalcTexDirs(vec3_t sdir, vec3_t tdir, const vec3_t v1, const vec3_t v2,
				   const vec3_t v3, const vec2_t w1, const vec2_t w2, const vec2_t w3);
vec_t R_CalcTangentSpace(vec3_t tangent, vec3_t bitangent, const vec3_t normal, const vec3_t sdir, const vec3_t tdir);
qbool R_CalcTangentVectors(srfVert_t * dv[3]);

#define	CULL_IN		0		// completely unclipped
#define	CULL_CLIP	1		// clipped by one or more planes
#define	CULL_OUT	2		// completely outside the clipping planes

void R_LocalPointToWorld (const vec3_t local, vec3_t world);
qint R_CullBox (vec3_t bounds[2]);
qint R_CullLocalBox( const vec3_t bounds[2] );
qint R_CullPointAndRadiusEx( const vec3_t origin, float radius, const cplane_t* frustum, qint numPlanes );
qint R_CullPointAndRadius( const vec3_t origin, float radius );
qint R_CullLocalPointAndRadius( const vec3_t origin, float radius );

void R_SetupProjection(viewParms_t *dest, float zProj, float zFar, qbool computeFrustum);
void R_RotateForEntity( const trRefEntity_t *ent, const viewParms_t *viewParms, orientationr_t *or );

/*
** GL wrapper/helper functions
*/
void	GL_BindToTMU( image_t *image, qint tmu );
void	GL_SetDefaultState (void);
void	GL_TextureMode( const qchar *string );
void	GL_CheckErrs( qchar *file, qint line );
#define GL_CheckErrors(...) GL_CheckErrs(__FILE__, __LINE__)
void	GL_State( unsigned long stateVector );
void    GL_SetProjectionMatrix(mat4_t matrix);
void    GL_SetModelviewMatrix(mat4_t matrix);
void	GL_Cull( qint cullType );

#define GLS_SRCBLEND_ZERO						0x00000001
#define GLS_SRCBLEND_ONE						0x00000002
#define GLS_SRCBLEND_DST_COLOR					0x00000003
#define GLS_SRCBLEND_ONE_MINUS_DST_COLOR		0x00000004
#define GLS_SRCBLEND_SRC_ALPHA					0x00000005
#define GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA		0x00000006
#define GLS_SRCBLEND_DST_ALPHA					0x00000007
#define GLS_SRCBLEND_ONE_MINUS_DST_ALPHA		0x00000008
#define GLS_SRCBLEND_ALPHA_SATURATE				0x00000009
#define GLS_SRCBLEND_BITS						0x0000000f

#define GLS_DSTBLEND_ZERO						0x00000010
#define GLS_DSTBLEND_ONE						0x00000020
#define GLS_DSTBLEND_SRC_COLOR					0x00000030
#define GLS_DSTBLEND_ONE_MINUS_SRC_COLOR		0x00000040
#define GLS_DSTBLEND_SRC_ALPHA					0x00000050
#define GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA		0x00000060
#define GLS_DSTBLEND_DST_ALPHA					0x00000070
#define GLS_DSTBLEND_ONE_MINUS_DST_ALPHA		0x00000080
#define GLS_DSTBLEND_BITS						0x000000f0

#define GLS_DEPTHMASK_TRUE						0x00000100

#define GLS_POLYMODE_LINE						0x00001000

#define GLS_DEPTHTEST_DISABLE					0x00010000
#define GLS_DEPTHFUNC_EQUAL						0x00020000
#define GLS_DEPTHFUNC_GREATER                   0x00040000
#define GLS_DEPTHFUNC_BITS                      0x00060000

#define GLS_ATEST_GT_0							0x10000000
#define GLS_ATEST_LT_80							0x20000000
#define GLS_ATEST_GE_80							0x40000000
#define GLS_ATEST_BITS							0x70000000

#define GLS_DEFAULT			GLS_DEPTHMASK_TRUE

void	RE_StretchRaw( qint x, qint y, qint w, qint h, qint cols, qint rows, const byte *data, qint client, qbool dirty );
void	RE_UploadCinematic( qint w, qint h, qint cols, qint rows, const byte *data, qint client, qbool dirty );

void		RE_BeginFrame( stereoFrame_t stereoFrame );
void		RE_BeginRegistration( glconfig_t *glconfig );
void		RE_LoadWorldMap( const qchar *mapname );
void		RE_SetWorldVisData( const byte *vis );
qhandle_t	RE_RegisterModel( const qchar *name );
qhandle_t	RE_RegisterSkin( const qchar *name );

qbool	R_GetEntityToken( qchar *buffer, qint size );

model_t		*R_AllocModel( void );

void    	R_Init( void );
void		R_UpdateSubImage( image_t *image, byte *pic, qint x, qint y, qint width, qint height, GLenum picFormat );

void		R_SetColorMappings( void );
void		R_GammaCorrect( byte *buffer, qint bufSize );

void	R_ImageList_f( void );
void	R_SkinList_f( void );
// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=516
const void *RB_TakeScreenshotCmd( const void *data );

void	R_InitFogTable( void );
float	R_FogFactor( float s, float t );
void	R_InitImages( void );
void	R_DeleteTextures( void );
qint		R_SumOfUsedImages( void );
void	R_InitSkins( void );
skin_t	*R_GetSkinByHandle( qhandle_t hSkin );

qint R_ComputeLOD( trRefEntity_t *ent );

const void *RB_TakeVideoFrameCmd( const void *data );

//
// tr_shader.c
//
shader_t	*R_FindShader( const qchar *name, qint lightmapIndex, qbool mipRawImage );
shader_t	*R_GetShaderByHandle( qhandle_t hShader );
shader_t	*R_GetShaderByState( qint index, long *cycleTime );
shader_t *R_FindShaderByName( const qchar *name );
void		R_InitShaders( void );
void		R_ShaderList_f( void );
void    R_RemapShader(const qchar *oldShader, const qchar *newShader, const qchar *timeOffset);

/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/

void		GLimp_InitExtraExtensions( void );

/*
====================================================================

TESSELATOR/SHADER DECLARATIONS

====================================================================
*/

typedef struct stageVars
{
	color4ub_t	colors[SHADER_MAX_VERTEXES];
	vec2_t		texcoords[NUM_TEXTURE_BUNDLES][SHADER_MAX_VERTEXES];
} stageVars_t;

typedef struct shaderCommands_s 
{
	glIndex_t	indexes[SHADER_MAX_INDEXES] QALIGN(16);
	vec4_t		xyz[SHADER_MAX_VERTEXES] QALIGN(16);
	int16_t		normal[SHADER_MAX_VERTEXES][4] QALIGN(16);
	int16_t		tangent[SHADER_MAX_VERTEXES][4] QALIGN(16);
	vec2_t		texCoords[SHADER_MAX_VERTEXES] QALIGN(16);
	vec2_t		lightCoords[SHADER_MAX_VERTEXES] QALIGN(16);
	uint16_t	color[SHADER_MAX_VERTEXES][4] QALIGN(16);
	int16_t		lightdir[SHADER_MAX_VERTEXES][4] QALIGN(16);
	//qint			vertexDlightBits[SHADER_MAX_VERTEXES] QALIGN(16);

	void *attribPointers[ATTR_INDEX_COUNT];
	vao_t       *vao;
	qbool    useInternalVao;
	qbool    useCacheVao;

	stageVars_t	svars QALIGN(16);

	//color4ub_t	constantColor255[SHADER_MAX_VERTEXES] QALIGN(16);

	shader_t	*shader;
	double		shaderTime;	// -EC- set to double for frameloss fix
	qint			fogNum;
	qint         cubemapIndex;

	qint			dlightBits;	// or together of all vertexDlightBits
	qint         pshadowBits;

	qint			firstIndex;
	qint			numIndexes;
	qint			numVertexes;

	// info extracted from current shader
	qint			numPasses;
	void		(*currentStageIteratorFunc)( void );
	shaderStage_t	**xstages;
} shaderCommands_t;

extern	shaderCommands_t	tess;

void RB_BeginSurface(shader_t *shader, qint fogNum, qint cubemapIndex );
void RB_EndSurface(void);
void RB_CheckOverflow( qint verts, qint indexes );
#define RB_CHECKOVERFLOW(v,i) if (tess.numVertexes + (v) >= SHADER_MAX_VERTEXES || tess.numIndexes + (i) >= SHADER_MAX_INDEXES ) {RB_CheckOverflow(v,i);}

void R_DrawElements( qint numIndexes, glIndex_t firstIndex );
void RB_StageIteratorGeneric( void );
void RB_StageIteratorSky( void );
void RB_StageIteratorVertexLitTexture( void );
void RB_StageIteratorLightmappedMultitexture( void );

void RB_AddQuadStamp( const vec3_t origin, const vec3_t left, const vec3_t up, const float color[4] );
void RB_AddQuadStampExt( const vec3_t origin, const vec3_t left, const vec3_t up, const float color[4], float s1, float t1, float s2, float t2 );
void RB_InstantQuad( vec4_t quadVerts[4] );
//void RB_InstantQuad2(vec4_t quadVerts[4], vec2_t texCoords[4], vec4_t color, shaderProgram_t *sp, vec2_t invTexRes);
void RB_InstantQuad2(vec4_t quadVerts[4], vec2_t texCoords[4]);

void RB_ShowImages( void );


/*
============================================================

WORLD MAP

============================================================
*/

void R_AddBrushModelSurfaces( trRefEntity_t *e );
void R_AddWorldSurfaces( void );
qbool R_inPVS( const vec3_t p1, const vec3_t p2 );


/*
============================================================

FLARES

============================================================
*/

void R_ClearFlares( void );

void RB_AddFlare( void *surface, qint fogNum, vec3_t point, vec3_t color, vec3_t normal );
void RB_AddDlightFlares( void );
void RB_RenderFlares (void);

/*
============================================================

LIGHTS

============================================================
*/

void R_DlightBmodel( bmodel_t *bmodel );
void R_SetupEntityLighting( const trRefdef_t *refdef, trRefEntity_t *ent );
void R_TransformDlights( qint count, dlight_t *dl, orientationr_t *or );
qint R_LightForPoint( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );
qint R_LightDirForPoint( vec3_t point, vec3_t lightDir, vec3_t normal, world_t *world );
qint R_CubemapForPoint( vec3_t point );


/*
============================================================

SHADOWS

============================================================
*/

void RB_ShadowTessEnd( void );
void RB_ShadowFinish( void );
void RB_ProjectionShadowDeform( void );

/*
============================================================

SKIES

============================================================
*/

void R_InitSkyTexCoords( float cloudLayerHeight );
void R_DrawSkyBox( const shaderCommands_t *shader );
void RB_DrawSun( float scale, shader_t *shader );

/*
============================================================

CURVE TESSELATION

============================================================
*/

#define PATCH_STITCHING

void R_SubdividePatchToGrid( srfBspSurface_t *grid, qint width, qint height,
								srfVert_t points[MAX_PATCH_SIZE*MAX_PATCH_SIZE] );
void R_GridInsertColumn( srfBspSurface_t *grid, qint column, qint row, vec3_t point, float loderror );
void R_GridInsertRow( srfBspSurface_t *grid, qint row, qint column, vec3_t point, float loderror );

/*
============================================================

MARKERS, POLYGON PROJECTION ON WORLD POLYGONS

============================================================
*/

qint R_MarkFragments( qint numPoints, const vec3_t *points, const vec3_t projection,
				   qint maxPoints, vec3_t pointBuffer, qint maxFragments, markFragment_t *fragmentBuffer );


/*
============================================================

VERTEX BUFFER OBJECTS

============================================================
*/

void R_VaoPackTangent(int16_t *out, vec4_t v);
void R_VaoPackNormal(int16_t *out, vec3_t v);
void R_VaoPackColor(uint16_t *out, const vec4_t c);
void R_VaoUnpackTangent(vec4_t v, int16_t *pack);
void R_VaoUnpackNormal(vec3_t v, int16_t *pack);

vao_t          *R_CreateVao(const qchar *name, byte *vertexes, qint vertexesSize, byte *indexes, qint indexesSize, vaoUsage_t usage);
vao_t          *R_CreateVao2(const qchar *name, qint numVertexes, srfVert_t *verts, qint numIndexes, glIndex_t *inIndexes);

void            R_BindVao(vao_t *vao);
void            R_BindNullVao(void);

void Vao_SetVertexPointers(vao_t *vao);

void            R_InitVaos(void);
void            R_ShutdownVaos(void);
void            R_VaoList_f(void);

void            RB_UpdateTessVao(unsigned qint attribBits);

void VaoCache_Commit(void);
void VaoCache_Init(void);
void VaoCache_BindVao(void);
void VaoCache_CheckAdd(qbool *endSurface, qbool *recycleVertexBuffer, qbool *recycleIndexBuffer, qint numVerts, qint numIndexes);
void VaoCache_RecycleVertexBuffer(void);
void VaoCache_RecycleIndexBuffer(void);
void VaoCache_InitQueue(void);
void VaoCache_AddSurface(srfVert_t *verts, qint numVerts, glIndex_t *indexes, qint numIndexes);

/*
============================================================

GLSL

============================================================
*/

void GLSL_InitGPUShaders(void);
void GLSL_ShutdownGPUShaders(void);
void GLSL_VertexAttribPointers(uint32_t attribBits);
void GLSL_BindProgram(shaderProgram_t * program);

void GLSL_SetUniformInt(shaderProgram_t *program, qint uniformNum, GLint value);
void GLSL_SetUniformFloat(shaderProgram_t *program, qint uniformNum, GLfloat value);
void GLSL_SetUniformFloat5(shaderProgram_t *program, qint uniformNum, const vec5_t v);
void GLSL_SetUniformVec2(shaderProgram_t *program, qint uniformNum, const vec2_t v);
void GLSL_SetUniformVec3(shaderProgram_t *program, qint uniformNum, const vec3_t v);
void GLSL_SetUniformVec4(shaderProgram_t *program, qint uniformNum, const vec4_t v);
void GLSL_SetUniformMat4(shaderProgram_t *program, qint uniformNum, const mat4_t matrix);
void GLSL_SetUniformMat4BoneMatrix(shaderProgram_t *program, qint uniformNum, /*const*/ mat4_t *matrix, qint numMatricies);

shaderProgram_t *GLSL_GetGenericShaderProgram(qint stage);

/*
============================================================

SCENE GENERATION

============================================================
*/

void R_InitNextFrame( void );

void RE_ClearScene( void );
void RE_AddRefEntityToScene( const refEntity_t *ent, qbool intShaderTime );
void RE_AddPolyToScene( qhandle_t hShader , qint numVerts, const polyVert_t *verts, qint num );
void RE_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );
void RE_AddAdditiveLightToScene( const vec3_t org, float intensity, float r, float g, float b );
void RE_BeginScene( const refdef_t *fd );
void RE_RenderScene( const refdef_t *fd );
void RE_EndScene( void );

/*
=============================================================

UNCOMPRESSING BONES

=============================================================
*/

#define MC_BITS_X (16)
#define MC_BITS_Y (16)
#define MC_BITS_Z (16)
#define MC_BITS_VECT (16)

#define MC_SCALE_X (1.0f/64)
#define MC_SCALE_Y (1.0f/64)
#define MC_SCALE_Z (1.0f/64)

void MC_UnCompress(float mat[3][4],const unsigned qchar * comp);

/*
=============================================================

ANIMATED MODELS

=============================================================
*/

void R_MDRAddAnimSurfaces( trRefEntity_t *ent );
void RB_MDRSurfaceAnim( mdrSurface_t *surface );
qbool R_LoadIQM (model_t *mod, void *buffer, qint filesize, const qchar *name );
void R_AddIQMSurfaces( trRefEntity_t *ent );
void RB_IQMSurfaceAnim( const surfaceType_t *surface );
void RB_IQMSurfaceAnimVao( const srfVaoIQModel_t *surface );
qint R_IQMLerpTag( orientation_t *tag, iqmData_t *data,
                  qint startFrame, qint endFrame,
                  float frac, const qchar *tagName );

/*
=============================================================
=============================================================
*/
void	R_TransformModelToClip( const vec3_t src, const float *modelMatrix, const float *projectionMatrix,
							vec4_t eye, vec4_t dst );
void	R_TransformClipToWindow( const vec4_t clip, const viewParms_t *view, vec4_t normalized, vec4_t window );

void	RB_DeformTessGeometry( void );

void	RB_CalcFogTexCoords( float *dstTexCoords );

void	RB_CalcScaleTexMatrix( const float scale[2], float *matrix );
void	RB_CalcScrollTexMatrix( const float scrollSpeed[2], float *matrix );
void	RB_CalcRotateTexMatrix( float degsPerSecond, float *matrix );
void RB_CalcTurbulentFactors( const waveForm_t *wf, float *amplitude, float *now );
void	RB_CalcTransformTexMatrix( const texModInfo_t *tmi, float *matrix  );
void	RB_CalcStretchTexMatrix( const waveForm_t *wf, float *matrix );

void	RB_CalcModulateColorsByFog( unsigned qchar *dstColors );
float	RB_CalcWaveAlphaSingle( const waveForm_t *wf );
float	RB_CalcWaveColorSingle( const waveForm_t *wf );

/*
=============================================================

RENDERER BACK END FUNCTIONS

=============================================================
*/

void RB_ExecuteRenderCommands( const void *data );

/*
=============================================================

RENDERER BACK END COMMAND QUEUE

=============================================================
*/

#define	MAX_RENDER_COMMANDS	0x40000

typedef struct {
	byte	cmds[MAX_RENDER_COMMANDS];
	qint		used;
} renderCommandList_t;

typedef struct {
	qint		commandId;
	float	color[4];
} setColorCommand_t;

typedef struct {
	qint		commandId;
	qint		buffer;
} drawBufferCommand_t;

typedef struct {
	qint		commandId;
	image_t	*image;
	qint		width;
	qint		height;
	void	*data;
} subImageCommand_t;

typedef struct {
	qint		commandId;
} swapBuffersCommand_t;

typedef struct {
	qint		commandId;
	qint		buffer;
} endFrameCommand_t;

typedef struct {
	qint		commandId;
	shader_t	*shader;
	float	x, y;
	float	w, h;
	float	s1, t1;
	float	s2, t2;
} stretchPicCommand_t;

typedef struct {
	qint		commandId;
	trRefdef_t	refdef;
	viewParms_t	viewParms;
	drawSurf_t *drawSurfs;
	qint		numDrawSurfs;
} drawSurfsCommand_t;

typedef struct {
	qint commandId;
	qint x;
	qint y;
	qint width;
	qint height;
	const qchar *fileName;
	qbool jpeg;
} screenshotCommand_t;

typedef struct {
	qint						commandId;
	qint						width;
	qint						height;
	byte					*captureBuffer;
	byte					*encodeBuffer;
	qbool			motionJpeg;
} videoFrameCommand_t;

typedef struct
{
	qint commandId;

	GLboolean rgba[4];
} colorMaskCommand_t;

typedef struct
{
	qint commandId;
} clearDepthCommand_t;

typedef struct {
	qint commandId;
	qint map;
	qint cubeSide;
} capShadowmapCommand_t;

typedef struct {
	qint		commandId;
	trRefdef_t	refdef;
	viewParms_t	viewParms;
} postProcessCommand_t;

typedef struct {
	qint commandId;
} exportCubemapsCommand_t;

typedef enum {
	RC_END_OF_LIST,
	RC_SET_COLOR,
	RC_STRETCH_PIC,
	RC_DRAW_SURFS,
	RC_DRAW_BUFFER,
	RC_SWAP_BUFFERS,
	RC_SCREENSHOT,
	RC_VIDEOFRAME,
	RC_COLORMASK,
	RC_CLEARDEPTH,
	RC_CAPSHADOWMAP,
	RC_POSTPROCESS,
	RC_EXPORT_CUBEMAPS
} renderCommand_t;


// these are sort of arbitrary limits.
// the limits apply to the sum of all scenes in a frame --
// the main view, all the 3D icons, etc
#define	MAX_POLYS		600
#define	MAX_POLYVERTS	3000

// all of the information needed by the back end must be
// contained in a backEndData_t
typedef struct {
	drawSurf_t	drawSurfs[MAX_DRAWSURFS];
	dlight_t	dlights[MAX_DLIGHTS];
	trRefEntity_t	entities[MAX_REFENTITIES];
	srfPoly_t	*polys;//[MAX_POLYS];
	polyVert_t	*polyVerts;//[MAX_POLYVERTS];
	pshadow_t pshadows[MAX_CALC_PSHADOWS];
	renderCommandList_t	commands;
} backEndData_t;

extern	qint		max_polys;
extern	qint		max_polyverts;

extern	backEndData_t	*backEndData;	// the second one may not be allocated


void *R_GetCommandBuffer( qint bytes );
void RB_ExecuteRenderCommands( const void *data );

void R_IssuePendingRenderCommands( void );

void R_AddDrawSurfCmd( drawSurf_t *drawSurfs, qint numDrawSurfs );
void R_AddCapShadowmapCmd( qint dlight, qint cubeSide );
void R_AddPostProcessCmd (void);

void RE_SetColor( const float *rgba );
void RE_StretchPic ( float x, float y, float w, float h, 
					  float s1, float t1, float s2, float t2, qhandle_t hShader );
void RE_BeginFrame( stereoFrame_t stereoFrame );
void RE_EndFrame( qint *frontEndMsec, qint *backEndMsec );
void RE_TakeVideoFrame( qint width, qint height,
		byte *captureBuffer, byte *encodeBuffer, qbool motionJpeg );

void RE_FinishBloom( void );
void RE_ThrottleBackend( void );
qbool RE_CanMinimize( void );
const glconfig_t *RE_GetConfig( void );
void RE_VertexLighting( qbool allowed );

#endif //TR_LOCAL_H
