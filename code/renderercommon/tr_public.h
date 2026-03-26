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
#ifndef __TR_PUBLIC_H
#define __TR_PUBLIC_H

#include "tr_types.h"
#include "vulkan/vulkan.h"

#define	REF_API_VERSION		8

//
// these are the functions exported by the refresh module
//
typedef enum {
	REF_KEEP_CONTEXT, // don't destroy window and context
	REF_KEEP_WINDOW,  // destroy context, keep window
	REF_DESTROY_WINDOW,
	REF_UNLOAD_DLL
} refShutdownCode_t;

typedef struct {
	// called before the library is unloaded
	// if the system is just reconfiguring, pass destroyWindow = qfalse,
	// which will keep the screen from flashing to the desktop.
	void	(*Shutdown)( refShutdownCode_t code );

	// All data that will be used in a level should be
	// registered before rendering any frames to prevent disk hits,
	// but they can still be registered at a later time
	// if necessary.
	//
	// BeginRegistration makes any existing media pointers invalid
	// and returns the current gl configuration, including screen width
	// and height, which can be used by the client to intelligently
	// size display elements
	void	(*BeginRegistration)( glconfig_t *config );
	qhandle_t (*RegisterModel)( const qchar *name );
	qhandle_t (*RegisterSkin)( const qchar *name );
	qhandle_t (*RegisterShader)( const qchar *name );
	qhandle_t (*RegisterShaderNoMip)( const qchar *name );
	void	(*LoadWorld)( const qchar *name );

	// the vis data is a large enough block of data that we go to the trouble
	// of sharing it with the clipmodel subsystem
	void	(*SetWorldVisData)( const byte *vis );

	// EndRegistration will draw a tiny polygon with each texture, forcing
	// them to be loaded into card memory
	void	(*EndRegistration)( void );

	// a scene is built up by calls to R_ClearScene and the various R_Add functions.
	// Nothing is drawn until R_RenderScene is called.
	void	(*ClearScene)( void );
	void	(*AddRefEntityToScene)( const refEntity_t *re, qbool intShaderTime );
	void	(*AddPolyToScene)( qhandle_t hShader , qint numVerts, const polyVert_t *verts, qint num );
	qint		(*LightForPoint)( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );
	void	(*AddLightToScene)( const vec3_t org, float intensity, float r, float g, float b );
	void	(*AddAdditiveLightToScene)( const vec3_t org, float intensity, float r, float g, float b );
	void	(*AddLinearLightToScene)( const vec3_t start, const vec3_t end, float intensity, float r, float g, float b );
	void	(*RenderScene)( const refdef_t *fd );

	void	(*SetColor)( const float *rgba );	// NULL = 1,1,1,1
	void	(*DrawStretchPic) ( float x, float y, float w, float h,
		float s1, float t1, float s2, float t2, qhandle_t hShader );	// 0 = white

	// Draw images for cinematic rendering, pass as 32 bit rgba
	void	(*DrawStretchRaw)( qint x, qint y, qint w, qint h, qint cols, qint rows, byte *data, qint client, qbool dirty );
	void	(*UploadCinematic)( qint w, qint h, qint cols, qint rows, byte *data, qint client, qbool dirty );

	void	(*BeginFrame)( stereoFrame_t stereoFrame );

	// if the pointers are not NULL, timing info will be returned
	void	(*EndFrame)( qint *frontEndMsec, qint *backEndMsec );


	qint		(*MarkFragments)( qint numPoints, const vec3_t *points, const vec3_t projection,
				   qint maxPoints, vec3_t pointBuffer, qint maxFragments, markFragment_t *fragmentBuffer );

	qint		(*LerpTag)( orientation_t *tag,  qhandle_t model, qint startFrame, qint endFrame,
					 float frac, const qchar *tagName );
	void	(*ModelBounds)( qhandle_t model, vec3_t mins, vec3_t maxs );

#ifdef __USEA3D
	void    (*A3D_RenderGeometry) (void *pVoidA3D, void *pVoidGeom, void *pVoidMat, void *pVoidGeomStatus);
#endif
	void	(*RegisterFont)(const qchar *fontName, qint pointSize, fontInfo_t *font);
	void	(*RemapShader)(const qchar *oldShader, const qchar *newShader, const qchar *offsetTime);
	qbool (*GetEntityToken)( qchar *buffer, qint size );
	qbool (*inPVS)( const vec3_t p1, const vec3_t p2 );

	void	(*TakeVideoFrame)( qint h, qint w, byte* captureBuffer, byte *encodeBuffer, qbool motionJpeg );

	void	(*ThrottleBackend)( void );
	void	(*FinishBloom)( void );

	void	(*SetColorMappings)( void );

	qbool (*CanMinimize)( void ); // == fbo enabled

	const glconfig_t *(*GetConfig)( void );

	void	(*VertexLighting)( qbool allowed );
	void	(*SyncRender)( void );


} refexport_t;

