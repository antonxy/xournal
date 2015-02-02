/*
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/stat.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgnomecanvas/libgnomecanvas.h>

#include "xournal.h"
#include "xo-interface.h"
#include "xo-support.h"
#include "xo-callbacks.h"
#include "xo-misc.h"
#include "xo-file.h"
#include "xo-paint.h"
#include "xo-shapes.h"

GtkWidget *winMain;
GnomeCanvas *canvas;

struct Journal journal; // the journal
struct BgPdf bgpdf;  // the PDF loader stuff
struct UIData ui;   // the user interface data
struct UndoItem *undo, *redo; // the undo and redo stacks

double DEFAULT_ZOOM;

void init_stuff (int argc, char *argv[])
{
  GtkWidget *w;
  GList *dev_list;
  GdkDevice *device;
  GdkScreen *screen;
  int i, j;
  struct Brush *b;
  gboolean can_xinput, success;
  gchar *tmppath, *tmpfn;

  // create some data structures needed to populate the preferences
  ui.default_page.bg = g_new(struct Background, 1);

  // initialize config file names
  tmppath = g_build_filename(g_get_home_dir(), CONFIG_DIR, NULL);
  mkdir(tmppath, 0700); // safer (MRU data may be confidential)
  ui.mrufile = g_build_filename(tmppath, MRU_FILE, NULL);
  ui.configfile = g_build_filename(tmppath, CONFIG_FILE, NULL);
  g_free(tmppath);

  // initialize preferences
  init_config_default();
  load_config_from_file();
  ui.font_name = g_strdup(ui.default_font_name);
  ui.font_size = ui.default_font_size;
  ui.hiliter_alpha_mask = 0xffffff00 + (guint)(255*ui.hiliter_opacity);

  // we need an empty canvas prior to creating the journal structures
  canvas = GNOME_CANVAS (gnome_canvas_new_aa ());

  // initialize data
  ui.default_page.bg->canvas_item = NULL;
  ui.layerbox_length = 0;

  if (argc > 3 || (argc == 2 && argv[1][0] == '-')) {
    printf(_("Invalid command line parameters.\n"
           "Usage: %s [filename.xoj]\n"), argv[0]);
    gtk_exit(0);
  }
   
  undo = NULL; redo = NULL;
  journal.pages = NULL;
  bgpdf.status = STATUS_NOT_INIT;

  new_journal();  
  
  ui.cur_item_type = ITEM_NONE;
  ui.cur_item = NULL;
  ui.cur_path.coords = NULL;
  ui.cur_path_storage_alloc = 0;
  ui.cur_path.ref_count = 1;
  ui.cur_widths = NULL;
  ui.cur_widths_storage_alloc = 0;

  ui.selection = NULL;
  ui.cursor = NULL;
  ui.pen_cursor_pix = ui.hiliter_cursor_pix = NULL;

  ui.cur_brush = &(ui.brushes[0][ui.toolno[0]]);
  for (j=0; j<=NUM_BUTTONS; j++)
    for (i=0; i < NUM_STROKE_TOOLS; i++) {
      b = &(ui.brushes[j][i]);
      b->tool_type = i;
      if (b->color_no>=0) {
        b->color_rgba = predef_colors_rgba[b->color_no];
        if (i == TOOL_HIGHLIGHTER) {
          b->color_rgba &= ui.hiliter_alpha_mask;
        }
      }
      b->thickness = predef_thickness[i][b->thickness_no];
    }
  for (i=0; i<NUM_STROKE_TOOLS; i++)
    g_memmove(ui.default_brushes+i, &(ui.brushes[0][i]), sizeof(struct Brush));

  ui.cur_mapping = 0;
  ui.which_unswitch_button = 0;

  // set up the page size and canvas size
  update_page_stuff();
  g_object_set_data (G_OBJECT (winMain), "canvas", canvas);

  screen = gtk_widget_get_screen(winMain);
  ui.screen_width = gdk_screen_get_width(screen);
  ui.screen_height = gdk_screen_get_height(screen);
  

  // and finally, open a file specified on the command line
  // (moved here because display parameters weren't initialized yet...)
  
  if (argc == 1) return;
  if (g_path_is_absolute(argv[1]))
    tmpfn = g_strdup(argv[1]);
  else {
    tmppath = g_get_current_dir();
    tmpfn = g_build_filename(tmppath, argv[1], NULL);
    g_free(tmppath);
  }
  success = open_journal(tmpfn);
  g_free(tmpfn);
  if (!success) {
    w = gtk_message_dialog_new(GTK_WINDOW (winMain), GTK_DIALOG_DESTROY_WITH_PARENT,
       GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, _("Error opening file '%s'"), argv[1]);
    gtk_dialog_run(GTK_DIALOG(w));
    gtk_widget_destroy(w);
  }
}


int
main (int argc, char *argv[])
{
#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif
  
  gtk_set_locale ();
  gtk_init (&argc, &argv);

  /*
   * The following code was added by Glade to create one of each component
   * (except popup menus), just so that you see something after building
   * the project. Delete any components that you don't want shown initially.
   */
  winMain = create_winMain ();
  
  init_stuff (argc, argv);
  print_to_pdf(argv[2]);
//gtk_main ();
  
  if (bgpdf.status != STATUS_NOT_INIT) shutdown_bgpdf();
  
  return 0;
}

