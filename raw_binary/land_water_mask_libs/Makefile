#-----------------------------------------------------------------------------
# Makefile
#
# for ESPA land water mask library
#-----------------------------------------------------------------------------
.PHONY: all install-headers install-lib install clean

# Inherit from upper-level make.config
TOP = ../..
include $(TOP)/make.config

#-----------------------------------------------------------------------------
# Set up compile options
CC    = gcc
RM    = rm
AR    = ar rcsv
EXTRA = -Wall $(EXTRA_OPTIONS)

# Define the include files
INC = ias_lw_geo.h ias_types.h ias_structures.h ias_math.h ias_const.h ias_logging.h config.h generate_land_water_mask.h

# Define the source code and object files
SRC = \
      generate_land_water_mask.c          \
      deg_to_dms.c                        \
      ias_math_point_in_closed_polygon.c  \
      ias_geo_convert_dms2deg.c           \
      ias_geo_convert_deg2dms.c           \
      ias_geo_find_deg.c                  \
      ias_geo_find_min.c                  \
      ias_geo_find_sec.c                  \
      ias_geo_handle_180.c                \
      ias_geo_projection_transformation.c \
      ias_geo_shape_file.c                \
      ias_geo_shape_mask.c                \
      ias_logging.c
OBJ = $(SRC:.c=.o)

# Define include paths
INCDIR = -I. -I../include -I$(XML2INC) -I$(HDFINC) -I$(HDFEOS_INC)
NCFLAGS = $(EXTRA) $(INCDIR)

# Define the object libraries and paths
# Not used in this library only directory
#EXLIB   =
#MATHLIB =
#LOADLIB = $(EXLIB) $(MATHLIB)

# Define the C library/archive
ARCHIVE = lib_espa_land_water_mask.a

#-----------------------------------------------------------------------------
all: $(ARCHIVE)

$(ARCHIVE): $(OBJ)
	$(AR) $(ARCHIVE) $(OBJ) 
	install -d ../lib
	install -d ../include
	install -m 644 $(ARCHIVE) ../lib
	install -m 644 $(INC) ../include

#-----------------------------------------------------------------------------
install-headers:
	install -d $(inc_link_path)
	install -d $(raw_binary_inc_install_path)
	@for inc in $(INC); do \
        echo "install -m 644 $$inc $(raw_binary_inc_install_path)/$$inc"; \
        install -m 644 $$inc $(raw_binary_inc_install_path)/$$inc || exit 1; \
        echo "ln -sf $(raw_binary_link_inc_path)/$$inc $(inc_link_path)/$$inc"; \
        ln -sf $(raw_binary_link_inc_path)/$$inc $(inc_link_path)/$$inc; \
        done

#-----------------------------------------------------------------------------
install-lib: all
	install -d $(lib_link_path)
	install -d $(raw_binary_lib_install_path)
	install -m 644 $(ARCHIVE) $(raw_binary_lib_install_path)
	ln -sf $(raw_binary_link_lib_path)/$(ARCHIVE) $(lib_link_path)/$(ARCHIVE)

#-----------------------------------------------------------------------------
install: install-lib install-headers

#-----------------------------------------------------------------------------
clean:
	$(RM) -f *.o $(ARCHIVE)

#-----------------------------------------------------------------------------
$(OBJ): $(INC)

.c.o:
	$(CC) $(NCFLAGS) -c $<

