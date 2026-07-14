// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//-----------------------------------------------------------------------------
/// \file
/// \brief Main program, simply calls D_SRB2Main and D_SRB2Loop, the high level loop.

#include "../doomdef.h"
#include "../m_argv.h"
#include "../d_main.h"
#include "../i_system.h"

#ifdef __GNUC__
#include <unistd.h>
#endif

#ifdef __vita__
#include <vitasdk.h>
#include <fcntl.h>

/* Écran de chargement : framebuffer sceDisplay autonome (debugScreen des
   samples VitaSDK), totalement indépendant de SDL/GXM — donc utilisable AVANT
   que le moteur graphique n'existe. Actif dès l'entrée de main(), il s'efface
   à la première frame du jeu (VitaBoot_Done, appelé par I_FinishUpdate).
   On dessine directement dans `base` (le framebuffer de debugScreen : 960 de
   stride, un uint32 par pixel au format 0x00BBGGRR). Le texte est tracé à la
   main à partir des glyphes 8x8 de la police (1 bit/pixel, 8 octets par
   caractère) : les fonctions psvDebugScreenPuts, elles, repeignent le fond de
   chaque caractère et masqueraient le dégradé. */
#include "SRB2Vita/debugScreen.h"
#include "SRB2Vita/debugScreen.c"
/* Le vrai logo du jeu (TTKBANNR + TTKART de gfx.kart), embarque dans le binaire
   par kart/livearea-src/make_bootlogo.py : ici les WADs ne sont pas encore
   charges, on ne peut donc pas aller le chercher dans les donnees. */
#include "SRB2Vita/bootlogo.h"

#define VITA_FB_W 960
#define VITA_FB_H 544
#define VITA_RGB(r,g,b) ((uint32_t)((b) << 16 | (g) << 8 | (r)))

static volatile INT32 vita_bootlog_on = 0;
static float vita_boot_progress = 0.0f;

static void VitaBoot_Rect(int x, int y, int w, int h, uint32_t col)
{
	int px, py;
	uint32_t *fb = (uint32_t *)base;

	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > VITA_FB_W) w = VITA_FB_W - x;
	if (y + h > VITA_FB_H) h = VITA_FB_H - y;

	for (py = y; py < y + h; py++)
		for (px = x; px < x + w; px++)
			fb[py * VITA_FB_W + px] = col;
}

/* Un caractère de la police 8x8, agrandi `scale` fois, fond transparent. */
static void VitaBoot_Char(int x, int y, unsigned char c, uint32_t col, int scale)
{
	const unsigned char *glyph = psvDebugScreenFont.glyphs + (size_t)c * 8;
	int row, bit;

	for (row = 0; row < 8; row++)
		for (bit = 0; bit < 8; bit++)
			if (glyph[row] & (0x80 >> bit))
				VitaBoot_Rect(x + bit * scale, y + row * scale, scale, scale, col);
}

static void VitaBoot_Text(int x, int y, const char *s, uint32_t col, int scale)
{
	for (; *s; s++, x += 8 * scale)
		VitaBoot_Char(x, y, (unsigned char)*s, col, scale);
}

static void VitaBoot_TextCentered(int y, const char *s, uint32_t col, int scale)
{
	VitaBoot_Text((VITA_FB_W - (int)strlen(s) * 8 * scale) / 2, y, s, col, scale);
}

/* Le logo du jeu : image indexée, l'indice 0 étant transparent. On le pose avec
   une ombre portée (sa silhouette, décalée) pour qu'il se détache du fond. */
static void VitaBoot_Logo(int x, int y)
{
	uint32_t *fb = (uint32_t *)base;
	const int decalage = 5;
	int px, py, passe;

	for (passe = 0; passe < 2; passe++) /* 0 = ombre, 1 = logo */
	{
		int ox = x + (passe ? 0 : decalage);
		int oy = y + (passe ? 0 : decalage);

		for (py = 0; py < BOOTLOGO_H; py++)
		{
			int fy = oy + py;
			if (fy < 0 || fy >= VITA_FB_H)
				continue;

			for (px = 0; px < BOOTLOGO_W; px++)
			{
				int fx = ox + px;
				unsigned char idx = bootlogo_pixels[py * BOOTLOGO_W + px];

				if (!idx || fx < 0 || fx >= VITA_FB_W)
					continue; /* indice 0 = transparent */

				fb[fy * VITA_FB_W + fx] = passe
					? bootlogo_palette[idx]
					: VITA_RGB(8, 10, 26); /* ombre */
			}
		}
	}
}

