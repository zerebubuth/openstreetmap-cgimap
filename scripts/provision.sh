#!/bin/bash

apt-get install -y git build-essential automake autoconf libtool;

apt-get install -y libxml2-dev libpqxx-dev libfcgi-dev \
  libboost-dev libboost-regex-dev libboost-program-options-dev \
  libboost-date-time-dev libboost-filesystem-dev \
  libboost-system-dev libboost-locale-dev libmemcached-dev \
  libcrypto++-dev;

# following stuff is for testing only
apt-get install -y postgresql postgresql-contrib postgis
sudo -u postgres createuser -s vagrant
