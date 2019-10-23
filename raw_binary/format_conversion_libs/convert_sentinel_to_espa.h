/*****************************************************************************
FILE: convert_sentinel_to_espa.h
  
PURPOSE: Contains defines and prototypes to read supported Sentinel files,
create the XML metadata file, and convert from JPEG2000 to the raw binary file
format.

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

LICENSE TYPE:  NASA Open Source Agreement Version 1.3

NOTES:
*****************************************************************************/

#ifndef CONVERT_SENTINEL_TO_ESPA_H
#define CONVERT_SENTINEL_TO_ESPA_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "error_handler.h"
#include "espa_metadata.h"
#include "write_metadata.h"
#include "envi_header.h"
#include "parse_sentinel_metadata.h"

/* Defines */
/* number of Sentinel bands in an L1C product; ignore TCI */
#define NUM_SENTINEL_BANDS 13

/* Prototypes */
int convert_sentinel_to_espa
(
    bool del_src             /* I: should the source .jp2 files be removed
                                   after conversion? */
);

#endif
