#! /usr/bin/env python

"""
License:
    NASA Open Source Agreement 1.3

Usage:
    create_dem.py --help prints the help message
"""

import os
import sys
import commands
import logging
import glob
from cStringIO import StringIO
from argparse import ArgumentParser


import h5py
import numpy as np
from lxml import objectify as objectify
from osgeo import gdal, osr


from espa_xml_interface import XMLError, XMLInterface
from espa_metadata_api import ESPAMetadataError, ESPAMetadata


ESPA_DEM_DIR = 'ESPA_DEM_DIR'

# Lat/lon coordinate limits
NORTH_LATITUDE_LIMIT = 90.0
SOUTH_LATITUDE_LIMIT = -90.0
EAST_LONGITUDE_LIMIT = 180.0
WEST_LONGITUDE_LIMIT = -180.0

# RAMP and GLSDEM threshold lat/lon coordinates
GLSDEM_NORTH_LATITUDE = 83.0
GLSDEM_SOUTH_LATITUDE = -53.0
RAMP_SOUTH_LATITUDE = -60.0

# RAMP name, location and EPSG code
RAMP_NAME = 'RAMP_200_DEM.h5'
RAMP_DIR = 'ramp'
RAMP_EPSG = 3031
RAMP_PROJ4 = ('+proj=stere +lat_0=-90 +lat_ts=-71 +lon_0=0'
              ' +k=1 +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs')

# GLS location
GLS_DIR = 'gls'

# GTOPT30 location
GTOPO30_DIR = 'gtopo30'
GTOPO30_DEMS = '[EW]???[NS]??.DEM'
GTOPO30_FILES = '[EW]???[NS]??.*'
GTOPO30_PADDING = 1.0  # Degrees, since we are in geographic

# DEM format and naming
#DEM_FORMAT = 'GTiff'
DEM_FORMAT = 'ENVI'
DEM_TYPE = 'Int16'
DEM_HEADER_FILENAME = '{0}_dem.hdr'
DEM_IMAGE_FILENAME = '{0}_dem.img'

# GDAL AUX files to remove
GDAL_AUX_REGEXP = '*.img.aux.xml'

NO_DATA_VALUE = -9999

# Padding to add to the max box
MAXBOX_PADDING = 0.2

# -- ENVI format projection values -- #
ENVI_GEO_PROJ = 1
ENVI_UTM_PROJ = 2
ENVI_PS_PROJ = 31


class Geo(object):
    """Provides methods for interfacing with geographic projections"""

    @staticmethod
    def convert_imageXY_to_mapXY(image_x, image_y, transform):
        """Translate image coordinates into map coordinates"""

        map_x = (transform[0] +
                 image_x * transform[1] +
                 image_y * transform[2])
        map_y = (transform[3] +
                 image_x * transform[4] +
                 image_y * transform[5])

        return (map_x, map_y)

    @staticmethod
    def convert_mapXY_to_imageXY(map_x, map_y, transform):
        """Translate map coordinates into image coordinates"""

        # Convert the transform from image->map to map->image
        (success, inv_transform) = gdal.InvGeoTransform(transform)

        image_x = (inv_transform[0] +
                   map_x * inv_transform[1] +
                   map_y * inv_transform[2])
        image_y = (inv_transform[3] +
                   map_x * inv_transform[4] +
                   map_y * inv_transform[5])

        return (image_x, image_y)

    @staticmethod
    def get_proj4_projection_string(img_filename):
        """Determine the proj4 projection parameters for the specified image

        Proj4 projection parameters are extracted from the specified image.

        Args:
            img_filename (str): The image to extract the projection
                                information from.

        Returns:
            proj4 (str): The proj4 projection string for the image.
        """

        data_set = gdal.Open(img_filename)
        if data_set is None:
            raise RuntimeError('GDAL failed to open ({0})'
                               .format(img_filename))

        ds_srs = osr.SpatialReference()
        ds_srs.ImportFromWkt(data_set.GetProjection())

        proj4 = ds_srs.ExportToProj4()

        del ds_srs
        del data_set

        return proj4

    @staticmethod
    def generate_raster_file(driver, filename, data, x_dim, y_dim,
                             geo_transform, proj_wkt,
                             no_data_value, data_type):
        """Creates a raster file on disk for the data

        Creates the raster using the specified driver.

        Notes:
            It is assumed that the driver supports setting of the no data
            value.  It is the callers responsibility to fix it if it does not.

            It is assumed that the caller specified the correct file
            extension in the filename parameter for the specfied driver.
        """

        try:
            raster = driver.Create(filename, x_dim, y_dim, 1, data_type)

            raster.SetGeoTransform(geo_transform)
            raster.SetProjection(proj_wkt)
            raster.GetRasterBand(1).WriteArray(data)
            raster.GetRasterBand(1).SetNoDataValue(no_data_value)
            raster.FlushCache()

            # Cleanup memory
            del raster

        except Exception:
            raise

    @staticmethod
    def update_envi_header(hdr_file_path, no_data_value=None):
        """Updates the specified ENVI header

        Especially the no data value, since it is not supported by the
        GDAL ENVI driver.
        """

        def find_ending_bracket(fd):
            """Method to find the ending bracket for an ENVI element"""
            while True:
                next_line = fd.readline()
                if (not next_line or
                        next_line.strip().endswith('}')):
                    break

        hdr_text = StringIO()
        with open(hdr_file_path, 'r') as tmp_fd:
            while True:
                line = tmp_fd.readline()
                if not line:
                    break

                if line.startswith('description'):
                    # These may be on multiple lines so read lines until
                    # we find the closing brace
                    if not line.strip().endswith('}'):
                        find_ending_bracket(tmp_fd)
                    hdr_text.write('description ='
                                   ' {USGS-EROS-ESPA generated}\n')
                elif line.startswith('band names'):
                    # These may be on multiple lines so read lines until
                    # we find the closing brace
                    if not line.strip().endswith('}'):
                        find_ending_bracket(tmp_fd)
                    hdr_text.write('band names ='
                                   ' {band 1 - DEM values}\n')
                elif (line.startswith('data type') and
                      (no_data_value is not None)):
                    hdr_text.write(line)
                    hdr_text.write('data ignore value ='
                                   ' {0}\n'.format(no_data_value))
                else:
                    hdr_text.write(line)

        # Do the actual replace here
        with open(hdr_file_path, 'w') as tmp_fd:
            tmp_fd.write(hdr_text.getvalue())


class MathError(Exception):
    """Exception to capture errors from the Math class"""
    pass


class Math(object):
    """Provides methods for mathematical algorithms"""

    @staticmethod
    def point_in_polygon(vertices, x_c, y_c):
        """Determines if a given point is within a closed polygon

        Algorithm:
            Based on W. Randolph Franklin's algorithm:
                http://www.ecse.rpi.edu/Homepages/wrf/Research/Short_Notes/pnpoly.html
            See also the Wikipedia page:
                http://eni.wikipedia.org/wiki/Point_in_polygon

        Notes:
            Derived from Landsat IAS code, which is based on above algorithm.
            A closed list is expected.  Where v[N] = v[0]
            The polygon is always expected to be clock-wise segments.
        """

        count = len(vertices)
        if count < 4:
            raise MathError('Insufficient Line Segments')

        # Convert to x and y lists
        (x_v, y_v) = map(list, zip(*vertices))

        if x_v[0] != x_v[count - 1] and y_v[0] != y_v[count - 1]:
            raise MathError('Not A Closed Polygon Vertex List')

        # Start off by saying outside the polygon
        inside_polygon = False

        # Test each segment
        for index in range(count - 1):
            if (((x_v[index] > x_c) != (x_v[index + 1] > x_c)) and
                (y_c < ((y_v[index + 1] - y_v[index]) *
                        (x_c - x_v[index]) /
                        (x_v[index + 1] - x_v[index]) +
                        y_v[index]))):

                inside_polygon = not inside_polygon

        return inside_polygon


