/*****************************************************************************
FILE: parse_sentinel_metadata.h
  
PURPOSE: Contains prototypes for parsing the Sentinel metadata and populating
the ESPA internal metadata

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

LICENSE TYPE:  NASA Open Source Agreement Version 1.3

NOTES:
*****************************************************************************/

#ifndef PARSE_SENTINEL_METADATA_H
#define PARSE_SENTINEL_METADATA_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlschemastypes.h>
#include "error_handler.h"
#include "espa_metadata.h"

/* number of sentinel resolutions - 10m, 20m, 60m */
#define NUM_SENTINEL_RES 3

int parse_sentinel_tile_metadata
(
    char *metafile,                 /* I: input Sentinel tile metadata file */
    Espa_internal_meta_t *metadata  /* I: input metadata structure which has
                                          been initialized via
                                          init_metadata_struct */
);

int parse_sentinel_product_metadata
(
    char *metafile,                 /* I: Sentinel product metadata file */
    Espa_internal_meta_t *metadata, /* I/O: input metadata structure which has
                                          been initialized via
                                          init_metadata_struct */
    char *prodtype,                 /* O: product type for all bands */
    char *proc_ver,                 /* O: processing version for all bands */
    char *l1_filename,              /* O: initial level-1 filename to be used
                                          for all band names */
    float *scale_factor             /* O: scale factor for all bands */
);

#endif
