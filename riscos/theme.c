
#include "netsurf/riscos/theme.h"
#include "oslib/wimp.h"
#include "oslib/messagetrans.h"
#include "oslib/osspriteop.h"
#include "string.h"
#include "netsurf/utils/utils.h"
#include "stdio.h"

void ro_theme_preload_template(ro_theme* theme, char* template_name,
    int* total_winicon, int* total_indirected)
{
  int winicon, indirected;
  if (wimp_load_template(0,0,0, (byte*)0xffffffff, template_name,0, &winicon, &indirected) == 0)
    fprintf(stderr, "Template not found!!!!!!!!!!!\n");
  *total_winicon = *total_winicon + winicon + 32;
  *total_indirected = *total_indirected + indirected + 32;
  return;
}

wimp_window* ro_theme_load_template(ro_theme* theme, char* template_name, char** indirected_ptr, char* indirected_end)
{
  int winicon, indirected;
  int temp;
  wimp_window* window;
  char* old_indirected = *indirected_ptr;

  if (wimp_load_template(0,0,0, (byte*)0xffffffff, template_name, 0, &winicon, &indirected) == 0)
    fprintf(stderr, "Template not found!!!!!!!!!!!\n");

fprintf(stderr, "Allocating %d bytes for window / icon data:\n",winicon);
fprintf(stderr, "Require %d bytes for indirected data:\n",indirected);
fprintf(stderr, "Indirected before %d:\n",(int)indirected_ptr);

  window = (wimp_window*) xcalloc(winicon + 1024, 1);
  if (wimp_load_template(window, (*indirected_ptr), indirected_end,
    (byte*)0xffffffff, template_name, 0, &temp, &temp) == 0)
    fprintf(stderr, "Template not found!!!!!!!!!!!\n");

  *indirected_ptr = *indirected_ptr + indirected;
fprintf(stderr, "Indirected after %d:\n",(int)*indirected_ptr);
fprintf(stderr, "Difference: %d\n",(int)*indirected_ptr - (int)old_indirected);

  //*winicon_ptr = old_winicon + winicon + 8 - ((int)(old_winicon + winicon) % 4);

//  *indirected_ptr = old_indirected + indirected + 8 - ((int)(old_indirected + indirected) % 4);

  return window;
}

