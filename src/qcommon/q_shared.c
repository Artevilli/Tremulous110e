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
// q_shared.c -- stateless support routines that are included in each code dll
#include "q_shared.h"

float Com_Clamp( float min, float max, float value ) {
	if ( value < min ) {
		return min;
	}
	if ( value > max ) {
		return max;
	}
	return value;
}


/*
============
COM_SkipPath
============
*/
qchar *COM_SkipPath (qchar *pathname)
{
	qchar	*last;
	
	last = pathname;
	while (*pathname)
	{
		if (*pathname=='/')
			last = pathname+1;
		pathname++;
	}
	return last;
}

/*
============
COM_GetExtension
============
*/
const qchar *
COM_GetExtension(const qchar *name)
{
  const qchar *dot = Q_strrchr(name, '.');
  const qchar *slash;

  if (dot && (!(slash = Q_strrchr(name, '/')) || slash < dot))
  {
    return dot + 1;
  }
  else
  {
    return "";
  }
}


/*
============
COM_StripExtension
============
*/
void
COM_StripExtension(const qchar *in, qchar *out, qint destsize)
{
  const qchar *dot = Q_strrchr(in, '.');
  const qchar *slash;

  if (dot && (!(slash = Q_strrchr(in, '/')) || slash < dot))
  {
    destsize = (destsize < dot - in + 1 ? destsize:dot - in + 1);
  }

  if (in == out && destsize > 1)
  {
    out[destsize - 1] = '\0';
  }
  else
  {
    Q_strncpyz(out, in, destsize);
  }
}

/*
==================
COM_CompareExtension

String compare the end of strings and return qtrue if strings match
==================
*/
qbool
COM_CompareExtension(const qchar *in, const qchar *ext)
{
  qint inlen;
  qint extlen;

  inlen = strlen(in);
  extlen = strlen(ext);

  if (extlen <= inlen)
  {
    in += inlen - extlen;

    if (!Q_stricmp(in, ext))
    {
      return qtrue;
    }
  }

  return qfalse;
}

/*
==================
COM_DefaultExtension

if path doesn't have an extension, then append
the specified one (which should include the .)
==================
*/
void
COM_DefaultExtension(qchar *path, qint maxSize, const qchar *extension)
{
  const qchar *dot = Q_strrchr(path, '.');
  const qchar *slash;

  if (dot && (!(slash = Q_strrchr(path, '/')) || slash < dot))
  {
    return;
  }

  Q_strcat(path, maxSize, extension);
}

/*
==================
COM_GenerateHashValue

used in renderer and filesystem
==================
*/
//ASCII lowcase conversion table with '\\' turned to '/' and '.' to '\0'
static const byte hash_locase[256] =
{
  0x00,
  0x01,
  0x02,
  0x03,
  0x04,
  0x05,
  0x06,
  0x07,
  0x08,
  0x09,
  0x0a,
  0x0b,
  0x0c,
  0x0d,
  0x0e,
  0x0f,
  0x10,
  0x11,
  0x12,
  0x13,
  0x14,
  0x15,
  0x16,
  0x17,
  0x18,
  0x19,
  0x1a,
  0x1b,
  0x1c,
  0x1d,
  0x1e,
  0x1f,
  0x20,
  0x21,
  0x22,
  0x23,
  0x24,
  0x25,
  0x26,
  0x27,
  0x28,
  0x29,
  0x2a,
  0x2b,
  0x2c,
  0x2d,
  0x00,
  0x2f,
  0x30,
  0x31,
  0x32,
  0x33,
  0x34,
  0x35,
  0x36,
  0x37,
  0x38,
  0x39,
  0x3a,
  0x3b,
  0x3c,
  0x3d,
  0x3e,
  0x3f,
  0x40,
  0x61,
  0x62,
  0x63,
  0x64,
  0x65,
  0x66,
  0x67,
  0x68,
  0x69,
  0x6a,
  0x6b,
  0x6c,
  0x6d,
  0x6e,
  0x6f,
  0x70,
  0x71,
  0x72,
  0x73,
  0x74,
  0x75,
  0x76,
  0x77,
  0x78,
  0x79,
  0x7a,
  0x5b,
  0x2f,
  0x5d,
  0x5e,
  0x5f,
  0x60,
  0x61,
  0x62,
  0x63,
  0x64,
  0x65,
  0x66,
  0x67,
  0x68,
  0x69,
  0x6a,
  0x6b,
  0x6c,
  0x6d,
  0x6e,
  0x6f,
  0x70,
  0x71,
  0x72,
  0x73,
  0x74,
  0x75,
  0x76,
  0x77,
  0x78,
  0x79,
  0x7a,
  0x7b,
  0x7c,
  0x7d,
  0x7e,
  0x7f,
  0x80,
  0x81,
  0x82,
  0x83,
  0x84,
  0x85,
  0x86,
  0x87,
  0x88,
  0x89,
  0x8a,
  0x8b,
  0x8c,
  0x8d,
  0x8e,
  0x8f,
  0x90,
  0x91,
  0x92,
  0x93,
  0x94,
  0x95,
  0x96,
  0x97,
  0x98,
  0x99,
  0x9a,
  0x9b,
  0x9c,
  0x9d,
  0x9e,
  0x9f,
  0xa0,
  0xa1,
  0xa2,
  0xa3,
  0xa4,
  0xa5,
  0xa6,
  0xa7,
  0xa8,
  0xa9,
  0xaa,
  0xab,
  0xac,
  0xad,
  0xae,
  0xaf,
  0xb0,
  0xb1,
  0xb2,
  0xb3,
  0xb4,
  0xb5,
  0xb6,
  0xb7,
  0xb8,
  0xb9,
  0xba,
  0xbb,
  0xbc,
  0xbd,
  0xbe,
  0xbf,
  0xc0,
  0xc1,
  0xc2,
  0xc3,
  0xc4,
  0xc5,
  0xc6,
  0xc7,
  0xc8,
  0xc9,
  0xca,
  0xcb,
  0xcc,
  0xcd,
  0xce,
  0xcf,
  0xd0,
  0xd1,
  0xd2,
  0xd3,
  0xd4,
  0xd5,
  0xd6,
  0xd7,
  0xd8,
  0xd9,
  0xda,
  0xdb,
  0xdc,
  0xdd,
  0xde,
  0xdf,
  0xe0,
  0xe1,
  0xe2,
  0xe3,
  0xe4,
  0xe5,
  0xe6,
  0xe7,
  0xe8,
  0xe9,
  0xea,
  0xeb,
  0xec,
  0xed,
  0xee,
  0xef,
  0xf0,
  0xf1,
  0xf2,
  0xf3,
  0xf4,
  0xf5,
  0xf6,
  0xf7,
  0xf8,
  0xf9,
  0xfa,
  0xfb,
  0xfc,
  0xfd,
  0xfe,
  0xff
};

