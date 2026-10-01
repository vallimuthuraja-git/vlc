/*****************************************************************************
 * qt.cpp : Qt interface
 ****************************************************************************
 * Copyright © 2006-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS

#include <QApplication>
#include <QDate>
#include <QGuiApplication>
#include <QMutex>
#include <QStyle>
#include <QStyleHints> /* QStyleHints::colorScheme() */

#include "qt.hpp"

#include "input_manager.hpp"    /* THEMIM destruction */
#include "dialogs_provider.hpp" /* THEDP creation */
#ifdef _WIN32
# include "main_interface_win32.hpp"
# include "util/qvlcframe.hpp" /* Accent Color */
#else
# include "main_interface.hpp"   /* MainInterface creation */
#endif
#include "extensions_manager.hpp" /* Extensions manager */
#include "managers/addons_manager.hpp" /* Addons manager */
#include "dialogs/help.hpp"     /* Launch Update */
#include "recents.hpp"          /* Recents Item destruction */
#include "util/qvlcapp.hpp"     /* QVLCApplication definition */
#include "components/playlist/playlist_model.hpp" /* for ~PLModel() */

#include <vlc_plugin.h>
#include <vlc_vout_window.h>

#ifdef _WIN32 /* For static builds */
 #include <QtPlugin>

 #ifdef QT_STATICPLUGIN
  Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
  Q_IMPORT_PLUGIN(QSvgIconPlugin)
  Q_IMPORT_PLUGIN(QSvgPlugin)
  #if !HAS_QT56
   Q_IMPORT_PLUGIN(AccessibleFactory)
  #endif
 #endif
#endif

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  OpenIntf     ( vlc_object_t * );
static int  OpenDialogs  ( vlc_object_t * );
static int  Open         ( vlc_object_t *, bool );
static void Close        ( vlc_object_t * );
static int  WindowOpen   ( vout_window_t *, const vout_window_cfg_t * );
static void WindowClose  ( vout_window_t * );
static void ShowDialog   ( intf_thread_t *, int, int, intf_dialog_args_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ADVANCED_PREFS_TEXT N_( "Show advanced preferences over simple ones" )
#define ADVANCED_PREFS_LONGTEXT N_( "Show advanced preferences and not simple "\
                                    "preferences when opening the preferences "\
                                    "dialog." )

#define SYSTRAY_TEXT N_( "Systray icon" )
#define SYSTRAY_LONGTEXT N_( "Show an icon in the systray " \
                             "allowing you to control VLC media player " \
                             "for basic actions." )

#define MINIMIZED_TEXT N_( "Start VLC with only a systray icon" )
#define MINIMIZED_LONGTEXT N_( "VLC will start with just an icon in " \
                               "your taskbar." )

#define KEEPSIZE_TEXT N_( "Resize interface to the native video size" )
#define KEEPSIZE_LONGTEXT N_( "You have two choices:\n" \
            " - The interface will resize to the native video size\n" \
            " - The video will fit to the interface size\n " \
            "By default, interface resize to the native video size." )

#define TITLE_TEXT N_( "Show playing item name in window title" )
#define TITLE_LONGTEXT N_( "Show the name of the song or video in the " \
                           "controller window title." )

#define NOTIFICATION_TEXT N_( "Show notification popup on track change" )
#define NOTIFICATION_LONGTEXT N_( \
    "Show a notification popup with the artist and track name when " \
    "the current playlist item changes, when VLC is minimized or hidden." )

#define OPACITY_TEXT N_( "Windows opacity between 0.1 and 1" )
#define OPACITY_LONGTEXT N_( "Sets the windows opacity between 0.1 and 1 " \
                             "for main interface, playlist and extended panel."\
                             " This option only works with Windows and " \
                             "X11 with composite extensions." )

#define OPACITY_FS_TEXT N_( "Fullscreen controller opacity between 0.1 and 1" )
#define OPACITY_FS_LONGTEXT N_( "Sets the fullscreen controller opacity between 0.1 and 1 " \
                             "for main interface, playlist and extended panel."\
                             " This option only works with Windows and " \
                             "X11 with composite extensions." )

#define ERROR_TEXT N_( "Show unimportant error and warnings dialogs" )

#define QT_DARK_TEXT N_( "Force the Qt interface to use the dark palette, or follow the system" )
#define QT_DARK_LONGTEXT N_( "Choose the colour theme of the Qt interface.\n" \
                       "\"System\" follows the desktop preference (light or dark) and " \
                       "updates when the system setting changes.\n" \
                       "\"Dark\" always uses the dark palette.\n" \
                       "\"Light\" always uses the classic palette." )

#define UPDATER_TEXT N_( "Activate the updates availability notification" )
#define UPDATER_LONGTEXT N_( "Activate the automatic notification of new " \
                            "versions of the software. It runs once every " \
                            "two weeks." )
#define UPDATER_DAYS_TEXT N_("Number of days between two update checks")

#define PRIVACY_TEXT N_( "Ask for network policy at start" )

#define RECENTPLAY_TEXT N_( "Save the recently played items in the menu" )

#define RECENTPLAY_FILTER_TEXT N_( "List of words separated by | to filter" )
#define RECENTPLAY_FILTER_LONGTEXT N_( "Regular expression used to filter " \
        "the recent items played in the player." )

#define SLIDERCOL_TEXT N_( "Define the colors of the volume slider" )
#define SLIDERCOL_LONGTEXT N_( "Define the colors of the volume slider\n" \
                       "By specifying the 12 numbers separated by a ';'\n" \
            "Default is '255;255;255;20;226;20;255;176;15;235;30;20'\n" \
            "An alternative can be '30;30;50;40;40;100;50;50;160;150;150;255'")

#define QT_MODE_TEXT N_( "Selection of the starting mode and look" )
#define QT_MODE_LONGTEXT N_( "Start VLC with:\n" \
                             " - normal mode\n"  \
                             " - a zone always present to show information " \
                                  "as lyrics, album arts...\n" \
                             " - minimal mode with limited controls" )

#define QT_FULLSCREEN_TEXT N_( "Show a controller in fullscreen mode" )
#define QT_NATIVEOPEN_TEXT N_( "Embed the file browser in open dialog" )

#define FULLSCREEN_NUMBER_TEXT N_( "Define which screen fullscreen goes" )
#define FULLSCREEN_NUMBER_LONGTEXT N_( "Screennumber of fullscreen, instead of " \
                                       "same screen where interface is." )

#define QT_AUTOLOAD_EXTENSIONS_TEXT N_( "Load extensions on startup" )
#define QT_AUTOLOAD_EXTENSIONS_LONGTEXT N_( "Automatically load the "\
                                            "extensions module on startup." )

#define QT_MINIMAL_MODE_TEXT N_("Start in minimal view (without menus)" )

#define QT_BGCONE_TEXT N_( "Display background cone or art" )
#define QT_BGCONE_LONGTEXT N_( "Display background cone or current album art " \
                            "when not playing. " \
                            "Can be disabled to prevent burning screen." )
#define QT_BGCONE_EXPANDS_TEXT N_( "Expanding background cone or art" )
#define QT_BGCONE_EXPANDS_LONGTEXT N_( "Background art fits window's size." )

#define QT_DISABLE_VOLUME_KEYS_TEXT N_( "Ignore keyboard volume buttons." )
#define QT_DISABLE_VOLUME_KEYS_LONGTEXT N_(                                             \
    "With this option checked, the volume up, volume down and mute buttons on your "    \
    "keyboard will always change your system volume. With this option unchecked, the "  \
    "volume buttons will change VLC's volume when VLC is selected and change the "      \
    "system volume when VLC is not selected." )

#define QT_PAUSE_MINIMIZED_TEXT N_( "Pause the video playback when minimized" )
#define QT_PAUSE_MINIMIZED_LONGTEXT N_( \
    "With this option enabled, the playback will be automatically paused when minimizing the window." )

#define ICONCHANGE_TEXT N_( "Allow automatic icon changes")
#define ICONCHANGE_LONGTEXT N_( \
    "This option allows the interface to change its icon on various occasions.")

#define VOLUME_MAX_TEXT N_( "Maximum Volume displayed" )

#define AUTORAISE_ON_PLAYBACK_TEXT N_( "When to raise the interface" )
#define AUTORAISE_ON_PLAYBACK_LONGTEXT N_( "This option allows the interface to be raised automatically " \
    "when a video/audio playback starts, or never." )

#define FULLSCREEN_CONTROL_PIXELS N_( "Fullscreen controller mouse sensitivity" )

#define CONTINUE_PLAYBACK_TEXT N_("Continue playback?")

static const int i_notification_list[] =
    { NOTIFICATION_NEVER, NOTIFICATION_MINIMIZED, NOTIFICATION_ALWAYS };

static const char *const psz_notification_list_text[] =
    { N_("Never"), N_("When minimized"), N_("Always") };

static const int i_continue_list[] =
    { 0, 1, 2 };

static const char *const psz_continue_list_text[] =
    { N_("Never"), N_("Ask"), N_("Always") };

/* Colour theme selection. 0 = follow the desktop, 1 = force dark, 2 = force light.
 * This replaces the old boolean "qt-dark-palette" while keeping the same
 * variable name so existing user configurations keep working: 0/1 map onto
 * the old false/true meaning, and the 0 entry now means "system" rather than
 * "light". */
static const int i_color_scheme_list[] =
    { 0, 1, 2 };

static const char *const psz_color_scheme_list_text[] =
    { N_("System"), N_("Dark"), N_("Light") };

static const int i_raise_list[] =
    { MainInterface::RAISE_NEVER, MainInterface::RAISE_VIDEO, \
      MainInterface::RAISE_AUDIO, MainInterface::RAISE_AUDIOVIDEO,  };

static const char *const psz_raise_list_text[] =
    { N_( "Never" ), N_( "Video" ), N_( "Audio" ), _( "Audio/Video" ) };

