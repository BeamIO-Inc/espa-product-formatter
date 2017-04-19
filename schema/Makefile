#------------------------------------------------------------------------------
# Makefile
#
# Simple makefile for installing the schema.
#------------------------------------------------------------------------------
.PHONY: all install clean

# Inherit from upper-level make.config
TOP = ..
include $(TOP)/make.config

SCHEMAS = espa_internal_metadata_v1_0.xsd \
          espa_internal_metadata_v1_1.xsd \
          espa_internal_metadata_v1_2.xsd \
          espa_internal_metadata_v1_3.xsd \
          espa_internal_metadata_v2_0.xsd

all:

install:
	install -d $(schema_link_path)
	install -d $(schema_lib_install_path)
	install -m 644 $(SCHEMAS) $(schema_lib_install_path) || exit 1
	@for schema in $(SCHEMAS); do \
        cmd="ln -sf $(schema_lib_link_path)/$$schema $(schema_link_path)/$$schema"; \
        echo "$$cmd"; $$cmd; \
        done

clean:

