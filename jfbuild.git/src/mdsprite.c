#ifdef USE_OPENGL
//------------------------------------- MD2/MD3 LIBRARY BEGINS -------------------------------------

#include "compat.h"
#include "build.h"
#include "glbuild.h"
#include "kplib.h"
#include "pragmas.h"
#include "cache1d.h"
#include "baselayer.h"
#include "md4.h"
#include "engine_priv.h"
#include "polymost_priv.h"
#include "hightile_priv.h"
#include "polymosttex_priv.h"
#include "mdsprite_priv.h"

#ifdef __POWERPC__
#define SHIFTMOD32(a) ((a)&31)
#else
#define SHIFTMOD32(a) (a)
#endif

#define VOXBORDWIDTH 1 //use 0 to save memory, but has texture artifacts; 1 looks better...
voxmodel *voxmodels[MAXVOXELS];

tile2model_t tile2model[MAXTILES];

	//Move this to appropriate place!
hudtyp hudmem[2][MAXTILES]; //~320KB ... ok for now ... could replace with dynamic alloc

char mdinited=0;
long mdtims, omdtims;

#define MODELALLOCGROUP 256
static long nummodelsalloced = 0;
long nextmodelid = 0;
mdmodel **models = NULL;

static long maxmodelverts = 0, allocmodelverts = 0;
static point3d *vertlist = NULL; //temp array to store interpolated vertices for drawing

mdmodel *mdload (const char *);
void mdfree (mdmodel *);

void freeallmodels ()
{
	int i;

	if (models)
	{
		for(i=0;i<nextmodelid;i++) mdfree(models[i]);
		free(models); models = NULL;
		nummodelsalloced = 0;
		nextmodelid = 0;
	}

	memset(tile2model,-1,sizeof(tile2model));

	if (vertlist)
	{
		free(vertlist);
		vertlist = NULL;
		allocmodelverts = maxmodelverts = 0;
	}
}

void clearskins ()
{
	mdmodel *m;
	int i, j;

	for(i=0;i<nextmodelid;i++)
	{
		m = models[i];
		if (m->mdnum == 1) {
			voxmodel *v = (voxmodel*)m;
			for(j=0;j<MAXPALOOKUPS;j++) {
				if (v->texid) {
					bglDeleteTextures(1, &v->texid[j]);
					v->texid = 0;
				}
			}
		} else if (m->mdnum == 2 || m->mdnum == 3) {
			md2model *m2 = (md2model*)m;
			mdskinmap_t *sk;
			for(j=0;j<m2->numskins*(HICEFFECTMASK+1);j++)
			{
				if (m2->tex[j] && m2->tex[j]->glpic) {
					bglDeleteTextures(1, &m2->tex[j]->glpic);
					m2->tex[j]->glpic = 0;
				}
			}

			for(sk=m2->skinmap;sk;sk=sk->next)
				for(j=0;j<(HICEFFECTMASK+1);j++)
				{
					if (sk->tex[j] && sk->tex[j]->glpic) {
						bglDeleteTextures(1, &sk->tex[j]->glpic);
						sk->tex[j]->glpic = 0;
					}
				}
		}
	}

	for(i=0;i<MAXVOXELS;i++)
	{
		voxmodel *v = (voxmodel*)voxmodels[i]; if (!v) continue;
		for(j=0;j<MAXPALOOKUPS;j++) {
			if (v->texid[j]) bglDeleteTextures(1,(GLuint*)&v->texid[j]);
			v->texid[j] = 0;
		}
	}
}

void mdinit ()
{
	memset(hudmem,0,sizeof(hudmem));
	freeallmodels();
	mdinited = 1;
}

int md_loadmodel (const char *fn)
{
	mdmodel *vm, **ml;

	if (!mdinited) mdinit();

	if (nextmodelid >= nummodelsalloced)
	{
		ml = (mdmodel **)realloc(models,(nummodelsalloced+MODELALLOCGROUP)*4); if (!ml) return(-1);
		models = ml; nummodelsalloced += MODELALLOCGROUP;
	}

	vm = mdload(fn); if (!vm) return(-1);
	models[nextmodelid++] = vm;
	return(nextmodelid-1);
}

int md_setmisc (int modelid, float scale, int shadeoff, float zadd)
{
	mdmodel *m;

	if (!mdinited) mdinit();

	if ((unsigned long)modelid >= (unsigned long)nextmodelid) return -1;
	m = models[modelid];
	m->bscale = scale;
	m->shadeoff = shadeoff;
	m->zadd = zadd;

	return 0;
}

int md_tilehasmodel (int tilenume)
{
	if (!mdinited) return -1;
	return tile2model[tilenume].modelid;
}

static long framename2index (mdmodel *vm, const char *nam)
{
	int i = 0;

	switch(vm->mdnum)
	{
		case 2:
			{
			md2model *m = (md2model *)vm;
			md2frame_t *fr;
			for(i=0;i<m->numframes;i++)
			{
				fr = (md2frame_t *)&m->frames[i*m->framebytes];
				if (!Bstrcmp(fr->name, nam)) break;
			}
			}
			break;
		case 3:
			{
			md3model *m = (md3model *)vm;
			for(i=0;i<m->numframes;i++)
				if (!Bstrcmp(m->head.frames[i].nam,nam)) break;
			}
			break;
	}
	return(i);
}

int md_defineframe (int modelid, const char *framename, int tilenume, int skinnum)
{
	void *vm;
	md2model *m;
	int i;

	if (!mdinited) mdinit();

	if ((unsigned long)modelid >= (unsigned long)nextmodelid) return(-1);
	if ((unsigned long)tilenume >= (unsigned long)MAXTILES) return(-2);
	if (!framename) return(-3);

	m = (md2model *)models[modelid];
	if (m->mdnum == 1) {
		tile2model[tilenume].modelid = modelid;
		tile2model[tilenume].framenum = tile2model[tilenume].skinnum = 0;
		return 0;
	}

	i = framename2index((mdmodel*)m,framename);
	if (i == m->numframes) return(-3);   // frame name invalid

	tile2model[tilenume].modelid = modelid;
	tile2model[tilenume].framenum = i;
	tile2model[tilenume].skinnum = skinnum;

	return 0;
}

int md_defineanimation (int modelid, const char *framestart, const char *frameend, int fpssc, int flags)
{
	md2model *m;
	mdanim_t ma, *map;
	int i;

	if (!mdinited) mdinit();

	if ((unsigned long)modelid >= (unsigned long)nextmodelid) return(-1);

	memset(&ma, 0, sizeof(ma));
	m = (md2model *)models[modelid];
	if (m->mdnum < 2) return 0;

		//find index of start frame
	i = framename2index((mdmodel*)m,framestart);
	if (i == m->numframes) return -2;
	ma.startframe = i;

		//find index of finish frame which must trail start frame
	i = framename2index((mdmodel*)m,frameend);
	if (i == m->numframes) return -3;
	ma.endframe = i;

	ma.fpssc = fpssc;
	ma.flags = flags;

	map = (mdanim_t*)calloc(1,sizeof(mdanim_t));
	if (!map) return(-4);
	memcpy(map, &ma, sizeof(ma));

	map->next = m->animations;
	m->animations = map;

	return(0);
}

int md_defineskin (int modelid, const char *skinfn, int palnum, int skinnum, int surfnum)
{
	mdskinmap_t *sk, *skl;
	md2model *m;

	if (!mdinited) mdinit();

	if ((unsigned long)modelid >= (unsigned long)nextmodelid) return -1;
	if (!skinfn) return -2;
	if ((unsigned)palnum >= (unsigned)MAXPALOOKUPS) return -3;

	m = (md2model *)models[modelid];
	if (m->mdnum < 2) return 0;
	if (m->mdnum == 2) surfnum = 0;

	skl = NULL;
	for (sk = m->skinmap; sk; skl = sk, sk = sk->next)
		if (sk->palette == (unsigned char)palnum && skinnum == sk->skinnum && surfnum == sk->surfnum) break;
	if (!sk) {
		sk = (mdskinmap_t *)calloc(1,sizeof(mdskinmap_t));
		if (!sk) return -4;

		if (!skl) m->skinmap = sk;
		else skl->next = sk;
	} else if (sk->fn) free(sk->fn);

	sk->palette = (unsigned char)palnum;
	sk->skinnum = skinnum;
	sk->surfnum = surfnum;
	sk->fn = (char *)malloc(strlen(skinfn)+1);
	if (!sk->fn) return(-4);
	strcpy(sk->fn, skinfn);

	return 0;
}

int md_definehud (int modelid, int tilex, double xadd, double yadd, double zadd, double angadd, int flags)
{
	if (!mdinited) mdinit();

	if ((unsigned long)modelid >= (unsigned long)nextmodelid) return -1;
	if ((unsigned long)tilex >= (unsigned long)MAXTILES) return -2;

	hudmem[(flags>>2)&1][tilex].xadd = xadd;
	hudmem[(flags>>2)&1][tilex].yadd = yadd;
	hudmem[(flags>>2)&1][tilex].zadd = zadd;
	hudmem[(flags>>2)&1][tilex].angadd = ((short)angadd)|2048;
	hudmem[(flags>>2)&1][tilex].flags = (short)flags;

	return 0;
}

int md_undefinetile(int tile)
{
	if (!mdinited) return 0;
	if ((unsigned)tile >= (unsigned)MAXTILES) return -1;

	tile2model[tile].modelid = -1;
	return 0;
}

int md_undefinemodel(int modelid)
{
	int i;
	if (!mdinited) return 0;
	if ((unsigned long)modelid >= (unsigned long)nextmodelid) return -1;

	for (i=MAXTILES-1; i>=0; i--)
		if (tile2model[i].modelid == modelid)
			tile2model[i].modelid = -1;

	if (models) {
		mdfree(models[modelid]);
		models[modelid] = NULL;
	}

	return 0;
}

static void md_calculateskinid(const char * filename, int effects, unsigned char id[16])
{
	struct {
		int effects;
		char filename[BMAX_PATH];
	} skinid;
	
	memset(&skinid, 0, sizeof(skinid));
	skinid.effects = effects;
	strncpy(skinid.filename, filename, BMAX_PATH);
	
	md4once((unsigned char *) &skinid, sizeof(skinid), id);
}

//Note: even though it says md2model, it works for both md2model&md3model
PTMHead * mdloadskin (md2model *m, int number, int pal, int surf)
{
	int i, err = 0;
	char *skinfile = 0, fn[BMAX_PATH+65];
	unsigned char id[16];
	PTMHead **tex = 0;
	mdskinmap_t *sk, *skzero = 0;
	
	if (m->mdnum == 2) {
		surf = 0;
	}
	
	if ((unsigned)pal >= (unsigned)MAXPALOOKUPS) {
		return 0;
	}
	
	i = -1;
	for (sk = m->skinmap; sk; sk = sk->next) {
		if ((int)sk->palette == pal && sk->skinnum == number && sk->surfnum == surf) {
			tex = &sk->tex[ hictinting[pal].f ];
			skinfile = sk->fn;
			strcpy(fn,skinfile);
			//OSD_Printf("Using exact match skin (pal=%d,skinnum=%d,surfnum=%d) %s\n",pal,number,surf,skinfile);
			break;
		}
		//If no match, give highest priority to number, then pal.. (Parkar's request, 02/27/2005)
		else if (((int)sk->palette ==   0) && (sk->skinnum == number) && (sk->surfnum == surf) && (i < 5)) { i = 5; skzero = sk; }
		else if (((int)sk->palette == pal) && (sk->skinnum ==      0) && (sk->surfnum == surf) && (i < 4)) { i = 4; skzero = sk; }
		else if (((int)sk->palette ==   0) && (sk->skinnum ==      0) && (sk->surfnum == surf) && (i < 3)) { i = 3; skzero = sk; }
		else if (((int)sk->palette ==   0) && (sk->skinnum == number) && (i < 2)) { i = 2; skzero = sk; }
		else if (((int)sk->palette == pal) && (sk->skinnum ==      0) && (i < 1)) { i = 1; skzero = sk; }
		else if (((int)sk->palette ==   0) && (sk->skinnum ==      0) && (i < 0)) { i = 0; skzero = sk; }
	}
	if (!sk) {
		if (skzero) {
			tex = &skzero->tex[ hictinting[pal].f ];
			skinfile = skzero->fn;
			strcpy(fn,skinfile);
			//OSD_Printf("Using def skin 0,0 as fallback, pal=%d\n", pal);
		} else {
			if ((unsigned)number >= (unsigned)m->numskins) {
				number = 0;
			}
			tex = &m->tex[ number * (HICEFFECTMASK+1) + hictinting[pal].f ];
			skinfile = m->skinfn + number*64;
			strcpy(fn,m->basepath);
			strcat(fn,skinfile);
			//OSD_Printf("Using MD2/MD3 skin (%d) %s, pal=%d\n",number,skinfile,pal);
		}
	}
	
	if (!skinfile[0]) {
		return 0;
	}
	
	if (*tex && (*tex)->glpic) {
		// texture already loaded
		return *tex;
	}
	
	if (!(*tex)) {
		// no PTMHead referenced yet at *tex
		md_calculateskinid(skinfile, hictinting[pal].f, id);
		*tex = PTM_GetHead(id);
		if (!(*tex)) {
			return 0;
		}
	}
	
	if (!(*tex)->glpic) {
		// no texture loaded in the PTMHead yet
		if ((err = PTM_LoadTextureFile(skinfile, *tex, PTH_CLAMPED, hictinting[pal].f))) {
			if (polymosttexverbosity >= 1) {
				const char * errstr = PTM_GetLoadTextureFileErrorString(err);
				initprintf("MDSprite: %s %s\n",
						   skinfile, errstr);
			}
			return 0;
		}
		m->usesalpha = (((*tex)->flags & PTH_HASALPHA) == PTH_HASALPHA);
	}

	if (!m->skinloaded)
	{
		if ((*tex)->sizx != (*tex)->tsizx || (*tex)->sizy != (*tex)->tsizy)
		{
			float fx, fy;
			fx = ((float)(*tex)->tsizx)/((float)(*tex)->sizx);
			fy = ((float)(*tex)->tsizy)/((float)(*tex)->sizy);
			if (m->mdnum == 2)
			{
				long *lptr;
				for(lptr=m->glcmds;(i=*lptr++);)
					for(i=labs(i);i>0;i--,lptr+=3)
					{
						((float *)lptr)[0] *= fx;
						((float *)lptr)[1] *= fy;
					}
			}
			else if (m->mdnum == 3)
			{
				md3model *m3 = (md3model *)m;
				md3surf_t *s;
				long surfi;
				for (surfi=0;surfi<m3->head.numsurfs;surfi++)
				{
					s = &m3->head.surfs[surfi];
					for(i=s->numverts-1;i>=0;i--)
					{
						s->uv[i].u *= fx;
						s->uv[i].v *= fy;
					}
				}
			}
		}
		m->skinloaded = 1+number;
	}

	bglTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,glfiltermodes[gltexfiltermode].mag);
	bglTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,glfiltermodes[gltexfiltermode].min);
	if (glinfo.maxanisotropy > 1.0)
		bglTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_ANISOTROPY_EXT,glanisotropy);
	bglTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
	bglTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
	
	return (*tex);
}	

	//Note: even though it says md2model, it works for both md2model&md3model
static void updateanimation (md2model *m, spritetype *tspr)
{
	mdanim_t *anim;
	long i, j;

	m->cframe = m->nframe = tile2model[tspr->picnum].framenum;

	for (anim = m->animations;
		  anim && anim->startframe != m->cframe;
		  anim = anim->next) ;
	if (!anim) { m->interpol = 0; return; }

	if (((long)spriteext[tspr->owner].mdanimcur) != anim->startframe ||
			(spriteext[tspr->owner].flags & SPREXT_NOMDANIM))
	{
		spriteext[tspr->owner].mdanimcur = (short)anim->startframe;
		spriteext[tspr->owner].mdanimtims = mdtims;
		m->cframe = m->nframe = anim->startframe;
		m->interpol = 0;
		return;
	}

	i = (mdtims-spriteext[tspr->owner].mdanimtims)*anim->fpssc;
	j = ((anim->endframe+1-anim->startframe)<<16);

		//Just in case you play the game for a VERY long time...
	if (i < 0) { i = 0; spriteext[tspr->owner].mdanimtims = mdtims; }
		//compare with j*2 instead of j to ensure i stays > j-65536 for MDANIM_ONESHOT
	if ((i >= j+j) && (anim->fpssc)) //Keep mdanimtims close to mdtims to avoid the use of MOD
		spriteext[tspr->owner].mdanimtims += j/anim->fpssc;

	if (anim->flags&MDANIM_ONESHOT)
		{ if (i > j-65536) i = j-65536; }
	else { if (i >= j) { i -= j; if (i >= j) i %= j; } }

	m->cframe = (i>>16)+anim->startframe;
	m->nframe = m->cframe+1; if (m->nframe > anim->endframe) m->nframe = anim->startframe;
	m->interpol = ((float)(i&65535))/65536.f;
}

//--------------------------------------- MD2 LIBRARY BEGINS ---------------------------------------

static void md2free (md2model *m)
{
	mdanim_t *anim, *nanim = NULL;
	mdskinmap_t *sk, *nsk = NULL;

	if (!m) return;

	for(anim=m->animations; anim; anim=nanim)
	{
		nanim = anim->next;
		free(anim);
	}
	for(sk=m->skinmap; sk; sk=nsk)
	{
		nsk = sk->next;
		free(sk->fn);
		free(sk);
	}

	if (m->frames) free(m->frames);
	if (m->glcmds) free(m->glcmds);
	if (m->basepath) free(m->basepath);
	if (m->skinfn) free(m->skinfn);

	if (m->tex) free(m->tex);
}

static md2model *md2load (int fil, const char *filnam)
{
	md2model *m;
	md2head_t head;
	char *buf, st[BMAX_PATH];
	long i;

	m = (md2model *)calloc(1,sizeof(md2model)); if (!m) return(0);
	m->mdnum = 2; m->scale = .01;

	kread(fil,(char *)&head,sizeof(md2head_t));
	head.id = B_LITTLE32(head.id);                 head.vers = B_LITTLE32(head.vers);
	head.skinxsiz = B_LITTLE32(head.skinxsiz);     head.skinysiz = B_LITTLE32(head.skinysiz);
	head.framebytes = B_LITTLE32(head.framebytes); head.numskins = B_LITTLE32(head.numskins);
	head.numverts = B_LITTLE32(head.numverts);     head.numuv = B_LITTLE32(head.numuv);
	head.numtris = B_LITTLE32(head.numtris);       head.numglcmds = B_LITTLE32(head.numglcmds);
	head.numframes = B_LITTLE32(head.numframes);   head.ofsskins = B_LITTLE32(head.ofsskins);
	head.ofsuv = B_LITTLE32(head.ofsuv);           head.ofstris = B_LITTLE32(head.ofstris);
	head.ofsframes = B_LITTLE32(head.ofsframes);   head.ofsglcmds = B_LITTLE32(head.ofsglcmds);
	head.ofseof = B_LITTLE32(head.ofseof);
	
	if ((head.id != 0x32504449) || (head.vers != 8)) { free(m); return(0); } //"IDP2"

	m->numskins = head.numskins;
	m->numframes = head.numframes;
	m->numverts = head.numverts;
	m->numglcmds = head.numglcmds;
	m->framebytes = head.framebytes;
	m->frames = (char *)calloc(m->numframes,m->framebytes); if (!m->frames) { free(m); return(0); }
	m->glcmds = (long *)calloc(m->numglcmds,sizeof(long)); if (!m->glcmds) { free(m->frames); free(m); return(0); }
	klseek(fil,head.ofsframes,SEEK_SET);
	if (kread(fil,(char *)m->frames,m->numframes*m->framebytes) != m->numframes*m->framebytes)
		{ free(m->glcmds); free(m->frames); free(m); return(0); }
	klseek(fil,head.ofsglcmds,SEEK_SET);
	if (kread(fil,(char *)m->glcmds,m->numglcmds*sizeof(long)) != (long)(m->numglcmds*sizeof(long)))
		{ free(m->glcmds); free(m->frames); free(m); return(0); }

#if B_BIG_ENDIAN != 0
	{
		char *f = (char *)m->frames;
		long *l,j;
		md2frame_t *fr;
		
		for (i = m->numframes-1; i>=0; i--) {
			fr = (md2frame_t *)f;
			l = (long *)&fr->mul;
			for (j=5;j>=0;j--) l[j] = B_LITTLE32(l[j]);
			f += m->framebytes;
		}
		
		for (i = m->numglcmds-1; i>=0; i--) {
			m->glcmds[i] = B_LITTLE32(m->glcmds[i]);
		}
	}
#endif

	strcpy(st,filnam);
	for(i=strlen(st)-1;i>0;i--)
		if ((st[i] == '/') || (st[i] == '\\')) { i++; break; }
	if (i<0) i=0;
	st[i] = 0;
	m->basepath = (char *)malloc(i+1); if (!m->basepath) { free(m->glcmds); free(m->frames); free(m); return(0); }
	strcpy(m->basepath, st);

	m->skinfn = (char *)calloc(m->numskins,64); if (!m->skinfn) { free(m->basepath); free(m->glcmds); free(m->frames); free(m); return(0); }
	klseek(fil,head.ofsskins,SEEK_SET);
	if (kread(fil,m->skinfn,64*m->numskins) != 64*m->numskins)
		{ free(m->glcmds); free(m->frames); free(m); return(0); }

	m->tex = (PTMHead **)calloc(m->numskins, sizeof(PTMHead *) * (HICEFFECTMASK+1));
	if (!m->tex) { free(m->skinfn); free(m->basepath); free(m->glcmds); free(m->frames); free(m); return(0); }

	maxmodelverts = max(maxmodelverts, m->numverts);

	return(m);
}

static int md2draw (md2model *m, spritetype *tspr)
{
	point3d fp, m0, m1, a0, a1;
	md2frame_t *f0, *f1;
	unsigned char *c0, *c1;
	long i, *lptr;
	float f, g, k0, k1, k2, k3, k4, k5, k6, k7, mat[16], pc[4];
	PTMHead *ptmh = 0;

	updateanimation(m,tspr);

// -------- Unnecessarily clean (lol) code to generate translation/rotation matrix for MD2 ---------

		//create current&next frame's vertex list from whole list
	f0 = (md2frame_t *)&m->frames[m->cframe*m->framebytes];
	f1 = (md2frame_t *)&m->frames[m->nframe*m->framebytes];
	f = m->interpol; g = 1-f;
	m0.x = f0->mul.x*m->scale*g; m1.x = f1->mul.x*m->scale*f;
	m0.y = f0->mul.y*m->scale*g; m1.y = f1->mul.y*m->scale*f;
	m0.z = f0->mul.z*m->scale*g; m1.z = f1->mul.z*m->scale*f;
	a0.x = f0->add.x*m->scale; a0.x = (f1->add.x*m->scale-a0.x)*f+a0.x;
	a0.y = f0->add.y*m->scale; a0.y = (f1->add.y*m->scale-a0.y)*f+a0.y;
	a0.z = f0->add.z*m->scale; a0.z = (f1->add.z*m->scale-a0.z)*f+a0.z + m->zadd*m->scale;
	c0 = &f0->verts[0].v[0]; c1 = &f1->verts[0].v[0];

	// Parkar: Moved up to be able to use k0 for the y-flipping code
	k0 = tspr->z;
	if ((globalorientation&128) && !((globalorientation&48)==32)) k0 += (float)((tilesizy[tspr->picnum]*tspr->yrepeat)<<1);
	
	// Parkar: Changed to use the same method as centeroriented sprites
	if (globalorientation&8) //y-flipping
	{
		m0.z = -m0.z; m1.z = -m1.z; a0.z = -a0.z;
		k0 -= (float)((tilesizy[tspr->picnum]*tspr->yrepeat)<<2);
	}
	if (globalorientation&4) { m0.y = -m0.y; m1.y = -m1.y; a0.y = -a0.y; } //x-flipping

	f = ((float)tspr->xrepeat)/64*m->bscale;
	m0.x *= f; m1.x *= f; a0.x *= f; f = -f;   // 20040610: backwards models aren't cool
	m0.y *= f; m1.y *= f; a0.y *= f;
	f = ((float)tspr->yrepeat)/64*m->bscale;
	m0.z *= f; m1.z *= f; a0.z *= f;
	
	// floor aligned
	k1 = tspr->y;
	if((globalorientation&48)==32)
	{
		m0.z = -m0.z; m1.z = -m1.z; a0.z = -a0.z;
		m0.y = -m0.y; m1.y = -m1.y; a0.y = -a0.y;
		f = a0.x; a0.x = a0.z; a0.z = f;
		k1 += (float)((tilesizy[tspr->picnum]*tspr->yrepeat)>>3);
	}

	f = (65536.0*512.0)/((float)xdimen*viewingrange);
	g = 32.0/((float)xdimen*gxyaspect);
	m0.y *= f; m1.y *= f; a0.y = (((float)(tspr->x-globalposx))/  1024.0 + a0.y)*f;
	m0.x *=-f; m1.x *=-f; a0.x = (((float)(k1     -globalposy))/ -1024.0 + a0.x)*-f;
	m0.z *= g; m1.z *= g; a0.z = (((float)(k0     -globalposz))/-16384.0 + a0.z)*g;

	k0 = ((float)(tspr->x-globalposx))*f/1024.0;
	k1 = ((float)(tspr->y-globalposy))*f/1024.0;
	f = gcosang2*gshang;
	g = gsinang2*gshang;
	k4 = (float)sintable[(tspr->ang+spriteext[tspr->owner].angoff+1024)&2047] / 16384.0;
	k5 = (float)sintable[(tspr->ang+spriteext[tspr->owner].angoff+ 512)&2047] / 16384.0;
	k2 = k0*(1-k4)+k1*k5;
	k3 = k1*(1-k4)-k0*k5;
	k6 = f*gstang - gsinang*gctang; k7 = g*gstang + gcosang*gctang;
	mat[0] = k4*k6 + k5*k7; mat[4] = gchang*gstang; mat[ 8] = k4*k7 - k5*k6; mat[12] = k2*k6 + k3*k7;
	k6 = f*gctang + gsinang*gstang; k7 = g*gctang - gcosang*gstang;
	mat[1] = k4*k6 + k5*k7; mat[5] = gchang*gctang; mat[ 9] = k4*k7 - k5*k6; mat[13] = k2*k6 + k3*k7;
	k6 =           gcosang2*gchang; k7 =           gsinang2*gchang;
	mat[2] = k4*k6 + k5*k7; mat[6] =-gshang;        mat[10] = k4*k7 - k5*k6; mat[14] = k2*k6 + k3*k7;
	
	mat[12] += a0.y*mat[0] + a0.z*mat[4] + a0.x*mat[ 8];
	mat[13] += a0.y*mat[1] + a0.z*mat[5] + a0.x*mat[ 9];
	mat[14] += a0.y*mat[2] + a0.z*mat[6] + a0.x*mat[10];
	
	// floor aligned
	if((globalorientation&48)==32)
	{
        f = mat[4]; mat[4] = mat[8]*16.0; mat[8] = -f*(1.0/16.0);
        f = mat[5]; mat[5] = mat[9]*16.0; mat[9] = -f*(1.0/16.0);
        f = mat[6]; mat[6] = mat[10]*16.0; mat[10] = -f*(1.0/16.0);
    }
    
		//Mirrors
	if (grhalfxdown10x < 0) { mat[0] = -mat[0]; mat[4] = -mat[4]; mat[8] = -mat[8]; mat[12] = -mat[12]; }

// ------ Unnecessarily clean (lol) code to generate translation/rotation matrix for MD2 ends ------

#if 0
	for(i=m->numverts-1;i>=0;i--) //interpolate (for animation) & transform to Build coords
	{
		fp.z = ((float)c0[(i<<2)+0])*m0.x + ((float)c1[(i<<2)+0])*m1.x;
		fp.y = ((float)c0[(i<<2)+2])*m0.z + ((float)c1[(i<<2)+2])*m1.z;
		fp.x = ((float)c0[(i<<2)+1])*m0.y + ((float)c1[(i<<2)+1])*m1.y;
		vertlist[i].x = fp.x*mat[0] + fp.y*mat[4] + fp.z*mat[ 8] + mat[12];
		vertlist[i].y = fp.x*mat[1] + fp.y*mat[5] + fp.z*mat[ 9] + mat[13];
		vertlist[i].z = fp.x*mat[2] + fp.y*mat[6] + fp.z*mat[10] + mat[14];
	}
#else
	for(i=m->numverts-1;i>=0;i--) //interpolate (for animation) & transform to Build coords
	{
		vertlist[i].z = ((float)c0[(i<<2)+0])*m0.x + ((float)c1[(i<<2)+0])*m1.x;
		vertlist[i].y = ((float)c0[(i<<2)+2])*m0.z + ((float)c1[(i<<2)+2])*m1.z;
		vertlist[i].x = ((float)c0[(i<<2)+1])*m0.y + ((float)c1[(i<<2)+1])*m1.y;
	}
	bglMatrixMode(GL_MODELVIEW); //Let OpenGL (and perhaps hardware :) handle the matrix rotation
	mat[3] = mat[7] = mat[11] = 0.f; mat[15] = 1.f; bglLoadMatrixf(mat);
#endif

	ptmh = mdloadskin(m,tile2model[tspr->picnum].skinnum,globalpal,0);
	if (!ptmh || !ptmh->glpic) return 0;

	//bit 10 is an ugly hack in game.c\animatesprites telling MD2SPRITE
	//to use Z-buffer hacks to hide overdraw problems with the shadows
	if (tspr->cstat&1024)
	{
		bglDepthFunc(GL_LESS); //NEVER,LESS,(,L)EQUAL,GREATER,(NOT,G)EQUAL,ALWAYS
		bglDepthRange(0.0,0.9999);
	}
	bglPushAttrib(GL_POLYGON_BIT);
	if ((grhalfxdown10x >= 0) ^ ((globalorientation&8) != 0) ^ ((globalorientation&4) != 0)) bglFrontFace(GL_CW); else bglFrontFace(GL_CCW);
	bglEnable(GL_CULL_FACE);
	bglCullFace(GL_BACK);

	bglEnable(GL_TEXTURE_2D);
	bglBindTexture(GL_TEXTURE_2D, ptmh->glpic);

	pc[0] = pc[1] = pc[2] = ((float)(numpalookups-min(max(globalshade+m->shadeoff,0),numpalookups)))/((float)numpalookups);
	pc[0] *= (float)hictinting[globalpal].r / 255.0;
	pc[1] *= (float)hictinting[globalpal].g / 255.0;
	pc[2] *= (float)hictinting[globalpal].b / 255.0;
	if (tspr->cstat&2) { if (!(tspr->cstat&512)) pc[3] = 0.66; else pc[3] = 0.33; } else pc[3] = 1.0;
	if (m->usesalpha) //Sprites with alpha in texture
	{
		bglEnable(GL_BLEND);// bglBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		bglEnable(GL_ALPHA_TEST); bglAlphaFunc(GL_GREATER,0.32);
	}
	else
	{
		if (tspr->cstat&2) bglEnable(GL_BLEND); else bglDisable(GL_BLEND);
	}
	bglColor4f(pc[0],pc[1],pc[2],pc[3]);

	for(lptr=m->glcmds;(i=*lptr++);)
	{
		if (i < 0) { bglBegin(GL_TRIANGLE_FAN); i = -i; }
				else { bglBegin(GL_TRIANGLE_STRIP); }
		for(;i>0;i--,lptr+=3)
		{
			bglTexCoord2f(((float *)lptr)[0],((float *)lptr)[1]);
			bglVertex3fv((float *)&vertlist[lptr[2]]);
		}
		bglEnd();
	}

	if (m->usesalpha) bglDisable(GL_ALPHA_TEST);
	bglDisable(GL_CULL_FACE);
	bglPopAttrib();
	if (tspr->cstat&1024)
	{
		bglDepthFunc(GL_LEQUAL); //NEVER,LESS,(,L)EQUAL,GREATER,(NOT,G)EQUAL,ALWAYS
		bglDepthRange(0.0,0.99999);
	}
	bglLoadIdentity();

	return 1;
}

//---------------------------------------- MD2 LIBRARY ENDS ----------------------------------------
//--------------------------------------- MD3 LIBRARY BEGINS ---------------------------------------

static md3model *md3load (int fil)
{
	char *buf, st[BMAX_PATH+2], bst[BMAX_PATH+2];
	long i, j, surfi, ofsurf, bsc, offs[4], leng[4];
	md3model *m;
	md3surf_t *s;

	m = (md3model *)calloc(1,sizeof(md3model)); if (!m) return(0);
	m->mdnum = 3; m->tex = 0; m->scale = .01;

	kread(fil,&m->head,sizeof(md3head_t));
	m->head.id = B_LITTLE32(m->head.id);             m->head.vers = B_LITTLE32(m->head.vers);
	m->head.flags = B_LITTLE32(m->head.flags);       m->head.numframes = B_LITTLE32(m->head.numframes);
	m->head.numtags = B_LITTLE32(m->head.numtags);   m->head.numsurfs = B_LITTLE32(m->head.numsurfs);
	m->head.numskins = B_LITTLE32(m->head.numskins); m->head.frames = (md3frame_t*)B_LITTLE32((long)m->head.frames);
	m->head.tags = (md3tag_t*)B_LITTLE32((long)m->head.tags); m->head.surfs = (md3surf_t*)B_LITTLE32((long)m->head.surfs);
	m->head.eof = B_LITTLE32(m->head.eof);
	
	if ((m->head.id != 0x33504449) && (m->head.vers != 15)) { free(m); return(0); } //"IDP3"

	m->numskins = m->head.numskins; //<- dead code?
	m->numframes = m->head.numframes;

	ofsurf = (long)m->head.surfs;

	klseek(fil,(long)m->head.frames,SEEK_SET); i = m->head.numframes*sizeof(md3frame_t);
	m->head.frames = (md3frame_t *)malloc(i); if (!m->head.frames) { free(m); return(0); }
	kread(fil,m->head.frames,i);

	if (m->head.numtags == 0) m->head.tags = NULL;
	else {
		klseek(fil,(long)m->head.tags,SEEK_SET); i = m->head.numtags*sizeof(md3tag_t);
		m->head.tags = (md3tag_t *)malloc(i); if (!m->head.tags) { free(m->head.frames); free(m); return(0); }
		kread(fil,m->head.tags,i);
	}

	klseek(fil,(long)m->head.surfs,SEEK_SET); i = m->head.numsurfs*sizeof(md3surf_t);
	m->head.surfs = (md3surf_t *)malloc(i); if (!m->head.surfs) { if (m->head.tags) free(m->head.tags); free(m->head.frames); free(m); return(0); }

#if B_BIG_ENDIAN != 0
	{
		long *l;
		
		for (i = m->head.numframes-1; i>=0; i--) {
			l = (long *)&m->head.frames[i].min;
			for (j=3+3+3+1-1;j>=0;j--) l[j] = B_LITTLE32(l[j]);
		}
		
		for (i = m->head.numtags-1; i>=0; i--) {
			l = (long *)&m->head.tags[i].p;
			for (j=3+3+3+3-1;j>=0;j--) l[j] = B_LITTLE32(l[j]);
		}
	}
#endif

	for(surfi=0;surfi<m->head.numsurfs;surfi++)
	{
		s = &m->head.surfs[surfi];
		klseek(fil,ofsurf,SEEK_SET); kread(fil,s,sizeof(md3surf_t));

#if B_BIG_ENDIAN != 0
		{
			long *l;
			s->id = B_LITTLE32(s->id);
			l =	(long *)&s->flags;
			for	(j=1+1+1+1+1+1+1+1+1+1-1;j>=0;j--) l[j] = B_LITTLE32(l[j]);
		}
#endif

		offs[0] = ofsurf+((long)(s->tris   )); leng[0] = s->numtris*sizeof(md3tri_t);
		offs[1] = ofsurf+((long)(s->shaders)); leng[1] = s->numshaders*sizeof(md3shader_t);
		offs[2] = ofsurf+((long)(s->uv     )); leng[2] = s->numverts*sizeof(md3uv_t);
		offs[3] = ofsurf+((long)(s->xyzn   )); leng[3] = s->numframes*s->numverts*sizeof(md3xyzn_t);

		s->tris = (md3tri_t *)malloc(leng[0]+leng[1]+leng[2]+leng[3]);
		if (!s->tris)
		{
			for(surfi--;surfi>=0;surfi--) free(m->head.surfs[surfi].tris);
			if (m->head.tags) free(m->head.tags); free(m->head.frames); free(m); return(0);
		}
		s->shaders = (md3shader_t *)(((long)s->tris   )+leng[0]);
		s->uv      = (md3uv_t     *)(((long)s->shaders)+leng[1]);
		s->xyzn    = (md3xyzn_t   *)(((long)s->uv     )+leng[2]);

		klseek(fil,offs[0],SEEK_SET); kread(fil,s->tris   ,leng[0]);
		klseek(fil,offs[1],SEEK_SET); kread(fil,s->shaders,leng[1]);
		klseek(fil,offs[2],SEEK_SET); kread(fil,s->uv     ,leng[2]);
		klseek(fil,offs[3],SEEK_SET); kread(fil,s->xyzn   ,leng[3]);

#if B_BIG_ENDIAN != 0
		{
			long *l;
			
			for (i=s->numtris-1;i>=0;i--) {
				for (j=2;j>=0;j--) s->tris[i].i[j] = B_LITTLE32(s->tris[i].i[j]);
			}
			for (i=s->numshaders-1;i>=0;i--) {
				s->shaders[i].i = B_LITTLE32(s->shaders[i].i);
			}
			for (i=s->numverts-1;i>=0;i--) {
				l = (long*)&s->uv[i].u;
				l[0] = B_LITTLE32(l[0]);
				l[1] = B_LITTLE32(l[1]);
			}
			for (i=s->numframes*s->numverts-1;i>=0;i--) {
				s->xyzn[i].x = (signed short)B_LITTLE16((unsigned short)s->xyzn[i].x);
				s->xyzn[i].y = (signed short)B_LITTLE16((unsigned short)s->xyzn[i].y);
				s->xyzn[i].z = (signed short)B_LITTLE16((unsigned short)s->xyzn[i].z);
			}
		}
#endif

		maxmodelverts = max(maxmodelverts, s->numverts);
		ofsurf += s->ofsend;
	}

	return(m);
}

static int md3draw (md3model *m, spritetype *tspr)
{
	point3d fp, m0, m1, a0, a1;
	md3xyzn_t *v0, *v1;
	long i, j, k, surfi, *lptr;
	float f, g, k0, k1, k2, k3, k4, k5, k6, k7, mat[16], pc[4];
	md3surf_t *s;
	PTMHead * ptmh = 0;

	updateanimation((md2model *)m,tspr);

		//create current&next frame's vertex list from whole list

	f = m->interpol; g = 1-f;
	m0.x = (1.0/64.0)*m->scale*g; m1.x = (1.0/64.0)*m->scale*f;
	m0.y = (1.0/64.0)*m->scale*g; m1.y = (1.0/64.0)*m->scale*f;
	m0.z = (1.0/64.0)*m->scale*g; m1.z = (1.0/64.0)*m->scale*f;
	a0.x = a0.y = 0; a0.z = m->zadd*m->scale;

    // Parkar: Moved up to be able to use k0 for the y-flipping code
	k0 = tspr->z;
	if ((globalorientation&128) && !((globalorientation&48)==32)) k0 += (float)((tilesizy[tspr->picnum]*tspr->yrepeat)<<1);

    // Parkar: Changed to use the same method as centeroriented sprites
	if (globalorientation&8) //y-flipping
	{
		m0.z = -m0.z; m1.z = -m1.z; a0.z = -a0.z;
		k0 -= (float)((tilesizy[tspr->picnum]*tspr->yrepeat)<<2);
	}
	if (globalorientation&4) { m0.y = -m0.y; m1.y = -m1.y; a0.y = -a0.y; } //x-flipping

	f = ((float)tspr->xrepeat)/64*m->bscale;
	m0.x *= f; m1.x *= f; a0.x *= f; f = -f;   // 20040610: backwards models aren't cool
	m0.y *= f; m1.y *= f; a0.y *= f;
	f = ((float)tspr->yrepeat)/64*m->bscale;
	m0.z *= f; m1.z *= f; a0.z *= f;
	
	// floor aligned
	k1 = tspr->y;
	if((globalorientation&48)==32)
	{
		m0.z = -m0.z; m1.z = -m1.z; a0.z = -a0.z;
		m0.y = -m0.y; m1.y = -m1.y; a0.y = -a0.y;
		f = a0.x; a0.x = a0.z; a0.z = f;
		k1 += (float)((tilesizy[tspr->picnum]*tspr->yrepeat)>>3);
	}

	f = (65536.0*512.0)/((float)xdimen*viewingrange);
	g = 32.0/((float)xdimen*gxyaspect);
	m0.y *= f; m1.y *= f; a0.y = (((float)(tspr->x-globalposx))/  1024.0 + a0.y)*f;
	m0.x *=-f; m1.x *=-f; a0.x = (((float)(k1     -globalposy))/ -1024.0 + a0.x)*-f;
	m0.z *= g; m1.z *= g; a0.z = (((float)(k0     -globalposz))/-16384.0 + a0.z)*g;

	k0 = ((float)(tspr->x-globalposx))*f/1024.0;
	k1 = ((float)(tspr->y-globalposy))*f/1024.0;
	f = gcosang2*gshang;
	g = gsinang2*gshang;
	k4 = (float)sintable[(tspr->ang+spriteext[tspr->owner].angoff+1024)&2047] / 16384.0;
	k5 = (float)sintable[(tspr->ang+spriteext[tspr->owner].angoff+ 512)&2047] / 16384.0;
	k2 = k0*(1-k4)+k1*k5;
	k3 = k1*(1-k4)-k0*k5;
	k6 = f*gstang - gsinang*gctang; k7 = g*gstang + gcosang*gctang;
	mat[0] = k4*k6 + k5*k7; mat[4] = gchang*gstang; mat[ 8] = k4*k7 - k5*k6; mat[12] = k2*k6 + k3*k7;
	k6 = f*gctang + gsinang*gstang; k7 = g*gctang - gcosang*gstang;
	mat[1] = k4*k6 + k5*k7; mat[5] = gchang*gctang; mat[ 9] = k4*k7 - k5*k6; mat[13] = k2*k6 + k3*k7;
	k6 =           gcosang2*gchang; k7 =           gsinang2*gchang;
	mat[2] = k4*k6 + k5*k7; mat[6] =-gshang;        mat[10] = k4*k7 - k5*k6; mat[14] = k2*k6 + k3*k7;

	mat[12] += a0.y*mat[0] + a0.z*mat[4] + a0.x*mat[ 8];
	mat[13] += a0.y*mat[1] + a0.z*mat[5] + a0.x*mat[ 9];
	mat[14] += a0.y*mat[2] + a0.z*mat[6] + a0.x*mat[10];

	// floor aligned
	if((globalorientation&48)==32)
	{
		f = mat[4]; mat[4] = mat[8]*16.0; mat[8] = -f*(1.0/16.0);
		f = mat[5]; mat[5] = mat[9]*16.0; mat[9] = -f*(1.0/16.0);
		f = mat[6]; mat[6] = mat[10]*16.0; mat[10] = -f*(1.0/16.0);
	}
    
	//Mirrors
	if (grhalfxdown10x < 0) { mat[0] = -mat[0]; mat[4] = -mat[4]; mat[8] = -mat[8]; mat[12] = -mat[12]; }
	
//------------
	//bit 10 is an ugly hack in game.c\animatesprites telling MD2SPRITE
	//to use Z-buffer hacks to hide overdraw problems with the shadows
	if (tspr->cstat&1024)
	{
		bglDepthFunc(GL_LESS); //NEVER,LESS,(,L)EQUAL,GREATER,(NOT,G)EQUAL,ALWAYS
		bglDepthRange(0.0,0.9999);
	}
	bglPushAttrib(GL_POLYGON_BIT);
	if ((grhalfxdown10x >= 0) ^ ((globalorientation&8) != 0) ^ ((globalorientation&4) != 0)) bglFrontFace(GL_CW); else bglFrontFace(GL_CCW);
	bglEnable(GL_CULL_FACE);
	bglCullFace(GL_BACK);

	bglEnable(GL_TEXTURE_2D);

	pc[0] = pc[1] = pc[2] = ((float)(numpalookups-min(max(globalshade+m->shadeoff,0),numpalookups)))/((float)numpalookups);
	pc[0] *= (float)hictinting[globalpal].r / 255.0;
	pc[1] *= (float)hictinting[globalpal].g / 255.0;
	pc[2] *= (float)hictinting[globalpal].b / 255.0;
	if (tspr->cstat&2) { if (!(tspr->cstat&512)) pc[3] = 0.66; else pc[3] = 0.33; } else pc[3] = 1.0;
	if (m->usesalpha) //Sprites with alpha in texture
	{
		bglEnable(GL_BLEND);// bglBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		bglEnable(GL_ALPHA_TEST); bglAlphaFunc(GL_GREATER,0.32);
	}
	else
	{
		if (tspr->cstat&2) bglEnable(GL_BLEND); else bglDisable(GL_BLEND);
	}
	bglColor4f(pc[0],pc[1],pc[2],pc[3]);
//------------

	for(surfi=0;surfi<m->head.numsurfs;surfi++)
	{
		s = &m->head.surfs[surfi];
		v0 = &s->xyzn[m->cframe*s->numverts];
		v1 = &s->xyzn[m->nframe*s->numverts];
#if 0
		for(i=s->numverts-1;i>=0;i--) //interpolate (for animation) & transform to Build coords
		{
			fp.z = ((float)v0[i].x)*m0.x + ((float)v1[i].x)*m1.x;
			fp.y = ((float)v0[i].z)*m0.z + ((float)v1[i].z)*m1.z;
			fp.x = ((float)v0[i].y)*m0.y + ((float)v1[i].y)*m1.y;
			vertlist[i].x = fp.x*mat[0] + fp.y*mat[4] + fp.z*mat[ 8] + mat[12];
			vertlist[i].y = fp.x*mat[1] + fp.y*mat[5] + fp.z*mat[ 9] + mat[13];
			vertlist[i].z = fp.x*mat[2] + fp.y*mat[6] + fp.z*mat[10] + mat[14];
		}
#else
		for(i=s->numverts-1;i>=0;i--) //interpolate (for animation) & transform to Build coords
		{
			vertlist[i].z = ((float)v0[i].x)*m0.x + ((float)v1[i].x)*m1.x;
			vertlist[i].y = ((float)v0[i].z)*m0.z + ((float)v1[i].z)*m1.z;
			vertlist[i].x = ((float)v0[i].y)*m0.y + ((float)v1[i].y)*m1.y;
		}
		bglMatrixMode(GL_MODELVIEW); //Let OpenGL (and perhaps hardware :) handle the matrix rotation
		mat[3] = mat[7] = mat[11] = 0.f; mat[15] = 1.f; bglLoadMatrixf(mat);
#endif


#if 0
		//precalc:
	float sinlut256[256+(256>>2)];
	for(i=0;i<sizeof(sinlut256)/sizeof(sinlut256[0]);i++) sinlut256[i] = sin(((float)i)*(PI*2/255.0));

		//normal to xyz:
	md3vert_t *mv = &md3vert[?];
	z = sinlut256[mv->nlng+(256>>2)];
	x = sinlut256[mv->nlat]*z;
	y = sinlut256[mv->nlat+(256>>2)]*z;
	z = sinlut256[mv->nlng];
#endif

		ptmh = mdloadskin((md2model *)m,tile2model[tspr->picnum].skinnum,globalpal,surfi);
		//ptmh = mdloadskin((md2model *)m,tile2model[tspr->picnum].skinnum,surfi); //hack for testing multiple surfaces per MD3
		if (!ptmh || !ptmh->glpic) continue;
		bglBindTexture(GL_TEXTURE_2D, ptmh->glpic);

		bglBegin(GL_TRIANGLES);
		for(i=s->numtris-1;i>=0;i--)
			for(j=0;j<3;j++)
			{
				k = s->tris[i].i[j];
				bglTexCoord2f(s->uv[k].u,s->uv[k].v);
				bglVertex3fv((float *)&vertlist[k]);
			}
		bglEnd();
	}

//------------
	if (m->usesalpha) bglDisable(GL_ALPHA_TEST);
	bglDisable(GL_CULL_FACE);
	bglPopAttrib();
	if (tspr->cstat&1024)
	{
		bglDepthFunc(GL_LEQUAL); //NEVER,LESS,(,L)EQUAL,GREATER,(NOT,G)EQUAL,ALWAYS
		bglDepthRange(0.0,0.99999);
	}
	bglLoadIdentity();

	return 1;
}

