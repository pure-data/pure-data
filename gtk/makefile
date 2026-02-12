OBJ = pdgtk.o tclparser.o canvas.o net.o

PKG_CONFIG ?= pkg-config
gtk.cflags := $(shell $(PKG_CONFIG) --cflags gtk4)
gtk.libs := $(shell $(PKG_CONFIG) --libs gtk4)
tcl.cflags := $(shell $(PKG_CONFIG) --cflags tcl)
tcl.libs := $(shell $(PKG_CONFIG) --libs tcl)

CFLAGS = -g
CFLAGS += $(gtk.cflags) $(tcl.cflags)

LIBS = -lc
LIBS += $(gtk.libs) $(tcl.libs)

pdgtk: $(OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

$(OBJ): pdgtk.h

.PHONY: clean
clean:
	rm -f pdgtk $(OBJ)
