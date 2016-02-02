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

/* Prototypes */
int clip_band_misalignment
(
    char *espa_xml_file    /* I: input ESPA XML metadata filename */
);

#endif