def execute_cmd(cmd):
    """Execute a command line

    The specified command line is executed and the terminal output is returned
    or an exception is reaised.

    Returns:
        output (str): The stdout and/or stderr from the executed command.
    """

    output = ''

    (status, output) = commands.getstatusoutput(cmd)

    if status < 0:
        message = 'Application terminated by signal [{0}]'.format(cmd)
        if len(output) > 0:
            message = ' Stdout/Stderr is: '.join([message, output])
        raise Exception(message)

    if status != 0:
        message = 'Application failed to execute [{0}]'.format(cmd)
        if len(output) > 0:
            message = ' Stdout/Stderr is: '.join([message, output])
        raise Exception(message)

    if os.WEXITSTATUS(status) != 0:
        message = ('Application [{0}] returned error code [{1}]'
                   .format(cmd, os.WEXITSTATUS(status)))
        if len(output) > 0:
            message = ' Stdout/Stderr is: '.join([message, output])
        raise Exception(message)

    return output


def longitude_norm(longitude):
    """Calculates the "canonical longitude" for the specified longitude value.

    Returns:
        result (float): Equivalent longitude value in the range
                        [-180.0, 180.0) degrees
    """

    result = longitude

    while result < -180.0:
        result += 360.0

    while result >= 180.0:
        result -= 360.0

    return result


class RAMPCoverageError(Exception):
    """Exception to capture RAMP data not covering the input data"""
    pass


class GLSOverWaterError(Exception):
    """Exception to capture when GLS data is over water"""
    pass


