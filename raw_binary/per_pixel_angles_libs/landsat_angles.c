/* Standard Library Includes */
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

/* IAS Library Includes */
#include "gxx_angle_gen_distro.h"
#include "xxx_Band.h"
#include "xxx_LogStatus.h"
#include "xxx_Sensor.h"
#include "gxx_sensor.h"

/* Local Includes */
#include "landsat_angles.h"

/* Local defines */
#define SCALED_R2D 4500.0 / atan(1.0)

/**************************************************************************
NAME: landsat_per_pixel_angles

PURPOSE: Calculates satellite viewing and solar illumination zenith and 
azimuth angles for each L1T pixel (with optional sub-sampling), for the
specified list of bands.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           An error occurred generating the per-pixel solar and/or
                view angles
SUCCESS         Angle band generation was successful

NOTES:
  1. The band pointers for solar zenith/azimuth and satellite zenith/azimuth
     will have space allocated for the entire band
     (nlines * nsamps * sizeof (short)), unless NULL is passed in for the
     address.  Thus these pointers are an array of NBANDS pointers, where each
     pointer contains the angle values for the entire band as a 1D product.
  2. If any of the above pointers are NULL, then those per-pixel angles will
     not be calculated.
  3. It will be up to the calling routine to delete the memory allocated
     for these per band angle arrays.
  4. The angles that are returned are in degrees and have been scaled by 100.
***************************************************************************/
int landsat_per_pixel_angles
(
    char *angle_coeff_name, /* I: Angle coefficient filename */
    int sub_sample,         /* I: Subsample factor used when calculating the
                                  angles (1=full resolution). OW take every Nth
                                  sample from the line, where N=sub_sample */
    char *band_list,        /* I: Band list used to calculate angles for.
                                  "ALL" - defaults to all bands 1-8 / 1-7.
                                  Must be comma separated with no spaces in
                                  between.  Example: 1,2,3,4,5,61,62,7,8
                                  The solar/sat_zenith/azimuth arrays will have
                                  angles processed for these bands */
    short *solar_zenith[L7_NBANDS],  /* O: Array of pointers for the solar
                                           zenith angle array, one per band
                                           (if NULL, don't process), degrees
                                            scaled by 100 */
    short *solar_azimuth[L7_NBANDS], /* O: Array of pointers for the solar
                                           azimuth angle array, one per band
                                           (if NULL, don't process), degrees
                                            scaled by 100 */
    short *sat_zenith[L7_NBANDS],    /* O: Array of pointers for the satellite
                                           zenith angle array, one per band
                                           (if NULL, don't process), degrees
                                            scaled by 100 */
    short *sat_azimuth[L7_NBANDS],   /* O: Array of pointers for the satellite
                                           azimuth angle array, one per band
                                           (if NULL, don't process), degrees
                                            scaled by 100 */
    int nlines[L7_NBANDS],  /* O: Number of lines for each band, based on the
                                  subsample factor */
    int nsamps[L7_NBANDS]   /* O: Number of samples for each band, based on the
                                  subsample factor */
)
{
    double scan_buffer = 0.0;             /* Scan buffering -- not used */
    gxx_angle_gen_metadata_TYPE metadata; /* Angle metadata structure */
    char sensor_type[STRLEN];             /* Sensor Type */
    char msg[STRLEN];                     /* Error messages */
    char band[STRLEN];                    /* Band number in the band_list */
    bool done;                            /* Done processing band_list? */
    bool band_found;                      /* Was the current band found in the
                                             user-specified band_list */
    int i;                                /* Looping variable for band_list */
    int band_count = 0;                   /* Count of bands in band_list */
    int band_index;                       /* Loop variable for bands */
    int start_char_loc;                   /* Starting character of the current
                                             band in band_list */
    int end_char_loc;                     /* Ending character of the current
                                             band in band_list */
    int bnd_list[L7_NBANDS];              /* List of bands as integers */
    int l45_bands[] = {1, 2, 3, 4, 5, 6, 7}; /* Landsat 4-5 band numbers */
    int l7_bands[] = {1, 2, 3, 4, 5, 61, 62, 7, 8}; /* Landsat 7 band numbers */

    /* Make sure there is something to process */
    if (solar_zenith == NULL && solar_azimuth == NULL &&
        sat_zenith == NULL && sat_azimuth == NULL)
    {
        xxx_LogStatus(PROGRAM, __FILE__, __LINE__,
            "Solar and Satellite zenith/azimuth pointer arrays are NULL. "
            "Nothing to process.");
        return ERROR;
    }

    /* If one solar array is specified, then both must be specified */
    if ((solar_zenith != NULL && solar_azimuth == NULL) ||
        (solar_zenith == NULL && solar_azimuth != NULL))
    {
        xxx_LogStatus(PROGRAM, __FILE__, __LINE__,
            "Both solar zenith/azimuth pointer arrays are must be set to "
            "non-NULL in order to process.");
        return ERROR;
    }

    /* If one sat array is specified, then both must be specified */
    if ((sat_zenith != NULL && sat_azimuth == NULL) ||
        (sat_zenith == NULL && sat_azimuth != NULL))
    {
        xxx_LogStatus(PROGRAM, __FILE__, __LINE__,
            "Both satellite zenith/azimuth pointer arrays are must be set to "
            "non-NULL in order to process.");
        return ERROR;
    }

    /* Read the angle coefficient file. */
    if (gxx_angle_gen_read_ang(angle_coeff_name, &metadata))
    {
        xxx_LogStatus(PROGRAM, __FILE__, __LINE__,
            "Error reading the angle coefficient file.");
        return ERROR;
    }

    /* Get the sat/sensor from the angle coefficient file. */
    strcpy(sensor_type, metadata.spacecraft_id);
    if (xxx_initialize_sensor_type(sensor_type) == IAS_SENSOR_UNKNOWN)
    {
        sprintf(msg, "Invalid SENSOR_TYPE string: %s", sensor_type);
        xxx_LogStatus(PROGRAM, __FILE__, __LINE__, msg);
        return ERROR;
    }

    /* Look at the user-specified band list, and initialize to all bands if
       specified */
    if (!strcmp (band_list, "ALL"))
    {
        if (!strcmp (sensor_type, "L7_ETM"))
        {
            band_count = L7_NBANDS;
            for (band_index = 0; band_index < L7_NBANDS; band_index++)
                bnd_list[band_index] = l7_bands[band_index];
        }
        else
        {
            band_count = L45_NBANDS;
            for (band_index = 0; band_index < L45_NBANDS; band_index++)
                bnd_list[band_index] = l45_bands[band_index];
        }
    }
    else
    {
        /* Parse the band_list string */
        band_count = 0;
        i = 0;
        start_char_loc = 0;
        end_char_loc = 0;
        done = false;
        while (!done)
        {
            /* Find the next comma or the end-of-string */
            while (band_list[i] != ',' && band_list[i] != '\0')
                i++;

            /* End of the band number is the previous location */
            end_char_loc = i-1;

            /* Copy these characters to their string */
            strncpy (band, &band_list[start_char_loc],
                end_char_loc - start_char_loc + 1);
            bnd_list[band_count++] = atoi(band);

            /* Set up the pointers for the next band, if we aren't done */
            if (band_list[i] == '\0')
                done = true;
            else
            {
                start_char_loc = i+1;
                i++;  /* move off the current comma */
            }
        }
    }


    /* Process the angles for each band */
    for (band_index = 0; band_index < (int)metadata.num_bands; band_index++)
    {
        int num_lines, num_samps;       /* Number of lines and samples */
        short *sat_zn = NULL;           /* Satellite zenith angle */
        short *sat_az = NULL;           /* Satellite azimuth angle */
        short *sun_zn = NULL;           /* Solar zenith angle */
        short *sun_az = NULL;           /* Solar azimuth angle */
        double satang[2];               /* Satellite viewing angles */
        double sunang[2];               /* Solar viewing angles */
        int index;                      /* Output sample counter */
        int line;                       /* Line index */
        int samp;                       /* Sample index */
        int outside_image;              /* Return was outside image */
        int tmp_percent;                /* Current percentage for printing
                                           status */
        int curr_tmp_percent;           /* Percentage for current line */
        double *height = NULL;          /* Height to evaluate */
        size_t angle_size;              /* Number of elements in angle array */

        /* Check if this band is in the user-specified list of bands to be
           processed */
        band_found = false;
        for (i = 0; i < band_count; i++)
        {
            if (bnd_list[i] ==
            xxx_get_user_band(metadata.band_metadata[band_index].band_number))
            {
                band_found = true;
                printf("Processing band %d ... ", bnd_list[i]);
                break;
            }
        }
        if (!band_found)
            continue;

        /* Calculate size of subsampled output image, and store the info */
        num_lines = (metadata.band_metadata[band_index].l1t_lines - 1) /
                     sub_sample + 1;
        num_samps = (metadata.band_metadata[band_index].l1t_samps - 1) /
                     sub_sample + 1;
        nlines[band_index] = num_lines;
        nsamps[band_index] = num_samps;

        /* Allocate the output buffers */
        angle_size = num_lines*num_samps*sizeof(short);
        if (sat_zenith)
        {
            sat_zenith[band_index] = malloc(angle_size);
            sat_zn = sat_zenith[band_index];
            if (sat_zn == NULL)
            {
                xxx_LogStatus(PROGRAM, __FILE__, __LINE__, "Error allocating "
                    "satellite zenith angle array.");
                gxx_angle_gen_free(&metadata);
                return ERROR;
            }
        }

        if (sat_azimuth)
        {
            sat_azimuth[band_index] = malloc(angle_size);
            sat_az = sat_azimuth[band_index];
            if (sat_az == NULL)
            {
                xxx_LogStatus(PROGRAM, __FILE__, __LINE__, "Error allocating "
                    "satellite azimuth angle array.");
                gxx_angle_gen_free(&metadata);
                return ERROR;
            }
        }

        if (solar_zenith)
        {
            solar_zenith[band_index] = malloc(angle_size);
            sun_zn = solar_zenith[band_index];
            if (sun_zn == NULL)
            {
                xxx_LogStatus(PROGRAM, __FILE__, __LINE__, "Error allocating "
                    "solar zenith angle array.");
                gxx_angle_gen_free(&metadata);
                return ERROR;
            }
        }

        if (solar_azimuth)
        {
            solar_azimuth[band_index] = malloc(angle_size);
            sun_az = solar_azimuth[band_index];
            if (sun_az == NULL)
            {
                xxx_LogStatus(PROGRAM, __FILE__, __LINE__, "Error allocating "
                    "solar azimuth angle array.");
                gxx_angle_gen_free(&metadata);
                return ERROR;
            }
        }

        /* Loop through the L1T lines and samples */
        tmp_percent = 0;
        index = 0;
        printf ("0%% ");
        for (line = 0; line < metadata.band_metadata[band_index].l1t_lines; 
             line += sub_sample)
        {
            /* update status? */
            curr_tmp_percent = 100 * line / num_lines;
            if (curr_tmp_percent > tmp_percent)
            {
                tmp_percent = curr_tmp_percent;
                if (tmp_percent % 10 == 0)
                {
                    printf ("%d%% ", tmp_percent);
                    fflush (stdout);
                }
            }

            for (samp = 0; samp < metadata.band_metadata[band_index].l1t_samps; 
                 samp += sub_sample, index++)
            {
                /* Process satellite angles */
                if (sat_az && sat_zn)
                {
                    if (gxx_angle_gen_calculate_angles_rpc(&metadata,
                        (double)line, (double)samp, height, band_index,
                        scan_buffer, sub_sample, GXX_ANGLE_GEN_SATELLITE,
                        &outside_image, satang) != SUCCESS)
                    {
                        sprintf(msg,"Error evaluating view angles in band %d.",
                                metadata.band_metadata[band_index].band_number);
                        xxx_LogStatus(PROGRAM, __FILE__, __LINE__, msg);
                        return ERROR;
                    }

                    /* Calculate satellite angles from vector */
                    sat_zn[index] = (short)floor(SCALED_R2D*satang[0] + 0.5);
                    sat_az[index] = (short)floor(SCALED_R2D*satang[1] + 0.5);
                }

                /* Process solar angles */
                if (sun_az && sun_zn)
                {
                    if (gxx_angle_gen_calculate_angles_rpc(&metadata,
                        (double)line, (double)samp, height, band_index,
                        scan_buffer, sub_sample, GXX_ANGLE_GEN_SOLAR,
                        &outside_image, sunang) != SUCCESS)
                    {
                        sprintf(msg,"Error evaluating solar angles in band %d.",
                                metadata.band_metadata[band_index].band_number);
                        xxx_LogStatus(PROGRAM, __FILE__, __LINE__, msg);
                        return ERROR;
                    }

                    /* Calculate solar angles from vector */
                    sun_zn[index] = (short)floor(SCALED_R2D*sunang[0] + 0.5);
                    sun_az[index] = (short)floor(SCALED_R2D*sunang[1] + 0.5);
                }
            }  /* for samp */
        }  /* for line */

        /* update status */
        printf ("100%%\n");
        fflush (stdout);
    }  /* for band_index */

    /* Free the ephemeris structure */
    gxx_angle_gen_free(&metadata);
    return SUCCESS;
}


