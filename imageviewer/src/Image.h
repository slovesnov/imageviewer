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

/* Note use mutex for every Image is bad idea too long g_mutex_init & g_mutex_clear so use GdkPixbuf*t
 * which is set by thread. Drawing using thumbnail class member. After loading thread set t class member and call
 * setShowThumbnail() to set thumbnail from t and queue draw if needed. So it's separates writing and reading
 * thumbnail class member.
 *
 * It's possible three options
 * t=thumbnail=nullptr - image isn't loaded
 * t!=nullptr & thumbnail=nullptr - thread loaded image, but setShowThumbnail wasn't called
 * t=thumbnail!=nullptr - thread loaded image, and setShowThumbnail was called
 *
 * loadid is using when user open another folder while current isn't fully loaded yet. In this case
 * need to stop all threads and clear setShowThumbnail() in queue. Now I don't know how to remove setShowThumbnail()
 * from queue so i use loadid which should be equal Frame.loadid otherwise all calls are old and just skipped
 *
 * loadid is also helpful when user deletes image from folder. In this case need to stop threads. Simulate all calls of
 * setShowThumbnail(index) because now image index in vector is now valid. Now it's possible to call erase() image from
 * vector and restart threads with new loadid to load all recent images
 *
 */

//#define IMAGE_COUNTERS

class Image{
	void assign(Image& o);
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
