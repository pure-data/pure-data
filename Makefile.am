AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS =  -I m4/generated -I m4

CFLAGS += @DEFS@
EXTRA_SUBDIRS = 

# pkg-config support
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = pd.pc

if ASIO
EXTRA_SUBDIRS += asio
endif

if PORTAUDIO
EXTRA_SUBDIRS += portaudio
endif

if PORTMIDI
EXTRA_SUBDIRS += portmidi
endif

# files that are included but not built
EXTRA_DIST = LICENSE.txt README.txt INSTALL.txt

# subdirs that are built
SUBDIRS = $(EXTRA_SUBDIRS) src man tcl po extra

# subdirs that are always included in the dist, etc.
DIST_SUBDIRS = asio extra man po portaudio portmidi src tcl

BUILT_SOURCES = 
if MSGFMT
BUILT_SOURCES += locales
endif

## FIXXXME
## $(pkglibdir) is used throughout the other Makefile.amS
##       simply ignoring the special case for other OSs...
if LINUX
libpddir = $(pkglibdir)
else
libpddir = $(prefix)
endif

# Symlinks don't work on Windows/MinGW but they do on Cygwin.
bin:
	$(LN_S) src bin

locales:
	make -C po all

install-data-local:
	$(INSTALL) -d $(DESTDIR)$(libpddir)/startup
	$(INSTALL) -d $(DESTDIR)$(libpddir)/startup/disabled

