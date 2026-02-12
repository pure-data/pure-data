/*
    pdtcl main progrm, adapted from the "scribble" test program for gtk.
    Starts up gtk and tcl and opens the Pd window.
*/

#include <gtk/gtk.h>
#define USINGGTK
#include "pdgtk.h"
#include <sys/socket.h>

GtkApplication *tcl_gtkapp;

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

static void activate(GtkApplication *app, gpointer user_data)
{
    GtkWidget *window;
    GtkWidget *frame;
    GtkWidget *drawing_area;

    window = gtk_application_window_new(app);
    gtk_window_set_title (GTK_WINDOW (window), "Pure Data GTK");
    g_signal_connect(window, "destroy", G_CALLBACK (close_window), NULL);

    frame = gtk_frame_new (NULL);
    gtk_window_set_child(GTK_WINDOW(window), frame);
    drawing_area = gtk_drawing_area_new();
        /* set minimum size */
    gtk_widget_set_size_request(drawing_area, 300, 300);
    gtk_frame_set_child(GTK_FRAME (frame), drawing_area);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area),
        pdwindow_draw, NULL, NULL);
    g_signal_connect_after(drawing_area, "resize", G_CALLBACK(pdwindow_resize),
        NULL);
    gtk_window_present(GTK_WINDOW(window));
}

static int sockfd;
static char commands[BIGSTRING+1];
static int commandfill;

static char *whereterminater(char *input)
{
    char *s;
    int i, n = strlen(input), bracketcount = 0, escaped = 0;
    for (i = 0; i < n; i++)
    {
        if (escaped)
            escaped = 0;
        else if (input[i] == '\\')
            escaped = 1;
        else if (input[i] == '{')
            bracketcount++;
        else if (input[i] == '}')
            bracketcount--;
        else if (bracketcount == 0 && input[i] == ';')
            return (input + i);
    }
    return (0);
}

static gboolean socket_callback(GIOChannel *gio,
    GIOCondition condition, gpointer data)
{
    int nbytes, i, nused = 0;
    char bashedchar1, bashedchar2, *wheresemi;
    // fprintf(stderr, "read from socket...\n");
    if ((nbytes = recv(sockfd, commands + commandfill,
        BIGSTRING - commandfill, MSG_DONTWAIT)) < 1)
    {
        if (nbytes < 0 && errno == EAGAIN)
            return (TRUE);
        else if (nbytes < 0)
        {
            perror("socket");
            exit (1);
        }
        else exit (0);
    }
    commands[commandfill + nbytes] = 0;
    // fprintf(stderr, "read done: \'%s\'\n", commands);

        /* find first semicolon - LATER check for escaping */
    while ((wheresemi = whereterminater(commands+nused)))
    {
        bashedchar1 = wheresemi[1];
        bashedchar2 = wheresemi[2];
        wheresemi[1] = '\n';
        wheresemi[2] = 0;
        tcl_runcommand(commands+nused);
        nused = wheresemi - commands + 1;
        wheresemi[1] = bashedchar1;
        wheresemi[2] = bashedchar2;
    }
    while (nused < commandfill + nbytes &&
        (commands[nused] == '\n') || (commands[nused] == 0))
            nused++;
    if (nused < commandfill + nbytes)
    {
        memcpy(commands, commands + nused, commandfill + nbytes - nused);
        commandfill = commandfill + nbytes - nused;
        fprintf(stderr, "note: %d extra chars (%c) saved for next command\n",
            commandfill, commands[0]);
    }
    else commandfill = 0;

    return (TRUE);
}

char *STARTUPSTR = "pd init . 0  7 4 10 9 5 13 11 7 16 14 8 19 21 13 29 32 \
19 44 15 9 21 18 11 25 23 14 32 28 17 38 43 26 58 65 39 88;\n";

int main(int argc, char **argv)
{
    int portno;
    GtkApplication *app;
    int status;
    GIOChannel *giochan;

    if (argc < 2 || (portno = atoi(argv[1])) < 1)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return (1);
    }
    if (tcl_init())
        return (1);
    if ((sockfd = socket_open(portno)) < 0)
    {
        fprintf(stderr, "socket open or connect failed\n");
        return (1);
    }
    if (!socket_send(STARTUPSTR))
    {
        fprintf(stderr, "could not send startup string\n");
        return (1);
    }
    if (getenv("DEBUG"))
        tcl_debug = 1;
    giochan = g_io_channel_unix_new(sockfd);
    g_io_add_watch(giochan, G_IO_IN, socket_callback, NULL);

    app = gtk_application_new("org.puredata.puredata",
        G_APPLICATION_DEFAULT_FLAGS);
    tcl_gtkapp = app;
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), 1, argv);
    g_object_unref(app);

    return (status);
}
