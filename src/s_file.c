/* Copyright (c) 1997-2004 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/*
 * this file implements a mechanism for storing and retrieving preferences.
 * Should later be renamed "preferences.c" or something.
 *
 * In unix this is handled by the "~/.pdsettings" file, in windows by
 * the registry, and in MacOS by the Preferences system.
 */

#include "m_pd.h"
#include "s_stuff.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif
#ifdef _WIN32
#include <windows.h>
#include <tchar.h>
#include <io.h>
#endif
#ifdef _MSC_VER  /* This is only for Microsoft's compiler, not cygwin, e.g. */
#define snprintf _snprintf
#endif
#ifdef __APPLE__ /* needed for plist handling */
#include <CoreFoundation/CoreFoundation.h>
#endif

void sys_doflags(void);

static PERTHREAD char *sys_prefbuf;
static PERTHREAD int sys_prefbufsize;
static PERTHREAD FILE *sys_prefsavefp;

static void sys_initloadpreferences_file(const char *filename)
{
    int fd;
    long length;
    if ((fd = open(filename, 0)) < 0)
    {
        if (sys_verbose)
            perror(filename);
        return;
    }
    length = lseek(fd, 0, 2);
    if (length < 0)
    {
        if (sys_verbose)
            perror(filename);
        close(fd);
        return;
    }
    lseek(fd, 0, 0);
    if (!(sys_prefbuf = malloc(length + 2)))
    {
        pd_error(0, "couldn't allocate memory for preferences buffer");
        close(fd);
        return;
    }
    sys_prefbuf[0] = '\n';
    if (read(fd, sys_prefbuf+1, length) < length)
    {
        perror(filename);
        sys_prefbuf[0] = 0;
        close(fd);
        return;
    }
    sys_prefbuf[length+1] = 0;
    close(fd);
    logpost(NULL, PD_VERBOSE, "success reading preferences from: %s", filename);
}

static int sys_getpreference_file(const char *key, char *value, int size)
{
    char searchfor[80], *where, *whereend;
    if (!sys_prefbuf)
        return (0);
    sprintf(searchfor, "\n%s:", key);
    where = strstr(sys_prefbuf, searchfor);
    if (!where)
        return (0);
    where += strlen(searchfor);
    while (*where == ' ' || *where == '\t')
        where++;
    for (whereend = where; *whereend && *whereend != '\n'; whereend++)
        ;
    if (*whereend == '\n')
        whereend--;
    if (whereend > where + size - 1)
        whereend = where + size - 1;
    strncpy(value, where, whereend+1-where);
    value[whereend+1-where] = 0;
    return (1);
}

static void sys_doneloadpreferences_file(void)
{
    if (sys_prefbuf)
        free(sys_prefbuf);
}

static void sys_initsavepreferences_file(const char *filename)
{
    if ((sys_prefsavefp = fopen(filename, "w")) == NULL)
        pd_error(0, "%s: %s", filename, strerror(errno));
}

static void sys_putpreference_file(const char *key, const char *value)
{
    if (sys_prefsavefp)
        fprintf(sys_prefsavefp, "%s: %s\n",
            key, value);
}

static void sys_donesavepreferences_file(void)
{
    if (sys_prefsavefp)
    {
        fclose(sys_prefsavefp);
        sys_prefsavefp = 0;
    }
}

#if defined(__APPLE__)
/*****  macos: read and write to ~/Library/Preferences plist file ******/

static PERTHREAD CFMutableDictionaryRef sys_prefdict = NULL;

// get preferences file load path into dst, returns 1 if embedded
static int preferences_getloadpath(char *dst, size_t size)
{
    char embedded_prefs[MAXPDSTRING];
    char user_prefs[MAXPDSTRING];
    char *homedir = getenv("HOME");
    struct stat statbuf;
    snprintf(embedded_prefs, MAXPDSTRING, "%s/../org.puredata.pd",
        sys_libdir->s_name);
    snprintf(user_prefs, MAXPDSTRING,
        "%s/Library/Preferences/org.puredata.pd.plist", homedir);
    if (stat(user_prefs, &statbuf) == 0)
    {
        strncpy(dst, user_prefs, size);
        return 0;
    }
    else
    {
        strncpy(dst, embedded_prefs, size);
        return 1;
    }
}

