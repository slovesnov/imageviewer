/*
 * Frame.cpp
 *
 *  Created on: 03.06.2021
 *      Author: alexey slovesnov
 * copyright(c/c++): 2014-doomsday
 *           E-mail: slovesnov@yandex.ru
 *         homepage: slovesnov.users.sourceforge.net
 */

#include <cmath>
#include "Frame.h"
#include "help.h"
#include <glib/gstdio.h>//g_remove
Frame *frame;

/* START_MODE=-1 no initial mode set, otherwise START_MODE = initial mode
 * START_MODE=-1 is normal, other values is using for debugging
 */
#define START_MODE -1

#if START_MODE==0
const MODE INITIAL_MODE=MODE::NORMAL;
#elif START_MODE==1
const MODE INITIAL_MODE=MODE::FIT;
#elif START_MODE==2
const MODE INITIAL_MODE=MODE::LIST;
#endif

int Frame::m_oneInstance;
std::vector<int> Frame::m_optionsDefalutValue;

static gpointer thumbnail_thread(gpointer data) {
	frame->thumbnailThread(GP2INT(data));
	return NULL;
}

static gboolean key_press(GtkWidget *widget, GdkEventKey *event, int n) {
	//println("%x %x %s",event->keyval,event->hardware_keycode,event->string)
	return frame->keyPress(widget, event, n);
}

static gboolean draw_callback(GtkWidget *widget, cairo_t *cr, gpointer data) {
	frame->draw(cr, widget);
	return FALSE;
}

static void drag_and_drop_received(GtkWidget*, GdkDragContext *context, gint x,
		gint y, GtkSelectionData *data, guint ttype, guint time, gpointer) {
	gint l = gtk_selection_data_get_length(data);
	gint a = gtk_selection_data_get_format(data);
	if (l >= 0 && a == 8) {
		gchar **pp = gtk_selection_data_get_uris(data);
		frame->openUris(pp);
		g_strfreev(pp);
		gtk_drag_finish(context, true, false, time);
	}
}

static gboolean mouse_scroll(GtkWidget *widget, GdkEventScroll *event,
		gpointer) {
	//	printl(event->direction,event->type,event->delta_x,event->delta_y);
	frame->scrollEvent(event);
	return FALSE;
}

static gboolean button_press(GtkWidget *widget, GdkEventButton *event,
		gpointer) {
	//printl(event->x,event->y,event->x_root,event->y_root);
	frame->buttonPress(event);
	return TRUE;
}

static void button_clicked(GtkWidget *widget, gpointer data) {
	frame->buttonClicked(GP2INT(data));
}

static void options_button_clicked(GtkWidget *widget, LANGUAGE data) {
	frame->optionsButtonClicked(data);
}

static void open_files(GtkWidget*, const char *data) {
	frame->load(data);
}

static gboolean set_show_thumbnail_thread(gpointer data) {
	frame->setShowThumbnail(GP2INT(data));
	return G_SOURCE_REMOVE;
}

static gboolean label_clicked(GtkWidget *label, const gchar *uri, gpointer) {
	openURL(uri);
	return TRUE;
}

static void directory_changed(GFileMonitor *monitor, GFile *file,
		GFile *other_file, GFileMonitorEvent event_type, gpointer user_data) {
	frame->addTimerEvent(TIMER::DIRECTORY);
}

static gboolean timer_changed(TIMER t) {
	frame->timerEventOccurred(t);
	return G_SOURCE_REMOVE;
}

static void entry_insert(GtkWidget *entry, gchar *new_text,
		gint new_text_length, gpointer position, int n) {
	frame->entryChanged(entry, n);
}

static void entry_delete(GtkWidget *entry, gint start_pos, gint end_pos,
		int n) {
	frame->entryChanged(entry, n);
}

static gboolean entry_focus_in(GtkWidget *w, GdkEventFocus event, int n) {
	frame->focusIn(w, n);
	return true;
}

static gboolean entry_focus_out(GtkWidget *w, GdkEventFocus event, int n) {
	frame->focusOut(w, n);
	return true;
}

Frame::Frame(GtkApplication *application, std::string path) {
	frame = this;

	int i, j, k;
	ENUM_CONFIG_TAGS ct;
	VString v;
	m_loadid = -1;
	for (auto &e : m_timer) {
		e = 0;
	}
	m_zoom = 1;
	m_lastNonListMode = MODE::ANY;
	m_proceedEvents = true;
	m_monitor = nullptr;
	m_saveRename = 0;
	m_lastManualOperationTime = 0;
	m_optionsPointer =
			{ &m_languageIndex, &m_warnBeforeDelete, &m_deleteOption,
					&m_warnBeforeSave, &m_showPopup, &m_oneInstance,
					&m_rememberLastOpenDirectory, &m_showToolbarFullscreen,
					&m_recursive };
	m_optionsDefalutValue = { 0, //m_languageIndex
			1, //m_warnBeforeDelete
			0, //m_deleteOption
			1, //m_warnBeforeSave
			1, //m_showPopup
			1, //m_oneInstance
			1, //m_rememberLastOpenDirectory
			0, //m_showToolbarFullscreen
			0, //m_recursive
			};
	assert(std::size(m_optionsPointer)==std::size(m_optionsDefalutValue));

	setlocale(LC_NUMERIC, "C"); //dot interpret as decimal separator for format(... , scale)
	m_pThread.resize(getNumberOfCores());
	for (auto &p : m_pThread) {
		p = nullptr;
	}
	g_mutex_init(&m_mutex);

	m_lastWidth = m_lastHeight = m_posh = m_posv = 0;
	m_filenameFontHeight = 0;

	//begin readConfig
	m_ascendingOrder = true;
	m_mode = MODE::NORMAL;
	m_listIconIndex = 0;
	resetOptions();

	MapStringString m;
	MapStringString::iterator it;
	if (loadConfig(m)) {
		i = j = 0;
		for (auto t : CONFIG_TAGS) {
			if ((it = m.find(t)) != m.end()) {
				ct = ENUM_CONFIG_TAGS(i);
				parseString(it->second, j);
				if (ct == ENUM_CONFIG_TAGS::MODE && j >= 0 && j < SIZEI(TMODE)) {
					m_mode = MODE(j);
				} else if (ct == ENUM_CONFIG_TAGS::LAST_OPEN_DIRECTORY) {
					m_dir = it->second;
				} else if (ct == ENUM_CONFIG_TAGS::ZOOM_FACTOR) {
					if (!parseString(it->second, m_zoomFactor) || m_zoomFactor <= MIN_ZOOM_FACTOR_BOUND
							|| m_zoomFactor > MAX_ZOOM_FACTOR) {
						m_zoomFactor = DEFAULT_ZOOM_FACTOR;
					}
				} else if (ct == ENUM_CONFIG_TAGS::KEYS) {
					v = split(it->second);
					if (v.size() == SIZE(m_key)) {
						k = -1;
						for (auto e : v) {
							k++;
							parseString(e, m_key[k]);
						}
					}
				}
#define M(id,max,variable) else if (ct == id){if( j >= 0 && j < max) {variable = j ;}}
				M(ENUM_CONFIG_TAGS::ORDER,2,m_ascendingOrder)
				M(ENUM_CONFIG_TAGS::LIST_ICON_INDEX,LIST_IMAGE_STEPS,m_listIconIndex)
				M(ENUM_CONFIG_TAGS::LANGUAGE,SIZEI(LNG),m_languageIndex)
				M(ENUM_CONFIG_TAGS::ASK_BEFORE_DELETE,2,m_warnBeforeDelete)
				M(ENUM_CONFIG_TAGS::DELETE_OPTION,2,m_deleteOption)
				M(ENUM_CONFIG_TAGS::ASK_BEFORE_SAVE,2,m_warnBeforeSave)
				M(ENUM_CONFIG_TAGS::SHOW_POPUP,2,m_showPopup)
				M(ENUM_CONFIG_TAGS::ONE_APPLICATION_INSTANCE,2,m_oneInstance)
				M(ENUM_CONFIG_TAGS::REMEMBER_THE_LAST_OPEN_DIRECTORY,2,m_rememberLastOpenDirectory)
				M(ENUM_CONFIG_TAGS::SHOW_THE_TOOLBAR_IN_FULLSCREEN_MODE,2,m_showToolbarFullscreen)
				M(ENUM_CONFIG_TAGS::RECURSIVE,2,m_recursive)
				M(ENUM_CONFIG_TAGS::SAVE_RENAME, 2, m_saveRename)
#undef M
			}
			i++;
		}
	}
	//end readConfig
	loadLanguage();

	setIconHeightWidth(0);

	GSList *formats;
	GSList *elem;
	GdkPixbufFormat *pf;
	char *c, *n, **extension_list;
	formats = gdk_pixbuf_get_formats();
	for (elem = formats; elem; elem = elem->next) {
		pf = (GdkPixbufFormat*) elem->data;
		j = gdk_pixbuf_format_is_writable(pf);
		n = gdk_pixbuf_format_get_name(pf);	//name need to store cann't use file extension
		extension_list = gdk_pixbuf_format_get_extensions(pf);

		for (i = 0; (c = extension_list[i]) != 0; i++) {
			m_supported.push_back( { c, n, i, bool(j) });
		}
		g_strfreev(extension_list);

	}
	g_slist_free(formats);
	m_supported.push_back( { "webp", "webp", 0, false });
	//alphabet order
	std::sort(m_supported.begin(), m_supported.end());
	/*
	 //begin inno script generator do not remove
	 const char *p;
	 std::string s, s1;
	 const char gn[] = "g";
	 const char tn[] = "t";
	 for (i = 0; i < 2; i++) {
	 j = -1;
	 for (auto &a : m_supported) {
	 j++;
	 s1 = a.extension;
	 p = s1.c_str();
	 if (i) {
	 printf(
	 "Root: HKCR; Subkey: \".%s\"; ValueType: string; ValueName: \"\"; ValueData: \"{#APPNAME}\"; Flags: uninsdeletevalue; Tasks: %s\\%s%d\n",
	 p, gn, tn, j);

	 } else {
	 printf(
	 "Name: %s\\%s%d; Description: \"*.%s\"; Flags: unchecked\n",
	 gn, tn, j, p);
	 if (!s.empty()) {
	 s += " or ";
	 }
	 s += gn + ("\\" + (tn + std::to_string(j)));
	 }
	 }
	 if (!i) {
	 printf("\n[Registry]\n");
	 }
	 }
	 const char *A[] =
	 {
	 R"(Root: HKCR; Subkey: {#APPNAME}; ValueType: string; ValueName: ""; ValueData: {#APPNAME}; Flags: uninsdeletekey; Tasks:)",
	 R"(Root: HKCR; Subkey: "{#APPNAME}\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\bin\{#APPNAME}.exe,0"; Tasks:)",
	 R"(Root: HKCR; Subkey: "{#APPNAME}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\bin\{#APPNAME}.exe"" ""%1"""; Tasks:)" };
	 for (auto a : A) {
	 printf("%s%s\n", a, s.c_str());
	 }
	 */

	loadCSS();

	m_window = gtk_application_window_new(GTK_APPLICATION(application));
	m_area = gtk_drawing_area_new();
	m_toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_widget_set_halign(m_toolbar, GTK_ALIGN_CENTER);
	//prevents destroy after gtk_container_remove
	g_object_ref(m_toolbar);

	//before setAscendingOrder
	for (auto a : ADDITIONAL_IMAGES) {
		m_additionalImages.push_back(pixbuf(a));
	}

	i = 0;
	for (auto a : TOOLBAR_IMAGES) {
		auto b = m_button[i] = gtk_button_new();
		m_buttonPixbuf[i][1] = pixbuf(a);
		m_buttonPixbuf[i][0] = gdk_pixbuf_copy(m_buttonPixbuf[i][1]);
		gdk_pixbuf_saturate_and_pixelate(m_buttonPixbuf[i][1],
				m_buttonPixbuf[i][0], .3f, false);

		setButtonState(i, true);

		g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(button_clicked),
				GP(i));
		gtk_box_pack_start(GTK_BOX(m_toolbar), b, FALSE, FALSE, 0);
		if (ONE_OF(TOOLBAR_INDEX(i), TOOLBAR_BUTTON_WITH_MARGIN)) {
			gtk_widget_set_margin_start(b, TOOLBAR_BUTTON_MARGIN);
		}
		i++;
	}

	m_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start(GTK_BOX(m_box), m_area, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(m_box), m_toolbar);

	gtk_container_add(GTK_CONTAINER(m_window), m_box);

	//set min size
	gtk_widget_set_size_request(m_window, 800, 600);

	gtk_window_maximize(GTK_WINDOW(m_window));
	gtk_widget_show_all(m_window);

	gtk_widget_add_events(m_window,
			GDK_SCROLL_MASK | GDK_KEY_PRESS_MASK | GDK_BUTTON_PRESS_MASK);
	g_signal_connect(m_window, "scroll-event", G_CALLBACK(mouse_scroll), NULL);
	g_signal_connect(m_window, "key-press-event", G_CALLBACK (key_press),
			gpointer(-1));

	g_signal_connect(m_window, "button-press-event", G_CALLBACK(button_press),
			NULL);

	g_signal_connect(m_window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	setPopups();

	g_signal_new(OPEN_FILE_SIGNAL_NAME, G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST, 0,
	NULL, NULL, g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1,
	G_TYPE_POINTER);

	g_signal_connect(m_window, OPEN_FILE_SIGNAL_NAME, G_CALLBACK(open_files),
			NULL);

	g_signal_connect(G_OBJECT (m_area), "draw", G_CALLBACK (draw_callback),
			NULL);

	setDragDrop(m_window);

	if (path.empty() && m_rememberLastOpenDirectory) {
		path = m_dir;
	}

	if (path.empty()) {
		setNoImage();
	} else {
		load(path, true); //after area is created
		if (m_mode == MODE::LIST) { //after vp is loading in load() function
			setListTopLeftIndexStartValue();
		}
		setAscendingOrder(m_ascendingOrder); //after toolbar is created
	}

	setTitle();

	gtk_main();
}

