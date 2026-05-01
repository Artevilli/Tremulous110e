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
//
// q_math.c -- stateless support routines that are included in each code module

// Some of the vector functions are static inline in q_shared.h. q3asm
// doesn't understand static functions though, so we only want them in
// one file. That's what this is about.
#if defined(Q3_VM)
#define __Q3_VM_MATH
#endif

#include "q_shared.h"

const vec3_t	vec3_origin = {0,0,0};
vec3_t	axisDefault[3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };

vec4_t colorBlack = {0, 0, 0, 1};
vec4_t colorRed = {1, 0, 0, 1};
vec4_t colorGreen = {0, 1, 0, 1};
vec4_t colorBlue = {0, 0, 1, 1};
vec4_t colorYellow = {1, 1, 0, 1};
vec4_t colorMagenta = {1, 0, 1, 1};
vec4_t colorCyan = {0, 1, 1, 1};
vec4_t colorWhite = {1, 1, 1, 1};
vec4_t colorLtGrey = {0.75, 0.75, 0.75, 1};
vec4_t colorMdGrey = {0.5, 0.5, 0.5, 1};
vec4_t colorDkGrey = {0.25, 0.25, 0.25, 1};

//actually there are 35 colors but we want to use bitmask safely
const vec4_t g_color_table[64] =
{
  {0.0f, 0.0f, 0.0f, 1.0f},
  {1.0f, 0.0f, 0.0f, 1.0f},
  {0.0f, 1.0f, 0.0f, 1.0f},
  {1.0f, 1.0f, 0.0f, 1.0f},
  {0.2f, 0.2f, 1.0f, 1.0f}, //{0.0, 0.0, 1.0, 1.0},
  {0.0f, 1.0f, 1.0f, 1.0f},
  {1.0f, 0.0f, 1.0f, 1.0f},
  {1.0f, 1.0f, 1.0f, 1.0f},

  //extend color codes from CPMA/CNQ3:
  {1.00000f, 0.50000f, 0.00000f, 1.00000f}, //8
  {0.60000f, 0.60000f, 1.00000f, 1.00000f}, //9

  //CPMA's alphabet rainbow
  {1.00000f, 0.00000f, 0.00000f, 1.00000f}, //a
  {1.00000f, 0.26795f, 0.00000f, 1.00000f}, //b
  {1.00000f, 0.50000f, 0.00000f, 1.00000f}, //c
  {1.00000f, 0.73205f, 0.00000f, 1.00000f}, //d
  {1.00000f, 1.00000f, 0.00000f, 1.00000f}, //e
  {0.73205f, 1.00000f, 0.00000f, 1.00000f}, //f
  {0.50000f, 1.00000f, 0.00000f, 1.00000f}, //g
  {0.26795f, 1.00000f, 0.00000f, 1.00000f}, //h
  {0.00000f, 1.00000f, 0.00000f, 1.00000f}, //i
  {0.00000f, 1.00000f, 0.26795f, 1.00000f}, //j
  {0.00000f, 1.00000f, 0.50000f, 1.00000f}, //k
  {0.00000f, 1.00000f, 0.73205f, 1.00000f}, //l
  {0.00000f, 1.00000f, 1.00000f, 1.00000f}, //m
  {0.00000f, 0.73205f, 1.00000f, 1.00000f}, //n
  {0.00000f, 0.50000f, 1.00000f, 1.00000f}, //o
  {0.00000f, 0.26795f, 1.00000f, 1.00000f}, //p
  {0.00000f, 0.00000f, 1.00000f, 1.00000f}, //q
  {0.26795f, 0.00000f, 1.00000f, 1.00000f}, //r
  {0.50000f, 0.00000f, 1.00000f, 1.00000f}, //s
  {0.73205f, 0.00000f, 1.00000f, 1.00000f}, //t
  {1.00000f, 0.00000f, 1.00000f, 1.00000f}, //u
  {1.00000f, 0.00000f, 0.73205f, 1.00000f}, //v
  {1.00000f, 0.00000f, 0.50000f, 1.00000f}, //w
  {1.00000f, 0.00000f, 0.26795f, 1.00000f}, //x
  {1.0, 1.0, 1.0, 1.0}, //y, white, duped so all colors can be expressed with this palette
  {0.5, 0.5, 0.5, 1.0}, //z, grey
};

qint
ColorIndexFromChar(qchar ccode)
{
  if (ccode >= '0' && ccode <= '9')
  {
    return (ccode - '0');
  }
  else if (ccode >= 'a' && ccode <= 'z')
  {
    return (ccode - 'a' + 10);
  }
  else if (ccode >= 'A' && ccode <= 'Z')
  {
    return (ccode - 'A' + 10);
  }
  else
  {
    return ColorIndex(COLOR_WHITE);
  }
}

