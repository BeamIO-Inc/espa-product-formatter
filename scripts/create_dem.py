#! /usr/bin/env python

'''
License:
    NASA Open Source Agreement 1.3

Description:
    Parse the input metadata file, create the needed OMF and ODL parameter
    files, and run the associated DEM-related applications to generate a
    scene-based DEM for the input metadata file.

    The metadata file should be the LPGS <scene_id>_MTL.txt file.

    All logging goes to STDOUT by default.

History:
    Created on January 25, 2013 by Gail Schmidt, USGS/EROS
    Updated on February 6, 2013 by Gail Schmidt, USGS/EROS
        Changed the script to update the map projection information in the
        GDAL header file to reflect the correct projection and not Geographic.
    Updated on March 20, 2013 by Gail Schmidt, USGS/EROS
        Modified to support Polar Stereographic projections.
    Updated Oct/2013 by Ron Dilley, USGS/EROS
        Renamed and modified for inclusion into the espa-common library.
        Modified to utilize espa-common functionality.
        Enhanced the running as a stand-alone application.
        Replaced using the depricated 'commands' with using 'subprocess'
    Updated Jan/2015 by Ron Dilley, USGS/EROS
        Significant updates for coding conventions and other LSRD standards.
        Replaced using the 'subprocess' with using 'commands', since python3
        has a better implementation of subprocess that works just like
        'commands' and more importantly works.

Usage:
    create_dem.py --help prints the help message
'''

import os
import sys
import commands
import logging
from argparse import ArgumentParser


# -- ENVI format projection values -- #
ENVI_GEO_PROJ = 1
ENVI_UTM_PROJ = 2
ENVI_PS_PROJ = 31

# -- GCTP projection values -- #
GCTP_GEO_PROJ = 0
GCTP_UTM_PROJ = 1
GCTP_PS_PROJ = 6

# Return codes for methods
SUCCESS = 0
ERROR = -1


# ============================================================================
def execute_cmd(cmd):
    '''
    Description:
      Execute a command line and return the terminal output or raise an
      exception

    Returns:
        output - The stdout and/or stderr from the executed command.
    '''

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


