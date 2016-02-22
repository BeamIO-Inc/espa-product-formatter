
"""
License:
    NASA Open Source Agreement 1.3
"""

import os
import sys
import logging
import urllib2


from lxml import objectify as objectify


from espa_xml_interface import XMLError, XMLInterface


class ESPAMetadataError(Exception):
    """Error Exception for ESPAMetadata"""
    pass


class ESPAMetadata(XMLInterface):
    """Enhances the XMLInterface specifically from an ESPA Metadata XML

    TODO TODO TODO
    Implements a class to wrap lxml functionality to provide validation of the
    XML document, as well as provide access to an lxml.objectify object for
    manipulation of the XML document.  Also parsing and writing from disk.

    Attributes:
            TODO TODO TODO
    """

    def __init__(self, xml_filename=None):
        """Object initialization and parsing of the XML document

        Creates an lxml schema from the specified XSD and creates a lxml
        parser from the schema to be used during parsing and validation.

        Args:
            xml_xsd (str): The XSD to use for validation.
            xml_filename (str) The name of the file to parse.

        Raises:
            XMLError: An error occurred using the lxml module.
        """

        # Get the logger to use
        self.logger = logging.getLogger('espa.processing')
        # Just in-case one was not defined
        self.logger.addHandler(logging.NullHandler())

        xsd_version = '1_3'
        xsd_filename = 'espa_internal_metadata_v{0}.xsd'.format(xsd_version)
        xsd_uri = ('http://espa.cr.usgs.gov/schema/{0}'.format(xsd_filename))

        # Create a schema object from the metadata xsd source
        xml_xsd = None
        # Search for the environment variable and use that if valid (first)
        if 'ESPA_SCHEMA' in os.environ:
            xsd_path = os.getenv('ESPA_SCHEMA')

            if os.path.isfile(xsd_path):
                with open(xsd_path, 'r') as xsd_fd:
                    xml_xsd = xsd_fd.read()
                self.logger.info('Using XSD source {0} for validation'
                                 .format(xsd_path))
            else:
                self.logger.info('Defaulting to espa-product-formatter'
                                 ' installation directory')
                xml_xsd = None
        else:
            self.logger.warning('Missing environment variable ESPA_SCHEMA'
                                ' defaulting to espa-product-formatter'
                                ' installation directory')
            xml_xsd = None

        # Use the espa-product-formatter installation directory (second)
        if xml_xsd is None:
            xsd_path = ('/usr/local/espa-product-formatter/schema/{0}'
                        .format(xsd_filename))
            if os.path.isfile(xsd_path):
                with open(xsd_path, 'r') as xsd_fd:
                    xml_xsd = xsd_fd.read()
                self.logger.info('Using XSD source {0} for validation'
                                 .format(xsd_path))
            else:
                self.logger.info('Defaulting to {0}'.format(xsd_uri))
                xml_xsd = None

        # Use the schema_uri (third)
        if xml_xsd is None:
            with urllib2.urlopen(xsd_uri) as xsd_fd:
                xml_xsd = xsd_fd.read()
            self.logger.info('Using schema source {0} for validation'
                             .format(xsd_uri))

        # (fail)
        if xml_xsd is None:
            raise ESPAMetadataError('Failed to find ESPA XML schema for'
                                    ' validation')

        super(ESPAMetadata, self).__init__(xml_xsd=xml_xsd,
                                           xml_filename=xml_filename)
