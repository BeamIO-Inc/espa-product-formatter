/*****************************************************************************
FILE: clip_band_misalignment.h
  
PURPOSE: Contains defines and prototypes for handling the band clipping.

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

LICENSE TYPE:  NASA Open Source Agreement Version 1.3

NOTES:
*****************************************************************************/

#ifndef CLIP_BAND_MISALIGNMENT_H
#define CLIP_BAND_MISALIGNMENT_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "error_handler.h"
#include "espa_metadata.h"
#include "parse_metadata.h"
#include "raw_binary_io.h"

/* Defines */
#define NBAND_OPTIONS 9
#define NBAND_OPTIONS_L89 11
#define LEVEL1_FILL 0
#define BQA_FILL 1

/* Prototypes */
int clip_band_misalignment
(
    Espa_internal_meta_t *xml_metadata  /* I: XML metadata structure populated
                                              from an ESPA XML file */
);

int clip_band_misalignment_landsat89
(
    Espa_internal_meta_t *xml_metadata  /* I: XML metadata structure populated
                                              from an ESPA XML file */
);

#endif