Frame::~Frame() {
	g_object_unref(m_toolbar);
	std::string s;
	int i = -1;
	for (auto e : m_key) {
		i++;
		if (i) {
			s += ' ';
		}
		s += forma(e);
	}

	WRITE_CONFIG(CONFIG_TAGS, IMAGE_VIEWER_VERSION, (int ) m_mode,
			m_ascendingOrder, m_listIconIndex, m_languageIndex,
			m_warnBeforeDelete, m_deleteOption, m_warnBeforeSave, m_showPopup,
			m_oneInstance, m_rememberLastOpenDirectory, m_showToolbarFullscreen,
			m_dir, s, m_recursive, m_saveRename, m_zoomFactor);
	stopThreads();
	g_mutex_clear(&m_mutex);
}

void Frame::openUris(char **uris) {
	gchar *fn;
	fn = g_filename_from_uri(uris[0], NULL, NULL);
	load(fn);
	g_free(fn);
}

void Frame::setTitle() {
	std::string s, t;
	int i;
	if (noImage()) {
		t = getTitleVersion();
	} else {
		t = m_dir + SEPARATOR;
		const int sz = size();
		if (m_mode == MODE::LIST) {
			double ts = m_totalFileSize;
			/* format("%d-%d/%d" a-b/size
			 * a=m_listTopLeftIndex + 1
			 * if LIST_ASCENDING_ORDER=1 a<b & b-a+1=m_listxy => b=a-1+m_listxy=m_listTopLeftIndex + m_listxy
			 * if LIST_ASCENDING_ORDER=0 a>b & a-b+1=m_listxy => b=a+1-m_listxy=m_listTopLeftIndex +2- m_listxy
			 */
			t += format("%d-%d/%d", m_listTopLeftIndex + 1,
					m_ascendingOrder ?
							MIN(m_listTopLeftIndex + m_listxy, sz) :
							MAX(m_listTopLeftIndex + 2 - m_listxy, 1), sz)
					+ SEPARATOR + getLanguageString(LANGUAGE::TOTAL) + " "
					+ getSizeMbKbB(ts) + ", "
					+ getLanguageString(LANGUAGE::AVERAGE) + " "
					+ getSizeMbKbB(ts / sz);
		} else {
			auto &f = m_vp[m_pi];
			//printl(m_pi,f.m_path)
			const std::string n = getFileInfo(f.m_path, FILEINFO::NAME);
			t += n + SEPARATOR + getLanguageString(LANGUAGE::SIZE)
					+ format(" %dx%d ", m_pw, m_ph)
					+ getLanguageString(LANGUAGE::SCALED)
					+ format(" %dx%d ", int(m_pw * m_zoom), int(m_ph * m_zoom))
					+ getSizeMbKbB(f.m_size) + SEPARATOR
					+ getLanguageString(LANGUAGE::ZOOM) + " "
					+ std::to_string(int(m_zoom * 100)) + "%" + SEPARATOR
					+ format("%d/%d", m_pi + 1, size());
			//get date if it's possible

			//std::string n = "IMG_21000229_130015-20230530_130015.jpg";
			GMatchInfo *match_info;
			GRegex *regex = g_regex_new(
					"(\\d{4})(\\d{2})(\\d{2})_(\\d{2})(\\d{2})(\\d{2})",
					GRegexCompileFlags(G_REGEX_RAW | G_REGEX_CASELESS),
					GRegexMatchFlags(0), NULL);
			int d[6];
			const int DMAX[] =
					{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
			int k;

			g_regex_match(regex, n.c_str(), G_REGEX_MATCH_DEFAULT, &match_info);
			while (g_match_info_matches(match_info)) {
				for (i = 0; i < SIZEI(d); i++) {
					gchar *word = g_match_info_fetch(match_info, i + 1);
					d[i] = atoi(word);
					g_free(word);
				}
				k = d[0];
				i = d[1] == 2 && (k % 4 == 0 && (k % 100 != 0 || k % 400 == 0));
				if (d[0] >= 1900 && d[1] > 0 && d[1] < 13 && d[2] > 0
						&& d[2] <= DMAX[d[1] - 1] + i && d[3] < 24 && d[4] < 60
						&& d[5] < 60) {
					t += SEPARATOR + std::to_string(d[2])
							+ getLanguageString(LANGUAGE::JAN, d[1] - 1)
							+ std::to_string(d[0]);
					for (i = 0; i < 3; i++) {
						t += (i ? ':' : ' ') + format("%02d", d[i + 3]);
					}
					break;
				}
				g_match_info_next(match_info, NULL);
			}
			g_match_info_free(match_info);
			g_regex_unref(regex);
		}
	}

	gtk_window_set_title(GTK_WINDOW(m_window), t.c_str());
}

void Frame::load(const std::string &path, bool start) {
	std::string s;
	stopThreads(); //endThreads=0 after
	m_loadid++;
	m_vp.clear();
	bool d = isDir(path);
	/* by default m_pi=0 - first image in directory
	 * if path is not dir m_pi=0 first image it's ok
	 * if path is file with bad extension then m_pi=0 it's ok
	 */
	m_dir = d ? path : getFileInfo(path, FILEINFO::DIRECTORY);
//	printl(m_dir,p)
	m_totalFileSize = 0;

	addDirectory(m_dir);
	sortFiles();
	if (d) {
		m_pi = 0;
	} else {
		setPictureIndex(path);
	}
	addMonitor(m_dir);
	recountListParameters();

	if (m_vp.empty()) {
		setNoImage();
		return;
	}

	//if was no image, need to set m_toolbar buttons enabled
	for (int i = 0; i < TOOLBAR_INDEX_SIZE; i++) {
		setButtonState(i, i != int(m_mode) + int(TOOLBAR_INDEX::MODE_ZOOM_ANY));
	}

	startThreads();
	setListTopLeftIndexStartValue(); //have to reset

	if (start) {
#if	START_MODE==-1
		setMode(m_mode, true);
#else
		setMode(INITIAL_MODE,true);
#endif
	} else {
		if (!d && m_mode == MODE::LIST) {
			setMode(MODE::NORMAL);
		}
	}

	//need to redraw anyway even if no image
	loadImage();

}

void Frame::addDirectory(const std::string &dir) {
	GDir *di;
	const gchar *filename;
	std::string s;
	//from gtk documentation order of file is arbitrary. Actually it's name (or may be date) ascending order
	if ((di = g_dir_open(dir.c_str(), 0, 0))) {
		while ((filename = g_dir_read_name(di))) {
			s = dir + G_DIR_SEPARATOR + filename;
			if (isDir(s)) {
				if (m_recursive) {
					addDirectory(s);
				}
			} else if (isSupportedImage(s)) {
				m_vp.push_back(Image(s, m_loadid));
				m_totalFileSize += m_vp.back().m_size;
			}
		}
	}

}

void Frame::loadImage() {
	if (!noImage()) {
		GError *err = NULL;
		std::string path = m_vp[m_pi].m_path;
		auto ext = getFileInfo(path, FILEINFO::LOWER_EXTENSION);
		if (ext == "webp") {
			m_pix = loadWebp(path);
			if (!m_pix) {
				println("Error loading", path);
				setNoImage();
				return;
			}
		} else {
			m_pix = gdk_pixbuf_new_from_file(path.c_str(), &err);
			if (err) {
				println("Error : %s", err->message);
				g_error_free(err);
				setNoImage();
				return;
			}
		}

		m_pw = m_pix.width();
		m_ph = m_pix.height();
		setDefaultZoom();
		updateNavigationButtonsState();
	}

	drawImage();

}

void Frame::draw(cairo_t *cr, GtkWidget *widget) {
	int i, j, k, l, width, height, w, h;
	const int sz = size();

	width = gtk_widget_get_allocated_width(widget);
	height = gtk_widget_get_allocated_height(widget);

	const GdkRGBA WHITE_COLOR = { 1, 1, 1, 1 };
	//1920 959
	//printl(width,height)
	const GdkRGBA BACKGROUND_COLOR = { 31. / 255, 31. / 255, 31. / 255, 1 };
	gdk_cairo_set_source_rgba(cr, &BACKGROUND_COLOR);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_fill(cr);

	if (noImage()) {
		drawTextToCairo(cr, getLanguageString(LANGUAGE::NO_IMAGE_HELP), 24,
				filenameFontBold, 0, 0, width, height, true, 1, WHITE_COLOR,
				false);
		return;
	}

	bool windowSizeChanged = m_lastWidth != width || m_lastHeight != height; //size of m_window is changed
	if (windowSizeChanged) {
		m_lastWidth = width;
		m_lastHeight = height;
		recountListParameters();
//		printl(m_lastWidth,m_lastHeight)
		setDefaultZoom();
		setTitle();
		setCenterPos();
	}

	if (!m_filenameFontHeight) {
		m_filenameFontHeight = countFontMaxHeight("IMG_20211004_093339",
				filenameFontBold, cr);
	}

	if (m_mode == MODE::LIST) {
		for (k = m_listTopLeftIndex, l = 0;
				((m_ascendingOrder && k < sz) || (!m_ascendingOrder && k >= 0))
						&& l < m_listxy; k += m_ascendingOrder ? 1 : -1, l++) {
			i = l % m_listx * m_listIconWidth + m_listdx;
			j = l / m_listx * m_listIconHeight + m_listdy;
			auto &o = m_vp[k];
			bool drawFileName=false;
			if (o.m_status==LOAD_STATUS::LOADED_OK) {
				GdkPixbuf *p = o.m_thumbnail[m_listIconIndex];
				if (p) {
					w = gdk_pixbuf_get_width(p);
					h = gdk_pixbuf_get_height(p);
					copy(p, cr, i + (m_listIconWidth - w) / 2,
							j + (m_listIconHeight - h) / 2, w, h, 0, 0);
					drawFileName=true;
				}
			}
			else if(o.m_status==LOAD_STATUS::LOADED_ERROR){
				drawFileName=true;
			}
			if(drawFileName){
				drawTextToCairo(cr,
						getFileInfo(o.m_path, FILEINFO::SHORT_NAME),
						m_filenameFontHeight, filenameFontBold, i, j + 1,
						m_listIconWidth, m_listIconHeight, true, 2,
						WHITE_COLOR, true);

				//				drawTextToCairo(cr, forma(l),
				//						m_filenameFontHeight*8, filenameFontBold, i, j + 1,
				//						m_listIconWidth, m_listIconHeight, true, 2, WHITE_COLOR,
				//						true);
			}
		}

	} else {
		cairo_rectangle(cr, 0, 0, width, height);
		cairo_scale(cr, m_zoom, m_zoom);
		gdk_cairo_set_source_pixbuf(cr, m_pix, -m_posh, -m_posv);
		cairo_fill(cr);
	}

	/*
	 const int A=200;
	 const int B=1920/2-A/2;

	 const GdkRGBA RED_COLOR = { 1, 0, 0, 1 };
	 GdkRectangle r = { 0, 0, width, height };
	 gdk_cairo_get_clip_rectangle(cr, &r);
	 gdk_cairo_set_source_rgba(cr, &RED_COLOR);

	 cairo_arc(cr, A/2, A/2, 6, 0, 2*G_PI);
	 cairo_fill(cr);

	 cairo_arc(cr, B+A/2, A/2, 6, 0, 2*G_PI);
	 cairo_fill(cr);

	 cairo_translate(cr, r.x + r.width / 2, r.y + r.height / 2);
	 cairo_arc(cr, 0, 0, 6, 0, 2*G_PI);
	 cairo_fill(cr);

	 //	double ko = (double(r.height) / r.width);
	 //	cairo_scale(cr, 1, ko);
	 //	cairo_set_line_width(cr, 3);
	 //	cairo_move_to(cr, r.width / 2, 0); //angle 0
	 //	cairo_arc(cr, 0, 0, r.width / 2, 0, 2 * G_PI);
	 //	cairo_stroke_preserve(cr);
	 */
}

void Frame::drawImage() {
	setCenterPos();
//	printl(m_posh,m_posv)
//	m_posh = m_posv = 0;
	redraw();
}

gboolean Frame::keyPress(GtkWidget *w, GdkEventKey *event, int n) {
	//	from gtk documentation return value
	//	TRUE to stop other handlers from being invoked for the event. FALSE to propagate the event further.
	const char *p;
//	guint key = event->keyval;
//	guint16 hwkey = event->hardware_keycode;
	auto keys = n == -1 ? m_key : m_tmpkey;
	int i;
	const int sz = TOOLBAR_INDEX_SIZE * MAX_HOTKEYS;
	for (i = 0; i < sz; i++, keys++) {
		auto e = *keys;
		if (getKey(e, event) == e) {
			break;
		}
	}

	if (n == -1) {
		if (i != sz) {
			i /= MAX_HOTKEYS;
			buttonClicked(i);
			return true;
		} else {
			return false;
		}
	}

	m_modalDialogEntryOK = i == sz || n == i;
	if (m_modalDialogEntryOK) {
		m_tmpkey[n] = getKey(event->hardware_keycode, event);
		p = gdk_keyval_name(m_tmpkey[n]);
	} else {
		p = getLanguageStringC(LANGUAGE::KEY_ALREADY_IN_USE);
	}
	addRemoveClass(w, "cerror", !m_modalDialogEntryOK);
	gtk_entry_set_text(GTK_ENTRY(w), p);
	return true;
}

void Frame::setDragDrop(GtkWidget *widget) {
	gtk_drag_dest_set(widget, GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
	gtk_drag_dest_add_uri_targets(widget);
	g_signal_connect(widget, "drag-data-received",
			G_CALLBACK(drag_and_drop_received), 0);
}

void Frame::scrollEvent(GdkEventScroll *event) {
	//printl(event->delta_x,event->delta_y,event->time,event->type)
	gdouble dx, dy;
	dx = event->delta_x;
	dy = event->delta_y;
	if (event->state == GDK_CONTROL_MASK) {
		if (dy) {
			//dy=-1 or 1
			buttonClicked(ZOOM_INOUT[dy == 1]);
		}
	} else {
		if (m_mode == MODE::LIST) {
			scrollList(dy * LIST_MULTIPLIER);
		} else if (dx || dy) {
			setPosRedraw(dx * WHEEL_MULTIPLIER, dy * WHEEL_MULTIPLIER);
		}
	}
}

void Frame::setPosRedraw(double dx, double dy) {
	int width = gtk_widget_get_allocated_width(m_area);
	int height = gtk_widget_get_allocated_height(m_area);
	if (m_pw - width / m_zoom > 0) {
		m_posh += dx;
	}
	if (m_ph - height / m_zoom > 0) {
		m_posv += dy;
	}
	adjustPos();
	redraw(false);
}

bool Frame::noImage() {
	return m_vp.empty();
}

void Frame::setNoImage() {
	m_vp.clear();
	setTitle();
	for (int i = 0; i < TOOLBAR_INDEX_SIZE; i++) {
		TOOLBAR_INDEX t = TOOLBAR_INDEX(i);
		setButtonState(i,
				oneOf(t, TOOLBAR_INDEX::OPEN, TOOLBAR_INDEX::SETTINGS));
	}
}

std::vector<FileSupported>::const_iterator Frame::supportedIterator(
		std::string const &p, bool writableOnly) {
	/* cann't use getFileInfo(p, FILEINFO::LOWER_EXTENSION)
	 * because get invalid extension for "svg.gz"
	 */
	std::vector<FileSupported>::const_iterator it;
	int d;
	std::string s = p;
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
		return std::tolower(c);
	});

	for (it = m_supported.begin(); it != m_supported.end(); it++) {
		d = int(s.length()) - int(it->extension.length());
		if (d > 0 && s[d - 1] == '.' && s.substr(d) == it->extension) {
			return !writableOnly || it->writable ? it : m_supported.end();
		}
	}
	return it;
}