long
Com_GenerateHashValue(const qchar *fname, const unsigned size) 
{
  const byte *s;
  unsigned long hash;
  qint c;

  s = (byte *)fname;
  hash = 0;
	
  while((c = hash_locase[(byte)*s++]) != '\0')
  {
    hash = hash * 101 + c;
  }
	
  hash = (hash ^ (hash >> 10) ^ (hash >> 20));
  hash &= (size-1);
  return hash;
}

/*
============================================================================

					BYTE ORDER FUNCTIONS

============================================================================
*/
/*
// can't just use function pointers, or dll linkage can
// mess up when qcommon is included in multiple places
static short	(*_BigShort) (short l);
static short	(*_LittleShort) (short l);
static qint		(*_BigLong) (qint l);
static qint		(*_LittleLong) (qint l);
static qint64	(*_BigLong64) (qint64 l);
static qint64	(*_LittleLong64) (qint64 l);
static float	(*_BigFloat) (const float *l);
static float	(*_LittleFloat) (const float *l);

short	BigShort(short l){return _BigShort(l);}
short	LittleShort(short l) {return _LittleShort(l);}
qint		BigLong (qint l) {return _BigLong(l);}
qint		LittleLong (qint l) {return _LittleLong(l);}
qint64 	BigLong64 (qint64 l) {return _BigLong64(l);}
qint64 	LittleLong64 (qint64 l) {return _LittleLong64(l);}
float	BigFloat (const float *l) {return _BigFloat(l);}
float	LittleFloat (const float *l) {return _LittleFloat(l);}
*/

void
CopyShortSwap(void *dest, void *src)
{
  byte *to = dest;
  byte *from = src;

  to[0] = from[1];
  to[1] = from[0];
}

void
CopyLongSwap(void *dest, void *src)
{
  byte *to = dest;
  byte *from = src;

  to[0] = from[3];
  to[1] = from[2];
  to[2] = from[1];
  to[3] = from[0];
}

short   ShortSwap (short l)
{
	byte    b1,b2;

	b1 = l&255;
	b2 = (l>>8)&255;

	return (b1<<8) + b2;
}

short	ShortNoSwap (short l)
{
	return l;
}

qint    LongSwap (qint l)
{
	byte    b1,b2,b3,b4;

	b1 = l&255;
	b2 = (l>>8)&255;
	b3 = (l>>16)&255;
	b4 = (l>>24)&255;

	return ((qint)b1<<24) + ((qint)b2<<16) + ((qint)b3<<8) + b4;
}

qint	LongNoSwap (qint l)
{
	return l;
}

qint64 Long64Swap (qint64 ll)
{
	qint64	result;

	result.b0 = ll.b7;
	result.b1 = ll.b6;
	result.b2 = ll.b5;
	result.b3 = ll.b4;
	result.b4 = ll.b3;
	result.b5 = ll.b2;
	result.b6 = ll.b1;
	result.b7 = ll.b0;

	return result;
}

qint64 Long64NoSwap (qint64 ll)
{
	return ll;
}

float FloatSwap (const float *f) {
	floatint_t out;

	out.f = *f;
	out.i = LongSwap(out.i);

	return out.f;
}

float FloatNoSwap (const float *f)
{
	return *f;
}

/*
================
Swap_Init
================
*/
/*
void Swap_Init (void)
{
	byte	swaptest[2] = {1,0};

// set the byte swapping variables in a portable manner	
	if ( *(short *)swaptest == 1)
	{
		_BigShort = ShortSwap;
		_LittleShort = ShortNoSwap;
		_BigLong = LongSwap;
		_LittleLong = LongNoSwap;
		_BigLong64 = Long64Swap;
		_LittleLong64 = Long64NoSwap;
		_BigFloat = FloatSwap;
		_LittleFloat = FloatNoSwap;
	}
	else
	{
		_BigShort = ShortNoSwap;
		_LittleShort = ShortSwap;
		_BigLong = LongNoSwap;
		_LittleLong = LongSwap;
		_BigLong64 = Long64NoSwap;
		_LittleLong64 = Long64Swap;
		_BigFloat = FloatNoSwap;
		_LittleFloat = FloatSwap;
	}

}
*/

/*
============================================================================

PARSING

============================================================================
*/

static qchar com_token[MAX_TOKEN_CHARS];
static qchar com_parsename[MAX_TOKEN_CHARS];
static qint com_lines;
static qint com_tokenline;

//for complex parser
tokenType_t com_tokentype;

void
COM_BeginParseSession(const qchar *name)
{
  com_lines = 1;
  com_tokenline = 0;
  Com_sprintf(com_parsename, sizeof(com_parsename), "%s", name);
}

qint
COM_GetCurrentParseLine(void)
{
  if (com_tokenline)
  {
    return com_tokenline;
  }

  return com_lines;
}

const qchar *
COM_Parse(const qchar **data_p)
{
  return COM_ParseExt(data_p, qtrue);
}

void
COM_ParseError(const qchar *format, ...)
{
  va_list argptr;
  static qchar string[4096];

  va_start(argptr, format);
  Q_vsnprintf(string, sizeof(string), format, argptr);
  va_end(argptr);

  Com_Printf("ERROR: %s, line %d: %s\n", com_parsename, COM_GetCurrentParseLine(), string);
}

