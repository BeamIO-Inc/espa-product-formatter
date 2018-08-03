/*****************************************************************************
FILE: convert_viirs_to_espa.h
  
PURPOSE: Contains defines and prototypes to read supported VIIRS files, create
the XML metadata file, and convert from HDF-EOS to raw binary file format.

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

LICENSE TYPE:  NASA Open Source Agreement Version 1.3

NOTES:
*****************************************************************************/

#ifndef CONVERT_VIIRS_TO_ESPA_H
#define CONVERT_VIIRS_TO_ESPA_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <hdf5.h>
#include <HE5_HdfEosDef.h>
#include "error_handler.h"
#include "espa_metadata.h"
#include "espa_geoloc.h"
#include "raw_binary_io.h"
#include "write_metadata.h"
#include "envi_header.h"

/* Defines */
/* maximum number of VIIRS bands/SDSs in a file -- only supporting the three
   500m image bands for now */
#define MAX_VIIRS_BANDS 3

/* maximum number of dimensions for each SDS, even though we will only support
   2D - still need to be able to read the dimensions from the SDS */
#define MAX_VIIRS_DIMS 2

/* Prototypes */
int read_viirs_hdf
(
    char *viirs_hdf_name,            /* I: name of VIIRS file to be read */
    Espa_internal_meta_t *metadata   /* I/O: input metadata structure to be
                                           populated from the VIIRS file */
);

int convert_hdf_to_img
(
    char *viirs_hdf_name,      /* I: name of VIIRS file to be processed */
    Espa_internal_meta_t *xml_metadata /* I: metadata structure for HDF file */
);

int convert_viirs_to_espa
(
    char *viirs_hdf_file,  /* I: input VIIRS HDF filename */
    char *espa_xml_file,   /* I: output ESPA XML metadata filename */
    bool del_src           /* I: should the source .tif files be removed after
                                 conversion? */
);

#endif