/* Fond : dégradé vertical bleu nuit -> bleu ciel, damier de drapeau à
   damier en haut et en bas (thème course). Dessiné une seule fois. */
static void VitaBoot_Background(void)
{
	const int band = 32; /* hauteur d'une bande de damier + taille d'une case */
	int y, x;
	uint32_t *fb = (uint32_t *)base;

	for (y = 0; y < VITA_FB_H; y++)
	{
		/* dégradé : (12,18,54) en haut -> (46,96,176) en bas */
		int r = 12 + (46 - 12) * y / VITA_FB_H;
		int g = 18 + (96 - 18) * y / VITA_FB_H;
		int b = 54 + (176 - 54) * y / VITA_FB_H;
		uint32_t col = VITA_RGB(r, g, b);
		for (x = 0; x < VITA_FB_W; x++)
			fb[y * VITA_FB_W + x] = col;
	}

	for (y = 0; y < band; y++)
		for (x = 0; x < VITA_FB_W; x++)
		{
			int dark = ((x / band) + (y / band)) & 1;
			uint32_t col = dark ? VITA_RGB(20, 20, 24) : VITA_RGB(235, 235, 240);
			fb[y * VITA_FB_W + x] = col;
			fb[(VITA_FB_H - 1 - y) * VITA_FB_W + x] = col;
		}

	VitaBoot_Logo((VITA_FB_W - BOOTLOGO_W) / 2, 28);
	VitaBoot_TextCentered(296, "PS VITA / PS TV", VITA_RGB(255, 190, 60), 2);
	/* L'ordre est voulu : le jeu et la couche Vita existaient bien avant ce portage.
	   Kart Krew et Rinnegatamante d'abord ; le porteur en dernier, en petit. */
	VitaBoot_TextCentered(470, "SRB2Kart by Kart Krew - vitaGL by Rinnegatamante", VITA_RGB(180, 200, 235), 1);
	VitaBoot_TextCentered(486, "Vita port by Esod, with Ryo's help", VITA_RGB(140, 160, 195), 1);
}

/* Barre + libellé de l'étape (+ une ligne d'info facultative).
   ⚠️ DEUX SOURCES DE SCINTILLEMENT, corrigées ici :
   1. Le framebuffer est affiché DIRECTEMENT (pas de double tampon) : effacer une
      zone puis la réécrire se VOIT. On ne redessine donc que ce qui a changé —
      les nouveaux pixels de la barre, et le texte seulement quand il change.
   2. Le fil d'animation et le thread principal dessinent tous les deux : sans
      verrou, ils se marchent dessus (c'est le défaut visible toutes les 2-3 s). */
static SceUID vita_boot_lock = -1;
/* Mis a 1 quand l'ecran vient d'etre REPEINT en entier (init de vitaGL : elle vole
   l'ecran, on redessine le fond). Sans ca, la barre croit avoir deja trace son
   cadre et ne le retrace jamais : la bordure blanche disparait apres le
   rafraichissement. */
static int vita_boot_repaint = 1;

