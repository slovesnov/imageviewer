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

Image::Image(std::string p, int id) {
	path = p;
	loadid = id;
	thumbnail = nullptr;
	size = getFileSize(path);
}

Image::~Image() {
	free();
}

Image::Image(Image &&o) {
	assign(o);
}

Image& Image::operator=(Image &&o) {
	free();
	assign(o);
	return *this;
}

void Image::assign(Image &o) {
	path = o.path;
	size = o.size;
	loadid = o.loadid;
	thumbnail = o.thumbnail.load();
	o.thumbnail = nullptr;
}

void Image::free() {
	if (thumbnail) {
		g_object_unref (thumbnail);
	}
}