bool Frame::isSupportedImage(const std::string &p, bool writableOnly) {
	return supportedIterator(p, writableOnly) != m_supported.end();
}

void Frame::adjustPos() {
	int width = gtk_widget_get_allocated_width(m_area);
	int height = gtk_widget_get_allocated_height(m_area);

	if (m_pw - width / m_zoom <= 0) {
		m_posh = (m_pw - width / m_zoom) / 2;
	} else {
		adjust(m_posh, 0, m_pw - width / m_zoom);
	}
	if (m_ph - height / m_zoom <= 0) {
		m_posv = (m_ph - height / m_zoom) / 2;
	} else {
		adjust(m_posv, 0, m_ph - height / m_zoom);
	}
}

void Frame::openDirectory() {
	std::string s = filechooser(m_window, m_dir);
	if (!s.empty()) {
		load(s);
	}
}

void Frame::switchImage(int v, bool add) {
	if (noImage()) {
		return;
	}
	int i = m_pi;
	m_pi += add ? v : -v;
	adjust(m_pi, 0, size() - 1);

	/* if last image in dir was scrolled down and user click next image (right key)
	 * do not need to call loadImage() because it load the same image and scroll it to start
	 * but nothing should happens
	 */
	if (i != m_pi) {
		loadImage();
	}
}

