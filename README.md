## ESPA-PRODUCT_FORMATTER Version 1.19.0 Release Notes
Release Date: November 2019

The product formatter project contains libraries and tools for working with the ESPA internal file format (raw binary with an XML metadata file). It currently supports Landsat 4-8, Sentinel-2 L1C, and limited MODIS products.

### Support Information
This project is unsupported software provided by the U.S. Geological Survey (USGS) Earth Resources Observation and Science (EROS) Land Satellite Data Systems (LSDS) Project. For questions regarding products produced by this source code, please contact us at custserv@usgs.gov.

### Disclaimer
This software is preliminary or provisional and is subject to revision. It is being provided to meet the need for timely best science. The software has not received final approval by the U.S. Geological Survey (USGS). No warranty, expressed or implied, is made by the USGS or the U.S. Government as to the functionality of the software and related material nor shall the fact of release constitute any such warranty. The software is provided on the condition that neither the USGS nor the U.S. Government shall be held liable for any damages resulting from the authorized or unauthorized use of the software.

### Downloads
espa-product-formatter source code

    git clone https://github.com/USGS-EROS/espa-product-formatter.git

See git tag [version_1.19.0]

### Dependencies
  * GCTP libraries (obtained from the GCTP directory in the HDF-EOS2 source code)
  * TIFF libraries (3.8.2 or most current) -- http://www.libtiff.org/
  * GeoTIFF libraries (1.2.5 or most current) -- https://trac.osgeo.org/geotiff/
  * HDF4 libraries (4.2.5 or most current) -- https://www.hdfgroup.org/ftp/HDF/releases/
  * HDF5 libraries (1.8.13 or most current) -- https://www.hdfgroup.org/ftp/HDF5/releases/
  * HDF-EOS2 libraries (2.18 or most current) -- https://hdfeos.org/software/library.php
  * HDF-EOS5 libraries (1.16 or most current) -- https://hdfeos.org/software/library.php
  * NetCDF libraries (4.1.1 or most current) -- http://www.unidata.ucar.edu/downloads/netcdf/index.jsp
  * CURL libraries (7.48.0 or most current) -- https://curl.haxx.se/download
  * IDN libraries (1.32 or most current) -- ftp://ftp.gnu.org/gnu/libidn
  * JPEG libraries (version 6b) -- http://www.ijg.org/files/
  * ZLIB libraries (version 1.2.8) -- http://zlib.net/
  * XML2 libraries -- ftp://xmlsoft.org/libxml2/
  * JBIG libraries -- http://www.cl.cam.ac.uk/~mgk25/jbigkit/
  * LZMA libraries -- http://www.7-zip.org/sdk.html
  * SZIP libraries -- http://www.compressconsult.com/szip/
  * Openjpeg libraries and tools -- http://www.openjpeg.org/
  * Land/water static polygon -- http://edclpdsftp.cr.usgs.gov/downloads/auxiliaries/land_water_poly/land_no_buf.ply.gz

NOTE: The HDF-EOS2 link currently provides the source for the HDF4, JPEG, and ZLIB libraries in addition to the HDF-EOS2 library.

### Installation
  * Install dependent libraries.  Many of these come standard with the Linux distribution.
  * Add the location of the opj_decompress (from openjpeg) tool to the PATH.
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
    export HDF5INC="path_to_HDF5_include_files"
    export HDF5LIB="path_to_HDF5_libraries"
    export HDFEOS_INC="path_to_HDFEOS2_include_files"
    export HDFEOS_LIB="path_to_HDFEOS2_libraries"
    export HDFEOS5_LIB="path_to_HDFEOS5_libraries"
    export HDFEOS5_INC="path_to_HDFEOS5_include_files"
    export NCDF4INC="path_to_NETCDF_include_files"
    export NCDF4LIB="path_to_NETCDF_libraries"
    export JPEGINC="path_to_JPEG_include_files"
    export JPEGLIB="path_to_JPEG_libraries"
    export XML2INC="path_to_XML2_include_files"
    export XML2LIB="path_to_XML2_libraries"
    export JBIGINC="path_to_JBIG_include_files"
    export JBIGLIB="path_to_JBIG_libraries"
    export ZLIBINC="path_to_ZLIB_include_files"
    export ZLIBLIB="path_to_ZLIB_libraries"    
    export SZIPINC="path_to_SZIP_include_files"
    export SZIPLIB="path_to_SZIP_libraries"    
    export CURLINC="path_to_CURL_include_files"
    export CURLLIB="path_to_CURL_libraries"
    export LZMAINC="path_to_LZMA_include_files"
    export LZMALIB="path_to_LZMA_libraries"
    export IDNINC="path_to_IDN_include_files"
    export IDNLIB="path_to_IDN_libraries"
    export ESPAINC="path_to_format_converter_raw_binary_include_directory"
    export ESPALIB="path_to_format_converter_raw_binary_lib_directory"
  ```
  Define $PREFIX to point to the directory in which you want the executables, static data, etc. to be installed.
  ```
    export PREFIX="path_to_directory_for_format_converter_build_data"
   ```

  * Download the static land/water polygon from http://edclpdsftp.cr.usgs.gov/downloads/auxiliaries/land_water_poly/land_no_buf.ply.gz. Unzip the file into $PREFIX/static_data.  Define the ESPA_LAND_MASS_POLYGON environment variable to point to the $PREFIX/static_data/land_no_buf.ply file in order to run the land/water mask code.
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
 -L$(ZLIBLIB) -lz \
 -L$(LZMALIB) -llzma -lm
```

```
 -L$(ESPALIB) -l_espa_format_conversion -l_espa_raw_binary -l_espa_common \
 -L$(XML2LIB) -lxml2 \
 -L$(HDFEOS5_LIB) -lhe5_hdfeos -lGctp \
 -L$(HDF5LIB) -lhdf5 -lhdf5_hl \
 -L$(ZLIBLIB) -lz \
 -L$(LZMALIB) -llzma -lm
```

### Verification Data

### User Manual

### Product Guide


## Release Notes
  * Cleaned up some warning codes flagged after migrating to a newer system.
  * Fixed a bug in parse_sentinel_metadata.c.  Prodtype is a character
    pointer and not an array, therefore using the size of the pointer is not
    valid when used as an argument in sizeof.
  * Added a script for unpackaging the Sentinel-2 product bundles so they are
    in the proper format for ingesting via convert_sentinel_to_espa.
  * Updated the python metadata library to use the version 2.2 schema. 
  * Modified the unpackage script to support both old and new Sentinel-2
    package formats.
  * Developed a filename convention for ESPA's version of the Sentinel-2
    products and the conversion of Sentinel to ESPA writes the output bands
    using this filename convention.