static void VitaBoot_DrawBar(const char *status, const char *hint)
{
	const int bx = 160, by = 330, bw = 640, bh = 26;
	static int last_fill = 0;
	static char last_pct[8] = "";
	static const char *last_status = NULL;
	static const char *last_hint = NULL;
	int fill, i;
	char pct[8];

	if (vita_boot_lock >= 0)
		sceKernelLockMutex(vita_boot_lock, 1, NULL);

	/* cadre + fond de la barre : au debut, et apres chaque repeinte de l'ecran */
	if (vita_boot_repaint)
	{
		VitaBoot_Rect(bx - 3, by - 3, bw + 6, bh + 6, VITA_RGB(255, 255, 255));
		VitaBoot_Rect(bx, by, bw, bh, VITA_RGB(16, 20, 40));
		last_fill = 0;      /* le remplissage est a refaire en entier */
		last_pct[0] = 0;    /* le texte aussi */
		vita_boot_repaint = 0;
	}

	/* remplissage : dégradé orange -> jaune. On ne trace QUE les colonnes
	   nouvellement gagnées (la barre ne recule jamais) : aucun effacement, donc
	   aucun scintillement. */
	fill = (int)(vita_boot_progress * (float)bw);
	if (fill > bw) fill = bw;

	for (i = last_fill; i < fill; i++)
	{
		int r = 255;
		int g = 130 + 90 * i / bw;
		int b = 20 + 40 * i / bw;
		VitaBoot_Rect(bx + i, by, 1, bh, VITA_RGB(r, g, b));
	}

	if (fill > last_fill)
		last_fill = fill;

	/* pourcentage + étape + info : réécrits SEULEMENT s'ils ont changé */
	snprintf(pct, sizeof pct, "%d%%", (int)(vita_boot_progress * 100.0f + 0.5f));

	if (strcmp(pct, last_pct) || status != last_status || hint != last_hint)
	{
		VitaBoot_Rect(0, by + bh + 14, VITA_FB_W, 56, VITA_RGB(30, 55, 120));
		VitaBoot_TextCentered(by + bh + 16, pct, VITA_RGB(255, 210, 90), 2);
		if (status)
			VitaBoot_TextCentered(by + bh + 40, status, VITA_RGB(210, 225, 250), 1);
		if (hint)
			VitaBoot_TextCentered(by + bh + 56, hint, VITA_RGB(255, 190, 60), 1);

		strcpy(last_pct, pct);
		last_status = status;
		last_hint = hint;
	}

	if (vita_boot_lock >= 0)
		sceKernelUnlockMutex(vita_boot_lock, 1);
}

/* Étapes du démarrage, dans l'ordre où le moteur les annonce.
   `progress` = avancement AU DÉBUT de l'étape, `expect` = sa durée attendue en
   secondes. Les deux sont calibrés sur des durées MESURÉES sur la console
   sur la console : la barre est donc proportionnelle au temps réel, et pas à un
   découpage arbitraire — « Textures » pèse 80 % du démarrage, elle occupe 80 %
   de la barre.
   La barre est animée EN CONTINU par un fil dédié (VitaBoot_Animator) : sans lui
   elle resterait figée pendant les 55 secondes de « Textures », où le moteur
   n'imprime rien — et un écran figé, ça ressemble à un plantage. */
static const struct { const char *marker; float progress; float expect; const char *label; const char *hint; } vita_boot_steps[] = {
	{ "Z_Init",              0.00f,  0.1f, "Setting up memory",       NULL },
	{ "I_InitializeTime",    0.005f, 0.1f, "Setting up timers",       NULL },
	{ "W_InitMultipleFiles", 0.01f,  0.1f, "Opening game data files", NULL },
	{ "Added file",          0.015f, 0.4f, "Loading game data",       NULL },
	{ "I_StartupGraphics",   0.02f,  0.7f, "Starting up graphics",    NULL },
	{ "HU_Init",             0.03f,  0.9f, "Heads-up display",        NULL },
	{ "M_Init",              0.04f,  0.1f, "Menus",                   NULL },
	{ "R_Init",              0.05f, 55.0f, "Textures and rendering",  "This takes about a minute, hang tight!" },
	/* ~18 s : les jingles (9,6 s) + le DECODAGE des bruitages de la liste chaude
	   (8,3 s). C'est ce decodage qui evite les gels en course — et, depuis qu'il
	   survit aux chargements de niveau, il n'est fait QU'ICI, une fois par session. */
	{ "S_InitSfxChannels",   0.75f, 18.0f, "Sound and music",         "Almost there!" },
	{ "ST_Init",             0.97f,  1.1f, "Status bar",              NULL },
	{ "D_CheckNetGame",      0.99f,  0.6f, "Network",                 NULL },
};

/* Etape en cours : c'est elle qui permet a la barre d'avancer AU TEMPS pendant les
   longues etapes silencieuses (les durees attendues, dans la table ci-dessus, ont ete
   mesurees sur la console). */
#define VITA_NSTEPS (sizeof(vita_boot_steps) / sizeof(vita_boot_steps[0]))
static uint64_t vita_step_t0;  /* debut de l'etape en cours */
static int vita_step_cur = -1;

static void VitaBoot_StepEnter(int idx)
{
	if (idx == vita_step_cur)
		return;

	vita_step_cur = idx;
	vita_step_t0 = sceKernelGetProcessTimeWide();
}