// get preferences file save path
static void preferences_getsavepath(char *dst, size_t size)
{
    char user_prefs[MAXPDSTRING];
    snprintf(user_prefs, MAXPDSTRING,
        "%s/Library/Preferences/org.puredata.pd.plist", getenv("HOME"));
    strncpy(dst, user_prefs, size);
}

static void sys_initloadpreferences(void)
{
    char user_prefs[MAXPDSTRING];
    CFStringRef path = NULL;
    CFURLRef fileURL = NULL;
    CFReadStreamRef stream = NULL;
    CFErrorRef err = NULL;
    CFPropertyListRef plist = NULL;

    if (sys_prefbuf || sys_prefdict)
    {
        bug("sys_initloadpreferences");
        return;
    }

    // open read stream
    preferences_getloadpath(user_prefs, MAXPDSTRING);
    path = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, user_prefs,
        kCFStringEncodingUTF8, kCFAllocatorNull);
    fileURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path,
        kCFURLPOSIXPathStyle, false); // false -> not a directory
    stream = CFReadStreamCreateWithFile(kCFAllocatorDefault, fileURL);
    if (!stream || !CFReadStreamOpen(stream)) goto cleanup;

    // read plist
    plist = CFPropertyListCreateWithStream(kCFAllocatorDefault, stream, 0,
        kCFPropertyListImmutable, NULL, &err);
    if (!plist) {
        CFStringRef errString = CFErrorCopyDescription(err);
        pd_error(0, "couldn't read preferences plist: %s",
            CFStringGetCStringPtr(errString, kCFStringEncodingUTF8));
        CFRelease(errString);
        goto cleanup;
    }
    CFRetain(plist);
    sys_prefdict = (CFMutableDictionaryRef)plist;

cleanup:
    if (stream) {
        if (CFReadStreamGetStatus(stream) == kCFStreamStatusOpen) {
            CFReadStreamClose(stream);
        }
        CFRelease(stream);
    }
    if (fileURL) {CFRelease(fileURL);}
    if (path) {CFRelease(path);}
    if (err) {CFRelease(err);}
}

static void sys_doneloadpreferences(void)
{
    if (sys_prefbuf)
        sys_doneloadpreferences_file();
    if (sys_prefdict)
    {
        CFRelease(sys_prefdict);
        sys_prefdict = NULL;
    }
}

static void sys_initsavepreferences(void)
{
    if (sys_prefsavefp)
    {
        bug("sys_initsavepreferences");
        return;
    }
    sys_prefdict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

static void sys_donesavepreferences(void)
{
    char user_prefs[MAXPDSTRING];
    CFStringRef path = NULL;
    CFURLRef fileURL = NULL;
    CFWriteStreamRef stream = NULL;
    CFErrorRef err = NULL;
    CFDataRef data = NULL;

    if (sys_prefsavefp)
        sys_donesavepreferences_file();
    if (!sys_prefdict) return;

    // convert dict to plist data
    data = CFPropertyListCreateData(kCFAllocatorDefault,
                                    (CFPropertyListRef)sys_prefdict,
                                    kCFPropertyListBinaryFormat_v1_0, 0, &err);
    if (!data)
    {
        CFStringRef errString = CFErrorCopyDescription(err);
        pd_error(0, "couldn't write preferences plist: %s",
            CFStringGetCStringPtr(errString, kCFStringEncodingUTF8));
        CFRelease(errString);
        goto cleanup;
    }

    // open write stream
    preferences_getsavepath(user_prefs, MAXPDSTRING);
    path = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, user_prefs,
        kCFStringEncodingUTF8, kCFAllocatorNull);
    fileURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path,
        kCFURLPOSIXPathStyle, false); // false -> not a directory
    stream = CFWriteStreamCreateWithFile(kCFAllocatorDefault, fileURL);
    if (!stream || !CFWriteStreamOpen(stream)) goto cleanup;

    // write plist
    if (CFWriteStreamWrite(stream, CFDataGetBytePtr(data),
                                   CFDataGetLength(data)) < 0) {
        pd_error(0, "couldn't write preferences plist");
        goto cleanup;
    }

