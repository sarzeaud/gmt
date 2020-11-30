#!/usr/bin/env bash
#
# Bash script to install GMT dependencies on Windows via vcpkg and chocolatey
#
# Environmental variables that can control the installation:
#
# - BUILD_DOCS: Build GMT documentation  [false]
# - RUN_TESTS:  Run GMT tests            [false]
# - PACKAGE:    Create GMT packages      [false]
#

# set defaults to false
BUILD_DOCS="${BUILD_DOCS:-false}"
RUN_TESTS="${RUN_TESTS:-false}"
PACKAGE="${PACKAGE:-false}"


# By default, vcpkg builds both release and debug configurations.
# Set VCPKG_BUILD_TYPE to build release only to save half time
echo 'set (VCPKG_BUILD_TYPE release)' >> ${VCPKG_ROOT}/triplets/x64-windows.cmake
cat ${VCPKG_ROOT}/triplets/x64-windows.cmake

# install libraries
$VCPKG_ROOT/vcpkg install netcdf-c gdal pcre2 fftw3[core,threads] clapack openblas --triplet x64-windows

vcpkg list

# install more packages using chocolatey
choco install ninja
choco install ghostscript --version 9.50

if [ "$BUILD_DOCS" = "true" ]; then
  choco install pngquant
  pip install --user sphinx
fi

if [ "$RUN_TESTS" = "true" ]; then
  choco install graphicsmagick
  choco install ffmpeg
fi