static void md3free (md3model *m)
{
	mdanim_t *anim, *nanim = NULL;
	mdskinmap_t *sk, *nsk = NULL;
	md3surf_t *s;
	long surfi;

	if (!m) return;

	for(anim=m->animations; anim; anim=nanim)
	{
		nanim = anim->next;
		free(anim);
	}
	for(sk=m->skinmap; sk; sk=nsk)
	{
		nsk = sk->next;
		free(sk->fn);
		free(sk);
	}

	if (m->head.surfs)
	{
		for(surfi=m->head.numsurfs-1;surfi>=0;surfi--)
		{
			s = &m->head.surfs[surfi];
			if (s->tris) free(s->tris);
		}
		free(m->head.surfs);
	}
	if (m->head.tags) free(m->head.tags);
	if (m->head.frames) free(m->head.frames);

	if (m->tex) free(m->tex);

	free(m);
}

//---------------------------------------- MD3 LIBRARY ENDS ----------------------------------------
//--------------------------------------- VOX LIBRARY BEGINS ---------------------------------------

	//For loading/conversion only
static long xsiz, ysiz, zsiz, yzsiz, *vbit = 0; //vbit: 1 bit per voxel: 0=air,1=solid
static float xpiv, ypiv, zpiv; //Might want to use more complex/unique names!
static long *vcolhashead = 0, vcolhashsizm1;
typedef struct { long p, c, n; } voxcol_t;
static voxcol_t *vcol = 0; long vnum = 0, vmax = 0;
typedef struct { short x, y; } spoint2d;
static spoint2d *shp;
static long *shcntmal, *shcnt = 0, shcntp;
static long mytexo5, *zbit, gmaxx, gmaxy, garea, pow2m1[33];
static voxmodel *gvox;

	//pitch must equal xsiz*4
unsigned gloadtex (long *picbuf, long xsiz, long ysiz, long is8bit, long dapal)
{
	unsigned rtexid;
	coltype *pic, *pic2;
	unsigned char *cptr;
	long i;

	pic = (coltype *)picbuf; //Correct for GL's RGB order; also apply gamma here..
	pic2 = (coltype *)malloc(xsiz*ysiz*sizeof(long)); if (!pic2) return((unsigned)-1);
	cptr = (unsigned char*)&britable[gammabrightness ? 0 : curbrightness][0];
	if (!is8bit)
	{
	for(i=xsiz*ysiz-1;i>=0;i--)
	{
		pic2[i].b = cptr[pic[i].r];
		pic2[i].g = cptr[pic[i].g];
		pic2[i].r = cptr[pic[i].b];
		pic2[i].a = 255;
		}
	}
	else
	{
		if (palookup[dapal] == 0) dapal = 0;
		for(i=xsiz*ysiz-1;i>=0;i--)
		{
			pic2[i].b = cptr[palette[(long)palookup[dapal][pic[i].a]*3+2]*4];
			pic2[i].g = cptr[palette[(long)palookup[dapal][pic[i].a]*3+1]*4];
			pic2[i].r = cptr[palette[(long)palookup[dapal][pic[i].a]*3+0]*4];
			pic2[i].a = 255;
		}
	}

	bglGenTextures(1,(GLuint*)&rtexid);
	bglBindTexture(GL_TEXTURE_2D,rtexid);
	bglTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	bglTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	bglTexImage2D(GL_TEXTURE_2D,0,4,xsiz,ysiz,0,GL_RGBA,GL_UNSIGNED_BYTE,(unsigned char *)pic2);
	free(pic2);
	return(rtexid);
}

static long getvox (long x, long y, long z)
{
	z += x*yzsiz + y*zsiz;
	for(x=vcolhashead[(z*214013)&vcolhashsizm1];x>=0;x=vcol[x].n)
		if (vcol[x].p == z) return(vcol[x].c);
	return(0x808080);
}

static void putvox (long x, long y, long z, long col)
{
	if (vnum >= vmax) { vmax = max(vmax<<1,4096); vcol = (voxcol_t *)realloc(vcol,vmax*sizeof(voxcol_t)); }

	z += x*yzsiz + y*zsiz;
	vcol[vnum].p = z; z = ((z*214013)&vcolhashsizm1);
	vcol[vnum].c = col;
	vcol[vnum].n = vcolhashead[z]; vcolhashead[z] = vnum++;
}

	//Set all bits in vbit from (x,y,z0) to (x,y,z1-1) to 0's
static void setzrange0 (long *lptr, long z0, long z1)
{
	long z, ze;
	if (!((z0^z1)&~31)) { lptr[z0>>5] &= ((~(-1<<SHIFTMOD32(z0)))|(-1<<SHIFTMOD32(z1))); return; }
	z = (z0>>5); ze = (z1>>5);
	lptr[z] &=~(-1<<SHIFTMOD32(z0)); for(z++;z<ze;z++) lptr[z] = 0;
	lptr[z] &= (-1<<SHIFTMOD32(z1));
}

	//Set all bits in vbit from (x,y,z0) to (x,y,z1-1) to 1's
static void setzrange1 (long *lptr, long z0, long z1)
{
	long z, ze;
	if (!((z0^z1)&~31)) { lptr[z0>>5] |= ((~(-1<<SHIFTMOD32(z1)))&(-1<<SHIFTMOD32(z0))); return; }
	z = (z0>>5); ze = (z1>>5);
	lptr[z] |= (-1<<SHIFTMOD32(z0)); for(z++;z<ze;z++) lptr[z] = -1;
	lptr[z] |=~(-1<<SHIFTMOD32(z1));
}

static long isrectfree (long x0, long y0, long dx, long dy)
{
#if 0
	long i, j, x;
	i = y0*gvox->mytexx + x0;
	for(dy=0;dy;dy--,i+=gvox->mytexx)
		for(x=0;x<dx;x++) { j = i+x; if (zbit[j>>5]&(1<<SHIFTMOD32(j))) return(0); }
#else
	long i, c, m, m1, x;

	i = y0*mytexo5 + (x0>>5); dx += x0-1; c = (dx>>5) - (x0>>5);
	m = ~pow2m1[x0&31]; m1 = pow2m1[(dx&31)+1];
	if (!c) { for(m&=m1;dy;dy--,i+=mytexo5) if (zbit[i]&m) return(0); }
	else
	{  for(;dy;dy--,i+=mytexo5)
		{
			if (zbit[i]&m) return(0);
			for(x=1;x<c;x++) if (zbit[i+x]) return(0);
			if (zbit[i+x]&m1) return(0);
		}
	}
#endif
	return(1);
}

static void setrect (long x0, long y0, long dx, long dy)
{
#if 0
	long i, j, y;
	i = y0*gvox->mytexx + x0;
	for(y=0;y<dy;y++,i+=gvox->mytexx)
		for(x=0;x<dx;x++) { j = i+x; zbit[j>>5] |= (1<<SHIFTMOD32(j)); }
#else
	long i, c, m, m1, x;

	i = y0*mytexo5 + (x0>>5); dx += x0-1; c = (dx>>5) - (x0>>5);
	m = ~pow2m1[x0&31]; m1 = pow2m1[(dx&31)+1];
	if (!c) { for(m&=m1;dy;dy--,i+=mytexo5) zbit[i] |= m; }
	else
	{  for(;dy;dy--,i+=mytexo5)
		{
			zbit[i] |= m;
			for(x=1;x<c;x++) zbit[i+x] = -1;
			zbit[i+x] |= m1;
		}
	}
#endif
}

static void cntquad (long x0, long y0, long z0, long UNUSED(x1), long UNUSED(y1), long UNUSED(z1), long x2, long y2, long z2, long UNUSED(face))
{
	long x, y, z;

	x = labs(x2-x0); y = labs(y2-y0); z = labs(z2-z0);
	if (!x) x = z; else if (!y) y = z;
	if (x < y) { z = x; x = y; y = z; }
	shcnt[y*shcntp+x]++;
	if (x > gmaxx) gmaxx = x;
	if (y > gmaxy) gmaxy = y;
	garea += (x+(VOXBORDWIDTH<<1))*(y+(VOXBORDWIDTH<<1));
	gvox->qcnt++;
}

