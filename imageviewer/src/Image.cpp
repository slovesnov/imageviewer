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
	int i;
	for (i = 0; i < LIST_IMAGE_STEPS; i++) {
		m_thumbnail[i] = nullptr;
	}
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
	int i;
	for (i = 0; i < LIST_IMAGE_STEPS; i++) {
		m_thumbnail[i] = o.m_thumbnail[i].load();
		o.m_thumbnail[i] = nullptr;
	}
}

void Image::free() {
	int i;
	for (i = 0; i < LIST_IMAGE_STEPS; i++) {
		if (m_thumbnail[i]) {
			g_object_unref(m_thumbnail[i]);
		}
	}
}
