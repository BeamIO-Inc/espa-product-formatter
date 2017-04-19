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
LANDSAT_OPTS = -DIAS_NO_SENSOR_META_SUPPORT -DIAS_NOVAS_NOT_AVAILABLE
EXTRA = -Wall $(LANDSAT_OPTS) $(EXTRA_OPTIONS)


# Define the include files
INC = gxx_angle_gen_distro.h \
      gxx_angle_gen_geo_utilities.h \
      gxx_angle_gen_leap_seconds.h \
      gxx_angle_gen_private.h \
      gxx_const.h \
      gxx_coord_conv.h \
      gxx_geo_math.h \
      gxx_proj.h \
      gxx_sensor.h \
      gxx_structures.h \
      lablib3.h \
      toolbox.h \
      xdb_Defines.h \
      xxx_Band.h \
      xxx_CloseUnmap.h \
      xxx_Const.h \
      xxx_DDR.h \
      xxx_Errno.h \
      xxx_FileLock.h \
      xxx_fulljd.h \
      xxx_GetDirFiles.h \
      xxx_GetTempName.h \
      xxx_GetTime.h \
      xxx_Imagery.h \
      xxx_LogError.h \
      xxx_LogStatus.h \
      xxx_ODL.h \
      xxx_OpenMap.h \
      xxx_RecursiveDeletion.h \
      xxx_Sensor.h \
      xxx_string_utils.h \
      xxx_SysCommands.h \
      xxx_TimeConvert.h \
      xxx_Types.h

# Define the source code object files
SRC = gxx_angle_gen_calculate_angles_rpc.c \
      gxx_angle_gen_calculate_vector.c \
      gxx_angle_gen_find_dir.c \
      gxx_angle_gen_geo_utilities.c \
      gxx_angle_gen_initialize.c \
      gxx_angle_gen_initialize_transformation.c \
      gxx_angle_gen_interpolate_ephemeris.c \
      gxx_angle_gen_read_ang.c \
      gxx_angle_gen_write_image.c \
      gxx_dmsdeg.c \
      gxx_dot.c \
      gxx_geod2cart.c \
      gxx_get_units.c \
      gxx_unit.c \
      xxx_Band.c \
      xxx_CloseODL.c \
      xxx_CloseUnmap.c \
      xxx_ConvertString.c \
      xxx_Errno.c \
      xxx_FileLock.c \
      xxx_GetDirFiles.c \
      xxx_GetODLField.c \
      xxx_GetTempName.c \
      xxx_GetTime.c \
      xxx_LogError.c \
      xxx_LogStatus.c \
      xxx_OpenMap.c \
      xxx_OpenODL.c \
      xxx_RecursiveDeletion.c \
      xxx_Sensor.c \
      xxx_strtoupper.c \
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
ARCHIVE = lib_espa_landsat_ang.a

#-----------------------------------------------------------------------------
all: $(ARCHIVE)

$(ARCHIVE): $(OBJ) $(INC)
	$(AR) $(ARCHIVE) $(OBJ)
	install -d ../../lib
	install -d ../../include
	install -d ../../include/etm
	install -d ../../include/tm
	install -d ../../include/mss
	install -m 644 $(ARCHIVE) ../../lib
	install -m 644 $(INC) ../../include
	install -m 644 etm/*.h ../../include/etm
	install -m 644 tm/*.h ../../include/tm
	install -m 644 mss/*.h ../../include/mss

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
clean:
	$(RM) -f *.o $(ARCHIVE)

#-----------------------------------------------------------------------------
$(OBJ): $(INC)

.c.o:
	$(CC) $(NCFLAGS) -c $<

