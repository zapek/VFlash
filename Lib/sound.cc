/////////////////////////////////////////////////////////////
// Flash Plugin and Player
// Copyright (C) 1998,1999 Olivier Debon
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
///////////////////////////////////////////////////////////////
//  Author : Olivier Debon  <odebon@club-internet.fr>
// $Id: sound.cc,v 1.7 2004/11/30 03:45:02 henes Exp $

#define BUFFERSIZE 16384
//#define BUFFERSIZE 8192

#undef GLOBAL
extern "C"
{
#include "../plugin/flash_mcc_private.h"
#include <libraries/asound.h>
#include <proto/asound.h>
	void dprintf(char *, ...);
};
#include "../debug.h"

#include "swf.h"

#include <unistd.h>
#include <fcntl.h>
#ifndef MBX
#include <sys/ioctl.h>
#endif /* MBX */

extern "C"
{
	extern void Fail(STRPTR);
	extern struct FlashPrefs *vfp;
};

#ifdef RCSID
static char *rcsid = "$Id: sound.cc,v 1.7 2004/11/30 03:45:02 henes Exp $";
#endif

#define PRINT 0

struct Library *ASoundBase;

//////////// SOUND

Sound::Sound(long id) : Character(SoundType, id)
{
	samples = 0;
	stereo = 0;
	soundRate = 0;
	sampleSize = 1;
}

Sound::~Sound()
{
	if (samples)
	{
		delete samples;
	}
}

void
Sound::setSoundFlags(long f)
{
	switch (GET_SOUND_RATE_CODE(f))
	{
	case 0:
		soundRate = 5500;
		break;
	case 1:
		soundRate = 11025;
		break;
	case 2:
		soundRate = 22050;
		break;
	case 3:
		soundRate = 44100;
		break;
	}
	if (f & soundIs16bit)
	{
		sampleSize = 2;
	}
	if (f & soundIsStereo)
	{
		stereo = 1;
	}

#define DEBUG 0
#if (DEBUG == 1)
	#define DB(x)   { dprintf x ; }
	DB(("-----\nFlags = %2x\n", f));
	DB(("Rate = %d kHz  ", soundRate));
	DB(("SampleSize = %d byte(s) ", sampleSize));
	if (f & soundIsStereo)
	{
		DB(("Stereo  "));
	}
	else
	{
		DB(("Mono  "));
	}
	if (f & soundIsADPCMCompressed)
	{
		DB(("ADPCM\n"));
	}
	else if (f & soundIsMP3Compressed)
	{
		DB(("MP3\n"));
	}
	else
	{
		DB(("Raw\n"));
	}
#endif
#define DEBUG 0
#define DB(x)
}

/* zapek: added reference argument for buffer overflow */
char *
Sound::setNbSamples(long n, long *size)
{
	if (samples)
	{
		delete samples;
	}
	nbSamples = n;

	*size = nbSamples * (stereo ? 2 : 1) * sampleSize;
	
	samples = new char[ *size ];

	memset((char *)samples,0, *size);

	return samples;
}

long
Sound::getRate()
{
	return soundRate;
}

long
Sound::getChannel()
{
	return stereo ? 2 : 1;
}

long
Sound::getNbSamples()
{
	return nbSamples;
}

long
Sound::getSampleSize()
{
	return sampleSize;
}

char *
Sound::getSamples()
{
	return samples;
}

//////////// SOUND MIXER

long  SoundMixer::dsp = -1;	// Init of descriptor
void *SoundMixer::ctx = 0;
long  SoundMixer::blockSize = 0;	// Driver sound buffer size
long  SoundMixer::nbInst = 0;	// Nb SoundMixer instances
long  SoundMixer::sampleSize = 0;
long  SoundMixer::stereo = 0;
long  SoundMixer::soundRate = 0;
char *SoundMixer::buffer[2] = {0, 0};

