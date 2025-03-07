/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "m_pd.h"
#include "s_stuff.h"
#include "g_canvas.h"
#include "s_utf8.h"

#define LMARGIN 2
#define RMARGIN 2
#define TMARGIN 3
#define BMARGIN 2

#define SEND_FIRST 1
#define SEND_UPDATE 2
#define SEND_CHECK 0

    /* An rtext object is created for each editdable text in a canvas.  This
    can be part of a text box (x_text nonzero) or of a scalar (x_words nonzero).
    */

struct _rtext
{
    char *x_buf;    /*-- raw byte string, assumed UTF-8 encoded (moo) --*/
    int x_bufsize;  /*-- byte length --*/
    int x_selstart; /*-- byte offset --*/
    int x_selend;   /*-- byte offset --*/
    int x_active;       /* 1 if 'actively' editing */
    int x_dragfrom;     /* character onset we're dragging from */
    t_text *x_text;         /* owner, if a text box */
    t_scalar *x_scalar;     /* associated scalar, otherwise */
    t_word *x_words;        /* ... and if so, associated data */
    t_gobj *x_drawtext;     /* ... and the drawing instruction */
    t_glist *x_glist;       /* glist owner belongs to */
    char x_tag[50];         /* tag for gui */
    struct _rtext *x_next;  /* next in editor list */
    int x_xpix;           /* (x,y) origin in pixels */
    int x_ypix;
    int x_pixwidth;
    int x_pixheight;
};

static void rtext_senditup(t_rtext *x, int action, int *widthp, int *heightp,
    int *indexp);

static t_rtext *rtext_add(t_glist *glist, t_rtext *last)
{
    t_rtext *x = (t_rtext *)getbytes(sizeof *x);
    x->x_text = 0;
    x->x_scalar = 0;
    x->x_words = 0;
    x->x_drawtext = 0;
    x->x_glist = glist;
    x->x_selstart = x->x_selend = x->x_active = 0;
    x->x_buf = 0;
    x->x_bufsize = 0;
    if (!last)  /* first in list */
        glist_getcanvas(glist)->gl_editor->e_rtext = x;
    else last->x_next = x;
    x->x_next = 0;
    sprintf(x->x_tag, ".x%lx.t%lx", (t_int)glist_getcanvas(x->x_glist),
        (t_int)x);
    x->x_xpix = x->x_ypix = 0;      /* empty rectangle */
    x->x_pixwidth = x->x_pixheight = -1;
    return (x);
}

/* find the rtext that goes with a text item */
t_rtext *glist_getrtext(t_glist *gl, t_text *who)
{
    t_rtext *x, *last = 0;
        /* This happens for text objs in GOPs - dunno why. */
    t_glist *canvas = glist_getcanvas(gl);
    if (!canvas->gl_editor)
        canvas_create_editor(canvas);
    for (x = canvas->gl_editor->e_rtext; x; x = x->x_next)
    {
        if (x->x_text == who)
            return (x);
        last = x;
    }
    x = rtext_add(gl, last);
    x->x_text = who;
    rtext_retext(x);
    return (x);
}

    /* find rtext for a field of a scalar being drawn by a drawtext */
t_rtext *glist_getforscalar(t_glist *gl, t_scalar *sc, t_word *words,
    t_gobj *drawtext)
{
    t_rtext *x, *last = 0;
    t_glist *canvas = glist_getcanvas(gl);
    if (!canvas->gl_editor)
        canvas_create_editor(canvas);
    for (x = canvas->gl_editor->e_rtext; x; x = x->x_next)
    {
        if (x->x_words == words && x->x_drawtext == drawtext)
            return (x);
        last = x;
    }
    x = rtext_add(gl, last);
    x->x_words = words;
    x->x_scalar = sc;
    x->x_drawtext = drawtext;
    return (x);
}

void rtext_free(t_rtext *x)
{
    t_glist *canvas = glist_getcanvas(x->x_glist);
    if (glist_textedfor(canvas) == x)
        glist_settexted(canvas, 0);
    if (canvas->gl_editor->e_rtext == x)
        canvas->gl_editor->e_rtext = x->x_next;
    else
    {
        t_rtext *e2;
        for (e2 = canvas->gl_editor->e_rtext; e2;
            e2 = e2->x_next)
                if (e2->x_next == x)
        {
            e2->x_next = x->x_next;
            break;
        }
    }
    if (x->x_buf)
        freebytes(x->x_buf, x->x_bufsize + 1); /* extra 0 byte */
    freebytes(x, sizeof *x);
}