class Base_DEM(object):
    """Defines the base class object for DEM generation/processing"""

    def __init__(self):
        """Class initialization"""
        super(Base_DEM, self).__init__()

        bounding_north_latitude = -9999.0
        bounding_south_latitude = 9999.0
        bounding_east_longitude = -9999.0
        bounding_west_longitude = 9999.0

        min_x_extent = -500001.0
        max_y_extent = -10000001.0
        max_x_extent = -500001.0
        min_y_extent = -10000001.0

        pixel_resolution_x = -9999.0
        pixel_resolution_y = -9999.0

        wrs_path = -9999.0
        wrs_row = -9999.0

        product_id = None

        map_projection = None
        utm_zone = None
        longitude_pole = None
        latitude_true_scale = None
        origin_latitude = None
        central_meridian = None
        standard_parallel_1 = None
        standard_parallel_2 = None
        false_easting = None
        false_northing = None

        target_srs = None

        if ESPA_DEM_DIR not in os.environ:
            raise RuntimeError('{0} environement variable not defined'
                               .format(ESPA_DEM_DIR))
        self.espa_dem_dir = os.environ.get(ESPA_DEM_DIR)

        dem_header_filename = None
        dem_image_filename = None
        dem_image_geotiff_filename = None

        # Register all the gdal drivers and choose the GeoTiff for our
        # temp output
        gdal.AllRegister()
        self.envi_driver = gdal.GetDriverByName('ENVI')
        self.geotiff_driver = gdal.GetDriverByName('GTiff')
        self.dem_buffered_name = 'espa_buffered_dem.tif'

    def parse_metadata(self):
        """Implement this to parse the metadata file"""
        raise NotImplementedError('Please Implement Me In {0}'
                                  .format(str(type(self))))

    def warp_to_final_dem(self, tiles):
        """Warp the tiles into the final DEM"""

        logger = logging.getLogger(__name__)

        # TODO TODO TODO
        # TODO TODO TODO
        # Should we be using bi-linear?????????????????
        # TODO TODO TODO
        # TODO TODO TODO
        cmd = ['gdalwarp', '-wm', '2048', '-multi',
               '-tr', str(self.pixel_resolution_x),
               str(self.pixel_resolution_y),
               # '-s_srs', ''.join(["'", something , "'"]),
               '-t_srs', ''.join(['"', self.target_srs, '"']),
               '-of', DEM_FORMAT,
               '-wt', DEM_TYPE,
               '-overwrite',
               '-te',
               str(self.min_x_extent), str(self.min_y_extent),
               str(self.max_x_extent), str(self.max_y_extent),
               '-srcnodata', str(NO_DATA_VALUE),
               '-dstnodata', str(NO_DATA_VALUE)]
        cmd.extend(tiles)
        cmd.append(self.dem_image_filename)

        cmd = ' '.join(cmd)
        output = ''
        try:
            logger.info('EXECUTING WARP COMMAND [{0}]'.format(cmd))
            output = execute_cmd(cmd)
        finally:
            if len(output) > 0:
                print(output)

    def _get_ramp_transform(self):

        logger = logging.getLogger(__name__)

        # Open the RAMP DEM
        ramp_h5fd = h5py.File(RAMP_NAME, 'r')
        logger.debug('Band Metadata: {0}'
                     .format(ramp_h5fd['Band Metadata'][0]))

        # Band Number, Band Name,
        # Upper Left Y, Upper Left X,
        # Upper Right Y, Upper Right X,
        # Lower Left Y, Lower Left X
        # Lower Right Y, Lower Right X,
        # Projection Distance X, Projection Distance Y
        # Maximum Pixel Value
        # Minimum Pixel Value
        # Pixel Range Valid
        # Maximum Radiance
        # Minimum Radiance
        # Spectral Radiance Scaling Offset
        # Spectral Radiance Scaling Gain
        # Radiance Valid
        # Reflectance Scaling Offset
        # Reflectance Scaling Gain
        # Reflectance Valid
        # Instrument Source
        ramp_UL_X = ramp_h5fd['Band Metadata'][0][3]
        ramp_UL_Y = ramp_h5fd['Band Metadata'][0][2]
        ramp_resolution_x = ramp_h5fd['Band Metadata'][0][10]
        ramp_resolution_y = ramp_h5fd['Band Metadata'][0][11]

        # Create the transform array
        ramp_transform = [ramp_UL_X, ramp_resolution_x, 0.0,
                          ramp_UL_Y, 0.0, -ramp_resolution_y]
        logger.debug('Ramp Transform: {0}'.format(str(ramp_transform)))

        # No longer needed
        del ramp_h5fd
        ramp_h5fd = None

        return ramp_transform

    def _verify_ramp_overlap(self,
                             ramp_lines,
                             ramp_samples,
                             ramp_transform,
                             map_ul_x, map_ul_y,
                             map_ur_x, map_ur_y,
                             map_lr_x, map_lr_y,
                             map_ll_x, map_ll_y,
                             map_center_x, map_center_y):
        """Verify that the input data is covered by the RAMP DEM"""

        logger = logging.getLogger(__name__)

        # Setup the X and Y polygons values
        vertices = list()

        # UL (X, Y)
        (ramp_x, ramp_y) = Geo.convert_imageXY_to_mapXY(0.0, 0.0,
                                                        ramp_transform)
        vertices.append([ramp_x, ramp_y])

        # UR (X, Y)
        (ramp_x, ramp_y) = Geo.convert_imageXY_to_mapXY(ramp_samples-1, 0.0,
                                                        ramp_transform)
        vertices.append([ramp_x, ramp_y])

        # LR (X, Y)
        (ramp_x, ramp_y) = Geo.convert_imageXY_to_mapXY(ramp_samples-1,
                                                        ramp_lines-1,
                                                        ramp_transform)
        vertices.append([ramp_x, ramp_y])

        # LL (X, Y)
        (ramp_x, ramp_y) = Geo.convert_imageXY_to_mapXY(0.0, ramp_lines-1,
                                                        ramp_transform)
        vertices.append([ramp_x, ramp_y])

        # Close the polygon
        vertices.append(vertices[0])

        logger.debug('Vertices: {0}'.format(str(vertices)))

        # No longer needed
        del ramp_transform
        ramp_transform = None

        # Determine how many points are within the RAMP DEM
        # and error accordingly
        points_in_polygon = 0
        if Math.point_in_polygon(vertices, map_ul_x, map_ul_y):
            points_in_polygon += 1

        if Math.point_in_polygon(vertices, map_ur_x, map_ur_y):
            points_in_polygon += 1

        if Math.point_in_polygon(vertices, map_lr_x, map_lr_y):
            points_in_polygon += 1

        if Math.point_in_polygon(vertices, map_ll_x, map_ll_y):
            points_in_polygon += 1

        if Math.point_in_polygon(vertices, map_center_x, map_center_y):
            points_in_polygon += 1

        if points_in_polygon < 3:
            raise RAMPCoverageError('Insufficient overlap:'
                                    ' Only found {0} points within'
                                    ' data when at least 3 are'
                                    ' required'.format(points_in_polygon))

    def generate_using_ramp(self):
        """Retrieve the RAMP DEM data"""

        logger = logging.getLogger(__name__)

        ramp_filename = os.path.join(self.espa_dem_dir, RAMP_DIR, RAMP_NAME)

        if not os.path.exists(RAMP_NAME):
            os.symlink(ramp_filename, RAMP_NAME)

        # Get the dataset
        ramp_ds = gdal.Open(RAMP_NAME)

        # SRS for the RAMP data
        ramp_srs = osr.SpatialReference()
        ramp_srs.ImportFromEPSG(RAMP_EPSG)
        ramp_srs.ImportFromProj4(RAMP_PROJ4)

        # SRS for Geographic
        latlon_srs = ramp_srs.CloneGeogCS()

        # Create the coordinate transformation
        ll_to_ramp = osr.CoordinateTransformation(latlon_srs, ramp_srs)

        # Find the center of the max box
        center_latitude = ((self.bounding_north_latitude +
                            self.bounding_south_latitude) / 2.0)
        center_longitude = ((self.bounding_east_longitude +
                             self.bounding_west_longitude) / 2.0)

        # Find the max box and center in map coordinates
        (test_x, test_y, height) = ll_to_ramp.TransformPoint(72.414150,
                                                             -76.624820)
        logger.debug('test_x, test_y: {0}, {1}'.format(test_x, test_y))
        (test_x, test_y, height) = ll_to_ramp.TransformPoint(-80.944270,
                                                             -79.537600)
        logger.debug('test_x, test_y: {0}, {1}'.format(test_x, test_y))

        (map_ul_x, map_ul_y, height) = (
            ll_to_ramp.TransformPoint(self.bounding_west_longitude,
                                      self.bounding_north_latitude))
        (map_ur_x, map_ur_y, height) = (
            ll_to_ramp.TransformPoint(self.bounding_east_longitude,
                                      self.bounding_north_latitude))
        (map_lr_x, map_lr_y, height) = (
            ll_to_ramp.TransformPoint(self.bounding_east_longitude,
                                      self.bounding_south_latitude))
        (map_ll_x, map_ll_y, height) = (
            ll_to_ramp.TransformPoint(self.bounding_west_longitude,
                                      self.bounding_south_latitude))
        (map_center_x, map_center_y, height) = (
            ll_to_ramp.TransformPoint(center_longitude, center_latitude))
        logger.debug('map_ul_x, map_ul_y: {0}, {1}'
                     .format(map_ul_x, map_ul_y))
        logger.debug('map_ur_x, map_ur_y: {0}, {1}'
                     .format(map_ur_x, map_ur_y))
        logger.debug('map_lr_x, map_lr_y: {0}, {1}'
                     .format(map_lr_x, map_lr_y))
        logger.debug('map_ll_x, map_ll_y: {0}, {1}'
                     .format(map_ll_x, map_ll_y))
        logger.debug('map_center_x, map_center_y: {0}, {1}'
                     .format(map_center_x, map_center_y))

        # Get the ramp transform
        ramp_transform = self._get_ramp_transform()

        # Get the lines and samples from the RAMP band data
        ramp_band = ramp_ds.GetRasterBand(1)
        ramp_lines = ramp_band.YSize
        ramp_samples = ramp_band.XSize
        logger.debug('Lines, Samples: {0}, {1}'
                     .format(ramp_lines, ramp_samples))

        # This causes an exception if they do not overlap
        self._verify_ramp_overlap(ramp_lines, ramp_samples, ramp_transform,
                                  map_ul_x, map_ul_y,
                                  map_ur_x, map_ur_y,
                                  map_lr_x, map_lr_y,
                                  map_ll_x, map_ll_y,
                                  map_center_x, map_center_y)

        # Determine image UL and LR coordinates
        (ul_x, ul_y) = Geo.convert_mapXY_to_imageXY(map_ul_x, map_ul_y,
                                                    ramp_transform)
        (ur_x, ur_y) = Geo.convert_mapXY_to_imageXY(map_ur_x, map_ur_y,
                                                    ramp_transform)
        (lr_x, lr_y) = Geo.convert_mapXY_to_imageXY(map_lr_x, map_lr_y,
                                                    ramp_transform)
        (ll_x, ll_y) = Geo.convert_mapXY_to_imageXY(map_ll_x, map_ll_y,
                                                    ramp_transform)

        # Convert to integers
        ul_x = int(round(ul_x))
        ul_y = int(round(ul_y))
        ur_x = int(round(ur_x))
        ur_y = int(round(ur_y))
        lr_x = int(round(lr_x))
        lr_y = int(round(lr_y))
        ll_x = int(round(ll_x))
        ll_y = int(round(ll_y))
        logger.info('ul_x, ul_y: {0}, {1}'.format(ul_x, ul_y))
        logger.info('ur_x, ur_y: {0}, {1}'.format(ur_x, ur_y))
        logger.info('lr_x, lr_y: {0}, {1}'.format(lr_x, lr_y))
        logger.info('ll_x, ll_y: {0}, {1}'.format(ll_x, ll_y))

        # Determine min and max x and y values for data extraction
        min_x = min(ul_x, ur_x, lr_x, ll_x)
        max_x = max(ul_x, ur_x, lr_x, ll_x)
        min_y = min(ul_y, ur_y, lr_y, ll_y)
        max_y = max(ul_y, ur_y, lr_y, ll_y)
        logger.info('min_x, max_x: {0}, {1}'.format(min_x, max_x))
        logger.info('min_y, max_y: {0}, {1}'.format(min_y, max_y))

        # No longer needed
        del ll_to_ramp
        ll_to_ramp = None
        del latlon_srs
        latlon_srs = None

        # Open the RAMP DEM and get the band data
        dem_data = ramp_band.ReadAsArray(min_x, min_y,
                                         max_x - min_x + 1,
                                         max_y - min_y + 1)

        # Create the DEM transform
        dem_transform = ramp_transform
        dem_transform[0] = min(map_ul_x, map_ur_x, map_lr_x, map_ll_x)
        dem_transform[3] = max(map_ul_y, map_ur_y, map_lr_y, map_ll_y)
        logger.debug('DEM Transform: {0}'.format(str(ramp_transform)))

        # Write the new temporary file out probably as geotiff
        Geo.generate_raster_file(self.geotiff_driver, self.dem_buffered_name,
                                 dem_data,
                                 max_x - min_x + 1,
                                 max_y - min_y + 1,
                                 dem_transform, ramp_srs.ExportToWkt(),
                                 NO_DATA_VALUE, gdal.GDT_Int16)

        # No longer needed
        del ramp_transform
        ramp_transform = None
        del ramp_band
        ramp_band = None
        del ramp_srs
        ramp_srs = None
        del ramp_ds
        ramp_ds = None

        # Warp to the final DEM
        self.warp_to_final_dem([self.dem_buffered_name])

        # Remove the symlink to the RAMP DEM
        os.unlink(RAMP_NAME)
        # Remove the temp dem
        os.unlink(self.dem_buffered_name)

    def get_gtopo30_tile_list(self):
        """Generate the list of GTOPO30 DEM tiles"""

        logger = logging.getLogger(__name__)

        # GTOPO30 tile information
        NORTH_LON_LOCATIONS = np.array([-180.0, -140.0, -100.0, -60.0, -20.0,
                                        20.0, 60.0, 100.0, 140.0])

        SOUTH_LON_LOCATIONS = np.array([-180.0, -120.0, -60.0,
                                        0.0, 60.0, 120.0])

        LAT_LOCATIONS = np.array([90.0, 40.0, -10.0, -60.0])

        GTOPO30_TILE_SET_CUTOFF_LATITUDE = -60.0

        '''
        The tiles do not necessarily go all the way to the boundary.  For
        example, the w100n90 tile only goes to -99.995833333334 longitude.
        Thus, if our UL longitude falls between -99.995833333334 and -100.0
        then we will need to use w140n90 to get the far left edge.  To
        account for this we'll artificially pad the East and West boundaries
        by 1 degree.

        Note: this does not seem to cause problems on the North and South
        borders, so we will no worry about them.
        '''
        ul_lon = longitude_norm(self.bounding_west_longitude - GTOPO30_PADDING)
        lr_lon = longitude_norm(self.bounding_east_longitude + GTOPO30_PADDING)
        logger.debug('ul_lon = {0}'.format(ul_lon))
        logger.debug('lr_lon = {0}'.format(lr_lon))

        '''
        There is a condition which could cause problems on the North and South
        boundaries as well.  This is if a scene comes very near the edge, but
        does not quite get there.  It could be that adding 1.5 to nrows before
        taking the ceiling (see calculation of nrows in get_gtopo30_data.c)
        causes the nrows to be large enough that we need data from the tile to
        the South, even though the scene stopped just before that tile.  To
        address this we will grow North and South by 1 degree as we did in the
        East and West directions above.  Note, the same problem can occur in
        the East and West directions, but was already addressed when adding
        one to the longitudes above.
        '''
        ul_lat = min((self.bounding_north_latitude + GTOPO30_PADDING),
                     NORTH_LATITUDE_LIMIT)
        lr_lat = max((self.bounding_south_latitude - GTOPO30_PADDING),
                     SOUTH_LATITUDE_LIMIT)
        logger.debug('ul_lat = {0}'.format(ul_lat))
        logger.debug('lr_lat = {0}'.format(lr_lat))

        # Determine the unique latitudes where the input data resides
        lat_list = list(set([min(LAT_LOCATIONS[
                                 np.where(ul_lat < LAT_LOCATIONS)]),
                             min(LAT_LOCATIONS[
                                 np.where(lr_lat < LAT_LOCATIONS)])]))
        logger.debug('lat_list = {0}'.format(lat_list))

        tile_list = list()
        # Determine the unique longitudes where the input data resides
        for lat in lat_list:
            lon_list = list()
            if lat > GTOPO30_TILE_SET_CUTOFF_LATITUDE:
                lon_list.append(
                    max(NORTH_LON_LOCATIONS[
                        np.where(ul_lon > NORTH_LON_LOCATIONS)]))
                lon_list.append(
                    max(NORTH_LON_LOCATIONS[
                        np.where(lr_lon > NORTH_LON_LOCATIONS)]))
            else:
                lon_list.append(
                    max(SOUTH_LON_LOCATIONS[
                        np.where(ul_lon > SOUTH_LON_LOCATIONS)]))
                lon_list.append(
                    max(SOUTH_LON_LOCATIONS[
                        np.where(lr_lon > SOUTH_LON_LOCATIONS)]))
            lon_list = list(set(lon_list))
            logger.debug('lon_list = {0}'.format(lon_list))

            n_s = 'n'
            if lat < 0:
                lat = -lat
                n_s = 's'

            for lon in lon_list:
                e_w = 'e'
                if lon <= 0:
                    lon = -lon
                    e_w = 'w'

                tile_list.append('{0}{1:03}{2}{3:02}'
                                 .format(e_w, int(lon), n_s, int(lat)))

        return tile_list

    def generate_using_gtopo30(self):
        """Generate the DEM using GTOPO30 data"""

        logger = logging.getLogger(__name__)

        dem_dir = os.path.join(self.espa_dem_dir, GTOPO30_DIR)

        tile_list = self.get_gtopo30_tile_list()
        print('GTOPO30 Tile Names: {0}'.format(', '.join(tile_list)))

        for tile in tile_list:
            tile_arch = '{0}.tar.gz'.format(tile)
            tile_path = os.path.join(dem_dir, tile_arch)

            output = ''
            try:
                cmd = 'cp {0} .'.format(tile_path)
                output = execute_cmd(cmd)
            finally:
                if len(output) > 0:
                    print(output)

            output = ''
            try:
                cmd = 'tar -xvf {0}'.format(tile_arch)
                output = execute_cmd(cmd)
            finally:
                if len(output) > 0:
                    print(output)

            os.unlink(tile_arch)

        # Grab the tile filenames from the extracted archives
        tile_dem_list = glob.glob(GTOPO30_DEMS)
        logger.info('GTOPO30 DEM Files: {0}'.format(', '.join(tile_dem_list)))

        # Warp to the final DEM
        self.warp_to_final_dem(tile_dem_list)

        # Cleanup intermediate GTOPO30 data
        remove_list = glob.glob(GTOPO30_FILES)
        for file_name in remove_list:
            os.unlink(file_name)

    def get_gls_data(self):
        """Retrieve the GLS DEM data"""

        dem_dir = os.path.join(self.espa_dem_dir, GLS_DIR)

        # TODO TODO TODO
        # TODO TODO TODO
        # TODO TODO TODO
        # TODO TODO TODO
        raise GLSOverWaterError('GLS DEM is over water')

    def generate(self):
        """Generates the DEM"""

        self.parse_metadata()

        logger = logging.getLogger(__name__)

        logger.debug('bounding_north_latitude = {0}'
                     .format(self.bounding_north_latitude))
        logger.debug('bounding_south_latitude = {0}'
                     .format(self.bounding_south_latitude))
        logger.debug('bounding_east_longitude = {0}'
                     .format(self.bounding_east_longitude))
        logger.debug('bounding_west_longitude = {0}'
                     .format(self.bounding_west_longitude))

        logger.debug('min_x_extent = {0}'.format(self.min_x_extent))
        logger.debug('max_y_extent = {0}'.format(self.max_y_extent))
        logger.debug('max_x_extent = {0}'.format(self.max_x_extent))
        logger.debug('min_y_extent = {0}'.format(self.min_y_extent))

        logger.debug('pixel_resolution_x = {0}'
                     .format(self.pixel_resolution_x))
        logger.debug('pixel_resolution_y = {0}'
                     .format(self.pixel_resolution_y))

        logger.debug('wrs_path = {0}'.format(self.wrs_path))
        logger.debug('wrs_row = {0}'.format(self.wrs_row))

        logger.debug('product_id = {0}'.format(self.product_id))

        logger.debug('map_projection = {0}'.format(self.map_projection))

        if self.map_projection == 'UTM':
            logger.debug('utm_zone = {0}'.format(self.utm_zone))

        if self.map_projection == 'PS':
            logger.debug('longitude_pole = {0}'.format(self.longitude_pole))
            logger.debug('latitude_true_scale = {0}'
                         .format(self.latitude_true_scale))

        if self.map_projection == 'ALBERS':
            logger.debug('origin_latitude = {0}'.format(self.origin_latitude))
            logger.debug('central_meridian = {0}'
                         .format(self.central_meridian))
            logger.debug('standard_parallel_1 = {0}'
                         .format(self.standard_parallel_1))
            logger.debug('standard_parallel_2 = {0}'
                         .format(self.standard_parallel_2))

        if self.map_projection in ['PS', 'ALBERS']:
            logger.debug('false_easting = {0}'.format(self.false_easting))
            logger.debug('false_northing = {0}'.format(self.false_northing))

        logger.debug('espa_dem_dir = {0}'.format(self.espa_dem_dir))

        # Pad the max box coordinate values
        self.bounding_north_latitude += MAXBOX_PADDING
        self.bounding_south_latitude -= MAXBOX_PADDING
        self.bounding_east_longitude += MAXBOX_PADDING
        self.bounding_west_longitude -= MAXBOX_PADDING

        # Retrieve the tiles, mosaic, and warp to the source data
        if self.bounding_north_latitude <= RAMP_SOUTH_LATITUDE:
            try:
                logger.info('Attempting to use RAMP DEM')
                self.generate_using_ramp()
            except RAMPCoverageError:
                logger.exception('RAMP DEM failed to cover input data'
                                 ' defaulting to GTOPO30')
                self.generate_using_gtopo30()

        elif ((self.bounding_north_latitude <= GLSDEM_SOUTH_LATITUDE and
               self.bounding_north_latitude > RAMP_SOUTH_LATITUDE and
               self.bounding_south_latitude <= GLSDEM_SOUTH_LATITUDE and
               self.bounding_south_latitude > RAMP_SOUTH_LATITUDE) or
              (self.bounding_north_latitude >= GLSDEM_NORTH_LATITUDE and
               self.bounding_south_latitude >= GLSDEM_NORTH_LATITUDE)):

            logger.info('Using GTOPO30 DEM')
            self.generate_using_gtopo30()

        else:
            try:
                logger.info('Attempting to use GLS DEM')
                self.get_gls_data()
            except GLSOverWaterError:
                logger.exception('GLS DEM over water defaulting to GTOPO30')
                self.generate_using_gtopo30()

        # TODO TODO TODO
        # if gtopo30 or GLS
        #     find_minmax_elevation ??????? why
        #     write the elevation to disk

        # TODO TODO TODO - Warp the temp_dem.tif to the final DEM
        # TODO TODO TODO
        # TODO TODO TODO
        # TODO TODO TODO

        # Cleanup the GDAL generated auxiliary files
        remove_list = glob.glob(GDAL_AUX_REGEXP)
        for file_name in remove_list:
            os.unlink(file_name)

        # Update the ENVI header because of GDAL
        Geo.update_envi_header(self.dem_header_filename, NO_DATA_VALUE)


