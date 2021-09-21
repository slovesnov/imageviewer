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

Image::Image(std::string p){
	path=p;
	thumbnail=NULL;

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
	printinfo
	if(thumbnail){
		printinfo
		g_object_unref(thumbnail);
		thumbnail=0;
	}
}



