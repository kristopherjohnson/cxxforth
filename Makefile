CMAKE ?= cmake
MKDIR ?= mkdir
RM ?= rm
CD ?= cd

BUILDDIR ?= build

all: $(BUILDDIR)/Makefile
	$(MAKE) -C $(BUILDDIR)
.PHONY: all

$(BUILDDIR)/Makefile: CMakeLists.txt
	$(MKDIR) -p $(BUILDDIR)
	cd $(BUILDDIR) && $(CMAKE) ..

clean:
	- $(RM) -r $(BUILDDIR)
.PHONY: clean

