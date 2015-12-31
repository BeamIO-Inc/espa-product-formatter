#-----------------------------------------------------------------------------
# Makefile
#
# for gctp3 library
#-----------------------------------------------------------------------------
.PHONY: all install clean

# Inherit from upper-level make.config
TOP = ../../..
include $(TOP)/make.config


#-----------------------------------------------------------------------------
CC    = gcc
RM    = rm
AR    = ar rcsv
EXTRA = -Wall $(EXTRA_OPTIONS)

# Define the include files
INC = gctp.h
PRIVATEINCS = isin.h

# Define the source code object files
SRC = \
      alberfor.c                   \
      alberinv.c                   \
      alconfor.c                   \
      alconinv.c                   \
      azimfor.c                    \
      aziminv.c                    \
      cproj.c                      \
      eqconfor.c                   \
      eqconinv.c                   \
      equifor.c                    \
      equiinv.c                    \
      for_init.c                   \
      gctp.c                       \
      gnomfor.c                    \
      gnominv.c                    \
      goodfor.c                    \
      goodinv.c                    \
      gvnspfor.c                   \
      gvnspinv.c                   \
      hamfor.c                     \
      haminv.c                     \
      imolwfor.c                   \
      imolwinv.c                   \
      inv_init.c                   \
      isinfor.c                    \
      isininv.c                    \
      lamazfor.c                   \
      lamazinv.c                   \
      merfor.c                     \
      merinv.c                     \
      millfor.c                    \
      millinv.c                    \
      molwfor.c                    \
      molwinv.c                    \
      obleqfor.c                   \
      obleqinv.c                   \
      orthfor.c                    \
      orthinv.c                    \
      paksz.c                      \
      robfor.c                     \
      robinv.c                     \
      sinfor.c                     \
      sininv.c                     \
      sphdz.c                      \
      sterfor.c                    \
      sterinv.c                    \
      untfz.c                      \
      vandgfor.c                   \
      vandginv.c                   \
      wivfor.c                     \
      wivinv.c                     \
      wviifor.c                    \
      wviiinv.c                    \
      gctp_create_transformation.c \
      gctp_dms2degrees.c           \
      gctp_transform.c             \
      gctp_print_message.c         \
      gctp_utility.c               \
      gctp_report.c                \
      geographic.c                 \
      oblique_mercator.c           \
      polar_stereographic.c        \
      polyconic.c                  \
      lambert_conformal_conic.c    \
      som.c                        \
      state_plane.c                \
      tm.c
OBJ = $(SRC:.c=.o)

# Define include paths
INCDIR = -I.
# -I../../include
NCFLAGS = $(EXTRA) $(INCDIR)

# Define the object libraries and paths
EXLIB   =
MATHLIB =
LOADLIB = $(EXLIB) $(MATHLIB)

# Define the C library/archive
ARCHIVE = libgctp3.a

#-----------------------------------------------------------------------------
all: $(ARCHIVE)

$(ARCHIVE): $(OBJ) $(INC)
	$(AR) $(ARCHIVE) $(OBJ)
	install -d ../../lib
	install -d ../../include
	install -m 644 $(ARCHIVE) ../../lib
	install -m 644 $(INC) ../../include


# RDD - I don't know what this crap is, but I commented it out, because it didn't seem to be required for us.  It seemed to be needed for the Landsat teams check-in processes for some dumb reason..........................
##-------------------------------------------
## Target to build the state plane data files
##-------------------------------------------
#data: nad27sp.lut nad83sp.lut
#
##----------------------------
## Target to install the files
##----------------------------
#install: all $(INCS)
#	mkdir -p $(GCTP3_LIB)
#	cp libgctp3.a $(GCTP3_LIB)
#	mkdir -p $(GCTP3_INC)
#	cp $(INCS) $(GCTP3_INC)
#
## only build if the source file is present to allow for the check-in process
#spload:
#	if [ -f spload.c ] ; then \
#        $(CC) $(CFLAGS) spload.c -o spload; \
#    fi
#
## only run if the spload is present to allow for the check-in process
#nad27sp.lut: spload
#	if [ -f spload ]; then \
#        ./spload; fi
#
## only run if the spload is present to allow for the check-in process
#nad83sp.lut: spload
#	@if [ -f spload ]; then \
#        ./spload; fi
#
#isinfor.o: isinfor.c $(PRIVATEINCS)
#
#isininv.o: isininv.c $(PRIVATEINCS)

#clean:
#	/bin/rm -f *.o *.a core make.log nad83sp.lut spload nad27sp.lut libgctp3.a

#-----------------------------------------------------------------------------
clean:
	$(RM) -f *.o $(ARCHIVE)

#-----------------------------------------------------------------------------
$(OBJ): $(INC)

.c.o:
	$(CC) $(NCFLAGS) -c $<

