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

void printHelp(bool toFile, const std::string& s, const char*f, const int l,
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

#endif /* HELP_H_ */
