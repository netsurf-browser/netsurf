/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003,4 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * About page creation.
 * Dynamically creates the about page, scanning for available plugin information.
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unixlib/local.h> /* for __unixify */
#include "oslib/fileswitch.h"
#include "oslib/osargs.h"
#include "oslib/osfile.h"
#include "oslib/osfind.h"
#include "oslib/osfscontrol.h"
#include "oslib/osgbpb.h"
#include "netsurf/utils/config.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"

#ifdef WITH_ABOUT

static const char *pabouthdr = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/transitional.dtd\"><html><head><title>%s</title></head><body bgcolor=\"#f3f3ff\"><!-- About header --><table border=\"0\" width=\"100%%\" bgcolor=\"#94adff\" cellspacing=\"2\"><tr><td><a href=\"http://netsurf.sf.net\"><img src=\"file:///%%3CNetSurf$Dir%%3E/About/nslogo\" alt=\"Netsurf logo\"></a><td><table bgcolor=\"#94adff\" border=\"0\"><tr><td>&nbsp;<tr><td align=\"center\"><h2>NetSurf %s</h2><tr><td align=\"center\"><h5>Copyright &copy; 2002, 2003 NetSurf Developers.</h5><tr><td>&nbsp;</table></table><hr>"; /**< About page header */
static const char *pabtplghd = "<!-- Plugin information --><strong><i>The following plugins are installed on your system:</i></strong><br>&nbsp;<br><table border=\"0\" cellspacing=\"2\" width=\"100%\">"; /**< Plugin table header */
static const char *paboutpl1 = "<tr valign=\"top\"><td width=\"30%%\"><font size=\"2\"><strong>%s</strong></font></td><td width=\"70%%\"><font size=\"2\">%s</font></td></tr><tr><td colspan=\"2\" bgcolor=\"#dddddd\" height=\"1\"></td></tr>"; /**< Plugin entry without image */
static const char *paboutpl2 = "<tr valign=\"top\"><td width=\"30%%\"><font size=\"2\"><strong>%s</strong></font><br><img src=\"%s\" alt=\"%s\"></td><td width=\"70%%\"><font size=\"2\">%s</font></td></tr><tr><td colspan=\"2\" bgcolor=\"#dddddd\" height=\"1\"></td></tr>";/**< Plugin entry with image (filename=nn) */
static const char *paboutpl3 = "<tr valign=\"top\"><td width=\"30%%\"><font size=\"2\"><strong>%s</strong></font><br><img src=\"%s\" alt=\"%s\" width=\"%d\" height=\"%d\"></td><td width=\"70%%\"><font size=\"2\">%s</font></td></tr><tr><td colspan=\"2\" bgcolor=\"#dddddd\" height=\"1\"></td></tr>"; /**< Plugin entry with image (filename=nnwwwwhhhh) */
static const char *pabtplgft = "</table>"; /**< Plugin table footer */
static const char *paboutftr = "</div></body></html>"; /**< Page footer */



/**
 * Create the browser about page.
 *
 * \param  url       requested url (about:...)
 * \param  callback  content callback function, for content_add_user()
 * \param  p1        user parameter for callback
 * \param  p2        user parameter for callback
 * \param  width     available width
 * \param  height    available height
 * \return  a new content containing the about page
 */

