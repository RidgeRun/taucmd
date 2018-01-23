#!/bin/sh
# you can either set the environment variables AUTOCONF, AUTOHEADER, AUTOMAKE,
# ACLOCAL, AUTOPOINT and/or LIBTOOLIZE to the right versions, or leave them
# unset and get the defaults

( \
   mkdir -p m4 && \
   aclocal && \
   autoreconf --verbose --force --install && \
   ./configure && \
   make \
) || {
 echo 'autogen.sh failed';
 exit 1;
}