static void rtext_findscreenlocation(t_rtext *x)
{
    int x2, y2;
    if (x->x_text)
        gobj_getrect(&x->x_text->te_g, x->x_glist,
            &x->x_xpix, &x->x_ypix, &x2, &y2);
    else bug("rtext_findscreenlocation");
}

    /* get width in characters - zero if grow-as-needed or if not a text box */
int rtext_charwidth(t_rtext *x)
{
    if (x->x_text)
        return (x->x_text->te_width);
    else return (0); /* LATER do we want a width field for drawtext obj? */
}

const char *rtext_gettag(t_rtext *x)
{
    return (x->x_tag);
}

void rtext_gettext(t_rtext *x, char **buf, int *bufsize)
{
    *buf = x->x_buf;
    *bufsize = x->x_bufsize;
}

void rtext_getseltext(t_rtext *x, char **buf, int *bufsize)
{
    *buf = x->x_buf + x->x_selstart;
    *bufsize = x->x_selend - x->x_selstart;
}

t_glist *rtext_getglist(t_rtext *x)
{
    return (x->x_glist);
}

void drawtext_newtext(t_gobj *drawtext, t_glist *gl, t_scalar *sc,
    t_word *words, char *buf);

    /* deal with an activated rtext when user clicks outside it.
    If the rtext is for a patchable object of type TE_ATOM, or if it is
    for a number or symbol 'data' field, terminate editing and adopt
    the new text in the rtext buffer.  Otherwise
    it's an object/message/comment box or text 'data' field, and
    we update the contents.  In case it's an object, we then
    also delete and re-create the object unless it has its own method
    to deal with text-changing. */
void rtext_unmouse(t_rtext *x)
{
    int onset = 0, drawtype = (x->x_drawtext ?
        drawtext_gettype(x->x_drawtext,
            drawtext_gettemplate(x->x_drawtext), &onset) :
                -1);
    t_canvas *canvas = glist_getcanvas(x->x_glist);
        /* if it's a scalar, deactivate; if a text box, deselect
            (which will deactivate but first check if text is dirty and
            possibly re-parse the text and update the object) */
    if (x->x_drawtext)
    {
        drawtext_newtext(x->x_drawtext, x->x_glist, x->x_scalar, x->x_words,
            x->x_buf);
        rtext_activate(glist_textedfor(canvas), 0);
        glist_settexted(canvas, 0);
    }
    else if (glist_isselected(x->x_glist, &x->x_text->te_g))
        glist_deselect(x->x_glist, &x->x_text->te_g);
            /* atom box - just simulate an 'enter' key */
    else if (x->x_text && x->x_text->te_type == T_ATOM)
        rtext_key(x, '\n', &s_);
}

void rtext_untype(t_rtext *x)
{
    if (x->x_text)
        rtext_retext(x);
    else
    {
        gobj_vis(&x->x_scalar->sc_gobj, x->x_glist, 0);
        gobj_vis(&x->x_scalar->sc_gobj, x->x_glist, 1);
    }
}

    /* search for an rtext matching a mouse click, filling in
    text/data pointers so caller can decide what to do with it */
t_rtext *rtext_findhit(t_glist *gl, int xpix, int ypix,
    t_text **text, t_scalar **scalar, t_word **words, t_gobj **drawtext)
{
    t_rtext *x;
        /* this shouldn't be necessary but just in case... */
    if (!gl->gl_editor)
        canvas_create_editor(gl);
    for (x = gl->gl_editor->e_rtext; x; x = x->x_next)
    {
        /* post("xpix %d (%d,%d) ypix %d (%d,%d)",
            xpix, x->x_xpix, x->x_xpix + x->x_pixwidth,
            ypix, x->x_ypix, x->x_ypix + x->x_pixheight); */
                /* check if the text is visible */
        if (x->x_text && !gobj_shouldvis(&x->x_text->te_g, x->x_glist) ||
            x->x_scalar && !gobj_shouldvis(&x->x_scalar->sc_gobj, x->x_glist))
                continue;
        if (xpix >= x->x_xpix && xpix <= x->x_xpix + x->x_pixwidth &&
            ypix >= x->x_ypix && ypix <= x->x_ypix + x->x_pixheight)
        {
            *text = x->x_text;
            *scalar = x->x_scalar;
            *words = x->x_words;
            *drawtext = x->x_drawtext;
            return (x);
        }
    }
    return (0);
}

/* LATER deal with tcl-significant characters */

/* firstone(), lastone()
 *  + returns byte offset of (first|last) occurrence of 'c' in 's[0..n-1]', or
 *    -1 if none was found
 *  + 's' is a raw byte string
 *  + 'c' is a byte value
 *  + 'n' is the length (in bytes) of the prefix of 's' to be searched.
 *  + we could make these functions work on logical characters in utf8 strings,
 *    but we don't really need to...
 */
static int firstone(char *s, int c, int n)
{
    char *s2 = s + n;
    int i = 0;
    while (s != s2)
    {
        if (*s == c) return (i);
        i++;
        s++;
    }
    return (-1);
}

static int lastone(char *s, int c, int n)
{
    char *s2 = s + n;
    while (s2 != s)
    {
        s2--;
        n--;
        if (*s2 == c) return (n);
    }
    return (-1);
}

    /* break the text into lines, and compute byte index of character at
    location (width, height).  Then reset (width, height) to report size of
    resulting line-broken text.  Used for object, message, and comment boxes;
    another version below is for atoms.  Also we report the onsets of
    the beginning and end of the selection, as byte onsets into the reformatted
    text, which we'll use to inform the GUI how to show the selection.

    The input is taken from x->buf and x->bufsize fields of the text object;
    the wrapped text is put in "tempbuf" with byte length outchars_b_p.

    x->x_buf is assumed to contain text in UTF-8 format, in which characters
    may occupy multiple bytes. variables with a "_b" suffix are raw byte
    strings, lengths, or offsets;  those with a "_c" suffix are logical
    character lengths or offsets.
    The UTF8 handling was contributed by Bryan Jurish, who says "moo." */

#define DEFAULTBOXWIDTH 60

static void rtext_formattext(t_rtext *x, int *widthp, int *heightp,
    int *indexp,  char *tempbuf, int *outchars_b_p, int *selstart_b_p,
    int *selend_b_p, int fontwidth, int fontheight)
{
    int widthspec_c = rtext_charwidth(x);
    int widthlimit_c = (widthspec_c ? widthspec_c : DEFAULTBOXWIDTH);
    int inindex_b = 0;
    int inindex_c = 0;
    int x_bufsize_c = u8_charnum(x->x_buf, x->x_bufsize);
    int nlines = 0, ncolumns = 0, reportedindex = 0;
    int findx = (*widthp + (fontwidth/2)) / fontwidth;
    int findy = *heightp / fontheight;

    *selstart_b_p = *selend_b_p = 0;
    while (x_bufsize_c - inindex_c > 0)
    {
        int inchars_b  = x->x_bufsize - inindex_b;
        int inchars_c  = x_bufsize_c  - inindex_c;
        int maxindex_c = (inchars_c > widthlimit_c ? widthlimit_c : inchars_c);
        int maxindex_b = u8_offset(x->x_buf + inindex_b, maxindex_c);
        int eatchar = 1;
        int foundit_b  = firstone(x->x_buf + inindex_b, '\n', maxindex_b);
        int foundit_c;
        if (foundit_b < 0)
        {
                /* too much text to fit in one line? */
            if (inchars_c > widthlimit_c)
            {
                    /* is there a space to break the line at?  OK if it's even
                    one byte past the end since in this context we know there's
                    more text */
                foundit_b = lastone(x->x_buf + inindex_b, ' ', maxindex_b + 1);
                if (foundit_b < 0)
                {
                    foundit_b = maxindex_b;
                    foundit_c = maxindex_c;
                    eatchar = 0;
                }
                else
                    foundit_c = u8_charnum(x->x_buf + inindex_b, foundit_b);
            }
            else
            {
                foundit_b = inchars_b;
                foundit_c = inchars_c;
                eatchar = 0;
            }
        }
        else
            foundit_c = u8_charnum(x->x_buf + inindex_b, foundit_b);

        if (nlines == findy)
        {
            int actualx = (findx < 0 ? 0 :
                (findx > foundit_c ? foundit_c : findx));
            *indexp = inindex_b + u8_offset(x->x_buf + inindex_b, actualx);
            reportedindex = 1;
        }
        strncpy(tempbuf+ *outchars_b_p, x->x_buf + inindex_b, foundit_b);
        if (x->x_selstart >= inindex_b &&
            x->x_selstart <= inindex_b + foundit_b + eatchar)
                *selstart_b_p = x->x_selstart + *outchars_b_p - inindex_b;
        if (x->x_selend >= inindex_b &&
            x->x_selend <= inindex_b + foundit_b + eatchar)
                *selend_b_p = x->x_selend + *outchars_b_p - inindex_b;
        *outchars_b_p += foundit_b;
        inindex_b += (foundit_b + eatchar);
        inindex_c += (foundit_c + eatchar);
        if (inindex_b < x->x_bufsize)
            tempbuf[(*outchars_b_p)++] = '\n';
        if (foundit_c > ncolumns)
            ncolumns = foundit_c;
        nlines++;
    }
    if (!reportedindex)
        *indexp = *outchars_b_p;
    if (nlines < 1)
        nlines = 1;

    if (x->x_text)
    {
        if (!widthspec_c)
        {
            while (ncolumns < (x->x_text->te_type == T_TEXT ? 1 : 3))
            {
                tempbuf[(*outchars_b_p)++] = ' ';
                ncolumns++;
            }
        }
        else ncolumns = widthspec_c;
    }
    *widthp = ncolumns * fontwidth +
        (x->x_text? (LMARGIN + RMARGIN) * glist_getzoom(x->x_glist) : 0);
    *heightp = nlines * fontheight +
        (x->x_text? (TMARGIN + BMARGIN) * glist_getzoom(x->x_glist) : 0);
}

    /* same as above, but for atom boxes, which are always on one line. */