void
COM_ParseWarning(qchar *format, ...)
{
  va_list argptr;
  static qchar string[4096];

  va_start(argptr, format);
  Q_vsnprintf(string, sizeof(string), format, argptr);
  va_end(argptr);

  Com_Printf("WARNING: %s, line %d: %s\n", com_parsename, COM_GetCurrentParseLine(), string);
}

/*
==============
COM_Parse

Parse a token out of a string
Will never return NULL, just empty strings

If "allowLineBreaks" is qtrue then an empty
string will be returned if the next token is
a newline.
==============
*/
static const qchar *SkipWhitespace( const qchar *data, qbool *hasNewLines ) {
	qint c;

	while( (c = *data) <= ' ') {
		if( !c ) {
			return NULL;
		}
		if( c == '\n' ) {
			com_lines++;
			*hasNewLines = qtrue;
		}
		data++;
	}

	return data;
}

qint
COM_Compress(qchar *data_p)
{
  const qchar *in;
  qchar *out;
  qint c;
  qbool newline = qfalse;
  qbool whitespace = qfalse;

  in = out = data_p;

  while((c = *in) != '\0')
  {
    //skip double slash comments
    if (c == '/' && in[1] == '/')
    {
      while(*in && *in != '\n')
      {
        in++;
      }
    }
    //skip /* */ comments
    else if (c == '/' && in[1] == '*')
    {
      while(*in && (*in != '*' || in[1] != '/'))
      {
        in++;
      }

      if (*in)
      {
        in += 2;
      }
    }
    //record when we hit a newline
    else if (c == '\n' || c == '\r')
    {
      newline = qtrue;
      in++;
    }
    //record when we hit whitespace
    else if (c == ' ' || c == '\t')
    {
      whitespace = qtrue;
      in++;
    }
    //an actual token
    else
    {
      //if we have a pending newline, emit it (and it counts as whitespace)
      if (newline)
      {
        *out++ = '\n';
        newline = qfalse;
        whitespace = qfalse;
      }
      else if (whitespace)
      {
        *out++ = ' ';
        whitespace = qfalse;
      }

      //copy quoted strings unmolested
      if (c == '"')
      {
        *out++ = c;
        in++;

        while(1)
        {
          c = *in;

          if (c && c != '"')
          {
            *out++ = c;
            in++;
          }
          else
          {
            break;
          }
        }

        if (c == '"')
        {
          *out++ = c;
          in++;
        }
      }
      else
      {
        *out++ = c;
        in++;
      }
    }
  }

  *out = '\0';
  return out - data_p;
}

qchar *COM_ParseExt( const qchar **data_p, qbool allowLineBreaks )
{
	qint c = 0, len;
	qbool hasNewLines = qfalse;
	const qchar *data;

	data = *data_p;
	len = 0;
	com_token[0] = '\0';
	com_tokenline = 0;

	// make sure incoming data is valid
	if ( !data )
	{
		*data_p = NULL;
		return com_token;
	}

	while ( 1 )
	{
		// skip whitespace
		data = SkipWhitespace( data, &hasNewLines );
		if ( !data )
		{
			*data_p = NULL;
			return com_token;
		}
		if ( hasNewLines && !allowLineBreaks )
		{
			*data_p = data;
			return com_token;
		}

		c = *data;

		// skip double slash comments
		if ( c == '/' && data[1] == '/' )
		{
			data += 2;
			while (*data && *data != '\n') {
				data++;
			}
		}
		// skip /* */ comments
		else if ( c=='/' && data[1] == '*' ) 
		{
			data += 2;
			while ( *data && ( *data != '*' || data[1] != '/' ) ) 
			{
				data++;
			}
			if ( *data ) 
			{
				data += 2;
			}
		}
		else
		{
			break;
		}
	}

        //token starts on this line
        com_tokenline = com_lines;

	// handle quoted strings
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				*data_p = data;
				return com_token;
			}
			if (len < MAX_TOKEN_CHARS - 1)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do
	{
		if (len < MAX_TOKEN_CHARS - 1)
		{
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
		if ( c == '\n' )
			com_lines++;
	} while (c>32);

	com_token[len] = 0;

	*data_p = data;
	return com_token;
}


#if 0
// no longer used
/*
===============
COM_ParseInfos
===============
*/
qint COM_ParseInfos( qchar *buf, qint max, qchar infos[][MAX_INFO_STRING] ) {
	qchar	*token;
	qint		count;
	qchar	key[MAX_TOKEN_CHARS];

	count = 0;

	while ( 1 ) {
		token = COM_Parse( &buf );
		if ( !token[0] ) {
			break;
		}
		if ( strcmp( token, "{" ) ) {
			Com_Printf( "Missing { in info file\n" );
			break;
		}

		if ( count == max ) {
			Com_Printf( "Max infos exceeded\n" );
			break;
		}

		infos[count][0] = 0;
		while ( 1 ) {
			token = COM_ParseExt( &buf, qtrue );
			if ( !token[0] ) {
				Com_Printf( "Unexpected end of info file\n" );
				break;
			}
			if ( !strcmp( token, "}" ) ) {
				break;
			}
			Q_strncpyz( key, token, sizeof( key ) );

			token = COM_ParseExt( &buf, qfalse );
			if ( !token[0] ) {
				strcpy( token, "<NULL>" );
			}
			Info_SetValueForKey( infos[count], key, token );
		}
		count++;
	}

	return count;
}
#endif

