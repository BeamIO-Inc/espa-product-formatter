/*****************************************************************************
FILE: convert_sentinel_to_espa.c
  
PURPOSE: Contains functions for reading Sentinel-2 1C products and writing to
the ESPA raw binary format.

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

LICENSE TYPE:  NASA Open Source Agreement Version 1.3

NOTES:
  1. The XML metadata format written via this library follows the ESPA internal
     metadata format found in ESPA Raw Binary Format v1.0.doc.  The schema for
     the ESPA internal metadata format is available at
     http://espa.cr.usgs.gov/schema/espa_internal_metadata_v1_x.xsd.
  2. Sentinel-2 data has an older file/packaging format (prior to October 2016)
     as well as the latest format.  Both are supported by this application.
     Only S2A data will be in the old format, since S2B didn't come online
     until March of 2017.
*****************************************************************************/
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <string.h>
#include "convert_sentinel_to_espa.h"

/* Band information for the Sentinel-2 L1C products. Ignore TCI (true color
   image). */
char sentinel_bands[NUM_SENTINEL_BANDS][STR_SIZE] =
    {"B01", "B02", "B03", "B04", "B05", "B06", "B07", "B08", "B8A",
     "B09", "B10", "B11", "B12"};
char sentinel_band_nums[NUM_SENTINEL_BANDS][STR_SIZE] =
    {"1", "2", "3", "4", "5", "6", "7", "8", "8A", "9", "10", "11", "12"};

/******************************************************************************
MODULE:  read_dir

PURPOSE: Read the current directory and look for the band 1 Sentinel-2 file.

RETURN VALUE:
Type = char *
Value           Description
-----           -----------
NULL            Error reading the directory and successful find of band 1
non-NULL        Band 1 filename was successfully found

NOTES:
******************************************************************************/
char *read_dir ()
{
    char FUNC_NAME[] = "read_dir";  /* function name */
    char errmsg[STR_SIZE];      /* error message */
    char *b1_name = NULL;       /* band 1 Sentinel-2 filename */
    DIR *dr = NULL;             /* ptr to current directory */
    struct dirent *de = NULL;   /* ptr for directory entry */
  
    /* Open the current directory */
    dr = opendir(".");
    if (dr == NULL)
    {
        sprintf (errmsg, "Could not open current directory");
        error_handler (true, FUNC_NAME, errmsg);
        return (NULL);
    }

    /* Loop through the files in the directory */
    while ((de = readdir (dr)) != NULL)
    {
        /* Is this band 1? */
        if (strstr (de->d_name, "_B01.jp2"))
        {
            b1_name = de->d_name;
            break;
        }
    }

    /* Close the directory */
    closedir (dr);

    /* Return the band 1 name (or NULL if not found) */
    return (b1_name);
}


