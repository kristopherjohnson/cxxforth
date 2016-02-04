# Targets:
# 
# - default    builds 'targets' and 'tags'
# - all        builds 'targets', 'optimized', and 'tags'
# - targets    builds cxxforth executable
# - optimized  builds cxxforth with runtime checks disabled
# - clean      removes build products

CMAKE ?= cmake
MKDIR ?= mkdir
RM ?= rm
CD ?= cd
CTAGS ?= ctags

BUILDDIR ?= build
OPTIMIZEDDIR ?= build_optimized

.PHONY: default
default: targets tags

.PHONY: all
all: targets optimized tags

.PHONY: targets
targets: $(BUILDDIR)/Makefile
	$(MAKE) -C $(BUILDDIR) 

$(BUILDDIR)/Makefile: CMakeLists.txt Makefile
	$(MKDIR) -p $(BUILDDIR)
	cd $(BUILDDIR) && $(CMAKE) $(CMAKEFLAGS) ..

.PHONY: optimized
optimized: $(OPTIMIZEDDIR)/Makefile
	$(MAKE) -C $(OPTIMIZEDDIR)

$(OPTIMIZEDDIR)/Makefile: CMakeLists.txt Makefile
	$(MKDIR) -p $(OPTIMIZEDDIR)
	cd $(OPTIMIZEDDIR) && $(CMAKE) $(CMAKEFLAGS) -DCXXFORTH_SKIP_RUNTIME_CHECKS=ON ..

tags: cxxforth.cpp cxxforth.h
	$(CTAGS) $^

.PHONY: clean
clean:
	- $(RM) -r $(BUILDDIR)
	- $(RM) -r $(OPTIMIZEDDIR)