void Frame::startThreads() {
	int i;
	for (i = getFirstListIndex(); m_vp[i].m_thumbnail[0] != nullptr;
			i += m_ascendingOrder ? 1 : -1) {
		if ((m_ascendingOrder && i >= size()) || (!m_ascendingOrder && i < 0)) {
			break;
		}
	}
	m_threadNumber = i;

	i = 0;
	for (auto &p : m_pThread) {
		p = g_thread_new("", thumbnail_thread, GP(i++));
	}
}

void Frame::thumbnailThread(int n) {
	int v, max = 0;
	Pixbuf p;

//#define SHOW_THREAD_TIME

#ifdef SHOW_THREAD_TIME
	clock_t start= clock();
	std::string s;
	bool stopped=false;
#endif

	if (m_ascendingOrder) {
		g_mutex_lock(&m_mutex);
		max = size();
		g_mutex_unlock(&m_mutex);
	}

	while ((m_ascendingOrder && (v = m_threadNumber++) < max)
			|| (!m_ascendingOrder && (v = m_threadNumber--) >= 0)) {

		if (g_atomic_int_get(&m_endThreads)) {
#ifdef SHOW_THREAD_TIME
			stopped = true;
#endif
			break;
		}
		auto &o = m_vp[v];
		//m_listIconIndex can changed during thread running so load every time
		int i, j = m_listIconIndex,k,h;
		std::string ext;

		if (o.m_status==LOAD_STATUS::NOT_LOADED) {
//			printl("proceed t"+std::to_string(n),v)
			//load pixbuf from path
			ext = getFileInfo(o.m_path, FILEINFO::LOWER_EXTENSION);
			if (ext == "webp") {
				p = loadWebp(o.m_path);
			} else {
				p = o.m_path;
			}
			o.m_status=p?LOAD_STATUS::LOADED_OK:LOAD_STATUS::LOADED_ERROR;
			if(p){
	//			set scaled images, current scale at first
				for(k=0;k<2;k++){
					for (i = 0; i < LIST_IMAGE_STEPS; i++) {
						if( (k==0 && i==j) || (k==1 && i!=j) ){
							h = LIST_IMAGE_HEIGHT[i];
							o.m_thumbnail[i] = scaleFit(p, getWidthForHeight(h), h);
							if(!k){
								gdk_threads_add_idle(set_show_thumbnail_thread, GP(v));
							}
						}
					}
				}
			}

		}
		//s+=" "+std::to_string(v);
	}

#ifdef SHOW_THREAD_TIME
	println("t%d%s %.2lf %d",n,stopped?" user stop":"",((double) (clock() - start)) / CLOCKS_PER_SEC,max)
#endif
}

void Frame::stopThreads() {
	g_atomic_int_set(&m_endThreads, 1);

	//	clock_t begin=clock();
	for (auto &p : m_pThread) {
		if (p) {		//1st time p==nullptr
			g_thread_join(p);
			p = nullptr;		//allow call stopThreads many times
		}
	}
	//	println("%.3lf",double(clock()-begin)/CLOCKS_PER_SEC);

	g_atomic_int_set(&m_endThreads, 0);
}

void Frame::buttonPress(GdkEventButton *event) {
	//middle button is three fingers tap (in windows10 settings)
	if (event->type == GDK_BUTTON_PRESS) {//ignore double clicks GDK_DOUBLE_BUTTON_PRESS
		const bool left = event->button == 1;
		if (left || event->button == 2) { //left/middle mouse button
			if (m_mode == MODE::LIST) {
				if (left) { // left button check click on image
					int cx = event->x;
					int cy = event->y;
					if (cx >= m_listdx && cx < m_lastWidth - m_listdx
							&& cy >= m_listdy && cy < m_lastHeight - m_listdy) {
						m_pi = ((cx - m_listdx) / m_listIconWidth
								+ (cy - m_listdy) / m_listIconHeight * m_listx)
								* (m_ascendingOrder ? 1 : -1)
								+ m_listTopLeftIndex;
						setMode(m_lastNonListMode);
						loadImage();
					}
				}
			}
		} else if (event->button == 3) { //on right mouse open file/directory
			buttonClicked(TOOLBAR_INDEX::OPEN);
		}
	}
}

void Frame::setShowThumbnail(int i) {
	auto &o = m_vp[i];
	if (o.m_loadid != m_loadid) {
		return;
	}
	if (m_mode == MODE::LIST
			&& ((m_ascendingOrder && i >= m_listTopLeftIndex
					&& i < m_listTopLeftIndex + m_listxy)
					|| (!m_ascendingOrder && i <= m_listTopLeftIndex && i >= 0))) {
		redraw(false);
	}
}

void Frame::setListTopLeftIndexStartValue() {
	m_listTopLeftIndex = getFirstListIndex();
	listTopLeftIndexChanged();
}

void Frame::scrollList(int v) {
	if (m_mode == MODE::LIST) {
		int i, j, min, max;
		if (m_listxy >= size()) {
			//min=max=0;
			return;
		}
		getListMinMaxIndex(min, max);
		if (v == GOTO_BEGIN) {
			i = m_ascendingOrder ? min : max;
		} else if (v == GOTO_END) {
			i = m_ascendingOrder ? max : min;
		} else {
			i = m_listTopLeftIndex + v * (m_ascendingOrder ? 1 : -1) * m_listx;
		}

		j = m_listTopLeftIndex;
		m_listTopLeftIndex = i;
		adjust(m_listTopLeftIndex, min, max);
		if (j != m_listTopLeftIndex) {
			listTopLeftIndexChanged();
		}
	}
}