/* Avancement attendu de l'étape en cours, d'après le TEMPS écoulé.
   On interpole entre l'avancement de cette étape et celui de la suivante, sur la
   durée mesurée. Si l'étape déborde (console plus lente, carte plus lente), on
   s'approche de la borne suivante sans jamais l'atteindre : la barre ralentit au
   lieu de mentir, et elle ne recule jamais. */
static float VitaBoot_ExpectedProgress(void)
{
	uint64_t now;
	float from, to, frac;
	double elapsed;
	int i = vita_step_cur;

	if (i < 0)
		return vita_boot_progress;

	now = sceKernelGetProcessTimeWide();
	elapsed = (double)(now - vita_step_t0) / 1000000.0;

	from = vita_boot_steps[i].progress;
	to = (i + 1 < (int)VITA_NSTEPS) ? vita_boot_steps[i + 1].progress : 1.0f;

	frac = (vita_boot_steps[i].expect > 0.0f)
		? (float)(elapsed / vita_boot_steps[i].expect) : 1.0f;

	if (frac > 1.0f) /* on deborde : on s'approche de la borne, sans jamais y arriver */
		frac = 1.0f - 1.0f / (1.0f + (frac - 1.0f) * 2.0f);

	return from + (to - from) * frac * 0.98f;
}

/* Fil d'animation : redessine la barre 10 fois par seconde pendant tout le
   démarrage. C'est LUI qui fait vivre l'écran pendant les longues étapes
   silencieuses (« Textures » : 55 s sans un seul message du moteur).
   Priorité basse : il ne doit rien voler au chargement. */
static const char *vita_boot_status = "Starting up...";
static const char *vita_boot_hint;

static int VitaBoot_Animator(SceSize args, void *argp)
{
	(void)args; (void)argp;

	while (vita_bootlog_on)
	{
		float p = VitaBoot_ExpectedProgress();

		if (p > vita_boot_progress) /* jamais de recul */
			vita_boot_progress = p;

		VitaBoot_DrawBar(vita_boot_status, vita_boot_hint);
		sceKernelDelayThread(100000); /* 100 ms */
	}

	return sceKernelExitDeleteThread(0);
}

void VitaBoot_StartAnimator(void);
void VitaBoot_StartAnimator(void)
{
	SceUID th;

	/* Le verrou doit exister AVANT le fil : les deux threads dessinent. */
	vita_boot_lock = sceKernelCreateMutex("bootbar", 0, 0, NULL);

	th = sceKernelCreateThread("bootbar", VitaBoot_Animator,
		0x10000100 + 20, 0x4000, 0, 0, NULL);

	if (th >= 0)
		sceKernelStartThread(th, 0, NULL);
}

void VitaBoot_Log(const char *msg);
void VitaBoot_Log(const char *msg)
{
	size_t i;
	float p;

	if (!vita_bootlog_on || !msg)
		return;

	/* On ne fait plus qu'ENTRER dans l'etape : l'avancement, lui, est calcule a
	   partir du TEMPS ecoule et dessine par le fil d'animation. */
	for (i = 0; i < sizeof(vita_boot_steps) / sizeof(vita_boot_steps[0]); i++)
	{
		if (strstr(msg, vita_boot_steps[i].marker) == NULL)
			continue;

		VitaBoot_StepEnter((int)i);
		vita_boot_status = vita_boot_steps[i].label;
		vita_boot_hint = vita_boot_steps[i].hint;
		break;
	}

	p = VitaBoot_ExpectedProgress();

	if (p > vita_boot_progress) /* jamais de recul */
		vita_boot_progress = p;

	VitaBoot_DrawBar(vita_boot_status, vita_boot_hint);
}

int VitaBoot_Active(void);
int VitaBoot_Active(void)
{
	return vita_bootlog_on;
}

