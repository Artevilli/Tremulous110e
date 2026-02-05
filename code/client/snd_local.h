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
// snd_local.h -- private sound definitions


#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "snd_public.h"

#define	PAINTBUFFER_SIZE		4096					// this is in samples

#define SND_CHUNK_SIZE			1024					// samples
#define SND_CHUNK_SIZE_FLOAT	(SND_CHUNK_SIZE/2)		// floats
#define SND_CHUNK_SIZE_BYTE		(SND_CHUNK_SIZE*2)		// floats

typedef struct {
	qint			left;	// the final values will be clamped to +/- 0x00ffff00 and shifted down
	qint			right;
} portable_samplepair_t;

typedef struct adpcm_state {
	short	sample;		/* Previous output value */
	qchar	index;		/* Index into stepsize table */
} adpcm_state_t;

typedef	struct sndBuffer_s {
	short					sndChunk[SND_CHUNK_SIZE];
	struct sndBuffer_s		*next;
	qint						size;
	adpcm_state_t			adpcm;
} sndBuffer;

typedef struct sfx_s {
	sndBuffer		*soundData;
	qbool		defaultSound;			// couldn't be loaded, so use buzz
	qbool		inMemory;				// not in Memory
	qbool		soundCompressed;		// not in Memory
	qint				soundCompressionMethod;
	qint 			soundLength;
	qint				soundChannels;
	qchar 			soundName[MAX_QPATH];
	qint				lastTimeUsed;
	qint				duration;
	struct sfx_s	*next;
} sfx_t;

typedef struct {
	unsigned qint channels;
	unsigned qint samples;				// mono samples in buffer
	qint			fullsamples;			// samples with all channels in buffer (samples divided by channels)
	qint			submission_chunk;		// don't mix less than this #
	qint			samplebits;
	qint			isfloat;
	qint			speed;
	byte		*buffer;
	const qchar	*driver;
} dma_t;

extern byte *dma_buffer2;

#define START_SAMPLE_IMMEDIATE	0x7fffffff

#define MAX_DOPPLER_SCALE 50.0f //arbitrary

typedef struct loopSound_s {
	vec3_t		origin;
	vec3_t		velocity;
	sfx_t		*sfx;
	qint			mergeFrame;
	qbool	active;
	qbool	kill;
	qbool	doppler;
	float		dopplerScale;
	float		oldDopplerScale;
	qint			framenum;
} loopSound_t;

typedef struct
{
	qint			allocTime;
	qint			startSample;	// START_SAMPLE_IMMEDIATE = set immediately on next mix
	qint			entnum;			// to allow overriding a specific sound
	qint			entchannel;		// to allow overriding a specific sound
	qint			leftvol;		// 0-255 volume after spatialization
	qint			rightvol;		// 0-255 volume after spatialization
	qint			master_vol;		// 0-255 volume before spatialization
	float		dopplerScale;
	float		oldDopplerScale;
	vec3_t		origin;			// only use if fixed_origin is set
	qbool	fixed_origin;	// use origin instead of fetching entnum's origin
	sfx_t		*thesfx;		// sfx structure
	qbool	doppler;
} channel_t;


#define WAV_FORMAT_PCM			0x0001
#define WAVE_FORMAT_IEEE_FLOAT	0x0003

typedef struct {
	qint			format;
	qint			rate;
	qint			width;
	qint			channels;
	qint			samples;
	qint			dataofs;		// chunk starts this many bytes from file start
} wavinfo_t;

// Interface between Q3 sound "api" and the sound backend
typedef struct
{
	void (*Shutdown)(void);
	void (*StartSound)( const vec3_t origin, qint entnum, qint entchannel, sfxHandle_t sfx );
	void (*StartLocalSound)( sfxHandle_t sfx, qint channelNum );
	void (*StartBackgroundTrack)( const qchar *intro, const qchar *loop );
	void (*StopBackgroundTrack)( void );
	void (*RawSamples)(qint samples, qint rate, qint width, qint channels, const byte *data, float volume);
	void (*StopAllSounds)( void );
	void (*ClearLoopingSounds)( qbool killall );
	void (*AddLoopingSound)( qint entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
	void (*AddRealLoopingSound)( qint entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
	void (*StopLoopingSound)(qint entityNum );
	void (*Respatialize)( qint entityNum, const vec3_t origin, vec3_t axis[3], qint inwater );
	void (*UpdateEntityPosition)( qint entityNum, const vec3_t origin );
	void (*Update)( qint msec );
	void (*DisableSounds)( void );
	void (*BeginRegistration)( void );
	sfxHandle_t (*RegisterSound)( const qchar *sample, qbool compressed );
	qint  (*SoundDuration)( sfxHandle_t handle );
	void (*ClearSoundBuffer)( void );
	void (*SoundInfo)( void );
	void (*SoundList)( void );
} soundInterface_t;


/*
====================================================================

  SYSTEM SPECIFIC FUNCTIONS

====================================================================
*/

// initializes cycling through a DMA buffer and returns information on it
qbool SNDDMA_Init(void);

// gets the current DMA position
qint		SNDDMA_GetDMAPos(void);

// shutdown the DMA xfer.
void	SNDDMA_Shutdown(void);

void	SNDDMA_BeginPainting (void);

void	SNDDMA_Submit(void);

//====================================================================

#define	MAX_CHANNELS			96

extern	channel_t   s_channels[MAX_CHANNELS];
extern	channel_t   loop_channels[MAX_CHANNELS];
extern	qint		numLoopChannels;

extern	qint		s_soundtime;
extern	qint		s_paintedtime;
extern	qint		s_rawend;
extern	vec3_t	listener_forward;
extern	vec3_t	listener_right;
extern	vec3_t	listener_up;
extern	dma_t	dma;

#define	MAX_RAW_SAMPLES	16384
extern	portable_samplepair_t	s_rawsamples[MAX_RAW_SAMPLES];

extern cvar_t *s_volume;
extern cvar_t *s_musicVolume;
extern cvar_t *s_doppler;
extern cvar_t *s_muteWhenUnfocused;
extern cvar_t *s_muteWhenMinimized;

extern cvar_t *s_testsound;

qbool S_LoadSound( sfx_t *sfx );

void		SND_free(sndBuffer *v);
sndBuffer*	SND_malloc( void );
void		SND_setup( void );
void		SND_shutdown( void );

void S_PaintChannels(qint endtime);

// spatializes a channel
void S_Spatialize(channel_t *ch);

// adpcm functions
qint  S_AdpcmMemoryNeeded( const wavinfo_t *info );
void S_AdpcmEncodeSound( sfx_t *sfx, short *samples );
void S_AdpcmGetSamples(sndBuffer *chunk, short *to);

// wavelet function

#define SENTINEL_MULAW_ZERO_RUN 127
#define SENTINEL_MULAW_FOUR_BIT_RUN 126

void S_FreeOldestSound( void );

#define	NXStream byte

void encodeWavelet(sfx_t *sfx, short *packets);
void decodeWavelet( sndBuffer *stream, short *packets);

void encodeMuLaw( sfx_t *sfx, short *packets);
extern short mulawToShort[256];

extern short *sfxScratchBuffer;
extern sfx_t *sfxScratchPointer;
extern qint	   sfxScratchIndex;

qbool S_Base_Init( soundInterface_t *si );
