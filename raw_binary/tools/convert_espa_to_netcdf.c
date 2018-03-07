/*****************************************************************************
FILE: convert_espa_to_netcdf
  
PURPOSE: Contains functions for converting the ESPA raw binary file format
to NetCDF.

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
#include "convert_espa_to_netcdf.h"

/******************************************************************************
MODULE: usage

PURPOSE: Prints the usage information for this application.

RETURN VALUE:
Type = None

NOTES:
******************************************************************************/
void usage ()
{
    printf ("convert_espa_to_netcdf converts the ESPA internal format (raw "
            "binary and associated XML metadata file) to NetCDF.\n\n");
    printf ("usage: convert_espa_to_netcdf "
            "--xml=input_metadata_filename "
            "--netcdf=output_netcdf_filename "
            "[--del_src_files]"
            "[--no_compression]\n");

    printf ("\nwhere the following parameters are required:\n");
    printf ("    -xml: name of the input XML metadata file which follows "
            "the ESPA internal raw binary schema\n");
    printf ("    -netcdf: filename of the output NetCDF file\n");
    printf ("\nand where the following parameters are optional:\n");
    printf ("    -del_src_files: if specified the source image and header "
            "files will be removed\n");
    printf ("    -no_compression: if specified compression will not be used "
            "(the default is compression is used)\n");
    printf ("\nExample: convert_espa_to_netcdf "
            "--xml=LE07_L1TP_022033_20140228_20161028_01_T1.xml "
            "--netcdf=LE07_L1TP_022033_20140228_20161028_01_T1.nc\n");
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
    int argc,              /* I: number of cmd-line args */
    char *argv[],          /* I: string of cmd-line args */
    char **xml_infile,     /* O: address of input XML filename */
    char **netcdf_outfile, /* O: address of output NetCDF filename */
    bool *del_src,         /* O: should source files be removed? */
    bool *no_compression   /* O: should compression be used? */
)
{
    int c;                           /* current argument index */
    int option_index;                /* index for the command-line option */
    char errmsg[STR_SIZE];           /* error message */
    char FUNC_NAME[] = "get_args";   /* function name */
    static int del_flag = 0;         /* flag for removing the source files */
    static int no_compression_flag = 0; /* flag for compressing NetCDF file */
    static struct option long_options[] =
    {
        {"del_src_files", no_argument, &del_flag, 1},
        {"no_compression", no_argument, &no_compression_flag, 1},
        {"xml", required_argument, 0, 'i'},
        {"netcdf", required_argument, 0, 'o'},
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
     
            case 'o':  /* NetCDF outfile */
                *netcdf_outfile = strdup (optarg);
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
        sprintf (errmsg, "XML input file is a required argument");
        error_handler (true, FUNC_NAME, errmsg);
        usage ();
        return (ERROR);
    }

    if (*netcdf_outfile == NULL)
    {
        sprintf (errmsg, "NetCDF output file is a required argument");
        error_handler (true, FUNC_NAME, errmsg);
        usage ();
        return (ERROR);
    }

    /* Check the delete source files flag */
    if (del_flag)
        *del_src = true;

    /* Check the "no compression" flag */
    if (no_compression_flag)
        *no_compression = true;


    return (SUCCESS);
}


/******************************************************************************
MODULE:  main

PURPOSE:  Converts the ESPA internal format (raw binary and associated XML
metadata file) to NetCDF.

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
    char *xml_infile = NULL;     /* input XML filename */
    char *netcdf_outfile = NULL; /* output NetCDF filename */
    bool del_src = false;        /* should source files be removed? */
    bool no_compression = false; /* should compression be used? */

    /* Read the command-line arguments */
    if (get_args (argc, argv, &xml_infile, &netcdf_outfile, &del_src, 
        &no_compression) != SUCCESS)
    {   /* get_args already printed the error message */
        exit (EXIT_FAILURE);
    }

    /* Convert the internal ESPA raw binary product to NetCDF */
    if (convert_espa_to_netcdf (xml_infile, netcdf_outfile, del_src, 
        no_compression) != SUCCESS)
    {  /* Error messages already written */
        exit (EXIT_FAILURE);
    }

    /* Free the pointers */
    free (xml_infile);
    free (netcdf_outfile);

    /* Successful completion */
    exit (EXIT_SUCCESS);
}
