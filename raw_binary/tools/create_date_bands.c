/*****************************************************************************
FILE: create_date_bands
  
PURPOSE: Creates the date/year bands.

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
#include "raw_binary_io.h"
#include "generate_date_bands.h"

/******************************************************************************
MODULE: usage

PURPOSE: Prints the usage information for this application.

RETURN VALUE:
Type = None

NOTES:
******************************************************************************/
void usage ()
{
    printf ("create_date_bands creates the date and year bands for the input "
            "scene, based the acquisition date/year in the XML file. The "
            "combined date band will be Year*1000 + DOY. The DOY band will be "
            "the DOY and the third band will be the year band.\n"
            "The output date/year filenames are the same as band 1 in the "
            "input XML file with the _B1.img replaced with _date.img, "
            "_doy.img, and _year.img for the combined date/year, day of year, "
            "and year bands respectively.\n\n");
    printf ("usage: create_date_bands --xml=input_metadata_filename\n");

    printf ("\nwhere the following parameters are required:\n");
    printf ("    -xml: name of the input XML metadata file which follows "
            "the ESPA internal raw binary schema\n");
    printf ("\nExample: create_date_bands "
            "--xml=LC08_L1TP_047027_20131014_20170308_01_T1.xml\n");
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

    /* Make sure the infiles and outfiles were specified */
    if (*xml_infile == NULL)
    {
        sprintf (errmsg, "XML input file is a required argument");
        error_handler (true, FUNC_NAME, errmsg);
        usage ();
        return (ERROR);
    }

    return (SUCCESS);
}


