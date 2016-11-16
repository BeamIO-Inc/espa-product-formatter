#ifndef _LANDSAT_ANGLE_BANDS_H_
#define _LANDSAT_ANGLE_BANDS_H_

#include "gxx_structures.h"
#include "gxx_proj.h"

/* Define the number of bands for L4-5 and L7. Most of the code uses the
   maximum number of bands, which is going to be the number of bands for L7. */
#define L45_NBANDS 7
#define L7_NBANDS 9

/* ESPA Library Includes */
#include "raw_binary_io.h"
#include "envi_header.h"

/* Used as median between API and main routine for the band metadata needed to
    write the image to file */
typedef struct angle_frame
{
    int band_number;                /* Band number */
    int num_lines;                  /* Number of lines in frame */
    int num_samps;                  /* Number of samples in frame */
    DBL_XY ul_corner;               /* Upper left corner coordinates */
    double parms[PROJPRMS_SIZE];    /* Projection coefficients */
    double pixel_size;              /* Pixel size in meters */
    gxx_projection_TYPE projection; /* Projection information */
} angle_frame_TYPE;


int landsat_per_pixel_angles
(
    char *angle_coeff_name, /* I: Angle coefficient filename */
    int sub_sample,         /* I: Subsample factor used when calculating the
                                  angles (1=full resolution). OW take every Nth
                                  sample from the line, where N=sub_sample */
    char *band_list,        /* I: Band list used to calculate angles for.
                                  "ALL" - defaults to all bands 1-8.
                                  Must be comma separated with no spaces in
                                  between.  Example: 1,2,3,4,5,61,62,7,8
                                  The solar/sat_zenith/azimuth arrays should
                                  will have angles processed for these bands */
                                  /* TODO - GAIL support this */
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
);

#endif
