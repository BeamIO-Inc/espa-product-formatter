/*****************************************************************************
FILE: generate_date_bands.c
  
PURPOSE: Contains defines and prototypes to generate a date/year band.

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

LICENSE TYPE:  NASA Open Source Agreement Version 1.3

NOTES:
  1. The XML metadata format written via this library follows the ESPA internal
     metadata format found in ESPA Raw Binary Format v1.0.doc.  The schema for
     the ESPA internal metadata format is available at
     http://espa.cr.usgs.gov/schema/espa_internal_metadata_v1_0.xsd.
*****************************************************************************/
#include <unistd.h>
#include "generate_date_bands.h"

/******************************************************************************
MODULE:  generate_doy

PURPOSE: Creates the DOY given the year, month, and day.

Type = int
Value           Description
-----           -----------
-1              Error creating the doy
1-366           Success creating the doy

NOTES:
  1. It is assumed the month and day values have been validated and are within
     1-12 for the month and 1-31 for the day.
******************************************************************************/
int generate_doy
(
    int year,     /* I: Year of date to be converted to DOY */
    int month,    /* I: Month to be converted to DOY (1-12) */
    int day       /* I: Day to be converted to DOY (1-31) */
)
{
    int i;               /* looping variable */
    int doy = 0;         /* DOY value */
    bool leap = false;   /* is this a leap year */
    const int month_len[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    /* Determine if this is a leap year */
    leap = (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0);

    /* Sum the month values through the previous month (which is 1-based) */
    for (i = 0; i < month-1; i++)
        doy += month_len[i];

    /* Add the day value */
    doy += day;

    /* Add one for a leap year if the month is past February */
    if (leap && month > 2)
        doy++;

    return doy;
}


/******************************************************************************
MODULE:  generate_date_bands

PURPOSE: Creates the date bands for the current scene.  These include a
DOY-year band, DOY band, and a year band.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error creating the date bands
SUCCESS         Successfully created the date bands

NOTES:
  1. The combined date-year band will be an unsigned 32-bit integer in the form
     of YYYYDOY (example 2015232 for Aug. 20, 2015).
  2. The individual date and year bands will be unsigned 16-bit integers.
  3. The number of lines and samples is pulled from band1 (LPGS level 1 product)     in the XML file.
******************************************************************************/
int generate_date_bands
(
    Espa_internal_meta_t *xml_meta,  /* I: input XML metadata */
    unsigned int **jdate_band,       /* O: pointer to date buffer with
                                           year*1000 + DOY */
    unsigned short **doy_band,       /* O: pointer to DOY buffer */
    unsigned short **year_band,      /* O: pointer to year buffer */
    int *nlines,                     /* O: number of lines in date bands */
    int *nsamps                      /* O: number of samples in date bands */
)
{
    char FUNC_NAME[] = "generate_date_bands";  /* function name */
    char errmsg[STR_SIZE];      /* error message */
    char year_str[5];           /* string for the year */
    char month_str[3];          /* string for the month */
    char day_str[3];            /* string for the day */
    int i;                      /* looping variable */
    int year, month, day;       /* year, month, and day from the acquisition
                                   date */
    int doy;                    /* day of year */
    int refl_indx = -9;         /* band index in XML file for the
                                   representative reflectance band */
    Espa_global_meta_t *gmeta = &xml_meta->global;
                                      /* pointer to global metadata structure */
    Espa_band_meta_t *bmeta = NULL;   /* pointer to band metadata structure */

    /* Pull the year, month, and day from the acquisition date in the XML
       metadata (Example YYYY-MM-DD) */
    strncpy (year_str, gmeta->acquisition_date, 4);
    year_str[4] = '\0';
    year = atoi (year_str);
    if (year < 1970 || year > 9999)
    {
        sprintf (errmsg, "Invalid year value from the acquisition date: %d. "
            "Should be between 1970 and 9999.\n", year);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    strncpy (month_str, &gmeta->acquisition_date[5], 2);
    month_str[2] = '\0';
    month = atoi (month_str);
    if (month < 1 || month > 12)
    {
        sprintf (errmsg, "Invalid month value from the acquisition date: %d. "
            "Should be between 1 and 12.\n", month);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    strncpy (day_str, &gmeta->acquisition_date[8], 2);
    day_str[2] = '\0';
    day = atoi (day_str);
    if (day < 1 || day > 31)
    {
        sprintf (errmsg, "Invalid day value from the acquisition date: %d. "
            "Should be between 1 and 31.\n", day);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }
     
    /* Use band 1 as the representative band in the XML */
    for (i = 0; i < xml_meta->nbands; i++)
    {
        if (!strcmp (xml_meta->band[i].name, "b1"))
        {
            /* this is the index we'll use for reflectance band info */
            refl_indx = i;
            break;
        }
    }

    /* Determine the day of year */
    doy = generate_doy (year, month, day);
    if (doy < 1 || doy > 366)
    {
        sprintf (errmsg, "Invalid DOY value from the acquisition date: %d. "
            "Should be between 1 and 366.\n", doy);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }
     
    /* Make sure the representative band was found in the XML file */
    if (refl_indx == -9)
    {
        sprintf (errmsg, "Band 1 (b1) was not found in the XML file");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }
    bmeta = &xml_meta->band[refl_indx];
    *nlines = bmeta->nlines;
    *nsamps = bmeta->nsamps;

    /* Allocate memory for the date, DOY, and year bands */
    *jdate_band = calloc (*nlines * *nsamps, sizeof (unsigned int));
    if (*jdate_band == NULL)
    {
        sprintf (errmsg, "Error allocating memory for the date/year band");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    *doy_band = calloc (*nlines * *nsamps, sizeof (unsigned short));
    if (*doy_band == NULL)
    {
        sprintf (errmsg, "Error allocating memory for the DOY band");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    *year_band = calloc (*nlines * *nsamps,
        sizeof (unsigned short));
    if (*year_band == NULL)
    {
        sprintf (errmsg, "Error allocating memory for the year band");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    printf ("INFO: acquisition_date is %s\n", gmeta->acquisition_date);
    printf ("INFO: year-month-day is %d-%d-%d\n", year, month, day);
    printf ("INFO: DOY is %d\n", doy);

    /* Loop through each pixel and assign the date information to all of the
       pixels */
    for (i = 0; i < *nlines * *nsamps; i++)
    {
        (*jdate_band)[i] = (unsigned int) (year * 1000 + doy);
        (*doy_band)[i] = (unsigned short) doy;
        (*year_band)[i] = (unsigned short) year;
    }

    /* Successful conversion */
    return (SUCCESS);
}

