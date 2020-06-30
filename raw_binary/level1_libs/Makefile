#-----------------------------------------------------------------------------
# Makefile
#
# for ESPA Level-1 related libraries
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
INC = clip_band_misalignment.h generate_date_bands.h

# Define the source code and object files
SRC = \
      clip_band_misalignment.c  \
      clip_band_misalignment_landsat89.c  \
      generate_date_bands.c
OBJ = $(SRC:.c=.o)

# Define include paths
INCDIR  = -I. -I../include -I$(XML2INC)
NCFLAGS = $(EXTRA) $(INCDIR)

# Define the object libraries and paths
# Not used in this library only directory
#EXLIB   =
#MATHLIB =
#LOADLIB = $(EXLIB) $(MATHLIB)

# Define the C library/archive
ARCHIVE = lib_espa_level1_libs.a

#-----------------------------------------------------------------------------
all: $(ARCHIVE)

$(ARCHIVE): $(OBJ) $(INC)
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

