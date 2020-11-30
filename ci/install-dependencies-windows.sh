#
#  Batch script to install GMT dependencies on Windows via vcpkg and chocolatey
#

echo 'set (VCPKG_BUILD_TYPE release)' >> ${VCPKG_INSTALLATION_ROOT}/triplets/x64-windows.cmake
vcpkg install netcdf-c gdal pcre2 fftw3[core,threads] clapack openblas --triplet x64-windows
