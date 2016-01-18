#-----------------------------------------------------------------------------
# Makefile
# for raw binary tools
#-----------------------------------------------------------------------------
.PHONY: all install clean

# Inherit from upper-level make.config
TOP = ../..
include $(TOP)/make.config

#-----------------------------------------------------------------------------
# Set up compile options
CC    = gcc
RM    = rm
EXTRA = -Wall $(EXTRA_OPTIONS)

# Define the include files
INC =

# Define the source code and object files
SRC1 = convert_lpgs_to_espa.c
OBJ1 = $(SRC1:.c=.o)

SRC2 = convert_espa_to_hdf.c
OBJ2 = $(SRC2:.c=.o)

SRC3 = convert_espa_to_gtif.c
OBJ3 = $(SRC3:.c=.o)

SRC4 = espa_band_subset.c
OBJ4 = $(SRC4:.c=.o)

SRC5 = espa_product_subset.c
OBJ5 = $(SRC5:.c=.o)

SRC6 = convert_modis_to_espa.c
OBJ6 = $(SRC6:.c=.o)

SRC7 = create_l8_angle_bands.c
OBJ7 = $(SRC7:.c=.o)

SRC8 = create_land_water_mask.c
OBJ8 = $(SRC8:.c=.o)

SRC9 = convert_espa_to_bip.c
OBJ9 = $(SRC9:.c=.o)

SRC10 = create_julian_date_bands.c
OBJ10 = $(SRC10:.c=.o)

SRC11 = clip_band_misalignment.c
OBJ11 = $(SRC11:.c=.o)

# Define include paths
INCDIR  = -I. -I../include -I$(XML2INC) -I$(TIFFINC) -I$(GEOTIFF_INC) \
          -I$(HDFINC) -I$(HDFEOS_INC) -I$(JBIGINC) -I$(ZLIBINC)
NCFLAGS = $(EXTRA) $(INCDIR)

# Define the object libraries and paths
MATHLIB = -lm

LIB1   = \
    -L../lib -l_espa_format_conversion -l_espa_raw_binary -l_espa_common \
    -L$(XML2LIB) -lxml2 \
    -L$(TIFFLIB) -ltiff \
    -L$(GEOTIFF_LIB) -lgeotiff \
    -L$(HDFEOS_GCTPLIB) -lGctp \
    -L$(JPEGLIB) -ljpeg \
    -L$(JBIGLIB) -ljbig \
    -L$(ZLIBLIB) -lz \
    $(MATHLIB)

LIB2   = \
    -L../lib -l_espa_format_conversion -l_espa_raw_binary -l_espa_common \
    -L$(XML2LIB) -lxml2 \
    -L$(HDFLIB) -lmfhdf -ldf \
    -L$(HDFEOS_LIB) -lhdfeos \
    -L$(HDFEOS_GCTPLIB) -lGctp \
    -L$(JPEGLIB) -ljpeg \
    -L$(JBIGLIB) -ljbig \
    -L$(ZLIBLIB) -lz \
    $(MATHLIB)

LIB3   = \
    -L../lib -l_espa_format_conversion -l_espa_raw_binary -l_espa_common \
    -L$(XML2LIB) -lxml2 \
    -L$(JPEGLIB) -ljpeg \
    -L$(JBIGLIB) -ljbig \
    -L$(ZLIBLIB) -lz \
    $(MATHLIB)

LIB4   = \
    -L../lib -l_espa_format_conversion -l_espa_raw_binary -l_espa_common \
    -L$(XML2LIB) -lxml2 \
    -L$(JPEGLIB) -ljpeg \
    -L$(JBIGLIB) -ljbig \
    -L$(ZLIBLIB) -lz \
    $(MATHLIB)

LIB5   = \
    -L../lib -l_espa_format_conversion -l_espa_raw_binary -l_espa_common \
    -L$(XML2LIB) -lxml2 \
    -L$(JPEGLIB) -ljpeg \
    -L$(JBIGLIB) -ljbig \
    -L$(ZLIBLIB) -lz \
    $(MATHLIB)

LIB6   = \
    -L../lib -l_espa_format_conversion -l_espa_raw_binary -l_espa_common \
    -L$(XML2LIB) -lxml2 \
    -L$(HDFLIB) -lmfhdf -ldf \
    -L$(HDFEOS_LIB) -lhdfeos \
    -L$(HDFEOS_GCTPLIB) -lGctp \
    -L$(JPEGLIB) -ljpeg \
    -L$(JBIGLIB) -ljbig \
    -L$(ZLIBLIB) -lz \
    $(MATHLIB)

LIB7   = \
    -L../lib -l_espa_raw_binary -l_espa_common \
    -l_espa_l8_angles -l_espa_l8_ang \
    -L$(ZLIBLIB) -lz \
    $(MATHLIB)