#define MAX_DATE_LEN 28
/******************************************************************************
MODULE:  main

PURPOSE: Creates the date/year bands for the current scene. These bands are
generated from the acquisition date/year in the XML file.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error creating the date bands
SUCCESS         No errors encountered

NOTES:
  1. The output date/year filenames are the same as band 1 in the input XML
     file with the _B1.img replaced with _date.img, _doy.img, and _year.img for
     the combined date/year, day of year, and year bands respectively.
  2. It is expected this will be run on the XML file that contains the
     converted LPGS Level 1 bands.
******************************************************************************/
int main (int argc, char** argv)
{
    char FUNC_NAME[] = "create_date_bands";  /* function name */
    char errmsg[STR_SIZE];       /* error message */
    char tmpstr[STR_SIZE];       /* temporary filename */
    char tmp_ext[STR_SIZE];      /* temporary filename extension */
    char jfile[STR_SIZE];        /* output date filename */
    char production_date[MAX_DATE_LEN+1]; /* current date/year for production */
    char *espa_xml_file = NULL;  /* input ESPA XML metadata filename */
    int i;                       /* looping variable */
    int nlines;                  /* number of lines in date bands */
    int nsamps;                  /* number of samples in date bands */
    int refl_indx = -99;         /* index of band1 or first band */
    unsigned int *jdate_buff = NULL;   /* date buffer */
    unsigned short *jdoy_buff = NULL;  /* DOY buffer */
    unsigned short *jyear_buff = NULL; /* year buffer */
    time_t tp;                   /* time structure */
    struct tm *tm = NULL;        /* time structure for UTC time */
    FILE *fptr=NULL;             /* file pointer */
    Envi_header_t envi_hdr;      /* output ENVI header information */
    Espa_global_meta_t *gmeta = NULL; /* pointer to global metadata structure */
    Espa_band_meta_t *bmeta = NULL;   /* pointer to band metadata structure */
    Espa_band_meta_t *out_bmeta = NULL;/* band metadata for bands */
    Espa_internal_meta_t out_meta;     /* output metadata for bands */
    Espa_internal_meta_t xml_metadata; /* XML metadata structure to be populated
                                          by reading the XML metadata file */

    /* Read the command-line arguments */
    if (get_args (argc, argv, &espa_xml_file) != SUCCESS)
    {   /* get_args already printed the error message */
        exit (ERROR);
    }

    /* Validate the input metadata file */
    if (validate_xml_file (espa_xml_file) != SUCCESS)
    {  /* Error messages already written */
        exit (ERROR);
    }

    /* Initialize the metadata structure */
    init_metadata_struct (&xml_metadata);

    /* Parse the metadata file into our internal metadata structure; also
       allocates space as needed for various pointers in the global and band
       metadata */
    if (parse_metadata (espa_xml_file, &xml_metadata) != SUCCESS)
    {  /* Error messages already written */
        exit (ERROR);
    }
    gmeta = &xml_metadata.global;

    /* Generate the date bands for this scene. Memory is allocated for the
       buffers in this function call. */
    if (generate_date_bands (&xml_metadata, &jdate_buff, &jdoy_buff,
        &jyear_buff, &nlines, &nsamps) != SUCCESS)
    {  /* Error messages already written */
        exit (ERROR);
    }

    /* Use band 1 as the representative band in the XML */
    for (i = 0; i < xml_metadata.nbands; i++)
    {
        if (!strcmp (xml_metadata.band[i].name, "b1"))
        {
            /* this is the index we'll use for reflectance band info */
            refl_indx = i;
            break;
        }
    }

    /* Make sure the representative band was found in the XML file */
    if (refl_indx == -9)
    {
        sprintf (errmsg, "Band 1 (b1) was not found in the XML file");
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /* Make sure the band 1 number of lines and samples matches what was used
       for creating the date bands, otherwise we will have a mismatch in the
       resolution and output XML information. */
    bmeta = &xml_metadata.band[refl_indx];
    if (nlines != bmeta->nlines || nsamps != bmeta->nsamps)
    {
        sprintf (errmsg, "Band 1 from this application does not match band 1 "
            "from the generate_date_bands function call.  Local nlines/nsamps: "
            "%d, %d   Returned nlines/nsamps: %d, %d", bmeta->nlines,
            bmeta->nsamps, nlines, nsamps);
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /* Initialize the output metadata structure.  The global metadata will
       not be used and will not be valid. */
    init_metadata_struct (&out_meta);

    /* Allocate memory for three output bands */
    if (allocate_band_metadata (&out_meta, 3) != SUCCESS)
    {
        sprintf (errmsg, "Cannot allocate memory for the date bands");
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

    /* Loop through the three bands and append them to the XML file */
    for (i = 0; i < 3; i++)
    {
        /* Set up the band metadata for the date bands */
        out_bmeta = &out_meta.band[i];
        strcpy (out_bmeta->product, "intermediate_data");
        strcpy (out_bmeta->source, "level1");

        /* Band-specific names */
        switch (i)
        {
            case (0):  /* combined date/year */
                strcpy (out_bmeta->name, "combined_date");
                strcpy (out_bmeta->category, "image");
                out_bmeta->data_type = ESPA_UINT32;
                strncpy (tmpstr, bmeta->short_name, 4);
                tmpstr[4] = '\0';
                sprintf (out_bmeta->short_name, "%sDATE", tmpstr);
                strcpy (out_bmeta->long_name,
                    "doy and year (YEAR * 1000 + DOY)");
                sprintf (tmp_ext, "date.img");
                strcpy (out_bmeta->data_units, "date");
                break;

            case (1):  /* date */
                strcpy (out_bmeta->name, "doy");
                strcpy (out_bmeta->category, "image");
                out_bmeta->data_type = ESPA_UINT16;
                strncpy (tmpstr, bmeta->short_name, 4);
                tmpstr[4] = '\0';
                sprintf (out_bmeta->short_name, "%sDOY", tmpstr);
                strcpy (out_bmeta->long_name, "day of year");
                sprintf (tmp_ext, "doy.img");
                out_bmeta->valid_range[0] = 1.0;
                out_bmeta->valid_range[1] = 366.0;
                strcpy (out_bmeta->data_units, "date");
                break;

            case (2):  /* year */
                strcpy (out_bmeta->name, "year");
                strcpy (out_bmeta->category, "image");
                out_bmeta->data_type = ESPA_UINT16;
                strncpy (tmpstr, bmeta->short_name, 4);
                tmpstr[4] = '\0';
                sprintf (out_bmeta->short_name, "%sYEAR", tmpstr);
                strcpy (out_bmeta->long_name, "year");
                sprintf (tmp_ext, "year.img");
                out_bmeta->valid_range[0] = 1970.0;
                out_bmeta->valid_range[1] = 9999.0;
                strcpy (out_bmeta->data_units, "date");
                break;
        }

        /* Use the product name to create the date filename */
        snprintf (out_bmeta->file_name, sizeof (out_bmeta->file_name), "%s_%s",
            gmeta->product_id, tmp_ext);

        out_bmeta->resample_method = ESPA_NN;
        out_bmeta->nlines = nlines;
        out_bmeta->nsamps = nsamps;
        out_bmeta->pixel_size[0] = bmeta->pixel_size[0];
        out_bmeta->pixel_size[1] = bmeta->pixel_size[1];
        strcpy (out_bmeta->pixel_units, bmeta->pixel_units);
        sprintf (out_bmeta->app_version, "create_date_bands_%s",
            ESPA_COMMON_VERSION);
        strcpy (out_bmeta->production_date, production_date);
    }

    /** Write the date/year file **/
    out_bmeta = &out_meta.band[0];
    strcpy (jfile, out_bmeta->file_name);
    fptr = open_raw_binary (jfile, "wb");
    if (!fptr)
    {
        sprintf (errmsg, "Unable to open the date/year file: %s", jfile);
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /* Write the data for this band */
    if (write_raw_binary (fptr, nlines, nsamps, sizeof (unsigned int),
        jdate_buff) != SUCCESS)
    {
        sprintf (errmsg, "Unable to write to the date/year file");
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /* Close the file and free the pointer for this band */
    close_raw_binary (fptr);
    free (jdate_buff);

    /* Create the ENVI header using the representative band */
    if (create_envi_struct (out_bmeta, gmeta, &envi_hdr) != SUCCESS)
    {
        sprintf (errmsg, "Error creating the ENVI header file.");
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /* Write the ENVI header */
    sprintf (tmpstr, "%s", jfile);
    sprintf (&tmpstr[strlen(tmpstr)-3], "hdr");
    if (write_envi_hdr (tmpstr, &envi_hdr) != SUCCESS)
    {
        sprintf (errmsg, "Writing the ENVI header file: %s.", tmpstr);
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /** Write the date file **/
    out_bmeta = &out_meta.band[1];
    strcpy (jfile, out_bmeta->file_name);
    fptr = open_raw_binary (jfile, "wb");
    if (!fptr)
    {
        sprintf (errmsg, "Unable to open the date file: %s", jfile);
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /* Write the data for this band */
    if (write_raw_binary (fptr, nlines, nsamps, sizeof (unsigned short),
        jdoy_buff) != SUCCESS)
    {
        sprintf (errmsg, "Unable to write to the date file");
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /* Close the file and free the pointer for this band */
    close_raw_binary (fptr);
    free (jdoy_buff);

    /* Create the ENVI header using the representative band */
    if (create_envi_struct (out_bmeta, gmeta, &envi_hdr) != SUCCESS)
    {
        sprintf (errmsg, "Error creating the ENVI header file.");
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /* Write the ENVI header */
    sprintf (tmpstr, "%s", jfile);
    sprintf (&tmpstr[strlen(tmpstr)-3], "hdr");
    if (write_envi_hdr (tmpstr, &envi_hdr) != SUCCESS)
    {
        sprintf (errmsg, "Writing the ENVI header file: %s.", tmpstr);
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /** Write the year file **/
    out_bmeta = &out_meta.band[2];
    strcpy (jfile, out_bmeta->file_name);
    fptr = open_raw_binary (jfile, "wb");
    if (!fptr)
    {
        sprintf (errmsg, "Unable to open the year file: %s", jfile);
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /* Write the data for this band */
    if (write_raw_binary (fptr, nlines, nsamps, sizeof (unsigned short),
        jyear_buff) != SUCCESS)
    {
        sprintf (errmsg, "Unable to write to the year file");
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /* Close the file and free the pointer for this band */
    close_raw_binary (fptr);
    free (jyear_buff);

    /* Create the ENVI header using the representative band */
    if (create_envi_struct (out_bmeta, gmeta, &envi_hdr) != SUCCESS)
    {
        sprintf (errmsg, "Error creating the ENVI header file.");
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /* Write the ENVI header */
    sprintf (tmpstr, "%s", jfile);
    sprintf (&tmpstr[strlen(tmpstr)-3], "hdr");
    if (write_envi_hdr (tmpstr, &envi_hdr) != SUCCESS)
    {
        sprintf (errmsg, "Writing the ENVI header file: %s.", tmpstr);
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /* Append the date bands to the XML file */
    if (append_metadata (3, out_meta.band, espa_xml_file) != SUCCESS)
    {
        sprintf (errmsg, "Appending date bands to the XML file.");
        error_handler (true, FUNC_NAME, errmsg);
        exit (ERROR);
    }

    /* Free the input and output XML metadata */
    free_metadata (&xml_metadata);
    free_metadata (&out_meta);

    /* Free the pointers */
    free (espa_xml_file);

    /* Successful completion */
    exit (SUCCESS);
}