/*
==============
COM_ParseComplex
==============
*/
qchar *
COM_ParseComplex(const qchar **data_p, qbool allowLineBreaks)
{
  static const byte is_separator[256] =
  {
    //\0 . . . . . . .\b\t\n . .\r . .
      1,0,0,0,0,0,0,0,0,1,1,0,0,1,0,0,
    // . . . . . . . . . . . . . . . .
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    //   ! " # $ % & ' ( ) * + , - . /
      1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0, // excl. '-' '.' '/'
    // 0 1 2 3 4 5 6 7 8 9 : ; < = > ?
      0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,
    // @ A B C D E F G H I J K L M N O
      1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    // P Q R S T U V W X Y Z [ \ ] ^ _
      0,0,0,0,0,0,0,0,0,0,0,1,0,1,1,0, // excl. '\\' '_'
    // ` a b c d e f g h i j k l m n o
      1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    // p q r s t u v w x y z { | } ~ 
      0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1
  };

  qint c;
  qint len;
  qint shift;
  const byte *str;

  str = (byte *)*data_p;
  len = 0; 
  shift = 0; //token line shift relative to com_lines
  com_tokentype = TK_GENERIC;
	
__reswitch:
  switch(*str)
  {
    case
    '\0':
      com_tokentype = TK_EOF;
      break;

    //whitespace
    case
    ' ':

    case
    '\t':
      str++;

      while((c = *str) == ' ' || c == '\t')
      {
        str++;
      }

      goto __reswitch;

    //newlines
    case
    '\n':

    case
    '\r':
      com_lines++;

      if (*str == '\r' && str[1] == '\n')
      {
        str += 2; //CR + LF
      }
      else
      {
        str++;
      }

      if (!allowLineBreaks)
      {
        com_tokentype = TK_NEWLINE;
        break;
      }

      goto __reswitch;

    // comments, single slash
    case
    '/':
      //until end of line
      if (str[1] == '/')
      {
        str += 2;

        while((c = *str) != '\0' && c != '\n' && c != '\r')
        {
          str++;
        }

        goto __reswitch;
      }

      //comment
      if (str[1] == '*')
      {
        str += 2;

        while((c = *str) != '\0' && (c != '*' || str[1] != '/'))
        {
          if (c == '\n' || c == '\r')
          {
            com_lines++;

            if (c == '\r' && str[1] == '\n') //CR + LF?
            {
              str++;
            }
          }

          str++;
        }

        if (c != '\0' && str[1] != '\0')
        {
          str += 2;
        }
        else
        {
          //FIXME: unterminated comment?
        }

        goto __reswitch;
      }

      //single slash
      com_token[len++] = *str++;
      break;
	
    //quoted string?
    case
    '"':
      str++; //skip leading '"'
      //com_tokenline = com_lines;

      while((c = *str) != '\0' && c != '"')
      {
        if (c == '\n' || c == '\r')
        {
          com_lines++; //FIXME: unterminated quoted string?
          shift++;
        }

        if (len < MAX_TOKEN_CHARS - 1) //overflow check
        {
          com_token[len++] = c;
        }

        str++;
      }

      if (c != '\0')
      {
        str++; //skip ending '"'
      }
      else
      {
        //FIXME: unterminated quoted string?
      }

      com_tokentype = TK_QUOTED;
      break;

    //single tokens:
    case
    '+':

    case
    '`':

    /*case
    '*':*/

    case
    '~':

    case
    '{':

    case
    '}':

    case
    '[':

    case
    ']':

    case
    '?':

    case
    ',':

    case
    ':':

    case
    ';':

    case
    '%':

    case
    '^':
      com_token[len++] = *str++;
      break;

    case
    '*':
      com_token[len++] = *str++;
      com_tokentype = TK_MATCH;
      break;

    case
    '(':
      com_token[len++] = *str++;
      com_tokentype = TK_SCOPE_OPEN;
      break;

    case
    ')':
      com_token[len++] = *str++;
      com_tokentype = TK_SCOPE_CLOSE;
      break;

    //!, !=
    case
    '!':
      com_token[len++] = *str++;

      if (*str == '=')
      {
        com_token[len++] = *str++;
        com_tokentype = TK_NEQ;
      }

      break;

    //=, ==
    case
    '=':
      com_token[len++] = *str++;

      if (*str == '=')
      {
        com_token[len++] = *str++;
        com_tokentype = TK_EQ;
      }

      break;

    //>, >=
    case
    '>':
      com_token[len++] = *str++;

      if (*str == '=')
      {
        com_token[len] = *str;
        com_tokentype = TK_GTE;
      }
      else
      {
        com_tokentype = TK_GT;
      }

      break;

    //<, <=
    case
    '<':
      com_token[len++] = *str++;

      if (*str == '=')
      {
        com_token[len++] = *str++;
        com_tokentype = TK_LTE;
      }
      else
      {
        com_tokentype = TK_LT;
      }

      break;

    //|, ||
    case
    '|':
      com_token[len++] = *str++;

      if (*str == '|')
      {
        com_token[len++] = *str++;
        com_tokentype = TK_OR;
      }

      break;

    //&, &&
    case
    '&':
      com_token[len++] = *str++;

      if (*str == '&')
      {
        com_token[len++] = *str++;
        com_tokentype = TK_AND;
      }

      break;

    //rest of the charset
    default:
      com_token[len++] = *str++;

      while(!is_separator[(c = *str)])
      {
        if (len < MAX_TOKEN_CHARS - 1)
        {
          com_token[len] = c;
          len++;
        }

        str++;
      }

      com_tokentype = TK_STRING;
      break;

  } //switch(*str)

  com_tokenline = com_lines - shift;
  com_token[len] = '\0';
  *data_p = (qchar *)str;
  return com_token;
}

/*
==================
COM_MatchToken
==================
*/
void COM_MatchToken( const qchar **buf_p, const qchar *match ) {
	const qchar *token;

	token = COM_Parse( buf_p );
	if ( strcmp( token, match ) ) {
		Com_Error( ERR_DROP, "MatchToken: %s != %s", token, match );
	}
}


/*
=================
SkipBracedSection

The next token should be an open brace.
Skips until a matching close brace is found.
Internal brace depths are properly skipped.
=================
*/
void SkipBracedSection (const qchar **program) {
	qchar			*token;
	qint				depth;

	depth = 0;
	do {
		token = COM_ParseExt( program, qtrue );
		if( token[1] == 0 ) {
			if( token[0] == '{' ) {
				depth++;
			}
			else if( token[0] == '}' ) {
				depth--;
			}
		}
	} while( depth && *program );
}

/*
=================
SkipRestOfLine
=================
*/
void SkipRestOfLine ( const qchar **data ) {
	const qchar *p;
	qint		c;

	p = *data;
	while ( (c = *p++) != 0 ) {
		if ( c == '\n' ) {
			com_lines++;
			break;
		}
	}

	*data = p;
}


void Parse1DMatrix (const qchar **buf_p, qint x, float *m) {
	const qchar	*token;
	qint		i;

	COM_MatchToken( buf_p, "(" );

	for (i = 0 ; i < x ; i++) {
		token = COM_Parse(buf_p);
		m[i] = Q_atof(token);
	}

	COM_MatchToken( buf_p, ")" );
}

void Parse2DMatrix (const qchar **buf_p, qint y, qint x, float *m) {
	qint		i;

	COM_MatchToken( buf_p, "(" );

	for (i = 0 ; i < y ; i++) {
		Parse1DMatrix (buf_p, x, m + i * x);
	}

	COM_MatchToken( buf_p, ")" );
}

void Parse3DMatrix (const qchar **buf_p, qint z, qint y, qint x, float *m) {
	qint		i;

	COM_MatchToken( buf_p, "(" );

	for (i = 0 ; i < z ; i++) {
		Parse2DMatrix (buf_p, y, x, m + i * x*y);
	}

	COM_MatchToken( buf_p, ")" );
}

static qint
Hex(qchar c)
{
  if (c >= '0' && c <= '9')
  {
    return c - '0';
  }
  else if (c >= 'A' && c <= 'F')
  {
    return 10 + c - 'A';
  }
  else if (c >= 'a' && c <= 'f')
  {
    return 10 + c - 'a';
  }

  return -1;
}

/*
=================
Com_HexStrToInt
=================
*/
qint
Com_HexStrToInt(const qchar *str)
{
  if (!str)
  {
    return -1;
  }

  //check for hex code
  if (str[0] == '0' && str[1] == 'x' && str[2] != '\0')
  {
    qint i;
    qint digit;
    qint n;
    const qint len = strlen(str);

    n = 0;

    for(i = 2;i < len;i++)
    {
      n *= 16;
      digit = Hex(str[i]);

      if (digit < 0)
      {
        return -1;
      }

      n += digit;
    }

    return n;
  }

  return -1;
}

/*
============================================================================

					LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

const byte locase[256] = {
  0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
  0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
  0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
  0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
  0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
  0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
  0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
  0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
  0x40,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
  0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
  0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
  0x78,0x79,0x7a,0x5b,0x5c,0x5d,0x5e,0x5f,
  0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
  0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
  0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
  0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
  0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,
  0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
  0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,
  0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
  0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,
  0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
  0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,
  0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
  0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,
  0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
  0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,
  0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
  0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,
  0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
  0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,
  0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
};

qint Q_isprint( qint c )
{
	if ( c >= 0x20 && c <= 0x7E )
		return ( 1 );
	return ( 0 );
}

qint Q_islower( qint c )
{
	if (c >= 'a' && c <= 'z')
		return ( 1 );
	return ( 0 );
}

qint Q_isupper( qint c )
{
	if (c >= 'A' && c <= 'Z')
		return ( 1 );
	return ( 0 );
}

qint Q_isalpha( qint c )
{
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
		return ( 1 );
	return ( 0 );
}

qchar* Q_strrchr( const qchar* string, qint c )
{
	qchar cc = c;
	qchar *s;
	qchar *sp=(qchar *)0;

	s = (qchar*)string;

	while (*s)
	{
		if (*s == cc)
			sp = s;
		s++;
	}
	if (cc == 0)
		sp = s;

	return sp;
}

qbool
Q_isanumber(const qchar *s)
{
	qchar *p;

	if (*s == '\0')
		return qfalse;

	strtod(s, &p);

	return *p == '\0';
}

qbool Q_isintegral( float f )
{
	return (qint)f == f;
}

#ifdef _MSC_VER
/*
=============
Q_vsnprintf
 
Special wrapper function for Microsoft's broken _vsnprintf() function.
MinGW comes with its own snprintf() which is not broken.
=============
*/

qint
Q_vsnprintf(qchar *str, size_t size, const qchar *format, va_list ap)
{
  qint retval;
	
  retval = _vsnprintf(str, size, format, ap);

  if (retval < 0 || retval == size)
  {
    //Microsoft doesn't adhere to the C99 standard of vsnprintf,
    //which states that the return value must be the number of
    //bytes written if the output string had sufficient length.
    //
    //Obviously we cannot determine that value from Microsoft's
    //implementation, so we have no choice but to return size.
		
    str[size - 1] = '\0';
    return size;
  }
	
  return retval;
}
#endif

/*
=============
Q_strncpyz
 
Safe strncpy that ensures a trailing zero
=============
*/
void
Q_strncpyz(qchar *dest, const qchar *src, qint destsize)
{
  if (!dest)
  {
    Com_Error(ERR_FATAL, "Q_strncpyz: NULL dest");
  }

  if (!src)
  {
    Com_Error(ERR_FATAL, "Q_strncpyz: NULL src");
  }

  if (destsize < 1)
  {
    Com_Error(ERR_FATAL,"Q_strncpyz: destsize < 1" ); 
  }

#if 1
  //do not fill whole remaining buffer with zeroes
  //this is obvious behavior change but actually it may affect only buggy qvms
  //which passes overlapping or short buffers to cvar reading routines
  //what is rather good than bad because it will no longer cause overwrites, maybe
  while(--destsize > 0 && (*dest++ = *src++) != '\0');

  *dest = '\0';
#else
  strncpy(dest, src, destsize-1);
  dest[destsize-1] = 0;
#endif
}

/*
=============
Q_strncpy

allows src and dest to be overlapped for QVM compatibility purposes
=============
*/
qchar *
Q_strncpy(qchar *dest, qchar *src, qint destsize)
{
  qchar *s = src;
  qchar *start = dest;
  qint src_len;
  qint i;

  while(*s != '\0')
  {
    ++s;
  }

  src_len = (qint)(s - src);

  if (src_len > destsize)
  {
    src_len = destsize;
  }

  destsize -= src_len;

  if (dest > src && dest < src + src_len)
  {
#if defined(_DEBUG)
    Com_Printf(S_COLOR_YELLOW "Q_strncpy: overlapped (dest > src) buffers\n");
#endif

    for(i = src_len - 1;i >= 0;--i)
    {
      dest[i] = src[i]; //back overlapping
    }

    dest += src_len;
  }
  else
  {
#if defined(_DEBUG)
    if (src >= dest && src < dest + src_len)
    {
      Com_Printf(S_COLOR_YELLOW "Q_strncpy: overlapped (src >= dst) buffers\n");
#if defined(_MSC_VER)
      //__debugbreak();
#endif
    }
#endif
    while(src_len > 0)
    {
      *dest++ = *src++;
      --src_len;
    }
  }

  while(destsize > 0)
  {
    *dest++ = '\0';
    --destsize;
  }

  return start;
}

