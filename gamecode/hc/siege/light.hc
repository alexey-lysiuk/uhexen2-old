/*
 * $Header: /home/ozzie/Download/0000/uhexen2/gamecode/hc/siege/light.hc,v 1.1 2005-01-26 17:26:11 sezero Exp $
 */
/*
==========================================================
LIGHT.HC
MG

Lights can be toggled/faded, shot out, etc.
==========================================================
*/

float START_LOW		= 1;

void initialize_lightstyle (void)
{
	if(self.spawnflags&START_LOW)
		if(self.lightvalue1<self.lightvalue2)
			lightstylestatic(self.style, self.lightvalue1);
		else
			lightstylestatic(self.style, self.lightvalue2);
	else
		if(self.lightvalue1<self.lightvalue2)
			lightstylestatic(self.style, self.lightvalue2);
		else
			lightstylestatic(self.style, self.lightvalue1);
}

void fadelight()
{
	self.frags+=self.cnt;
	self.light_lev+=self.frags;
	lightstylestatic(self.style, self.light_lev);
	self.count+=1;
	//dprint(ftos(self.light_lev));
	//dprint("\n");
	if(self.count/20>=self.fadespeed)
	{
		//dprint("light timed out\n");
		remove(self);
	}
	else if((self.cnt<0&&self.light_lev<=self.level)||(self.cnt>0&&self.light_lev>=self.level))
	{
		//dprint("light fade done\n");
		lightstylestatic(self.style, self.level);
		remove(self);
	}
	else
	{
		self.nextthink=time+0.05;
		self.think=fadelight;
	}
}

void lightstyle_change_think()
{
	//dprint("initializing light change\n");
	self.speed=self.lightvalue2 - self.lightvalue1;
	self.light_lev=lightstylevalue(self.style);
	if(self.light_lev==self.lightvalue1)
		self.level = self.lightvalue2;
	else if(self.light_lev==self.lightvalue2)
		self.level = self.lightvalue1;
	else if(self.speed>0)
		if(self.light_lev<self.lightvalue1+self.speed*0.5)
			self.level=self.lightvalue2;
		else
			self.level=self.lightvalue1;
	else if(self.speed<0)
		if(self.light_lev<self.lightvalue2-self.speed*0.5)
			self.level=self.lightvalue1;
		else
			self.level=self.lightvalue2;

	self.cnt=(self.level-self.light_lev)/self.fadespeed/20;
	self.think = fadelight;
	self.nextthink = time;
	/*dprint("light style # ");
	dprint(ftos(self.style));
	dprint(":\n");
	dprint("value1: ");
	dprint(ftos(self.lightvalue1));
	dprint("\n");
	dprint("value2: ");
	dprint(ftos(self.lightvalue2));
	dprint("\n");
	dprint("difference: ");
	dprint(ftos(self.speed));
	dprint("\n");
	dprint("fadespeed: ");
	dprint(ftos(self.fadespeed));
	dprint("\n");
	dprint("current light level: ");
	dprint(ftos(self.light_lev));
	dprint("\n");
	dprint("target light level: ");
	dprint(ftos(self.level));
	dprint("\n");
	dprint("luminosity interval: ");
	dprint(ftos(self.cnt));
	dprint("\n");*/
}

void lightstyle_change (entity light_targ)
{
//dprint("spawning light changer\n");
	newmis=spawn();
	newmis.lightvalue1=light_targ.lightvalue1;
	newmis.lightvalue2=light_targ.lightvalue2;
	newmis.fadespeed=light_targ.fadespeed;
	newmis.style=self.style;
	newmis.think=lightstyle_change_think;
	newmis.nextthink=time;
}

void torch_death ()
{
	lightstylestatic(self.style,0);
	chunk_death();
}

void torch_think (void)
{
float lightstate;
	lightstate=lightstylevalue(self.style);
	if(!lightstate)			//Use "off" frames
	{
		if(self.mdl)
			setmodel(self,self.mdl);
		self.drawflags(-)MLS_ABSLIGHT;
	}
	else
	{
		if(self.weaponmodel)
			setmodel(self,self.weaponmodel);
		self.drawflags(+)MLS_ABSLIGHT;
	}
	if(time>self.fadespeed)
		self.nextthink=-1;
	else
		self.nextthink=time+0.05;
	self.think=torch_think;
}

