/*
 * help.h
 *
 *  Created on: 04.10.2021
 *      Author: alexey slovesnov
 * copyright(c/c++): 2014-doomsday
 *           E-mail: slovesnov@yandex.ru
 *         homepage: slovesnov.users.sourceforge.net
 */

#ifndef HELP_H_
#define HELP_H_

#include <gtk/gtk.h>
#include <string>
#include "Pixbuf.h"

const std::string LNG[] = { "en", "ru" };
const std::string LNG_LONG[] = { "english", "russian" };

void getTextExtents(std::string text, int height, bool bold, int &w, int &h,
		cairo_t *cr);
void drawTextToCairo(cairo_t *ct, std::string text, int height, bool bold,
		int rleft, int rtop, int rwidth, int rheight, bool centerx, int oy,
		const GdkRGBA &color, bool blackBackground = false);

void adjust(int &v, int min, int max = INT_MAX);
GdkPixbuf* scaleFit(GdkPixbuf *src, int destW, int destH);
bool deleteFileToRecycleBin(const std::string &path);
FILE* open(int i, std::string s);

#endif /* HELP_H_ */
