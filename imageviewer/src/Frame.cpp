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

const int GOTO_BEGIN = INT_MIN;
const int GOTO_END = INT_MAX;
const int MIN_ICON_HEIGHT = 32;
const int MAX_ICON_HEIGHT = 200;

const char *ADDITIONAL_IMAGES[] = { "sort_ascending.png", "sort_descending.png",
		"arrow_in.png", "arrow_out.png" };

const char *TOOLBAR_IMAGES[] = { "magnifier_zoom_in.png",
		"magnifier_zoom_out.png", "application_view_columns.png",

		"arrow_rotate_anticlockwise.png", "arrow_refresh.png",
		"arrow_rotate_clockwise.png", "leftright.png", "updown.png",

		"control_start_blue.png", "control_rewind_blue.png", "previous.png",
		"control_play_blue.png", "control_fastforward_blue.png",
		"control_end_blue.png",

		"folder.png", "cross.png", ADDITIONAL_IMAGES[0],

		"fullscreen.png", "settings.png", "help.png" };

const TOOLBAR_INDEX IMAGE_MODIFY[] = { TOOLBAR_INDEX::ROTATE_CLOCKWISE,
		TOOLBAR_INDEX::ROTATE_180, TOOLBAR_INDEX::ROTATE_ANTICLOCKWISE,
		TOOLBAR_INDEX::FLIP_HORIZONTAL, TOOLBAR_INDEX::FLIP_VERTICAL };

const TOOLBAR_INDEX NAVIGATION[] = { TOOLBAR_INDEX::HOME,
		TOOLBAR_INDEX::PAGE_UP, TOOLBAR_INDEX::PREVIOUS, TOOLBAR_INDEX::NEXT,
		TOOLBAR_INDEX::PAGE_DOWN, TOOLBAR_INDEX::END };

const TOOLBAR_INDEX TMODE[] = { TOOLBAR_INDEX::MODE_NORMAL,
		TOOLBAR_INDEX::MODE_FIT, TOOLBAR_INDEX::MODE_LIST };

const TOOLBAR_INDEX TOOLBAR_BUTTON_WITH_MARGIN[] = {
		TOOLBAR_INDEX::ROTATE_ANTICLOCKWISE, TOOLBAR_INDEX::HOME,
		TOOLBAR_INDEX::OPEN };

const std::string CONFIG_TAGS[] = { "mode", "order", "list icon height",
		"language", "warning before delete", "delete option", "show popup",
		"one application instance" };
enum class ENUM_CONFIG_TAGS {
	CMODE,
	ORDER,
	LIST_ICON_HEIGHT,
	CLANGUAGE,
	ASK_BEFORE_DELETE,
	DELETE_OPTION,
	SHOW_POPUP,
	ONE_APPLICATION_INSTANCE
};
const std::string SEPARATOR = "       ";
static const int EVENT_TIME = 1000; //milliseconds, may be problems with small timer
int Frame::m_oneInstance;

static gpointer thumbnail_thread(gpointer data) {
	frame->thumbnailThread(GP2INT(data));
	return NULL;
}