/*
=============
Q_stricmpn
=============
*/
qint
Q_stricmpn(const qchar *s1, const qchar *s2, qint n)
{
  qint c1;
  qint c2;

  //bk001129 - moved in 1.17 fix not in id codebase
  if (s1 == NULL)
  {
    if (s2 == NULL)
    {
      return 0;
    }

    return -1;
  }
  else if (s2 == NULL)
  {
    return 1;
  }

  do
  {
    c1 = *s1++;
    c2 = *s2++;

    if (!n--)
    {
      return 0; //strings are equal until end point
    }
		
    if (c1 != c2)
    {
      if (c1 >= 'a' && c1 <= 'z')
      {
        c1 -= ('a' - 'A');
      }

      if (c2 >= 'a' && c2 <= 'z')
      {
        c2 -= ('a' - 'A');
      }

      if (c1 != c2)
      {
        return c1 < c2 ? -1:1;
      }
    }
  }
  while(c1);
	
  return 0; //strings are equal
}

qint
Q_strncmp(const qchar *s1, const qchar *s2, qint n)
{
  qint c1;
  qint c2;

  do
  {
    c1 = *s1++;
    c2 = *s2++;

    if (!n--)
    {
      return 0; //strings are equal until end point
    }
		
    if (c1 != c2)
    {
      return c1 < c2 ? -1:1;
    }
  }
  while(c1);
	
  return 0; //strings are equal
}

qbool
Q_streq(const qchar *s1, const qchar *s2)
{
  qint c1;
  qint c2;

  do
  {
    c1 = *s1++;
    c2 = *s2++;

    if (c1 != c2)
    {
      return qfalse;
    }
  }
  while(c1 != '\0');

  return qtrue;
}

qint
Q_stricmp(const qchar *s1, const qchar *s2)
{
  unsigned qchar c1;
  unsigned qchar c2;

  if (s1 == NULL)
  {
    if (s2 == NULL)
    {
      return 0;
    }
    else
    {
      return -1;
    }
  }
  else if (s2 == NULL)
  {
    return 1;
  }

  do
  {
    c1 = *s1++;
    c2 = *s2++;

    if (c1 != c2)
    {
      if (c1 <= 'Z' && c1 >= 'A')
      {
        c1 += ('a' - 'A');
      }

      if (c2 <= 'Z' && c2 >= 'A')
      {
        c2 += ('a' - 'A');
      }

      if (c1 != c2)
      {
        return c1 < c2 ? -1:1;
      }
    }
  }
  while(c1);

  return 0;
}


qchar *
Q_strlwr(qchar *s1)
{
  qchar *s;

  s = s1;

  while(*s)
  {
    *s = locase[(byte)*s];
    s++;
  }

  return s1;
}

qchar *
Q_strupr(qchar *s1)
{
  qchar *s;

  s = s1;

  while(*s)
  {
    if (*s >= 'a' && *s <= 'z')
    {
      *s = *s - 'a' + 'A';
    }

    s++;
  }

  return s1;
}


// never goes past bounds or leaves without a terminating 0
void Q_strcat( qchar *dest, qint size, const qchar *src ) {
	qint		l1;

	l1 = strlen( dest );
	if ( l1 >= size ) {
		Com_Error( ERR_FATAL, "Q_strcat: already overflowed" );
	}
	Q_strncpyz( dest + l1, src, size - l1 );
}

qchar *
Q_stradd(qchar *dst, const qchar *src)
{
  qchar c;

  while((c = *src++) != '\0')
  {
    *dst++ = c;
  }

  *dst = '\0';
  return dst;
}

/*
* Find the first occurrence of find in s.
*/
const qchar *
Q_stristr(const qchar *s, const qchar *find)
{
  qchar c;
  qchar sc;
  size_t len;

  if ((c = *find++) != 0)
  {
    if (c >= 'a' && c <= 'z')
    {
      c -= ('a' - 'A');
    }

    len = strlen(find);

    do
    {
      do
      {
        if ((sc = *s++) == 0)
        {
          return NULL;
        }

        if (sc >= 'a' && sc <= 'z')
        {
          sc -= ('a' - 'A');
        }
      }
      while(sc != c);
    }
    while(Q_stricmpn(s, find, len) != 0);

    s--;
  }
  return s;
}

/*
=============
Q_strreplace

replaces content of find by replace in dest
=============
*/
qbool
Q_strreplace(qchar *dest, qint destsize, const qchar *find, const qchar *replace)
{
  qint lstart;
  const qint lfind = strlen(find);
  const qint lreplace = strlen(replace);
  const qint lend = strlen(dest) - 1;
  qchar *s;
  qchar backup[32000]; //big, but small enough to fit in PPC stack

  if (lend >= destsize)
  {
    Com_Error(ERR_FATAL, "Q_strreplace: already overflowed");
  }

  s = strstr(dest, find);

  if (!s)
  {
    return qfalse;
  }
  else
  {
    Q_strncpyz(backup, dest, lend + 1);
    lstart = s - dest;
    strncpy(s, replace, destsize - lstart - 1);
    strncpy(s + lreplace, backup + lstart + lfind, destsize - lstart - lreplace - 1);
    return qtrue;
  }
}

qint Q_PrintStrlen( const qchar *string ) {
	qint			len;
	const qchar	*p;

	if( !string ) {
		return 0;
	}

	len = 0;
	p = string;
	while( *p ) {
		if( Q_IsColorString( p ) ) {
			p += 2;
			continue;
		}
		p++;
		len++;
	}

	return len;
}


qchar *Q_CleanStr( qchar *string ) {
	qchar*	d;
	qchar*	s;
	qint		c;

	s = string;
	d = string;
	while ((c = *s) != 0 ) {
		if ( Q_IsColorString( s ) ) {
			s++;
		}		
		else if ( c >= 0x20 && c <= 0x7E ) {
			*d++ = c;
		}
		s++;
	}
	*d = '\0';

	return string;
}