/**********************************************************************/
vlc_module_begin ()
    set_shortname( "Qt" )
    set_description( N_("Qt interface") )
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_MAIN )
    set_capability( "interface", 151 )
    set_callbacks( OpenIntf, Close )

    add_shortcut("qt")

    add_bool( "qt-minimal-view", false, QT_MINIMAL_MODE_TEXT,
              QT_MINIMAL_MODE_TEXT, false );

    add_bool( "qt-system-tray", true, SYSTRAY_TEXT, SYSTRAY_LONGTEXT, false)

    add_integer( "qt-notification", NOTIFICATION_MINIMIZED,
                 NOTIFICATION_TEXT,
                 NOTIFICATION_LONGTEXT, false )
            change_integer_list( i_notification_list, psz_notification_list_text )

    add_bool( "qt-start-minimized", false, MINIMIZED_TEXT,
              MINIMIZED_LONGTEXT, true)
    add_bool( "qt-pause-minimized", false, QT_PAUSE_MINIMIZED_TEXT,
              QT_PAUSE_MINIMIZED_LONGTEXT, false )

    add_float_with_range( "qt-opacity", 1., 0.1, 1., OPACITY_TEXT,
                          OPACITY_LONGTEXT, false )
    add_float_with_range( "qt-fs-opacity", 0.8, 0.1, 1., OPACITY_FS_TEXT,
                          OPACITY_FS_LONGTEXT, false )

    add_bool( "qt-video-autoresize", true, KEEPSIZE_TEXT,
              KEEPSIZE_LONGTEXT, false )
    add_bool( "qt-name-in-title", true, TITLE_TEXT,
              TITLE_LONGTEXT, false )
    add_bool( "qt-fs-controller", true, QT_FULLSCREEN_TEXT,
              QT_FULLSCREEN_TEXT, false )

    add_bool( "qt-recentplay", true, RECENTPLAY_TEXT,
              RECENTPLAY_TEXT, false )
    add_string( "qt-recentplay-filter", "",
                RECENTPLAY_FILTER_TEXT, RECENTPLAY_FILTER_LONGTEXT, false )
    add_integer( "qt-continue", 1, CONTINUE_PLAYBACK_TEXT, CONTINUE_PLAYBACK_TEXT, false )
            change_integer_list(i_continue_list, psz_continue_list_text )
    add_integer( "qt-dark-palette", 0, QT_DARK_TEXT,
                   QT_DARK_LONGTEXT, false )
            change_integer_list( i_color_scheme_list, psz_color_scheme_list_text )

#ifdef UPDATE_CHECK
    add_bool( "qt-updates-notif", true, UPDATER_TEXT,
              UPDATER_LONGTEXT, false )
    add_integer_with_range( "qt-updates-days", 3, 0, 180,
              UPDATER_DAYS_TEXT, UPDATER_DAYS_TEXT, false )
#endif

#ifdef _WIN32
    add_bool( "qt-disable-volume-keys"             /* name */,
              true                                 /* default value */,
              QT_DISABLE_VOLUME_KEYS_TEXT          /* text */,
              QT_DISABLE_VOLUME_KEYS_LONGTEXT      /* longtext */,
              false                                /* advanced mode only */)
#endif

    add_bool( "qt-embedded-open", false, QT_NATIVEOPEN_TEXT,
               QT_NATIVEOPEN_TEXT, false )


    add_bool( "qt-advanced-pref", false, ADVANCED_PREFS_TEXT,
              ADVANCED_PREFS_LONGTEXT, false )
    add_bool( "qt-error-dialogs", true, ERROR_TEXT,
              ERROR_TEXT, false )

    add_string( "qt-slider-colours", "153;210;153;20;210;20;255;199;15;245;39;29",
                SLIDERCOL_TEXT, SLIDERCOL_LONGTEXT, false )

    add_bool( "qt-privacy-ask", true, PRIVACY_TEXT, PRIVACY_TEXT,
              false )
        change_private ()

    add_integer( "qt-fullscreen-screennumber", -1, FULLSCREEN_NUMBER_TEXT,
               FULLSCREEN_NUMBER_LONGTEXT, false );

    add_bool( "qt-autoload-extensions", true,
              QT_AUTOLOAD_EXTENSIONS_TEXT, QT_AUTOLOAD_EXTENSIONS_LONGTEXT,
              false )

    add_bool( "qt-bgcone", true, QT_BGCONE_TEXT, QT_BGCONE_LONGTEXT, true )
    add_bool( "qt-bgcone-expands", false, QT_BGCONE_EXPANDS_TEXT,
              QT_BGCONE_EXPANDS_LONGTEXT, true )

    add_bool( "qt-icon-change", true, ICONCHANGE_TEXT, ICONCHANGE_LONGTEXT, true )

    add_integer_with_range( "qt-max-volume", 125, 60, 300, VOLUME_MAX_TEXT, VOLUME_MAX_TEXT, true)

    add_integer_with_range( "qt-fs-sensitivity", 3, 0, 4000, FULLSCREEN_CONTROL_PIXELS,
            FULLSCREEN_CONTROL_PIXELS, true)

    add_obsolete_bool( "qt-blingbling" )      /* Suppressed since 1.0.0 */
    add_obsolete_integer( "qt-display-mode" ) /* Suppressed since 1.1.0 */

    add_obsolete_bool( "qt-adv-options" )     /* Since 2.0.0 */
    add_obsolete_bool( "qt-volume-complete" ) /* Since 2.0.0 */
    add_obsolete_integer( "qt-startvolume" )  /* Since 2.0.0 */

    add_integer( "qt-auto-raise", MainInterface::RAISE_VIDEO, AUTORAISE_ON_PLAYBACK_TEXT,
                 AUTORAISE_ON_PLAYBACK_LONGTEXT, false )
            change_integer_list( i_raise_list, psz_raise_list_text )

    cannot_unload_broken_library()

    add_submodule ()
        set_description( "Dialogs provider" )
        set_capability( "dialogs provider", 51 )

        set_callbacks( OpenDialogs, Close )

    add_submodule ()
        set_capability( "vout window", 0 )
        set_callbacks( WindowOpen, WindowClose )

vlc_module_end ()

/*****************************************/

/* Ugly, but the Qt interface assumes single instance anyway */
static vlc_sem_t ready;
static QMutex lock;
static bool busy = false;
static bool active = false;

/*****************************************************************************
 * Module callbacks
 *****************************************************************************/

static void *ThreadPlatform( void *, char * );

static void *Thread( void *data )
{
    return ThreadPlatform( data, NULL );
}

