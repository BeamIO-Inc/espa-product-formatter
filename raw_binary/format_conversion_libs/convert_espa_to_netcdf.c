/*****************************************************************************
FILE: convert_espa_to_netcdf.c
  
PURPOSE: Contains functions for creating the NetCDF metadata

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
#include "convert_espa_to_netcdf.h"
#include "gctp_defines.h"

#define OUTPUT_PROVIDER ("DataProvider")
#define OUTPUT_SAT ("Satellite")
#define OUTPUT_INST ("Instrument")
#define OUTPUT_ACQ_DATE ("AcquisitionDate")
#define OUTPUT_L1_PROD_DATE ("Level1ProductionDate")
#define OUTPUT_LPGS_METADATA ("LPGSMetadataFile")
#define OUTPUT_SUN_ZEN ("SolarZenith")
#define OUTPUT_SUN_AZ ("SolarAzimuth")
#define OUTPUT_EARTH_SUN_DIST ("EarthSunDist")
#define OUTPUT_WRS_SYS ("WRS_System")
#define OUTPUT_WRS_PATH ("WRS_Path")
#define OUTPUT_WRS_ROW ("WRS_Row")
#define OUTPUT_SHORT_NAME ("ShortName")
#define OUTPUT_LOCAL_GRAN_ID ("LocalGranuleID")
#define OUTPUT_PROD_DATE ("ProductionDate")

#define OUTPUT_WEST_BOUND  ("WestBoundingCoordinate")
#define OUTPUT_EAST_BOUND  ("EastBoundingCoordinate")
#define OUTPUT_NORTH_BOUND ("NorthBoundingCoordinate")
#define OUTPUT_SOUTH_BOUND ("SouthBoundingCoordinate")
#define UL_LAT_LONG ("UpperLeftCornerLatLong")
#define LR_LAT_LONG ("LowerRightCornerLatLong")
#define OUTPUT_NETCDF_VERSION ("NetCDFVersion")

#define OUTPUT_LONG_NAME        ("long_name")
#define OUTPUT_UNITS            ("units")
#define OUTPUT_VALID_RANGE      ("valid_range")
#define OUTPUT_FILL_VALUE       ("_FillValue")
#define OUTPUT_SATU_VALUE       ("_SaturateValue")
#define OUTPUT_SCALE_FACTOR     ("scale_factor")
#define OUTPUT_ADD_OFFSET       ("add_offset")
#define OUTPUT_APP_VERSION      ("app_version")

/******************************************************************************
MODULE:  write_band_attributes

PURPOSE: Write the attributes (metadata) for the current band in NetCDF format,
using the original metadata from the current band.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error writing the band attributes
SUCCESS         Successfully wrote the band attributes

NOTES:
******************************************************************************/
int write_band_attributes
(
    int ncid,                 /* I: NetCDF file ID to write attributes */
    Espa_band_meta_t *bmeta,  /* I: pointer to band metadata structure */
    int band_varid,           /* I: NetCDF variable ID for the band */ 
    int data_type             /* I: NetCDF data type of the band data */ 
)
{
    char FUNC_NAME[] = "write_band_attributes";  /* function name */
    char errmsg[STR_SIZE];      /* error message */
    char tmp_msg[STR_SIZE];     /* temporary message */
    char message[5000];         /* description of QA bits or classes */
    int i;                      /* looping variable for each SDS */
    int count;                  /* number of chars copied in snprintf */
    signed char byte_dval[MAX_TOTAL_BANDS];/* attribute values to be written */
    unsigned char ubyte_dval[MAX_TOTAL_BANDS];/* attribute values to be 
                                   written */
    float float_dval[MAX_TOTAL_BANDS];/* attribute values to be written */
    double double_dval[MAX_TOTAL_BANDS];/* attribute values to be written */
    int int_dval[MAX_TOTAL_BANDS];/* attribute values to be written */
    unsigned int uint_dval[MAX_TOTAL_BANDS];/* attribute values to be written */
    short short_dval[MAX_TOTAL_BANDS];/* attribute values to be written */
    unsigned short ushort_dval[MAX_TOTAL_BANDS];/* attribute values to be 
                                   written */
    int retval = 0;             /* function call return value */

    /* Write the band-related attributes to the NetCDF file.  Some are required
       and others are optional.  If the optional fields are not defined, then
       they won't be written. */
    retval = nc_put_att_text (ncid, band_varid, OUTPUT_LONG_NAME, 
         strlen(bmeta->long_name), bmeta->long_name);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Writing attribute (long name) to band: %s",
            bmeta->name);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    retval = nc_put_att_text (ncid, band_varid, OUTPUT_UNITS, 
         strlen(bmeta->data_units), bmeta->data_units);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Writing attribute (units ref) to band: %s",
            bmeta->name);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    if (fabs (bmeta->valid_range[0] - ESPA_FLOAT_META_FILL) > ESPA_EPSILON &&
        fabs (bmeta->valid_range[1] - ESPA_FLOAT_META_FILL) > ESPA_EPSILON)
    {
        float_dval[0] = (float) bmeta->valid_range[0];
        float_dval[1] = (float) bmeta->valid_range[1];
        retval = nc_put_att_float (ncid, band_varid, OUTPUT_VALID_RANGE, 
             NC_FLOAT, 2, float_dval);
        if (retval)
        {
            netCDF_ERR (retval);
            sprintf (errmsg, "Writing attribute (valid range) to band: %s",
                bmeta->name);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }

    /* The fill data type must match the band data type. */
    if (bmeta->fill_value != ESPA_INT_META_FILL)
    {
        /* Determine the NetCDF data type */
        switch (data_type)
        {
            case (NC_BYTE):
                byte_dval[0] = (char) bmeta->fill_value;
                retval = nc_put_att_schar (ncid, band_varid, OUTPUT_FILL_VALUE, 
                     NC_BYTE, 1, byte_dval);
                break;
            case (NC_UBYTE):
                ubyte_dval[0] = (unsigned char) bmeta->fill_value;
                retval = nc_put_att_ubyte (ncid, band_varid, OUTPUT_FILL_VALUE, 
                     NC_UBYTE, 1, ubyte_dval);
                break;
            case (NC_SHORT):
                short_dval[0] = (short int) bmeta->fill_value;
                retval = nc_put_att_short (ncid, band_varid, OUTPUT_FILL_VALUE, 
                     NC_SHORT, 1, short_dval);
                break;
            case (NC_USHORT):
                ushort_dval[0] = (unsigned short int) bmeta->fill_value;
                retval = nc_put_att_ushort (ncid, band_varid, OUTPUT_FILL_VALUE,
                     NC_USHORT, 1, ushort_dval);
                break;
            case (NC_INT):
                int_dval[0] = (int) bmeta->fill_value;
                retval = nc_put_att_int (ncid, band_varid, OUTPUT_FILL_VALUE, 
                     NC_INT, 1, int_dval);
                break;
            case (NC_UINT):
                uint_dval[0] = (unsigned int) bmeta->fill_value;
                retval = nc_put_att_uint (ncid, band_varid, OUTPUT_FILL_VALUE, 
                     NC_UINT, 1, uint_dval);
                break;
            case (NC_FLOAT):
                float_dval[0] = (float) bmeta->fill_value;
                retval = nc_put_att_float (ncid, band_varid, OUTPUT_FILL_VALUE, 
                     NC_FLOAT, 1, float_dval);
                break;
            case (NC_DOUBLE):
                double_dval[0] = (double) bmeta->fill_value;
                retval = nc_put_att_double (ncid, band_varid, OUTPUT_FILL_VALUE,
                     NC_DOUBLE, 1, double_dval);
                break;
            default:
                sprintf (errmsg, "Unsupported NetCDF data type.");
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
        }
        if (retval)
        {
            netCDF_ERR (retval);
            sprintf (errmsg, "Writing attribute (fill value) to band: %s",
                bmeta->name);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }

    if (bmeta->saturate_value != ESPA_INT_META_FILL)
    {
        int_dval[0] = (int) bmeta->saturate_value;
        retval = nc_put_att_int (ncid, band_varid, OUTPUT_SATU_VALUE, 
             NC_INT, 1, int_dval);
        if (retval)
        {
            netCDF_ERR (retval);
            sprintf (errmsg, "Writing attribute (saturate value) to band: %s",
                bmeta->name);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }

    if (fabs (bmeta->scale_factor - ESPA_FLOAT_META_FILL) > ESPA_EPSILON)
    {
        float_dval[0] = (float) bmeta->scale_factor;
        retval = nc_put_att_float (ncid, band_varid, OUTPUT_SCALE_FACTOR, 
             NC_FLOAT, 1, float_dval);
        if (retval)
        {
            netCDF_ERR (retval);
            sprintf (errmsg, "Writing attribute (scale factor) to band: %s",
                bmeta->name);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }

    if (fabs (bmeta->add_offset - ESPA_FLOAT_META_FILL) > ESPA_EPSILON)
    {
        float_dval[0] = (float) bmeta->add_offset;
        retval = nc_put_att_float (ncid, band_varid, OUTPUT_ADD_OFFSET, 
             NC_FLOAT, 1, float_dval);
        if (retval)
        {
            netCDF_ERR (retval);
            sprintf (errmsg, "Writing attribute (add offset) to band: %s",
                bmeta->name);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }

    if (bmeta->nbits != ESPA_INT_META_FILL && bmeta->nbits > 0)
    {
        count = snprintf (message, sizeof (message),
            "\n\tBits are numbered from right to left "
            "(bit 0 = LSB, bit N = MSB):\n"
            "\tBit    Description\n");
        if (count < 0 || count >= sizeof (message))
        {
            sprintf (errmsg, "Overflow of message string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        for (i = 0; i < bmeta->nbits; i++)
        {
            count = snprintf (tmp_msg, sizeof (tmp_msg), "\t%d      %s\n", i,
                bmeta->bitmap_description[i]);
            if (count < 0 || count >= sizeof (tmp_msg))
            {
                sprintf (errmsg, "Overflow of tmp_msg string");
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }

            if (strlen (message) + strlen (tmp_msg) >= sizeof (message))
            {
                sprintf (errmsg, "Overflow of message string");
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
            strcat (message, tmp_msg);
        }

        retval = nc_put_att_text (ncid, band_varid, "Bitmap description", 
             strlen(message), message);
        if (retval)
        {
            netCDF_ERR (retval);
            sprintf (errmsg, "Writing attribute (Bitmap description) to band: "
                "%s", bmeta->name);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }

    if (bmeta->nclass != ESPA_INT_META_FILL && bmeta->nclass > 0)
    {
        count = snprintf (message, sizeof (message),
            "\n\tClass  Description\n");
        if (count < 0 || count >= sizeof (message))
        {
            sprintf (errmsg, "Overflow of message string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        for (i = 0; i < bmeta->nclass; i++)
        {
            count = snprintf (tmp_msg, sizeof (tmp_msg), "\t%d      %s\n",
                bmeta->class_values[i].class,
                bmeta->class_values[i].description);
            if (count < 0 || count >= sizeof (tmp_msg))
            {
                sprintf (errmsg, "Overflow of tmp_msg string");
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }

            if (strlen (message) + strlen (tmp_msg) >= sizeof (message))
            {
                sprintf (errmsg, "Overflow of message string");
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
            strcat (message, tmp_msg);
        }

        retval = nc_put_att_text (ncid, band_varid, "Class description", 
             strlen(message), message);
        if (retval)
        {
            netCDF_ERR (retval);
            sprintf (errmsg, "Writing attribute (Class description) to band: "
                "%s", bmeta->name);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }

    retval = nc_put_att_text (ncid, band_varid, OUTPUT_APP_VERSION, 
         strlen(bmeta->app_version), bmeta->app_version);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Writing attribute (app version) to band: %s",
            bmeta->name);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Successful write */
    return (SUCCESS);
}

/******************************************************************************
MODULE:  write_global_attributes

PURPOSE: Write the global attributes (metadata) for the NetCDF file, using the
metadata from the XML file.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error writing the global attributes
SUCCESS         Successfully wrote the global attributes

NOTES:
******************************************************************************/
int write_global_attributes
(
    int ncid,                /* I: NetCDF file ID to write attributes */
    Espa_internal_meta_t *xml_metadata  /* I: pointer to metadata structure */
)
{
    char FUNC_NAME[] = "write_global_attributes";  /* function name */
    char errmsg[STR_SIZE];        /* error message */
    const char *netcdf_version;   /* NetCDF library version */
    int retval = 0;               /* function call return value */


    Espa_global_meta_t *gmeta = &xml_metadata->global;
                                  /* pointer to global metadata structure */

    /* Write the global attributes to the NetCDF file.  Some are required and
       others are optional.  If the optional fields are not defined, then
       they won't be written. */
    retval = nc_put_att_text (ncid, NC_GLOBAL, OUTPUT_PROVIDER, 
         strlen(gmeta->data_provider), gmeta->data_provider);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Error defining the global attribute (data "
                 "provider");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    retval = nc_put_att_text (ncid, NC_GLOBAL, OUTPUT_SAT, 
         strlen(gmeta->satellite), gmeta->satellite);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Error defining the global attribute (satellite)");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    retval = nc_put_att_text (ncid, NC_GLOBAL, OUTPUT_INST, 
         strlen(gmeta->instrument), gmeta->instrument);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Error defining the global attribute (instrument)");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    retval = nc_put_att_text (ncid, NC_GLOBAL, OUTPUT_ACQ_DATE, 
         strlen(gmeta->acquisition_date), gmeta->acquisition_date);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Error defining the global attribute (acquisition "
                 "date)");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    retval = nc_put_att_text (ncid, NC_GLOBAL, OUTPUT_L1_PROD_DATE, 
         strlen(gmeta->level1_production_date), gmeta->level1_production_date);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Error defining the global attribute (production "
                 "date)");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    retval = nc_put_att_text (ncid, NC_GLOBAL, OUTPUT_LPGS_METADATA, 
         strlen(gmeta->lpgs_metadata_file), gmeta->lpgs_metadata_file);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Error defining the global attribute (LPGS metadata "
                 "file)");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    retval = nc_put_att_float (ncid, NC_GLOBAL, OUTPUT_SUN_ZEN, 
         NC_FLOAT, 1, &gmeta->solar_zenith);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Error defining the global attribute (solar zenith)");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    retval = nc_put_att_float (ncid, NC_GLOBAL, OUTPUT_SUN_AZ, 
         NC_FLOAT, 1, &gmeta->solar_azimuth);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Error defining the global attribute (solar azimuth)");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    retval = nc_put_att_float (ncid, NC_GLOBAL, OUTPUT_EARTH_SUN_DIST, 
         NC_FLOAT, 1, &gmeta->earth_sun_dist);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Error defining the global attribute (earth sun "
                "distance)");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    retval = nc_put_att_int (ncid, NC_GLOBAL, OUTPUT_WRS_PATH, NC_INT, 1,
         &gmeta->wrs_path);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Error defining the global attribute (WRS path)");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    retval = nc_put_att_int (ncid, NC_GLOBAL, OUTPUT_WRS_ROW, NC_INT, 1,
         &gmeta->wrs_row);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Error defining the global attribute (WRS row)");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    retval = nc_put_att_double (ncid, NC_GLOBAL, UL_LAT_LONG, NC_DOUBLE, 2,
         gmeta->ul_corner);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Error defining the global attribute (UL corner)");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    retval = nc_put_att_double (ncid, NC_GLOBAL, LR_LAT_LONG, NC_DOUBLE, 2,
         gmeta->lr_corner);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Error defining the global attribute (LR corner)");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    retval = nc_put_att_double (ncid, NC_GLOBAL, OUTPUT_WEST_BOUND, 
        NC_DOUBLE, 1, &gmeta->bounding_coords[ESPA_WEST]);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Error defining the global attribute (west bounding "
                "coord)");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    retval = nc_put_att_double (ncid, NC_GLOBAL, OUTPUT_EAST_BOUND, 
        NC_DOUBLE, 1, &gmeta->bounding_coords[ESPA_EAST]);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Error defining the global attribute (east bounding "
                "coord)");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    retval = nc_put_att_double (ncid, NC_GLOBAL, OUTPUT_NORTH_BOUND, 
        NC_DOUBLE, 1, &gmeta->bounding_coords[ESPA_NORTH]);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Error defining the global attribute (north bounding "
                "coord)");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    retval = nc_put_att_double (ncid, NC_GLOBAL, OUTPUT_SOUTH_BOUND, 
        NC_DOUBLE, 1, &gmeta->bounding_coords[ESPA_SOUTH]);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Error defining the global attribute (south bounding "
                "coord)");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    netcdf_version = nc_inq_libvers();
    retval = nc_put_att_text (ncid, NC_GLOBAL, OUTPUT_NETCDF_VERSION, 
         strlen(netcdf_version), netcdf_version);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Error defining the global attribute (NetCDF "
                 "Version)");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Use the production date from the first band */
    retval = nc_put_att_text (ncid, NC_GLOBAL, OUTPUT_PROD_DATE, 
         strlen(xml_metadata->band[0].production_date), 
         xml_metadata->band[0].production_date);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Error defining the global attribute (production "
                 "date)");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Successful write */
    return (SUCCESS);
}

/******************************************************************************
MODULE:  create_netCDF_metadata

PURPOSE: Create the NetCDF metadata file using info from the XML file.  The 
file will also include the existing raw binary bands.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error creating the NetCDF file
SUCCESS         Successfully created the NetCDF file

NOTES:
  1. The ESPA products are 2D thus only 2D products are supported.
  2. XDim, YDim will refer to the x,y dimension size for the first band.  From
     there, different x,y dimensions will contain the pixel size at the end of
     XDim, YDim.  Example: XDim_15, YDim_15.  For Geographic projections, the
     name will be based on the count of grids instead of the pixel size.
******************************************************************************/
int create_netcdf_metadata
(
    char *netcdf_file,     /* I: output NetCDF filename */
    Espa_internal_meta_t *xml_metadata, /* I: XML metadata structure */
    bool del_src,          /* I: should the source files be removed after
                                 conversion? */
    bool no_compression    /* I: use compression for the NetCDF output file? */
)
{
    char FUNC_NAME[] = "create_netcdf_metadata";  /* function name */
    char errmsg[STR_SIZE];   /* error message */
    char dim_name[2][STR_SIZE];   /* array of dimension names */
    char hdr_file[STR_SIZE];      /* ENVI header file */
    char *cptr = NULL;            /* pointer to the file extension */
    int i;                        /* looping variable for each band */
    int nbytes;                   /* number of bytes in the data type */
    int nlines;                   /* number of lines in the band */
    int nsamps;                   /* number of samples in the band */
    int count;                    /* number of chars copied in snprintf */
    int ngrids;                   /* current number of grids in the product;
                                     different grids are written for different
                                     resolutions (1-based) */
    int mycount;                  /* integer value to use in the name of the
                                     2nd, 3rd, etc. grid dimensions */
    int data_type;                /* data type for NetCDF file */
    int rank = 2;                 /* rank of the band; set for 2D products */
    int x_dimid;                  /* x-dimension ID */
    int y_dimid;                  /* y-dimension ID */
    int x_varid;                  /* x coordinate variable ID */
    int y_varid;                  /* y coordinate variable ID */
    int dimids[2];                /* array for the dimension IDs */
    float *xdims = NULL;          /* coordinate values for the x-dimension */
    float *ydims = NULL;          /* coordinate values for the y-dimension */
    int x;                        /* loop index */
    int y;                        /* loop index */
    FILE *fp_rb = NULL;           /* file pointer for the raw binary file */
    void *file_buf = NULL;        /* pointer to correct input file buffer */
    int ncid;                     /* NetCDF file ID */
    int band_varid;               /* Variable ID for band */
    int retval = 0;               /* function call return value */

    /* Create the NetCDF file.  The NC_NETCDF4 parameter tells NetCDF to create
       a file in NetCDF-4/HDF5 standard. NC_CLOBBER tells NetCDF to overwrite
       this file, if it already exists. */ 
    retval = nc_create (netcdf_file, NC_NETCDF4|NC_CLOBBER, &ncid);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Error creating NetCDF file %s\n", netcdf_file);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Write the global metadata */
    if (write_global_attributes (ncid, xml_metadata) != SUCCESS)
    {
        sprintf (errmsg, "Writing global attributes for this NetCDF file.");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Loop through the bands in the XML file and put each band in the NetCDF 
       file */
    ngrids = 1;
    for (i = 0; i < xml_metadata->nbands; i++)
    {
        /* Provide the status of processing */
        printf ("Processing band: %s\n", xml_metadata->band[i].name);

        /* Open the file for this band of data to allow for reading */
        fp_rb = open_raw_binary (xml_metadata->band[i].file_name, "rb");
        if (fp_rb == NULL)
        {
            sprintf (errmsg, "Opening the input raw binary file: %s",
                xml_metadata->band[i].file_name);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Define the dimensions for this band */
        nlines = xml_metadata->band[i].nlines;
        nsamps = xml_metadata->band[i].nsamps;

        /* Determine the NetCDF data type */
        switch (xml_metadata->band[i].data_type)
        {
            case (ESPA_INT8):
                data_type = NC_BYTE;
                nbytes = 1;
                break;
            case (ESPA_UINT8):
                data_type = NC_UBYTE;
                nbytes = 1;
                break;
            case (ESPA_INT16):
                data_type = NC_SHORT;
                nbytes = 2;
                break;
            case (ESPA_UINT16):
                data_type = NC_USHORT;
                nbytes = 2;
                break;
            case (ESPA_INT32):
                data_type = NC_INT;
                nbytes = 4;
                break;
            case (ESPA_UINT32):
                data_type = NC_UINT;
                nbytes = 4;
                break;
            case (ESPA_FLOAT32):
                data_type = NC_FLOAT;
                nbytes = 4;
                break;
            case (ESPA_FLOAT64):
                data_type = NC_DOUBLE;
                nbytes = 8;
                break;
            default:
                sprintf (errmsg, "Unsupported ESPA data type.");
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
        }

        /* Allocate memory for the file buffer */
        file_buf = calloc (nlines * nsamps, nbytes);
        if (file_buf == NULL)
        {
            sprintf (errmsg, "Error allocating memory for the file buffer.");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Read the data from the raw binary file */
        if (read_raw_binary (fp_rb, nlines, nsamps, nbytes, file_buf) !=
            SUCCESS)
        {
            sprintf (errmsg, "Reading image data from the raw binary file");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Close the raw binary file */
        close_raw_binary (fp_rb);

        /* Set the dimension names for this band.  The default is to use YDim,
           XDim for the first band or for any bands matching the resolution of
           the first band */
        if (i == 0 ||
            ((xml_metadata->band[i].pixel_size[0] ==
             xml_metadata->band[0].pixel_size[0]) &&
            (xml_metadata->band[i].pixel_size[1] ==
             xml_metadata->band[0].pixel_size[1])))
        {  /* first band or resolution matching the first band */
            count = snprintf (dim_name[0], sizeof(dim_name[0]), "YDim_%s", 
                xml_metadata->band[i].name);
            if (count < 0 || count >= sizeof (dim_name[0]))
            {
                sprintf (errmsg, "Overflow of dim_name[0] string");
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
            count = snprintf (dim_name[1], sizeof(dim_name[1]), "XDim_%s", 
                xml_metadata->band[i].name);
            if (count < 0 || count >= sizeof (dim_name[1]))
            {
                sprintf (errmsg, "Overflow of dim_name[1] string");
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
        }
        else
        {  /* create new dimension name for this resolution */
            /* Use the pixel size for non-geographic projections otherwise
               use the grid count */
            ngrids++;
            if (xml_metadata->global.proj_info.proj_type == GCTP_GEO_PROJ)
                mycount = ngrids;
            else
                mycount = (int) xml_metadata->band[i].pixel_size[1]; /* Y dim */

            count = snprintf (dim_name[0], sizeof (dim_name[0]), 
                "YDim_%s_%d", xml_metadata->band[i].name, mycount);  /* Y dim */
            if (count < 0 || count >= sizeof (dim_name[0]))
            {
                sprintf (errmsg, "Overflow of dim_name[0] string");
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }

            if (xml_metadata->global.proj_info.proj_type == GCTP_GEO_PROJ)
                mycount = ngrids;
            else
                mycount = (int) xml_metadata->band[i].pixel_size[0]; /* X dim */

            count = snprintf (dim_name[1], sizeof (dim_name[1]),
                "XDim_%s_%d", xml_metadata->band[i].name, mycount);  /* X dim */
            if (count < 0 || count >= sizeof (dim_name[1]))
            {
                sprintf (errmsg, "Overflow of dim_name[1] string");
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
        }

        /* Define the dimensions for the band. */
        if ((retval = nc_def_dim (ncid, dim_name[1], nsamps, &x_dimid)))
        {
            netCDF_ERR (retval);
            sprintf (errmsg, "Error creating the x dimension of size %d", 
                nsamps);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
        if ((retval = nc_def_dim (ncid, dim_name[0], nlines, &y_dimid)))
        {
            netCDF_ERR (retval);
            sprintf (errmsg, "Error creating the y dimension of size %d", 
                nlines);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Define the x coordinate variable and attributes */
        if ((retval = nc_def_var (ncid, dim_name[1], NC_FLOAT, 1, &x_dimid,
             &x_varid)))
        {
            netCDF_ERR (retval);
            sprintf (errmsg, "Error defining variable: %s", dim_name[1]);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Set up data compression if it was specified */
        if (!no_compression)
        {
            /* Specify compression for this variable */
            if ((retval = nc_def_var_deflate (ncid, x_varid, SHUFFLE, DEFLATE,
                 DEFLATE_LEVEL)))
            {
                netCDF_ERR (retval);
                sprintf (errmsg, "Error specifying the compression for "
                    "variable: %s", dim_name[1]);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }

            /* Increase the chunk cache size */
            if ((retval = nc_set_var_chunk_cache (ncid, x_varid, CACHE_SIZE,
                 CACHE_NELEMS, CACHE_PREEMPTION)))
            {
                netCDF_ERR (retval);
                sprintf (errmsg, "Error specifying the chunk cache size for "
                    "variable: %s", dim_name[1]);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
        }

        /* Define the y coordinate variable and attributes */
        if ((retval = nc_def_var (ncid, dim_name[0], NC_FLOAT, 1, &y_dimid,
             &y_varid)))
        {
            netCDF_ERR (retval);
            sprintf (errmsg, "Error defining variable: %s", dim_name[0]);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Set up data compression if it was specified */
        if (!no_compression)
        {
            /* Specify compression for this variable */
            if ((retval = nc_def_var_deflate (ncid, y_varid, SHUFFLE, DEFLATE,
                 DEFLATE_LEVEL)))
            {
                netCDF_ERR (retval);
                sprintf (errmsg, "Error specifying the compression for "
                    "variable: %s", dim_name[0]);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }

            /* Increase the chunk cache size */
            if ((retval = nc_set_var_chunk_cache (ncid, y_varid, CACHE_SIZE,
                 CACHE_NELEMS, CACHE_PREEMPTION)))
            {
                netCDF_ERR (retval);
                sprintf (errmsg, "Error specifying the chunk cache size for "
                    "variable: %s", dim_name[0]);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
        }

        /* Define the band variable */
        dimids[0] = y_dimid;   /* lines */
        dimids[1] = x_dimid;   /* samples */
        if ((retval = nc_def_var (ncid, xml_metadata->band[i].name, data_type,
            rank, dimids, &band_varid)))
        {
            netCDF_ERR (retval);
            sprintf (errmsg, "Error defining band variable: %s", 
                xml_metadata->band[i].name);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Specify compression for the primary variable */
        if (!no_compression)
        {
            if ((retval = nc_def_var_deflate (ncid, band_varid, SHUFFLE,
                 DEFLATE, DEFLATE_LEVEL)))
            {
                netCDF_ERR (retval);
                sprintf (errmsg, "Error specifying the compression for "
                    "variable: %s", xml_metadata->band[i].name);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }

            /* Increase the chunk cache size */
            if ((retval = nc_set_var_chunk_cache (ncid, band_varid, CACHE_SIZE,
                 CACHE_NELEMS, CACHE_PREEMPTION)))
            {
                netCDF_ERR (retval);
                sprintf (errmsg, "Error specifying the chunk cache size for "
                    "variable: %s", xml_metadata->band[i].name);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
        }

        /* End define mode. This tells NetCDF we are done defining metadata
           and are moving to writing the data. */
        if ((retval = nc_enddef (ncid)))
        {
            netCDF_ERR (retval);
            sprintf (errmsg, "Error ending the define mode.");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Allocate space for the x coordinate variable */
        xdims = (float *) calloc (nsamps, sizeof (float));
        if (xdims == NULL)
        {
            sprintf (errmsg, "Error allocating %d floats for xdims", nsamps);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Create the x grid locations for the coordinate variables */
        for (x = 0; x < nsamps; x++)
        {
            xdims[x] = xml_metadata->global.proj_info.ul_corner[0] 
                + xml_metadata->band[i].pixel_size[0] * x;
        }

        /* Write the x coordinate variables */
        if ((retval = nc_put_var_float (ncid, x_varid, xdims)))
        {
            netCDF_ERR (retval);
            sprintf (errmsg, "Error writing x coordinate data to variable");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Free the x coordinate variables */
        free (xdims);
        xdims = NULL;

        /* Allocate space for the y coordinate variable */
        ydims = (float *) calloc (nlines, sizeof (float));
        if (ydims == NULL)
        {
            sprintf (errmsg, "Error allocating %d floats for ydims", nlines);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Create the y grid locations for the coordinate variables */
        for (y = 0; y < nlines; y++)
        {
            ydims[y] = xml_metadata->global.proj_info.ul_corner[1] 
                - xml_metadata->band[i].pixel_size[1] * y;
        }

        /* Write the y coordinate variables */
        if ((retval = nc_put_var_float (ncid, y_varid, ydims)))
        {
            netCDF_ERR (retval);
            sprintf (errmsg, "Error writing y coordinate data to variable");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Free the y coordinate variables */
        free (ydims);
        ydims = NULL;

        /* Write the band metadata.  This must happen before the band data is
           written since the fill value must be written before the band data. */
        if (write_band_attributes (ncid, &xml_metadata->band[i], band_varid,
            data_type) != SUCCESS)
        {
            sprintf (errmsg, "Writing %s attributes for this NetCDF file.",
                xml_metadata->band[i].name);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Write the band variable */
        if ((retval = nc_put_var (ncid, band_varid, file_buf)))
        {
            netCDF_ERR (retval);
            sprintf (errmsg, "Error writing %s data to variable",
                xml_metadata->band[i].name);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Free the file buffer. */
        free (file_buf);
        file_buf = NULL;

        /* Remove the source files if specified */
        if (del_src)
        {
            /* .img file */
            printf ("  Removing %s\n", xml_metadata->band[i].file_name);
            if (unlink (xml_metadata->band[i].file_name) != 0)
            {
                sprintf (errmsg, "Deleting source file: %s",
                    xml_metadata->band[i].file_name);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }

            /* .hdr file */
            count = snprintf (hdr_file, sizeof (hdr_file), "%s",
                xml_metadata->band[i].file_name);
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
    }

    /* Close the NetCDF file. This frees up any internal NetCDF resources
       associated with the file and flushes any buffers. */
    retval = nc_close (ncid);
    if (retval)
    {
        netCDF_ERR (retval);
        sprintf (errmsg, "Error closing NetCDF file %s\n", netcdf_file);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Successful conversion */
    return (SUCCESS);
}

/******************************************************************************
MODULE:  convert_espa_to_netcdf

PURPOSE: Converts the internal ESPA raw binary file to NetCDF file format.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error converting to NetCDF
SUCCESS         Successfully converted to NetCDF

NOTES:
  1. The ESPA raw binary band files will be included in the NetCDF file, 
     rather than being external files. 
  2. No ENVI header file will be created. 
  3. Compression will be used.
******************************************************************************/
int convert_espa_to_netcdf
(
    char *espa_xml_file,   /* I: input ESPA XML metadata filename */
    char *netcdf_file,     /* I: output NetCDF filename */
    bool del_src,          /* I: should the source files be removed after
                                 conversion? */
    bool no_compression    /* I: use compression for the NetCDF output file? */
)
{
    char FUNC_NAME[] = "convert_espa_to_netcdf";  /* function name */
    char errmsg[STR_SIZE];   /* error message */
    char xml_file[STR_SIZE]; /* new XML file for the NetCDF product */
    char *cptr = NULL;       /* pointer to empty space in the band name */
    int i;                   /* band looping variable */
    int count;               /* number of chars copied in snprintf */
    Espa_internal_meta_t xml_metadata;  /* XML metadata structure to be
                                populated by reading the MTL metadata file */

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

    /* Create the NetCDF file for the NetCDF metadata from the XML metadata. */
    if (create_netcdf_metadata (netcdf_file, &xml_metadata, del_src, 
        no_compression) != SUCCESS)
    {
        sprintf (errmsg, "Creating the NetCDF metadata file (%s) which "
            "includes the raw binary bands.", netcdf_file);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Remove the source files if specified */
    if (del_src)
    {
        /* XML file */
        printf ("  Removing %s\n", espa_xml_file);
        if (unlink (espa_xml_file) != 0)
        {
            sprintf (errmsg, "Deleting source file: %s", espa_xml_file);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }

    /* Loop through the bands and modify the band names to be those of the
       overall netCDF product */
    for (i = 0; i < xml_metadata.nbands; i++)
        strcpy (xml_metadata.band[i].file_name, netcdf_file);

    /* Create the XML file for the NetCDF product */
    count = snprintf (xml_file, sizeof (xml_file), "%s", netcdf_file);
    if (count < 0 || count >= sizeof (xml_file))
    {
        sprintf (errmsg, "Overflow of xml_file string");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    cptr = strrchr (xml_file, '.');
    if (cptr != NULL)
    {
        /* File extension found.  Replace it with the new extension */
        *cptr = '\0';
        strcpy (cptr, "_nc.xml");
    }
    else
    {
        /* File extension found.  Replace it with the new extension */
        strcat (xml_file, "_nc.xml");
    }

    /* Write the new XML file containing the new band names */
    if (write_metadata (&xml_metadata, xml_file) != SUCCESS)
    {
        sprintf (errmsg, "Error writing updated XML for the NetCDF product: %s",
            xml_file);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Free the metadata structure */
    free_metadata (&xml_metadata);

    /* Successful conversion */
    return (SUCCESS);
}