# this is at the end because its loooong
# these files go into libpddir defined above
nobase_dist_libpd_DATA = \
	doc/1.manual/1.introduction.txt \
	doc/1.manual/fig1.1.png \
	doc/1.manual/fig1.2.jpg \
	doc/1.manual/fig1.3.jpg \
	doc/1.manual/fig1.4.png \
	doc/1.manual/fig1.5.jpg \
	doc/1.manual/fig11.1.png \
	doc/1.manual/fig11.2.png \
	doc/1.manual/fig11.3.png \
	doc/1.manual/fig11.4.png \
	doc/1.manual/fig3.1.jpg \
	doc/1.manual/fig3.10.jpg \
	doc/1.manual/fig3.2.jpg \
	doc/1.manual/fig3.3.jpg \
	doc/1.manual/fig3.4.jpg \
	doc/1.manual/fig3.5.jpg \
	doc/1.manual/fig3.6.jpg \
	doc/1.manual/fig3.7.jpg \
	doc/1.manual/fig3.8.jpg \
	doc/1.manual/fig3.9.jpg \
	doc/1.manual/fig7.1.jpg \
	doc/1.manual/fig7.2.jpg \
	doc/1.manual/fig7.3.jpg \
	doc/1.manual/fig7.4.jpg \
	doc/1.manual/fig7.5.jpg \
	doc/1.manual/fig7.6.jpg \
	doc/1.manual/fig8.1.jpg \
	doc/1.manual/fig8.2.jpg \
	doc/1.manual/fig8.3.jpg \
	doc/1.manual/fig8.4.jpg \
	doc/1.manual/fig8.5.jpg \
	doc/1.manual/fig8.6.jpg \
	doc/1.manual/fig9.1.jpg \
	doc/1.manual/fig9.2.jpg \
	doc/1.manual/fig9.3.jpg \
	doc/1.manual/index.htm \
	doc/1.manual/pdmanual.css \
	doc/1.manual/x1.htm \
	doc/1.manual/x2.htm \
	doc/1.manual/x3.htm \
	doc/1.manual/x4.htm \
	doc/1.manual/x5.htm \
	doc/2.control.examples/00.INTRO.txt \
	doc/2.control.examples/01.PART1.hello.pd \
	doc/2.control.examples/02.editing.pd \
	doc/2.control.examples/03.connections.pd \
	doc/2.control.examples/04.messages.pd \
	doc/2.control.examples/05.counter.pd \
	doc/2.control.examples/06.more.counters.pd \
	doc/2.control.examples/07.time.pd \
	doc/2.control.examples/08.depthfirst.pd \
	doc/2.control.examples/09.send_receive.pd \
	doc/2.control.examples/10.more.messages.pd \
	doc/2.control.examples/11.review.pd \
	doc/2.control.examples/12.PART2.subpatch.pd \
	doc/2.control.examples/13.locality.pd \
	doc/2.control.examples/14.dollarsigns.pd \
	doc/2.control.examples/15.array.pd \
	doc/2.control.examples/15.file.txt \
	doc/2.control.examples/16.more.arrays.pd \
	doc/2.control.examples/17.PART3.midi.pd \
	doc/2.control.examples/18.conditional.pd \
	doc/2.control.examples/19.random.pd \
	doc/2.control.examples/20.weighted-random.pd \
	doc/2.control.examples/21.markov.chain.pd \
	doc/2.control.examples/22.random-walk.pd \
	doc/2.control.examples/23.sequencing.pd \
	doc/2.control.examples/24.loops.pd \
	doc/2.control.examples/dollarsign.pd \
	doc/2.control.examples/dollarsign2.pd \
	doc/2.control.examples/sendnumber.pd \
	doc/3.audio.examples/A00.intro.pd \
	doc/3.audio.examples/A00.intro.txt \
	doc/3.audio.examples/A01.sinewave.pd \
	doc/3.audio.examples/A02.amplitude.pd \
	doc/3.audio.examples/A03.line.pd \
	doc/3.audio.examples/A04.line2.pd \
	doc/3.audio.examples/A05.output.subpatch.pd \
	doc/3.audio.examples/A06.frequency.pd \
	doc/3.audio.examples/A07.fusion.pd \
	doc/3.audio.examples/A08.beating.pd \
	doc/3.audio.examples/A09.frequency.mod.pd \
	doc/3.audio.examples/A10.review.pd \
	doc/3.audio.examples/adsr.pd \
	doc/3.audio.examples/B01.wavetables.pd \
	doc/3.audio.examples/B02.two-wavetables.pd \
	doc/3.audio.examples/B03.tabread4.pd \
	doc/3.audio.examples/B04.tabread4.interpolation.pd \
	doc/3.audio.examples/B05.tabread.FM.pd \
	doc/3.audio.examples/B06.table.switching.pd \
	doc/3.audio.examples/B07.sampler.pd \
	doc/3.audio.examples/B08.sampler.loop.pd \
	doc/3.audio.examples/B09.sampler.loop.smooth.pd \
	doc/3.audio.examples/B10.sampler.scratch.pd \
	doc/3.audio.examples/B11.sampler.nodoppler.pd \
	doc/3.audio.examples/B12.sampler.transpose.pd \
	doc/3.audio.examples/B13.sampler.overlap.pd \
	doc/3.audio.examples/B14.sampler.rockafella.pd \
	doc/3.audio.examples/B15.tabread4~-onset.pd \
	doc/3.audio.examples/B16.long-varispeed.pd \
	doc/3.audio.examples/buttercoef3.pd \
	doc/3.audio.examples/butterworth3~.pd \
	doc/3.audio.examples/C01.nyquist.pd \
	doc/3.audio.examples/C02.sawtooth-foldover.pd \
	doc/3.audio.examples/C03.zipper.noise.pd \
	doc/3.audio.examples/C04.control.to.signal.pd \
	doc/3.audio.examples/C05.sampler.oneshot.pd \
	doc/3.audio.examples/C06.signal.to.control.pd \
	doc/3.audio.examples/C07.envelope.follower.pd \
	doc/3.audio.examples/C08.analog.sequencer.pd \
	doc/3.audio.examples/C09.sample.hold.pd \
	doc/3.audio.examples/C10.monophonic.synth.pd \
	doc/3.audio.examples/D01.envelope.gen.pd \
	doc/3.audio.examples/D02.adsr.pd \
	doc/3.audio.examples/D03.envelope.dB.pd \
	doc/3.audio.examples/D04.envelope.quartic.pd \
	doc/3.audio.examples/D05.envelope.pitch.pd \
	doc/3.audio.examples/D06.envelope.portamento.pd \
	doc/3.audio.examples/D07.additive.pd \
	doc/3.audio.examples/D08.table.spectrum.pd \
	doc/3.audio.examples/D09.shepard.tone.pd \
	doc/3.audio.examples/D10.sampler.notes.pd \
	doc/3.audio.examples/D11.sampler.poly.pd \
	doc/3.audio.examples/D12.sampler.bis.pd \
	doc/3.audio.examples/D13.additive.qlist.pd \
	doc/3.audio.examples/D14.vibrato.pd \
	doc/3.audio.examples/E01.spectrum.pd \
	doc/3.audio.examples/E02.ring.modulation.pd \
	doc/3.audio.examples/E03.octave.divider.pd \
	doc/3.audio.examples/E04.difference.tone.pd \
	doc/3.audio.examples/E05.chebychev.pd \
	doc/3.audio.examples/E06.exponential.pd \
	doc/3.audio.examples/E07.evenodd.pd \
	doc/3.audio.examples/E08.phase.mod.pd \
	doc/3.audio.examples/E09.FM.spectrum.pd \
	doc/3.audio.examples/E10.complex.FM.pd \
	doc/3.audio.examples/F01.pulse.pd \
	doc/3.audio.examples/F02.just.say.pd \
	doc/3.audio.examples/F03.pulse.spectrum.pd \
	doc/3.audio.examples/F04.waveshaping.pulse.pd \
	doc/3.audio.examples/F05.ring.modulation.pd \
	doc/3.audio.examples/F06.packets.pd \
	doc/3.audio.examples/F07.packet.spectrum.pd \
	doc/3.audio.examples/F08.two.cosines.pd \
	doc/3.audio.examples/F09.declickit.pd \
	doc/3.audio.examples/F10.sweepable.FM.pd \
	doc/3.audio.examples/F11.anharmonic.FM.pd \
	doc/3.audio.examples/F12.paf.pd \
	doc/3.audio.examples/F13.paf.control.pd \
	doc/3.audio.examples/F14.wave.packet.pd \
	doc/3.audio.examples/filter-graph1.pd \
	doc/3.audio.examples/filter-graph2.pd \
	doc/3.audio.examples/G01.delay.pd \
	doc/3.audio.examples/G02.delay.loop.pd \
	doc/3.audio.examples/G03.delay.variable.pd \
	doc/3.audio.examples/G04.control.blocksize.pd \
	doc/3.audio.examples/G05.execution.order.pd \
	doc/3.audio.examples/G06.octave.doubler.pd \
	doc/3.audio.examples/G07.shaker.pd \
	doc/3.audio.examples/G08.reverb.pd \
	doc/3.audio.examples/G09.pitchshift.pd \
	doc/3.audio.examples/H01.low-pass.pd \
	doc/3.audio.examples/H02.high-pass.pd \
	doc/3.audio.examples/H03.band-pass.pd \
	doc/3.audio.examples/H04.filter.sweep.pd \
	doc/3.audio.examples/H05.filter.floyd.pd \
	doc/3.audio.examples/H06.envelope.follower.pd \
	doc/3.audio.examples/H07.measure.spectrum.pd \
	doc/3.audio.examples/H08.heterodyning.pd \
	doc/3.audio.examples/H09.ssb.modulation.pd \
	doc/3.audio.examples/H10.measurement.pd \
	doc/3.audio.examples/H11.shelving.pd \
	doc/3.audio.examples/H12.peaking.pd \
	doc/3.audio.examples/H13.butterworth.pd \
	doc/3.audio.examples/H14.all.pass.pd \
	doc/3.audio.examples/H15.phaser.pd \
	doc/3.audio.examples/H16.adsr.filter.qlist.pd \
	doc/3.audio.examples/I01.Fourier.analysis.pd \
	doc/3.audio.examples/I02.Hann.window.pd \
	doc/3.audio.examples/I03.resynthesis.pd \
	doc/3.audio.examples/I04.noisegate.pd \
	doc/3.audio.examples/I05.compressor.pd \
	doc/3.audio.examples/I06.timbre.stamp.pd \
	doc/3.audio.examples/I07.phase.vocoder.pd \
	doc/3.audio.examples/I08.pvoc.reverb.pd \
	doc/3.audio.examples/I09.sheep.from.goats.pd \
	doc/3.audio.examples/I10.phase.bash.pd \
	doc/3.audio.examples/J01.even.odd.pd \
	doc/3.audio.examples/J02.trapezoids.pd \
	doc/3.audio.examples/J03.pulse.width.mod.pd \
	doc/3.audio.examples/J04.corners.pd \
	doc/3.audio.examples/J05.triangle.pd \
	doc/3.audio.examples/J06.enveloping.pd \
	doc/3.audio.examples/J07.oversampling.pd \
	doc/3.audio.examples/J08.classicsynth.pd \
	doc/3.audio.examples/J09.bandlimited.pd \
	doc/3.audio.examples/osc-voice.pd \
	doc/3.audio.examples/output~.pd \
	doc/3.audio.examples/partial.pd \
	doc/3.audio.examples/qlist-sampler.txt \
	doc/3.audio.examples/qlist.txt \
	doc/3.audio.examples/qlist2.txt \
	doc/3.audio.examples/reverb-echo.pd \
	doc/3.audio.examples/sampvoice.pd \
	doc/3.audio.examples/sampvoice2.pd \
	doc/3.audio.examples/shepvoice.pd \
	doc/3.audio.examples/sinevoice.pd \
	doc/3.audio.examples/spectrum-partial.pd \
	doc/4.data.structures/00.intro.txt \
	doc/4.data.structures/01.scalars.pd \
	doc/4.data.structures/02.getting.data.pd \
	doc/4.data.structures/03.setting.data.pd \
	doc/4.data.structures/04.append.pd \
	doc/4.data.structures/05.array.pd \
	doc/4.data.structures/06.file.pd \
	doc/4.data.structures/07.sequencer.pd \
	doc/4.data.structures/08.selection.pd \
	doc/4.data.structures/09.scaling.pd \
	doc/4.data.structures/10.onoff.pd \
	doc/4.data.structures/11.array.controls.pd \
	doc/4.data.structures/12.beat-patterns.pd \
	doc/4.data.structures/13.sliderule.pd \
	doc/4.data.structures/14.partialtracer.pd \
	doc/4.data.structures/add-trace.pd \
	doc/4.data.structures/beat-maker.pd \
	doc/4.data.structures/data-array.pd \
	doc/4.data.structures/data-start.pd \
	doc/4.data.structures/file.txt \
	doc/4.data.structures/osc-voice.pd \
	doc/4.data.structures/output~.pd \
	doc/4.data.structures/voice.pd \
	doc/4.data.structures/z.txt \
	doc/5.reference/0_all_guis-INTRO.txt \
	doc/5.reference/abs~-help.pd \
	doc/5.reference/acoustics-help.pd \
	doc/5.reference/acoustics~-help.pd \
	doc/5.reference/adc~_dac~-help.pd \
	doc/5.reference/append-help.pd \
	doc/5.reference/bag-help.pd \
	doc/5.reference/bang-help.pd \
	doc/5.reference/bang~-help.pd \
	doc/5.reference/biquad~-help.pd \
	doc/5.reference/block~-help.pd \
	doc/5.reference/bng-help.pd \
	doc/5.reference/bp~-help.pd \
	doc/5.reference/canvas-help.pd \
	doc/5.reference/change-help.pd \
	doc/5.reference/clip~-help.pd \
	doc/5.reference/cos~-help.pd \
	doc/5.reference/cpole~-help.pd \
	doc/5.reference/cputime-help.pd \
	doc/5.reference/czero_rev~-help.pd \
	doc/5.reference/czero~-help.pd \
	doc/5.reference/declare-help.pd \
	doc/5.reference/delay-help.pd \
	doc/5.reference/delread~-help.pd \
	doc/5.reference/delwrite~-help.pd \
	doc/5.reference/drawnumber-help.pd \
	doc/5.reference/drawpolygon-help.pd \
	doc/5.reference/element-help.pd \
	doc/5.reference/env~-help.pd \
	doc/5.reference/exp~-help.pd \
	doc/5.reference/fft~-help.pd \
	doc/5.reference/float-help.pd \
	doc/5.reference/framp~-help.pd \
	doc/5.reference/gatom-help.pd \
	doc/5.reference/get-help.pd \
	doc/5.reference/getsize-help.pd \
	doc/5.reference/graph-help.pd \
	doc/5.reference/hdial-help.pd \
	doc/5.reference/help-intro.pd \
	doc/5.reference/hip~-help.pd \
	doc/5.reference/hslider-help.pd \
	doc/5.reference/int-help.pd \
	doc/5.reference/key-help.pd \
	doc/5.reference/line-help.pd \
	doc/5.reference/line~-help.pd \
	doc/5.reference/list-help.pd \
	doc/5.reference/log~-help.pd \
	doc/5.reference/lop~-help.pd \
	doc/5.reference/makefilename-help.pd \
	doc/5.reference/makenote-help.pd \
	doc/5.reference/math-help.pd \
	doc/5.reference/message-help.pd \
	doc/5.reference/metro.pd \
	doc/5.reference/midi-help.pd \
	doc/5.reference/moses-help.pd \
	doc/5.reference/my_canvas-help.pd \
	doc/5.reference/namecanvas-help.pd \
	doc/5.reference/netreceive-help.pd \
	doc/5.reference/netsend-help.pd \
	doc/5.reference/noise~-help.pd \
	doc/5.reference/numbox2-help.pd \
	doc/5.reference/openpanel-help.pd \
	doc/5.reference/operators-help.pd \
	doc/5.reference/osc~-help.pd \
	doc/5.reference/otherbinops-help.pd \
	doc/5.reference/pack-help.pd \
	doc/5.reference/pd-help.pd \
	doc/5.reference/phasor~-help.pd \
	doc/5.reference/pipe-help.pd \
	doc/5.reference/plot-help.pd \
	doc/5.reference/pointer-help.pd \
	doc/5.reference/poly-help.pd \
	doc/5.reference/pow~-help.pd \
	doc/5.reference/print-help.pd \
	doc/5.reference/print~-help.pd \
	doc/5.reference/qlist-help.pd \
	doc/5.reference/qlist.txt \
	doc/5.reference/random-help.pd \
	doc/5.reference/readsf~-help.pd \
	doc/5.reference/realtime-help.pd \
	doc/5.reference/receive-help.pd \
	doc/5.reference/route-help.pd \
	doc/5.reference/rpole~-help.pd \
	doc/5.reference/rsqrt~-help.pd \
	doc/5.reference/rzero_rev~-help.pd \
	doc/5.reference/rzero~-help.pd \
	doc/5.reference/samphold~-help.pd \
	doc/5.reference/savepanel-help.pd \
	doc/5.reference/select-help.pd \
	doc/5.reference/send-help.pd \
	doc/5.reference/send~-help.pd \
	doc/5.reference/set-help.pd \
	doc/5.reference/setsize-help.pd \
	doc/5.reference/setsize.txt \
	doc/5.reference/sigbinops-help.pd \
	doc/5.reference/sig~-help.pd \
	doc/5.reference/snapshot~-help.pd \
	doc/5.reference/soundfiler-help.pd \
	doc/5.reference/spigot-help.pd \
	doc/5.reference/sqrt~-help.pd \
	doc/5.reference/stripnote-help.pd \
	doc/5.reference/struct-help.pd \
	doc/5.reference/sublist-help.pd \
	doc/5.reference/swap-help.pd \
	doc/5.reference/switch~-help.pd \
	doc/5.reference/table.txt \
	doc/5.reference/tabosc4~-help.pd \
	doc/5.reference/tabplay~-help.pd \
	doc/5.reference/tabread-help.pd \
	doc/5.reference/tabread4~-help.pd \
	doc/5.reference/tabreceive~-help.pd \
	doc/5.reference/tabsend~-help.pd \
	doc/5.reference/tabwrite-help.pd \
	doc/5.reference/tabwrite~-help.pd \
	doc/5.reference/text-help.pd \
	doc/5.reference/textfile-help.pd \
	doc/5.reference/textfile.txt \
	doc/5.reference/threshold~-help.pd \
	doc/5.reference/throw~-help.pd \
	doc/5.reference/timer-help.pd \
	doc/5.reference/toggle-help.pd \
	doc/5.reference/trigger-help.pd \
	doc/5.reference/unpack-help.pd \
	doc/5.reference/until-help.pd \
	doc/5.reference/value-help.pd \
	doc/5.reference/vcf~-help.pd \
	doc/5.reference/vdial-help.pd \
	doc/5.reference/vd~-help.pd \
	doc/5.reference/vline~-help.pd \
	doc/5.reference/vslider-help.pd \
	doc/5.reference/vu-help.pd \
	doc/5.reference/wrap~-help.pd \
	doc/5.reference/writesf~-help.pd \
	doc/5.reference/x_all_guis.pd \
	doc/6.externs/0.README.txt \
	doc/6.externs/dspobj~.c \
	doc/6.externs/makefile \
	doc/6.externs/obj1.c \
	doc/6.externs/obj2.c \
	doc/6.externs/obj3.c \
	doc/6.externs/obj4.c \
	doc/6.externs/obj5.c \
	doc/6.externs/test-dspobj~.pd \
	doc/6.externs/test-obj1.pd \
	doc/6.externs/test-obj2.pd \
	doc/6.externs/test-obj3.pd \
	doc/6.externs/test-obj4.pd \
	doc/6.externs/test-obj5.pd \
	doc/7.stuff/soundfile-tools/1.ring-mod.pd \
	doc/7.stuff/soundfile-tools/2.bandpass.pd \
	doc/7.stuff/soundfile-tools/3.phase.vocoder.pd \
	doc/7.stuff/soundfile-tools/4.looper.pd \
	doc/7.stuff/soundfile-tools/5.reverb.pd \
	doc/7.stuff/soundfile-tools/6.vocoder.pd \
	doc/7.stuff/soundfile-tools/README.txt \
	doc/7.stuff/synth/1.poly.synth.pd \
	doc/7.stuff/synth/gadsr.pd \
	doc/7.stuff/synth/numset.pd \
	doc/7.stuff/synth/preset.pd \
	doc/7.stuff/synth/preset1.txt \
	doc/7.stuff/synth/preset2.txt \
	doc/7.stuff/synth/preset3.txt \
	doc/7.stuff/synth/preset4.txt \
	doc/7.stuff/synth/README.txt \
	doc/7.stuff/synth/synthvoice.pd \
	doc/7.stuff/synth/test-gadsr.pd \
	doc/7.stuff/tools/latency.pd \
	doc/7.stuff/tools/load-meter.pd \
	doc/7.stuff/tools/testtone.pd \
	doc/7.stuff/tools/testtone16.pd \
	doc/sound/bell.aiff \
	doc/sound/voice.wav \
	doc/sound/voice2.wav