static void rtext_formatatom(t_rtext *x, int *widthp, int *heightp,
    int *indexp,  char *tempbuf, int *outchars_b_p, int *selstart_b_p,
    int *selend_b_p, int fontwidth, int fontheight)
{
    int findx = *widthp / fontwidth;  /* character index; want byte index */
    int charwidth = rtext_charwidth(x);
    *indexp = 0;
        /* special case: for number boxes, try to pare the number down
        to the specified width of the box. */
    if (charwidth > 0 && binbuf_getnatom(x->x_text->te_binbuf) == 1 &&
        binbuf_getvec(x->x_text->te_binbuf)->a_type == A_FLOAT &&
        x->x_bufsize > charwidth)
    {
            /* try to reduce size by dropping decimal digits */
        int wantreduce = x->x_bufsize - charwidth;
        char *decimal = 0, *nextchar, *ebuf = x->x_buf + x->x_bufsize,
            *s1, *s2;
        int ndecimals;
        strncpy(tempbuf, x->x_buf, x->x_bufsize);
        tempbuf[x->x_bufsize] = 0;
        ebuf = tempbuf + x->x_bufsize;
        for (decimal = tempbuf; decimal < ebuf; decimal++)
            if (*decimal == '.')
                break;
        if (decimal >= ebuf)
            goto giveup;
        for (nextchar = decimal + 1; nextchar < ebuf; nextchar++)
            if (*nextchar < '0' || *nextchar > '9')
                break;
        if (nextchar - decimal - 1 < wantreduce)
            goto giveup;
        for (s1 = nextchar - wantreduce, s2 = s1 + wantreduce;
            s2 < ebuf; s1++, s2++)
                *s1 = *s2;
        *outchars_b_p = charwidth;
        goto done;
    giveup:
            /* give up and bash last char to '>' */
        tempbuf[charwidth-1] = '>';
        tempbuf[charwidth] = 0;
        *outchars_b_p = charwidth;
    done: ;
        *indexp = findx;
        *widthp = charwidth * fontwidth;
    }
    else
    {
        int outchars_c = 0, prev_b = 0;
        int widthlimit_c = (charwidth > 0 ? charwidth :
                1000);   /* nice big fat limit since we can't wrap */
        uint32_t thischar;
        *outchars_b_p = 0;
        for (outchars_c = 0;
            *outchars_b_p < x->x_bufsize && outchars_c < widthlimit_c;
                outchars_c++)
        {
            prev_b = *outchars_b_p;
            thischar = u8_nextchar(x->x_buf, outchars_b_p);
            if (findx > outchars_c)
                *indexp = *outchars_b_p;
            if (thischar == '\n' || !thischar)
            {
                *(outchars_b_p) = prev_b;
                break;
            }
            memcpy(tempbuf + prev_b, x->x_buf + prev_b, *outchars_b_p - prev_b);
                /* if box is full and there's more, bash last char to '>' */
            if (outchars_c == widthlimit_c-1 && x->x_bufsize > *(outchars_b_p)
                 && (x->x_buf[*(outchars_b_p)] != ' ' ||
                    x->x_bufsize > *(outchars_b_p)+1))
            {
                tempbuf[prev_b] = '>';
            }
        }
        if (charwidth > 0)
            *widthp = charwidth * fontwidth;
        else *widthp = (outchars_c > 3 ? outchars_c : 3) * fontwidth;
        tempbuf[*outchars_b_p] = 0;
    }
    if (*indexp > *outchars_b_p)
        *indexp = *outchars_b_p;
    if (*indexp < 0)
        *indexp = 0;
    *selstart_b_p = x->x_selstart;
    *selend_b_p = x->x_selend;
    *widthp += (LMARGIN + RMARGIN - 2) * glist_getzoom(x->x_glist);
    *heightp = fontheight + (TMARGIN + BMARGIN - 1) * glist_getzoom(x->x_glist);
}

    /* the following routine computes line breaks and carries out
    some action which could be:
        SEND_FIRST - draw the box  for the first time
        SEND_UPDATE - redraw the updated box
        otherwise - don't draw, just calculate.
    Called with *widthp and *heightp as coordinates of
    a test point, the routine reports the index of the character found
    there in *indexp.  *widthp and *heightp are set to the width and height
    of the entire text in pixels.
    */

    /* LATER get this and sys_vgui to work together properly,
        breaking up messages as needed.  As of now, there's
        a limit of 1950 characters, imposed by sys_vgui(). */
