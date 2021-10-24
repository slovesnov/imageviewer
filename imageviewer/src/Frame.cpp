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
#include <glib/gstdio.h>//g_remove
#include "Frame.h"
#include "help.h"

Frame *frame;
VString Frame::sLowerExtension;

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


const char *MONTH[] = { "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug",
		"sep", "oct", "nov", "dec" };
//const char *MONTH[] = { "January", "February", "March", "April", "May", "June",
//		"July", "August", "September", "October", "November", "December" };
const char LOADING[] ="loading...";
const int GOTO_BEGIN=INT_MIN;
const int GOTO_END=INT_MAX;
const int MIN_ICON_HEIGHT=32;
const int MAX_ICON_HEIGHT=200;

const char *TOOLBAR_IMAGES[] = { "magnifier_zoom_in.png",
		"magnifier_zoom_out.png", "application_view_columns.png",

		"arrow_rotate_anticlockwise.png",
		"arrow_refresh.png",
		"arrow_rotate_clockwise.png",

		"control_start_blue.png",
		"control_rewind_blue.png",
		"previous.png",
		"control_play_blue.png",
		"control_fastforward_blue.png",
		"control_end_blue.png",

		"folder.png","cross.png",

		"help.png"
};

const char *ADDITIONAL_IMAGES[] = {
		"sort_ascending.png", "sort_descending.png",
		"arrow_in.png", "arrow_out.png" };

const TOOLBAR_INDEX ROTATE[] = { TOOLBAR_INDEX::ROTATE_CLOCKWISE,
		TOOLBAR_INDEX::ROTATE_180, TOOLBAR_INDEX::ROTATE_ANTICLOCKWISE };

const TOOLBAR_INDEX NAVIGATION[] = { TOOLBAR_INDEX::HOME,
		TOOLBAR_INDEX::PAGE_UP, TOOLBAR_INDEX::PREVIOUS, TOOLBAR_INDEX::NEXT,
		TOOLBAR_INDEX::PAGE_DOWN, TOOLBAR_INDEX::END };

const TOOLBAR_INDEX TMODE[] = { TOOLBAR_INDEX::MODE_NORMAL,
		TOOLBAR_INDEX::MODE_FIT, TOOLBAR_INDEX::MODE_LIST };

const std::string CONFIG_TAGS[] = { "mode", "order", "list icon height" };
const std::string SEPARATOR = "          ";

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

static void open_files(GtkWidget*, const char *data) {
	frame->load(data);
}

static gboolean set_show_thumbnail_thread(gpointer data) {
	frame->setShowThumbnail(GP2INT(data));
	return G_SOURCE_REMOVE;
}

