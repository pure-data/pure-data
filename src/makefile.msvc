# Makefile for PD using Microsoft Visual C/C++ (MSVC) -- note defines
# which will have to be adapted to whichever specific version of MSVC you
# have lying around.

MSCC = cl
MSLN = link
COPY = copy
DELETE = del

VCSDK = "C:/Program Files/Microsoft SDKs/Windows/v6.0A"
VC9 = "C:/Program Files/Microsoft Visual Studio 9.0/VC"

WISH = wish85.exe

EXTRA_LIBPATH=
EXTRA_INCLUDES=
## the following adds the search paths for compiler/linker
## if the build system hasn't been properly setup by calling
## the 'vcvarsall.bat' (MSVC) resp. 'SetEnv.bat' (MSSDK) scripts.
## the logic checks whether some variables are set/unset

## the code is a bit convoluted because we want it to work
## with both 'GNU make' and 'nmake'
## see https://stackoverflow.com/a/30906085/1169096

# \
!ifndef 0 # \
# nmake code here \
!IFNDEF VCINSTALLDIR # \
VCINSTALLDIR = $(VC9) # \
EXTRA_INCLUDES = /I$(VC9)/Include $(EXTRA_INCLUDES)  # \
EXTRA_LIBPATH = /LIBPATH:$(VC9)/lib $(EXTRA_LIBPATH) # \
!ENDIF # \
!IFNDEF WINDOWSSDKDIR # \
WINDOWSSDKDIR = $(VCSDK) # \
EXTRA_INCLUDES = /I$(VCSDK)/Include $(EXTRA_INCLUDES) # \
EXTRA_LIBPATH = /LIBPATH:$(VCSDK)/lib $(EXTRA_LIBPATH)# \
!ENDIF # \
!else
# GNU make code here
ifndef VCINSTALLDIR
VCINSTALLDIR = $(VC9)
EXTRA_INCLUDES += /I$(VC9)/Include
EXTRA_LIBPATH += /LIBPATH:$(VC9)/lib
endif
ifndef WINDOWSSDKDIR
WINDOWSSDKDIR = $(VCSDK)
EXTRA_INCLUDES += /I$(VCSDK)/Include
EXTRA_LIBPATH += /LIBPATH:$(VCSDK)/lib
endif
# \
!endif

PDINCLUDE = /I./ $(EXTRA_INCLUDES)

PDLIB = /NODEFAULTLIB:libcmt /NODEFAULTLIB:libcpmt /NODEFAULTLIB:oldnames \
    /NODEFAULTLIB:libc /NODEFAULTLIB:uuid /NODEFAULTLIB:ole32 \
    $(EXTRA_LIBPATH) \
    kernel32.lib \
    wsock32.lib winmm.lib \
    advapi32.lib setupapi.lib \
    ../bin/pthreadVC.lib \
    libcmt.lib oldnames.lib

AFLAGS = /D__i386__
# MSW, NT: legacy defines of Pd
# WIN32: portmidi, portaudio
# WINDOWS: portaudio
# _WINDOWS: unused!
CFLAGS = /nologo \
	/W3 /Ox $(AFLAGS) \
	/DMSW /DNT /DWIN32 /DWINDOWS /D_WINDOWS \
	/DWISH=\"$(WISH)\" \
	/DPD /DPD_INTERNAL \
	/DUSEAPI_MMIO /DUSEAPI_PORTAUDIO \
	/DPA_LITTLE_ENDIAN /DPA19 \
	/D_CRT_SECURE_NO_WARNINGS
LFLAGS = /nologo

SYSSRC = s_audio_pa.c s_audio_paring.c \
    s_audio_mmio.c s_midi_pm.c

