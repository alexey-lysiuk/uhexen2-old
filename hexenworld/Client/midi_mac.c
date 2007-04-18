/*
	midi_mac.c
	$Id: midi_mac.c,v 1.11 2007-04-18 13:33:39 sezero Exp $

	MIDI module for Mac OS X using QuickTime:
	Taken from the macglquake project with adjustments to make
	it work with Mac OS X and Hexen II: Hammer of Thyrion.

	Copyright (C) 2002  contributors of the macglquake project
	Copyright (C) 2006  Levent Yavas

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
		51 Franklin St, Fifth Floor,
		Boston, MA  02110-1301  USA
*/

#include "quakedef.h" 
#include <unistd.h>

#include <Sound.h>
#include "/Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/QuickTime.framework/Versions/A/Headers/Movies.h"
//#include <QuickTime/QuickTime.h>


static Movie midiTrack = NULL;
static qboolean bMidiInited, bPaused, bLooped;
extern cvar_t bgmvolume;
static float bgm_volume_old = -1.0f;

//
// MusicEvents
// Called in the event loop to keep track of MIDI music
//
void MIDI_Update (void)
{
	if (midiTrack)
	{
		// pOx - adjust volume if changed
		if (bgm_volume_old != bgmvolume.value)
		{
			SetMovieVolume(midiTrack, (short)(bgmvolume.value*256));
			bgm_volume_old = bgmvolume.value;
		}

		// Let QuickTime get some time
		MoviesTask (midiTrack, 0);

		// If this song is looping, restart it
		if (IsMovieDone (midiTrack))
		{
			if (bLooped)
			{
				GoToBeginningOfMovie (midiTrack);
				StartMovie (midiTrack);
			}
			else
			{
				DisposeMovie (midiTrack);
				midiTrack = NULL;
			}
		}
	}
}


static void MIDI_Play_f (void)
{
	if (Cmd_Argc () == 2)
	{
		MIDI_Play(Cmd_Argv(1));
	}
}

static void MIDI_Stop_f (void)
{
	MIDI_Stop();
}

static void MIDI_Pause_f (void)
{
	MIDI_Pause (MIDI_TOGGLE_PAUSE);
}

static void MIDI_Loop_f (void)
{
	if (Cmd_Argc () == 2)
	{
		if (Q_strcasecmp(Cmd_Argv(1),"on") == 0 || Q_strcasecmp(Cmd_Argv(1),"1") == 0)
			MIDI_Loop(1);
		else if (Q_strcasecmp(Cmd_Argv(1),"off") == 0 || Q_strcasecmp(Cmd_Argv(1),"0") == 0)
			MIDI_Loop(0);
		else if (Q_strcasecmp(Cmd_Argv(1),"toggle") == 0)
			MIDI_Loop(2);
	}

	if (bLooped)
		Con_Printf("MIDI music will be looped\n");
	else
		Con_Printf("MIDI music will not be looped\n");
}

qboolean MIDI_Init (void)
{
	OSErr		theErr;

	Con_Printf("%s: ", __FUNCTION__);

	if (safemode || COM_CheckParm("-nomidi") || COM_CheckParm("-nosound") || COM_CheckParm("-s"))
	{
		Con_Printf("disabled by commandline\n");
		bMidiInited = 0;
		return 0;
	}

	theErr = EnterMovies ();
	if (theErr != noErr)
	{
		Con_Printf ("Unable to initialize QuickTime.\n");
		return 0;
	}

	Con_Printf("Started QuickTime midi.\n");

	bPaused = false;
	bLooped = true;
	bMidiInited = true;

	Cmd_AddCommand ("midi_play", MIDI_Play_f);
	Cmd_AddCommand ("midi_stop", MIDI_Stop_f);
	Cmd_AddCommand ("midi_pause", MIDI_Pause_f);
	Cmd_AddCommand ("midi_loop", MIDI_Loop_f);

	return true;
}

void MIDI_Cleanup (void)
{
	if (bMidiInited)
	{
		MIDI_Stop();
		Con_Printf("%s: closing QuickTime.\n", __FUNCTION__);
		ExitMovies ();
		bMidiInited = 0;
	}
}

