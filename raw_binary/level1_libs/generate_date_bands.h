/*****************************************************************************
FILE: generate_date_bands
  
PURPOSE: Contains defines and prototypes to generate a date/year band.

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

LICENSE TYPE:  NASA Open Source Agreement Version 1.3

NOTES:
*****************************************************************************/

#ifndef GENERATE_DATE_BAND_H
#define GENERATE_DATE_BAND_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "error_handler.h"
#include "espa_metadata.h"
#include "parse_metadata.h"
#include "write_metadata.h"
#include "raw_binary_io.h"
#include "envi_header.h"

/* Defines */

/* Prototypes */
int generate_date_bands
(
    Espa_internal_meta_t *xml_meta,  /* I: input XML metadata */
    unsigned int **jdate_band,       /* O: pointer to date buffer with
                                           year*1000 + DOY */
    unsigned short **doy_band,       /* O: pointer to DOY buffer */
    unsigned short **year_band,      /* O: pointer to year buffer */
    int *nlines,                     /* O: number of lines in date bands */
    int *nsamps                      /* O: number of samples in date bands */
);

#endif
