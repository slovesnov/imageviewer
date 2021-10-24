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
#include "aslov.h"

Image::Image(std::string p,int id){
	path=p;
	loadid=id;
	t=thumbnail=nullptr;
	size=getFileSize(path);
}

Image::~Image(){
	//see Image.h for documentation
	free();
}

Image::Image(Image &&o) {
	assign(o);
}

Image& Image::operator=(Image &&o) {
	//it thumpbnail!=nullptr then thumpbnail=t and don't need free memory
	free();
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

void Image::free() {
	if(t){
		::free(t);
	}
}