/* Appelé à la fin de D_SRB2Main : le jeu est prêt, on rend l'écran. */
void VitaBoot_Done(void);
void VitaBoot_Done(void)
{
	/* Le boot est termine : on demarre le compteur qui armera le hot-swap de
	   resolution deux frames plus tard (cf. vita_hotres_ready / I_FinishUpdate
	   dans i_video.c). A poser AVANT le retour anticipe ci-dessous, pour le cas
	   ou l'ecran de boot serait desactive. */
	extern UINT8 vita_boot_finished;
	vita_boot_finished = 1;

	if (!vita_bootlog_on)
		return;

	vita_boot_progress = 1.0f;
	VitaBoot_DrawBar("Ready!", NULL);

	/* Pas de psvDebugScreenFinish() : il ferait sceDisplaySetFrameBuf(NULL)
	   et masquerait la frame que le jeu va présenter. On abandonne juste le
	   framebuffer (2 Mo de CDRAM, sans conséquence) : dès qu'on cesse de le
	   ré-armer, la première frame du jeu reprend l'écran. */
	vita_bootlog_on = 0;
}

/* =====================================================================
   ÉCRAN DE CHARGEMENT D'UNE COURSE
   =====================================================================
   Pendant P_SetupLevel, le jeu ne présente aucune frame : l'écran reste figé
   plusieurs secondes, et depuis qu'on coupe la musique, il est aussi SILENCIEUX
   — donc rien ne dit au joueur que le jeu n'a pas planté (remarque de Julien).
   On reprend la mécanique de l'écran de démarrage (elle marche) : on récupère le
   framebuffer brut et un fil y fait défiler un compteur. Dès que le jeu présente
   sa première frame OpenGL, il reprend l'écran naturellement. */
static volatile int vita_load_on = 0;
static uint64_t vita_load_t0;
static volatile float vita_load_progress;   /* 0 -> 1 */

/* Appele par le decodeur (mixer_sound.c) : avancement REEL, on sait combien de sons
   la liste chaude contient. */
void VitaLoad_SetProgress(float p);
void VitaLoad_SetProgress(float p)
{
	if (p > vita_load_progress)
		vita_load_progress = p;
}

static int VitaLoad_Animator(SceSize args, void *argp)
{
	/* ⚠️ by = 380, PAS 300 : le fond (VitaBoot_Background) ecrit « PS VITA / PS TV »
	   en orange a y=296..312. Une barre a y=300 tombait pile dessus, et le texte
	   depassait derriere elle. */
	const int bx = 230, by = 380, bw = 500, bh = 20;
	int last_fill = 0;

	(void)args; (void)argp;

	if (vita_boot_lock >= 0)
		sceKernelLockMutex(vita_boot_lock, 1, NULL);

	VitaBoot_Rect(bx - 3, by - 3, bw + 6, bh + 6, VITA_RGB(255, 255, 255));
	VitaBoot_Rect(bx, by, bw, bh, VITA_RGB(16, 20, 40));
	VitaBoot_TextCentered(by - 30, "Loading race...", VITA_RGB(210, 225, 250), 1);

	if (vita_boot_lock >= 0)
		sceKernelUnlockMutex(vita_boot_lock, 1);

	while (vita_load_on)
	{
		double secs = (double)(sceKernelGetProcessTimeWide() - vita_load_t0) / 1000000.0;
		float frac, p;
		int fill, i;

		/* Le chargement d'un niveau ne dit RIEN sur son avancement (le moteur charge
		   la geometrie et les textures sans rien annoncer), et depuis que les sons
		   sont decodes au demarrage, il n'y a plus rien a compter ici. La barre avance
		   donc AU TEMPS, sur la duree mesuree (~3 s sur console) — c'est un peu
		   « fake », mais honnete : si le chargement deborde, elle RALENTIT et s'approche
		   de 100 % sans jamais l'atteindre. Elle n'arrive donc jamais avant le jeu, et
		   ne reste jamais figee. */
		frac = (float)(secs / 3.0);

		if (frac > 1.0f)
			frac = 1.0f - 1.0f / (1.0f + (frac - 1.0f) * 2.0f);

		p = 0.99f * frac;

		if (p > vita_load_progress) /* le decodeur peut nous devancer : on prend le max */
			vita_load_progress = p;

		fill = (int)(vita_load_progress * (float)bw);
		if (fill > bw) fill = bw;

		if (fill > last_fill)
		{
			if (vita_boot_lock >= 0)
				sceKernelLockMutex(vita_boot_lock, 1, NULL);

			/* on ne trace que les colonnes gagnees : pas d'effacement, pas de
			   scintillement (meme lecon que la barre de demarrage) */
			for (i = last_fill; i < fill; i++)
			{
				int g = 130 + 90 * i / bw;
				int b = 20 + 40 * i / bw;
				VitaBoot_Rect(bx + i, by, 1, bh, VITA_RGB(255, g, b));
			}

			if (vita_boot_lock >= 0)
				sceKernelUnlockMutex(vita_boot_lock, 1);

			last_fill = fill;
		}

		sceKernelDelayThread(30000); /* 30 ms : mouvement continu */
	}

	return sceKernelExitDeleteThread(0);
}

