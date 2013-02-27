#!/bin/bash

aclocal -I aclocal
autoheader
autoconf
automake -a
