#-----------------------------------------------------------------------------
# Makefile
#
# Project Name: product formatter
#-----------------------------------------------------------------------------
.PHONY: check-environment all install clean all-script install-script clean-script all-raw-binary install-raw-binary clean-raw-binary

include make.config

DIR_RAW_BINARY = raw_binary
DIR_PYTHON = py_modules
DIR_SCHEMA = schema

all: all-script all-raw-binary

install: check-environment install-script install-raw-binary install-python install-schema

clean: clean-script clean-raw-binary

#-----------------------------------------------------------------------------
all-script:
	echo "make all in scripts"; \
        (cd scripts; $(MAKE) all);

install-script:
	echo "make install in scripts"; \
        (cd scripts; $(MAKE) install);

clean-script:
	echo "make clean in scripts"; \
        (cd scripts; $(MAKE) clean);

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
install-python:
	echo "make install in $(DIR_PYTHON)"; \
        (cd $(DIR_PYTHON); $(MAKE) install);

#-----------------------------------------------------------------------------
install-schema:
	echo "make install in $(DIR_SCHEMA)"; \
        (cd $(DIR_SCHEMA); $(MAKE) install);

#-----------------------------------------------------------------------------
check-environment:
ifndef PREFIX
    $(error Environment variable PREFIX is not defined)
endif