static void addquad (long x0, long y0, long z0, long x1, long y1, long z1, long x2, long y2, long z2, long face)
{
	long i, j, x, y, z, xx, yy, nx = 0, ny = 0, nz = 0, *lptr;
	voxrect_t *qptr;

	x = labs(x2-x0); y = labs(y2-y0); z = labs(z2-z0);
	if (!x) { x = y; y = z; i = 0; } else if (!y) { y = z; i = 1; } else i = 2;
	if (x < y) { z = x; x = y; y = z; i += 3; }
	z = shcnt[y*shcntp+x]++;
	lptr = &gvox->mytex[(shp[z].y+VOXBORDWIDTH)*gvox->mytexx+(shp[z].x+VOXBORDWIDTH)];
	switch(face)
	{
		case 0: ny = y1; x2 = x0; x0 = x1; x1 = x2; break;
		case 1: ny = y0; y0++; y1++; y2++; break;
		case 2: nz = z1; y0 = y2; y2 = y1; y1 = y0; z0++; z1++; z2++; break;
		case 3: nz = z0; break;
		case 4: nx = x1; y2 = y0; y0 = y1; y1 = y2; x0++; x1++; x2++; break;
		case 5: nx = x0; break;
	}
	for(yy=0;yy<y;yy++,lptr+=gvox->mytexx)
		for(xx=0;xx<x;xx++)
		{
			switch(face)
			{
				case 0: if (i < 3) { nx = x1+x-1-xx; nz = z1+yy;   } //back
								  else { nx = x1+y-1-yy; nz = z1+xx;   } break;
				case 1: if (i < 3) { nx = x0+xx;     nz = z0+yy;   } //front
								  else { nx = x0+yy;     nz = z0+xx;   } break;
				case 2: if (i < 3) { nx = x1-x+xx;   ny = y1-1-yy; } //bot
								  else { nx = x1-1-yy;   ny = y1-1-xx; } break;
				case 3: if (i < 3) { nx = x0+xx;     ny = y0+yy;   } //top
								  else { nx = x0+yy;     ny = y0+xx;   } break;
				case 4: if (i < 3) { ny = y1+x-1-xx; nz = z1+yy;   } //right
								  else { ny = y1+y-1-yy; nz = z1+xx;   } break;
				case 5: if (i < 3) { ny = y0+xx;     nz = z0+yy;   } //left
								  else { ny = y0+yy;     nz = z0+xx;   } break;
			}
			lptr[xx] = getvox(nx,ny,nz);
		}

		//Extend borders horizontally
	for(yy=VOXBORDWIDTH;yy<y+VOXBORDWIDTH;yy++)
		for(xx=0;xx<VOXBORDWIDTH;xx++)
		{
			lptr = &gvox->mytex[(shp[z].y+yy)*gvox->mytexx+shp[z].x];
			lptr[xx] = lptr[VOXBORDWIDTH]; lptr[xx+x+VOXBORDWIDTH] = lptr[x-1+VOXBORDWIDTH];
		}
		//Extend borders vertically
	for(yy=0;yy<VOXBORDWIDTH;yy++)
	{
		memcpy(&gvox->mytex[(shp[z].y+yy)*gvox->mytexx+shp[z].x],
				 &gvox->mytex[(shp[z].y+VOXBORDWIDTH)*gvox->mytexx+shp[z].x],
				 (x+(VOXBORDWIDTH<<1))<<2);
		memcpy(&gvox->mytex[(shp[z].y+y+yy+VOXBORDWIDTH)*gvox->mytexx+shp[z].x],
				 &gvox->mytex[(shp[z].y+y-1+VOXBORDWIDTH)*gvox->mytexx+shp[z].x],
				 (x+(VOXBORDWIDTH<<1))<<2);
	}

	qptr = &gvox->quad[gvox->qcnt];
	qptr->v[0].x = x0; qptr->v[0].y = y0; qptr->v[0].z = z0;
	qptr->v[1].x = x1; qptr->v[1].y = y1; qptr->v[1].z = z1;
	qptr->v[2].x = x2; qptr->v[2].y = y2; qptr->v[2].z = z2;
	for(j=0;j<3;j++) { qptr->v[j].u = shp[z].x+VOXBORDWIDTH; qptr->v[j].v = shp[z].y+VOXBORDWIDTH; }
	if (i < 3) qptr->v[1].u += x; else qptr->v[1].v += y;
	qptr->v[2].u += x; qptr->v[2].v += y;

	qptr->v[3].u = qptr->v[0].u - qptr->v[1].u + qptr->v[2].u;
	qptr->v[3].v = qptr->v[0].v - qptr->v[1].v + qptr->v[2].v;
	qptr->v[3].x = qptr->v[0].x - qptr->v[1].x + qptr->v[2].x;
	qptr->v[3].y = qptr->v[0].y - qptr->v[1].y + qptr->v[2].y;
	qptr->v[3].z = qptr->v[0].z - qptr->v[1].z + qptr->v[2].z;
	if (gvox->qfacind[face] < 0) gvox->qfacind[face] = gvox->qcnt;
	gvox->qcnt++;

}

static long isolid (long x, long y, long z)
{
	if ((unsigned long)x >= (unsigned long)xsiz) return(0);
	if ((unsigned long)y >= (unsigned long)ysiz) return(0);
	if ((unsigned long)z >= (unsigned long)zsiz) return(0);
	z += x*yzsiz + y*zsiz; return(vbit[z>>5]&(1<<SHIFTMOD32(z)));
}

static voxmodel *vox2poly ()
{
	long i, j, x, y, z, v, ov, oz = 0, cnt, sc, x0, y0, dx, dy, i0, i1, *bx0, *by0;
	void (*daquad)(long, long, long, long, long, long, long, long, long, long);
	coltype *pic;
	unsigned char *cptr, ch;

	gvox = (voxmodel *)malloc(sizeof(voxmodel)); if (!gvox) return(0);
	memset(gvox,0,sizeof(voxmodel));

		//x is largest dimension, y is 2nd largest dimension
	x = xsiz; y = ysiz; z = zsiz;
	if ((x < y) && (x < z)) x = z; else if (y < z) y = z;
	if (x < y) { z = x; x = y; y = z; }
	shcntp = x; i = x*y*sizeof(long);
	shcntmal = (long *)malloc(i); if (!shcntmal) { free(gvox); return(0); }
	memset(shcntmal,0,i); shcnt = &shcntmal[-shcntp-1];
	gmaxx = gmaxy = garea = 0;

	if (pow2m1[32] != -1) { for(i=0;i<32;i++) pow2m1[i] = (1<<i)-1; pow2m1[32] = -1; }
	for(i=0;i<7;i++) gvox->qfacind[i] = -1;

	i = ((max(ysiz,zsiz)+1)<<2);
	bx0 = (long *)malloc(i<<1); if (!bx0) { free(gvox); return(0); }
	by0 = (long *)(((long)bx0)+i);

	for(cnt=0;cnt<2;cnt++)
	{
		if (!cnt) daquad = cntquad;
			  else daquad = addquad;
		gvox->qcnt = 0;

		memset(by0,-1,(max(ysiz,zsiz)+1)<<2); v = 0;

		for(i=-1;i<=1;i+=2)
			for(y=0;y<ysiz;y++)
				for(x=0;x<=xsiz;x++)
					for(z=0;z<=zsiz;z++)
					{
						ov = v; v = (isolid(x,y,z) && (!isolid(x,y+i,z)));
						if ((by0[z] >= 0) && ((by0[z] != oz) || (v >= ov)))
							{ daquad(bx0[z],y,by0[z],x,y,by0[z],x,y,z,i>=0); by0[z] = -1; }
						if (v > ov) oz = z; else if ((v < ov) && (by0[z] != oz)) { bx0[z] = x; by0[z] = oz; }
					}

		for(i=-1;i<=1;i+=2)
			for(z=0;z<zsiz;z++)
				for(x=0;x<=xsiz;x++)
					for(y=0;y<=ysiz;y++)
					{
						ov = v; v = (isolid(x,y,z) && (!isolid(x,y,z-i)));
						if ((by0[y] >= 0) && ((by0[y] != oz) || (v >= ov)))
							{ daquad(bx0[y],by0[y],z,x,by0[y],z,x,y,z,(i>=0)+2); by0[y] = -1; }
						if (v > ov) oz = y; else if ((v < ov) && (by0[y] != oz)) { bx0[y] = x; by0[y] = oz; }
					}

		for(i=-1;i<=1;i+=2)
			for(x=0;x<xsiz;x++)
				for(y=0;y<=ysiz;y++)
					for(z=0;z<=zsiz;z++)
					{
						ov = v; v = (isolid(x,y,z) && (!isolid(x-i,y,z)));
						if ((by0[z] >= 0) && ((by0[z] != oz) || (v >= ov)))
							{ daquad(x,bx0[z],by0[z],x,y,by0[z],x,y,z,(i>=0)+4); by0[z] = -1; }
						if (v > ov) oz = z; else if ((v < ov) && (by0[z] != oz)) { bx0[z] = y; by0[z] = oz; }
					}

		if (!cnt)
		{
			shp = (spoint2d *)malloc(gvox->qcnt*sizeof(spoint2d));
			if (!shp) { free(bx0); free(gvox); return(0); }

			sc = 0;
			for(y=gmaxy;y;y--)
				for(x=gmaxx;x>=y;x--)
				{
					i = shcnt[y*shcntp+x]; shcnt[y*shcntp+x] = sc; //shcnt changes from counter to head index
					for(;i>0;i--) { shp[sc].x = x; shp[sc].y = y; sc++; }
				}

			for(gvox->mytexx=32;gvox->mytexx<(gmaxx+(VOXBORDWIDTH<<1));gvox->mytexx<<=1);
			for(gvox->mytexy=32;gvox->mytexy<(gmaxy+(VOXBORDWIDTH<<1));gvox->mytexy<<=1);
			while (gvox->mytexx*gvox->mytexy*8 < garea*9) //This should be sufficient to fit most skins...
			{
skindidntfit:;
				if (gvox->mytexx <= gvox->mytexy) gvox->mytexx <<= 1; else gvox->mytexy <<= 1;
			}
			mytexo5 = (gvox->mytexx>>5);

			i = (((gvox->mytexx*gvox->mytexy+31)>>5)<<2);
			zbit = (long *)malloc(i); if (!zbit) { free(bx0); free(gvox); free(shp); return(0); }
			memset(zbit,0,i);

			v = gvox->mytexx*gvox->mytexy;
			for(z=0;z<sc;z++)
			{
				dx = shp[z].x+(VOXBORDWIDTH<<1); dy = shp[z].y+(VOXBORDWIDTH<<1); i = v;
				do
				{
#if (VOXUSECHAR != 0)
					x0 = (((rand()&32767)*(min(gvox->mytexx,255)-dx))>>15);
					y0 = (((rand()&32767)*(min(gvox->mytexy,255)-dy))>>15);
#else
					x0 = (((rand()&32767)*(gvox->mytexx+1-dx))>>15);
					y0 = (((rand()&32767)*(gvox->mytexy+1-dy))>>15);
#endif
					i--;
					if (i < 0) //Time-out! Very slow if this happens... but at least it still works :P
					{
						free(zbit);

							//Re-generate shp[].x/y (box sizes) from shcnt (now head indices) for next pass :/
						j = 0;
						for(y=gmaxy;y;y--)
							for(x=gmaxx;x>=y;x--)
							{
								i = shcnt[y*shcntp+x];
								for(;j<i;j++) { shp[j].x = x0; shp[j].y = y0; }
								x0 = x; y0 = y;
							}
						for(;j<sc;j++) { shp[j].x = x0; shp[j].y = y0; }

						goto skindidntfit;
					}
				} while (!isrectfree(x0,y0,dx,dy));
				while ((y0) && (isrectfree(x0,y0-1,dx,1))) y0--;
				while ((x0) && (isrectfree(x0-1,y0,1,dy))) x0--;
				setrect(x0,y0,dx,dy);
				shp[z].x = x0; shp[z].y = y0; //Overwrite size with top-left location
			}

			gvox->quad = (voxrect_t *)malloc(gvox->qcnt*sizeof(voxrect_t));
			if (!gvox->quad) { free(zbit); free(shp); free(bx0); free(gvox); return(0); }

			gvox->mytex = (long *)malloc(gvox->mytexx*gvox->mytexy*sizeof(long));
			if (!gvox->mytex) { free(gvox->quad); free(zbit); free(shp); free(bx0); free(gvox); return(0); }
		}
	}
	free(shp); free(zbit); free(bx0);
	return(gvox);
}