class XML_DEM(Base_DEM):
    """Defines the class object for XML based DEM generation/processing"""

    def __init__(self, xml_filename):
        """Class initialization"""
        super(XML_DEM, self).__init__()

        self.xml_filename = xml_filename

    def parse_metadata(self):
        """Parse the input metadata file

        Search for the desired fields in the metadata file, and set the
        associated class variables with the values read for that field.

        Notes:
          It is expected the input metadata file is an ESPA *.xml file and
          follows the format of those files.
        """

        espa_metadata = ESPAMetadata()
        espa_metadata.parse(xml_filename=self.xml_filename)

        self.bounding_north_latitude = float(espa_metadata.xml_object
                                             .global_metadata
                                             .bounding_coordinates.north)
        self.bounding_south_latitude = float(espa_metadata.xml_object
                                             .global_metadata
                                             .bounding_coordinates.south)
        self.bounding_east_longitude = float(espa_metadata.xml_object
                                             .global_metadata
                                             .bounding_coordinates.east)
        self.bounding_west_longitude = float(espa_metadata.xml_object
                                             .global_metadata
                                             .bounding_coordinates.west)

        for corner_point in (espa_metadata.xml_object
                             .global_metadata
                             .projection_information.corner_point):
            if corner_point.attrib['location'] == 'UL':
                self.min_x_extent = float(corner_point.attrib['x'])
                self.max_y_extent = float(corner_point.attrib['y'])
            if corner_point.attrib['location'] == 'LR':
                self.max_x_extent = float(corner_point.attrib['x'])
                self.min_y_extent = float(corner_point.attrib['y'])

        self.wrs_path = int(espa_metadata.xml_object
                            .global_metadata.wrs.attrib['path'])
        self.wrs_row = int(espa_metadata.xml_object
                           .global_metadata.wrs.attrib['row'])

        try:
            self.product_id = (espa_metadata.xml_object
                               .global_metadata.product_id)
        except:
            self.product_id = (espa_metadata.xml_object
                               .global_metadata.scene_id)

        self.dem_header_filename = DEM_HEADER_FILENAME.format(self.product_id)
        self.dem_image_filename = DEM_IMAGE_FILENAME.format(self.product_id)

        self.map_projection = (espa_metadata.xml_object
                               .global_metadata
                               .projection_information.attrib['projection'])

        if self.map_projection not in ['UTM', 'PS', 'ALBERS']:
            raise NotImplementedError('Unsupported Map Projection {0}'
                                      .format(self.map_projection))

        if self.map_projection == 'UTM':
            self.utm_zone = int(espa_metadata.xml_object
                                .global_metadata
                                .projection_information
                                .utm_proj_params.zone_code)

        if self.map_projection == 'PS':
            self.longitude_pole = float(espa_metadata.xml_object
                                        .global_metadata
                                        .projection_information
                                        .ps_proj_params.longitude_pole)
            self.latitude_true_scale = float(espa_metadata.xml_object
                                             .global_metadata
                                             .projection_information
                                             .ps_proj_params
                                             .latitude_true_scale)
            self.false_easting = float(espa_metadata.xml_object
                                       .global_metadata
                                       .projection_information
                                       .ps_proj_params.false_easting)
            self.false_northing = float(espa_metadata.xml_object
                                        .global_metadata
                                        .projection_information
                                        .ps_proj_params.false_northing)

        if self.map_projection == 'ALBERS':
            self.origin_latitude = float(espa_metadata.xml_object
                                         .global_metadata
                                         .projection_information
                                         .albers_proj_params.origin_latitude)
            self.central_meridian = float(espa_metadata.xml_object
                                          .global_metadata
                                          .projection_information
                                          .albers_proj_params.central_meridian)
            self.standard_parallel_1 = float(espa_metadata.xml_object
                                             .global_metadata
                                             .projection_information
                                             .albers_proj_params
                                             .standard_parallel1)
            self.standard_parallel_2 = float(espa_metadata.xml_object
                                             .global_metadata
                                             .projection_information
                                             .albers_proj_params
                                             .standard_parallel2)
            self.false_easting = float(espa_metadata.xml_object
                                       .global_metadata
                                       .projection_information
                                       .albers_proj_params.false_easting)
            self.false_northing = float(espa_metadata.xml_object
                                        .global_metadata
                                        .projection_information
                                        .albers_proj_params.false_northing)

        for band in espa_metadata.xml_object.bands.band:
            if (band.attrib['product'] in ['L1T', 'L1GT'] and
                    band.attrib['name'] == 'band1'):
                self.target_srs = (
                    Geo.get_proj4_projection_string(str(band.file_name)))
                self.pixel_resolution_x = float(band.pixel_size.attrib['x'])
                self.pixel_resolution_y = float(band.pixel_size.attrib['y'])
                break

        # Adjust the coordinates for image extents becuse they are in
        # center of pixel, and we need to supply the warping with actual
        # extents
        self.min_x_extent = (self.min_x_extent -
                             self.pixel_resolution_x * 0.5)
        self.max_x_extent = (self.max_x_extent +
                             self.pixel_resolution_x * 0.5)
        self.min_y_extent = (self.min_y_extent -
                             self.pixel_resolution_y * 0.5)
        self.max_y_extent = (self.max_y_extent +
                             self.pixel_resolution_y * 0.5)

        del espa_metadata


