.POSIX:
ALL_WARNING = -Wall -Wextra -pedantic
ALL_LDFLAGS = -lxcb -lxcb-icccm $(LDFLAGS)
ALL_CFLAGS = $(CPPFLAGS) $(CFLAGS) -std=c99 $(ALL_WARNING)
PREFIX = /usr/local
LDLIBS = -lm
BINDIR = $(PREFIX)/bin
XSNDIR = $(PREFIX)/share/xsessions

SRC = xgridwm.c
OBJ = ${SRC:.c=.o}

all: xgridwm
.c.o:
	${CC} -c ${CFLAGS} $< 
${OBJ}: config.h
config.h:
	cp config.def.h $@
xgridwm: ${OBJ}
	$(CC) $(ALL_LDFLAGS) -o $@ ${OBJ} $(LDLIBS)
clean:
	rm -f xgridwm *.o
install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(XSNDIR)
	cp -f xgridwm $(DESTDIR)$(BINDIR)
	chmod 755 $(DESTDIR)$(BINDIR)/xgridwm
	cp -f resources/xgridwm-session $(DESTDIR)$(BINDIR)
	chmod 755 $(DESTDIR)$(BINDIR)/xgridwm-session
	cp -f resources/xgridwm.desktop $(DESTDIR)$(XSNDIR)
	chmod 644 $(DESTDIR)$(XSNDIR)/xgridwm.desktop
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/xgridwm\
		$(DESTDIR)$(BINDIR)/xgridwm-session\
		$(DESTDIR)$(XSNDIR)/xgridwm.desktop
.PHONY: all clean install uninstall
