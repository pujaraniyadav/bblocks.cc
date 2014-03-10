ifndef OBJDIR
OBJDIR = $(PWD)/../build
endif

STDCPP = /usr/include/c++/4.6.3/

SUBDIR = src/			\
	 src/kmod	 	\
	 test			\

clean: build-teardown

#
# Disabled compiling kernel modules temporarily. Need to enable them.
#

libsrc: all

modules:
	@echo '  KMOD		'
	@make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd)/src/kmod modules
	@make INSTALL_MOD_PATH=$(OBJDIR) -C /lib/modules/$(shell uname -r)/build M=$(PWD)/src/kmod modules_install
	@make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd)/src/kmod clean

build-setup:
	@echo ' SETUP		' $(OBJDIR)
	@mkdir -p $(OBJDIR)

build-teardown:
	@echo ' CLEAN		' $(OBJDIR)
	@rm -r -f $(OBJDIR)

build-doc:
	@echo ' DOC		' $(OBJDIR)/doc
	@echo SETUP $(OBJDIR)/doc
	@rm -f -r ../build/doc
	@mkdir $(OBJDIR)/doc
	@doxygen doc/doxygen/Doxyfile

ubuntu-setup:
	@echo ' UBUNTU-SETUP	'
	apt-get install build-essential libaio-dev libaio1 libaio1-dbg \
                        libboost-dev libboost-doc libboost-dbg \
                        libboost-program-options-dev zlibc zlib1g-dev \
                        libtcmalloc-minimal0 libtcmalloc-minimal0-dbg \
                        valgrind fakeroot build-essential crash kexec-tools \
			makedumpfile kernel-wedge

	ln /usr/lib/libtcmalloc_minimal.so.4 /usr/lib/libtcmalloc_minimal.so

.DEFAULT_GOAL := libsrc

include MakefileRules
