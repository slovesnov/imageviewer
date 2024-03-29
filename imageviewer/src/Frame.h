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

#include <atomic>
#include "aslov.h"
#include "Image.h"

enum class MODE {
	NORMAL, FIT, LIST
};

enum class TOOLBAR_INDEX{
	MODE_NORMAL,MODE_FIT,MODE_LIST,
	ROTATE_ANTICLOCKWISE,ROTATE_180,ROTATE_CLOCKWISE,
	HOME,PAGE_UP,PREVIOUS,NEXT,PAGE_DOWN,END,
	OPEN,DELETE,FULLSCREEN,HELP
	,TB_SIZE
};
const int TOOLBAR_INDEX_SIZE=int(TOOLBAR_INDEX::TB_SIZE);

const TOOLBAR_INDEX LIST_ZOOM_OUT=TOOLBAR_INDEX::ROTATE_ANTICLOCKWISE;//increase size
const TOOLBAR_INDEX LIST_ZOOM_IN=TOOLBAR_INDEX::ROTATE_180;//decrease size
const TOOLBAR_INDEX ASCENDING_DESCENDING=TOOLBAR_INDEX::DELETE;

const gchar OPEN_FILE_SIGNAL_NAME[] = "imageviewer_open_file";

//all system constants
const bool ONE_INSTANCE = true; //if oneInstance=true then not open new imageviewer window if click on image
const int WHEEL_MULTIPLIER = 80;
const int SCROLL_DELAY_MILLISECONDS = 500;

class Frame {
public:
	GtkWidget *window, *area,*box,*toolbar,*button[TOOLBAR_INDEX_SIZE];
	int lastWidth, lastHeight;
	int posh, posv;
	Pixbuf pix, pixs;
	int pw, ph, aw, ah, pws, phs;
	double scale;
	int pi;
	MODE mode;
	VImage vp;
	//lower cased allowable files extension
	VString vLowerExtension;
	std::string extensionString;
	std::string dir;
	guint32 lastScroll;
	std::vector<GThread*>pThread;
	GMutex mutex;
	std::atomic_int threadNumber;
	int loadid;
	gint endThreads;//already have function stopThreads
	int loadingFontHeight,filenameFontHeight,listTopLeftIndex,totalFileSize;
	int listx,listy,listdx,listdy,listxy;
	Pixbuf buttonPixbuf[TOOLBAR_INDEX_SIZE][2];
	std::vector<GdkPixbuf*>addi;
	bool listAscendingOrder;
	MODE lastNonListMode;
	const static bool filenameFontBold=true;
	int listIconHeight,listIconWidth;

	Frame(GtkApplication *application, std::string const path = "");
	virtual ~Frame();

	void setTitle();
	void load(const std::string &p, int index = 0,bool start=false); //supports dir & file
	void loadImage();
	void drawImage();
	void draw(cairo_t *cr, GtkWidget *widget);
	gboolean keyPress(GdkEventKey *event);
	void setDragDrop(GtkWidget *widget);
	void openUris(char **uris);
	void scroll(GdkEventScroll *event);
	void setPosRedraw(double dx, double dy, guint32 time = 0);
	bool noImage();
	void setNoImage();
	bool isSupportedImage(std::string const &p);
	void adjustPos();

	void openDirectory();
	void switchImage(int v, bool add);

	void setSmallImage();
	void rotatePixbuf(Pixbuf &p, int &w, int &h, int angle);

	int showConfirmation(const std::string text);
	void startThreads();
	void thumbnailThread(int n);
	void stopThreads();
	void buttonPress(GdkEventButton *event);
	void setShowThumbnail(int i);
	void setListTopLeftIndexStartValue();
	void scrollList(int v);

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
	int getFirstListIndex();

	void setButtonImage(int i,bool enable,GdkPixbuf*p);
	void setVariableImagesButtonsState();
	void redraw(bool withTitle=true);
	int countFontMaxHeight(const std::string& s,bool bold,cairo_t *cr);
	void setIconHeightWidth(int height);
};

#endif /* FRAME_H_ */
