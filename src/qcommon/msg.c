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
#include "q_shared.h"
#include "qcommon.h"
#include "../game/g_local.h"

qint pcount[256];

/*
==============================================================================

			MESSAGE IO FUNCTIONS

Handles byte ordering and avoids alignment errors
==============================================================================
*/

void MSG_Init( msg_t *buf, byte *data, qint length ) {
	Com_Memset (buf, 0, sizeof(*buf));
	buf->data = data;
	buf->maxsize = length;
	buf->maxbits = length * 8;
}

void MSG_InitOOB( msg_t *buf, byte *data, qint length ) {
	Com_Memset (buf, 0, sizeof(*buf));
	buf->data = data;
	buf->maxsize = length;
	buf->maxbits = length * 8;
	buf->oob = qtrue;
}

void MSG_Clear( msg_t *buf ) {
	buf->cursize = 0;
	buf->overflowed = qfalse;
	buf->bit = 0;					//<- in bits
}


void MSG_Bitstream( msg_t *buf ) {
	buf->oob = qfalse;
}

void MSG_BeginReading( msg_t *msg ) {
	msg->readcount = 0;
	msg->bit = 0;
	msg->oob = qfalse;
}

void MSG_BeginReadingOOB( msg_t *msg ) {
	msg->readcount = 0;
	msg->bit = 0;
	msg->oob = qtrue;
}

void MSG_Copy(msg_t *buf, byte *data, qint length, msg_t *src)
{
	if (length<src->cursize) {
		Com_Error( ERR_DROP, "MSG_Copy: can't copy into a smaller msg_t buffer");
	}
	Com_Memcpy(buf, src, sizeof(msg_t));
	buf->data = data;
	Com_Memcpy(buf->data, src->data, src->cursize);
}

/*
=============================================================================

bit functions
  
=============================================================================
*/

// negative bit values include signs
void
MSG_WriteBits(msg_t *msg, qint value, qint bits)
{
  qint i;

  if (bits == 0 || bits < -31 || bits > 32)
  {
    Com_Error(ERR_DROP, "MSG_WriteBits: bad bits %i", bits);
  }

  if (msg->overflowed != qfalse)
  {
    return;
  }

  if (bits < 0)
  {
    bits = -bits;
  }

  if (msg->oob)
  {
    if (bits == 8)
    {
      msg->data[msg->cursize] = value;
      msg->cursize += 1;
      msg->bit += 8;
    }
    else if (bits == 16)
    {
      short temp = value;

      CopyLittleShort(&msg->data[msg->cursize], &temp);
      msg->cursize += 2;
      msg->bit += 16;
    }
    else if (bits == 32)
    {
      CopyLittleLong(&msg->data[msg->cursize], &value);
      msg->cursize += 4;
      msg->bit += 32;
    }
    else
    {
      Com_Error(ERR_DROP, "can't write %d bits", bits);
    }
  }
  else
  {
    value &= (0xffffffff >> (32 - bits));

    if (bits & 7)
    {
      qint nbits;

      nbits = bits & 7;

      for(i = 0;i < nbits;i++)
      {
        HuffmanPutBit(msg->data, msg->bit, (value & 1));
        msg->bit++;
        value = (value >> 1);
      }

      bits = bits - nbits;
    }

    if (bits)
    {
      for(i = 0;i < bits;i += 8)
      {
        msg->bit += HuffmanPutSymbol(msg->data, msg->bit, (value & 0xFF));
        value = (value >> 8);
      }
    }

    msg->cursize = (msg->bit >> 3) + 1;
  }

  if (msg->bit > msg->maxbits)
  {
    msg->overflowed = qtrue;
  }
}

qint
MSG_ReadBits(msg_t *msg, qint bits)
{
  qint value;
  qbool sgn;
  qint i;
  unsigned sym;
  const byte *buffer = msg->data; //dereference optimization

  if (msg->bit >= msg->maxbits)
  {
    return 0;
  }

  value = 0;

  if (bits < 0)
  {
    bits = -bits; //always greater than zero
    sgn = qtrue;
  }
  else
  {
    sgn = qfalse;
  }

  if (msg->oob)
  {
    if (bits == 8)
    {
      value = *(buffer + msg->readcount);
      msg->readcount += 1;
      msg->bit += 8;
    }
    else if (bits == 16)
    {
      short temp;

      CopyLittleShort(&temp, buffer + msg->readcount);
      value = temp;
      msg->readcount += 2;
      msg->bit += 16;
    }
    else if (bits == 32)
    {
      CopyLittleLong(&value, buffer + msg->readcount);
      msg->readcount += 4;
      msg->bit += 32;
    }
    else
    {
      Com_Error(ERR_DROP, "can't read %d bits", bits);
    }
  }
  else
  {
    const qint nbits = bits & 7;
    qint bitIndex = msg->bit; //dereference optimization

    if (nbits)
    {		
      for(i = 0;i < nbits;i++)
      {
        value |= HuffmanGetBit(buffer, bitIndex) << i;
        bitIndex++;
      }

      bits -= nbits;
    }

    if (bits)
    {
      for(i = 0;i < bits;i += 8)
      {
        bitIndex += HuffmanGetSymbol(&sym, buffer, bitIndex);
        value |= (sym << (i + nbits));
      }
    }

    msg->bit = bitIndex;
    msg->readcount = (bitIndex >> 3) + 1;
  }

  if (sgn && bits < 32)
  {
    if (value & (1 << (bits - 1)))
    {
      value |= -1 ^ ((BIT(bits)) - 1);
    }
  }

  return value;
}



//================================================================================

//
// writing functions
//

void MSG_WriteChar( msg_t *sb, qint c ) {
#ifdef PARANOID
	if (c < -128 || c > 127)
		Com_Error (ERR_FATAL, "MSG_WriteChar: range error");
#endif

	MSG_WriteBits( sb, c, 8 );
}

void MSG_WriteByte( msg_t *sb, qint c ) {
#ifdef PARANOID
	if (c < 0 || c > 255)
		Com_Error (ERR_FATAL, "MSG_WriteByte: range error");
#endif

	MSG_WriteBits( sb, c, 8 );
}

void MSG_WriteData( msg_t *buf, const void *data, qint length ) {
	qint i;
	for(i=0;i<length;i++) {
		MSG_WriteByte(buf, ((byte *)data)[i]);
	}
}

void MSG_WriteShort( msg_t *sb, qint c ) {
#ifdef PARANOID
	if (c < ((short)0x8000) || c > (short)0x7fff)
		Com_Error (ERR_FATAL, "MSG_WriteShort: range error");
#endif

	MSG_WriteBits( sb, c, 16 );
}

