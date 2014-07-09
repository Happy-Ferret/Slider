/*****************************************************\
* MEDIA.C
* By: Jesse McClure (c) 2012-2014
* See slider.c or COPYING for license information
\*****************************************************/

#include "slider.h"
#include "xlib-actions.h"

#define MAX_COMMAND	256

typedef struct MediaLink {
	PopplerRectangle pdfrect;
	PopplerRectangle rect;
	PopplerAnnotType type;
	PopplerAnnot *link;
} MediaLink;

static void media_link(MediaLink *);
static void media_sound(MediaLink *);
static void media_movie(MediaLink *);
static MediaLink *mouse_select();
static void spawn(PopplerAnnotType, const char *, PopplerRectangle *);

static PopplerDocument *pdf;
static PopplerPage *page;
static double dx, dy, scx, scy;
static double pdfw, pdfh;
static MediaLink *ml = NULL;
static int nml = 0;
const char *atypes[] = {
	[POPPLER_ANNOT_UNKNOWN]				= "Unknown",
	[POPPLER_ANNOT_FREE_TEXT]			= "FreeText",
	[POPPLER_ANNOT_TEXT]					= "Text",
	[POPPLER_ANNOT_LINK]					= "Link",
	[POPPLER_ANNOT_LINE]					= "Line",
	[POPPLER_ANNOT_SQUARE]				= "Square",
	[POPPLER_ANNOT_CIRCLE]				= "Circle",
	[POPPLER_ANNOT_POLYGON]				= "Polygon",
	[POPPLER_ANNOT_POLY_LINE]			= "Polyline",
	[POPPLER_ANNOT_HIGHLIGHT]			= "Highlight",
	[POPPLER_ANNOT_UNDERLINE]			= "Underline",
	[POPPLER_ANNOT_SQUIGGLY]			= "Squiggly",
	[POPPLER_ANNOT_STRIKE_OUT]			= "Strikeout",
	[POPPLER_ANNOT_STAMP]				= "Stamp",
	[POPPLER_ANNOT_CARET]				= "Caret",
	[POPPLER_ANNOT_INK]					= "Ink",
	[POPPLER_ANNOT_POPUP]				= "Popup",
	[POPPLER_ANNOT_FILE_ATTACHMENT]	= "File",
	[POPPLER_ANNOT_SOUND]				= "Sound",
	[POPPLER_ANNOT_MOVIE]				= "Movie",
	[POPPLER_ANNOT_WIDGET]				= "Widget",
	[POPPLER_ANNOT_SCREEN]				= "Screen",
	[POPPLER_ANNOT_PRINTER_MARK]		= "Mark",
	[POPPLER_ANNOT_TRAP_NET]			= "TrapNet",
	[POPPLER_ANNOT_WATERMARK]			= "Watermark",
	[POPPLER_ANNOT_3D]					= "3D",
};
static void (*media_handler[POPPLER_ANNOT_3D]) (MediaLink *) = {
	[POPPLER_ANNOT_LINK]					= media_link,
	[POPPLER_ANNOT_SOUND]				= media_sound,
	[POPPLER_ANNOT_MOVIE]				= media_movie,
};


//static void action_launch(PopplerAction *act, PopplerRectangle r) {
//	PopplerActionLaunch *l = &act->launch;
//	char sys_cmd[MAX_COMMAND];
//	memset(sys_cmd, 0, MAX_COMMAND);
//	sprintf(sys_cmd, "%s %s %04g %04g %04g %04g",
//		l->file_name, (!l->params ? "" : l->params),
//		r.x1, r.y1, r.x2, r.y2);
//	if (sys_cmd[0] == '\0') return;
//	if (conf.launch) system(sys_cmd);
//	else fprintf(stderr, "Action launch blocked: \"%s\"\n");
//}

int action(const char *cmd) {
	/* read annots */
	if (!(pdf=poppler_document_new_from_file(show->uri, NULL, NULL)))
		return;
	page = poppler_document_get_page(pdf, show->cur);
	poppler_page_get_size(page, &pdfw, &pdfh);
	PopplerAnnot *annot;
	GList *annots, *list;
	annots = poppler_page_get_annot_mapping(page);
	scx = show->w / pdfw, scy = show->h / pdfh;
	dx = dy = 0.0;
	if (conf.lock_aspect) {
		if (scx > scy) dx = (show->w - pdfw * (scx=scy)) / 2.0;
		else dy = (show->h - pdfh * (scy=scx)) / 2.0;
	}
	double tmp;
	for (list = annots, nml = 0; list; list = list->next, nml++) {
		ml = realloc(ml, (nml + 1) * sizeof(MediaLink));
		ml[nml].rect = ((PopplerAnnotMapping *)list->data)->area;
		ml[nml].pdfrect = ml[nml].rect;
		annot = ((PopplerAnnotMapping *)list->data)->annot;
		ml[nml].link = annot;
		ml[nml].type = poppler_annot_get_annot_type(annot);
		/* convert pdf coordinates to screen coordinates */
		tmp = pdfh - ml[nml].rect.y1;
		ml[nml].rect.y1 = pdfh - ml[nml].rect.y2;
		ml[nml].rect.y2 = tmp;
		ml[nml].rect.x1 = scx * ml[nml].rect.x1 + dx;
		ml[nml].rect.x2 = scx * ml[nml].rect.x2 + dx;
		ml[nml].rect.y1 = scy * ml[nml].rect.y1 + dy;
		ml[nml].rect.y2 = scy * ml[nml].rect.y2 + dy;
	}
	/* determine selection method, and select annots */
	MediaLink *sel = NULL;
	int nsel;
	char *opt = strchr(cmd,' ');
	while (opt && *opt == ' ') opt++;
	if (!opt) sel = mouse_select();
	else if ( (nsel = atoi(opt)) ) sel = &ml[nsel - 1];
	else {
		/* TODO select by type */
	}
	XDefineCursor(dpy, wshow, invisible_cursor);
	if (sel) {
		if (media_handler[sel->type]) media_handler[sel->type](sel);
		else fprintf(stderr,"No media handler for type \"%s\"\n",
				atypes[sel->type]);
	}
	/* clean up and exit */
	if (ml) free(ml);
	ml = NULL;
	poppler_page_free_annot_mapping(annots);
	draw(None);
}

void media_link(MediaLink *m) {
	char cmd[MAX_COMMAND];
	cmd[0] = '\0';
	/* convert annot link to action link */
	PopplerAction *act = NULL;
	PopplerRectangle rect;
	PopplerLinkMapping *lmap;
	GList *links, *list;
	links = poppler_page_get_link_mapping(page);
	for (list = links; list; list = list->next) {
		lmap = list->data;
		rect = lmap->area;
		if (	(rect.x1 == m->pdfrect.x1) &&
				(rect.x2 == m->pdfrect.x2) &&
				(rect.y1 == m->pdfrect.y1) &&
				(rect.y2 == m->pdfrect.y2) ) break;
	}
	if (list) act = lmap->action;
	/* determine type of action link */
	if (!act) return;
	if (act->type == POPPLER_ACTION_GOTO_DEST) {
		PopplerDest *d, *dest = (&act->goto_dest)->dest;
		if (dest->type == POPPLER_DEST_NAMED) {
			d = poppler_document_find_dest(pdf, dest->named_dest);
			if (d) {
				show->cur = d->page_num - 1;
				poppler_dest_free(d);
			}
		}
		else {
			show->cur = dest->page_num - 1;
		}
	}
	else if (act->type == POPPLER_ACTION_NAMED) {
		PopplerActionNamed *n = &act->named;
		PopplerDest *d = poppler_document_find_dest(pdf, n->named_dest);
		if (d) {
			show->cur = d->page_num - 1;
			poppler_dest_free(d);
		}
	}
	else if (act->type == POPPLER_ACTION_LAUNCH) {
		//action_launch(act, m);
	}
	else if (act->type == POPPLER_ACTION_URI) {
		PopplerActionUri *uri = &act->uri;
		spawn(POPPLER_ANNOT_LINK, uri->uri, &m->rect);
	}
	else if (act->type == POPPLER_ACTION_MOVIE) {
		PopplerActionMovie *mov = &act->movie;
		spawn(POPPLER_ANNOT_MOVIE, poppler_movie_get_filename(mov->movie), &m->rect);
	}
	else if (act->type == POPPLER_ACTION_RENDITION) {
		PopplerActionRendition *r = &act->rendition;
		spawn(POPPLER_ANNOT_SOUND, poppler_media_get_filename(r->media), &m->rect);
	}
	else {
		fprintf(stderr,"Action type %d not recognized\n", act->type);

	}
}