static gboolean key_press(GtkWidget *widget, GdkEventKey *event,
		gpointer data) {
	//println("%x %x %s",event->keyval,event->hardware_keycode,event->string)
	return frame->keyPress(event);
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
	frame->scroll(event);
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

void directory_changed(GFileMonitor *monitor, GFile *file, GFile *other_file,
		GFileMonitorEvent event_type, gpointer user_data) {
	frame->addEvent();
}

static gboolean timer_animation_handler(gpointer data) {
	frame->directoryChanged();
	return G_SOURCE_REMOVE;
}

Frame::Frame(GtkApplication *application, std::string const path) {
	frame = this;

	int i, j;
	ENUM_CONFIG_TAGS ct;
	m_loadid = -1;
	m_timer = 0;

	setlocale(LC_NUMERIC, "C"); //dot interpret as decimal separator for format(... , scale)
	pThread.resize(getNumberOfCores());
	for (auto &p : pThread) {
		p = nullptr;
	}
	g_mutex_init(&m_mutex);

	m_lastWidth = m_lastHeight = posh = posv = 0;
	m_loadingFontHeight = 0;

	//begin readConfig
	m_ascendingOrder = true;
	m_lastNonListMode = mode = MODE::NORMAL;
	//drawing area height 959,so got 10 rows
	//4*95/3 = 126, 1920/126=15.23 so got 15 columns
	setIconHeightWidth(95);

	resetOptions();

	MapStringString m;
	MapStringString::iterator it;
	if (loadConfig(m)) {
		i = 0;
		for (auto t : CONFIG_TAGS) {
			if ((it = m.find(t)) != m.end()) {
				if (!parseString(it->second, j)) {
					printl("error")
					;
					break;
				}
				ct = ENUM_CONFIG_TAGS(i);
				if (ct == ENUM_CONFIG_TAGS::CMODE) {
					if (j < 0 || j > 2) {
						printl("error")
						;
						break;
					}
					m_lastNonListMode = mode = MODE(j);
				} else if (ct == ENUM_CONFIG_TAGS::ORDER) {
					if (j < 0 || j > 1) {
						printl("error")
						;
						break;
					}
					m_ascendingOrder = j == 1;
				} else if (ct
						== ENUM_CONFIG_TAGS::CLANGUAGE&& j>=0 && j<SIZEI(LNG)) {
					m_languageIndex = j;
				} else if (ct == ENUM_CONFIG_TAGS::ASK_BEFORE_DELETE && j >= 0
						&& j < 2) {
					m_warningBeforeDelete = j;
				} else if (ct == ENUM_CONFIG_TAGS::DELETE_OPTION && j >= 0
						&& j < 2) {
					m_deleteOption = j;
				} else if (ct == ENUM_CONFIG_TAGS::SHOW_POPUP && j >= 0
						&& j < 2) {
					m_showPopup = j;
				} else if (ct == ENUM_CONFIG_TAGS::ONE_APPLICATION_INSTANCE && j >= 0
						&& j < 2) {
					m_oneInstance=j;
				} else {
					if (j < MIN_ICON_HEIGHT || j > MAX_ICON_HEIGHT) {
						printl("error")
						;
						break;
					}
					setIconHeightWidth(j);
				}

			}
			i++;
		}
	}
	//end readConfig
	loadLanguage();

	GSList *formats;
	GSList *elem;
	GdkPixbufFormat *pf;
	char *c, **extension_list;
	formats = gdk_pixbuf_get_formats();
	for (elem = formats; elem; elem = elem->next) {
		pf = (GdkPixbufFormat*) elem->data;
		//p = gdk_pixbuf_format_get_name(pf);
		extension_list = gdk_pixbuf_format_get_extensions(pf);

		for (i = 0; (c = extension_list[i]) != 0; i++) {
			if (i == 0) {
				m_vExtension.push_back(c);
			}
			m_vLowerExtension.push_back(c);
		}
		g_strfreev(extension_list);

	}
	g_slist_free(formats);

	m_window = gtk_application_window_new(GTK_APPLICATION(application));
	m_area = gtk_drawing_area_new();
	m_toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_widget_set_halign(m_toolbar, GTK_ALIGN_CENTER);
	//prevents destroy after gtk_container_remove
	g_object_ref(m_toolbar);

	//before setButtonState
	for (auto a : ADDITIONAL_IMAGES) {
		m_additionalImages.push_back(pixbuf(a));
	}

	i = 0;
	for (auto a : TOOLBAR_IMAGES) {
		auto b = m_button[i] = gtk_button_new();
		buttonPixbuf[i][1] = pixbuf(a);
		buttonPixbuf[i][0] = gdk_pixbuf_copy(buttonPixbuf[i][1]);
		gdk_pixbuf_saturate_and_pixelate(buttonPixbuf[i][1], buttonPixbuf[i][0],
				.3f, false);

		setButtonState(i, true);

		g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(button_clicked),
				GP(i));
		gtk_box_pack_start(GTK_BOX(m_toolbar), b, FALSE, FALSE, 0);
		if (ONE_OF(TOOLBAR_INDEX(i), TOOLBAR_BUTTON_WITH_MARGIN)) {
			gtk_widget_set_margin_start(b, 15);
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
	g_signal_connect(m_window, "key-press-event", G_CALLBACK (key_press), NULL);

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

	lastScroll = 0;

	if (path.empty()) {
		setNoImage();
	} else {
		load(path, 0, true); //after area is created
		if (mode == MODE::LIST) { //after vp is loading in load() function
			setListTopLeftIndexStartValue();
		}
	}

	setTitle();

	gtk_main();
}

Frame::~Frame() {
	g_object_unref(m_toolbar);

	WRITE_CONFIG(CONFIG_TAGS, (int ) m_lastNonListMode, m_ascendingOrder,
			m_listIconHeight, m_languageIndex, m_warningBeforeDelete,
			m_deleteOption, m_showPopup, m_oneInstance);
	stopThreads();
	g_mutex_clear(&m_mutex);
}

void Frame::openUris(char **uris) {
	gchar *fn;
	fn = g_filename_from_uri(uris[0], NULL, NULL);
	load(fn);
	g_free(fn);
//	int i;
//	for (i = 0; uris[i] != 0; i++) {
//		fn = g_filename_from_uri(uris[i], NULL, NULL);
//		load(fn);
//		g_free(fn);
//		break;
//	}
}

void Frame::setTitle() {
	std::string s, t;
	int i;
	if (noImage()) {
		t = getTitleVersion();
	} else {
		t = dir + SEPARATOR;
		const int sz = size();
		if (mode == MODE::LIST) {
			double ts = totalFileSize / double(1 << 20);
			/* format("%d-%d/%d" a-b/size
			 * a=m_listTopLeftIndex + 1
			 * if LIST_ASCENDING_ORDER=1 a<b & b-a+1=listxy => b=a-1+listxy=m_listTopLeftIndex + listxy
			 * if LIST_ASCENDING_ORDER=0 a>b & a-b+1=listxy => b=a+1-listxy=m_listTopLeftIndex +2- listxy
			 */
			t += format("%d-%d/%d", m_listTopLeftIndex + 1,
					m_ascendingOrder ?
							MIN(m_listTopLeftIndex + listxy, sz) :
							MAX(m_listTopLeftIndex + 2 - listxy, 1), sz)
					+ SEPARATOR + getLanguageString(LANGUAGE::TOTAL) + " "
					+ getSizeMbKbB(ts) + ", "
					+ getLanguageString(LANGUAGE::AVERAGE) + " "
					+ getSizeMbKbB(ts / sz);
		} else {
			const std::string n = getFileInfo(vp[pi].m_path, FILEINFO::NAME);
			t += n + SEPARATOR + format("%dx%d", pw, ph) + SEPARATOR
					+ getLanguageString(LANGUAGE::SCALE) + " "
					+ (scale == 1 || mode == MODE::NORMAL ?
							"1" : format("%.2lf", scale)) + SEPARATOR
					+ format("%d/%d", pi + 1, size());

			/* allow IMG_20210823_110315.jpg & compressed IMG_20210813_121527-min.jpg
			 * compress images online https://compressjpeg.com/ru/
			 */
			GRegex *r = g_regex_new("^IMG_\\d{8}_\\d{6}.*\\.jpg$",
					GRegexCompileFlags(G_REGEX_RAW | G_REGEX_CASELESS),
					GRegexMatchFlags(0), NULL);
			if (g_regex_match(r, n.c_str(), GRegexMatchFlags(0), NULL)) {
				i = stoi(n.substr(8, 2)) - 1;
				t += SEPARATOR + n.substr(10, 2)
						+ (i >= 0 && i < 12 ?
								getLanguageString(LANGUAGE::JAN, i) : "???")
						+ n.substr(4, 4);
				for (i = 0; i < 3; i++) {
					t += (i ? ':' : ' ') + n.substr(13 + i * 2, 2);
				}
			}
			g_regex_unref(r);
		}
	}

	gtk_window_set_title(GTK_WINDOW(m_window), t.c_str());
}

void Frame::load(const std::string &p, int index, bool start) {
	std::string s;
	GDir *di;
	const gchar *filename;
//	printi
	stopThreads(); //endThreads=0 after
//	printi
	m_loadid++;
	vp.clear();
	const bool d = isDir(p);
	/* by default pi=0 - first image in directory
	 * if path is not dir pi=0 first image it's ok
	 * if path is file with bad extension then pi=0 it's ok
	 */
	pi = index;
	dir = d ? p : getFileInfo(p, FILEINFO::DIRECTORY);

	totalFileSize = 0;

	//from gtk documentation order of file is arbitrary. Actually it's name (or may be date) ascending order
	if ((di = g_dir_open(dir.c_str(), 0, 0))) {
		while ((filename = g_dir_read_name(di))) {
			s = dir + G_DIR_SEPARATOR + filename;
			if (!isDir(s) && isSupportedImage(s)) {
				if (!d && s == p) {
					pi = size();
				}
				vp.push_back(Image(s, m_loadid));
				totalFileSize += vp.back().m_size;
			}
		}
	}

	addMonitor(dir);

	recountListParameters();

	if (vp.empty()) {
		setNoImage();
	} else {
		//if was no image, need to set m_toolbar buttons enabled
		for (int i = 0; i < TOOLBAR_INDEX_SIZE; i++) {
			setButtonState(i, i != int(mode));
		}

		startThreads();
		setListTopLeftIndexStartValue(); //have to reset
	}

	if (start) {
#if	START_MODE==-1
		setMode(mode, true);
#else
		setMode(INITIAL_MODE,true);
#endif
	} else {
		if (!d && mode == MODE::LIST) {
			setMode(MODE::NORMAL);
		}
	}

	//need to redraw anyway even if no image
	loadImage();

}

void Frame::loadImage() {
	if (!noImage()) {
		GError *err = NULL;
		/* Create pixbuf */
		//here full path
		pix = gdk_pixbuf_new_from_file(vp[pi].m_path.c_str(), &err);

		if (err) {
			println("Error : %s", err->message);
			g_error_free(err);
			setNoImage();
			return;
		}
		pw = pix.width();
		ph = pix.height();

		if (mode == MODE::FIT && m_lastWidth > 0) {
			setSmallImage();
		}

		updateNavigationButtonsState();
	}

	drawImage();

}

void Frame::draw(cairo_t *cr, GtkWidget *widget) {
	if (noImage()) {
		return;
	}

	int i, j, k, l, width, height, destx, desty, w, h;
	const int sz = size();

	width = gtk_widget_get_allocated_width(widget);
	height = gtk_widget_get_allocated_height(widget);

	//1920 959
	//printl(width,height)

	bool windowSizeChanged = m_lastWidth != width || m_lastHeight != height; //size of m_window is changed

	if (windowSizeChanged) {
		m_lastWidth = width;
		m_lastHeight = height;
		recountListParameters();
	}

	if (!m_loadingFontHeight) {
		m_loadingFontHeight = countFontMaxHeight(
				getLanguageString(LANGUAGE::LOADING).c_str(), false, cr);
		filenameFontHeight = countFontMaxHeight("IMG_20211004_093339",
				filenameFontBold, cr);

		//printl(fontHeight)
		if (mode == MODE::LIST) {
			setTitle(); //now lisx, listy counted so need to set title
		}
	}

	const GdkRGBA BLACK_COLOR = { 0, 0, 0, 1 };
	const GdkRGBA WHITE_COLOR = { 1, 1, 1, 1 };

	if (mode == MODE::LIST) {
		for (k = m_listTopLeftIndex, l = 0;
				((m_ascendingOrder && k < sz) || (!m_ascendingOrder && k >= 0))
						&& l < listxy; k += m_ascendingOrder ? 1 : -1, l++) {
			i = l % listx * m_listIconWidth + listdx;
			j = l / listx * m_listIconHeight + listdy;
			auto &o = vp[k];
			GdkPixbuf *p = o.m_thumbnail;
//			printl(k,sz,bool(p),m_ascendingOrder)
			if (p) {
				w = gdk_pixbuf_get_width(p);
				h = gdk_pixbuf_get_height(p);
				copy(p, cr, i + (m_listIconWidth - w) / 2,
						j + (m_listIconHeight - h) / 2, w, h, 0, 0);

				drawTextToCairo(cr, getFileInfo(o.m_path, FILEINFO::SHORT_NAME),
						filenameFontHeight, filenameFontBold, i, j,
						m_listIconWidth, m_listIconHeight, true, 2, WHITE_COLOR,
						true);
			} else {
				drawTextToCairo(cr,
						getLanguageString(LANGUAGE::LOADING).c_str(),
						m_loadingFontHeight, false, i, j, m_listIconWidth,
						m_listIconHeight, true, 2, BLACK_COLOR);
			}
		}

	} else {
		if (mode == MODE::FIT) {
			if (windowSizeChanged) {
				setSmallImage(); //set pws,phs
			}
			w = pws;
			h = phs;
		} else {
			w = pw;
			h = ph;
		}

		destx = (width - w) / 2;
		desty = (height - h) / 2;

		adjust(destx, 0);
		adjust(desty, 0);

		aw = MIN(w, width);
		ah = MIN(h, height);

		if (windowSizeChanged) {
			adjustPos();
			if (mode == MODE::FIT) {
				setTitle();		//scale changed
			}
		}

		copy(mode == MODE::FIT ? pixs : pix, cr, destx, desty, aw, ah, posh,
				posv);
	}

	/*
	 GdkRectangle r={0,0,width,height};
	 gdk_cairo_get_clip_rectangle(cr,&r);
	 double ko=(double(r.height)/r.width);
	 cairo_translate(cr, r.x+r.width/2, r.y+r.height/2);
	 cairo_scale(cr, 1, ko);
	 gdk_cairo_set_source_rgba(cr, &RED_COLOR);
	 cairo_set_line_width(cr, 3);
	 cairo_move_to(cr,r.width/2,0);//angle 0
	 cairo_arc(cr, 0, 0, r.width/2, 0, 2 * G_PI);
	 cairo_stroke_preserve(cr);
	 */

}

void Frame::drawImage() {
	posh = posv = 0;
	redraw();
}

gboolean Frame::keyPress(GdkEventKey *event) {
//	from gtk documentation return value
//	TRUE to stop other handlers from being invoked for the event. FALSE to propagate the event further.
	const int k = event->keyval;
	const guint16 hwkey = event->hardware_keycode;

	//printl(k, GDK_KEY_minus,GDK_KEY_equal,GDK_KEY_KP_Subtract, GDK_KEY_minus);
	const bool plus = k == GDK_KEY_KP_Add || k == GDK_KEY_equal;
	const bool minus = k == GDK_KEY_KP_Subtract || k == GDK_KEY_minus;
	int i = indexOfV(true, plus, minus, hwkey == 'L', hwkey == 'E',
			hwkey == 'R', hwkey == 'T', hwkey == 'H', hwkey == 'V'

			, oneOf(k, GDK_KEY_Home, GDK_KEY_KP_7),
			oneOf(k, GDK_KEY_Page_Up, GDK_KEY_KP_9),
			oneOf(k, GDK_KEY_Left, GDK_KEY_KP_4),
			oneOf(k, GDK_KEY_Right, GDK_KEY_KP_6),
			oneOf(k, GDK_KEY_Page_Down, GDK_KEY_KP_3),
			oneOf(k, GDK_KEY_End, GDK_KEY_KP_1)

			//english and russian keyboard layout (& caps lock)
					, hwkey == 'O',
			oneOf(k, GDK_KEY_Delete, GDK_KEY_KP_Decimal),
			false,		//no hotkey for order
			hwkey == 'F' || oneOf(k, GDK_KEY_F11, GDK_KEY_Escape),
			hwkey == 'H' || k == GDK_KEY_F1);

	if (i != -1) {
		if (mode == MODE::LIST && (plus || minus)) {
			buttonClicked(plus ? LIST_ZOOM_IN : LIST_ZOOM_OUT);
		} else {
			buttonClicked(i);
		}
	}
	return i != -1;
}

void Frame::setDragDrop(GtkWidget *widget) {
	gtk_drag_dest_set(widget, GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
	gtk_drag_dest_add_uri_targets(widget);
	g_signal_connect(widget, "drag-data-received",
			G_CALLBACK(drag_and_drop_received), 0);
}

void Frame::scroll(GdkEventScroll *event) {
	//printl(event->delta_x,event->delta_y,event->time,event->type)
	auto x = event->delta_x * WHEEL_MULTIPLIER;
	auto y = event->delta_y * WHEEL_MULTIPLIER;

	if (mode == MODE::NORMAL && (x != 0 || y != 0)) {
		setPosRedraw(x, y, event->time);
	} else if (mode == MODE::LIST) {
		scrollList(event->delta_y * 50);
	}
}

void Frame::setPosRedraw(double dx, double dy, guint32 time) {
	/* if no horizontal scroll then switch next/previous image
	 * if no vertical scroll then switch next/previous image
	 */
	if (((pw <= aw && dx != 0 && dy == 0) || (ph <= ah && dx == 0 && dy != 0))
			&& time > SCROLL_DELAY_MILLISECONDS + lastScroll) {
		lastScroll = time;
//		switchImage(1, dx > 0 || dy > 0);
		return;
	}
	posh += dx;
	posv += dy;
	adjustPos();
	redraw(false);
}

bool Frame::noImage() {
	return vp.empty();
}

void Frame::setNoImage() {
	vp.clear();
	setTitle();

	for (int i = 0; i < TOOLBAR_INDEX_SIZE; i++) {
		TOOLBAR_INDEX t = TOOLBAR_INDEX(i);
		setButtonState(i,
				oneOf(t, TOOLBAR_INDEX::OPEN, TOOLBAR_INDEX::SETTINGS,
						TOOLBAR_INDEX::HELP));
	}
}

bool Frame::isSupportedImage(const std::string &p) {
	return oneOf(getFileInfo(p, FILEINFO::LOWER_EXTENSION), m_vLowerExtension);
}

void Frame::adjustPos() {
	adjust(posh, 0, pw - aw);
	adjust(posv, 0, ph - ah);
}

void Frame::openDirectory() {
	std::string s = filechooser(m_window, dir);
	if (!s.empty()) {
		load(s);
	}
}

void Frame::switchImage(int v, bool add) {
	if (noImage()) {
		return;
	}
	int i = pi;
	pi += add ? v : -v;
	adjust(pi, 0, size() - 1);

	/* if last image in dir was scrolled down and user click next image (right key)
	 * do not need to call loadImage() because it load the same image and scroll it to start
	 * but nothing should happens
	 */
	if (i != pi) {
		loadImage();
	}
}

void Frame::setSmallImage() {
	pixs = scaleFit(pix, m_lastWidth, m_lastHeight, pws, phs);
	scale = double(pws) / pw;
}

void Frame::rotatePixbuf(Pixbuf &p, int &w, int &h, int angle) {
	//angle 90 or 180 or 270
	p = gdk_pixbuf_rotate_simple(p,
			angle == 180 ?
					GDK_PIXBUF_ROTATE_UPSIDEDOWN :
					(angle == 90 ?
							GDK_PIXBUF_ROTATE_CLOCKWISE :
							GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE));
	if (angle != 180) {
		std::swap(w, h);
	}
}

void Frame::flipPixbuf(Pixbuf &p, bool horizontal) {
	p = gdk_pixbuf_flip(p, horizontal);
}

int Frame::showConfirmation(const std::string &text) {
	GtkWidget *dialog;
	dialog = gtk_dialog_new_with_buttons(
			getLanguageString(LANGUAGE::QUESTION).c_str(), GTK_WINDOW(m_window),
			GTK_DIALOG_MODAL, getLanguageString(LANGUAGE::YES).c_str(),
			GTK_RESPONSE_YES, getLanguageString(LANGUAGE::NO).c_str(),
			GTK_RESPONSE_NO,
			NULL);
	auto content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_container_add(GTK_CONTAINER(content_area),
			gtk_label_new(
					getLanguageString(
							LANGUAGE::DO_YOU_REALLY_WANT_TO_DELETE_THE_IMAGE).c_str()));
	gtk_widget_show_all(dialog);
	auto r = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	return r;
}

void Frame::startThreads() {
	int i;
	for (i = getFirstListIndex(); vp[i].m_thumbnail != nullptr;
			i += m_ascendingOrder ? 1 : -1) {
		if ((m_ascendingOrder && i >= size()) || (!m_ascendingOrder && i < 0)) {
			break;
		}
	}
	m_threadNumber = i;

	i = 0;
	for (auto &p : pThread) {
		p = g_thread_new("", thumbnail_thread, GP(i++));
	}
}

void Frame::thumbnailThread(int n) {
	int w, h, v, max = 0;
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
		auto &o = vp[v];

		if (!o.m_thumbnail) {
			//full path
			p = o.m_path;
			o.m_thumbnail = scaleFit(p, m_listIconWidth, m_listIconHeight, w,
					h);

			gdk_threads_add_idle(set_show_thumbnail_thread, GP(v));
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
	for (auto &p : pThread) {
		if (p) {		//1st time p==nullptr
			g_thread_join(p);
			p = nullptr;		//allow call stopThreads many times
		}
	}
	//	println("%.3lf",double(clock()-begin)/CLOCKS_PER_SEC);

	g_atomic_int_set(&m_endThreads, 0);

//	int i=888;
//	gboolean b=g_source_remove_by_user_data ( GP(i));
//	if(b){
//		printl("removed",i)
//	}

	/*
	 bool r=false;
	 int i;
	 for(i=0;i<size();i++){
	 gboolean b=g_source_remove_by_user_data ( GP(i));
	 if(b){
	 r=true;
	 printl("removed",i)
	 }
	 }
	 if(!r){
	 printl("nothing to remove",size())
	 }
	 */

}

void Frame::buttonPress(GdkEventButton *event) {
	//middle button is three fingers tap (in windows10 settings)
	if (event->type == GDK_BUTTON_PRESS) {//ignore double clicks GDK_DOUBLE_BUTTON_PRESS
		const bool left = event->button == 1;
		if (left || event->button == 2) { //left/middle mouse button
			if (mode == MODE::LIST) {
				if (left) { // left button check click on image
					int cx = event->x;
					int cy = event->y;
					if (cx >= listdx && cx < m_lastWidth - listdx
							&& cy >= listdy && cy < m_lastHeight - listdy) {
						pi = ((cx - listdx) / m_listIconWidth
								+ (cy - listdy) / m_listIconHeight * listx)
								* (m_ascendingOrder ? 1 : -1)
								+ m_listTopLeftIndex;
						setMode(MODE::FIT);
						loadImage();
					}
				}
			} else { //next/previous image
					 //buttonClicked(left ? TOOLBAR_INDEX::NEXT : TOOLBAR_INDEX::PREVIOUS);
			}
		} else if (event->button == 3) { //on right mouse open file/directory
			buttonClicked(TOOLBAR_INDEX::OPEN);
		}
	}
}

void Frame::setShowThumbnail(int i) {
	auto &o = vp[i];
	if (o.m_loadid != m_loadid) {
		//printl("skipped",o.m_loadid,m_loadid,"i=",i);
		return;
	}
	if (mode == MODE::LIST
			&& ((m_ascendingOrder && i >= m_listTopLeftIndex
					&& i < m_listTopLeftIndex + listxy)
					|| (!m_ascendingOrder && i <= m_listTopLeftIndex && i >= 0))) {
		redraw(false);
	}
}

void Frame::setListTopLeftIndexStartValue() {
	m_listTopLeftIndex = getFirstListIndex();
	listTopLeftIndexChanged();
}

void Frame::scrollList(int v) {
	if (mode == MODE::LIST) {
		int i, j, min, max;
		if (listxy >= size()) {
			//min=max=0;
			return;
		}
		getListMinMaxIndex(min, max);
		if (v == GOTO_BEGIN) {
			i = m_ascendingOrder ? min : max;
		} else if (v == GOTO_END) {
			i = m_ascendingOrder ? max : min;
		} else {
			i = m_listTopLeftIndex + v * (m_ascendingOrder ? 1 : -1) * listx;
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
	if (t == TOOLBAR_INDEX::OPEN) {
		openDirectory();
		return;
	} else if (t == TOOLBAR_INDEX::SETTINGS) {
		showSettings();
		return;
	} else if (t == TOOLBAR_INDEX::HELP) {
		showHelp();
		return;
	} else if (t == TOOLBAR_INDEX::FULLSCREEN) {
		GdkWindow *gdk_window = gtk_widget_get_window(m_window);
		GdkWindowState state = gdk_window_get_state(gdk_window);
		if (state & GDK_WINDOW_STATE_FULLSCREEN) {
			gtk_container_add(GTK_CONTAINER(m_box), m_toolbar);
			gtk_window_unfullscreen(GTK_WINDOW(m_window));
		} else {
			gtk_container_remove(GTK_CONTAINER(m_box), m_toolbar);
			gtk_window_fullscreen(GTK_WINDOW(m_window));
		}
		return;
	}

	if (noImage()) {
		return;
	}

	i = INDEX_OF(t, TMODE);
	if (i != -1) {
		MODE m = MODE(i);
		if (mode != m) {
			setMode(m);
			if (mode == MODE::LIST) {
				//setListTopLeftIndexStartValue();
				redraw();
			} else {
				if (mode == MODE::FIT) {
					setSmallImage();
				}
				drawImage();
			}
		}
		return;
	}

	i = INDEX_OF(t, NAVIGATION);
	if (i != -1) {
		if (mode == MODE::LIST) {
			int a[] = { GOTO_BEGIN, -listy, -1, 1, listy, GOTO_END };
			scrollList(a[i]);
		} else {
			if (t == TOOLBAR_INDEX::HOME || t == TOOLBAR_INDEX::END) {
				pi = ((t == TOOLBAR_INDEX::HOME) == m_ascendingOrder) ?
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
		m_ascendingOrder = !m_ascendingOrder;
		if (mode == MODE::LIST) {
			stopThreads();
			m_listTopLeftIndex = getFirstListIndex();
			startThreads();
			setVariableImagesButtonsState();
			redraw();
		} else {
			m_listTopLeftIndex = getFirstListIndex();
			setVariableImagesButtonsState();
		}
	}

	if (t == TOOLBAR_INDEX::DELETE_FILE) { //mode!=MODE::LIST can be called from keyboard
		if (mode != MODE::LIST
				&& (!m_warningBeforeDelete
						|| showConfirmation(
								getLanguageString(
										LANGUAGE::DO_YOU_REALLY_WANT_TO_DELETE_THE_IMAGE))
								== GTK_RESPONSE_YES)) {

			//not need full reload
			stopThreads();

			//before g_remove change totalFileSize
			auto &a = vp[pi];
			totalFileSize -= a.m_size;

			if (m_deleteOption) {
				g_remove(a.m_path.c_str());
			} else {
				deleteFileToRecycleBin(a.m_path);
			}
			vp.erase(vp.begin() + pi);
			int sz = size();
			if (sz == 0) {
				setNoImage();
			} else {
				adjust(m_listTopLeftIndex, 0, sz - 1);
				adjust(pi, 0, sz - 1);
				recountListParameters();
				loadImage();

				//see Image.h for documentation why need to set new m_loadid & set o.thumbmails
				//old indexes for setShowThumbnail() are not valid so make new m_loadid & set o.thumbmails
				m_loadid++;
				for (auto &o : vp) {
					o.m_loadid = m_loadid;
				}
				startThreads();
			}
		}
		return;
	}

	i = INDEX_OF(t, IMAGE_MODIFY);
	if (i != -1) {
		if (mode == MODE::LIST) {
			if (t == LIST_ZOOM_IN || t == LIST_ZOOM_OUT) {
				i = m_listIconHeight + (t == LIST_ZOOM_IN ? 1 : -1) * 5;
				if (i >= MIN_ICON_HEIGHT && i <= MAX_ICON_HEIGHT) {
					stopThreads();
					setIconHeightWidth(i);
					recountListParameters();
					m_loadingFontHeight = 0;				//to recount font
					m_loadid++;
					for (auto &o : vp) {
						o.free();
						o.m_thumbnail = nullptr;
						o.m_loadid = m_loadid;
					}
					startThreads();
					redraw(false);
				}
			}
		} else {
			if (i >= 3) {
				flipPixbuf(pix, i == 3);
			} else {
				rotatePixbuf(pix, pw, ph, 90 * (i + 1));
			}
			//small image angle should match with big image, so always call setSmallImage
			setSmallImage();
			drawImage();
		}
		return;
	}
}

void Frame::buttonClicked(int t) {
	buttonClicked(TOOLBAR_INDEX(t));
}

void Frame::showSettings() {
	int i;
	GtkWidget *grid, *w;
	VString v;
	std::string s;
	char *markup;

	grid = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(grid), 15);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
	gtk_widget_set_margin_bottom(grid, 5);

	for (i = 0; i < SIZE_OPTIONS; i++) {
		w = gtk_label_new(getLanguageString(OPTIONS[i]).c_str());
		gtk_widget_set_halign(w, GTK_ALIGN_START);
		gtk_grid_attach(GTK_GRID(grid), w, 0, i, 1, 1);
		auto e = OPTIONS[i];
		if (e == LANGUAGE::LANGUAGE) {
			w = createLanguageCombo(i);
			gtk_widget_set_halign(w, GTK_ALIGN_START);			//not stretch
		} else if (e == LANGUAGE::REMOVE_FILE_OPTION) {
			for (auto e : { LANGUAGE::REMOVE_FILES_TO_RECYCLE_BIN,
					LANGUAGE::REMOVE_FILES_PERMANENTLY }) {
				v.push_back(getLanguageString(e));
			}
			w = createTextCombo(i, v, 0);
			gtk_widget_set_halign(w, GTK_ALIGN_START);			//not stretch
		} else if (oneOf(e, LANGUAGE::ASK_BEFORE_DELETING_A_FILE,
				LANGUAGE::SHOW_POPUP_TIPS,
				LANGUAGE::ONE_APPLICATION_INSTANCE)) {
			w = m_options[i] = gtk_check_button_new();
			if(e==LANGUAGE::ONE_APPLICATION_INSTANCE){
				w= gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
				gtk_container_add(GTK_CONTAINER(w), m_options[i]);
				gtk_container_add(GTK_CONTAINER(w),gtk_label_new(getLanguageString(LANGUAGE::REQUIRES_RESTARTING).c_str()));
			}
		} else if (e == LANGUAGE::HOMEPAGE) {
			s = getLanguageString(e, 1);
			w = gtk_label_new(NULL);
			markup = g_markup_printf_escaped("<a href=\"%s,%s\">\%s,%s</a>",
					s.c_str(), LNG_LONG[m_languageIndex].c_str(),
					s.substr(s.find("slo")).c_str(),
					LNG_LONG[m_languageIndex].c_str());
			gtk_label_set_markup(GTK_LABEL(w), markup);
			g_free(markup);
			g_signal_connect(w, "activate-link", G_CALLBACK(label_clicked),
					gpointer(s.c_str()));
			gtk_widget_set_halign(w, GTK_ALIGN_START);
		} else if (e == LANGUAGE::SUPPORTED_FORMATS) {
			w = gtk_label_new(getExtensionString(true).c_str());
		} else {
			w = gtk_label_new(getLanguageString(e, 1).c_str());
			gtk_widget_set_halign(w, GTK_ALIGN_START);
		}
		gtk_grid_attach(GTK_GRID(grid), w, 1, i, 1, 1);
	}

	s = getBuildVersionString(false) + ", "
			+ getLanguageString(LANGUAGE::FILE_SIZE) + " "
			+ toString(getApplicationFileSize(), ',');
	gtk_grid_attach(GTK_GRID(grid), gtk_label_new(s.c_str()), 0, i++, 2, 1);
	showModalDialog(grid, 1);
}

void Frame::showHelp() {
	auto s = getLanguageString(LANGUAGE::HELP) + "\n"
			+ getLanguageString(LANGUAGE::SUPPORTED_FORMATS) + " "
			+ getExtensionString(false);
	auto l = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(l), s.c_str());
	showModalDialog(l, 0);
}

void Frame::showModalDialog(GtkWidget *w, int o) {
	GtkWidget *b, *b1, *b2;
	auto d = m_modal = gtk_dialog_new();
	gtk_window_set_modal(GTK_WINDOW(d), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(d), GTK_WINDOW(m_window));

	auto title = getTitleVersion();
	gtk_window_set_title(GTK_WINDOW(d), title.c_str());
	gtk_window_set_resizable(GTK_WINDOW(d), 1);

	if (o) {
		b = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
		gtk_container_add(GTK_CONTAINER(b), w);
		b1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
		for (auto e : { LANGUAGE::OK, LANGUAGE::RESET, LANGUAGE::CANCEL }) {
			b2 = gtk_button_new_with_label(getLanguageString(e).c_str());
			g_signal_connect(b2, "clicked", G_CALLBACK(options_button_clicked),
					GP(e));
			gtk_container_add(GTK_CONTAINER(b1), b2);
		}

		gtk_container_add(GTK_CONTAINER(b), b1);
		updateOptions();
	} else {
		b = w;
	}
	auto ca = gtk_dialog_get_content_area(GTK_DIALOG(d));
	gtk_container_add(GTK_CONTAINER(ca), b);

	gtk_widget_show_all(d);

	gtk_dialog_run(GTK_DIALOG(d));
	gtk_widget_destroy(d);
}

void Frame::recountListParameters() {
	const int sz = size();
	listx = MAX(1, m_lastWidth / m_listIconWidth);
	listy = MAX(1, m_lastHeight / m_listIconHeight);
	//MIN(listx,sz) center horizontally if images less than listx
	listdx = (m_lastWidth - MIN(listx,sz) * m_listIconWidth) / 2;
	//MIN(listx,sz) center vertically if images less than listx*listy
	int i;
	i = sz / listx;
	if (sz % listx != 0) {
		i++;
	}
	listdy = (m_lastHeight - MIN(listy, i) * m_listIconHeight) / 2;
	listxy = listx * listy;
	updateNavigationButtonsState();
}

void Frame::updateNavigationButtonsState() {
	const int sz = size();
	if (mode == MODE::LIST) {
		if (sz <= listxy) {					//all images in screen
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
			setNavigationButtonsState(pi != 0, pi != size() - 1);
		} else {
			setNavigationButtonsState(pi != size() - 1, pi != 0);
		}
	}
}

void Frame::setButtonState(int i, bool enable) {
	setButtonImage(i, enable, buttonPixbuf[i][enable]);
}

void Frame::setMode(MODE m, bool start) {
	if (mode != m || start) {
		mode = m;
		if (mode != MODE::LIST) {
			m_lastNonListMode = mode;
		}
		int i = 0;
		for (auto e : TMODE) {
			setButtonState(e, i++ != int(m));
		}

		for (auto e : { TOOLBAR_INDEX::ROTATE_CLOCKWISE,
				TOOLBAR_INDEX::FLIP_HORIZONTAL, TOOLBAR_INDEX::FLIP_VERTICAL,
				TOOLBAR_INDEX::DELETE_FILE }) {
			setButtonState(e, mode != MODE::LIST);
		}

		setVariableImagesButtonsState();
		updateNavigationButtonsState();
		setPopups();
	}
}

int Frame::size() {
	return vp.size();
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
		if (sz % listx != 0) {
			j += listx - sz % listx;
		}
		min = 0;
		max = j - listxy;
	} else {
		j = 0;
		if (sz % listx != 0) {
			j -= listx - sz % listx;
		}
		min = j + listxy - 1;
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

void Frame::setVariableImagesButtonsState() {
	int i = 0;
	setButtonImage(int(TOOLBAR_INDEX::REORDER_FILE), true,
			m_additionalImages[!m_ascendingOrder]);
	TOOLBAR_INDEX t[] = { LIST_ZOOM_OUT, LIST_ZOOM_IN };
	for (auto a : t) {
		if (mode == MODE::LIST) {
			setButtonImage(int(a), true, m_additionalImages[i + 2]);
		} else {
			setButtonState(a, true);
		}
		i++;
	}

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

void Frame::setIconHeightWidth(int height) {
	m_listIconHeight = height;
	m_listIconWidth = 4 * height / 3;
}

void Frame::loadLanguage() {
	static const int MAX_BUFF_LEN = 2048;
	FILE *f;
	char buff[MAX_BUFF_LEN];
	std::string s, s1;
	m_language.clear();

	f = open(m_languageIndex, "language");
	assert(f!=NULL);

	int i = 0;
	while (fgets(buff, MAX_BUFF_LEN, f) != NULL) {
		s = buff;
		if (i >= int(LANGUAGE::HELP)) {
			s1 += s;
		} else {
			if (s.empty()) {
				continue;
			}
			if (s[s.length() - 1] == '\n') {
				s = s.substr(0, s.length() - 1);
			}
			m_language.push_back(s);
			i++;
		}
	}
	fclose(f);
	m_language.push_back(s1);
}

GtkWidget* Frame::createLanguageCombo(int n) {
	int i;
	GtkTreeIter iter;
	GtkCellRenderer *renderer;
	const char *image[] = { "en.gif", "ru.gif" };

	GtkListStore *gls = gtk_list_store_new(1, GDK_TYPE_PIXBUF);
	for (i = 0; i < 2; i++) {
		gtk_list_store_append(gls, &iter);
		gtk_list_store_set(gls, &iter, 0, pixbuf(image[i]), -1);
	}
	auto w = m_options[n] = gtk_combo_box_new_with_model(GTK_TREE_MODEL(gls));
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(w), renderer,
	TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(w), renderer, "pixbuf", 0,
	NULL);
	gtk_combo_box_set_active(GTK_COMBO_BOX(w), m_languageIndex);
	return w;
}

std::string Frame::getTitleVersion() {
	return getLanguageString(LANGUAGE::IMAGE_VIEWER) + " "
			+ getLanguageString(LANGUAGE::VERSION)
			+ format(" %.1lf", IMAGE_VIEWER_VERSION);
}

std::string& Frame::getLanguageString(LANGUAGE l, int add) {
	return m_language[int(l) + add];
}

GtkWidget* Frame::createTextCombo(int n, VString &v, int active) {
	GtkWidget *w = m_options[n] = gtk_combo_box_text_new();
	for (auto &a : v) {
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(w), a.c_str());
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(w), active);
	return w;
}

void Frame::optionsButtonClicked(LANGUAGE l) {
	int i = 0;
	auto oldLanguage = m_languageIndex;
	auto oldShowPopup = m_showPopup;
	if (l == LANGUAGE::OK) {
		m_languageIndex = gtk_combo_box_get_active(
				GTK_COMBO_BOX(m_options[i++]));
		m_warningBeforeDelete = gtk_toggle_button_get_active(
				GTK_TOGGLE_BUTTON(m_options[i++]));
		m_deleteOption = gtk_combo_box_get_active(
				GTK_COMBO_BOX(m_options[i++]));
		m_showPopup = gtk_toggle_button_get_active(
				GTK_TOGGLE_BUTTON(m_options[i++]));
		m_oneInstance=gtk_toggle_button_get_active(
				GTK_TOGGLE_BUTTON(m_options[i++]));
	} else if (l == LANGUAGE::RESET) {
		resetOptions();
		updateOptions();
	}
	gtk_dialog_response(GTK_DIALOG(m_modal), GTK_RESPONSE_OK);
	if (m_languageIndex != oldLanguage) {
		loadLanguage();
		setTitle();					//update title after dialog is closed
	}
	if (m_languageIndex != oldLanguage || m_showPopup != oldShowPopup) {
		setPopups();
	}
}

void Frame::resetOptions() {
	m_languageIndex = 0;
	m_warningBeforeDelete = 1;
	m_deleteOption = 0;
	m_showPopup = 1;
	m_oneInstance = 1;
}

void Frame::updateOptions() {
	int i = 0;
	gtk_combo_box_set_active(GTK_COMBO_BOX(m_options[i++]), m_languageIndex);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_options[i++]),
			m_warningBeforeDelete);
	gtk_combo_box_set_active(GTK_COMBO_BOX(m_options[i++]), m_deleteOption);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_options[i++]),
			m_showPopup);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_options[i++]),
			m_oneInstance);
}