#define UPBUFSIZE 4000

void text_getfont(t_text *x, t_glist *thisglist,
    int *fheightp, int *fwidthp, int *guifsize);

static void rtext_senditup(t_rtext *x, int action, int *widthp, int *heightp,
    int *indexp)
{
    char smallbuf[200], *tempbuf;
    int outchars_b = 0, guifontsize, fontwidth, fontheight;
    t_canvas *canvas = glist_getcanvas(x->x_glist);
    int selstart_b, selend_b;   /* beginning and end of selection in bytes */

        /* if we're a GOP (the new, "goprect" style) borrow the font size
        from the inside to preserve the spacing.  If we're in a data
        struct, just grab the glist's fint info  */

    if (x->x_text)
    {
        text_getfont(x->x_text, x->x_glist,
            &fontwidth, &fontheight, &guifontsize);
        x->x_xpix = text_xpix(x->x_text, x->x_glist);
        x->x_ypix = text_ypix(x->x_text, x->x_glist);
    }
    else text_getfont(&x->x_glist->gl_obj, x->x_glist,
            &fontwidth, &fontheight, &guifontsize);
    if (x->x_bufsize >= 100)
         tempbuf = (char *)t_getbytes(2 * x->x_bufsize + 1);
    else tempbuf = smallbuf;
    tempbuf[0] = 0;

    if (x->x_text && x->x_text->te_type == T_ATOM)
        rtext_formatatom(x, widthp, heightp, indexp,
            tempbuf, &outchars_b, &selstart_b,  &selend_b,
            fontwidth, fontheight);
    else rtext_formattext(x, widthp, heightp, indexp,
            tempbuf, &outchars_b, &selstart_b, &selend_b,
            fontwidth, fontheight);
    tempbuf[outchars_b]=0;
        /* rtext_charwidth(x) is nonzero only for "text" rtexts so we can
        access x->x_text in this test: */
    if (action && rtext_charwidth(x) &&
        x->x_text && x->x_text->te_type != T_ATOM)
    {
            /* if our width is specified but the "natural" width is the
            same as the specified width, set specified width to zero
            so future text editing will automatically change width.
            Except atoms whose content changes at runtime. */
        int widthwas = rtext_charwidth(x), newwidth = 0, newheight = 0,
            newindex = 0;
        x->x_text->te_width = 0;
        rtext_senditup(x, 0, &newwidth, &newheight, &newindex);
        if (newwidth != *widthp)
            x->x_text->te_width = widthwas;
    }

    if (action && !canvas->gl_havewindow)
        action = 0;

    if (action == SEND_FIRST)
    {
        const char *tags[] = {x->x_tag, "text"};
        int lmargin = (x->x_text ? LMARGIN : 0),
            tmargin = (x->x_text ? TMARGIN : 0);
        if (glist_getzoom(x->x_glist) > 1)
        {
            /* zoom margins */
            lmargin *= glist_getzoom(x->x_glist);
            tmargin *= glist_getzoom(x->x_glist);
        }
            /* we add an extra space to the string just in case the last
            character is an unescaped backslash ('\') which would have confused
            tcl/tk by escaping the close brace otherwise.  The GUI code
            drops the last character in the string. */
        pdgui_vmess("pdtk_text_new", "c S ii s i r",
            canvas,
            2, tags,
            x->x_xpix + lmargin, x->x_ypix + tmargin,
            tempbuf,
            guifontsize,
            (x->x_text && glist_isselected(x->x_glist, &x->x_text->te_g)?
                "blue" : "black"));
    }
    else if (action == SEND_UPDATE)
    {
        pdgui_vmess("pdtk_text_set", "cs s",
                  canvas, x->x_tag,
                  tempbuf);
        if (x->x_text && (*widthp != x->x_pixwidth ||
            *heightp != x->x_pixheight))
                text_drawborder(x->x_text, x->x_glist, x->x_tag, 0);
        if (x->x_active)
        {
            if (selend_b > selstart_b)
            {
                pdgui_vmess(0, "crr si",
                    canvas, "select", "from",
                    x->x_tag, u8_charnum(x->x_buf, selstart_b));
                pdgui_vmess(0, "crr si",
                    canvas, "select", "to",
                    x->x_tag, u8_charnum(x->x_buf, selend_b) - 1);
                pdgui_vmess(0, "crs", canvas, "focus", "");
            }
            else
            {
                pdgui_vmess(0, "crr", canvas, "select", "clear");
                pdgui_vmess(0, "cr si", canvas, "icursor", x->x_tag,
                    u8_charnum(x->x_buf, selstart_b));
                pdgui_vmess("focus", "c", canvas);
                pdgui_vmess(0, "crs", canvas, "focus", x->x_tag);
            }
        }
    }
    x->x_pixwidth = *widthp;
    x->x_pixheight = *heightp;
    if (tempbuf != smallbuf)
        t_freebytes(tempbuf, 2 * x->x_bufsize + 1);
}

    /* make or remake text buffer from binbuf (text boxes only) */