#ifdef Q_OS_MAC
/* Used to abort the app.exec() on OSX after libvlc_Quit is called */
#include "../../../lib/libvlc_internal.h" /* libvlc_SetExitHandler */
static void Abort( void *obj )
{
    QVLCApp::triggerQuit();
}
#endif

#if defined (QT_HAS_X11)
# include <vlc_xlib.h>
# include "qt_x11.hpp"

static void *ThreadXCB( void *data )
{
    char platform_name[] = "xcb";
    return ThreadPlatform( data, platform_name );
}

static bool HasX11( vlc_object_t *obj )
{
    if( !vlc_xlib_init( obj ) )
        return false;

    Display *dpy = XOpenDisplay( NULL );
    if( dpy == NULL )
        return false;

    XCloseDisplay( dpy );
    return true;
}
#endif

/* Palette that was in use before the dark palette was applied. It is captured
 * once so that turning the dark palette off restores the exact classic colours
 * instead of guessing from the current style, which may itself be dark on a
 * dark desktop theme. */
static QPalette classicPalette;
static bool classicPaletteSaved = false;

/* The palette the platform theme handed us at startup, sampled once before
 * anything calls QApplication::setStyle(). That call replaces the application
 * palette with the chosen style's defaults, and Fusion's defaults are always
 * light, so asking the live style later can no longer tell a light desktop
 * from a dark one. */
static QPalette platformPalette;
static bool platformPaletteSaved = false;

/* Must be called right after the QApplication is built and before any
 * QApplication::setStyle(). */
static void savePlatformPalette()
{
    if (platformPaletteSaved)
        return;
    const QStyle *style = QApplication::style();
    if (style == NULL)
        return;
    platformPalette = style->standardPalette();
    platformPaletteSaved = true;
}

/* Returns true when the desktop is currently asking applications to use a dark
 * appearance. Covers the three ways that can be known:
 *   - Qt 6.5+ exposes it directly through QStyleHints::colorScheme().
 *   - On Windows the registry holds AppsUseLightTheme.
 *   - Everywhere else the platform default palette's window colour is the only
 *     signal available, so it is compared against mid grey.
 * Returns false when the desktop gives us no usable answer. */
static bool systemPrefersDark()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    /* Qt 6.5+ exposes the desktop scheme directly, and is also the only
     * version that tells us when it changes. Qt::ColorScheme does not exist
     * before 6.5, so the whole block has to be version-guarded. */
    const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
    if (scheme == Qt::ColorScheme::Dark)
        return true;
    if (scheme == Qt::ColorScheme::Light)
        return false;
#endif

#ifdef Q_OS_WIN
    /* Missing key (or an unreadable one) means "not specified", and the
     * documented default for AppsUseLightTheme is light. */
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_READ, &key) != ERROR_SUCCESS)
        return false;
    DWORD value = 1, size = sizeof(value), type = 0;
    const LSTATUS ret = RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, &type,
                                         reinterpret_cast<LPBYTE>(&value), &size);
    RegCloseKey(key);
    if (ret != ERROR_SUCCESS || type != REG_DWORD)
        return false;
    return value == 0; /* 0 == apps should use the dark theme */
#else
    /* Qt older than 6.5 has no scheme query and no change notification, so the
     * platform palette captured at startup is the only signal available.
     * Compare the window colour's lightness rather than any single channel, so
     * a tinted dark theme is still recognised as dark. Reading the live style
     * here instead would be wrong: by now Fusion may have been forced, and its
     * standard palette is always light, which would make a light -> dark
     * desktop switch undetectable for the rest of the session. */
    if (!platformPaletteSaved)
        return false;
    return platformPalette.color(QPalette::Active, QPalette::Window).lightness() < 128;
#endif
}

/* Last value read from the config: 0 = System, 1 = Dark, 2 = Light. Refreshed
 * by isDarkPaletteEnabled() whenever a live object is available; kept here so
 * the system-theme watcher can consult it without needing one. */
static int64_t s_configuredScheme = 0;

/* The theme the user asked for, independent of what is currently applied. */
enum class ColorScheme { System, Dark, Light };

static ColorScheme configuredColorScheme()
{
    switch (s_configuredScheme)
    {
        case 1:  return ColorScheme::Dark;
        case 2:  return ColorScheme::Light;
        default: return ColorScheme::System;
    }
}

/* True when the effective theme, after resolving "System", is dark. */
bool isDarkPaletteEnabled(intf_thread_t *p_intf)
{
    /* Read live rather than from a cached static bool: the preference can
     * change at runtime from the Interface preferences panel, and the widgets
     * re-theme themselves when it does. */
    if (p_intf != NULL)
        s_configuredScheme = var_InheritInteger(p_intf, "qt-dark-palette");

    const ColorScheme scheme = configuredColorScheme();
    if (scheme == ColorScheme::System)
        return systemPrefersDark();
    return scheme == ColorScheme::Dark;
}

