#ifndef PD_S_INTER_GUI_H
#define PD_S_INTER_GUI_H

#include "m_pd.h"
#include <stdarg.h>
#include <stddef.h>

/* Private Tcl/Tk transport.  Only the Tk backend and legacy stub transport
 * may include this header. */
void pdgui_vmess(const char *destination, const char *fmt, ...);
void pdgui_vamess(const char *destination, const char *fmt, va_list args);
void pdgui_startmess(void);
void pdgui_endmess(void);
char *pdgui_strnescape(char *dst, size_t dstlen, const char *src,
    size_t srclen);
int pdgui_tk_transport_init(const char *libdir);

#endif /* PD_S_INTER_GUI_H */
