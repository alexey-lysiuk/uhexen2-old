#include <sys/types.h>
#include <sys/time.h>

#ifdef PLATFORM_UNIX
#include <pwd.h>
#include <dirent.h>
#endif

#include <unistd.h>
#ifndef PLATFORM_UNIX
#include <winsock.h>
#endif
#include "qwsvdef.h"


cvar_t	sys_nostdout = {"sys_nostdout","0"};

#ifdef PLATFORM_UNIX
/*
================
Sys_GetUserdir
================
*/
int Sys_GetUserdir(char *buff, unsigned int len)
{
    struct passwd *pwent;

    pwent = getpwuid( getuid() );
    if ( pwent == NULL ) {
	perror( "getpwuid" );
	return 0;
    }

    if ( strlen( pwent->pw_dir ) + strlen( AOT_USERDIR) + 2 > (unsigned)len ) {
	return 0;
    }

    sprintf( buff, "%s/%s", pwent->pw_dir, AOT_USERDIR );
    Sys_mkdir(buff);

    return 1;
}
#endif

/*
================
Sys_FileTime
================
*/
int	Sys_FileTime (char *path)
{
	FILE	*f;
	
	f = fopen(path, "rb");
	if (f)
	{
		fclose(f);
		return 1;
	}
	
	return -1;
}

/*
================
Sys_mkdir
================
*/
void Sys_mkdir (char *path)
{
}


/*
================
Sys_Error
================
*/
void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr,error);
	vsprintf (text, error,argptr);
	va_end (argptr);

//    MessageBox(NULL, text, "Error", 0 /* MB_OK */ );
	printf ("ERROR: %s\n", text);

//#ifdef _DEBUG
	//getch();
//#endif

	exit (1);
}


/*
================
Sys_DoubleTime
================
*/
double Sys_DoubleTime (void)
{
	struct timeval  tp;
	struct timezone tzp;
	static int              secbase;

	gettimeofday(&tp, &tzp);

	if (!secbase) {
		secbase = tp.tv_sec;
		return tp.tv_usec/1000000.0;
	}

	return (tp.tv_sec - secbase) + tp.tv_usec/1000000.0;
}


/*
================
Sys_ConsoleInput
================
*/
char *Sys_ConsoleInput (void)
{
	static char	text[256];
	static int	len;
	int	count;
	int	i;
	char	c;
	fd_set		set;
	struct timeval	timeout;

	FD_ZERO (&set);
	FD_SET (0, &set);	/* 0 is stdin?? */
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	while (select (1, &set, NULL, NULL, &timeout))
	{
		read (0, &c, 1);
		if (c == '\n' || c == '\r')
		{
			text[len] = 0;
			len = 0;
			return text;
		}
		else if (c == 8)
		{
			if (len)
			{
				len--;
				text[len] = 0;
			}
			continue;
		}
		text[len] = c;
		len++;
		text[len] = 0;
		if (len == sizeof(text))
			len = 0;
	}

	return NULL;
}


/*
================
Sys_Printf
================
*/
void Sys_Printf (char *fmt, ...)
{
	va_list		argptr;
	
	if (sys_nostdout.value)
		return;
		
	va_start (argptr,fmt);
	vprintf (fmt,argptr);
	va_end (argptr);
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
=============
Sys_Init

Quake calls this so the system can register variables before host_hunklevel
is marked
=============
*/
void Sys_Init (void)
{
	Cvar_RegisterVariable (&sys_nostdout);
}

/*
==================
main

==================
*/
char	*newargv[256];

int main (int argc, char **argv)
{
	quakeparms_t	parms;
	double			newtime, time, oldtime;
	static	char	cwd[1024];
	struct timeval	timeout;
	fd_set			fdset;
	int				t;
	static  char    userdir[1024];

	COM_InitArgv (argc, argv);
	
	parms.argc = com_argc;
	parms.argv = com_argv;

	parms.memsize = 16*1024*1024;

	if ((t = COM_CheckParm ("-heapsize")) != 0 &&
		t + 1 < com_argc)
		parms.memsize = atoi (com_argv[t + 1]) * 1024;

	if ((t = COM_CheckParm ("-mem")) != 0 &&
		t + 1 < com_argc)
		parms.memsize = atoi (com_argv[t + 1]) * 1024 * 1024;

	parms.membase = malloc (parms.memsize);

	if (!parms.membase)
		Sys_Error("Insufficient memory.\n");

	if (!(Sys_GetUserdir(userdir,sizeof(userdir))))
	  Sys_Error ("Couldn't determine userspace directory");
	/*else printf("userdir is: %s\n",userdir);*/

	parms.basedir = ".";
	parms.cachedir = NULL;

#ifdef PLATFORM_UNIX
	parms.userdir = userdir;
#else
	parms.userdir = parms.basedir;
#endif

	SV_Init (&parms);

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

	return true;
}