//
// these are the functions imported by the refresh module
//
typedef struct {
	// print message on the local console
	void	FORMAT_PRINTF(2, 3) (QDECL *Printf)( printParm_t printLevel, const qchar *fmt, ... );

	// abort the game
	void	NORETURN_PTR FORMAT_PRINTF(2, 3)(QDECL *Error)( errorParm_t errorLevel, const qchar *fmt, ... );

	// milliseconds should only be used for profiling, never
	// for anything game related.  Get time from the refdef
	qint		(*Milliseconds)( void );

	int64_t	(*Microseconds)( void );

	// stack based memory allocation for per-level things that
	// won't be freed
#ifdef HUNK_DEBUG
	void	*(*Hunk_AllocDebug)( size_t size, ha_pref pref, const qchar *label, const qchar *file, qint line );
#else
	void	*(*Hunk_Alloc)( size_t size, ha_pref pref );
#endif
	void	*(*Hunk_AllocateTempMemory)( size_t size );
	void	(*Hunk_FreeTempMemory)( void *block );

	// dynamic memory allocator for things that need to be freed
	void	*(*Malloc)( size_t bytes );
	void	(*Free)( void *buf );
	void	(*FreeAll)( void );

	cvar_t	*(*Cvar_Get)( const qchar *name, const qchar *value, qint flags );
	void	(*Cvar_Set)( const qchar *name, const qchar *value );
	void	(*Cvar_SetValue) (const qchar *name, float value);
	void	(*Cvar_CheckRange)( cvar_t *cv, const qchar *minVal, const qchar *maxVal, cvarValidator_t type );
	void	(*Cvar_SetDescription)( cvar_t *cv, const qchar *description );

	void	(*Cvar_SetGroup)( cvar_t *var, cvarGroup_t group );
	qint		(*Cvar_CheckGroup)( cvarGroup_t group );
	void	(*Cvar_ResetGroup)( cvarGroup_t group, qbool resetModifiedFlags );

	void	(*Cvar_VariableStringBuffer)( const qchar *var_name, qchar *buffer, qint bufsize );
	const qchar *(*Cvar_VariableString)( const qchar *var_name );
	qint		(*Cvar_VariableIntegerValue)( const qchar *var_name );

	void	(*Cmd_AddCommand)( const qchar *name, void(*cmd)(void) );
	void	(*Cmd_RemoveCommand)( const qchar *name );

	qint		(*Cmd_Argc) (void);
	const qchar	*(*Cmd_Argv) (qint i);

	void	(*Cmd_ExecuteText)( cbufExec_t exec_when, const qchar *text );

	byte	*(*CM_ClusterPVS)(qint cluster);

	// visualization for debugging collision detection
	void	(*CM_DrawDebugSurface)( void (*drawPoly)(qint color, qint numPoints, float *points) );

	// a qfalse return means the file does not exist
	// NULL can be passed for buf to just determine existence
	//qint		(*FS_FileIsInPAK)( const qchar *name, qint *pCheckSum );
	qint		(*FS_ReadFile)( const qchar *name, void **buf );
	void	(*FS_FreeFile)( void *buf );
	qchar **	(*FS_ListFiles)( const qchar *name, const qchar *extension, qint *numfilesfound );
	void	(*FS_FreeFileList)( qchar **filelist );
	void	(*FS_WriteFile)( const qchar *qpath, const void *buffer, qint size );
	qbool (*FS_FileExists)( const qchar *file );

	// cinematic stuff
	void	(*CIN_UploadCinematic)( qint handle );
	qint		(*CIN_PlayCinematic)( const qchar *arg0, qint xpos, qint ypos, qint width, qint height, qint bits );
	e_status (*CIN_RunCinematic)( qint handle );

	void	(*CL_WriteAVIVideoFrame)( const byte *buffer, qint size );

	size_t	(*CL_SaveJPGToBuffer)( byte *buffer, size_t bufSize, qint quality, qint image_width, qint image_height, byte *image_buffer, qint padding );
	void	(*CL_SaveJPG)( const qchar *filename, qint quality, qint image_width, qint image_height, byte *image_buffer, qint padding );
	void	(*CL_LoadJPG)( const qchar *filename, unsigned qchar **pic, qint *width, qint *height );

	qbool (*CL_IsMinimized)( void );
	void	(*CL_SetScaling)( float factor, qint captureWidth, qint captureHeight );

	void	(*Sys_SetClipboardBitmap)( const byte *bitmap, qint size );
	qbool(*Sys_LowPhysicalMemory)( void );

	qint		(*Com_RealTime)( qtime_t *qtime );

	// platform-dependent functions
	void(*GLimp_InitGamma)(glconfig_t *config);
	void(*GLimp_SetGamma)(unsigned qchar red[256], unsigned qchar green[256], unsigned qchar blue[256]);

	// OpenGL
	void	(*GLimp_Init)( glconfig_t *config );
	void	(*GLimp_Shutdown)( qbool unloadDLL );
	void	(*GLimp_EndFrame)( void );
	void*	(*GL_GetProcAddress)( const qchar *name );

	// Vulkan
	void	(*VKimp_Init)( glconfig_t *config );
	void	(*VKimp_Shutdown)( qbool unloadDLL );
	void*	(*VK_GetInstanceProcAddr)( VkInstance instance, const qchar *name );
	qbool (*VK_CreateSurface)( VkInstance instance, VkSurfaceKHR *pSurface );

} refimport_t;

extern	refimport_t	ri;

// this is the only function actually exported at the linker level
// If the module can't init to a valid rendering state, NULL will be
// returned.
#ifdef USE_RENDERER_DLOPEN
typedef	refexport_t* (QDECL *GetRefAPI_t) (qint apiVersion, refimport_t * rimp);
#else
refexport_t*GetRefAPI( qint apiVersion, refimport_t *rimp );
#endif

#endif	// __TR_PUBLIC_H