struct content *about_create(const char *url,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, const char *error),
		void *p1, void *p2, unsigned long width, unsigned long height)
{
  struct content *c = 0;
  FILE *fp;
  char *buf, *val, var[20], *ptype, *pdetails, *fname, *furl;
  int i, nofiles, j, w, h, size;
  fileswitch_object_type fot;
  os_error *e;
  const char *params[] = { 0 };

  c = content_create(url);
  c->width = width;
  c->height = height;
  content_add_user(c, callback, p1, p2);
  content_set_type(c, CONTENT_HTML, "text/html", params);

  /* Page header */
  buf = xcalloc(strlen(pabouthdr) + 50, sizeof(char));
  snprintf(buf, strlen(pabouthdr) + 50, pabouthdr, "About NetSurf",
           netsurf_version);
  content_process_data(c, buf, strlen(buf));
  free(buf);

  /* browser details */
  buf = load("<NetSurf$Dir>.About.About");
  content_process_data(c, buf, strlen(buf));
  free(buf);

  /* plugin header */
  content_process_data(c, pabtplghd, strlen(pabtplghd));

  /* plugins registered */
  for (i=0; i!=4096; i++) {
    sprintf(var, "Plugin$About_%3.3x", i);

    if ((val = getenv(var)) != 0) {
      /* Plugin Name */
      sprintf(var, "Plugin$Type_%3.3x", i);
      ptype = getenv(var);

      buf = xcalloc(strlen(val) + 20, sizeof(char));
      /* count files which match <Plugin$About_i>.About* */
      sprintf(buf, "%s.About*", val);
      xosfscontrol_count(buf,0,0,0,0,0,0,&nofiles);

      for (j=0; j!=nofiles; j++) {
        /* get plugin details */
        if (j == 0) {
          sprintf(buf, "%s.About", val);
        }
        else {
          sprintf(buf, "%s.About%2.2d", val, j);
        }
        e = xosfile_read_stamped_no_path(buf,&fot,0,0,&size,0,0);

        /* If only one file, name can be "About" or "About00" */
        if(e || (j == 0 && (int)fot != 1)) {
          sprintf(buf, "%s.About%2.2d", val, j);
          e = xosfile_read_stamped_no_path(buf,&fot,0,0,&size,0,0);
        }
        /* ok, no file found. try again */
        if(e || (int)fot != 1) {
          continue;
        }

        /* read contents of file */
        fp = fopen(buf, "r");
        pdetails = xcalloc((unsigned int)size + 10, sizeof(char));
        fread(pdetails, sizeof(char), (unsigned int)size, fp);
        fclose(fp);

        /* now see if there's an image to display */
        sprintf(buf, "%s.%2.2d", val, j);
        LOG(("buf: %s", buf));
        e = xosfile_read_stamped_no_path(buf,&fot,0,0,0,0,0);

        if(e || (int)fot != 1) {
          sprintf(buf, "%s.%2.2d*", val, j);
          LOG(("buf: %s", buf));
          e = xosfile_read_stamped_no_path(buf,&fot,0,0,0,0,0);

          if(e || (int)fot != 1) {
            /* Type 1: no image file */
            furl = xcalloc(strlen(paboutpl1) + strlen(ptype) + strlen(pdetails) + 10, sizeof(char));
            sprintf(furl, paboutpl1, ptype, pdetails);
            LOG(("furl: %s", furl));
            content_process_data(c, furl, strlen(furl));
            xfree(pdetails);
            continue;
          }
          else {
            void *name;

            /* Type 3: image file with name xxwwwwhhhh */
            /* get actual file name */
            sprintf(var, "%2.2d*", j);
            LOG(("var: %s", var));

            name = (void*)xcalloc((unsigned int)20, sizeof(char));

            e = xosgbpb_dir_entries(val, (osgbpb_string_list*)name,
                                    1, 0, 255, var, NULL, NULL);
            if (e) {
              LOG(("%s", e->errmess));
              xfree(name);
              xfree(pdetails);
              continue;
            }
            LOG(("fname: %s", (char*)name));
            sprintf(buf, "%s.%s", val, (char*)name);
            furl = xcalloc(strlen(buf) + 20, sizeof(char));

            /* grab leafname and get width and height */
            h = atoi((char*)name+6);
            ((char*)name)[6] = 0;
            w = atoi((char*)name+2);

            xfree(name);

            /* convert to URL */
            __unixify(buf, 0, furl, strlen(buf)+20, 0);
            sprintf(buf, "file://%s", furl);
            xfree(furl);

            LOG(("furl: %s", buf));
            furl = xcalloc(strlen(paboutpl3) + strlen(ptype) + strlen(buf) +
                           strlen(pdetails) + 10, sizeof(char));
            sprintf(furl, paboutpl3, ptype, buf, ptype, w, h, pdetails);
            content_process_data(c, furl, strlen(furl));
            xfree(pdetails);
            continue;
          }
        }
        else {
          /* Type 2: image file with name xx */
          /* convert RO path to url */
          fname = xcalloc(strlen(buf) + 10, sizeof(char));
          furl = xcalloc(strlen(buf) + 10, sizeof(char));
          __unixify(buf, 0, furl, strlen(buf) + 10, 0);
          sprintf(fname, "file://%s", furl);
          xfree(furl);

          furl = xcalloc(strlen(paboutpl2) + strlen(ptype) + strlen(fname) + strlen(pdetails) + 10, sizeof(char));
          sprintf(furl, paboutpl2, ptype, fname, ptype, pdetails);
          content_process_data(c, furl, strlen(furl));
          xfree(fname);
          xfree(pdetails);
        }
      }
      if (buf != 0) {
        xfree(buf);
      }
    }
  }

  /* plugin footer */
  content_process_data(c, pabtplgft, strlen(pabtplgft));

  /* Page footer */
  content_process_data(c, paboutftr, strlen(paboutftr));

  content_convert(c, c->width, c->height);

  return c;
}