void VitaLoad_Begin(void);
void VitaLoad_Begin(void)
{
	SceUID th;

	if (vita_load_on || vita_bootlog_on) /* pas pendant le demarrage */
		return;

	if (vita_boot_lock < 0)
		vita_boot_lock = sceKernelCreateMutex("bootbar", 0, 0, NULL);

	/* La bascule d'ecran de vitaGL est ASYNCHRONE : il faut attendre que sa file
	   d'affichage soit videe avant de reprendre l'ecran, sinon sa derniere frame
	   nous repasse dessus (meme piege qu'au demarrage). */
	sceGxmDisplayQueueFinish();
	psvDebugScreenRearm();
	VitaBoot_Background();

	vita_load_t0 = sceKernelGetProcessTimeWide();
	vita_load_progress = 0.0f;
	vita_load_on = 1;

	th = sceKernelCreateThread("loadbar", VitaLoad_Animator,
		0x10000100, 0x4000, 0, 0, NULL);

	if (th >= 0)
		sceKernelStartThread(th, 0, NULL);
}

void VitaLoad_End(void);
void VitaLoad_End(void)
{
	vita_load_on = 0; /* le jeu reprend l'ecran a sa premiere frame GL */
}

/* Rappelé après l'init de vitaGL, qui vole l'écran (cf. i_video.c). */
void VitaBoot_Redraw(void);
void VitaBoot_Redraw(void)
{
	if (!vita_bootlog_on)
		return;

	VitaBoot_Background();
	vita_boot_repaint = 1; /* le fond vient d'etre repeint : cadre + barre a refaire */
	/* On garde le libelle de l'etape en cours : afficher « Loading... » ferait
	   perdre l'information (« Textures and rendering », et son message d'attente). */
	VitaBoot_DrawBar(vita_boot_status, vita_boot_hint);
}
#endif

#ifdef HAVE_SDL

#ifdef HAVE_TTF
#include "SDL.h"
#include "i_ttf.h"
#endif

#if defined (_WIN32) && !defined (main)
//#define SDLMAIN
#endif

#ifdef SDLMAIN
#include "SDL_main.h"
#elif defined(FORCESDLMAIN)
extern int SDL_main(int argc, char *argv[]);
#endif

#ifdef LOGMESSAGES
FILE *logstream = NULL;
char  logfilename[1024];
#endif

#ifndef DOXYGEN
#ifndef O_TEXT
#define O_TEXT 0
#endif

#ifndef O_SEQUENTIAL
#define O_SEQUENTIAL 0
#endif
#endif

#ifdef _WIN32
#ifndef _AMD64_
#include "exchndl.h"
#define DRMINGW
#endif
#endif

#if defined (_WIN32)
#include "../win32/win_dbg.h"
typedef BOOL (WINAPI *p_IsDebuggerPresent)(VOID);
#endif

#if defined (_WIN32)
static inline VOID MakeCodeWritable(VOID)
{
#ifdef USEASM // Disable write-protection of code segment
	DWORD OldRights;
	const DWORD NewRights = PAGE_EXECUTE_READWRITE;
	PBYTE pBaseOfImage = (PBYTE)GetModuleHandle(NULL);
	PIMAGE_DOS_HEADER dosH =(PIMAGE_DOS_HEADER)pBaseOfImage;
	PIMAGE_NT_HEADERS ntH = (PIMAGE_NT_HEADERS)(pBaseOfImage + dosH->e_lfanew);
	PIMAGE_OPTIONAL_HEADER oH = (PIMAGE_OPTIONAL_HEADER)
		((PBYTE)ntH + sizeof (IMAGE_NT_SIGNATURE) + sizeof (IMAGE_FILE_HEADER));
	LPVOID pA = pBaseOfImage+oH->BaseOfCode;
	SIZE_T pS = oH->SizeOfCode;
#if 1 // try to find the text section
	PIMAGE_SECTION_HEADER ntS = IMAGE_FIRST_SECTION (ntH);
	WORD s;
	for (s = 0; s < ntH->FileHeader.NumberOfSections; s++)
	{
		if (memcmp (ntS[s].Name, ".text\0\0", 8) == 0)
		{
			pA = pBaseOfImage+ntS[s].VirtualAddress;
			pS = ntS[s].Misc.VirtualSize;
			break;
		}
	}
#endif

	if (!VirtualProtect(pA,pS,NewRights,&OldRights))
		I_Error("Could not make code writable\n");
#endif
}
#endif