vec3_t	bytedirs[NUMVERTEXNORMALS] =
{
{-0.525731f, 0.000000f, 0.850651f}, {-0.442863f, 0.238856f, 0.864188f}, 
{-0.295242f, 0.000000f, 0.955423f}, {-0.309017f, 0.500000f, 0.809017f}, 
{-0.162460f, 0.262866f, 0.951056f}, {0.000000f, 0.000000f, 1.000000f}, 
{0.000000f, 0.850651f, 0.525731f}, {-0.147621f, 0.716567f, 0.681718f}, 
{0.147621f, 0.716567f, 0.681718f}, {0.000000f, 0.525731f, 0.850651f}, 
{0.309017f, 0.500000f, 0.809017f}, {0.525731f, 0.000000f, 0.850651f}, 
{0.295242f, 0.000000f, 0.955423f}, {0.442863f, 0.238856f, 0.864188f}, 
{0.162460f, 0.262866f, 0.951056f}, {-0.681718f, 0.147621f, 0.716567f}, 
{-0.809017f, 0.309017f, 0.500000f},{-0.587785f, 0.425325f, 0.688191f}, 
{-0.850651f, 0.525731f, 0.000000f},{-0.864188f, 0.442863f, 0.238856f}, 
{-0.716567f, 0.681718f, 0.147621f},{-0.688191f, 0.587785f, 0.425325f}, 
{-0.500000f, 0.809017f, 0.309017f}, {-0.238856f, 0.864188f, 0.442863f}, 
{-0.425325f, 0.688191f, 0.587785f}, {-0.716567f, 0.681718f, -0.147621f}, 
{-0.500000f, 0.809017f, -0.309017f}, {-0.525731f, 0.850651f, 0.000000f}, 
{0.000000f, 0.850651f, -0.525731f}, {-0.238856f, 0.864188f, -0.442863f}, 
{0.000000f, 0.955423f, -0.295242f}, {-0.262866f, 0.951056f, -0.162460f}, 
{0.000000f, 1.000000f, 0.000000f}, {0.000000f, 0.955423f, 0.295242f}, 
{-0.262866f, 0.951056f, 0.162460f}, {0.238856f, 0.864188f, 0.442863f}, 
{0.262866f, 0.951056f, 0.162460f}, {0.500000f, 0.809017f, 0.309017f}, 
{0.238856f, 0.864188f, -0.442863f},{0.262866f, 0.951056f, -0.162460f}, 
{0.500000f, 0.809017f, -0.309017f},{0.850651f, 0.525731f, 0.000000f}, 
{0.716567f, 0.681718f, 0.147621f}, {0.716567f, 0.681718f, -0.147621f}, 
{0.525731f, 0.850651f, 0.000000f}, {0.425325f, 0.688191f, 0.587785f}, 
{0.864188f, 0.442863f, 0.238856f}, {0.688191f, 0.587785f, 0.425325f}, 
{0.809017f, 0.309017f, 0.500000f}, {0.681718f, 0.147621f, 0.716567f}, 
{0.587785f, 0.425325f, 0.688191f}, {0.955423f, 0.295242f, 0.000000f}, 
{1.000000f, 0.000000f, 0.000000f}, {0.951056f, 0.162460f, 0.262866f}, 
{0.850651f, -0.525731f, 0.000000f},{0.955423f, -0.295242f, 0.000000f}, 
{0.864188f, -0.442863f, 0.238856f}, {0.951056f, -0.162460f, 0.262866f}, 
{0.809017f, -0.309017f, 0.500000f}, {0.681718f, -0.147621f, 0.716567f}, 
{0.850651f, 0.000000f, 0.525731f}, {0.864188f, 0.442863f, -0.238856f}, 
{0.809017f, 0.309017f, -0.500000f}, {0.951056f, 0.162460f, -0.262866f}, 
{0.525731f, 0.000000f, -0.850651f}, {0.681718f, 0.147621f, -0.716567f}, 
{0.681718f, -0.147621f, -0.716567f},{0.850651f, 0.000000f, -0.525731f}, 
{0.809017f, -0.309017f, -0.500000f}, {0.864188f, -0.442863f, -0.238856f}, 
{0.951056f, -0.162460f, -0.262866f}, {0.147621f, 0.716567f, -0.681718f}, 
{0.309017f, 0.500000f, -0.809017f}, {0.425325f, 0.688191f, -0.587785f}, 
{0.442863f, 0.238856f, -0.864188f}, {0.587785f, 0.425325f, -0.688191f}, 
{0.688191f, 0.587785f, -0.425325f}, {-0.147621f, 0.716567f, -0.681718f}, 
{-0.309017f, 0.500000f, -0.809017f}, {0.000000f, 0.525731f, -0.850651f}, 
{-0.525731f, 0.000000f, -0.850651f}, {-0.442863f, 0.238856f, -0.864188f}, 
{-0.295242f, 0.000000f, -0.955423f}, {-0.162460f, 0.262866f, -0.951056f}, 
{0.000000f, 0.000000f, -1.000000f}, {0.295242f, 0.000000f, -0.955423f}, 
{0.162460f, 0.262866f, -0.951056f}, {-0.442863f, -0.238856f, -0.864188f}, 
{-0.309017f, -0.500000f, -0.809017f}, {-0.162460f, -0.262866f, -0.951056f}, 
{0.000000f, -0.850651f, -0.525731f}, {-0.147621f, -0.716567f, -0.681718f}, 
{0.147621f, -0.716567f, -0.681718f}, {0.000000f, -0.525731f, -0.850651f}, 
{0.309017f, -0.500000f, -0.809017f}, {0.442863f, -0.238856f, -0.864188f}, 
{0.162460f, -0.262866f, -0.951056f}, {0.238856f, -0.864188f, -0.442863f}, 
{0.500000f, -0.809017f, -0.309017f}, {0.425325f, -0.688191f, -0.587785f}, 
{0.716567f, -0.681718f, -0.147621f}, {0.688191f, -0.587785f, -0.425325f}, 
{0.587785f, -0.425325f, -0.688191f}, {0.000000f, -0.955423f, -0.295242f}, 
{0.000000f, -1.000000f, 0.000000f}, {0.262866f, -0.951056f, -0.162460f}, 
{0.000000f, -0.850651f, 0.525731f}, {0.000000f, -0.955423f, 0.295242f}, 
{0.238856f, -0.864188f, 0.442863f}, {0.262866f, -0.951056f, 0.162460f}, 
{0.500000f, -0.809017f, 0.309017f}, {0.716567f, -0.681718f, 0.147621f}, 
{0.525731f, -0.850651f, 0.000000f}, {-0.238856f, -0.864188f, -0.442863f}, 
{-0.500000f, -0.809017f, -0.309017f}, {-0.262866f, -0.951056f, -0.162460f}, 
{-0.850651f, -0.525731f, 0.000000f}, {-0.716567f, -0.681718f, -0.147621f}, 
{-0.716567f, -0.681718f, 0.147621f}, {-0.525731f, -0.850651f, 0.000000f}, 
{-0.500000f, -0.809017f, 0.309017f}, {-0.238856f, -0.864188f, 0.442863f}, 
{-0.262866f, -0.951056f, 0.162460f}, {-0.864188f, -0.442863f, 0.238856f}, 
{-0.809017f, -0.309017f, 0.500000f}, {-0.688191f, -0.587785f, 0.425325f}, 
{-0.681718f, -0.147621f, 0.716567f}, {-0.442863f, -0.238856f, 0.864188f}, 
{-0.587785f, -0.425325f, 0.688191f}, {-0.309017f, -0.500000f, 0.809017f}, 
{-0.147621f, -0.716567f, 0.681718f}, {-0.425325f, -0.688191f, 0.587785f}, 
{-0.162460f, -0.262866f, 0.951056f}, {0.442863f, -0.238856f, 0.864188f}, 
{0.162460f, -0.262866f, 0.951056f}, {0.309017f, -0.500000f, 0.809017f}, 
{0.147621f, -0.716567f, 0.681718f}, {0.000000f, -0.525731f, 0.850651f}, 
{0.425325f, -0.688191f, 0.587785f}, {0.587785f, -0.425325f, 0.688191f}, 
{0.688191f, -0.587785f, 0.425325f}, {-0.955423f, 0.295242f, 0.000000f}, 
{-0.951056f, 0.162460f, 0.262866f}, {-1.000000f, 0.000000f, 0.000000f}, 
{-0.850651f, 0.000000f, 0.525731f}, {-0.955423f, -0.295242f, 0.000000f}, 
{-0.951056f, -0.162460f, 0.262866f}, {-0.864188f, 0.442863f, -0.238856f}, 
{-0.951056f, 0.162460f, -0.262866f}, {-0.809017f, 0.309017f, -0.500000f}, 
{-0.864188f, -0.442863f, -0.238856f}, {-0.951056f, -0.162460f, -0.262866f}, 
{-0.809017f, -0.309017f, -0.500000f}, {-0.681718f, 0.147621f, -0.716567f}, 
{-0.681718f, -0.147621f, -0.716567f}, {-0.850651f, 0.000000f, -0.525731f}, 
{-0.688191f, 0.587785f, -0.425325f}, {-0.587785f, 0.425325f, -0.688191f}, 
{-0.425325f, 0.688191f, -0.587785f}, {-0.425325f, -0.688191f, -0.587785f}, 
{-0.587785f, -0.425325f, -0.688191f}, {-0.688191f, -0.587785f, -0.425325f}
};