void rtext_retext(t_rtext *x)
{
    int w = 0, h = 0, indx;
    if (x->x_buf)
        t_freebytes(x->x_buf, x->x_bufsize + 1); /* extra 0 byte */
    if (!x->x_text)
        bug("rtext_retext");
    binbuf_gettext(x->x_text->te_binbuf, &x->x_buf, &x->x_bufsize);
        /* allocate extra space for hidden null terminator */
    x->x_buf = resizebytes(x->x_buf, x->x_bufsize, x->x_bufsize+1);
    x->x_buf[x->x_bufsize] = 0;
    rtext_findscreenlocation(x);
    rtext_senditup(x, SEND_UPDATE, &w, &h, &indx);
}

    /* make text buffer for scalar's drawtext instruction and draw it */
void rtext_retextforscalar(t_rtext *x, char *buf, int len, int xpix, int ypix)
{
    int w = 0, h = 0, indx;
    x->x_buf = resizebytes(x->x_buf, x->x_bufsize+1, len+1);
    strncpy(x->x_buf, buf, len+1);
    x->x_buf[len] = 0;
    x->x_bufsize = len;
    x->x_xpix = xpix;
    x->x_ypix = ypix;
    rtext_senditup(x, SEND_FIRST, &w, &h, &indx);
}

void rtext_getrect(t_rtext *x, int *x1p, int *y1p, int *x2p, int *y2p)
{
    int w = 0, h = 0, indx;
    rtext_senditup(x, SEND_CHECK, &w, &h, &indx);
    *x2p = (*x1p = x->x_xpix) + w;
    *y2p = (*y1p = x->x_ypix) + h;
}

void rtext_draw(t_rtext *x)
{
    int w = 0, h = 0, indx;
        /* subgraphs are displaced by simply erasing and redrawing -
        so here we have to recheck our screen location */
    if (x->x_text)
        rtext_findscreenlocation(x);
    rtext_senditup(x, SEND_FIRST, &w, &h, &indx);
}

void rtext_erase(t_rtext *x)
{
    pdgui_vmess(0, "crs", glist_getcanvas(x->x_glist), "delete", x->x_tag);
}

void rtext_displace(t_rtext *x, int dx, int dy)
{
    x->x_xpix += dx;
    x->x_ypix += dy;
    pdgui_vmess(0, "crs ii", glist_getcanvas(x->x_glist), "move", x->x_tag,
        dx, dy);
}

void rtext_select(t_rtext *x, int state)
{
    pdgui_vmess(0, "crs rr",
        glist_getcanvas(x->x_glist), "itemconfigure", x->x_tag,
        "-fill", (state? "blue" : "black"));
}

void rtext_activate(t_rtext *x, int state)
{
    int w = 0, h = 0, indx;
    t_glist *glist = x->x_glist;
    t_canvas *canvas = glist_getcanvas(glist);
    if (state)
    {
        pdgui_vmess("pdtk_text_editing", "^si", canvas, x->x_tag, 1);
        glist_settexted(canvas, x);
        canvas->gl_editor->e_textdirty = 0;
        x->x_dragfrom = x->x_selstart = 0;
        x->x_selend = x->x_bufsize;
        x->x_active = 1;
    }
    else
    {
        pdgui_vmess("pdtk_text_editing", "^si", canvas, "", 0);
        if (glist_textedfor(canvas) == x)
            glist_settexted(canvas, 0);
        x->x_active = 0;
    }
    rtext_senditup(x, SEND_UPDATE, &w, &h, &indx);
}

    /* figure out which atom a click falls into if any; -1 if you
    clicked on a space or something */
