/*****************************************************************************
FILE: convert_lpgs_to_espa.c
  
PURPOSE: Contains functions for reading LPGS input GeoTIFF products and
writing to ESPA raw binary format.

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

LICENSE TYPE:  NASA Open Source Agreement Version 1.3

NOTES:
  1. The XML metadata format written via this library follows the ESPA internal
     metadata format found in ESPA Raw Binary Format vx.y.doc.  The schema for
     the ESPA internal metadata format is available at
     http://espa.cr.usgs.gov/schema/espa_internal_metadata_vx.y.xsd.
*****************************************************************************/
#include <unistd.h>
#include <math.h>
#include "convert_lpgs_to_espa.h"

/******************************************************************************
MODULE:  read_lpgs_mtl

PURPOSE: Read the LPGS MTL metadata file and populate the ESPA internal
metadata structure

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error reading the metadata file
SUCCESS         Successfully populated the ESPA metadata structure

NOTES:
1. The new MTL files contain the gain and bias coefficients for the TOA
   reflectance and brightness temp calculations.  These coefficients are
   parsed and written to our XML metadata file, if they exist.
2. When processing OLI_TIRS stack the 11 image bands first, then add the
   QA band to the list.
******************************************************************************/
int read_lpgs_mtl
(
    char *mtl_file,                  /* I: name of the MTL metadata file to
                                           be read */
    Espa_internal_meta_t *metadata,  /* I/O: input metadata structure to be
                                           populated from the MTL file */
    int *nlpgs_bands,                /* O: number of bands in LPGS product */
    char lpgs_bands[][STR_SIZE]      /* O: array containing the filenames of
                                           the LPGS bands */
)
{
    char FUNC_NAME[] = "read_lpgs_mtl";  /* function name */
    char errmsg[STR_SIZE];    /* error message */
    char category[MAX_LPGS_BANDS][STR_SIZE]; /* band category - qa, image */
    char band_num[MAX_LPGS_BANDS][STR_SIZE]; /* band number for band name */
    int i;                    /* looping variable */
    int count;                /* number of chars copied in snprintf */
    int band_count = 0;       /* count of the bands processed so we don't have
                                 to specify each band number directly, which
                                 get complicated as we are supporting TM, ETM+,
                                 OLI, etc. */
    bool done_with_mtl;       /* are we done processing the MTL file? */
    bool gain_bias_available; /* are the radiance gain/bias values available
                                 in the MTL file? */
    bool refl_gain_bias_available; /* are TOA reflectance gain/bias values and
                                 K1/K2 constants available in the MTL file? */
    bool thermal[MAX_LPGS_BANDS]; /* is this band a thermal band? */
    bool all_bands_read = false;  /* all filenames been read from MTL file? */
    FILE *mtl_fptr=NULL;      /* file pointer to the MTL metadata file */
    Espa_global_meta_t *gmeta = &metadata->global;  /* pointer to the global
                                                       metadata structure */
    Espa_band_meta_t *bmeta;  /* pointer to the array of bands metadata */
    Espa_band_meta_t tmp_bmeta;    /* for temporary storage of the band-related
                                      metadata for reflective bands */
    Espa_band_meta_t tmp_bmeta_th; /* for temporary storage of the band-related
                                      metadata for thermal bands */
    Espa_band_meta_t tmp_bmeta_pan; /* for temporary storage of the band-
                                       related metadata for pan bands */
    Space_def_t geoloc_def;  /* geolocation space information */
    Geoloc_t *geoloc_map = NULL;  /* geolocation mapping information */
    Geo_bounds_t bounds;     /* image boundary for the scene */
    double ur_corner[2];     /* geographic UR lat, long */
    double ll_corner[2];     /* geographic LL lat, long */
    char band_fname[MAX_LPGS_BANDS][STR_SIZE];  /* filenames for each band */
    int band_min[MAX_LPGS_BANDS];  /* minimum value for each band */
    int band_max[MAX_LPGS_BANDS];  /* maximum value for each band */
    float band_gain[MAX_LPGS_BANDS]; /* gain values for band radiance
                                        calculations */
    float band_bias[MAX_LPGS_BANDS]; /* bias values for band radiance
                                        calculations */
    float refl_gain[MAX_LPGS_BANDS]; /* gain values for TOA reflectance 
                                        calculations */
    float refl_bias[MAX_LPGS_BANDS]; /* bias values for TOA reflectance
                                        calculations */
    float k1[MAX_LPGS_BANDS]; /* K1 consts for brightness temp calculations */
    float k2[MAX_LPGS_BANDS]; /* K2 consts for brightness temp calculations */

    /* vars used in parameter parsing */
    char buffer[STR_SIZE] = "\0";          /* line buffer from MTL file */
    char *label = NULL;                    /* label value in the line */
    char *tokenptr = NULL;                 /* pointer to process each line */
    char *seperator = "=\" \t";            /* separator string */
    float fnum;                            /* temporary variable for floating
                                              point numbers */

    /* Open the metadata MTL file with read privelages */
    mtl_fptr = fopen (mtl_file, "r");
    if (mtl_fptr == NULL)
    {
        sprintf (errmsg, "Opening %s for read access.", mtl_file);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* The sensor ID is needed for parsing the rest of the MTL.  It needs to
       be read since it falls after many of the other tokens in the MTL. */
    while (fgets (buffer, STR_SIZE, mtl_fptr) != NULL)
    {
        /* If the last character is the end of line, then strip it off */
        if (buffer[strlen(buffer)-1] == '\n')
            buffer[strlen(buffer)-1] = '\0';

        /* Get string token */
        tokenptr = strtok (buffer, seperator);
        label = tokenptr;

        if (tokenptr != NULL)
        {
            tokenptr = strtok (NULL, seperator);

            if (!strcmp (label, "SENSOR_ID"))
            {
                count = snprintf (gmeta->instrument, sizeof (gmeta->instrument),
                    "%s", tokenptr);
                if (count < 0 || count >= sizeof (gmeta->instrument))
                {
                    sprintf (errmsg, "Overflow of gmeta->instrument string");
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }

                break;  /* we have found what we needed */
            }
        } /* end if tokenptr */
    }  /* end while fgets */

    /* Rewind the buffer to the start */
    rewind (mtl_fptr);

    /* Make sure the sensor ID was found */
    if (!gmeta->instrument)
    {
        sprintf (errmsg, "SENSOR ID was not found in the MTL file.");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Process the MTL file line by line */
    gain_bias_available = false;
    refl_gain_bias_available = false;
    done_with_mtl = false;
    while (fgets (buffer, STR_SIZE, mtl_fptr) != NULL)
    {
        /* If the last character is the end of line, then strip it off */
        if (buffer[strlen(buffer)-1] == '\n')
            buffer[strlen(buffer)-1] = '\0';

        /* Get string token */
        tokenptr = strtok (buffer, seperator);
        label = tokenptr;
 
        if (tokenptr != NULL)
        {
            tokenptr = strtok (NULL, seperator);

            /* Process each token */
            if (!strcmp (label, "PROCESSING_SOFTWARE_VERSION"))
            {
                count = snprintf (tmp_bmeta.app_version,
                    sizeof (tmp_bmeta.app_version), "%s", tokenptr);
                if (count < 0 || count >= sizeof (tmp_bmeta.app_version))
                {
                    sprintf (errmsg, "Overflow of tmp_bmeta.app_version");
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
            }
            else if (!strcmp (label, "PROCESSING_LEVEL"))
            {
                count = snprintf (tmp_bmeta.product, sizeof (tmp_bmeta.product),
                    "%s", tokenptr);
                if (count < 0 || count >= sizeof (tmp_bmeta.product))
                {
                    sprintf (errmsg, "Overflow of tmp_bmeta.product string");
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
            }
            else if (!strcmp (label, "LANDSAT_PRODUCT_ID"))
            {
                count = snprintf (gmeta->product_id, sizeof (gmeta->product_id),
                    "%s", tokenptr);
                if (count < 0 || count >= sizeof (gmeta->product_id))
                {
                    sprintf (errmsg, "Overflow of gmeta->product_id string");
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
            }
            else if (!strcmp (label, "SPACECRAFT_ID"))
            {
                if (!strcmp (tokenptr, "LANDSAT_9"))
                    strcpy (gmeta->satellite, "LANDSAT_9");
                else if (!strcmp (tokenptr, "LANDSAT_8"))
                    strcpy (gmeta->satellite, "LANDSAT_8");
                else if (!strcmp (tokenptr, "LANDSAT_7"))
                    strcpy (gmeta->satellite, "LANDSAT_7");
                else if (!strcmp (tokenptr, "LANDSAT_5"))
                    strcpy (gmeta->satellite, "LANDSAT_5");
                else if (!strcmp (tokenptr, "LANDSAT_4"))
                    strcpy (gmeta->satellite, "LANDSAT_4");
                else
                {
                    sprintf (errmsg, "Unsupported satellite type: %s",
                        tokenptr);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
            }
            else if (!strcmp (label, "DATE_ACQUIRED"))
            {
                count = snprintf (gmeta->acquisition_date,
                    sizeof (gmeta->acquisition_date), "%s", tokenptr);
                if (count < 0 || count >= sizeof (gmeta->acquisition_date))
                {
                    sprintf (errmsg, "Overflow of gmeta->acquisition_date");
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
            }
            else if (!strcmp (label, "SCENE_CENTER_TIME"))
            {
                count = snprintf (gmeta->scene_center_time,
                    sizeof (gmeta->scene_center_time), "%s", tokenptr);
                if (count < 0 || count >= sizeof (gmeta->scene_center_time))
                {
                    sprintf (errmsg, "Overflow of gmeta->scene_center_time");
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
            }
            else if (!strcmp (label, "DATE_PRODUCT_GENERATED"))
            {
                count = snprintf (gmeta->level1_production_date,
                    sizeof (gmeta->level1_production_date), "%s", tokenptr);
                if (count < 0 ||
                    count >= sizeof (gmeta->level1_production_date))
                {
                    sprintf (errmsg,
                        "Overflow of gmeta->level1_production_date");
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
            }
            else if (!strcmp (label, "SUN_ELEVATION"))
            {
                sscanf (tokenptr, "%f", &fnum);
                gmeta->solar_zenith = 90.0 - fnum;
            }
            else if (!strcmp (label, "SUN_AZIMUTH"))
                sscanf (tokenptr, "%f", &gmeta->solar_azimuth);
            else if (!strcmp (label, "EARTH_SUN_DISTANCE"))
                sscanf (tokenptr, "%f", &gmeta->earth_sun_dist);
            else if (!strcmp (label, "WRS_PATH"))
                sscanf (tokenptr, "%d", &gmeta->wrs_path);
            else if (!strcmp (label, "WRS_ROW"))
                sscanf (tokenptr, "%d", &gmeta->wrs_row);

            else if (!strcmp (label, "CORNER_UL_LAT_PRODUCT"))
                sscanf (tokenptr, "%lf", &gmeta->ul_corner[0]);
            else if (!strcmp (label, "CORNER_UL_LON_PRODUCT"))
                sscanf (tokenptr, "%lf", &gmeta->ul_corner[1]);
            else if (!strcmp (label, "CORNER_LR_LAT_PRODUCT"))
                sscanf (tokenptr, "%lf", &gmeta->lr_corner[0]);
            else if (!strcmp (label, "CORNER_LR_LON_PRODUCT"))
                sscanf (tokenptr, "%lf", &gmeta->lr_corner[1]);

            else if (!strcmp (label, "CORNER_UR_LAT_PRODUCT"))
                sscanf (tokenptr, "%lf", &ur_corner[0]);
            else if (!strcmp (label, "CORNER_UR_LON_PRODUCT"))
                sscanf (tokenptr, "%lf", &ur_corner[1]);
            else if (!strcmp (label, "CORNER_LL_LAT_PRODUCT"))
                sscanf (tokenptr, "%lf", &ll_corner[0]);
            else if (!strcmp (label, "CORNER_LL_LON_PRODUCT"))
                sscanf (tokenptr, "%lf", &ll_corner[1]);

            else if (!strcmp (label, "CORNER_UL_PROJECTION_X_PRODUCT"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.ul_corner[0]);
            else if (!strcmp (label, "CORNER_UL_PROJECTION_Y_PRODUCT"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.ul_corner[1]);
            else if (!strcmp (label, "CORNER_LR_PROJECTION_X_PRODUCT"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.lr_corner[0]);
            else if (!strcmp (label, "CORNER_LR_PROJECTION_Y_PRODUCT"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.lr_corner[1]);

            else if (!strcmp (label, "REFLECTIVE_SAMPLES"))
                sscanf (tokenptr, "%d", &tmp_bmeta.nsamps);
            else if (!strcmp (label, "REFLECTIVE_LINES"))
                sscanf (tokenptr, "%d", &tmp_bmeta.nlines);
            else if (!strcmp (label, "THERMAL_SAMPLES"))
                sscanf (tokenptr, "%d", &tmp_bmeta_th.nsamps);
            else if (!strcmp (label, "THERMAL_LINES"))
                sscanf (tokenptr, "%d", &tmp_bmeta_th.nlines);
            else if (!strcmp (label, "PANCHROMATIC_SAMPLES"))
                sscanf (tokenptr, "%d", &tmp_bmeta_pan.nsamps);
            else if (!strcmp (label, "PANCHROMATIC_LINES"))
                sscanf (tokenptr, "%d", &tmp_bmeta_pan.nlines);

            else if (!strcmp (label, "MAP_PROJECTION"))
            {
                if (!strcmp (tokenptr, "UTM"))
                    gmeta->proj_info.proj_type = GCTP_UTM_PROJ;
                else if (!strcmp (tokenptr, "PS"))
                    gmeta->proj_info.proj_type = GCTP_PS_PROJ;
                else if (!strcmp (tokenptr, "AEA"))  /* ALBERS */
                    gmeta->proj_info.proj_type = GCTP_ALBERS_PROJ;
                else
                {
                    sprintf (errmsg, "Unsupported projection type: %s. "
                        "Only UTM, PS, and ALBERS EQUAL AREA are supported "
                        "for LPGS.", tokenptr);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
            }
            else if (!strcmp (label, "DATUM"))
            {
                if (!strcmp (tokenptr, "WGS84"))
                    gmeta->proj_info.datum_type = ESPA_WGS84;
                else
                {
                    sprintf (errmsg, "Unexpected datum type: %s", tokenptr);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
            }
            else if (!strcmp (label, "GRID_CELL_SIZE_REFLECTIVE"))
            {
                sscanf (tokenptr, "%lf", &tmp_bmeta.pixel_size[0]);
                tmp_bmeta.pixel_size[1] = tmp_bmeta.pixel_size[0];
            }
            else if (!strcmp (label, "GRID_CELL_SIZE_THERMAL"))
            {
                sscanf (tokenptr, "%lf", &tmp_bmeta_th.pixel_size[0]);
                tmp_bmeta_th.pixel_size[1] = tmp_bmeta_th.pixel_size[0];
            }
            else if (!strcmp (label, "GRID_CELL_SIZE_PANCHROMATIC"))
            {
                sscanf (tokenptr, "%lf", &tmp_bmeta_pan.pixel_size[0]);
                tmp_bmeta_pan.pixel_size[1] = tmp_bmeta_pan.pixel_size[0];
            }
            else if (!strcmp (label, "UTM_ZONE"))
                sscanf (tokenptr, "%d", &gmeta->proj_info.utm_zone);

            /* PS projection parameters */
            else if (!strcmp (label, "VERTICAL_LON_FROM_POLE"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.longitude_pole);
            else if (!strcmp (label, "TRUE_SCALE_LAT"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.latitude_true_scale);
            else if (!strcmp (label, "FALSE_EASTING"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.false_easting);
            else if (!strcmp (label, "FALSE_NORTHING"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.false_northing);

            /* ALBERS projection parameters (in addition to false easting and
               northing under PS proj params) */
            else if (!strcmp (label, "STANDARD_PARALLEL_1_LAT"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.standard_parallel1);
            else if (!strcmp (label, "STANDARD_PARALLEL_2_LAT"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.standard_parallel2);
            else if (!strcmp (label, "CENTRAL_MERIDIAN_LON"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.central_meridian);
            else if (!strcmp (label, "ORIGIN_LAT"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.origin_latitude);

            else if (!strcmp (label, "RESAMPLING_OPTION"))
            {
                if (!strcmp (tokenptr, "CUBIC_CONVOLUTION"))
                    tmp_bmeta.resample_method = ESPA_CC;
                else if (!strcmp (tokenptr, "NEAREST_NEIGHBOR"))
                    tmp_bmeta.resample_method = ESPA_NN;
                else if (!strcmp (tokenptr, "BILINEAR"))
                    tmp_bmeta.resample_method = ESPA_BI;
                else
                {
                    sprintf (errmsg, "Unsupported resampling option: %s",
                        tokenptr);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
            }

            /* Read the band names and identify band-specific metadata
               information. If band 1 and the level-1 pixel qa bands have
               been read, then assume all bands have been read and don't
               repeat the filename reads. The Collection 02 MTL has two
               locations for the same band filenames. */
            else if (!strcmp (label, "FILE_NAME_BAND_1") && !all_bands_read)
            {
                count = snprintf (band_fname[band_count],
                    sizeof (band_fname[band_count]), "%s", tokenptr);
                if (count < 0 || count >= sizeof (band_fname[band_count]))
                {
                    sprintf (errmsg, "Overflow of band_fname[%d] string",
                        band_count);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
                strcpy (category[band_count], "image");
                strcpy (band_num[band_count], "1");
                thermal[band_count] = false;
                band_count++;  /* increment the band count */
            }
            else if (!strcmp (label, "FILE_NAME_BAND_2") && !all_bands_read)
            {
                count = snprintf (band_fname[band_count],
                    sizeof (band_fname[band_count]), "%s", tokenptr);
                if (count < 0 || count >= sizeof (band_fname[band_count]))
                {
                    sprintf (errmsg, "Overflow of band_fname[%d] string",
                        band_count);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
                strcpy (category[band_count], "image");
                strcpy (band_num[band_count], "2");
                thermal[band_count] = false;
                band_count++;  /* increment the band count */
            }
            else if (!strcmp (label, "FILE_NAME_BAND_3") && !all_bands_read)
            {
                count = snprintf (band_fname[band_count],
                    sizeof (band_fname[band_count]), "%s", tokenptr);
                if (count < 0 || count >= sizeof (band_fname[band_count]))
                {
                    sprintf (errmsg, "Overflow of band_fname[%d] string",
                        band_count);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
                strcpy (category[band_count], "image");
                strcpy (band_num[band_count], "3");
                thermal[band_count] = false;
                band_count++;  /* increment the band count */
            }
            else if (!strcmp (label, "FILE_NAME_BAND_4") && !all_bands_read)
            {
                count = snprintf (band_fname[band_count],
                    sizeof (band_fname[band_count]), "%s", tokenptr);
                if (count < 0 || count >= sizeof (band_fname[band_count]))
                {
                    sprintf (errmsg, "Overflow of band_fname[%d] string",
                        band_count);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
                strcpy (category[band_count], "image");
                strcpy (band_num[band_count], "4");
                thermal[band_count] = false;
                band_count++;  /* increment the band count */
            }
            else if (!strcmp (label, "FILE_NAME_BAND_5") && !all_bands_read)
            {
                count = snprintf (band_fname[band_count],
                    sizeof (band_fname[band_count]), "%s", tokenptr);
                if (count < 0 || count >= sizeof (band_fname[band_count]))
                {
                    sprintf (errmsg, "Overflow of band_fname[%d] string",
                        band_count);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
                strcpy (category[band_count], "image");
                strcpy (band_num[band_count], "5");
                thermal[band_count] = false;
                band_count++;  /* increment the band count */
            }
            else if (!strcmp (label, "FILE_NAME_BAND_6") && !all_bands_read)
            {
                count = snprintf (band_fname[band_count],
                    sizeof (band_fname[band_count]), "%s", tokenptr);
                if (count < 0 || count >= sizeof (band_fname[band_count]))
                {
                    sprintf (errmsg, "Overflow of band_fname[%d] string",
                        band_count);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
                strcpy (category[band_count], "image");
                strcpy (band_num[band_count], "6");
                if (!strcmp (gmeta->instrument, "TM"))
                    thermal[band_count] = true;  /* TM thermal */
                else
                    thermal[band_count] = false;
                band_count++;  /* increment the band count */
            }
            else if (!strcmp (label, "FILE_NAME_BAND_7") && !all_bands_read)
            {
                count = snprintf (band_fname[band_count],
                    sizeof (band_fname[band_count]), "%s", tokenptr);
                if (count < 0 || count >= sizeof (band_fname[band_count]))
                {
                    sprintf (errmsg, "Overflow of band_fname[%d] string",
                        band_count);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
                strcpy (&category[band_count][0], "image");
                strcpy (&band_num[band_count][0], "7");
                thermal[band_count] = false;
                band_count++;  /* increment the band count */
            }
            else if (!strcmp (label, "FILE_NAME_BAND_8") && !all_bands_read)
            {
                count = snprintf (band_fname[band_count],
                    sizeof (band_fname[band_count]), "%s", tokenptr);
                if (count < 0 || count >= sizeof (band_fname[band_count]))
                {
                    sprintf (errmsg, "Overflow of band_fname[%d] string",
                        band_count);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
                strcpy (category[band_count], "image");
                strcpy (band_num[band_count], "8");
                thermal[band_count] = false;
                band_count++;  /* increment the band count */
            }
            else if (!strcmp (label, "FILE_NAME_BAND_6_VCID_1") &&
                !all_bands_read)
            {
                count = snprintf (band_fname[band_count],
                    sizeof (band_fname[band_count]), "%s", tokenptr);
                if (count < 0 || count >= sizeof (band_fname[band_count]))
                {
                    sprintf (errmsg, "Overflow of band_fname[%d] string",
                        band_count);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
                strcpy (category[band_count], "image");
                strcpy (band_num[band_count], "61");
                thermal[band_count] = true;  /* ETM+ thermal */
                band_count++;  /* increment the band count */
            }
            else if (!strcmp (label, "FILE_NAME_BAND_6_VCID_2") &&
                !all_bands_read)
            {
                count = snprintf (band_fname[band_count],
                    sizeof (band_fname[band_count]), "%s", tokenptr);
                if (count < 0 || count >= sizeof (band_fname[band_count]))
                {
                    sprintf (errmsg, "Overflow of band_fname[%d] string",
                        band_count);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
                strcpy (category[band_count], "image");
                strcpy (band_num[band_count], "62");
                thermal[band_count] = true;  /* ETM+ thermal */
                band_count++;  /* increment the band count */
            }
            else if (!strcmp (label, "FILE_NAME_BAND_9") && !all_bands_read)
            {
                count = snprintf (band_fname[band_count],
                    sizeof (band_fname[band_count]), "%s", tokenptr);
                if (count < 0 || count >= sizeof (band_fname[band_count]))
                {
                    sprintf (errmsg, "Overflow of band_fname[%d] string",
                        band_count);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
                strcpy (category[band_count], "image");
                strcpy (band_num[band_count], "9");
                thermal[band_count] = false;
                band_count++;  /* increment the band count */
            }
            else if (!strcmp (label, "FILE_NAME_BAND_10") && !all_bands_read)
            {
                count = snprintf (band_fname[band_count],
                    sizeof (band_fname[band_count]), "%s", tokenptr);
                if (count < 0 || count >= sizeof (band_fname[band_count]))
                {
                    sprintf (errmsg, "Overflow of band_fname[%d] string",
                        band_count);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
                strcpy (category[band_count], "image");
                strcpy (band_num[band_count], "10");
                thermal[band_count] = true;  /* TIRS */
                band_count++;  /* increment the band count */
            }
            else if (!strcmp (label, "FILE_NAME_BAND_11") && !all_bands_read)
            {
                count = snprintf (band_fname[band_count],
                    sizeof (band_fname[band_count]), "%s", tokenptr);
                if (count < 0 || count >= sizeof (band_fname[band_count]))
                {
                    sprintf (errmsg, "Overflow of band_fname[%d] string",
                        band_count);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
                strcpy (category[band_count], "image");
                strcpy (band_num[band_count], "11");
                thermal[band_count] = true;  /* TIRS */
                band_count++;  /* increment the band count */
            }
            else if (!strcmp (label, "FILE_NAME_ANGLE_SENSOR_AZIMUTH_BAND_4") &&
                !all_bands_read)
            {
                count = snprintf (band_fname[band_count],
                    sizeof (band_fname[band_count]), "%s", tokenptr);
                if (count < 0 || count >= sizeof (band_fname[band_count]))
                {
                    sprintf (errmsg, "Overflow of band_fname[%d] string",
                        band_count);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
                strcpy (category[band_count], "qa");
                strcpy (band_num[band_count], "vaa");
                thermal[band_count] = false;
                band_count++;  /* increment the band count */
            }
            else if (!strcmp (label, "FILE_NAME_ANGLE_SENSOR_ZENITH_BAND_4") &&
                !all_bands_read)
            {
                count = snprintf (band_fname[band_count],
                    sizeof (band_fname[band_count]), "%s", tokenptr);
                if (count < 0 || count >= sizeof (band_fname[band_count]))
                {
                    sprintf (errmsg, "Overflow of band_fname[%d] string",
                        band_count);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
                strcpy (category[band_count], "qa");
                strcpy (band_num[band_count], "vza");
                thermal[band_count] = false;
                band_count++;  /* increment the band count */
            }
            else if (!strcmp (label, "FILE_NAME_ANGLE_SOLAR_AZIMUTH_BAND_4") &&
                !all_bands_read)
            {
                count = snprintf (band_fname[band_count],
                    sizeof (band_fname[band_count]), "%s", tokenptr);
                if (count < 0 || count >= sizeof (band_fname[band_count]))
                {
                    sprintf (errmsg, "Overflow of band_fname[%d] string",
                        band_count);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
                strcpy (category[band_count], "qa");
                strcpy (band_num[band_count], "saa");
                thermal[band_count] = false;
                band_count++;  /* increment the band count */
            }
            else if (!strcmp (label, "FILE_NAME_ANGLE_SOLAR_ZENITH_BAND_4") &&
                !all_bands_read)
            {
                count = snprintf (band_fname[band_count],
                    sizeof (band_fname[band_count]), "%s", tokenptr);
                if (count < 0 || count >= sizeof (band_fname[band_count]))
                {
                    sprintf (errmsg, "Overflow of band_fname[%d] string",
                        band_count);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
                strcpy (category[band_count], "qa");
                strcpy (band_num[band_count], "sza");
                thermal[band_count] = false;
                band_count++;  /* increment the band count */
            }
            else if (!strcmp (label, "FILE_NAME_QUALITY_L1_PIXEL") &&
                !all_bands_read)
            {
                count = snprintf (band_fname[band_count],
                    sizeof (band_fname[band_count]), "%s", tokenptr);
                if (count < 0 || count >= sizeof (band_fname[band_count]))
                {
                    sprintf (errmsg, "Overflow of band_fname[%d] string",
                        band_count);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
                strcpy (category[band_count], "qa");
                strcpy (band_num[band_count], "qa_pixel");
                thermal[band_count] = false;
                band_count++;  /* increment the band count */
            }
            else if (!strcmp (label,
                "FILE_NAME_QUALITY_L1_RADIOMETRIC_SATURATION") &&
                !all_bands_read)
            {
                count = snprintf (band_fname[band_count],
                    sizeof (band_fname[band_count]), "%s", tokenptr);
                if (count < 0 || count >= sizeof (band_fname[band_count]))
                {
                    sprintf (errmsg, "Overflow of band_fname[%d] string",
                        band_count);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
                strcpy (category[band_count], "qa");
                strcpy (band_num[band_count], "qa_radsat");
                thermal[band_count] = false;
                band_count++;  /* increment the band count */
            }

            /* Read the min pixel values */
            else if (!strcmp (label, "QUANTIZE_CAL_MIN_BAND_1"))
                sscanf (tokenptr, "%d", &band_min[0]);
            else if (!strcmp (label, "QUANTIZE_CAL_MIN_BAND_2"))
                sscanf (tokenptr, "%d", &band_min[1]);
            else if (!strcmp (label, "QUANTIZE_CAL_MIN_BAND_3"))
                sscanf (tokenptr, "%d", &band_min[2]);
            else if (!strcmp (label, "QUANTIZE_CAL_MIN_BAND_4"))
                sscanf (tokenptr, "%d", &band_min[3]);
            else if (!strcmp (label, "QUANTIZE_CAL_MIN_BAND_5"))
                sscanf (tokenptr, "%d", &band_min[4]);
            else if (!strcmp (label, "QUANTIZE_CAL_MIN_BAND_6"))
                sscanf (tokenptr, "%d", &band_min[5]);
            else if (!strcmp (label, "QUANTIZE_CAL_MIN_BAND_7"))
            {
                if (!strcmp (gmeta->instrument, "TM") ||
                    !strcmp (gmeta->instrument, "OLI_TIRS") ||
                    !strcmp (gmeta->instrument, "OLI"))
                    sscanf (tokenptr, "%d", &band_min[6]);
                else if (!strncmp (gmeta->instrument, "ETM", 3))
                    sscanf (tokenptr, "%d", &band_min[7]);
            }
            else if (!strcmp (label, "QUANTIZE_CAL_MIN_BAND_8"))
            {
                if (!strcmp (gmeta->instrument, "OLI_TIRS") ||
                    !strcmp (gmeta->instrument, "OLI"))
                    sscanf (tokenptr, "%d", &band_min[7]);
                else if (!strncmp (gmeta->instrument, "ETM", 3))
                    sscanf (tokenptr, "%d", &band_min[8]);
            }
            else if (!strcmp (label, "QUANTIZE_CAL_MIN_BAND_6_VCID_1"))
                sscanf (tokenptr, "%d", &band_min[5]);
            else if (!strcmp (label, "QUANTIZE_CAL_MIN_BAND_6_VCID_2"))
                sscanf (tokenptr, "%d", &band_min[6]);
            else if (!strcmp (label, "QUANTIZE_CAL_MIN_BAND_9"))
                sscanf (tokenptr, "%d", &band_min[8]);
            else if (!strcmp (label, "QUANTIZE_CAL_MIN_BAND_10"))
                sscanf (tokenptr, "%d", &band_min[9]);
            else if (!strcmp (label, "QUANTIZE_CAL_MIN_BAND_11"))
                sscanf (tokenptr, "%d", &band_min[10]);

            /* Read the max pixel values */
            else if (!strcmp (label, "QUANTIZE_CAL_MAX_BAND_1"))
                sscanf (tokenptr, "%d", &band_max[0]);
            else if (!strcmp (label, "QUANTIZE_CAL_MAX_BAND_2"))
                sscanf (tokenptr, "%d", &band_max[1]);
            else if (!strcmp (label, "QUANTIZE_CAL_MAX_BAND_3"))
                sscanf (tokenptr, "%d", &band_max[2]);
            else if (!strcmp (label, "QUANTIZE_CAL_MAX_BAND_4"))
                sscanf (tokenptr, "%d", &band_max[3]);
            else if (!strcmp (label, "QUANTIZE_CAL_MAX_BAND_5"))
                sscanf (tokenptr, "%d", &band_max[4]);
            else if (!strcmp (label, "QUANTIZE_CAL_MAX_BAND_6"))
                sscanf (tokenptr, "%d", &band_max[5]);
            else if (!strcmp (label, "QUANTIZE_CAL_MAX_BAND_7"))
            {
                if (!strcmp (gmeta->instrument, "TM") ||
                    !strcmp (gmeta->instrument, "OLI_TIRS") ||
                    !strcmp (gmeta->instrument, "OLI"))
                    sscanf (tokenptr, "%d", &band_max[6]);
                else if (!strncmp (gmeta->instrument, "ETM", 3))
                    sscanf (tokenptr, "%d", &band_max[7]);
            }
            else if (!strcmp (label, "QUANTIZE_CAL_MAX_BAND_8"))
            {
                if (!strcmp (gmeta->instrument, "OLI_TIRS") ||
                    !strcmp (gmeta->instrument, "OLI"))
                    sscanf (tokenptr, "%d", &band_max[7]);
                else if (!strncmp (gmeta->instrument, "ETM", 3))
                    sscanf (tokenptr, "%d", &band_max[8]);
            }
            else if (!strcmp (label, "QUANTIZE_CAL_MAX_BAND_6_VCID_1"))
                sscanf (tokenptr, "%d", &band_max[5]);
            else if (!strcmp (label, "QUANTIZE_CAL_MAX_BAND_6_VCID_2"))
                sscanf (tokenptr, "%d", &band_max[6]);
            else if (!strcmp (label, "QUANTIZE_CAL_MAX_BAND_9"))
                sscanf (tokenptr, "%d", &band_max[8]);
            else if (!strcmp (label, "QUANTIZE_CAL_MAX_BAND_10"))
                sscanf (tokenptr, "%d", &band_max[9]);
            else if (!strcmp (label, "QUANTIZE_CAL_MAX_BAND_11"))
                sscanf (tokenptr, "%d", &band_max[10]);

            /* Read the radiance gains */
            else if (!strcmp (label, "RADIANCE_MULT_BAND_1"))
            {
                sscanf (tokenptr, "%f", &band_gain[0]);

                /* Assume that if gain value for band 1 is available, then
                   the gain and bias values for all bands will be available */
                gain_bias_available = true;
            }
            else if (!strcmp (label, "RADIANCE_MULT_BAND_2"))
                sscanf (tokenptr, "%f", &band_gain[1]);
            else if (!strcmp (label, "RADIANCE_MULT_BAND_3"))
                sscanf (tokenptr, "%f", &band_gain[2]);
            else if (!strcmp (label, "RADIANCE_MULT_BAND_4"))
                sscanf (tokenptr, "%f", &band_gain[3]);
            else if (!strcmp (label, "RADIANCE_MULT_BAND_5"))
                sscanf (tokenptr, "%f", &band_gain[4]);
            else if (!strcmp (label, "RADIANCE_MULT_BAND_6"))
                sscanf (tokenptr, "%f", &band_gain[5]);
            else if (!strcmp (label, "RADIANCE_MULT_BAND_7"))
            {
                if (!strcmp (gmeta->instrument, "TM") ||
                    !strcmp (gmeta->instrument, "OLI_TIRS") ||
                    !strcmp (gmeta->instrument, "OLI"))
                    sscanf (tokenptr, "%f", &band_gain[6]);
                else if (!strncmp (gmeta->instrument, "ETM", 3))
                    sscanf (tokenptr, "%f", &band_gain[7]);
            }
            else if (!strcmp (label, "RADIANCE_MULT_BAND_8"))
            {
                if (!strcmp (gmeta->instrument, "OLI_TIRS") ||
                    !strcmp (gmeta->instrument, "OLI"))
                    sscanf (tokenptr, "%f", &band_gain[7]);
                else if (!strncmp (gmeta->instrument, "ETM", 3))
                    sscanf (tokenptr, "%f", &band_gain[8]);
            }
            else if (!strcmp (label, "RADIANCE_MULT_BAND_6_VCID_1"))
                sscanf (tokenptr, "%f", &band_gain[5]);
            else if (!strcmp (label, "RADIANCE_MULT_BAND_6_VCID_2"))
                sscanf (tokenptr, "%f", &band_gain[6]);
            else if (!strcmp (label, "RADIANCE_MULT_BAND_9"))
                sscanf (tokenptr, "%f", &band_gain[8]);
            else if (!strcmp (label, "RADIANCE_MULT_BAND_10"))
                sscanf (tokenptr, "%f", &band_gain[9]);
            else if (!strcmp (label, "RADIANCE_MULT_BAND_11"))
                sscanf (tokenptr, "%f", &band_gain[10]);

            /* Read the radiance biases */
            else if (!strcmp (label, "RADIANCE_ADD_BAND_1"))
                sscanf (tokenptr, "%f", &band_bias[0]);
            else if (!strcmp (label, "RADIANCE_ADD_BAND_2"))
                sscanf (tokenptr, "%f", &band_bias[1]);
            else if (!strcmp (label, "RADIANCE_ADD_BAND_3"))
                sscanf (tokenptr, "%f", &band_bias[2]);
            else if (!strcmp (label, "RADIANCE_ADD_BAND_4"))
                sscanf (tokenptr, "%f", &band_bias[3]);
            else if (!strcmp (label, "RADIANCE_ADD_BAND_5"))
                sscanf (tokenptr, "%f", &band_bias[4]);
            else if (!strcmp (label, "RADIANCE_ADD_BAND_6"))
                sscanf (tokenptr, "%f", &band_bias[5]);
            else if (!strcmp (label, "RADIANCE_ADD_BAND_7"))
            {
                if (!strcmp (gmeta->instrument, "TM") ||
                    !strcmp (gmeta->instrument, "OLI_TIRS") ||
                    !strcmp (gmeta->instrument, "OLI"))
                    sscanf (tokenptr, "%f", &band_bias[6]);
                else if (!strncmp (gmeta->instrument, "ETM", 3))
                    sscanf (tokenptr, "%f", &band_bias[7]);
            }
            else if (!strcmp (label, "RADIANCE_ADD_BAND_8"))
            {
                if (!strcmp (gmeta->instrument, "OLI_TIRS") ||
                    !strcmp (gmeta->instrument, "OLI"))
                    sscanf (tokenptr, "%f", &band_bias[7]);
                else if (!strncmp (gmeta->instrument, "ETM", 3))
                    sscanf (tokenptr, "%f", &band_bias[8]);
            }
            else if (!strcmp (label, "RADIANCE_ADD_BAND_6_VCID_1"))
                sscanf (tokenptr, "%f", &band_bias[5]);
            else if (!strcmp (label, "RADIANCE_ADD_BAND_6_VCID_2"))
                sscanf (tokenptr, "%f", &band_bias[6]);
            else if (!strcmp (label, "RADIANCE_ADD_BAND_9"))
                sscanf (tokenptr, "%f", &band_bias[8]);
            else if (!strcmp (label, "RADIANCE_ADD_BAND_10"))
                sscanf (tokenptr, "%f", &band_bias[9]);
            else if (!strcmp (label, "RADIANCE_ADD_BAND_11"))
                sscanf (tokenptr, "%f", &band_bias[10]);

            /* Read the reflectance gains */
            else if (!strcmp (label, "REFLECTANCE_MULT_BAND_1"))
            {
                sscanf (tokenptr, "%f", &refl_gain[0]);

                /* Assume that if the reflectance gain value for band 1 is
                   available, then the gain and bias values for all bands will
                   be available */
                refl_gain_bias_available = true;
            }
            else if (!strcmp (label, "REFLECTANCE_MULT_BAND_2"))
                sscanf (tokenptr, "%f", &refl_gain[1]);
            else if (!strcmp (label, "REFLECTANCE_MULT_BAND_3"))
                sscanf (tokenptr, "%f", &refl_gain[2]);
            else if (!strcmp (label, "REFLECTANCE_MULT_BAND_4"))
                sscanf (tokenptr, "%f", &refl_gain[3]);
            else if (!strcmp (label, "REFLECTANCE_MULT_BAND_5"))
                sscanf (tokenptr, "%f", &refl_gain[4]);
            else if (!strcmp (label, "REFLECTANCE_MULT_BAND_6"))
                sscanf (tokenptr, "%f", &refl_gain[5]);
            else if (!strcmp (label, "REFLECTANCE_MULT_BAND_7"))
            {
                if (!strcmp (gmeta->instrument, "TM") ||
                    !strcmp (gmeta->instrument, "OLI_TIRS") ||
                    !strcmp (gmeta->instrument, "OLI"))
                    sscanf (tokenptr, "%f", &refl_gain[6]);
                else if (!strncmp (gmeta->instrument, "ETM", 3))
                    sscanf (tokenptr, "%f", &refl_gain[7]);
            }
            else if (!strcmp (label, "REFLECTANCE_MULT_BAND_8"))
            {
                if (!strcmp (gmeta->instrument, "OLI_TIRS") ||
                    !strcmp (gmeta->instrument, "OLI"))
                    sscanf (tokenptr, "%f", &refl_gain[7]);
                else if (!strncmp (gmeta->instrument, "ETM", 3))
                    sscanf (tokenptr, "%f", &refl_gain[8]);
            }
            else if (!strcmp (label, "REFLECTANCE_MULT_BAND_9"))
                sscanf (tokenptr, "%f", &refl_gain[8]);

            /* Read the reflectance biases */
            else if (!strcmp (label, "REFLECTANCE_ADD_BAND_1"))
                sscanf (tokenptr, "%f", &refl_bias[0]);
            else if (!strcmp (label, "REFLECTANCE_ADD_BAND_2"))
                sscanf (tokenptr, "%f", &refl_bias[1]);
            else if (!strcmp (label, "REFLECTANCE_ADD_BAND_3"))
                sscanf (tokenptr, "%f", &refl_bias[2]);
            else if (!strcmp (label, "REFLECTANCE_ADD_BAND_4"))
                sscanf (tokenptr, "%f", &refl_bias[3]);
            else if (!strcmp (label, "REFLECTANCE_ADD_BAND_5"))
                sscanf (tokenptr, "%f", &refl_bias[4]);
            else if (!strcmp (label, "REFLECTANCE_ADD_BAND_6"))
                sscanf (tokenptr, "%f", &refl_bias[5]);
            else if (!strcmp (label, "REFLECTANCE_ADD_BAND_7"))
            {
                if (!strcmp (gmeta->instrument, "TM") ||
                    !strcmp (gmeta->instrument, "OLI_TIRS") ||
                    !strcmp (gmeta->instrument, "OLI"))
                    sscanf (tokenptr, "%f", &refl_bias[6]);
                else if (!strncmp (gmeta->instrument, "ETM", 3))
                    sscanf (tokenptr, "%f", &refl_bias[7]);
            }
            else if (!strcmp (label, "REFLECTANCE_ADD_BAND_8"))
            {
                if (!strcmp (gmeta->instrument, "OLI_TIRS") ||
                    !strcmp (gmeta->instrument, "OLI"))
                    sscanf (tokenptr, "%f", &refl_bias[7]);
                else if (!strncmp (gmeta->instrument, "ETM", 3))
                    sscanf (tokenptr, "%f", &refl_bias[8]);
            }
            else if (!strcmp (label, "REFLECTANCE_ADD_BAND_9"))
                sscanf (tokenptr, "%f", &refl_bias[8]);

            /* Read the K1, K2 constants */
            /* TIRS */
            else if (!strcmp (label, "K1_CONSTANT_BAND_10"))
                sscanf (tokenptr, "%f", &k1[9]);
            else if (!strcmp (label, "K1_CONSTANT_BAND_11"))
                sscanf (tokenptr, "%f", &k1[10]);
            else if (!strcmp (label, "K2_CONSTANT_BAND_10"))
                sscanf (tokenptr, "%f", &k2[9]);
            else if (!strcmp (label, "K2_CONSTANT_BAND_11"))
                sscanf (tokenptr, "%f", &k2[10]);

            /* ETM+ */
            else if (!strcmp (label, "K1_CONSTANT_BAND_6_VCID_1"))
                sscanf (tokenptr, "%f", &k1[5]);
            else if (!strcmp (label, "K1_CONSTANT_BAND_6_VCID_2"))
                sscanf (tokenptr, "%f", &k1[6]);
            else if (!strcmp (label, "K2_CONSTANT_BAND_6_VCID_1"))
                sscanf (tokenptr, "%f", &k2[5]);
            else if (!strcmp (label, "K2_CONSTANT_BAND_6_VCID_2"))
                sscanf (tokenptr, "%f", &k2[6]);

            /* TM */
            else if (!strcmp (label, "K1_CONSTANT_BAND_6"))
                sscanf (tokenptr, "%f", &k1[5]);
            else if (!strcmp (label, "K2_CONSTANT_BAND_6"))
                sscanf (tokenptr, "%f", &k2[5]);

            /* Catch the end of the file */
            else if (!strcmp (label, "END"))
            {
                done_with_mtl = true;
                break;
            }

            /* If we are past the PRODUCT_CONTENTS group, then don't re-read
               the filenames in the next section of the Collection 02 metadata
               which contains the same filenames. */
            else if (!strcmp (label, "END_GROUP") &&
                !strcmp (tokenptr, "PRODUCT_CONTENTS"))
            {
                all_bands_read = true;
            }
        } /* end if tokenptr */

        /* If we are done */
        if (done_with_mtl)
            break;
    }  /* end while fgets */

    /* Check the band count to make sure we didn't go over the maximum
       expected */
    if (band_count > MAX_LPGS_BANDS)
    {
        sprintf (errmsg, "The total band count of LPGS bands converted for "
            "this product (%d) exceeds the maximum expected (%d).", band_count,
            MAX_LPGS_BANDS);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Set defaults that aren't in the MTL file */
    gmeta->wrs_system = 2;
    gmeta->orientation_angle = 0.0;
    strcpy (gmeta->data_provider, "USGS/EROS");
    strcpy (gmeta->solar_units, "degrees");

    count = snprintf (gmeta->lpgs_metadata_file,
        sizeof (gmeta->lpgs_metadata_file), "%s", mtl_file);
    if (count < 0 || count >= sizeof (gmeta->lpgs_metadata_file))
    {
        sprintf (errmsg, "Overflow of gmeta->lpgs_metadata_file string");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    count = snprintf (gmeta->proj_info.units, sizeof (gmeta->proj_info.units),
        "%s", "meters");
    if (count < 0 || count >= sizeof (gmeta->proj_info.units))
    {
        sprintf (errmsg, "Overflow of gmeta->proj_info.units string");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* UL and LR corner projection coords in the MTL file are for the center
       of the pixel.  Given there are different resolution bands, leave the
       corners as the center of the pixel. */
    strcpy (gmeta->proj_info.grid_origin, "CENTER");

    /* Set up the number of total bands */
    metadata->nbands = band_count;
    if (allocate_band_metadata (metadata, metadata->nbands) != SUCCESS)
    {   /* Error messages already printed */
        return (ERROR);
    }
    bmeta = metadata->band;

    /* Fill in the band-related metadata for each of the bands */
    *nlpgs_bands = metadata->nbands;
    for (i = 0; i < metadata->nbands; i++)
    {
        /* Handle the general metadata for each band */
        count = snprintf (lpgs_bands[i], sizeof (lpgs_bands[i]), "%s",
            band_fname[i]);
        if (count < 0 || count >= sizeof (lpgs_bands[i]))
        {
            sprintf (errmsg, "Overflow of lpgs_bands[i] string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        count = snprintf (bmeta[i].product, sizeof (bmeta[i].product), "%s",
            tmp_bmeta.product);
        if (count < 0 || count >= sizeof (bmeta[i].product))
        {
            sprintf (errmsg, "Overflow of bmeta[i].product string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        count = snprintf (bmeta[i].category, sizeof (bmeta[i].category), "%s",
            category[i]);
        if (count < 0 || count >= sizeof (bmeta[i].category))
        {
            sprintf (errmsg, "Overflow of bmeta[i].category string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        count = snprintf (bmeta[i].app_version, sizeof (bmeta[i].app_version),
            "%s", tmp_bmeta.app_version);
        if (count < 0 || count >= sizeof (bmeta[i].app_version))
        {
            sprintf (errmsg, "Overflow of bmeta[i].app_version string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        bmeta[i].valid_range[0] = (float) band_min[i];
        bmeta[i].valid_range[1] = (float) band_max[i];

        if (gain_bias_available)
        {
            bmeta[i].rad_gain = band_gain[i];
            bmeta[i].rad_bias = band_bias[i];
        }

        if (refl_gain_bias_available)
        {
            /* Gain/bias only exist for image bands */
            if (!strcmp (category[i], "image"))
            {
                /* Reflectance gain/bias values don't exist for the thermal
                   bands, but the K constants do */
                if (thermal[i])
                {
                    bmeta[i].k1_const = k1[i];
                    bmeta[i].k2_const = k2[i];
                }
                else
                {
                    bmeta[i].refl_gain = refl_gain[i];
                    bmeta[i].refl_bias = refl_bias[i];
                }
            }
            else
            {
                /* QA bands don't have these */
                bmeta[i].refl_gain = ESPA_FLOAT_META_FILL;
                bmeta[i].refl_bias = ESPA_FLOAT_META_FILL;
                bmeta[i].k1_const = ESPA_FLOAT_META_FILL;
                bmeta[i].k2_const = ESPA_FLOAT_META_FILL;
            }
        }

        count = snprintf (bmeta[i].data_units, sizeof (bmeta[i].data_units),
            "%s", "digital numbers");
        if (count < 0 || count >= sizeof (bmeta[i].data_units))
        {
            sprintf (errmsg, "Overflow of bmeta[i].data_units string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        count = snprintf (bmeta[i].pixel_units, sizeof (bmeta[i].pixel_units),
            "%s", "meters");
        if (count < 0 || count >= sizeof (bmeta[i].pixel_units))
        {
            sprintf (errmsg, "Overflow of bmeta[i].pixel_units string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        count = snprintf (bmeta[i].production_date,
            sizeof (bmeta[i].production_date), "%s",
            gmeta->level1_production_date);
        if (count < 0 || count >= sizeof (bmeta[i].production_date))
        {
            sprintf (errmsg, "Overflow of bmeta[i].production_date string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        if (!strcmp (gmeta->instrument, "TM"))
        {
            bmeta[i].data_type = ESPA_UINT8;
            bmeta[i].fill_value = 0;
            if (!strcmp (gmeta->satellite, "LANDSAT_4"))
                strcpy (bmeta[i].short_name, "LT04DN");
            else if (!strcmp (gmeta->satellite, "LANDSAT_5"))
                strcpy (bmeta[i].short_name, "LT05DN");
        }
        else if (!strncmp (gmeta->instrument, "ETM", 3))
        {
            bmeta[i].data_type = ESPA_UINT8;
            bmeta[i].fill_value = 0;
            strcpy (bmeta[i].short_name, "LE07DN");
        }
        else if (!strcmp (gmeta->instrument, "OLI_TIRS"))
        {
            bmeta[i].data_type = ESPA_UINT16;
            bmeta[i].fill_value = 0;
            strcpy (bmeta[i].short_name, "LC08DN");
        }
        else if (!strcmp (gmeta->instrument, "OLI"))
        {
            bmeta[i].data_type = ESPA_UINT16;
            bmeta[i].fill_value = 0;
            strcpy (bmeta[i].short_name, "LO08DN");
        }
        else if (!strcmp (gmeta->instrument, "TIRS"))
        {
            bmeta[i].data_type = ESPA_UINT16;
            bmeta[i].fill_value = 0;
            strcpy (bmeta[i].short_name, "LT08DN");
        }
        else
        {
            sprintf (errmsg, "Invalid Landsat-based instrument %s",
                gmeta->instrument);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Set up the band names */
        if (!strcmp (band_num[i], "qa_pixel"))
        {
            strcpy (bmeta[i].name, "qa_pixel");
            strcpy (bmeta[i].long_name, "level-1 pixel quality");
            bmeta[i].data_type = ESPA_UINT16;
            bmeta[i].valid_range[0] = 0.0;
            bmeta[i].valid_range[1] = 65535.0;
            bmeta[i].rad_gain = ESPA_FLOAT_META_FILL;
            bmeta[i].rad_bias = ESPA_FLOAT_META_FILL;
            strcpy (&bmeta[i].short_name[4], "PQA");
            strcpy (bmeta[i].data_units, "quality/feature classification");
        }
        else if (!strcmp (band_num[i], "qa_radsat"))
        {
            strcpy (bmeta[i].name, "qa_radsat");
            strcpy (bmeta[i].long_name,
                "level-1 radiometric saturation and terrain occlusion");
            bmeta[i].data_type = ESPA_UINT16;
            bmeta[i].valid_range[0] = 0.0;
            bmeta[i].valid_range[1] = 65535.0;
            bmeta[i].rad_gain = ESPA_FLOAT_META_FILL;
            bmeta[i].rad_bias = ESPA_FLOAT_META_FILL;
            strcpy (&bmeta[i].short_name[4], "RADSAT");
            strcpy (bmeta[i].data_units, "quality/feature classification");
        }
        else if (!strcmp (band_num[i], "vaa"))
        {
            strcpy (bmeta[i].name, band_num[i]);
            strcpy (bmeta[i].long_name, "band 4 view/sensor azimuth angles");
            bmeta[i].data_type = ESPA_INT16;
            bmeta[i].rad_gain = ESPA_FLOAT_META_FILL;
            bmeta[i].rad_bias = ESPA_FLOAT_META_FILL;
            bmeta[i].scale_factor = 0.01;  /* from DFCB, not in MTL file */
            strcpy (&bmeta[i].short_name[4], "SENAZ");
            strcpy (bmeta[i].data_units, "degrees");
            strcpy (bmeta[i].product, "angle_bands");
            bmeta[i].fill_value = ESPA_INT_META_FILL;  /* no fill value */
            bmeta[i].valid_range[0] = ESPA_FLOAT_META_FILL;
            bmeta[i].valid_range[1] = ESPA_FLOAT_META_FILL;
        }
        else if (!strcmp (band_num[i], "vza"))
        {
            strcpy (bmeta[i].name, band_num[i]);
            strcpy (bmeta[i].long_name, "band 4 view/sensor zenith angles");
            bmeta[i].data_type = ESPA_INT16;
            bmeta[i].rad_gain = ESPA_FLOAT_META_FILL;
            bmeta[i].rad_bias = ESPA_FLOAT_META_FILL;
            bmeta[i].scale_factor = 0.01;  /* from DFCB, not in MTL file */
            strcpy (&bmeta[i].short_name[4], "SENZEN");
            strcpy (bmeta[i].data_units, "degrees");
            strcpy (bmeta[i].product, "angle_bands");
            bmeta[i].fill_value = ESPA_INT_META_FILL;  /* no fill value */
            bmeta[i].valid_range[0] = ESPA_FLOAT_META_FILL;
            bmeta[i].valid_range[1] = ESPA_FLOAT_META_FILL;
        }
        else if (!strcmp (band_num[i], "saa"))
        {
            strcpy (bmeta[i].name, band_num[i]);
            strcpy (bmeta[i].long_name, "band 4 solar azimuth angles");
            bmeta[i].data_type = ESPA_INT16;
            bmeta[i].rad_gain = ESPA_FLOAT_META_FILL;
            bmeta[i].rad_bias = ESPA_FLOAT_META_FILL;
            bmeta[i].scale_factor = 0.01;  /* from DFCB, not in MTL file */
            strcpy (&bmeta[i].short_name[4], "SOLAZ");
            strcpy (bmeta[i].data_units, "degrees");
            strcpy (bmeta[i].product, "angle_bands");
            bmeta[i].fill_value = ESPA_INT_META_FILL;  /* no fill value */
            bmeta[i].valid_range[0] = ESPA_FLOAT_META_FILL;
            bmeta[i].valid_range[1] = ESPA_FLOAT_META_FILL;
        }
        else if (!strcmp (band_num[i], "sza"))
        {
            strcpy (bmeta[i].name, band_num[i]);
            strcpy (bmeta[i].long_name, "band 4 solar zenith angles");
            bmeta[i].data_type = ESPA_INT16;
            bmeta[i].rad_gain = ESPA_FLOAT_META_FILL;
            bmeta[i].rad_bias = ESPA_FLOAT_META_FILL;
            bmeta[i].scale_factor = 0.01;  /* from DFCB, not in MTL file */
            strcpy (&bmeta[i].short_name[4], "SOLZEN");
            strcpy (bmeta[i].data_units, "degrees");
            strcpy (bmeta[i].product, "angle_bands");
            bmeta[i].fill_value = ESPA_INT_META_FILL;  /* no fill value */
            bmeta[i].valid_range[0] = ESPA_FLOAT_META_FILL;
            bmeta[i].valid_range[1] = ESPA_FLOAT_META_FILL;
        }
        else
        {  /* bands other than band quality and per-pixel bands */
            sprintf (bmeta[i].name, "b%s", band_num[i]);
            sprintf (bmeta[i].long_name, "band %s digital numbers",
              band_num[i]);
            bmeta[i].resample_method = tmp_bmeta.resample_method;
        }

        count = snprintf (bmeta[i].file_name, sizeof (bmeta[i].file_name),
            "%s_%s.img", gmeta->product_id, bmeta[i].name);
        if (count < 0 || count >= sizeof (bmeta[i].file_name))
        {
            sprintf (errmsg, "Overflow of bmeta[i].file_name");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Set up the image size and resolution */
        if (thermal[i])
        {  /* thermal bands */
            bmeta[i].nlines = tmp_bmeta_th.nlines;
            bmeta[i].nsamps = tmp_bmeta_th.nsamps;
            bmeta[i].pixel_size[0] = tmp_bmeta_th.pixel_size[0];
            bmeta[i].pixel_size[1] = tmp_bmeta_th.pixel_size[1];
        }
        else if (!strcmp (band_num[i], "8"))
        {  /* pan bands - both ETM+ and OLI band 8 are pan bands */
            bmeta[i].nlines = tmp_bmeta_pan.nlines;
            bmeta[i].nsamps = tmp_bmeta_pan.nsamps;
            bmeta[i].pixel_size[0] = tmp_bmeta_pan.pixel_size[0];
            bmeta[i].pixel_size[1] = tmp_bmeta_pan.pixel_size[1];
        }
        else
        {  /* QA, per-pixel angles, and reflective bands */
            bmeta[i].nlines = tmp_bmeta.nlines;
            bmeta[i].nsamps = tmp_bmeta.nsamps;
            bmeta[i].pixel_size[0] = tmp_bmeta.pixel_size[0];
            bmeta[i].pixel_size[1] = tmp_bmeta.pixel_size[1];
        }

        /* If this is one of the QA bands then write the bitmap definition */
        if (!strcmp (band_num[i], "qa_pixel"))
        {
            if (allocate_bitmap_metadata (&bmeta[i], 16) != SUCCESS)
            {
                sprintf (errmsg, "Allocating 16 bits for the bitmap");
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }

            strcpy (bmeta[i].bitmap_description[0],
                "Data Fill Flag (0 = image data, 1 = fill data)");
            strcpy (bmeta[i].bitmap_description[1],
                "Dilated Cloud (0 = cloud not dilated or no cloud, "
                "1 = cloud dilation)");

            if (!strncmp (gmeta->instrument, "OLI", 3))
            {
                strcpy (bmeta[i].bitmap_description[2],
                    "Cirrus (0 = no confidence level set or low confidence, "
                    "1 = high confidence cirrus)");
            }
            else
                strcpy (bmeta[i].bitmap_description[2], "Not used");

            strcpy (bmeta[i].bitmap_description[3],
                "Cloud (0 = cloud confidence is not high, "
                "1 = high confidence cloud)");
            strcpy (bmeta[i].bitmap_description[4],
                "Cloud Shadow (0 = cloud shadow confidence is not high, "
                "1 = high confidence cloud shadow)");
            strcpy (bmeta[i].bitmap_description[5],
                "Snow (0 = snow/ice confidence is not high, "
                "1 = high confidence snow cover)");
            strcpy (bmeta[i].bitmap_description[6],
                "Clear (0 = cloud or dilated cloud bits are set, "
                "1 = cloud and dilated cloud bits are not set");
            strcpy (bmeta[i].bitmap_description[7],
                "Water (0 = land or cloud, 1 = for water");
            strcpy (bmeta[i].bitmap_description[8], "Cloud Confidence");
            strcpy (bmeta[i].bitmap_description[9], "Cloud Confidence");
            strcpy (bmeta[i].bitmap_description[10], "Cloud Shadow Confidence");
            strcpy (bmeta[i].bitmap_description[11], "Cloud Shadow Confidence");
            strcpy (bmeta[i].bitmap_description[12], "Snow/Ice Confidence");
            strcpy (bmeta[i].bitmap_description[13], "Snow/Ice Confidence");

            if (!strncmp (gmeta->instrument, "OLI", 3))
            {
                strcpy (bmeta[i].bitmap_description[14], "Cirrus Confidence");
                strcpy (bmeta[i].bitmap_description[15], "Cirrus Confidence");
            }
            else
            {
                strcpy (bmeta[i].bitmap_description[14], "Not used");
                strcpy (bmeta[i].bitmap_description[15], "Not used");
            }
        }
        else if (!strcmp (band_num[i], "qa_radsat"))
        {
            if (allocate_bitmap_metadata (&bmeta[i], 16) != SUCCESS)
            {
                sprintf (errmsg, "Allocating 16 bits for the bitmap");
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }

            strcpy (bmeta[i].bitmap_description[0],
                "Band 1 saturation (0 = no saturation, 1 = saturated data)");
            strcpy (bmeta[i].bitmap_description[1],
                "Band 2 saturation (0 = no saturation, 1 = saturated data)");
            strcpy (bmeta[i].bitmap_description[2],
                "Band 3 saturation (0 = no saturation, 1 = saturated data)");
            strcpy (bmeta[i].bitmap_description[3],
                "Band 4 saturation (0 = no saturation, 1 = saturated data)");
            strcpy (bmeta[i].bitmap_description[4],
                "Band 5 saturation (0 = no saturation, 1 = saturated data)");

            if (!strncmp (gmeta->instrument, "OLI", 3))
            {
                strcpy (bmeta[i].bitmap_description[5],
                    "Band 6 saturation (0 = no saturation, "
                    "1 = saturated data)");
                strcpy (bmeta[i].bitmap_description[8],
                    "Band 9 saturation (0 = no saturation, "
                    "1 = saturated data)");
                strcpy (bmeta[i].bitmap_description[9], "Not used");
                strcpy (bmeta[i].bitmap_description[11],
                    "Terrain occlusion (0 = no terrain occlusion, " 
                    "1 = terrain occlusion");
            }
            else if (!strncmp (gmeta->instrument, "ETM", 3))
            {
                strcpy (bmeta[i].bitmap_description[5],
                    "Band 6L saturation (0 = no saturation, "
                    "1 = saturated data)");
                strcpy (bmeta[i].bitmap_description[8],
                    "Band 6H saturation (0 = no saturation, "
                    "1 = saturated data)");
                strcpy (bmeta[i].bitmap_description[9], "Dropped Pixel");
                strcpy (bmeta[i].bitmap_description[11], "Not used");
            }
            else if (!strcmp (gmeta->instrument, "TM"))
            {
                strcpy (bmeta[i].bitmap_description[5],
                    "Band 6 saturation (0 = no saturation, "
                    "1 = saturated data)");
                strcpy (bmeta[i].bitmap_description[8], "Not used");
                strcpy (bmeta[i].bitmap_description[9], "Dropped Pixel");
                strcpy (bmeta[i].bitmap_description[11], "Not used");
            }

            strcpy (bmeta[i].bitmap_description[6],
                "Band 7 saturation (0 = no saturation, 1 = saturated data)");
            strcpy (bmeta[i].bitmap_description[7], "Not used");
            strcpy (bmeta[i].bitmap_description[10], "Not used");
            strcpy (bmeta[i].bitmap_description[12], "Not used");
            strcpy (bmeta[i].bitmap_description[13], "Not used");
            strcpy (bmeta[i].bitmap_description[14], "Not used");
            strcpy (bmeta[i].bitmap_description[15], "Not used");
        }
    }

    /* Close the metadata file */
    fclose (mtl_fptr);

    /* Get geolocation information from the XML file to prepare for computing
       the bounding coordinates */
    if (!get_geoloc_info (metadata, &geoloc_def))
    {
        sprintf (errmsg, "Copying the geolocation information from the XML "
            "metadata structure.");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Setup the mapping structure */
    geoloc_map = setup_mapping (&geoloc_def);
    if (geoloc_map == NULL)
    {
        sprintf (errmsg, "Setting up the geolocation mapping structure.");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }
    
    /* Compute the geographic bounds using the reflectance band coordinates */
    /* For ascending scenes and scenes in the polar regions, the scenes are
       flipped upside down.  The bounding coords will be correct in North
       represents the northernmost latitude and South represents the
       southernmost latitude.  However, the UL corner in this case would be
       more south than the LR corner.  Comparing the UL and LR corners will
       allow the user to determine if the scene is flipped. */
    if (!compute_bounds (geoloc_map, tmp_bmeta.nlines, tmp_bmeta.nsamps,
        &bounds))
    {
        sprintf (errmsg, "Setting up the geolocation mapping structure.");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }
    gmeta->bounding_coords[ESPA_WEST] = bounds.min_lon;
    gmeta->bounding_coords[ESPA_EAST] = bounds.max_lon;
    gmeta->bounding_coords[ESPA_NORTH] = bounds.max_lat;
    gmeta->bounding_coords[ESPA_SOUTH] = bounds.min_lat;

    /* Free the geolocation structure */
    free (geoloc_map);

    /* Successful read */
    return (SUCCESS);
}


/******************************************************************************
MODULE:  convert_gtif_to_img

PURPOSE: Convert the LPGS Cloud Optimized GeoTIFF band to ESPA raw binary
(.img) file and writes the associated ENVI header for each band.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error converting the GeoTIFF file
SUCCESS         Successfully converterd GeoTIFF to raw binary

NOTES:
  1. The GDAL tools will be used for converting the Cloud Optimized GeoTIFF
     to raw binary (ENVI format).
  2. An associated .tfw (ESRI world file) will be generated for each GeoTIFF
     file.
******************************************************************************/
int convert_gtif_to_img
(
    char *gtif_file,           /* I: name of input GeoTIFF file for this band */
    Espa_band_meta_t *bmeta,   /* I: pointer to band metadata for this band */
    Espa_global_meta_t *gmeta  /* I: pointer to global metadata */
)
{
    char FUNC_NAME[] = "convert_gtif_to_img";  /* function name */
    char errmsg[STR_SIZE];    /* error message */
    char gdal_cmd[STR_SIZE];  /* command string for GDAL call */
    char tmpfile[STR_SIZE];   /* filename of file.img.aux.xml */
    int count;                /* number of chars copied in snprintf */

    /* Check if the fill value is defined */
    if ((int) bmeta->fill_value == (int) ESPA_INT_META_FILL)
    {
        /* Fill value is not defined so don't write the nodata tag */
        count = snprintf (gdal_cmd, sizeof (gdal_cmd),
            "gdal_translate -of Envi -q %s %s", gtif_file, bmeta->file_name);
    }
    else
    {
        /* Fill value is defined so use the nodata tag */
        count = snprintf (gdal_cmd, sizeof (gdal_cmd),
         "gdal_translate -of Envi -a_nodata %ld -q %s %s",
            bmeta->fill_value, gtif_file, bmeta->file_name);
    }
    if (count < 0 || count >= sizeof (gdal_cmd))
    {
        sprintf (errmsg, "Overflow of gdal_cmd string");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }
 
    if (system (gdal_cmd) == -1)
    {
        sprintf (errmsg, "Running gdal_translate: %s", gdal_cmd);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }
 
    /* Remove the {gtif_name}.tif.aux.xml file since it's not needed and
       clutters the results.  Don't worry about testing the unlink
       results.  If it doesn't unlink it's not fatal. */
    count = snprintf (tmpfile, sizeof (tmpfile), "%s.aux.xml",
        bmeta->file_name);
    if (count < 0 || count >= sizeof (tmpfile))
    {
        sprintf (errmsg, "Overflow of tmpfile string");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }
    unlink (tmpfile);

    /* Successful conversion */
    return (SUCCESS);
}


/******************************************************************************
MODULE:  convert_lpgs_to_espa

PURPOSE: Converts the input LPGS GeoTIFF files (and associated MTL file) to
the ESPA internal raw binary file format (and associated XML file).

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error converting the GeoTIFF file
SUCCESS         Successfully converted GeoTIFF to raw binary

NOTES:
  1. The LPGS GeoTIFF band files will be deciphered from the LPGS MTL file.
  2. The ESPA raw binary band files will be generated from the ESPA XML
     filename.
******************************************************************************/
int convert_lpgs_to_espa
(
    char *lpgs_mtl_file,   /* I: input LPGS MTL metadata filename */
    char *espa_xml_file,   /* I: output ESPA XML metadata filename */
    bool del_src           /* I: should the source .tif files be removed after
                                 conversion? */
)
{
    char FUNC_NAME[] = "convert_lpgs_to_espa";  /* function name */
    char errmsg[STR_SIZE];   /* error message */
    Espa_internal_meta_t xml_metadata;  /* XML metadata structure to be
                                populated by reading the MTL metadata file */
    int i;                   /* looping variable */
    int nlpgs_bands;         /* number of bands in the LPGS product */
    char lpgs_bands[MAX_LPGS_BANDS][STR_SIZE];  /* array containing the file
                                names of the LPGS bands */

    /* Initialize the metadata structure */
    init_metadata_struct (&xml_metadata);

    /* Read the LPGS MTL file and populate our internal ESPA metadata
       structure */
    if (read_lpgs_mtl (lpgs_mtl_file, &xml_metadata, &nlpgs_bands,
        lpgs_bands) != SUCCESS)
    {
        sprintf (errmsg, "Reading the LPGS MTL file: %s", lpgs_mtl_file);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Write the metadata from our internal metadata structure to the output
       XML filename */
    if (write_metadata (&xml_metadata, espa_xml_file) != SUCCESS)
    {  /* Error messages already written */
        return (ERROR);
    }

    /* Validate the input metadata file */
    if (validate_xml_file (espa_xml_file) != SUCCESS)
    {  /* Error messages already written */
        return (ERROR);
    }

    /* Convert each of the LPGS GeoTIFF files to raw binary */
    for (i = 0; i < nlpgs_bands; i++)
    {
        printf ("  Band %d: %s to %s\n", i, lpgs_bands[i],
            xml_metadata.band[i].file_name);
        if (convert_gtif_to_img (lpgs_bands[i], &xml_metadata.band[i],
            &xml_metadata.global) != SUCCESS)
        {
            sprintf (errmsg, "Converting band %d: %s", i, lpgs_bands[i]);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Remove the source file if specified */
        if (del_src)
        {
            printf ("  Removing %s\n", lpgs_bands[i]);
            if (unlink (lpgs_bands[i]) != 0)
            {
                sprintf (errmsg, "Deleting source file: %s", lpgs_bands[i]);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
        }
    }

    /* Free the metadata structure */
    free_metadata (&xml_metadata);

    /* Successful conversion */
    return (SUCCESS);
}