void torch_use (void)
{
	self.fadespeed=time+other.fadespeed+1;
	torch_think();
}

/*QUAKED light (0 1 0) (-8 -8 -8) (8 8 8) START_LOW NO_REMOVE
Non-displayed fading light.
Default light value is 300
Default style is 0
----------------------------------
If triggered, will toggle between lightvalue1 and lightvalue2
.lightvalue1 (default 0) 
.lightvalue2 (default 11, equivalent to 300 brightness)
Two values the light will fade-toggle between, 0 is black, 25 is brightest, 11 is equivalent to a value of 300.
.fadespeed (default 1) = How many seconds it will take to complete the desired lighting change
The light will start on at a default of the higher light value unless you turn on the startlow flag.
START_LOW = will make the light start at the lower of the lightvalues you specify (default uses brighter)
NO_REMOVE = Light entity will not be removed on spawn (for banks of triggered lights,one should stick around for reference)

NOTE: IF YOU DON'T PLAN ON USING THE DEFAULTS, ALL LIGHTS IN THE BANK OF LIGHTS NEED THIS INFO
*/
void light()
{
	if (self.targetname == "")
	{
		remove(self);
	}
	else
	{
		if(!self.lightvalue2)
			self.lightvalue2=11;
		if(!self.fadespeed)
			self.fadespeed = 1;
//		dprintf("Targeted Light with style of %s, value of ",self.style);
//		dprintf("%s\n",self.lightvalue2);
		initialize_lightstyle();
//		dprintf("My lightstyle was set to: %s\n",lightstylevalue(self.style));
		remove(self);
//		self.think=SUB_Remove;
//		thinktime self : 1;
	}
}

/*QUAK-ED light_globe (0 1 0) (-8 -8 -8) (8 8 8) START_LOW
Sphere globe light.
Default light value is 300
Default style is 0
----------------------------------
If triggered, will toggle between lightvalue1 and lightvalue2
.lightvalue1 (default 0) 
.lightvalue2 (default 11, equivalent to 300 brightness)
Two values the light will fade-toggle between, 0 is black, 25 is brightest, 11 is equivalent to a value of 300.
.fadespeed (default 1) = How many seconds it will take to complete the desired lighting change
The light will start on at a default of the higher light value unless you turn on the startlow flag.
START_LOW = will make the light start at the lower of the lightvalues you specify (default uses brighter)

NOTE: IF YOU DON'T PLAN ON USING THE DEFAULTS, ALL LIGHTS IN THE BANK OF LIGHTS NEED THIS INFO
*/
/*
void() light_globe =
{
	precache_model ("models/s_light.spr");
	setmodel (self, "models/s_light.spr");
	if(self.targetname)
		self.use=torch_use;
	self.mdl = "models/null.spr";
	self.weaponmodel = "models/s_light.spr";
	if(self.style>=32)
	{
		if(!self.lightvalue2)
			self.lightvalue2=11;
		if(!self.fadespeed)
			self.fadespeed = 1;
		initialize_lightstyle();
		self.think = torch_think;
		self.nextthink = time+1;
	}
	else
	{
		setmodel(self,self.weaponmodel);
		makestatic (self);
	}
};
*/

void() FireAmbient =
{
//FIXME: remove ambient sound if light is off, start it again if turned back on
	precache_sound ("raven/flame1.wav");
// attenuate fast
	ambientsound (self.origin, "raven/flame1.wav", 0.5, ATTN_STATIC);
};

/*QUAK-ED light_torch_small_walltorch (0 .5 0) (-10 -10 -20) (10 10 20) START_LOW
Short wall torch
Default light value is 200
Default style is 0
----------------------------------
If triggered, will toggle between lightvalue1 and lightvalue2
.lightvalue1 (default 0) 
.lightvalue2 (default 11, equivalent to 300 brightness)
Two values the light will fade-toggle between, 0 is black, 25 is brightest, 11 is equivalent to a value of 300.
.fadespeed (default 1) = How many seconds it will take to complete the desired lighting change
The light will start on at a default of the higher light value unless you turn on the startlow flag.
START_LOW = will make the light start at the lower of the lightvalues you specify (default uses brighter)

NOTE: IF YOU DON'T PLAN ON USING THE DEFAULTS, ALL LIGHTS IN THE BANK OF LIGHTS NEED THIS INFO
*/
void() light_torch_small_walltorch =
{
	precache_model ("models/flame.mdl");
	FireAmbient ();
	if(self.targetname)
		self.use=torch_use;
	self.mdl = "models/null.spr";
	self.weaponmodel = "models/flame.mdl";

	self.abslight = .75;

	if(self.style>=32)
	{
		if(!self.lightvalue2)
			self.lightvalue2=11;
		if(!self.fadespeed)
			self.fadespeed = 1;
		initialize_lightstyle();
		self.think = torch_think;
		self.nextthink = time+1;
	}
	else
	{
		self.drawflags(+)MLS_ABSLIGHT;
		setmodel(self,self.weaponmodel);
		makestatic (self);
	}
};

