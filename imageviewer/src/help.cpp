/*
 * help.cpp
 *
 *  Created on: 04.10.2021
 *      Author: alexey slovesnov
 * copyright(c/c++): 2014-doomsday
 *           E-mail: slovesnov@yandex.ru
 *         homepage: slovesnov.users.sourceforge.net
 */
#include "help.h"
#include "consts.h"
#include "aslov.h"
#include <windows.h>//deleteFileToRecycleBin

void adjust(int &v, int min, int max /*= INT_MAX*/) {
	if (v < min) {
		v = min;
	} else if (v > max) {
		v = max;
	}
}

GdkPixbuf* scaleFit(GdkPixbuf *src, int destW, int destH) {
	int w, h;
	int pw = gdk_pixbuf_get_width(src);
	int ph = gdk_pixbuf_get_height(src);
	if (pw <= destW && ph <= destH) { //image smaller than screen
		w = pw;
		h = ph;
	} else {
		//pw*k<=destW && ph*k<=destH k=min(destW/pw,destH/ph)
		if (destW * ph <= destH * pw) {
			w = destW;
			h = ph * destW / pw;
		} else {
			w = pw * destH / ph;
			h = destH;
		}
	}
	return gdk_pixbuf_scale_simple(src, w, h, GDK_INTERP_BILINEAR);
}

PangoLayout* createPangoLayout(std::string text, int height, bool bold,
		cairo_t *cr, bool withBackground = false) {
	PangoLayout *layout = pango_cairo_create_layout(cr);
	//not all heights supported for Times
	//return pango_font_description_from_string(forma("Times New Roman Normal ",height).c_str());
	std::string s = "Arial ";
	if (bold) {
		s += "bold ";
	}
	s += std::to_string(height);
	PangoFontDescription *desc = pango_font_description_from_string(s.c_str());
	pango_layout_set_font_description(layout, desc);

	if (withBackground) {
		s = "<span background='#333333'>" + text + "</span>";
	} else {
		s = text;
	}
	pango_layout_set_markup(layout, s.c_str(), -1);
	pango_font_description_free(desc);
	return layout;
}

void getTextExtents(std::string text, int height, bool bold, int &w, int &h,
		cairo_t *cr) {
	PangoLayout *layout = createPangoLayout(text.c_str(), height, bold, cr);
	pango_layout_get_pixel_size(layout, &w, &h);
	g_object_unref(layout);
}

void drawTextToCairo(cairo_t *ct, std::string text, int height, bool bold,
		int rleft, int rtop, int rwidth, int rheight, bool centerx, int oy,
		const GdkRGBA &color, bool withBackground/*=false*/) {
	int w, h;
	gdk_cairo_set_source_rgba(ct, &color);
	PangoLayout *layout = createPangoLayout(text, height, bold, ct,
			withBackground);
	pango_layout_get_pixel_size(layout, &w, &h);
	double px = rleft;
	double py = rtop;
	if (centerx) {
		px += (rwidth - w) / 2;
	}
	if (oy == 1) {	//centery
		py += (rheight - h) / 2;
	} else if (oy == 2) {	//bottom
		py += rheight - h;
	}

	cairo_rectangle(ct, rleft, rtop, rwidth - 1, rheight - 1);
	cairo_clip(ct);

	pango_cairo_update_layout(ct, layout);
	cairo_move_to(ct, px, py);
	pango_cairo_show_layout(ct, layout);

	g_object_unref(layout);

	cairo_reset_clip(ct);
}

/* deleteFileToRecycleBin support utf8 chars in file names
 * https://stackoverflow.com/questions/70257751/move-a-file-or-folder-to-the-recyclebin-trash-c17
 */
bool deleteFileToRecycleBin(const std::string &path) {
	auto p = utf8ToLocale(path) + '\0';
	SHFILEOPSTRUCT fileOp;
	fileOp.hwnd = NULL;
	fileOp.wFunc = FO_DELETE;
	fileOp.pFrom = p.c_str();
	fileOp.pTo = NULL;
	fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOERRORUI | FOF_NOCONFIRMATION
			| FOF_SILENT;
	return !SHFileOperation(&fileOp);
}

std::string getShortLanguageString(int i) {
	return LNG[i];
}

guint getKey(guint e, GdkEventKey *event) {
	return e >= 'A' && e <= 'Z' ? event->hardware_keycode : event->keyval;
}