cleanup:
    if (sys_prefdict)
    {
        CFRelease(sys_prefdict);
        sys_prefdict = NULL;
    }
    if (data) {CFRelease(data);}
    if (stream) {
        if(CFWriteStreamGetStatus(stream) == kCFStreamStatusOpen) {
            CFWriteStreamClose(stream);
        }
        CFRelease(stream);
    }
    if (fileURL) {CFRelease(fileURL);}
    if (path) {CFRelease(path);}
    if (err) {CFRelease(err);}
}

static int sys_getpreference(const char *key, char *value, int size)
{
    if (sys_prefbuf)
        return (sys_getpreference_file(key, value, size));
    if (sys_prefdict) {
        /* read from loaded plist dict */
        CFStringRef k = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault,
            key, kCFStringEncodingUTF8, kCFAllocatorNull);
        void *v = NULL;
        int ret = 0;
        if (CFDictionaryGetValueIfPresent(sys_prefdict, k,
                                          (const void **)&v)) {
            ret = CFStringGetCString((CFStringRef)v, value, size,
                                     kCFStringEncodingUTF8);
#if 0
            if (ret) fprintf(stderr, "plist read %s = %s\n", key, value);
#endif
            if (v) CFRelease(v);
        }
        CFRelease(k);
        return (ret);
    }
    else {
        /* fallback to defaults command */
        char cmdbuf[256];
        int nread = 0, nleft = size;
        char path[MAXPDSTRING];
        int embedded = preferences_getloadpath(path, MAXPDSTRING);
        if (embedded)
            snprintf(cmdbuf, 256, "defaults read %s %s 2> /dev/null\n",
                path, key);
        else
            snprintf(cmdbuf, 256, "defaults read org.puredata.pd %s 2> /dev/null\n",
                key);
        FILE *fp = popen(cmdbuf, "r");
        while (nread < size)
        {
            int newread = fread(value+nread, 1, size-nread, fp);
            if (newread <= 0)
                break;
            nread += newread;
        }
        pclose(fp);
        if (nread < 1)
            return (0);
        if (nread >= size)
            nread = size-1;
        value[nread] = 0;
        if (value[nread-1] == '\n')     /* remove newline character at end */
            value[nread-1] = 0;
        return (1);
    }
}

static void sys_putpreference(const char *key, const char *value)
{
    if (sys_prefsavefp)
    {
        sys_putpreference_file(key, value);
        return;
    }
    if (sys_prefdict) {
        CFStringRef k = CFStringCreateWithCString(kCFAllocatorDefault, key,
                                                  kCFStringEncodingUTF8);
        CFStringRef v = CFStringCreateWithCString(kCFAllocatorDefault, value,
                                                  kCFStringEncodingUTF8);
        CFDictionarySetValue((CFMutableDictionaryRef)sys_prefdict, k, v);
        CFRelease(k);
        CFRelease(v);
#if 0
        fprintf(stderr, "plist write %s = %s\n", key, value);
#endif
    }
    else {
        /* fallback to defaults command */
        char cmdbuf[MAXPDSTRING];
        snprintf(cmdbuf, MAXPDSTRING,
            "defaults write org.puredata.pd %s \"%s\" 2> /dev/null\n", key, value);
        system(cmdbuf);
    }
}

#elif defined(_WIN32)
/*****  windows: read and write to registry ******/

static void sys_initloadpreferences(void)
{
    if (sys_prefbuf)
        bug("sys_initloadpreferences");
}

static void sys_doneloadpreferences(void)
{
    if (sys_prefbuf)
        sys_doneloadpreferences_file();
}

static void sys_initsavepreferences(void)
{
    if (sys_prefsavefp)
        bug("sys_initsavepreferences");
}