//==============================================================

qint		Q_rand( qint *seed ) {
	*seed = (69069 * *seed + 1);
	return *seed;
}

float	Q_random( qint *seed ) {
	return ( Q_rand( seed ) & 0xffff ) / (float)0x10000;
}

float	Q_crandom( qint *seed ) {
	return 2.0 * ( Q_random( seed ) - 0.5 );
}

//=======================================================

signed qchar ClampChar( qint i ) {
	if ( i < -128 ) {
		return -128;
	}
	if ( i > 127 ) {
		return 127;
	}
	return i;
}

signed qchar
ClampCharMove(qint i)
{
  if (i < -127)
  {
    return -127;
  }

  if (i > 127)
  {
    return 127;
  }

  return i;
}

signed short ClampShort( qint i ) {
	if ( i < -32768 ) {
		return -32768;
	}
	if ( i > 0x7fff ) {
		return 0x7fff;
	}
	return i;
}


// this isn't a real cheap function to call!
qint DirToByte( vec3_t dir ) {
	qint		i, best;
	float	d, bestd;

	if ( !dir ) {
		return 0;
	}

	bestd = 0;
	best = 0;
	for (i=0 ; i<NUMVERTEXNORMALS ; i++)
	{
		d = DotProduct (dir, bytedirs[i]);
		if (d > bestd)
		{
			bestd = d;
			best = i;
		}
	}

	return best;
}

void ByteToDir( qint b, vec3_t dir ) {
	if ( b < 0 || b >= NUMVERTEXNORMALS ) {
		VectorCopy( vec3_origin, dir );
		return;
	}
	VectorCopy (bytedirs[b], dir);
}

#if 0
unsigned ColorBytes3 (float r, float g, float b) {
	unsigned	i;

	( (byte *)&i )[0] = r * 255;
	( (byte *)&i )[1] = g * 255;
	( (byte *)&i )[2] = b * 255;

	return i;
}

unsigned ColorBytes4 (float r, float g, float b, float a) {
	unsigned	i;

	( (byte *)&i )[0] = r * 255;
	( (byte *)&i )[1] = g * 255;
	( (byte *)&i )[2] = b * 255;
	( (byte *)&i )[3] = a * 255;

	return i;
}
#endif

float NormalizeColor( const vec3_t in, vec3_t out ) {
	float	max;
	
	max = in[0];
	if ( in[1] > max ) {
		max = in[1];
	}
	if ( in[2] > max ) {
		max = in[2];
	}

	if ( !max ) {
		VectorClear( out );
	} else {
		out[0] = in[0] / max;
		out[1] = in[1] / max;
		out[2] = in[2] / max;
	}
	return max;
}

/*
=====================
PlaneFromPoints

Returns false if the triangle is degenerate.
The normal will point out of the clock for clockwise ordered points
=====================
*/
qbool PlaneFromPoints( vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c ) {
	vec3_t	d1, d2;

	VectorSubtract( b, a, d1 );
	VectorSubtract( c, a, d2 );
	CrossProduct( d2, d1, plane );
	if ( VectorNormalize( plane ) == 0 ) {
		return qfalse;
	}

	plane[3] = DotProduct( a, plane );
	return qtrue;
}

