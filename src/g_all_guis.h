/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution. */
/* g_7_guis.h written by Thomas Musil (c) IEM KUG Graz Austria 2000-2001 */


#define IEM_GUI_COLNR_WHITE          0
#define IEM_GUI_COLNR_ML_GREY        1
#define IEM_GUI_COLNR_D_GREY         2
#define IEM_GUI_COLNR_L_RED          3
#define IEM_GUI_COLNR_L_ORANGE       4
#define IEM_GUI_COLNR_L_YELLOW       5
#define IEM_GUI_COLNR_L_GREEN        6
#define IEM_GUI_COLNR_L_CYAN         7
#define IEM_GUI_COLNR_L_BLUE         8
#define IEM_GUI_COLNR_L_MAGENTA      9

#define IEM_GUI_COLNR_LL_GREY        10
#define IEM_GUI_COLNR_M_GREY         11
#define IEM_GUI_COLNR_DD_GREY        12
#define IEM_GUI_COLNR_RED            13
#define IEM_GUI_COLNR_ORANGE         14
#define IEM_GUI_COLNR_YELLOW         15
#define IEM_GUI_COLNR_GREEN          16
#define IEM_GUI_COLNR_CYAN           17
#define IEM_GUI_COLNR_BLUE           18
#define IEM_GUI_COLNR_MAGENTA        19

#define IEM_GUI_COLNR_L_GREY         20
#define IEM_GUI_COLNR_MD_GREY        21
#define IEM_GUI_COLNR_BLACK          22
#define IEM_GUI_COLNR_D_RED          23
#define IEM_GUI_COLNR_D_ORANGE       24
#define IEM_GUI_COLNR_D_YELLOW       25
#define IEM_GUI_COLNR_D_GREEN        26
#define IEM_GUI_COLNR_D_CYAN         27
#define IEM_GUI_COLNR_D_BLUE         28
#define IEM_GUI_COLNR_D_MAGENTA      29

#define IEM_GUI_COLOR_SELECTED       255
#define IEM_GUI_COLOR_NORMAL         0

#define IEM_GUI_MAX_COLOR            30

#define IEM_GUI_DEFAULTSIZE 15
#define IEM_GUI_MINSIZE 8
#define IEM_GUI_MAXSIZE 1000
#define IEM_SL_DEFAULTSIZE 128
#define IEM_SL_MINSIZE 2
#define IEM_FONT_MINSIZE 4

#define IEM_BNG_DEFAULTHOLDFLASHTIME 250
#define IEM_BNG_DEFAULTBREAKFLASHTIME 50
#define IEM_BNG_MINHOLDFLASHTIME 50
#define IEM_BNG_MINBREAKFLASHTIME 10

#define IEM_VU_DEFAULTSIZE 3
#define IEM_VU_LARGESMALL  2
#define IEM_VU_MINSIZE     2
#define IEM_VU_MAXSIZE     25
#define IEM_VU_STEPS       40

#define IEM_VU_MINDB    -99.9
#define IEM_VU_MAXDB    12.0
#define IEM_VU_OFFSET   100.0

#define IEM_RADIO_MAX   128

#define IEM_SYM_UNIQUE_SND  256
#define IEM_SYM_UNIQUE_RCV  512
#define IEM_SYM_UNIQUE_LAB  1024
#define IEM_SYM_UNIQUE_ALL  1792
#define IEM_FONT_STYLE_ALL  255

#define IEM_MAX_SYM_LEN      127

#define IEM_GUI_DRAW_MODE_UPDATE 0
#define IEM_GUI_DRAW_MODE_MOVE   1
#define IEM_GUI_DRAW_MODE_NEW    2
#define IEM_GUI_DRAW_MODE_SELECT 3
#define IEM_GUI_DRAW_MODE_ERASE  4
#define IEM_GUI_DRAW_MODE_CONFIG 5
#define IEM_GUI_DRAW_MODE_IO     6


#define IS_A_POINTER(atom,index) ((atom+index)->a_type == A_POINTER)
#define IS_A_FLOAT(atom,index) ((atom+index)->a_type == A_FLOAT)
#define IS_A_SYMBOL(atom,index) ((atom+index)->a_type == A_SYMBOL)
#define IS_A_DOLLAR(atom,index) ((atom+index)->a_type == A_DOLLAR)
#define IS_A_DOLLSYM(atom,index) ((atom+index)->a_type == A_DOLLSYM)

#define IEM_FSTYLE_FLAGS_ALL 0x007fffff
#define IEM_INIT_ARGS_ALL    0x01ffffff

#define IEM_GUI_OLD_SND_FLAG 1
#define IEM_GUI_OLD_RCV_FLAG 2

#define IEM_GUI_COLOR_EDITED 16711680
#define IEMGUI_MAX_NUM_LEN 32