void Frame::buttonClicked(TOOLBAR_INDEX t) {
	int i;
	std::string s;

	//not proceed if button is disabled
	if (!gtk_widget_get_sensitive(m_button[int(t)])) {
		return;
	}

	if (t == TOOLBAR_INDEX::OPEN) {
		openDirectory();
		return;
	} else if (t == TOOLBAR_INDEX::SETTINGS) {
		showSettings();
		return;
	} else if (t == TOOLBAR_INDEX::FULLSCREEN) {
		if (isFullScreen()) {
			if (!m_showToolbarFullscreen) {
				gtk_container_add(GTK_CONTAINER(m_box), m_toolbar);
			}
			gtk_window_unfullscreen(GTK_WINDOW(m_window));
		} else {
			if (!m_showToolbarFullscreen) {
				gtk_container_remove(GTK_CONTAINER(m_box), m_toolbar);
			}
			gtk_window_fullscreen(GTK_WINDOW(m_window));
		}
		return;
	}

	if (noImage()) {
		return;
	}

	i = INDEX_OF(t, ZOOM_INOUT);
	if (i != -1) {
		if (m_mode == MODE::LIST) {
			if ((m_listIconIndex > 0 && t == TOOLBAR_INDEX::ZOOM_OUT)
					|| (m_listIconIndex < LIST_IMAGE_STEPS - 1
							&& t == TOOLBAR_INDEX::ZOOM_IN)) {
				setIconHeightWidth(t == TOOLBAR_INDEX::ZOOM_IN ? 1 : -1);
				recountListParameters();
				m_filenameFontHeight = 0; //to recount font
				redraw(); //with title m_listxy changed
			}
		} else {
			const double k =
					t == TOOLBAR_INDEX::ZOOM_IN ?
							m_zoomFactor : 1 / m_zoomFactor;
			if (m_zoom * k * m_ph >= MIN_SCALED_IMAGE_HEIGHT) {
				m_zoom *= k;
				//this code leave center point in center after zoom
				int width = gtk_widget_get_allocated_width(m_area);
				int height = gtk_widget_get_allocated_height(m_area);
				m_posh += width / m_zoom / 2 * (k - 1);
				m_posv += height / m_zoom / 2 * (k - 1);
				adjustPos();
				updateModesButtonState();
				redraw();
			}
		}
		updateZoomButtonsState();
		return;
	}

	i = INDEX_OF(t, TMODE);
	if (i != -1) {
		MODE m = MODE(i);
		//can be m_mode=zoom_fit after rotate or zoom then m_mode the same but need rescale image
		setMode(m);
		if (m_mode == MODE::LIST) {
			redraw();
		} else {
			setDefaultZoom();
			drawImage();
		}
		updateModesButtonState();
		return;
	}

	i = INDEX_OF(t, NAVIGATION);
	if (i != -1) {
		if (m_mode == MODE::LIST) {
			int a[] = { GOTO_BEGIN, -m_listy, -1, 1, m_listy, GOTO_END };
			scrollList(a[i]);
		} else {
			if (t == TOOLBAR_INDEX::HOME || t == TOOLBAR_INDEX::END) {
				m_pi = ((t == TOOLBAR_INDEX::HOME) == m_ascendingOrder) ?
						0 : size() - 1;
				loadImage();
			} else {
				switchImage(
						t == TOOLBAR_INDEX::PAGE_UP
								|| t == TOOLBAR_INDEX::PAGE_DOWN ?
								sqrt(size()) : 1,
						oneOf(t, TOOLBAR_INDEX::NEXT, TOOLBAR_INDEX::PAGE_DOWN)
								== m_ascendingOrder);
			}

		}
		return;
	}

	if (t == TOOLBAR_INDEX::REORDER_FILE) {
		setAscendingOrder(!m_ascendingOrder);
		m_listTopLeftIndex = getFirstListIndex();
		if (m_mode == MODE::LIST) {
			redraw();
		}
		return;
	}

	if (t == TOOLBAR_INDEX::DELETE_FILE) { //m_mode==MODE::LIST can be called from keyboard
		if (m_mode != MODE::LIST
				&& (!m_warnBeforeDelete
						|| showDeleteDialog() == GTK_RESPONSE_YES)) {

			m_lastManualOperationTime = clock();
			//not need full reload

			//before g_remove change totalFileSize
			auto &a = m_vp[m_pi];
			m_totalFileSize -= a.m_size;

			if (m_deleteOption) {
				g_remove(a.m_path.c_str());
			} else {
				deleteFileToRecycleBin(a.m_path);
			}
			m_vp.erase(m_vp.begin() + m_pi);
			int sz = size();
			if (sz == 0) {
				setNoImage();
			} else {
				adjust(m_listTopLeftIndex, 0, sz - 1);
				adjust(m_pi, 0, sz - 1);
				recountListParameters();
				loadImage();
			}
		}
		return;
	}

	if (t == TOOLBAR_INDEX::SAVE_FILE) { //m_mode==MODE::LIST can be called from keyboard
		if (m_mode != MODE::LIST
				&& (!m_warnBeforeSave || showSaveDialog() == GTK_RESPONSE_YES)) {
			bool b = false;
			s = m_warnBeforeSave ? m_modalDialogEntryText : m_vp[m_pi].m_path;
			int rename = m_warnBeforeSave ? m_saveRename : 0;
//			printl(s)
			auto it = supportedIterator(s, true);
			if (it == m_supported.end()) { //user click save non writable file m_warnBeforeSave=false
				if (showSaveDialog(true) == GTK_RESPONSE_YES) {
					it = supportedIterator(m_modalDialogEntryText, true);
					rename = m_saveRename;
					b = it != m_supported.end();
				}
			} else {
				b = true;
			}
			if (b) {
				GError *error = NULL;
				Pixbuf pixScaled = gdk_pixbuf_scale_simple(m_pix, m_zoom * m_pw,
						m_zoom * m_ph, GDK_INTERP_BILINEAR);
				/* !rename && m_vp[m_pi].m_path==m_modalDialogEntryText save but with same name it's just rename
				 * rename is used later so set rename variable
				 */
				bool same = m_vp[m_pi].m_path == m_modalDialogEntryText;

				if (same) {
					rename = true;
					m_modalDialogEntryText = utf8ToLocale(
							getWritableFilePath(
									getFileInfo(m_modalDialogEntryText,
											FILEINFO::NAME)));
				}
				if (gdk_pixbuf_save((GdkPixbuf*) pixScaled,
						m_modalDialogEntryText.c_str(), it->type.c_str(),
						&error,
						NULL)) {
					if (rename) {
						g_remove(m_vp[m_pi].m_path.c_str());
					} else {
						m_vp.push_back(Image("", m_loadid));
					}
					if (same) {
						g_rename(m_modalDialogEntryText.c_str(),
								m_vp[m_pi].m_path.c_str());
						m_modalDialogEntryText = m_vp[m_pi].m_path;
					}
					auto &e = m_vp[rename ? m_pi : m_vp.size() - 1];
					e.m_path = m_modalDialogEntryText;
					e.m_status = LOAD_STATUS::NOT_LOADED; //reload this image
					sortFiles();
					setPictureIndex(m_modalDialogEntryText);

					//printl(rename ? m_pi : m_vp.size() - 1,std::distance(m_vp.begin(), it))

					m_lastManualOperationTime = clock(); //disable monitoring call
					stopThreads();
					startThreads();

					setTitle();
				} else {
					showDialog(DIALOG::ERROR,
							std::to_string(error->code) + " " + error->message);
					g_error_free(error);
				}
			}
		}
		return;
	}

	i = INDEX_OF(t, IMAGE_MODIFY);
	if (i != -1) {
		if (m_mode != MODE::LIST) {
			if (i >= 3) {
				flipPixbuf(m_pix, i == 3);
			} else {
				rotatePixbuf(m_pix, m_pw, m_ph, i + 1);
			}
			updateModesButtonState();
			drawImage();
		}
		return;
	}
}

void Frame::buttonClicked(int t) {
	buttonClicked(TOOLBAR_INDEX(t));
}

void Frame::showSettings() {
	int i, j, k, l, m;
	GtkWidget *grid, *w, *w1, *tab[3], *box;
	VString v;
	std::string s;
	char *markup;
	bool b, b1;

	tab[0] = grid = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(grid), 15);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 10);

	i = -1;
	for (auto e : OPTIONS) {
		i++;
		w = gtk_label_new(getLanguageStringC(e));
		gtk_widget_set_halign(w, GTK_ALIGN_START);
		gtk_grid_attach(GTK_GRID(grid), w, 0, i, 1, 1);

		if (e == LANGUAGE::LANGUAGE) {
			w = m_options[i] = createLanguageCombo();
			gtk_widget_set_halign(w, GTK_ALIGN_START);			//not stretch
		} else if (e == LANGUAGE::REMOVE_FILE_OPTION) {
			for (auto e : { LANGUAGE::REMOVE_FILES_TO_RECYCLE_BIN,
					LANGUAGE::REMOVE_FILES_PERMANENTLY }) {
				v.push_back(getLanguageString(e));
			}
			w = m_options[i] = createTextCombo(v, 0);
			gtk_widget_set_halign(w, GTK_ALIGN_START);			//not stretch
		} else if (e == LANGUAGE::ZOOM_FACTOR) {
			m_modalDialogFactor = m_zoomFactor;
			w = m_options[i] = gtk_entry_new();
			addInsertDeleteEvents(w, ZOOM_FACTOR_ID);
			addFocusEvents(w, ZOOM_FACTOR_ID);
			w = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
			gtk_container_add(GTK_CONTAINER(w), gtk_label_new("k="));
			gtk_container_add(GTK_CONTAINER(w), m_options[i]);
			w1 = gtk_label_new("");
			s = " " + forma(MIN_ZOOM_FACTOR_BOUND) + " &lt; k &#8804; "
					+ forma(MAX_ZOOM_FACTOR);
			gtk_label_set_markup(GTK_LABEL(w1), s.c_str());
			gtk_container_add(GTK_CONTAINER(w), w1);
		} else {
			w = m_options[i] = gtk_check_button_new();
			if (e == LANGUAGE::ONE_APPLICATION_INSTANCE) {
				w = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
				gtk_container_add(GTK_CONTAINER(w), m_options[i]);
				gtk_container_add(GTK_CONTAINER(w),
						gtk_label_new(
								getLanguageStringC(
										LANGUAGE::REQUIRES_RESTARTING)));
			}
		}
		gtk_grid_attach(GTK_GRID(grid), w, 1, i, 1, 1);
	}

	tab[1] = grid = gtk_grid_new();
	std::copy(std::begin(m_key), std::end(m_key), std::begin(m_tmpkey));
	j = k = 0;
	for (i = 0; i < TOOLBAR_INDEX_SIZE; i++) {
		w = gtk_button_new();
		gtk_button_set_image(GTK_BUTTON(w),
				gtk_image_new_from_pixbuf(m_buttonPixbuf[i][1]));
		if (ONE_OF(TOOLBAR_INDEX(i), TOOLBAR_BUTTON_WITH_MARGIN)
				&& TOOLBAR_INDEX(i) != TOOLBAR_BUTTON_WITH_MARGIN[0]) {
			k = 0;
			j += 2;
		}
		if (j) {
			gtk_widget_set_margin_start(w, 6);
		}
		gtk_grid_attach(GTK_GRID(grid), w, j, k, 1, 1);

		box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		for (l = 0; l < MAX_HOTKEYS; l++) {
			w = gtk_entry_new();
			addClass(w, "key");

			m = i * MAX_HOTKEYS + l;
			gtk_widget_add_events(w, GDK_KEY_PRESS_MASK);
			g_signal_connect(G_OBJECT (w), "key_press_event",
					G_CALLBACK (key_press), GP(m));
			addFocusEvents(w, m);

			if (m_key[m] != INVALID_KEY) {
				gtk_entry_set_text(GTK_ENTRY(w), gdk_keyval_name(m_key[m]));
			}
			gtk_entry_set_width_chars(GTK_ENTRY(w), 22);
			gtk_entry_set_placeholder_text(GTK_ENTRY(w),
					getLanguageStringC(LANGUAGE::CLICK_TO_SET_THE_KEY));
			gtk_container_add(GTK_CONTAINER(box), w);
		}
		gtk_grid_attach(GTK_GRID(grid), box, j + 1, k, 1, 1);
		k++;
	}
	s = getLanguageStringMultiline(LANGUAGE::HELP_TAB2);
	w = gtk_label_new(s.c_str());
	gtk_widget_set_halign(w, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(grid), w, 0, k,
	SIZE(TOOLBAR_BUTTON_WITH_MARGIN) * 2, 1);

	tab[2] = grid = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(grid), 15);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
	i = -1;
	for (auto e : { LANGUAGE::HOMEPAGE, LANGUAGE::AUTHOR, LANGUAGE::EMAIL,
			LANGUAGE::READABLE_FORMATS, LANGUAGE::READABLE_EXTENSIONS,
			LANGUAGE::WRITABLE_FORMATS, LANGUAGE::WRITABLE_EXTENSIONS }) {
		i++;
		w = gtk_label_new(getLanguageStringC(e));
		gtk_widget_set_halign(w, GTK_ALIGN_START);
		gtk_grid_attach(GTK_GRID(grid), w, 0, i, 1, 1);

		if (e == LANGUAGE::HOMEPAGE) {
			s = getLanguageString(e, 1);
			w = gtk_label_new(NULL);
			markup = g_markup_printf_escaped("<a href=\"%s,%s\">\%s,%s</a>",
					s.c_str(), LNG_LONG[m_languageIndex].c_str(), s.c_str(),
					LNG_LONG[m_languageIndex].c_str());
			gtk_label_set_markup(GTK_LABEL(w), markup);
			g_free(markup);
			g_signal_connect(w, "activate-link", G_CALLBACK(label_clicked),
					gpointer(s.c_str()));
		} else if (oneOf(e, LANGUAGE::AUTHOR, LANGUAGE::EMAIL)) {
			w = gtk_label_new(getLanguageStringC(e, 1));
		} else {
			b = oneOf(e, LANGUAGE::WRITABLE_FORMATS,
					LANGUAGE::WRITABLE_EXTENSIONS);
			b1 = oneOf(e, LANGUAGE::READABLE_FORMATS,
					LANGUAGE::WRITABLE_FORMATS);
			k = 1 + (e == LANGUAGE::READABLE_EXTENSIONS);
			w = gtk_label_new(getExtensionString(b, b1, k).c_str());
		}
		gtk_widget_set_halign(w, GTK_ALIGN_START);
		gtk_widget_set_valign(w, GTK_ALIGN_CENTER);
		gtk_grid_attach(GTK_GRID(grid), w, 1, i, 1, 1);
	}
	i++;
	s = getBuildVersionString(true) + ", file size "
	//+ getLanguageString(LANGUAGE::SIZE)+ " "
			+ toString(getApplicationFileSize(), ',');
	gtk_grid_attach(GTK_GRID(grid), gtk_label_new(s.c_str()), 0, i, 2, 1);

	m_notebook = gtk_notebook_new();
	i = 0;
	for (auto e : tab) {
		addClass(e, "bg");
//		gtk_widget_set_hexpand(e, 1);
		w = gtk_label_new(getLanguageStringC(LANGUAGE::OPTIONS, i));
		gtk_notebook_append_page(GTK_NOTEBOOK(m_notebook), e, w);
		i++;
	}

	showModalDialog(m_notebook, DIALOG::SETTINGS);
}

