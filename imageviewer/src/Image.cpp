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
	m_path = p;
	m_loadid = id;
	m_thumbnail[0] = nullptr;
	m_size = getFileSize(m_path);
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
	m_path = o.m_path;
	m_size = o.m_size;
	m_loadid = o.m_loadid;
	m_thumbnail[0] = o.m_thumbnail[0].load();
	o.m_thumbnail[0] = nullptr;
}

void Image::free() {
	if (m_thumbnail[0]) {
		g_object_unref(m_thumbnail[0]);
	}
}