Frame::Frame(GtkApplication *application, std::string const path) {
	frame = this;

	int i,j;
	loadid=-1;

	setlocale(LC_NUMERIC, "C"); //dot interpret as decimal separator for format(... , scale)
	pThread.resize(getNumberOfCores());
	for (auto& p:pThread) {
		p=nullptr;
	}
	g_mutex_init(&mutex);

	lastWidth = lastHeight = posh = posv = 0;
	loadingFontHeight=0;

	//begin readConfig
	listAscendingOrder = true;
	lastNonListMode = mode = MODE::NORMAL;
	//drawing area height 959,so got 10 rows
	//4*95/3 = 126, 1920/126=15.23 so got 15 columns
	setIconHeightWidth(95);

	MapStringString m;
	MapStringString::iterator it;
	if (loadConfig(m)) {
		i = 0;
		for (auto t : CONFIG_TAGS) {
			if ((it = m.find(t)) != m.end()) {
				if (!parseString(it->second, j)) {
					printl("error");
					break;
				}

				if (i == 0) {
					if (j < 0 || j > 2) {
						printl("error");
						break;
					}
					lastNonListMode = mode = MODE(j);
				} else if(i==1){
					if (j < 0 || j > 1) {
						printl("error");
						break;
					}
					listAscendingOrder = j == 1;
				}
				else{
					if (j < MIN_ICON_HEIGHT || j > MAX_ICON_HEIGHT) {
						printl("error");
						break;
					}
					setIconHeightWidth(j);
				}

			}
			i++;
		}
	}
	//end readConfig

	if (sLowerExtension.empty()) {
		sLowerExtension = { "jpg" }; //in list only "jpeg" not jpg
		GSList *formats;
		GSList *elem;
		GdkPixbufFormat *pf;
		formats = gdk_pixbuf_get_formats();
		for (elem = formats; elem; elem = elem->next) {
			pf = (GdkPixbufFormat*) elem->data;
			auto p = gdk_pixbuf_format_get_name(pf);
			sLowerExtension.push_back(p);
		}
		g_slist_free(formats);
	}

	window = gtk_application_window_new(GTK_APPLICATION(application));
	area = gtk_drawing_area_new();
	//auto box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	auto toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_widget_set_halign(toolbar, GTK_ALIGN_CENTER);


	//before setButtonState
	for(auto a:ADDITIONAL_IMAGES){
		addi.push_back(pixbuf(a));
	}

	i=0;
	for(auto a:TOOLBAR_IMAGES){
		auto b =button[i]= gtk_button_new();

		buttonPixbuf[i][1]=pixbuf(a);

		buttonPixbuf[i][0] = gdk_pixbuf_copy(buttonPixbuf[i][1]);
		gdk_pixbuf_saturate_and_pixelate(buttonPixbuf[i][1], buttonPixbuf[i][0],
				.3f, false);

		setButtonState(i, true);

		g_signal_connect(G_OBJECT(b), "clicked", G_CALLBACK(button_clicked), GP(i));
		gtk_box_pack_start(GTK_BOX(toolbar), b, FALSE, FALSE, 0);
		if(oneOf(i,3,6,12)){
			gtk_widget_set_margin_start(b, 15);
		}
		i++;
	}

	auto box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start(GTK_BOX(box), area, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(box), toolbar);

	gtk_container_add(GTK_CONTAINER(window), box);

	gtk_window_maximize(GTK_WINDOW(window));
	gtk_widget_show_all(window);

	gtk_widget_add_events(window,
			GDK_SCROLL_MASK | GDK_KEY_PRESS_MASK | GDK_BUTTON_PRESS_MASK);

	g_signal_connect(window, "scroll-event", G_CALLBACK(mouse_scroll), NULL);
	g_signal_connect(window, "key-press-event", G_CALLBACK (key_press), NULL);

	g_signal_connect(window, "button-press-event", G_CALLBACK(button_press),
			NULL);

	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	g_signal_new(OPEN_FILE_SIGNAL_NAME, G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST, 0,
	NULL, NULL, g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1,
	G_TYPE_POINTER);

	g_signal_connect(window, OPEN_FILE_SIGNAL_NAME, G_CALLBACK(open_files),
			NULL);

	g_signal_connect(G_OBJECT (area), "draw", G_CALLBACK (draw_callback), NULL);

	setDragDrop(window);

	pix = pixs = 0;
	lastScroll = 0;

	if (path.empty()) {
		setNoImage();
	} else {
		load(path,0,true); //after area is created
		if(mode==MODE::LIST){//after vp is loading in load() function
			setListTopLeftIndexStartValue();
		}
	}

	setTitle();

	gtk_main();
}

Frame::~Frame() {
	int i, j;

	WRITE_CONFIG(CONFIG_TAGS,  (int) lastNonListMode,listAscendingOrder,listIconHeight );

	stopThreads();
	free();
	g_mutex_clear(&mutex);

	for (i = 0; i < SIZEI(buttonPixbuf); i++) {
		for (j = 0; j < SIZEI(*buttonPixbuf); j++) {
			g_object_unref(buttonPixbuf[i][j]);
		}
	}
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
	std::string s,t;
	int i;
	if (noImage()) {
		t = getApplicationName();
	} else {
		t = dir+SEPARATOR ;
		const int sz=size();
		if(mode==MODE::LIST){
			double ts=totalFileSize/double(1<<20);
			/* format("%d-%d/%d" a-b/size
			 * a=listTopLeftIndex + 1
			 * if LIST_ASCENDING_ORDER=1 a<b & b-a+1=listxy => b=a-1+listxy=listTopLeftIndex + listxy
			 * if LIST_ASCENDING_ORDER=0 a>b & a-b+1=listxy => b=a+1-listxy=listTopLeftIndex +2- listxy
			 */
			t += format("%d-%d/%d", listTopLeftIndex + 1,
					listAscendingOrder ?
							MIN(listTopLeftIndex + listxy, sz) :
							MAX(listTopLeftIndex + 2 - listxy, 1), sz)
					+ SEPARATOR
					+ format("total %.2lfMb avg ", ts);
			double avg=ts / sz;//mb
			const std::string P[]={"Mb","Kb","b"};
			for(i=0;i<3;i++,avg*=1024){
				s=format("%.2lf", avg);
				if(s!="0.00" || i==2){
					t+=s+P[i];
					break;
				}
			}
		}
		else{
			const std::string n = getFileInfo(vp[pi].path, FILEINFO::NAME);
			t += n + SEPARATOR + format("%dx%d", pw, ph) + SEPARATOR
					+ "scale"
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
						+ (i >= 0 && i < 12 ? MONTH[i] : "???") + n.substr(4, 4);
				for (i = 0; i < 3; i++) {
					t += (i ? ':' : ' ') + n.substr(13 + i * 2, 2);
				}
			}
			g_regex_unref(r);
		}
	}

	gtk_window_set_title(GTK_WINDOW(window), t.c_str());
}

