# Makefile for the ebmodule package
# Tamito KAJIYAMA <12 September 2001>
# $Id: Makefile,v 1.2 2001/09/22 12:08:19 kajiyama Exp $

PYTHON= python
LIBDIR= `$(PYTHON) -c "import sys; print sys.prefix+'/lib/python'+sys.version[:3]"`
SITEPACKAGEDIR= $(LIBDIR)/site-packages

all:	src/ebmodule.so

src/Makefile.pre.in:
	cp $(LIBDIR)/config/Makefile.pre.in src

src/Makefile:	src/Makefile.pre.in
	(cd src ; $(MAKE) -f Makefile.pre.in boot)

src/ebmodule.so:    src/Makefile
	(cd src ; $(MAKE))

.PHONY:	test
test:	src/ebmodule.so
	(cd test ; $(MAKE) test)

install:	src/ebmodule.so
	(cd src ; $(MAKE) install)
	install -m 644 eblib.py $(SITEPACKAGEDIR)
	$(PYTHON) $(LIBDIR)/compileall.py $(SITEPACKAGEDIR)

clean:	src/Makefile
	(cd src ; $(MAKE) distclean)
	(cd test ; $(MAKE) clean)
	$(RM) -r build
	$(RM) *~ MANIFEST src/Makefile.pre.in

archive:
	$(PYTHON) setup.py sdist -f
