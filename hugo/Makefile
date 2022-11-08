OUTDIR = "../doc/1.manual/html"

all:
	if [ -d $(OUTDIR) ]; then rm -d -r $(OUTDIR); fi ;\
	hugo; \
	mv public $(OUTDIR); \

install: all

clean:
	if [ -d $(OUTDIR) ]; then rm -d -r $(OUTDIR); fi ;
