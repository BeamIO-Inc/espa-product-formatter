
# This spec file can be used to build an RPM package for installation.
# **NOTE**
#     Version, Release, and tagname information should be updated for the
#     particular release to build an RPM for.

# ----------------------------------------------------------------------------
Name:		espa-schemas
# This version represents the schema version, and not the
# espa-product-formatter version.
Version:	1.3.0
Release:	1%{?dist}
Summary:	ESPA Metadata Schemas

Group:		ESPA
License:	Nasa Open Source Agreement
URL:		https://github.com/USGS-EROS/espa-product-formatter.git

BuildRoot:	%(mktemp -ud %{_tmppath}/${name}-%{version}-%{release}-XXXXXX)
BuildArch:	x86_64
Packager:	USGS EROS LSRD


# ----------------------------------------------------------------------------
%description
Provides the schemas used by ESPA processing.


# ----------------------------------------------------------------------------
# Specify the repository tag/branch to clone and build from
%define tagname dev_v1.6.0
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
# Nothing to build for the schemas


# ----------------------------------------------------------------------------
%install
# Start with a clean installation location
rm -rf %{buildroot}
# Install the applications for a specific path
cd %{clonedname}
make install-schema PREFIX=%{buildroot}/usr/local

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
/usr/local/schema/*
/usr/local/espa-product-formatter/schema


# ----------------------------------------------------------------------------
%changelog
* Wed Feb 10 2016 Ronald D Dilley <rdilley@usgs.gov>
- New RPM for Mar 2016 release.

