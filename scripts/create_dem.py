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
import math
from cStringIO import StringIO
from argparse import ArgumentParser


import numpy as np
from lxml import objectify as objectify
from osgeo import gdal, osr


from espa_xml_interface import XMLError, XMLInterface
from espa_metadata_api import ESPAMetadataError, ESPAMetadata


# Environment variable for the locations of the DEMs
ESPA_DEM_DIR = 'ESPA_DEM_DIR'


class GeoError(Exception):
    """Exception to capture errors from the Geo class"""
    pass


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

    @staticmethod
    def warp(resampling_method=None,
             resolution_x=None,
             resolution_y=None,
             target_srs=None,
             image_extents=None,
             destination_no_data=None,
             output_data_type=None,
             output_format=None,
             source_data=None,
             output_filename=None):

        logger = logging.getLogger(__name__)

        # Base Command
        cmd = ['gdalwarp', '-wm', '2048', '-multi', '-overwrite']

        # Add resampling
        if resampling_method is not None:
            cmd.extend(['-r', resampling_method])

        # Add resolution
        if ((resolution_x is not None and resolution_y is None) or
                (resolution_x is None and resolution_y is not None)):
            raise GeoError('Must specify both X and Y resolution for warping')

        if resolution_x is not None and resolution_y is not None:
            cmd.extend(['-tr', str(resolution_x), str(resolution_y)])

        # Add target projection
        if target_srs is not None:
            cmd.extend(['-t_srs', ''.join(['"', target_srs, '"'])])

        # Add image extents
        if image_extents is not None:
            cmd.extend(['-te',
                        str(image_extents['min_x']),
                        str(image_extents['min_y']),
                        str(image_extents['max_x']),
                        str(image_extents['max_y'])])

        # Add output data type
        if destination_no_data is not None:
            cmd.extend(['-dstnodata', str(destination_no_data)])

        # Add output data type
        if output_data_type is not None:
            cmd.extend(['-ot', output_data_type])

        # Add output format
        if output_format is not None:
            cmd.extend(['-of', output_format])

        # Add the source data
        if source_data is None:
            raise GeoError('Must provide source data')

        if type(source_data) is list:
            cmd.extend(source_data)
        else:
            cmd.append(source_data)

        # Add the output filename
        if output_filename is None:
            raise GeoError('Must provide the output filename')

        cmd.append(output_filename)

        # Convert to a string for the execution
        cmd = ' '.join(cmd)

        output = ''
        try:
            logger.info('EXECUTING WARP COMMAND [{0}]'.format(cmd))
            output = execute_cmd(cmd)
        finally:
            if len(output) > 0:
                print(output)


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
        for index in xrange(count - 1):
            if (((x_v[index] > x_c) != (x_v[index + 1] > x_c)) and
                (y_c < ((y_v[index + 1] - y_v[index]) *
                        (x_c - x_v[index]) /
                        (x_v[index + 1] - x_v[index]) +
                        y_v[index]))):

                inside_polygon = not inside_polygon

        return inside_polygon

    @staticmethod
    def longitude_norm(longitude):
        """Calculates the "canonical longitude" for the longitude value

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

        # Grab what we need from the environment first
        if ESPA_DEM_DIR not in os.environ:
            raise RuntimeError('{0} environement variable not defined'
                               .format(ESPA_DEM_DIR))
        self.espa_dem_dir = os.environ.get(ESPA_DEM_DIR)

        # Padding to add to the max box
        self.maxbox_padding = 0.2

        # Latitude coordinate limits
        self.north_latitude_limit = 90.0
        self.south_latitude_limit = -90.0
        # Longitude coordinate limits
        self.east_longitude_limit = 180.0
        self.west_longitude_limit = -180.0

        # GLS latitude coordinate limits
        self.glsdem_north_limit = 83.0
        self.glsdem_south_limit = -53.0

        # RAMP lat coordinate limits
        self.ramp_south_limit = -60.0

        # WGS84 Information
        self.wgs84_dir = 'geoid'
        self.wgs84_header_name = 'geoid.hdr'
        self.wgs84_image_name = 'geoid.dem'

        self.wgs84_header_path = os.path.join(self.espa_dem_dir,
                                              self.wgs84_dir,
                                              self.wgs84_header_name.upper())
        self.wgs84_image_path = os.path.join(self.espa_dem_dir,
                                             self.wgs84_dir,
                                             self.wgs84_image_name.upper())

        # RAMP Information
        self.ramp_dir = 'ramp'
        self.ramp_header_name = 'ramp200dem_wgs_v2.hdr'
        self.ramp_image_name = 'ramp200dem_wgs_v2.img'

        self.ramp_header_path = os.path.join(self.espa_dem_dir,
                                             self.ramp_dir,
                                             self.ramp_header_name)
        self.ramp_image_path = os.path.join(self.espa_dem_dir,
                                            self.ramp_dir,
                                            self.ramp_image_name)

        # GLS Information
        self.gls_dir = 'gls'
        self.gls_projection_template = 'gls_projection.prj'

        # GTOPT30 Information
        self.gtopo30_dir = 'gtopo30'
        self.gtopo30_dems_regexp = '[EW]???[NS]??.DEM'
        self.gtopo30_files_regexp = '[EW]???[NS]??.*'
        self.gtopo30_padding = 1.0  # Degrees, since we are in geographic

        # DEM format and naming
        self.dem_format = 'ENVI'
        self.dem_type = 'Int16'
        self.dem_header_name_fmt = '{0}_dem.hdr'
        self.dem_image_name_fmt = '{0}_dem.img'
        # Landsat uses bi-linear for all DEM warping
        self.dem_resampling_method = 'bilinear'

        # MOSAIC Filenames
        self.mosaic_header_name = 'espa-mosaic-dem.hdr'
        self.mosaic_image_name = 'espa-mosaic-dem.img'

        # GDAL AUX files to remove
        self.gdal_aux_regexp = '*.img.aux.xml'

        # Initialize the following to something bad
        self.bounding_north_latitude = -9999.0
        self.bounding_south_latitude = 9999.0
        self.bounding_east_longitude = -9999.0
        self.bounding_west_longitude = 9999.0

        self.min_x_extent = None
        self.max_y_extent = None
        self.max_x_extent = None
        self.min_y_extent = None

        self.pixel_resolution_x = None
        self.pixel_resolution_y = None

        self.target_srs = None

        self.dem_header_name = None
        self.dem_image_name = None

    def parse_metadata(self):
        """Implement this to parse the metadata file"""
        raise NotImplementedError('Please Implement Me In {0}'
                                  .format(str(type(self))))

    def mosaic_tiles(self, tiles):
        """MOSAIC the specified tiles into one file"""

        logger = logging.getLogger(__name__)

        '''
        Set the no data value to 0 so we fill-in with sea-level,
        because missing tiles in GLS will be water(ocean)
        '''
        Geo.warp(destination_no_data=0,
                 output_data_type=self.dem_type,
                 output_format=self.dem_format,
                 source_data=tiles,
                 output_filename=self.mosaic_image_name)

    def mosaic_cleanup(self):
        """Remove the MOSAIC files"""

        os.unlink(self.mosaic_header_name)
        os.unlink(self.mosaic_image_name)

    def warp_to_final_dem(self, source_name):
        """Warp the tiles into the final DEM"""

        logger = logging.getLogger(__name__)

        image_extents = {'min_x': self.min_x_extent,
                         'min_y': self.min_y_extent,
                         'max_x': self.max_x_extent,
                         'max_y': self.max_y_extent}

        Geo.warp(resampling_method=self.dem_resampling_method,
                 resolution_x=self.pixel_resolution_x,
                 resolution_y=self.pixel_resolution_y,
                 target_srs=self.target_srs,
                 image_extents=image_extents,
                 output_data_type=self.dem_type,
                 output_format=self.dem_format,
                 source_data=source_name,
                 output_filename=self.dem_image_name)

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

        # Determine how many points are within the RAMP DEM and error
        # accordingly
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

        # Link the RAMP data to the current directory
        if not os.path.exists(self.ramp_image_name):
            # Should only need to test for one of them
            os.symlink(self.ramp_header_path, self.ramp_header_name)
            os.symlink(self.ramp_image_name, self.ramp_image_name)

        # Open the RAMP dataset
        ramp_ds = gdal.Open(self.ramp_image_name)

        # Create the RAMP SRS
        ramp_srs = osr.SpatialReference()
        ramp_srs.ImportFromWkt(ramp_ds.GetProjection())
        logger.debug('RAMP WKT: {0}'.format(ramp_ds.GetProjection()))

        # Create the Geographic SRS
        latlon_srs = ramp_srs.CloneGeogCS()

        # Create the coordinate transformation
        ll_to_ramp = osr.CoordinateTransformation(latlon_srs, ramp_srs)

        # Find the center of the max box
        center_latitude = ((self.bounding_north_latitude +
                            self.bounding_south_latitude) / 2.0)
        center_longitude = ((self.bounding_east_longitude +
                             self.bounding_west_longitude) / 2.0)

        # Find the max box and center in map coordinates
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

        # Get the RAMP transform
        ramp_transform = ramp_ds.GetGeoTransform()

        # Get the lines and samples from the RAMP band data
        ramp_band = ramp_ds.GetRasterBand(1)
        logger.debug('Lines, Samples: {0}, {1}'
                     .format(ramp_band.YSize, ramp_band.XSize))

        # This causes an exception if they do not overlap
        self._verify_ramp_overlap(ramp_band.YSize, ramp_band.XSize,
                                  ramp_transform,
                                  map_ul_x, map_ul_y,
                                  map_ur_x, map_ur_y,
                                  map_lr_x, map_lr_y,
                                  map_ll_x, map_ll_y,
                                  map_center_x, map_center_y)

        # Cleanup memory before warping
        del ll_to_ramp
        del latlon_srs
        del ramp_transform
        del ramp_band
        del ramp_srs
        del ramp_ds

        # Warp to the final DEM
        self.warp_to_final_dem(self.ramp_image_name)

        # Remove the symlink to the RAMP DEM
        os.unlink(self.ramp_header_name)
        os.unlink(self.ramp_image_name)

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
        ul_lon = Math.longitude_norm(self.bounding_west_longitude -
                                     self.gtopo30_padding)
        lr_lon = Math.longitude_norm(self.bounding_east_longitude +
                                     self.gtopo30_padding)
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
        ul_lat = min((self.bounding_north_latitude + self.gtopo30_padding),
                     self.north_latitude_limit)
        lr_lat = max((self.bounding_south_latitude - self.gtopo30_padding),
                     self.south_latitude_limit)
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

    def get_gtopo30_dems(self):
        """Retrieves the GTOPO30 DEM archives and extracts them"""

        logger = logging.getLogger(__name__)

        dem_dir = os.path.join(self.espa_dem_dir, self.gtopo30_dir)

        # Determine the GTOPO30 tiles
        tile_list = self.get_gtopo30_tile_list()
        logger.info('GTOPO30 Tile Names: {0}'.format(', '.join(tile_list)))

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

        # Grab the tile DEM filenames from the extracted archives
        tile_dem_list = glob.glob(self.gtopo30_dems_regexp)
        logger.info('GTOPO30 DEM Files: {0}'.format(', '.join(tile_dem_list)))

        return tile_dem_list

    def generate_using_gtopo30(self):
        """Generate the DEM using GTOPO30 data"""

        logger = logging.getLogger(__name__)

        # Retireve the GTOPO30 tiles
        tile_dem_list = self.get_gtopo30_dems()

        # MOSAIC the tiles together
        self.mosaic_tiles(tile_dem_list)

        # Warp to the final DEM
        self.warp_to_final_dem(self.mosaic_image_name)

        # Cleanup intermediate data
        self.mosaic_cleanup()

        remove_list = glob.glob(self.gtopo30_files_regexp)
        for file_name in remove_list:
            os.unlink(file_name)

    def generate_using_gls(self):
        """Retrieve the GLS DEM data"""

        logger = logging.getLogger(__name__)

        dem_dir = os.path.join(self.espa_dem_dir, self.gls_dir)

        '''
        GLS tiles are named with a format like n47w118 where the lat/long
        in the file name are the lower-left corner of the tile.  Each tile
        covers a 1-degree by 1-degree area. */
        '''
        start_latitude = int(math.floor(self.bounding_north_latitude))
        end_latitude = int(math.floor(self.bounding_south_latitude))
        start_longitude = int(math.floor(self.bounding_west_longitude))
        end_longitude = int(math.floor(self.bounding_east_longitude))
        logger.debug('Start Latitude: {0}'.format(start_latitude))
        logger.debug('End Latitude: {0}'.format(end_latitude))
        logger.debug('Start Longitude: {0}'.format(start_longitude))
        logger.debug('End Longitude: {0}'.format(end_longitude))

        tile_list = list()
        for lat in xrange(end_latitude, start_latitude + 1):
            logger.debug('Latitude: {0}'.format(lat))
            n_s = 'n'
            if lat < 0:
                n_s = 's'

            for lon in xrange(start_longitude, end_longitude + 1):
                logger.debug('Longitude: {0}'.format(lon))

                e_w = 'e'
                if lon < 0:
                    e_w = 'w'

                tile_list.append('{0}{1:02}{2}{3:03}'
                                 .format(n_s, int(lat), e_w, int(lon)))

        tile_count = len(tile_list)
        if tile_count == 0:
            raise RuntimeError('Unable to determine tiles while retrieving'
                               ' required GLS DEM tile list')

        '''
        Generate lists of the BIL, HDR, and PRJ filenames as well as
        determining any missing tiles
        '''
        missing_count = 0
        bil_list = list()
        hdr_list = list()
        prj_list = list()
        prj_path = os.path.join(dem_dir, self.gls_projection_template)
        logger.debug('PRJ Path: {0}'.format(prj_path))
        for tile in tile_list:
            bil_name = '{0}.bil'.format(tile)
            hdr_name = '{0}.hdr'.format(tile)
            prj_name = '{0}.prj'.format(tile)

            bil_path = os.path.join(dem_dir, bil_name)
            hdr_path = os.path.join(dem_dir, hdr_name)
            logger.debug('BIL Path: {0}'.format(bil_path))
            logger.debug('HDR Path: {0}'.format(hdr_path))

            if (os.path.isfile(bil_path) and
                    os.path.isfile(hdr_path)):

                # Link them to the current directory
                os.symlink(bil_path, bil_name)
                os.symlink(hdr_path, hdr_name)
                os.symlink(prj_path, prj_name)

                bil_list.append(bil_name)
                hdr_list.append(hdr_name)
                prj_list.append(prj_name)

            else:
                logger.debug('Missing Tile: {0}'.format(bil_name))
                missing_count += 1

        logger.debug('Expected Tile Count: {0}'.format(tile_count))
        logger.debug('Missing Tile Count: {0}'.format(missing_count))

        # Check if we are missing all the tiles, which indicates over water
        if missing_count >= tile_count:
            # Cleanup the tile links
            for file_name in bil_list:
                os.unlink(file_name)
            for file_name in hdr_list:
                os.unlink(file_name)
            for file_name in prj_list:
                os.unlink(file_name)
            raise GLSOverWaterError('GLS DEM is over water')

        logger.info('GLS DEM Files: {0}'.format(', '.join(bil_list)))

        # MOSAIC the tiles together
        self.mosaic_tiles(bil_list)

        # Warp to the final DEM
        self.warp_to_final_dem(self.mosaic_image_name)

        # Cleanup intermediate data
        self.mosaic_cleanup()

        for file_name in bil_list:
            os.unlink(file_name)
        for file_name in hdr_list:
            os.unlink(file_name)
        for file_name in prj_list:
            os.unlink(file_name)

    def adjust_elevation_to_wgs84(self):
        """Adjusts the warped DEM to the WGS84 GEOID"""

        logger = logging.getLogger(__name__)

        # Link the WGS84 GEOID data to the current directory
        if not os.path.exists(self.wgs84_image_name):
            # Should only need to test for one of them
            os.symlink(self.wgs84_header_path, self.wgs84_header_name)
            os.symlink(self.wgs84_image_path, self.wgs84_image_name)

        # TODO TODO TODO - Warp the GEOID to the DEM/product projection
        # TODO TODO TODO - Warp the GEOID to the DEM/product projection
        # TODO TODO TODO - Warp the GEOID to the DEM/product projection
        # TODO TODO TODO - Warp the GEOID to the DEM/product projection
        # TODO TODO TODO - Warp the GEOID to the DEM/product projection

        # Open the WGS84 and DEM datasets
        wgs84_ds = gdal.Open(self.wgs84_image_name)
        dem_ds = gdal.Open(self.dem_image_name)

        # Get the WGS84 and DEM band data
        wgs84_band = wgs84_ds.GetRasterBand(1)
        dem_band = dem_ds.GetRasterBand(1)

        wgs84_data = wgs84_band.ReadAsArray(0, 0, dem_band.XSize, dem_band.YSize)
        dem_data = dem_band.ReadAsArray(0, 0, dem_band.XSize, dem_band.YSize)

        # TODO TODO TODO - Use numpy math to add the datasets together
        # TODO TODO TODO - Use numpy math to add the datasets together
        # TODO TODO TODO - Use numpy math to add the datasets together
        # TODO TODO TODO - Use numpy math to add the datasets together
        # TODO TODO TODO - Use numpy math to add the datasets together

        # TODO TODO TODO - Overrite the DEM band with the new data
        # TODO TODO TODO - Overrite the DEM band with the new data
        # TODO TODO TODO - Overrite the DEM band with the new data
        # TODO TODO TODO - Overrite the DEM band with the new data
        # TODO TODO TODO - Overrite the DEM band with the new data

        # Cleanup memory
        del dem_band
        del dem_ds
        del wgs84_band
        del wgs84_ds

        # TODO TODO TODO - Move this up
        # Remove the symlink to the WGS84 GEOID
        os.unlink(self.wgs84_header_name)
        os.unlink(self.wgs84_image_name)

        # TODO TODO TODO - Remove the warped GEOID data
        # TODO TODO TODO - Remove the warped GEOID data
        # TODO TODO TODO - Remove the warped GEOID data
        # TODO TODO TODO - Remove the warped GEOID data
        # TODO TODO TODO - Remove the warped GEOID data

    def generate(self):
        """Generates the DEM"""

        logger = logging.getLogger(__name__)

        self.parse_metadata()

        # Just a bunch of debug reporting follows
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

        logger.debug('dem_header_name = {0}'
                     .format(self.dem_header_name))
        logger.debug('dem_image_name = {0}'
                     .format(self.dem_image_name))

        logger.debug('espa_dem_dir = {0}'.format(self.espa_dem_dir))

        # Pad the max box coordinate values
        self.bounding_north_latitude += self.maxbox_padding
        self.bounding_south_latitude -= self.maxbox_padding
        self.bounding_east_longitude += self.maxbox_padding
        self.bounding_west_longitude -= self.maxbox_padding

        # Retrieve the tiles, mosaic, and warp to the source data
        if self.bounding_north_latitude <= self.ramp_south_limit:
            try:
                logger.info('Attempting to use RAMP DEM')
                self.generate_using_ramp()
                '''
                According to Landsat the RAMP DEM does not need adjusting to
                the WGS84 GEOID
                '''
                # TODO TODO TODO - Verify this during testing
            except RAMPCoverageError:
                logger.exception('RAMP DEM failed to cover input data'
                                 ' defaulting to GTOPO30')
                self.generate_using_gtopo30()
                self.adjust_elevation_to_wgs84()

        elif ((self.bounding_north_latitude <= self.glsdem_south_limit and
               self.bounding_north_latitude > self.ramp_south_limit and
               self.bounding_south_latitude <= self.glsdem_south_limit and
               self.bounding_south_latitude > self.ramp_south_limit) or
              (self.bounding_north_latitude >= self.glsdem_north_limit and
               self.bounding_south_latitude >= self.glsdem_north_limit)):

            logger.info('Using GTOPO30 DEM')
            self.generate_using_gtopo30()
            self.adjust_elevation_to_wgs84()

        else:
            try:
                logger.info('Attempting to use GLS DEM')
                self.generate_using_gls()
            except GLSOverWaterError:
                logger.exception('GLS DEM over water defaulting to GTOPO30')
                self.generate_using_gtopo30()

            self.adjust_elevation_to_wgs84()

        # Cleanup the GDAL generated auxiliary files
        remove_list = glob.glob(self.gdal_aux_regexp)
        for file_name in remove_list:
            os.unlink(file_name)

        # Update the ENVI header
        Geo.update_envi_header(self.dem_header_name)

        # TODO TODO TODO - Add the DEM to the MTL file


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

        product_id = None
        try:
            product_id = espa_metadata.xml_object.global_metadata.product_id
        except:
            product_id = espa_metadata.xml_object.global_metadata.scene_id

        self.dem_header_name = self.dem_header_name_fmt.format(product_id)
        self.dem_image_name = self.dem_image_name_fmt.format(product_id)

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

        file_name_band_1 = None
        product_id = None

        # open the metadata file for reading
        with open(self.mtl_filename, 'r') as mtl_fd:

            # read and process one line at a time, using the = sign as the
            # deliminator for the desired parameters.  remove the trailing
            # end of line and leading/trailing white space
            for line in mtl_fd:
                myline = line.rstrip('\r\n').strip()
                # logger.debug('DEBUG: [{0}]'.format(myline))
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
                # logger.debug('    DEBUG: param [{0}]'.format(param))
                # logger.debug('    DEBUG: value [{0}]'
                #              .format(value.strip('\"')))
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
                elif param == 'CORNER_UL_LON_PRODUCT':
                    if f_value < self.bounding_west_longitude:
                        self.bounding_west_longitude = f_value
                elif param == 'CORNER_UR_LON_PRODUCT':
                    if f_value > self.bounding_east_longitude:
                        self.bounding_east_longitude = f_value
                elif param == 'CORNER_LL_LON_PRODUCT':
                    if f_value < self.bounding_west_longitude:
                        self.bounding_west_longitude = f_value
                elif param == 'CORNER_LR_LON_PRODUCT':
                    if f_value > self.bounding_east_longitude:
                        self.bounding_east_longitude = f_value
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
                elif param == 'LANDSAT_SCENE_ID':
                    product_id = value.strip('\"')
                elif param == 'FILE_NAME_BAND_1':
                    file_name_band_1 = value.strip('\"')

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

        if file_name_band_1 is None:
            raise RuntimeError('Obtaining FILE_NAME_BAND_1 field from: [{0}]'
                               .format(self.mtl_filename))

        if product_id is None:
            raise RuntimeError('Obtaining LANDSAT_SCENE_ID field from: [{0}]'
                               .format(self.mtl_filename))

        self.dem_header_name = self.dem_header_name_fmt.format(product_id)
        self.dem_image_name = self.dem_image_name_fmt.format(product_id)

        self.target_srs = Geo.get_proj4_projection_string(file_name_band_1)

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


def main():
    """Provides the main processing for the script"""

    # get the command line argument for the metadata file
    description = ('Create a DEM using either the MTL or XML metadata as the'
                   ' information source')
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
