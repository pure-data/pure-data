/*
    pdtcl main progrm, adapted from the "scribble" test program for gtk.
    Starts up gtk and tcl and opens the Pd window.
*/

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "pdgtk.h"
#include <sys/socket.h>

GtkWidget *gtk_mainwindow;

static void pdwindow_resize(GtkWidget *widget, int width, int height,
    gpointer data)
{
}

static void pdwindow_draw(GtkDrawingArea *drawing_area, cairo_t *cr, int width,
    int height, gpointer data)
{
}

static void close_window(void)
{
}

static char pdtcl_newdir[BIGSTRING];

static void pdgtk_openfile(GFile *file)
{
    GFile *parent = g_file_get_parent(file);
    char *basename = g_file_get_basename(file), *dir = g_file_get_path(parent);
    char pdcmd[BIGSTRING];

    /* fprintf(stderr, "pdgtk_openfile: pd open %s %s;\n", basename, dir); */
    snprintf(pdcmd, BIGSTRING, "pd open %s %s;\n", basename, dir);
    pdcmd[BIGSTRING-1] = 0;
    socket_send(pdcmd);
    if (!*pdtcl_newdir)
        strncpy(pdtcl_newdir, dir, BIGSTRING), pdtcl_newdir[BIGSTRING-1] = 0;
    g_free(basename);
    g_free(dir);
    g_object_unref (parent);
}

static void pdgtk_menu_open(GObject *source, GAsyncResult *result,
    gpointer user_data)
{
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    GError *error = NULL;
    GFile *file;

    file = gtk_file_dialog_open_finish(dialog, result, &error);
    if (!file)
    {
        g_error_free(error);
        return;
    }
    pdgtk_openfile(file);
    g_object_unref (file);
}

/* enquote a string for find, path, and startup dialog panels, to be decoded
    in Pd by sys_decodedialog() */

static void pdtcl_enquote_path(char *quotedir, const char *dir)
{
    int i, j;
    for (i = j = 0; dir[i] && j < BIGSTRING-2; i++)
        switch (dir[i])
    {
    case ' ':
        quotedir[j] = '+'; quotedir[j+1] = '_'; j+=2; break;
    case '$':
        quotedir[j] = '+'; quotedir[j+1] = 'd'; j+=2; break;
    case ';':
        quotedir[j] = '+'; quotedir[j+1] = 's'; j+=2; break;
    case ',':
        quotedir[j] = '+'; quotedir[j+1] = 'c'; j+=2; break;
    default:
        quotedir[j] = dir[i]; j++; break;
    }
    quotedir[j] = 0;
}

static void filemenu_new(GApplication *app, gpointer *data)
{
    char pdcmd[BIGSTRING], quotedir[BIGSTRING];
    static int untitlednumber = 0;
    if (!*pdtcl_newdir)
    {
        if (!getenv("HOME"))
            strcpy(pdtcl_newdir, "/");
        else strncpy(pdtcl_newdir, getenv("HOME"), BIGSTRING);
        pdtcl_newdir[BIGSTRING-1] = 0;
    }
    fprintf(stderr, "new\n");
    pdcmd[BIGSTRING-1] = 0;
    pdtcl_enquote_path(quotedir, pdtcl_newdir);
    if (untitlednumber)
        snprintf(pdcmd, BIGSTRING, "pd menunew UNTITLED-%s %s;\n",
            untitlednumber, quotedir);
    else snprintf(pdcmd, BIGSTRING, "pd menunew UNTITLED %s;\n", quotedir);
    untitlednumber++;
    socket_send(pdcmd);
}

static void filemenu_open(GApplication *app, gpointer *data)
{
    GtkFileDialog *dialog = gtk_file_dialog_new ();

    GtkFileFilter* filefilter = gtk_file_filter_new();
    gtk_file_filter_add_suffix(filefilter,"pd");
    gtk_file_filter_set_name(filefilter,"Pd");

    GListStore* liststore = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(liststore, filefilter);

    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(liststore));

    gtk_file_dialog_open (dialog, GTK_WINDOW(gtk_mainwindow),
        NULL, pdgtk_menu_open, data);
    g_object_unref (dialog);
}

static void filemenu_quit(GApplication *app, gpointer *data)
{
    fprintf(stderr, "pd verifyquit\n");
    socket_send("pd verifyquit;\n");
}

static int pdgtk_havemainwindow;
#ifdef __APPLE__
#define ACCELKEY "<Meta>"
#else
#define ACCELKEY "<Control>"
#endif

static void pdgtk_menuitem(GtkApplication *app, GMenu *menu, char *name,
    char *action, char *accel)
{
    const char *twoaccels[2] = {0, 0};
    GMenuItem *menu_item = g_menu_item_new(name, action);
    g_menu_append_item(menu, menu_item);
    g_object_unref(menu_item);
    twoaccels[0] = accel;
    gtk_application_set_accels_for_action(app, action, twoaccels);
}

static void pdgtk_startup(GtkApplication *app, gpointer user_data)
{
    const char *twoaccels[2] = {0, 0};
    GMenu *menubar, *filemenu, *editmenu, *putmenu, *section;
    GMenuItem *menu_item_thismenu, *menu_item,
        *menu_item_new, *menu_item_open,
        *menu_item_save, *menu_item_saveas, *menu_item_close, *menu_item_quit;
    GSimpleAction *act_new, *act_save, *act_quit;
    GSimpleAction *action_open, *action;

        /* set up menu - there's only one, not one per window. */
    menubar = g_menu_new();

        /* ---------------- "file" menu --------------- */
    menu_item_thismenu = g_menu_item_new(_("File"), NULL);
    filemenu = g_menu_new();

    section = g_menu_new();

    pdgtk_menuitem(app, section, _("Open"), "app.open", ACCELKEY "o");

    act_new = g_simple_action_new("new", NULL);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(act_new));
    g_signal_connect(act_new, "activate", G_CALLBACK(filemenu_new), NULL);
    pdgtk_menuitem(app, section, _("New"), "app.new", ACCELKEY "n");

    g_menu_append_section(filemenu, 0, G_MENU_MODEL(section));
    g_object_unref(section);

    section = g_menu_new();

    pdgtk_menuitem(app, section, _("Save"), "win.zsave", ACCELKEY "s");
    pdgtk_menuitem(app, section, _("Save as"), "win.zsaveas", ACCELKEY "S");
    pdgtk_menuitem(app, section, _("Close"), "win.zclose", ACCELKEY "w");

    act_quit = g_simple_action_new("quit", NULL);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(act_quit));
    g_signal_connect(act_quit, "activate", G_CALLBACK(filemenu_quit), NULL);
    pdgtk_menuitem(app, section, _("Quit"), "app.quit", ACCELKEY "q");

    g_menu_append_section(filemenu, 0, G_MENU_MODEL(section));
    g_object_unref(section);

    g_menu_item_set_submenu(menu_item_thismenu, G_MENU_MODEL(filemenu));
    g_menu_append_item(menubar, menu_item_thismenu);
    g_object_unref(menu_item_thismenu);

        /* ---------------- "edit" menu --------------- */
    menu_item_thismenu = g_menu_item_new(_("Edit"), NULL);
    editmenu = g_menu_new();

    section = g_menu_new();
    pdgtk_menuitem(app, section, _("Undo"), "win.zundo", ACCELKEY "z");
    pdgtk_menuitem(app, section, _("Redo"), "win.zredo", ACCELKEY "Z");
    g_menu_append_section(editmenu, 0, G_MENU_MODEL(section));
    g_object_unref(section);

    section = g_menu_new();
    pdgtk_menuitem(app, section, _("Cut"), "win.zcut", ACCELKEY "x");
    pdgtk_menuitem(app, section, _("Copy"), "win.zcopy", ACCELKEY "c");
    pdgtk_menuitem(app, section, _("Paste"), "win.zpaste", ACCELKEY "v");
    pdgtk_menuitem(app, section, _("Duplicate"), "win.zduplicate",
        ACCELKEY "d");
    pdgtk_menuitem(app, section, _("Edit"), "win.zedit", ACCELKEY "e");
    g_menu_append_section(editmenu, 0, G_MENU_MODEL(section));
    g_object_unref(section);

    g_menu_item_set_submenu(menu_item_thismenu, G_MENU_MODEL(editmenu));
    g_menu_append_item(menubar, menu_item_thismenu);
    g_object_unref(menu_item_thismenu);

        /* ---------------- "put" menu --------------- */
    menu_item_thismenu = g_menu_item_new(_("Put"), NULL);
    putmenu = g_menu_new();

    section = g_menu_new();
    pdgtk_menuitem(app, section, _("Object"), "win.zputobj", ACCELKEY "1");
    pdgtk_menuitem(app, section, _("Message"), "win.zputmsg", ACCELKEY "2");
    pdgtk_menuitem(app, section, _("Number"), "win.zputnum", ACCELKEY "3");
    pdgtk_menuitem(app, section, _("List"), "win.zputlist", ACCELKEY "4");
    pdgtk_menuitem(app, section, _("Comment"), "win.zputtext", ACCELKEY "5");
    g_menu_append_section(putmenu, 0, G_MENU_MODEL(section));
    g_object_unref(section);

    section = g_menu_new();
    pdgtk_menuitem(app, section, _("Bang"), "win.zputbang", ACCELKEY "B");
    pdgtk_menuitem(app, section, _("Toggle"), "win.zputtgl", ACCELKEY "B");
    pdgtk_menuitem(app, section, _("Number2"), "win.zputn2", ACCELKEY "N");
    pdgtk_menuitem(app, section, _("Vslider"), "win.zputvsl", ACCELKEY "V");
    pdgtk_menuitem(app, section, _("Hslider"), "win.zputhsl", ACCELKEY "J");
    pdgtk_menuitem(app, section, _("Vradio"), "win.zputvrad", ACCELKEY "D");
    pdgtk_menuitem(app, section, _("Hradio"), "win.zputhrad", ACCELKEY "I");
    g_menu_append_section(putmenu, 0, G_MENU_MODEL(section));
    g_object_unref(section);

    section = g_menu_new();
    pdgtk_menuitem(app, section, _("Graph"), "win.zputgraph", ACCELKEY "G");
    pdgtk_menuitem(app, section, _("Array"), "win.zputarray", ACCELKEY "A");
    g_menu_append_section(putmenu, 0, G_MENU_MODEL(section));
    g_object_unref(section);

    g_menu_item_set_submenu(menu_item_thismenu, G_MENU_MODEL(putmenu));
    g_menu_append_item(menubar, menu_item_thismenu);
    g_object_unref(menu_item_thismenu);



    gtk_application_set_menubar(GTK_APPLICATION(app), G_MENU_MODEL(menubar));

        /* handle "open" actions sent from OS - should this be here? */
    action_open = g_simple_action_new("open", NULL);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(action_open));
    g_signal_connect(action_open, "activate", G_CALLBACK(filemenu_open), NULL);
}

static void pdgtk_createmainwindow(GtkApplication *app, gpointer user_data)
{
    GtkWidget *frame;
    GtkWidget *drawing_area;

    if (pdgtk_havemainwindow)
        return;
    gtk_mainwindow = gtk_application_window_new(app);
    gtk_window_set_title (GTK_WINDOW(gtk_mainwindow), "Pure Data GTK");
    g_signal_connect(gtk_mainwindow, "destroy", G_CALLBACK(close_window),
        NULL);

        /* make drawing area */
    frame = gtk_frame_new (NULL);
    gtk_window_set_child(GTK_WINDOW(gtk_mainwindow), frame);
    drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(drawing_area, 300, 300);
    gtk_frame_set_child(GTK_FRAME (frame), drawing_area);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area),
        pdwindow_draw, NULL, NULL);
    g_signal_connect_after(drawing_area, "resize", G_CALLBACK(pdwindow_resize),
        NULL);

    gtk_application_window_set_show_menubar(
         GTK_APPLICATION_WINDOW(gtk_mainwindow), TRUE);
    gtk_window_present(GTK_WINDOW(gtk_mainwindow));
    pdgtk_havemainwindow = 1;
}

static void pdgtk_activate(GtkApplication *app, gpointer user_data)
{
    pdgtk_createmainwindow(app, user_data);
}

static void pdgtk_open(GtkApplication *app, GFile** files, gint n_files,
    const gchar* hint, gpointer user_data)
{
    int i;
    for (i = 0; i < n_files; i++)
         pdgtk_openfile(files[i]);
}

static gboolean cmd_do_watchdog(void* data)
{
    socket_send("pd watchdog;\n");
    return TRUE;
}

void pdgtk_start_watchdog( void)
{
    g_timeout_add_seconds(2, cmd_do_watchdog, 0);
    socket_send("pd watchdog;\n");
}


int pdsockfd;
void socket_callback( void);

static gboolean gtk_socket_callback(GIOChannel *gio,
    GIOCondition condition, gpointer data)
{
    socket_callback();
    return (TRUE);
}

char *STARTUPSTR = "pd init . 0  7 4 10 9 5 13 11 7 16 14 8 19 21 13 29 32 \
19 44 15 9 21 18 11 25 23 14 32 28 17 38 43 26 58 65 39 88;\n";

static GtkApplication *pdgtk_app;

void *pdgtk_thisapp( void)
{
    return (pdgtk_app);
}

int main(int argc, char **argv)
{
    int portno = -1;
    GtkApplication *app;
    int status;
    GIOChannel *giochan;
    if (getenv("PDDEBUG"))
    {
        tcl_debug = 1;
        fprintf(stderr, "**debug flag set\n");
    }
    if (tcl_init())
        return (1);

    if (argc < 2 || (portno = atoi(argv[1])) < 1024)
    {
        if (tcl_debug)
            fprintf(stderr, "no port, argc %d, argv[1] '%s'\n", argc, argv[1]);
        if ((pdsockfd = socket_startpd(argc, argv)) < 0)
        {
             fprintf(stderr, "pdgtk: couldn't start pd\n");
             return (1);
        }
    }
    else
    {
        if (tcl_debug)
            fprintf(stderr, "port %d\n", portno);
        if ((pdsockfd = socket_open(portno)) < 0)
        {
            fprintf(stderr, "socket open or connect failed\n");
            return (1);
        }
        argc--; argv++;
    }
    if (!socket_send(STARTUPSTR))
    {
        fprintf(stderr, "could not send startup string\n");
        return (1);
    }
    giochan = g_io_channel_unix_new(pdsockfd);
    g_io_add_watch(giochan, G_IO_IN, gtk_socket_callback, NULL);

    app = pdgtk_app = gtk_application_new("org.puredata.pd.devel",
        G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "startup", G_CALLBACK(pdgtk_startup), NULL);
    g_signal_connect(app, "activate", G_CALLBACK(pdgtk_activate), NULL);
    g_signal_connect(app, "open", G_CALLBACK(pdgtk_open), NULL);
    status = g_application_run(G_APPLICATION(app), 1, argv);
    g_object_unref(app);

    return (status);
}
