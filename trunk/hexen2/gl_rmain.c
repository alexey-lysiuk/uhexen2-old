// gl_main.c

/*
 * $Header: /home/ozzie/Download/0000/uhexen2/hexen2/gl_rmain.c,v 1.1.1.1 2004-11-28 00:03:04 sezero Exp $
 */

#include "quakedef.h"

entity_t	r_worldentity;

qboolean	r_cache_thrash;		// compatability

vec3_t		modelorg, r_entorigin;
entity_t	*currententity;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

mplane_t	frustum[4];

int			c_brush_polys, c_alias_polys, c_sky_polys;

qboolean	envmap;				// true during envmap command capture 
int			currenttexture;		// to avoid unnecessary texture sets

int			particletexture;	// little dot for particles
int			playertextures;		// up to 16 color translated skins
int			gl_extra_textures[MAX_EXTRA_TEXTURES];   // generic textures for models

int			mirrortexturenum;	// quake texturenum, not gltexturenum
qboolean	mirror;
mplane_t	*mirror_plane;

float		model_constant_alpha;

float		r_time1;
float		r_lasttime1 = 0;

extern model_t *player_models[NUM_CLASSES];

//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

float	r_world_matrix[16];
float	r_base_world_matrix[16];

//
// screen size info
//
refdef_t	r_refdef;

mleaf_t		*r_viewleaf, *r_oldviewleaf;

texture_t	*r_notexture_mip;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value


void R_MarkLeaves (void);

cvar_t	r_norefresh = {"r_norefresh","0"};
cvar_t	r_drawentities = {"r_drawentities","1"};
cvar_t	r_drawviewmodel = {"r_drawviewmodel","1"};
cvar_t	r_speeds = {"r_speeds","0"};
cvar_t	r_fullbright = {"r_fullbright","0"};
cvar_t	r_lightmap = {"r_lightmap","0"};
cvar_t	r_shadows = {"r_shadows","0"};
cvar_t	r_mirroralpha = {"r_mirroralpha","1"};
cvar_t	r_wateralpha = {"r_wateralpha",".4", true};
cvar_t	r_dynamic = {"r_dynamic","1"};
cvar_t	r_novis = {"r_novis","0"};
cvar_t	r_wholeframe = {"r_wholeframe", "1", true};

cvar_t	gl_clear = {"gl_clear","0"};
cvar_t	gl_cull = {"gl_cull","1"};
cvar_t	gl_texsort = {"gl_texsort","1"};
cvar_t	gl_smoothmodels = {"gl_smoothmodels","1"};
cvar_t	gl_affinemodels = {"gl_affinemodels","0"};
cvar_t	gl_polyblend = {"gl_polyblend","1"};
cvar_t	gl_flashblend = {"gl_flashblend","0"};
cvar_t	gl_playermip = {"gl_playermip","0"};
cvar_t	gl_nocolors = {"gl_nocolors","0"};
cvar_t	gl_keeptjunctions = {"gl_keeptjunctions","1",true};
cvar_t	gl_reporttjunctions = {"gl_reporttjunctions","0"};

extern	cvar_t	gl_ztrick;
static qboolean AlwaysDrawModel;

static void R_RotateForEntity2(entity_t *e);


/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	int		i;

	for (i=0 ; i<4 ; i++)
		if (BoxOnPlaneSide (mins, maxs, &frustum[i]) == 2)
			return true;
	return false;
}


void R_RotateForEntity (entity_t *e)
{
    glfunc.glTranslatef_fp (e->origin[0],  e->origin[1],  e->origin[2]);

    glfunc.glRotatef_fp (e->angles[1],  0, 0, 1);
    glfunc.glRotatef_fp (-e->angles[0],  0, 1, 0);
    glfunc.glRotatef_fp (-e->angles[2],  1, 0, 0);
}

//==========================================================================
//
// R_RotateForEntity2
//
// Same as R_RotateForEntity(), but checks for EF_ROTATE and modifies
// yaw appropriately.
//
//==========================================================================

