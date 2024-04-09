/*
 * consts.h
 *
 *  Created on: 03.04.2024
 *      Author: alexey slovesnov
 * copyright(c/c++): 2014-doomsday
 *           E-mail: slovesnov@yandex.ru
 *         homepage: slovesnov.users.sourceforge.net
 */

#ifndef CONSTS_H_
#define CONSTS_H_

#include <string>

#include "enums.h"

/* internal gtk library not scaled some svg images properly (for example arrow0.svg from bridge project) now just scale pixbuf as raster when zoom
 * rsvg library not working with arrow0.svg
 * i've tried use libraries. They don't always work correctly.
 * nanosvg from github has problems with arrow2.svg from bridge project
 * lunasvg from github works good but for deck1.svg from bridge project not shows all svg elements
 */
//#define USE_EXTERNAL_SVG_LIB

const std::string LNG[] = { "en", "ru" };
const std::string LNG_LONG[] = { "english", "russian" };

const LANGUAGE OPTIONS[] = { LANGUAGE::LANGUAGE,
		LANGUAGE::WARN_BEFORE_DELETING_A_FILE, LANGUAGE::REMOVE_FILE_OPTION,
		LANGUAGE::WARN_BEFORE_SAVING_A_FILE, LANGUAGE::SHOW_POPUP_TIPS,
		LANGUAGE::ONE_APPLICATION_INSTANCE,
		LANGUAGE::REMEMBER_THE_LAST_OPEN_DIRECTORY,
		LANGUAGE::SHOW_THE_TOOLBAR_IN_FULLSCREEN_MODE,LANGUAGE::RECURSIVE_DIRECTORY
		//zoom factor should be last insert new values before it see Frame::updateOptions()
		, LANGUAGE::ZOOM_FACTOR
};

const int TOOLBAR_INDEX_SIZE = int(TOOLBAR_INDEX::SZ);

const gchar OPEN_FILE_SIGNAL_NAME[] = "imageviewer_open_file";

const bool DEFAULT_ONE_INSTANCE = true; //if oneInstance=true then not open new imageviewer window if click on image
const int WHEEL_MULTIPLIER = 80;
const int LIST_MULTIPLIER = 50;
const int SCROLL_DELAY_MILLISECONDS = 500;
const double IMAGE_VIEWER_VERSION = 1.0;
const int MAX_HOTKEYS = 3;
const guint INVALID_KEY = 0;
const int ZOOM_FACTOR_ID = -1;
const int GOTO_BEGIN = INT_MIN;
const int GOTO_END = INT_MAX;
const int MIN_SCALED_IMAGE_HEIGHT = 16;
const double MIN_ZOOM_FACTOR_BOUND = 1;
const double DEFAULT_ZOOM_FACTOR = 1.1;
const double MAX_ZOOM_FACTOR = 2;
const double INVALID_ZOOM_FACTOR = 3;
//assert(INVALID_ZOOM_FACTOR<MIN_ZOOM_FACTOR_BOUND || INVALID_ZOOM_FACTOR>MAX_ZOOM_FACTOR);
const double MIN_MANULAL_OPERATION_ELAPSE=5;//seconds max was 3.6

const int LIST_IMAGE_STEP = 15;
const int LIST_IMAGE_STEPS = 8;
const int MAX_LIST_IMAGE_HEIGHT = 128; //MIN_LIST_IMAGE_HEIGHT+ (LIST_IMAGE_STEPS - 1) * LIST_IMAGE_STEP; //1920/15=128 15-icons
const int MIN_LIST_IMAGE_HEIGHT = MAX_LIST_IMAGE_HEIGHT-(LIST_IMAGE_STEPS - 1) * LIST_IMAGE_STEP;//128-7*15=23

static const char *ADDITIONAL_IMAGES[] = { "sort_ascending.png",
		"sort_descending.png" };

static const char *TOOLBAR_IMAGES[] = { "magnifier_zoom_in.png",
		"magnifier_zoom_out.png",

		"zoom.png", "zoom_actual.png", "zoom_fit.png", "large_tiles.png",

		"arrow_rotate_anticlockwise.png", "arrow_refresh.png",
		"arrow_rotate_clockwise.png", "leftright.png", "updown.png",

		"control_start_blue.png", "control_rewind_blue.png", "previous.png",
		"control_play_blue.png", "control_fastforward_blue.png",
		"control_end_blue.png",

		"folder.png", "cross.png", "save.png", ADDITIONAL_IMAGES[0],

		"fullscreen.png", "setting_tools.png" };
static_assert(std::size(TOOLBAR_IMAGES)==TOOLBAR_INDEX_SIZE);

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
		"delete option", "warn before save", "show popup tips",
		"one application instance", "remember the last open directory",
		"show the toolbar in full-screen mode", "last open directory", "keys"
		,"recursive directory"
		,"save rename"
		,"zoom factor"
};

const guint KEY[] = {
GDK_KEY_KP_Add, GDK_KEY_equal, INVALID_KEY, GDK_KEY_KP_Subtract, GDK_KEY_minus,
		INVALID_KEY

		, '0', INVALID_KEY, INVALID_KEY, '1', INVALID_KEY, INVALID_KEY, '2',
		INVALID_KEY, INVALID_KEY, '3', 'L', INVALID_KEY

		, 'E', INVALID_KEY, INVALID_KEY, 'R', INVALID_KEY, INVALID_KEY, 'T',
		INVALID_KEY, INVALID_KEY, 'H', INVALID_KEY, INVALID_KEY, 'V',
		INVALID_KEY, INVALID_KEY

		, GDK_KEY_Home, GDK_KEY_KP_7, INVALID_KEY, GDK_KEY_Page_Up,
		GDK_KEY_KP_9, INVALID_KEY,
		GDK_KEY_Left, GDK_KEY_KP_4, INVALID_KEY, GDK_KEY_Right,
		GDK_KEY_KP_6, INVALID_KEY,
		GDK_KEY_Page_Down, GDK_KEY_KP_3, INVALID_KEY, GDK_KEY_End,
		GDK_KEY_KP_1, INVALID_KEY

		, 'O', INVALID_KEY, INVALID_KEY, GDK_KEY_Delete,
		GDK_KEY_KP_Decimal, INVALID_KEY, 'S', INVALID_KEY, INVALID_KEY,
		INVALID_KEY, INVALID_KEY, INVALID_KEY, 'F',
		GDK_KEY_F11, GDK_KEY_Escape, GDK_KEY_F1, INVALID_KEY, INVALID_KEY //H already is used

		};
static_assert(TOOLBAR_INDEX_SIZE*MAX_HOTKEYS==std::size(KEY));

const std::string SEPARATOR = "         ";
//milliseconds
const int EVENT_TIME[] = { 800 };
static_assert(int(TIMER::SZ)==std::size(EVENT_TIME));

#endif /* CONSTS_H_ */
