#!/bin/bash

convert_lpgs_to_espa --help

files=(
      /usr/local/bin/convert_espa_to_bip
      /usr/local/bin/convert_espa_to_gtif
      /usr/local/bin/convert_espa_to_hdf
      /usr/local/bin/convert_espa_to_netcdf
      /usr/local/bin/convert_lpgs_to_espa
      /usr/local/bin/convert_modis_to_espa
      /usr/local/bin/convert_sentinel_to_espa
      /usr/local/bin/convert_viirs_to_espa
      /usr/local/include/convert_espa_to_gtif.h
      /usr/local/include/convert_espa_to_hdf.h
      /usr/local/include/convert_espa_to_netcdf.h
      /usr/local/include/convert_espa_to_raw_binary_bip.h
      /usr/local/include/convert_lpgs_to_espa.h
      /usr/local/include/convert_modis_to_espa.h
      /usr/local/include/convert_sentinel_to_espa.h
      /usr/local/include/convert_viirs_to_espa.h
      /usr/local/espa-product-formatter/raw_binary/bin/convert_espa_to_bip
      /usr/local/espa-product-formatter/raw_binary/bin/convert_espa_to_gtif
      /usr/local/espa-product-formatter/raw_binary/bin/convert_espa_to_hdf
      /usr/local/espa-product-formatter/raw_binary/bin/convert_espa_to_netcdf
      /usr/local/espa-product-formatter/raw_binary/bin/convert_lpgs_to_espa
      /usr/local/espa-product-formatter/raw_binary/bin/convert_modis_to_espa
      /usr/local/espa-product-formatter/raw_binary/bin/convert_sentinel_to_espa
      /usr/local/espa-product-formatter/raw_binary/bin/convert_viirs_to_espa
      /usr/local/espa-product-formatter/raw_binary/include/convert_espa_to_gtif.h
      /usr/local/espa-product-formatter/raw_binary/include/convert_espa_to_hdf.h
      /usr/local/espa-product-formatter/raw_binary/include/convert_espa_to_netcdf.h
      /usr/local/espa-product-formatter/raw_binary/include/convert_espa_to_raw_binary_bip.h
      /usr/local/espa-product-formatter/raw_binary/include/convert_lpgs_to_espa.h
      /usr/local/espa-product-formatter/raw_binary/include/convert_modis_to_espa.h
      /usr/local/espa-product-formatter/raw_binary/include/convert_sentinel_to_espa.h
      /usr/local/espa-product-formatter/raw_binary/include/convert_viirs_to_espa.h
)

for i in "${files[@]}"; do
    if [ ! -f "$i" ]; then
      echo "Missing expected file $i"
      exit 1
    else
      echo "Expected file found $i"
    fi
done