/******************************************************************************
MODULE:  rename_jp2

PURPOSE: Rename the Sentinel JPEG2000 files from the shortened JP2 filename to
the more informative granule name {product_id}_{band}.jp2.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error renaming the JP2 files
SUCCESS         Successfully renamed the JP2 files

NOTES:
The ESPA Sentinel filename convention (for both old and new S2 formats) is as
follows: S2X_MSI_L1C_TTTTTT_ YYYYMMDD_yyyymmdd_CC_TX

Where:
  * X = A or B
  * TTTTTT = Sentinel tile number (ex. T10TFR)
  * YYYYMMDD = Acquisition year, month, day
  * yyyymmdd = Processing year, month, day

Example: S2A_MSI_L1C_T10TFR_20180816_20180903

******************************************************************************/
int rename_jp2
(
    Espa_internal_meta_t *xml_metadata /* I: valid ESPA metadata structure */
)
{
    const int TILE_CHARS=6;  /* number of chars in the S2 tile name */
    const int YEAR_CHARS=4;  /* number of chars in the year string */
    const int MONTH_CHARS=2; /* number of chars in the month string */
    const int DAY_CHARS=2;   /* number of chars in the day string */
    const int DATE_CHARS=YEAR_CHARS+MONTH_CHARS+DAY_CHARS;
                             /* number of chars in the date string */
    char FUNC_NAME[] = "rename_jp2";  /* function name */
    char errmsg[STR_SIZE];      /* error message */
    char newfile[STR_SIZE];     /* name of the new Sentinel file */
    char acq_date[DATE_CHARS+1];  /* acquisition date */
    char prod_date[DATE_CHARS+1]; /* production date */
    char s2_tile[TILE_CHARS+1]; /* Sentinel tile */
    char year[YEAR_CHARS+1];    /* acquisition or production year */
    char month[MONTH_CHARS+1];  /* acquisition or production month */
    char day[DAY_CHARS+1];      /* acquisition or production day */
    char sat_x;                 /* A or B Sentinel satellite */
    int i;                      /* looping variable for bands in XML file */
    int count;                  /* number of chars copied in snprintf */
    Espa_band_meta_t *bmeta = NULL;  /* pointer to band metadata */
    Espa_global_meta_t *gmeta = &xml_metadata->global;  /* global metadata */

    /* Get the satellite letter - A or B */
    sat_x = gmeta->satellite[strlen(gmeta->satellite)-1];

    /* Get the tile number, which is in different locations depending on
       whether this is an old or new Sentinel-2 product */
    if (!strncmp (gmeta->product_id, "S2", 2))
    { /* old S2 format
         (Ex. S2A_OPER_MSI_L1C_TL_SGS__20151231T122251_A002735_T34MFS) */
        strncpy (s2_tile,
                 &gmeta->product_id[strlen(gmeta->product_id)-TILE_CHARS],
                 TILE_CHARS);
    }
    else
    { /* new S2 format (Ex. T10TFR_20180816T185921) */
        strncpy (s2_tile, gmeta->product_id, TILE_CHARS);
    }
    s2_tile[TILE_CHARS] = '\0';

    /* Get the acquisition date, but remove the dashes */
    strncpy (year, gmeta->acquisition_date, YEAR_CHARS);
    year[YEAR_CHARS] = '\0';

    strncpy (month, &gmeta->acquisition_date[YEAR_CHARS+1], MONTH_CHARS);
    month[MONTH_CHARS] = '\0';

    strncpy (day, &gmeta->acquisition_date[YEAR_CHARS+MONTH_CHARS+2],
        DAY_CHARS);
    day[DAY_CHARS] = '\0';
    sprintf (acq_date, "%s%s%s", year, month, day);

    /* Get the production date, but remove the dashes */
    strncpy (year, gmeta->level1_production_date, YEAR_CHARS);
    year[YEAR_CHARS] = '\0';

    strncpy (month, &gmeta->level1_production_date[YEAR_CHARS+1], MONTH_CHARS);
    month[MONTH_CHARS] = '\0';

    strncpy (day, &gmeta->level1_production_date[YEAR_CHARS+MONTH_CHARS+2],
        DAY_CHARS);
    day[DAY_CHARS] = '\0';
    sprintf (prod_date, "%s%s%s", year, month, day);

    /* Generate the new product ID */
    sprintf (gmeta->product_id, "S2%c_MSI_L1C_%s_%s_%s", sat_x, s2_tile,
        acq_date, prod_date);

    /* Loop through the bands in the metadata file and convert each one to
       the ESPA format */
    for (i = 0; i < xml_metadata->nbands; i++)
    {
        /* Set up the band metadata pointer */
        bmeta = &xml_metadata->band[i];

        /* Rename the current JP2 filename to {product_id}_{bandname}.jp2 */
        sprintf (newfile, "%s_%s.jp2", gmeta->product_id, sentinel_bands[i]);
        if (rename (bmeta->file_name, newfile))
        {
            sprintf (errmsg, "Unable to rename the original Sentinel JP2 "
                "file (%s) to the new ESPA filename (%s)", bmeta->file_name,
                newfile);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Write the new filename to the ESPA metadata */
        count = snprintf (bmeta->file_name, sizeof (bmeta->file_name), "%s",
            newfile);
        if (count < 0 || count >= sizeof (bmeta->file_name))
        {
            sprintf (errmsg, "Overflow of bmeta->file_name string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }

    /* Successful rename */
    return (SUCCESS);
}


/******************************************************************************
MODULE:  convert_jp2_to_img

PURPOSE: Convert the Sentinel JP2 bands to an ESPA raw binary (.img) file,
and generate the ENVI header file.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error converting the JP2 file
SUCCESS         Successfully converted JP2 file

NOTES:
******************************************************************************/
int convert_jp2_to_img
(
    Espa_internal_meta_t *xml_metadata /* I: valid ESPA metadata structure */
)
{
    char FUNC_NAME[] = "convert_jp2_to_img";  /* function name */
    char errmsg[STR_SIZE];    /* error message */
    char *cptr = NULL;        /* pointer to the file extension */
    char envi_file[STR_SIZE]; /* name of the output ENVI header file */
    char raw_file[STR_SIZE];  /* name of the output raw binary file (.raw) */
    char jp2_cmd[STR_SIZE];   /* command string for opj_decompress */
    int i;                    /* looping variable for bands in XML file */
    int count;                /* number of chars copied in snprintf */
    Envi_header_t envi_hdr;   /* output ENVI header information */
    Espa_band_meta_t *bmeta = NULL;  /* pointer to band metadata */
    Espa_global_meta_t *gmeta = &xml_metadata->global;  /* global metadata */

    /* Setup the opj_decompress command for converting all the bands in the
       current directory from JP2 to img.  This does not create an ENVI header
       file for the bands. */
    strcpy (jp2_cmd, "opj_decompress -ImgDir . -OutFor RAW -quiet");
    if (system (jp2_cmd) == -1)
    {
        sprintf (errmsg, "Decompressing JP2 files: %s. Make sure the current "
            "directory is writable and the openjpeg opj_decompress tool is in "
            "your system PATH", jp2_cmd);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Loop through the bands in the metadata file and convert each one to
       the ESPA format */
    for (i = 0; i < xml_metadata->nbands; i++)
    {
        /* Set up the band metadata pointer */
        bmeta = &xml_metadata->band[i];

        /* Determine the name of the output raw binary file.  Replace the
           jp2 file extension with img in the Sentinel filenames. */
        cptr = strrchr (bmeta->file_name, '.');
        if (cptr == NULL)
        {
            sprintf (errmsg, "No file extension found in the Sentinel JP2 "
                "file: %s\n", bmeta->file_name);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
        strcpy (cptr, ".img");

        /* Rename the .raw files from the opg_decompress to .img files */
        count = snprintf (raw_file, sizeof (raw_file), "%s", bmeta->file_name);
        if (count < 0 || count >= sizeof (raw_file))
        {
            sprintf (errmsg, "Overflow of raw_file string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
        cptr = strrchr (raw_file, '.');
        strcpy (cptr, ".raw");

        if (rename (raw_file, bmeta->file_name))
        {
            sprintf (errmsg, "Unable to rename the decompressed Sentinel raw "
                "file (%s) to the new ESPA filename (%s)", raw_file,
                bmeta->file_name);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Create the ENVI header file for this band */
        if (create_envi_struct (bmeta, gmeta, &envi_hdr) != SUCCESS)
        {
            sprintf (errmsg, "Creating the ENVI header structure for this "
                "file: %s", bmeta->file_name);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Write the ENVI header */
        count = snprintf (envi_file, sizeof (envi_file), "%s",
            bmeta->file_name);
        if (count < 0 || count >= sizeof (envi_file))
        {
            sprintf (errmsg, "Overflow of envi_file string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
        cptr = strrchr (envi_file, '.');
        strcpy (cptr, ".hdr");

        if (write_envi_hdr (envi_file, &envi_hdr) != SUCCESS)
        {
            sprintf (errmsg, "Writing the ENVI header file: %s.", envi_file);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }  /* end for */

    /* Successful conversion */
    return (SUCCESS);
}


/******************************************************************************
MODULE:  convert_sentinel_to_espa

PURPOSE: Converts the input Sentinel-2 (A&B L1C) files to the ESPA internal raw
binary file format (and associated XML file).  The MTD_MSIL1C.xml and MTD_TL.xml
files are expected to be in the same directory as the Sentinel band data.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error converting the Sentinel-2 product
SUCCESS         Successfully converted Sentinel-2 product to ESPA format

NOTES:
  1. The Sentinel JP2 band files will be deciphered from the Sentinel XML file.
  2. The JP2 band files listed in the MTD_MSIL1C.xml file need to be available
     in the same directory as both the MTD_MSIL1C product and MTD_TL tile XML
     files.
******************************************************************************/
int convert_sentinel_to_espa
(
    bool del_src      /* I: should the source .jp2 files be removed after
                            conversion? */
)
{
    char FUNC_NAME[] = "convert_sentinel_to_espa";  /* function name */
    char errmsg[STR_SIZE];            /* error message */
    char espa_xml_file[STR_SIZE];     /* output ESPA XML metadata filename */
    char jp2_file[STR_SIZE];          /* jp2 image file to delete */
    char sentinel_xml_file[STR_SIZE]; /* current Sentinel XML filename */
    char orig_bandname[STR_SIZE];     /* original band1 filename */
    char prodtype[STR_SIZE];          /* product type string for all bands */
    char proc_ver[STR_SIZE];          /* processing ver string for all bands */
    char l1_filename[STR_SIZE];       /* original level-1 filename for the
                                         initial band to be used as base for
                                         all bands */
    char *b1_name = NULL;             /* band 1 Sentinel-2 filename */
    char *cptr = NULL;                /* pointer to the file extension */
    float scale_factor;               /* scale factor for all bands */
    int i;                            /* looping variable */
    int count;                        /* number of chars copied in snprintf */
    Espa_internal_meta_t xml_metadata;  /* ESPA XML metadata structure to be
                                           populated by reading the Sentinel
                                           XML file */
    Espa_global_meta_t *gmeta = NULL; /* global metadata structure */
    Espa_band_meta_t *bmeta = NULL;   /* band metadata pointer to all bands */

    /* Initialize the metadata structure */
    init_metadata_struct (&xml_metadata);
    gmeta = &xml_metadata.global;

    /* Read the Sentinel MTD_MSIL1C product XML file and populate our internal
       ESPA metadata structure. The acquisition date/time, product generation
       date/time, lat/long coords, product type, and scale factor are all
       available in this XML file. */
    strcpy (sentinel_xml_file, "MTD_MSIL1C.xml");
    if (parse_sentinel_product_metadata (sentinel_xml_file, &xml_metadata,
        prodtype, proc_ver, l1_filename, &scale_factor) != SUCCESS)
    {
        sprintf (errmsg, "Reading Sentinel product XML file: %s",
            sentinel_xml_file);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Allocate band metadata */
    if (allocate_band_metadata (&xml_metadata, NUM_SENTINEL_BANDS) != SUCCESS)
    {   /* Error messages already printed */
        return (ERROR);
    }

    /* Get the band 1 filename in the current directory */
    b1_name = read_dir();
    if (b1_name == NULL)
    {
        sprintf (errmsg, "Not able to find the Sentinel-2 band 1 file in "
            "the current directory");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Strip off the band and jpeg 2000 file extension. Store this as the
       product_id in the global metadata. */
    cptr = strstr (b1_name, "_B01.jp2");
    *cptr = '\0';
    snprintf (gmeta->product_id, sizeof (gmeta->product_id), "%s",
        (const char *) b1_name);
    
    /* Strip the band from the level-1 filename to get the basename */
    cptr = strrchr (l1_filename, '_');
    *cptr = '\0';

    /* The filename, product type, app version, and scale factor need to be
       added to the band metadata for each of the bands */
    for (i = 0; i < xml_metadata.nbands; i++)
    {
        bmeta = &xml_metadata.band[i];
        snprintf (bmeta->file_name, sizeof (bmeta->file_name), "%s_%s.jp2",
            (const char *) b1_name, sentinel_bands[i]);
        strcpy (bmeta->short_name, prodtype);
        bmeta->scale_factor = 1.0 / scale_factor;
        snprintf (bmeta->l1_filename, sizeof (bmeta->l1_filename), "%s_%s",
            (const char *) l1_filename, sentinel_bands[i]);

        /* Sentinel-2 XML files contain the processing baseline version, so
           that will be used to keep track of the ESA PDGS version number */
        sprintf (bmeta->app_version, "ESA Payload Data Ground Segment v%s",
            proc_ver);
    }

    /* Read the Sentinel MTD_TL tile XML file and populate our internal ESPA
       metadata structure. The tile level datum, projection, and zone are
       available. The number of lines/samples for each resolution are
       available. The UL x/y position are also availble. */
    strcpy (sentinel_xml_file, "MTD_TL.xml");
    if (parse_sentinel_tile_metadata (sentinel_xml_file, &xml_metadata) !=
        SUCCESS)
    {
        sprintf (errmsg, "Reading Sentinel tile XML file: %s",
            sentinel_xml_file);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Add the data provider USGS/EROS (Sentinel products delivered via EE) */
    strcpy (gmeta->data_provider, "USGS/EROS");

    /* Add the instrument which is MSI - MultiSpectral Instrument */
    strcpy (gmeta->instrument, "MSI");

    /* Set the orientation angle to 0 */
    gmeta->orientation_angle = 0.0;

    /* Update remaining information for band metadata for each of the bands */
    for (i = 0; i < xml_metadata.nbands; i++)
    {
        bmeta = &xml_metadata.band[i];
        strcpy (bmeta->product, "MSIL1C");
        strcpy (bmeta->name, sentinel_bands[i]);
        strcpy (bmeta->category, "image");
        bmeta->data_type = ESPA_UINT16;
        bmeta->fill_value = 0;
        bmeta->saturate_value = 65535;
        bmeta->valid_range[0] = 0.0;
        bmeta->valid_range[1] = 65534.0;
        strcpy (bmeta->data_units, "reflectance");
        strcpy (bmeta->production_date, gmeta->level1_production_date);
        sprintf (bmeta->long_name, "band %s top-of-atmosphere reflectance",
            sentinel_band_nums[i]);
    }

    /* If the source data is going to get removed, then save the band 1
       filename before it is renamed */
    if (del_src)
    {
        bmeta = &xml_metadata.band[0];
        strcpy (orig_bandname, bmeta->file_name);
    }

    /* Rename the current Sentinel JP2 bands to a new filename (using the
       product_id) to be used by ESPA */
    if (rename_jp2 (&xml_metadata) != SUCCESS)
    {
        sprintf (errmsg, "Renaming Sentinel JP2 image files");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Convert each of the Sentinel JP2 bands to raw binary, also create the
       ENVI header files using the XML metadata. Updates the filenames for
       each band to raw binary. */
    if (convert_jp2_to_img (&xml_metadata) != SUCCESS)
    {
        sprintf (errmsg, "Converting JP2 bands to raw binary");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Write the metadata from our internal metadata structure to the output
       XML filename */
    sprintf (espa_xml_file, "%s.xml", gmeta->product_id);
    if (write_metadata (&xml_metadata, espa_xml_file) != SUCCESS)
    {  /* Error messages already written */
        return (ERROR);
    }

    /* Validate the output metadata file */
    if (validate_xml_file (espa_xml_file) != SUCCESS)
    {  /* Error messages already written */
        return (ERROR);
    }

    /* Remove the source JP2 files if specified */
    if (del_src)
    {
        /* Remove the image band */
        for (i = 0; i < xml_metadata.nbands; i++)
        {
            /* Point to the band metadata for this band */
            bmeta = &xml_metadata.band[i];

            /* Remove the .jp2 files */
            count = snprintf (jp2_file, sizeof (jp2_file), "%s",
                bmeta->file_name);
            if (count < 0 || count >= sizeof (jp2_file))
            {
                sprintf (errmsg, "Overflow of jp2_file string");
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
            cptr = strrchr (jp2_file, '.');
            strcpy (cptr, ".jp2");

            /* Remove the source file */
            printf ("  Removing source JPEG2000 file: %s\n", jp2_file);
            if (unlink (jp2_file) != 0)
            {
                sprintf (errmsg, "Deleting source file: %s", jp2_file);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
        }

        /* Remove the TCI jp2 band, which retains its original filename. Only
           exists in the new S2 format so don't error check if it fails. */
        count = snprintf (jp2_file, sizeof (jp2_file), "%s", orig_bandname);
        if (count < 0 || count >= sizeof (jp2_file))
        {
            sprintf (errmsg, "Overflow of jp2_file string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
        cptr = strrchr (jp2_file, '_');
        strcpy (cptr, "_TCI.jp2");
        printf ("  Removing TCI jp2: %s\n", jp2_file);
        unlink (jp2_file);

        /* Also remove the TCI raw band; only exists in the new S2 format */
        cptr = strrchr (jp2_file, '.');
        strcpy (cptr, ".raw");
        printf ("  Removing TCI raw: %s\n", jp2_file);
        unlink (jp2_file);
    }

    /* Free the metadata structure */
    free_metadata (&xml_metadata);

    /* Successful conversion */
    return (SUCCESS);
}

