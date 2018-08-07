/*****************************************************************************
FILE: doy_to_month_day
  
PURPOSE: Contains defines and prototypes for handling DOY to month/day
conversions.

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

LICENSE TYPE:  NASA Open Source Agreement Version 1.3

NOTES:
*****************************************************************************/

#ifndef DOY_TO_MONTH_H
#define DOY_TO_MONTH_H

#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include "espa_common.h"
#include "error_handler.h"

/* Constants */
#ifndef NMONTHS
#define NMONTHS 12
#endif

/* Prototypes */
int doy_to_month_day
(
    int year,            /* I: year of the DOY to be converted */
    int doy,             /* I: DOY to be converted */
    int *month,          /* O: month of the DOY */
    int *day             /* O: day of the DOY */
);

#endif