SoundMixer::SoundMixer(struct FlashPrefs *vfp)
{
#ifndef NOSOUND
	int status;
	long fmt;

	list = 0;	// No sound to play

	if (nbInst++ || !vfp->vfp_soundswitch)
	{
		/*
		 * Device already opened or no sound mode
		 */
		return;
	}

	if (vfp->vfp_soundswitch == VSS_DOSDRIVER)
	{
		dsp = open(vfp->vfp_dosdriver.name, O_WRONLY);
		if (dsp < 0)
		{
			//Fail("Couldn't open sound driver\n");
			//perror("open dsp");
			return;
		}
	}
	else if (vfp->vfp_soundswitch == VSS_AMBIENT)
	{
		if (!(ASoundBase = OpenLibrary("ambient_sound.library", 1)))
		{
			//Fail("Couldn't open ambient_sound.library\n");
			return;
		}
	}
	else
	{
		return; //TOFIX!!! when the other sound modes are implemented..
	}

	// Set sample size
	/*
	 * See how hard it would be to make it mix in big endian, the
	 * mode all computers should use (and Intel must die)
	 */
	#if 0
	sampleSize = vfp->vfp_resolution + 1;

	// Set stereo channel
	stereo = vfp->vfp_stereo;

	switch (vfp->vfp_samplingrate)
	{
	case 0:
		soundRate = 5500;
		break;
	case 1:
		soundRate = 11025;
		break;
	case 2:
		soundRate = 22050;
		break;
	case 3:
		soundRate = 44100;
		break;
	}
	#else
	/*
	 * Defaults.
	 */
	sampleSize = 2; /* 16-bits */
	stereo = 1; /* stereo */
	soundRate = 11025;
	#endif

	// Get device buffer size
	//status = ioctl(dsp, SNDCTL_DSP_GETBLKSIZE, &blockSize);
	//if (status < 0) perror("ioctl SNDCTL_DSP_GETBLKSIZE");
	//if (blockSize < 1024) {
	//	  blockSize = 32768;
	//}
	blockSize = BUFFERSIZE; // make that settable or play around...
	blockSize *= 2;

	buffer[0] = (char *)malloc(blockSize);
	buffer[1] = (char *)malloc(blockSize);
	if (buffer[0] == 0 || buffer[1] == 0)
	{
		if (buffer[0]) free(buffer[0]);
		if (buffer[1]) free(buffer[1]);

		if (vfp->vfp_soundswitch == VSS_DOSDRIVER)
		{
			close(dsp);
			dsp = -1;
		}
		else if (vfp->vfp_soundswitch == VSS_AMBIENT)
		{
			CloseLibrary(ASoundBase);
			return;
		}
		else
		{
			return;
		}
	}

	if (vfp->vfp_soundswitch == VSS_AMBIENT)
	{
		/* XXX: Emm must die! impossible to get those stub to end in libaboxstubs.a */
#if 1
		struct TagItem tags[5];

		tags[0].ti_Tag = SOUNDTAG_Channels;
		tags[0].ti_Data = stereo ? 2 : 1;
		tags[1].ti_Tag = SOUNDTAG_Resolution;
		tags[1].ti_Data = (sampleSize == 2) ? SOUNDVAL_Resolution_16 : SOUNDVAL_Resolution_8;
		tags[2].ti_Tag = SOUNDTAG_Frequency;
		tags[2].ti_Data = soundRate;
		tags[3].ti_Tag = SOUNDTAG_Volume;
		tags[3].ti_Data = vfp->vfp_audio.volume * 1024;
		tags[4].ti_Tag = TAG_DONE;

		ctx = ASound_Create(SOUNDMODE_NORMAL, tags);

#else
		ctx	= ASound_CreateTags(SOUNDMODE_NORMAL,
		                        etc.. pp..
		                        TAG_DONE);
#endif

		if (!ctx)
		{
			CloseLibrary(ASoundBase);
		}
	}

#if (DEBUG == 1)
	DB(("Sound Rate  = %d\n", soundRate));
	DB(("Stereo      = %d\n", stereo));
	DB(("Sample Size = %d\n", sampleSize));
	DB(("Buffer Size = %d\n", blockSize));
#endif /* PRINT */

#endif	/* NOSOUND */
}

SoundMixer::~SoundMixer()
{
	if (--nbInst == 0)
	{
		if (dsp > 0)
		{
			if (vfp->vfp_soundswitch == VSS_DOSDRIVER)
			{
				close(dsp);
			}
		}
		if (ctx)
		{
			ASound_Delete(ctx);
			CloseLibrary(ASoundBase);
		}
		free(buffer[0]);
		free(buffer[1]);
	}
}

void
SoundMixer::stopSounds()
{
#ifndef NOSOUND
	SoundList *sl,*del;

	for(sl = list; sl; )
	{
		del = sl;
		sl = sl->next;
		delete del;
	}
	list = 0;
#endif
}

void
SoundMixer::startSound(Sound *sound, long loopcnt)
{
#ifndef NOSOUND
	SoundList *sl;

	if (sound)
	{
		// Add sound in list
		sl = new SoundList;
		sl->rate = sound->getRate();
		sl->stereo = (sound->getChannel() == 2);
		sl->sampleSize = sound->getSampleSize();
		sl->current = sound->getSamples();
		sl->start = sl->current;
		sl->remaining = sound->getSampleSize()*sound->getNbSamples()*sound->getChannel();
		sl->total = sl->remaining;
		sl->next = list;
		sl->loop = loopcnt;
		//dprintf("loops: %ld\n", loopcnt);
		list = sl;
	}
#endif
}

static long bufnum = 0;