/*
==================
SetupRotationMatrix

Setup rotation matrix given the normalized direction vector and angle to rotate
around this vector. Adapted from Mesa 3D.
==================
*/
void
SetupRotationMatrix(vec3_t matrix[3], const vec3_t dir, float degrees)
{
  vec_t angle;
  vec_t s;
  vec_t c;
  vec_t one_c;
  vec_t xx;
  vec_t yy;
  vec_t zz;
  vec_t xy;
  vec_t yz;
  vec_t zx;
  vec_t xs;
  vec_t ys;
  vec_t zs;

  angle = DEG2RAD(degrees);
  s = sin(angle);
  c = cos(angle);
  one_c = 1.0F - c;

  xx = dir[0] * dir[0];
  yy = dir[1] * dir[1];
  zz = dir[2] * dir[2];
  xy = dir[0] * dir[1];
  yz = dir[1] * dir[2];
  zx = dir[2] * dir[0];
  xs = dir[0] * s;
  ys = dir[1] * s;
  zs = dir[2] * s;

  matrix[0][0] = (one_c * xx) + c;
  matrix[0][1] = (one_c * xy) - zs;
  matrix[0][2] = (one_c * zx) + ys;

  matrix[1][0] = (one_c * xy) + zs;
  matrix[1][1] = (one_c * yy) + c;
  matrix[1][2] = (one_c * yz) - xs;

  matrix[2][0] = (one_c * zx) - ys;
  matrix[2][1] = (one_c * yz) + xs;
  matrix[2][2] = (one_c * zz) + c;
}

/*
===============
RotatePointAroundVector
===============
*/
void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point,
							 float degrees ) {
  vec3_t matrix[3];

  SetupRotationMatrix(matrix, dir, degrees);
  VectorRotate(point, matrix, dst);
}

/*
===============
RotateAroundDirection
===============
*/
void RotateAroundDirection( vec3_t axis[3], vec_t angle ) {
	vec_t scale;

	angle = DEG2RAD( angle );

	// create an arbitrary axis[1]
	PerpendicularVector( axis[ 1 ], axis[ 0 ] );

	// cross to get axis[2]
	CrossProduct( axis[ 0 ], axis[ 1 ], axis[ 2 ] );

	// rotate
	scale = cos( angle );
	VectorScale( axis[ 1 ], scale, axis[ 1 ] );

	scale = sin( angle );
	VectorMA( axis[ 1 ], scale, axis[ 2 ], axis[ 1 ] );

	// recalculate axis[2]
	CrossProduct( axis[ 0 ], axis[ 1 ], axis[ 2 ] );
}



void vectoangles( const vec3_t value1, vec3_t angles ) {
	float	forward;
	float	yaw, pitch;
	
	if ( value1[1] == 0 && value1[0] == 0 ) {
		yaw = 0;
		if ( value1[2] > 0 ) {
			pitch = 90;
		}
		else {
			pitch = 270;
		}
	}
	else {
		if ( value1[0] ) {
			yaw = ( atan2 ( value1[1], value1[0] ) * 180 / M_PI );
		}
		else if ( value1[1] > 0 ) {
			yaw = 90;
		}
		else {
			yaw = 270;
		}
		if ( yaw < 0 ) {
			yaw += 360;
		}

		forward = sqrt ( value1[0]*value1[0] + value1[1]*value1[1] );
		pitch = ( atan2(value1[2], forward) * 180 / M_PI );
		if ( pitch < 0 ) {
			pitch += 360;
		}
	}

	angles[PITCH] = -pitch;
	angles[YAW] = yaw;
	angles[ROLL] = 0;
}


/*
=================
AxisToAngles

Takes an axis (forward + right + up)
and returns angles -- including a roll
=================
*/
void AxisToAngles( vec3_t axis[3], vec3_t angles ) {
	float length1;
	float yaw, pitch, roll = 0.0f;

	if ( axis[0][1] == 0 && axis[0][0] == 0 ) {
		yaw = 0;
		if ( axis[0][2] > 0 ) {
			pitch = 90;
		}
		else {
			pitch = 270;
		}
	}
	else {
		if ( axis[0][0] ) {
			yaw = ( atan2 ( axis[0][1], axis[0][0] ) * 180 / M_PI );
		}
		else if ( axis[0][1] > 0 ) {
			yaw = 90;
		}
		else {
			yaw = 270;
		}
		if ( yaw < 0 ) {
			yaw += 360;
		}

		length1 = sqrt ( axis[0][0]*axis[0][0] + axis[0][1]*axis[0][1] );
		pitch = ( atan2(axis[0][2], length1) * 180 / M_PI );
		if ( pitch < 0 ) {
			pitch += 360;
		}

		roll = ( atan2( axis[1][2], axis[2][2] ) * 180 / M_PI );
		if ( roll < 0 ) {
			roll += 360;
		}
	}

	angles[PITCH] = -pitch;
	angles[YAW] = yaw;
	angles[ROLL] = roll;
}

/*
=================
AnglesToAxis
=================
*/
void AnglesToAxis( const vec3_t angles, vec3_t axis[3] ) {
	vec3_t	right;

	// angle vectors returns "right" instead of "y axis"
	AngleVectors( angles, axis[0], right, axis[2] );
	VectorSubtract( vec3_origin, right, axis[1] );
}

void AxisClear( vec3_t axis[3] ) {
	axis[0][0] = 1;
	axis[0][1] = 0;
	axis[0][2] = 0;
	axis[1][0] = 0;
	axis[1][1] = 1;
	axis[1][2] = 0;
	axis[2][0] = 0;
	axis[2][1] = 0;
	axis[2][2] = 1;
}

