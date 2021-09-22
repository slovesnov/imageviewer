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

/* Note use mutex for every Image is bad idea too long g_mutex_init & g_mutex_clear
 *
 */
//#define IMAGE_COUNTERS

class Image{
public:
	std::string path;
	int size,loadid;
	//t - is used for thread loading, see Image::~Image()
	GdkPixbuf*thumbnail,*t;

	Image(std::string p,int id);
	~Image();

	//for correct VImage.erase()
	Image(Image&& o);
	Image& operator=(Image&& o);

#ifdef IMAGE_COUNTERS
	static int c1;
	static int c2;
#endif
};

typedef std::vector<Image> VImage;


#endif /* IMAGE_H_ */
