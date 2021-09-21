/*
 * Image.h
 *
 *  Created on: 21.09.2021
 *      Author: alexey slovesnov
 * copyright(c/c++): 2014-doomsday
 *           E-mail: slovesnov@yandex.ru
 *         homepage: slovesnov.users.sourceforge.net
 */

#ifndef IMAGE_H_
#define IMAGE_H_

#include <string>
#include <gtk/gtk.h>
#include <vector>

struct Image{
	std::string path;
	int size;
	GdkPixbuf*thumbnail;

	Image(std::string p);
	~Image();
	static int c1;
	static int c2;
};

typedef std::vector<Image> VImage;


#endif /* IMAGE_H_ */
