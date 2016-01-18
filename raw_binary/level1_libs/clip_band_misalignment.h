/*****************************************************************************
FILE: clip_band_misalignment.h
  
PURPOSE: Contains defines and prototypes to read the ESPA XML metadata file
and imagery, and convert from raw binary to HDF file format.

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

LICENSE TYPE:  NASA Open Source Agreement Version 1.3

HISTORY:
Date         Programmer       Reason
----------   --------------   -------------------------------------
1/6/2014     Gail Schmidt     Original development

NOTES:
*****************************************************************************/

#ifndef CLIP_BAND_MISALIGNMENT_H
#define CLIP_BAND_MISALIGNMENT_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "error_handler.h"
#include "espa_metadata.h"
#include "parse_metadata.h"
#include "raw_binary_io.h"

/* Defines */
#define NBAND_OPTIONS 9
typedef unsigned char uint8;
typedef unsigned short uint16;

/* Prototypes */
int clip_band_misalignment
(
    char *espa_xml_file    /* I: input ESPA XML metadata filename */
);

#endif
