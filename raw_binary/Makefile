#-----------------------------------------------------------------------------
# Makefile
#
# Simple makefile for building and installing raw_binary.
#-----------------------------------------------------------------------------
.PHONY: all install-headers install-lib install clean

LIBDIRS = common \
          io_libs \
          format_conversion_libs \
          level1_libs \
          per_pixel_angles_libs/l8_ias_lib \
          per_pixel_angles_libs/landsat_ias_lib \
          per_pixel_angles_libs \
          land_water_mask_libs/GCTP3 \
          land_water_mask_libs
EXEDIRS = tools \
		  scripts

#-----------------------------------------------------------------------------
all: executables

#-----------------------------------------------------------------------------
libraries:
	@for dir in $(LIBDIRS); do \
        echo "make all in $$dir..."; \
        $(MAKE) -C $$dir || exit 1; done

#-----------------------------------------------------------------------------
executables: libraries
	@for dir in $(EXEDIRS); do \
        echo "make all in $$dir..."; \
        $(MAKE) -C $$dir || exit 1; done

#-----------------------------------------------------------------------------
install-headers:
# if the ESPAINC environment variable points to the 'include' directory, then
# there is no need to install anything
ifneq ($(ESPAINC), $(CURDIR)/include)
	@for dir in $(LIBDIRS); do \
        echo "installing all in $$dir..."; \
        $(MAKE) -C $$dir install-headers || exit 1; done
else
	echo "$(ESPAINC) is the same as the include directory. Include files already installed."
endif

#-----------------------------------------------------------------------------
install-lib: libraries
# if the ESPALIB environment variable points to the 'lib' directory, then
# there is no need to install anything
ifneq ($(ESPALIB), $(CURDIR)/lib)
	@for dir in $(LIBDIRS); do \
        echo "installing all in $$dir..."; \
        $(MAKE) -C $$dir install-lib || exit 1; done
else
	echo "$(ESPALIB) is the same as the lib directory. Libraries already installed."
endif

#-----------------------------------------------------------------------------
install-executables: executables
	@for dir in $(EXEDIRS); do \
        echo "installing all in $$dir..."; \
        (cd $$dir; $(MAKE) install); done

#-----------------------------------------------------------------------------
install: install-lib install-headers install-executables

#-----------------------------------------------------------------------------
clean:
# all directories need to be cleaned
	@for dir in $(LIBDIRS) $(EXEDIRS); do \
        echo "make clean in $$dir..."; \
        (cd $$dir; $(MAKE) clean); done
	rm -r include lib