#ifdef WITH_COOKIES
/**
 * Creates the cookie list and stores it in <Wimp$ScrapDir>.WWW.Netsurf
 */
void cookie_create(void) {

  FILE *fp;
  int len, count=0;
  char *cookies = 0, *pos;
  char domain[256], flag[10], path[256], secure[10],
       exp[50], name[256], val[256];
  unsigned int expiry;

  fp = fopen(messages_get("cookiefile"), "r");
  if (!fp) {
    LOG(("Failed to open cookie jar"));
    return;
  }

  /* read file length */
  fseek(fp, 0, SEEK_END);
  len = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  cookies = xcalloc((unsigned int)len, sizeof(char));
  fread(cookies, (unsigned int)len, sizeof(char), fp);
  fclose(fp);

  xosfile_create_dir("<Wimp$ScrapDir>.WWW", 77);
  xosfile_create_dir("<Wimp$ScrapDir>.WWW.NetSurf", 77);
  fp = fopen("<Wimp$ScrapDir>.WWW.NetSurf.Cookies", "w+");
  if (!fp) {
    xfree(cookies);
    LOG(("Failed to create file"));
    return;
  }
  fprintf(fp, pabouthdr, "About NetSurf - Cookies", netsurf_version);
  fprintf(fp, "<strong><i>The following cookies are stored on your system:</i></strong><div align=\"center\"><table cellspacing=\"2\" cellpadding=\"2\" width=\"100%%\"><strong><thead><td nowrap>Domain:</td><td nowrap>Flag:</td><td nowrap>Path:</td><td nowrap>Secure:</td><td nowrap>Expiration:</td><td nowrap>Name:</td><td nowrap>Value:</td></thead></strong><tbody>");
  pos = cookies;
  while (pos != (cookies+len-1)) {
    if (*pos == '#') {
      for (; *pos != '\n'; pos++);
      pos += 1;
      continue;
    }
    sscanf(pos, "%s\t%s\t%s\t%s\t%s\t%s\t%s\n", domain, flag, path, secure,
                                                exp, name, val);
    pos += (strlen(domain) + strlen(flag) + strlen(path) + strlen(secure) +
            strlen(exp) + strlen(name) +strlen(val) + 7);
    sscanf(exp, "%u", &expiry);
    fprintf(fp, "<tr%s><td nowrap>%s</td><td nowrap>%s</td><td nowrap>%s</td><td nowrap>%s</td><td nowrap>%s</td><td nowrap>%s</td><td nowrap>%s</td></tr>", (count%2 == 0 ? " bgcolor=\"#ddddee\"" : ""), domain, flag, path, secure,
    (expiry == 0 ? "Expires on exit" : ctime((time_t*)&expiry)), name, val);
    count++;
  }

  fprintf(fp, "</tbody></table></div></body></html>");
  fclose(fp);
  xosfile_set_type("<Wimp$ScrapDir>.WWW.NetSurf.Cookies", 0xfaf);
  xfree(cookies);
  return;
}
#endif

/**
 * Clean up created files
 */
void about_quit(void) {

  xosfile_delete("<Wimp$ScrapDir>.WWW.NetSurf.About", 0, 0, 0, 0, 0);
#ifdef WITH_COOKIES
  xosfile_delete("<Wimp$ScrapDir>.WWW.NetSurf.Cookies", 0, 0, 0, 0, 0);
#endif
}

#endif
