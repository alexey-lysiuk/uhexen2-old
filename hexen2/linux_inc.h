#ifndef _LINUX_INC
#define _LINUX_INC

#define _inline inline
#define __inline inline
#define HANDLE int
#define HINST int
#define HWND int
#define PASCAL
#define FAR
#define SOCKET int
#define WORD unsigned short
#define BOOL int
#define LPWSADATA int
#define UINT unsigned int
#define DWORD unsigned int
#define LONG long
#define LONGLONG long long

#define stricmp strcasecmp
#define Q_strncasecmp strncasecmp
#define Q_strcasecmp strcasecmp
#define strnicmp strncasecmp
#define _strnicmp strncasecmp
#define strcmpi strcasecmp

typedef struct RECT_s {
	int left;
	int right;
	int top;
	int bottom;
} RECT;

typedef union _LARGE_INTEGER { 
	struct {
		DWORD LowPart; 
		LONG  HighPart; 
	} part;
	LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER; 

#endif
