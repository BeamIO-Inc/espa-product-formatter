#include <stdbool.h>
#include "doy_to_month_day.h"

/******************************************************************************
MODULE:  doy_to_month_day

PURPOSE: Convert the DOY to month and day.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error converting from DOY to month and day
SUCCESS         Successfully converted from the DOY to the month and day

NOTES:
******************************************************************************/
int doy_to_month_day
(
    int year,            /* I: year of the DOY to be converted */
    int doy,             /* I: DOY to be converted (one-based) */
    int *month,          /* O: month of the DOY (one-based) */
    int *day             /* O: day of the DOY (one-based) */
)
{
    char FUNC_NAME[] = "doy_to_month_day";  /* function name */
    char errmsg[STR_SIZE];    /* error message */
    bool leap;                /* is this a leap year? */
    int i;                    /* looping variable */

    /* number of days in each month (for leap year) */
    int nday_lp[NMONTHS] =
        {31, 29, 31, 30,  31,  30,  31,  31,  30,  31,  30,  31};

    /* starting DOY for each month (for leap year) */
    int idoy_lp[NMONTHS] =
        { 1, 32, 61, 92, 122, 153, 183, 214, 245, 275, 306, 336};

    /* number of days in each month (with Feb being a leap year) */
    int nday[NMONTHS] =
        {31, 28, 31, 30,  31,  30,  31,  31,  30,  31,  30,  31};

    /* starting DOY for each month */
    int idoy[NMONTHS] =
        { 1, 32, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};

    /* Is this a leap year? */
    leap = (bool) (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));

    /* Validate the DOY */
    if (doy <= 0 || doy > 366)
    {
        sprintf (errmsg, "Invalid DOY value (1-366): %d", doy);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Determine which month the DOY falls in */
    *month = 0;
    if (leap)
    {  /* leap year -- start with February */
        for (i = 1; i < NMONTHS; i++)
        {
            if (idoy_lp[i] > doy)
            {
                *month = i;
                *day = doy - idoy_lp[i-1] + 1;
                break;
            }
        }

        /* if the month isn't set, then it's a December scene */
        if (*month == 0)
        {
            *month = NMONTHS;
            *day = doy - idoy_lp[NMONTHS-1] + 1;
        }
    }
    else
    {  /* non leap year -- start with February */
        for (i = 1; i < NMONTHS; i++)
        {
            if (idoy[i] > doy)
            {
                *month = i;
                *day = doy - idoy[i-1] + 1;
                break;
            }
        }

        /* if the month isn't set, then it's a December scene */
        if (*month == 0)
        {
            *month = NMONTHS;
            *day = doy - idoy[NMONTHS-1] + 1;
        }
    }

    /* Validate the month and day */
    if (*month < 1 || *month > NMONTHS)
    {
        sprintf (errmsg, "Invalid month: %d", *month);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }
    if (leap)
    {  /* leap year */
        if (*day < 1 || *day > nday_lp[(*month)-1])
        {
            sprintf (errmsg, "Invalid day: %d-%d-%d", year, *month, *day);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }
    else
    {  /* non leap year */
        if (*day < 1 || *day > nday[(*month)-1])
        {
            sprintf (errmsg, "Invalid day: %d-%d-%d", year, *month, *day);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }

    /* Successful conversion */
    return (SUCCESS);
}