static int MIDI_ExtractFile (FILE *inFile, const char *Name, size_t size)
{
	FILE		*outFile;
	size_t		remaining, count;
	int		err = 0;
	char		buf[16384];

	outFile = fopen (Name, "wb");
	if (!outFile)
		return -1;
	remaining = size;
	while (remaining)
	{
		if (remaining < sizeof(buf))
			count = remaining;
		else
			count = sizeof(buf);
		fread (buf, 1, count, inFile);
		err = ferror(inFile);
		if (err)
			break;
		fwrite (buf, 1, count, outFile);
		err = ferror(outFile);
		if (err)
			break;
		remaining -= count;
	}

	fclose (outFile);
	return err;
}

#define	TEMP_MUSICNAME	"tmpmusic"

void MIDI_Play (const char *Name)
{
	FILE		*midiFile;
	char	midiName[MAX_OSPATH], tempName[MAX_QPATH];
	OSErr	err;
	FSSpec	midiSpec;
	FSRef	midiRef;
	short	midiRefNum;

	if (!bMidiInited)	//don't try to play if there is no midi
		return;

	MIDI_Stop();

	if (!Name || !*Name)
	{
		Sys_Printf("no midi music to play\n");
		return;
	}

	snprintf (tempName, sizeof(tempName), "%s.%s", Name, "mid");
	FS_OpenFile (va("%s/%s", "midi", tempName), &midiFile, false);
	if (!midiFile)
	{
		Con_Printf("music file %s not found\n", tempName);
		return;
	}
	else
	{
		if (file_from_pak)
		{
			int		ret;

			Con_Printf("Extracting %s from pakfile\n", tempName);
			snprintf (midiName, sizeof(midiName), "%s/%s.%s", host_parms->userdir, TEMP_MUSICNAME, "mid");
			ret = MIDI_ExtractFile (midiFile, midiName, fs_filesize);
			fclose (midiFile);
			if (ret != 0)
			{
				Con_Printf("Error while extracting from pak\n");
				return;
			}
		}
		else	/* use the file directly */
		{
			fclose (midiFile);
			snprintf (midiName, sizeof(midiName), "%s/%s/%s", fs_filepath, "midi", tempName);
		}
	}

	// converting path to FSSpec. found in CarbonCocoaIntegration.pdf:
	// page 27, Obtaining an FSSpec Structure
	err = FSPathMakeRef ((UInt8*)midiName, &midiRef, NULL);
	if (err != noErr)
	{
		Con_Printf ("MIDI: FSPathMakeRef: error while opening %s\n", midiName);
		return;
	}

	err = FSGetCatalogInfo (&midiRef, kFSCatInfoNone, NULL, NULL, &midiSpec, NULL);
	if (err != noErr)
	{
		Con_Printf ("MIDI: FSGetCatalogInfo: error while opening %s\n", midiName);
		return;
	}

	err = OpenMovieFile (&midiSpec, &midiRefNum, fsRdPerm);
	if (err != noErr)
	{
		Con_Printf ("MIDI: OpenMovieStream: error opening midi file\n");
		return;
	}

	err = NewMovieFromFile (&midiTrack, midiRefNum, NULL, NULL, newMovieActive, NULL);
	if (err != noErr || !midiTrack)
	{
		Con_Printf ("MIDI: QuickTime error in creating stream.\n");
		return;
	}

	GoToBeginningOfMovie (midiTrack);
	PrerollMovie (midiTrack, 0, 0);

	// pOx - set initial volume
	SetMovieVolume(midiTrack, (short)(bgmvolume.value*256));
	bgm_volume_old = bgmvolume.value;

	StartMovie (midiTrack);
	Con_Printf ("Started midi music %s\n", tempName);
}

void MIDI_Pause (int mode)
{
	if (!midiTrack)
		return;

	if ((mode == MIDI_TOGGLE_PAUSE && bPaused) || mode == MIDI_ALWAYS_RESUME)
	{
		StartMovie (midiTrack);
		bPaused = false;
	}
	else
	{
		StopMovie (midiTrack);
		bPaused = true;
	}
}

void MIDI_Loop (int NewValue)
{
	if (NewValue == 2)
	{
		bLooped = !bLooped;
	}
	else 
	{
		bLooped = NewValue;
	}
}

void MIDI_Stop (void)
{
	if (!bMidiInited)	//Just to be safe
		return;

	if (midiTrack)
	{
		StopMovie (midiTrack);
		DisposeMovie (midiTrack);
		midiTrack = NULL;
		bPaused = 0;
	}
}

