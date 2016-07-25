/*****************************************************************************
FILE: convert_espa_to_netcdf.h
  
PURPOSE: Contains defines and prototypes to read the ESPA XML metadata file
and imagery, and convert from raw binary to netCDF file format.

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

LICENSE TYPE:  NASA Open Source Agreement Version 1.3

NOTES:
*****************************************************************************/

#ifndef CONVERT_ESPA_TO_NETCDF_H
#define CONVERT_ESPA_TO_NETCDF_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netcdf.h>
#include "error_handler.h"
#include "espa_metadata.h"
#include "parse_metadata.h"
#include "write_metadata.h"
#include "raw_binary_io.h"

/* Define the compression parameters - use data shuffling (NC_SUFFLE),
   turn on compression, and use a mid-level compression */
#define SHUFFLE NC_SHUFFLE
#define DEFLATE 1
#define DEFLATE_LEVEL 4

/* Chunking cache parameters - cache size of 1 GB. Number of cache elements
 *    should be over 1000 and a prime number. */
#define CACHE_SIZE 1000000000
#define CACHE_NELEMS 1009
#define CACHE_PREEMPTION 0.75

/* Handle netCDF errors by printing an error message */
#define netCDF_ERR(e) {printf("netCDF error: %s\n", nc_strerror(e));}

/* Constant values for NetCDF variables */
#define XDIM_NAME "x"
#define YDIM_NAME "y"

/* Prototypes */
int write_global_attributes
(
    int ncid,                /* I: netCDF file ID to write attributes */
    Espa_internal_meta_t *xml_metadata  /* I: pointer to metadata structure */
);

int create_netcdf_metadata
(
    char *netcdf_file,     /* I: output netCDF filename */
    Espa_internal_meta_t *xml_metadata, /* I: XML metadata structure */
    bool del_src,          /* I: should the source files be removed after
                                 conversion? */
    bool no_compression    /* I: use compression for the NetCDF output file? */
);

int convert_espa_to_netcdf
(
    char *espa_xml_file,   /* I: input ESPA XML metadata filename */
    char *netcdf_file,     /* I: output netCDF filename */
    bool del_src,          /* I: should the source files be removed after
                                 conversion? */
    bool no_compression    /* I: use compression for the NetCDF output file? */
);

#endif
