/*****************************************************************************
FILE: test_read_gtif.c
  
PURPOSE: Contains functions for reading/writing the ARD tiles as part of
testing the GeoTiff/Tiff libraries.

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

LICENSE TYPE:  NASA Open Source Agreement Version 1.3

NOTES:
  1. The XML metadata format parsed or written via this library follows the
     ESPA internal metadata format found in ESPA Raw Binary Format v1.0.doc.
     The schema for the ESPA internal metadata format is available at
     http://espa.cr.usgs.gov/schema/espa_internal_metadata_vX_X.xsd.
*****************************************************************************/
#include <getopt.h>
#include "tiff_io.h"

/******************************************************************************
MODULE: usage

PURPOSE: Prints the usage information for this application.

RETURN VALUE:
Type = None

NOTES:
******************************************************************************/
void usage ()
{
    printf ("test_read_gtif parses the XML, reads the Tiff files, and writes "
            "back out the GeoTiff test files to duplicate each band.\n\n");
    printf ("usage: test_read_gtif --xml=xml_filename\n");

    printf ("\nwhere the following parameters are required:\n");
    printf ("    -xml: name of the input XML metadata file which follows "
            "the ESPA internal raw binary schema\n");
    printf ("\nExample: test_read_gtif "
            "--xml=LE07_L1TP_022033_20140228_20160905_01_T1.xml\n");
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

            case 'i':  /* XML infile */
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

    /* Make sure the infile was specified */
    if (*xml_infile == NULL)
    {
        sprintf (errmsg, "XML input file is a required argument");
        error_handler (true, FUNC_NAME, errmsg);
        usage ();
        return (ERROR);
    }

    return (SUCCESS);
}


/******************************************************************************
MODULE:  main

PURPOSE:  Test program for ARD products to parse the input XML, loop through
the GeoTiff bands, read the Tiff, write the Tiff, and write the Tiff/GeoTiff
geolocation tags.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error processing the ARD product
SUCCESS         No errors encountered

NOTES:
******************************************************************************/
int main (int argc, char** argv)
{
    char FUNC_NAME[] = "test_read_gtif"; /* function name */
    char errmsg[STR_SIZE];             /* error message */
    char outname[STR_SIZE];            /* output band name */
    char *xml_infile = NULL;           /* input XML filename */
    int status;                        /* return status */
    int i;                             /* looping variable */
    void *band_buffer = NULL;          /* image buffer for the current band */
    Espa_internal_meta_t xml_metadata; /* XML metadata structure to be
                                          populated by reading the XML
                                          metadata file */
    Espa_global_meta_t *gmeta = NULL;  /* pointer to the global metadata
                                          structure */
    Espa_band_meta_t *bmeta = NULL;    /* pointer to current band metadata */
    TIFF *tif_fptr = NULL;             /* file pointer for Tiff file */

    /* Read the command-line arguments */
    if (get_args (argc, argv, &xml_infile) != SUCCESS)
    {   /* get_args already printed the error message */
        exit (EXIT_FAILURE);
    }

    /* Validate the input metadata file */
    if (validate_xml_file (xml_infile) != SUCCESS)
    {  /* Error messages already written */
        exit (EXIT_FAILURE);
    }

    /* Initialize the metadata structure */
    init_metadata_struct (&xml_metadata);

    /* Parse the metadata file into our internal metadata structure; also
       allocates space as needed for various pointers in the global and band
       metadata */
    if (parse_metadata (xml_infile, &xml_metadata) != SUCCESS)
    {  /* Error messages already written */
        exit (EXIT_FAILURE);
    }
    gmeta = &xml_metadata.global;

    /* Loop through each band in the XML file */
    for (i = 0; i < xml_metadata.nbands; i++)
    {
        /* Allocate memory for the current band */
        bmeta = &xml_metadata.band[i];
        printf ("Processing band %d: %s\n", i, bmeta->name);
        switch (bmeta->data_type)
        {
            case ESPA_INT8:
                band_buffer = calloc (bmeta->nlines * bmeta->nsamps,
                    sizeof (int8_t));
                break;
            case ESPA_UINT8:
                band_buffer = calloc (bmeta->nlines * bmeta->nsamps,
                    sizeof (uint8_t));
                break;
            case ESPA_INT16:
                band_buffer = calloc (bmeta->nlines * bmeta->nsamps,
                    sizeof (int16_t));
                break;
            case ESPA_UINT16:
                band_buffer = calloc (bmeta->nlines * bmeta->nsamps,
                    sizeof (uint16_t));
                break;
            case ESPA_INT32:
                band_buffer = calloc (bmeta->nlines * bmeta->nsamps,
                    sizeof (int32_t));
                break;
            case ESPA_UINT32:
                band_buffer = calloc (bmeta->nlines * bmeta->nsamps,
                    sizeof (uint32_t));
                break;
            case ESPA_FLOAT32:
                band_buffer = calloc (bmeta->nlines * bmeta->nsamps,
                    sizeof (float));
                break;
            case ESPA_FLOAT64:
                band_buffer = calloc (bmeta->nlines * bmeta->nsamps,
                    sizeof (double));
                break;
        }
        if (band_buffer == NULL)
        {
            sprintf (errmsg, "Unable to allocate memory for the image data");
            error_handler (true, FUNC_NAME, errmsg);
            exit (EXIT_FAILURE);
        }

        /* Open the current band as a Tiff file for reading */
        tif_fptr = open_tiff (bmeta->file_name, "r");
        if (tif_fptr == NULL)
        {  /* Error messages already written */
            exit (EXIT_FAILURE);
        }

        /* Read the current band */
        status = read_tiff (tif_fptr, bmeta->data_type, bmeta->nlines,
            bmeta->nsamps, band_buffer);
        if (status != SUCCESS)
        {
            sprintf (errmsg, "Error reading the Tiff file %s",
                bmeta->file_name);
            error_handler (true, FUNC_NAME, errmsg);
            exit (EXIT_FAILURE);
        }

        /* Close the input Tiff file */
        close_tiff (tif_fptr);

        /* Open the output band as a Tiff file for writing */
        sprintf (outname, "output/%s", bmeta->file_name);
        tif_fptr = open_tiff (outname, "w");
        if (tif_fptr == NULL)
        {  /* Error messages already written */
            exit (EXIT_FAILURE);
        }

        /* Set the Tiff tags before writing so the Tiff library knows the
           specifics of the band */
        set_tiff_tags (tif_fptr, bmeta->data_type, bmeta->nlines,
            bmeta->nsamps);

        /* Write the current band to the output directory */
        status = write_tiff (tif_fptr, bmeta->data_type, bmeta->nlines,
            bmeta->nsamps, band_buffer);
        if (status != SUCCESS)
        {
            sprintf (errmsg, "Error writing the Tiff file %s", outname);
            error_handler (true, FUNC_NAME, errmsg);
            exit (EXIT_FAILURE);
        }

        /* Write the GeoTiff tags */
        status = set_geotiff_tags (tif_fptr, bmeta, &gmeta->proj_info);
        if (status != SUCCESS)
        {
            sprintf (errmsg, "Error writing the GeoTiff tags for %s", outname);
            error_handler (true, FUNC_NAME, errmsg);
            exit (EXIT_FAILURE);
        }

        /* Close the output Tiff file */
        close_tiff (tif_fptr);

        /* Free the image buffer */
        free (band_buffer);
    }

    /* Free the metadata structure */
    free_metadata (&xml_metadata);

    /* Free the pointers */
    free (xml_infile);

    /* Successful completion */
    exit (EXIT_SUCCESS);
}