class MTL_DEM(Base_DEM):
    """Defines the class object for MTL based DEM generation/processing"""

    def __init__(self, mtl_filename):
        """Class initialization"""
        super(MTL_DEM, self).__init__()

        self.mtl_filename = mtl_filename

    def parse_metadata(self):
        """Parse the input metadata file

        Search for the desired fields in the metadata file, and set the
        associated class variables with the values read for that field.

        Notes:
          It is expected the input metadata file is an LPGS _MTL.txt file and
          follows the format (contains the same fields) of those files.
        """

        # Get the logger
        logger = logging.getLogger(__name__)

        # open the metadata file for reading
        with open(self.mtl_filename, 'r') as mtl_fd:

            # read and process one line at a time, using the = sign as the
            # deliminator for the desired parameters.  remove the trailing
            # end of line and leading/trailing white space
            for line in mtl_fd:
                myline = line.rstrip('\r\n').strip()
                logger.debug('DEBUG: [{0}]'.format(myline))
                # break out if at the end of the metadata in the metadata file
                if myline == 'END':
                    break

                # process the myline which should be parameter = value strings
                # some strings may need the "s stripped from the value itself
                (param, value) = myline.split('=', 1)
                param = param.strip()
                value = value.strip()
                try:
                    f_value = float(value)
                except ValueError:
                    pass
                logger.debug('    DEBUG: param [{0}]'.format(param))
                logger.debug('    DEBUG: value [{0}]'
                             .format(value.strip('\"')))
                if param == 'CORNER_UL_LAT_PRODUCT':
                    if f_value > self.bounding_north_latitude:
                        self.bounding_north_latitude = f_value
                elif param == 'CORNER_UR_LAT_PRODUCT':
                    if f_value > self.bounding_north_latitude:
                        self.bounding_north_latitude = f_value
                elif param == 'CORNER_LL_LAT_PRODUCT':
                    if f_value < self.bounding_south_latitude:
                        self.bounding_south_latitude = f_value
                elif param == 'CORNER_LR_LAT_PRODUCT':
                    if f_value < self.bounding_south_latitude:
                        self.bounding_south_latitude = f_value
                elif param == 'CORNER_UR_LON_PRODUCT':
                    if f_value > self.bounding_east_longitude:
                        self.bounding_east_longitude = f_value
                elif param == 'CORNER_LR_LON_PRODUCT':
                    if f_value > self.bounding_east_longitude:
                        self.bounding_east_longitude = f_value
                elif param == 'CORNER_UL_LON_PRODUCT':
                    if f_value < self.bounding_west_longitude:
                        self.bounding_west_longitude = f_value
                elif param == 'CORNER_LL_LON_PRODUCT':
                    if f_value < self.bounding_west_longitude:
                        self.bounding_west_longitude = f_value
                elif param == 'CORNER_UL_PROJECTION_X_PRODUCT':
                    self.min_x_extent = f_value
                elif param == 'CORNER_UL_PROJECTION_Y_PRODUCT':
                    self.max_y_extent = f_value
                elif param == 'CORNER_LR_PROJECTION_X_PRODUCT':
                    self.max_x_extent = f_value
                elif param == 'CORNER_LR_PROJECTION_Y_PRODUCT':
                    self.min_y_extent = f_value
                elif param == 'GRID_CELL_SIZE_REFLECTIVE':
                    self.pixel_resolution_x = f_value
                    self.pixel_resolution_y = f_value
                elif param == 'WRS_PATH':
                    self.wrs_path = int(value)
                elif param == 'WRS_ROW':
                    self.wrs_row = int(value)
                elif param == 'LANDSAT_SCENE_ID':
                    self.product_id = value.strip('\"')
                elif param == 'MAP_PROJECTION':
                    self.map_projection = value.strip('\"')
                    if self.map_projection == 'AEA':
                        self.map_projection = 'ALBERS'
                elif param == 'UTM_ZONE':
                    self.utm_zone = int(value)
                elif param == 'VERTICAL_LON_FROM_POLE':
                    self.longitude_pole = f_value
                elif param == 'TRUE_SCALE_LAT':
                    self.latitude_true_scale = f_value
                elif param == 'ORIGIN_LATITUDE':
                    self.origin_latitude = f_value
                elif param == 'CENTRAL_MERIDIAN':
                    self.central_meridian = f_value
                elif param == 'STANDARD_PARALLEL_1':
                    self.standard_parallel_1 = f_value
                elif param == 'STANDARD_PARALLEL_2':
                    self.standard_parallel_2 = f_value
                elif param == 'FALSE_EASTING':
                    self.false_easting = f_value
                elif param == 'FALSE_NORTHING':
                    self.false_northing = f_value

        # validate retrieval of parameters
        if self.bounding_north_latitude == -9999.0:
            raise RuntimeError('Obtaining north bounding metadata field from:'
                               ' [{0}]'.format(self.mtl_filename))

        if self.bounding_south_latitude == 9999.0:
            raise RuntimeError('Obtaining south bounding metadata field from:'
                               ' [{0}]'.format(self.mtl_filename))

        if self.bounding_east_longitude == -9999.0:
            raise RuntimeError('Obtaining east bounding metadata field from:'
                               ' [{0}]'.format(self.mtl_filename))

        if self.bounding_west_longitude == 9999.0:
            raise RuntimeError('Obtaining west bounding metadata field from:'
                               ' [{0}]'.format(self.mtl_filename))

        if self.min_x_extent is None:
            raise RuntimeError('Obtaining UL Map X field from: [{0}]'
                               .format(self.mtl_filename))

        if self.max_y_extent is None:
            raise RuntimeError('Obtaining UL Map Y field from: [{0}]'
                               .format(self.mtl_filename))

        if self.max_x_extent is None:
            raise RuntimeError('Obtaining LR Map X field from: [{0}]'
                               .format(self.mtl_filename))

        if self.min_y_extent is None:
            raise RuntimeError('Obtaining LR Map Y field from: [{0}]'
                               .format(self.mtl_filename))

        if self.pixel_resolution_x is None:
            raise RuntimeError('Obtaining reflective grid cell size field'
                               ' from: [{0}]'.format(self.mtl_filename))

        if self.pixel_resolution_y is None:
            raise RuntimeError('Obtaining reflective grid cell size field'
                               ' from: [{0}]'.format(self.mtl_filename))

        if self.wrs_path is None:
            raise RuntimeError('Obtaining path metadata field from: [{0}]'
                               .format(self.mtl_filename))

        if self.wrs_row is None:
            raise RuntimeError('Obtaining row metadata field from: [{0}]'
                               .format(self.mtl_filename))

        if self.map_projection not in ['UTM', 'PS', 'ALBERS']:
            raise NotImplementedError('Unsupported Map Projection {0}'
                                      .format(self.map_projection))

        if self.map_projection == 'UTM' and self.utm_zone is None:
            raise RuntimeError('Obtaining UTM zone metadata field from:'
                               ' [{0}]'.format(self.mtl_filename))

        if self.map_projection == 'PS':
            if self.longitude_pole is None:
                raise RuntimeError('Obtaining VERTICAL_LON_FROM_POLE metadata'
                                   ' field from: [{0}]'
                                   .format(self.mtl_filename))

            if self.latitude_true_scale is None:
                raise RuntimeError('Obtaining TRUE_SCALE_LAT metadata field'
                                   ' from: [{0}]'.format(self.mtl_filename))

        if self.map_projection == 'ALBERS':
            if self.origin_latitude is None:
                raise RuntimeError('Obtaining ORIGIN_LATITUDE metadata field'
                                   ' from: [{0}]'.format(self.mtl_filename))

            if self.central_meridian is None:
                raise RuntimeError('Obtaining CENTRAL_MERIDIAN metadata field'
                                   ' from: [{0}]'.format(self.mtl_filename))

            if self.standard_parallel_1 is None:
                raise RuntimeError('Obtaining STANDARD_PARALLEL_1 metadata'
                                   ' field from: [{0}]'
                                   .format(self.mtl_filename))

            if self.standard_parallel_2 is None:
                raise RuntimeError('Obtaining STANDARD_PARALLEL_2 metadata'
                                   ' field from: [{0}]'
                                   .format(self.mtl_filename))

        if self.map_projection in ['PS', 'ALBERS']:
            if self.false_easting is None:
                raise RuntimeError('Obtaining FALSE_EASTING metadata field'
                                   ' from: [{0}]'.format(self.mtl_filename))

            if self.false_northing is None:
                raise RuntimeError('Obtaining FALSE_NORTHING metadata field'
                                   ' from: [{0}]'.format(self.mtl_filename))

    """
    # ------------------------------------------------------------------------
    def generate(self):
    """
    '''
        Description:
          Use the parameter passed for the LPGS metadata file (*_MTL.txt) to
          parse the necessary fields to create the OMF and ODL files needed
          for running retrieve_elevation, makegeomgrid, and geomresample in
          order to generate a scene-based DEM.

        Inputs:
          mtl_filename - name of the Landsat metadata file to be processed
          dem_filename - name of the DEM file to create
          usebin - this specifies if the DEM exes reside in the $BIN directory;
                   if None then the exes are expected to be in the PATH
          keep_intermediate - keep the intermediate files

        Returns:
          ERROR - error running the DEM applications
          SUCCESS - successful processing

        Notes:
          The script obtains the path of the metadata file and changes
          directory to that path for running the DEM code.  If the mtl_filename
          directory is not writable, then this script exits with an error.
    '''
    """

        # Get the logger
        logger = logging.getLogger(__name__)

        mtl_filename=None
        dem_filename=None,
        usebin=False
        keep_intermediate=False

        # if no parameters were passed then get the info from the command line
        if mtl_filename is None:
            # get the command line argument for the metadata file
            description = 'Create a DEM using LPGS DEM creation executables'
            parser = ArgumentParser(description=description)

            parser.add_argument('--mtl', '--mtl_filename',
                                action='store',
                                dest='mtl_filename',
                                help='name of Landsat LPGS MTL file',
                                metavar='FILE')

            parser.add_argument('--dem', '--dem_filename',
                                action='store',
                                dest='dem_filename',
                                help='name of the DEM file to create',
                                metavar='FILE')

            parser.add_argument('--usebin',
                                action='store_true',
                                dest='usebin',
                                default=False,
                                help=('use BIN environment variable as the'
                                      ' location of DEM apps'))

            parser.add_argument('--keep_intermediate',
                                action='store_true',
                                dest='keep_intermediate',
                                default=False,
                                help='keep the intermediate files')

            args = parser.parse_args()

            # validate the command-line options
            mtl_filename = args.mtl_filename  # name of the metadata file
            if dem_filename is None:
                dem_filename = args.dem_filename  # name of the DEM file
            usebin = args.usebin  # should $BIN directory be used
            keep_intermediate = args.keep_intermediate  # keep them

            if mtl_filename is None:
                parser.error('missing mtl_filename command-line argument')
                return ERROR

        if dem_filename is not None:
            self.dem_filename_provided = True
            self.set_dem_envi_names(self, dem_filename=dem_filename)

        # open the log file if it exists and the log handler wasn't already
        # specified as a parameter; use line buffering for the output; if the
        # log handler was passed as a parameter, then don't close the log
        # handler.  that will be up to the calling routine.
        logger.info('Processing scene-based DEMs for Landsat metadata file:'
                    ' [{0}]'.format(mtl_filename))

        # should we expect the DEM applications to be in the PATH or in the
        # BIN directory?
        if usebin:
            # get the BIN dir environment variable
            bin_dir = os.environ.get('BIN')
            bin_dir = bin_dir + '/'
            msg = 'BIN environment variable: [{0}]'.format(bin_dir)
            logger.info(msg)
        else:
            # don't use a path to the DEM applications, they are expected
            # to be in the PATH
            bin_dir = ''
            msg = 'DEM executables expected to be in the PATH'
            logger.info(msg)

        # make sure the metadata file exists
        if not os.path.isfile(mtl_filename):
            logger.critical('Error: metadata file does not exist or is not'
                            ' accessible: [{0}]'.format(mtl_filename))
            return ERROR

        # use the base metadata filename and not the full path.
        base_mtl_filename = os.path.basename(mtl_filename)
        logger.info('Processing metadata file: [{0}]'
                    .format(base_mtl_filename))

        # get the path of the MTL file and change directory to that location
        # for running this script.  save the current working directory for
        # return to upon error or when processing is complete.  Note: use
        # abspath to handle the case when the filepath is just the filename
        # and doesn't really include a file path (i.e. the current working
        # directory).
        mydir = os.getcwd()
        metadir = os.path.dirname(os.path.abspath(mtl_filename))
        if not os.access(metadir, os.W_OK):
            logger.info('Path of metadata file is not writable: [{0}].'
                        '  DEM apps need  write access to the metadata'
                        ' directory.'.format(metadir))
            return ERROR

        logger.info('Changing directories for DEM processing: [{0}]'
                    .format(metadir))
        os.chdir(metadir)

        # parse the metadata file to get the necessary parameters
        return_value = self.parse_metadata()
        if return_value != SUCCESS:
            msg = 'Error parsing the metadata. Processing will terminate.'
            logger.info(msg)
            os.chdir(mydir)
            return ERROR

        # create the OMF and ODL files
        self.write_parameters(base_mtl_filename)

        # ------
        # Retrieve the DEM data from the static source
        cmd = ' '.join(['{0}retrieve_elevation'.format(bin_dir),
                        self.retrieve_elev_odl])
        output = ''
        try:
            logger.info('Executing: [{0}]'.format(cmd))
            output = execute_cmd(cmd)
        except Exception:
            logger.exception('Error running retrieve_elevation.'
                             '  Processing will terminate.')
            os.chdir(mydir)
            return ERROR
        finally:
            if len(output) > 0:
                logger.info(output)

        # ------
        # Create a grid for the data to be extracted from the source DEM
        cmd = ' '.join(['{0}makegeomgrid'.format(bin_dir),
                        self.makegeomgrid_odl])
        output = ''
        try:
            logger.info('Executing: [{0}]'.format(cmd))
            output = execute_cmd(cmd)
        except Exception:
            logger.exception('Error running makegeomgrid.'
                             '  Processing will terminate.')
            os.chdir(mydir)
            return ERROR
        finally:
            if len(output) > 0:
                logger.info(output)

        # ------
        # Extract and resample the DEM data to match the scene
        cmd = ' '.join(['{0}geomresample'.format(bin_dir),
                        self.geomresample_odl])
        output = ''
        try:
            logger.info('Executing: [{0}]'.format(cmd))
            output = execute_cmd(cmd)
        except Exception:
            logger.exception('Error running geomresample.'
                             '  Processing will terminate.')
            os.chdir(mydir)
            return ERROR
        finally:
            if len(output) > 0:
                logger.info(output)

        # ------
        # Convert the HDF5 DEM to ENVI format
        cmd = ' '.join(['gdal_translate',
                        '-of', 'ENVI',
                        'HDF5:\"{0}\"://B01'.format(self.scene_dem),
                        self.dem_envi_img])
        output = ''
        try:
            logger.info('Executing: [{0}]'.format(cmd))
            output = execute_cmd(cmd)
        except Exception:
            logger.exception('Error running gdal_translate, which is'
                             ' expected to be in your PATH.'
                             '  Processing will terminate.')
            os.chdir(mydir)
            return ERROR
        finally:
            if len(output) > 0:
                logger.info(output)

        # modify the gdal output header since it doesn't contain the correct
        # projection information; instead it just flags the image as being
        # in the Geographic projection
        logger.debug('DEBUG: Updating the GDAL header file')
        self.fix_gdal_hdr(self.dem_envi_hdr)

        # remove intermediate files
        if not keep_intermediate:
            os.remove(self.retrieve_elev_odl)
            os.remove(self.makegeomgrid_odl)
            os.remove(self.geomresample_odl)
            os.remove(self.lsrd_omf)
            os.remove(self.source_dem)
            os.remove(self.scene_dem)
            os.remove(self.geomgrid)
            os.remove(self.dem_gdal_aux)

        # successful completion.  return to the original directory.
        os.chdir(mydir)
        logger.info('Completion of scene based DEM generation.')

        return SUCCESS
    """