#ifdef _WIN32
static void
ChDirToExe (void)
{
	CHAR path[MAX_PATH];
	if (GetModuleFileNameA(NULL, path, MAX_PATH) > 0)
	{
		strrchr(path, '\\')[0] = '\0';
		SetCurrentDirectoryA(path);
	}
}
#endif


/**	\brief	The main function

	\param	argc	number of arg
	\param	*argv	string table

	\return	int
*/
#if defined (__GNUC__) && (__GNUC__ >= 4)
#pragma GCC diagnostic ignored "-Wmissing-noreturn"
#endif

#ifdef FORCESDLMAIN
int SDL_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
	const char *logdir = NULL;
	myargc = argc;
	myargv = argv; /// \todo pull out path to exe from this string

#ifdef __vita__
	scePowerSetArmClockFrequency(444);
	scePowerSetBusClockFrequency(222);
	scePowerSetGpuClockFrequency(222);
	scePowerSetGpuXbarClockFrequency(166);

	sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
	SceNetInitParam netInitParam;
	netInitParam.memory = malloc(1 * 1024 * 1024);
	netInitParam.size = 1 * 1024 * 1024;
	netInitParam.flags = 0;
	sceNetInit(&netInitParam);
	sceNetCtlInit();

	psvDebugScreenInit();
	vita_bootlog_on = 1;
	VitaBoot_Background();
	VitaBoot_DrawBar("Starting up...", NULL);
	VitaBoot_StartAnimator(); /* la barre doit VIVRE pendant les 55 s de « Textures » */
#endif

#ifdef HAVE_TTF
#ifdef _WIN32
	I_StartupTTF(FONTPOINTSIZE, SDL_INIT_VIDEO|SDL_INIT_AUDIO, SDL_SWSURFACE);
#else
	I_StartupTTF(FONTPOINTSIZE, SDL_INIT_VIDEO, SDL_SWSURFACE);
#endif
#endif

#ifdef _WIN32
	ChDirToExe();
#endif

	logdir = D_Home();

#ifdef LOGMESSAGES
#ifdef __vita__
	/* $HOME n'existe pas sur Vita : sans ce chemin explicite, le log
	   partait vers app0: (lecture seule) et on déboguait en aveugle */
	strcpy(logfilename, "ux0:/data/srb2kart/log.txt");
#else
#ifdef DEFAULTDIR
	if (logdir)
		strcpy(logfilename, va("%s/"DEFAULTDIR"/log.txt",logdir));
	else
#endif
		strcpy(logfilename, "./log.txt");
#endif

	logstream = fopen(logfilename, "wt");
#endif

	//I_OutputMsg("I_StartupSystem() ...\n");
	I_StartupSystem();
#if defined (_WIN32)
	{
#if 0 // just load the DLL
		p_IsDebuggerPresent pfnIsDebuggerPresent = (p_IsDebuggerPresent)GetProcAddress(GetModuleHandleA("kernel32.dll"), "IsDebuggerPresent");
		if ((!pfnIsDebuggerPresent || !pfnIsDebuggerPresent())
#ifdef BUGTRAP
			&& !InitBugTrap()
#endif
			)
#endif
		{
#ifdef DRMINGW
			ExcHndlInit();
#endif
		}
	}
#ifndef __MINGW32__
	prevExceptionFilter = SetUnhandledExceptionFilter(RecordExceptionInfo);
#endif
	MakeCodeWritable();
#endif

	// startup SRB2
	CONS_Printf("Setting up SRB2Kart...\n");
	D_SRB2Main();
	CONS_Printf("Entering main game loop...\n");
	// never return
	D_SRB2Loop();

#ifdef BUGTRAP
	// This is safe even if BT didn't start.
	ShutdownBugTrap();
#endif

	// return to OS
	return 0;
}
#endif
