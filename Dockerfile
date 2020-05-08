FROM eroslab.cr.usgs.gov:4567/lsrd/espa-worker:builder-latest

MAINTAINER USGS EROS LSRD http://eros.usgs.gov

LABEL description="Provides an espa-worker build environment with the espa-product-formatter installed"

ENV PREFIX=/usr/local

COPY . /source

RUN cd /source \
    && make ENABLE_THREADING=yes BUILD_STATIC=yes \
    && make install PREFIX=$PREFIX \
    && make clean

COPY ./bin/test_image.sh /source/test_image.sh
