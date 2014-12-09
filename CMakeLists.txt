cmake_minimum_required(VERSION 2.6)
project(maxpower)

message(STATUS "MAXCOMPILERDIR: $ENV{MAXCOMPILERDIR}")
message(STATUS "MAXELEROSDIR  : $ENV{MAXELEROSDIR}")

set(SLICCOMPILE $ENV{MAXCOMPILERDIR}/bin/sliccompile)

add_subdirectory(src/maxpower/hash)

set(CPACK_PACKAGE_VERSION "0.1.0")
set(CPACK_GENERATOR "TGZ")
include(CPack)