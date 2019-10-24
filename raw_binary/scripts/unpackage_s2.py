#! /usr/bin/env python
import sys
import os
import shutil
import glob
from optparse import OptionParser
from zipfile import ZipFile
import logging

ERROR = 1
SUCCESS = 0


#############################################################################
# Created on August 23, 2019 by Gail Schmidt, USGS/EROS
# Created Python script to unpackage the Sentinel-2 products from their
# native SENTINEL-SAFE format into a single directory containing the
# Sentinal-2 bands as JPEG2000 in addition to the XML files needed for
# convert_sentinel_to_espa.
#
# Usage: unpackage_s2.py --help prints the help message
############################################################################
class S2SAFE():

    def __init__(self):
        pass


    ########################################################################
    # Description: unpackage will unzip the specified Sentinel-2 .zip file
    # into the specified directory.  From there, the image products and
    # required JPEG2000 files will be copied to the top directory.  All the
    # other files and subdirectories will be removed.
    #
    # Inputs:
    #   infile - name of input Sentinel-2 .zip file
    #   outdir - name of output directory into which to unzip the Sentinel-2
    #            product
    #
    # Returns:
    #     ERROR - error unpackaging the Sentinel-2 product
    #     SUCCESS - successful unpackage
    #
    # Notes:
    #   1. The script obtains the path of the output product. If that
    #      directory is not writable, then this script exits with an error.
    #######################################################################
    def unpackage (self, infile=None, outdir=None):
        # if no parameters were passed then get the info from the
        # command line
        if infile == None:
            # get the command line argument for the input file
            parser = OptionParser()
            parser.add_option ("-i", "--infile",
                               type="string", dest="infile",
                               help="name of input Sentinel-2 .zip file",
                               metavar="FILE")
            parser.add_option ("-o", "--outdir",
                               type="string", dest="outdir",
                               help="name of output directory into which the "
                                    "Sentinel-2 product will be unzipped",
                               metavar="DIR")
            (options, args) = parser.parse_args()
    
            # S2 input file
            infile = options.infile
            if infile == None:
                parser.error ('missing S2 input file command-line argument');
                return ERROR

            # S2 output directory
            outdir = options.outdir
            if outdir == None:
                parser.error ('missing S2 output directory command-line '
                              'argument');
                return ERROR

        # get the logger
        logger = logging.getLogger(__name__)
        msg = ('Unpackaging Sentinel-2 package {} into {}'
               .format(infile, outdir))
        logger.info(msg)
        
        # make sure the input file exists
        if not os.path.isfile(infile):
            msg = ('Input Sentinel-2 package does not exist or is not '
                   'accessible: {}'.format(infile))
            logger.error(msg)
            return ERROR

        # make sure the output directory exists otherwise create it. if it
        # exists, make sure it is writable.
        if not os.path.exists(outdir):
            msg = ('Making directory {}'.format(outdir))
            logger.error(msg)
            os.mkdir(outdir)
        else:
            if not os.access(outdir, os.W_OK):
                msg = ('Path of output directory is not writable: {}. Script '
                       'needs write access to this directory.'.format(outdir))
                logger.error(msg)
                return ERROR

        zipfile = os.path.basename(infile)

        # get the top-level directory from the .zip file
        safe_dirname = None
        inspire_xmlname = None
        with ZipFile(infile, 'r') as zip:
            listOfFileNames = zip.namelist()

            # iterate over the file names looking for INSPIRE.xml, which should
            # be valid in both the old and new S2 format. The directory should
            # be the .SAFE directory.
            for filename in listOfFileNames:
                if filename.endswith('INSPIRE.xml'):
                    inspire_xmlname = filename
                    break

        # make sure we found the tile-level XML file
        if inspire_xmlname is None:
            msg = ('Tile-level XML file in the Sentinel-2 .zip file should '
                   'exist. Unexpected file format.')
            logger.error(msg)
            return ERROR

        # save the top-level directory as the .SAFE directory
        safe_dirname = inspire_xmlname.split(os.sep)[0]

        # unzip the file into the output directory
        with ZipFile(infile, 'r') as zip:
            # extract all the files
            zip.extractall(path=outdir)

        # change directories to the Sentinel-2 output directory. the Sentinel-2
        # SAFE directory is the same as the .zip file with .zip replaced by
        # .SAFE.
        s2dir = '{}/{}'.format(outdir, safe_dirname)
        mydir = os.getcwd()
        msg = ('Changing directories to: {}'.format(s2dir))
        logger.info(msg)
        os.chdir(s2dir)

        # make a temp directory to save the desired files
        espa_temp = 'ESPATEMP'
        os.mkdir(espa_temp)

        # copy the MTD_MSIL1C.xml file into the temp directory. If it doesn't
        # exist, then this is the old S2 format and we need to look for
        # a file with S2[A|B]_OPER_MTD_*.xml.
        mtd_xmlname = 'MTD_MSIL1C.xml'
        old_s2_format = False
        if not os.path.isfile(mtd_xmlname):
            msg = 'Processing older Sentinel-2 package...'
            logger.info(msg)

            # find the MTD XML name
            xmlfiles = glob.glob('*.xml')
            found = False
            for xmlname in xmlfiles:
                if (xmlname.find('OPER_MTD_') != -1):
                    # found the desired XML file
                    os.rename(xmlname, mtd_xmlname)
                    found = True
                    old_s2_format = True
                    break

            # make sure the MTD XML file was found
            if not found:
                msg = ('Top-level XML file was not found.  Looking for '
                       '{} or something similar to S2[A|B]_OPER_MTD_*.xml.'
                       .format(mtd_xmlname))
                logger.error(msg)
                return ERROR

        # copy the product XML file to the ESPA temporary dir
        shutil.copy(mtd_xmlname, espa_temp)

        # determine the name of the {product_id} directory under GRANULE
        granule_dir = 'GRANULE'
        found = False
        gran_dirs = glob.glob('{}/*'.format(granule_dir))
        for tmpdir in gran_dirs:
            # only look at the directories
            if os.path.isdir(tmpdir):
                # old S2 - looking for directories with S2[A|B]_OPER_MSI_L1C_TL*
                if old_s2_format and (tmpdir.find('OPER_MSI_L1C_TL') != -1):
                    # found desired directory
                    prodid_dir = tmpdir
                    found = True
                    break

                # new S2 - looking for directories with L1C_*
                elif not old_s2_format and (tmpdir.find('L1C_') != -1):
                    # found desired directory
                    prodid_dir = tmpdir
                    found = True
                    break

        # make sure the product_id directory was found
        if not found:
            msg = ('Product ID directory under GRANULE was not found. Looking '
                   'for something similar to S2[A|B]_OPER_MSI_L1C_TL* under '
                   'the top-level .SAFE/GRANULE directory.')
            logger.error(msg)
            return ERROR

        # copy the MTD_TL.xml file from GRANULE/{product_id} into the temp
        # directory.  If this is the old Sentinel format, then we need to look
        # for a file with S2[A|B]_OPER_MTD_L1C_TL*.xml.
        tile_xmlname = 'MTD_TL.xml'
        if old_s2_format:
            # find the MTD XML name
            xmlfiles = glob.glob('{}/*.xml'.format(prodid_dir))
            found = False
            for xmlname in xmlfiles:
                if (xmlname.find('OPER_MTD_L1C_TL') != -1):
                    # found the desired XML file
                    tile_xmlname = '{}/{}'.format(granule_dir, tile_xmlname)
                    os.rename(xmlname, tile_xmlname)
                    found = True
                    break

            # make sure the MTD XML file was found
            if not found:
                msg = ('Tile-level XML file was not found.  Looking for {} '
                       'or something similar to S2[A|B]_OPER_MTD_L1C_TL*.xml.'
                       .format(tile_xmlname))
                logger.error(msg)
                return ERROR

        else:
            tile_xmlname = '{}/{}'.format(prodid_dir, tile_xmlname)

        # copy the tile XML file to the ESPA temporary dir
        shutil.copy(tile_xmlname, espa_temp)

        # copy the JPEG2000 image files from GRANULE/{product_id}/IMG_DATA
        # into the temp directory
        imgdir = '{}/IMG_DATA'.format(prodid_dir)
        jp2files = glob.glob('{}/*.jp2'.format(imgdir))
        for jp2 in jp2files:
            shutil.copy(jp2, espa_temp)

        # cleanup all the directories and files except the temp directory
        filelist = glob.glob('*')
        for myfile in filelist:
            if myfile != espa_temp:
                # remove this file or directory (and all contents)
                if os.path.isdir(myfile):
                    shutil.rmtree(myfile)
                elif os.path.isfile(myfile):
                    os.remove(myfile)

        # move the contents of the temp directory to the top level directory
        filelist = glob.glob('{}/*'.format(espa_temp))
        for myfile in filelist:
            shutil.copy(myfile, os.getcwd())

        # remove the temp directory
        shutil.rmtree(espa_temp)

        # successful completion.  return to the original directory.
        os.chdir (mydir)
        msg = 'Completion of Sentinel-2 unpackaging into: {}'.format(s2dir)
        logger.info(msg)
        return SUCCESS

######end of S2SAFE class######

if __name__ == "__main__":
    # setup the default logger format and level. log to STDOUT.
    logging.basicConfig(format=('%(asctime)s.%(msecs)03d %(process)d'
                                ' %(levelname)-8s'
                                ' %(filename)s:%(lineno)d:'
                                '%(funcName)s -- %(message)s'),
                        datefmt='%Y-%m-%d %H:%M:%S',
                        level=logging.INFO)
    sys.exit (S2SAFE().unpackage())