void Frame::showDialog(DIALOG d, std::string s) {
	auto l = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(l), s.c_str());
	showModalDialog(l, d);
}

gint Frame::showDeleteDialog() {
	auto w = gtk_label_new(
			getLanguageStringC(
					LANGUAGE::DO_YOU_REALLY_WANT_TO_DELETE_THE_IMAGE));
	return showModalDialog(w, DIALOG::DELETE);
}

gint Frame::showSaveDialog(bool error) {
	GtkWidget *w, *grid;
	std::string s, s1;
	int i, j;
	grid = gtk_grid_new();
	i = 0;
	if (error) {
		s = getLanguageStringMultiline(LANGUAGE::SAVE_ERROR_MESSAGE);
		w = gtk_label_new(s.c_str());
		gtk_grid_attach(GTK_GRID(grid), w, 0, i++, 2, 1);
	}
	w = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	VString v;
	v = split(
			getLanguageStringC(LANGUAGE::DO_YOU_REALLY_WANT_TO_SAVE_THE_IMAGE),
			"%");
	gtk_container_add(GTK_CONTAINER(w), gtk_label_new(v[0].c_str()));
	m_modalDialogCombo = createTextCombo( { getLanguageString(LANGUAGE::SAVE),
			getLanguageString(LANGUAGE::RENAME) }, m_saveRename);
	gtk_container_add(GTK_CONTAINER(w), m_modalDialogCombo);
	gtk_container_add(GTK_CONTAINER(w), gtk_label_new(v[1].c_str()));
	gtk_grid_attach(GTK_GRID(grid), w, 0, i++, 2, 1);

	m_modalDialogEntry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(m_modalDialogEntry),
			m_vp[m_pi].m_path.c_str());
	addInsertDeleteEvents(m_modalDialogEntry, 0);

	w = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_container_add(GTK_CONTAINER(w),
			gtk_label_new(getLanguageStringC(LANGUAGE::FILE)));
	gtk_box_pack_start(GTK_BOX(w), m_modalDialogEntry, TRUE, TRUE, 0);
	gtk_grid_attach(GTK_GRID(grid), w, 0, i++, 2, 1);

	for (j = 0; j < 2; j++) {
		s = getLanguageString(LANGUAGE::WRITABLE_FORMATS, j);
		gtk_grid_attach(GTK_GRID(grid), gtk_label_new(s.c_str()), 0, i, 1, 1);

		s = getExtensionString(true, !j);
		w = gtk_entry_new();
		gtk_entry_set_text(GTK_ENTRY(w), s.c_str());
		gtk_editable_set_editable(GTK_EDITABLE(w), 0);
		gtk_widget_set_size_request(w, 275, -1);
		gtk_grid_attach(GTK_GRID(grid), w, 1, i, 1, 1);
		i++;
	}
	return showModalDialog(grid, DIALOG::SAVE);
}

gint Frame::showModalDialog(GtkWidget *w, DIALOG o) {
	GtkWidget *b, *b1, *b2;
	m_modalDialogIndex = o;
	auto d = m_modal = gtk_dialog_new();
	addClass(d, o == DIALOG::SETTINGS ? "bf" : "bg");
	gtk_window_set_modal(GTK_WINDOW(d), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(d), GTK_WINDOW(m_window));

	bool sd = oneOf(o, DIALOG::DELETE, DIALOG::SAVE);
	auto s =
			o == DIALOG::ERROR || sd ?
					getLanguageString(
							sd ? LANGUAGE::QUESTION : LANGUAGE::ERROR) :
					getTitleVersion();
	gtk_window_set_title(GTK_WINDOW(d), s.c_str());
	gtk_window_set_resizable(GTK_WINDOW(d), 0);

	if (oneOf(o, DIALOG::HELP, DIALOG::ERROR)) {
		b = w;
	} else {
		b = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
		gtk_container_add(GTK_CONTAINER(b), w);
		b1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
		std::vector<LANGUAGE> v;
		if (sd) {
			v = { LANGUAGE::YES, LANGUAGE::NO };
		} else {
			v = { LANGUAGE::OK, LANGUAGE::RESET, LANGUAGE::CANCEL };
		}
		for (auto e : v) {
			b2 = gtk_button_new_with_label(getLanguageString(e).c_str());
			if (oneOf(e, LANGUAGE::YES, LANGUAGE::OK)) {
				m_showModalDialogButtonOK = b2;
			}
			g_signal_connect(b2, "clicked", G_CALLBACK(options_button_clicked),
					GP(e));
			gtk_container_add(GTK_CONTAINER(b1), b2);
		}

		gtk_container_add(GTK_CONTAINER(b), b1);
		if (o == DIALOG::SETTINGS) {
			updateOptions();
		}
	}

	auto ca = gtk_dialog_get_content_area(GTK_DIALOG(d));
	gtk_container_add(GTK_CONTAINER(ca), b);

	gtk_widget_show_all(d);
	if (o == DIALOG::SAVE) {
		//if file format isn't writable disable yes button
		entryChanged(m_modalDialogEntry, 0);
	}

	auto r = gtk_dialog_run(GTK_DIALOG(d));
	gtk_widget_destroy(d);
	return r;
}

void Frame::recountListParameters() {
	const int sz = size();
	m_listx = MAX(1, m_lastWidth / m_listIconWidth);
	m_listy = MAX(1, m_lastHeight / m_listIconHeight);
	//MIN(m_listx,sz) center horizontally if images less than m_listx
	m_listdx = (m_lastWidth - MIN(m_listx,sz) * m_listIconWidth) / 2;
	//MIN(m_listx,sz) center vertically if images less than m_listx*m_listy
	int i;
	i = sz / m_listx;
	if (sz % m_listx != 0) {
		i++;
	}
	m_listdy = (m_lastHeight - MIN(m_listy, i) * m_listIconHeight) / 2;
	m_listxy = m_listx * m_listy;
	updateNavigationButtonsState();
}

