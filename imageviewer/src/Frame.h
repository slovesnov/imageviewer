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
#include "enums.h"
#include "consts.h"
#include "Image.h"

struct FileSupported {
	std::string extension, type;
	int index;
	bool writable;
	bool operator<(FileSupported const &a) const {
		return extension < a.extension;
	}
};

class Frame {
public:
	GtkWidget *m_window, *m_area, *m_box, *m_toolbar,
			*m_button[TOOLBAR_INDEX_SIZE], *m_options[SIZE(OPTIONS)], *m_modal;

	int m_lastWidth, m_lastHeight;
	Pixbuf m_pix;
	/*
	 * m_pw, m_ph - image width, height
	 * m_posh, m_posv - source image copy x,y
	 * m_aw, m_ah - image copy width, height
	 */
	int m_pw, m_ph, m_posh, m_posv, m_aw, m_ah, m_pi;
	MODE m_mode, m_lastNonListMode; //m_lastNonListMode need when user click on image
	VImage m_vp;
	std::vector<FileSupported> m_supported;
	std::string m_dir;
	std::vector<GThread*> m_pThread;
	GMutex m_mutex;
	std::atomic_int m_threadNumber;
	gint m_endThreads;
	int m_loadid;
	int m_filenameFontHeight, m_listTopLeftIndex, m_totalFileSize;
	int m_listx, m_listy, m_listdx, m_listdy, m_listxy;
	Pixbuf m_buttonPixbuf[TOOLBAR_INDEX_SIZE][2];
	std::vector<GdkPixbuf*> m_additionalImages;
	bool m_ascendingOrder;
	int m_listIconHeight, m_listIconWidth, m_listIconIndex;
	VString m_language;
	int m_languageIndex, m_warnBeforeDelete, m_deleteOption, m_warnBeforeSave,
			m_showPopup, m_rememberLastOpenDirectory, m_showToolbarFullscreen;
	guint m_timer[int(TIMER::SZ)];
	double m_zoom, m_zoomFactor;
	std::vector<int*> m_optionsPointer;
	guint m_key[TOOLBAR_INDEX_SIZE * MAX_HOTKEYS];
	int m_recursive;
	GFileMonitor *m_monitor;
	clock_t m_lastManualOperationTime;

	//options dialog variables
	DIALOG m_modalDialogIndex;
	GtkWidget *m_modalDialogEntry, *m_modalDialogCombo,
			*m_showModalDialogButtonOK;
	std::string m_modalDialogEntryText;
	int m_saveRename;
	//settings dialog variables
	GtkWidget *m_notebook;
	guint m_tmpkey[TOOLBAR_INDEX_SIZE * MAX_HOTKEYS];
	bool m_modalDialogEntryOK;
	double m_modalDialogFactor;
	bool m_proceedEvents;

	static std::vector<int> m_optionsDefalutValue;
	static int m_oneInstance;
	const static bool filenameFontBold = true;

	Frame(GtkApplication *application, std::string path = "");
	virtual ~Frame();

	void setTitle();
	void load(const std::string &path, bool start = false); //supports dir & file
	void addDirectory(const std::string &dir);
	void loadImage();
	void drawImage();
	void draw(cairo_t *cr, GtkWidget *widget);
	gboolean keyPress(GtkWidget *w, GdkEventKey *event, int n);
	void setDragDrop(GtkWidget *widget);
	void openUris(char **uris);
	void scrollEvent(GdkEventScroll *event);
	void setPosRedraw(double dx, double dy);
	bool noImage();
	void setNoImage();
	std::vector<FileSupported>::const_iterator supportedIterator(
			std::string const &p, bool writableOnly);
	bool isSupportedImage(std::string const &p, bool writableOnly = false);
	void adjustPos();

	void openDirectory();
	void switchImage(int v, bool add);

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
	void showDialog(DIALOG d, std::string s);
	void showSettings();
	gint showDeleteDialog();
	gint showSaveDialog(bool error = false);
	gint showModalDialog(GtkWidget *w, DIALOG o);

	void recountListParameters();
	void updateNavigationButtonsState();
	void updateZoomButtonsState();
	void setButtonState(int i, bool enable);
	void setButtonState(TOOLBAR_INDEX i, bool enable);
	void setMode(MODE m, bool start = false);
	int size();
	void listTopLeftIndexChanged();
	void setNavigationButtonsState(bool c1, bool c2);
	void getListMinMaxIndex(int &min, int &max);
	int getFirstListIndex();

	void setButtonImage(int i, bool enable, GdkPixbuf *p);
	void redraw(bool withTitle = true);
	int countFontMaxHeight(const std::string &s, bool bold, cairo_t *cr);
	int getWidthForHeight(int height);
	void setIconHeightWidth(int delta);
	void loadLanguage();
	std::string getTitleVersion();
	std::string const& getLanguageString(LANGUAGE l, int add = 0);
	std::string getLanguageStringMultiline(LANGUAGE l);
	const char* getLanguageStringC(LANGUAGE l, int add = 0);
	GtkWidget* createTextCombo(VString const &v, int active);
	void resetOptions();
	void updateOptions();
	void setPopups();
	std::string filechooser(GtkWidget *parent, const std::string &dir);
	std::string getSizeMbKbB(double v);
	void addMonitor(std::string &path);
	void timerEventOccurred(TIMER t);
	void addTimerEvent(TIMER t);
	void stopTimer(guint &t);
	std::string getExtensionString(bool writableOnly = false, bool onlyIndex0 =
			true, int rows = 1);
	static bool isOneInstanceOnly();
	double getDefaultZoomFit();
	void setDefaultZoom();
	void setCenterPos();
	GtkWidget* createLanguageCombo();
	bool isFullScreen();
	void setAscendingOrder(bool b);
	void entryChanged(GtkWidget *w, int n);
	void focusIn(GtkWidget *w, int n);
	void focusOut(GtkWidget *w, int n);
	static void addInsertDeleteEvents(GtkWidget *w, int n);
	static void addFocusEvents(GtkWidget *w, int n);
	void sortFiles();
	void setPictureIndex(const std::string &path);
	void updateModesButtonState();
};

#endif /* FRAME_H_ */