void media_sound(MediaLink *m) {
	//TODO
//spawn(POPPLER_ANNOT_SOUND, poppler_media_get_filename(r->media), &m->rect);
}

void media_movie(MediaLink *m) {
	PopplerMovie *mov;
	mov = poppler_annot_movie_get_movie((PopplerAnnotMovie *) m->link);
	spawn(m->type, poppler_movie_get_filename(mov), &m->rect);
}

MediaLink *mouse_select() {
	XUndefineCursor(dpy, wshow);
	XEvent ev;
	XMaskEvent(dpy, ButtonPressMask | KeyPressMask, &ev);
	if (ev.type == KeyPress) XPutBackEvent(dpy, &ev);
	else if (ev.type == ButtonPress) {
		int i;
		for (i = 0; i < nml; i++) {
			if ( (ml[i].rect.x1 < ev.xbutton.x) &&
					(ml[i].rect.x2 > ev.xbutton.x) &&
					(ml[i].rect.y1 < ev.xbutton.y) &&
					(ml[i].rect.y2 > ev.xbutton.y) )
				break;
		}
		if (i < nml) return &ml[i];
	}
	XFlush(dpy);
	return NULL;
}

void spawn(PopplerAnnotType type, const char *s, PopplerRectangle *r) {
	char cmd[MAX_COMMAND];
	cmd[0] = '\0';
	char num[8];
	const char *fmt = conf.media_link[type];
	if (!fmt) {
		fprintf(stderr,"No media handler for \"%s\"\n", atypes[type]);
		return;
	}
	int i, len;
	for (i = 0; i < strlen(fmt); i++) {
		if (fmt[i] == '%' && i + 1 < strlen(fmt)) {
			switch (fmt[i+1]) {
				case '%': strcat(cmd, "%"); break;
				case 's': strcat(cmd, s); break;
				case 'x': case 'X':
					sprintf(num, "%d", (int) (r->x1 + 0.5));
					strcat(cmd, num); break;
				case 'y': case 'Y':
					sprintf(num, "%d", (int) (r->y1 + 0.5));
					strcat(cmd, num); break;
				case 'w': case 'W':
					sprintf(num, "%d", (int) (r->x2 - r->x1 + 0.5));
					strcat(cmd, num); break;
				case 'h': case 'H':
					sprintf(num, "%d", (int) (r->y2 - r->y1 + 0.5));
					strcat(cmd, num); break;
			}
			i++;
		}
		else {
			len = strlen(cmd);
			cmd[len+1] = '\0';
			cmd[len] = fmt[i];
		}
	}
/*
	char **arg = NULL;
	char *tok;
	int n = 0;
	for (tok = strtok(cmd, " "); tok; tok = strtok(NULL, " ")) {
		arg = realloc(arg, (n+1) * sizeof(char *));
		arg[n] = strdup(tok);
	}
*/
fprintf(stderr,"%s\n",cmd);
	system(cmd);
}

