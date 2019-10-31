#------------------------------------------------------------------------------
# Makefile
#
# Simple makefile for installing the modules.
#------------------------------------------------------------------------------
.PHONY: all install clean

# Inherit from upper-level make.config
TOP = ../..
include $(TOP)/make.config

# Define the script installation path
script_install_path = $(espa_project_dir)/bin
script_source_link_path = ../$(project_name)/bin

SCRIPTS = unpackage_s2.py

all:

install:
	install -d $(bin_link_path)
	install -d $(script_install_path)
	@for script in $(SCRIPTS); do \
			echo "install -m 755 $$script $(script_install_path)"; \
			install -m 755 $$script $(script_install_path) || exit 1; \
			echo "ln -sf $(script_source_link_path)/$$script $(bin_link_path)/$$script"; \
			ln -sf $(script_source_link_path)/$$script $(bin_link_path)/$$script; \
		done

clean:

