/*
 * help.cpp
 *
 *  Created on: 03.06.2021
 *      Author: alexey slovesnov
 * copyright(c/c++): 2014-doomsday
 *           E-mail: slovesnov@yandex.ru
 *         homepage: slovesnov.users.sourceforge.net
 */

#include <cstring>
#include "help.h"

const char LOG_FILE_NAME[] = "log.txt";

void printHelp(bool toFile, const std::string& s, const char*f, const int l,
		const char*fu) {
	const char*p = strrchr(f, G_DIR_SEPARATOR);
	p=p ? p + 1 : f;
	if (toFile) {
		time_t t = time(NULL);
		tm* q = localtime(&t);
		FILE*w = fopen(LOG_FILE_NAME, "a");
		fprintf(w, "%s %s:%d %s() %02d:%02d:%02d %02d.%02d.%d\n", s.c_str(), p,
				l, fu, q->tm_hour, q->tm_min, q->tm_sec, q->tm_mday, q->tm_mon + 1,
				q->tm_year + 1900);
		fclose(w);
	}
	else {
		g_print("%-40s %s:%d %s()\n", s.c_str(), p, l, fu);
	}
}

//format to string example format("%d %s",1234,"some")
std::string format(const char* f, ...) {
	va_list a;
	va_start(a, f);
	size_t size = vsnprintf(nullptr, 0, f, a) + 1;
	va_end(a);
	std::string s;
	if (size > 1) {
		s.resize(size);
		va_start(a, f);
		vsnprintf(&s[0], size, f, a);
		va_end(a);
		s.resize(size - 1);
	}
	return s;
}


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

std::string getFileInfo(std::string filepath, FILEINFO fi) {
	//"c:\\slove.sno\\1\\rr" -> extension = ""
	std::size_t pos = filepath.rfind(G_DIR_SEPARATOR);
	if (fi == FILEINFO::DIRECTORY) {
		return filepath.substr(0, pos);
	}
	std::string name = filepath.substr(pos + 1);
	if (fi == FILEINFO::NAME) {
		return name;
	}
	pos = name.rfind('.');
	if (fi == FILEINFO::SHORT_NAME) {
		if (pos == std::string::npos) {
			return name;
		}
		else{
			return name.substr(0, pos);
		}
	}
	if (pos == std::string::npos) {
		return "";
	}
	std::string s = name.substr(pos + 1);
	if (fi == FILEINFO::LOWER_EXTENSION) {
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

void adjust(int &v, int min,int max /*= INT_MAX*/) {
	if (v < min) {
		v = min;
	} else if (v > max) {
		v = max;
	}
}

//void adjustCycle(int &v, int n) {
//	v %= n;
//	if (v < 0) {
//		v += n;
//	}
//}

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
PangoLayout* createPangoLayout(std::string text,int height,bool bold,cairo_t *cr) {
	PangoLayout *layout = pango_cairo_create_layout(cr);
	//not all heights supported for Times
	//return pango_font_description_from_string(forma("Times New Roman Normal ",height).c_str());
	std::string s="Arial ";
	if(bold){
		s+="bold ";
	}
	s+=std::to_string(height);
	PangoFontDescription*desc = pango_font_description_from_string(s.c_str());
	pango_layout_set_font_description(layout, desc);

	pango_layout_set_markup(layout, text.c_str(), -1);
	pango_font_description_free(desc);
	return layout;
}

void getTextExtents(std::string text,int height,bool bold,int&w,int&h,cairo_t *cr) {
	PangoLayout *layout = createPangoLayout(text.c_str(),height,bold,cr);
	pango_layout_get_pixel_size(layout, &w, &h);
	g_object_unref(layout);
}

void drawTextToCairo(cairo_t* ct, std::string text,int height,bool bold, int rleft,int rtop,int rwidth,int rheight,
		bool centerx, int oy,const GdkRGBA&color) {
	int w, h;
	gdk_cairo_set_source_rgba(ct, &color);
	PangoLayout *layout = createPangoLayout(text,height,bold,ct);
	pango_layout_get_pixel_size(layout, &w, &h);
	double px = rleft;
	double py = rtop;
	if (centerx) {
		px += (rwidth - w) / 2;
	}
	if (oy==1) {//centery
		py += (rheight - h) / 2;
	}
	else if (oy==2) {//bottom
		py += rheight - h;
	}

	cairo_rectangle(ct,rleft, rtop, rwidth-1, rheight-1);
	cairo_clip(ct);

	pango_cairo_update_layout(ct, layout);
	cairo_move_to(ct, px, py);
	pango_cairo_show_layout(ct, layout);

	g_object_unref(layout);

	cairo_reset_clip(ct);
}
//END from bridge modified

void freePixbuf(GdkPixbuf*&p){
	if (p) {
		g_object_unref(p);
		p = 0;
	}
}