LIB8   = \
    -L../lib -l_espa_raw_binary -l_espa_common \
    -l_espa_land_water_mask -l_espa_l8_ang \
    -L$(XML2LIB) -lxml2 \
    -lgctp3 \
    -L$(ZLIBLIB) -lz \
    $(MATHLIB)

LIB9   = \
    -L../lib -l_espa_format_conversion -l_espa_raw_binary -l_espa_common \
    -L$(XML2LIB) -lxml2 \
    -L$(JBIGLIB) -ljbig \
    -L$(ZLIBLIB) -lz \
    $(MATHLIB)

LIB10   = \
    -L../lib -l_espa_level1_libs -l_espa_raw_binary -l_espa_common \
    -L$(XML2LIB) -lxml2 \
    -L$(JBIGLIB) -ljbig \
    -L$(ZLIBLIB) -lz \
    $(MATHLIB)

LIB11   = \
    -L../lib -l_espa_level1_libs -l_espa_raw_binary -l_espa_common \
    -L$(XML2LIB) -lxml2 \
    -L$(JBIGLIB) -ljbig \
    -L$(ZLIBLIB) -lz \
    $(MATHLIB)

# Define C executables
EXE1 = convert_lpgs_to_espa
EXE2 = convert_espa_to_hdf
EXE3 = convert_espa_to_gtif
EXE4 = espa_band_subset
EXE5 = espa_product_subset
EXE6 = convert_modis_to_espa
EXE7 = create_l8_angle_bands
EXE8 = create_land_water_mask
EXE9 = convert_espa_to_bip
EXE10 = create_julian_date_bands
EXE11 = clip_band_misalignment
ALL_EXES = $(EXE1) $(EXE2) $(EXE3) $(EXE4) $(EXE5) $(EXE6) $(EXE7) $(EXE8) $(EXE9) $(EXE10) $(EXE11)

#-----------------------------------------------------------------------------
all: $(ALL_EXES)

$(EXE1): $(OBJ1) $(INC)
	$(CC) $(NCFLAGS) -o $(EXE1) $(OBJ1) $(LIB1)

$(EXE2): $(OBJ2) $(INC)
	$(CC) $(NCFLAGS) -o $(EXE2) $(OBJ2) $(LIB2)

$(EXE3): $(OBJ3) $(INC)
	$(CC) $(NCFLAGS) -o $(EXE3) $(OBJ3) $(LIB3)

$(EXE4): $(OBJ4) $(INC)
	$(CC) $(NCFLAGS) -o $(EXE4) $(OBJ4) $(LIB4)

$(EXE5): $(OBJ5) $(INC)
	$(CC) $(NCFLAGS) -o $(EXE5) $(OBJ5) $(LIB5)

$(EXE6): $(OBJ6) $(INC)
	$(CC) $(NCFLAGS) -o $(EXE6) $(OBJ6) $(LIB6)

$(EXE7): $(OBJ7) $(INC)
	$(CC) $(NCFLAGS) -o $(EXE7) $(OBJ7) $(LIB7)

$(EXE8): $(OBJ8) $(INC)
	$(CC) $(NCFLAGS) -o $(EXE8) $(OBJ8) $(LIB8)

$(EXE9): $(OBJ9) $(INC)
	$(CC) $(NCFLAGS) -o $(EXE9) $(OBJ9) $(LIB9)

$(EXE10): $(OBJ10) $(INC)
	$(CC) $(NCFLAGS) -o $(EXE10) $(OBJ10) $(LIB10)

$(EXE11): $(OBJ11) $(INC)
	$(CC) $(NCFLAGS) -o $(EXE11) $(OBJ11) $(LIB11)

#-----------------------------------------------------------------------------
install: $(ALL_EXES)
	install -d $(bin_link_path)
	install -d $(raw_binary_bin_install_path)
	install -m 755 $(ALL_EXES) $(raw_binary_bin_install_path)
	@for executable in $(ALL_EXES); do \
        echo "ln -sf $(raw_binary_link_bin_path)/$$executable $(bin_link_path)/$$executable"; \
        ln -sf $(raw_binary_link_bin_path)/$$executable $(bin_link_path)/$$executable; \
        done

#-----------------------------------------------------------------------------
clean:
	$(RM) -f *.o $(ALL_EXES)

#-----------------------------------------------------------------------------
$(OBJ1): $(INC)
$(OBJ2): $(INC)
$(OBJ3): $(INC)
$(OBJ4): $(INC)
$(OBJ5): $(INC)
$(OBJ6): $(INC)
$(OBJ7): $(INC)
$(OBJ8): $(INC)
$(OBJ9): $(INC)
$(OBJ10): $(INC)
$(OBJ11): $(INC)

.c.o:
	$(CC) $(NCFLAGS) -c $<