void Frame::load(const std::string &p, int index,bool start) {
	std::string s;
	GDir *di;
	const gchar *filename;

	stopThreads();//endThreads=0 after
	loadid++;
	vp.clear();
	const bool d = isDir(p);
	/* by default pi=0 - first image in directory
	 * if path is not dir pi=0 first image it's ok
	 * if path is file with bad extension then pi=0 it's ok
	 */
	pi = index;
	dir = d ? p : getFileInfo(p, FILEINFO::DIRECTORY);

	totalFileSize=0;

	//from gtk documentation order of file is arbitrary. Actually it's name (or may be date) ascending order
	if ((di = g_dir_open(dir.c_str(), 0, 0))) {
		while ((filename = g_dir_read_name(di))) {
			s = dir + G_DIR_SEPARATOR + filename;
			if (!isDir(s) && isSupportedImage(s)) {
				if (!d && s == p) {
					pi = size();
				}
				vp.push_back(Image(s,loadid));
				totalFileSize += vp.back().size;
			}
		}
	}

	recountListParameters();

	if (vp.empty()) {
		setNoImage();
	}
	else{
		//if was no image, need to set toolbar buttons enabled
		for(int i=0;i<TOOLBAR_INDEX_SIZE;i++){
			setButtonState(i,i!=int(mode));
		}

		startThreads();
		setListTopLeftIndexStartValue();//have to reset
	}

	if(start){
	#if	START_MODE==-1
		setMode(mode,true);
	#else
		setMode(INITIAL_MODE,true);
	#endif
	}
	else{
		if(!d && mode==MODE::LIST){
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
		free();
		//here full path
		pix = gdk_pixbuf_new_from_file(vp[pi].path.c_str(), &err);

		if (err) {
			println("Error : %s", err->message);
			g_error_free(err);
			setNoImage();
			return;
		}
		getPixbufWH(pix,pw,ph);

		if (mode == MODE::FIT && lastWidth > 0) {
			setSmallImage();
		}

		setNavigationButtonsState(pi!= 0, pi!=size()-1);
	}

	drawImage();

}

void Frame::draw(cairo_t *cr, GtkWidget *widget) {
	if (noImage()) {
		return;
	}

	int i,j,k,l,width, height, destx, desty, w, h;
	const int sz=size();

	width = gtk_widget_get_allocated_width(widget);
	height = gtk_widget_get_allocated_height(widget);

	//1920 959
	//printl(width,height)

	bool windowSizeChanged = lastWidth != width || lastHeight != height; //size of window is changed

	if (windowSizeChanged) {
		lastWidth = width;
		lastHeight = height;
		recountListParameters();
	}

	if(!loadingFontHeight){
		loadingFontHeight=countFontMaxHeight(LOADING, false,cr);
		filenameFontHeight=countFontMaxHeight("IMG_20211004_093339", filenameFontBold,cr);

		//printl(fontHeight)
		if (mode == MODE::LIST) {
			setTitle();//now lisx, listy counted so need to set title
		}
	}

	const GdkRGBA BLACK_COLOR = { 0, 0, 0, 1 };
	const GdkRGBA RED_COLOR = { 1, 0, 0, 1 };

	if (mode == MODE::LIST) {
//		printl(listTopLeftIndex,listAscendingOrder)
		for (k = listTopLeftIndex, l = 0;
				((listAscendingOrder && k < sz)
						|| (!listAscendingOrder && k >= 0)) && l < listxy;
				k += listAscendingOrder ? 1 : -1, l++) {
			i=l%listx*listIconWidth+listdx;
			j=l/listx*listIconHeight+listdy;
			auto& o=vp[k];
			GdkPixbuf*p=o.thumbnail;
			if(p){
				getPixbufWH(p,w,h);
				copy(p, cr, i+(listIconWidth-w)/2, j+(listIconHeight-h)/2, w, h, 0,0);

				drawTextToCairo(cr, getFileInfo(o.path, FILEINFO::SHORT_NAME),
						filenameFontHeight, filenameFontBold, i, j, listIconWidth,
						listIconHeight, true, 2, RED_COLOR);
			}
			else{
				drawTextToCairo(cr, LOADING,loadingFontHeight,false
						, i,j,listIconWidth,listIconHeight,
						true, 2,BLACK_COLOR);
			}
		}


	}
	else{
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

		adjust(destx,0);
		adjust(desty,0);

		aw = MIN(w, width);
		ah = MIN(h, height);

		if (windowSizeChanged) {
			adjustPos();
			if (mode == MODE::FIT) {
				setTitle();		//scale changed
			}
		}

		copy(mode == MODE::FIT ? pixs : pix, cr, destx, desty, aw, ah, posh, posv);
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
	const guint16 hwkey=event->hardware_keycode;

	const bool plus=k == GDK_KEY_KP_Add;
	const bool minus=k == GDK_KEY_KP_Subtract;
	int i=indexOfV(true, plus, minus, hwkey== 'L',
			hwkey == 'E',hwkey == 'R',hwkey == 'T',

			oneOf(k, GDK_KEY_Home, GDK_KEY_KP_7),
			oneOf(k, GDK_KEY_Page_Up, GDK_KEY_KP_9),
			oneOf(k, GDK_KEY_Left, GDK_KEY_KP_4),
			oneOf(k, GDK_KEY_Right, GDK_KEY_KP_6),
			oneOf(k, GDK_KEY_Page_Down, GDK_KEY_KP_3),
			oneOf(k, GDK_KEY_End, GDK_KEY_KP_1),

			//english and russian keyboard layout (& caps lock)
			hwkey == 'O',
			oneOf(k, GDK_KEY_Delete, GDK_KEY_KP_Decimal),
			hwkey == 'H' || k==GDK_KEY_F1
	);

	if(i!=-1){
		if(mode==MODE::LIST && (plus || minus)){
			buttonClicked(plus ? LIST_ZOOM_IN:LIST_ZOOM_OUT  );
		}
		else{
			buttonClicked(i);
		}
	}
	return i!=-1;
}

void Frame::free() {
	::free(pix);
	::free(pixs);
}

void Frame::setDragDrop(GtkWidget *widget) {
	gtk_drag_dest_set(widget, GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
	gtk_drag_dest_add_uri_targets(widget);
	g_signal_connect(widget, "drag-data-received",
			G_CALLBACK(drag_and_drop_received), 0);
}

void Frame::scroll(GdkEventScroll *event) {
	//printl(event->delta_x,event->delta_y,event->time,event->type)
	auto x=event->delta_x* WHEEL_MULTIPLIER;
	auto y=event->delta_y* WHEEL_MULTIPLIER;

	if (mode == MODE::NORMAL && (x != 0 || y != 0)) {
		setPosRedraw(x , y, event->time);
	}
	else if(mode==MODE::LIST){
		scrollList(event->delta_y* 50);
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

	for(int i=0;i<TOOLBAR_INDEX_SIZE;i++){
		TOOLBAR_INDEX t=TOOLBAR_INDEX(i);
		setButtonState(i,t==TOOLBAR_INDEX::OPEN || t==TOOLBAR_INDEX::HELP);
	}
}

bool Frame::isSupportedImage(const std::string &p) {
	return oneOf(getFileInfo(p, FILEINFO::LOWER_EXTENSION),sLowerExtension);
}

void Frame::adjustPos() {
	adjust(posh,0, pw - aw);
	adjust(posv,0, ph - ah);
}

void Frame::openDirectory() {
	std::string s = filechooser(window);
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
	scaleFit(pix, pixs, lastWidth, lastHeight, pws, phs);
	scale = double(pws) / pw;
}

void Frame::rotatePixbuf(GdkPixbuf *&q, int &w, int &h, int angle) {
	//angle 90 or 180 or 270
	GdkPixbuf *p = q;
	q = gdk_pixbuf_rotate_simple(p,
			angle == 180 ?
					GDK_PIXBUF_ROTATE_UPSIDEDOWN :
					(angle == 90 ?
							GDK_PIXBUF_ROTATE_CLOCKWISE :
							GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE));
	g_object_unref(p);
	if (angle != 180) {
		std::swap(w, h);
	}
}

int Frame::showConfirmation(const std::string text) {
	GtkWidget *dialog;
	dialog = gtk_message_dialog_new(GTK_WINDOW(window),
			GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION,
			GTK_BUTTONS_YES_NO, text.c_str());
	gtk_window_set_title(GTK_WINDOW(dialog), "Question");
	auto r = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

//files are bigger
//	for(std::string ext:{"jpeg","png"}){
//		auto path=format("C:\\slovesno\\1.%s",ext.c_str());
//		if (ext == "jpeg") {
//			gdk_pixbuf_save(pix, path.c_str(), ext.c_str(), NULL, "quality",
//					"100", NULL);
//		}
//		else {
//			gdk_pixbuf_save(pix, path.c_str(), ext.c_str(), NULL, NULL);
//		}
//	}

	return r;
}

void Frame::startThreads() {
	int i;

	for (i = getFirstListIndex(); vp[i].thumbnail != nullptr;
			i += listAscendingOrder ? 1 : -1) {
		if ((listAscendingOrder && i >= size())
				|| (!listAscendingOrder && i < 0)) {
			break;
		}
	}
	threadNumber = i;

	i = 0;
	for (auto &p : pThread) {
		p = g_thread_new("", thumbnail_thread, GP(i++));
	}
}

void Frame::thumbnailThread(int n) {
	int w, h, v, max=0;
	GdkPixbuf*p;

//#define SHOW_THREAD_TIME

#ifdef SHOW_THREAD_TIME
	clock_t start= clock();
	std::string s;
	bool stopped=false;
#endif

	if(listAscendingOrder){
		g_mutex_lock(&mutex);
		max=size();
		g_mutex_unlock(&mutex);
	}

	while ((listAscendingOrder && (v = threadNumber++) < max)
			|| (!listAscendingOrder && (v = threadNumber--) >= 0)) {

		if(g_atomic_int_get (&endThreads)){
#ifdef SHOW_THREAD_TIME
			stopped = true;
#endif
			break;
		}
		auto & o=vp[v];

		if(!o.t){
			//full path
			p = gdk_pixbuf_new_from_file(o.path.c_str(), NULL);
			scaleFit(p, o.t, listIconWidth, listIconHeight, w, h);
			g_object_unref(p);

			gdk_threads_add_idle(set_show_thumbnail_thread, GP(v));
		}
		//s+=" "+std::to_string(v);
	}

#ifdef SHOW_THREAD_TIME
	println("t%d%s %.2lf",n,stopped?" user stop":"",((double) (clock() - start)) / CLOCKS_PER_SEC)
#endif
}

void Frame::stopThreads() {
	g_atomic_int_set(&endThreads, 1);

	//	clock_t begin=clock();
	for (auto& p:pThread) {
		if(p){//1st time p==nullptr
			g_thread_join(p);
			p=nullptr;//allow call stopThreads many times
		}
	}
	//	println("%.3lf",double(clock()-begin)/CLOCKS_PER_SEC);

	g_atomic_int_set(&endThreads, 0);


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
		const bool left=event->button == 1;
		if (left || event->button == 2) { //left/middle mouse button
			if(mode==MODE::LIST){
				if(left){// left button check click on image
					int cx=event->x;
					int cy=event->y;
					if(cx>=listdx && cx<lastWidth-listdx && cy>=listdy && cy<lastHeight-listdy){
						pi = ((cx-listdx)/listIconWidth+(cy-listdy)/listIconHeight*listx)*(listAscendingOrder?1:-1)+listTopLeftIndex;
						setMode(MODE::FIT);
						loadImage();
					}
				}
			}
			else{ //next/previous image
				//buttonClicked(left ? TOOLBAR_INDEX::NEXT : TOOLBAR_INDEX::PREVIOUS);
			}
		} else if (event->button == 3) { //on right mouse open file/directory
			buttonClicked(TOOLBAR_INDEX::OPEN);
		}
	}
}

void Frame::setShowThumbnail(int i) {
	auto& o=vp[i];
	if(o.loadid!=loadid){
		//printl("skipped",o.loadid,loadid,"i=",i);
		return;
	}
	o.thumbnail=o.t;
	if (mode == MODE::LIST
			&& ((listAscendingOrder && i >= listTopLeftIndex
					&& i < listTopLeftIndex + listxy)
					|| (!listAscendingOrder && i <= listTopLeftIndex && i >= 0))) {
		redraw(false);
	}
}

void Frame::setListTopLeftIndexStartValue() {
	listTopLeftIndex = getFirstListIndex();
	listTopLeftIndexChanged();
}

void Frame::scrollList(int v) {
	if(mode==MODE::LIST){
		int i,j,min,max;
		if(listxy>=size()){
			//min=max=0;
			return;
		}
		getListMinMaxIndex(min, max);
		if(v==GOTO_BEGIN){
			i=listAscendingOrder?min:max;
		}
		else if(v==GOTO_END){
			i=listAscendingOrder?max:min;
		}
		else{
			i=listTopLeftIndex+v*(listAscendingOrder?1:-1)*listx;
		}

		j=listTopLeftIndex;
		listTopLeftIndex= i;
		adjust(listTopLeftIndex,min,max);
		if(j!=listTopLeftIndex){
			listTopLeftIndexChanged();
		}
	}
}

void Frame::buttonClicked(TOOLBAR_INDEX t) {
	int i;
	if(t==TOOLBAR_INDEX::OPEN){
		openDirectory();
		return;
	}
	if(t==TOOLBAR_INDEX::HELP){
		showHelp();
		return;
	}

	if(noImage()){
		return;
	}

	i=INDEX_OF(t,TMODE);
	if(i!=-1){
		MODE m=MODE(i);
		if(mode!=m){
			setMode(m);
			if (mode == MODE::LIST) {
				setListTopLeftIndexStartValue();
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

	i=INDEX_OF(t,NAVIGATION);
	if(i!=-1){
		if(mode==MODE::LIST){
			int a[]={GOTO_BEGIN,-listy,-1,1,listy,GOTO_END};
			scrollList(a[i]);
		}
		else{
			if (t==TOOLBAR_INDEX::HOME || t==TOOLBAR_INDEX::END) {
				pi = t==TOOLBAR_INDEX::HOME ? 0 : size() - 1;
				loadImage();
			} else {
				switchImage(t==TOOLBAR_INDEX::PAGE_UP || t==TOOLBAR_INDEX::PAGE_DOWN ? sqrt(size()) : 1,
						t==TOOLBAR_INDEX::NEXT || t==TOOLBAR_INDEX::PAGE_DOWN);
			}
		}
		return;
	}

	if(t==TOOLBAR_INDEX::DELETE){
		if(mode==MODE::LIST){
			//ORDER KEY
			stopThreads();
			listAscendingOrder=!listAscendingOrder;
			listTopLeftIndex = getFirstListIndex();
			startThreads();
			setVariableImagesButtonsState();
			redraw();
		}
		else{
			if (showConfirmation("Do you really want to delete current image?")
					== GTK_RESPONSE_YES) {
				//not need full reload

				stopThreads();

				//before g_remove change totalFileSize
				auto&a=vp[pi];
				totalFileSize-=a.size;
				g_remove(a.path.c_str());

				int sz=size();
				vp.erase(vp.begin()+pi);
				if(sz==1){
					setNoImage();
				}
				else{
					if(pi == sz - 1){//cann't call size() here because vp.size() is changed after erase()
						pi=0;
					}
					recountListParameters();
					loadImage();

					//see Image.h for documentation why need to set new loadid & set o.thumbmails
					//old indexes for setShowThumbnail() are not valid so make new loadid & set o.thumbmails
					loadid++;
					for(auto&o:vp){
						if(!o.thumbnail && o.t){
							o.thumbnail=o.t;
						}
						o.loadid=loadid;
					}
					startThreads();
				}
			}
		}
		return;
	}

	i=INDEX_OF(t,ROTATE);
	if(i!=-1){
		if(mode==MODE::LIST){
			if(t==LIST_ZOOM_IN || t==LIST_ZOOM_OUT){
				i=listIconHeight+(t==LIST_ZOOM_IN?1:-1)*5;
				if(i>=MIN_ICON_HEIGHT && i<=MAX_ICON_HEIGHT){
					stopThreads();
					setIconHeightWidth(i);
					recountListParameters();
					loadingFontHeight=0;//to recount font
					loadid++;
					for(auto&o:vp){
						o.free();
						o.thumbnail=o.t=nullptr;
						o.loadid=loadid;
					}
					startThreads();
					redraw(false);
				}
			}
		}
		else{
			rotatePixbuf(pix, pw, ph, 90 * (i+1));
			//small image angle should match with big image, so always call setSmallImage
			setSmallImage();
			drawImage();
		}
		return;
	}
}

void Frame::showHelp() {
	const char t[] =R"(if drag & drop file -> view DIRECTORY which includes dropped file, starts from dropped file if file is supported
if dropped file isn't supported then first supported image in DIRECTORY
if drag & drop DIRECTORY -> view DIRECTORY which includes dropped file, starts from first supported file in DIRECTORY

(GDK_KEY_Right, GDK_KEY_KP_6) / (GDK_KEY_Left, GDK_KEY_KP_4) {
	LIST MODE scroll one row
	FIT/NORMAL MODES next/previous image in DIRECTORY
}
(GDK_KEY_Page_Down, GDK_KEY_KP_3) / (GDK_KEY_Page_Up, GDK_KEY_KP_9) {
	LIST MODE scroll screen
	FIT/NORMAL MODES +/-sqrt (number of images in DIRECTORY)
}
(GDK_KEY_Home, GDK_KEY_KP_7) / (GDK_KEY_End, GDK_KEY_KP_1) {
	ALL MODES go to first/last image in DIRECTORY
}

mouse_right, O { 
	open file/folder to select folder or file. Use "select" button in dialog to open file or folder, "open" button works only with files
}
vertical mouse scroll {
	LIST MODE scroll one row
	FIT MODE ignored
	NORMAL MODES scroll image up/down if image higher than window
}
horizontal mouse scroll {
	LIST, FIT MODES ignored
	NORMAL MODES scroll image left/right if image wider than window
}

GDK_KEY_KP_Add (+ on extended keyboard) - switch to NORMAL MODE
GDK_KEY_KP_Subtract (- on extended keyboard) - switch to FIT MODE
l - switch to LIST MODE

e - rotate image by 270 degrees
r - rotate image by 180 degrees
t - rotate image by 90 degrees

GDK_KEY_Delete, GDK_KEY_KP_Decimal {
	NORMAL, FIT MODES - remove current image with confirm dialog and goes to next image in DIRECTORY
	LIST MODE - change ascending/descending list order
}

h, F1 - show help)";

/*
	vertical mouse scroll {
		LIST MODE scroll one row
		FIT MODE ignored
		NORMAL MODES scroll image up/down if image higher than window or switch to next/previous image if image height is less than window
	}
	horizontal mouse scroll {
		LIST, FIT MODES ignored
		NORMAL MODES scroll image left/right if image wider than window or switch to next/previous image if image is narrower than window
	}
*/

	auto d=gtk_dialog_new();
	gtk_window_set_modal(GTK_WINDOW(d), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(d),
			GTK_WINDOW(window));

	auto s= getApplicationName()+SEPARATOR+getBuildVersionString(false);
	s+=SEPARATOR+"file size "+toString(getApplicationFileSize(),',');

	gtk_window_set_title(GTK_WINDOW(d), s.c_str());
	gtk_window_set_resizable(GTK_WINDOW(d), 1);

	auto l=gtk_label_new(t);
	auto ca= gtk_dialog_get_content_area(GTK_DIALOG(d));

	gtk_container_add(GTK_CONTAINER(ca),l);

	gtk_widget_show_all(d);

	gtk_dialog_run(GTK_DIALOG(d));
	gtk_widget_destroy(d);
}

void Frame::recountListParameters() {
	const int sz=size();
	listx=MAX(1,lastWidth/listIconWidth);
	listy=MAX(1,lastHeight/listIconHeight);
	//MIN(listx,sz) center horizontally if images less than listx
	listdx=(lastWidth-MIN(listx,sz)*listIconWidth)/2;
	//MIN(listx,sz) center vertically if images less than listx*listy
	int i;
	i=sz/listx;
	if(sz%listx!=0){
		i++;
	}
	listdy=(lastHeight-MIN(listy, i)*listIconHeight)/2;
	listxy=listx*listy;
	//printl(sz,listx,listy)
}

void Frame::setButtonState(int i, bool enable) {
	setButtonImage(i, enable, buttonPixbuf[i][enable]);
}

void Frame::setMode(MODE m,bool start) {
	if(mode!=m || start){
		mode=m;
		if(mode!=MODE::LIST){
			lastNonListMode=mode;
		}
		int i=0;
		for(auto a:TMODE){
			setButtonState(a,i++!=int(m));
		}

		//other two buttons wet in setVariableImagesButtonsState()
		setButtonState(TOOLBAR_INDEX::ROTATE_ANTICLOCKWISE,mode!=MODE::LIST);

		setVariableImagesButtonsState();
	}
}

void Frame::listTopLeftIndexChanged() {
	int min,max;
	getListMinMaxIndex(min,max);
	if(listAscendingOrder){
		//setNavigationButtonsState(listTopLeftIndex!=0 , listTopLeftIndex<size()-listxy );
		setNavigationButtonsState(listTopLeftIndex>min , listTopLeftIndex<max );
	}
	else{
		setNavigationButtonsState(listTopLeftIndex<max , listTopLeftIndex>min );
	}
	redraw();
}

void Frame::setNavigationButtonsState(bool c1,bool c2) {
	int i=0;
	for(auto a:NAVIGATION){
		setButtonState(a, i<3 ? c1:c2 );
		i++;
	}
}

void Frame::getListMinMaxIndex(int &min, int &max) {
	int j,sz=size();
	if(listAscendingOrder){
		j=sz;
		if(sz%listx!=0){
			j+=listx-sz%listx;
		}
		min=0;
		max=j-listxy;
	}
	else{
		j=0;
		if(sz%listx!=0){
			j-=listx-sz%listx;
		}
		min=j+listxy-1;
		max=sz-1;
	}
}

int Frame::getFirstListIndex() {
	return listAscendingOrder?0:size()-1;
}

void Frame::setButtonImage(int i, bool enable, GdkPixbuf *p) {
	auto b=button[i];
	gtk_widget_set_sensitive (b, enable);
	gtk_button_set_image(GTK_BUTTON(b), gtk_image_new_from_pixbuf(p));
}

void Frame::setVariableImagesButtonsState() {
	int i=0;
	TOOLBAR_INDEX t[] = { ASCENDING_DESCENDING, LIST_ZOOM_OUT, LIST_ZOOM_IN };
	if(mode==MODE::LIST){
		int b[]={listAscendingOrder,2,3};
		i=0;
		for(auto a:t){
			setButtonImage(int(a), true, addi[b[i]]);
			i++;
		}
	}
	else{
		bool b[]={mode!=MODE::LIST,true,true};
		i=0;
		for(auto a:t){
			setButtonState(a, b[i]);
			i++;
		}
	}
}

void Frame::redraw(bool withTitle) {
	if(withTitle){
		setTitle();
	}
	gtk_widget_queue_draw(area);
	//gtk_widget_queue_draw_area(widget, x, y, width, height)
}

int Frame::countFontMaxHeight(const std::string &s, bool bold,cairo_t *cr) {
	int w,h;
	for(int i=1;;i++){
		getTextExtents(s, i,false, w, h, cr);
		if(w>=listIconWidth || h>=listIconHeight){
			return i-1;
		}
	}
	assert(0);
	return 16;
}

void Frame::setIconHeightWidth(int height) {
	listIconHeight=height;
	listIconWidth=4*height/3;
}
