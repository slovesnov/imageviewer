/*
 * help.cpp
 *
 *  Created on: 03.06.2021
 *      Author: alexey slovesnov
 * copyright(c/c++): 2014-doomsday
 *           E-mail: slovesnov@yandex.ru
 *         homepage: slovesnov.users.sourceforge.net
 */

#include <gtk/gtk.h>
#include <string>
#include <cstring>

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


