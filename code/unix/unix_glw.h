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
#if !( defined __linux__ || defined __FreeBSD__ || defined __OpenBSD__ || defined __sun || defined __APPLE__ )
#error You should include this file only on Linux/FreeBSD/Solaris platforms
#endif

#ifndef __GLW_LINUX_H__
#define __GLW_LINUX_H__

#include <X11/Xlib.h>
#include <X11/Xfuncproto.h>

typedef struct sym_s
{
	void **symbol;
	const qchar *name;
} sym_t;

typedef struct
{
	void *OpenGLLib; // instance of OpenGL library
	void *VulkanLib; // instance of Vulkan library
	FILE *log_fp;

	qint	monitorCount;

	qbool gammaSet;

	qbool cdsFullscreen;

	glconfig_t *config; // feedback to renderer module

	qbool dga_ext;

	qbool vidmode_ext;
	qbool vidmode_active;
	qbool vidmode_gamma;

	qbool randr_ext;
	qbool randr_active;
	qbool randr_gamma;

	qbool desktop_ok;
	qint desktop_width;
	qint desktop_height;
	qint desktop_x;
	qint desktop_y;

} glwstate_t;

extern glwstate_t glw_state;
extern Display *dpy;
extern Window win;
extern qint scrnum;

qbool BuildGammaRampTable( unsigned qchar *red, unsigned qchar *green, unsigned qchar *blue, qint gammaRampSize, unsigned short table[3][4096] );

// DGA extension
qbool DGA_Init( Display *_dpy );
void DGA_Mouse( qbool enable );
void DGA_Done( void );

// VidMode extension
qbool VidMode_Init( void );
void VidMode_Done( void );
qbool VidMode_SetMode( qint *width, qint *height, qint *rate );
void VidMode_RestoreMode( void );
void VidMode_SetGamma( unsigned qchar red[256], unsigned qchar green[256], unsigned qchar blue[256] );
void VidMode_RestoreGamma( void );

// XRandR extension
qbool RandR_Init( qint x, qint y, qint w, qint h );
void RandR_Done( void );
void RandR_UpdateMonitor( qint x, qint y, qint w, qint h );
qbool RandR_SetMode( qint *width, qint *height, qint *rate );
void RandR_RestoreMode( void );
void RandR_SetGamma( unsigned qchar red[256], unsigned qchar green[256], unsigned qchar blue[256] );
void RandR_RestoreGamma( void );

#endif
