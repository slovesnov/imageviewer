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
	ANY, NORMAL, FIT, LIST
};

enum class LANGUAGE {
	IMAGE_VIEWER,
	LANGUAGE,
	ASK_BEFORE_DELETING_A_FILE,
	REMOVE_FILE_OPTION,
	SHOW_POPUP_TIPS,
	REMOVE_FILES_TO_RECYCLE_BIN,
	REMOVE_FILES_PERMANENTLY,
	FILE_SIZE,
	SUPPORTED_FORMATS,
	HOMEPAGE,
	HOMEPAGE1,
	AUTHOR,
	AUTHOR1,
	REMEMBER_THE_LAST_OPEN_DIRECTORY,
	VERSION,
	YES,
	NO,
	OK,
	CANCEL,
	RESET,
	JAN,
	FEB,
	MAR,
	APR,
	MAY,
	JUN,
	JUL,
	AUG,
	SEP,
	OCT,
	NOV,
	DEC,
	OPEN,
	OPEN_FILE,
	SELECT,
	QUESTION,
	DO_YOU_REALLY_WANT_TO_DELETE_THE_IMAGE,
	ZOOM,
	MEGABYTES,
	KILOBYTES,
	BYTES,
	AVERAGE,
	TOTAL,
	ONE_APPLICATION_INSTANCE,
	REQUIRES_RESTARTING,
	LTOOLTIP1,
	LTOOLTIP2,
	LTOOLTIP3,
	LTOOLTIP4,
	LTOOLTIP5,
	LTOOLTIP6,
	LTOOLTIP7,
	LTOOLTIP8,
	LTOOLTIP9,
	LTOOLTIP10,
	LTOOLTIP11,
	LTOOLTIP12,
	LTOOLTIP13,
	LTOOLTIP14,
	LTOOLTIP15,
	LTOOLTIP16,
	LTOOLTIP17,
	LTOOLTIP18,
	LTOOLTIP19,
	LTOOLTIP20,
	LTOOLTIP21,
	LTOOLTIP22,
	LTOOLTIP23,
	HELP
};

const LANGUAGE OPTIONS[] = { LANGUAGE::LANGUAGE,
		LANGUAGE::ASK_BEFORE_DELETING_A_FILE, LANGUAGE::REMOVE_FILE_OPTION,
		LANGUAGE::SHOW_POPUP_TIPS, LANGUAGE::ONE_APPLICATION_INSTANCE,
		LANGUAGE::REMEMBER_THE_LAST_OPEN_DIRECTORY, LANGUAGE::HOMEPAGE,
		LANGUAGE::AUTHOR, LANGUAGE::SUPPORTED_FORMATS };
const int SIZE_OPTIONS = SIZE(OPTIONS);

//if add TOOLBAR_INDEX enum need also add toopltip LTOOLTIP.. also need change Frame::keyPress
enum class TOOLBAR_INDEX {
	ZOOM_IN,
	ZOOM_OUT,
	MODE_ZOOM_ANY,
	MODE_ZOOM_100,
	MODE_ZOOM_FIT,
	MODE_LIST,
	ROTATE_ANTICLOCKWISE,
	ROTATE_180,
	ROTATE_CLOCKWISE,
	FLIP_HORIZONTAL,
	FLIP_VERTICAL,
	HOME,
	PAGE_UP,
	PREVIOUS,
	NEXT,
	PAGE_DOWN,
	END,
	OPEN,
	DELETE_FILE,
	REORDER_FILE,
	FULLSCREEN,
	SETTINGS,
	HELP,
	TB_SIZE
};
const int TOOLBAR_INDEX_SIZE = int(TOOLBAR_INDEX::TB_SIZE);

const gchar OPEN_FILE_SIGNAL_NAME[] = "imageviewer_open_file";

//all system constants
const bool DEFAULT_ONE_INSTANCE = true; //if oneInstance=true then not open new imageviewer window if click on image
const int WHEEL_MULTIPLIER = 80;
const int SCROLL_DELAY_MILLISECONDS = 500;
const double IMAGE_VIEWER_VERSION = 1.0;