/******************************************************************************
MODULE:  init_per_pixel_angles

PURPOSE:  Initializes the solar and satellite angle arrays to NULL, for each
band in the array.  This allows the free_per_pixel_angles to work properly.

RETURN VALUE: N/A
******************************************************************************/
void init_per_pixel_angles
(
    short *solar_zenith[L7_NBANDS],  /* O: Array of pointers for the solar
                                           zenith angle array, one per band
                                           (if NULL, don't process) */
    short *solar_azimuth[L7_NBANDS], /* O: Array of pointers for the solar
                                           azimuth angle array, one per band
                                           (if NULL, don't process) */
    short *sat_zenith[L7_NBANDS],    /* O: Array of pointers for the satellite
                                           zenith angle array, one per band
                                           (if NULL, don't process) */
    short *sat_azimuth[L7_NBANDS]    /* O: Array of pointers for the satellite
                                           azimuth angle array, one per band
                                           (if NULL, don't process) */
)
{
    int i;   /* looping variable */

    /* Initialize the pointers to NULL for each band, if that array pointer
       is not NULL */
    for (i = 0; i < L7_NBANDS; i++)
    {
        if (solar_zenith)
            solar_zenith[i] = NULL;
        if (solar_azimuth)
            solar_azimuth[i] = NULL;
        if (sat_zenith)
            sat_zenith[i] = NULL;
        if (sat_azimuth)
            sat_azimuth[i] = NULL;
    }
}


