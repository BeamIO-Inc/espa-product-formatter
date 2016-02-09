#------------------------------------------------------------------------------
# Makefile
#
# Simple makefile for installing the schema.
#------------------------------------------------------------------------------
.PHONY: all install clean

# Inherit from upper-level make.config
TOP = ..
include $(TOP)/make.config

SCHEMA = espa_internal_metadata_v1_3.xsd

all:

install:
	install -d $(schema_link_path)
	install -d $(schema_lib_install_path)
	install -m 644 $(SCHEMA) $(schema_lib_install_path)
	ln -sf $(schema_lib_link_path)/$(SCHEMA) $(schema_link_path)/$(SCHEMA)

clean:

