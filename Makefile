#-----------------------------------------------------------------------------
# Makefile
#
# Project Name: cloud masking
#-----------------------------------------------------------------------------
.PHONY: check-environment all install clean all-raw-binary install-raw-binary clean-raw-binary

include make.config

DIR_RAW_BINARY = raw_binary

all: all-raw-binary

install: check-environment install-raw-binary

clean: clean-raw-binary

#-----------------------------------------------------------------------------
all-raw-binary:
	echo "make all in $(DIR_RAW_BINARY)"; \
        (cd $(DIR_RAW_BINARY); $(MAKE) all);

install-raw-binary:
	echo "make install in $(DIR_RAW_BINARY)"; \
        (cd $(DIR_RAW_BINARY); $(MAKE) install);

clean-raw-binary:
	echo "make clean in $(DIR_RAW_BINARY)"; \
        (cd $(DIR_RAW_BINARY); $(MAKE) clean);

#-----------------------------------------------------------------------------
check-environment:
ifndef PREFIX
    $(error Environment variable PREFIX is not defined)
endif

