#-----------------------------------------------------------------------------
# Makefile
#
# for IAS libraries
#-----------------------------------------------------------------------------
.PHONY: all install clean

# Inherit from upper-level make.config
TOP = ../../..
include $(TOP)/make.config


#-----------------------------------------------------------------------------
# Set up compile options
CC    = gcc
RM    = rm
AR    = ar rcsv
EXTRA = -Wall $(EXTRA_OPTIONS)


# Define the include files
INC = config.h                   \
      ias_angle_gen_distro.h     \
      ias_angle_gen_private.h    \
      ias_angle_gen_includes.h   \
      ias_math.h                 \
      ias_const.h                \
      ias_geo.h                  \
      ias_types.h                \
      ias_structures.h           \
      ias_logging.h              \
      ias_odl.h                  \
      ias_satellite_attributes.h \
      ias_miscellaneous.h        \
      ias_parm.h                 \
      ias_parm_private.h         \
      local_defines.h            \
      lablib3.h                  \
      toolbox.h

# Define the source code object files
SRC = \
      ias_angle_gen_calculate_angles_rpc.c      \
      ias_angle_gen_read_ang.c                  \
      ias_angle_gen_utilities.c                 \
      ias_angle_gen_initialize.c                \
      ias_angle_gen_write_image.c               \
      ias_angle_gen_find_scas.c                 \
      ias_geo_convert_dms2deg.c                 \
      ias_math_find_line_segment_intersection.c \
      ias_logging.c                             \
      ias_misc_create_output_image_trim_lut.c   \
      ias_misc_convert_to_uppercase.c           \
      ias_misc_write_envi_header.c              \
      ias_odl_free_tree.c                       \
      ias_odl_get_field.c                       \
      ias_odl_read_tree.c                       \
      ias_parm_provide_help.c                   \
      ias_parm_read.c                           \
      ias_parm_map_odl_type.c                   \
      ias_parm_check_ranges.c                   \
      ias_satellite_attributes.c                \
      landsat8.c                                \
      lablib3.c
OBJ = $(SRC:.c=.o)

# Define include paths
INCDIR = -I. -I../../include
NCFLAGS = $(EXTRA) $(INCDIR)

# Define the object libraries and paths
EXLIB   =
MATHLIB =
LOADLIB = $(EXLIB) $(MATHLIB)

# Define the C library/archive
ARCHIVE = lib_espa_l8_ang.a

#-----------------------------------------------------------------------------
all: $(ARCHIVE)

$(ARCHIVE): $(OBJ) $(INC)
	$(AR) $(ARCHIVE) $(OBJ)
	install -d ../../lib
	install -d ../../include
	install -m 644 $(ARCHIVE) ../../lib
	install -m 644 $(INC) ../../include

#-----------------------------------------------------------------------------
clean:
	$(RM) -f *.o $(ARCHIVE)

#-----------------------------------------------------------------------------
$(OBJ): $(INC)

.c.o:
	$(CC) $(NCFLAGS) -c $<

