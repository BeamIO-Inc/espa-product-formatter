
# This spec file can be used to build an RPM package for installation.
# **NOTE**
#     Version, Release, and tagname information should be updated for the
#     particular release to build an RPM for.

# ----------------------------------------------------------------------------
Name:		espa-product-formatter
Version:	201605
Release:	2%{?dist}
Summary:	ESPA Product Formatting Software

Group:		ESPA
License:	Nasa Open Source Agreement
URL:		https://github.com/USGS-EROS/espa-product-formatter.git

BuildRoot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
BuildArch:	x86_64
Packager:	USGS EROS LSRD


# ----------------------------------------------------------------------------
%description
Provides executables for converting from input formats to our internal format, as well as, converting from the internal format to the output formats.  This application also provided for generating the land water mask.  These applications are implementated in C and are statically built.


# ----------------------------------------------------------------------------
# Specify the repository tag/branch to clone and build from
%define tagname dev_v1.7.0
# Specify the name of the directory to clone into
%define clonedname %{name}-%{tagname}


# ----------------------------------------------------------------------------
# Turn off the brp-python-bytecompile script
%global __os_install_post %(echo '%{__os_install_post}' | sed -e 's!/usr/lib[^[:space:]]*/brp-python-bytecompile[[:space:]].*$!!g')


# ----------------------------------------------------------------------------
%prep
# We don't need to perform anything here


# ----------------------------------------------------------------------------
%build

# Start with a clean clone of the repo
rm -rf %{clonedname}
git clone --depth 1 --branch %{tagname} %{url} %{clonedname}
# Build the applications
cd %{clonedname}
make BUILD_STATIC=yes


# ----------------------------------------------------------------------------
%install
# Start with a clean installation location
rm -rf %{buildroot}
# Install the applications for a specific path
cd %{clonedname}
make install PREFIX=%{buildroot}/usr/local

# ----------------------------------------------------------------------------
%clean
# Cleanup our cloned repository
rm -rf %{clonedname}
# Cleanup our installation location
rm -rf %{buildroot}


# ----------------------------------------------------------------------------
%files
%defattr(-,root,root,-)
# All sub-directories are automatically included
/usr/local/bin/*
/usr/local/include/*
/usr/local/lib/*
/usr/local/python/*
/usr/local/schema/*
/usr/local/%{name}


# ----------------------------------------------------------------------------
%changelog
* Wed Apr 27 2016 Ronald D Dilley <rdilley@usgs.gov>
- Updated for release number

* Tue Apr 12 2016 Ronald D Dilley <rdilley@usgs.gov>
- Updated for May 2016 release
* Mon Mar 07 2016 Ronald D Dilley <rdilley@usgs.gov>
- Revisioned for a bug in the generated libraries
* Thu Mar 03 2016 Ronald D Dilley <rdilley@usgs.gov>
- Revisioned for a bug in the generated libraries
* Wed Feb 10 2016 Ronald D Dilley <rdilley@usgs.gov>
- Revisioned for a bug in the metadata_api.
* Tue Feb 09 2016 Ronald D Dilley <rdilley@usgs.gov>
- Updated for Mar 2016 release.  Change the package name.
* Wed Nov 18 2015 William D Howe <whowe@usgs.gov>
- New version release 1.5.1.
* Fri Jun 26 2015 William D Howe <whowe@usgs.gov>
- Changed to git repo and cleaned up comments.
* Thu Apr 28 2015 Cory B Turner <cbturner@usgs.gov>
- Rebuild to 1.4.0 to capture changes to espa-common for May 2015 release
* Fri Jan 16 2015 Adam J Dosch <adosch@usgs.gov>
- Rebuild to 1.3.1 to make correction for Landsat 4-7 QA band descriptions and fill value
* Mon Dec 22 2014 Adam J Dosch <adosch@usgs.gov>
- Rebuild to 1.3.0 to fix espa-common release from August 2014, 1.2.0 will not exist anymore as a release
* Mon Dec 07 2014 Adam J Dosch <adosch@usgs.gov>
- Rebuild to capture changes to espa-common for December 2014 release
* Wed Sep 24 2014 Adam J Dosch <adosch@usgs.gov>
- Build for September 2014 release to capture l8_sr updates
* Mon Aug 25 2014 Adam J Dosch <adosch@usgs.gov>
- Rebuild to capture schema changes for August 2014 release
* Thu Aug 21 2014 Adam J Dosch <adosch@usgs.gov>
- Build for August 2014 release
- Adding svnrelease to go with the new subversion testing/releases structure vs. using tags 
- Updated Release conditional macro to expand if exists, if non-exists must be broke?
* Fri Jul 24 2014 Adam J Dosch <adosch@usgs.gov>
- Rebuild of espa-common for miscellaneous bugfixes
* Fri Jul 18 2014 Adam J Dosch <adosch@usgs.gov>
- Merging RHEL5 and 6 changes together to maintain one spec file
* Mon Jul 14 2014 Adam J Dosch <adosch@usgs.gov>
- Rebuild to capture some newly added espa-common changes for 2.4.0 release
* Tue Jun 10 2014 Adam J Dosch <adosch@usgs.gov>
- Adding logic to build more areas of espa-common for raw-binary transition for July 2014 release
* Thu Nov 07 2013 Adam J Dosch <adosch@usgs.gov>
- Rebuild for version 1.0.0 for November 2013 release
- Initial spec file generation for espa-common