ro_theme* ro_theme_create(char* pathname)
{
  char filename[1024];

  int winicon, indirected;
  char* winicon_ptr;
  char* indirected_ptr;

  ro_theme* theme;
  messagetrans_file_flags flags;
  int size;

  FILE * fp;
  int i;


  theme = (ro_theme*) xcalloc(sizeof(ro_theme), 1);

fprintf(stderr, "Loading templates...\n");
  sprintf(filename, "%s.Templates", pathname);
fprintf(stderr, "%s\n",filename);

  winicon = 0;
  indirected = 0;

  wimp_open_template(filename);
fprintf(stderr, "Preload theme_info\n");
  ro_theme_preload_template(theme, "theme_info\0 ", &winicon, &indirected);
fprintf(stderr, "Preload toolbar\n");
  ro_theme_preload_template(theme, "toolbar\0    ", &winicon, &indirected);

fprintf(stderr, "Allocate %d bytes of indirected data\n",indirected + 16);
  theme->indirected_data = xcalloc(sizeof(char), indirected + 16);
  indirected_ptr = theme->indirected_data;
fprintf(stderr, "Allocate %d bytes of window and icon data\n",winicon + 16);
//  theme->window_and_icon_data = xcalloc(sizeof(char), winicon + 16);
//  winicon_ptr = theme->window_and_icon_data;

fprintf(stderr, "Load toolbar\n");
  theme->toolbar =
    ro_theme_load_template(theme, "toolbar\0    ", &indirected_ptr, theme->indirected_data + indirected + 16);
fprintf(stderr, "Load theme_info\n");
  theme->theme_info =
    ro_theme_load_template(theme, "theme_info\0 ", &indirected_ptr, theme->indirected_data + indirected + 16);

fprintf(stderr, "Close template\n");
  wimp_close_template();

fprintf(stderr, "Loading icon names...\n");
  sprintf(filename, "%s.IconNames", pathname);
fprintf(stderr, "%s\n", filename);
  messagetrans_file_info(filename, &flags, &size);

fprintf(stderr, "Allocating %d bytes for icon names data\n", size);
  theme->iconNames.data = xcalloc(size, sizeof(char));
fprintf(stderr, "Allocating %d bytes for filename\n", strlen(filename)+2);
  theme->iconNames.filename = xcalloc(strlen(filename)+2, sizeof(char));
  strcpy(theme->iconNames.filename, filename);
fprintf(stderr, "Opening messagetrans file\n");
  messagetrans_open_file(&theme->iconNames.cb, theme->iconNames.filename,
                         theme->iconNames.data);



fprintf(stderr, "Loading icon sizes...\n");
  sprintf(filename, "%s.IconSizes", pathname);
fprintf(stderr, "%s\n", filename);
  messagetrans_file_info(filename, &flags, &size);

fprintf(stderr, "Allocating %d bytes for icon sizes data\n", size);
  theme->iconSizes.data = xcalloc(size, sizeof(char));
fprintf(stderr, "Allocating %d bytes for filename\n", strlen(filename)+2);
  theme->iconSizes.filename = xcalloc(strlen(filename)+2, sizeof(char));
  strcpy(theme->iconSizes.filename, filename);
fprintf(stderr, "Opening messagetrans file\n");
  messagetrans_open_file(&theme->iconSizes.cb, theme->iconSizes.filename,
                         theme->iconSizes.data);



  sprintf(filename, "%s.Sprites", pathname);
  fp = fopen(filename, "rb");
  if (fp == 0) die("Failed to open file");
  if (fseek(fp, 0, SEEK_END) != 0) die("fseek() failed");
  if ((size = (int) ftell(fp)) == -1) die("ftell() failed");
  fclose(fp);

  theme->sprites = xcalloc(size + 16, 1);
  if (theme->sprites == NULL)
    die("Can't claim memory for theme sprites");

  theme->sprites->size = size + 16;
  theme->sprites->sprite_count = 0;
  theme->sprites->first = 16;
  theme->sprites->used = 16;
  osspriteop_clear_sprites(osspriteop_USER_AREA, theme->sprites);
  osspriteop_load_sprite_file(osspriteop_USER_AREA, theme->sprites, filename);

  fprintf(stderr, "sprites loaded. %d counted\n", theme->sprites->sprite_count);
  theme->throbs = 0;
  for (i = 1; i <= theme->sprites->sprite_count; i++)
  {
    char name[32];
    fprintf(stderr, "returning name for %d\n", i);
    osspriteop_return_name(osspriteop_USER_AREA, theme->sprites, name, 32, i);
    if (strncmp(name, "throbber", 8) == 0)
    {
      int this_number = atoi(name+8);
      if (this_number > theme->throbs)
        theme->throbs = this_number;
    }
  }
  fprintf(stderr, "%d throbbers found.\n", theme->throbs);

fprintf(stderr, "Returning theme...\n");
  return theme;
}

wimp_i ro_theme_icon(ro_theme* theme, theme_window_type type, const char* token)
{
  int used;
  char buffer[32];
  
  messagetrans_lookup(&theme->iconNames.cb, token, buffer, 32, 0,0,0,0, &used);
  if (used > 0)
    return atoi(buffer);
  else
    return -1;
}

void ro_theme_set_indirected(ro_theme* theme, char* find, wimp_window* win, theme_window_type wintype, char* indirected)
{
  int context = 0, used;
  char token[32];

fprintf(stderr, "setting indirected text...\n");
fprintf(stderr, "enumerating token '%s'\n", find);

  while (messagetrans_enumerate_tokens(&theme->iconNames.cb, find, token, 32, context, &used, &context))
  {
    fprintf(stderr, "finding theme icon to set indirected text\n");
    win->icons[ro_theme_icon(theme, wintype, token)].data.indirected_text.text = indirected;
  }
fprintf(stderr, "<-- returning\n");
}