SRC = g_canvas.c g_graph.c g_text.c g_rtext.c g_array.c g_template.c g_io.c \
    g_scalar.c g_traversal.c g_guiconnect.c g_readwrite.c g_editor.c g_clone.c \
    g_all_guis.c g_bang.c g_hdial.c g_hslider.c g_mycanvas.c g_numbox.c \
    g_toggle.c g_undo.c g_vdial.c g_vslider.c g_vumeter.c \
    g_editor_extras.c \
    m_pd.c m_class.c m_obj.c m_atom.c m_memory.c m_binbuf.c \
    m_conf.c m_glob.c m_sched.c \
    s_main.c s_inter.c s_file.c s_print.c \
    s_loader.c s_path.c s_entry.c s_audio.c s_midi.c s_utf8.c \
    d_ugen.c d_ctl.c d_arithmetic.c d_osc.c d_filter.c d_dac.c d_misc.c \
    d_math.c d_fft.c d_fft_fftsg.c d_array.c d_global.c \
    d_delay.c d_resample.c d_soundfile.c \
    x_arithmetic.c x_connective.c x_interface.c x_midi.c x_misc.c \
    x_time.c x_acoustics.c x_net.c x_text.c x_gui.c x_list.c x_array.c \
    x_scalar.c  x_vexp.c x_vexp_if.c x_vexp_fun.c \
    $(SYSSRC)

PADIR = ../portaudio/portaudio
INCPA = -I$(PADIR)/include -I$(PADIR)/src/common -I$(PADIR)/src/os/win
PASRC = $(PADIR)/src

PAOBJ = pa_stream.obj pa_trace.obj pa_process.obj \
	pa_front.obj pa_dither.obj pa_cpuload.obj pa_converters.obj \
	pa_allocation.obj pa_ringbuffer.obj \
	pa_win_hostapis.obj pa_win_util.obj pa_win_waveformat.obj \
	pa_win_wmme.obj
#       pa_win_wdmks.obj

PMDIR = ../portmidi/portmidi
INCPM = -I$(PMDIR)/pm_common -I$(PMDIR)/pm_win -I$(PMDIR)/porttime -DNEWBUFFER
SRCPM = $(PADIR)/pm_common/portmidi.c \
	$(PMDIR)/pm_common/pmutil.c \
	$(PMDIR)/porttime/porttime.c \
	$(PMDIR)/porttime/ptwinmm.c \
	$(PMDIR)/pm_win/pmwin.c \
	$(PMDIR)/pm_win/pmwinmm.c

PMOBJ = portmidi.obj pmutil.obj porttime.obj ptwinmm.obj pmwin.obj pmwinmm.obj

OBJC = $(SRC:.c=.obj) $(PAOBJ) $(PMOBJ)

GSRC =  t_main.c t_tkcmd.c

GOBJ = $(GSRC:.c=.obj)

ALLCF = $(CFLAGS) $(PDINCLUDE) $(INCPA) $(INCPM)

.SUFFIXES: .obj
.c.obj:
	$(MSCC) /c $(ALLCF) /Tc$*.c

all: pd ../bin/pdsend.exe ../bin/pdreceive.exe

.PHONY: pd

pd: ../bin/pd.exe ../bin/pd.com

../bin/pd.exe: s_entry.obj ../bin/pd.lib
	$(MSLN) $(LFLAGS) /OUT:../bin/pd.exe /INCREMENTAL:NO s_entry.obj \
	    ../bin/pd.lib $(PDLIB)

../bin/pd.dll ../bin/pd.lib: $(OBJC)
	$(MSLN) /DLL /OUT:../bin/pd.dll /EXPORT:sys_main $(LFLAGS) $(OBJC) \
	    $(PDLIB)

../bin/pdsend.exe: u_pdsend.obj
	$(MSLN) $(LFLAGS) /out:../bin/pdsend.exe /INCREMENTAL:NO \
		u_pdsend.obj  $(PDLIB)

../bin/pdreceive.exe: u_pdreceive.obj
	$(MSLN) $(LFLAGS) /out:../bin/pdreceive.exe /INCREMENTAL:NO \
	    u_pdreceive.obj $(PDLIB)

../bin/pd.com: s_entry_com.obj ../bin/pd.lib
	$(MSLN) $(LFLAGS) /out:../bin/pd.com /INCREMENTAL:NO s_entry_com.obj \
	    ../bin/pd.lib $(PDLIB)

s_entry_com.obj: s_entry.c
	$(COPY) s_entry.c s_entry_com.c
	$(MSCC) /c -DCOMMANDVERSION s_entry_com.c
	$(DELETE) s_entry_com.c

