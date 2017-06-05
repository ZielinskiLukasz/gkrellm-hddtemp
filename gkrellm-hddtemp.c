/* Hard disk drive temperature plugin for Gkrellm
 *
 * Copyright (C) 2001  Emmanuel VARAGNAT <coredump@free.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <wait.h>
#include <sys/ipc.h>
#include <errno.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <assert.h>

#ifdef GKRELLM2
#include <gkrellm2/gkrellm.h>
#else
#include <gkrellm/gkrellm.h>
#define GkrellmPanel     Panel
#define GkrellmTextstyle TextStyle
#define GkrellmDecal     Decal
#define GkrellmMonitor   Monitor
#define GkrellmStyle     Style
#endif

#define	CONFIG_NAME	"HDDtemp"
#define	STYLE_NAME	"hddtemp"

typedef struct {
  GkrellmTextstyle  ts;
  gint		    lbearing,
                    rbearing,
                    width,
                    ascent,
                    descent;
} Extents;

typedef struct {
  GkrellmPanel  *panel;
  GkrellmDecal  *d_hdd, *d_temp,
                *d_unit, *d_degree;
} Drive;

typedef struct {
  char *drive;
  char *model;
  char *value;
  char *unit;
} DriveInfos;
  
typedef struct {
  int   code;      /* 0 for no error */
  char  msg[255];
  GkrellmPanel *panel;
  GkrellmDecal *decal;
} Error;

static GkrellmMonitor  *mon_hddtemp;
static Extents         hdd_extents, temp_extents, unit_extents, degree_extents;
static gint	       style_id;

static Drive    *drives = NULL;
static guint    nb_drives;

static Error    error = { 0 , "" , NULL, NULL };


static void string_extents(gchar *string, Extents *ext) {
  gdk_string_extents(ext->ts.font, string, &ext->lbearing, &ext->rbearing,
		     &ext->width, &ext->ascent, &ext->descent);
}

/*
static int drive_compare(const void* s1, const void*s2) {
  return strcmp((*(Drive**)s1)->name, (*(Drive**)s2)->name);
}
*/

char *parse_next(char *start, char separator, DriveInfos *infos) {

  if(*start == '\0')
    return NULL;

  infos->drive = ++start;
  while(*start != separator)
    start++;
  *start = '\0';

  infos->model = ++start;
  while(*start != separator)
    start++;
  *start = '\0';

  infos->value = ++start;
  while(*start != separator)
    start++;
  *start = '\0';

  infos->unit = ++start;
  while(*start != separator)
    start++;
  *start = '\0';

  start++;

  return start;
}

