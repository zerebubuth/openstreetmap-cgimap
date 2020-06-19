#!/bin/bash

apt-get update

apt-get install -y git build-essential automake autoconf libtool

apt-get install -y libboost-dev libboost-filesystem-dev libboost-locale-dev \
                   libboost-program-options-dev libboost-system-dev \
                   libcrypto++-dev libfcgi-dev libmemcached-dev libpqxx-dev \
                   libxml2-dev libyajl-dev

# following stuff is for testing only
apt-get install -y postgresql postgresql-contrib postgis
sudo -u postgres createuser -s vagrant