qint Q_CountChar(const qchar *string, qchar tocount)
{
	qint count;
	
	for(count = 0; *string; string++)
	{
		if(*string == tocount)
			count++;
	}
	
	return count;
}

#if defined(_DEBUG) && defined(_WIN32)
#include <windows.h>
#endif

qint QDECL
Com_sprintf(qchar *dest, qint size, const qchar *fmt, ...)
{
  qint len;
  va_list argptr;
  qchar bigbuffer[32000]; //big, but small enough to fit in PPC stack

  if (!dest)
  {
    Com_Error(ERR_FATAL, "Com_sprintf: NULL dest");
#if defined(_DEBUG) && defined(_WIN32)
    DebugBreak();
#endif
    return 0;
  }

  va_start(argptr, fmt);
  len = vsprintf(bigbuffer, fmt, argptr);
  va_end(argptr);

  if (len >= sizeof(bigbuffer) || len < 0)
  {
    Com_Error(ERR_FATAL, "Com_sprintf: overflowed bigbuffer");
#if defined(_DEBUG) && defined(_WIN32)
    DebugBreak();
#endif
    return 0;
  }

  if (len >= size)
  {
    Com_Printf(S_COLOR_YELLOW "Com_sprintf: overflow of %i in %i\n", len, size);
#if defined(_DEBUG) && defined(_WIN32)
    DebugBreak();
#endif
    len = size - 1;
  }

  //Q_strncpyz(dest, bigbuffer, size);
  //strncpy(dest, bigbuffer, len);
  Com_Memcpy(dest, bigbuffer, len);
  dest[len] = '\0';
  return len;
}


/*
============
va
============
*/
const qchar *
va(qchar *str, const qchar *format, ...)
{
  va_list argptr;
  qint size;
  qint ret_size;

  size = sizeof(qchar) * strlen(format) + sizeof(qchar);
  ret_size = 0;

  if (!str)
  {
    str = (qchar *)malloc(size);
  }
  else
  {
    str = (qchar *)realloc(str, size);
  }

  if (!str)
  {
    return NULL;
  }

  Com_Memset(str, 0, size);

  while(1)
  {
    va_start(argptr, format);
    ret_size = Q_vsnprintf(str, size, format, argptr);
    va_end(argptr);

    if (!str)
    {
      return NULL;
    }

    if (ret_size >= size)
    {
      size *= 2; //truncated
    }
    else
    {
      break; //format done
    }

    str = (qchar *)realloc(str, size);
  }

  //drop unused memory
  str = (qchar *)realloc(str, sizeof(qchar) * strlen(str) + sizeof(qchar));

  if (!str)
  {
    return NULL;
  }

  //be sure last character is a terminator
  *(str + (sizeof(qchar) * strlen(str))) = '\0';
  return str;
}

/*
============
Com_TruncateLongString

Assumes buffer is at least TRUNCATE_LENGTH big
============
*/
void Com_TruncateLongString( qchar *buffer, const qchar *s )
{
	qint length = strlen( s );

	if( length <= TRUNCATE_LENGTH )
		Q_strncpyz( buffer, s, TRUNCATE_LENGTH );
	else
	{
		Q_strncpyz( buffer, s, ( TRUNCATE_LENGTH / 2 ) - 3 );
		Q_strcat( buffer, TRUNCATE_LENGTH, " ... " );
		Q_strcat( buffer, TRUNCATE_LENGTH, s + length - ( TRUNCATE_LENGTH / 2 ) + 3 );
	}
}

/*
=====================================================================

  INFO STRINGS

=====================================================================
*/

static qbool
Q_strkey(const qchar *str, const qchar *key, const unsigned key_len)
{
  unsigned i;

  for(i = 0;i < key_len;i++)
  {
    if (locase[(byte)str[i]] != locase[(byte)key[i]])
    {
      return qfalse;
    }
  }

  return qtrue;
}

/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
FIXME: overflow check?
===============
*/
const qchar *
Info_ValueForKey(const qchar *s, const qchar *key)
{
  static qchar value[2][BIG_INFO_VALUE]; //use two buffers so compares work without stomping on each other
  static qint valueindex = 0;
  const qchar *v;
  const qchar *pkey;
  qchar *o;
  qchar *o2;
  qint klen;
  qint len;
	
  if (!s || !key || !*key)
  {
    return "";
  }

  klen = (qint)strlen(key);

  if (*s == '\\')
  {
    s++;
  }

  while(1)
  {
    pkey = s;

    while(*s != '\\')
    {
      if (*s == '\0')
      {
        return "";
      }

      ++s;
    }

    len = (qint)(s - pkey);
    s++; //skip '\\'
    v = s;

    while(*s != '\\' && *s !='\0')
    {
      s++;
    }

    if (len == klen && Q_strkey(pkey, key, klen))
    {
      o = o2 = value[valueindex ^= 1];

      if ((qint)(s - v) >= BIG_INFO_VALUE)
      {
        Com_Error(ERR_DROP, "Info_ValueForKey: oversize infostring");
      }
      else 
      {
        while(v < s)
        {
          *o++ = *v++;
        }
      }

      *o = '\0';
      return o2;
    }

    if (*s == '\0')
    {
      break;
    }

    s++;
  }

  return "";
}

#define MAX_INFO_TOKENS ((MAX_INFO_STRING / 3) + 2)

static const qchar *info_keys[MAX_INFO_TOKENS];
static const qchar *info_values[MAX_INFO_TOKENS];
static unsigned info_tokens;