static long loadvox (const char *filnam)
{
	long i, j, k, x, y, z, pal[256], fil;
	unsigned char c[3], *tbuf;

	fil = kopen4load((char *)filnam,0); if (fil < 0) return(-1);
	kread(fil,&xsiz,4); xsiz = B_LITTLE32(xsiz);
	kread(fil,&ysiz,4); ysiz = B_LITTLE32(ysiz);
	kread(fil,&zsiz,4); zsiz = B_LITTLE32(zsiz);
	xpiv = ((float)xsiz)*.5;
	ypiv = ((float)ysiz)*.5;
	zpiv = ((float)zsiz)*.5;

	klseek(fil,-768,SEEK_END);
	for(i=0;i<256;i++)
		{ kread(fil,c,3); pal[i] = (((long)c[0])<<18)+(((long)c[1])<<10)+(((long)c[2])<<2)+(i<<24); }
	pal[255] = -1;

	vcolhashsizm1 = 8192-1;
	vcolhashead = (long *)malloc((vcolhashsizm1+1)*sizeof(long)); if (!vcolhashead) { kclose(fil); return(-1); }
	memset(vcolhashead,-1,(vcolhashsizm1+1)*sizeof(long));

	yzsiz = ysiz*zsiz; i = ((xsiz*yzsiz+31)>>3);
	vbit = (long *)malloc(i); if (!vbit) { kclose(fil); return(-1); }
	memset(vbit,0,i);

	tbuf = (unsigned char *)malloc(zsiz*sizeof(char)); if (!tbuf) { kclose(fil); return(-1); }

	klseek(fil,12,SEEK_SET);
	for(x=0;x<xsiz;x++)
		for(y=0,j=x*yzsiz;y<ysiz;y++,j+=zsiz)
		{
			kread(fil,tbuf,zsiz);
			for(z=zsiz-1;z>=0;z--)
				{ if (tbuf[z] != 255) { i = j+z; vbit[i>>5] |= (1<<SHIFTMOD32(i)); } }
		}

	klseek(fil,12,SEEK_SET);
	for(x=0;x<xsiz;x++)
		for(y=0,j=x*yzsiz;y<ysiz;y++,j+=zsiz)
		{
			kread(fil,tbuf,zsiz);
			for(z=0;z<zsiz;z++)
			{
				if (tbuf[z] == 255) continue;
				if ((!x) || (!y) || (!z) || (x == xsiz-1) || (y == ysiz-1) || (z == zsiz-1))
					{ putvox(x,y,z,pal[tbuf[z]]); continue; }
				k = j+z;
				if ((!(vbit[(k-yzsiz)>>5]&(1<<SHIFTMOD32(k-yzsiz)))) ||
					 (!(vbit[(k+yzsiz)>>5]&(1<<SHIFTMOD32(k+yzsiz)))) ||
					 (!(vbit[(k- zsiz)>>5]&(1<<SHIFTMOD32(k- zsiz)))) ||
					 (!(vbit[(k+ zsiz)>>5]&(1<<SHIFTMOD32(k+ zsiz)))) ||
					 (!(vbit[(k-    1)>>5]&(1<<SHIFTMOD32(k-    1)))) ||
					 (!(vbit[(k+    1)>>5]&(1<<SHIFTMOD32(k+    1)))))
					{ putvox(x,y,z,pal[tbuf[z]]); continue; }
			}
		}

	free(tbuf); kclose(fil); return(0);
}

static long loadkvx (const char *filnam)
{
	long i, j, k, x, y, z, pal[256], z0, z1, mip1leng, ysizp1, fil;
	unsigned short *xyoffs;
	unsigned char c[3], *tbuf, *cptr;

	fil = kopen4load((char *)filnam,0); if (fil < 0) return(-1);
	kread(fil,&mip1leng,4); mip1leng = B_LITTLE32(mip1leng);
	kread(fil,&xsiz,4);     xsiz = B_LITTLE32(xsiz);
	kread(fil,&ysiz,4);     ysiz = B_LITTLE32(ysiz);
	kread(fil,&zsiz,4);     zsiz = B_LITTLE32(zsiz);
	kread(fil,&i,4); xpiv = ((float)B_LITTLE32(i))/256.0;
	kread(fil,&i,4); ypiv = ((float)B_LITTLE32(i))/256.0;
	kread(fil,&i,4); zpiv = ((float)B_LITTLE32(i))/256.0;
	klseek(fil,(xsiz+1)<<2,SEEK_CUR);
	ysizp1 = ysiz+1;
	i = xsiz*ysizp1*sizeof(short);
	xyoffs = (unsigned short *)malloc(i); if (!xyoffs) { kclose(fil); return(-1); }
	kread(fil,xyoffs,i); for (i=i/sizeof(short)-1; i>=0; i--) xyoffs[i] = B_LITTLE16(xyoffs[i]);

	klseek(fil,-768,SEEK_END);
	for(i=0;i<256;i++)
		{ kread(fil,c,3); pal[i] = B_LITTLE32((((long)c[0])<<18)+(((long)c[1])<<10)+(((long)c[2])<<2)+(i<<24)); }

	yzsiz = ysiz*zsiz; i = ((xsiz*yzsiz+31)>>3);
	vbit = (long *)malloc(i); if (!vbit) { free(xyoffs); kclose(fil); return(-1); }
	memset(vbit,0,i);

	for(vcolhashsizm1=4096;vcolhashsizm1<(mip1leng>>1);vcolhashsizm1<<=1); vcolhashsizm1--; //approx to numvoxs!
	vcolhashead = (long *)malloc((vcolhashsizm1+1)*sizeof(long)); if (!vcolhashead) { free(xyoffs); kclose(fil); return(-1); }
	memset(vcolhashead,-1,(vcolhashsizm1+1)*sizeof(long));

	klseek(fil,28+((xsiz+1)<<2)+((ysizp1*xsiz)<<1),SEEK_SET);

	i = kfilelength(fil)-ktell(fil);
	tbuf = (unsigned char *)malloc(i); if (!tbuf) { free(xyoffs); kclose(fil); return(-1); }
	kread(fil,tbuf,i); kclose(fil);

	cptr = tbuf;
	for(x=0;x<xsiz;x++) //Set surface voxels to 1 else 0
		for(y=0,j=x*yzsiz;y<ysiz;y++,j+=zsiz)
		{
			i = xyoffs[x*ysizp1+y+1] - xyoffs[x*ysizp1+y]; if (!i) continue;
			z1 = 0;
			while (i)
			{
				z0 = (long)cptr[0]; k = (long)cptr[1]; cptr += 3;
				if (!(cptr[-1]&16)) setzrange1(vbit,j+z1,j+z0);
				i -= k+3; z1 = z0+k;
				setzrange1(vbit,j+z0,j+z1);
				for(z=z0;z<z1;z++) putvox(x,y,z,pal[*cptr++]);
			}
		}

	free(tbuf); free(xyoffs); return(0);
}

static long loadkv6 (const char *filnam)
{
	long i, j, x, y, z, numvoxs, z0, z1, fil;
	unsigned short *ylen;
	unsigned char c[8];

	fil = kopen4load((char *)filnam,0); if (fil < 0) return(-1);
	kread(fil,&i,4); if (B_LITTLE32(i) != 0x6c78764b) { kclose(fil); return(-1); } //Kvxl
	kread(fil,&xsiz,4);    xsiz = B_LITTLE32(xsiz);
	kread(fil,&ysiz,4);    ysiz = B_LITTLE32(ysiz);
	kread(fil,&zsiz,4);    zsiz = B_LITTLE32(zsiz);
	kread(fil,&i,4);       xpiv = (float)(B_LITTLE32(i));
	kread(fil,&i,4);       ypiv = (float)(B_LITTLE32(i));
	kread(fil,&i,4);       zpiv = (float)(B_LITTLE32(i));
	kread(fil,&numvoxs,4); numvoxs = B_LITTLE32(numvoxs);

	ylen = (unsigned short *)malloc(xsiz*ysiz*sizeof(short));
	if (!ylen) { kclose(fil); return(-1); }

	klseek(fil,32+(numvoxs<<3)+(xsiz<<2),SEEK_SET);
	kread(fil,ylen,xsiz*ysiz*sizeof(short)); for (i=xsiz*ysiz-1; i>=0; i--) ylen[i] = B_LITTLE16(ylen[i]);
	klseek(fil,32,SEEK_SET);

	yzsiz = ysiz*zsiz; i = ((xsiz*yzsiz+31)>>3);
	vbit = (long *)malloc(i); if (!vbit) { free(ylen); kclose(fil); return(-1); }
	memset(vbit,0,i);

	for(vcolhashsizm1=4096;vcolhashsizm1<numvoxs;vcolhashsizm1<<=1); vcolhashsizm1--;
	vcolhashead = (long *)malloc((vcolhashsizm1+1)*sizeof(long)); if (!vcolhashead) { free(ylen); kclose(fil); return(-1); }
	memset(vcolhashead,-1,(vcolhashsizm1+1)*sizeof(long));

	for(x=0;x<xsiz;x++)
		for(y=0,j=x*yzsiz;y<ysiz;y++,j+=zsiz)
		{
			z1 = zsiz;
			for(i=ylen[x*ysiz+y];i>0;i--)
			{
				kread(fil,c,8); //b,g,r,a,z_lo,z_hi,vis,dir
				z0 = B_LITTLE16(*(unsigned short *)&c[4]);
				if (!(c[6]&16)) setzrange1(vbit,j+z1,j+z0);
				vbit[(j+z0)>>5] |= (1<<SHIFTMOD32(j+z0));
				putvox(x,y,z0,B_LITTLE32(*(long *)&c[0])&0xffffff);
				z1 = z0+1;
			}
		}
	free(ylen); kclose(fil); return(0);
}

#if 0
	//While this code works, it's way too slow and can only cause trouble.
static long loadvxl (const char *filnam)
{
	long i, j, x, y, z, fil;
	unsigned char *v, *vbuf;

	fil = kopen4load((char *)filnam,0); if (fil < 0) return(-1);
	kread(fil,&i,4);
	kread(fil,&xsiz,4);
	kread(fil,&ysiz,4);
	if ((i != 0x09072000) || (xsiz != 1024) || (ysiz != 1024)) { kclose(fil); return(-1); }
	zsiz = 256;
	klseek(fil,96,SEEK_CUR); //skip pos&orient
	xpiv = ((float)xsiz)*.5;
	ypiv = ((float)ysiz)*.5;
	zpiv = ((float)zsiz)*.5;

	yzsiz = ysiz*zsiz; i = ((xsiz*yzsiz+31)>>3);
	vbit = (long *)malloc(i); if (!vbit) { kclose(fil); return(-1); }
	memset(vbit,-1,i);

	vcolhashsizm1 = 1048576-1;
	vcolhashead = (long *)malloc((vcolhashsizm1+1)*sizeof(long)); if (!vcolhashead) { kclose(fil); return(-1); }
	memset(vcolhashead,-1,(vcolhashsizm1+1)*sizeof(long));

		//Allocate huge buffer and load rest of file into it...
	i = kfilelength(fil)-ktell(fil);
	vbuf = (unsigned char *)malloc(i); if (!vbuf) { kclose(fil); return(-1); }
	kread(fil,vbuf,i);
	kclose(fil);

	v = vbuf;
	for(y=0;y<ysiz;y++)
		for(x=0,j=y*zsiz;x<xsiz;x++,j+=yzsiz)
		{
			z = 0;
			while (1)
			{
				setzrange0(vbit,j+z,j+v[1]);
				for(z=v[1];z<=v[2];z++) putvox(x,y,z,(*(long *)&v[(z-v[1]+1)<<2])&0xffffff);
				if (!v[0]) break; z = v[2]-v[1]-v[0]+2; v += v[0]*4;
				for(z+=v[3];z<v[3];z++) putvox(x,y,z,(*(long *)&v[(z-v[3])<<2])&0xffffff);
			}
			v += ((((long)v[2])-((long)v[1])+2)<<2);
		}
	free(vbuf); return(0);
}
#endif

void voxfree (voxmodel *m)
{
	if (!m) return;
	if (m->mytex) free(m->mytex);
	if (m->quad) free(m->quad);
	if (m->texid) free(m->texid);
	free(m);
}

