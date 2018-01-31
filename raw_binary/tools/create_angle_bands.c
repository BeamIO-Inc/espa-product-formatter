/*****************************************************************************
FILE: create_angle_bands

PURPOSE: Creates the Landsat 8 solar and view/satellite per-pixel angles.
Both the zenith and azimuth angles are created for each angle type for each
Landsat band or for the average of the Landsat reflective bands.

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

LICENSE TYPE:  NASA Open Source Agreement Version 1.3

NOTES:
*****************************************************************************/
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "error_handler.h"
#include "envi_header.h"
#include "parse_metadata.h"
#include "write_metadata.h"
#include "landsat_angles.h"
#include "l8_angles.h"

/* Define the band information for each of the instruments.  Currently the
   maximum number of input bands is the number of bands for L8. */
#define MAX_NBANDS L8_NBANDS

/* Define the fill value and the scaling factors (offsets the scale applied
   in the  */
#define ANGLE_BAND_FILL -32768
#define ANGLE_BAND_SCALE_FACT 0.01

/* Define the solar/sensor angle band indices */
typedef enum
{
    SOLAR_ZEN = 0,
    SOLAR_AZ,
    SENSOR_ZEN,
    SENSOR_AZ,
    NANGLE_BANDS
} Angle_band_t;

/******************************************************************************
MODULE: usage

PURPOSE: Prints the usage information for this application.

RETURN VALUE:
Type = None

NOTES:
******************************************************************************/
void usage ()
{
    printf ("create_angle_bands creates the Landsat 8 solar and view "
            "(satellite) per-pixel angles for each band or for an average of "
            "the reflective bands.  Both the zenith and azimuth angles are "
            "created for each angle.  Values are written in degrees and scaled "
            "by 100.\n\n");
    printf ("usage: create_angle_bands "
            "--xml=input_metadata_filename\n"
            "{--average}");

    printf ("\nwhere the following parameters are required:\n");
    printf ("    -xml: name of the input XML metadata file wich follows the "
            "ESPA internal raw binary schema\n");
    printf ("    -average: write the reflectance band averages instead of "
            "writing each of the band angles\n\n");

    printf ("\nExample: create_angle_bands "
            "--xml=LC08_L1TP_047027_20131014_20170308_01_T1.xml\n");
    printf ("This writes a single band file for each of the bands for the "
            "solar azimuth/zenith and the satellite azimuth/zenith angles.\n");

    printf ("\nExample: create_angle_bands "
            "--xml=LC08_L1TP_047027_20131014_20170308_01_T1.xml --average\n");
    printf ("This writes an average band file for the solar azimuth/zenith "
            "and the satellite azimuth/zenith angles.\n");
}