static void sys_donesavepreferences(void)
{
    if (sys_prefsavefp)
        sys_donesavepreferences_file();
}

static int sys_getpreference(const char *key, char *value, int size)
{
    if (sys_prefbuf)
        return (sys_getpreference_file(key, value, size));
    else
    {
        HKEY hkey;
        DWORD bigsize = size;
        LONG err = RegOpenKeyEx(HKEY_CURRENT_USER,
            "Software\\Pure-Data", 0,  KEY_QUERY_VALUE, &hkey);
        if (err != ERROR_SUCCESS)
            return (0);
        err = RegQueryValueEx(hkey, key, 0, 0, value, &bigsize);
        if (err != ERROR_SUCCESS)
        {
            RegCloseKey(hkey);
            return (0);
        }
        RegCloseKey(hkey);
        return (1);
    }
}

static void sys_putpreference(const char *key, const char *value)
{
    if (sys_prefsavefp)
        sys_putpreference_file(key, value);
    else
    {
        HKEY hkey;
        LONG err = RegCreateKeyEx(HKEY_CURRENT_USER,
            "Software\\Pure-Data", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE,
            NULL, &hkey, NULL);
        if (err != ERROR_SUCCESS)
        {
            pd_error(0, "unable to create registry entry: %s\n", key);
            return;
        }
        err = RegSetValueEx(hkey, key, 0, REG_EXPAND_SZ, value, strlen(value)+1);
        if (err != ERROR_SUCCESS)
            pd_error(0, "unable to set registry entry: %s\n", key);
        RegCloseKey(hkey);
    }
}

#else
/*****  linux/android/BSD etc: read and write to ~/.pdsettings file ******/

static void sys_initloadpreferences(void)
{
    char filenamebuf[MAXPDSTRING], *homedir = getenv("HOME");
    int fd, length;
    char user_prefs_file[MAXPDSTRING]; /* user prefs file */
        /* default prefs embedded in the package */
    char default_prefs_file[MAXPDSTRING];
    struct stat statbuf;

    snprintf(default_prefs_file, MAXPDSTRING, "%s/default.pdsettings",
        sys_libdir->s_name);
    snprintf(user_prefs_file, MAXPDSTRING, "%s/.pdsettings",
        (homedir ? homedir : "."));
    if (stat(user_prefs_file, &statbuf) == 0)
        strncpy(filenamebuf, user_prefs_file, MAXPDSTRING);
    else if (stat(default_prefs_file, &statbuf) == 0)
        strncpy(filenamebuf, default_prefs_file, MAXPDSTRING);
    else return;
    filenamebuf[MAXPDSTRING-1] = 0;
    sys_initloadpreferences_file(filenamebuf);
}

static int sys_getpreference(const char *key, char *value, int size)
{
    return (sys_getpreference_file(key, value, size));
}

static void sys_doneloadpreferences(void)
{
    sys_doneloadpreferences_file();
}

static void sys_initsavepreferences(void)
{
    char filenamebuf[MAXPDSTRING],
        *homedir = getenv("HOME");
    FILE *fp;

    if (!homedir)
        return;
    snprintf(filenamebuf, MAXPDSTRING, "%s/.pdsettings", homedir);
    filenamebuf[MAXPDSTRING-1] = 0;
    sys_initsavepreferences_file(filenamebuf);
}

static void sys_putpreference(const char *key, const char *value)
{
    sys_putpreference_file(key, value);
}

static void sys_donesavepreferences(void)
{
    sys_donesavepreferences_file();
}

#endif

