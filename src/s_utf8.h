#ifndef S_UTF8_H
#define S_UTF8_H

/*--moo--*/
#ifndef u_int32_t
# define u_int32_t unsigned int
#endif

#ifndef UCS4
# define UCS4 u_int32_t
#endif

/* UTF8_SUPPORT_FULL_UCS4
 *  define this to support the full potential range of UCS-4 codepoints
 *  (in anticipation of a future UTF-8 standard)
 */
/*#define UTF8_SUPPORT_FULL_UCS4 1*/
#undef UTF8_SUPPORT_FULL_UCS4

/* UTF8_MAXBYTES
 *   maximum number of bytes required to represent a single character in UTF-8
 *
 * UTF8_MAXBYTES1 = UTF8_MAXBYTES+1 
 *  maximum bytes per character including NUL terminator
 */
#ifdef UTF8_SUPPORT_FULL_UCS4
# ifndef UTF8_MAXBYTES
#  define UTF8_MAXBYTES  6
# endif
# ifndef UTF8_MAXBYTES1
#  define UTF8_MAXBYTES1 7
# endif
#else
# ifndef UTF8_MAXBYTES
#  define UTF8_MAXBYTES  4
# endif
# ifndef UTF8_MAXBYTES1
#  define UTF8_MAXBYTES1 5
# endif
#endif
/*--/moo--*/

/* is c the start of a utf8 sequence? */
#define isutf(c) (((c)&0xC0)!=0x80)

/* convert UTF-8 data to wide character */
int u8_toucs(u_int32_t *dest, int sz, char *src, int srcsz);

/* the opposite conversion */
int u8_toutf8(char *dest, int sz, u_int32_t *src, int srcsz);

/* moo: get byte length of character number, or 0 if not supported */
int u8_wc_nbytes(u_int32_t ch);

/* moo: compute required storage for UTF-8 encoding of 's[0..n-1]' */
int u8_wcs_nbytes(u_int32_t *ucs, int size);

/* single character to UTF-8, no NUL termination */
int u8_wc_toutf8(char *dest, u_int32_t ch);

/* moo: single character to UTF-8, with NUL termination */
int u8_wc_toutf8_nul(char *dest, u_int32_t ch);

/* character number to byte offset */
int u8_offset(char *str, int charnum);

/* byte offset to character number */
int u8_charnum(char *s, int offset);

/* return next character, updating an index variable */
u_int32_t u8_nextchar(char *s, int *i);

/* move to next character */
void u8_inc(char *s, int *i);

/* move to previous character */
void u8_dec(char *s, int *i);

/* moo: move pointer to next character */
void u8_inc_ptr(char **sp);

/* moo: move pointer to previous character */
void u8_dec_ptr(char **sp);

/* returns length of next utf-8 sequence */
int u8_seqlen(char *s);

#endif /* S_UTF8_H */
