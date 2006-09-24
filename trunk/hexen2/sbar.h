/*
	sbar.h

	$Id: sbar.h,v 1.7 2006-09-24 17:28:42 sezero Exp $
*/

#ifndef __HX2_SBAR_H
#define __HX2_SBAR_H

// HEADER FILES ------------------------------------------------------------

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void Sbar_Init(void);
void Sbar_Changed(void);
void Sbar_Draw(void);
void SB_InvChanged(void);
void SB_InvReset(void);
void SB_ViewSizeChanged(void);
void Sbar_DeathmatchOverlay(void);

// PUBLIC DATA DECLARATIONS ------------------------------------------------

extern int sb_lines; // scan lines to draw

#endif	/* __HX2_SBAR_H */

