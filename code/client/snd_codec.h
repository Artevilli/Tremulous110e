/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2005 Stuart Dalton (badcdev@gmail.com)

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

#ifndef _SND_CODEC_H_
#define _SND_CODEC_H_

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

typedef struct snd_info_s
{
	qint rate;
	qint width;
	qint channels;
	qint samples;
	qint size;
	qint dataofs;
} snd_info_t;

typedef struct snd_codec_s snd_codec_t;

typedef struct snd_stream_s
{
	snd_codec_t *codec;
	fileHandle_t file;
	snd_info_t info;
	qint length;
	qint pos;
	void *ptr;
} snd_stream_t;

// Codec functions
typedef void *(*CODEC_LOAD)(const qchar *filename, snd_info_t *info);
typedef snd_stream_t *(*CODEC_OPEN)(const qchar *filename);
typedef qint (*CODEC_READ)(snd_stream_t *stream, qint bytes, void *buffer);
typedef void (*CODEC_CLOSE)(snd_stream_t *stream);

// Codec data structure
struct snd_codec_s
{
	const qchar *ext;
	CODEC_LOAD load;
	CODEC_OPEN open;
	CODEC_READ read;
	CODEC_CLOSE close;
	snd_codec_t *next;
};

// Codec management
void S_CodecInit( void );
void S_CodecShutdown( void );
void *S_CodecLoad(const qchar *filename, snd_info_t *info);
snd_stream_t *S_CodecOpenStream(const qchar *filename);
void S_CodecCloseStream(snd_stream_t *stream);
qint S_CodecReadStream(snd_stream_t *stream, qint bytes, void *buffer);

// Util functions (used by codecs)
snd_stream_t *S_CodecUtilOpen(const qchar *filename, snd_codec_t *codec);
void S_CodecUtilClose(snd_stream_t **stream);

// WAV Codec
extern snd_codec_t wav_codec;
void *S_WAV_CodecLoad(const qchar *filename, snd_info_t *info);
snd_stream_t *S_WAV_CodecOpenStream(const qchar *filename);
void S_WAV_CodecCloseStream(snd_stream_t *stream);
qint S_WAV_CodecReadStream(snd_stream_t *stream, qint bytes, void *buffer);

// Ogg Vorbis codec
#ifdef USE_OGG_VORBIS
extern snd_codec_t ogg_codec;
void *S_OGG_CodecLoad(const qchar *filename, snd_info_t *info);
snd_stream_t *S_OGG_CodecOpenStream(const qchar *filename);
void S_OGG_CodecCloseStream(snd_stream_t *stream);
qint S_OGG_CodecReadStream(snd_stream_t *stream, qint bytes, void *buffer);
#endif // USE_OGG_VORBIS

#endif // !_SND_CODEC_H_