void AxisCopy( vec3_t in[3], vec3_t out[3] ) {
	VectorCopy( in[0], out[0] );
	VectorCopy( in[1], out[1] );
	VectorCopy( in[2], out[2] );
}

void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal )
{
	float d;
	vec3_t n;
	float inv_denom;

	inv_denom = 1.0f / DotProduct( normal, normal );
#if !defined(Q3_VM)
	assert( Q_fabs(inv_denom) != 0.0f ); // zero vectors get here
#endif
	inv_denom = 1.0f / inv_denom;

	d = DotProduct( normal, p ) * inv_denom;

	n[0] = normal[0] * inv_denom;
	n[1] = normal[1] * inv_denom;
	n[2] = normal[2] * inv_denom;

	dst[0] = p[0] - d * n[0];
	dst[1] = p[1] - d * n[1];
	dst[2] = p[2] - d * n[2];
}

/*
================
MakeNormalVectors

Given a normalized forward vector, create two
other perpendicular vectors
================
*/
void MakeNormalVectors( const vec3_t forward, vec3_t right, vec3_t up) {
	float		d;

	// this rotate and negate guarantees a vector
	// not colinear with the original
	right[1] = -forward[0];
	right[2] = forward[1];
	right[0] = forward[2];

	d = DotProduct (right, forward);
	VectorMA (right, -d, forward, right);
	VectorNormalize (right);
	CrossProduct (right, forward, up);
}


void VectorRotate( const vec3_t in, const vec3_t matrix[3], vec3_t out )
{
	out[0] = DotProduct( in, matrix[0] );
	out[1] = DotProduct( in, matrix[1] );
	out[2] = DotProduct( in, matrix[2] );
}

//============================================================================

#if defined(_MSC_SSE2)
#include <intrin.h>
#endif

#if !idppc
/*
** float Q_rsqrt( float number )
*/
float Q_rsqrt( float number )
{
#if defined(_MSC_SSE2)
  float ret;

  _mm_store_ss(&ret, _mm_rsqrt_ss(_mm_load_ss(&number)));
  return ret;
#elif defined(_GCC_SSE2)
  /*writing it this way allows gcc to recognize that rsqrt can be used with -ffast-math*/
  return 1.0f / sqrtf(number);
#elif defined(_GCC_VSX)
	/* VSX scalar reciprocal sqrt estimate (POWER7+, ~14-bit precision)
	 * + one Newton-Raphson iteration → matches SSE rsqrtss + NR accuracy.
	 * Scalar form avoids the splat/extract overhead of the vector path. */
	float y;
	__asm__( "xsrsqrtesp %x0,%x1" : "=wa"(y) : "wa"(number) );
	y = y * ( 1.5f - 0.5f * number * y * y );
	return y;
#else
	floatint_t t;
	float x2, y;
	const float threehalfs = 1.5F;

	x2 = number * 0.5F;
	t.f  = number;
	t.i  = 0x5f3759df - ( t.i >> 1 );               // what the fuck?
	y  = t.f;
	y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
//	y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed

	return y;
#endif
}

float
Q_fabs(float f)
{
  floatint_t fi;

  fi.f = f;
  fi.i &= 0x7FFFFFFF;
  return fi.f;
}
#endif

//============================================================

/*
===============
LerpAngle

===============
*/
float LerpAngle (float from, float to, float frac) {
	float	a;

	if ( to - from > 180 ) {
		to -= 360;
	}
	if ( to - from < -180 ) {
		to += 360;
	}
	a = from + frac * (to - from);

	return a;
}


/*
=================
AngleSubtract

Always returns a value from -180 to 180
=================
*/
float	AngleSubtract( float a1, float a2 ) {
	float	a;

	a = a1 - a2;
	while ( a > 180 ) {
		a -= 360;
	}
	while ( a < -180 ) {
		a += 360;
	}
	return a;
}


void AnglesSubtract( vec3_t v1, vec3_t v2, vec3_t v3 ) {
	v3[0] = AngleSubtract( v1[0], v2[0] );
	v3[1] = AngleSubtract( v1[1], v2[1] );
	v3[2] = AngleSubtract( v1[2], v2[2] );
}


float	AngleMod(float a) {
	a = (360.0/65536) * ((qint)(a*(65536/360.0)) & 65535);
	return a;
}


/*
=================
AngleNormalize360

returns angle normalized to the range [0 <= angle < 360]
=================
*/
float AngleNormalize360 ( float angle ) {
	return (360.0 / 65536) * ((qint)(angle * (65536 / 360.0)) & 65535);
}


/*
=================
AngleNormalize180

returns angle normalized to the range [-180 < angle <= 180]
=================
*/
float AngleNormalize180 ( float angle ) {
	angle = AngleNormalize360( angle );
	if ( angle > 180.0 ) {
		angle -= 360.0;
	}
	return angle;
}


/*
=================
AngleDelta

returns the normalized delta from angle1 to angle2
=================
*/
float AngleDelta ( float angle1, float angle2 ) {
	return AngleNormalize180( angle1 - angle2 );
}


//============================================================


/*
=================
SetPlaneSignbits
=================
*/
void SetPlaneSignbits (cplane_t *out) {
	qint	bits, j;

	// for fast box on planeside test
	bits = 0;
	for (j=0 ; j<3 ; j++) {
		if (out->normal[j] < 0) {
			bits |= 1<<j;
		}
	}
	out->signbits = bits;
}


/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2

