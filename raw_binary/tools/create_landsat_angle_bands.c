/*****************************************************************************
FILE: create_landsat_angle_bands

PURPOSE: Creates the Landsat 4-7 solar and view/satellite per-pixel angles for
the specified Landsat bands (4 - representative band).  Both the zenith and
azimuth angles are created for each angle type for each Landsat band.

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

/* Define the scaling factor */
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
    printf ("create_angle_bands creates the zenith and azimuth per-pixel "
            "values for the solar and view (satellite) angles.  These "
            "per-pixel angle values are only generated for band 4, which is "
            "the representative band for TM and ETM+.  Values are written in "
            "degrees and scaled by 100.\n\n");
    printf ("usage: create_angle_bands "
            "--xml=input_metadata_filename\n");

    printf ("\nwhere the following parameters are required:\n");
    printf ("    -xml: name of the input XML metadata file wich follows the "
            "ESPA internal raw binary schema\n");

    printf ("\nExample: create_angle_bands "
            "--xml=LE07_L1TP_022033_20140228_20161028_01_T1.xml\n");
    printf ("This writes a single band file for each of the bands (b4) for the "
            "solar azimuth/zenith and the satellite azimuth/zenith angles.\n");
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
    char **xml_infile     /* O: address of input XML filename */
)
{
    int c;                           /* current argument index */
    int option_index;                /* index for the command-line option */
    char errmsg[STR_SIZE];           /* error message */
    char FUNC_NAME[] = "get_args";   /* function name */
    static struct option long_options[] =
    {
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

    /* Make sure the XML file was specified */
    if (*xml_infile == NULL)
    {
        sprintf (errmsg, "Input XML file is a required argument");
        error_handler (true, FUNC_NAME, errmsg);
        usage ();
        return (ERROR);
    }

    return (SUCCESS);
}


#define MAX_DATE_LEN 28
/******************************************************************************
MODULE:  main

PURPOSE: Creates the Landsat solar and view/satellite per-pixel angles.  Both
the zenith and azimuth angles are created for each angle type for each
band.

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
3. The landsat_per_pixel_angles library expects an array of solar/satellite
   azimuth/zenith pointers for all the input bands, as that's the possible
   full list of output bands.  So, even though we are not processing all the
   bands in the output list, we still need to provide an array of that size.
   This requires us to keep track of the indices for each of the output bands
   into the array of input bands, for both ETM and TM arrays.
******************************************************************************/
int main (int argc, char** argv)
{
    char FUNC_NAME[] = "create_angle_bands";  /* function name */
    char errmsg[STR_SIZE];       /* error message */
    char tmpstr[STR_SIZE];       /* temporary string */
    char tmpfile[STR_SIZE];      /* temporary filename */
    char ang_infile[STR_SIZE];   /* input angle coefficient filename */
    char outfile[STR_SIZE];      /* output base filename for angle bands */
    char band_list[STR_SIZE];    /* char array of list of bands to process */
    char etm_list[] = "4";       /* list of ETM bands to process */
    char tm_list[] = "4";        /* list of TM bands to process */
    char production_date[MAX_DATE_LEN+1]; /* current date/year for production */
    char band_angle[NANGLE_BANDS][STR_SIZE] = {"solar zenith", "solar azimuth",
                                    "sensor zenith", "sensor azimuth"};
    char *cptr = NULL;           /* pointer to file extension */
    char *xml_infile = NULL;     /* input XML filename */
    bool process_l7 = false;     /* are we processing L7 vs. L4-5 */
    bool process_l45 = false;    /* are we processing L4-5 vs. L7 */
    int i;                       /* looping variable for bands */
    int curr_bnd;                /* current input band location */
    int curr_band;               /* current input band number */
    int curr_bndx;               /* index of current input band */
    int curr_index;              /* index of current output band in the input
                                    band array */
    int nbands_input;            /* number of input bands available */
    int etm_nbands = 1;          /* number of ETM bands to process for PPA */
    int tm_nbands = 1;           /* number of TM bands to process for PPA */
    int landsat_nbands;          /* number of bands to process for PPA */
    int *landsat_bands = NULL;   /* array of output bands to be processed;
                                    set to point to either etm_bands or
                                    tm_bands */
    int etm_bands[] = {4};       /* output bands to be processed */
    int tm_bands[] = {4};        /* output bands to be processed */
    int *band_indx = NULL;       /* array of indices for the output bands within
                                    the full set of input bands; set to point
                                    to either etm_band_indx or tm_band_indx */
    int etm_band_indx[] = {3};   /* index in the overall input bands for
                                    the output bands [1,2,3,4,5,61,62,7,8] */
    int tm_band_indx[] = {3};    /* index in the overall input bands for
                                    the output bands [1,2,3,4,5,6,7,8] */
    int out_nbands;              /* number of output bands to be written */
    int nlines[L7_NBANDS];       /* number of lines for each band */
    int nsamps[L7_NBANDS];       /* number of samples for each band */
    Angle_band_t ang;            /* looping variable for solar/senor angle */
    short *solar_zenith[L7_NBANDS];  /* array of pointers for the solar zenith
                                        angle array, one per band */
    short *solar_azimuth[L7_NBANDS]; /* array of pointers for the solar azimuth
                                        angle array, one per band */
    short *sat_zenith[L7_NBANDS];    /* array of pointers for the satellite
                                        zenith angle array, one per band */
    short *sat_azimuth[L7_NBANDS];   /* array of pointers for the satellite
                                        azimuth angle array, one per band */
    short *curr_angle = NULL;      /* pointer to the current angle array */
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
    if (get_args (argc, argv, &xml_infile) != SUCCESS)
    {   /* get_args already printed the error message */
        exit (ERROR);
    }
    printf ("Processing the per-pixel angle bands for L4-7 ...\n");

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

    /* Determine which instrument is being processed */
    if (!strncmp (gmeta->instrument, "ETM", 3))
        process_l7 = true;
    else
        process_l45 = true;

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
    if (process_l7)
        nbands_input = L7_NBANDS;
    else
        nbands_input = L45_NBANDS;

    /* Determine the number of output bands */
    if (process_l7)
    {
        landsat_bands = etm_bands;
        landsat_nbands = etm_nbands;
        out_nbands = landsat_nbands * NANGLE_BANDS;
        band_indx = etm_band_indx;
        strcpy (band_list, etm_list);
    }
    else
    {
        landsat_bands = tm_bands;
        landsat_nbands = tm_nbands;
        out_nbands = landsat_nbands * NANGLE_BANDS;
        band_indx = tm_band_indx;
        strcpy (band_list, tm_list);
    }

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

    /* Initialize the Landsat angle bands to NULL */
    init_per_pixel_angles (solar_zenith, solar_azimuth, sat_zenith,
        sat_azimuth);

    /* Create the Landsat angle bands for the specified bands.  Create a full
       resolution product. */
    if (landsat_per_pixel_angles (ang_infile, 1, band_list, solar_zenith,
        solar_azimuth, sat_zenith, sat_azimuth, nlines, nsamps) != SUCCESS)
    {  /* Error messages already written */
        free_per_pixel_angles (solar_zenith, solar_azimuth, sat_zenith,
            sat_azimuth);
        exit (ERROR);
    }

    /* Setup the XML file for these bands */
    for (i = 0; i < out_nbands; i++)
    {
        /* Set up the band metadata for the current band */
        out_bmeta = &out_meta.band[i];
        strcpy (out_bmeta->product, "intermediate_data");
        strcpy (out_bmeta->source, "level1");
        strcpy (out_bmeta->category, "image");

        /* Setup filename-related items for all four bands: solar zenith,
           solar azimuth, sensor zenith, sensor azimuth.  L4-5 and L8 band
           numbers follow a normal numbering scheme.  L7 band numbering
           needs a little help to get it correct. */
        curr_bnd = i / NANGLE_BANDS + 1;  /* current input band number */
        curr_bndx = curr_bnd - 1;         /* index of current input band */
        curr_band = landsat_bands[curr_bndx]; /* actual band number */
        curr_index = band_indx[curr_bndx];    /* index in output array */

        switch (i % NANGLE_BANDS)
        {
            case (SOLAR_ZEN):  /* solar zenith */
                /* Determine the output file for the solar zenith band */
                snprintf (tmpfile, sizeof (tmpfile),
                    "%s_B%d_solar_zenith.img", outfile, curr_band);
                sprintf (out_bmeta->name, "solar_zenith_band%d", curr_band);
                strncpy (tmpstr, bmeta->short_name, 3);
                sprintf (out_bmeta->short_name, "%sSOLZEN", tmpstr);
                sprintf (out_bmeta->long_name,
                    "band %d solar zenith angles", curr_band);
                break;

            case (SOLAR_AZ):  /* solar azimuth */
                /* Determine the output file for the solar azimuth band */
                snprintf (tmpfile, sizeof (tmpfile),
                    "%s_B%d_solar_azimuth.img", outfile, curr_band);
                sprintf (out_bmeta->name, "solar_azimuth_band%d",
                    curr_band);
                strncpy (tmpstr, bmeta->short_name, 3);
                sprintf (out_bmeta->short_name, "%sSOLAZ", tmpstr);
                sprintf (out_bmeta->long_name,
                    "band %d solar azimuth angles", curr_band);
                break;

            case (SENSOR_ZEN):  /* sensor zenith */
                /* Determine the output file for the sensor zenith band */
                snprintf (tmpfile, sizeof (tmpfile),
                    "%s_B%d_sensor_zenith.img", outfile, curr_band);
                sprintf (out_bmeta->name, "sensor_zenith_band%d",
                    curr_band);
                strncpy (tmpstr, bmeta->short_name, 3);
                sprintf (out_bmeta->short_name, "%sSENZEN", tmpstr);
                sprintf (out_bmeta->long_name,
                    "band %d sensor zenith angles", curr_band);
                break;

            case (SENSOR_AZ):  /* sensor azimuth */
                /* Determine the output file for the sensor azimuth band */
                snprintf (tmpfile, sizeof (tmpfile),
                    "%s_B%d_sensor_azimuth.img", outfile, curr_band);
                sprintf (out_bmeta->name, "sensor_azimuth_band%d",
                    curr_band);
                strncpy (tmpstr, bmeta->short_name, 3);
                sprintf (out_bmeta->short_name, "%sSENAZ", tmpstr);
                sprintf (out_bmeta->long_name,
                    "band %d sensor azimuth angles", curr_band);
                break;
        }

        snprintf (out_bmeta->file_name, sizeof (out_bmeta->file_name), "%s",
            tmpfile);
        out_bmeta->data_type = ESPA_INT16;
        out_bmeta->scale_factor = ANGLE_BAND_SCALE_FACT;
        strcpy (out_bmeta->data_units, "degrees");
        out_bmeta->nlines = nlines[curr_index];
        out_bmeta->nsamps = nsamps[curr_index];
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
        for (i = 0; i < landsat_nbands; i++)
        {
            /* Determine the index of this band in the overall list of input
               bands */
            curr_index = band_indx[i];

            /* Grab the correct data array to be written for this angle
               band */
            switch (ang)
            {
                case (SOLAR_ZEN):
                    curr_angle = &solar_zenith[curr_index][0];
                    break;
                case (SOLAR_AZ):
                    curr_angle = &solar_azimuth[curr_index][0];
                    break;
                case (SENSOR_ZEN):
                    curr_angle = &sat_zenith[curr_index][0];
                    break;
                case (SENSOR_AZ):
                    curr_angle = &sat_azimuth[curr_index][0];
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
                free_per_pixel_angles (solar_zenith, solar_azimuth, sat_zenith,
                    sat_azimuth);
                sprintf (errmsg, "Unable to open the %s file",
                    band_angle[ang]);
                error_handler (true, FUNC_NAME, errmsg);
                exit (ERROR);
            }

            /* Write the data for this band */
            if (write_raw_binary (fptr, nlines[curr_index], nsamps[curr_index],
                sizeof (short), curr_angle) != SUCCESS)
            {
                free_per_pixel_angles (solar_zenith, solar_azimuth, sat_zenith,
                    sat_azimuth);
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
                free_per_pixel_angles (solar_zenith, solar_azimuth, sat_zenith,
                    sat_azimuth);
                sprintf (errmsg, "Error creating the ENVI header file.");
                error_handler (true, FUNC_NAME, errmsg);
                exit (ERROR);
            }

            /* Write the ENVI header */
            sprintf (tmpfile, "%s", out_bmeta->file_name);
            sprintf (&tmpfile[strlen(tmpfile)-3], "hdr");
            if (write_envi_hdr (tmpfile, &envi_hdr) != SUCCESS)
            {
                free_per_pixel_angles (solar_zenith, solar_azimuth, sat_zenith,
                    sat_azimuth);
                sprintf (errmsg, "Writing the ENVI header file: %s.",
                    tmpfile);
                error_handler (true, FUNC_NAME, errmsg);
                exit (ERROR);
            }
        }  /* for i < landsat_nbands */
    }  /* for ang < NANGLE_BANDS */

    /* Free the pointers */
    free_per_pixel_angles (solar_zenith, solar_azimuth, sat_zenith,
        sat_azimuth);

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
