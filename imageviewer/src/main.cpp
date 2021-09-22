/*
 * main.cpp
 *
 *       Created on: 03.06.2021
 *           Author: aleksey slovesnov
 * Copyright(c/c++): 2014-doomsday
 *           E-mail: slovesnov@yandex.ru
 *         Homepage: slovesnov.users.sourceforge.net
 */

/* TODO
 *
 * ?remove push_back use set to decrease object creation (watcho on counters)
 * +setShowThumbnail ignore if from another dir (use id)
 * finish load after delete,stop threads before
 *
 * save/load mode on exit, order?
 * redraw only part in showThumbnail()
 *
 * scroll mouse list multiplier
 * ascending/descending button on delete (disable/enable problems)
 *
 *
 *
 * c:\downloads\fatcow-hosting-icons-3000\32x32\alitalk.png
 *
 */

#include "Frame.h"
#include "Image.h"
#include "help.h"

//signature https://developer.gnome.org/gio/stable/GApplication.html#GApplication-open
static void application_open(GtkApplication *application, GFile **files, gint n_files,
		const gchar *hint, gpointer data) {
	int i;
	char* c;
	std::string s;

	//no encode string here, tested on russian filenames using windows association
	for (i = 0; i < n_files; i++) {
		c = g_file_get_path(files[i]);
		s += c;
		g_free(c);
		break;
	}

	//list is always is NULL if application is created with G_APPLICATION_NON_UNIQUE flag
	GList* list = gtk_application_get_windows(application);
	if (list) {
		g_signal_emit_by_name(list->data, OPEN_FILE_SIGNAL_NAME, s.c_str());
		gtk_window_present(GTK_WINDOW(list->data));
	}
	else {
		Frame(application,s,(const char*)(data));
	}
}

//signature https://developer.gnome.org/gio/stable/GApplication.html#GApplication-activate
static void activate(GtkApplication *application, gpointer data) {
	application_open(application, NULL, 0, NULL, data);
}

int main(int argc, char *argv[]) {
	//Frame::setApplicationPath(argv[0]);
	const char appName[]="org.imageviewer";
	GApplicationFlags flags = GApplicationFlags(
			G_APPLICATION_HANDLES_OPEN
					| (ONE_INSTANCE ?
							G_APPLICATION_FLAGS_NONE : G_APPLICATION_NON_UNIQUE));
	GtkApplication *app = gtk_application_new(appName, flags);
	g_signal_connect(app, "activate", G_CALLBACK (activate), 0); //this function is called when application has no arguments
	g_signal_connect(app, "open", G_CALLBACK (application_open), GP(argv[0])); //this function is called when application has arguments
	g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);

#ifdef IMAGE_COUNTERS
	printl(Image::c1,Image::c2)
#endif

	return 0;
}