int rtext_findatomfor(t_rtext *x, int xpos, int ypos)
{
    int w = xpos, h = ypos, indx, natom = 0, i, gotone = 0;
        /* get byte index of character clicked on */
    rtext_senditup(x, SEND_UPDATE, &w, &h, &indx);
        /* search through for whitespace before that index */
    for (i = 0; i <= indx; i++)
    {
        if (x->x_buf[i] == ';' || x->x_buf[i] == ',')
            natom++, gotone = 0;
        else if (x->x_buf[i] == ' ' || x->x_buf[i] == '\n')
            gotone = 0;
        else
        {
            if (!gotone)
                natom++;
            gotone = 1;
        }
    }
    return (natom-1);
}

void gatom_key(void *z, t_symbol *keysym, t_floatarg f);

void rtext_key(t_rtext *x, int keynum, t_symbol *keysym)
{
    int w = 0, h = 0, indx, i, newsize, ndel;
    char *s1, *s2;
        /* CR to atom boxes sends message and resets */
    if (keynum == '\n')
    {
        if (x->x_text && x->x_text->te_type == T_ATOM)
        {
            gatom_key(x->x_text, keysym, keynum);
            return;
        }
        else if (x->x_scalar)
        {
            t_atom at;
            t_template *template =
                template_findbyname(x->x_scalar->sc_template);
            int onset, type = drawtext_gettype(x->x_drawtext, template, &onset);
            if (type == DT_FLOAT || type == DT_SYMBOL)
            {
                if (type == DT_FLOAT)
                    x->x_words[onset].w_float = atof(x->x_buf);
                else x->x_words[onset].w_symbol = gensym(x->x_buf);
                template_notifyforscalar(template, x->x_glist,
                        x->x_scalar, gensym("change"), 1, &at);
                scalar_redraw(x->x_scalar, x->x_glist);
            }
        }
    }
    if (keynum)
    {
        int n = keynum;
        if (n == '\r') n = '\n';
        if (n == '\b')  /* backspace */
        {
                    /* LATER delete the box if all text is selected...
                    this causes reentrancy problems now. */
            /* if ((!x->x_selstart) && (x->x_selend == x->x_bufsize))
            {
                ....
            } */
            if (x->x_selstart && (x->x_selstart == x->x_selend))
                u8_dec(x->x_buf, &x->x_selstart);
        }
        else if (n == 127)      /* delete */
        {
            if (x->x_selend < x->x_bufsize && (x->x_selstart == x->x_selend))
                u8_inc(x->x_buf, &x->x_selend);
        }

        ndel = x->x_selend - x->x_selstart;
        for (i = x->x_selend; i < x->x_bufsize; i++)
            x->x_buf[i- ndel] = x->x_buf[i];
        newsize = x->x_bufsize - ndel;
            /* allocate extra space for hidden null terminator */
        x->x_buf = resizebytes(x->x_buf, x->x_bufsize, newsize+1);
        x->x_buf[newsize] = 0;
        x->x_bufsize = newsize;

/* at Guenter's suggestion, use 'n>31' to test whether a character might
be printable in whatever 8-bit character set we find ourselves. */

/*-- moo:
  ... but test with "<" rather than "!=" in order to accommodate unicode
  codepoints for n (which we get since Tk is sending the "%A" substitution
  for bind <Key>), effectively reducing the coverage of this clause to 7
  bits.  Case n>127 is covered by the next clause.
*/
        if (n == '\n' || (n > 31 && n < 127))
        {
            newsize = x->x_bufsize+1;
                /* allocate extra space for hidden null terminator */
            x->x_buf = resizebytes(x->x_buf, x->x_bufsize, newsize+1);
            for (i = x->x_bufsize; i > x->x_selstart; i--)
                x->x_buf[i] = x->x_buf[i-1];
            x->x_buf[x->x_selstart] = n;
            x->x_buf[newsize] = 0;
            x->x_bufsize = newsize;
            x->x_selstart = x->x_selstart + 1;
        }
        /*--moo: check for unicode codepoints beyond 7-bit ASCII --*/
        else if (n > 127)
        {
            int ch_nbytes = u8_wc_nbytes(n);
            newsize = x->x_bufsize + ch_nbytes;
                /* allocate extra space for hidden null terminator */
            x->x_buf = resizebytes(x->x_buf, x->x_bufsize, newsize+1);

            for (i = newsize-1; i > x->x_selstart; i--)
                x->x_buf[i] = x->x_buf[i-ch_nbytes];
            x->x_buf[newsize] = 0;
            x->x_bufsize = newsize;
            /*-- moo: assume canvas_key() has encoded keysym as UTF-8 */
            strncpy(x->x_buf+x->x_selstart, keysym->s_name, ch_nbytes);
            x->x_selstart = x->x_selstart + ch_nbytes;
        }
        x->x_selend = x->x_selstart;
        glist_getcanvas(x->x_glist)->gl_editor->e_textdirty = 1;
    }
    else if (!strcmp(keysym->s_name, "Home"))
    {
        if (x->x_selend == x->x_selstart)
        {
            x->x_selend = x->x_selstart = 0;
        }
        else
            x->x_selstart = 0;
    }
    else if (!strcmp(keysym->s_name, "End"))
    {
        if (x->x_selend == x->x_selstart)
        {
            x->x_selend = x->x_selstart = x->x_bufsize;
        }
        else
            x->x_selend = x->x_bufsize;
    }
    else if (!strcmp(keysym->s_name, "Right"))
    {
        if (x->x_selend == x->x_selstart && x->x_selstart < x->x_bufsize)
        {
            u8_inc(x->x_buf, &x->x_selstart);
            x->x_selend = x->x_selstart;
        }
        else
            x->x_selstart = x->x_selend;
    }
    else if (!strcmp(keysym->s_name, "Left"))
    {
        if (x->x_selend == x->x_selstart && x->x_selstart > 0)
        {
            u8_dec(x->x_buf, &x->x_selstart);
            x->x_selend = x->x_selstart;
        }
        else
            x->x_selend = x->x_selstart;
    }
        /* this should be improved...  life's too short */
    else if (!strcmp(keysym->s_name, "Up"))
    {
        if (x->x_selstart)
            u8_dec(x->x_buf, &x->x_selstart);
        while (x->x_selstart > 0 && x->x_buf[x->x_selstart] != '\n')
            u8_dec(x->x_buf, &x->x_selstart);
        x->x_selend = x->x_selstart;
    }
    else if (!strcmp(keysym->s_name, "Down"))
    {
        while (x->x_selend < x->x_bufsize &&
            x->x_buf[x->x_selend] != '\n')
            u8_inc(x->x_buf, &x->x_selend);
        if (x->x_selend < x->x_bufsize)
            u8_inc(x->x_buf, &x->x_selend);
        x->x_selstart = x->x_selend;
    }
    rtext_senditup(x, SEND_UPDATE, &w, &h, &indx);
}