# ============================================================================
class SceneDEM(object):
    '''
      Defines the class object for scene based DEM generation/processing
    '''

    # class attributes
    west_bound_lon = 9999.0           # west bounding coordinate
    east_bound_lon = -9999.0          # east bounding coordinate
    north_bound_lat = -9999.0         # north bounding coordinate
    south_bound_lat = 9999.0          # south bounding coordinate
    ul_proj_x = -13.0                 # UL projection x coordinate
    ul_proj_y = -13.0                 # UL projection y coordinate
    pixsize = -13.0                   # pixel size
    path = 0                          # WRS path
    row = 0                           # WRS row
    map_proj = 'None'                 # map projection (UTM or PS)
    utm_zone = 99                     # UTM zone
    vert_lon_from_pole = -9999.0      # vertical longitude from pole (PS proj)
    true_scale_lat = -9999.0          # true scale latitude (PS proj)
    false_east = -9999.0              # false easting
    false_north = -9999.0             # false northing
    retrieve_elev_odl = 'retrieve_elevation.odl'  # LPGS parameters
    makegeomgrid_odl = 'makegeomgrid.odl'         # LPGS parameters
    geomresample_odl = 'geomresample.odl'         # LPGS parameters
    lsrd_omf = 'LSRD.omf'                        # LPGS processing parameters
    source_dem = 'dem.lsrd_source.h5'            # source DEM HDF filename
    scene_dem = 'dem.lsrd.h5'        # scene based DEM HDF filename
    geomgrid = 'grid.lsrd_geometric.h5'  # geometric grid HDF filename
    dem_filename_provided = False  # to determine if it was provided or not

    # ------------------------------------------------------------------------
    def __init__(self):
        self.set_dem_envi_names(scene_id='lsrd')

    # ------------------------------------------------------------------------
    def set_dem_envi_names(self, scene_id=None, dem_filename=None):
        '''Setup some of the filenames to use'''
        # Image
        if dem_filename is not None:
            self.dem_envi_img = dem_filename
        elif scene_id is not None:
            self.dem_envi_img = '{0}_dem.img'.format(scene_id)
        else:
            self.dem_envi_img = 'lsrd_dem.img'
        # Header
        self.dem_envi_hdr = self.dem_envi_img.replace('.img', '.hdr')
        # GDAL garbage
        self.dem_gdal_aux = self.dem_envi_img.replace('.img', '.aux.xml')

    # ------------------------------------------------------------------------
    def fix_gdal_hdr(self, gdal_hdr):
        '''
        Description:
          Parse the GDAL generated header file for the scene based DEM and
          update the map projection information to be correct, based on the
          projection information in the MTL file.

        Inputs:
          gdal_hdr - name of the gdal_hdr file to be parsed

        Returns: Nothing

        Notes:
          It is expected the GDAL header exists and contains the map info
          field along with all the other fields needed for the header.
        '''

        # open the GDAL header file for reading
        with open(gdal_hdr, 'r') as hdr_fd:

            # open a temporary new file for writing (copying the current GDAL
            # header information)
            temp_hdr = 'temp.hdr'
            with open(temp_hdr, 'w') as temp_fd:

                # Read and process one line at a time, looking for the line
                # with 'map info'
                for line in hdr_fd:
                    myline = line.strip()

                    # if the current line contains 'map info' then we want to
                    # modify this line with the new projection info
                    if 'map info' in myline:
                        # Always skip it and add it ourself
                        pass
                    else:
                        # copy this line as-is to the temporary header file
                        temp_fd.write(line)

                if self.map_proj == 'UTM':
                    if self.utm_zone > 0:
                        map_info_str = ('map info = {{UTM, 1.000, 1.000,'
                                        ' {0}, {1}, {2}, {3}, {4}, North,'
                                        ' WGS-84, units=Meters}}\n'
                                        .format(self.ul_proj_x,
                                                self.ul_proj_y,
                                                self.pixsize,
                                                self.pixsize,
                                                self.utm_zone))
                    else:
                        map_info_str = ('map info = {{UTM, 1.000, 1.000,'
                                        ' {0}, {1}, {2}, {3}, {4}, South,'
                                        ' WGS-84, units=Meters}}\n'
                                        .format(self.ul_proj_x,
                                                self.ul_proj_y,
                                                self.pixsize,
                                                self.pixsize,
                                                -self.utm_zone))
                    temp_fd.write(map_info_str)
                elif self.map_proj == 'PS':
                    map_info_str = ('map info = {{Polar Stereographic,'
                                    ' 1.000, 1.000, {0}, {1}, {2}, {3},'
                                    ' WGS-84, units=Meters}}\n'
                                    .format(self.ul_proj_x,
                                            self.ul_proj_y,
                                            self.pixsize,
                                            self.pixsize))
                    temp_fd.write(map_info_str)
                    proj_info_str = ('projection info = {{{0}, 6378137.0,'
                                     ' 6356752.314245179, {1}, {2}, {3}, {4},'
                                     ' WGS-84, Polar Stereographic,'
                                     ' units=Meters}}\n'
                                     .format(ENVI_PS_PROJ,
                                             self.true_scale_lat,
                                             self.vert_lon_from_pole,
                                             self.false_east,
                                             self.false_north))
                    temp_fd.write(proj_info_str)
                else:
                    raise Exception('Unsupported map projection {0}'
                                    .format(self.map_proj))

        # remove the header file
        os.remove(gdal_hdr)

        # rename the temporary file to the header file
        os.rename(temp_hdr, gdal_hdr)

    # ------------------------------------------------------------------------
    def parse_metadata(self, mtl_filename):
        '''
        Description:
          Parse the input metadata file, search for the desired fields in the
          metadata file, and set the associated class variables with the
          values read for that field.

        Inputs:
          mtl_filename - name of the metadata file to be parsed

        Returns:
          ERROR - error occurred while parsing the metadata file
          SUCCESS - successful processing of the metadata file

        Notes:
          It is expected the input metadata file is an LPGS _MTL.txt file and
          follows the format (contains the same fields) of those files.
        '''

        # Get the logger
        logger = logging.getLogger(__name__)

        # open the metadata file for reading
        with open(mtl_filename, 'r') as mtl_fd:

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
                f_value = float(value)
                logger.debug('    DEBUG: param [{0}]'.format(param))
                logger.debug('    DEBUG: value [{0}]'
                             .format(value.strip('\"')))
                if param == 'CORNER_UL_LAT_PRODUCT':
                    if f_value > self.north_bound_lat:
                        self.north_bound_lat = f_value
                elif param == 'CORNER_UL_LON_PRODUCT':
                    if f_value < self.west_bound_lon:
                        self.west_bound_lon = f_value
                elif param == 'CORNER_UR_LAT_PRODUCT':
                    if f_value > self.north_bound_lat:
                        self.north_bound_lat = f_value
                elif param == 'CORNER_UR_LON_PRODUCT':
                    if f_value > self.east_bound_lon:
                        self.east_bound_lon = f_value
                elif param == 'CORNER_LL_LAT_PRODUCT':
                    if f_value < self.south_bound_lat:
                        self.south_bound_lat = f_value
                elif param == 'CORNER_LL_LON_PRODUCT':
                    if f_value < self.west_bound_lon:
                        self.west_bound_lon = f_value
                elif param == 'CORNER_LR_LAT_PRODUCT':
                    if f_value < self.south_bound_lat:
                        self.south_bound_lat = f_value
                elif param == 'CORNER_LR_LON_PRODUCT':
                    if f_value > self.east_bound_lon:
                        self.east_bound_lon = f_value
                elif param == 'CORNER_UL_PROJECTION_X_PRODUCT':
                    self.ul_proj_x = f_value
                elif param == 'CORNER_UL_PROJECTION_Y_PRODUCT':
                    self.ul_proj_y = f_value
                elif param == 'GRID_CELL_SIZE_REFLECTIVE':
                    self.pixsize = f_value
                elif param == 'WRS_PATH':
                    self.path = int(value)
                elif param == 'WRS_ROW':
                    self.row = int(value)
                elif param == 'MAP_PROJECTION':
                    self.map_proj = value.strip('\"')
                elif param == 'LANDSAT_SCENE_ID':
                    scene_id = value.strip('\"')
                    self.source_dem = 'dem.source.{0}.h5'.format(scene_id)
                    self.scene_dem = 'dem.scene.{0}.h5'.format(scene_id)
                    if not self.dem_filename_provided:
                        self.set_dem_envi_names(scene_id=scene_id)
                elif param == 'UTM_ZONE':
                    self.utm_zone = int(value)
                elif param == 'VERTICAL_LON_FROM_POLE':
                    self.vert_lon_from_pole = f_value
                elif param == 'TRUE_SCALE_LAT':
                    self.true_scale_lat = f_value
                elif param == 'FALSE_EASTING':
                    self.false_east = f_value
                elif param == 'FALSE_NORTHING':
                    self.false_north = f_value

        # validate input
        if ((self.north_bound_lat == -9999.0) or
                (self.south_bound_lat == 9999.0) or
                (self.east_bound_lon == -9999.0) or
                (self.west_bound_lon == 9999.0)):

            logger.critical('obtaining north/south and/or east/west bounding'
                            ' metadata fields from: [{0}]'
                            .format(mtl_filename))
            return ERROR

        if (self.ul_proj_x == -13.0) or (self.ul_proj_y == -13.0):
            logger.critical('obtaining UL project x/y fields from: [{0}]'
                            .format(mtl_filename))
            return ERROR

        if self.pixsize == -13.0:
            logger.critical('obtaining reflective grid cell size field from:'
                            ' [{0}]'.format(mtl_filename))
            return ERROR

        if (self.path == 0) or (self.row == 0):
            logger.critical('obtaining path/row metadata fields from: [{0}]'
                            .format(mtl_filename))
            return ERROR

        if (self.map_proj != 'UTM') and (self.map_proj != 'PS'):
            logger.critical('obtaining map projection metadata field from:'
                            ' [{0}].  Only UTM and PS are supported.'
                            .format(mtl_filename))
            return ERROR

        if self.map_proj == 'UTM' and self.utm_zone == 0:
                logger.critical('obtaining UTM zone metadata field from:'
                                ' [{0}]'.format(mtl_filename))
                return ERROR

        if self.map_proj == 'PS':
            if ((self.vert_lon_from_pole == -9999.0) or
                    (self.true_scale_lat == -9999.0) or
                    (self.false_east == -9999.0) or
                    (self.false_north == -9999.0)):

                logger.critical('obtaining Polar Stereographic metadata'
                                ' fields (vertical longitude from pole, true'
                                ' scale latitude, false easting, and/or false'
                                ' northing from: [{0}]'.format(mtl_filename))
                return ERROR

        # convert the UL corner to represent the UL corner of the pixel
        # instead of the center of the pixel.  the coords in the MTL file
        # represent the center of the pixel.
        self.ul_proj_x -= 0.5 * self.pixsize
        self.ul_proj_y += 0.5 * self.pixsize

        return SUCCESS

    # ------------------------------------------------------------------------
    def write_parameters(self, mtl_filename):
        '''
        Description:
            Creates and writes the necessary ODL and OMF parameter files for
            running retrieve_elevation, makegeomgrid, and geomresample using
            the parameter values previously obtained.

        Inputs:
            mtl_filename - name of the metadata file to be used for processing

        Returns: Nothing
        '''

        # open the OMF file for writing
        with open(self.lsrd_omf, 'w') as omf_fd:
            # build the OMF file
            omf_string = ('OBJECT = OMF\n'
                          '  SATELLITE = 8\n'
                          '  UL_BOUNDARY_LAT_LON = ({0}, {3})\n'
                          '  UR_BOUNDARY_LAT_LON = ({0}, {2})\n'
                          '  LL_BOUNDARY_LAT_LON = ({1}, {3})\n'
                          '  LR_BOUNDARY_LAT_LON = ({1}, {2})\n'
                          '  TARGET_WRS_PATH = {4}\n'
                          '  TARGET_WRS_ROW = {5}\n'
                          .format(self.north_bound_lat,
                                  self.south_bound_lat,
                                  self.east_bound_lon,
                                  self.west_bound_lon,
                                  self.path, self.row))

            if self.map_proj == 'UTM':
                omf_string = ''.join([omf_string,
                                      '  TARGET_PROJECTION = 1\n',
                                      ('  UTM_ZONE = {0}\n'
                                       .format(self.utm_zone))])
            elif self.map_proj == 'PS':
                # GAIL finish!!  Add support for the projection params,
                # which are then read by makegeomgrid/get_proj_info_toa_refl.c
                omf_string = ''.join([omf_string,
                                      '  TARGET_PROJECTION = 6\n'])

            omf_string = ''.join([omf_string,
                                  ('  GRID_FILENAME_PASS_1 = "{0}"\n\n'
                                   .format(mtl_filename)),
                                  'END_OBJECT = OMF\n',
                                  'END\n'])
            omf_fd.write(omf_string)

        # open the retrieve_elevation ODL file for writing
        with open(self.retrieve_elev_odl, 'w') as odl_fd:
            # build the ODL file
            odl_string = ('OBJECT = SCA\n'
                          '  WO_DIRECTORY = "."\n'
                          '  WORK_ORDER_ID = LSRD\n'
                          '  DEM_FILENAME = "{0}"\n'
                          'END_OBJECT = SCA\n'
                          'END\n'.format(self.source_dem))
            odl_fd.write(odl_string)

        # open the makegeomgrid ODL file for writing
        with open(self.makegeomgrid_odl, 'w') as odl_fd:
            # build the ODL file
            odl_string = ('OBJECT = GRID_TERRAIN\n'
                          '  WO_DIRECTORY = "."\n'
                          '  WORK_ORDER_ID = LSRD\n'
                          '  CELL_LINES = 50\n'
                          '  CELL_SAMPLES = 50\n'
                          '  GEOM_GRID_FILENAME = "{0}"\n'
                          '  PROCESSING_PASS = 1\n'
                          '  SOURCE_BAND_NUMBER_LIST = 1\n'
                          '  SOURCE_IMAGE_TYPE = 0\n'
                          '  TARGET_BAND_NUMBER_LIST = 1\n'
                          'END_OBJECT = GRID_TERRAIN\n'
                          'END\n'.format(self.geomgrid))
            odl_fd.write(odl_string)

        # open the geomresample ODL file for writing
        with open(self.geomresample_odl, 'w') as odl_fd:
            # build the ODL file
            odl_string = ('OBJECT = RESAMPLE_TERRAIN\n'
                          '  WO_DIRECTORY = "."\n'
                          '  WORK_ORDER_ID = LSRD\n'
                          '  BACKGRND = 0.000000\n'
                          '  BAND_LIST = 1\n'
                          '  MINMAX_OUTPUT = (-500.000000,9000.000000)\n'
                          '  ODTYPE = "I*2"\n'
                          '  OUTPUT_IMAGE_FILENAME = "{0}"\n'
                          '  PCCALPHA = -0.500000\n'
                          '  PROCESSING_PASS = 1\n'
                          '  RESAMPLE = BI\n'
                          '  SOURCE_IMAGE_TYPE = 0\n'
                          '  WINDOW_FLAG = 0\n'
                          'END_OBJECT = RESAMPLE_TERRAIN\n'
                          'END\n'.format(self.scene_dem))
            odl_fd.write(odl_string)

    # ------------------------------------------------------------------------
    def create_dem(self, mtl_filename=None, dem_filename=None,
                   usebin=False, keep_intermediate=False):
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

        # Get the logger
        logger = logging.getLogger(__name__)

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
        return_value = self.parse_metadata(base_mtl_filename)
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


# ============================================================================
def main():
    '''Provides the main processing for the script'''

    # Setup the default logger format and level.  Log to STDOUT.
    logging.basicConfig(format=('%(asctime)s.%(msecs)03d %(process)d'
                                ' %(levelname)-8s'
                                ' %(filename)s:%(lineno)d:'
                                '%(funcName)s -- %(message)s'),
                        datefmt='%Y-%m-%d %H:%M:%S',
                        level=logging.INFO,
                        stream=sys.stdout)

    # Call the core processing
    if SceneDEM().create_dem() != SUCCESS:
        sys.exit(1)  # EXIT_FAILURE

    sys.exit(0)  # EXIT_SUCCESS


# ============================================================================
if __name__ == '__main__':
    main()