/******************************************************************************
MODULE:  free_per_pixel_angles

PURPOSE:  Frees the solar and satellite angle arrays, for each band in the
array.

RETURN VALUE: N/A
******************************************************************************/
void free_per_pixel_angles
(
    short *solar_zenith[L7_NBANDS],  /* O: Array of pointers for the solar
                                           zenith angle array, one per band
                                           (if NULL, don't process) */
    short *solar_azimuth[L7_NBANDS], /* O: Array of pointers for the solar
                                           azimuth angle array, one per band
                                           (if NULL, don't process) */
    short *sat_zenith[L7_NBANDS],    /* O: Array of pointers for the satellite
                                           zenith angle array, one per band
                                           (if NULL, don't process) */
    short *sat_azimuth[L7_NBANDS]    /* O: Array of pointers for the satellite
                                           azimuth angle array, one per band
                                           (if NULL, don't process) */
)
{
    int i;   /* looping variable */

    /* Free the pointers for each band if that array is not NULL */
    for (i = 0; i < L7_NBANDS; i++)
    {
        if (solar_zenith)
            free (solar_zenith[i]);
        if (solar_azimuth)
            free (solar_azimuth[i]);
        if (sat_zenith)
            free (sat_zenith[i]);
        if (sat_azimuth)
            free (sat_azimuth[i]);
    }
}