class Frame {
public:
	GtkWidget *m_window, *m_area, *m_box, *m_toolbar,
			*m_button[TOOLBAR_INDEX_SIZE], *m_options[SIZE_OPTIONS], *m_modal;
	int m_lastWidth, m_lastHeight;
	Pixbuf m_pix, m_pixScaled;
	/*
	 * m_pw, m_ph - image width, height
	 * m_posh, m_posv - source image copy x,y
	 * m_aw, m_ah - image copy width, height
	 */
	int m_pw, m_ph, m_posh, m_posv, m_aw, m_ah, m_pi;
	MODE m_mode, m_lastNonListMode; //m_lastNonListMode need when user click on image
	VImage m_vp;
	//lower cased allowable files extension
	VString m_vLowerExtension, m_vExtension;
	std::string m_dir;
	guint32 m_lastScroll;
	std::vector<GThread*> m_pThread;
	GMutex m_mutex;
	std::atomic_int m_threadNumber;
	int m_loadid;
	gint m_endThreads; //already have function stopThreads
	int m_filenameFontHeight, m_listTopLeftIndex, totalFileSize;
	int m_listx, m_listy, m_listdx, m_listdy, m_listxy;
	Pixbuf m_buttonPixbuf[TOOLBAR_INDEX_SIZE][2];
	std::vector<GdkPixbuf*> m_additionalImages;
	bool m_ascendingOrder;
	int m_listIconHeight, m_listIconWidth;
	VString m_language;
	int m_languageIndex, m_warningBeforeDelete, m_deleteOption, m_showPopup,
			m_rememberLastOpenDirectory;
	guint m_timer;
	double m_zoom;
	std::vector<int*> m_optionsPointer;
	static std::vector<int> m_optionsDefalutValue;
	static int m_oneInstance;
	const static bool filenameFontBold = true;

	Frame(GtkApplication *application, std::string path = "");
	virtual ~Frame();

	void setTitle();
	void load(const std::string &p, int index = 0, bool start = false); //supports dir & file
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

	void rotatePixbuf(Pixbuf &p, int &w, int &h, int angle);
	void flipPixbuf(Pixbuf &p, bool horizontal);

	int showConfirmation(const std::string &text);
	void startThreads();
	void thumbnailThread(int n);
	void stopThreads();
	void buttonPress(GdkEventButton *event);
	void setShowThumbnail(int i);
	void setListTopLeftIndexStartValue();
	void scrollList(int v);

	void buttonClicked(TOOLBAR_INDEX t);
	void buttonClicked(int t);
	void optionsButtonClicked(LANGUAGE l);
	void showHelp();
	void showSettings();
	void showModalDialog(GtkWidget *w, int o);

	void recountListParameters();
	void updateNavigationButtonsState();
	void setButtonState(int i, bool enable);
	void setButtonState(TOOLBAR_INDEX i, bool enable) {
		setButtonState(int(i), enable);
	}
	void setMode(MODE m, bool start = false);
	int size();
	void listTopLeftIndexChanged();
	void setNavigationButtonsState(bool c1, bool c2);
	void getListMinMaxIndex(int &min, int &max);
	int getFirstListIndex();

	void setButtonImage(int i, bool enable, GdkPixbuf *p);
	void redraw(bool withTitle = true);
	int countFontMaxHeight(const std::string &s, bool bold, cairo_t *cr);
	void setIconHeightWidth(int height);
	void loadLanguage();
	std::string getTitleVersion();
	std::string& getLanguageString(LANGUAGE l, int add = 0);
	GtkWidget* createTextCombo(VString &v, int active);
	void resetOptions();
	void updateOptions();
	void setPopups();
	std::string filechooser(GtkWidget *parent, const std::string &dir);
	std::string getSizeMbKbB(double v);
	void addMonitor(std::string &path);
	void directoryChanged();
	void directoryChangedAddEvent();
	void stopTimer(guint &t);
	std::string getExtensionString(bool b);
	static bool isOneInstanceOnly();
	void setDefaultZoom();
	GtkWidget* createLanguageCombo();
	void setAscendingOrder(bool b);
	void updateSaveDeleteButton();
};

#endif /* FRAME_H_ */