void sys_loadpreferences(const char *filename, int startingup)
{
    t_audiosettings as;
    int nmidiindev, midiindev[MAXMIDIINDEV];
    int nmidioutdev, midioutdev[MAXMIDIOUTDEV];
    int midiapi, nolib, maxi, i;
    char prefbuf[MAXPDSTRING], keybuf[80];
    sys_get_audio_settings(&as);

    if (*filename)
        sys_initloadpreferences_file(filename);
    else sys_initloadpreferences();
        /* load audio preferences */
    if (!sys_externalschedlib
        && sys_getpreference("audioapi", prefbuf, MAXPDSTRING)
        && sscanf(prefbuf, "%d", &as.a_api) < 1)
            as.a_api = -1;
            /* JMZ/MB: brackets for initializing */
    if (sys_getpreference("noaudioin", prefbuf, MAXPDSTRING) &&
        (!strcmp(prefbuf, ".") || !strcmp(prefbuf, "True")))
            as.a_nindev = 0;
    else
    {
        for (as.a_nindev = 0; as.a_nindev < MAXAUDIOINDEV; as.a_nindev++)
        {
                /* first try to find a name - if that matches an existing
                device use it.  Otherwise fall back to device number. */
            int devn;
                /* read in device number and channel count */
            sprintf(keybuf, "audioindev%d", as.a_nindev+1);
            if (!sys_getpreference(keybuf, prefbuf, MAXPDSTRING))
                break;
            if (sscanf(prefbuf, "%d %d",
                &as.a_indevvec[as.a_nindev], &as.a_chindevvec[as.a_nindev]) < 2)
                    break;
                /* possibly override device number if the device name was
                also saved and if it matches one we have now */
            sprintf(keybuf, "audioindevname%d", as.a_nindev+1);
            if (sys_getpreference(keybuf, prefbuf, MAXPDSTRING)
                && (devn = sys_audiodevnametonumber(0, prefbuf)) >= 0)
                    as.a_indevvec[as.a_nindev] = devn;
            as.a_nindev++;
        }
            /* if no preferences at all, set -1 for default behavior */
        if (as.a_nindev == 0)
            as.a_nindev = -1;
    }
        /* JMZ/MB: brackets for initializing */
    if (sys_getpreference("noaudioout", prefbuf, MAXPDSTRING) &&
        (!strcmp(prefbuf, ".") || !strcmp(prefbuf, "True")))
            as.a_noutdev = 0;
    else
    {
        for (as.a_noutdev = 0; as.a_noutdev < MAXAUDIOOUTDEV; as.a_noutdev++)
        {
            int devn;
            sprintf(keybuf, "audiooutdev%d", as.a_noutdev+1);
            if (!sys_getpreference(keybuf, prefbuf, MAXPDSTRING))
                break;
            if (sscanf(prefbuf, "%d %d",
                &as.a_outdevvec[as.a_noutdev],
                    &as.a_choutdevvec[as.a_noutdev]) < 2)
                        break;
            sprintf(keybuf, "audiooutdevname%d", as.a_noutdev+1);
            if (sys_getpreference(keybuf, prefbuf, MAXPDSTRING)
                && (devn = sys_audiodevnametonumber(1, prefbuf)) >= 0)
                    as.a_outdevvec[as.a_noutdev] = devn;
            as.a_noutdev++;
        }
        if (as.a_noutdev == 0)
            as.a_noutdev = -1;
    }
    if (sys_getpreference("rate", prefbuf, MAXPDSTRING))
        sscanf(prefbuf, "%d", &as.a_srate);
    if (sys_getpreference("audiobuf", prefbuf, MAXPDSTRING))
        sscanf(prefbuf, "%d", &as.a_advance);
    if (sys_getpreference("callback", prefbuf, MAXPDSTRING))
        sscanf(prefbuf, "%d", &as.a_callback);
    if (sys_getpreference("audioblocksize", prefbuf, MAXPDSTRING))
        sscanf(prefbuf, "%d", &as.a_blocksize);
#ifndef _WIN32
    else if (sys_getpreference("blocksize", prefbuf, MAXPDSTRING))
        sscanf(prefbuf, "%d", &as.a_blocksize);
#endif
    sys_set_audio_settings(&as);

        /* load MIDI preferences */
    if (sys_getpreference("midiapi", prefbuf, MAXPDSTRING)
        && sscanf(prefbuf, "%d", &midiapi) > 0)
            sys_set_midi_api(midiapi);
        /* JMZ/MB: brackets for initializing */
    if (sys_getpreference("nomidiin", prefbuf, MAXPDSTRING) &&
        (!strcmp(prefbuf, ".") || !strcmp(prefbuf, "True")))
            nmidiindev = 0;
    else for (nmidiindev = 0; nmidiindev < MAXMIDIINDEV; nmidiindev++)
    {
            /* first try to find a name - if that matches an existing device
            use it.  Otherwise fall back to device number. */
        int devn;
        sprintf(keybuf, "midiindevname%d", nmidiindev+1);
        if (sys_getpreference(keybuf, prefbuf, MAXPDSTRING)
            && (devn = sys_mididevnametonumber(0, prefbuf)) >= 0)
                midiindev[nmidiindev] = devn;
        else
        {
            sprintf(keybuf, "midiindev%d", nmidiindev+1);
            if (!sys_getpreference(keybuf, prefbuf, MAXPDSTRING))
                break;
            if (sscanf(prefbuf, "%d", &midiindev[nmidiindev]) < 1)
                break;
        }
    }
        /* JMZ/MB: brackets for initializing */
    if (sys_getpreference("nomidiout", prefbuf, MAXPDSTRING) &&
        (!strcmp(prefbuf, ".") || !strcmp(prefbuf, "True")))
            nmidioutdev = 0;
    else for (nmidioutdev = 0; nmidioutdev < MAXMIDIOUTDEV; nmidioutdev++)
    {
        int devn;
        sprintf(keybuf, "midioutdevname%d", nmidioutdev+1);
        if (sys_getpreference(keybuf, prefbuf, MAXPDSTRING)
            && (devn = sys_mididevnametonumber(1, prefbuf)) >= 0)
                midioutdev[nmidioutdev] = devn;
        else
        {
            sprintf(keybuf, "midioutdev%d", nmidioutdev+1);
            if (!sys_getpreference(keybuf, prefbuf, MAXPDSTRING))
                break;
            if (sscanf(prefbuf, "%d", &midioutdev[nmidioutdev]) < 1)
                break;
        }
    }
    sys_open_midi(nmidiindev, midiindev, nmidioutdev, midioutdev, 0);

        /* search path */
    if (sys_getpreference("npath", prefbuf, MAXPDSTRING))
        sscanf(prefbuf, "%d", &maxi);
    else maxi = 0x7fffffff;
    for (i = 0; i < maxi; i++)
    {
        sprintf(keybuf, "path%d", i+1);
        if (!sys_getpreference(keybuf, prefbuf, MAXPDSTRING))
            break;
        STUFF->st_searchpath =
            namelist_append_files(STUFF->st_searchpath, prefbuf);
    }
    if (sys_getpreference("standardpath", prefbuf, MAXPDSTRING))
        sscanf(prefbuf, "%d", &sys_usestdpath);
    if (sys_getpreference("verbose", prefbuf, MAXPDSTRING))
        sscanf(prefbuf, "%d", &sys_verbose);

        /* startup settings */
    if (sys_getpreference("nloadlib", prefbuf, MAXPDSTRING))
        sscanf(prefbuf, "%d", &maxi);
    else maxi = 0x7fffffff;
    for (i = 0; i<maxi; i++)
    {
        sprintf(keybuf, "loadlib%d", i+1);
        if (!sys_getpreference(keybuf, prefbuf, MAXPDSTRING))
            break;
        STUFF->st_externlist = namelist_append_files(STUFF->st_externlist, prefbuf);
    }
    if (sys_getpreference("defeatrt", prefbuf, MAXPDSTRING))
        sscanf(prefbuf, "%d", &sys_defeatrt);
    if (sys_getpreference("flags", prefbuf, MAXPDSTRING) &&
        strcmp(prefbuf, "."))
    {
        sys_flags = gensym(prefbuf);
        if (startingup)
            sys_doflags();
    }
    if (sys_defeatrt)
        sys_hipriority = 0;
    else
#if defined(ANDROID)
        sys_hipriority = 0;
#else
        sys_hipriority = 1;
#endif
    if (sys_getpreference("zoom", prefbuf, MAXPDSTRING))
        sscanf(prefbuf, "%d", &sys_zoom_open);

    sys_doneloadpreferences();
}