/*QUAKED light_flame_large_yellow (0 1 0) (-10 -10 -12) (12 12 18) START_LOW
Large yellow flame
----------------------------------
If triggered, will toggle between lightvalue1 and lightvalue2
.lightvalue1 (default 0) 
.lightvalue2 (default 11, equivalent to 300 brightness)
Two values the light will fade-toggle between, 0 is black, 25 is brightest, 11 is equivalent to a value of 300.
.fadespeed (default 1) = How many seconds it will take to complete the desired lighting change
The light will start on at a default of the higher light value unless you turn on the startlow flag.
START_LOW = will make the light start at the lower of the lightvalues you specify (default uses brighter)

NOTE: IF YOU DON'T PLAN ON USING THE DEFAULTS, ALL LIGHTS IN THE BANK OF LIGHTS NEED THIS INFO
*/
void() light_flame_large_yellow =
{
	precache_model ("models/flame1.mdl");
	FireAmbient ();
	if(self.targetname)
		self.use=torch_use;

	self.abslight = .75;

	self.mdl = "models/null.spr";
	self.weaponmodel = "models/flame1.mdl";
	if(self.style>=32)
	{
		if(!self.lightvalue2)
			self.lightvalue2=11;
		if(!self.fadespeed)
			self.fadespeed = 1;
		initialize_lightstyle();
		self.think = torch_think;
		self.nextthink = time+1;
	}
	else
	{
		self.drawflags(+)MLS_ABSLIGHT;
		setmodel(self,self.weaponmodel);
		makestatic (self);
	}
};

/*QUAKED light_flame_small_yellow (0 1 0) (-8 -8 -8) (8 8 8) START_LOW
Small yellow flame ball
----------------------------------
If triggered, will toggle between lightvalue1 and lightvalue2
.lightvalue1 (default 0) 
.lightvalue2 (default 11, equivalent to 300 brightness)
Two values the light will fade-toggle between, 0 is black, 25 is brightest, 11 is equivalent to a value of 300.
.fadespeed (default 1) = How many seconds it will take to complete the desired lighting change
The light will start on at a default of the higher light value unless you turn on the startlow flag.
START_LOW = will make the light start at the lower of the lightvalues you specify (default uses brighter)

NOTE: IF YOU DON'T PLAN ON USING THE DEFAULTS, ALL LIGHTS IN THE BANK OF LIGHTS NEED THIS INFO
*/
void() light_flame_small_yellow =
{
	precache_model ("models/flame2.mdl");
	FireAmbient ();
	if(self.targetname)
		self.use=torch_use;

	self.abslight = .75;

	self.mdl = "models/null.spr";
	self.weaponmodel = "models/flame2.mdl";
	if(self.style>=32)
	{
		if(!self.lightvalue2)
			self.lightvalue2=11;
		if(!self.fadespeed)
			self.fadespeed = 1;
		initialize_lightstyle();
		self.think = torch_think;
		self.nextthink = time+1;
	}
	else
	{
		self.drawflags(+)MLS_ABSLIGHT;
		setmodel(self,self.weaponmodel);
		makestatic (self);
	}
};