/******************************************************************************
MODULE:  get_args

PURPOSE:  Gets the command-line arguments and validates that the required
arguments were specified.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error getting the command-line arguments or a command-line
                argument and associated value were not specified
SUCCESS         No errors encountered

NOTES:
  1. Memory is allocated for the input and output files.  All of these should
     be character pointers set to NULL on input.  The caller is responsible
     for freeing the allocated memory upon successful return.
******************************************************************************/
short get_args
(
    int argc,             /* I: number of cmd-line args */
    char *argv[],         /* I: string of cmd-line args */
    char **xml_infile,    /* O: address of input XML filename */
    bool *band_avg        /* O: should the reflectance band average be
                                processed? */
)
{
    int c;                           /* current argument index */
    int option_index;                /* index for the command-line option */
    static int avg_flag = 0;         /* flag to indicate if the band average
                                        should be processed */
    char errmsg[STR_SIZE];           /* error message */
    char FUNC_NAME[] = "get_args";   /* function name */
    static struct option long_options[] =
    {
        {"average", no_argument, &avg_flag, 1},
        {"xml", required_argument, 0, 'i'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    /* Loop through all the cmd-line options */
    opterr = 0;   /* turn off getopt_long error msgs as we'll print our own */
    while (1)
    {
        /* optstring in call to getopt_long is empty since we will only
           support the long options */
        c = getopt_long (argc, argv, "", long_options, &option_index);
        if (c == -1)
        {   /* Out of cmd-line options */
            break;
        }

        switch (c)
        {
            case 0:
                /* If this option set a flag, do nothing else now. */
                if (long_options[option_index].flag != 0)
                    break;
     
            case 'h':  /* help */
                usage ();
                return (ERROR);
                break;

            case 'i':  /* XML file */
                *xml_infile = strdup (optarg);
                break;
     
            case '?':
            default:
                sprintf (errmsg, "Unknown option %s", argv[optind-1]);
                error_handler (true, FUNC_NAME, errmsg);
                usage ();
                return (ERROR);
                break;
        }
    }

    /* Make sure the infiles and outfiles were specified */
    if (*xml_infile == NULL)
    {
        sprintf (errmsg, "Input XML file is a required argument");
        error_handler (true, FUNC_NAME, errmsg);
        usage ();
        return (ERROR);
    }

    /* Check the band average flag */
    if (avg_flag)
        *band_avg = true;

    return (SUCCESS);
}


#define MAX_DATE_LEN 28
/******************************************************************************
MODULE:  main

PURPOSE: Creates the Landsat solar and view/satellite per-pixel angles.  Both
the zenith and azimuth angles are created for each angle type for each
band.  An option is supported for OLI to write the average of the reflectance
bands for each angle instead of writing the angle for each band.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error creating the angle bands
SUCCESS         No errors encountered

NOTES:
1. Angles are written in degrees and scaled by 100.
2. There are 4 bands written per input band (or average): solar zenith, solar
   azimuth, sensor zenith, sensor azimuth.
3. Note this is a memory hog.  The goal here was to have an application to
   be able to write out the per-pixel angle bands for testing the per-pixel
   angles.  In order to make this a less memory hog, then break it down to
   process the solar angles, write the solar angles, process the
   satellite/sensor/view angles, and write the satellite/sensor/view angles.
******************************************************************************/
int main (int argc, char** argv)
{
    char FUNC_NAME[] = "create_angle_bands";  /* function name */
    char errmsg[STR_SIZE];       /* error message */
    char tmpstr[STR_SIZE];       /* temporary string */
    char tmpfile[STR_SIZE];      /* temporary filename */
    char ang_infile[STR_SIZE];   /* input angle coefficient filename */
    char outfile[STR_SIZE];      /* output base filename for angle bands */
    char production_date[MAX_DATE_LEN+1]; /* current date/year for production */
    char band_angle[NANGLE_BANDS][STR_SIZE] = {"solar zenith", "solar azimuth",
                                    "sensor zenith", "sensor azimuth"};
    char *cptr = NULL;           /* pointer to file extension */
    char *xml_infile = NULL;     /* input XML filename */
    bool band_avg = false;       /* should the reflectance band average be
                                    processed? */
    bool process_l8 = false;     /* are we processing L8 vs. L4-7 */
    bool process_l7 = false;     /* are we processing L7 vs. L4-5 or L8 */
    bool process_l45 = false;    /* are we processing L4-5 vs. L7 or L8 */
    int i;                       /* looping variable for bands */
    int count;                   /* number of chars copied in snprintf */
    int curr_band;               /* current input band number */
    int curr_bndx;               /* index of current input band */
    int nbands;                  /* number of input bands to be read */
    int out_nbands;              /* number of output bands to be written */
    int nlines[MAX_NBANDS];      /* number of lines for each band */
    int nsamps[MAX_NBANDS];      /* number of samples for each band */
    int avg_nlines;              /* number of lines for band average */
    int avg_nsamps;              /* number of samples for band average */
    int l7_bands[] = {1, 2, 3, 4, 5, 61, 62, 7, 8}; /* Landsat 7 band numbers */
    Angle_band_t ang;            /* looping variable for solar/senor angle */
    ANGLES_FRAME frame[MAX_NBANDS];   /* image frame info for each band */
    short *solar_zenith[MAX_NBANDS];  /* array of pointers for the solar zenith
                                         angle array, one per band */
    short *solar_azimuth[MAX_NBANDS]; /* array of pointers for the solar azimuth
                                         angle array, one per band */
    short *sat_zenith[MAX_NBANDS];    /* array of pointers for the satellite
                                         zenith angle array, one per band */
    short *sat_azimuth[MAX_NBANDS];   /* array of pointers for the satellite
                                         azimuth angle array, one per band */
    short *curr_angle = NULL;      /* pointer to the current angle array */
    ANGLES_FRAME avg_frame;        /* image frame info for band average */
    short *avg_solar_zenith=NULL;  /* array for solar zenith angle average */
    short *avg_solar_azimuth=NULL; /* array for solar azimuth angle average */
    short *avg_sat_zenith=NULL;    /* array for satellite zenith angle avg */
    short *avg_sat_azimuth=NULL;   /* array for satellite azimuth angle avg */
    time_t tp;                     /* time structure */
    struct tm *tm = NULL;          /* time structure for UTC time */
    FILE *fptr=NULL;               /* file pointer */
    Envi_header_t envi_hdr;        /* output ENVI header information */
    Espa_internal_meta_t xml_metadata;
                                   /* XML metadata structure to be populated by
                                      reading the input XML metadata file */
    Espa_band_meta_t *bmeta=NULL;    /* pointer to array of bands metadata */
    Espa_global_meta_t *gmeta=NULL;  /* pointer to the global metadata struct */
    Espa_band_meta_t *out_bmeta = NULL; /* band metadata for angle bands */
    Espa_internal_meta_t out_meta;      /* output metadata for angle bands */

    /* Read the command-line arguments */
    if (get_args (argc, argv, &xml_infile, &band_avg) != SUCCESS)
    {   /* get_args already printed the error message */
        exit (ERROR);
    }

    /* Validate the input metadata file */
    if (validate_xml_file (xml_infile) != SUCCESS)
    {  /* Error messages already written */
        return (ERROR);
    }

    /* Initialize the metadata structure */
    init_metadata_struct (&xml_metadata);

    /* Parse the metadata file into our internal metadata structure; also
       allocates space as needed for various pointers in the global and band
       metadata */
    if (parse_metadata (xml_infile, &xml_metadata) != SUCCESS)
    {  /* Error messages already written */
        return (ERROR);
    }
    bmeta = xml_metadata.band;
    gmeta = &xml_metadata.global;

    /* Determine if L8 is being processed */
    if (!strncmp (gmeta->instrument, "OLI", 3))
        process_l8 = true;
    else if (!strncmp (gmeta->instrument, "ETM", 3))
        process_l7 = true;
    else
        process_l45 = true;

    /* If processing L4-7 then the band average is not supported */
    if ((process_l7 || process_l45) && band_avg)
    {
        sprintf (errmsg, "Band average is only supported for Landsat 8");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Determine the angle coefficient filename and the output file basename */
    strcpy (ang_infile, xml_infile);
    cptr = strchr (ang_infile, '.');
    strcpy (cptr, "_ANG.txt");

    strcpy (outfile, xml_infile);
    cptr = strchr (outfile, '.');
    *cptr = '\0';

    /* Initialize the output metadata structure.  The global metadata will
       not be used and will not be valid. */
    init_metadata_struct (&out_meta);

    /* Determine the number of input bands */
    if (process_l8)
        nbands = L8_NBANDS;
    else if (process_l7)
        nbands = L7_NBANDS;
    else
        nbands = L45_NBANDS;

    /* Determine the number of output bands */
    if (band_avg)
        out_nbands = NANGLE_BANDS;
    else if (process_l8)
        out_nbands = L8_NBANDS * NANGLE_BANDS;
    else if (process_l7)
        out_nbands = L7_NBANDS * NANGLE_BANDS;
    else
        out_nbands = L45_NBANDS * NANGLE_BANDS;

    /* Allocate memory for the output bands */
    if (allocate_band_metadata (&out_meta, out_nbands) != SUCCESS)
    {
        sprintf (errmsg, "Cannot allocate memory for the %d angle bands",
            out_nbands);
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /* Get the current date/time (UTC) for the production date of each band */
    if (time (&tp) == -1)
    {
        sprintf (errmsg, "Unable to obtain the current time.");
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    tm = gmtime (&tp);
    if (tm == NULL)
    {
        sprintf (errmsg, "Converting time to UTC.");
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    if (strftime (production_date, MAX_DATE_LEN, "%Y-%m-%dT%H:%M:%SZ", tm) == 0)
    {
        sprintf (errmsg, "Formatting the production date/time.");
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /* Process the solar/sensor angle bands */
    if (!band_avg)
    {
        /* Create the Landsat angle bands for all bands.  Create a full
           resolution product with a fill value to match the Landsat image
           data. */
        if (process_l8)
        {  /* Landsat 8 */
            if (l8_per_pixel_angles (ang_infile, 1, ANGLE_BAND_FILL, "ALL",
                frame, solar_zenith, solar_azimuth, sat_zenith, sat_azimuth,
                nlines, nsamps) != SUCCESS)
            {  /* Error messages already written */
                exit (ERROR);
            }
        }
        else
        {  /* Landsat 4-7 */
            if (landsat_per_pixel_angles (ang_infile, 1, "ALL", solar_zenith,
                solar_azimuth, sat_zenith, sat_azimuth, nlines, nsamps) !=
                SUCCESS)
            {  /* Error messages already written */
                exit (ERROR);
            }
        }

        /* Setup the XML file for these bands */
        for (i = 0; i < out_nbands; i++)
        {
            /* Set up the band metadata for the current band */
            out_bmeta = &out_meta.band[i];
            strcpy (out_bmeta->product, "angle_bands");
            strcpy (out_bmeta->source, "level1");
            strcpy (out_bmeta->category, "image");

            /* Setup filename-related items for all four bands: solar zenith,
               solar azimuth, sensor zenith, sensor azimuth.  L4-5 and L8 band
               numbers follow a normal numbering scheme.  L7 band numbering
               needs a little help to get it correct. */
            curr_band = i / NANGLE_BANDS + 1;  /* current input band number */
            curr_bndx = curr_band - 1;   /* index of current input band */
            if (process_l7)
                curr_band = l7_bands[curr_bndx];
            switch (i % NANGLE_BANDS)
            {
                case (SOLAR_ZEN):  /* solar zenith */
                    /* Determine the output file for the solar zenith band */
                    snprintf (tmpfile, sizeof (tmpfile),
                        "%s_b%d_solar_zenith.img", outfile, curr_band);
                    sprintf (out_bmeta->name, "solar_zenith_band%d", curr_band);
                    strncpy (tmpstr, bmeta->short_name, 4);
                    sprintf (out_bmeta->short_name, "%sSOLZEN", tmpstr);
                    sprintf (out_bmeta->long_name,
                        "band %d solar zenith angles", curr_band);
                    break;

                case (SOLAR_AZ):  /* solar azimuth */
                    /* Determine the output file for the solar azimuth band */
                    snprintf (tmpfile, sizeof (tmpfile),
                        "%s_b%d_solar_azimuth.img", outfile, curr_band);
                    sprintf (out_bmeta->name, "solar_azimuth_band%d",
                        curr_band);
                    strncpy (tmpstr, bmeta->short_name, 4);
                    sprintf (out_bmeta->short_name, "%sSOLAZ", tmpstr);
                    sprintf (out_bmeta->long_name,
                        "band %d solar azimuth angles", curr_band);
                    break;

                case (SENSOR_ZEN):  /* sensor zenith */
                    /* Determine the output file for the sensor zenith band */
                    snprintf (tmpfile, sizeof (tmpfile),
                        "%s_b%d_sensor_zenith.img", outfile, curr_band);
                    sprintf (out_bmeta->name, "sensor_zenith_band%d",
                        curr_band);
                    strncpy (tmpstr, bmeta->short_name, 4);
                    sprintf (out_bmeta->short_name, "%sSENZEN", tmpstr);
                    sprintf (out_bmeta->long_name,
                        "band %d sensor zenith angles", curr_band);
                    break;

                case (SENSOR_AZ):  /* sensor azimuth */
                    /* Determine the output file for the sensor azimuth band */
                    snprintf (tmpfile, sizeof (tmpfile),
                        "%s_b%d_sensor_azimuth.img", outfile, curr_band);
                    sprintf (out_bmeta->name, "sensor_azimuth_band%d",
                        curr_band);
                    strncpy (tmpstr, bmeta->short_name, 4);
                    sprintf (out_bmeta->short_name, "%sSENAZ", tmpstr);
                    sprintf (out_bmeta->long_name,
                        "band %d sensor azimuth angles", curr_band);
                    break;
            }

            snprintf (out_bmeta->file_name, sizeof (out_bmeta->file_name), "%s",
                tmpfile);
            out_bmeta->data_type = ESPA_INT16;
            if (process_l8)  /* only L8 has fill */
                out_bmeta->fill_value = ANGLE_BAND_FILL;
            out_bmeta->scale_factor = ANGLE_BAND_SCALE_FACT;
            strcpy (out_bmeta->data_units, "degrees");
            out_bmeta->nlines = nlines[curr_bndx];
            out_bmeta->nsamps = nsamps[curr_bndx];
            out_bmeta->pixel_size[0] = bmeta[curr_bndx].pixel_size[0];
            out_bmeta->pixel_size[1] = bmeta[curr_bndx].pixel_size[1];
            strcpy (out_bmeta->pixel_units, bmeta[curr_bndx].pixel_units);
            sprintf (out_bmeta->app_version, "create_angle_bands_%s",
                ESPA_COMMON_VERSION);
            strcpy (out_bmeta->production_date, production_date);
        }

        /* Loop through the four different angle files and write them for each
           band */
        for (ang = 0; ang < NANGLE_BANDS; ang++)
        {
            /* Write the angle bands */
            printf ("Writing %s angles ...\n", band_angle[ang]);
            for (i = 0; i < nbands; i++)
            {
                /* Grab the correct data array to be written for this angle
                   band */
                switch (ang)
                {
                    case (SOLAR_ZEN):
                        curr_angle = &solar_zenith[i][0];
                        break;
                    case (SOLAR_AZ):
                        curr_angle = &solar_azimuth[i][0];
                        break;
                    case (SENSOR_ZEN):
                        curr_angle = &sat_zenith[i][0];
                        break;
                    case (SENSOR_AZ):
                        curr_angle = &sat_azimuth[i][0];
                        break;
                    default:
                        sprintf (errmsg, "Invalid angle type %d", ang);
                        error_handler (true, FUNC_NAME, errmsg);
                        exit (ERROR);
                }

                /* Open the output file for this band */
                out_bmeta = &out_meta.band[i*NANGLE_BANDS + ang];
                fptr = open_raw_binary (out_bmeta->file_name, "wb");
                if (!fptr)
                {
                    sprintf (errmsg, "Unable to open the %s file",
                        band_angle[ang]);
                    error_handler (true, FUNC_NAME, errmsg);
                    exit (ERROR);
                }

                /* Write the data for this band */
                if (write_raw_binary (fptr, nlines[i], nsamps[i],
                    sizeof (short), curr_angle) != SUCCESS)
                {
                    sprintf (errmsg, "Unable to write to the %s file",
                        band_angle[ang]);
                    error_handler (true, FUNC_NAME, errmsg);
                    exit (ERROR);
                }

                /* Close the file for this band */
                close_raw_binary (fptr);

                /* Create the ENVI header */
                if (create_envi_struct (out_bmeta, gmeta, &envi_hdr) != SUCCESS)
                {
                    sprintf (errmsg, "Error creating the ENVI header file.");
                    error_handler (true, FUNC_NAME, errmsg);
                    exit (ERROR);
                }

                /* Write the ENVI header */
                sprintf (tmpfile, "%s", out_bmeta->file_name);
                sprintf (&tmpfile[strlen(tmpfile)-3], "hdr");
                if (write_envi_hdr (tmpfile, &envi_hdr) != SUCCESS)
                {
                    sprintf (errmsg, "Writing the ENVI header file: %s.",
                        tmpfile);
                    error_handler (true, FUNC_NAME, errmsg);
                    exit (ERROR);
                }
            }  /* for i < nbands */
        }  /* for ang < NANGLE_BANDS */

        /* Free the pointers */
        for (i = 0; i < nbands; i++)
        {
            free (solar_zenith[i]);
            free (solar_azimuth[i]);
            free (sat_zenith[i]);
            free (sat_azimuth[i]);
        }
    }  /* if !band_avg */
    else
    {
        /* Create the average Landsat angle bands over the reflectance bands.
           Create a full resolution product with a fill value to match the
           Landsat image data. */
        if (process_l8)
        {  /* Landsat 8 */
            if (l8_per_pixel_avg_refl_angles (ang_infile, 1, ANGLE_BAND_FILL,
                &avg_frame, &avg_solar_zenith, &avg_solar_azimuth,
                &avg_sat_zenith, &avg_sat_azimuth, &avg_nlines, &avg_nsamps) !=
                SUCCESS)
            {  /* Error messages already written */
                exit (ERROR);
            }
        }
        else
        {  /* Landsat 4-7 */
           /* TODO HANDLE THIS */
            sprintf (errmsg, "Only Landsat 8 is currently supported for band "
                "averages on the solar/view angles");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Setup the XML file for these bands */
        for (i = 0; i < out_nbands; i++)
        {
            /* Set up the band metadata for the current band */
            out_bmeta = &out_meta.band[i];
            strcpy (out_bmeta->product, "angle_bands");
            strcpy (out_bmeta->source, "level1");
            strcpy (out_bmeta->category, "image");

            /* Setup filename-related items for all four bands: solar zenith,
               solar azimuth, sensor zenith, sensor azimuth */
            switch (i)
            {
                case (SOLAR_ZEN):  /* solar zenith */
                    /* Determine the output file for the solar zenith band */
                    count = snprintf (tmpfile, sizeof (tmpfile),
                        "%s_avg_solar_zenith.img", outfile);
                    sprintf (out_bmeta->name, "avg_solar_zenith_band");
                    strncpy (tmpstr, bmeta->short_name, 4);
                    sprintf (out_bmeta->short_name, "%sSOLZEN", tmpstr);
                    sprintf (out_bmeta->long_name,
                        "average solar zenith angles");
                    break;

                case (SOLAR_AZ):  /* solar zenith */
                    /* Determine the output file for the solar azimuth band */
                    count = snprintf (tmpfile, sizeof (tmpfile),
                        "%s_avg_solar_azimuth.img", outfile);
                    sprintf (out_bmeta->name, "avg_solar_azimuth_band");
                    strncpy (tmpstr, bmeta->short_name, 4);
                    sprintf (out_bmeta->short_name, "%sSOLAZ", tmpstr);
                    sprintf (out_bmeta->long_name,
                        "average solar azimuth angles");
                    break;

                case (SENSOR_ZEN):  /* sensor zenith */
                    /* Determine the output file for the sensor zenith band */
                    count = snprintf (tmpfile, sizeof (tmpfile),
                        "%s_avg_sensor_zenith.img", outfile);
                    sprintf (out_bmeta->name, "avg_sensor_zenith_band");
                    strncpy (tmpstr, bmeta->short_name, 4);
                    sprintf (out_bmeta->short_name, "%sSENZEN", tmpstr);
                    sprintf (out_bmeta->long_name,
                        "average sensor zenith angles");
                    break;

                case (SENSOR_AZ):  /* sensor azimuth */
                    /* Determine the output file for the sensor azimuth band */
                    count = snprintf (tmpfile, sizeof (tmpfile),
                        "%s_avg_sensor_azimuth.img", outfile);
                    sprintf (out_bmeta->name, "avg_sensor_azimuth_band");
                    strncpy (tmpstr, bmeta->short_name, 4);
                    sprintf (out_bmeta->short_name, "%sSENAZ", tmpstr);
                    sprintf (out_bmeta->long_name,
                        "average sensor azimuth angles");
                    break;
            }

            snprintf (out_bmeta->file_name, sizeof (out_bmeta->file_name), "%s",
                tmpfile);
            out_bmeta->data_type = ESPA_INT16;
            out_bmeta->fill_value = ANGLE_BAND_FILL;
            out_bmeta->scale_factor = ANGLE_BAND_SCALE_FACT;
            strcpy (out_bmeta->data_units, "degrees");
            out_bmeta->nlines = avg_nlines;
            out_bmeta->nsamps = avg_nsamps;
            out_bmeta->pixel_size[0] = bmeta[0].pixel_size[0];
            out_bmeta->pixel_size[1] = bmeta[0].pixel_size[1];
            strcpy (out_bmeta->pixel_units, bmeta[0].pixel_units);
            sprintf (out_bmeta->app_version, "create_angle_bands_%s",
                ESPA_COMMON_VERSION);
            strcpy (out_bmeta->production_date, production_date);
        }

        /* Loop through the four different angle bands and write them */
        for (ang = 0; ang < NANGLE_BANDS; ang++)
        {
            printf ("Writing %s band average angle ...\n", band_angle[ang]);

            /* Grab the correct data array to be written for this angle
               band */
            switch (ang)
            {
                case (SOLAR_ZEN):
                    curr_angle = avg_solar_zenith;
                    break;
                case (SOLAR_AZ):
                    curr_angle = avg_solar_azimuth;
                    break;
                case (SENSOR_ZEN):
                    curr_angle = avg_sat_zenith;
                    break;
                case (SENSOR_AZ):
                    curr_angle = avg_sat_azimuth;
                    break;
                default:
                    sprintf (errmsg, "Invalid angle type %d", ang);
                    error_handler (true, FUNC_NAME, errmsg);
                    exit (ERROR);
            }

            /* Open the output file for this angle */
            out_bmeta = &out_meta.band[ang];
            fptr = open_raw_binary (out_bmeta->file_name, "wb");
            if (!fptr)
            {
                sprintf (errmsg, "Unable to open the average %s file",
                    band_angle[ang]);
                error_handler (true, FUNC_NAME, errmsg);
                exit (ERROR);
            }
    
            /* Write the data for this band */
            if (write_raw_binary (fptr, avg_nlines, avg_nsamps, sizeof (short),
                curr_angle) != SUCCESS)
            {
                sprintf (errmsg, "Unable to write to average %s file",
                    band_angle[ang]);
                error_handler (true, FUNC_NAME, errmsg);
                exit (ERROR);
            }

            /* Close the file and free the memory for this angle */
            close_raw_binary (fptr);
            free (curr_angle);

            /* Create the ENVI header */
            if (create_envi_struct (out_bmeta, gmeta, &envi_hdr) != SUCCESS)
            {   
                sprintf (errmsg, "Error creating the ENVI header file.");
                error_handler (true, FUNC_NAME, errmsg);
                exit (ERROR);
            }
    
            /* Write the ENVI header */
            sprintf (tmpfile, "%s", out_bmeta->file_name);
            sprintf (&tmpfile[strlen(tmpfile)-3], "hdr");
            if (write_envi_hdr (tmpfile, &envi_hdr) != SUCCESS)
            {
                sprintf (errmsg, "Writing the ENVI header file: %s.", tmpfile);
                error_handler (true, FUNC_NAME, errmsg);
                exit (ERROR);
            }
        }  /* for ang < NANGLE_BANDS */
    }  /* else (if !band_avg) */

    /* Append the solar/sensor angle bands to the XML file */
    if (append_metadata (out_nbands, out_meta.band, xml_infile) != SUCCESS)
    {
        sprintf (errmsg, "Appending solar/sensor angle bands to the XML file.");
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /* Free the input and output XML metadata */
    free_metadata (&xml_metadata);
    free_metadata (&out_meta);

    /* Free the pointers */
    free (xml_infile);

    /* Successful completion */
    exit (SUCCESS);
}
