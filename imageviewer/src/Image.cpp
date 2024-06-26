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

Image::Image(std::string const &path, int id) {
	m_loadid = id;
	setPathClear(path);
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
	m_status = o.m_status;
	int i;
	for (i = 0; i < LIST_IMAGE_STEPS; i++) {
		m_thumbnail[i] = o.m_thumbnail[i].load();
		o.m_thumbnail[i] = nullptr;
	}
}

void Image::free() {
	int i;
	m_status = LOAD_STATUS::NOT_LOADED;
	for (i = 0; i < LIST_IMAGE_STEPS; i++) {
		if (m_thumbnail[i]) {
			g_object_unref(m_thumbnail[i]);
		}
	}
}

void Image::setPathClear(std::string const &path) {
	m_path = path;
	m_status = LOAD_STATUS::NOT_LOADED;
	for (auto &a : m_thumbnail) {
		a = nullptr;
	}
	m_size = getFileSize(m_path);
}

