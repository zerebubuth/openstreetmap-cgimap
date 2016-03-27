#!/bin/bash

apt-get install -y git build-essential automake autoconf libtool;

apt-get install -y libxml2-dev libpqxx-dev libfcgi-dev \
  libboost-dev libboost-regex-dev libboost-program-options-dev \
  libboost-date-time-dev libboost-filesystem-dev \
  libboost-system-dev libmemcached-dev;

# following stuff is for testing only
apt-get install -y postgresql postgresql-contrib postgis \
  ruby-libxml ruby-pg
sudo -u postgres createuser -s vagrant
