/*
    pdtcl main progrm, adapted from the "scribble" test program for gtk.
    Starts up gtk and tcl and opens the Pd window.
*/

#include <gtk/gtk.h>
#define USINGGTK
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

static void pdgtk_openfile(GFile *file)
{
    GFile *parent = g_file_get_parent(file);
    char *basename = g_file_get_basename(file), *dir = g_file_get_path(parent);
    char pdcmd[BIGSTRING];

    /* fprintf(stderr, "pdgtk_openfile: pd open %s %s;\n", basename, dir); */
    snprintf(pdcmd, BIGSTRING, "pd open %s %s;\n", basename, dir);
    pdcmd[BIGSTRING-1] = 0;
    socket_send(pdcmd);
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

static int pdgtk_havemainwindow;
#ifdef __APPLE__
#define ACCELERATORKEY "<Meta>"
#else
#define ACCELERATORKEY "<Control>"
#endif

static void pdgtk_createmainwindow(GtkApplication *app, gpointer user_data)
{
    GtkWidget *frame;
    GtkWidget *drawing_area;
    GMenu *menubar, *filemenu;
    GMenuItem *menu_item_filemenu, *menu_item_open;
    GSimpleAction *action_open;
    const char *twoaccels[2] = {0, 0};

    if (pdgtk_havemainwindow)
        return;
    gtk_mainwindow = gtk_application_window_new(app);
    gtk_window_set_title (GTK_WINDOW(gtk_mainwindow), "Pure Data GTK");
    g_signal_connect(gtk_mainwindow, "destroy", G_CALLBACK (close_window),
        NULL);

        /* handle "open" actions sent from OS - should this be in main()? */
    action_open = g_simple_action_new("open", NULL);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(action_open));
    g_signal_connect(action_open, "activate", G_CALLBACK(filemenu_open), NULL);

        /* set up menu - there's only one, not one per window. */
    menubar = g_menu_new();
    menu_item_filemenu = g_menu_item_new("File", NULL);
    filemenu = g_menu_new();
    menu_item_open = g_menu_item_new("open", "app.open");
    g_menu_append_item(filemenu, menu_item_open);
    twoaccels[0] = ACCELERATORKEY "o";
    gtk_application_set_accels_for_action(app, "app.open", twoaccels);
    g_object_unref(menu_item_open);
    g_menu_item_set_submenu(menu_item_filemenu, G_MENU_MODEL(filemenu));
    g_menu_append_item(menubar, menu_item_filemenu);
    g_object_unref(menu_item_filemenu);
    gtk_application_set_menubar(GTK_APPLICATION(app), G_MENU_MODEL(menubar));
    gtk_application_window_set_show_menubar(
         GTK_APPLICATION_WINDOW(gtk_mainwindow), TRUE);

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
    g_signal_connect(app, "activate", G_CALLBACK(pdgtk_activate), NULL);
    g_signal_connect(app, "open", G_CALLBACK(pdgtk_open), NULL);
    status = g_application_run(G_APPLICATION(app), 1, argv);
    g_object_unref(app);

    return (status);
}