long
SoundMixer::playSounds()
{
#ifndef NOSOUND
	long		 nbBytes, n;
	SoundList	*sl,*prev;
	int		 status;

	// Init failed
	if (vfp->vfp_soundswitch == VSS_DOSDRIVER)
	{
		if (dsp < 0) return 0; //TOFIX !!
	}


	if (vfp->vfp_soundswitch == VSS_AMBIENT)
	{
		if (!ctx) return 0;
	}

	// No sound to play
	if (list == 0) return 0;

	// Get free DMA buffer space
	//status = ioctl(dsp, SNDCTL_DSP_GETOSPACE, &bufInfo);

	// Free space is not large enough to output data without blocking
	// But there are still sounds to play. We must wait.
	//if (bufInfo.bytes < blockSize) return 1;
	if (vfp->vfp_soundswitch == VSS_AMBIENT)
	{
		/* XXX: that's the buffer size of the playing buffer.. not overall.. sucks somehow */
		status = (int)ASound_GetAttr(ctx, SOUNDATTR_BufferFillState);

		//if (status > (blockSize / 2)) return (1);
		if (status > 2048) return (1); /* XXX: that trick sucks though.. */
	}

	nbBytes = 0;

	// Fill buffer with silence.
	memset((void*)buffer[bufnum], 0, blockSize);

	prev = 0;
	sl = list;
	while(sl)
	{

		// Ask sound to fill the buffer
		// according to device capabilities
		n = fillSoundBuffer(sl, buffer[bufnum], blockSize);

		// Remember the largest written size
		if (n > nbBytes)
		{
			nbBytes = n;
		}

		// No more samples for this sound
		if (sl->remaining == 0)
		{
			// Remove sound from list
			if (prev)
			{
				prev->next = sl->next;
				delete sl;
				sl = prev->next;
			}
			else
			{
				list = sl->next;
				delete sl;
				sl = list;
			}
		}
		else
		{
			sl = sl->next;
		}
	}

	if (nbBytes)
	{
		// At last ! Play It !
		if (vfp->vfp_soundswitch == VSS_DOSDRIVER)
		{
			write(dsp,buffer[bufnum],nbBytes); //TOFIX!!
		}
		else if (vfp->vfp_soundswitch == VSS_AMBIENT)
		{
			ASound_PlayAsync(ctx, buffer[bufnum], nbBytes);
		}

		//status = ioctl(dsp, SNDCTL_DSP_POST);
	}

	bufnum ^= 1;

	return nbBytes;
#else
return 0;
#endif
}

#define USE_LOOPS 0

long
SoundMixer::fillSoundBuffer(SoundList *sl, char *buff, long buffSize)
{
	long sampleLeft, sampleRight;
	long skipOut, skipOutInit;
	long skipIn, skipInInit;
	long freqRatio;
	long totalOut = 0;

	sampleLeft = sampleRight = 0;
	skipOutInit = skipInInit = 0;

	freqRatio = sl->rate / soundRate;
	if (freqRatio)
	{
		skipOutInit = freqRatio - 1;
		skipInInit = 0;
	}

	freqRatio = soundRate / sl->rate;
	if (freqRatio)
	{
		skipInInit = freqRatio - 1;
		skipOutInit = 0;
	}

	skipOut = skipOutInit;
	skipIn = skipInInit;
	
	while (buffSize && sl->remaining)
	{
		if (skipIn-- == 0)
		{
			// Get sampleLeft
			if (sl->sampleSize == 2)
			{
				sampleLeft = (long)(*(short *)(sl->current));
				if (sampleSize == 1)
				{
					sampleLeft = (sampleLeft >> 8) &0xff;
				}
			}
			else
			{
				sampleLeft = (long)*(sl->current);
				if (sampleSize == 2)
				{
					sampleLeft <<= 8;
				}
			}
			sl->current += sl->sampleSize;
			sl->remaining -= sl->sampleSize;

			if (sl->stereo)
			{
				// Get sampleRight
				if (sl->sampleSize == 2)
				{
					sampleRight = (long)(*(short *)(sl->current));
					if (sampleSize == 1)
					{
						sampleRight = (sampleRight >> 8) &0xff;
					}
				}
				else
				{
					sampleRight = (long)*(sl->current);
					if (sampleSize == 2)
					{
						sampleRight <<= 8;
					}
				}
				sl->current += sl->sampleSize;
				sl->remaining -= sl->sampleSize;

			}
			else
			{
				sampleRight = sampleLeft;
			}

			skipIn = skipInInit;
		}

		if (skipOut-- == 0)
		{
			// Output
			if (stereo)
			{
				if (sampleSize == 2)
				{
					*((short *)buff) += sampleLeft/2;
					buffSize -= sampleSize;
					buff += sampleSize;
					*((short *)buff) += sampleRight/2;
					buffSize -= sampleSize;
					buff += sampleSize;
				}
				else
				{
					*((char *)buff) += sampleLeft/2;
					buffSize -= sampleSize;
					buff += sampleSize;
					*((char *)buff) += sampleRight/2;
					buffSize -= sampleSize;
					buff += sampleSize;
				}
				totalOut += 2*sampleSize;
			}
			else
			{
				if (sampleSize == 2)
				{
					*((short *)buff) += (sampleLeft+sampleRight)>>2;
					buffSize -= sampleSize;
					buff += sampleSize;
				}
				else
				{
					*((char *)buff) += (sampleLeft+sampleRight)>>2;
					buffSize -= sampleSize;
					buff += sampleSize;
				}
				totalOut += sampleSize;
			}

			skipOut = skipOutInit;
		}
	
		/*
		 * Check if the sound is in looping mode.
		 */
		#if USE_LOOPS
		if (sl->remaining == 0)
		{
			if (sl->loop)
			{
				sl->current = sl->start;
				sl->remaining = sl->total;
				sl->loop--;
			}
		}
		#endif
	}

	return totalOut;
}