/*
===================
Info_Tokenize

Tokenizes all key/value pairs from specified infostring
NOT suitable for big infostrings
===================
*/
void
Info_Tokenize(const qchar *s)
{
  static qchar tokenBuffer[MAX_INFO_STRING];
  qchar *o = tokenBuffer;

  info_tokens = 0;
  *o = '\0';

  for(;;)
  {
    while(*s == '\\') //skip leading/trailing separators
    {
      s++;
    }

    if (*s == '\0')
    {
      break;
    }

    info_keys[info_tokens] = o;

    while(*s != '\\')
    {
      if (*s == '\0')
      {
        *o = '\0'; //terminate key
        info_values[info_tokens] = o;
        info_tokens++;
        return;
      }

      *o++ = *s++;
    }

    *o++ = '\0'; //terminate key
    s++; //skip '\\'
    info_values[info_tokens] = o;
    info_tokens++;

    while(*s != '\\' && *s != '\0')
    {
      *o++ = *s++;
    }

    *o++ = '\0';
  }
}

/*
===================
Info_ValueForKeyToken

Fast lookup from tokenized infostring
===================
*/
const qchar *
Info_ValueForKeyToken(const qchar *key)
{
  unsigned i;

  for(i = 0;i < info_tokens;i++)
  {
    if (!Q_stricmp(info_keys[i], key))
    {
      return info_values[i];
    }
  }

  return "";
}

/*
===================
Info_NextPair

Used to itterate through all the key/value pairs in an info string
===================
*/
const qchar *
Info_NextPair(const qchar *s, qchar *key, qchar *value)
{
  qchar *o;

  if (*s == '\\')
  {
    s++;
  }

  key[0] = '\0';
  value[0] = '\0';
  o = key;

  while(*s != '\\')
  {
    if (!*s)
    {
      *o = '\0';
      return s;
    }

    *o++ = *s++;
  }

  *o = '\0';
  s++;
  o = value;

  while(*s != '\\' && *s)
  {
    *o++ = *s++;
  }

  *o = '\0';

  return s;
}


/*
===================
Info_RemoveKey
===================
*/
unsigned
Info_RemoveKey(qchar *s, const qchar *key)
{
  qchar *start;
  const qchar *pkey;
  unsigned key_len;
  unsigned len;
  unsigned ret;

  key_len = (qint)strlen(key);
  ret = 0;

  while(1)
  {
    start = s;

    if (*s == '\\')
    {
      ++s;
    }

    pkey = s;

    while(*s != '\\')
    {
      if (*s == '\0')
      {
        if (s != start)
        {
          //remove any trailing empty keys
          *start=  '\0';
          ret += (unsigned)(s - start);
        }

        return ret;
      }

      ++s;
    }

    len = (unsigned)(s - pkey);
    ++s; //skip '\\'

    while(*s != '\\' && *s != '\0')
    {
      ++s;
    }

    if (len == key_len && Q_strkey(pkey, key, key_len))
    {
      memmove(start, s, strlen(s) + 1); //remove this part
      ret += (unsigned)(s - start);
      s = start;
    }

    if (*s == '\0')
    {
      break;
    }
  }

  return ret;
}

/*
==================
Info_Validate

Some characters are illegal in info strings because they
can mess up the server's parsing
==================
*/
qbool
Info_Validate(const qchar *s)
{
  for(;;)
  {
    switch(*s++)
    {
      case
      '\0':
        return qtrue;

      case
      '\"':

      case
      ';':
        return qfalse;

      default:
        break;
    }
  }
}

/*
==================
Info_ValidateKeyValue

Some characters are illegal in key values because they
can mess up the server's parsing
==================
*/
qbool
Info_ValidateKeyValue(const qchar *s)
{
  for(;;)
  {
    switch(*s++)
    {
      case
      '\0':
        return qtrue;

      case
      '\\':

      case
      '\"':

      case
      ';':
        return qfalse;

      default:
        break;
    }
  }
}

/*
==================
Info_SetValueForKey_s

Changes or adds a key/value pair
==================
*/
qbool
Info_SetValueForKey_s(qchar *s, unsigned slen, const qchar *key, const qchar *value)
{
  qchar newi[BIG_INFO_STRING + 2];
  unsigned len1;
  unsigned len2;

  len1 = (unsigned)strlen(s);

  if (len1 >= slen)
  {
    Com_Printf(S_COLOR_YELLOW "Info_SetValueForKey(%s): oversize infostring\n", key);
    return qfalse;
  }

  if (!key || !Info_ValidateKeyValue(key) || *key == '\0')
  {
    Com_Printf(S_COLOR_YELLOW "Invalid key name: '%s'\n", key);
    return qfalse;
  }

  if (value && !Info_ValidateKeyValue(value))
  {
    Com_Printf(S_COLOR_YELLOW "Invalid value name: '%s'\n", value);
    return qfalse;
  }

  len1 -= Info_RemoveKey(s, key);

  if (value == NULL || *value == '\0')
  {
    return qtrue;
  }

  len2 = Com_sprintf(newi, sizeof(newi), "\\%s\\%s", key, value);

  if (len1 + len2 >= slen)
  {
    Com_Printf(S_COLOR_YELLOW "Info string length exceeded for key '%s'\n", key);
    return qfalse;
  }

  strcpy(s + len1, newi);
  return qtrue;
}

//====================================================================

/*
==================
Com_CharIsOneOfCharset
==================
*/
static qbool
Com_CharIsOneOfCharset(qchar c, const qchar *set)
{
  unsigned i;

  for(i = 0;i < (unsigned)(strlen(set));i++)
  {
    if (set[i] == c)
    {
      return qtrue;
    }
  }

  return qfalse;
}

/*
==================
Com_SkipCharset
==================
*/
const qchar *
Com_SkipCharset(const qchar *s, const qchar *sep)
{
  const qchar *p = s;

  while(p)
  {
    if (Com_CharIsOneOfCharset(*p, sep))
    {
      p++;
    }
    else
    {
      break;
    }
  }

  return p;
}

/*
==================
Com_SkipTokens
==================
*/
const qchar *
Com_SkipTokens(const qchar *s, unsigned numTokens, const qchar *sep)
{
  unsigned sepCount;
  const qchar *p = s;

  sepCount = 0;

  while(sepCount < numTokens)
  {
    if (Com_CharIsOneOfCharset(*p++, sep))
    {
      sepCount++;

      while(Com_CharIsOneOfCharset(*p, sep))
      {
        p++;
      }
    }
    else if (*p == '\0')
    {
      break;
    }
  }

  if (sepCount == numTokens)
  {
    return p;
  }
  else
  {
    return s;
  }
}
