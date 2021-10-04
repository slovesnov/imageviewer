/*
 * help.h
 *
 *  Created on: 03.06.2021
 *      Author: alexey slovesnov
 * copyright(c/c++): 2014-doomsday
 *           E-mail: slovesnov@yandex.ru
 *         homepage: slovesnov.users.sourceforge.net
 */

#ifndef HELP_H_
#define HELP_H_

#define GP(a) gpointer(int64_t(a))
#define GP2INT(a) int(int64_t(a))
//#define SIZE G_N_ELEMENTS
#define SIZEI(a) int(G_N_ELEMENTS(a))
#define INDEX_OF(a,id) indexOf(a,SIZEI(a),id)

#include <string>
#include <sstream>
#include <algorithm>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <windows.h>
#include <vector>

void aslovPrintHelp(bool toFile, const std::string& s, const char*f, const int l,
		const char*fu);

//output info to screen example println("%d %s",1234,"some")
#define println(f, ...)  printHelp(0,format(f,##__VA_ARGS__),__FILE__,__LINE__,__func__);

//output info to screen example printl(1234,"some")
#define printl(f, ...)  printHelp(0,forma(f,##__VA_ARGS__),__FILE__,__LINE__,__func__);

#define printinfo println("")

//format to string example format("%d %s",1234,"some")
std::string format(const char* f, ...);

//format to string example forma(1234,"some")
template <typename Arg, typename... Args>
std::string forma(Arg const& arg, Args const&... args){
	std::stringstream c;
	c << arg;
	((c << ' ' << args), ...);
	return c.str();
}

enum class FILEINFO {
	NAME, EXTENSION, LOWER_EXTENSION, DIRECTORY, SHORT_NAME
};

bool isDir(const char *url);
bool isDir(const std::string& s);
std::string getFileInfo(std::string filepath, FILEINFO fi);
void copy(GdkPixbuf *source, cairo_t *dest, int destx, int desty, int width,
		int height, int sourcex, int sourcey);
void adjust(int &v, int min,int max = INT_MAX);
std::string filechooser(GtkWidget *parent);


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

int getNumberOfCores();
void scaleFit(GdkPixbuf *src, GdkPixbuf *&dest, int destW, int destH,
		int &w, int &h);
void getPixbufWH(GdkPixbuf *p,int&w,int&h);
void getTextExtents(std::string text,int height,bool bold,int&w,int&h,cairo_t *cr);
void drawTextToCairo(cairo_t* ct, std::string text,int height,bool bold, int rleft,int rtop,int rwidth,int rheight,
		bool centerx, int oy,const GdkRGBA&color);

void freePixbuf(GdkPixbuf*&p);
bool toInt(const char*d,int&v);

#endif /* HELP_H_ */
