/*****************************************************************************
FILE: convert_espa_to_raw_binary_bip.c
  
PURPOSE: Contains functions for creating the raw binary band interleave by
pixel (BIP) product and adding bands for this product to the output XML file.

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

LICENSE TYPE:  NASA Open Source Agreement Version 1.3

HISTORY:
Date         Programmer       Reason
----------   --------------   -------------------------------------
8/25/2015    Gail Schmidt     Original development

NOTES:
  1. The XML metadata format written via this library follows the ESPA internal
     metadata format found in ESPA Raw Binary Format v1.0.doc.  The schema for
     the ESPA internal metadata format is available at
     http://espa.cr.usgs.gov/schema/espa_internal_metadata_v1_0.xsd.
*****************************************************************************/
#include <unistd.h>
#include "convert_espa_to_raw_binary_bip.h"

/******************************************************************************
MODULE:  convert_espa_to_raw_binary_bip

PURPOSE: Converts the internal ESPA raw binary file to a raw binary band
interleave by pixel format.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error converting to BIP
SUCCESS         Successfully converted to BIP

HISTORY:
Date         Programmer       Reason
----------   --------------   -------------------------------------
8/25/2015    Gail Schmidt     Original development
8/25/2015    Gail Schmidt     Add support for converting the QA bands to the
                              same data type as band 1

NOTES:
  1. The bands in the XML file will be written, in order, to the BIP file.
     These bands must be of the same datatype and same size, otherwise this
     function will exit with an error.
  2. If the data types are not the same, the convert_qa flag will allow the
     user to specify that the QA bands (uint8) should be included in the output
     BIP product however the QA bands will be converted to the same data type
     as the first band in the XML file.
******************************************************************************/
int convert_espa_to_raw_binary_bip
(
    char *espa_xml_file,   /* I: input ESPA XML metadata filename */
    char *bip_file,        /* I: output BIP filename */
    bool convert_qa,       /* I: should the QA bands (uint8) be converted to
                                 the data type of band 1 (if QA bands are of
                                 a different data type)? */
    bool del_src           /* I: should the source files be removed after
                                 conversion? */
)
{
    char FUNC_NAME[] = "convert_espa_to_raw_binary_bip";  /* function name */
    char errmsg[STR_SIZE];      /* error message */
    char hdr_file[STR_SIZE];    /* name of the header file for this band */
    char xml_file[STR_SIZE];    /* new XML file for the BIP product */
    char envi_file[STR_SIZE];   /* name of the output ENVI header file */
    char *cptr = NULL;          /* pointer to empty space in the band name */
    int i;                      /* looping variable for each band */
    int l;                      /* looping variable for each line */
    int s;                      /* looping variable for each sample */
    int nbytes;                 /* number of bytes per pixel in the data type */
    int nbytes_line;            /* number of bytes per line in the data type */
    int count;                  /* number of chars copied in snprintf */
    int curr_pix;               /* index for current pixel for QA conversion */
    int curr_ipix;              /* index for current input pixel */
    int curr_opix;              /* index for current output pixel */
    int number_elements;        /* number of elements per line for all bands */
    void *file_buf = NULL;      /* pointer to correct input file buffer */
    uint8 *tmp_buf_u8 = NULL;   /* buffer for uint8 QA data to be read */
    uint8 *file_buf_u8 = NULL;  /* buffer for uint8 data to be read */
    int16 *file_buf_i16 = NULL; /* buffer for int16 data to be read */
    int16 *file_buf_u16 = NULL; /* buffer for uint16 data to be read */
    void *ofile_buf = NULL;     /* pointer to correct output file buffer */
    uint8 *ofile_buf_u8 = NULL; /* buffer for output uint8 data to be written */
    int16 *ofile_buf_i16 = NULL;/* buffer for output int16 data to be written */
    int16 *ofile_buf_u16 = NULL;/* buffer for output uint16 data to be
                                   written */
    FILE **fp_rb = NULL;        /* array of file pointers for the input raw
                                   binary files */
    FILE *fp_bip = NULL;        /* file pointer for the BIP raw binary file */
    Espa_internal_meta_t xml_metadata;  /* XML metadata structure to be
                                   populated by reading the input XML metadata
                                   file */
    Espa_band_meta_t *bmeta=NULL; /* pointer to the array of bands metadata */
    Espa_global_meta_t *gmeta=NULL; /* pointer to the global metadata
                                   structure */
    Envi_header_t envi_hdr;     /* output ENVI header information */

    /* Validate the input metadata file */
    if (validate_xml_file (espa_xml_file) != SUCCESS)
    {  /* Error messages already written */
        return (ERROR);
    }

    /* Initialize the metadata structure */
    init_metadata_struct (&xml_metadata);

    /* Parse the metadata file into our internal metadata structure; also
       allocates space as needed for various pointers in the global and band
       metadata */
    if (parse_metadata (espa_xml_file, &xml_metadata) != SUCCESS)
    {  /* Error messages already written */
        return (ERROR);
    }
    bmeta = xml_metadata.band;
    gmeta = &xml_metadata.global;
    printf ("convert_espa_to_raw_binary_bip processing %d bands ...\n",
        xml_metadata.nbands);

    /* Allocate file pointers for each band */
    fp_rb = calloc (xml_metadata.nbands, sizeof (FILE *));
    if (fp_rb == NULL)
    {
        sprintf (errmsg, "Allocating file pointers for all %d bands.",
            xml_metadata.nbands);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Loop through the bands in the XML file and verify they are all of the
       same data type and the same size */
    for (i = 1; i < xml_metadata.nbands; i++)
    {
        if (bmeta[i].data_type != bmeta[0].data_type)
        {
            /* Convert uint8 data types that are flagged as QA */
            if (convert_qa && bmeta[i].data_type == ESPA_UINT8 &&
                !strcmp (bmeta[i].category, "qa"))
            {
                /* all is good, data type will be converted */
                printf ("Band %s will be converted to native data type.\n",
                    bmeta[i].name);
            }
            else
            {
                sprintf (errmsg, "Data type for band %d (%s) in the XML file "
                    "does not match that of the first band.  All bands must "
                    "have the same data type to be written to BIP raw binary. "
                    "Otherwise convert_qa can be specified to convert the QA "
                    "bands (UINT8).", i+1, bmeta[i].name);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
        }
        else if (bmeta[i].nlines != bmeta[0].nlines)
        {
            sprintf (errmsg, "Number of lines for band %d (%s) in the XML file "
                "does not match that of the first band.  All bands must be of "
                "the same image size to be written to BIP raw binary.", i+1,
                bmeta[i].name);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
        else if (bmeta[i].nsamps != bmeta[0].nsamps)
        {
            sprintf (errmsg, "Number of samples for band %d (%s) in the XML "
                "file does not match that of the first band.  All bands must "
                "be of the same image size to be written to BIP raw binary.",
                i+1, bmeta[i].name);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }

    /* Loop through the bands in the XML file and open each band file for
       reading */
    for (i = 0; i < xml_metadata.nbands; i++)
    {
        /* Open the file for this band of data to allow for reading */
        fp_rb[i] = open_raw_binary (bmeta[i].file_name, "rb");
        if (fp_rb[i] == NULL)
        {
            sprintf (errmsg, "Opening the input raw binary file: %s",
                bmeta[i].file_name);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }

    /* Open the output BIP file to allow for writing */
    fp_bip = open_raw_binary (bip_file, "wb");
    if (fp_bip == NULL)
    {
        sprintf (errmsg, "Opening the output raw binary BIP file: %s",
            bip_file);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Allocate memory for a single line of the image and all the bands, based
       on the input data type of the first band */
    if (bmeta[0].data_type == ESPA_UINT8)
    {
        nbytes = sizeof (uint8);

        /* Input data */
        file_buf_u8 = calloc (bmeta[0].nsamps * xml_metadata.nbands, nbytes);
        if (file_buf_u8 == NULL)
        {
            sprintf (errmsg, "Allocating memory for a line of uint8 data "
                "containing %d samples for all %d bands.", bmeta[0].nsamps,
                xml_metadata.nbands);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
        file_buf = file_buf_u8;

        /* Output data */
        ofile_buf_u8 = calloc (bmeta[0].nsamps * xml_metadata.nbands, nbytes);
        if (ofile_buf_u8 == NULL)
        {
            sprintf (errmsg, "Allocating memory for a line of uint8 data "
                "containing %d samples for all %d bands.", bmeta[0].nsamps,
                xml_metadata.nbands);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
        ofile_buf = ofile_buf_u8;
    }
    else if (bmeta[0].data_type == ESPA_INT16)
    {
        nbytes = sizeof (int16);

        /* Input data */
        file_buf_i16 = calloc (bmeta[0].nsamps * xml_metadata.nbands, nbytes);
        if (file_buf_i16 == NULL)
        {
            sprintf (errmsg, "Allocating memory for a line of int16 data "
                "containing %d samples for all %d bands.", bmeta[0].nsamps,
                xml_metadata.nbands);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
        file_buf = file_buf_i16;

        /* Output data */
        ofile_buf_i16 = calloc (bmeta[0].nsamps * xml_metadata.nbands, nbytes);
        if (ofile_buf_i16 == NULL)
        {
            sprintf (errmsg, "Allocating memory for a line of int16 data "
                "containing %d samples for all %d bands.", bmeta[0].nsamps,
                xml_metadata.nbands);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
        ofile_buf = ofile_buf_i16;
    }
    else if (bmeta[0].data_type == ESPA_UINT16)
    {
        nbytes = sizeof (uint16);

        /* Input data */
        file_buf_u16 = calloc (bmeta[0].nsamps * xml_metadata.nbands, nbytes);
        if (file_buf_u16 == NULL)
        {
            sprintf (errmsg, "Allocating memory for a line of uint16 data "
                "containing %d samples for all %d bands.", bmeta[0].nsamps,
                xml_metadata.nbands);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
        file_buf = file_buf_u16;

        /* Output data */
        ofile_buf_u16 = calloc (bmeta[0].nsamps * xml_metadata.nbands, nbytes);
        if (ofile_buf_u16 == NULL)
        {
            sprintf (errmsg, "Allocating memory for a line of uint16 data "
                "containing %d samples for all %d bands.", bmeta[0].nsamps,
                xml_metadata.nbands);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
        ofile_buf = ofile_buf_u16;
    }
    else
    {
        sprintf (errmsg, "Unsupported data type.  Currently only uint8, "
            "int16, and uint16 are supported.");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* The QA bands will be converted so allocate space for a temporary UINT8
       input array */
    if (convert_qa)
    {
        tmp_buf_u8 = calloc (bmeta[0].nsamps, sizeof (uint8));
        if (tmp_buf_u8 == NULL)
        {
            sprintf (errmsg, "Allocating memory for a line of QA data "
                "containing %d samples.", bmeta[0].nsamps);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }

    /* Loop through the lines in the input raw binary file.  Read each line
       for each band, put into the output BIP buffer, and write to the output
       file. */
    nbytes_line = nbytes * bmeta[0].nsamps;
    for (l = 0; l < bmeta[0].nlines; l++)
    {
        if (l % 100 == 0)
            printf ("Line %d\n", l);

        for (i = 0; i < xml_metadata.nbands; i++)
        {
            /* Check to make sure the current band data type is the same as 
               the output data type, otherwise this is a QA band that will
               get converted to the output data type */
            if ((bmeta[0].data_type != bmeta[i].data_type) &&
                (bmeta[i].data_type == ESPA_UINT8) && convert_qa)
            {
                /* Read the current line from the raw binary file into the
                   temporary UINT8 buffer */
                if (read_raw_binary (fp_rb[i], 1, bmeta[0].nsamps,
                    sizeof (uint8), tmp_buf_u8) != SUCCESS)
                {
                    sprintf (errmsg, "Reading QA data from the raw binary "
                        "file for line %d and band %d", l, i);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }

                /* Convert the data and write it to the output buffer */
                if (bmeta[0].data_type == ESPA_INT16)
                {
                    curr_pix = i * bmeta[0].nsamps;
                    for (s = 0; s < bmeta[0].nsamps; s++, curr_pix++)
                        file_buf_i16[curr_pix] = (int16) tmp_buf_u8[s];
                }
                else if (bmeta[0].data_type == ESPA_UINT16)
                {
                    curr_pix = i * bmeta[0].nsamps;
                    for (s = 0; s < bmeta[0].nsamps; s++, curr_pix++)
                        file_buf_u16[curr_pix] = (uint16) tmp_buf_u8[s];
                }
            }
            else
            {
                /* Read the current line from the raw binary file */
                if (read_raw_binary (fp_rb[i], 1, bmeta[0].nsamps, nbytes,
                    &file_buf[i*nbytes_line]) != SUCCESS)
                {
                    sprintf (errmsg, "Reading image data from the raw binary "
                        "file for line %d and band %d", l, i);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
            }
        }  /* end for i */

        /* Loop through the samples and put each band for each pixel into the
           output buffer */
        for (s = 0; s < bmeta[0].nsamps; s++)
        {
            curr_opix = s * xml_metadata.nbands;
            for (i = 0; i < xml_metadata.nbands; i++, curr_opix++)
            {
                curr_ipix = i * bmeta[0].nsamps + s;
                if (bmeta[0].data_type == ESPA_UINT8)
                {
                    ofile_buf_u8[curr_opix] = file_buf_u8[curr_ipix];
                }
                else if (bmeta[0].data_type == ESPA_INT16)
                {
                    ofile_buf_i16[curr_opix] = file_buf_i16[curr_ipix];
                }
                else if (bmeta[0].data_type == ESPA_UINT16)
                {
                    ofile_buf_u16[curr_opix] = file_buf_u16[curr_ipix];
                }
            }
        }

        /* Write the current line of data containing all the bands to the
           output file */
        number_elements = bmeta[0].nsamps * xml_metadata.nbands;
        if (fwrite (ofile_buf, nbytes, number_elements, fp_bip) !=
            number_elements)
        {
            sprintf (errmsg, "Writing data to the BIP raw binary file for "
                "line %d", l);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }  /* end for l */

    /* Close the raw binary files */
    for (i = 0; i < xml_metadata.nbands; i++)
        close_raw_binary (fp_rb[i]);
    close_raw_binary (fp_bip);

    /* Free the memory */
    free (tmp_buf_u8);
    free (file_buf_u8);
    free (file_buf_i16);
    free (file_buf_u16);
    free (ofile_buf_u8);
    free (ofile_buf_i16);
    free (ofile_buf_u16);

    /* Create the ENVI header file for this BIP product */
    if (create_envi_struct (&bmeta[0], gmeta, &envi_hdr) != SUCCESS)
    {
        sprintf (errmsg, "Creating the ENVI header structure for this file.");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Update the ENVI header (created by default for a single BSQ band) to
       represent that this product is a multi-band, BIP file */
    envi_hdr.nbands = xml_metadata.nbands;

    count = snprintf (envi_hdr.interleave, sizeof (envi_hdr.interleave), "%s",
        "BIP");
    if (count < 0 || count >= sizeof (envi_hdr.interleave))
    {
        sprintf (errmsg, "Overflow of envi_hdr.interleave");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    for (i = 0; i < xml_metadata.nbands; i++)
    {
        count = snprintf (envi_hdr.band_names[i],
            sizeof (envi_hdr.band_names[i]), "%s", bmeta[i].name);
        if (count < 0 || count >= sizeof (envi_hdr.band_names))
        {
            sprintf (errmsg, "Overflow of envi_hdr.band_names");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }

    /* Write the ENVI header */
    count = snprintf (envi_file, sizeof (envi_file), "%s", bip_file);
    if (count < 0 || count >= sizeof (envi_file))
    {
        sprintf (errmsg, "Overflow of envi_file string");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }
    cptr = strchr (envi_file, '.');
    strcpy (cptr, ".hdr");

    if (write_envi_hdr (envi_file, &envi_hdr) != SUCCESS)
    {
        sprintf (errmsg, "Writing the ENVI header file: %s.", envi_file);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Remove the source files if specified */
    if (del_src)
    {
        /* Remove the image and header files for each band */
        for (i = 0; i < xml_metadata.nbands; i++)
        {
            printf ("  Removing %s\n", xml_metadata.band[i].file_name);
            if (unlink (xml_metadata.band[i].file_name) != 0)
            {
                sprintf (errmsg, "Deleting source file: %s",
                    xml_metadata.band[i].file_name);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }

            /* .hdr file */
            count = snprintf (hdr_file, sizeof (hdr_file), "%s",
                xml_metadata.band[i].file_name);
            if (count < 0 || count >= sizeof (hdr_file))
            {
                sprintf (errmsg, "Overflow of hdr_file string");
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }

            cptr = strrchr (hdr_file, '.');
            strcpy (cptr, ".hdr");
            printf ("  Removing %s\n", hdr_file);
            if (unlink (hdr_file) != 0)
            {
                sprintf (errmsg, "Deleting source file: %s", hdr_file);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
        }

        /* Remove the source XML */
        printf ("  Removing %s\n", espa_xml_file);
        if (unlink (espa_xml_file) != 0)
        {
            sprintf (errmsg, "Deleting source file: %s", espa_xml_file);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }

    /* Use the input XML file structure for the output XML file since it's the
       same except for the band filenames.  Loop through the bands in the XML
       file and change the filenames to be the single output BIP filename. */
    for (i = 0; i < xml_metadata.nbands; i++)
    {
        count = snprintf (bmeta[i].file_name, sizeof (bmeta[i].file_name), "%s",
            bip_file);
        if (count < 0 || count >= sizeof (bmeta[i].file_name))
        {
            sprintf (errmsg, "Overflow of bmeta.file_name string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }

    /* Create the XML file for the BIP product */
    count = snprintf (xml_file, sizeof (xml_file), "%s", bip_file);
    if (count < 0 || count >= sizeof (xml_file))
    {
        sprintf (errmsg, "Overflow of xml_file string");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }
    cptr = strrchr (xml_file, '.');
    strcpy (cptr, "_bip.xml");

    /* Write the new XML file */
    if (write_metadata (&xml_metadata, xml_file) != SUCCESS)
    {
        sprintf (errmsg, "Error writing updated XML for the GeoTIFF product: "
            "%s", xml_file);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Free the metadata structure */
    free_metadata (&xml_metadata);

    /* Successful conversion */
    return (SUCCESS);
}