wimp_w ro_theme_create_window(ro_theme* theme, ro_theme_window* create)
{
  wimp_window* win = NULL;

  if (create == NULL)
    return 0;

  if (create->type == THEME_TOOLBAR)
  {
  int i;
fprintf(stderr, "Creating toolbar from theme (%d bytes)\n", sizeof(*theme->toolbar));
for (i = 0; i < 152/4; i++)
{
  fprintf(stderr, "%d:\x09%d\n", i,((int*)(theme->toolbar))[i]);
}
    //win = xcalloc(1024, 1);
    //memcpy(win, theme->toolbar, 1024);
    win = theme->toolbar;
    win->flags = win->flags | wimp_WINDOW_FURNITURE_WINDOW;

//    win->next = wimp_TOP;
//    win->title_flags = wimp_ICON_TEXT;
    win->sprite_area = theme->sprites;
    ro_theme_set_indirected(theme, "TOOLBAR_URL*", win, THEME_TOOLBAR, create->data.toolbar.indirected_url);
    ro_theme_set_indirected(theme, "TOOLBAR_STATUS*", win, THEME_TOOLBAR, create->data.toolbar.indirected_status);
  }
  else if (create->type == THEME_THEMEINFO)
  {
fprintf(stderr, "Creating theme_info from theme (%d bytes)\n", sizeof(*theme->theme_info));
//    win = xcalloc(sizeof(*theme->theme_info), 1);
//    memcpy(win, theme->toolbar, sizeof(*theme->theme_info));
    win = theme->theme_info;
  }

  if (win != NULL)
  {
    wimp_w w = wimp_create_window(win);
    //xfree(win);
    return w;
  }
  else
    return 0;
}

int ro_theme_toolbar_height(ro_theme* theme)
{
  return abs(theme->toolbar->extent.y1 - theme->toolbar->extent.y0);
}

void ro_theme_resize(ro_theme* theme, theme_window_type wintype, wimp_w w, int width, int height)
{
  int context = 0, used;
  char token[32];
  char* find;
  char formula[256];
  char buffer[256];
  char widths[32];
  char heights[32];

  sprintf(widths, "%d", width);
  sprintf(heights, "%d", height);

  if (wintype == THEME_TOOLBAR)
    find = "TOOLBAR_*";
  else
    return;

  while (messagetrans_enumerate_tokens(&theme->iconSizes.cb, find, token, 32, context, &used, &context))
  {
    char* x0;// = strstr(token, "_X0");
    char* x1;// = strstr(token, "_X1");

    x0 = strstr(token, "_X0");
    x1 = strstr(token, "_X1");

    if (x0 != 0 || x1 != 0)
    {
      char* icon_num = token + strlen(find) - 1;
      char* underscore = token + strlen(find) - 1;
      wimp_i i;
      int new_x, rx0, rx1;

      messagetrans_lookup(&theme->iconSizes.cb, token, formula, 255, widths, heights,0,0, &used);

      while (*underscore > 32)
      {
        if (*underscore == '_')
          *underscore = '\0';
        underscore++;
      }
      
      i = (wimp_i) atoi(icon_num);

      if (os_evaluate_expression(formula, buffer, 255, &new_x) == 0)
      {
        wimp_icon_state ic;

        ic.w = w;
        ic.i = i;
        wimp_get_icon_state(&ic);

        rx0 = ic.icon.extent.x0;
        rx1 = ic.icon.extent.x1;

        if (x0 != 0)
        {
          if (new_x < rx0)
            rx0 = new_x;
          wimp_resize_icon(w, i, new_x, ic.icon.extent.y0, ic.icon.extent.x1, ic.icon.extent.y1);
        }
        else if (x1 != 0)
        {
          if (new_x > rx1)
            rx1 = new_x;
          wimp_resize_icon(w, i, ic.icon.extent.x0, ic.icon.extent.y0, new_x, ic.icon.extent.y1);
        }
        wimp_force_redraw(w, rx0, ic.icon.extent.y0, rx1, ic.icon.extent.y1);
      }
    }
  }
}

