/*****************************************************************************
FILE: convert_lpgs_to_espa
  
PURPOSE: Contains functions for converting the LPGS products to the ESPA
internal raw binary file format.

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
#include "convert_lpgs_to_espa.h"

/******************************************************************************
MODULE: usage

PURPOSE: Prints the usage information for this application.

RETURN VALUE:
Type = None

NOTES:
******************************************************************************/
void usage ()
{
    printf ("convert_lpgs_to_espa converts the LPGS products (MTL file and "
            "associated GeoTIFF files) to the ESPA internal format (XML "
            "metadata file and associated raw binary files).\n\n");
    printf ("usage: convert_lpgs_to_espa "
            "--mtl=input_mtl_filename "
            "[--del_src_files]\n");

    printf ("\nwhere the following parameters are required:\n");
    printf ("    -mtl: name of the input LPGS MTL metadata file\n");
    printf ("    -del_src_files: if specified the source GeoTIFF files will "
            "be removed.  The _MTL.txt file will remain along with the "
            "gap directory for ETM+ products.\n");
    printf ("\nExample: convert_lpgs_to_espa "
            "--mtl=LE07_L1TP_022033_20140228_20161028_01_T1_MTL.txt\n");
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
    char **mtl_infile,    /* O: address of input LPGS MTL filename */
    char **xml_outfile,   /* O: address of output XML filename */
    bool *del_src         /* O: should source files be removed? */
)
{
    int c;                           /* current argument index */
    int option_index;                /* index for the command-line option */
    char *cptr = NULL;               /* pointer to _MTL.txt in MTL filename */
    char errmsg[STR_SIZE];           /* error message */
    char FUNC_NAME[] = "get_args";   /* function name */
    static int del_flag = 0;         /* flag for removing the source files */
    static struct option long_options[] =
    {
        {"del_src_files", no_argument, &del_flag, 1},
        {"mtl", required_argument, 0, 'i'},
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

            case 'i':  /* LPGS MTL infile */
                *mtl_infile = strdup (optarg);
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

    /* Make sure the input MTL file was specified */
    if (*mtl_infile == NULL)
    {
        sprintf (errmsg, "LPGS MTL input file is a required argument");
        error_handler (true, FUNC_NAME, errmsg);
        usage ();
        return (ERROR);
    }

    /* Generate the XML filename from the MTL filename.  Find the _MTL.txt and
       change that to .xml. */
    *xml_outfile = strdup (*mtl_infile);
    cptr = strrchr (*xml_outfile, '_');
    *cptr = '\0';
    sprintf (*xml_outfile, "%s.xml", *xml_outfile);
    if (*xml_outfile == NULL)
    {
        sprintf (errmsg, "XML output file was not correctly generated");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Check the delete source files flag */
    if (del_flag)
        *del_src = true;

    return (SUCCESS);
}


/******************************************************************************
MODULE:  main

PURPOSE:  Converts the LPGS products (MTL file and associated GeoTIFF files) to
the ESPA internal format (XML metadata file and associated raw binary files).

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
    char *mtl_infile = NULL;      /* input LPGS MTL filename */
    char *xml_outfile = NULL;     /* output XML filename */
    bool del_src = false;         /* should source files be removed? */

    /* Read the command-line arguments */
    if (get_args (argc, argv, &mtl_infile, &xml_outfile, &del_src) != SUCCESS)
    {   /* get_args already printed the error message */
        exit (EXIT_FAILURE);
    }

    /* Convert the LPGS MTL and data to ESPA raw binary and XML */
    if (convert_lpgs_to_espa (mtl_infile, xml_outfile, del_src) != SUCCESS)
    {  /* Error messages already written */
        exit (EXIT_FAILURE);
    }

    /* Free the pointers */
    free (mtl_infile);
    free (xml_outfile);

    /* Successful completion */
    exit (EXIT_SUCCESS);
}
