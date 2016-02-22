/*****************************************************************************
FILE: convert_espa_to_raw_binary_bip.h
  
PURPOSE: Contains defines and prototypes to read the ESPA XML metadata file
and imagery, and convert from raw binary to raw binary BIP file format.

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

LICENSE TYPE:  NASA Open Source Agreement Version 1.3

NOTES:
*****************************************************************************/

#ifndef CONVERT_ESPA_TO_RAW_BINARY_BIP_H
#define CONVERT_ESPA_TO_RAW_BINARY_BIP_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "xtiffio.h"
#include "error_handler.h"
#include "espa_metadata.h"
#include "parse_metadata.h"
#include "write_metadata.h"
#include "raw_binary_io.h"
#include "envi_header.h"

/* Defines */

/* Prototypes */
int convert_espa_to_raw_binary_bip
(
    char *espa_xml_file,   /* I: input ESPA XML metadata filename */
    char *bip_file,        /* I: output BIP filename */
    bool convert_qa,       /* I: should the QA bands (uint8) be converted to
                                 the data type of band 1 (if QA bands are of
                                 a different data type)? */
    bool del_src           /* I: should the source files be removed after
                                 conversion? */
);

#endif