/*QUAK-ED light_flame_small_white (0 1 0) (-10 -10 -40) (10 10 40) START_LOW
Small white flame ball
----------------------------------
If triggered, will toggle between lightvalue1 and lightvalue2
.lightvalue1 (default 0) 
.lightvalue2 (default 11, equivalent to 300 brightness)
Two values the light will fade-toggle between, 0 is black, 25 is brightest, 11 is equivalent to a value of 300.
.fadespeed (default 1) = How many seconds it will take to complete the desired lighting change
The light will start on at a default of the higher light value unless you turn on the startlow flag.
START_LOW = will make the light start at the lower of the lightvalues you specify (default uses brighter)

NOTE: IF YOU DON'T PLAN ON USING THE DEFAULTS, ALL LIGHTS IN THE BANK OF LIGHTS NEED THIS INFO
*/
/*
void() light_flame_small_white =
{
	precache_model ("models/flame2.mdl");
	FireAmbient ();
	if(self.targetname)
		self.use=torch_use;

	self.abslight = .75;

	self.mdl = "models/null.spr";
	self.weaponmodel = "models/flame2.mdl";
	if(self.style>=32)
	{
		if(!self.lightvalue2)
			self.lightvalue2=11;
		if(!self.fadespeed)
			self.fadespeed = 1;
		initialize_lightstyle();
		self.think = torch_think;
		self.nextthink = time+1;
	}
	else
	{
		self.drawflags(+)MLS_ABSLIGHT;
		setmodel(self,self.weaponmodel);
		makestatic (self);
	}
};
*/

/*QUAKED light_gem (0 1 0) (-8 -8 -8) (8 8 8) START_LOW
A gem that displays light.
Default light value is 300
Default style is 0
----------------------------------
If triggered, will toggle between lightvalue1 and lightvalue2
.lightvalue1 (default 0) 
.lightvalue2 (default 11, equivalent to 300 brightness)
Two values the light will fade-toggle between, 0 is black, 25 is brightest, 11 is equivalent to a value of 300.
.fadespeed (default 1) = How many seconds it will take to complete the desired lighting change
The light will start on at a default of the higher light value unless you turn on the startlow flag.
START_LOW = will make the light start at the lower of the lightvalues you specify (default uses brighter)

NOTE: IF YOU DON'T PLAN ON USING THE DEFAULTS, ALL LIGHTS IN THE BANK OF LIGHTS NEED THIS INFO
*/
void() light_gem =
{
	precache_model ("models/gemlight.mdl");
	if(self.targetname)
		self.use=torch_use;
	self.mdl = "models/null.spr";
	self.weaponmodel = "models/gemlight.mdl";

	self.abslight = .75;

	if(self.style>=32)
	{
		if(!self.lightvalue2)
			self.lightvalue2=11;
		if(!self.fadespeed)
			self.fadespeed = 1;
		initialize_lightstyle();
		self.think = torch_think;
		self.nextthink = time+1;
	}
	else
	{
		self.drawflags(+)MLS_ABSLIGHT;
		setmodel(self,self.weaponmodel);
		makestatic (self);
	}
};

void fade_down ();
void fade_up ();
void wait_fade_down ()
{
	self.aflag*=-1;
	self.think=fade_down;
	thinktime self : self.delay;
}

void wait_fade_up ()
{
	self.aflag*=-1;
	self.think=fade_up;
	thinktime self : self.delay;
}

void fade_down ()
{
	self.light_lev=lightstylevalue(self.style);
	if(self.light_lev==self.level)
	{
		if(self.spawnflags&1)
			wait_fade_up();
		else
		{
			self.think=SUB_Null;
			self.nextthink=-1;
		}
		return;
	}

	self.think=fade_down;
	thinktime self : self.wait;

	self.light_lev+=self.aflag;
	if(self.light_lev<0)
		self.light_lev=0;
	else if(self.light_lev<self.level)
		self.light_lev=self.level;
	lightstylestatic(self.style, self.light_lev);
}

void fade_up ()
{
	self.light_lev=lightstylevalue(self.style);
	if(self.light_lev==self.lightvalue1)
	{
		if(self.spawnflags&1)
			wait_fade_down();
		else
		{
			self.think=SUB_Null;
			self.nextthink=-1;
		}
		return;
	}

	self.think=fade_up;
	thinktime self : self.wait;

	self.light_lev+=self.aflag;
	if(self.light_lev>25)
		self.light_lev=25;
	else if(self.light_lev>self.lightvalue1)
		self.light_lev=self.lightvalue1;
	lightstylestatic(self.style, self.light_lev);
}

