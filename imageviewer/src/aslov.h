/*
 * aslov.h
 *
 *  Created on: 03.06.2021
 *      Author: alexey slovesnov
 * copyright(c/c++): 2014-doomsday
 *           E-mail: slovesnov@yandex.ru
 *         homepage: slovesnov.users.sourceforge.net
 */

#ifndef ASLOV_H_
#define ASLOV_H_

#include <string>
#include <sstream>
#include <algorithm>
#include <initializer_list>
#ifndef NOGTK
#include <gtk/gtk.h>
#endif

#define GP(a) gpointer(int64_t(a))
#define GP2INT(a) int(int64_t(a))
#define SIZE G_N_ELEMENTS
#define SIZEI(a) int(G_N_ELEMENTS(a))
#define INDEX_OF(a,id) indexOf(a,SIZEI(a),id)
#define IN_ARRAY(array,item) (INDEX_OF(array,item)!=-1)
#ifdef NOGTK
#define g_print printf
#define G_DIR_SEPARATOR '\\'
#define G_N_ELEMENTS(arr)		(sizeof (arr) / sizeof ((arr)[0]))
#endif


extern std::string applicationName, applicationPath, workingDirectory;


//BEGIN application functions
void aslovInit(const char* argv0);
int getApplicationFileSize();
FILE* openApplicationConfig(const char *flags);
FILE* openApplicationLog(const char *flags);
//END application functions


//BEGIN file functions
enum class FILEINFO {
	NAME, EXTENSION, LOWER_EXTENSION, DIRECTORY, SHORT_NAME
};
bool isDir(const char *url);
bool isDir(const std::string& s);
std::string getFileInfo(std::string filepath, FILEINFO fi);

int getFileSize(const std::string &path);
FILE* open(std::string filepath, const char *flags);
//END file functions


//BEGIN string functions
std::string intToString(int v, char separator);
bool stringToInt(const char*d,int&v);
bool startsWith(const char *buff, const char *begin);
bool startsWith(const char *buff, const std::string &begin);
bool endsWith(std::string const &s, std::string const &e);
std::string replaceAll(std::string subject, const std::string &from,
		const std::string &to);
int charIndex(const char *p, char c);

const std::string localeToUtf8(const std::string &s);
const std::string utf8ToLocale(const std::string &s);

//convert localed "s" to lowercase in cp1251
std::string localeToLowerCase(const std::string &s, bool onlyRussainChars =
		false);
std::string utf8ToLowerCase(const std::string &s,
		bool onlyRussainChars = false);

//END string functions


//BEGIN 2 dimensional array functions
template<class T>T** create2dArray(int dimension1, int dimension2){
	T **p = new T*[dimension1];
	for (int i = 0; i < dimension1; i++) {
		p[i] = new T[dimension2];
	}
	return p;
}

template<class T>void delete2dArray(T **p, int dimension1){
	for (int i = 0; i < dimension1; i++) {
		delete[] p[i];
	}
	delete[] p;
}
//END 2 dimensional array functions

//BEGIN pixbuf functions
void getPixbufWH(GdkPixbuf *p,int&w,int&h);
void freePixbuf(GdkPixbuf*&p);
void copy(GdkPixbuf *source, cairo_t *dest, int destx, int desty, int width,
		int height, int sourcex, int sourcey);
//END pixbuf functions

void aslovPrintHelp(bool toFile, const std::string &s, const char *f, const int l,
		const char *fu);

//output info to screen example println("%d %s",1234,"some")
#define println(f, ...)  aslovPrintHelp(0,format(f,##__VA_ARGS__),__FILE__,__LINE__,__func__);

//output info to screen example printl(1234,"some")
#define printl(f, ...)  aslovPrintHelp(0,forma(f,##__VA_ARGS__),__FILE__,__LINE__,__func__);

#define printinfo println("")

//output info to log file printlog("%d %s",1234,"some")
#define printlog(f, ...)  aslovPrintHelp(1,format(f,##__VA_ARGS__),__FILE__,__LINE__,__func__);

//output info  to log file printlo(1234,"some")
#define printlo(f, ...)  aslovPrintHelp(1,forma(f,##__VA_ARGS__),__FILE__,__LINE__,__func__);

#define printloginfo printlog("")

//format to string example format("%d %s",1234,"some")
std::string format(const char *f, ...);

//format to string example forma(1234,"some")
template<typename Arg, typename ... Args>
std::string forma(Arg const &arg, Args const &... args) {
	std::stringstream c;
	c << arg;
	((c << ' ' << args), ...);
	return c.str();
}

int getNumberOfCores();

template <typename Arg, typename... Args>
bool oneOf(Arg const& arg, Args const&... args){
	return ((arg== args)|| ...);
}

template<class T>
bool oneOf(T const& v,std::vector<T> const& t ) {
	return std::find(begin(t), end(t), v) != end(t);
}

//cann't use indexOf name because sometimes compiler cann't deduce
template <typename Arg, typename... Args>
int indexOfV(Arg const& t,Args const&... args) {
	auto l = {args...};
    auto i=std::find(std::begin(l),std::end(l),t);
    return i==std::end(l)?-1:i-std::begin(l);
}

template<class T> int indexOf(const T a[], const unsigned aSize,
		const T e) {
	unsigned i = std::find(a, a + aSize, e) - a;
	return i == aSize ? -1 : i;
}

#endif /* ASLOV_H_ */