PAAPI = -DPA_USE_WMME
PACF= $(PAAPI) $(ALLCF)
# explicit rules to compile portaudio sources:
pa_stream.obj: $(PASRC)/common/pa_stream.c
	$(MSCC) /c $(PACF) $(PASRC)/common/pa_stream.c
pa_trace.obj: $(PASRC)/common/pa_trace.c
	$(MSCC) /c $(PACF) $(PASRC)/common/pa_trace.c
pa_process.obj: $(PASRC)/common/pa_process.c
	$(MSCC) /c $(PACF) $(PASRC)/common/pa_process.c
pa_front.obj: $(PASRC)/common/pa_front.c
	$(MSCC) /c $(PACF) $(PASRC)/common/pa_front.c
pa_dither.obj: $(PASRC)/common/pa_dither.c
	$(MSCC) /c $(PACF) $(PASRC)/common/pa_dither.c
pa_cpuload.obj: $(PASRC)/common/pa_cpuload.c
	$(MSCC) /c $(PACF) $(PASRC)/common/pa_cpuload.c
pa_converters.obj: $(PASRC)/common/pa_converters.c
	$(MSCC) /c $(PACF) $(PASRC)/common/pa_converters.c
pa_allocation.obj: $(PASRC)/common/pa_allocation.c
	$(MSCC) /c $(PACF) $(PASRC)/common/pa_allocation.c
pa_ringbuffer.obj: $(PASRC)/common/pa_ringbuffer.c
	$(MSCC) /c $(PACF) $(PASRC)/common/pa_ringbuffer.c

pa_win_hostapis.obj: $(PASRC)/os/win/pa_win_hostapis.c
	$(MSCC) /c $(PACF) $(PASRC)/os/win/pa_win_hostapis.c
pa_win_util.obj: $(PASRC)/os/win/pa_win_util.c
	$(MSCC) /c $(PACF) $(PASRC)/os/win/pa_win_util.c
pa_win_waveformat.obj: $(PASRC)/os/win/pa_win_waveformat.c
	$(MSCC) /c $(PACF) $(PASRC)/os/win/pa_win_waveformat.c
pa_win_coinitialize.obj: $(PASRC)/os/win/pa_win_coinitialize.c
	$(MSCC) /c $(PACF) $(PASRC)/os/win/pa_win_coinitialize.c
pa_win_wmme.obj: $(PASRC)/hostapi/wmme/pa_win_wmme.c
	$(MSCC) /c $(PACF) $(PASRC)/hostapi/wmme/pa_win_wmme.c
pa_win_wdmks.obj: $(PADIR)/a_win_wdmks/pa_win_wdmks.c
	$(MSCC) /c $(PACF) \
	-DWINVER=0x400 -DKSAUDIO_SPEAKER_DIRECTOUT \
	$(PADIR)/pa_win_wdmks/pa_win_wdmks.c
pa_asio.obj: $(PASRC)/hostapi/asio/pa_asio.cpp
	$(MSCC) /c $(PACF) $(PASRC)/hostapi/asio/pa_asio.cpp

portmidi.obj: $(PMDIR)/pm_common/portmidi.c
	$(MSCC) /c $(PACF) $(PMDIR)/pm_common/portmidi.c
pmutil.obj: $(PMDIR)/pm_common/pmutil.c
	$(MSCC) /c $(PACF) $(PMDIR)/pm_common/pmutil.c
pmwin.obj: $(PMDIR)/pm_win/pmwin.c
	$(MSCC) /c $(PACF) $(PMDIR)/pm_win/pmwin.c
pmwinmm.obj: $(PMDIR)/pm_win/pmwinmm.c
	$(MSCC) /c $(PACF) $(PMDIR)/pm_win/pmwinmm.c
porttime.obj: $(PMDIR)/porttime/porttime.c
	$(MSCC) /c $(PACF) $(PMDIR)/porttime/porttime.c
ptwinmm.obj: $(PMDIR)/porttime/ptwinmm.c
	$(MSCC) /c $(PACF) $(PMDIR)/porttime/ptwinmm.c

# the following should also clean up "bin" but it doesn't because "bin" holds
# precious stuff from elsewhere (luckily, our stuff matches 'pd*')
clean:
	$(DELETE) *.obj
	$(DELETE) ../bin/pd*.*