void Frame::setPopups() {
	int i, j;
	std::string s;
	VString v;
	i = 0;
	for (auto e : m_button) {
		if (m_showPopup && gtk_widget_get_sensitive(e)) {
			v = split(getLanguageString(LANGUAGE::LTOOLTIP1, i), '|');
			j = v.size() == 2 && mode == MODE::LIST;
			s = v[j];
			gtk_widget_set_tooltip_text(e, s.c_str());
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
			getLanguageString(LANGUAGE::OPEN_FILE).c_str(), GTK_WINDOW(parent),
			onlyFolder ?
					GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER :
					GTK_FILE_CHOOSER_ACTION_OPEN,
			getLanguageString(LANGUAGE::CANCEL).c_str(), GTK_RESPONSE_CANCEL,
			getLanguageString(LANGUAGE::OPEN).c_str(), GTK_RESPONSE_ACCEPT,
			NULL);

	if (!onlyFolder) {
		/* add the additional "Select" button
		 * "select" button allow select folder and file
		 * "open" button can open only files
		 */
		gtk_dialog_add_button(GTK_DIALOG(dialog),
				getLanguageString(LANGUAGE::SELECT).c_str(), MY_SELECTED);
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
	for (int i = 0; i < 3; i++, v *= 1024) {
		s = format("%.2lf", v);
		if (s != "0.00" || i == 2) {
			return s + getLanguageString(LANGUAGE::MEGABYTES, i);
		}
	}
	return "";
}

void Frame::addMonitor(std::string &path) {
	GFileMonitor *monitor;
	GFile *directory = g_file_new_for_path(path.c_str());
	monitor = g_file_monitor_directory(directory, G_FILE_MONITOR_SEND_MOVED,
			NULL, NULL);
	if (monitor != NULL) {
		g_signal_connect_object(G_OBJECT(monitor), "changed",
				G_CALLBACK(directory_changed),
				NULL, G_CONNECT_AFTER);
	}
}

void Frame::directoryChanged() {
	m_timer = 0;
	load(dir);
}

void Frame::addEvent() {
	//if delete several files then got many delete events, the same if copy file to folder so just proceed last signal
	stopTimer(m_timer);
	m_timer = g_timeout_add(EVENT_TIME, timer_animation_handler, gpointer(0));
}

void Frame::stopTimer(guint &t) {
	if (t) {
		g_source_remove(t);
		t = 0;
	}
}

std::string Frame::getExtensionString(bool b) {
	if (b) {
		int sz = m_vExtension.size(), i = 0;
		std::string s;
		for (auto &e : m_vExtension) {
			if (i) {
				s += i == sz / 2 ? '\n' : ' ';
			}
			s += e;
			i++;
		}
		return s;
	} else {
		return joinV(m_vExtension, ' ');
	}
}

bool Frame::isOneInstanceOnly() {
	MapStringString m;
	MapStringString::iterator it;
	int j;
	m_oneInstance = DEFAULT_ONE_INSTANCE;
	if (loadConfig(m)
			&& (it =
					m.find(
							CONFIG_TAGS[int(
									ENUM_CONFIG_TAGS::ONE_APPLICATION_INSTANCE)]))
					!= m.end()) {
		if (parseString(it->second, j)) {
			m_oneInstance = j;
		}
	}
	return m_oneInstance;
}
