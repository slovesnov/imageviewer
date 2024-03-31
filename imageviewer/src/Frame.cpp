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

const char *ADDITIONAL_IMAGES[] =
		{ "sort_ascending.png", "sort_descending.png" };

const char *TOOLBAR_IMAGES[] = { "magnifier_zoom_in.png",
		"magnifier_zoom_out.png",

		"zoom.png", "zoom_actual.png", "zoom_fit.png", "large_tiles.png",

		"arrow_rotate_anticlockwise.png", "arrow_refresh.png",
		"arrow_rotate_clockwise.png", "leftright.png", "updown.png",

		"control_start_blue.png", "control_rewind_blue.png", "previous.png",
		"control_play_blue.png", "control_fastforward_blue.png",
		"control_end_blue.png",

		"folder.png", "cross.png", ADDITIONAL_IMAGES[0],

		"fullscreen.png", "setting_tools.png", "help.png" };
static_assert(SIZEI(TOOLBAR_IMAGES)==TOOLBAR_INDEX_SIZE);

const TOOLBAR_INDEX ZOOM_INOUT[] = { TOOLBAR_INDEX::ZOOM_IN,
		TOOLBAR_INDEX::ZOOM_OUT };

const TOOLBAR_INDEX IMAGE_MODIFY[] = { TOOLBAR_INDEX::ROTATE_CLOCKWISE,
		TOOLBAR_INDEX::ROTATE_180, TOOLBAR_INDEX::ROTATE_ANTICLOCKWISE,
		TOOLBAR_INDEX::FLIP_HORIZONTAL, TOOLBAR_INDEX::FLIP_VERTICAL };

const TOOLBAR_INDEX NAVIGATION[] = { TOOLBAR_INDEX::HOME,
		TOOLBAR_INDEX::PAGE_UP, TOOLBAR_INDEX::PREVIOUS, TOOLBAR_INDEX::NEXT,
		TOOLBAR_INDEX::PAGE_DOWN, TOOLBAR_INDEX::END };

const TOOLBAR_INDEX TMODE[] = { TOOLBAR_INDEX::MODE_ZOOM_ANY,
		TOOLBAR_INDEX::MODE_ZOOM_100, TOOLBAR_INDEX::MODE_ZOOM_FIT,
		TOOLBAR_INDEX::MODE_LIST };

const TOOLBAR_INDEX TOOLBAR_BUTTON_WITH_MARGIN[] = {
		TOOLBAR_INDEX::MODE_ZOOM_ANY, TOOLBAR_INDEX::ROTATE_ANTICLOCKWISE,
		TOOLBAR_INDEX::HOME, TOOLBAR_INDEX::OPEN };
const int TOOLBAR_BUTTON_MARGIN = 25;

const std::string CONFIG_TAGS[] = { "version", "mode", "order",
		"list icon height", "language", "warning before delete",
		"delete option", "show popup tips", "one application instance",
		"remember the last open directory", "last open directory" };