char * query_hddtemp_deamon(const char * server, unsigned int port) {
  struct sockaddr_in  saddr;
  struct hostent      *he;
  int                 sk;
  int                 n, pos;
  const char          *serv_name = server;
  char                buff[2];

  static char         *str = NULL;
  static unsigned int str_size = 0;

  if((sk = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("gkrellm-hddtemp: socket");
    return NULL;
  }
  
  memset(&saddr, 0, sizeof(struct sockaddr_in));
  if((he = gethostbyname(serv_name)) == NULL) {
    perror("gkrellm-hddtemp: gethostbyaddr");
    return NULL;
  }
  memcpy(&(saddr.sin_addr.s_addr), he->h_addr, he->h_length);

  saddr.sin_family = AF_INET;
  saddr.sin_port = htons(port);

  if(connect(sk, (struct sockaddr *)&saddr, (socklen_t)sizeof(saddr)) == -1) {
    perror("gkrellm-hddtemp: connect");
    return NULL;
  }

  if(str == NULL) {
    str_size = 2;

    if((str = malloc(str_size * sizeof(char))) == NULL) {
      perror("gkrellm-hddtemp: malloc");
      return NULL;
    }
  }

  pos = 0;
  while((n=read(sk, &buff, sizeof(buff))) > 0) {
    if(pos+n+1 > str_size) {
      str_size *= 2;
      if((str=realloc(str, str_size)) == NULL) {
	perror("gkrellm-hddtemp: realloc");
	return NULL;
      }
    }

    strncpy(str+pos, buff, n);
    pos += n;
  }
  str[pos] = '\0';  

  close(sk);

  return strdup(str);
}


static void update_plugin() {
  /* every minute */
  if ( (GK.timer_ticks % 600) == 0) {
    char *reply;
    char *p, *drv;
    char sep = '\0';
    DriveInfos infos;
    unsigned int i, j;

    if(error.code) {
      gkrellm_draw_decal_text(error.panel, error.decal, "ERROR", -1);
      return;
    }

    reply = query_hddtemp_deamon("127.0.0.1", 7634);    

    if(reply) {
      sep = reply[0];

      assert(sep != '\0');
    }

    p = reply;
    for(i = 0; reply && (p = parse_next(p, sep, &infos)) && i < nb_drives; i++ ) {

      if((drv=strrchr (infos.drive, '/')))
	infos.drive = drv + 1;

      gkrellm_draw_decal_text(drives[i].panel, drives[i].d_hdd, infos.drive, -1);
      gkrellm_draw_decal_text(drives[i].panel, drives[i].d_temp, infos.value, -1);
      if(infos.unit[0] == '*') {
	gkrellm_draw_decal_text(drives[i].panel, drives[i].d_degree, "", -1);
	gkrellm_draw_decal_text(drives[i].panel, drives[i].d_unit, "", -1);
      } else {
	gkrellm_draw_decal_text(drives[i].panel, drives[i].d_degree, "°", -1);
	gkrellm_draw_decal_text(drives[i].panel, drives[i].d_unit, infos.unit, -1);
      }
      
      gkrellm_draw_panel_layers(drives[i].panel);
    }

    for(j = i; j < nb_drives; i++ ) {
	gkrellm_panel_destroy(drives[j].panel);
    }

    nb_drives = j;

    if(reply)
      free(reply);
  }
}


static gint panel_expose_event(GtkWidget *widget, GdkEventExpose *ev, gpointer p) {
  GkrellmPanel *panel = (GkrellmPanel*)p;

  gdk_draw_pixmap(widget->window,
		  widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
		  panel->pixmap,
		  ev->area.x, ev->area.y,
		  ev->area.x, ev->area.y,
		  ev->area.width, ev->area.height);
  return FALSE;
}


static void create_plugin(GtkWidget *vbox, gint first_create) {
  GkrellmStyle	*style;
  char *reply;
  char sep;
  unsigned int i, len;
  
  reply = query_hddtemp_deamon("127.0.0.1", 7634);

  /* ERROR */
  if(reply == NULL) {
    error.code = 1;

    /* Create a panel to display the error */
    error.panel = gkrellm_panel_new0();

    style = gkrellm_meter_style(style_id);
    
    hdd_extents.ts = *gkrellm_meter_textstyle(style_id);
#ifdef GKRELLM2
    hdd_extents.ts.font = gkrellm_default_font(0);
#else
    hdd_extents.ts.font = GK.small_font;
#endif
    string_extents("ERROR", &hdd_extents);
    error.decal = gkrellm_create_decal_text(error.panel, "ERROR", &hdd_extents.ts,
					    style, 0, -1, 0);    
    /*
    error.decal->x = (panel->h - decal->h) / 2;
    error.decal->y = (panel->w - decal->w) / 2;
    */
    error.decal->x = 4;
    error.decal->y = 4;

    /* creation */
    gkrellm_panel_configure(error.panel, NULL, style);
    gkrellm_panel_create(vbox, mon_hddtemp, error.panel);

    gtk_signal_connect(GTK_OBJECT (error.panel->drawing_area), "expose_event",
		       (GtkSignalFunc) panel_expose_event, error.panel);

    gkrellm_draw_decal_text(error.panel, error.decal, "ERROR", -1);

    return;
  }

  /* the first character is used as the separator */
  sep = reply[0];

  if(sep == '\0')
    return;

  /* calculate the number of drives */
  nb_drives = 0;
  len = strlen(reply);
  for(i = 0; i < len; i++) {
    if(reply[i] == sep
       && (reply[i+1] == '\0' || reply[i+1] == sep)) {

      nb_drives++;
    }
  }
  free(reply);

  /* allocation of panel structs */
  drives = NULL;
  if(nb_drives)
    drives = (Drive*)malloc(nb_drives * sizeof(Drive));
  
  if(drives == NULL) {
    perror("gkrellm-hddtemp: allocation error:");
    exit(1);
  }
  
  for(i = 0; i < nb_drives; i++) {
    drives[i].panel  = NULL;
    drives[i].d_hdd  = NULL;
    drives[i].d_temp = NULL;
  }

  /* init styles and fonts */
  style = gkrellm_meter_style(style_id);
    
  hdd_extents.ts = *gkrellm_meter_textstyle(style_id);
#ifdef GKRELLM2
  hdd_extents.ts.font = gkrellm_default_font(0);
#else
  hdd_extents.ts.font = GK.small_font;
#endif
  string_extents("8M", &hdd_extents);

  temp_extents.ts = *gkrellm_meter_alt_textstyle(style_id);
#ifdef GKRELLM2
  temp_extents.ts.font = gkrellm_default_font(2);
#else
  temp_extents.ts.font = GK.large_font;
#endif
  string_extents("88", &temp_extents);    

  unit_extents.ts = *gkrellm_meter_alt_textstyle(style_id);
#ifdef GKRELLM2
  unit_extents.ts.font = gkrellm_default_font(0);
#else
  unit_extents.ts.font = GK.small_font;
#endif
  string_extents("W", &unit_extents);    

  degree_extents.ts = *gkrellm_meter_alt_textstyle(style_id);
#ifdef GKRELLM2
  degree_extents.ts.font = gkrellm_default_font(0);
#else
  degree_extents.ts.font = GK.small_font;
#endif
  string_extents("°", &degree_extents);    


  /* create and initialize each panel */
  for(i = 0; i < nb_drives; i++) {
    if (first_create) {
      drives[i].panel = gkrellm_panel_new0();
    }

    /* drive name */
    drives[i].d_hdd =  gkrellm_create_decal_text(drives[i].panel, "hdd", &hdd_extents.ts,
						  style, 0, -1, 0);
    /* temperature */
    drives[i].d_temp = gkrellm_create_decal_text(drives[i].panel, "88", &temp_extents.ts,
						  style, 0, -1, 0);
    /* unit */
    drives[i].d_unit = gkrellm_create_decal_text(drives[i].panel, "W", &unit_extents.ts,
						  style, 0, -1, 0);
    /* degree */
    drives[i].d_degree = gkrellm_create_decal_text(drives[i].panel, "°", &degree_extents.ts,
						  style, 0, -1, 0);

    /* placement */
    drives[i].d_hdd->x = 4;
    drives[i].d_temp->x = drives[i].d_hdd->x + drives[i].d_hdd->w + 2;
    drives[i].d_temp->y = 4;
    drives[i].d_hdd->y = drives[i].d_temp->y + ( drives[i].d_temp->h - drives[i].d_hdd->h ) / 2 ;

    drives[i].d_degree->x = drives[i].d_temp->x + drives[i].d_temp->w + 1;
    drives[i].d_unit->x = drives[i].d_degree->x + drives[i].d_degree->w;
    drives[i].d_degree->y = drives[i].d_temp->y - 1;
    drives[i].d_unit->y = drives[i].d_temp->y + drives[i].d_temp->h - drives[i].d_unit->h;    

    /* creation */
    gkrellm_panel_configure(drives[i].panel, NULL, style);
    gkrellm_panel_create(vbox, mon_hddtemp, drives[i].panel);

    if (first_create) {
      gtk_signal_connect(GTK_OBJECT (drives[i].panel->drawing_area), "expose_event",
			 (GtkSignalFunc) panel_expose_event, drives[i].panel);
    }
  }
}


static void config_plugin(GtkWidget *vbox) {
  gtk_box_pack_start(GTK_BOX(vbox), gtk_label_new("Gkrellm plugin for hddtemp (v0.2)\n\n"
						  "contact me: coredump@free.fr\n"
						  "website: http://coredump.free.fr/linux"), TRUE, TRUE, 0);
}


static GkrellmMonitor	plugin_mon = {
  CONFIG_NAME,       /* Title for config clist.   */
  0,		     /* Id,  0 if a plugin       */
  create_plugin,     /* The create function      */
  update_plugin,     /* The update function      */
  config_plugin,     /* The config tab create function   */
  NULL,		     /* Apply the config function        */
  
  NULL,		     /* Save user config */
  NULL,		     /* Load user config */
  NULL,		     /* config keyword   */
  
  NULL,		      /* Undefined 2	*/
  NULL,		      /* Undefined 1	*/
  NULL,		      /* private	*/
  
  MON_DISK,	      /* Insert plugin before this monitor	*/
  
  NULL,		      /* Handle if a plugin, filled in by GKrellM     */
  NULL		      /* path if a plugin, filled in by GKrellM       */
};


#ifndef GKRELLM2
GkrellmMonitor * init_plugin() {
#else
GkrellmMonitor * gkrellm_init_plugin(void) {
#endif
  style_id = gkrellm_add_meter_style(&plugin_mon, STYLE_NAME);
  mon_hddtemp = &plugin_mon;

  return &plugin_mon;
}