voxmodel *voxload (const char *filnam)
{
	long i, is8bit, ret;
	voxmodel *vm;

	i = strlen(filnam)-4; if (i < 0) return(0);
		  if (!Bstrcasecmp(&filnam[i],".vox")) { ret = loadvox(filnam); is8bit = 1; }
	else if (!Bstrcasecmp(&filnam[i],".kvx")) { ret = loadkvx(filnam); is8bit = 1; }
	else if (!Bstrcasecmp(&filnam[i],".kv6")) { ret = loadkv6(filnam); is8bit = 0; }
 //else if (!Bstrcasecmp(&filnam[i],".vxl")) { ret = loadvxl(filnam); is8bit = 0; }
	else return(0);
	if (ret >= 0) vm = vox2poly(); else vm = 0;
	if (vm)
	{
		vm->mdnum = 1; //VOXel model id
		vm->scale = vm->bscale = 1.0;
		vm->xsiz = xsiz; vm->ysiz = ysiz; vm->zsiz = zsiz;
		vm->xpiv = xpiv; vm->ypiv = ypiv; vm->zpiv = zpiv;
		vm->is8bit = is8bit;

		vm->texid = (unsigned int *)calloc(MAXPALOOKUPS,sizeof(unsigned int));
		if (!vm->texid) { voxfree(vm); vm = 0; }
	}
	if (shcntmal) { free(shcntmal); shcntmal = 0; }
	if (vbit) { free(vbit); vbit = 0; }
	if (vcol) { free(vcol); vcol = 0; vnum = 0; vmax = 0; }
	if (vcolhashead) { free(vcolhashead); vcolhashead = 0; }
	return(vm);
}

	//Draw voxel model as perfect cubes
int voxdraw (voxmodel *m, spritetype *tspr)
{
	point3d fp, m0, a0;
	long i, j, k, fi, *lptr, xx, yy, zz;
	float ru, rv, uhack[2], vhack[2], phack[2], clut[6] = {1,1,1,1,1,1}; //1.02,1.02,0.94,1.06,0.98,0.98};
	float f, g, k0, k1, k2, k3, k4, k5, k6, k7, mat[16], omat[16], pc[4];
	vert_t *vptr;

	//updateanimation((md2model *)m,tspr);
	if ((tspr->cstat&48)==32) return 0;

	m0.x = m->scale;
	m0.y = m->scale;
	m0.z = m->scale;
	a0.x = a0.y = 0; a0.z = m->zadd*m->scale;

	//if (globalorientation&8) //y-flipping
	//{
	//   m0.z = -m0.z; a0.z = -a0.z;
	//      //Add height of 1st frame (use same frame to prevent animation bounce)
	//   a0.z += m->zsiz*m->scale;
	//}
	//if (globalorientation&4) { m0.y = -m0.y; a0.y = -a0.y; } //x-flipping

	f = ((float)tspr->xrepeat)*(256.0/320.0)/64.0*m->bscale;
	m0.x *= f; a0.x *= f; f = -f;
	m0.y *= f; a0.y *= f;
	f = ((float)tspr->yrepeat)/64.0*m->bscale;
	m0.z *= f; a0.z *= f;

	k0 = tspr->z;
	if (globalorientation&128) k0 += (float)((tilesizy[tspr->picnum]*tspr->yrepeat)<<1);

	f = (65536.0*512.0)/((float)xdimen*viewingrange);
	g = 32.0/((float)xdimen*gxyaspect);
	m0.y *= f; a0.y = (((float)(tspr->x-globalposx))/  1024.0 + a0.y)*f;
	m0.x *=-f; a0.x = (((float)(tspr->y-globalposy))/ -1024.0 + a0.x)*-f;
	m0.z *= g; a0.z = (((float)(k0     -globalposz))/-16384.0 + a0.z)*g;

	k0 = ((float)(tspr->x-globalposx))*f/1024.0;
	k1 = ((float)(tspr->y-globalposy))*f/1024.0;
	f = gcosang2*gshang;
	g = gsinang2*gshang;
	k4 = (float)sintable[(tspr->ang+spriteext[tspr->owner].angoff+1024)&2047] / 16384.0;
	k5 = (float)sintable[(tspr->ang+spriteext[tspr->owner].angoff+ 512)&2047] / 16384.0;
	k2 = k0*(1-k4)+k1*k5;
	k3 = k1*(1-k4)-k0*k5;
	k6 = f*gstang - gsinang*gctang; k7 = g*gstang + gcosang*gctang;
	mat[0] = k4*k6 + k5*k7; mat[4] = gchang*gstang; mat[ 8] = k4*k7 - k5*k6; mat[12] = k2*k6 + k3*k7;
	k6 = f*gctang + gsinang*gstang; k7 = g*gctang - gcosang*gstang;
	mat[1] = k4*k6 + k5*k7; mat[5] = gchang*gctang; mat[ 9] = k4*k7 - k5*k6; mat[13] = k2*k6 + k3*k7;
	k6 =           gcosang2*gchang; k7 =           gsinang2*gchang;
	mat[2] = k4*k6 + k5*k7; mat[6] =-gshang;        mat[10] = k4*k7 - k5*k6; mat[14] = k2*k6 + k3*k7;

	mat[12] += a0.y*mat[0] + a0.z*mat[4] + a0.x*mat[ 8];
	mat[13] += a0.y*mat[1] + a0.z*mat[5] + a0.x*mat[ 9];
	mat[14] += a0.y*mat[2] + a0.z*mat[6] + a0.x*mat[10];

		//Mirrors
	if (grhalfxdown10x < 0) { mat[0] = -mat[0]; mat[4] = -mat[4]; mat[8] = -mat[8]; mat[12] = -mat[12]; }

//------------
	//bit 10 is an ugly hack in game.c\animatesprites telling MD2SPRITE
	//to use Z-buffer hacks to hide overdraw problems with the shadows
	if (tspr->cstat&1024)
	{
		bglDepthFunc(GL_LESS); //NEVER,LESS,(,L)EQUAL,GREATER,(NOT,G)EQUAL,ALWAYS
		bglDepthRange(0.0,0.9999);
	}
	bglPushAttrib(GL_POLYGON_BIT);
	if ((grhalfxdown10x >= 0) /*^ ((globalorientation&8) != 0) ^ ((globalorientation&4) != 0)*/) bglFrontFace(GL_CW); else bglFrontFace(GL_CCW);
	bglEnable(GL_CULL_FACE);
	bglCullFace(GL_BACK);

	bglEnable(GL_TEXTURE_2D);

	pc[0] = pc[1] = pc[2] = ((float)(numpalookups-min(max(globalshade+m->shadeoff,0),numpalookups)))/((float)numpalookups);
	pc[0] *= (float)hictinting[globalpal].r / 255.0;
	pc[1] *= (float)hictinting[globalpal].g / 255.0;
	pc[2] *= (float)hictinting[globalpal].b / 255.0;
	if (tspr->cstat&2) { if (!(tspr->cstat&512)) pc[3] = 0.66; else pc[3] = 0.33; } else pc[3] = 1.0;
	if (tspr->cstat&2) bglEnable(GL_BLEND); else bglDisable(GL_BLEND);
//------------

		//transform to Build coords
	memcpy(omat,mat,sizeof(omat));
	f = 1.f/64.f;
	g = m0.x*f; mat[0] *= g; mat[1] *= g; mat[2] *= g;
	g = m0.y*f; mat[4] = omat[8]*g; mat[5] = omat[9]*g; mat[6] = omat[10]*g;
	g =-m0.z*f; mat[8] = omat[4]*g; mat[9] = omat[5]*g; mat[10] = omat[6]*g;
	mat[12] -= (m->xpiv*mat[0] + m->ypiv*mat[4] + (m->zpiv+m->zsiz*.5)*mat[ 8]);
	mat[13] -= (m->xpiv*mat[1] + m->ypiv*mat[5] + (m->zpiv+m->zsiz*.5)*mat[ 9]);
	mat[14] -= (m->xpiv*mat[2] + m->ypiv*mat[6] + (m->zpiv+m->zsiz*.5)*mat[10]);
	bglMatrixMode(GL_MODELVIEW); //Let OpenGL (and perhaps hardware :) handle the matrix rotation
	mat[3] = mat[7] = mat[11] = 0.f; mat[15] = 1.f;

	bglLoadMatrixf(mat);

	ru = 1.f/((float)m->mytexx);
	rv = 1.f/((float)m->mytexy);
#if (VOXBORDWIDTH == 0)
	uhack[0] = ru*.125; uhack[1] = -uhack[0];
	vhack[0] = rv*.125; vhack[1] = -vhack[0];
#endif
	phack[0] = 0; phack[1] = 1.f/256.f;

	if (!m->texid[globalpal]) m->texid[globalpal] = gloadtex(m->mytex,m->mytexx,m->mytexy,m->is8bit,globalpal);
					 else bglBindTexture(GL_TEXTURE_2D,m->texid[globalpal]);
	bglBegin(GL_QUADS);
	for(i=0,fi=0;i<m->qcnt;i++)
	{
		if (i == m->qfacind[fi]) { f = clut[fi++]; bglColor4f(pc[0]*f,pc[1]*f,pc[2]*f,pc[3]*f); }
		vptr = &m->quad[i].v[0];

		xx = vptr[0].x+vptr[2].x;
		yy = vptr[0].y+vptr[2].y;
		zz = vptr[0].z+vptr[2].z;

		for(j=0;j<4;j++)
		{
#if (VOXBORDWIDTH == 0)
			bglTexCoord2f(((float)vptr[j].u)*ru+uhack[vptr[j].u!=vptr[0].u],
							  ((float)vptr[j].v)*rv+vhack[vptr[j].v!=vptr[0].v]);
#else
			bglTexCoord2f(((float)vptr[j].u)*ru,((float)vptr[j].v)*rv);
#endif
			fp.x = ((float)vptr[j].x) - phack[xx>vptr[j].x*2] + phack[xx<vptr[j].x*2];
			fp.y = ((float)vptr[j].y) - phack[yy>vptr[j].y*2] + phack[yy<vptr[j].y*2];
			fp.z = ((float)vptr[j].z) - phack[zz>vptr[j].z*2] + phack[zz<vptr[j].z*2];
			bglVertex3fv((float *)&fp);
		}
	}
	bglEnd();

//------------
	bglDisable(GL_CULL_FACE);
	bglPopAttrib();
	if (tspr->cstat&1024)
	{
		bglDepthFunc(GL_LEQUAL); //NEVER,LESS,(,L)EQUAL,GREATER,(NOT,G)EQUAL,ALWAYS
		bglDepthRange(0.0,0.99999);
	}
	bglLoadIdentity();
	return 1;
}

//---------------------------------------- VOX LIBRARY ENDS ----------------------------------------
//--------------------------------------- MD LIBRARY BEGINS  ---------------------------------------

mdmodel *mdload (const char *filnam)
{
	mdmodel *vm;
	int fil;
	long i;

	vm = (mdmodel*)voxload(filnam); if (vm) return(vm);

	fil = kopen4load((char *)filnam,0); if (fil < 0) return(0);
	kread(fil,&i,4); klseek(fil,0,SEEK_SET);
	switch(B_LITTLE32(i))
	{
		case 0x32504449: vm = (mdmodel*)md2load(fil,filnam); break; //IDP2
		case 0x33504449: vm = (mdmodel*)md3load(fil); break; //IDP3
		default: vm = (mdmodel*)0; break;
	}
	kclose(fil);
	return(vm);
}

int mddraw (spritetype *tspr)
{
	mdanim_t *anim;
	mdmodel *vm;

	if (maxmodelverts > allocmodelverts)
	{
		point3d *vl = (point3d *)realloc(vertlist,sizeof(point3d)*maxmodelverts);
		if (!vl) { initprintf("ERROR: Not enough memory to allocate %d vertices!\n",maxmodelverts); return 0; }
		vertlist = vl; allocmodelverts = maxmodelverts;
	}

	vm = models[tile2model[tspr->picnum].modelid];
	if (vm->mdnum == 1) { return voxdraw((voxmodel *)vm,tspr); }
	if (vm->mdnum == 2) { return md2draw((md2model *)vm,tspr); }
	if (vm->mdnum == 3) { return md3draw((md3model *)vm,tspr); }
	return 0;
}

void mdfree (mdmodel *vm)
{
	if (vm->mdnum == 1) { voxfree((voxmodel *)vm); return; }
	if (vm->mdnum == 2) { md2free((md2model *)vm); return; }
	if (vm->mdnum == 3) { md3free((md3model *)vm); return; }
}

//---------------------------------------- MD LIBRARY ENDS  ----------------------------------------
#endif