#define IEMGUI_ZOOM(x) ((x)->x_gui.x_glist->gl_zoom)

typedef struct _iem_fstyle_flags
{
    unsigned int x_font_style:6;
    unsigned int x_rcv_able:1;
    unsigned int x_snd_able:1;
    unsigned int x_lab_is_unique:1;
    unsigned int x_rcv_is_unique:1;
    unsigned int x_snd_is_unique:1;
    unsigned int x_lab_arg_tail_len:6;
    unsigned int x_lab_is_arg_num:6;
    unsigned int x_shiftdown:1;
    unsigned int x_selected:1;
    unsigned int x_finemoved:1;
    unsigned int x_put_in2out:1;
    unsigned int x_change:1;
    unsigned int x_thick:1;
    unsigned int x_lin0_log1:1;
    unsigned int x_steady:1;
} t_iem_fstyle_flags;

typedef struct _iem_init_symargs
{
    unsigned int x_loadinit:1;
    unsigned int x_rcv_arg_tail_len:6;
    unsigned int x_snd_arg_tail_len:6;
    unsigned int x_rcv_is_arg_num:6;
    unsigned int x_snd_is_arg_num:6;
    unsigned int x_scale:1;
    unsigned int x_flashed:1;
    unsigned int x_locked:1;
} t_iem_init_symargs;

typedef void (*t_iemfunptr)(void *x, t_glist *glist, int mode);

typedef struct _iemgui
{
    t_object           x_obj;
    t_glist            *x_glist;
    t_iemfunptr        x_draw;
    int                x_h;
    int                x_w;
    int                x_ldx;
    int                x_ldy;
    char               x_font[MAXPDSTRING]; /* font names can be long! */
    t_iem_fstyle_flags x_fsf;
    int                x_fontsize;
    t_iem_init_symargs x_isa;
    int                x_fcol;
    int                x_bcol;
    int                x_lcol;
    t_symbol           *x_snd;              /* send symbol */
    t_symbol           *x_rcv;              /* receive */
    t_symbol           *x_lab;              /* label */
    t_symbol           *x_snd_unexpanded;   /* same 3, with '$' unexpanded */
    t_symbol           *x_rcv_unexpanded;
    t_symbol           *x_lab_unexpanded;
    int                x_binbufindex;       /* where in binbuf to find these */
    int                x_labelbindex;       /* where in binbuf to find label */
} t_iemgui;

typedef struct _bng
{
    t_iemgui x_gui;
    int      x_flashed;
    int      x_flashtime_break;
    int      x_flashtime_hold;
    t_clock  *x_clock_hld;
    t_clock  *x_clock_brk;
    t_clock  *x_clock_lck;
    double x_lastflashtime;
} t_bng;

typedef struct _hslider
{
    t_iemgui x_gui;
    int      x_pos;
    int      x_val;
    int      x_lin0_log1;
    int      x_steady;
    double   x_min;
    double   x_max;
    double   x_k;
    t_float  x_fval;
} t_hslider;

typedef struct _hdial
{
    t_iemgui x_gui;
    int      x_on;
    int      x_on_old;  /* LATER delete this; it's used for old version */
    int      x_change;
    int      x_number;
    int      x_drawn;
    t_float  x_fval;
    t_atom   x_at[2];
} t_hdial;

typedef struct _toggle
{
    t_iemgui x_gui;
    t_float    x_on;
    t_float    x_nonzero;
} t_toggle;

typedef struct _my_canvas
{
    t_iemgui x_gui;
    t_atom   x_at[3];
    int      x_vis_w;
    int      x_vis_h;
} t_my_canvas;

typedef struct _vslider
{
    t_iemgui x_gui;
    int      x_pos;
    int      x_val;
    int      x_lin0_log1;
    int      x_steady;
    double   x_min;
    double   x_max;
    double   x_k;
    t_float  x_fval;
} t_vslider;

typedef struct _vu
{
    t_iemgui x_gui;
    int      x_led_size;
    int      x_peak;
    int      x_rms;
    t_float  x_fp;
    t_float  x_fr;
    int      x_scale;
    void     *x_out_rms;
    void     *x_out_peak;
    unsigned int x_updaterms:1;
    unsigned int x_updatepeak:1;
} t_vu;

typedef struct _my_numbox
{
    t_iemgui x_gui;
    t_clock  *x_clock_reset;
    t_clock  *x_clock_wait;
    double   x_val;
    double   x_min;
    double   x_max;
    double   x_k;
    int      x_lin0_log1;
    char     x_buf[IEMGUI_MAX_NUM_LEN];
    int      x_numwidth;
    int      x_log_height;
} t_my_numbox;

