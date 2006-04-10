#ifndef	LAUNCHER_COMMONDEFS_H
#define	LAUNCHER_COMMONDEFS_H

// definitions common to all of the launcher source files.

#define	__STRINGIFY(x) #x
#define	STRINGIFY(x) __STRINGIFY(x)

// Hammer of Thyrion version num.
#define HOT_VERSION_MAJ		1
#define HOT_VERSION_MID		4
#define HOT_VERSION_MIN		0
#define HOT_VERSION_STR		STRINGIFY(HOT_VERSION_MAJ) "." STRINGIFY(HOT_VERSION_MID) "." STRINGIFY(HOT_VERSION_MIN)

// Launcher version num.
#define LAUNCHER_VERSION_MAJ	0
#define LAUNCHER_VERSION_MID	7
#define LAUNCHER_VERSION_MIN	3
#define LAUNCHER_VERSION_STR	STRINGIFY(LAUNCHER_VERSION_MAJ) "." STRINGIFY(LAUNCHER_VERSION_MID) "." STRINGIFY(LAUNCHER_VERSION_MIN)

#ifndef DEMOBUILD
#define	AOT_USERDIR	".hexen2"
#else
#define	AOT_USERDIR	".hexen2demo"
#endif

#define	DEST_H2		0
#define	DEST_HW		1

#define	H2_BINARY_NAME	"hexen2"
#define	HW_BINARY_NAME	"hwcl"

#define	RES_320		0
#define	RES_400		1
#define	RES_512		2
#define	RES_640		3
#define	RES_800		4
#define	RES_1024	5
#define	RES_1280	6
#define	RES_1600	7
#define	RES_MAX		8

#define	MAX_H2GAMES	3	// max entries in the h2game_names table
#define	MAX_HWGAMES	6	// max entries in the hwgame_names table
#if defined(__linux__)
#define	MAX_SOUND	4	// max entries in the snddrv_names table
#else
#define	MAX_SOUND	3	// ... this time excluding alsa
#endif
#define	MAX_RATES	6	// max entries in the snd_rates table

#define HEAP_MINSIZE	16384	// minimum heap memory size in KB
#define HEAP_DEFAULT	32768	// default heap memory size in KB
#define HEAP_MAXSIZE	98304	// maximum heap memory size in KB
#define ZONE_MINSIZE	256	// minimum zone memory size in KB
#define ZONE_DEFAULT	256	// default zone memory size in KB
#define ZONE_MAXSIZE	1024	// maximum zone memory size in KB

#endif	// LAUNCHER_COMMONDEFS_H