void applyDarkPalette()
{
    /* Remember the classic palette before we replace it, so the user can go
     * back to it later. Only the first call captures it, otherwise a second
     * "enable" would store the dark palette as the classic one. */
    if (!classicPaletteSaved)
    {
        classicPalette = QApplication::palette();
        classicPaletteSaved = true;
    }

    QPalette darkPalette;

    /* Base greys, kept deliberately close together so that the window, the
     * input fields and the raised surfaces stay distinguishable without the
     * interface turning into a set of unrelated black boxes. */
    static const QColor windowColor (43, 43, 43);    /* #2B2B2B panels        */
    static const QColor baseColor   (24, 24, 24);    /* #181818 text fields  */
    static const QColor altColor    (35, 35, 35);    /* #232323 zebra rows   */
    static const QColor midColor    (62, 62, 62);    /* #3E3E3E separators   */
    static const QColor lightColor  (95, 95, 95);    /* #5F5F5F 3D borders   */
    static const QColor darkColor   (15, 15, 15);    /* #0F0F0F shadows      */
    static const QColor textColor   (255, 255, 255);
    static const QColor dimText     (150, 150, 150); /* unfocused / secondary */
    /* Disabled text. Must stay >= 3:1 against the darkest background it can
     * sit on (Base, #181818), otherwise disabled labels become unreadable
     * rather than merely recessive. */
    static const QColor disabledText(120, 120, 120);
    static const QColor disabledBg  (20, 20, 20);

    /* VLC brand orange: readable on the dark backgrounds and consistent with
     * the artwork used across the interface. On Windows the user accent
     * colour is honoured instead. */
#ifdef Q_OS_WIN
    QColor accentColor = getWindowsAccentColor();
    if (!accentColor.isValid())
        accentColor = QColor(255, 136, 0);
#else
    QColor accentColor (255, 136, 0);
#endif
    /* Links must not use the accent directly: white-on-orange is weak text,
     * so a lighter tint of the accent is used for text and the pure accent is
     * kept for selection fills. */
    const QColor linkColor = accentColor.lighter(140);
    const QColor linkVisited = accentColor.lighter(105);
    const QColor highlightText = QColor(20, 20, 20); /* dark text on orange */

    const auto setAll = [&darkPalette](QPalette::ColorRole role,
                                       const QColor &active,
                                       const QColor &inactive,
                                       const QColor &disabled)
    {
        darkPalette.setColor(QPalette::Active,   role, active);
        darkPalette.setColor(QPalette::Inactive, role, inactive);
        darkPalette.setColor(QPalette::Disabled, role, disabled);
    };

    setAll(QPalette::Window,          windowColor, windowColor, windowColor);
    setAll(QPalette::WindowText,      textColor,   dimText,     disabledText);
    setAll(QPalette::Base,            baseColor,   baseColor,   disabledBg);
    setAll(QPalette::AlternateBase,   altColor,    altColor,    altColor);
    setAll(QPalette::Button,          windowColor, windowColor, disabledBg);
    setAll(QPalette::ButtonText,      textColor,   dimText,     disabledText);
    setAll(QPalette::Text,            textColor,   dimText,     disabledText);
    setAll(QPalette::Highlight,       accentColor, accentColor, QColor(70, 70, 70));
    setAll(QPalette::HighlightedText, highlightText, dimText,    disabledText);
    setAll(QPalette::Link,            linkColor,   linkColor,   disabledText);
    setAll(QPalette::LinkVisited,     linkVisited, linkVisited, disabledText);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
    /* QPalette::PlaceholderText only exists from Qt 5.12 on. */
    setAll(QPalette::PlaceholderText, dimText,     dimText,     disabledText);
#endif

    /* Tooltips are shown on a lighter surface than the window so they read as
     * floating rather than as holes cut into the interface. */
    static const QColor tooltipColor (58, 58, 58);
    setAll(QPalette::ToolTipBase, tooltipColor, tooltipColor, tooltipColor);
    setAll(QPalette::ToolTipText, textColor,   textColor,   disabledText);

    /* Non-text roles are used by the styles for borders, 3D edges and
     * separators; leaving them at their light defaults is what makes a dark
     * palette look like light widgets with a dark background. */
    setAll(QPalette::Light,      lightColor,  lightColor,  darkColor);
    setAll(QPalette::Midlight,   midColor,    midColor,    darkColor);
    setAll(QPalette::Mid,        midColor,    midColor,    darkColor);
    setAll(QPalette::Dark,       darkColor,   darkColor,   darkColor);
    setAll(QPalette::Shadow,     darkColor,   darkColor,   darkColor);
    setAll(QPalette::BrightText, QColor(255, 120, 0), QColor(255, 120, 0), disabledText);
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    setAll(QPalette::Accent, accentColor, accentColor, QColor(70, 70, 70));
#endif

    /* Apply the dark palette globally */
    QApplication::setPalette(darkPalette);
}

void applyClassicPalette()
{
    if (classicPaletteSaved)
        QApplication::setPalette(classicPalette);
    else
        QApplication::setPalette(QApplication::style()->standardPalette());
}

/* Applies whichever theme the current configuration resolves to, and records
 * it so applyCurrentColorScheme() can be called again (after a preference
 * change, or a system theme change) without the wrong branch winning. */
static bool bDarkPaletteApplied = false;

/* Store the user's choice. Callers that already have the new value (the
 * preferences panel right after writing it) use this instead of going back
 * through the config machinery. */
void setColorSchemePreference( int64_t value )
{
    s_configuredScheme = value;
}

void applyCurrentColorScheme(intf_thread_t *p_intf)
{
    const bool dark = isDarkPaletteEnabled( p_intf );
    if (dark)
        applyDarkPalette();
    else
        applyClassicPalette();
    bDarkPaletteApplied = dark;
}

/* Called when the desktop tells us its appearance changed. Only meaningful for
 * the "System" setting: an explicit Dark or Light choice must not be
 * overridden by the desktop. */
void onSystemColorSchemeChanged()
{
    if (configuredColorScheme() != ColorScheme::System)
        return;
    if (isDarkPaletteEnabled( NULL ) == bDarkPaletteApplied)
        return; /* Nothing actually changed; avoid a pointless repaint. */
    applyCurrentColorScheme( NULL );
}