// this is the slow, general version
qint BoxOnPlaneSide2 (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	qint		i;
	float	dist1, dist2;
	qint		sides;
	vec3_t	corners[2];

	for (i=0 ; i<3 ; i++)
	{
		if (p->normal[i] < 0)
		{
			corners[0][i] = emins[i];
			corners[1][i] = emaxs[i];
		}
		else
		{
			corners[1][i] = emins[i];
			corners[0][i] = emaxs[i];
		}
	}
	dist1 = DotProduct (p->normal, corners[0]) - p->dist;
	dist2 = DotProduct (p->normal, corners[1]) - p->dist;
	sides = 0;
	if (dist1 >= 0)
		sides = 1;
	if (dist2 < 0)
		sides |= 2;

	return sides;
}

==================
*/

qint
BoxOnPlaneSide(vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
  float dist[2];
  qint sides;
  qint b;
  qint i;

  //fast axial cases
  if (p->type < 3)
  {
    if (p->dist <= emins[p->type])
    {
      return 1;
    }

    if (p->dist >= emaxs[p->type])
    {
      return 2;
    }

    return 3;
  }

  //general case
  dist[0] = dist[1] = 0;

  if (p->signbits < 8) //>= 8: default case is original code (dist[0] = dist[1] = 0)
  {
    for(i = 0;i < 3;i++)
    {
      b = (p->signbits >> i) & 1;
      dist[b] += p->normal[i] * emaxs[i];
      dist[!b] += p->normal[i] * emins[i];
    }
  }

  sides = 0;

  if (dist[0] >= p->dist)
  {
    sides = 1;
  }

  if (dist[1] < p->dist)
  {
    sides |= 2;
  }

  return sides;
}

/*
=================
RadiusFromBounds
=================
*/
float RadiusFromBounds( const vec3_t mins, const vec3_t maxs ) {
	qint		i;
	vec3_t	corner;
	float	a, b;

	for (i=0 ; i<3 ; i++) {
		a = fabs( mins[i] );
		b = fabs( maxs[i] );
		corner[i] = a > b ? a : b;
	}

	return VectorLength (corner);
}


void ClearBounds( vec3_t mins, vec3_t maxs ) {
	mins[0] = mins[1] = mins[2] = 99999;
	maxs[0] = maxs[1] = maxs[2] = -99999;
}

void AddPointToBounds( const vec3_t v, vec3_t mins, vec3_t maxs ) {
	if ( v[0] < mins[0] ) {
		mins[0] = v[0];
	}
	if ( v[0] > maxs[0]) {
		maxs[0] = v[0];
	}

	if ( v[1] < mins[1] ) {
		mins[1] = v[1];
	}
	if ( v[1] > maxs[1]) {
		maxs[1] = v[1];
	}

	if ( v[2] < mins[2] ) {
		mins[2] = v[2];
	}
	if ( v[2] > maxs[2]) {
		maxs[2] = v[2];
	}
}

qbool BoundsIntersect(const vec3_t mins, const vec3_t maxs,
		const vec3_t mins2, const vec3_t maxs2)
{
	if ( maxs[0] < mins2[0] ||
		maxs[1] < mins2[1] ||
		maxs[2] < mins2[2] ||
		mins[0] > maxs2[0] ||
		mins[1] > maxs2[1] ||
		mins[2] > maxs2[2])
	{
		return qfalse;
	}

	return qtrue;
}

qbool BoundsIntersectSphere(const vec3_t mins, const vec3_t maxs,
		const vec3_t origin, vec_t radius)
{
	if ( origin[0] - radius > maxs[0] ||
		origin[0] + radius < mins[0] ||
		origin[1] - radius > maxs[1] ||
		origin[1] + radius < mins[1] ||
		origin[2] - radius > maxs[2] ||
		origin[2] + radius < mins[2])
	{
		return qfalse;
	}

	return qtrue;
}

qbool BoundsIntersectPoint(const vec3_t mins, const vec3_t maxs,
		const vec3_t origin)
{
	if ( origin[0] > maxs[0] ||
		origin[0] < mins[0] ||
		origin[1] > maxs[1] ||
		origin[1] < mins[1] ||
		origin[2] > maxs[2] ||
		origin[2] < mins[2])
	{
		return qfalse;
	}

	return qtrue;
}

vec_t
VectorNormalize(vec3_t v)
{
  float	length;
  float ilength;

  length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];

  if (length)
  {
    /*writing it this way allows gcc to recognize that rsqrt can be used*/
    ilength = 1 / (float)sqrt(length);
    /*sqrt(length) = length * (1 / sqrt(length))*/
    length *= ilength;
    v[0] *= ilength;
    v[1] *= ilength;
    v[2] *= ilength;
  }
		
  return length;
}

vec_t
VectorNormalize2(const vec3_t v, vec3_t out)
{
  float length;
  float ilength;

  length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];

  if (length)
  {
    /*writing it this way allows gcc to recognize that rsqrt can be used*/
    ilength = 1 / (float)sqrt(length);
    /*sqrt(length) = length * (1 / sqrt(length))*/
    length *= ilength;
    out[0] = v[0]*ilength;
    out[1] = v[1]*ilength;
    out[2] = v[2]*ilength;
  }
  else
  {
    VectorClear(out);
  }
		
  return length;
}

void _VectorMA( const vec3_t veca, float scale, const vec3_t vecb, vec3_t vecc) {
	vecc[0] = veca[0] + scale*vecb[0];
	vecc[1] = veca[1] + scale*vecb[1];
	vecc[2] = veca[2] + scale*vecb[2];
}


