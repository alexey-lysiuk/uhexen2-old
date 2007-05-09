/*
	sys_win.c
	Win32 system interface code

	$Header: /home/ozzie/Download/0000/uhexen2/hexenworld/Server/win_stuff/sys_win.c,v 1.30 2007-05-09 18:11:39 sezero Exp $
*/

#include "quakedef.h"
#include <sys/types.h>
#include <limits.h>
#include <sys/timeb.h>
#include <winsock.h>
#include <errno.h>
#include <io.h>
#include <conio.h>


// heapsize: minimum 8 mb, standart 16 mb, max is 32 mb.
// -heapsize argument will abide by these min/max settings
// unless the -forcemem argument is used
#define MIN_MEM_ALLOC	0x0800000
#define STD_MEM_ALLOC	0x1000000
#define MAX_MEM_ALLOC	0x2000000

cvar_t		sys_nostdout = {"sys_nostdout", "0", CVAR_NONE};
int		devlog;	/* log the Con_DPrintf and Sys_DPrintf content when !developer.integer */


/*
================
Sys_mkdir
================
*/
int Sys_mkdir (const char *path)
{
	int rc;

	rc = _mkdir (path);
	if (rc != 0 && errno == EEXIST)
		rc = 0;

	return rc;
}

/*
=================================================
simplified findfirst/findnext implementation:
Sys_FindFirstFile and Sys_FindNextFile return
filenames only, not a dirent struct. this is
what we presently need in this engine.
=================================================
*/
static HANDLE  findhandle;
static WIN32_FIND_DATA finddata;