/* Open Interface */
static int Open( vlc_object_t *p_this, bool isDialogProvider )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    void *(*thread)(void *) = Thread;

#ifdef QT_HAS_X11
    if( HasX11( p_this ) )
        thread = ThreadXCB;
    else
        return VLC_EGENERIC;
#endif

    QMutexLocker locker (&lock);
    if (busy)
    {
        msg_Err (p_this, "cannot start Qt multiple times");
        return VLC_EGENERIC;
    }

    /* Allocations of p_sys */
    intf_sys_t *p_sys = p_intf->p_sys = new intf_sys_t;
    p_sys->b_isDialogProvider = isDialogProvider;
    p_sys->p_mi = NULL;
    p_sys->pl_model = NULL;

    /* set up the playlist to work on */
    if( isDialogProvider )
        p_sys->p_playlist = pl_Get( (intf_thread_t *)p_intf->obj.parent );
    else
        p_sys->p_playlist = pl_Get( p_intf );

    /* */
    vlc_sem_init (&ready, 0);
#ifdef Q_OS_MAC
    /* Run mainloop on the main thread as Cocoa requires */
    libvlc_SetExitHandler( p_intf->obj.libvlc, Abort, p_intf );
    thread( (void *)p_intf );
#else
    if( vlc_clone( &p_sys->thread, thread, p_intf, VLC_THREAD_PRIORITY_LOW ) )
    {
        delete p_sys;
        return VLC_ENOMEM;
    }
#endif

    /* Wait for the interface to be ready. This prevents the main
     * LibVLC thread from starting video playback before we can create
     * an embedded video window. */
    vlc_sem_wait (&ready);
    vlc_sem_destroy (&ready);
    busy = active = true;

    return VLC_SUCCESS;
}

/* Open Qt interface */
static int OpenIntf( vlc_object_t *p_this )
{
    return Open( p_this, false );
}

/* Open Dialog Provider */
static int OpenDialogs( vlc_object_t *p_this )
{
    return Open( p_this, true );
}

static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    if( !p_sys->b_isDialogProvider )
    {
        playlist_t *pl = THEPL;

        playlist_Deactivate (pl); /* release window provider if needed */
    }

    /* And quit */
    msg_Dbg( p_this, "requesting exit..." );
    QVLCApp::triggerQuit();

    msg_Dbg( p_this, "waiting for UI thread..." );
#ifndef Q_OS_MAC
    vlc_join (p_sys->thread, NULL);
#endif
    delete p_sys;

    QMutexLocker locker (&lock);
    assert (busy);
    busy = false;
}