vec_t _DotProduct( const vec3_t v1, const vec3_t v2 ) {
	return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

void _VectorSubtract( const vec3_t veca, const vec3_t vecb, vec3_t out ) {
	out[0] = veca[0]-vecb[0];
	out[1] = veca[1]-vecb[1];
	out[2] = veca[2]-vecb[2];
}

void _VectorAdd( const vec3_t veca, const vec3_t vecb, vec3_t out ) {
	out[0] = veca[0]+vecb[0];
	out[1] = veca[1]+vecb[1];
	out[2] = veca[2]+vecb[2];
}

void _VectorCopy( const vec3_t in, vec3_t out ) {
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

void _VectorScale( const vec3_t in, vec_t scale, vec3_t out ) {
	out[0] = in[0]*scale;
	out[1] = in[1]*scale;
	out[2] = in[2]*scale;
}

void Vector4Scale( const vec4_t in, vec_t scale, vec4_t out ) {
	out[0] = in[0]*scale;
	out[1] = in[1]*scale;
	out[2] = in[2]*scale;
	out[3] = in[3]*scale;
}


qint Q_log2( qint val ) {
	qint answer;

	answer = 0;
	while ( ( val>>=1 ) != 0 ) {
		answer++;
	}
	return answer;
}



/*
=================
PlaneTypeForNormal
=================
*/
/*
qint	PlaneTypeForNormal (vec3_t normal) {
	if ( normal[0] == 1.0 )
		return PLANE_X;
	if ( normal[1] == 1.0 )
		return PLANE_Y;
	if ( normal[2] == 1.0 )
		return PLANE_Z;
	
	return PLANE_NON_AXIAL;
}
*/


/*
================
MatrixMultiply
================
*/
void MatrixMultiply(float in1[3][3], float in2[3][3], float out[3][3]) {
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
				in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
				in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
				in1[0][2] * in2[2][2];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
				in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
				in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
				in1[1][2] * in2[2][2];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
				in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
				in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
				in1[2][2] * in2[2][2];
}

/*
================
VectorMatrixMultiply
================
*/
void VectorMatrixMultiply( const vec3_t p, vec3_t m[ 3 ], vec3_t out )
{
	out[ 0 ] = m[ 0 ][ 0 ] * p[ 0 ] + m[ 1 ][ 0 ] * p[ 1 ] + m[ 2 ][ 0 ] * p[ 2 ];
	out[ 1 ] = m[ 0 ][ 1 ] * p[ 0 ] + m[ 1 ][ 1 ] * p[ 1 ] + m[ 2 ][ 1 ] * p[ 2 ];
	out[ 2 ] = m[ 0 ][ 2 ] * p[ 0 ] + m[ 1 ][ 2 ] * p[ 1 ] + m[ 2 ][ 2 ] * p[ 2 ];
}


void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up) {
	float		angle;
	static float		sr, sp, sy, cr, cp, cy;
	// static to help MS compiler fp bugs

	angle = angles[YAW] * (float)(M_PI*2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[PITCH] * (float)(M_PI*2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[ROLL] * (float)(M_PI*2 / 360);
	sr = sin(angle);
	cr = cos(angle);

	if (forward)
	{
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;
	}
	if (right)
	{
		right[0] = (-1*sr*sp*cy+-1*cr*-sy);
		right[1] = (-1*sr*sp*sy+-1*cr*cy);
		right[2] = -1*sr*cp;
	}
	if (up)
	{
		up[0] = (cr*sp*cy+-sr*-sy);
		up[1] = (cr*sp*sy+-sr*cy);
		up[2] = cr*cp;
	}
}

/*
** assumes "src" is normalized
*/
void PerpendicularVector( vec3_t dst, const vec3_t src )
{
	qint	pos;
	qint i;
	float minelem = 1.0F;
	vec3_t tempvec;

	/*
	** find the smallest magnitude axially aligned vector
	*/
	for ( pos = 0, i = 0; i < 3; i++ )
	{
		if ( fabs( src[i] ) < minelem )
		{
			pos = i;
			minelem = fabs( src[i] );
		}
	}
	tempvec[0] = tempvec[1] = tempvec[2] = 0.0F;
	tempvec[pos] = 1.0F;

	/*
	** project the point onto the plane defined by src
	*/
	ProjectPointOnPlane( dst, tempvec, src );

	/*
	** normalize the result
	*/
	VectorNormalize( dst );
}

/*
=================
pointToLineDistance

Distance from a point to some line
=================
*/
float pointToLineDistance( const vec3_t p0, const vec3_t p1, const vec3_t p2 )
{
	vec3_t	v, w, y;
	float	 c1, c2;

	VectorSubtract( p2, p1, v );
	VectorSubtract( p1, p0, w );

	CrossProduct( w, v, y );
	c1 = VectorLength( y );
	c2 = VectorLength( v );

	if( c2 == 0.0f )
		return 0.0f;
	else
		return c1 / c2;
}

/*
=================
GetPerpendicularViewVector

Used to find an "up" vector for drawing a sprite so that it always faces the view as best as possible
=================
*/
void GetPerpendicularViewVector( const vec3_t point, const vec3_t p1, const vec3_t p2, vec3_t up )
{
	vec3_t	v1, v2;

	VectorSubtract( point, p1, v1 );
	VectorNormalize( v1 );

	VectorSubtract( point, p2, v2 );
	VectorNormalize( v2 );

	CrossProduct( v1, v2, up );
	VectorNormalize( up );
}

/*
================
ProjectPointOntoVector
================
*/
void ProjectPointOntoVector( vec3_t point, vec3_t vStart, vec3_t vEnd, vec3_t vProj )
{
	vec3_t pVec, vec;

	VectorSubtract( point, vStart, pVec );
	VectorSubtract( vEnd, vStart, vec );
	VectorNormalize( vec );
	// project onto the directional vector for this segment
	VectorMA( vStart, DotProduct( pVec, vec ), vec, vProj );
}

/*
================
VectorMaxComponent

Return the biggest component of some vector
================
*/
float VectorMaxComponent( vec3_t v )
{
	float biggest = v[ 0 ];

	if( v[ 1 ] > biggest )
		biggest = v[ 1 ];

	if( v[ 2 ] > biggest )
		biggest = v[ 2 ];

	return biggest;
}

/*
================
VectorMinComponent

Return the smallest component of some vector
================
*/
float VectorMinComponent( vec3_t v )
{
	float smallest = v[ 0 ];

	if( v[ 1 ] < smallest )
		smallest = v[ 1 ];

	if( v[ 2 ] < smallest )
		smallest = v[ 2 ];

	return smallest;
}


#define LINE_DISTANCE_EPSILON 1e-05f

/*
================
DistanceBetweenLineSegmentsSquared

Return the smallest distance between two line segments, squared
================
*/
vec_t DistanceBetweenLineSegmentsSquared(
    const vec3_t sP0, const vec3_t sP1,
    const vec3_t tP0, const vec3_t tP1,
    float *s, float *t )
{
  vec3_t  sMag, tMag, diff;
  float   a, b, c, d, e;
  float   D;
  float   sN, sD;
  float   tN, tD;
  vec3_t  separation;

  VectorSubtract( sP1, sP0, sMag );
  VectorSubtract( tP1, tP0, tMag );
  VectorSubtract( sP0, tP0, diff );
  a = DotProduct( sMag, sMag );
  b = DotProduct( sMag, tMag );
  c = DotProduct( tMag, tMag );
  d = DotProduct( sMag, diff );
  e = DotProduct( tMag, diff );
  sD = tD = D = a * c - b * b;

  if( D < LINE_DISTANCE_EPSILON )
  {
    // the lines are almost parallel
    sN = 0.0;   // force using point P0 on segment S1
    sD = 1.0;   // to prevent possible division by 0.0 later
    tN = e;
    tD = c;
  }
  else
  {
    // get the closest points on the infinite lines
    sN = ( b * e - c * d );
    tN = ( a * e - b * d );

    if( sN < 0.0 )
    {
      // sN < 0 => the s=0 edge is visible
      sN = 0.0;
      tN = e;
      tD = c;
    }
    else if( sN > sD )
    {
      // sN > sD => the s=1 edge is visible
      sN = sD;
      tN = e + b;
      tD = c;
    }
  }

  if( tN < 0.0 )
  {
    // tN < 0 => the t=0 edge is visible
    tN = 0.0;

    // recompute sN for this edge
    if( -d < 0.0 )
      sN = 0.0;
    else if( -d > a )
      sN = sD;
    else
    {
      sN = -d;
      sD = a;
    }
  }
  else if( tN > tD )
  {
    // tN > tD => the t=1 edge is visible
    tN = tD;

    // recompute sN for this edge
    if( ( -d + b ) < 0.0 )
      sN = 0;
    else if( ( -d + b ) > a )
      sN = sD;
    else
    {
      sN = ( -d + b );
      sD = a;
    }
  }

  // finally do the division to get *s and *t
  *s = ( fabs( sN ) < LINE_DISTANCE_EPSILON ? 0.0 : sN / sD );
  *t = ( fabs( tN ) < LINE_DISTANCE_EPSILON ? 0.0 : tN / tD );

  // get the difference of the two closest points
  VectorScale( sMag, *s, sMag );
  VectorScale( tMag, *t, tMag );
  VectorAdd( diff, sMag, separation );
  VectorSubtract( separation, tMag, separation );

  return VectorLengthSquared( separation );
}

/*
================
DistanceBetweenLineSegments

Return the smallest distance between two line segments
================
*/
vec_t DistanceBetweenLineSegments(
    const vec3_t sP0, const vec3_t sP1,
    const vec3_t tP0, const vec3_t tP1,
    float *s, float *t )
{
  return (vec_t)sqrt( DistanceBetweenLineSegmentsSquared(
        sP0, sP1, tP0, tP1, s, t ) );
}

/*
=================
Q_isnan

Don't pass doubles to this
================
*/
qint
Q_isnan(float x)
{
  floatint_t fi;

  fi.f = x;
  fi.u &= 0x7FFFFFFF;
  fi.u = 0x7F800000 - fi.u;

  return (qint)(fi.u >> 31);
}

//------------------------------------------------------------------------

/*
================
Q_isfinite
================
*/
static qbool
Q_isfinite(float f)
{
  floatint_t fi;

  fi.f = f;

  if (fi.u == 0xFF800000 || fi.u == 0x7F800000)
  {
    return qfalse; //-INF or +INF
  }

  fi.u = 0x7F800000 - (fi.u & 0x7FFFFFFF);

  if ((qint)(fi.u >> 31))
  {
    return qfalse; //-NAN or +NAN
  }

  return qtrue;
}

/*
================
Q_atof
================
*/
float
Q_atof(const qchar *str)
{
  float f;

  f = Q_atofdef(str);

  //modern C11-like implementations of atof() may return INF or NAN
  //which breaks all FP code where such values getting passed
  //and effectively corrupts range checks for cvars as well
  if (!Q_isfinite(f))
  {
    return 0.0f;
  }

  return f;
}

/*
================
Q_log2f
================
*/
float
Q_log2f(float f)
{
  const float v = logf(f);

  return v / M_LN2;
}

/*
================
Q_exp2f
================
*/
float
Q_exp2f(float f)
{
  return powf(2.0f, f);
}

#if !defined(Q3_VM)
/*
=====================
Q_acos

the msvc acos doesn't always return a value between -PI and PI:

qint i;
i = 1065353246;
acos(*(float *)&i) == -1.#IND0
=====================
*/
float
Q_acos(float c)
{
  float angle;

  angle = acos(c);

  if (angle > M_PI)
  {
    return (float)M_PI;
  }

  if (angle < -M_PI)
  {
    return (float)M_PI;
  }

  return angle;
}
#endif
