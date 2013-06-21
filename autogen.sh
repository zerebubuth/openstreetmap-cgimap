#!/bin/bash

aclocal -I aclocal
autoheader
automake -a
autoconf
