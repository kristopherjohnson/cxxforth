CMAKE ?= cmake
MKDIR ?= mkdir
RM ?= rm
CD ?= cd
CTAGS ?= ctags

BUILDDIR ?= build

.PHONY: all
all: cmaketargets tags

.PHONY: cmaketargets
cmaketargets: $(BUILDDIR)/Makefile
	$(MAKE) -C $(BUILDDIR)

$(BUILDDIR)/Makefile: CMakeLists.txt
	$(MKDIR) -p $(BUILDDIR)
	cd $(BUILDDIR) && $(CMAKE) $(CMAKE_FLAGS) ..

tags: forth.cpp forth.h cxxforth.c
	$(CTAGS) $^

.PHONY: clean
clean:
	- $(RM) -r $(BUILDDIR)

