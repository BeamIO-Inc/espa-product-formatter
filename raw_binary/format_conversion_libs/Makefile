#-----------------------------------------------------------------------------
# Makefile
#
# for ESPA format conversion libraries
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
INC = convert_lpgs_to_espa.h convert_espa_to_hdf.h convert_espa_to_netcdf.h \
      espa_hdf.h espa_hdf_eos.h convert_espa_to_gtif.h espa_geoloc.h \
      convert_modis_to_espa.h convert_espa_to_raw_binary_bip.h

# Define the source code and object files
SRC = \
      convert_lpgs_to_espa.c           \
      convert_espa_to_hdf.c            \
      espa_hdf.c                       \
      espa_hdf_eos.c                   \
      convert_espa_to_gtif.c           \
      convert_modis_to_espa.c          \
      espa_geoloc.c                    \
      convert_espa_to_raw_binary_bip.c

OBJ = $(SRC:.c=.o)

# Define include paths.  Make a separate path for HDF5/netCDF since HDF4
# interferes with them.
INCDIR  = -I. -I../include -I$(XML2INC) -I$(TIFFINC) -I$(GEOTIFF_INC) \
          -I$(HDFINC) -I$(HDFEOS_INC) -I$(HDFEOS_GCTPINC)
          
NCFLAGS = $(EXTRA) $(INCDIR)

# Make separate HDF5/netCDF definitions since HDF4 interferes with them.
SRC_NETCDF = convert_espa_to_netcdf.c
OBJ_NETCDF = $(SRC_NETCDF:.c=.o)
NETCDF_INCDIR = -I. -I../include -I$(XML2INC) -I$(HDF5INC) -I$(NCDF4INC)
NETCDF_NCFLAGS = $(EXTRA) $(NETCDF_INCDIR)

# Define the object libraries and paths
# Not used in this library only directory
#EXLIB   =
#MATHLIB =
#LOADLIB = $(EXLIB) $(MATHLIB)

# Define the C library/archive
ARCHIVE = lib_espa_format_conversion.a

#-----------------------------------------------------------------------------
all: $(ARCHIVE)

$(ARCHIVE): $(OBJ) $(INC) $(OBJ_NETCDF)
	$(AR) $(ARCHIVE) $(OBJ)
	$(AR) $(ARCHIVE) $(OBJ_NETCDF)
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
        install -m 644 $$inc $(raw_binary_inc_install_path)/$$inc; \
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

$(OBJ_NETCDF): $(SRC_NETCDF)
	$(CC) $(NETCDF_NCFLAGS) -c $<

.c.o:
	$(CC) $(NCFLAGS) -c $<