void rtext_mouse(t_rtext *x, int xval, int yval, int flag)
{
    int w = xval, h = yval, indx;
    rtext_senditup(x, SEND_CHECK, &w, &h, &indx);
    if (flag == RTEXT_DOWN)
    {
        x->x_dragfrom = x->x_selstart = x->x_selend = indx;
    }
    else if (flag == RTEXT_DBL)
    {
        int whereseparator, newseparator;
        x->x_dragfrom = -1;
        whereseparator = 0;
        if ((newseparator = lastone(x->x_buf, ' ', indx)) > whereseparator)
            whereseparator = newseparator+1;
        if ((newseparator = lastone(x->x_buf, '\n', indx)) > whereseparator)
            whereseparator = newseparator+1;
        if ((newseparator = lastone(x->x_buf, ';', indx)) > whereseparator)
            whereseparator = newseparator+1;
        if ((newseparator = lastone(x->x_buf, ',', indx)) > whereseparator)
            whereseparator = newseparator+1;
        x->x_selstart = whereseparator;

        whereseparator = x->x_bufsize - indx;
        if ((newseparator =
            firstone(x->x_buf+indx, ' ', x->x_bufsize - indx)) >= 0 &&
                newseparator < whereseparator)
                    whereseparator = newseparator;
        if ((newseparator =
            firstone(x->x_buf+indx, '\n', x->x_bufsize - indx)) >= 0 &&
                newseparator < whereseparator)
                    whereseparator = newseparator;
        if ((newseparator =
            firstone(x->x_buf+indx, ';', x->x_bufsize - indx)) >= 0 &&
                newseparator < whereseparator)
                    whereseparator = newseparator;
        if ((newseparator =
            firstone(x->x_buf+indx, ',', x->x_bufsize - indx)) >= 0 &&
                newseparator < whereseparator)
                    whereseparator = newseparator;
        x->x_selend = indx + whereseparator;
    }
    else if (flag == RTEXT_SHIFT)
    {
        if (indx * 2 > x->x_selstart + x->x_selend)
            x->x_dragfrom = x->x_selstart, x->x_selend = indx;
        else
            x->x_dragfrom = x->x_selend, x->x_selstart = indx;
    }
    else if (flag == RTEXT_DRAG)
    {
        if (x->x_dragfrom < 0)
            return;
        x->x_selstart = (x->x_dragfrom < indx ? x->x_dragfrom : indx);
        x->x_selend = (x->x_dragfrom > indx ? x->x_dragfrom : indx);
    }
    rtext_senditup(x, SEND_UPDATE, &w, &h, &indx);
}
