#!/bin/bash

aclocal -I aclocal
autoheader
libtoolize -i
automake -a
autoconf
