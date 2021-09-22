/*
 * Image.cpp
 *
 *  Created on: 21.09.2021
 *      Author: alexey slovesnov
 * copyright(c/c++): 2014-doomsday
 *           E-mail: slovesnov@yandex.ru
 *         homepage: slovesnov.users.sourceforge.net
 */

#include "Image.h"
#include "help.h"

//int Image::c1=0;
//int Image::c2=0;

Image::Image(std::string p){
	path=p;
	thumbnail=NULL;
//	loaded=false;

	/* Note stat() function works bad with utf8 non standard ascii symbols in filename,
	 * may be need encoding, so use g_stat()
	 */
	GStatBuf b;
	if(g_stat(path.c_str(), &b)!=0){
		println("error");
		size=0;
	}
	else{
		size=b.st_size;
	}
	//g_mutex_init(&mutex);
}

Image::~Image(){
//	c1++;
	if(thumbnail){
//		c2++;
		freePixbuf(thumbnail);
	}
	//g_mutex_clear(&mutex);
}

//bool Image::isLoaded() {
//	g_mutex_lock(&mutex);
//	bool b=loaded;
//	g_mutex_unlock(&mutex);
//	return b;
//}
//
//void Image::setThumbnail(GdkPixbuf *p) {
//	g_mutex_lock(&mutex);
//	thumbnail=p;
//	loaded=true;
//	g_mutex_unlock(&mutex);
//}
