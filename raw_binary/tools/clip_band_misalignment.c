/*****************************************************************************
FILE: clip_band_misalignment
  
PURPOSE: Contains functions for clipping the mis-aligned bands in TM/ETM+, OLI,
and OLI/TIRS products stored in the internal ESPA raw binary format.

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
#include "clip_band_misalignment.h"

/******************************************************************************
MODULE: usage

PURPOSE: Prints the usage information for this application.

RETURN VALUE:
Type = None

NOTES:
******************************************************************************/
void usage ()
{
    printf ("clip_band_misalignment clips the TM, ETM+, OLI, or OLI/TIRS bands "
            "to handle the band mis-alignment. SWIR and the thermal bands are "
            "clipped so that they all have the same image boundaries. The band "
            "quality band is updated to appropriately flag the fill pixels "
            "after this band clipping.\n\n");
    printf ("usage: clip_band_misalignment --xml=xml_filename\n");

    printf ("\nwhere the following parameters are required:\n");
    printf ("    -xml: name of the input XML metadata file which follows "
            "the ESPA internal raw binary schema\n");
    printf ("\nExample: clip_band_misalignment "
            "--xml=LE07_L1TP_022033_20140228_20161028_01_T1.xml\n");
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

    /* Make sure the infiles were specified */
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

PURPOSE:  Clips the band-misalignment for TM/ETM+, OLI, and OLI/TIRS products.
SWIR and the thermal bands are all clipped so that each band aligns.  The band
quality band is updated to update the fill pixels after the band clipping.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error clipping the bands
SUCCESS         No errors encountered

NOTES:
******************************************************************************/
int main (int argc, char** argv)
{
    char *xml_infile = NULL;           /* input XML filename */
    Espa_internal_meta_t xml_metadata; /* XML metadata structure to be
                                          populated by reading the XML
                                          metadata file */
    Espa_global_meta_t *gmeta = NULL;  /* pointer to the global metadata
                                          structure */

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

    /* Clip the bands based on the instrument type */
    /* Is this OLI or OLI/TIRS? */
    if (!strncmp (gmeta->instrument, "OLI", 3))
    {
        if (clip_band_misalignment_landsat8 (&xml_metadata) != SUCCESS)
        {  /* Error messages already written */
            exit (EXIT_FAILURE);
        }
    }

    /* Is this TM or ETM+? */
    else if (!strcmp (gmeta->instrument, "TM") ||
             !strcmp (gmeta->instrument, "ETM"))
    {
        if (clip_band_misalignment (&xml_metadata) != SUCCESS)
        {  /* Error messages already written */
            exit (EXIT_FAILURE);
        }
    }

    /* Free the metadata structure */
    free_metadata (&xml_metadata);

    /* Free the pointers */
    free (xml_infile);

    /* Successful completion */
    exit (EXIT_SUCCESS);
}
