/*
 * common.h
 *
 *  Created on: 04.06.2021
 *      Author: alexey slovesnov
 * copyright(c/c++): 2014-doomsday
 *           E-mail: slovesnov@yandex.ru
 *         homepage: slovesnov.users.sourceforge.net
 */

#ifndef COMMON_H_
#define COMMON_H_

#include <algorithm>
#include <array>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <windows.h>

bool isDir(const char *url) {
	GStatBuf b;
	if (g_stat(url, &b) != 0) {
		//error get file stat
		return false;
	}

	return S_ISDIR(b.st_mode);
}

bool isDir(const std::string& s) {
	return isDir(s.c_str());
}

enum class FILEINFO {
	name, extension, lowerExtension, directory
};

std::string getFileInfo(std::string filepath, FILEINFO fi) {
	//"c:\\slove.sno\\1\\rr" -> extension = ""
	std::size_t pos = filepath.rfind(G_DIR_SEPARATOR);
	if (fi == FILEINFO::directory) {
		return filepath.substr(0, pos);
	}
	std::string name = filepath.substr(pos + 1);
	if (fi == FILEINFO::name) {
		return name;
	}
	pos = name.rfind('.');
	if (pos == std::string::npos) {
		return "";
	}
	std::string s = name.substr(pos + 1);
	if (fi == FILEINFO::lowerExtension) {
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
			return std::tolower(c);
		});
	}
	return s;
}

void copy(GdkPixbuf *source, cairo_t *dest, int destx, int desty, int width,
		int height, int sourcex, int sourcey) {
	gdk_cairo_set_source_pixbuf(dest, source, destx - sourcex, desty - sourcey);
	cairo_rectangle(dest, destx, desty, width, height);
	cairo_fill(dest);
}

void adjust(int &v, int min,int max = INT_MAX) {
	if (v < min) {
		v = min;
	} else if (v > max) {
		v = max;
	}
}

void adjustCycle(int &v, int n) {
	v %= n;
	if (v < 0) {
		v += n;
	}
}

std::string filechooser(GtkWidget *parent) {
	std::string s;
	bool onlyFolder = false;
	const gint MY_SELECTED = 0;

	GtkWidget *dialog = gtk_file_chooser_dialog_new("Open File",
			GTK_WINDOW(parent),
			onlyFolder ?
					GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER :
					GTK_FILE_CHOOSER_ACTION_OPEN, "_Cancel",
			GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);

	if (!onlyFolder) {
		/* add the additional "Select" button
		 * "select" button allow select folder and file
		 * "open" button can open only files
		 */
		gtk_dialog_add_button(GTK_DIALOG(dialog), "Select", MY_SELECTED);
	}

	gint response = gtk_dialog_run(GTK_DIALOG(dialog));
	auto v=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
	if (response == GTK_RESPONSE_ACCEPT
			|| (!onlyFolder && response == MY_SELECTED && v)) {
		s = v;
	}

	gtk_widget_destroy(dialog);

	return s;
}

template <typename Arg, typename... Args>
bool oneOf(Arg const& arg, Args const&... args){
	return ((arg== args)|| ...);
}

template<class T>
bool oneOf(T const& v,std::vector<T> const& t ) {
	return std::find(begin(t), end(t), v) != end(t);
}

template <typename Arg, typename... Args>
int indexOf(Arg const& t,Args const&... args) {
	auto l = {args...};
    auto i=std::find(begin(l),end(l),t);
    return i==end(l)?-1:i-begin(l);
}

template<class T>
int indexOf(T const a[], int size, const T& item) {
	int i;
	for (i = 0; i < size; i++) {
		if (item == a[i]) {
			return i;
		}
	}
	return -1;
}

int getNumberOfCores() {
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return sysinfo.dwNumberOfProcessors;
}

void scaleFit(GdkPixbuf *src, GdkPixbuf *&dest, int destW, int destH,
		int &w, int &h) {
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
	dest = gdk_pixbuf_scale_simple(src, w, h, GDK_INTERP_BILINEAR);
}

void getPixbufWH(GdkPixbuf *p,int&w,int&h){
	w = gdk_pixbuf_get_width(p);
	h = gdk_pixbuf_get_height(p);
}

//BEGIN from bridge modified
PangoLayout* createPangoLayout(std::string text,int height,cairo_t *cr) {
	PangoLayout *layout = pango_cairo_create_layout(cr);
	//not all heights supported for Times
	//return pango_font_description_from_string(forma("Times New Roman Normal ",height).c_str());
	PangoFontDescription*desc = pango_font_description_from_string(forma("Arial ",height).c_str());
	pango_layout_set_font_description(layout, desc);

	pango_layout_set_markup(layout, text.c_str(), -1);
	pango_font_description_free(desc);
	return layout;
}

void getTextExtents(std::string text,int height,int&w,int&h,cairo_t *cr) {
	PangoLayout *layout = createPangoLayout(text.c_str(),height,cr);
	pango_layout_get_pixel_size(layout, &w, &h);
	g_object_unref(layout);
}

void drawTextToCairo(cairo_t* ct, std::string text,int height, int rleft,int rtop,int rwidth,int rheight,
		bool centerx, bool centery) {
	const GdkRGBA BLACK_COLOR = { 0., 0., 0., 1. };
	int w, h;
	gdk_cairo_set_source_rgba(ct, &BLACK_COLOR);
	PangoLayout *layout = createPangoLayout(text,height,ct);
	pango_layout_get_pixel_size(layout, &w, &h);
	double px = rleft;
	double py = rtop;
	if (centerx) {
		px += (rwidth - w) / 2;
	}
	if (centery) {
		py += (rheight - h) / 2;
	}

	cairo_move_to(ct, px, py);
	pango_cairo_update_layout(ct, layout);
	pango_cairo_show_layout(ct, layout);

	g_object_unref(layout);
}
//END from bridge modified

int getFileSize(std::string const&path){
	/* Note stat() function works bad with utf8 non standard ascii symbols in filename,
	 * may be need encoding, so use g_stat()
	 */
	GStatBuf b;
	if(g_stat(path.c_str(), &b)!=0){
		println("error");
		return 0;
	}
	return b.st_size;

}

#endif /* COMMON_H_ */