void Frame::updateNavigationButtonsState() {
	const int sz = size();
	if (m_mode == MODE::LIST) {
		if (sz <= m_listxy) {					//all images in screen
			setNavigationButtonsState(0, 0);
		} else {
			int min, max;
			getListMinMaxIndex(min, max);
			if (m_ascendingOrder) {
				setNavigationButtonsState(m_listTopLeftIndex > min,
						m_listTopLeftIndex < max);
			} else {
				setNavigationButtonsState(m_listTopLeftIndex < max,
						m_listTopLeftIndex > min);
			}
		}
	} else {
		if (m_ascendingOrder) {
			setNavigationButtonsState(m_pi != 0, m_pi != size() - 1);
		} else {
			setNavigationButtonsState(m_pi != size() - 1, m_pi != 0);
		}
	}
}

void Frame::updateZoomButtonsState() {
	if (m_mode == MODE::LIST) {
		setButtonState(TOOLBAR_INDEX::ZOOM_IN,
				m_listIconHeight < MAX_LIST_IMAGE_HEIGHT);
		setButtonState(TOOLBAR_INDEX::ZOOM_OUT,
				m_listIconHeight > MIN_LIST_IMAGE_HEIGHT);
	} else {
		setButtonState(TOOLBAR_INDEX::ZOOM_IN, true);
		setButtonState(TOOLBAR_INDEX::ZOOM_OUT,
				m_zoom * m_ph > MIN_SCALED_IMAGE_HEIGHT);
	}
}

void Frame::setButtonState(int i, bool enable) {
	setButtonImage(i, enable, m_buttonPixbuf[i][enable]);
}

void Frame::setButtonState(TOOLBAR_INDEX i, bool enable) {
	setButtonState(int(i), enable);
}

void Frame::setMode(MODE m, bool start) {
	if (m_mode != m || start) {
		m_mode = m;
		if (m != MODE::LIST) {
			m_lastNonListMode = m;
		}
		int i;
		for (i = 0; i < SIZEI(TMODE); i++) {
			setButtonState(i + int(TOOLBAR_INDEX::MODE_ZOOM_ANY), i != int(m));
		}

		for (auto e : IMAGE_MODIFY) {
			setButtonState(e, m_mode != MODE::LIST);
		}
		for (auto e : { TOOLBAR_INDEX::DELETE_FILE, TOOLBAR_INDEX::SAVE_FILE }) {
			setButtonState(e, m_mode != MODE::LIST);
		}
		updateNavigationButtonsState();
		updateZoomButtonsState();
		setPopups();
	}
}

int Frame::size() {
	return m_vp.size();
}

void Frame::listTopLeftIndexChanged() {
	updateNavigationButtonsState();
	redraw();
}

void Frame::setNavigationButtonsState(bool c1, bool c2) {
	int i = 0;
	for (auto a : NAVIGATION) {
		setButtonState(a, i < 3 ? c1 : c2);
		i++;
	}
}

void Frame::getListMinMaxIndex(int &min, int &max) {
	int j, sz = size();
	if (m_ascendingOrder) {
		j = sz;
		if (sz % m_listx != 0) {
			j += m_listx - sz % m_listx;
		}
		min = 0;
		max = j - m_listxy;
	} else {
		j = 0;
		if (sz % m_listx != 0) {
			j -= m_listx - sz % m_listx;
		}
		min = j + m_listxy - 1;
		max = sz - 1;
	}
}

int Frame::getFirstListIndex() {
	return m_ascendingOrder ? 0 : size() - 1;
}

void Frame::setButtonImage(int i, bool enable, GdkPixbuf *p) {
	auto b = m_button[i];
	gtk_widget_set_sensitive(b, enable);
	gtk_widget_set_has_tooltip(b, enable);
	gtk_button_set_image(GTK_BUTTON(b), gtk_image_new_from_pixbuf(p));
}

void Frame::redraw(bool withTitle) {
	if (withTitle) {
		setTitle();
	}
	gtk_widget_queue_draw(m_area);
	//gtk_widget_queue_draw_area(widget, x, y, width, height)
}

int Frame::countFontMaxHeight(const std::string &s, bool bold, cairo_t *cr) {
	int w, h;
	for (int i = 1;; i++) {
		getTextExtents(s, i, false, w, h, cr);
		if (w >= m_listIconWidth || h >= m_listIconHeight) {
			return i - 1;
		}
	}
	assert(0);
	return 16;
}

int Frame::getWidthForHeight(int height) {
	return 4 * height / 3;
}

void Frame::setIconHeightWidth(int delta) {
	m_listIconIndex += delta;
	m_listIconHeight = LIST_IMAGE_HEIGHT[m_listIconIndex];
	m_listIconWidth = getWidthForHeight(m_listIconHeight);
}

void Frame::loadLanguage() {
	std::string s;
	m_language.clear();
	std::ifstream f = openResourceFileAsStream(
			getShortLanguageString(m_languageIndex) + "/language.txt");
	while (std::getline(f, s)) {
		m_language.push_back(s);
	}
}

std::string Frame::getTitleVersion() {
	return getLanguageString(LANGUAGE::IMAGE_VIEWER) + " "
			+ getLanguageString(LANGUAGE::VERSION)
			+ format(" %.1lf", IMAGE_VIEWER_VERSION);
}

std::string const& Frame::getLanguageString(LANGUAGE l, int add) {
	return m_language[int(l) + add];
}

std::string Frame::getLanguageStringMultiline(LANGUAGE l) {
	return replaceAll(getLanguageString(l), "\\n", "\n");
}

const char* Frame::getLanguageStringC(LANGUAGE l, int add) {
	return m_language[int(l) + add].c_str();
}

GtkWidget* Frame::createTextCombo(VString const &v, int active) {
	GtkWidget *w = gtk_combo_box_text_new();
	for (auto &a : v) {
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(w), a.c_str());
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(w), active);
	return w;
}

void Frame::optionsButtonClicked(LANGUAGE l) {
	int i;

	if (oneOf(m_modalDialogIndex, DIALOG::DELETE, DIALOG::SAVE)) {
		if (m_modalDialogIndex == DIALOG::SAVE) {
			m_modalDialogEntryText = gtk_entry_get_text(
					GTK_ENTRY(m_modalDialogEntry));
			m_saveRename = gtk_combo_box_get_active(
					GTK_COMBO_BOX(m_modalDialogCombo));
		}
		gtk_dialog_response(GTK_DIALOG(m_modal),
				l == LANGUAGE::YES ? GTK_RESPONSE_YES : GTK_RESPONSE_NO);
		return;
	}

	if (m_modalDialogIndex == DIALOG::SETTINGS) {
		i = gtk_notebook_get_current_page(GTK_NOTEBOOK(m_notebook));
		auto oldLanguage = m_languageIndex;
		auto oldShowPopup = m_showPopup;
		auto oldFullscreen = m_showToolbarFullscreen;
		if (l == LANGUAGE::RESET) {
			resetOptions();
		} else if (l == LANGUAGE::OK) {
			if (i == 0) {
				i = 0;
				for (auto e : m_optionsPointer) {
					auto w = m_options[i++];
					*e = GTK_IS_COMBO_BOX(w) ?
							gtk_combo_box_get_active(GTK_COMBO_BOX(w)) :
							gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
				}
				m_zoomFactor = m_modalDialogFactor;
			} else if (i == 1) {
				std::copy(std::begin(m_tmpkey), std::end(m_tmpkey),
						std::begin(m_key));
			}
		}
		gtk_dialog_response(GTK_DIALOG(m_modal), GTK_RESPONSE_OK);
		if (m_languageIndex != oldLanguage) {
			loadLanguage();
			setTitle();				//update title after dialog is closed
		}
		if (m_languageIndex != oldLanguage || m_showPopup != oldShowPopup) {
			setPopups();
		}
		if (m_showToolbarFullscreen != oldFullscreen && isFullScreen()) {
			if (m_showToolbarFullscreen) {
				gtk_container_add(GTK_CONTAINER(m_box), m_toolbar);
			} else {
				gtk_container_remove(GTK_CONTAINER(m_box), m_toolbar);
			}
		}
		return;
	}

}

void Frame::resetOptions() {
	int i = 0;
	for (auto e : m_optionsPointer) {
		*e = m_optionsDefalutValue[i++];
	}
	m_zoomFactor = DEFAULT_ZOOM_FACTOR;
	i = 0;
	for (auto &e : KEY) {
		m_key[i++] = e;
	}
}

void Frame::updateOptions() {
	int i = 0;
	GtkWidget *w;
	for (auto e : m_optionsPointer) {
		w = m_options[i];
		if (GTK_IS_COMBO_BOX(w)) {
			gtk_combo_box_set_active(GTK_COMBO_BOX(w), *e);
		} else {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), *e);
		}
		i++;
	}
	w = m_options[i++];
	m_proceedEvents = false;
	gtk_entry_set_text(GTK_ENTRY(w), forma(m_zoomFactor).c_str());
	m_proceedEvents = true;
}

void Frame::setPopups() {
	int i = 0;
	for (auto e : m_button) {
		if (m_showPopup && gtk_widget_get_sensitive(e)) {
			gtk_widget_set_tooltip_text(e,
					getLanguageStringC(LANGUAGE::TOOLTIP1, i));
		} else {
			gtk_widget_set_has_tooltip(e, 0);
		}
		i++;
	}
}