void sys_savepreferences(const char *filename)
{
    t_audiosettings as;
    int i;
    char buf1[MAXPDSTRING], buf2[MAXPDSTRING];
    int nmidiindev, midiindev[MAXMIDIINDEV];
    int nmidioutdev, midioutdev[MAXMIDIOUTDEV];

    if (filename && *filename)
        sys_initsavepreferences_file(filename);
    else sys_initsavepreferences();
        /* audio settings */
    sys_get_audio_settings(&as);

    sprintf(buf1, "%d", as.a_api);
    sys_putpreference("audioapi", buf1);
    sys_putpreference("noaudioin", (as.a_nindev <= 0 ? "True":"False"));
    for (i = 0; i < as.a_nindev; i++)
    {
        sprintf(buf1, "audioindev%d", i+1);
        sprintf(buf2, "%d %d", as.a_indevvec[i], as.a_chindevvec[i]);
        sys_putpreference(buf1, buf2);
        sprintf(buf1, "audioindevname%d", i+1);
        sys_audiodevnumbertoname(0, as.a_indevvec[i], buf2, MAXPDSTRING);
        if (! *buf2)
            strcat(buf2, "?");
        sys_putpreference(buf1, buf2);
    }
    sys_putpreference("noaudioout", (as.a_noutdev <= 0 ? "True":"False"));
    for (i = 0; i < as.a_noutdev; i++)
    {
        sprintf(buf1, "audiooutdev%d", i+1);
        sprintf(buf2, "%d %d", as.a_outdevvec[i], as.a_choutdevvec[i]);
        sys_putpreference(buf1, buf2);
        sprintf(buf1, "audiooutdevname%d", i+1);
        sys_audiodevnumbertoname(1, as.a_outdevvec[i], buf2, MAXPDSTRING);
        if (! *buf2)
            strcat(buf2, "?");
        sys_putpreference(buf1, buf2);
   }

    sprintf(buf1, "%d", as.a_advance);
    sys_putpreference("audiobuf", buf1);

    sprintf(buf1, "%d", as.a_srate);
    sys_putpreference("rate", buf1);

    sprintf(buf1, "%d", as.a_callback);
    sys_putpreference("callback", buf1);

    sprintf(buf1, "%d", as.a_blocksize);
    sys_putpreference("audioblocksize", buf1);

        /* MIDI settings */
    sprintf(buf1, "%d", sys_midiapi);
    sys_putpreference("midiapi", buf1);

    sys_get_midi_params(&nmidiindev, midiindev, &nmidioutdev, midioutdev);
    sys_putpreference("nomidiin", (nmidiindev <= 0 ? "True" : "False"));
    for (i = 0; i < nmidiindev; i++)
    {
        sprintf(buf1, "midiindev%d", i+1);
        sprintf(buf2, "%d", midiindev[i]);
        sys_putpreference(buf1, buf2);
        sprintf(buf1, "midiindevname%d", i+1);
        sys_mididevnumbertoname(0, midiindev[i], buf2, MAXPDSTRING);
        if (! *buf2)
            strcat(buf2, "?");
        sys_putpreference(buf1, buf2);
    }
    sys_putpreference("nomidiout", (nmidioutdev <= 0 ? "True" : "False"));
    for (i = 0; i < nmidioutdev; i++)
    {
        sprintf(buf1, "midioutdev%d", i+1);
        sprintf(buf2, "%d", midioutdev[i]);
        sys_putpreference(buf1, buf2);
        sprintf(buf1, "midioutdevname%d", i+1);
        sys_mididevnumbertoname(1, midioutdev[i], buf2, MAXPDSTRING);
        if (! *buf2)
            strcat(buf2, "?");
        sys_putpreference(buf1, buf2);
    }
        /* file search path */

    for (i = 0; 1; i++)
    {
        const char *pathelem = namelist_get(STUFF->st_searchpath, i);
        if (!pathelem)
            break;
        sprintf(buf1, "path%d", i+1);
        sys_putpreference(buf1, pathelem);
    }
    sprintf(buf1, "%d", i);
    sys_putpreference("npath", buf1);
    sprintf(buf1, "%d", sys_usestdpath);
    sys_putpreference("standardpath", buf1);
    sprintf(buf1, "%d", sys_verbose);
    sys_putpreference("verbose", buf1);

        /* startup */
    for (i = 0; 1; i++)
    {
        const char *pathelem = namelist_get(STUFF->st_externlist, i);
        if (!pathelem)
            break;
        sprintf(buf1, "loadlib%d", i+1);
        sys_putpreference(buf1, pathelem);
    }
    sprintf(buf1, "%d", i);
    sys_putpreference("nloadlib", buf1);
    sprintf(buf1, "%d", sys_defeatrt);
    sys_putpreference("defeatrt", buf1);
    sys_putpreference("flags",
        (sys_flags ? sys_flags->s_name : ""));
        /* misc */
    sprintf(buf1, "%d", sys_zoom_open);
    sys_putpreference("zoom", buf1);
    sys_putpreference("loading", "no");

    sys_donesavepreferences();
}

    /* calls from GUI to load/save from/to a file */
