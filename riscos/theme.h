#ifndef _MIGRATE_RISCOS_THEME_H_
#define _MIGRATE_RISCOS_THEME_H_

#include "oslib/wimp.h"
#include "oslib/messagetrans.h"

typedef enum {THEME_THEMEINFO, THEME_TOOLBAR} theme_window_type;

struct ro_theme
{
  wimp_window* theme_info;
  wimp_window* toolbar;

  char* filename;

  char* indirected_data;
  char* window_and_icon_data;

  osspriteop_area* sprites;
  int throbs;

  struct
  {
    messagetrans_control_block cb;
    char* data;
    char* filename;
  } iconNames;

  struct
  {
    messagetrans_control_block cb;
    char* data;
    char* filename;
  } iconSizes;

};

struct ro_theme_window
{
  theme_window_type type;

  union {
    struct {
      char* indirected_url;
      char* indirected_status;
    } toolbar;
    struct {
      char* indirected_url;
      char* indirected_title;
      char* indirected_size;
      char* indirected_process;
    } about;
  } data;
};

typedef struct ro_theme_window ro_theme_window;
typedef struct ro_theme ro_theme;

typedef enum {theme_TOOLBAR_UNKNOWN,
              theme_TOOLBAR_BACK, theme_TOOLBAR_FORWARD, theme_TOOLBAR_RELOAD,
              theme_TOOLBAR_URL, theme_TOOLBAR_STATUS} theme_gadget;

/* install a new theme */
ro_theme* ro_theme_create(char* pathname);

/* return icon number */
wimp_i ro_theme_icon(ro_theme* theme, theme_window_type type, char* token);

/* create a window */
wimp_w ro_theme_create_window(ro_theme* theme, ro_theme_window* create);

int ro_theme_toolbar_height(ro_theme* theme);

#endif