std::string Frame::filechooser(GtkWidget *parent, const std::string &dir) {
	std::string s;
	bool onlyFolder = false;
	const gint MY_SELECTED = 0;

	GtkWidget *dialog = gtk_file_chooser_dialog_new(
			getLanguageStringC(LANGUAGE::OPEN_FILE), GTK_WINDOW(parent),
			onlyFolder ?
					GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER :
					GTK_FILE_CHOOSER_ACTION_OPEN,
			getLanguageStringC(LANGUAGE::CANCEL), GTK_RESPONSE_CANCEL,
			getLanguageStringC(LANGUAGE::OPEN), GTK_RESPONSE_ACCEPT,
			NULL);

	if (!onlyFolder) {
		/* add the additional "Select" button
		 * "select" button allow select folder and file
		 * "open" button can open only files
		 */
		gtk_dialog_add_button(GTK_DIALOG(dialog),
				getLanguageStringC(LANGUAGE::SELECT), MY_SELECTED);
	}
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
			(dir + "/..").c_str());

	gint response = gtk_dialog_run(GTK_DIALOG(dialog));
	auto v = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
	if (response == GTK_RESPONSE_ACCEPT
			|| (!onlyFolder && response == MY_SELECTED && v)) {
		s = v;
	}

	gtk_widget_destroy(dialog);

	return s;
}

std::string Frame::getSizeMbKbB(double v) {
	std::string s;
	std::size_t i, j;
	for (j = 0; j < 2 && v >= 1024; j++) {
		v /= 1024;
	}
	s = format("%.2lf", v);
	for (i = s.size() - 1; i > 0 && strchr("0.", s[i]); i--)
		;
	return s.substr(0, i + 1) + getLanguageString(LANGUAGE::BYTES, j);
}

void Frame::addMonitor(std::string &path) {
	GFile *directory = g_file_new_for_path(path.c_str());
	if (m_monitor) {
		//remove previous monitor directory
		g_file_monitor_cancel(G_FILE_MONITOR(m_monitor));
	}
	m_monitor = g_file_monitor_directory(directory, G_FILE_MONITOR_SEND_MOVED,
	NULL, NULL);
	if (m_monitor) {
		g_signal_connect_object(G_OBJECT(m_monitor), "changed",
				G_CALLBACK(directory_changed),
				NULL, G_CONNECT_AFTER);
	}
}

void Frame::timerEventOccurred(TIMER t) {
	if (t == TIMER::DIRECTORY) {
//		printl(timeElapse(m_lastManualOperationTime),MIN_MANULAL_OPERATION_ELAPSE,timeElapse(m_lastManualOperationTime)>=MIN_MANULAL_OPERATION_ELAPSE?"load":"not load")
		if (timeElapse(m_lastManualOperationTime)
				>= MIN_MANULAL_OPERATION_ELAPSE) {
			load(m_dir);
		}
	}
	m_timer[int(t)] = 0;
}

void Frame::addTimerEvent(TIMER t) {
	//if delete several files then got many delete events, the same if copy file to folder so just proceed last signal
	int i = int(t);
	stopTimer(m_timer[i]);
//	printl("stopped");
	m_timer[i] = g_timeout_add(EVENT_TIME[i], G_SOURCE_FUNC(timer_changed),
			GP(i));
}

void Frame::stopTimer(guint &t) {
	if (t) {
		g_source_remove(t);
		t = 0;
	}
}
std::string Frame::getExtensionString(bool writableOnly, bool onlyIndex0,
		int rows) {
	int i = -1;
	std::string s;
	int sz = m_supported.size();
	for (auto &e : m_supported) {
		i++;
		if ((onlyIndex0 && e.index != 0) || (writableOnly && !e.writable)) {
			continue;
		}
		if (!s.empty()) {
			s += i == sz / rows ? '\n' : ' ';
		}
		s += e.extension;
	}
	return s;
}

bool Frame::isOneInstanceOnly() {
	MapStringString m;
	MapStringString::iterator it;
	int j;
	m_oneInstance = DEFAULT_ONE_INSTANCE;
	if (loadConfig(m)) {
		it = m.find(
				CONFIG_TAGS[int(ENUM_CONFIG_TAGS::ONE_APPLICATION_INSTANCE)]);
		if (it != m.end() && parseString(it->second, j) && j >= 0 && j < 2) {
			m_oneInstance = j;
		}
	}
	return m_oneInstance;
}

double Frame::getDefaultZoomFit() {
	return MIN(m_lastWidth / double(m_pw), m_lastHeight / double(m_ph));
}

void Frame::setDefaultZoom() {
	if (m_mode == MODE::NORMAL) {
		m_zoom = 1;
	} else if (m_mode == MODE::FIT) {
		m_zoom = getDefaultZoomFit();
	}
}

void Frame::setCenterPos() {
	int width = gtk_widget_get_allocated_width(m_area);
	int height = gtk_widget_get_allocated_height(m_area);
	m_posh = (m_pw - width / m_zoom) / 2;
	m_posv = (m_ph - height / m_zoom) / 2;
}

bool Frame::isFullScreen() {
	GdkWindow *gdk_window = gtk_widget_get_window(m_window);
	GdkWindowState state = gdk_window_get_state(gdk_window);
	return state & GDK_WINDOW_STATE_FULLSCREEN;
}

GtkWidget* Frame::createLanguageCombo() {
	GdkPixbuf *pb;
	GtkTreeIter iter;
	GtkTreeStore *store;
	guint i;
	store = gtk_tree_store_new(2, GDK_TYPE_PIXBUF, G_TYPE_STRING);
	for (i = 0; i < SIZE(LNG); i++) {
		pb = pixbuf((LNG[i] + ".gif").c_str());
		gtk_tree_store_append(store, &iter, NULL);
		gtk_tree_store_set(store, &iter, COL::PIXBUF, pb, COL::TEXT,
				(" " + LNG_LONG[i]).c_str(), -1);
		g_object_unref(pb);
	}

	auto w = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
	GtkCellRenderer *renderer;
	for (i = 0; i < 2; i++) {
		renderer =
				i ? gtk_cell_renderer_text_new() : gtk_cell_renderer_pixbuf_new();
		gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(w), renderer, FALSE);
		gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(w), renderer,
				i ? "text" : "pixbuf", i, NULL);
	}
	return w;
}

void Frame::setAscendingOrder(bool b) {
	m_ascendingOrder = b;
	setButtonImage(int(TOOLBAR_INDEX::REORDER_FILE), true,
			m_additionalImages[!m_ascendingOrder]);
}

void Frame::entryChanged(GtkWidget *w, int n) {
	if (!m_proceedEvents) {
		return;
	}
	auto s = gtk_entry_get_text(GTK_ENTRY(w));
	bool b;
	if (n == ZOOM_FACTOR_ID) {
		double d = INVALID_ZOOM_FACTOR;
		b = parseString(s, d);
		if (b && (d <= MIN_ZOOM_FACTOR_BOUND || d > MAX_ZOOM_FACTOR)) {
			d = INVALID_ZOOM_FACTOR;
			b = false;
		}
		gtk_widget_set_sensitive(m_showModalDialogButtonOK, b);
		addRemoveClass(w, "cerror", !b);
		m_modalDialogFactor = d;
//		printl(b,d)
	} else {
		b = isSupportedImage(s, true);
		gtk_widget_set_sensitive(m_showModalDialogButtonOK, b);
	}
}

void Frame::focusIn(GtkWidget *w, int n) {
	if (!m_proceedEvents) {
		return;
	}
	if (n == ZOOM_FACTOR_ID) {
		return;
	}

	m_modalDialogEntryOK = 0;//need gtk_entry_set_text(GTK_ENTRY(w),"") on kill focus
	m_tmpkey[n] = INVALID_KEY;				//clear hotkey
	gtk_entry_set_text(GTK_ENTRY(w),
			getLanguageStringC(LANGUAGE::PRESS_ANY_KEY));
}

void Frame::focusOut(GtkWidget *w, int n) {
	if (!m_proceedEvents) {
		return;
	}
	if (n == ZOOM_FACTOR_ID) {
		if (m_modalDialogFactor == INVALID_ZOOM_FACTOR) {
			removeClass(w, "cerror");
			m_modalDialogFactor = m_zoomFactor;
			gtk_entry_set_text(GTK_ENTRY(w), forma(m_zoomFactor).c_str());
		}
		return;
	}

	if (!m_modalDialogEntryOK) {
		gtk_entry_set_text(GTK_ENTRY(w), "");
		removeClass(w, "cerror");
	}
}

void Frame::addInsertDeleteEvents(GtkWidget *w, int n) {
	g_signal_connect_after(G_OBJECT(w), "insert-text", G_CALLBACK(entry_insert),
			GP(n));
	g_signal_connect_after(G_OBJECT(w), "delete-text", G_CALLBACK(entry_delete),
			GP(n));
}

void Frame::addFocusEvents(GtkWidget *w, int n) {
	g_signal_connect_after(G_OBJECT(w), "focus-in-event",
			G_CALLBACK(entry_focus_in), GP(n));
	g_signal_connect_after(G_OBJECT(w), "focus-out-event",
			G_CALLBACK(entry_focus_out), GP(n));
}

void Frame::sortFiles() {
	//g_strcmp0
	//return strcoll (utf8ToLocale(a.m_path).c_str() , utf8ToLocale(b.m_path).c_str())<0;
	std::sort(m_vp.begin(), m_vp.end(), [](const auto &a, const auto &b) {
		return g_utf8_collate(a.m_path.c_str(), b.m_path.c_str()) < 0;
	});
}

void Frame::setPictureIndex(const std::string &path) {
	auto it = std::find_if(m_vp.begin(), m_vp.end(), [&path](const auto &e) {
		return e.m_path == path;
	});
	m_pi = std::distance(m_vp.begin(), it);
}

void Frame::updateModesButtonState() {
	if (m_mode != MODE::LIST) {
		setButtonState(TOOLBAR_INDEX::MODE_ZOOM_100, m_zoom != 1);
//		println ("%dx%d %dx%d",m_lastWidth, m_lastHeight , m_pw ,m_ph );
//		println("%lf %lf z=%lf",m_zoom*m_pw,m_zoom*m_ph,m_zoom)
		setButtonState(TOOLBAR_INDEX::MODE_ZOOM_FIT,
				m_zoom != getDefaultZoomFit());
	}
}
