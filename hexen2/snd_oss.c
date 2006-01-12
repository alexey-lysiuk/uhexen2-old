/*
	snd_oss.c
	$Id: snd_oss.c,v 1.17 2006-01-12 13:10:49 sezero Exp $

	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/

#define _SND_SYS_MACROS_ONLY

#include "quakedef.h"
#include "snd_sys.h"

#if defined(HAVE_OSS_SOUND)

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/wait.h>
// FIXME: <sys/soundcard.h> is "by the book" but we should take care of
// <soundcard.h>, <linux/soundcard.h> and <machine/soundcard.h> someday.
#include <sys/soundcard.h>
#include <errno.h>

int audio_fd = -1;
int snd_inited;
static char *ossdev = "/dev/dsp";
extern int desired_bits, desired_speed, desired_channels;
extern int tryrates[MAX_TRYRATES];
unsigned long mmaplen;

qboolean S_OSS_Init(void)
{
	int i, caps, rc, tmp;
	int retries = 3;
	unsigned long sz;
	struct audio_buf_info info;

	snd_inited = 0;

// open /dev/dsp, confirm capability to mmap, and get size of dma buffer
	if ((tmp = COM_CheckParm("-ossdev")) != 0)
		ossdev = com_argv[tmp+1];
	Con_Printf ("Using OSS device %s\n", ossdev);

	audio_fd = open(ossdev, O_RDWR|O_NONBLOCK);
	if (audio_fd < 0)
	{	// Failed open, retry up to 3 times if it's busy
		while ((audio_fd < 0) && retries-- &&
			((errno == EAGAIN) || (errno == EBUSY)))
		{
			sleep (1);
			audio_fd = open(ossdev, O_RDWR|O_NONBLOCK);
		}
		if (audio_fd < 0)
		{
			perror(ossdev);
			Con_Printf("Could not open %s\n", ossdev);
			return 0;
		}
	}

	rc = ioctl(audio_fd, SNDCTL_DSP_RESET, 0);
	if (rc < 0)
	{
		perror(ossdev);
		Con_Printf("Could not reset %s\n", ossdev);
		close(audio_fd);
		return 0;
	}

	if (ioctl(audio_fd, SNDCTL_DSP_GETCAPS, &caps)==-1)
	{
		perror(ossdev);
		Con_Printf("Couldn't retrieve soundcard capabilities\n");
		close(audio_fd);
		return 0;
	}

	if (!(caps & DSP_CAP_TRIGGER) || !(caps & DSP_CAP_MMAP))
	{
		Con_Printf("Audio driver doesn't support mmap or trigger\n");
		close(audio_fd);
		return 0;
	}

	if (ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &info)==-1)
	{
		perror("GETOSPACE");
		Con_Printf("Couldn't retrieve buffer status\n");
		close(audio_fd);
		return 0;
	}

	shm = &sn;

	shm->splitbuffer = 0;

// set sample bits & speed
	i = desired_bits;
	tmp = (desired_bits == 16) ? AFMT_S16_LE : AFMT_U8;
	rc = ioctl(audio_fd, SNDCTL_DSP_SETFMT, &tmp);
	if (rc < 0)
	{
		Con_Printf("Could not support %d-bit data, retrying..\n", desired_bits);
		// try what the device gives us
		ioctl(audio_fd, SNDCTL_DSP_GETFMTS, &tmp);
		if (tmp & AFMT_S16_LE)
		{
			i = 16;
			tmp = AFMT_S16_LE;
		}
		else if (tmp & AFMT_U8)
		{
			i = 8;
			tmp = AFMT_U8;
		}
		else
		{
			perror(ossdev);
			Con_Printf("Could not retrieve supported sound formats!..\n");
			close(audio_fd);
			return 0;
		}
		rc = ioctl(audio_fd, SNDCTL_DSP_SETFMT, &tmp);
		if (rc < 0)
		{
			perror(ossdev);
			Con_Printf("No supported sound formats!..\n");
			close(audio_fd);
			return 0;
		}
	}
	shm->samplebits = i;

	tmp = desired_speed;
	rc = ioctl(audio_fd, SNDCTL_DSP_SPEED, &tmp);
	if (rc < 0)
	{
		Con_Printf("Problems setting dsp speed, trying alternatives..\n");
		shm->speed = 0;
		for (i=0 ; i<MAX_TRYRATES ; i++)
		{
			tmp = tryrates[i];
			rc= ioctl(audio_fd, SNDCTL_DSP_SPEED, &tmp);
			if (rc < 0)
			{
				Con_DPrintf ("Could not set dsp to speed %d\n", tryrates[i]);
			}
			else
			{
				if (tmp != tryrates[i])
				{
					Con_Printf ("Warning: Rate set (%d) didn't match requested rate (%d)!\n", tmp, tryrates[i]);
				//	close(audio_fd);
				//	return 0;
				}
				shm->speed = tmp;
				break;
			}
		}
		if (shm->speed == 0)
		{
			Con_Printf("Could not set %s speed!\n", ossdev);
			close(audio_fd);
			return 0;
		}
	}
	else
	{
		if (tmp != desired_speed)
		{
			Con_Printf ("Warning: Rate set (%d) didn't match requested rate (%d)!\n", tmp, desired_speed);
		//	close(audio_fd);
		//	return 0;
		}
		shm->speed = tmp;
	}

	tmp = (desired_channels == 2) ? 1 : 0;
	rc = ioctl(audio_fd, SNDCTL_DSP_STEREO, &tmp);
	if (rc < 0)
	{
		Con_Printf("Problems setting mono/stereo, retrying..\n");
		tmp = (desired_channels == 2) ? 0 : 1;
		rc = ioctl(audio_fd, SNDCTL_DSP_STEREO, &tmp);
		if (rc < 0)
		{
			perror(ossdev);
			Con_Printf("Could not set dsp to mono or stereo\n");
			close(audio_fd);
			return 0;
		}
	}
	shm->channels = tmp +1;

	shm->samples = info.fragstotal * info.fragsize / (shm->samplebits/8);
	shm->submission_chunk = 1;

// memory map the dma buffer
	sz = sysconf (_SC_PAGESIZE);
	mmaplen = info.fragstotal * info.fragsize;
	mmaplen = (mmaplen + sz - 1) & ~(sz - 1);
	shm->buffer = (unsigned char *) mmap(NULL, mmaplen, PROT_READ|PROT_WRITE,
					     MAP_FILE|MAP_SHARED, audio_fd, 0);
	if (!shm->buffer || shm->buffer == MAP_FAILED)
	{
		perror(ossdev);
		Con_Printf("Could not mmap %s\n", ossdev);
		close(audio_fd);
		return 0;
	}

// toggle the trigger & start her up

	tmp = 0;
	rc  = ioctl(audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
	if (rc < 0)
	{
		perror(ossdev);
		Con_Printf("Could not toggle %s\n", ossdev);
		munmap (shm->buffer, mmaplen);
		close(audio_fd);
		return 0;
	}
	tmp = PCM_ENABLE_OUTPUT;
	rc = ioctl(audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
	if (rc < 0)
	{
		perror(ossdev);
		Con_Printf("Could not toggle %s\n", ossdev);
		munmap (shm->buffer, mmaplen);
		close(audio_fd);
		return 0;
	}

	shm->samplepos = 0;

	snd_inited = 1;
	Con_Printf("Audio Subsystem initialized in OSS mode.\n");
	return 1;
}

int S_OSS_GetDMAPos(void)
{
	struct count_info count;

	if (!snd_inited)
		return 0;

	if (ioctl(audio_fd, SNDCTL_DSP_GETOPTR, &count)==-1)
	{
		perror(ossdev);
		Con_Printf("Uh, sound dead.\n");
		munmap (shm->buffer, mmaplen);
		close(audio_fd);
		audio_fd = -1;
		snd_inited = 0;
		return 0;
	}
//	shm->samplepos = (count.bytes / (shm->samplebits / 8)) & (shm->samples-1);
//	fprintf(stderr, "%d    \r", count.ptr);
	shm->samplepos = count.ptr / (shm->samplebits / 8);

	return shm->samplepos;
}

void S_OSS_Shutdown(void)
{
	int tmp = 0;
	if (snd_inited)
	{
		Con_Printf ("Shutting down OSS sound\n");
		snd_inited = 0;
		munmap (shm->buffer, mmaplen);
		ioctl(audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
		ioctl(audio_fd, SNDCTL_DSP_RESET, 0);
		close(audio_fd);
		audio_fd = -1;
	}
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void S_OSS_Submit(void)
{
}

#endif	// HAVE_OSS_SOUND

