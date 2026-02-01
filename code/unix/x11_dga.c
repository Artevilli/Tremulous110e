#include "../client/client.h"
#include "unix_glw.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

#include <X11/extensions/Xxf86dga.h>

static void *d_lib = NULL;

Bool (*_XF86DGAQueryVersion)( Display *dpy, qint *majorVersion, qint *minorVersion );
Bool (*_XF86DGAQueryExtension)( Display *dpy, qint *event_base, qint *error_base );
Status (*_XF86DGADirectVideo)( Display *dpy, qint screen, qint enable );

static sym_t d_list[] =
{
	{ (void**)&_XF86DGAQueryVersion, "XF86DGAQueryVersion" },
	{ (void**)&_XF86DGAQueryExtension, "XF86DGAQueryExtension" },
	{ (void**)&_XF86DGADirectVideo, "XF86DGADirectVideo" }
};

qbool DGA_Init( Display *_dpy )
{
	qint event_base, error_base;
	qint ver_major = 0, ver_minor = 0;
	qint i;

	glw_state.dga_ext = qfalse;

	if ( d_lib == NULL )
	{
		d_lib = Sys_LoadLibrary( "libXxf86dga.so.1" );
		if ( d_lib == NULL )
		{
			d_lib = Sys_LoadLibrary( "libXxf86dga.so.1" );
		}
		if ( d_lib == NULL )
		{
			Com_Printf( "...error loading libXxf86dga\n" );
			goto __fail;
		}
	}

	for ( i = 0 ; i < ARRAY_LEN( d_list ); i++ )
	{
		*d_list[ i ].symbol = Sys_LoadFunction( d_lib, d_list[ i ].name );
		if ( *d_list[ i ].symbol == NULL )
		{
			Com_Printf( "...couldn't find '%s' in libXxf86dga\n", d_list[ i ].name );
			goto __fail;
		}
	}

	dpy = _dpy;

	if ( !_XF86DGAQueryExtension( dpy, &event_base, &error_base ) || !_XF86DGAQueryVersion( dpy, &ver_major, &ver_minor ) )
	{
		Com_Printf( "...DGA extension is not available.\n" );
		goto __fail;
	}

	Com_Printf( "...DGA extension version %i.%i detected.\n", ver_major, ver_minor );

	glw_state.dga_ext = qtrue;

	return qtrue;

__fail:
	DGA_Done();
	return qfalse;
}


void DGA_Done( void )
{
	if ( d_lib )
	{
		Sys_UnloadLibrary( d_lib );
		d_lib = NULL;
	}
	glw_state.dga_ext = qfalse;
}

void DGA_Mouse( qbool enable )
{
	if ( !glw_state.dga_ext )
		return;

	_XF86DGADirectVideo( dpy, DefaultScreen( dpy ), enable ? XF86DGADirectMouse : 0 );
}
