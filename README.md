## ESPA-PRODUCT_FORMATTER Version 1.6.0 Release Notes
Release Date: March 2016

The product formatter project contains libraries and tools for working with the ESPA internal file format (raw binary with an XML metadata file). It currently supports Landsat 4-8.

This project is hosted by the US Geological Survey (USGS) Earth Resources Observation and Science (EROS) Land Satellite Data Systems (LSDS) Science Research and Development (LSRD) Project. For questions regarding this source code, please contact the Landsat Contact Us page and specify USGS CDR/ECV in the "Regarding" section. https://landsat.usgs.gov/contactus.php

### Downloads
espa-product-formatter source code

    git clone https://github.com/USGS-EROS/espa-product-formatter.git

See git tag [version_1.6.0]

### Dependencies
  * GCTP libraries (obtained from the GCTP directory in the HDF-EOS2 source code)
  * TIFF libraries (3.8.2 or most current) -- [Found here](ftp://ftp.remotesensing.org/pub/libtiff/)
  * GeoTIFF libraries (1.2.5 or most current) -- [Found here](ftp://ftp.remotesensing.org/pub/geotiff/libgeotiff/)
  * HDF4 libraries (4.2.5 or most current) -- [Found here](https://www.hdfgroup.org/ftp/HDF/releases/)
  * HDF-EOS2 libraries (2.18 or most current) -- [Found here](ftp://edhs1.gsfc.nasa.gov/edhs/hdfeos/latest_release/)
  * JPEG libraries (version 6b) -- [Found here](http://www.ijg.org/files/)
  * ZLIB libraries (version 1.2.8) -- [Found here](http://zlib.net/)
  * XML2 libraries -- [Found here](ftp://xmlsoft.org/libxml2/)
  * JBIG libraries -- [Found here] (http://www.cl.cam.ac.uk/~mgk25/jbigkit/)
  * Land/water static polygon -- [Found here](http://espa.cr.usgs.gov/downloads/auxiliaries/land_water_poly/land_no_buf.ply.gz)

NOTE: The HDF-EOS2 link currently provides the source for the HDF4, JPEG, and ZLIB libraries in addition to the HDF-EOS2 library.

### Installation
  * Install dependent libraries - HDF-EOS GCTP (from HDF-EOS2), HDF4, HDF-EOS2, TIFF, GeoTIFF, JPEG, XML2, JBIG, and ZLIB.

  * Set up environment variables.  Can create an environment shell file or add the following to your bash shell.  For C shell, use 'setenv VAR "directory"'.  Note: If the HDF library was configured and built with szip support, then the user will also need to add an environment variable for SZIP include (SZIPINC) and library (SZIPLIB) files.
  ```
    export HDFEOS_GCTPINC="path_to_HDF-EOS_GCTP_include_files"
    export HDFEOS_GCTPLIB="path_to_HDF-EOS_GCTP_libraries"
    export TIFFINC="path_to_TIFF_include_files"
    export TIFFLIB="path_to_TIFF_libraries"
    export GEOTIFF_INC="path_to_GEOTIFF_include_files"
    export GEOTIFF_LIB="path_to_GEOTIFF_libraries"
    export HDFINC="path_to_HDF4_include_files"
    export HDFLIB="path_to_HDF4_libraries"
    export HDFEOS_INC="path_to_HDFEOS2_include_files"
    export HDFEOS_LIB="path_to_HDFEOS2_libraries"
    export JPEGINC="path_to_JPEG_include_files"
    export JPEGLIB="path_to_JPEG_libraries"
    export XML2INC="path_to_XML2_include_files"
    export XML2LIB="path_to_XML2_libraries"
    export JBIGINC="path_to_JBIG_include_files"
    export JBIGLIB="path_to_JBIG_libraries"
    export ZLIBINC="path_to_ZLIB_include_files"
    export ZLIBLIB="path_to_ZLIB_libraries"    
    export ESPAINC="path_to_format_converter_raw_binary_include_directory"
    export ESPALIB="path_to_format_converter_raw_binary_lib_directory"
  ```
  Define $PREFIX to point to the directory in which you want the executables, static data, etc. to be installed.
  ```
    export PREFIX="path_to_directory_for_format_converter_build_data"
   ```

  * Download the static land/water polygon from http://espa.cr.usgs.gov/downloads/auxiliaries/land_water_poly/land_no_buf.ply.gz. Unzip the file into $PREFIX/static_data.  Define the ESPA_LAND_MASS_POLYGON environment variable to point to the $PREFIX/static_data/land_no_buf.ply file in order to run the land/water mask code.
  ```
    export ESPA_LAND_MASS_POLYGON=$PREFIX/static_data/land_no_buf.ply
  ```
  
* Install ESPA product formatter libraries and tools by downloading the source from Downloads above.  Goto the src/raw\_binary directory and build the source code there. ESPAINC and ESPALIB above refer to the include and lib directories created by building this source code using make followed by make install. The ESPA raw binary conversion tools will be located in the $PREFIX/bin directory.

  Note: if the HDF library was configured and built with szip support, then the user will also need to add "-L$(SZIPLIB) -lsz" at the end of the library defines in the Makefiles.  The user should also add "-I$(SZIPINC)" to the include directory defines in the Makefile.

  Note: on some platforms, the JBIG library may be needed for the XML library support, if it isn't already installed.  If so, then the JBIGLIB environment variable needs to point to the location of the JBIG library.

### Linking these libraries for other applications
The following is an example of how to link these libraries into your
source code. Depending on your needs, some of these libraries may not
be needed for your application or other espa product formatter libraries may need to be added.
```
 -L$(ESPALIB) -l_espa_format_conversion -l_espa_raw_binary -l_espa_common \
 -L$(XML2LIB) -lxml2 \
 -L$(HDFEOS_LIB) -lhdfeos -L$(HDFEOS_GCTPLIB) -lGctp \
 -L$(HDFLIB) -lmfhdf -ldf -L$(JPEGLIB) -ljpeg -L$(JBIGLIB) -ljbig \
 -L$(ZLIBLIB) -lz -lm
```

### Verification Data

### User Manual

### Product Guide


## Release Notes
  * The UTM zone in the MTL file is always positive.  When converting from LPGS to ESPA, if 3 or more of the corners have a negative latitude, then negate the UTM zone so that it reflects the scene in the southern hemisphere.  This will generate a negative UTM zone in the XML file as well as correctly identify the zone as South (vs. North) in the ENVI header files.
  * Updated schema (v1.3) and supporting libraries for schema changes including adding support for cfmask cloud cover and other percentages at the band level, adding support for the scene ID, and removal of the calibrated_nt.
  * Modified to store only the major version number in the schema and XML namespace instead of the major and minor version number.  Any schema changes that require that are non-optional will require a change to the major version.  Minor version changes should allow previously generated XMLs for the same major version to be used within the ESPA applications.
  * Updated Makefiles for ESPA environment and RPMs.
  * Updated to support Albers products in addition to the current UTM and PS.
  * Updated to support the new L1T file naming convention.
  * Updated to support the band quality band (_BQA)
  * Added library routines in level1_libs to clip the TM/ETM+ bands 1-7 and thermal bands to the same extent.  This also updates the band quality band to mark the new fill pixels.  The clip_band_misalignment tool resides under the raw_binary/tools directory and takes the XML file as input.
  * Modified create_l8_angle_bands to be create_angle_bands.  This routine generates solar and sensor angle bands for any Landsat instrument (4-8) and adds them to the XML file.