void MSG_WriteLong( msg_t *sb, qint c ) {
	MSG_WriteBits( sb, c, 32 );
}

void MSG_WriteFloat( msg_t *sb, float f ) {
        floatint_t dat;

	dat.f = f;
	MSG_WriteBits( sb, dat.i, 32 );
}

void MSG_WriteString( msg_t *sb, const qchar *s ) {
	if ( !s ) {
		MSG_WriteData (sb, "", 1);
	} else {
		qint		l,i;
		qchar	string[MAX_STRING_CHARS];

		l = strlen( s );
		if ( l >= MAX_STRING_CHARS ) {
			Com_Printf( "MSG_WriteString: MAX_STRING_CHARS" );
			MSG_WriteData (sb, "", 1);
			return;
		}
		Q_strncpyz( string, s, sizeof( string ) );

		// get rid of 0x80+ chars, because old clients don't like them
		for ( i = 0 ; i < l ; i++ ) {
			if ( ((byte *)string)[i] > 127 ) {
				string[i] = '.';
			}
		}

		MSG_WriteData (sb, string, l+1);
	}
}

void MSG_WriteBigString( msg_t *sb, const qchar *s ) {
	if ( !s ) {
		MSG_WriteData (sb, "", 1);
	} else {
		qint		l,i;
		qchar	string[BIG_INFO_STRING];

		l = strlen( s );
		if ( l >= BIG_INFO_STRING ) {
			Com_Printf( "MSG_WriteString: BIG_INFO_STRING" );
			MSG_WriteData (sb, "", 1);
			return;
		}
		Q_strncpyz( string, s, sizeof( string ) );

		// get rid of 0x80+ chars, because old clients don't like them
		for ( i = 0 ; i < l ; i++ ) {
			if ( ((byte *)string)[i] > 127 ) {
				string[i] = '.';
			}
		}

		MSG_WriteData (sb, string, l+1);
	}
}

void MSG_WriteAngle( msg_t *sb, float f ) {
	MSG_WriteByte (sb, (qint)(f*256/360) & 255);
}

void MSG_WriteAngle16( msg_t *sb, float f ) {
	MSG_WriteShort (sb, ANGLE2SHORT(f));
}


//============================================================

//
// reading functions
//

// returns -1 if no more characters are available
qint MSG_ReadChar (msg_t *msg ) {
	qint	c;
	
	c = (signed qchar)MSG_ReadBits( msg, 8 );
	if ( msg->readcount > msg->cursize ) {
		c = -1;
	}	
	
	return c;
}

qint MSG_ReadByte( msg_t *msg ) {
	qint	c;
	
	c = (unsigned qchar)MSG_ReadBits( msg, 8 );
	if ( msg->readcount > msg->cursize ) {
		c = -1;
	}	
	return c;
}

qint MSG_ReadShort( msg_t *msg ) {
	qint	c;
	
	c = (short)MSG_ReadBits( msg, 16 );
	if ( msg->readcount > msg->cursize ) {
		c = -1;
	}	

	return c;
}

qint MSG_ReadLong( msg_t *msg ) {
	qint	c;
	
	c = MSG_ReadBits( msg, 32 );
	if ( msg->readcount > msg->cursize ) {
		c = -1;
	}	
	
	return c;
}

float MSG_ReadFloat( msg_t *msg ) {
        floatint_t dat;
	
	dat.i = MSG_ReadBits( msg, 32 );
	if ( msg->readcount > msg->cursize ) {
		dat.f = -1;
	}	
	
	return dat.f;	
}

qchar *MSG_ReadString( msg_t *msg ) {
	static qchar	string[MAX_STRING_CHARS];
	qint		l,c;
	
	l = 0;
	do {
		c = MSG_ReadByte(msg);		// use ReadByte so -1 is out of bounds
		if ( c <= 0 /*c == -1 || c == 0*/ ) {
			break;
		}
		// translate all fmt spec to avoid crash bugs
		if ( c == '%' ) {
			c = '.';
		}
		// don't allow higher ascii values
		if ( c > 127 ) {
			c = '.';
		}

		string[l] = c;
		l++;
	} while (l < sizeof(string)-1);
	
	string[l] = '\0';
	
	return string;
}

qchar *MSG_ReadBigString( msg_t *msg ) {
	static qchar	string[BIG_INFO_STRING];
	qint		l,c;
	
	l = 0;
	do {
		c = MSG_ReadByte(msg);		// use ReadByte so -1 is out of bounds
		if ( c <= 0 /*c == -1 || c == 0*/ ) {
			break;
		}
		// translate all fmt spec to avoid crash bugs
		if ( c == '%' ) {
			c = '.';
		}
		// don't allow higher ascii values
		if ( c > 127 ) {
			c = '.';
		}

		string[l] = c;
		l++;
	} while (l < sizeof(string)-1);
	
	string[l] = '\0';
	
	return string;
}

qchar *MSG_ReadStringLine( msg_t *msg ) {
	static qchar	string[MAX_STRING_CHARS];
	qint		l,c;

	l = 0;
	do {
		c = MSG_ReadByte(msg);		// use ReadByte so -1 is out of bounds
		if (c <= 0 /*c == -1 || c == 0*/ || c == '\n') {
			break;
		}
		// translate all fmt spec to avoid crash bugs
		if ( c == '%' ) {
			c = '.';
		}
		// don't allow higher ascii values
		if ( c > 127 ) {
			c = '.';
		}

		string[l] = c;
		l++;
	} while (l < sizeof(string)-1);
	
	string[l] = '\0';
	
	return string;
}

float MSG_ReadAngle16( msg_t *msg ) {
	return SHORT2ANGLE(MSG_ReadShort(msg));
}

void MSG_ReadData( msg_t *msg, void *data, qint len ) {
	qint		i;

	for (i=0 ; i<len ; i++) {
		((byte *)data)[i] = MSG_ReadByte (msg);
	}
}

//a string hasher which gives the same hash value even if the
//string is later modified via the legacy MSG read/write code
qint
MSG_HashKey(const qchar *string, const qint maxlen)
{
  qint hash;
  qint i;

  hash = 0;

  for(i = 0;i < maxlen && string[i] != '\0';i++)
  {
    if (string[i] & 0x80 || string[i] == '%')
    {
      hash += '.' * (119 + i);
    }
    else
    {
      hash += string[i] * (119 + i);
    }
  }

  hash = (hash ^ (hash >> 10) ^ (hash >> 20));
  return hash;
}

/*
=============================================================================

delta functions
  
=============================================================================
*/
#if !defined(DEDICATED)
extern cvar_t *cl_shownet;

#define	LOG(x) if( cl_shownet && cl_shownet->integer == 4 ) { Com_Printf("%s ", x ); };
#else
#define LOG(x)
#endif
void MSG_WriteDelta( msg_t *msg, qint oldV, qint newV, qint bits ) {
	if ( oldV == newV ) {
		MSG_WriteBits( msg, 0, 1 );
		return;
	}
	MSG_WriteBits( msg, 1, 1 );
	MSG_WriteBits( msg, newV, bits );
}

qint	MSG_ReadDelta( msg_t *msg, qint oldV, qint bits ) {
	if ( MSG_ReadBits( msg, 1 ) ) {
		return MSG_ReadBits( msg, bits );
	}
	return oldV;
}

void MSG_WriteDeltaFloat( msg_t *msg, float oldV, float newV ) {
        floatint_t fi;
	if ( oldV == newV ) {
		MSG_WriteBits( msg, 0, 1 );
		return;
	}
	fi.f = newV;
	MSG_WriteBits( msg, 1, 1 );
	MSG_WriteBits( msg, fi.i, 32 );
}

float MSG_ReadDeltaFloat( msg_t *msg, float oldV ) {
	if ( MSG_ReadBits( msg, 1 ) ) {
		floatint_t fi;

		fi.i = MSG_ReadBits( msg, 32 );
		return fi.f;
	}
	return oldV;
}

/*
=============================================================================

delta functions with keys
  
=============================================================================
*/

qint kbitmask[32] = {
	0x00000001, 0x00000003, 0x00000007, 0x0000000F,
	0x0000001F,	0x0000003F,	0x0000007F,	0x000000FF,
	0x000001FF,	0x000003FF,	0x000007FF,	0x00000FFF,
	0x00001FFF,	0x00003FFF,	0x00007FFF,	0x0000FFFF,
	0x0001FFFF,	0x0003FFFF,	0x0007FFFF,	0x000FFFFF,
	0x001FFFFf,	0x003FFFFF,	0x007FFFFF,	0x00FFFFFF,
	0x01FFFFFF,	0x03FFFFFF,	0x07FFFFFF,	0x0FFFFFFF,
	0x1FFFFFFF,	0x3FFFFFFF,	0x7FFFFFFF,	0xFFFFFFFF,
};

void MSG_WriteDeltaKey( msg_t *msg, qint key, qint oldV, qint newV, qint bits ) {
	if ( oldV == newV ) {
		MSG_WriteBits( msg, 0, 1 );
		return;
	}
	MSG_WriteBits( msg, 1, 1 );
	MSG_WriteBits( msg, newV ^ key, bits );
}

qint	MSG_ReadDeltaKey( msg_t *msg, qint key, qint oldV, qint bits ) {
	if ( MSG_ReadBits( msg, 1 ) ) {
		return MSG_ReadBits( msg, bits ) ^ (key & kbitmask[bits]);
	}
	return oldV;
}

void MSG_WriteDeltaKeyFloat( msg_t *msg, qint key, float oldV, float newV ) {
        floatint_t fi;
	if ( oldV == newV ) {
		MSG_WriteBits( msg, 0, 1 );
		return;
	}
	fi.f = newV;
	MSG_WriteBits( msg, 1, 1 );
	MSG_WriteBits( msg, fi.i ^ key, 32 );
}

float MSG_ReadDeltaKeyFloat( msg_t *msg, qint key, float oldV ) {
	if ( MSG_ReadBits( msg, 1 ) ) {
		floatint_t fi;

		fi.i = MSG_ReadBits( msg, 32 ) ^ key;
		return fi.f;
	}
	return oldV;
}


/*
============================================================================

usercmd_t communication

============================================================================
*/

// ms is allways sent, the others are optional
#define	CM_ANGLE1 	(1<<0)
#define	CM_ANGLE2 	(1<<1)
#define	CM_ANGLE3 	(1<<2)
#define	CM_FORWARD	(1<<3)
#define	CM_SIDE		(1<<4)
#define	CM_UP		(1<<5)
#define	CM_BUTTONS	(1<<6)
#define CM_WEAPON	(1<<7)

/*
=====================
MSG_WriteDeltaUsercmd
=====================
*/
void MSG_WriteDeltaUsercmd( msg_t *msg, usercmd_t *from, usercmd_t *to ) {
	if ( to->serverTime - from->serverTime < 256 ) {
		MSG_WriteBits( msg, 1, 1 );
		MSG_WriteBits( msg, to->serverTime - from->serverTime, 8 );
	} else {
		MSG_WriteBits( msg, 0, 1 );
		MSG_WriteBits( msg, to->serverTime, 32 );
	}
	MSG_WriteDelta( msg, from->angles[0], to->angles[0], 16 );
	MSG_WriteDelta( msg, from->angles[1], to->angles[1], 16 );
	MSG_WriteDelta( msg, from->angles[2], to->angles[2], 16 );
	MSG_WriteDelta( msg, from->forwardmove, to->forwardmove, 8 );
	MSG_WriteDelta( msg, from->rightmove, to->rightmove, 8 );
	MSG_WriteDelta( msg, from->upmove, to->upmove, 8 );
	MSG_WriteDelta( msg, from->buttons, to->buttons, 16 );
	MSG_WriteDelta( msg, from->weapon, to->weapon, 8 );
}


/*
=====================
MSG_ReadDeltaUsercmd
=====================
*/
void MSG_ReadDeltaUsercmd( msg_t *msg, usercmd_t *from, usercmd_t *to ) {
	if ( MSG_ReadBits( msg, 1 ) ) {
		to->serverTime = from->serverTime + MSG_ReadBits( msg, 8 );
	} else {
		to->serverTime = MSG_ReadBits( msg, 32 );
	}
	to->angles[0] = MSG_ReadDelta( msg, from->angles[0], 16);
	to->angles[1] = MSG_ReadDelta( msg, from->angles[1], 16);
	to->angles[2] = MSG_ReadDelta( msg, from->angles[2], 16);
	to->forwardmove = MSG_ReadDelta( msg, from->forwardmove, 8);
	to->rightmove = MSG_ReadDelta( msg, from->rightmove, 8);
	to->upmove = MSG_ReadDelta( msg, from->upmove, 8);
	to->buttons = MSG_ReadDelta( msg, from->buttons, 16);
	to->weapon = MSG_ReadDelta( msg, from->weapon, 8);
}

/*
=====================
MSG_WriteDeltaUsercmd
=====================
*/
void MSG_WriteDeltaUsercmdKey( msg_t *msg, qint key, usercmd_t *from, usercmd_t *to ) {
	if ( to->serverTime - from->serverTime < 256 ) {
		MSG_WriteBits( msg, 1, 1 );
		MSG_WriteBits( msg, to->serverTime - from->serverTime, 8 );
	} else {
		MSG_WriteBits( msg, 0, 1 );
		MSG_WriteBits( msg, to->serverTime, 32 );
	}
	if (from->angles[0] == to->angles[0] &&
		from->angles[1] == to->angles[1] &&
		from->angles[2] == to->angles[2] &&
		from->forwardmove == to->forwardmove &&
		from->rightmove == to->rightmove &&
		from->upmove == to->upmove &&
		from->buttons == to->buttons &&
		from->weapon == to->weapon) {
			MSG_WriteBits( msg, 0, 1 );				// no change
			return;
	}
	key ^= to->serverTime;
	MSG_WriteBits( msg, 1, 1 );
	MSG_WriteDeltaKey( msg, key, from->angles[0], to->angles[0], 16 );
	MSG_WriteDeltaKey( msg, key, from->angles[1], to->angles[1], 16 );
	MSG_WriteDeltaKey( msg, key, from->angles[2], to->angles[2], 16 );
	MSG_WriteDeltaKey( msg, key, from->forwardmove, to->forwardmove, 8 );
	MSG_WriteDeltaKey( msg, key, from->rightmove, to->rightmove, 8 );
	MSG_WriteDeltaKey( msg, key, from->upmove, to->upmove, 8 );
	MSG_WriteDeltaKey( msg, key, from->buttons, to->buttons, 16 );
	MSG_WriteDeltaKey( msg, key, from->weapon, to->weapon, 8 );
}


/*
=====================
MSG_ReadDeltaUsercmd
=====================
*/
void MSG_ReadDeltaUsercmdKey( msg_t *msg, qint key, const usercmd_t *from, usercmd_t *to ) {
	if ( MSG_ReadBits( msg, 1 ) ) {
		to->serverTime = from->serverTime + MSG_ReadBits( msg, 8 );
	} else {
		to->serverTime = MSG_ReadBits( msg, 32 );
	}
	if ( MSG_ReadBits( msg, 1 ) ) {
		key ^= to->serverTime;
		to->angles[0] = MSG_ReadDeltaKey( msg, key, from->angles[0], 16);
		to->angles[1] = MSG_ReadDeltaKey( msg, key, from->angles[1], 16);
		to->angles[2] = MSG_ReadDeltaKey( msg, key, from->angles[2], 16);
		to->forwardmove = MSG_ReadDeltaKey( msg, key, from->forwardmove, 8);
		to->rightmove = MSG_ReadDeltaKey( msg, key, from->rightmove, 8);
		to->upmove = MSG_ReadDeltaKey( msg, key, from->upmove, 8);
		to->buttons = MSG_ReadDeltaKey( msg, key, from->buttons, 16);
		to->weapon = MSG_ReadDeltaKey( msg, key, from->weapon, 8);
	} else {
		to->angles[0] = from->angles[0];
		to->angles[1] = from->angles[1];
		to->angles[2] = from->angles[2];
		to->forwardmove = from->forwardmove;
		to->rightmove = from->rightmove;
		to->upmove = from->upmove;
		to->buttons = from->buttons;
		to->weapon = from->weapon;
	}
}

/*
=============================================================================

entityState_t communication
  
=============================================================================
*/

/*
=================
MSG_ReportChangeVectors_f

Prints out a table from the current statistics for copying to code
=================
*/
void MSG_ReportChangeVectors_f( void ) {
	qint i;
	for(i=0;i<256;i++) {
		if (pcount[i]) {
			Com_Printf("%d used %d\n", i, pcount[i]);
		}
	}
}

typedef struct
{
  const qchar *name;
  const qint offset;
  const qint bits; //0 = float
}
netField_t;

// using the stringizing operator to save typing...
#define	NETF(x) #x,(size_t)&((entityState_t*)0)->x

static const netField_t entityStateFields[] = 
{
{ NETF(pos.trTime), 32 },
{ NETF(pos.trBase[0]), 0 },
{ NETF(pos.trBase[1]), 0 },
{ NETF(pos.trDelta[0]), 0 },
{ NETF(pos.trDelta[1]), 0 },
{ NETF(pos.trBase[2]), 0 },
{ NETF(apos.trBase[1]), 0 },
{ NETF(pos.trDelta[2]), 0 },
{ NETF(apos.trBase[0]), 0 },
{ NETF(event), 10 },
{ NETF(angles2[1]), 0 },
{ NETF(eType), 8 },
{ NETF(torsoAnim), 8 },
{ NETF(eventParm), 8 },
{ NETF(legsAnim), 8 },
{ NETF(groundEntityNum), GENTITYNUM_BITS },
{ NETF(pos.trType), 8 },
{ NETF(eFlags), 19 },
{ NETF(otherEntityNum), GENTITYNUM_BITS },
{ NETF(weapon), 8 },
{ NETF(clientNum), 8 },
{ NETF(angles[1]), 0 },
{ NETF(pos.trDuration), 32 },
{ NETF(apos.trType), 8 },
{ NETF(origin[0]), 0 },
{ NETF(origin[1]), 0 },
{ NETF(origin[2]), 0 },
{ NETF(solid), 24 },
{ NETF(misc), MAX_MISC },
{ NETF(modelindex), 8 },
{ NETF(otherEntityNum2), GENTITYNUM_BITS },
{ NETF(loopSound), 8 },
{ NETF(generic1), 8 },
{ NETF(origin2[2]), 0 },
{ NETF(origin2[0]), 0 },
{ NETF(origin2[1]), 0 },
{ NETF(modelindex2), 8 },
{ NETF(angles[0]), 0 },
{ NETF(time), 32 },
{ NETF(apos.trTime), 32 },
{ NETF(apos.trDuration), 32 },
{ NETF(apos.trBase[2]), 0 },
{ NETF(apos.trDelta[0]), 0 },
{ NETF(apos.trDelta[1]), 0 },
{ NETF(apos.trDelta[2]), 0 },
{ NETF(time2), 32 },
{ NETF(angles[2]), 0 },
{ NETF(angles2[0]), 0 },
{ NETF(angles2[2]), 0 },
{ NETF(constantLight), 32 },
{ NETF(frame), 16 }
};


// if (qint)f == f and (qint)f + ( 1<<(FLOAT_INT_BITS-1) ) < ( 1 << FLOAT_INT_BITS )
// the float will be sent with FLOAT_INT_BITS, otherwise all 32 bits will be sent
#define	FLOAT_INT_BITS	13
#define	FLOAT_INT_BIAS	(1<<(FLOAT_INT_BITS-1))

/*
==================
MSG_WriteDeltaEntity

Writes part of a packetentities message, including the entity number.
Can delta from either a baseline or a previous packet_entity
If to is NULL, a remove entity update will be sent
If force is not set, then nothing at all will be generated if the entity is
identical, under the assumption that the in-order delta code will catch it.
==================
*/
void MSG_WriteDeltaEntity( msg_t *msg, const entityState_t *from, const entityState_t *to, 
						   qbool force ) {
	qint			i, lc;
	qint			numFields;
	const netField_t	*field;
	qint			trunc;
	float		fullFloat;
	qint			*fromF, *toF;

	numFields = ARRAY_LEN(entityStateFields);

	// all fields should be 32 bits to avoid any compiler packing issues
	// the "number" field is not part of the field list
	// if this assert fails, someone added a field to the entityState_t
	// struct without updating the message fields
	assert( numFields + 1 == sizeof( *from )/4 );

	// a NULL to is a delta remove message
	if ( to == NULL ) {
		if ( from == NULL ) {
			return;
		}
		MSG_WriteBits( msg, from->number, GENTITYNUM_BITS );
		MSG_WriteBits( msg, 1, 1 );
		return;
	}

	if ( to->number < 0 || to->number >= MAX_GENTITIES ) {
		Com_Error (ERR_FATAL, "MSG_WriteDeltaEntity: Bad entity number: %i", to->number );
	}

	lc = 0;
	// build the change vector as bytes so it is endien independent
	for ( i = 0, field = entityStateFields ; i < numFields ; i++, field++ ) {
		fromF = (qint *)( (byte *)from + field->offset );
		toF = (qint *)( (byte *)to + field->offset );
		if ( *fromF != *toF ) {
			lc = i+1;
		}
	}

	if ( lc == 0 ) {
		// nothing at all changed
		if ( !force ) {
			return;		// nothing at all
		}
		// write two bits for no change
		MSG_WriteBits( msg, to->number, GENTITYNUM_BITS );
		MSG_WriteBits( msg, 0, 1 );		// not removed
		MSG_WriteBits( msg, 0, 1 );		// no delta
		return;
	}

	MSG_WriteBits( msg, to->number, GENTITYNUM_BITS );
	MSG_WriteBits( msg, 0, 1 );			// not removed
	MSG_WriteBits( msg, 1, 1 );			// we have a delta

	MSG_WriteByte( msg, lc );	// # of changes


	for ( i = 0, field = entityStateFields ; i < lc ; i++, field++ ) {
		fromF = (qint *)( (byte *)from + field->offset );
		toF = (qint *)( (byte *)to + field->offset );

		if ( *fromF == *toF ) {
			MSG_WriteBits( msg, 0, 1 );	// no change
			continue;
		}

		MSG_WriteBits( msg, 1, 1 );	// changed

		if ( field->bits == 0 ) {
			// float
			fullFloat = *(float *)toF;
			trunc = (qint)fullFloat;

			if (fullFloat == 0.0f) {
					MSG_WriteBits(msg, 0, 1);
			} else {
				MSG_WriteBits( msg, 1, 1 );
				if ( trunc == fullFloat && trunc + FLOAT_INT_BIAS >= 0 && 
					trunc + FLOAT_INT_BIAS < ( 1 << FLOAT_INT_BITS ) ) {
					// send as small integer
					MSG_WriteBits( msg, 0, 1 );
					MSG_WriteBits( msg, trunc + FLOAT_INT_BIAS, FLOAT_INT_BITS );
				} else {
					// send as full floating point value
					MSG_WriteBits( msg, 1, 1 );
					MSG_WriteBits( msg, *toF, 32 );
				}
			}
		} else {
			if (*toF == 0) {
				MSG_WriteBits( msg, 0, 1 );
			} else {
				MSG_WriteBits( msg, 1, 1 );
				// integer
				MSG_WriteBits( msg, *toF, field->bits );
			}
		}
	}
}

/*
==================
MSG_ReadDeltaEntity

The entity number has already been read from the message, which
is how the from state is identified.

If the delta removes the entity, entityState_t->number will be set to MAX_GENTITIES-1

Can go from either a baseline or a previous packet_entity
==================
*/
void MSG_ReadDeltaEntity( msg_t *msg, const entityState_t *from, entityState_t *to, 
						 qint number) {
	qint			i, lc;
	qint			numFields;
	const netField_t	*field;
	qint			*fromF, *toF;
	qint			print;
	qint			trunc;
	qint			startBit, endBit;

	if ( number < 0 || number >= MAX_GENTITIES) {
		Com_Error( ERR_DROP, "Bad delta entity number: %i", number );
	}

	if ( msg->bit == 0 ) {
		startBit = msg->readcount * 8 - GENTITYNUM_BITS;
	} else {
		startBit = ( msg->readcount - 1 ) * 8 + msg->bit - GENTITYNUM_BITS;
	}

	// check for a remove
	if ( MSG_ReadBits( msg, 1 ) == 1 ) {
		Com_Memset( to, 0, sizeof( *to ) );	
		to->number = MAX_GENTITIES - 1;
#if !defined(DEDICATED)
		if ( cl_shownet && (cl_shownet->integer >= 2 || cl_shownet->integer == -1) ) {
			Com_Printf( "%3i: #%-3i remove\n", msg->readcount, number );
		}
#endif
		return;
	}

	// check for no delta
	if ( MSG_ReadBits( msg, 1 ) == 0 ) {
		*to = *from;
		to->number = number;
		return;
	}

	numFields = ARRAY_LEN(entityStateFields);
	lc = MSG_ReadByte(msg);

        if (lc > numFields || lc < 0)
        {
          Com_Error(ERR_DROP, "invalid entityState field count");
        }
#if !defined(DEDICATED)
	// shownet 2/3 will interleave with other printed info, -1 will
	// just print the delta records`
	if ( cl_shownet && (cl_shownet->integer >= 2 || cl_shownet->integer == -2) ) {
		print = 1;
		Com_Printf( "%3i: #%-3i ", msg->readcount, to->number );
	} else {
		print = 0;
	}
#else
        print = 0;
#endif
	to->number = number;

	for ( i = 0, field = entityStateFields ; i < lc ; i++, field++ ) {
		fromF = (qint *)( (byte *)from + field->offset );
		toF = (qint *)( (byte *)to + field->offset );

		if ( ! MSG_ReadBits( msg, 1 ) ) {
			// no change
			*toF = *fromF;
		} else {
			if ( field->bits == 0 ) {
				// float
				if ( MSG_ReadBits( msg, 1 ) == 0 ) {
						*(float *)toF = 0.0f; 
				} else {
					if ( MSG_ReadBits( msg, 1 ) == 0 ) {
						// integral float
						trunc = MSG_ReadBits( msg, FLOAT_INT_BITS );
						// bias to allow equal parts positive and negative
						trunc -= FLOAT_INT_BIAS;
						*(float *)toF = trunc; 
						if ( print ) {
							Com_Printf( "%s:%i ", field->name, trunc );
						}
					} else {
						// full floating point value
						*toF = MSG_ReadBits( msg, 32 );
						if ( print ) {
							Com_Printf( "%s:%f ", field->name, *(float *)toF );
						}
					}
				}
			} else {
				if ( MSG_ReadBits( msg, 1 ) == 0 ) {
					*toF = 0;
				} else {
					// integer
					*toF = MSG_ReadBits( msg, field->bits );
					if ( print ) {
						Com_Printf( "%s:%i ", field->name, *toF );
					}
				}
			}
//			pcount[i]++;
		}
	}
	for ( i = lc, field = &entityStateFields[lc] ; i < numFields ; i++, field++ ) {
		fromF = (qint *)( (byte *)from + field->offset );
		toF = (qint *)( (byte *)to + field->offset );
		// no change
		*toF = *fromF;
	}

	if ( print ) {
		if ( msg->bit == 0 ) {
			endBit = msg->readcount * 8 - GENTITYNUM_BITS;
		} else {
			endBit = ( msg->readcount - 1 ) * 8 + msg->bit - GENTITYNUM_BITS;
		}
		Com_Printf( " (%i bits)\n", endBit - startBit  );
	}
}

/*
================
enitityShared_t communication
================
*/

#define	ESF(x) #x,(size_t)&((entityShared_t*)0)->x

static const netField_t entitySharedFields[] = 
{
  {ESF(linked), 1},
  {ESF(linkcount), 8},
  {ESF(bmodel), 1},
  {ESF(svFlags), 12},
  {ESF(singleClient), CLIENTNUM_BITS},
  {ESF(contents), 32},
  {ESF(ownerNum), GENTITYNUM_BITS},
  {ESF(mins[0]), 0},
  {ESF(mins[1]), 0},
  {ESF(mins[2]), 0},
  {ESF(maxs[0]), 0},
  {ESF(maxs[1]), 0},
  {ESF(maxs[2]), 0 },
  {ESF(absmin[0]), 0},
  {ESF(absmin[1]), 0},
  {ESF(absmin[2]), 0},
  {ESF(absmax[0]), 0},
  {ESF(absmax[1]), 0},
  {ESF(absmax[2]), 0},
  {ESF(currentOrigin[0]), 0},
  {ESF(currentOrigin[1]), 0},
  {ESF(currentOrigin[2]), 0},
  {ESF(currentAngles[0]), 0},
  {ESF(currentAngles[1]), 0},
  {ESF(currentAngles[2]), 0}
};


/*
==================
MSG_WriteDeltaSharedEntity
==================
*/
void
MSG_WriteDeltaSharedEntity(msg_t *msg, void *from, void *to, qbool force, qint number)
{
  qint i;
  qint lc;
  qint numFields;
  const netField_t *field;
  qint trunc;
  float fullFloat;
  qint *fromF;
  qint *toF;
  
  numFields = ARRAY_LEN(entitySharedFields);
  //all fields should be 32 bits
  assert(numFields == (sizeof(entityShared_t) - sizeof(entityState_t)) / 4);
  
  lc = 0;
  //build change vector as bytes
  for(i = 0, field = entitySharedFields;i < numFields;i++, field++)
  {
    fromF = (qint *)( (byte *)from + field->offset);
    toF = (qint *)( (byte *)to + field->offset);
    if (*fromF != *toF)
    {
      lc = i+1;
    }
  }
  
  if (lc == 0)
  {
    //nothing changed
    if (!force)
    {
      return; //nothing
    }
    
    //write bits for nothing
    MSG_WriteBits(msg, number, GENTITYNUM_BITS);
    MSG_WriteBits(msg, 0, 1); //no delta
    return;
  }
  
  MSG_WriteBits(msg, number, GENTITYNUM_BITS);
  MSG_WriteBits(msg, 1, 1); //delta
  MSG_WriteByte( msg, lc ); //number of changes
  
  for(i = 0, field = entitySharedFields;i < lc ;i++, field++)
  {
    fromF = (qint *)((byte *)from + field->offset);
    toF = (qint *)((byte *)to + field->offset);
    
    if (*fromF == *toF)
    {
      MSG_WriteBits(msg, 0, 1); //no change
      continue;
    }
    
    MSG_WriteBits(msg, 1, 1); //changed
    
    if (field->bits == 0)
    {
      //float
      fullFloat = *(float *)toF;
      trunc = (qint)fullFloat;
      
      if (fullFloat == 0.0f)
      {
        MSG_WriteBits(msg, 0, 1);
      }
      else
      {
        MSG_WriteBits(msg, 1, 1);
        
        if (trunc == fullFloat && trunc + FLOAT_INT_BIAS >= 0 && trunc + FLOAT_INT_BIAS < (1 << FLOAT_INT_BITS))
        {
          //send as small integer
          MSG_WriteBits(msg, 0, 1);
          MSG_WriteBits(msg, trunc + FLOAT_INT_BIAS, FLOAT_INT_BITS);
        }
        else
        {
          //send as float
          MSG_WriteBits(msg, 1, 1);
          MSG_WriteBits(msg, *toF, 32);
        }
      }
    }
    else
    {
      if (*toF == 0)
      {
        MSG_WriteBits(msg, 0, 1);
      }
      else
      {
        MSG_WriteBits(msg, 1, 1);
        MSG_WriteBits(msg, *toF, field->bits); //qint
      }
    }
  }
}

/*
==================
MSG_ReadDeltaSharedEntity
==================
*/
void
MSG_ReadDeltaSharedEntity(msg_t *msg, void *from, void *to, qint number)
{
  qint i;
  qint lc;
  qint numFields;
  const netField_t *field;
  qint *fromF;
  qint *toF;
  qint trunc;
  
  //no delta check
  if (MSG_ReadBits(msg, 1) == 0)
  {
    *(entityShared_t*)to = *(entityShared_t*)from;
    return;
  }
  
  numFields = ARRAY_LEN(entitySharedFields);
  lc = MSG_ReadByte(msg);
  
  for(i = 0, field = entitySharedFields;i < lc;i++, field++)
  {
    fromF = (qint *)((byte *)from + field->offset);
    toF = (qint *)((byte *)to + field->offset);
    
    if (!MSG_ReadBits(msg, 1))
    {
      //nothing
      *toF = *fromF;
    }
    else
    {
      if (field->bits == 0)
      {
        //float
        if (MSG_ReadBits(msg, 1) == 0)
        {
          *(float *)toF = 0.0f; 
        }
        else
        {
          if (MSG_ReadBits(msg, 1) == 0)
          {
            trunc = MSG_ReadBits( msg, FLOAT_INT_BITS ); //float
            trunc -= FLOAT_INT_BIAS; //equal
            *(float *)toF = trunc; 
          }
          else
          {
            *toF = MSG_ReadBits( msg, 32 ); //float
          }
        }
      }
      else
      {
        if (MSG_ReadBits(msg, 1) == 0)
        {
          *toF = 0;
        }
        else
        {
          *toF = MSG_ReadBits( msg, field->bits ); //qint
        }
      }
    //pcount[i]++;
    }
  }
  for(i = lc, field = &entitySharedFields[lc];i < numFields;i++, field++)
  {
    fromF = (qint *)((byte *)from + field->offset);
    toF = (qint *)( (byte *)to + field->offset);
    //nothing
    *toF = *fromF;
  }
}

/*
============================================================================

plyer_state_t communication

============================================================================
*/

// using the stringizing operator to save typing...
#define	PSF(x) #x,(size_t)&((playerState_t*)0)->x

static const netField_t playerStateFields[] = 
{
{ PSF(commandTime), 32 },				
{ PSF(origin[0]), 0 },
{ PSF(origin[1]), 0 },
{ PSF(bobCycle), 8 },
{ PSF(velocity[0]), 0 },
{ PSF(velocity[1]), 0 },
{ PSF(viewangles[1]), 0 },
{ PSF(viewangles[0]), 0 },
{ PSF(weaponTime), -16 },
{ PSF(origin[2]), 0 },
{ PSF(velocity[2]), 0 },
{ PSF(legsTimer), 8 },
{ PSF(pm_time), -16 },
{ PSF(eventSequence), 16 },
{ PSF(torsoAnim), 8 },
{ PSF(movementDir), 4 },
{ PSF(events[0]), 8 },
{ PSF(legsAnim), 8 },
{ PSF(events[1]), 8 },
{ PSF(pm_flags), 16 },
{ PSF(groundEntityNum), GENTITYNUM_BITS },
{ PSF(weaponstate), 4 },
{ PSF(eFlags), 16 },
{ PSF(externalEvent), 10 },
{ PSF(gravity), 16 },
{ PSF(speed), 16 },
{ PSF(delta_angles[1]), 16 },
{ PSF(externalEventParm), 8 },
{ PSF(viewheight), -8 },
{ PSF(damageEvent), 8 },
{ PSF(damageYaw), 8 },
{ PSF(damagePitch), 8 },
{ PSF(damageCount), 8 },
{ PSF(generic1), 8 },
{ PSF(pm_type), 8 },					
{ PSF(delta_angles[0]), 16 },
{ PSF(delta_angles[2]), 16 },
{ PSF(torsoTimer), 12 },
{ PSF(eventParms[0]), 8 },
{ PSF(eventParms[1]), 8 },
{ PSF(clientNum), 8 },
{ PSF(weapon), 5 },
{ PSF(viewangles[2]), 0 },
{ PSF(grapplePoint[0]), 0 },
{ PSF(grapplePoint[1]), 0 },
{ PSF(grapplePoint[2]), 0 },
{PSF(otherEntityNum), GENTITYNUM_BITS},
{ PSF(loopSound), 16 }
};

/*
=============
MSG_WriteDeltaPlayerstate

=============
*/
void MSG_WriteDeltaPlayerstate( msg_t *msg, const playerState_t *from, const playerState_t *to ) {
        static const playerState_t dummy = {0};
	qint				i;
	qint				statsbits;
	qint				persistantbits;
	qint				ammobits;
	qint				miscbits;
	qint				numFields;
	qint				c;
	const netField_t		*field;
	const qint				*fromF, *toF;
	float			fullFloat;
	qint				trunc, lc;

	if (!from) {
		from = &dummy;
	}

	c = msg->cursize;

	numFields = ARRAY_LEN(playerStateFields);

	lc = 0;
	for ( i = 0, field = playerStateFields ; i < numFields ; i++, field++ ) {
		fromF = (qint *)( (byte *)from + field->offset );
		toF = (qint *)( (byte *)to + field->offset );
		if ( *fromF != *toF ) {
			lc = i+1;
		}
	}

	MSG_WriteByte( msg, lc );	// # of changes

	for ( i = 0, field = playerStateFields ; i < lc ; i++, field++ ) {
		fromF = (qint *)( (byte *)from + field->offset );
		toF = (qint *)( (byte *)to + field->offset );

		if ( *fromF == *toF ) {
			MSG_WriteBits( msg, 0, 1 );	// no change
			continue;
		}

		MSG_WriteBits( msg, 1, 1 );	// changed
//		pcount[i]++;

		if ( field->bits == 0 ) {
			// float
			fullFloat = *(float *)toF;
			trunc = (qint)fullFloat;

			if ( trunc == fullFloat && trunc + FLOAT_INT_BIAS >= 0 && 
				trunc + FLOAT_INT_BIAS < ( 1 << FLOAT_INT_BITS ) ) {
				// send as small integer
				MSG_WriteBits( msg, 0, 1 );
				MSG_WriteBits( msg, trunc + FLOAT_INT_BIAS, FLOAT_INT_BITS );
			} else {
				// send as full floating point value
				MSG_WriteBits( msg, 1, 1 );
				MSG_WriteBits( msg, *toF, 32 );
			}
		} else {
			// integer
			MSG_WriteBits( msg, *toF, field->bits );
		}
	}
	c = msg->cursize - c;


	//
	// send the arrays
	//
	statsbits = 0;
	for (i=0 ; i<MAX_STATS ; i++) {
		if (to->stats[i] != from->stats[i]) {
			statsbits |= 1<<i;
		}
	}
	persistantbits = 0;
	for (i=0 ; i<MAX_PERSISTANT ; i++) {
		if (to->persistant[i] != from->persistant[i]) {
			persistantbits |= 1<<i;
		}
	}
	ammobits = (to->ammo != from->ammo) | (to->clips != from->clips) << 1;
	for (i=0 ; i<14 ; i++) {
		if (to->padding[i] != from->padding[i]) {
			ammobits |= 4<<i;
		}
	}
	miscbits = 0;
	for (i=0 ; i<MAX_MISC ; i++) {
		if (to->misc[i] != from->misc[i]) {
			miscbits |= 1<<i;
		}
	}

	if (!statsbits && !persistantbits && !ammobits && !miscbits) {
		MSG_WriteBits( msg, 0, 1 );	// no change
		return;
	}
	MSG_WriteBits( msg, 1, 1 );	// changed

	if ( statsbits ) {
		MSG_WriteBits( msg, 1, 1 );	// changed
		MSG_WriteBits( msg, statsbits, MAX_STATS );
		for (i=0 ; i<MAX_STATS ; i++)
			if (statsbits & (1<<i) )
				MSG_WriteShort (msg, to->stats[i]);
	} else {
		MSG_WriteBits( msg, 0, 1 );	// no change
	}


	if ( persistantbits ) {
		MSG_WriteBits( msg, 1, 1 );	// changed
		MSG_WriteBits( msg, persistantbits, MAX_PERSISTANT );
		for (i=0 ; i<MAX_PERSISTANT ; i++)
			if (persistantbits & (1<<i) )
				MSG_WriteShort (msg, to->persistant[i]);
	} else {
		MSG_WriteBits( msg, 0, 1 );	// no change
	}


	if ( ammobits ) {
		MSG_WriteBits( msg, 1, 1 );	// changed
		MSG_WriteBits( msg, ammobits, MAX_WEAPONS );
		if(ammobits & 1)
			MSG_WriteShort(msg, to->ammo);
		if(ammobits & 2)
			MSG_WriteShort(msg, to->clips);
		for (i=0 ; i<14 ; i++)
			if (ammobits & (4<<i) )
				MSG_WriteShort (msg, to->padding[i]);
	} else {
		MSG_WriteBits( msg, 0, 1 );	// no change
	}


	if ( miscbits ) {
		MSG_WriteBits( msg, 1, 1 );	// changed
		MSG_WriteBits( msg, miscbits, MAX_MISC );
		for (i=0 ; i<MAX_MISC ; i++)
			if (miscbits & (1<<i) )
				MSG_WriteLong( msg, to->misc[i] );
	} else {
		MSG_WriteBits( msg, 0, 1 );	// no change
	}
}


/*
===================
MSG_ReadDeltaPlayerstate
===================
*/
void MSG_ReadDeltaPlayerstate (msg_t *msg, const playerState_t *from, playerState_t *to ) {
	qint			i, lc;
	qint			bits;
	const netField_t	*field;
	qint			numFields;
	qint			startBit, endBit;
	qint			print;
	const qint			*fromF;
	qint *toF;
	qint			trunc;
	playerState_t	dummy;

	if ( !from ) {
		from = &dummy;
		Com_Memset( &dummy, 0, sizeof( dummy ) );
	}
	*to = *from;

	if ( msg->bit == 0 ) {
		startBit = msg->readcount * 8 - GENTITYNUM_BITS;
	} else {
		startBit = ( msg->readcount - 1 ) * 8 + msg->bit - GENTITYNUM_BITS;
	}
#if !defined(DEDICATED)
	// shownet 2/3 will interleave with other printed info, -2 will
	// just print the delta records
	if ( cl_shownet->integer >= 2 || cl_shownet->integer == -2 ) {
		print = 1;
		Com_Printf( "%3i: playerstate ", msg->readcount );
	} else {
		print = 0;
	}
#else
        print = 0;
#endif
	numFields = ARRAY_LEN(playerStateFields);
	lc = MSG_ReadByte(msg);

        if (lc > numFields || lc < 0)
        {
          Com_Error(ERR_DROP, "invalid playerState field count");
        }

	for ( i = 0, field = playerStateFields ; i < lc ; i++, field++ ) {
		fromF = (qint *)( (byte *)from + field->offset );
		toF = (qint *)( (byte *)to + field->offset );

		if ( ! MSG_ReadBits( msg, 1 ) ) {
			// no change
			*toF = *fromF;
		} else {
			if ( field->bits == 0 ) {
				// float
				if ( MSG_ReadBits( msg, 1 ) == 0 ) {
					// integral float
					trunc = MSG_ReadBits( msg, FLOAT_INT_BITS );
					// bias to allow equal parts positive and negative
					trunc -= FLOAT_INT_BIAS;
					*(float *)toF = trunc; 
					if ( print ) {
						Com_Printf( "%s:%i ", field->name, trunc );
					}
				} else {
					// full floating point value
					*toF = MSG_ReadBits( msg, 32 );
					if ( print ) {
						Com_Printf( "%s:%f ", field->name, *(float *)toF );
					}
				}
			} else {
				// integer
				*toF = MSG_ReadBits( msg, field->bits );
				if ( print ) {
					Com_Printf( "%s:%i ", field->name, *toF );
				}
			}
		}
	}
	for ( i=lc,field = &playerStateFields[lc];i<numFields; i++, field++) {
		fromF = (qint *)( (byte *)from + field->offset );
		toF = (qint *)( (byte *)to + field->offset );
		// no change
		*toF = *fromF;
	}


	// read the arrays
	if (MSG_ReadBits( msg, 1 ) ) {
		// parse stats
		if ( MSG_ReadBits( msg, 1 ) ) {
			LOG("PS_STATS");
			bits = MSG_ReadBits (msg, MAX_STATS);
			for (i=0 ; i<MAX_STATS ; i++) {
				if (bits & (1<<i) ) {
					to->stats[i] = MSG_ReadShort(msg);
				}
			}
		}

		// parse persistant stats
		if ( MSG_ReadBits( msg, 1 ) ) {
			LOG("PS_PERSISTANT");
			bits = MSG_ReadBits (msg, MAX_PERSISTANT);
			for (i=0 ; i<MAX_PERSISTANT ; i++) {
				if (bits & (1<<i) ) {
					to->persistant[i] = MSG_ReadShort(msg);
				}
			}
		}

		// parse ammo
		if ( MSG_ReadBits( msg, 1 ) ) {
			LOG("PS_AMMO");
			bits = MSG_ReadBits (msg, MAX_WEAPONS);
			if(bits & 1)
				to->ammo = MSG_ReadShort(msg);
			if(bits & 2)
				to->clips = MSG_ReadShort(msg);
			for (i=0 ; i<14 ; i++) {
				if (bits & (4<<i) ) {
					to->padding[i] = MSG_ReadShort(msg);
				}
			}
		}

		// parse misc data
		if ( MSG_ReadBits( msg, 1 ) ) {
			LOG("PS_MISC");
			bits = MSG_ReadBits (msg, MAX_MISC);
			for (i=0 ; i<MAX_MISC ; i++) {
				if (bits & (1<<i) ) {
					to->misc[i] = MSG_ReadLong(msg);
				}
			}
		}
	}

	if ( print ) {
		if ( msg->bit == 0 ) {
			endBit = msg->readcount * 8 - GENTITYNUM_BITS;
		} else {
			endBit = ( msg->readcount - 1 ) * 8 + msg->bit - GENTITYNUM_BITS;
		}
		Com_Printf( " (%i bits)\n", endBit - startBit  );
	}
}

//===========================================================================
