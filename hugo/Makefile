OUTDIR = "../doc/1.manual/html"
INDEXFILE = "../doc/1.manual/index.htm"

all:
	if [ -d $(OUTDIR) ]; then rm -d -r $(OUTDIR); fi ;\
	hugo; \
	mv public $(OUTDIR); \
	cp static/x/x-index $(INDEXFILE)

install: all

clean:
	if [ -d $(OUTDIR) ]; then rm -d -r $(OUTDIR); fi ;\
	cp static/x/z-index $(INDEXFILE)