def main():
    """Provides the main processing for the script"""

    # get the command line argument for the metadata file
    description = 'Create a DEM using either MTL or XML as information source'
    parser = ArgumentParser(description=description)

    parser.add_argument('--mtl', '--mtl_filename',
                        action='store',
                        dest='mtl_filename',
                        default=None,
                        help='name of Landsat MTL file',
                        metavar='FILE')

    parser.add_argument('--xml', '--xml_filename',
                        action='store',
                        dest='xml_filename',
                        default=None,
                        help='name of ESPA XML Metadata file',
                        metavar='FILE')

    parser.add_argument('--debug',
                        action='store_true',
                        dest='debug',
                        default=False,
                        help='turn debug logging on')

    parser.add_argument('--keep_intermediate',
                        action='store_true',
                        dest='keep_intermediate',
                        default=False,
                        help='keep the intermediate files')

    args = parser.parse_args()

    # Check logging level
    logging_level = logging.INFO
    if args.debug:
        logging_level = logging.DEBUG

    # Setup the default logger format and level.  Log to STDOUT.
    logging.basicConfig(format=('%(asctime)s.%(msecs)03d %(process)d'
                                ' %(levelname)-8s'
                                ' %(filename)s:%(lineno)d:'
                                '%(funcName)s -- %(message)s'),
                        datefmt='%Y-%m-%d %H:%M:%S',
                        level=logging_level,
                        stream=sys.stdout)

    logger = logging.getLogger(__name__)

    if ESPA_DEM_DIR not in os.environ:
        print('Invalid environment: Missing ESPA_DEM_DIR\n')
        sys.exit(1)  # EXIT_FAILURE

    dem = None
    # Call the core processing
    if args.mtl_filename is not None:
        print(args.mtl_filename)
        dem = MTL_DEM(args.mtl_filename)
    elif args.xml_filename is not None:
        print(args.xml_filename)
        dem = XML_DEM(args.xml_filename)
    else:
        print('Must specify either an MTL or ESPA XML input file\n')
        parser.print_help()
        print('')
        sys.exit(1)  # EXIT_FAILURE

    try:
        dem.generate()
    except:
        logger.exception('DEM generation failed')
        sys.exit(1)  # EXIT_FAILURE

    sys.exit(0)  # EXIT_SUCCESS


if __name__ == '__main__':
    main()
