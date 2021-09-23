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

#ifdef IMAGE_COUNTERS
int Image::c1=0;
int Image::c2=0;
#endif


Image::Image(std::string p,int id){
	path=p;
	loadid=id;
	t=thumbnail=nullptr;

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
}

Image::~Image(){
	//see Image.h for documentation
#ifdef IMAGE_COUNTERS
		c1++;
#endif
	if(t){
#ifdef IMAGE_COUNTERS
		c2++;
#endif
		freePixbuf(t);
	}
}

Image::Image(Image &&o) {
	assign(o);
}

Image& Image::operator=(Image &&o) {
	//it thumpbnail!=nullptr then thumpbnail=t and don't need free memory
	freePixbuf(t);
	assign(o);
	return *this;
}

void Image::assign(Image& o){
	path = o.path;
	size = o.size;
	loadid=o.loadid;
	thumbnail = o.thumbnail;
	t = o.t;
	o.t = o.thumbnail = nullptr;
}


