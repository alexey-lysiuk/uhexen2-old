#include "com_sys.h"
#include "interface.h"
#include "widget_defs.h"
#include "callbacks.h"
#include "launcher_defs.h"
#include "support.h"

extern unsigned missingexe;
extern int opengl_support;
extern int fullscreen;
extern int resolution;
extern int sound;
extern int sndrate;
extern int destiny;
#ifndef DEMOBUILD
extern int mp_support;
extern int h2game;
extern int hwgame;
extern const char *h2game_names[MAX_HWGAMES][2];
extern const char *hwgame_names[MAX_HWGAMES][2];
#endif
extern const char *snddrv_names[MAX_SOUND][2];
extern const char *snd_rates[MAX_RATES];
unsigned int bmore = 0;

const char *res_names[]={
	" 320 x 240 ",
	" 400 x 300 ",
	" 512 x 384 ",
	" 640 x 480 ",
	" 800 x 600 ",
	"1024 x 768 ",
	"1280 x 1024"
};

const char *stats[]={

	"  Ready to run the game",
	"  Binary missing or not executable"
};

void on_SND (GtkEditable *editable, sndwidget_t *wgt) {
	unsigned short i;
	gchar *tmp = gtk_editable_get_chars (editable, 0, -1);
	for (i=0; i<MAX_SOUND; i++) {
		if (strcmp(tmp, snddrv_names[i][1]) == 0) {
			g_free(tmp);
			sound = i;
			gtk_widget_set_sensitive (wgt->MUSIC_MIDI, sound);
			gtk_widget_set_sensitive (wgt->MUSIC_CD, sound);
			gtk_widget_set_sensitive (wgt->SND_RATE, sound);
			gtk_widget_set_sensitive (wgt->SND_BITS, sound);
			gtk_widget_set_sensitive (wgt->SND_STEREO, sound);
			return;
		}
// Normally, we should be all set within this loop, thus no "else"
	}
}

void on_SRATE (GtkEditable *editable, gpointer user_data) {
	unsigned short i;
	gchar *tmp = gtk_editable_get_chars (editable, 0, -1);
	for (i=0; i<MAX_RATES; i++) {
		if (strcmp(tmp, snd_rates[i]) == 0) {
			g_free(tmp);
			sndrate = i;
			return;
		}
// Normally, we should be all set within this loop, thus no "else"
	}
}

void on_SBITS (GtkButton *button, int *opt) {
	(*(opt)) = !(*(opt));
	gtk_button_set_label(button, (*(opt)) ? "16 bit" : " 8 bit");
}

void on_OGL (GtkToggleButton *button, gamewidget_t *wgt) {
	opengl_support=!opengl_support;
	switch (opengl_support) {
	case 0:
		if (resolution > 3 )
			resolution = 3;
		break;
	case 1:
		if (resolution < 3 )
			resolution = 3;
		break;
	}
	UpdateRScale (wgt->RESOL_ADJUST, wgt->RESOL_TEXT1, 1);
	UpdateStats(&(wgt->Launch));
}

void res_Change (GtkAdjustment *adj, struct Video_s *wgt) {
	// FIXME: This is silly but it's less work ;)  Change it.
	resolution = (int)(adj->value);
	if ( (adj->value)-resolution > .65 )
		resolution++;
	UpdateRScale (adj, wgt->RESOL_TEXT0, 0);
}

void UpdateRScale (GtkAdjustment *adj, GtkWidget *label, int ch_scale) {
	gtk_adjustment_set_value (adj, resolution);
	if (ch_scale) {
		adj->lower=2*opengl_support;
		adj->upper=3+3*opengl_support;
		gtk_signal_emit_by_name (GTK_OBJECT (adj), "changed");
	}
	gtk_label_set_text (GTK_LABEL (label), res_names[resolution]);
}

void UpdateStats (struct Launch_s *wgt) {
	CheckExe();
	gtk_statusbar_push (GTK_STATUSBAR (wgt->STATUSBAR), wgt->BinStat, stats[missingexe]);
	gtk_widget_set_sensitive (wgt->LAUNCH_BUTTON, !missingexe);
}

void on_HEXEN2 (GtkButton *button, gamewidget_t *wgt) {
	destiny=DEST_H2;
#ifndef DEMOBUILD
	mp_support = 0;
	gtk_widget_set_sensitive (wgt->H2GAME, TRUE);
	gtk_widget_set_sensitive (wgt->HWGAME, FALSE);
	gtk_widget_set_sensitive (wgt->LAN_BUTTON, !h2game);
#else
	gtk_widget_set_sensitive (wgt->LAN_BUTTON, TRUE);
#endif
	UpdateStats(&(wgt->Launch));
}

#ifndef DEMOBUILD
void on_H2MP (GtkButton *button, gamewidget_t *wgt) {
	destiny=DEST_H2;
	mp_support = 1;
	gtk_widget_set_sensitive (wgt->H2GAME, FALSE);
	gtk_widget_set_sensitive (wgt->HWGAME, FALSE);
	gtk_widget_set_sensitive (wgt->LAN_BUTTON, TRUE);
	UpdateStats(&(wgt->Launch));
}
#endif

void on_H2W (GtkButton *button, gamewidget_t *wgt) {
	destiny=DEST_HW;
#ifndef DEMOBUILD
	gtk_widget_set_sensitive (wgt->H2GAME, FALSE);
	gtk_widget_set_sensitive (wgt->HWGAME, TRUE);
#endif
	gtk_widget_set_sensitive (wgt->LAN_BUTTON, FALSE);
	UpdateStats(&(wgt->Launch));
}

#ifndef DEMOBUILD
void H2GameChange (GtkEditable *editable, gamewidget_t *wgt) {
	unsigned short i;
	gchar *tmp = gtk_editable_get_chars (editable, 0, -1);
	for (i=0; i<MAX_H2GAMES; i++) {
		if (strcmp(tmp, h2game_names[i][1]) == 0) {
			g_free(tmp);
			h2game = i;
			gtk_widget_set_sensitive (wgt->LAN_BUTTON, !h2game);
			return;
		}
// Normally, we should be all set within this loop, thus no "else"
	}
}

void HWGameChange (GtkEditable *editable, gpointer user_data) {
	unsigned short i;
	gchar *tmp = gtk_editable_get_chars (editable, 0, -1);
	for (i=0; i<MAX_HWGAMES; i++) {
		if (strcmp(tmp, hwgame_names[i][1]) == 0) {
			g_free(tmp);
			hwgame = i;
			return;
		}
// Normally, we should be all set within this loop, thus no "else"
	}
}
#endif

void ReverseOpt (GtkObject *Unused, int *opt) {
	(*(opt)) = !(*(opt));
}

void on_MORE (GtkButton *button, GtkWidget *window) {
	bmore = !bmore;
	gtk_widget_set_size_request(window, bmore ? 420 : 230, 352);
	gtk_button_set_label(button, bmore ? _("<<< Less") : _("More >>>"));
	// window1 has one child which is fixed1
	gtk_fixed_move(GTK_FIXED(GTK_BIN(window)->child), GTK_WIDGET(button), bmore ? 330 : 132, bmore ? 300 : 272);
//	gtk_widget_set_uposition(GTK_WIDGET(button), bmore ? 330 : 132, bmore ? 300 : 272);
}