enum class ENUM_CONFIG_TAGS {
	CVERSION,
	CMODE,
	ORDER,
	LIST_ICON_HEIGHT,
	CLANGUAGE,
	ASK_BEFORE_DELETE,
	DELETE_OPTION,
	SHOW_POPUP,
	ONE_APPLICATION_INSTANCE,
	REMEMBER_THE_LAST_OPEN_DIRECTORY,
	LAST_OPEN_DIRECTORY, //not show in options
};
const std::string SEPARATOR = "       ";
static const int EVENT_TIME = 1000; //milliseconds, may be problems with small timer
int Frame::m_oneInstance;
std::vector<int> Frame::m_optionsDefalutValue;

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
	m_zoom = 1;
	m_lastNonListMode = MODE::ANY;
	m_optionsPointer = { &m_languageIndex, &m_warningBeforeDelete,
			&m_deleteOption, &m_showPopup, &m_oneInstance,
			&m_rememberLastOpenDirectory };
	m_optionsDefalutValue = {
	/*m_languageIndex */0,
	/*m_warningBeforeDelete */1,
	/*m_deleteOption */0,
	/*m_showPopup */1,
	/*m_oneInstance */1,
	/*m_rememberLastOpenDirectory*/1, };

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
	//drawing area height 959,so got 10 rows
	//4*95/3 = 126, 1920/126=15.23 so got 15 columns
	setIconHeightWidth(95);

	resetOptions();

	MapStringString m;
	MapStringString::iterator it;
	if (loadConfig(m)) {
		i = j = 0;
		for (auto t : CONFIG_TAGS) {
			if ((it = m.find(t)) != m.end()) {
				ct = ENUM_CONFIG_TAGS(i);
				parseString(it->second, j);
				if (ct == ENUM_CONFIG_TAGS::CMODE && j >= 0 && j < SIZEI(TMODE)) {
					m_mode = MODE(j);
				} else if (ct == ENUM_CONFIG_TAGS::ORDER && j >= 0 && j < 2) {
					m_ascendingOrder = j == 1;
				} else if (ct == ENUM_CONFIG_TAGS::LIST_ICON_HEIGHT
						&& j >= MIN_ICON_HEIGHT && j <= MAX_ICON_HEIGHT) {
					setIconHeightWidth(j);
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
				} else if (ct == ENUM_CONFIG_TAGS::ONE_APPLICATION_INSTANCE
						&& j >= 0 && j < 2) {
					m_oneInstance = j;
				} else if (ct
						== ENUM_CONFIG_TAGS::REMEMBER_THE_LAST_OPEN_DIRECTORY
						&& j >= 0 && j < 2) {
					m_rememberLastOpenDirectory = j;
				} else if (ct == ENUM_CONFIG_TAGS::LAST_OPEN_DIRECTORY) {
					m_dir = it->second;
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
		m_buttonPixbuf[i][1] = pixbuf(a);
		m_buttonPixbuf[i][0] = gdk_pixbuf_copy(m_buttonPixbuf[i][1]);
		gdk_pixbuf_saturate_and_pixelate(m_buttonPixbuf[i][1], m_buttonPixbuf[i][0],
				.3f, false);

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

	m_lastScroll = 0;

	if (path.empty()) {
		if (m_rememberLastOpenDirectory) {
			load(m_dir, 0, true);
		} else {
			setNoImage();
		}
	} else {
		load(path, 0, true); //after area is created
		if (m_mode == MODE::LIST) { //after vp is loading in load() function
			setListTopLeftIndexStartValue();
		}
	}

//	printi
	setTitle();

	gtk_main();
}

Frame::~Frame() {
	g_object_unref(m_toolbar);
	WRITE_CONFIG(CONFIG_TAGS, IMAGE_VIEWER_VERSION, (int ) m_mode,
			m_ascendingOrder, m_listIconHeight, m_languageIndex,
			m_warningBeforeDelete, m_deleteOption, m_showPopup, m_oneInstance,
			m_rememberLastOpenDirectory, m_dir);
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
			double ts = totalFileSize / double(1 << 20);
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
			const std::string n = getFileInfo(m_vp[m_pi].m_path, FILEINFO::NAME);
			t += n + SEPARATOR + format("%dx%d", m_pw, m_ph) + SEPARATOR
					+ getLanguageString(LANGUAGE::ZOOM) + " "
					+ std::to_string(int(m_zoom * 100)) + "%" + SEPARATOR
					+ format("%d/%d", m_pi + 1, size());

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
	m_vp.clear();
	const bool d = isDir(p);
	/* by default m_pi=0 - first image in directory
	 * if path is not dir m_pi=0 first image it's ok
	 * if path is file with bad extension then m_pi=0 it's ok
	 */
	m_pi = index;
	m_dir = d ? p : getFileInfo(p, FILEINFO::DIRECTORY);

	totalFileSize = 0;

	//from gtk documentation order of file is arbitrary. Actually it's name (or may be date) ascending order
	if ((di = g_dir_open(m_dir.c_str(), 0, 0))) {
		while ((filename = g_dir_read_name(di))) {
			s = m_dir + G_DIR_SEPARATOR + filename;
			if (!isDir(s) && isSupportedImage(s)) {
				if (!d && s == p) {
					m_pi = size();
				}
				m_vp.push_back(Image(s, m_loadid));
				totalFileSize += m_vp.back().m_size;
			}
		}
	}

	addMonitor(m_dir);

	recountListParameters();

	if (m_vp.empty()) {
		setNoImage();
	} else {
		//if was no image, need to set m_toolbar buttons enabled
		for (int i = 0; i < TOOLBAR_INDEX_SIZE; i++) {
			setButtonState(i,
					i != int(m_mode) + int(TOOLBAR_INDEX::MODE_ZOOM_ANY));
		}

		startThreads();
		setListTopLeftIndexStartValue(); //have to reset
	}

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

void Frame::loadImage() {
	if (!noImage()) {
		GError *err = NULL;
		/* Create pixbuf */
		//here full path
		m_pix = gdk_pixbuf_new_from_file(m_vp[m_pi].m_path.c_str(), &err);

		if (err) {
			println("Error : %s", err->message);
			g_error_free(err);
			setNoImage();
			return;
		}
		m_pw = m_pix.width();
		m_ph = m_pix.height();
		setDefaultZoom();

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

	const GdkRGBA BACKGROUND_COLOR = { 31. / 255, 31. / 255, 31. / 255, 1 };
	const GdkRGBA WHITE_COLOR = { 1, 1, 1, 1 };
	//1920 959
	//printl(width,height)
	gdk_cairo_set_source_rgba(cr, &BACKGROUND_COLOR);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_fill(cr);

	bool windowSizeChanged = m_lastWidth != width || m_lastHeight != height; //size of m_window is changed

	if (windowSizeChanged) {
		m_lastWidth = width;
		m_lastHeight = height;
		recountListParameters();
		setDefaultZoom();
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
			GdkPixbuf *p = o.m_thumbnail;
			if (p) {
				w = gdk_pixbuf_get_width(p);
				h = gdk_pixbuf_get_height(p);
				copy(p, cr, i + (m_listIconWidth - w) / 2,
						j + (m_listIconHeight - h) / 2, w, h, 0, 0);

				drawTextToCairo(cr, getFileInfo(o.m_path, FILEINFO::SHORT_NAME),
						m_filenameFontHeight, filenameFontBold, i, j + 1,
						m_listIconWidth, m_listIconHeight, true, 2, WHITE_COLOR,
						true);
			}
		}

	} else {
		w = m_zoom * m_pw;
		h = m_zoom * m_ph;
//		printl(m_pw,m_ph,w,h,m_zoom)
		m_pixScaled = gdk_pixbuf_scale_simple(m_pix, w, h, GDK_INTERP_BILINEAR);
		destx = (width - w) / 2;
		desty = (height - h) / 2;

		adjust(destx, 0);
		adjust(desty, 0);

		m_aw = MIN(w, width);
		m_ah = MIN(h, height);

		adjustPos();

//		printl(destx, desty, aw, ah, m_posh,
//				m_posv)

		copy(m_pixScaled, cr, destx, desty, m_aw, m_ah, m_posh, m_posv);
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
	m_posh = m_posv = 0;
	redraw();
}

gboolean Frame::keyPress(GdkEventKey *event) {
//	from gtk documentation return value
//	TRUE to stop other handlers from being invoked for the event. FALSE to propagate the event further.
	const int k = event->keyval;
	const guint16 hwkey = event->hardware_keycode;

	//printl(k, GDK_KEY_minus,GDK_KEY_equal,GDK_KEY_KP_Subtract, GDK_KEY_minus);
	bool v[] = { k == GDK_KEY_KP_Add || k == GDK_KEY_equal, k
			== GDK_KEY_KP_Subtract || k == GDK_KEY_minus

	, hwkey == '0', hwkey == '1', hwkey == '2', hwkey == 'L'

	, hwkey == 'E', hwkey == 'R', hwkey == 'T', hwkey == 'H', hwkey == 'V'

	, oneOf(k, GDK_KEY_Home, GDK_KEY_KP_7), oneOf(k, GDK_KEY_Page_Up,
	GDK_KEY_KP_9), oneOf(k, GDK_KEY_Left, GDK_KEY_KP_4), oneOf(k, GDK_KEY_Right,
			GDK_KEY_KP_6), oneOf(k, GDK_KEY_Page_Down,
	GDK_KEY_KP_3), oneOf(k, GDK_KEY_End, GDK_KEY_KP_1)

	//english and russian keyboard layout (& caps lock)
			, hwkey == 'O', oneOf(k, GDK_KEY_Delete,
	GDK_KEY_KP_Decimal), false,		//no hotkey for order
			hwkey == 'F' || oneOf(k, GDK_KEY_F11, GDK_KEY_Escape), false, hwkey
					== 'H' || k == GDK_KEY_F1 };

	assert(TOOLBAR_INDEX_SIZE==std::size(v));

	int i = INDEX_OF(true, v);
	if (i != -1) {
		buttonClicked(i);
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
	auto dx = event->delta_x * WHEEL_MULTIPLIER;
	auto dy = event->delta_y * WHEEL_MULTIPLIER;

	if (m_mode == MODE::LIST) {
		scrollList(event->delta_y * 50);
	} else if (dx != 0 || dy != 0) {
		setPosRedraw(dx, dy, event->time);
	}
}

void Frame::setPosRedraw(double dx, double dy, guint32 time) {
	/* if no horizontal scroll then switch next/previous image
	 * if no vertical scroll then switch next/previous image
	 */
	if (((m_pw <= m_aw && dx != 0 && dy == 0) || (m_ph <= m_ah && dx == 0 && dy != 0))
			&& time > SCROLL_DELAY_MILLISECONDS + m_lastScroll) {
		m_lastScroll = time;
//		switchImage(1, dx > 0 || dy > 0);
		return;
	}
	m_posh += dx;
	m_posv += dy;
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
				oneOf(t, TOOLBAR_INDEX::OPEN, TOOLBAR_INDEX::SETTINGS,
						TOOLBAR_INDEX::HELP));
	}
}

bool Frame::isSupportedImage(const std::string &p) {
	return oneOf(getFileInfo(p, FILEINFO::LOWER_EXTENSION), m_vLowerExtension);
}

void Frame::adjustPos() {
	adjust(m_posh, 0, m_pw*m_zoom - m_aw);
	adjust(m_posv, 0, m_ph*m_zoom - m_ah);
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
	for (i = getFirstListIndex(); m_vp[i].m_thumbnail != nullptr;
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

		if (!o.m_thumbnail) {
			//full path
			p = o.m_path;
			o.m_thumbnail = scaleFit(p, m_listIconWidth, m_listIconHeight);

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
	for (auto &p : m_pThread) {
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
						//printl(int(m_lastNonListMode));
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
		//printl("skipped",o.m_loadid,m_loadid,"i=",i);
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

	i = INDEX_OF(t, ZOOM_INOUT);
	if (i != -1) {
		if (m_mode == MODE::LIST) {
			i = m_listIconHeight + (t == TOOLBAR_INDEX::ZOOM_IN ? 1 : -1) * 5;
			if (i >= MIN_ICON_HEIGHT && i <= MAX_ICON_HEIGHT) {
				stopThreads();
				setIconHeightWidth(i);
				recountListParameters();
				m_filenameFontHeight = 0; //to recount font
				m_loadid++;
				for (auto &o : m_vp) {
					o.free();
					o.m_thumbnail = nullptr;
					o.m_loadid = m_loadid;
				}
				startThreads();
				redraw(false);
			}
		} else {
			const double ZOOM_MULTIPLIER = 1.1;
			m_zoom *=
					t == TOOLBAR_INDEX::ZOOM_IN ?
							ZOOM_MULTIPLIER : 1 / ZOOM_MULTIPLIER;
			//TODO
			redraw();
		}
		return;
	}

	i = INDEX_OF(t, TMODE);
	if (i != -1) {
		MODE m = MODE(i);
		if (m_mode != m) {
			setMode(m);
			if (m_mode == MODE::LIST) {
				redraw();
			} else {
				setDefaultZoom();
				drawImage();
			}
		}
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
		m_ascendingOrder = !m_ascendingOrder;
		if (m_mode == MODE::LIST) {
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

	if (t == TOOLBAR_INDEX::DELETE_FILE) { //m_mode!=MODE::LIST can be called from keyboard
		if (m_mode != MODE::LIST
				&& (!m_warningBeforeDelete
						|| showConfirmation(
								getLanguageString(
										LANGUAGE::DO_YOU_REALLY_WANT_TO_DELETE_THE_IMAGE))
								== GTK_RESPONSE_YES)) {

			//not need full reload
			stopThreads();

			//before g_remove change totalFileSize
			auto &a = m_vp[m_pi];
			totalFileSize -= a.m_size;

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

				//see Image.h for documentation why need to set new m_loadid & set o.thumbmails
				//old indexes for setShowThumbnail() are not valid so make new m_loadid & set o.thumbmails
				m_loadid++;
				for (auto &o : m_vp) {
					o.m_loadid = m_loadid;
				}
				startThreads();
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
				rotatePixbuf(m_pix, m_pw, m_ph, 90 * (i + 1));
			}
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
		} else if (e == LANGUAGE::AUTHOR) {
			w = gtk_label_new(getLanguageString(e, 1).c_str());
			gtk_widget_set_halign(w, GTK_ALIGN_START);
		} else {
			w = m_options[i] = gtk_check_button_new();
			if (e == LANGUAGE::ONE_APPLICATION_INSTANCE) {
				w = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
				gtk_container_add(GTK_CONTAINER(w), m_options[i]);
				gtk_container_add(GTK_CONTAINER(w),
						gtk_label_new(
								getLanguageString(LANGUAGE::REQUIRES_RESTARTING).c_str()));
			}
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

void Frame::setButtonState(int i, bool enable) {
	setButtonImage(i, enable, m_buttonPixbuf[i][enable]);
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
		setButtonState(TOOLBAR_INDEX::DELETE_FILE, m_mode != MODE::LIST);

		setVariableImagesButtonsState();
		updateNavigationButtonsState();
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

void Frame::setVariableImagesButtonsState() {
	setButtonImage(int(TOOLBAR_INDEX::REORDER_FILE), true,
			m_additionalImages[!m_ascendingOrder]);
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
			s = s.substr(0, s.length() - 1);
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
		for (auto e : m_optionsPointer) {
			auto w = m_options[i++];
			*e = GTK_IS_COMBO_BOX(w) ?
					gtk_combo_box_get_active(GTK_COMBO_BOX(w)) :
					gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
		}
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
	int i = 0;
	for (auto e : m_optionsPointer) {
		*e = m_optionsDefalutValue[i++];
	}
}

void Frame::updateOptions() {
	int i = 0;
	for (auto e : m_optionsPointer) {
		auto w = m_options[i++];
		if (GTK_IS_COMBO_BOX(w)) {
			gtk_combo_box_set_active(GTK_COMBO_BOX(w), *e);
		} else {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), *e);
		}
	}
}

void Frame::setPopups() {
	int i = 0;
	for (auto e : m_button) {
		if (m_showPopup && gtk_widget_get_sensitive(e)) {
			gtk_widget_set_tooltip_text(e,
					getLanguageString(LANGUAGE::LTOOLTIP1, i).c_str());
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
	load(m_dir);
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

void Frame::setDefaultZoom() {
	if (m_mode == MODE::NORMAL) {
		m_zoom = 1;
	} else if (m_mode == MODE::FIT) {
		m_zoom = MIN(m_lastWidth / double(m_pw), m_lastHeight / double(m_ph));
		//m_zoom=MIN(double(m_pw)/m_lastWidth,double(m_ph)/m_lastHeight);
//		printl(m_zoom)
	}
}