static void R_RotateForEntity2(entity_t *e)
{
	float	forward;
	float	yaw, pitch;
	vec3_t			angles;

	glfunc.glTranslatef_fp(e->origin[0], e->origin[1], e->origin[2]);

	if (e->model->flags & EF_FACE_VIEW)
	{
		VectorSubtract(e->origin,r_origin,angles);
		VectorSubtract(r_origin,e->origin,angles);
		VectorNormalize(angles);

		if (angles[1] == 0 && angles[0] == 0)
		{
			yaw = 0;
			if (angles[2] > 0)
				pitch = 90;
			else
				pitch = 270;
		}
		else
		{
			yaw = (int) (atan2(angles[1], angles[0]) * 180 / M_PI);
			if (yaw < 0)
				yaw += 360;

			forward = sqrt (angles[0]*angles[0] + angles[1]*angles[1]);
			pitch = (int) (atan2(angles[2], forward) * 180 / M_PI);
			if (pitch < 0)
				pitch += 360;
		}

		angles[0] = pitch;
		angles[1] = yaw;
		angles[2] = 0;

		glfunc.glRotatef_fp(-angles[0], 0, 1, 0);
		glfunc.glRotatef_fp(angles[1], 0, 0, 1);
//		glfunc.glRotatef_fp(-angles[2], 1, 0, 0);
		glfunc.glRotatef_fp(-e->angles[2], 1, 0, 0);
	}
	else 
	{
		if (e->model->flags & EF_ROTATE)
		{
			glfunc.glRotatef_fp(anglemod((e->origin[0]+e->origin[1])*0.8
				+(108*cl.time)), 0, 0, 1);
		}
		else
		{
			glfunc.glRotatef_fp(e->angles[1], 0, 0, 1);
		}
		glfunc.glRotatef_fp(-e->angles[0], 0, 1, 0);
		glfunc.glRotatef_fp(-e->angles[2], 1, 0, 0);
	}
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t *R_GetSpriteFrame (msprite_t *psprite)
{
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	int				i, numframes, frame;
	float			*pintervals, fullinterval, targettime, time;

	frame = currententity->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		Con_Printf ("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = cl.time + currententity->syncbase;

	// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
	// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i=0 ; i<(numframes-1) ; i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}


/*
=================
R_DrawSpriteModel

=================
*/
typedef struct
{
	vec3_t			vup, vright, vpn;	// in worldspace
} spritedesc_t;

void R_DrawSpriteModel (entity_t *e)
{
	vec3_t	point;
	mspriteframe_t	*frame;
	msprite_t		*psprite;
	
	vec3_t			tvec;
	float			dot, angle, sr, cr;
	spritedesc_t			r_spritedesc;
	int i;

	psprite = currententity->model->cache.data;

	frame = R_GetSpriteFrame (psprite);

	if (currententity->drawflags & DRF_TRANSLUCENT)
	{
		// rjr
		glfunc.glBlendFunc_fp (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glfunc.glEnable_fp( GL_BLEND );
		glfunc.glColor4f_fp (1,1,1,r_wateralpha.value);
	}
	else if (currententity->model->flags & EF_TRANSPARENT)
	{
		// rjr
		glfunc.glBlendFunc_fp (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glfunc.glEnable_fp( GL_BLEND );
		glfunc.glColor3f_fp(1,1,1);
	}
	else
	{
		// rjr
		glfunc.glBlendFunc_fp (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glfunc.glEnable_fp( GL_BLEND );
		glfunc.glColor3f_fp(1,1,1);
	}

	if (psprite->type == SPR_FACING_UPRIGHT)
	{
	// generate the sprite's axes, with vup straight up in worldspace, and
	// r_spritedesc.vright perpendicular to modelorg.
	// This will not work if the view direction is very close to straight up or
	// down, because the cross product will be between two nearly parallel
	// vectors and starts to approach an undefined state, so we don't draw if
	// the two vectors are less than 1 degree apart
		tvec[0] = -modelorg[0];
		tvec[1] = -modelorg[1];
		tvec[2] = -modelorg[2];
		VectorNormalize (tvec);
		dot = tvec[2];	// same as DotProduct (tvec, r_spritedesc.vup) because
						//  r_spritedesc.vup is 0, 0, 1
		if ((dot > 0.999848) || (dot < -0.999848))	// cos(1 degree) = 0.999848
			return;
		r_spritedesc.vup[0] = 0;
		r_spritedesc.vup[1] = 0;
		r_spritedesc.vup[2] = 1;
		r_spritedesc.vright[0] = tvec[1];
								// CrossProduct(r_spritedesc.vup, -modelorg,
		r_spritedesc.vright[1] = -tvec[0];
								//              r_spritedesc.vright)
		r_spritedesc.vright[2] = 0;
		VectorNormalize (r_spritedesc.vright);
		r_spritedesc.vpn[0] = -r_spritedesc.vright[1];
		r_spritedesc.vpn[1] = r_spritedesc.vright[0];
		r_spritedesc.vpn[2] = 0;
					// CrossProduct (r_spritedesc.vright, r_spritedesc.vup,
					//  r_spritedesc.vpn)
	}
	else if (psprite->type == SPR_VP_PARALLEL)
	{
	// generate the sprite's axes, completely parallel to the viewplane. There
	// are no problem situations, because the sprite is always in the same
	// position relative to the viewer
		for (i=0 ; i<3 ; i++)
		{
			r_spritedesc.vup[i] = vup[i];
			r_spritedesc.vright[i] = vright[i];
			r_spritedesc.vpn[i] = vpn[i];
		}
	}
	else if (psprite->type == SPR_VP_PARALLEL_UPRIGHT)
	{
	// generate the sprite's axes, with vup straight up in worldspace, and
	// r_spritedesc.vright parallel to the viewplane.
	// This will not work if the view direction is very close to straight up or
	// down, because the cross product will be between two nearly parallel
	// vectors and starts to approach an undefined state, so we don't draw if
	// the two vectors are less than 1 degree apart
		dot = vpn[2];	// same as DotProduct (vpn, r_spritedesc.vup) because
						//  r_spritedesc.vup is 0, 0, 1
		if ((dot > 0.999848) || (dot < -0.999848))	// cos(1 degree) = 0.999848
			return;
		r_spritedesc.vup[0] = 0;
		r_spritedesc.vup[1] = 0;
		r_spritedesc.vup[2] = 1;
		r_spritedesc.vright[0] = vpn[1];
										// CrossProduct (r_spritedesc.vup, vpn,
		r_spritedesc.vright[1] = -vpn[0];	//  r_spritedesc.vright)
		r_spritedesc.vright[2] = 0;
		VectorNormalize (r_spritedesc.vright);
		r_spritedesc.vpn[0] = -r_spritedesc.vright[1];
		r_spritedesc.vpn[1] = r_spritedesc.vright[0];
		r_spritedesc.vpn[2] = 0;
					// CrossProduct (r_spritedesc.vright, r_spritedesc.vup,
					//  r_spritedesc.vpn)
	}
	else if (psprite->type == SPR_ORIENTED)
	{
	// generate the sprite's axes, according to the sprite's world orientation
		AngleVectors (currententity->angles, r_spritedesc.vpn,
					  r_spritedesc.vright, r_spritedesc.vup);
	}
	else if (psprite->type == SPR_VP_PARALLEL_ORIENTED)
	{
	// generate the sprite's axes, parallel to the viewplane, but rotated in
	// that plane around the center according to the sprite entity's roll
	// angle. So vpn stays the same, but vright and vup rotate
		angle = currententity->angles[ROLL] * (M_PI*2 / 360);
		sr = sin(angle);
		cr = cos(angle);

		for (i=0 ; i<3 ; i++)
		{
			r_spritedesc.vpn[i] = vpn[i];
			r_spritedesc.vright[i] = vright[i] * cr + vup[i] * sr;
			r_spritedesc.vup[i] = vright[i] * -sr + vup[i] * cr;
		}
	}
	else
	{
		Sys_Error ("R_DrawSprite: Bad sprite type %d", psprite->type);
	}

//	R_RotateSprite (psprite->beamlength);

    GL_Bind(frame->gl_texturenum);

	glfunc.glBegin_fp (GL_QUADS);

	glfunc.glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glfunc.glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glfunc.glTexCoord2f_fp (0, 1);
	VectorMA (e->origin, frame->down, r_spritedesc.vup, point);
	VectorMA (point, frame->left, r_spritedesc.vright, point);
	glfunc.glVertex3fv_fp (point);

	glfunc.glTexCoord2f_fp (0, 0);
	VectorMA (e->origin, frame->up, r_spritedesc.vup, point);
	VectorMA (point, frame->left, r_spritedesc.vright, point);
	glfunc.glVertex3fv_fp (point);

	glfunc.glTexCoord2f_fp (1, 0);
	VectorMA (e->origin, frame->up, r_spritedesc.vup, point);
	VectorMA (point, frame->right, r_spritedesc.vright, point);
	glfunc.glVertex3fv_fp (point);

	glfunc.glTexCoord2f_fp (1, 1);
	VectorMA (e->origin, frame->down, r_spritedesc.vup, point);
	VectorMA (point, frame->right, r_spritedesc.vright, point);
	glfunc.glVertex3fv_fp (point);

	glfunc.glEnd_fp ();

	//restore tex parms
	glfunc.glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glfunc.glTexParameterf_fp(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glfunc.glDisable_fp( GL_BLEND );
}

/*
=============================================================

  ALIAS MODELS

=============================================================
*/


#define NUMVERTEXNORMALS	162

float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

vec3_t	shadevector;
float	shadelight, ambientlight;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anorm_dots.h"
;

float	*shadedots = r_avertexnormal_dots[0];

int	lastposenum;

/*
=============
GL_DrawAliasFrame
=============
*/
extern float RTint[256],GTint[256],BTint[256];

void GL_DrawAliasFrame (aliashdr_t *paliashdr, int posenum)
{
	float	s, t;
	float 	l;
	int		i, j;
	int		index;
	trivertx_t	*v, *verts;
	int		list;
	int		*order;
	vec3_t	point;
	float	*normal;
	int		count;
	float	r,g,b,p;

lastposenum = posenum;

	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	if (currententity->colorshade)
	{
		r = RTint[currententity->colorshade];
		g = GTint[currententity->colorshade];
		b = BTint[currententity->colorshade];
	}
	else
		r = g = b = 1;

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
			glfunc.glBegin_fp (GL_TRIANGLE_FAN);
		}
		else
			glfunc.glBegin_fp (GL_TRIANGLE_STRIP);

		do
		{
			// texture coordinates come from the draw list
			glfunc.glTexCoord2f_fp (((float *)order)[0], ((float *)order)[1]);
			order += 2;

			// normals and vertexes come from the frame list
			l = shadedots[verts->lightnormalindex] * shadelight;
			glfunc.glColor4f_fp (r*l, g*l, b*l, model_constant_alpha);
			glfunc.glVertex3f_fp (verts->v[0], verts->v[1], verts->v[2]);
			verts++;
		} while (--count);

		glfunc.glEnd_fp ();
	}
}


/*
=============
GL_DrawAliasShadow
=============
*/
extern	vec3_t			lightspot;

void GL_DrawAliasShadow (aliashdr_t *paliashdr, int posenum)
{
	float	s, t, l;
	int		i, j;
	int		index;
	trivertx_t	*v, *verts;
	int		list;
	int		*order;
	vec3_t	point;
	float	*normal;
	float	height, lheight;
	int		count;

	lheight = currententity->origin[2] - lightspot[2];

	height = 0;
	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	height = -lheight + 1.0;

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
			glfunc.glBegin_fp (GL_TRIANGLE_FAN);
		}
		else
			glfunc.glBegin_fp (GL_TRIANGLE_STRIP);

		do
		{
			// texture coordinates come from the draw list
			// (skipped for shadows) glfunc.glTexCoord2f_fpv ((float *)order);
			order += 2;

			// normals and vertexes come from the frame list
			point[0] = verts->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			point[1] = verts->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			point[2] = verts->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

			point[0] -= shadevector[0]*(point[2]+lheight);
			point[1] -= shadevector[1]*(point[2]+lheight);
			point[2] = height;
//			height -= 0.001;
			glfunc.glVertex3fv_fp (point);

			verts++;
		} while (--count);

		glfunc.glEnd_fp ();
	}	
}



/*
=================
R_SetupAliasFrame

=================
*/
void R_SetupAliasFrame (int frame, aliashdr_t *paliashdr)
{
	int				pose, numposes;
	float			interval;

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1)
	{
		interval = paliashdr->frames[frame].interval;
		pose += (int)(cl.time / interval) % numposes;
	}

	GL_DrawAliasFrame (paliashdr, pose);
}



/*
=================
R_DrawAliasModel

=================
*/
void R_DrawAliasModel (entity_t *e)
{
	int			i, j;
	int			lnum;
	vec3_t		dist;
	float		add;
	model_t		*clmodel;
	vec3_t		mins, maxs;
	aliashdr_t	*paliashdr;
	trivertx_t	*verts, *v;
	int			index;
	float		s, t, an;
	static float	tmatrix[3][4];
	float entScale;
	float xyfact;
	float zfact;
	qpic_t		*stonepic;
	glpic_t			*gl;
	char temp[40];
	int mls;
	vec3_t		adjust_origin;

	clmodel = currententity->model;

	VectorAdd (currententity->origin, clmodel->mins, mins);
	VectorAdd (currententity->origin, clmodel->maxs, maxs);

	if (!AlwaysDrawModel && R_CullBox (mins, maxs))
		return;


	VectorCopy (currententity->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	//
	// get lighting information
	//

	VectorCopy(currententity->origin, adjust_origin);
	adjust_origin[2] += (currententity->model->mins[2] + currententity->model->maxs[2]) / 2;
	ambientlight = shadelight = R_LightPoint (adjust_origin);

	// allways give the gun some light
	if (e == &cl.viewent && ambientlight < 24)
		ambientlight = shadelight = 24;

	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		if (cl_dlights[lnum].die >= cl.time)
		{
			VectorSubtract (currententity->origin,
							cl_dlights[lnum].origin,
							dist);
			add = cl_dlights[lnum].radius - Length(dist);

			if (add > 0)
				ambientlight += add;
		}
	}

	// clamp lighting so it doesn't overbright as much
	if (ambientlight > 128)
		ambientlight = 128;
	if (ambientlight + shadelight > 192)
		shadelight = 192 - ambientlight;

	mls = currententity->drawflags&MLS_MASKIN;
	if(currententity->model->flags&EF_ROTATE)
	{
		ambientlight = shadelight =
			60+34+sin(currententity->origin[0]+currententity->origin[1]
				+(cl.time*3.8))*34;
	}
	else if (mls == MLS_ABSLIGHT)
	{
		ambientlight = shadelight = currententity->abslight;
	}
	else if (mls != MLS_NONE)
	{ // Use a model light style (25-30)
		ambientlight = shadelight = d_lightstylevalue[24+mls]/2;
	}


	shadedots = r_avertexnormal_dots[((int)(e->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
	shadelight = shadelight / 200.0;
	
	an = e->angles[1]/180*M_PI;
	shadevector[0] = cos(-an);
	shadevector[1] = sin(-an);
	shadevector[2] = 1;
	VectorNormalize (shadevector);

	//
	// locate the proper data
	//
	paliashdr = (aliashdr_t *)Mod_Extradata (currententity->model);

	c_alias_polys += paliashdr->numtris;

	//
	// draw all the triangles
	//

    glfunc.glPushMatrix_fp ();
	R_RotateForEntity2(e);

	if(currententity->scale != 0 && currententity->scale != 100)
	{
		entScale = (float)currententity->scale/100.0;
		switch(currententity->drawflags&SCALE_TYPE_MASKIN)
		{
		case SCALE_TYPE_UNIFORM:
			tmatrix[0][0] = paliashdr->scale[0]*entScale;
			tmatrix[1][1] = paliashdr->scale[1]*entScale;
			tmatrix[2][2] = paliashdr->scale[2]*entScale;
			xyfact = zfact = (entScale-1.0)*127.95;
			break;
		case SCALE_TYPE_XYONLY:
			tmatrix[0][0] = paliashdr->scale[0]*entScale;
			tmatrix[1][1] = paliashdr->scale[1]*entScale;
			tmatrix[2][2] = paliashdr->scale[2];
			xyfact = (entScale-1.0)*127.95;
			zfact = 1.0;
			break;
		case SCALE_TYPE_ZONLY:
			tmatrix[0][0] = paliashdr->scale[0];
			tmatrix[1][1] = paliashdr->scale[1];
			tmatrix[2][2] = paliashdr->scale[2]*entScale;
			xyfact = 1.0;
			zfact = (entScale-1.0)*127.95;
			break;
		}
		switch(currententity->drawflags&SCALE_ORIGIN_MASKIN)
		{
		case SCALE_ORIGIN_CENTER:
			tmatrix[0][3] = paliashdr->scale_origin[0]-paliashdr->scale[0]*xyfact;
			tmatrix[1][3] = paliashdr->scale_origin[1]-paliashdr->scale[1]*xyfact;
			tmatrix[2][3] = paliashdr->scale_origin[2]-paliashdr->scale[2]*zfact;
			break;
		case SCALE_ORIGIN_BOTTOM:
			tmatrix[0][3] = paliashdr->scale_origin[0]-paliashdr->scale[0]*xyfact;
			tmatrix[1][3] = paliashdr->scale_origin[1]-paliashdr->scale[1]*xyfact;
			tmatrix[2][3] = paliashdr->scale_origin[2];
			break;
		case SCALE_ORIGIN_TOP:
			tmatrix[0][3] = paliashdr->scale_origin[0]-paliashdr->scale[0]*xyfact;
			tmatrix[1][3] = paliashdr->scale_origin[1]-paliashdr->scale[1]*xyfact;
			tmatrix[2][3] = paliashdr->scale_origin[2]-paliashdr->scale[2]*zfact*2.0;
			break;
		}
	}
	else
	{
		tmatrix[0][0] = paliashdr->scale[0];
		tmatrix[1][1] = paliashdr->scale[1];
		tmatrix[2][2] = paliashdr->scale[2];
		tmatrix[0][3] = paliashdr->scale_origin[0];
		tmatrix[1][3] = paliashdr->scale_origin[1];
		tmatrix[2][3] = paliashdr->scale_origin[2];
	}

	if(clmodel->flags&EF_ROTATE)
	{ // Floating motion
		tmatrix[2][3] += sin(currententity->origin[0]
			+currententity->origin[1]+(cl.time*3))*5.5;
	}

// [0][3] [1][3] [2][3]
//	glfunc.glTranslatef_fp (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
	glfunc.glTranslatef_fp (tmatrix[0][3],tmatrix[1][3],tmatrix[2][3]);
// [0][0] [1][1] [2][2]
//	glfunc.glScalef_fp (paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
	glfunc.glScalef_fp (tmatrix[0][0],tmatrix[1][1],tmatrix[2][2]);

	if ((currententity->model->flags & EF_SPECIAL_TRANS))
	{
		// rjr
		glfunc.glEnable_fp (GL_BLEND);
		glfunc.glBlendFunc_fp (GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
//		glColor3f( 1,1,1);
		model_constant_alpha = 1.0f;
		glfunc.glDisable_fp( GL_CULL_FACE );
	}
	else if (currententity->drawflags & DRF_TRANSLUCENT)
	{
		// rjr
		glfunc.glEnable_fp (GL_BLEND);
		glfunc.glBlendFunc_fp (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//		glfunc.glColor4f_fp( 1,1,1,r_wateralpha.value);
		model_constant_alpha = r_wateralpha.value;
	}
	else if ((currententity->model->flags & EF_TRANSPARENT))
	{
		// rjr
		glfunc.glEnable_fp (GL_BLEND);
		glfunc.glBlendFunc_fp (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//		glColor3f( 1,1,1);
		model_constant_alpha = 1.0f;
	}
	else if ((currententity->model->flags & EF_HOLEY))
	{
		// rjr
		glfunc.glEnable_fp (GL_BLEND);
		glfunc.glBlendFunc_fp (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

//		glColor3f( 1,1,1);
		model_constant_alpha = 1.0f;
	}
	else
	{
		// rjr
		glfunc.glColor3f_fp( 1,1,1);
		model_constant_alpha = 1.0f;
	}

	if (currententity->skinnum >= 100)
	{
		if (currententity->skinnum > 255) 
		{
			Sys_Error ("skinnum > 255");
		}

		if (gl_extra_textures[currententity->skinnum-100] == -1)  // Need to load it in
		{
			sprintf(temp,"gfx/skin%d.lmp",currententity->skinnum);
			stonepic = Draw_CachePic(temp);
			gl = (glpic_t *)stonepic->data;
			gl_extra_textures[currententity->skinnum-100] = gl->texnum;
		}

		GL_Bind(gl_extra_textures[currententity->skinnum-100]);
	}
	else
	{
		GL_Bind(paliashdr->gl_texturenum[currententity->skinnum]);

		// we can't dynamically colormap textures, so they are cached
		// seperately for the players.  Heads are just uncolored.
	
		if (currententity->colormap != vid.colormap && !gl_nocolors.value)
		{
			if (currententity->model == player_models[0] ||
			    currententity->model == player_models[1] ||
			    currententity->model == player_models[2] ||
			    currententity->model == player_models[3] ||
			    currententity->model == player_models[4])
			{
				i = currententity - cl_entities;
				if (i >= 1 && i<=cl.maxclients)
					GL_Bind(playertextures - 1 + i);
			}
		}
	}

	if (gl_smoothmodels.value)
		glfunc.glShadeModel_fp (GL_SMOOTH);
	glfunc.glTexEnvf_fp(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (gl_affinemodels.value)
		glfunc.glHint_fp (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	R_SetupAliasFrame (currententity->frame, paliashdr);
	if ((currententity->drawflags & DRF_TRANSLUCENT) ||
		(currententity->model->flags & EF_SPECIAL_TRANS))
		glfunc.glDisable_fp (GL_BLEND);

	if ((currententity->model->flags & EF_TRANSPARENT))
		glfunc.glDisable_fp (GL_BLEND);

	if ((currententity->model->flags & EF_HOLEY))
		glfunc.glDisable_fp (GL_BLEND);

	if ((currententity->model->flags & EF_SPECIAL_TRANS))
	{
		// rjr
		glfunc.glEnable_fp( GL_CULL_FACE );
	}

	glfunc.glTexEnvf_fp(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glfunc.glShadeModel_fp (GL_FLAT);
	if (gl_affinemodels.value)
		glfunc.glHint_fp (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glfunc.glPopMatrix_fp ();

	if (r_shadows.value)
	{
		glfunc.glPushMatrix_fp ();
		R_RotateForEntity2(e);
		glfunc.glDisable_fp (GL_TEXTURE_2D);
		glfunc.glEnable_fp (GL_BLEND);
		glfunc.glColor4f_fp (0,0,0,0.5);
		GL_DrawAliasShadow (paliashdr, lastposenum);
		glfunc.glEnable_fp (GL_TEXTURE_2D);
		glfunc.glDisable_fp (GL_BLEND);
		glfunc.glColor4f_fp (1,1,1,1);
		glfunc.glPopMatrix_fp ();
	}

}

//==================================================================================

typedef struct sortedent_s {
	entity_t *ent;
	vec_t len;
} sortedent_t;

sortedent_t     cl_transvisedicts[MAX_VISEDICTS];
sortedent_t		cl_transwateredicts[MAX_VISEDICTS];

int				cl_numtransvisedicts;
int				cl_numtranswateredicts;

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList (void)
{
	int		i;
	qboolean item_trans;
	mleaf_t *pLeaf;

	cl_numtransvisedicts=0;
	cl_numtranswateredicts=0;

	if (!r_drawentities.value)
		return;

	// draw sprites seperately, because of alpha blending
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		switch (currententity->model->type)
		{
		case mod_alias:
			item_trans = ((currententity->drawflags & DRF_TRANSLUCENT) ||
						  (currententity->model->flags & (EF_TRANSPARENT|EF_HOLEY|EF_SPECIAL_TRANS))) != 0;
			if (!item_trans)
				R_DrawAliasModel (currententity);

			break;

		case mod_brush:
			item_trans = ((currententity->drawflags & DRF_TRANSLUCENT)) != 0;
			if (!item_trans)
				R_DrawBrushModel (currententity,false);

			break;


		case mod_sprite:
			item_trans = true;

			break;

		default:
			item_trans = false;
			break;
		}
		
		if (item_trans) {
			pLeaf = Mod_PointInLeaf (currententity->origin, cl.worldmodel);
//			if (pLeaf->contents == CONTENTS_EMPTY)
			if (pLeaf->contents != CONTENTS_WATER)
				cl_transvisedicts[cl_numtransvisedicts++].ent = currententity;
			else
				cl_transwateredicts[cl_numtranswateredicts++].ent = currententity;
		}
	}
}

/*
================
R_DrawTransEntitiesOnList
Implemented by: jack
================
*/

int transCompare(const void *arg1, const void *arg2 ) {
	const sortedent_t *a1, *a2;
	a1 = (sortedent_t *) arg1;
	a2 = (sortedent_t *) arg2;
	return (a2->len - a1->len); // Sorted in reverse order.  Neat, huh?
}

void R_DrawTransEntitiesOnList ( qboolean inwater) {
	int i;
	int numents;
	sortedent_t *theents;
	int depthMaskWrite = 0;
	vec3_t result;

	theents = (inwater) ? cl_transwateredicts : cl_transvisedicts;
	numents = (inwater) ? cl_numtranswateredicts : cl_numtransvisedicts;

	for (i=0; i<numents; i++) {
		VectorSubtract(
			theents[i].ent->origin, 
			r_origin, 
			result);
//		theents[i].len = Length(result);
		theents[i].len = (result[0] * result[0]) + (result[1] * result[1]) + (result[2] * result[2]);
	}

	qsort((void *) theents, numents, sizeof(sortedent_t), transCompare);
	// Add in BETTER sorting here

	glfunc.glDepthMask_fp(0);
	for (i=0;i<numents;i++) {
		currententity = theents[i].ent;

		switch (currententity->model->type)
		{
		case mod_alias:
			if (!depthMaskWrite) {
				depthMaskWrite = 1;
				glfunc.glDepthMask_fp(1);
			}
			R_DrawAliasModel (currententity);
			break;
		case mod_brush:
			if (!depthMaskWrite) {
				depthMaskWrite = 1;
				glfunc.glDepthMask_fp(1);
			}
			R_DrawBrushModel (currententity,true);
			break;
		case mod_sprite:
			if (depthMaskWrite) {
				depthMaskWrite = 0;
				glfunc.glDepthMask_fp(0);
			}
			R_DrawSpriteModel (currententity);
			break;
		}
	}
	if (!depthMaskWrite) 
		glfunc.glDepthMask_fp(1);
}

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel (void)
{
	float		ambient[4], diffuse[4];
	int			j;
	int			lnum;
	vec3_t		dist;
	float		add;
	dlight_t	*dl;
	int			ambientlight, shadelight;

	if (!r_drawviewmodel.value)
		return;

	if (chase_active.value)
		return;

	if (envmap)
		return;

	if (!r_drawentities.value)
		return;

	if (cl.items & IT_INVISIBILITY)
		return;

	if (cl.v.health <= 0)
		return;

	currententity = &cl.viewent;
	if (!currententity->model)
		return;

	j = R_LightPoint (currententity->origin);

	if (j < 24)
		j = 24;		// allways give some light on gun
	ambientlight = j;
	shadelight = j;

// add dynamic lights		
	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		dl = &cl_dlights[lnum];
		if (!dl->radius)
			continue;
		if (!dl->radius)
			continue;
		if (dl->die < cl.time)
			continue;

		VectorSubtract (currententity->origin, dl->origin, dist);
		add = dl->radius - Length(dist);
		if (add > 0)
			ambientlight += add;
	}

#ifdef QUAKE2RJ
	cl.light_level = ambientlight;
#endif

	ambient[0] = ambient[1] = ambient[2] = ambient[3] = (float)ambientlight / 128;
	diffuse[0] = diffuse[1] = diffuse[2] = diffuse[3] = (float)shadelight / 128;

	// hack the depth range to prevent view model from poking into walls
	glfunc.glDepthRange_fp (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));
	AlwaysDrawModel = true;
	R_DrawAliasModel (currententity);
	AlwaysDrawModel = false;
	glfunc.glDepthRange_fp (gldepthmin, gldepthmax);
}


/*
============
R_PolyBlend
============
*/
void R_PolyBlend (void)
{
	if (!gl_polyblend.value)
		return;
	if (!v_blend[3])
		return;

	glfunc.glDisable_fp (GL_ALPHA_TEST);
	glfunc.glEnable_fp (GL_BLEND);
	glfunc.glDisable_fp (GL_DEPTH_TEST);
	glfunc.glDisable_fp (GL_TEXTURE_2D);

    glfunc.glLoadIdentity_fp ();

    glfunc.glRotatef_fp (-90,  1, 0, 0);	    // put Z going up
    glfunc.glRotatef_fp (90,  0, 0, 1);	    // put Z going up

	glfunc.glColor4fv_fp (v_blend);

	glfunc.glBegin_fp (GL_QUADS);

	glfunc.glVertex3f_fp (10, 10, 10);
	glfunc.glVertex3f_fp (10, -10, 10);
	glfunc.glVertex3f_fp (10, -10, -10);
	glfunc.glVertex3f_fp (10, 10, -10);
	glfunc.glEnd_fp ();

	glfunc.glDisable_fp (GL_BLEND);
	glfunc.glEnable_fp (GL_TEXTURE_2D);
	glfunc.glEnable_fp (GL_ALPHA_TEST);
}


int SignbitsForPlane (mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}


void R_SetFrustum (void)
{
	int		i;

	// front side is visible

	VectorAdd (vpn, vright, frustum[0].normal);
	VectorSubtract (vpn, vright, frustum[1].normal);

	VectorAdd (vpn, vup, frustum[2].normal);
	VectorSubtract (vpn, vup, frustum[3].normal);

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}



/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
	int				edgecount;
	vrect_t			vrect;
	float			w, h;

// don't allow cheats in multiplayer
	if (cl.maxclients > 1)
		Cvar_Set ("r_fullbright", "0");

	R_AnimateLight ();

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

	V_SetContentsColor (r_viewleaf->contents);
	V_CalcBlend ();

	r_cache_thrash = false;

	c_brush_polys = c_alias_polys = c_sky_polys = 0;

}


void MYgluPerspective( GLdouble fovy, GLdouble aspect,
		     GLdouble zNear, GLdouble zFar )
{
   GLdouble xmin, xmax, ymin, ymax;

   ymax = zNear * tan( fovy * M_PI / 360.0 );
   ymin = -ymax;

   xmin = ymin * aspect;
   xmax = ymax * aspect;

   glfunc.glFrustum_fp( xmin, xmax, ymin, ymax, zNear, zFar );
}


/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	float	screenaspect;
	float	yfov;
	int		i;
	extern	int glwidth, glheight;
	int		x, x2, y2, y, w, h;

	//
	// set up viewpoint
	//
	glfunc.glMatrixMode_fp(GL_PROJECTION);
    glfunc.glLoadIdentity_fp ();
	// JACK: Changes for non-scaled
	x = r_refdef.vrect.x * glwidth/vid.width /*320*/;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth/vid.width /*320*/;
	y = (vid.height/*200*/-r_refdef.vrect.y) * glheight/vid.height /*200*/;
	y2 = (vid.height/*200*/ - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight/vid.height /*200*/;

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < glwidth)
		x2++;
	if (y2 < 0)
		y2--;
	if (y < glheight)
		y++;

	w = x2 - x;
	h = y - y2;

	if (envmap)
	{
		x = y2 = 0;
		w = h = 256;
	}

	glfunc.glViewport_fp (glx + x, gly + y2, w, h);
    screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;
	yfov = 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*180/M_PI;
    MYgluPerspective (yfov,  screenaspect,  4,  4096);

	if (mirror)
	{
		if (mirror_plane->normal[2])
			glfunc.glScalef_fp (1, -1, 1);
		else
			glfunc.glScalef_fp (-1, 1, 1);
		glfunc.glCullFace_fp(GL_BACK);
	}
	else
		glfunc.glCullFace_fp(GL_FRONT);

	glfunc.glMatrixMode_fp(GL_MODELVIEW);
    glfunc.glLoadIdentity_fp ();

    glfunc.glRotatef_fp (-90,  1, 0, 0);	    // put Z going up
    glfunc.glRotatef_fp (90,  0, 0, 1);	    // put Z going up
    glfunc.glRotatef_fp (-r_refdef.viewangles[2],  1, 0, 0);
    glfunc.glRotatef_fp (-r_refdef.viewangles[0],  0, 1, 0);
    glfunc.glRotatef_fp (-r_refdef.viewangles[1],  0, 0, 1);
    glfunc.glTranslatef_fp (-r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);

	glfunc.glGetFloatv_fp (GL_MODELVIEW_MATRIX, r_world_matrix);

	//
	// set drawing parms
	//
	if (gl_cull.value)
		glfunc.glEnable_fp(GL_CULL_FACE);
	else
		glfunc.glDisable_fp(GL_CULL_FACE);

	glfunc.glDisable_fp(GL_BLEND);
	glfunc.glDisable_fp(GL_ALPHA_TEST);
	glfunc.glEnable_fp(GL_DEPTH_TEST);
}

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
void R_RenderScene ()
{
	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_MarkLeaves ();	// done here so we know if we're in water

	R_DrawWorld ();		// adds static entities to the list

	S_ExtraUpdate ();	// don't let sound get messed up if going slow
	
	R_DrawEntitiesOnList ();
/*	else
	{
		glfunc.glDepthMask_fp( 0 );
	}*/

	R_RenderDlights ();

#if 0
	if (!Translucent)
	{
		Test_Draw ();
	}
#endif

//	glfunc.glDepthMask_fp( 1 );
}


/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
	if (r_mirroralpha.value != 1.0)
	{
		if (gl_clear.value)
			glfunc.glClear_fp (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			glfunc.glClear_fp (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 0.5;
		glfunc.glDepthFunc_fp (GL_LEQUAL);
	}
	else if (gl_ztrick.value)
	{
		static int trickframe;

		if (gl_clear.value)
			glfunc.glClear_fp (GL_COLOR_BUFFER_BIT);

		trickframe++;
		if (trickframe & 1)
		{
			gldepthmin = 0;
			gldepthmax = 0.49999;
			glfunc.glDepthFunc_fp (GL_LEQUAL);
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5;
			glfunc.glDepthFunc_fp (GL_GEQUAL);
		}
	}
	else
	{
		if (gl_clear.value)
			glfunc.glClear_fp (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			glfunc.glClear_fp (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 1;
		glfunc.glDepthFunc_fp (GL_LEQUAL);
	}

	glfunc.glDepthRange_fp (gldepthmin, gldepthmax);
}

/*
=============
R_Mirror
=============
*/
void R_Mirror (void)
{
	float		d;
	msurface_t	*s;
	entity_t	*ent;

	if (!mirror)
		return;

	memcpy (r_base_world_matrix, r_world_matrix, sizeof(r_base_world_matrix));

	d = DotProduct (r_refdef.vieworg, mirror_plane->normal) - mirror_plane->dist;
	VectorMA (r_refdef.vieworg, -2*d, mirror_plane->normal, r_refdef.vieworg);

	d = DotProduct (vpn, mirror_plane->normal);
	VectorMA (vpn, -2*d, mirror_plane->normal, vpn);

	r_refdef.viewangles[0] = -asin (vpn[2])/M_PI*180;
	r_refdef.viewangles[1] = atan2 (vpn[1], vpn[0])/M_PI*180;
	r_refdef.viewangles[2] = -r_refdef.viewangles[2];

	ent = &cl_entities[cl.viewentity];
	if (cl_numvisedicts < MAX_VISEDICTS)
	{
		cl_visedicts[cl_numvisedicts] = ent;
		cl_numvisedicts++;
	}

	gldepthmin = 0.5;
	gldepthmax = 1;
	glfunc.glDepthRange_fp (gldepthmin, gldepthmax);
	glfunc.glDepthFunc_fp (GL_LEQUAL);

	R_RenderScene ();

	glfunc.glDepthMask_fp(0);

	R_DrawParticles ();
// THIS IS THE F*S*D(KCING MIRROR ROUTINE!  Go down!!!
	R_DrawTransEntitiesOnList( true ); // This restores the depth mask

	R_DrawWaterSurfaces ();

	R_DrawTransEntitiesOnList( false );

	gldepthmin = 0;
	gldepthmax = 0.5;
	glfunc.glDepthRange_fp (gldepthmin, gldepthmax);
	glfunc.glDepthFunc_fp (GL_LEQUAL);

	// blend on top
	glfunc.glEnable_fp (GL_BLEND);
	glfunc.glMatrixMode_fp(GL_PROJECTION);
	if (mirror_plane->normal[2])
		glfunc.glScalef_fp (1,-1,1);
	else
		glfunc.glScalef_fp (-1,1,1);
	glfunc.glCullFace_fp(GL_FRONT);
	glfunc.glMatrixMode_fp(GL_MODELVIEW);

	glfunc.glLoadMatrixf_fp (r_base_world_matrix);

	glfunc.glColor4f_fp (1,1,1,r_mirroralpha.value);
	s = cl.worldmodel->textures[mirrortexturenum]->texturechain;
	for ( ; s ; s=s->texturechain)
		R_RenderBrushPoly (s, true);
	cl.worldmodel->textures[mirrortexturenum]->texturechain = NULL;
	glfunc.glDisable_fp (GL_BLEND);
	glfunc.glColor4f_fp (1,1,1,1);
}

/*
=============
R_PrintTimes
=============
*/
void R_PrintTimes(void)
{
	float r_time2;
	float ms, fps;

	r_lasttime1 = r_time2 = Sys_FloatTime();

	ms = 1000*(r_time2-r_time1);
	fps = 1000/ms;

	Con_Printf("%3.1f fps %5.0f ms\n%4i wpoly  %4i epoly %4i spoly\n",
		fps, ms, c_brush_polys, c_alias_polys, c_sky_polys);
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView (void)
{
	double	time1, time2;

	if (r_norefresh.value)
		return;

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (r_speeds.value)
	{
		glfunc.glFinish_fp ();
		if (r_wholeframe.value)
		   r_time1 = r_lasttime1;
	   else
		   r_time1 = Sys_FloatTime ();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	mirror = false;

//	glfunc.glFinish_fp ();

	R_Clear ();

	// render normal view
	R_RenderScene ();

	glfunc.glDepthMask_fp(0);

	R_DrawParticles ();

	R_DrawTransEntitiesOnList( r_viewleaf->contents == CONTENTS_EMPTY ); // This restores the depth mask

	R_DrawWaterSurfaces ();

	R_DrawTransEntitiesOnList( r_viewleaf->contents != CONTENTS_EMPTY );

	R_DrawViewModel();

	// render mirror view
	R_Mirror ();

	R_PolyBlend ();

	if (r_speeds.value)
		R_PrintTimes ();
}

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.3  2002/01/06 02:49:24  theoddone33
 * Fix accidental checkin
 *
 * Revision 1.2  2002/01/06 02:46:37  theoddone33
 * Ensure screenshots and other files are written to ~/.aot/
 *
 * Revision 1.1  2002/01/02 15:14:44  phneutre
 * dlsym'ed all calls to GL functions with SDL_GL_GetProcAddress
 *
 * Revision 1.1.1.1  2001/11/09 17:04:00  theoddone33
 * Inital import
 *
 * 
 * 4     3/30/98 10:57a Jmonroe
 * 
 * 3     2/26/98 9:19p Jmonroe
 * shortened memory struct for sprites, added sprite orientation code in
 * gl (need to test)
 * 
 * 2     1/18/98 8:06p Jmonroe
 * all of rick's patch code is in now
 * 
 * 38    10/28/97 6:11p Jheitzman
 * 
 * 36    9/25/97 2:10p Rjohnson
 * Smaller status bar
 * 
 * 35    9/23/97 9:47p Rjohnson
 * Fix for dedicated gl server and color maps for sheeps
 * 
 * 34    9/15/97 1:26p Rjohnson
 * Fixed sprites
 * 
 * 33    9/09/97 5:24p Rjohnson
 * Play skin updates
 * 
 * 32    9/03/97 9:10a Rjohnson
 * Update
 * 
 * 31    8/31/97 9:27p Rjohnson
 * GL Updates
 * 
 * 30    8/21/97 11:31a Rjohnson
 * Fix for color map
 * 
 * 29    8/20/97 10:38p Rjohnson
 * Fix for view model drawing
 * 
 * 28    8/17/97 4:14p Rjohnson
 * Fix for model lighting
 * 
 * 27    8/15/97 3:10p Rjohnson
 * Precache Update
 * 
 * 26    8/06/97 2:59p Rjohnson
 * Fix for getting the light on the gl version
 * 
 * 25    7/24/97 5:31p Rjohnson
 * Updates to the gl version for color tinting
 * 
 * 24    6/17/97 10:03a Rjohnson
 * GL Updates
 * 
 * 23    6/16/97 5:47p Bgokey
 * 
 * 22    6/16/97 4:20p Rjohnson
 * Added a sky poly count
 * 
 * 21    6/16/97 5:28a Rjohnson
 * Minor fixes
 * 
 * 20    6/14/97 2:30p Rjohnson
 * Changed the defaults of some of the console variables
 * 
 * 19    6/14/97 1:59p Rjohnson
 * Updated the fps display in the gl size, as well as meshing directories
 * 
 * 18    5/31/97 3:43p Rjohnson
 * Fix for translucent water
 * 
 * 17    6/02/97 3:42p Gmctaggart
 * GL Catchup
 * 
 * 16    5/31/97 3:43p Rjohnson
 * Drawing items in the right order
 * 
 * 15    5/31/97 11:12a Rjohnson
 * GL Updates
 * 
 * 14    5/28/97 3:54p Rjohnson
 * Effect to make a model always face you
 * 
 * 13    5/15/97 6:34p Rjohnson
 * Code Cleanup
 * 
 * 12    4/17/97 2:38p Bgokey
 * 
 * 11    4/17/97 2:30p Bgokey
 * 
 * 10    4/17/97 12:15p Rjohnson
 * Fixed some compiling problems
 * 
 * 9     4/15/97 9:02p Bgokey
 * 
 * 8     3/25/97 12:48a Bgokey
 * 
 * 7     3/22/97 5:19p Rjohnson
 * Changed the stone drawing flag to just a generic way based upon the
 * skin number
 * 
 * 6     3/22/97 3:23p Rjohnson
 * Reversed a debugging test
 * 
 * 5     3/22/97 3:22p Rjohnson
 * Added the stone texture ability for models
 * 
 * 4     3/13/97 10:54p Rjohnson
 * Added support for transparent sprites
 * 
 * 3     3/13/97 12:24p Rjohnson
 * Added the dynamic scaling of models to the gl version
 */
