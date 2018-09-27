#########################################
##### Defaults & Paths #####

AUTOMAKE_OPTIONS = foreign

# default doc location is $(prefix)/doc/pd,
# but we want $(prefix)/lib/pd/doc
libpddir = $(pkglibdir)/doc

# about text file, needed to write version string into
ABOUT_FILE=1.manual/1.introduction.txt

# location to pd header
M_PD=$(top_srcdir)/src/m_pd.h

# get version info from m_pd.h to use in doc/1.manual/1.introduction.txt
PD_MAJOR_VERSION = $(shell grep PD_MAJOR_VERSION $(M_PD) | \
    sed 's|^.define *PD_MAJOR_VERSION *\([0-9]*\).*|\1|' )
PD_MINOR_VERSION = $(shell grep PD_MINOR_VERSION $(M_PD) | \
    sed 's|^.define *PD_MINOR_VERSION *\([0-9]*\).*|\1|' )
PD_BUGFIX_VERSION = $(shell grep PD_BUGFIX_VERSION $(M_PD) | \
    sed 's|^.define *PD_BUGFIX_VERSION *\([0-9]*\).*|\1|' )
PD_TEST_VERSION = $(shell grep PD_TEST_VERSION $(M_PD) | \
    sed 's|^.define *PD_TEST_VERSION *"\(.*\)".*|\1|' )
PD_VERSION = $(PD_MAJOR_VERSION).$(PD_MINOR_VERSION).$(PD_BUGFIX_VERSION)

#########################################
##### Targets #####

# build version string & write into about file during install
install-data-hook:
	mv $(DESTDIR)$(libpddir)/$(ABOUT_FILE) $(DESTDIR)$(libpddir)/$(ABOUT_FILE).tmp
	if test "x${PD_TEST_VERSION}" == "x" ; then \
	    cat $(DESTDIR)$(libpddir)/$(ABOUT_FILE).tmp | \
	    sed 's|PD_VERSION|Pd version $(PD_VERSION)|' \
	    > $(DESTDIR)$(libpddir)/$(ABOUT_FILE) ; \
	else \
	    cat $(DESTDIR)$(libpddir)/$(ABOUT_FILE).tmp | \
	    sed 's|PD_VERSION|Pd version $(PD_VERSION)-$(PD_TEST_VERSION)|' \
	    > $(DESTDIR)$(libpddir)/$(ABOUT_FILE) ; \
	fi
	rm $(DESTDIR)$(libpddir)/$(ABOUT_FILE).tmp

#########################################
##### Files, Binaries, & Libs #####

# empty var for data list footer
empty=

