/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
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

#include "netsurf/desktop/netsurf.h"
#include "netsurf/riscos/about.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"

#include "oslib/fileswitch.h"
#include "oslib/osargs.h"
#include "oslib/osfile.h"
#include "oslib/osfind.h"
#include "oslib/osfscontrol.h"
#include "oslib/osgbpb.h"

static const char *pabouthdr = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/transitional.dtd\"><html><head><title>%s</title></head><body bgcolor=\"#f3f3ff\"><!-- About header --><table border=\"0\" width=\"100%%\" bgcolor=\"#94adff\" cellspacing=\"2\"><tr><td><a href=\"http://netsurf.sf.net\"><img src=\"file:///%%3CNetSurf$Dir%%3E/About/nslogo\" alt=\"Netsurf logo\"></a><td><table bgcolor=\"#94adff\" border=\"0\"><tr><td>&nbsp;<tr><td align=\"center\"><h2>NetSurf %s</h2><tr><td align=\"center\"><h5>Copyright &copy; 2002, 2003 NetSurf Developers.</h5><tr><td>&nbsp;</table></table><hr>"; /**< About page header */
static const char *pabtplghd = "<!-- Plugin information --><strong><i>The following plugins are installed on your system:</i></strong><br>&nbsp;<br><table border=\"0\" cellspacing=\"2\" width=\"100%\">"; /**< Plugin table header */
static const char *paboutpl1 = "<tr valign=\"top\"><td width=\"30%%\"><font size=\"2\"><strong>%s</strong></font></td><td width=\"70%%\"><font size=\"2\">%s</font></td></tr><tr><td colspan=\"2\" bgcolor=\"#dddddd\" height=\"1\"></td></tr>"; /**< Plugin entry without image */
static const char *paboutpl2 = "<tr valign=\"top\"><td width=\"30%%\"><font size=\"2\"><strong>%s</strong></font><br><img src=\"%s\" alt=\"%s\"></td><td width=\"70%%\"><font size=\"2\">%s</font></td></tr><tr><td colspan=\"2\" bgcolor=\"#dddddd\" height=\"1\"></td></tr>";/**< Plugin entry with image (filename=nn) */
static const char *paboutpl3 = "<tr valign=\"top\"><td width=\"30%%\"><font size=\"2\"><strong>%s</strong></font><br><img src=\"%s\" alt=\"%s\" width=\"%d\" height=\"%d\"></td><td width=\"70%%\"><font size=\"2\">%s</font></td></tr><tr><td colspan=\"2\" bgcolor=\"#dddddd\" height=\"1\"></td></tr>"; /**< Plugin entry with image (filename=nnwwwwhhhh) */
static const char *pabtplgft = "</table>"; /**< Plugin table footer */
static const char *paboutftr = "</div></body></html>"; /**< Page footer */

/** The about page */
struct about_page {

  char *header;           /**< page header */
  char *browser;          /**< browser details */
  char *plghead;          /**< plugin header */
  struct plugd *plugd;    /**< plugin details list */
  char *plgfoot;          /**< plugin footer */
  char *footer;           /**< page footer */
};

/** A set of plugin details */
struct plugd {

  char *details;          /**< plugin details */
  struct plugd *next;     /**< next plugin details */
};

struct plugd *new_plugin(struct plugd *pd, char* details);

/**
 * Adds a plugin's details to the head of the linked list of plugin details
 * Returns the new head of the list
 */
struct plugd *new_plugin(struct plugd *pd, char* details) {

  struct plugd *np = xcalloc(1, sizeof(*np));

  np->details = 0;
  np->details = details;

  np->next = pd;
  return np;
}

/**
 * Creates the about page and stores it in <Wimp$ScrapDir>.WWW.Netsurf
 */
void about_create(void) {

  struct about_page *abt;
  struct plugd *temp;
  FILE *fp;
  char *buf, *val, var[20], *ptype, *pdetails, *fname, *furl;
  int i, nofiles, j, w, h, size;
  fileswitch_object_type fot;
  os_error *e;

  abt = (struct about_page*)xcalloc(1, sizeof(*abt));
  abt->plugd = 0;

  /* Page header */
  buf = xcalloc(strlen(pabouthdr) + 50, sizeof(char));
  snprintf(buf, strlen(pabouthdr) + 50, pabouthdr, "About NetSurf",
           netsurf_version);
  abt->header = xstrdup(buf);
  xfree(buf);

  /* browser details */
  xosfile_read_stamped_no_path("<NetSurf$Dir>.About.About",0,0,0,&i,0,0);
  fp = fopen("<NetSurf$Dir>.About.About", "r");
  buf = xcalloc((unsigned int)i + 10, sizeof(char));
  fread(buf, sizeof(char), (unsigned int)i, fp);
  fclose(fp);
  abt->browser = xstrdup(buf);
  xfree(buf);

  /* plugin header */
  abt->plghead = xstrdup(pabtplghd);

  /* plugin footer */
  abt->plgfoot = xstrdup(pabtplgft);

  /* Page footer */
  abt->footer = xstrdup(paboutftr);

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
            abt->plugd = new_plugin(abt->plugd, furl);
            xfree(pdetails);
            continue;
          }
          else {
            /* Type 3: image file with name xxwwwwhhhh */
            /* get actual file name */
            sprintf(var, "%2.2d*", j);
            LOG(("var: %s", var));

            void *name = (void*)xcalloc((unsigned int)20, sizeof(char));

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
            abt->plugd = new_plugin(abt->plugd, furl);
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
          abt->plugd = new_plugin(abt->plugd, furl);
          xfree(fname);
          xfree(pdetails);
        }
      }
      if (buf != 0) {
        xfree(buf);
      }
    }
  }

  /* write file */
  xosfile_create_dir("<Wimp$ScrapDir>.WWW", 77);
  xosfile_create_dir("<Wimp$ScrapDir>.WWW.NetSurf", 77);

  fp = fopen("<Wimp$ScrapDir>.WWW.Netsurf.About", "w+");
  fprintf(fp, "%s", abt->header);
  fprintf(fp, "%s", abt->browser);
  fprintf(fp, "%s", abt->plghead);
  while (abt->plugd != 0) {
    fprintf(fp, "%s", abt->plugd->details);
    temp = abt->plugd;
    abt->plugd = abt->plugd->next;
    xfree(temp);
  }
  fprintf(fp, "%s", abt->plgfoot);
  fprintf(fp, "%s", abt->footer);
  fclose(fp);

  xosfile_set_type("<Wimp$ScrapDir>.WWW.NetSurf.About", 0xfaf);

  xfree(abt);

  return;
}

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

/**
 * Clean up created files
 */
void about_quit(void) {

  xosfile_delete("<Wimp$ScrapDir>.WWW.NetSurf.About", 0, 0, 0, 0, 0);
  xosfile_delete("<Wimp$ScrapDir>.WWW.NetSurf.Cookies", 0, 0, 0, 0, 0);
}

