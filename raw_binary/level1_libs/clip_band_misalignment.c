/*****************************************************************************
FILE: clip_band_misalignment.c
  
PURPOSE: Contains functions for clipping the band mis-alignment in TM and ETM+
products.

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

LICENSE TYPE:  NASA Open Source Agreement Version 1.3

NOTES:
  1. The XML metadata format written via this library follows the ESPA internal
     metadata format found in ESPA Raw Binary Format v1.0.doc.  The schema for
     the ESPA internal metadata format is available at
     http://espa.cr.usgs.gov/schema/espa_internal_metadata_v1_0.xsd.
*****************************************************************************/

#include <unistd.h>
#include <math.h>
#include "clip_band_misalignment.h"


/******************************************************************************
MODULE:  clip_band_misalignment

PURPOSE: Clips bands 1-7 and the thermal band to clean up the band
  misalignment.  Any pixel that is fill in one band will be fill in all bands.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error clipping bands
SUCCESS         Successfully clipped bands

NOTES:
  1. Bands 1-7 and the thermal bands will be clipped so that the alignment of
     all bands match.  The quality band will be updated to mark fill pixels due
     to the band clipping.
  2. This only applies to TM and ETM+ products, thus any other sensors will
     simply be returned as-is.
  3. This is meant to be run on the Level-1 raw binary dataset.
******************************************************************************/
int clip_band_misalignment
(
    Espa_internal_meta_t *xml_metadata  /* I: XML metadata structure populated
                                              from an ESPA XML file */
)
{
    char FUNC_NAME[] = "clip_band_misalignment";  /* function name */
    char errmsg[STR_SIZE];    /* error message */
    char curr_band[STR_SIZE]; /* current band to process */
    int i;                    /* looping variable */
    int l, s;                 /* line, sample looping variable */
    int bnd_count;            /* count of bands to process */
    int bnd;                  /* current band to process */
    int nlines = -99;         /* number of lines in the bands */
    int nsamps = -99;         /* number of samples in the bands */
    int band_options[NBAND_OPTIONS] = {1, 2, 3, 4, 5, 6, 61, 62, 7};
                              /* various bands that will be used for clipping */
    bool fill;                /* is the current pixel fill */
    uint8_t *tmp_file_buf = NULL; /* overall buffer for uint8 input band data */
    uint8_t *file_buf[NBAND_OPTIONS]; /* buffer for uint8 input band data one
                                         for each band */
    uint16_t *bqa_buf = NULL; /* buffer for band quality data */
    Espa_global_meta_t *gmeta = NULL; /* pointer to global metadata structure */
    Espa_band_meta_t *bmeta = NULL;   /* pointer to array of bands metadata */
    FILE *fp_rb[NBAND_OPTIONS];  /* file pointer for the bands */
    FILE *fp_bqa = NULL;         /* file pointer for the band quality band */

    /* Set up the global and band metadata pointers */
    gmeta = &(xml_metadata->global);
    bmeta = xml_metadata->band;

    /* Only process TM and ETM+ bands */
    if (strcmp (gmeta->instrument, "TM") && strcmp (gmeta->instrument, "ETM"))
    {
        sprintf (errmsg, "Only TM and ETM+ will be processed for band "
            "misalignment.  All other instruments are passed back as-is.");
        error_handler (false, FUNC_NAME, errmsg);
        return (SUCCESS);
    }

    /* Loop through the bands and open bands 1-7 and the thermal bands */
    bnd_count = 0;
    for (i = 0; i < xml_metadata->nbands; i++)
    {
        for (bnd = 0; bnd < NBAND_OPTIONS; bnd++)
        {
            /* Is the current band in the metadata one of our expected bands */
            sprintf (curr_band, "b%d", band_options[bnd]);
            if (!strcmp (bmeta[i].name, curr_band))
            {
                /* Open the band file */
                fp_rb[bnd_count] = open_raw_binary (bmeta[i].file_name, "r+");
                if (fp_rb[bnd_count] == NULL)
                {
                    sprintf (errmsg, "Opening the raw binary file: %s",
                        bmeta[i].file_name);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }

                /* If this is the first band then store the image size */
                if (bnd == 0)
                {
                    nlines = bmeta[i].nlines;
                    nsamps = bmeta[i].nsamps;
                }

                /* Increment the band count and goto the next metadata band */
                bnd_count++;
                break;
            }
        }

        /* Is this the quality band */
        sprintf (curr_band, "qa_pixel");
        if (!strcmp (bmeta[i].name, curr_band))
        {
            fp_bqa = open_raw_binary (bmeta[i].file_name, "r+");
            if (fp_bqa == NULL)
            {
                sprintf (errmsg, "Opening the quality band binary file: %s",
                    bmeta[i].file_name);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
        }
    }

    /* Validate the band count TM - 7 bands and ETM+ - 8 bands */
    if (!strcmp (gmeta->instrument, "TM") && (bnd_count != 7))
    {
        sprintf (errmsg, "Expecting 7 TM bands, but only %d bands found.",
            bnd_count);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }
    else if (!strcmp (gmeta->instrument, "ETM") && (bnd_count != 8))
    {
        sprintf (errmsg, "Expecting 8 ETM+ bands, but only %d bands found.",
            bnd_count);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Validate the nlines/nsamps */
    if (nlines == -99 || nsamps == -99)
    {
        sprintf (errmsg, "nlines and/or nsamps are not valid");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Make sure the quality band was found */
    if (fp_bqa == NULL)
    {
        sprintf (errmsg, "Unable to find the band quality band");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Allocate one line of data for each band */
    tmp_file_buf = calloc (nsamps * bnd_count, sizeof (uint8_t));
    if (tmp_file_buf == NULL)
    {
        sprintf (errmsg, "Allocating memory for %d bands of uint8 data "
            "containing %d samples.", bnd_count, nsamps);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Break the buffer into bands */
    file_buf[0] = tmp_file_buf;
    for (i = 1; i < bnd_count; i++)
        file_buf[i] = file_buf[i-1] + nsamps;

    /* Allocate one line of data for the band quality band */
    bqa_buf = calloc (nsamps, sizeof (uint16_t));
    if (bqa_buf == NULL)
    {
        sprintf (errmsg, "Allocating memory for band quality uint16 data "
            "containing %d samples.", nsamps);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Loop through the lines of data and process each file */
    for (l = 0; l < nlines; l++)
    {
        /* Read the current line from each band */
        for (i = 0; i < bnd_count; i++)
        {
            /* Seek to the correct position to read the current line */
            if (fseek (fp_rb[i], l * nsamps * sizeof (uint8_t), SEEK_SET) == -1)
            {   
                sprintf (errmsg, "Not able to seek for line %d of raw binary "
                    "file %d", l, i);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }

            /* Read the line */
            if (read_raw_binary (fp_rb[i], 1, nsamps, sizeof (uint8_t),
                file_buf[i]) != SUCCESS)
            {   
                sprintf (errmsg, "Reading line %d of raw binary file %d", l, i);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
        }

        /* Seek to the correct position to read the current line */
        if (fseek (fp_bqa, l * nsamps * sizeof (uint16_t), SEEK_SET) == -1)
        {   
            sprintf (errmsg, "Not able to seek for line %d of band quality "
                "file", l);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Read the current line from the band quality band */
        if (read_raw_binary (fp_bqa, 1, nsamps, sizeof (uint16_t), bqa_buf) !=
            SUCCESS)
        {   
            sprintf (errmsg, "Reading line %d of band quality file", l);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Loop through all the pixels and assess if any are fill */
        for (s = 0; s < nsamps; s++)
        {
            /* Check the current pixel for each band to be fill */
            fill = false;
            for (i = 0; i < bnd_count; i++)
            {
                /* Check for fill in the current band */
                if (file_buf[i][s] == LEVEL1_FILL)
                {
                    fill = true;
                    break;
                }
            }

            /* If a fill pixel was found or this pixel is flagged as fill in
               the band quality band, then set all bands to fill and set the
               band quality to fill. Technically if the band quality is set to
               fill, then one of the bands should have been flagged as fill.
               However, we have found a few cases where the band quality is
               set to fill and none of the bands are fill. That case is fixed
               in the following code block. */
            if (fill || (bqa_buf[s] == BQA_FILL))
            {
                for (i = 0; i < bnd_count; i++)
                    file_buf[i][s] = LEVEL1_FILL;
                bqa_buf[s] = BQA_FILL;  /* first bit set to 1 for fill */
            }
        }

        /* Write the current line for each band */
        for (i = 0; i < bnd_count; i++)
        {
            /* Seek to the correct position to write the current line */
            if (fseek (fp_rb[i], l * nsamps * sizeof (uint8_t), SEEK_SET) == -1)
            {   
                sprintf (errmsg, "Not able to seek for line %d of raw binary "
                    "file %d", l, i);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }

            /* Write the current line back out to the file */
            if (write_raw_binary (fp_rb[i], 1, nsamps, sizeof (uint8_t),
                file_buf[i]) != SUCCESS)
            {   
                sprintf (errmsg, "Writing line %d of raw binary file %d", l, i);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
        }

        /* Seek to the correct position to write the current line */
        if (fseek (fp_bqa, l * nsamps * sizeof (uint16_t), SEEK_SET) == -1)
        {   
            sprintf (errmsg, "Not able to seek for line %d of band quality "
                "file", l);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Write the current line back out to the band quality file */
        if (write_raw_binary (fp_bqa, 1, nsamps, sizeof (uint16_t), bqa_buf) !=
            SUCCESS)
        {   
            sprintf (errmsg, "Writing line %d of band quality file", l);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }  /* for l in nlines */

    /* Free the raw binary band buffer and the band quality band buffer */
    free (tmp_file_buf);
    free (bqa_buf);

    /* Close the data files */
    for (i = 0; i < bnd_count; i++)
        close_raw_binary (fp_rb[i]);
    close_raw_binary (fp_bqa);

    /* Successful conversion */
    return (SUCCESS);
}
