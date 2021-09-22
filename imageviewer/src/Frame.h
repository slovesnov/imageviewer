/*
 * Frame.h
 *
 *  Created on: 03.06.2021
 *      Author: alexey slovesnov
 * copyright(c/c++): 2014-doomsday
 *           E-mail: slovesnov@yandex.ru
 *         homepage: slovesnov.users.sourceforge.net
 */

#ifndef FRAME_H_
#define FRAME_H_

#include "help.h"
#include "Image.h"

typedef std::vector<std::string> VString;

enum class MODE {
	NORMAL, FIT, LIST
};

//Note DELETE defined in winnt.h
enum class TOOLBAR_INDEX{
	MODE_NORMAL,MODE_FIT,MODE_LIST,
	ROTATE_ANTICLOCKWISE,ROTATE_180,ROTATE_CLOCKWISE,
	HOME,PAGE_UP,PREVIOUS,NEXT,PAGE_DOWN,END,
	OPEN,REMOVE,HELP
	,SIZE
};
const int TOOLBAR_INDEX_SIZE=int(TOOLBAR_INDEX::SIZE);

const gchar OPEN_FILE_SIGNAL_NAME[] = "imageviewer_open_file";

//all system constants
const bool ONE_INSTANCE = true; //if oneInstance=true then not open new imageviewer window if click on image
const int WHEEL_MULTIPLIER = 80;
const int SCROLL_DELAY_MILLISECONDS = 500;
const int ICON_HEIGHT=95;//drawing area height 959,so got 10 rows
const int ICON_WIDTH=4*ICON_HEIGHT/3;//4*95/3 = 126, 1920/126=15.23 so got 15 columns
const bool LIST_ASCENDING_ORDER=1;

class Frame {
public:
	GtkWidget *window, *area,*button[TOOLBAR_INDEX_SIZE];
	int lastWidth, lastHeight;
	int posh, posv;
	GdkPixbuf *pix, *pixs;
	int pw, ph, aw, ah, pws, phs;
	double scale;
	int pi;
	MODE mode;
	VImage vp;
	//lower cased allowable pixbuf formats
	static VString sLowerExtension;
	std::string dir;
	guint32 lastScroll;
	std::vector<GThread*>pThread;
	GMutex mutex;
	int threadNumber;
	gint endThreads;//already have function stopThreads
	int fontHeight,listTopLeftIndex,totalFileSize;
	int listx,listy,listdx,listdy,listxy;
	static std::string workPath;
	GdkPixbuf* buttonPixbuf[TOOLBAR_INDEX_SIZE][2];

	Frame(GtkApplication *application, std::string const path = "",const char* apppath="");
	virtual ~Frame();

	void setTitle();
	void load(const std::string &p, int index = 0,bool start=false); //supports dir & file
	void loadImage();
	void drawImage();
	void draw(cairo_t *cr, GtkWidget *widget);
	gboolean keyPress(GdkEventKey *event);
	void free();
	void setDragDrop(GtkWidget *widget);
	void openUris(char **uris);
	void scroll(GdkEventScroll *event);
	void setPosRedraw(double dx, double dy, guint32 time = 0);
	bool noImage();
	void setNoImage();
	static bool isSupportedImage(std::string const &p);
	void adjustPos();

	void openDirectory();
	void switchImage(int v, bool add);

	void setSmallImage();
	void rotatePixbuf(GdkPixbuf *&p, int &w, int &h, int angle);

	int showConfirmation(const std::string text);
	void loadThumbnails();
	void thumbnailThread(int n);
	void stopThreads();
	void buttonPress(GdkEventButton *event);
	void setShowThumbnail(int i);
	void setListTopLeftIndexStartValue();
	void scrollList(int v);

	static std::string imageString(const char *name) {
		return workPath+format("../images/%s", name);
	}

//	static GtkWidget* image(const char *name) {
//		return gtk_image_new_from_file(imageString(name).c_str());
//	}

	void buttonClicked(TOOLBAR_INDEX t);
	void buttonClicked(int t){
		buttonClicked(TOOLBAR_INDEX(t));
	}
	void showHelp();
	void recountListParameters();
	void setButtonState(int i,bool enable);
	void setButtonState(TOOLBAR_INDEX i,bool enable){
		setButtonState(int(i),enable);
	}
	void setMode(MODE m,bool start=false);
	int size(){
		return vp.size();
	}
	void listTopLeftIndexChanged();
	void setNavigationButtonsState(bool c1,bool c2);
	void getListMinMaxIndex(int&min,int&max);
};

#endif /* FRAME_H_ */