static void *ThreadPlatform( void *obj, char *platform_name )
{
    intf_thread_t *p_intf = (intf_thread_t *)obj;
    intf_sys_t *p_sys = p_intf->p_sys;
    char vlc_name[] = "vlc"; /* for WM_CLASS */
    char platform_parm[] = "-platform";
    char *argv[4];
    int argc = 0;

    argv[argc++] = vlc_name;
    if( platform_name != NULL )
    {
        argv[argc++] = platform_parm;
        argv[argc++] = platform_name;
    }
    argv[argc] = NULL;

    Q_INIT_RESOURCE( vlc );

#if HAS_QT56
    QApplication::setAttribute( Qt::AA_EnableHighDpiScaling );
    QApplication::setAttribute( Qt::AA_UseHighDpiPixmaps );
#endif
#if HAS_QT57
    QApplication::setAttribute( Qt::AA_UseStyleSheetPropagationInWidgetStyles, true);
#endif

    /* Start the QApplication here */
    QVLCApp app( argc, argv );

    /* Set application direction to locale direction,
     * necessary for  RTL locales */
    app.setLayoutDirection(QLocale().textDirection());

    /* The colour theme is applied further down, once the widget style is known:
     * QApplication::setStyle() resets the application palette, so applying it
     * here would be undone. */
    /* Sample the platform palette before any setStyle() call replaces it - the
     * non-Windows theme fallback reads it later to spot a dark desktop. */
    savePlatformPalette();

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    /* Follow the desktop for as long as the user chose "System". The signal
     * only exists from Qt 6.5 on; older Qt resolves the theme at startup. */
    QObject::connect( app.styleHints(), &QStyleHints::colorSchemeChanged,
                      &app, []() { onSystemColorSchemeChanged(); } );
#endif

    p_sys->p_app = &app;


    /* All the settings are in the .conf/.ini style */
#ifdef _WIN32
    char *cConfigDir = config_GetUserDir( VLC_CONFIG_DIR );
    QString configDir = cConfigDir;
    free( cConfigDir );
    if( configDir.endsWith( "\\vlc" ) )
        configDir.chop( 4 ); /* the "\vlc" dir is added again by QSettings */
    QSettings::setPath( QSettings::IniFormat, QSettings::UserScope, configDir );
#endif

    p_sys->mainSettings = new QSettings(
#ifdef _WIN32
            QSettings::IniFormat,
#else
            QSettings::NativeFormat,
#endif
            QSettings::UserScope, "vlc", "vlc-qt-interface" );

    if( QDate::currentDate().dayOfYear() >= QT_XMAS_JOKE_DAY && var_InheritBool( p_intf, "qt-icon-change" ) )
        app.setWindowIcon( QIcon::fromTheme( "vlc-xmas", QIcon( ":/logo/vlc128-xmas.png" ) ) );
    else
        app.setWindowIcon( QIcon::fromTheme( "vlc", QIcon( ":/logo/vlc256.png" ) ) );

    /* Initialize the Dialog Provider and the Main Input Manager */
    DialogsProvider::getInstance( p_intf );
    MainInputManager* mim = MainInputManager::getInstance( p_intf );
    mim->probeCurrentInput();

#ifdef UPDATE_CHECK
    /* Checking for VLC updates */
    if( var_InheritBool( p_intf, "qt-updates-notif" ) &&
        !var_InheritBool( p_intf, "qt-privacy-ask" ) )
    {
        int interval = var_InheritInteger( p_intf, "qt-updates-days" );
        if( QDate::currentDate() >
             getSettings()->value( "updatedate" ).toDate().addDays( interval ) )
        {
            /* The constructor of the update Dialog will do the 1st request */
            UpdateDialog::getInstance( p_intf );
            getSettings()->setValue( "updatedate", QDate::currentDate() );
        }
    }
#endif

    /* Create the normal interface in non-DP mode */
    MainInterface *p_mi = NULL;

    if( !p_sys->b_isDialogProvider )
    {
#ifdef _WIN32
        p_mi = new MainInterfaceWin32( p_intf );
#else
        p_mi = new MainInterface( p_intf );
#endif
        p_sys->p_mi = p_mi;

        /* Check window type from the Qt platform back-end */
        p_sys->voutWindowType = VOUT_WINDOW_TYPE_INVALID;
        QString platform = app.platformName();
        if( platform == qfu("xcb") )
            p_sys->voutWindowType = VOUT_WINDOW_TYPE_XID;
        else if( platform == qfu("wayland") )
            p_sys->voutWindowType = VOUT_WINDOW_TYPE_WAYLAND;
        else if( platform == qfu("windows") )
            p_sys->voutWindowType = VOUT_WINDOW_TYPE_HWND;
        else if( platform == qfu("cocoa" ) )
            p_sys->voutWindowType = VOUT_WINDOW_TYPE_NSOBJECT;
        else
            msg_Err( p_intf, "unknown Qt platform: %s", qtu(platform) );

        var_Create( THEPL, "qt4-iface", VLC_VAR_ADDRESS );
        var_SetAddress( THEPL, "qt4-iface", p_intf );
        var_Create( THEPL, "window", VLC_VAR_STRING );
        if( p_sys->voutWindowType != VOUT_WINDOW_TYPE_INVALID )
            var_SetString( THEPL, "window", "qt,any" );
    }

    /* Explain how to show a dialog :D */
    p_intf->pf_show_dialog = ShowDialog;

    /* Tell the main LibVLC thread we are ready */
    vlc_sem_post (&ready);

#ifdef Q_OS_MAC
    /* We took over main thread, register and start here */
    if( !p_sys->b_isDialogProvider )
        playlist_Play( THEPL );
#endif

    /* Last settings */
    app.setQuitOnLastWindowClosed( false );

    /* Retrieve last known path used in file browsing */
    const QUrl homeUrl = QUrl::fromLocalFile(QVLCUserDir( VLC_HOME_DIR ));
    p_sys->filepath =
         getSettings()->value( "filedialog-path", homeUrl ).toUrl();

    /* Loads and tries to apply the preferred QStyle */
    QString s_style = getSettings()->value( "MainWindow/QtStyle", "" ).toString();
    if (!s_style.isEmpty())
        QApplication::setStyle( s_style );

    /* Apply the colour theme. This must come after the style is set, because
     * QApplication::setStyle() resets the application palette to the style's
     * defaults - applying the palette first would silently discard it.
     * The native platform styles ignore palette colours, so when the user has
     * not chosen a style explicitly, fall back to Fusion for the dark theme to
     * actually be honoured. */
    if (isDarkPaletteEnabled(p_intf) && s_style.isEmpty())
        QApplication::setStyle( QStringLiteral( "Fusion" ) );
    applyCurrentColorScheme(p_intf);

    /* Launch */
    app.exec();

    msg_Dbg( p_intf, "QApp exec() finished" );
    if (p_mi != NULL)
    {
        var_Destroy( THEPL, "window" );
        var_Destroy( THEPL, "qt4-iface" );

        QMutexLocker locker (&lock);
        active = false;

        p_sys->p_mi = NULL;
        /* Destroy first the main interface because it is connected to some
           slots in the MainInputManager */
        delete p_mi;
    }

    /* */
    ExtensionsManager::killInstance();
    AddonsManager::killInstance();

    /* Destroy all remaining windows,
       because some are connected to some slots
       in the MainInputManager
       Settings must be destroyed after that.
     */
    DialogsProvider::killInstance();

    /* Delete the recentsMRL object before the configuration */
    RecentsMRL::killInstance();

    /* Save the path or delete if recent play are disabled */
    if( var_InheritBool( p_intf, "qt-recentplay" ) )
        getSettings()->setValue( "filedialog-path", p_sys->filepath );
    else
        getSettings()->remove( "filedialog-path" );

    /* */
    delete p_sys->pl_model;

    /* Destroy the MainInputManager */
    MainInputManager::killInstance();

    /* Delete the configuration. Application has to be deleted after that. */
    delete p_sys->mainSettings;

    /* Delete the application automatically */
    return NULL;
}

/*****************************************************************************
 * Callback to show a dialog
 *****************************************************************************/
static void ShowDialog( intf_thread_t *p_intf, int i_dialog_event, int i_arg,
                        intf_dialog_args_t *p_arg )
{
    VLC_UNUSED( p_intf );
    DialogEvent *event = new DialogEvent( i_dialog_event, i_arg, p_arg );
    QApplication::postEvent( THEDP, event );
}

/**
 * Video output window provider
 *
 * TODO move it out of here ?
 */
