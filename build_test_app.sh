#!/bin/sh
set -ex
CC="${CC:-cc}"

$CC $CFLAGS -DMYPATH_BUILD_TEST_APP -ldl -o mypath mypath.c
$CC $CFLAGS -DMYPATH_BUILD_TEST_APP -DMYPATH_DISABLE_DLADDR -static -o mypath_static mypath.c