/*QUAKED ambient_lightfader (.5 .5 .5) (-4 -4 -4) (4 4 4) TOGGLE
"wait" - how long to wait between fade steps (only 25 light values)
	default 60 (1 minute)
"level" - what light value to stop at (from 1 - 25)
	default 0 
"aflag" - direction and increment to fade at (1 is one step brighter, -1 is default-fade down 1 step)
	default -1

TOGGLE - Will reach end of it's fade, then start fading back in/out the other way
"delay" - If toggled, how long to wait before changing fade direction and starting again.
	default 300 (5 minutes)
*/
void al_delayed_init ()
{
float savelevel;
	self.use=SUB_Null;
	self.lightvalue1 = lightstylevalue(self.style);
	if(self.aflag>0)
	{
		if(self.level<self.lightvalue1)
		{
			savelevel = self.level;
			self.level = self.lightvalue1;
			self.lightvalue1=self.level;
		}
		self.think = fade_up;
	}
	else
	{
		if(self.level>self.lightvalue1)
		{
			savelevel = self.level;
			self.level = self.lightvalue1;
			self.lightvalue1=self.level;
		}
		self.think = fade_down;
	}
	thinktime self : self.wait;
}

void ambient_lightfader ()
{
//	if(!self.wait)
		self.wait = 180;
	if(!self.aflag)
		self.aflag=-1;
	if(!self.delay)
		self.delay = 180;

	self.use=al_delayed_init;
	self.think=al_delayed_init;
	thinktime self : 180;//start fading after 3 minutes if not used first
}

/*
 * $Log: not supported by cvs2svn $
 * 
 * 7     5/25/98 1:39p Mgummelt
 * 
 * 6     5/05/98 8:33p Mgummelt
 * Added 6th playerclass for Siege only- Dwarf
 * 
 * 5     4/27/98 4:34p Mgummelt
 * Siege beta version 0.13
 * 
 * 4     4/24/98 1:31a Mgummelt
 * Siege version 0.02 4/24/98 1:31 AM
 * 
 * 3     4/23/98 5:19p Mgummelt
 * Siege version 0.01 4/23/98
 * 
 * 1     2/04/98 1:59p Rjohnson
 * 
 * 35    9/02/97 3:04p Mgummelt
 * 
 * 34    9/02/97 2:55a Mgummelt
 * 
 * 33    8/23/97 7:15p Rlove
 * 
 * 32    8/21/97 5:10p Rjohnson
 * Change for ablight
 * 
 * 31    8/21/97 3:19p Rjohnson
 * Abslight change
 * 
 * 30    8/20/97 5:18p Rjohnson
 * Abslight for everything
 * 
 * 29    8/19/97 10:48p Rjohnson
 * Abslight for flames
 * 
 * 28    8/13/97 5:52p Mgummelt
 * 
 * 27    7/19/97 9:57p Mgummelt
 * 
 * 26    7/10/97 5:28p Rlove
 * 
 * 25    6/19/97 3:01p Rjohnson
 * Removed old id code
 * 
 * 24    6/15/97 5:10p Mgummelt
 * 
 * 23    6/15/97 3:15p Mgummelt
 * 
 * 22    6/13/97 3:29p Mgummelt
 * 
 * 21    6/09/97 10:04a Mgummelt
 * 
 * 20    6/06/97 4:08p Mgummelt
 * 
 * 19    6/06/97 11:38a Mgummelt
 * 
 * 18    6/06/97 10:58a Rjohnson
 * Fix for lights
 * 
 * 17    6/05/97 8:55p Mgummelt
 * 
 * 14    5/15/97 6:34p Rjohnson
 * Code cleanup
 * 
 * 13    4/04/97 4:53p Jweier
 * Removed more of Adam's code
 * 
 * 7     3/28/97 4:47p Aleggett
 * Updated light fading even further...
 * 
 * 6     3/20/97 2:15p Aleggett
 * Made fading lights use the new lightstyle method.
 * 
 * 5     3/05/97 4:48p Rlove
 * Small fix for flame1.mdl
 * 
 * 4     3/05/97 1:54p Rlove
 * Placed light entites in LIGHT.HC and placed breakable entities in
 * BREAKABL.HC
 * 
 * 3     3/05/97 12:22p Rlove
 * Added all four types of torches
 * 
 * 2     2/28/97 6:11p Rlove
 * New Egypt Light
 * 
 * 1     2/28/97 5:39p Rlove
 */