static int WindowControl( vout_window_t *, int i_query, va_list );

typedef struct {
    MainInterface *mi;
#ifdef QT_HAS_X11
    Display *dpy;
#endif
    QMutex lock;
} vout_window_qt_t;

static int WindowOpen( vout_window_t *p_wnd, const vout_window_cfg_t *cfg )
{
    if( cfg->is_standalone )
        return VLC_EGENERIC;

    intf_thread_t *p_intf =
        (intf_thread_t *)var_InheritAddress( p_wnd, "qt4-iface" );
    if( !p_intf )
    {   /* If another interface is used, this plugin cannot work */
        msg_Dbg( p_wnd, "Qt interface not found" );
        return VLC_EGENERIC;
    }

    if( cfg->type != VOUT_WINDOW_TYPE_INVALID
     && cfg->type != p_intf->p_sys->voutWindowType )
        return VLC_EGENERIC;

    switch( p_intf->p_sys->voutWindowType )
    {
        case VOUT_WINDOW_TYPE_XID:
            if( var_InheritBool( p_wnd, "video-wallpaper" ) )
                return VLC_EGENERIC;
            break;
    }

    QMutexLocker locker (&lock);
    if (unlikely(!active))
        return VLC_EGENERIC;

    vout_window_qt_t *sys = new vout_window_qt_t;

    sys->mi = p_intf->p_sys->p_mi;
    p_wnd->sys = (vout_window_sys_t *)sys;
    msg_Dbg( p_wnd, "requesting video window..." );

#ifdef QT_HAS_X11
    Window xid;

    if (vlcQtIsX11())
    {
        sys->dpy = XOpenDisplay(NULL);
        if (unlikely(sys->dpy == NULL))
        {
            delete sys;
            return VLC_EGENERIC;
        }

        int snum = DefaultScreen(sys->dpy);
        unsigned long black = BlackPixel(sys->dpy, snum);

        xid = XCreateSimpleWindow(sys->dpy, RootWindow(sys->dpy, snum),
                                  0, 0, cfg->width, cfg->height,
                                  0, black, black);
    }
#endif

    if (!sys->mi->getVideo(p_wnd, cfg->width, cfg->height, cfg->is_fullscreen))
    {
#ifdef QT_HAS_X11
        if (vlcQtIsX11())
            XCloseDisplay(sys->dpy);
#endif
        delete sys;
        return VLC_EGENERIC;
    }

#ifdef QT_HAS_X11
    if (vlcQtIsX11())
    {
        QMutexLocker locker2(&sys->lock);

        XReparentWindow(sys->dpy, xid, p_wnd->handle.xid, 0, 0);
        XMapWindow(sys->dpy, xid);
        XSync(sys->dpy, True);
        p_wnd->handle.xid = xid;
    }
#endif
    p_wnd->info.has_double_click = true;
    p_wnd->control = WindowControl;
    return VLC_SUCCESS;
}

void WindowResized(vout_window_t *wnd, const QSize& size)
{
#ifdef QT_HAS_X11
    vout_window_qt_t *sys = (vout_window_qt_t *)wnd->sys;

    if (vlcQtIsX11())
    {
        XResizeWindow(sys->dpy, wnd->handle.xid, size.width(), size.height());
        XSync(sys->dpy, True);
    }
#endif
    vout_window_ReportSize(wnd, size.width(), size.height());
}

static int WindowControl( vout_window_t *p_wnd, int i_query, va_list args )
{
    vout_window_qt_t *sys = (vout_window_qt_t *)p_wnd->sys;
    QMutexLocker locker (&lock);

    if (unlikely(!active))
    {
        msg_Warn (p_wnd, "video already released before control");
        return VLC_EGENERIC;
    }
    return sys->mi->controlVideo(i_query, args);
}

void WindowOrphaned(vout_window_t *wnd)
{
    vout_window_qt_t *sys = (vout_window_qt_t *)wnd->sys;
    QMutexLocker locker(&sys->lock);

    msg_Warn(wnd, "orphaned video window");
#if defined (QT_HAS_X11)
    if (vlcQtIsX11())
    {   /* In the unlikely event that WindowOpen() has not yet reparented the
         * window, WindowOpen() will skip reparenting. Then this call will be
         * a no-op.
         */
        XUnmapWindow (sys->dpy, wnd->handle.xid);
        XReparentWindow(sys->dpy, wnd->handle.xid,
                        RootWindow(sys->dpy, DefaultScreen(sys->dpy)), 0, 0);
        XSync(sys->dpy, True);
    }
#endif
}

static void WindowClose( vout_window_t *p_wnd )
{
    vout_window_qt_t *sys = (vout_window_qt_t *)p_wnd->sys;
    QMutexLocker locker (&lock);

    /* Normally, the interface terminates after the video. In the contrary, the
     * Qt main loop is gone, so we cannot send any event to the user interface
     * widgets. Ideally, we would keep the Qt main loop running until after
     * the video window is released. But it is far simpler to just have the Qt
     * thread destroy the window early, and to turn this function into a stub.
     *
     * That assumes the video output will behave sanely if it window is
     * destroyed asynchronously.
     * XCB and Xlib-XCB are fine with that. Plain Xlib wouldn't, */
    if (likely(active))
    {
        msg_Dbg(p_wnd, "releasing video...");
        sys->mi->releaseVideo();
    }
    else
        msg_Warn (p_wnd, "video already released");

#if defined (QT_HAS_X11)
    if (vlcQtIsX11())
        XCloseDisplay(sys->dpy);
#endif
    delete sys;
}
