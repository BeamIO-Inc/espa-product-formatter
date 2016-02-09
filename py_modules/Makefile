#------------------------------------------------------------------------------
# Makefile
#
# Simple makefile for installing the modules.
#------------------------------------------------------------------------------
.PHONY: all install clean

# Inherit from upper-level make.config
TOP = ..
include $(TOP)/make.config

PYTHON_MODULES = espa_constants.py espa_logging.py metadata_api.py

all:

install:
	install -d $(python_link_path)
	install -d $(python_lib_install_path)
	install -m 644 $(PYTHON_MODULES) $(python_lib_install_path)
	@for module in $(PYTHON_MODULES); do \
        echo "ln -sf $(python_lib_link_path)/$$module $(python_link_path)/$$module"; \
        ln -sf $(python_lib_link_path)/$$module $(python_link_path)/$$module; \
        done

clean:

