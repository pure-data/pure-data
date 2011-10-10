/*
  Basic UTF-8 manipulation routines
  by Jeff Bezanson
  placed in the public domain Fall 2005

  This code is designed to provide the utilities you need to manipulate
  UTF-8 as an internal string encoding. These functions do not perform the
  error checking normally needed when handling UTF-8 data, so if you happen
  to be from the Unicode Consortium you will want to flay me alive.
  I do this because error checking can be performed at the boundaries (I/O),
  with these routines reserved for higher performance on data known to be
  valid.

  modified by Bryan Jurish (moo) March 2009
  + removed some unneeded functions (escapes, printf etc), added others
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "s_utf8.h"

/* moo: get byte length of character number, or 0 if not supported */
int u8_wc_nbytes(u_int32_t ch)
{
  if (ch < 0x80) return 1;
  if (ch < 0x800) return 2;
  if (ch < 0x10000) return 3;
  if (ch < 0x200000) return 4;
  return 0; /*-- bad input --*/
}

int u8_wc_toutf8(char *dest, u_int32_t ch)
{
    if (ch < 0x80) {
        dest[0] = (char)ch;
        return 1;
    }
    if (ch < 0x800) {
        dest[0] = (ch>>6) | 0xC0;
        dest[1] = (ch & 0x3F) | 0x80;
        return 2;
    }
    if (ch < 0x10000) {
        dest[0] = (ch>>12) | 0xE0;
        dest[1] = ((ch>>6) & 0x3F) | 0x80;
        dest[2] = (ch & 0x3F) | 0x80;
        return 3;
    }
    if (ch < 0x110000) {
        dest[0] = (ch>>18) | 0xF0;
        dest[1] = ((ch>>12) & 0x3F) | 0x80;
        dest[2] = ((ch>>6) & 0x3F) | 0x80;
        dest[3] = (ch & 0x3F) | 0x80;
        return 4;
    }
    return 0;
}

/*-- moo --*/
int u8_wc_toutf8_nul(char *dest, u_int32_t ch)
{
  int sz = u8_wc_toutf8(dest,ch);
  dest[sz] = '\0';
  return sz;
}

/* charnum => byte offset */
int u8_offset(char *str, int charnum)
{
    char *string = str;

    while (charnum > 0 && *string != '\0') {
        if (*string++ & 0x80) {
            if (!isutf(*string)) {
                ++string;
                if (!isutf(*string)) {
                    ++string;
                    if (!isutf(*string)) {
                        ++string;
                    }
                }
            }
        }
        --charnum;
    }

    return (int)(string - str);
}

/* byte offset => charnum */
int u8_charnum(char *s, int offset)
{
    int charnum = 0;
    char *string = s;
    char *const end = string + offset;

    while (string < end && *string != '\0') {
        if (*string++ & 0x80) {
            if (!isutf(*string)) {
                ++string;
                if (!isutf(*string)) {
                    ++string;
                    if (!isutf(*string)) {
                        ++string;
                    }
                }
            }
        }
        ++charnum;
    }
    return charnum;
}

void u8_inc(char *s, int *i)
{
    if (s[(*i)++] & 0x80) {
        if (!isutf(s[*i])) {
            ++(*i);
            if (!isutf(s[*i])) {
                ++(*i);
                if (!isutf(s[*i])) {
                    ++(*i);
                }
            }
        }
    }
}

void u8_dec(char *s, int *i)
{
    (void)(isutf(s[--(*i)]) || isutf(s[--(*i)]) ||
           isutf(s[--(*i)]) || --(*i));
}