char *Sys_FindFirstFile (const char *path, const char *pattern)
{
	if (findhandle)
		Sys_Error ("Sys_FindFirst without FindClose");

	findhandle = FindFirstFile(va("%s/%s", path, pattern), &finddata);

	if (findhandle != INVALID_HANDLE_VALUE)
	{
		if (finddata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			return Sys_FindNextFile();
		else
			return finddata.cFileName;
	}

	return NULL;
}

char *Sys_FindNextFile (void)
{
	BOOL	retval;

	if (!findhandle || findhandle == INVALID_HANDLE_VALUE)
		return NULL;

	retval = FindNextFile(findhandle,&finddata);
	while (retval)
	{
		if (finddata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			retval = FindNextFile(findhandle,&finddata);
			continue;
		}

		return finddata.cFileName;
	}

	return NULL;
}

void Sys_FindClose (void)
{
	if (findhandle != INVALID_HANDLE_VALUE)
		FindClose(findhandle);
	findhandle = NULL;
}


/*
================
Sys_Error
================
*/
void Sys_Error (const char *error, ...)
{
	va_list		argptr;
	char		text[MAX_PRINTMSG];

	va_start (argptr, error);
	vsnprintf (text, sizeof(text), error, argptr);
	va_end (argptr);

	printf ("\nFATAL ERROR: %s\n\n", text);
	if (sv_logfile)
	{
		fprintf (sv_logfile, "\nFATAL ERROR: %s\n\n", text);
		fflush (sv_logfile);
	}

#ifdef DEBUG_BUILD
	getch();
#endif

	exit (1);
}


/*
================
Sys_PrintTerm
================
*/
void Sys_PrintTerm (const char *msgtxt)
{
	unsigned char		*p;

	if (sys_nostdout.integer)
		return;

	for (p = (unsigned char *) msgtxt; *p; p++)
		putc (*p, stdout);
}

/*
================
Sys_Quit
================
*/
void Sys_Quit (void)
{
	exit (0);
}


/*
================
Sys_DoubleTime
================
*/
double Sys_DoubleTime (void)
{
#if 0
	double t;
	struct _timeb	tstruct;
	static int	starttime;

	_ftime( &tstruct );

	if (!starttime)
		starttime = tstruct.time;
	t = (tstruct.time-starttime) + tstruct.millitm*0.001;

	return t;
#else
	static qboolean first = true;
	static DWORD starttime;
	DWORD now;

	now = timeGetTime();

	if (first)
	{
		first = false;
		starttime = now;
		return 0.0;
	}

	if (now < starttime) // wrapped?
		return (now / 1000.0) + (LONG_MAX - starttime / 1000.0);

	if (now - starttime == 0)
		return 0.0;

	return (now - starttime) / 1000.0;
#endif
}


/*
================
Sys_ConsoleInput
================
*/
char *Sys_ConsoleInput (void)
{
	static char	con_text[256];
	static int		textlen;
	int		c;

	// read a line out
	while (_kbhit())
	{
		c = _getch();
		putch (c);
		if (c == '\r')
		{
			con_text[textlen] = 0;
			putch ('\n');
			textlen = 0;
			return con_text;
		}
		if (c == 8)
		{
			if (textlen)
			{
				putch (' ');
				putch (c);
				textlen--;
				con_text[textlen] = 0;
			}
			continue;
		}
		con_text[textlen] = c;
		textlen++;
		con_text[textlen] = 0;
		if (textlen == sizeof(con_text))
			textlen = 0;
	}

	return NULL;
}


static void PrintVersion (void)
{
	printf ("HexenWorld server %4.2f (%s)\n", ENGINE_VERSION, VERSION_PLATFORM);
#if HOT_VERSION_BETA
	printf ("Hammer of Thyrion, %s-%s (%s) pre-release\n", HOT_VERSION_STR, HOT_VERSION_BETA_STR, HOT_VERSION_REL_DATE);
#else
	printf ("Hammer of Thyrion, release %s (%s)\n", HOT_VERSION_STR, HOT_VERSION_REL_DATE);
#endif
}

/*
===============================================================================

MAIN

===============================================================================
*/
static quakeparms_t	parms;
static char	cwd[MAX_OSPATH];

int main (int argc, char **argv)
{
	int			i;
	double		newtime, time, oldtime;
	char		*tmp;
	struct timeval	timeout;
	fd_set		fdset;

	PrintVersion();

	if (argc > 1)
	{
		for (i = 1; i < argc; i++)
		{
			if ( !(strcmp(argv[i], "-v")) || !(strcmp(argv[i], "-version" )) ||
				!(strcmp(argv[i], "--version")) )
			{
				exit(0);
			}
			else if ( !(strcmp(argv[i], "-h")) || !(strcmp(argv[i], "-help" )) ||
				  !(strcmp(argv[i], "-?")) || !(strcmp(argv[i], "--help")) )
			{
				printf ("See the documentation for details\n");
				exit (0);
			}
		}
	}

	memset (cwd, 0, sizeof(cwd));
	if ( _getcwd (cwd, sizeof(cwd)-1) == NULL )
		Sys_Error ("Couldn't determine current directory");

	tmp = cwd;
	while (*tmp != 0)
		tmp++;
	while (*tmp == 0)
	{
		--tmp;
		if (*tmp == '\\' || *tmp == '/')
			*tmp = 0;
	}

	/* initialize the host params */
	memset (&parms, 0, sizeof(parms));
	parms.basedir = cwd;
	parms.userdir = cwd;	/* no userdir on win32 */
	parms.argc = argc;
	parms.argv = argv;
	host_parms = &parms;

	devlog = COM_CheckParm("-devlog");

	Sys_Printf("basedir is: %s\n", parms.basedir);
	Sys_Printf("userdir is: %s\n", parms.userdir);

	parms.memsize = STD_MEM_ALLOC;

	i = COM_CheckParm ("-heapsize");
	if (i && i < com_argc-1)
	{
		parms.memsize = atoi (com_argv[i+1]) * 1024;

		if ((parms.memsize > MAX_MEM_ALLOC) && !(COM_CheckParm ("-forcemem")))
		{
			Sys_Printf ("Requested memory (%d Mb) too large, using the default maximum.\n", parms.memsize/(1024*1024));
			Sys_Printf ("If you are sure, use the -forcemem switch.\n");
			parms.memsize = MAX_MEM_ALLOC;
		}
		else if ((parms.memsize < MIN_MEM_ALLOC) && !(COM_CheckParm ("-forcemem")))
		{
			Sys_Printf ("Requested memory (%d Mb) too little, using the default minimum.\n", parms.memsize/(1024*1024));
			Sys_Printf ("If you are sure, use the -forcemem switch.\n");
			parms.memsize = MIN_MEM_ALLOC;
		}
	}

	parms.membase = malloc (parms.memsize);

	if (!parms.membase)
		Sys_Error ("Insufficient memory.\n");

	SV_Init();

// report the filesystem to the user
	Sys_Printf("fs_gamedir is: %s\n", fs_gamedir);
	Sys_Printf("fs_userdir is: %s\n", fs_userdir);

	timeBeginPeriod (1);

// run one frame immediately for first heartbeat
	SV_Frame (HX_FRAME_TIME);

//
// main loop
//
	oldtime = Sys_DoubleTime () - HX_FRAME_TIME;
	while (1)
	{
	// select on the net socket and stdin
	// the only reason we have a timeout at all is so that if the last
	// connected client times out, the message would not otherwise
	// be printed until the next event.
		FD_ZERO(&fdset);
		FD_SET(net_socket, &fdset);
		timeout.tv_sec = 0;
		timeout.tv_usec = 10000;
		if (select (net_socket+1, &fdset, NULL, NULL, &timeout) == -1)
			continue;

	// find time passed since last cycle
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;
		oldtime = newtime;

		SV_Frame (time);
	}

	return 0;
}

