/*****************************************************************************
FILE: convert_sentinel_to_espa
  
PURPOSE: Contains functions for converting the Sentinel-2 A&B L1C products to
the ESPA internal raw binary file format.

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

LICENSE TYPE:  NASA Open Source Agreement Version 1.3

NOTES:
  1. The XML metadata format parsed or written via this library follows the
     ESPA internal metadata format found in ESPA Raw Binary Format v1.0.doc.
     The schema for the ESPA internal metadata format is available at
     http://espa.cr.usgs.gov/schema/espa_internal_metadata_v1_0.xsd.
*****************************************************************************/
#include <getopt.h>
#include "convert_sentinel_to_espa.h"

/******************************************************************************
MODULE: usage

PURPOSE: Prints the usage information for this application.

RETURN VALUE:
Type = None

NOTES:
******************************************************************************/
void usage ()
{
    printf ("convert_sentinel_to_espa converts the Sentinel-2 A&B L1C products "
            "to the ESPA internal format (XML metadata file and associated raw "
            "binary files). The MTD_MSIL1C product XML and MTD_TL granule XML "
            "files must be copied into the same directory as the granule-level "
            "image files (S2[A|B]_MSIL1C_*.SAFE/GRANULE/L1C_*/IMG_DATA). Only "
            "these two XML files and all 14 bands of the L1C JP2 files are "
            "needed. The rest of the SAFE directory structure and files are "
            "not needed. The executable must be run from the directory "
            "containing the XML files and JP2 image data.\n");
    printf ("version: %s\n\n", ESPA_COMMON_VERSION);
    printf ("usage: convert_sentinel_to_espa [--del_src_files]\n");

    printf ("\nwhere the following parameters are optional:\n");
    printf ("    -del_src_files: if specified the source JP2 file will "
            "be removed.\n");
    printf ("\nExample: convert_sentinel_to_espa --del_src_files\n");
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
******************************************************************************/
short get_args
(
    int argc,             /* I: number of cmd-line args */
    char *argv[],         /* I: string of cmd-line args */
    bool *del_src         /* O: should source files be removed? */
)
{
    int c;                           /* current argument index */
    int option_index;                /* index for the command-line option */
    char errmsg[STR_SIZE];           /* error message */
    char FUNC_NAME[] = "get_args";   /* function name */
    static int del_flag = 0;         /* flag for removing the source files */
    static struct option long_options[] =
    {
        {"del_src_files", no_argument, &del_flag, 1},
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

            case '?':
            default:
                sprintf (errmsg, "Unknown option %s", argv[optind-1]);
                error_handler (true, FUNC_NAME, errmsg);
                usage ();
                return (ERROR);
                break;
        }
    }

    /* Check the delete source files flag */
    if (del_flag)
        *del_src = true;

    return (SUCCESS);
}


/******************************************************************************
MODULE:  main

PURPOSE:  Converts the Sentinel-2 product to the ESPA internal format (XML
metadata file and associated raw binary files).

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error doing the conversion
SUCCESS         No errors encountered

NOTES:
******************************************************************************/
int main (int argc, char** argv)
{
    bool del_src = false;         /* should source files be removed? */

    printf ("convert_sentinel_to_espa version: %s\n", ESPA_COMMON_VERSION);

    /* Read the command-line arguments */
    if (get_args (argc, argv, &del_src) != SUCCESS)
    {   /* get_args already printed the error message */
        exit (EXIT_FAILURE);
    }

    /* Convert the Sentinel JP2 and data to ESPA raw binary and XML */
    if (convert_sentinel_to_espa (del_src) != SUCCESS)
    {  /* Error messages already written */
        exit (EXIT_FAILURE);
    }

    /* Successful completion */
    exit (EXIT_SUCCESS);
}