typedef struct _vdial
{
    t_iemgui x_gui;
    int      x_on;
    int      x_on_old;
    int      x_change;
    int      x_number;
    int      x_drawn;
    t_float  x_fval;
    t_atom   x_at[2];
} t_vdial;

#define t_vradio t_vdial
#define t_hradio t_hdial

extern int iemgui_color_hex[];
extern int iemgui_vu_db2i[];
extern int iemgui_vu_col[];
extern char *iemgui_vu_scale_str[];

EXTERN int iemgui_clip_size(int size);
EXTERN int iemgui_clip_font(int size);
EXTERN t_symbol *iemgui_unique2dollarzero(t_symbol *s, int unique_num, int and_unique_flag);
EXTERN t_symbol *iemgui_sym2dollararg(t_symbol *s, int nth_arg, int tail_len);
EXTERN t_symbol *iemgui_dollarzero2unique(t_symbol *s, int unique_num);
EXTERN t_symbol *iemgui_dollararg2sym(t_symbol *s, int nth_arg, int tail_len, int pargc, t_atom *pargv);
EXTERN int iemgui_is_dollarzero(t_symbol *s);
EXTERN int iemgui_is_dollararg(t_symbol *s, int *tail_len);
EXTERN void iemgui_fetch_unique(t_iemgui *iemgui);
EXTERN void iemgui_fetch_parent_args(t_iemgui *iemgui, int *pargc, t_atom **pargv);
EXTERN void iemgui_verify_snd_ne_rcv(t_iemgui *iemgui);
EXTERN void iemgui_all_unique2dollarzero(t_iemgui *iemgui, t_symbol **srlsym);
EXTERN void iemgui_all_sym2dollararg(t_iemgui *iemgui, t_symbol **srlsym);
EXTERN void iemgui_all_dollarzero2unique(t_iemgui *iemgui, t_symbol **srlsym);
EXTERN t_symbol *iemgui_new_dogetname(t_iemgui *iemgui, int indx, t_atom *argv);
EXTERN void iemgui_new_getnames(t_iemgui *iemgui, int indx, t_atom *argv);
EXTERN void iemgui_all_dollararg2sym(t_iemgui *iemgui, t_symbol **srlsym);
EXTERN void iemgui_all_loadcolors(t_iemgui *iemgui, t_atom*bcol, t_atom*fcol, t_atom*lcol);
EXTERN void iemgui_all_dollar2raute(t_symbol **srlsym);
EXTERN void iemgui_all_raute2dollar(t_symbol **srlsym);
EXTERN void iemgui_send(void *x, t_iemgui *iemgui, t_symbol *s);
EXTERN void iemgui_receive(void *x, t_iemgui *iemgui, t_symbol *s);
EXTERN void iemgui_label(void *x, t_iemgui *iemgui, t_symbol *s);
EXTERN void iemgui_label_pos(void *x, t_iemgui *iemgui, t_symbol *s, int ac, t_atom *av);
EXTERN void iemgui_label_font(void *x, t_iemgui *iemgui, t_symbol *s, int ac, t_atom *av);
EXTERN void iemgui_size(void *x, t_iemgui *iemgui);
EXTERN void iemgui_delta(void *x, t_iemgui *iemgui, t_symbol *s, int ac, t_atom *av);
EXTERN void iemgui_pos(void *x, t_iemgui *iemgui, t_symbol *s, int ac, t_atom *av);
EXTERN void iemgui_color(void *x, t_iemgui *iemgui, t_symbol *s, int ac, t_atom *av);
EXTERN int iemgui_list(void *x, t_iemgui *iemgui, t_symbol *s, int ac, t_atom *av);
EXTERN void iemgui_displace(t_gobj *z, t_glist *glist, int dx, int dy);
EXTERN void iemgui_select(t_gobj *z, t_glist *glist, int selected);
EXTERN void iemgui_delete(t_gobj *z, t_glist *glist);
EXTERN void iemgui_vis(t_gobj *z, t_glist *glist, int vis);
EXTERN void iemgui_save(t_iemgui *iemgui, t_symbol **srl, t_symbol **bflcol);
EXTERN void iemgui_zoom(t_iemgui *iemgui, t_floatarg zoom);
EXTERN void iemgui_properties(t_iemgui *iemgui, t_symbol **srl);
EXTERN int iemgui_dialog(t_iemgui *iemgui, t_symbol **srl, int argc, t_atom *argv);

EXTERN int canvas_getdollarzero(void);
EXTERN void canvas_getargs(int *argcp, t_atom **argvp);

EXTERN void iem_inttosymargs(t_iem_init_symargs *symargp, int n);
EXTERN int iem_symargstoint(t_iem_init_symargs *symargp);
EXTERN void iem_inttofstyle(t_iem_fstyle_flags *fstylep, int n);
EXTERN int iem_fstyletoint(t_iem_fstyle_flags *fstylep);