void glob_loadpreferences(t_pd *dummy, t_symbol *filesym)
{
    sys_loadpreferences(filesym->s_name, 0);
    sys_close_audio();
    sys_reopen_audio();
    sys_close_midi();
    sys_reopen_midi();
}

void glob_savepreferences(t_pd *dummy, t_symbol *filesym)
{
    sys_savepreferences(filesym->s_name);
}

void glob_forgetpreferences(t_pd *dummy)
{
#if !defined(_WIN32) && !defined(__APPLE__)
    char user_prefs_file[MAXPDSTRING]; /* user prefs file */
    const char *homedir = getenv("HOME");
    struct stat statbuf;
    snprintf(user_prefs_file, MAXPDSTRING, "%s/.pdsettings",
        (homedir ? homedir : "."));
    user_prefs_file[MAXPDSTRING-1] = 0;
    if (stat(user_prefs_file, &statbuf) != 0) {
        post("no Pd settings to clear");
    } else if (!unlink(user_prefs_file)) {
        post("removed %s file", user_prefs_file);
    } else {
        post("couldn't delete %s file: %s", user_prefs_file, strerror(errno));
    }
#endif  /* !defined(_WIN32) && !defined(__APPLE__) */
#ifdef __APPLE__
    char cmdbuf[MAXPDSTRING];
    int warn = 1;
    if (!sys_getpreference("audioapi", cmdbuf, MAXPDSTRING))
        post("no Pd settings to clear"), warn = 0;
            /* do it anyhow, why not... */
    snprintf(cmdbuf, MAXPDSTRING,
        "defaults delete org.puredata.pd 2> /dev/null\n");
    if (system(cmdbuf) && warn)
        post("failed to erase Pd settings");
    else if(warn) post("erased Pd settings");
#endif /* __APPLE__ */
#ifdef _WIN32
    HKEY hkey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER,
        "Software", 0,  KEY_QUERY_VALUE, &hkey) != ERROR_SUCCESS)
            post("no Pd settings to erase");
    else
    {
        if (RegDeleteKey(hkey, "Pure-Data") != ERROR_SUCCESS)
            post("no Pd settings to erase");
        else post("erased Pd settings");
        RegCloseKey(hkey);
    }
#endif /* _WIN32 */
}

int sys_oktoloadfiles(int done)
{
#if defined(_WIN32) || defined(__APPLE__)
    if (done)
    {
        sys_putpreference("loading", "no");
        return (1);
    }
    else
    {
        char prefbuf[MAXPDSTRING];
        if (sys_getpreference("loading", prefbuf, MAXPDSTRING) &&
            strcmp(prefbuf, "no"))
        {
            post(
    "skipping loading preferences... Pd seems to have crashed on startup");
            post("(re-save preferences to reinstate them)");
            return (0);
        }
        else
        {
            sys_putpreference("loading", "yes");
            return (1);
        }
    }
#else
    return (1);
#endif
}