# this is at the end because its loooong
# these files go into libpddir defined above
# to regenerate:
# cd doc
# find . -type f -not -path "./Makefile*" | sort | awk '{print "    ", $1, "\\"}'; echo '     $(empty)'
nobase_dist_libpd_DATA = \
     ./1.manual/1.introduction.txt \
     ./1.manual/fig1.1.png \
     ./1.manual/fig1.2.jpg \
     ./1.manual/fig1.3.jpg \
     ./1.manual/fig1.4.png \
     ./1.manual/fig1.5.jpg \
     ./1.manual/fig11.1.png \
     ./1.manual/fig11.2.png \
     ./1.manual/fig11.3.png \
     ./1.manual/fig11.4.png \
     ./1.manual/fig3.1.jpg \
     ./1.manual/fig3.10.jpg \
     ./1.manual/fig3.2.jpg \
     ./1.manual/fig3.3.jpg \
     ./1.manual/fig3.4.jpg \
     ./1.manual/fig3.5.jpg \
     ./1.manual/fig3.6.jpg \
     ./1.manual/fig3.7.jpg \
     ./1.manual/fig3.8.jpg \
     ./1.manual/fig3.9.jpg \
     ./1.manual/fig7.1.jpg \
     ./1.manual/fig7.2.jpg \
     ./1.manual/fig7.3.jpg \
     ./1.manual/fig7.4.jpg \
     ./1.manual/fig7.5.jpg \
     ./1.manual/fig7.6.jpg \
     ./1.manual/fig8.1.jpg \
     ./1.manual/fig8.2.jpg \
     ./1.manual/fig8.3.jpg \
     ./1.manual/fig8.4.jpg \
     ./1.manual/fig8.5.jpg \
     ./1.manual/fig8.6.jpg \
     ./1.manual/fig9.1.jpg \
     ./1.manual/fig9.2.jpg \
     ./1.manual/fig9.3.jpg \
     ./1.manual/index.htm \
     ./1.manual/pdmanual.css \
     ./1.manual/x1.htm \
     ./1.manual/x2.htm \
     ./1.manual/x3.htm \
     ./1.manual/x4.htm \
     ./1.manual/x5.htm \
     ./2.control.examples/00.INTRO.txt \
     ./2.control.examples/01.PART1.hello.pd \
     ./2.control.examples/02.editing.pd \
     ./2.control.examples/03.connections.pd \
     ./2.control.examples/04.messages.pd \
     ./2.control.examples/05.counter.pd \
     ./2.control.examples/06.more.counters.pd \
     ./2.control.examples/07.time.pd \
     ./2.control.examples/08.depthfirst.pd \
     ./2.control.examples/09.send_receive.pd \
     ./2.control.examples/10.more.messages.pd \
     ./2.control.examples/11.review.pd \
     ./2.control.examples/12.PART2.subpatch.pd \
     ./2.control.examples/13.locality.pd \
     ./2.control.examples/14.dollarsigns.pd \
     ./2.control.examples/15.array.pd \
     ./2.control.examples/15.file.txt \
     ./2.control.examples/16.more.arrays.pd \
     ./2.control.examples/17.PART3.midi.pd \
     ./2.control.examples/18.conditional.pd \
     ./2.control.examples/19.random.pd \
     ./2.control.examples/20.weighted-random.pd \
     ./2.control.examples/21.markov.chain.pd \
     ./2.control.examples/22.random-walk.pd \
     ./2.control.examples/23.sequencing.pd \
     ./2.control.examples/24.loops.pd \
     ./2.control.examples/dollarsign.pd \
     ./2.control.examples/dollarsign2.pd \
     ./2.control.examples/sendnumber.pd \
     ./3.audio.examples/A00.intro.pd \
     ./3.audio.examples/A00.intro.txt \
     ./3.audio.examples/A01.sinewave.pd \
     ./3.audio.examples/A02.amplitude.pd \
     ./3.audio.examples/A03.line.pd \
     ./3.audio.examples/A04.line2.pd \
     ./3.audio.examples/A05.output.subpatch.pd \
     ./3.audio.examples/A06.frequency.pd \
     ./3.audio.examples/A07.fusion.pd \
     ./3.audio.examples/A08.beating.pd \
     ./3.audio.examples/A09.frequency.mod.pd \
     ./3.audio.examples/A10.review.pd \
     ./3.audio.examples/B01.wavetables.pd \
     ./3.audio.examples/B02.two-wavetables.pd \
     ./3.audio.examples/B03.tabread4.pd \
     ./3.audio.examples/B04.tabread4.interpolation.pd \
     ./3.audio.examples/B05.tabread.FM.pd \
     ./3.audio.examples/B06.table.switching.pd \
     ./3.audio.examples/B07.sampler.pd \
     ./3.audio.examples/B08.sampler.loop.pd \
     ./3.audio.examples/B09.sampler.loop.smooth.pd \
     ./3.audio.examples/B10.sampler.scratch.pd \
     ./3.audio.examples/B11.sampler.nodoppler.pd \
     ./3.audio.examples/B12.sampler.transpose.pd \
     ./3.audio.examples/B13.sampler.overlap.pd \
     ./3.audio.examples/B14.sampler.rockafella.pd \
     ./3.audio.examples/B15.tabread4~-onset.pd \
     ./3.audio.examples/B16.long-varispeed.pd \
     ./3.audio.examples/C01.nyquist.pd \
     ./3.audio.examples/C02.sawtooth-foldover.pd \
     ./3.audio.examples/C03.zipper.noise.pd \
     ./3.audio.examples/C04.control.to.signal.pd \
     ./3.audio.examples/C05.sampler.oneshot.pd \
     ./3.audio.examples/C06.signal.to.control.pd \
     ./3.audio.examples/C07.envelope.follower.pd \
     ./3.audio.examples/C08.analog.sequencer.pd \
     ./3.audio.examples/C09.sample.hold.pd \
     ./3.audio.examples/C10.monophonic.synth.pd \
     ./3.audio.examples/D01.envelope.gen.pd \
     ./3.audio.examples/D02.adsr.pd \
     ./3.audio.examples/D03.envelope.dB.pd \
     ./3.audio.examples/D04.envelope.quartic.pd \
     ./3.audio.examples/D05.envelope.pitch.pd \
     ./3.audio.examples/D06.envelope.portamento.pd \
     ./3.audio.examples/D07.additive.pd \
     ./3.audio.examples/D08.table.spectrum.pd \
     ./3.audio.examples/D09.shepard.tone.pd \
     ./3.audio.examples/D10.sampler.notes.pd \
     ./3.audio.examples/D11.sampler.poly.pd \
     ./3.audio.examples/D12.sampler.bis.pd \
     ./3.audio.examples/D13.additive.qlist.pd \
     ./3.audio.examples/D14.vibrato.pd \
     ./3.audio.examples/E01.spectrum.pd \
     ./3.audio.examples/E02.ring.modulation.pd \
     ./3.audio.examples/E03.octave.divider.pd \
     ./3.audio.examples/E04.difference.tone.pd \
     ./3.audio.examples/E05.chebychev.pd \
     ./3.audio.examples/E06.exponential.pd \
     ./3.audio.examples/E07.evenodd.pd \
     ./3.audio.examples/E08.phase.mod.pd \
     ./3.audio.examples/E09.FM.spectrum.pd \
     ./3.audio.examples/E10.complex.FM.pd \
     ./3.audio.examples/F01.pulse.pd \
     ./3.audio.examples/F02.just.say.pd \
     ./3.audio.examples/F03.pulse.spectrum.pd \
     ./3.audio.examples/F04.waveshaping.pulse.pd \
     ./3.audio.examples/F05.ring.modulation.pd \
     ./3.audio.examples/F06.packets.pd \
     ./3.audio.examples/F07.packet.spectrum.pd \
     ./3.audio.examples/F08.two.cosines.pd \
     ./3.audio.examples/F09.declickit.pd \
     ./3.audio.examples/F10.sweepable.FM.pd \
     ./3.audio.examples/F11.anharmonic.FM.pd \
     ./3.audio.examples/F12.paf.pd \
     ./3.audio.examples/F13.paf.control.pd \
     ./3.audio.examples/F14.wave.packet.pd \
     ./3.audio.examples/G01.delay.pd \
     ./3.audio.examples/G02.delay.loop.pd \
     ./3.audio.examples/G03.delay.variable.pd \
     ./3.audio.examples/G04.control.blocksize.pd \
     ./3.audio.examples/G05.execution.order.pd \
     ./3.audio.examples/G06.octave.doubler.pd \
     ./3.audio.examples/G07.shaker.pd \
     ./3.audio.examples/G08.reverb.pd \
     ./3.audio.examples/G09.pitchshift.pd \
     ./3.audio.examples/H01.low-pass.pd \
     ./3.audio.examples/H02.high-pass.pd \
     ./3.audio.examples/H03.band-pass.pd \
     ./3.audio.examples/H04.filter.sweep.pd \
     ./3.audio.examples/H05.filter.floyd.pd \
     ./3.audio.examples/H06.envelope.follower.pd \
     ./3.audio.examples/H07.measure.spectrum.pd \
     ./3.audio.examples/H08.heterodyning.pd \
     ./3.audio.examples/H09.ssb.modulation.pd \
     ./3.audio.examples/H10.measurement.pd \
     ./3.audio.examples/H11.shelving.pd \
     ./3.audio.examples/H12.peaking.pd \
     ./3.audio.examples/H13.butterworth.pd \
     ./3.audio.examples/H14.all.pass.pd \
     ./3.audio.examples/H15.phaser.pd \
     ./3.audio.examples/H16.adsr.filter.qlist.pd \
     ./3.audio.examples/I01.Fourier.analysis.pd \
     ./3.audio.examples/I02.Hann.window.pd \
     ./3.audio.examples/I03.resynthesis.pd \
     ./3.audio.examples/I04.noisegate.pd \
     ./3.audio.examples/I05.compressor.pd \
     ./3.audio.examples/I06.timbre.stamp.pd \
     ./3.audio.examples/I07.phase.vocoder.pd \
     ./3.audio.examples/I08.pvoc.reverb.pd \
     ./3.audio.examples/I09.sheep.from.goats.pd \
     ./3.audio.examples/I10.phase.bash.pd \
     ./3.audio.examples/J01.even.odd.pd \
     ./3.audio.examples/J02.trapezoids.pd \
     ./3.audio.examples/J03.pulse.width.mod.pd \
     ./3.audio.examples/J04.corners.pd \
     ./3.audio.examples/J05.triangle.pd \
     ./3.audio.examples/J06.enveloping.pd \
     ./3.audio.examples/J07.oversampling.pd \
     ./3.audio.examples/J08.classicsynth.pd \
     ./3.audio.examples/J09.bandlimited.pd \
     ./3.audio.examples/adsr.pd \
     ./3.audio.examples/buttercoef3.pd \
     ./3.audio.examples/butterworth3~.pd \
     ./3.audio.examples/filter-graph1.pd \
     ./3.audio.examples/filter-graph2.pd \
     ./3.audio.examples/osc-voice.pd \
     ./3.audio.examples/output~.pd \
     ./3.audio.examples/partial.pd \
     ./3.audio.examples/qlist-sampler.txt \
     ./3.audio.examples/qlist.txt \
     ./3.audio.examples/qlist2.txt \
     ./3.audio.examples/reverb-echo.pd \
     ./3.audio.examples/sampvoice.pd \
     ./3.audio.examples/sampvoice2.pd \
     ./3.audio.examples/shepvoice.pd \
     ./3.audio.examples/sinevoice.pd \
     ./3.audio.examples/spectrum-partial.pd \
     ./4.data.structures/00.intro.txt \
     ./4.data.structures/01.scalars.pd \
     ./4.data.structures/02.getting.data.pd \
     ./4.data.structures/03.setting.data.pd \
     ./4.data.structures/04.append.pd \
     ./4.data.structures/05.array.pd \
     ./4.data.structures/06.file.pd \
     ./4.data.structures/07.sequencer.pd \
     ./4.data.structures/08.selection.pd \
     ./4.data.structures/09.scaling.pd \
     ./4.data.structures/10.onoff.pd \
     ./4.data.structures/11.array.controls.pd \
     ./4.data.structures/12.beat-patterns.pd \
     ./4.data.structures/13.sliderule.pd \
     ./4.data.structures/14.partialtracer.pd \
     ./4.data.structures/add-trace.pd \
     ./4.data.structures/beat-maker.pd \
     ./4.data.structures/data-array.pd \
     ./4.data.structures/data-start.pd \
     ./4.data.structures/file.txt \
     ./4.data.structures/osc-voice.pd \
     ./4.data.structures/output~.pd \
     ./4.data.structures/voice.pd \
     ./4.data.structures/z.txt \
     ./5.reference/0_all_guis-INTRO.txt \
     ./5.reference/abs~-help.pd \
     ./5.reference/acoustics-help.pd \
     ./5.reference/acoustics~-help.pd \
     ./5.reference/adc~_dac~-help.pd \
     ./5.reference/append-help.pd \
     ./5.reference/array-object-help.pd \
     ./5.reference/array-object-help.txt \
     ./5.reference/bag-help.pd \
     ./5.reference/bang-help.pd \
     ./5.reference/bang~-help.pd \
     ./5.reference/biquad~-help.pd \
     ./5.reference/block~-help.pd \
     ./5.reference/bng-help.pd \
     ./5.reference/bp~-help.pd \
     ./5.reference/canvas-help.pd \
     ./5.reference/change-help.pd \
     ./5.reference/clip-help.pd \
     ./5.reference/clip~-help.pd \
     ./5.reference/clone-abstraction.pd \
     ./5.reference/clone-help.pd \
     ./5.reference/cos~-help.pd \
     ./5.reference/cpole~-help.pd \
     ./5.reference/cputime-help.pd \
     ./5.reference/czero_rev~-help.pd \
     ./5.reference/czero~-help.pd \
     ./5.reference/declare-help.pd \
     ./5.reference/delay-help.pd \
     ./5.reference/delread4~-help.pd \
     ./5.reference/delread~-help.pd \
     ./5.reference/delwrite~-help.pd \
     ./5.reference/drawpolygon-help.pd \
     ./5.reference/drawtext-help.pd \
     ./5.reference/element-help.pd \
     ./5.reference/env~-help.pd \
     ./5.reference/expr-help.pd \
     ./5.reference/exp~-help.pd \
     ./5.reference/fft~-help.pd \
     ./5.reference/float-help.pd \
     ./5.reference/framp~-help.pd \
     ./5.reference/fudiformat-help.pd \
     ./5.reference/fudiparse-help.pd \
     ./5.reference/gatom-help.pd \
     ./5.reference/get-help.pd \
     ./5.reference/getsize-help.pd \
     ./5.reference/graph-help.pd \
     ./5.reference/hdial-help.pd \
     ./5.reference/help-intro.pd \
     ./5.reference/hip~-help.pd \
     ./5.reference/hradio-help.pd \
     ./5.reference/hslider-help.pd \
     ./5.reference/int-help.pd \
     ./5.reference/key-help.pd \
     ./5.reference/line-help.pd \
     ./5.reference/line~-help.pd \
     ./5.reference/list-help.pd \
     ./5.reference/loadbang-help.pd \
     ./5.reference/log~-help.pd \
     ./5.reference/lop~-help.pd \
     ./5.reference/makefilename-help.pd \
     ./5.reference/makenote-help.pd \
     ./5.reference/math-help.pd \
     ./5.reference/message-help.pd \
     ./5.reference/metro-help.pd \
     ./5.reference/midi-help.pd \
     ./5.reference/moses-help.pd \
     ./5.reference/my_canvas-help.pd \
     ./5.reference/namecanvas-help.pd \
     ./5.reference/netreceive-help.pd \
     ./5.reference/netsend-help.pd \
     ./5.reference/noise~-help.pd \
     ./5.reference/numbox2-help.pd \
     ./5.reference/openpanel-help.pd \
     ./5.reference/operators-help.pd \
     ./5.reference/oscformat-help.pd \
     ./5.reference/oscparse-help.pd \
     ./5.reference/osc~-help.pd \
     ./5.reference/otherbinops-help.pd \
     ./5.reference/out1~.pd \
     ./5.reference/pack-help.pd \
     ./5.reference/pd-help.pd \
     ./5.reference/phasor~-help.pd \
     ./5.reference/pipe-help.pd \
     ./5.reference/plot-help.pd \
     ./5.reference/pointer-help.pd \
     ./5.reference/poly-help.pd \
     ./5.reference/pow~-help.pd \
     ./5.reference/print-help.pd \
     ./5.reference/print~-help.pd \
     ./5.reference/qlist-help.pd \
     ./5.reference/qlist.txt \
     ./5.reference/random-help.pd \
     ./5.reference/readsf~-help.pd \
     ./5.reference/realtime-help.pd \
     ./5.reference/receive-help.pd \
     ./5.reference/route-help.pd \
     ./5.reference/rpole~-help.pd \
     ./5.reference/rsqrt~-help.pd \
     ./5.reference/rzero_rev~-help.pd \
     ./5.reference/rzero~-help.pd \
     ./5.reference/samphold~-help.pd \
     ./5.reference/samplerate~-help.pd \
     ./5.reference/savepanel-help.pd \
     ./5.reference/savestate-example.pd \
     ./5.reference/savestate-help.pd \
     ./5.reference/scalar-object-help.pd \
     ./5.reference/select-help.pd \
     ./5.reference/send-help.pd \
     ./5.reference/send~-help.pd \
     ./5.reference/set-help.pd \
     ./5.reference/setsize-help.pd \
     ./5.reference/setsize.txt \
     ./5.reference/sigbinops-help.pd \
     ./5.reference/sig~-help.pd \
     ./5.reference/snapshot~-help.pd \
     ./5.reference/soundfiler-help.pd \
     ./5.reference/spigot-help.pd \
     ./5.reference/sqrt~-help.pd \
     ./5.reference/stripnote-help.pd \
     ./5.reference/struct-help.pd \
     ./5.reference/swap-help.pd \
     ./5.reference/switch~-help.pd \
     ./5.reference/symbol-help.pd \
     ./5.reference/table.txt \
     ./5.reference/tabosc4~-help.pd \
     ./5.reference/tabplay~-help.pd \
     ./5.reference/tabread-help.pd \
     ./5.reference/tabread4-help.pd \
     ./5.reference/tabread4~-help.pd \
     ./5.reference/tabread~-help.pd \
     ./5.reference/tabreceive~-help.pd \
     ./5.reference/tabsend~-help.pd \
     ./5.reference/tabwrite-help.pd \
     ./5.reference/tabwrite~-help.pd \
     ./5.reference/text-help.pd \
     ./5.reference/text-object-help.pd \
     ./5.reference/text-object-help.txt \
     ./5.reference/textfile-help.pd \
     ./5.reference/textfile.txt \
     ./5.reference/threshold~-help.pd \
     ./5.reference/throw~-help.pd \
     ./5.reference/timer-help.pd \
     ./5.reference/toggle-help.pd \
     ./5.reference/trigger-help.pd \
     ./5.reference/unpack-help.pd \
     ./5.reference/until-help.pd \
     ./5.reference/value-help.pd \
     ./5.reference/vcf~-help.pd \
     ./5.reference/vdial-help.pd \
     ./5.reference/vline~-help.pd \
     ./5.reference/vradio-help.pd \
     ./5.reference/vslider-help.pd \
     ./5.reference/vsnapshot~-help.pd \
     ./5.reference/vu-help.pd \
     ./5.reference/wrap~-help.pd \
     ./5.reference/writesf~-help.pd \
     ./5.reference/x_all_guis.pd \
     ./6.externs/0.README.txt \
     ./6.externs/dspobj~.c \
     ./6.externs/makefile \
     ./6.externs/obj1.c \
     ./6.externs/obj2.c \
     ./6.externs/obj3.c \
     ./6.externs/obj4.c \
     ./6.externs/obj5.c \
     ./6.externs/test-dspobj~.pd \
     ./6.externs/test-obj1.pd \
     ./6.externs/test-obj2.pd \
     ./6.externs/test-obj3.pd \
     ./6.externs/test-obj4.pd \
     ./6.externs/test-obj5.pd \
     ./7.stuff/soundfile-tools/1.ring-mod.pd \
     ./7.stuff/soundfile-tools/2.bandpass.pd \
     ./7.stuff/soundfile-tools/3.phase.vocoder.pd \
     ./7.stuff/soundfile-tools/4.looper.pd \
     ./7.stuff/soundfile-tools/5.reverb.pd \
     ./7.stuff/soundfile-tools/6.vocoder.pd \
     ./7.stuff/soundfile-tools/README.txt \
     ./7.stuff/synth/1.poly.synth.pd \
     ./7.stuff/synth/README.txt \
     ./7.stuff/synth/gadsr.pd \
     ./7.stuff/synth/numset.pd \
     ./7.stuff/synth/preset.pd \
     ./7.stuff/synth/preset1.txt \
     ./7.stuff/synth/preset2.txt \
     ./7.stuff/synth/preset3.txt \
     ./7.stuff/synth/preset4.txt \
     ./7.stuff/synth/synthvoice.pd \
     ./7.stuff/synth/test-gadsr.pd \
     ./7.stuff/tools/latency.pd \
     ./7.stuff/tools/load-meter.pd \
     ./7.stuff/tools/miditester.pd \
     ./7.stuff/tools/sizingtest.pd \
     ./7.stuff/tools/testtone.pd \
     ./7.stuff/tools/testtone16.pd \
     ./Makefile.am \
     ./sound/bell.aiff \
     ./sound/voice.wav \
     ./sound/voice2.wav \
     $(empty)
