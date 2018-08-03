/*****************************************************************************
FILE: convert_viirs_to_espa.c
  
PURPOSE: Contains functions for reading VIIRS HDF5 products and writing to ESPA
raw binary format.

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

LICENSE TYPE:  NASA Open Source Agreement Version 1.3

NOTES:
  1. The XML metadata format written via this library follows the ESPA internal
     metadata format found in ESPA Raw Binary Format v1.0.doc.  The schema for
     the ESPA internal metadata format is available at
     http://espa.cr.usgs.gov/schema/espa_internal_metadata_vx_y.xsd.
  2. The VIIRS VNP09GA products are defined in
     https://lpdaac.usgs.gov/dataset_discovery/viirs/viirs_products_table/vnp09ga_v001
  3. This library will only ingest and support the three 500m imagery bands
     from the VIIRS surface reflectance product.  Those bands exist in the
     /HDFEOS/GRIDS/VNP_Grid_500m_2D/Data Fields structure and are named
     SurfReflect_I[1|2|3]
  4. The geolocation information requires accessing the HDF5 product as an
     HDFEOS5 product, using the HDFEOS5 libraries.
*****************************************************************************/
#include <unistd.h>
#include <math.h>
#include <ctype.h>
#include "convert_viirs_to_espa.h"

/******************************************************************************
MODULE:  doy_to_month_day

PURPOSE: Convert the DOY to month and day.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error converting from DOY to month and day
SUCCESS         Successfully converted from the DOY to the month and day

NOTES:
******************************************************************************/
int doy_to_month_day
(
    int year,            /* I: year of the DOY to be converted */
    int doy,             /* I: DOY to be converted */
    int *month,          /* O: month of the DOY */
    int *day             /* O: day of the DOY */
)
{
    char FUNC_NAME[] = "doy_to_month_day";  /* function name */
    char errmsg[STR_SIZE];    /* error message */
    bool leap;                /* is this a leap year? */
    int i;                    /* looping variable */
    int nday_lp[12] = {31, 29, 31, 30,  31,  30,  31,  31,  30,  31,  30,  31};
        /* number of days in each month (for leap year) */
    int idoy_lp[12] = { 1, 32, 61, 92, 122, 153, 183, 214, 245, 275, 306, 336};
        /* starting DOY for each month (for leap year) */
    int nday[12] = {31, 28, 31, 30,  31,  30,  31,  31,  30,  31,  30,  31};
        /* number of days in each month (with Feb being a leap year) */
    int idoy[12] = { 1, 32, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};
        /* starting DOY for each month */

    /* Is this a leap year? */
    leap = (bool) (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));

    /* Determine which month the DOY falls in */
    *month = 0;
    if (leap)
    {  /* leap year -- start with February */
        for (i = 1; i < 12; i++)
        {
            if (idoy_lp[i] > doy)
            {
                *month = i;
                *day = doy - idoy_lp[i-1] + 1;
                break;
            }
        }

        /* if the month isn't set, then it's a December scene */
        if (*month == 0)
        {
            *month = 12;
            *day = doy - idoy_lp[11] + 1;
        }
    }
    else
    {  /* non leap year -- start with February */
        for (i = 1; i < 12; i++)
        {
            if (idoy[i] > doy)
            {
                *month = i;
                *day = doy - idoy[i-1] + 1;
                break;
            }
        }

        /* if the month isn't set, then it's a December scene */
        if (*month == 0)
        {
            *month = 12;
            *day = doy - idoy[11] + 1;
        }
    }

    /* Validate the month and day */
    if (*month < 1 || *month > 12)
    {
        sprintf (errmsg, "Invalid month: %d\n", *month);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    if (leap)
    {  /* leap year */
        if (*day < 1 || *day > nday_lp[(*month)-1])
        {
            sprintf (errmsg, "Invalid day: %d-%d-%d\n", year, *month, *day);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }
    else
    {  /* non leap year */
        if (*day < 1 || *day > nday[(*month)-1])
        {
            sprintf (errmsg, "Invalid day: %d-%d-%d\n", year, *month, *day);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }

    /* Successful conversion */
    return (SUCCESS);
}


/******************************************************************************
MODULE:  cleanup_file_name

PURPOSE:  Cleans up the filenames by replacing blank spaces in the filename
          with underscores.

RETURN VALUE:
Type = None

NOTES:
******************************************************************************/
void cleanup_file_name
(
    char instr[]         /* I: input string to be cleaned up */
)

{
    char *inptr = instr;     /* pointer to the input string */

    while (*inptr != '\0')
    {
        /* Change ' ' to '_' */
        if (*inptr == ' ')
            *inptr = '_';

        /* Next character */
        inptr++;
    }
}


/******************************************************************************
MODULE: get_acquisition_date

PURPOSE: Get the acquisition date from the input HDF filename and populate
the acquisition date in the global metadata.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error determining acquisition date
SUCCESS         Successfully obtained acquisition date

NOTES:
******************************************************************************/
int get_acquisition_date
(
    char *basename,        /* I: base filename (no path) of HDF file */
    Espa_global_meta_t *gmeta  /* I/O: pointer to the global metadata; the
                                       acquisition date is populated */
)
{
    char FUNC_NAME[] = "get_acquisition_date";  /* function name */
    char errmsg[STR_SIZE];    /* error message */
    char yearstr[5];          /* string to hold the acquisition year */
    char doystr[4];           /* string to hold the acquisition DOY */
    int acq_doy;              /* acquisition DOY */
    int acq_year;             /* acquisition year */
    int acq_month;            /* acquisition month */
    int acq_day;              /* acquisition day */
    int count;                /* number of chars copied in snprintf */

    /* Use the HDF5 filename to determine the acquisition date as yyyyddd.
       Example - VNP09GA.A2012289.h09v05.001.2016325003544.h5 */
    if (strncpy (yearstr, &basename[9], 4) == NULL)
    {
        sprintf (errmsg, "Error pulling the acquisition year from the base "
            "filename: %s", basename);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }
    yearstr[4] = '\0';
    acq_year = atoi (yearstr);

    if (strncpy (doystr, &basename[13], 3) == NULL)
    {
        sprintf (errmsg, "Error pulling the acquisition DOY from the base "
            "filename: %s", basename);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }
    doystr[3] = '\0';
    acq_doy = atoi (doystr);

    /* Year and DOY need to be converted to yyyy-mm-dd */
    if (doy_to_month_day (acq_year, acq_doy, &acq_month, &acq_day) != SUCCESS)
    {
        sprintf (errmsg, "Error converting %d-%d to yyyy-mm-dd", acq_year,
            acq_doy);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    count = snprintf (gmeta->acquisition_date, sizeof (gmeta->acquisition_date),
        "%04d-%02d-%02d", acq_year, acq_month, acq_day);
    if (count < 0 || count >= sizeof (gmeta->acquisition_date))
    {
        sprintf (errmsg, "Overflow of gmeta->acquisition_date string");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    return (SUCCESS);
}


/******************************************************************************
MODULE: get_tile

PURPOSE: Get the htile/vtile from the input HDF filename and populate the
htile/vtile in the global metadata.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error determining htile/vtile
SUCCESS         Successfully obtained htile/vtile

NOTES:
******************************************************************************/
int get_tile
(
    char *basename,        /* I: base filename (no path) of HDF file */
    Espa_global_meta_t *gmeta  /* I/O: pointer to the global metadata; the
                                       htile/vtile is populated */
)
{
    char FUNC_NAME[] = "get_tile";  /* function name */
    char errmsg[STR_SIZE];    /* error message */
    char htile[3];            /* string to hold the horiztonal tile */
    char vtile[3];            /* string to hold the vertical tile */

    /* Use the HDF5 filename to determine the horizontal and vertical tile
       numbers.  Example - VNP09GA.A2012289.h09v05.001.2016325003544.h5 */
    if (strncpy (htile, &basename[19], 2) == NULL)
    {
        sprintf (errmsg, "Error pulling the horizontal tile number from the "
            "base filename: %s", basename);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }
    htile[2] = '\0';
    gmeta->htile = atoi (htile);

    if (strncpy (vtile, &basename[22], 2) == NULL)
    {
        sprintf (errmsg, "Error pulling the vertical tile number from the "
            "base filename: %s", basename);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }
    vtile[2] = '\0';
    gmeta->vtile = atoi (vtile);

    return (SUCCESS);
}


/******************************************************************************
MODULE:  read_attribute

PURPOSE: Read the specified attribute from the specified dataset.  The
data type for the attribute must match the data type on the variable used
for output and memory must already be allocated.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error reading the specified attribute
SUCCESS         Successfully read the attribute

NOTES:
******************************************************************************/
int read_attribute
(
    char *attr_name,       /* I: name of the attribute to read */
    hid_t dataset_id,      /* I: dataset ID to read the specified attribute */
    void *attr_val         /* O: pointer to the attribute value; datatype
                                 must match that which is specified by the
                                 data_type */
)
{
    char FUNC_NAME[] = "read_attribute";  /* function name */
    char errmsg[STR_SIZE];    /* error message */
    hid_t attr_id;            /* attribute ID for the current dataset */
    hid_t atype;              /* datatype of the attribute */
    herr_t status;            /* return status */

    /* Read the specified attribute from the dataset */
    attr_id = H5Aopen_name (dataset_id, attr_name);
    if (attr_id < 0)
    {
        sprintf (errmsg, "Unable to open attribute: %s", attr_name);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    atype = H5Aget_type (attr_id);
    status = H5Aread (attr_id, atype, attr_val);
    if (status < 0)
    {
        sprintf (errmsg, "Unable to read attribute: %s", attr_name);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    status = H5Aclose (attr_id);
    if (status < 0)
    {
        sprintf (errmsg, "Terminating access to attribute: %s", attr_name);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    return (SUCCESS);
}


/******************************************************************************
MODULE:  read_viirs_500m_geo_meta

PURPOSE: Read the geolocation metadata (HDF-EOS) for the surface reflectance
imagery bands from VIIRS 500m grid and populate the ESPA internal metadata
structure.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error reading the VIIRS HDF-EOS metadata
SUCCESS         Successfully populated the ESPA metadata structure

NOTES:
******************************************************************************/
int read_viirs_500m_geo_meta
(
    char *viirs_hdf_name,            /* I: name of VIIRS file to be read */
    Espa_internal_meta_t *metadata   /* I/O: input metadata structure to be
                                           populated from the VIIRS file */
)
{
    char FUNC_NAME[] = "read_viirs_500m_geo_meta";  /* function name */
    char errmsg[STR_SIZE];    /* error message */
    char *grid500 = NULL;     /* name of the 500m HDF-EOS grid group */
    hid_t file_id;            /* file ID for HDF-EOS */
    hid_t grid_id;            /* grid ID for 500m grid */
    hid_t status;             /* return status from HDF-EOS function */
    int projcode;             /* projection code */
    int zonecode;             /* UTM zone code */
    int spherecode;           /* sphere code */
    int origincode;           /* grid origin for corner points */
    long xdimsize;            /* x-dimension */
    long ydimsize;            /* y-dimension */
    double projparm[15];      /* projection parameters */
    double central_meridian;  /* central meridian for the sinusoidal projection
                                 (in DMS) */
    Espa_global_meta_t *gmeta = &metadata->global;  /* pointer to the global
                                                       metadata structure */

    /* Open VIIRS file for reading as a HDF-EOS file */
    file_id = HE5_GDopen (viirs_hdf_name, H5F_ACC_RDONLY);
    if (file_id < 0)
    {
        sprintf (errmsg, "Unable to open %s", viirs_hdf_name);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Attach to the 500m grid */
    grid500 = "VNP_Grid_500m_2D";
    grid_id = HE5_GDattach (file_id, grid500);
    if (grid_id < 0)
    {
        sprintf (errmsg, "Unable to attach to grid: %s", grid500);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Get the projection information for this grid */
    status = HE5_GDprojinfo (grid_id, &projcode, &zonecode, &spherecode,
        projparm);
    if (status != 0)
    {
        sprintf (errmsg, "Reading grid projection information from HDFEOS "
            "header");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Store the projection info */
    if (projcode != HE5_GCTP_SNSOID)
    {
        sprintf (errmsg, "Invalid projection type.  VIIRS data is "
            "expected to be in the Sinusoidal projection.");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }
    gmeta->proj_info.proj_type = GCTP_SIN_PROJ;

    if (spherecode != ESPA_NODATUM)
    {
        sprintf (errmsg, "Invalid sphere code.  VIIRS data is expected "
            "to be in the Sinusoidal projection and have a sphere code "
            "of %d.", ESPA_NODATUM);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }
    gmeta->proj_info.datum_type = ESPA_NODATUM;
    strcpy (gmeta->proj_info.units, "meters");

    /* Store the input projection parameters for Sinusoidal */
    gmeta->proj_info.sphere_radius = projparm[0];
    central_meridian = projparm[4];
    gmeta->proj_info.false_easting = projparm[6];
    gmeta->proj_info.false_northing = projparm[7];

    /* According to HDF-EOS documentation angular projection parameters
       in HDF-EOS structural metadata are in DMS.  Convert the central
       meridian from DMS to decimal degrees. */
    dmsdeg (central_meridian, &gmeta->proj_info.central_meridian);

    /* Get the grid dimension and corner info. Projection coords are in
       meters since this should be the Sinusoidal projection. */
    status = HE5_GDgridinfo (grid_id, &xdimsize, &ydimsize,
        gmeta->proj_info.ul_corner, gmeta->proj_info.lr_corner);
    if (status != 0)
    {
        sprintf (errmsg, "Reading dimension and corner information "
            "from HDF header");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Get the coordinate system origin.  If the grid origin isn't
       specified then default to the UL which is standard for HDF. */
    status = HE5_GDorigininfo (grid_id, &origincode);
    if (status != 0)
        strcpy (gmeta->proj_info.grid_origin, "UL");
    else if (origincode == HE5_HDFE_GD_UL)
        strcpy (gmeta->proj_info.grid_origin, "UL");
    else
        strcpy (gmeta->proj_info.grid_origin, "CENTER");

    /* Close grid */
    HE5_GDdetach (grid_id);

    /* Close the HDF-EOS file */
    HE5_GDclose (file_id);

    /* Successful read */
    return (SUCCESS);
}


/******************************************************************************
MODULE:  read_viirs_500m_grid_meta

PURPOSE: Read the metadata for the surface reflectance imagery bands from the
VIIRS 500m grid and populate the ESPA internal metadata structure for each of
the bands.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error reading the VIIRS metadata
SUCCESS         Successfully populated the ESPA metadata structure

NOTES:
******************************************************************************/
int read_viirs_500m_grid_meta
(
    char *viirs_hdf_name,            /* I: name of VIIRS file to be read */
    Espa_internal_meta_t *metadata   /* I/O: input metadata structure to be
                                           populated from the VIIRS file */
)
{
    char FUNC_NAME[] = "read_viirs_500m_grid_meta";  /* function name */
    char errmsg[STR_SIZE];    /* error message */
    char basename[STR_SIZE];  /* filename without path (uppercase) */
    char core_basename[STR_SIZE]; /* filename without path and extension */
    char dataset_name[STR_SIZE];  /* name of dataset in the 500m grid */
    char *grid500 = NULL;         /* name of the 500m grid group */
    char prod_date_time[STR_SIZE];  /* production date/time */
    char pge_version[STR_SIZE];     /* PGE version */
    char viirs_bands[MAX_VIIRS_BANDS][STR_SIZE]; /* array containing names of
                                 the VIIRS bands/SDSs to be written to the
                                 ESPA XML file */
    char longname[MAX_VIIRS_BANDS][STR_SIZE]; /* array long_name attributes */
    char qa_desc[MAX_VIIRS_BANDS][HUGE_STR_SIZE]; /* array qa description
                                                     attributes */
    char units[MAX_VIIRS_BANDS][STR_SIZE];    /* array units attributes */
    char *cptr = NULL;        /* character pointer for strings */
    int i;                    /* looping variables */
    int count;                /* number of chars copied in snprintf */
    int nviirs_bands;         /* number of bands that will be in the ESPA
                                 product from the VIIRS file */
    double scalevalue[MAX_VIIRS_BANDS];    /* scale factor for current SDS */
    double offsetvalue[MAX_VIIRS_BANDS];   /* offset for current SDS */
    double minvalue[MAX_VIIRS_BANDS];  /* minimum band value for current SDS */
    double maxvalue[MAX_VIIRS_BANDS];  /* maximum band value for current SDS */
    double fillvalue[MAX_VIIRS_BANDS]; /* fill value for current SDS */
    int data_type[MAX_VIIRS_BANDS];    /* data type for each SDS */

    herr_t status;             /* return status */
    hid_t file_id;             /* file ID for the VIIRS file */
    hid_t root_id;             /* group ID for the root grid */
    hid_t grid500_id;          /* group ID for the 500m grid */
    hid_t dataset_id;          /* current dataset ID for the VIIRS file */
    hid_t dtype_id;            /* datatype ID for the current dataset */
    hid_t dspace_id;           /* data space ID for the current dataset */
    size_t t_size;             /* size of the datatype in bytes */
    H5G_info_t grid500_info;   /* group information for the 500m grid */
    H5T_class_t t_class;       /* data type class */
    H5T_sign_t t_sign;         /* signed/unsigned datatype */
    int ndims;                 /* number of dimensions in the dataset */
    hsize_t dims[2];           /* 2D array dimensions */
    int grid_dims[MAX_VIIRS_BANDS][2];  /* x,y dimensions of current band */

    Img_coord_float_t img;        /* image coordinates for current pixel */
    Geo_coord_t geo;              /* geodetic coordinates (note radians) */
    Space_def_t geoloc_def;       /* geolocation space information */
    Geoloc_t *geoloc_map = NULL;  /* geolocation mapping information */
    Espa_global_meta_t *gmeta = &metadata->global;  /* pointer to the global
                                                       metadata structure */
    Espa_band_meta_t *bmeta=NULL; /* pointer to the array of bands metadata */

    /* Get the basename of the input HDF5 file */
    cptr = strrchr (viirs_hdf_name, '/');
    if (cptr != NULL)
    {
        /* Copy the basename from the cptr, after moving off of the '/' */
        cptr++;
        count = snprintf (basename, sizeof (basename), "%s", cptr);
        if (count < 0 || count >= sizeof (basename))
        {
            sprintf (errmsg, "Overflow of basename string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }
    else
    {
        /* Copy the filename itself as the basename since it doesn't have a
           path in the filename */
        count = snprintf (basename, sizeof (basename), "%s", viirs_hdf_name);
        if (count < 0 || count >= sizeof (basename))
        {
            sprintf (errmsg, "Overflow of basename string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }

    /* Strip the extension off the basename */
    count = snprintf (core_basename, sizeof (core_basename), "%s", basename);
    if (count < 0 || count >= sizeof (core_basename))
    {
        sprintf (errmsg, "Overflow of core_basename string");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    cptr = strrchr (core_basename, '.');
    if (cptr != NULL)
        *cptr = '\0';

    /* Make sure the basename is uppercase */
    cptr = basename;
    while (*cptr != '\0')
    {
        *cptr = toupper ((unsigned char) *cptr);
        cptr++;
    }

    /* Set the data_provider, satellite, and instrument */
    strcpy (gmeta->data_provider, "USGS/EROS LPDAAC");
    strcpy (gmeta->instrument, "VIIRS");
    strcpy (gmeta->satellite, "National Polar-Orbiting Partnership (NPP)");

    /* Determine the acquisition date for the global metadata */
    if (get_acquisition_date (basename, gmeta) != SUCCESS)
    {  /* error message already printed */
        return (ERROR);
    }

    /* Determine the horizontal and vertical tile for the global metadata */
    if (get_tile (basename, gmeta) != SUCCESS)
    {  /* error message already printed */
        return (ERROR);
    }

    /* Open as HDF5 file for reading */
    file_id = H5Fopen (viirs_hdf_name, H5F_ACC_RDONLY, H5P_DEFAULT);
    if (file_id < 0)
    {
        sprintf (errmsg, "Unable to open %s for reading", viirs_hdf_name);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Confirm the 500m grid exists then open it. Otherwise flag an error if
       the grid is not found. */
    grid500 = "/HDFEOS/GRIDS/VNP_Grid_500m_2D/Data Fields";
    if (H5Lexists (file_id, grid500, H5P_DEFAULT) > 0)
    {
        grid500_id = H5Gopen (file_id, grid500, H5P_DEFAULT);
        if (grid500_id < 0)
        {
            sprintf (errmsg, "Unable to open 500m grid: %s", grid500);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }
    else
    {
        sprintf (errmsg, "Unable to find the 500m grid: %s", grid500);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* We will only parse the three surface reflectance image data fields from
       the 500m grid */
    /* Get information about the group, then loop through the datasets to
       pull out info on the 500m image bands */
    if (H5Gget_info(grid500_id, &grid500_info) >= 0)
    {
        printf ("%d datasets in the 500m grid group\n",
            (int) grid500_info.nlinks);
        nviirs_bands = 0;
        for (i=0; i < (int)grid500_info.nlinks; i++)
        {
            /* Get the name of the dataset */
    	    H5Lget_name_by_idx(grid500_id, ".", H5_INDEX_NAME, H5_ITER_NATIVE,
                (hsize_t)i, dataset_name, STR_SIZE, H5P_DEFAULT);
    	    printf("Object's name is %s\n", dataset_name);

            /* If this dataset is one of the surface reflectance datasets, then
               keep the dataset name as one to be processed */
            if (strstr (dataset_name, "SurfReflect_I"))
            {
                /* Store the band/SDS information */
                count = snprintf (viirs_bands[nviirs_bands],
                    sizeof (viirs_bands[nviirs_bands]), "%s", dataset_name);
                if (count < 0 || count >= sizeof (viirs_bands[nviirs_bands]))
                {
                    sprintf (errmsg, "Overflow of viirs_bands[] string");
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }

                /* Open the dataset */
                dataset_id = H5Dopen (grid500_id, dataset_name, H5F_ACC_RDONLY);
                if (dataset_id < 0)
                {
                    sprintf (errmsg, "Unable to open 500m dataset: %s",
                        dataset_name);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }

                /* Get the datatype */
                dtype_id = H5Dget_type (dataset_id);
                if (dtype_id < 0)
                {
                    sprintf (errmsg, "Unable to get the datatype");
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }

                /* Get the data type class determine the properties of the
                   datatype */
                t_class = H5Tget_class (dtype_id);
                if (t_class < 0)
                {
                    sprintf (errmsg, "Invalid datatype");
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }

                if (t_class == H5T_INTEGER)
                {
                    /* Signed or unsigned (unsigned is 0, signed is 1) */
                    t_sign = H5Tget_sign (dtype_id);
                    if (t_sign < 0)
                    {
                        sprintf (errmsg, "Invalid datatype sign");
                        error_handler (true, FUNC_NAME, errmsg);
                        return (ERROR);
                    }

                    /* How many bytes */
                    t_size = H5Tget_size (dtype_id);
                    if (t_size < 0)
                    {
                        sprintf (errmsg, "Invalid datatype size");
                        error_handler (true, FUNC_NAME, errmsg);
                        return (ERROR);
                    }

                    /* Data is expected to be signed 16-bit */
                    if (t_sign != 1)
                    {
                        sprintf (errmsg, "Data is expected to be signed");
                        error_handler (true, FUNC_NAME, errmsg);
                        return (ERROR);
                    }

                    if (t_size != 2)
                    {
                        sprintf (errmsg, "Data is expected to be 16-bit");
                        error_handler (true, FUNC_NAME, errmsg);
                        return (ERROR);
                    }
                }
                else
                {
                    sprintf (errmsg, "Unexpected datatype for the current "
                        "band: %s.  Integer expected.", dataset_name);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }

                /* Close the datatype */
                status = H5Tclose (dtype_id);
                if (status < 0)
                {
                    sprintf (errmsg, "Terminating access to the datatype");
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }

                /* Get the data space */
                dspace_id = H5Dget_space (dataset_id);
                if (dspace_id < 0)
                {
                    sprintf (errmsg, "Unable to get the data space");
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }

                /* Get the number of dimensions and verify it's 2-D */
                ndims = H5Sget_simple_extent_ndims (dspace_id);
                if (ndims < 0)
                {
                    sprintf (errmsg, "Unable to determine the number of "
                        "dimensions of this dataset, but two dimensions are "
                        "expected");
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
                else if (ndims != 2)
                {
                    sprintf (errmsg, "Dataset is expected to be a 2-D dataset, "
                        "however is has %d dimensions", ndims);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
                data_type[nviirs_bands] = ESPA_INT16;
                printf ("    ndims is %d (2D expected)\n", ndims);

                /* Determine the number of dimensions as the size of each
                   dimension */
                status = H5Sget_simple_extent_dims (dspace_id, dims, NULL);
                if (status < 0)
                {
                    sprintf (errmsg, "Unable to determine the dimensions of "
                        "this dataset, but two dimensions are expected");
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
                printf ("    dataset dimensions are %d x %d\n",
                    (int)dims[0], (int)dims[1]);

                /* Save the dimensions */
                grid_dims[nviirs_bands][0] = (int) dims[0];
                grid_dims[nviirs_bands][1] = (int) dims[1];

                /* Close the data space */
                status = H5Sclose (dspace_id);
                if (status < 0)
                {
                    sprintf (errmsg, "Terminating access to the dataspace");
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }

                /* Read the long_name attribute as a character string */
                if (read_attribute ("long_name", dataset_id,
                    &longname[nviirs_bands]) != SUCCESS)
                {  /* Error message already printed */
                    return (ERROR);
                }
                printf ("    long_name: %s\n", longname[nviirs_bands]);

                /* Some of the attributes in the VIIRS dataset don't describe
                   the values as correctly as needed for the XML metadata.  The
                   attribute information is going to be hardcoded from the
                   LP DAAC table specified in
                   https://lpdaac.usgs.gov/dataset_discovery/viirs/
                   viirs_products_table/vnp09ga_v001 */
                scalevalue[nviirs_bands] = 0.0001;
                offsetvalue[nviirs_bands] = 0.0;
                minvalue[nviirs_bands] = -100;
                maxvalue[nviirs_bands] = 16000;
                fillvalue[nviirs_bands] = -28672;
                strcpy (units[nviirs_bands], "reflectance");
                strcpy (qa_desc[nviirs_bands], "ELLIPSOID_INT16_FILL = -994, "
                    "VDNE_INT16_FILL = -993, SOUB_INT16_FILL = -992, "
                    "OUT_OF_RANGE_FILL = -100");

                /* Close the dataset */
                status = H5Dclose (dataset_id);
                if (status < 0)
                {
                    sprintf (errmsg, "Terminating access to 500m dataset: %s",
                        dataset_name);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }

                /* Increment the band/SDS count */
                nviirs_bands++;
            }  /* if this is a 500m surface reflectance band */
    	}  /* for i in number of bands in the 500m grid */
    }
    else
    {
        sprintf (errmsg, "Unable to retrieve information about the 500m grid: "
            "%s", grid500);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Close the 500m grid */
    status = H5Gclose (grid500_id);
    if (status < 0)
    {
        sprintf (errmsg, "Terminating access to 500m grid: %s", grid500);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Open the global/root grid */
    root_id = H5Gopen (file_id, "/", H5P_DEFAULT);
    if (root_id < 0)
    {
        sprintf (errmsg, "Unable to open root grid");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Read the production date/time attribute as a character string */
    if (read_attribute ("ProductionTime", root_id, prod_date_time) != SUCCESS)
    {  /* Error message already printed */
        return (ERROR);
    }

    /* Fix the production date/time to be in Zulu (going from
       2016-11-20 00:35:44.000 to 2016-11-20T00:35:44.000Z) */
    prod_date_time[10] = 'T';
    strcat (prod_date_time, "Z");

    /* Read the PGE version attribute as a character string */
    if (read_attribute ("PGEVersion", root_id, pge_version) != SUCCESS)
    {  /* Error message already printed */
        return (ERROR);
    }

    /* Read the bounding coords attributes as a double value */
    if (read_attribute ("WestBoundingCoord", root_id, 
        &gmeta->bounding_coords[0]) != SUCCESS)
    {  /* Error message already printed */
        return (ERROR);
    }

    if (read_attribute ("EastBoundingCoord", root_id,
        &gmeta->bounding_coords[1]) != SUCCESS)
    {  /* Error message already printed */
        return (ERROR);
    }

    if (read_attribute ("NorthBoundingCoord", root_id,
        &gmeta->bounding_coords[2]) != SUCCESS)
    {  /* Error message already printed */
        return (ERROR);
    }

    if (read_attribute ("SouthBoundingCoord", root_id,
        &gmeta->bounding_coords[3]) != SUCCESS)
    {  /* Error message already printed */
        return (ERROR);
    }

    /* Close the root grid */
    status = H5Gclose (root_id);
    if (status < 0)
    {
        sprintf (errmsg, "Terminating access to root grid");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Close the HDF5 file */
    status = H5Fclose (file_id);
    if (status < 0)
    {
        sprintf (errmsg, "Terminating access to HDF5 file: %s", viirs_hdf_name);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Allocate bands for the XML structure */
    metadata->nbands = nviirs_bands;
    if (allocate_band_metadata (metadata, metadata->nbands) != SUCCESS)
    {   /* Error messages already printed */
        return (ERROR);
    }
    bmeta = metadata->band;

    /* Loop back through the bands and fill in the band-related metadata */
    for (i = 0; i < nviirs_bands; i++)
    {
        /* Fill in the band information already obtained.  Use 'sr_refl'
           for the product type.  Copy the first 7 characters of the
           basename as the short name.  Use 'image' for the category. */
        strcpy (bmeta[i].product, "sr_refl");
        strncpy (bmeta[i].short_name, basename, 7);
        bmeta[i].short_name[7] = '\0';
        strcpy (bmeta[i].category, "image");
        bmeta[i].nsamps = grid_dims[i][0];
        bmeta[i].nlines = grid_dims[i][1];

        /* Use the SDS name as the band name as well as the file name */
        count = snprintf (bmeta[i].name, sizeof (bmeta[i].name), "%s",
            viirs_bands[i]);
        if (count < 0 || count >= sizeof (bmeta[i].name))
        {
            sprintf (errmsg, "Overflow of bmeta[i].name string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Set up the filename, but replace any blank spaces in the
           filename (due to the SDS names) with underscores */
        count = snprintf (bmeta[i].file_name, sizeof (bmeta[i].file_name),
            "%s.%s.img", core_basename, viirs_bands[i]);
        if (count < 0 || count >= sizeof (bmeta[i].file_name))
        {
            sprintf (errmsg, "Overflow of bmeta[].file_name string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
        cleanup_file_name (bmeta[i].file_name);

        /* Store the ESPA data type */
        bmeta[i].data_type = data_type[i];

        /* Compute the pixel size */
        bmeta[i].pixel_size[1] = (gmeta->proj_info.ul_corner[1] -
            gmeta->proj_info.lr_corner[1]) / bmeta[i].nlines;
        bmeta[i].pixel_size[0] = (gmeta->proj_info.lr_corner[0] -
            gmeta->proj_info.ul_corner[0]) / bmeta[i].nsamps;
        strcpy (bmeta[i].pixel_units, "meters");

        /* Assign the scale, offset, min/max, and fill values.  Fill value
           is required, so assign it as 0 if it doesn't exist. */
        if (fillvalue[i] == ESPA_INT_META_FILL)
            bmeta[i].fill_value = 0;
        else
            bmeta[i].fill_value = fillvalue[i];
        bmeta[i].scale_factor = scalevalue[i];
        bmeta[i].add_offset = offsetvalue[i];
        bmeta[i].valid_range[0] = minvalue[i];
        bmeta[i].valid_range[1] = maxvalue[i];

        /* Set the resample method to nearest neighbor, since it's not
           available in the VIIRS file but it's a known entity. */
        bmeta[i].resample_method = ESPA_NN;

        /* Assign the long_name and data_units values */
        count = snprintf (bmeta[i].long_name, sizeof (bmeta[i].long_name), "%s",
            longname[i]);
        if (count < 0 || count >= sizeof (bmeta[i].long_name))
        {
            sprintf (errmsg, "Overflow of bmeta[].long_name string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        count = snprintf (bmeta[i].data_units, sizeof (bmeta[i].data_units),
            "%s", units[i]);
        if (count < 0 || count >= sizeof (bmeta[i].data_units))
        {
            sprintf (errmsg, "Overflow of bmeta[].data_units string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Get the QA description information */
        count = snprintf (bmeta[i].qa_desc, sizeof (bmeta[i].qa_desc), "%s",
            qa_desc[i]);
        if (count < 0 || count >= sizeof (bmeta[i].qa_desc))
        {
            sprintf (errmsg, "Overflow of bmeta[].qa_desc string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Add the production date/time and PGE version from the core
           metadata */
        count = snprintf (bmeta[i].production_date,
            sizeof (bmeta[i].production_date), "%s", prod_date_time);
        if (count < 0 || count >= sizeof (bmeta[i].production_date))
        {
            sprintf (errmsg, "Overflow of bmeta[].production_date string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        count = snprintf (bmeta[i].app_version, sizeof (bmeta[i].app_version),
            "PGE Version %s", pge_version);
        if (count < 0 || count >= sizeof (bmeta[i].app_version))
        {
            sprintf (errmsg, "Overflow of bmeta[].app_version string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }  /* end for i (loop through the bands) */

    /* Set the orientation angle to 0.0 */
    gmeta->orientation_angle = 0.0;

    /* Get geolocation information from the XML file (using the first band) to
       prepare for computing the bounding coordinates */
    if (!get_geoloc_info (metadata, &geoloc_def))
    {
        sprintf (errmsg, "Copying the geolocation information from the XML "
            "metadata structure.");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Setup the mapping structure */
    geoloc_map = setup_mapping (&geoloc_def);
    if (geoloc_map == NULL)
    {
        sprintf (errmsg, "Setting up the geolocation mapping structure.");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Get the geographic coords for the UL corner */
    img.l = 0.0;
    img.s = 0.0;
    img.is_fill = false;
    if (!from_space (geoloc_map, &img, &geo))
    {
        sprintf (errmsg, "Mapping UL corner to lat/long");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }
    gmeta->ul_corner[0] = geo.lat * DEG;
    gmeta->ul_corner[1] = geo.lon * DEG;

    /* Get the geographic coords for the LR corner */
    img.l = bmeta[0].nlines-1;
    img.s = bmeta[0].nsamps-1;
    img.is_fill = false;
    if (!from_space (geoloc_map, &img, &geo))
    {
        sprintf (errmsg, "Mapping UL corner to lat/long");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }
    gmeta->lr_corner[0] = geo.lat * DEG;
    gmeta->lr_corner[1] = geo.lon * DEG;

    /* Free the geolocation structure */
    free (geoloc_map);

    /* Successful read */
    return (SUCCESS);
}


/******************************************************************************
MODULE:  convert_hdf_to_img

PURPOSE: Convert the VIIRS HDF5 500m image bands to an ESPA raw binary (.img)
file and writes the associated ENVI header for each band.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error converting the VIIRS SDS
SUCCESS         Successfully converted VIIRS SDS to raw binary

NOTES:
******************************************************************************/
int convert_hdf_to_img
(
    char *viirs_hdf_name,      /* I: name of VIIRS file to be processed */
    Espa_internal_meta_t *xml_metadata /* I: metadata structure for HDF file */
)
{
    char FUNC_NAME[] = "convert_hdf_to_img";  /* function name */
    char errmsg[STR_SIZE];    /* error message */
    char dataset_name[STR_SIZE];  /* name of the dataset to read */
    char envi_file[STR_SIZE]; /* name of the output ENVI header file */
    char *cptr = NULL;        /* pointer to the file extension */
    char *img_file = NULL;    /* name of the output raw binary file */
    char *grid500 = NULL;     /* name of the 500m grid group */
    int i;                    /* looping variable for bands in XML file */
    int nbytes;               /* number of bytes in the data type */
    int count;                /* number of chars copied in snprintf */
    hid_t file_id;            /* file ID for the VIIRS file */
    hid_t grid500_id;         /* group ID for the 500m grid */
    hid_t dataset_id;         /* dataset ID in the VIIRS file */
    herr_t status;            /* return status of the HDF function */
    int16_t *file_buf=NULL;   /* 1D array for the image data */
    FILE *fp_rb = NULL;       /* file pointer for the raw binary file */
    Envi_header_t envi_hdr;   /* output ENVI header information */
    Espa_band_meta_t *bmeta = NULL;  /* pointer to band metadata */
    Espa_global_meta_t *gmeta = &xml_metadata->global;  /* global metadata */

    /* Open as HDF5 file for reading */
    file_id = H5Fopen (viirs_hdf_name, H5F_ACC_RDONLY, H5P_DEFAULT);
    if (file_id < 0)
    {
        sprintf (errmsg, "Unable to open %s for reading", viirs_hdf_name);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Confirm the 500m grid exists then open it. Otherwise flag an error if
       the grid is not found. */
    grid500 = "/HDFEOS/GRIDS/VNP_Grid_500m_2D/Data Fields";
    if (H5Lexists (file_id, grid500, H5P_DEFAULT) > 0)
    {
        grid500_id = H5Gopen (file_id, grid500, H5P_DEFAULT);
        if (grid500_id < 0)
        {
            sprintf (errmsg, "Unable to open 500m grid: %s", grid500);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }
    else
    {
        sprintf (errmsg, "Unable to find the 500m grid: %s", grid500);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Loop through the bands in the metadata file and convert each one to
       the ESPA format */
    for (i = 0; i < xml_metadata->nbands; i++)
    {
        /* Set up the band metadata pointer */
        bmeta = &xml_metadata->band[i];

        printf ("Reading band %d: %s\n", i, bmeta->name);
        printf ("    nlines x nsamps: %d x %d\n", bmeta->nlines, bmeta->nsamps);
        /* Open the current band as a dataset in the HDF5 file */
        strcpy (dataset_name, bmeta->name);
        dataset_id = H5Dopen (grid500_id, dataset_name, H5F_ACC_RDONLY);
        if (dataset_id < 0)
        {
            sprintf (errmsg, "Unable to access %s for reading", dataset_name);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Open the raw binary file for writing */
        img_file = bmeta->file_name;
        fp_rb = open_raw_binary (img_file, "wb");
        if (fp_rb == NULL)
        {
            sprintf (errmsg, "Opening the output raw binary file: %s",
                img_file);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Allocate memory for the entire 500m image, which is a signed 16-bit
           integer.  Allocate as a 1D contiguous array. */
        nbytes = sizeof (int16_t);
        printf ("    nbytes: %d\n", nbytes);
        file_buf = calloc (bmeta->nlines * bmeta->nsamps, nbytes);
        if (file_buf == NULL)
        {
            sprintf (errmsg, "Allocating memory for the image data containing "
                "%d lines x %d samples. (1D)", bmeta->nlines, bmeta->nsamps);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Read the entire band */
        status = H5Dread (dataset_id, H5T_NATIVE_INT16, H5S_ALL, H5S_ALL,
            H5P_DEFAULT, file_buf);
        if (status < 0)
        {
            sprintf (errmsg, "Reading data from the SDS: %s", bmeta->name);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Write entire image to the raw binary file */
        if (write_raw_binary (fp_rb, bmeta->nlines, bmeta->nsamps, nbytes,
            (void *) file_buf) != SUCCESS)
        {
            sprintf (errmsg, "Writing image to the raw binary file: %s",
                img_file);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Close the HDF5 dataset and raw binary file */
        close_raw_binary (fp_rb);
        status = H5Dclose (dataset_id);
        if (status < 0)
        {
            sprintf (errmsg, "Terminating access to 500m dataset: %s",
                bmeta->name);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Free the memory */
        free (file_buf);

        /* Create the ENVI header file this band */
        if (create_envi_struct (bmeta, gmeta, &envi_hdr) != SUCCESS)
        {
            sprintf (errmsg, "Creating the ENVI header structure for this "
                "file: %s", bmeta->file_name);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Write the ENVI header */
        count = snprintf (envi_file, sizeof (envi_file), "%s", img_file);
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

    /* Close the HDF5 file */
    status = H5Fclose (file_id);
    if (status < 0)
    {
        sprintf (errmsg, "Terminating access to HDF5 file: %s", viirs_hdf_name);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Successful conversion */
    return (SUCCESS);
}


/******************************************************************************
MODULE:  convert_viirs_to_espa

PURPOSE: Converts the input VIIRS HDF5 file to the ESPA internal raw binary
file format (and associated XML file).

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error converting the VIIRS file
SUCCESS         Successfully converted VIIRS to ESPA format

NOTES:
  1. The ESPA raw binary band files will be generated from the ESPA XML
     filename.
  2. Only the 500m image bands will be ingested at the current time since the
     main focus of supporting this product in ESPA is to generate the 500m
     NDVI to compare with the MODIS imagery.
******************************************************************************/
int convert_viirs_to_espa
(
    char *viirs_hdf_file,  /* I: input VIIRS HDF5 filename */
    char *espa_xml_file,   /* I: output ESPA XML metadata filename */
    bool del_src           /* I: should the source .tif files be removed after
                                 conversion? */
)
{
    char FUNC_NAME[] = "convert_viirs_to_espa";  /* function name */
    char errmsg[STR_SIZE];   /* error message */
    char *cptr = NULL;       /* pointer to .h5 extention in the filename */
    int count;               /* number of chars copied in snprintf */
    Espa_internal_meta_t xml_metadata;  /* XML metadata structure to be
                                populated by reading the MTL metadata file */

    /* Initialize the metadata structure */
    init_metadata_struct (&xml_metadata);

    /* Read the geolocation information from the VIIRS product for the 500m
       grid */
    if (read_viirs_500m_geo_meta (viirs_hdf_file, &xml_metadata) != SUCCESS)
    {
        sprintf (errmsg, "Reading the VIIRS HDF-EOS file: %s", viirs_hdf_file);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Read the VIIRS 500m grid metadata and populate our internal ESPA
       metadata structure for the 500m bands, including the global geolocation
       data */
    if (read_viirs_500m_grid_meta (viirs_hdf_file, &xml_metadata) != SUCCESS)
    {
        sprintf (errmsg, "Reading the VIIRS HDF file: %s", viirs_hdf_file);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Add the product ID which is pulled from the VIIRS HDF filename
       ({product_id}.h5) */
    count = snprintf (xml_metadata.global.product_id,
        sizeof (xml_metadata.global.product_id), "%s", viirs_hdf_file);
    if (count < 0 || count >= sizeof (xml_metadata.global.product_id))
    {
        sprintf (errmsg, "Overflow of xml_metadata.global.product_id string");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Strip off .h5 filename extension to get the actual product name */
    cptr = strrchr (xml_metadata.global.product_id, '.');
    *cptr = '\0';

    /* Write the metadata from our internal metadata structure to the output
       XML filename */
    if (write_metadata (&xml_metadata, espa_xml_file) != SUCCESS)
    {  /* Error messages already written */
        return (ERROR);
    }

    /* Validate the output metadata file */
    if (validate_xml_file (espa_xml_file) != SUCCESS)
    {  /* Error messages already written */
        return (ERROR);
    }

    /* Convert each of the VIIRS 500m image bands to raw binary */
    if (convert_hdf_to_img (viirs_hdf_file, &xml_metadata) != SUCCESS)
    {
        sprintf (errmsg, "Converting %s to ESPA", viirs_hdf_file);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Remove the source file if specified */
    if (del_src)
    {
        printf ("  Removing %s\n", viirs_hdf_file);
        if (unlink (viirs_hdf_file) != 0)
        {
            sprintf (errmsg, "Deleting source file: %s", viirs_hdf_file);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }

    /* Free the metadata structure */
    free_metadata (&xml_metadata);

    /* Successful conversion */
    printf ("Successful ingest of VIIRS product\n");
    return (SUCCESS);
}

