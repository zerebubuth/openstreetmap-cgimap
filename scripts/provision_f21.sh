#!/bin/bash

yum install -y git gcc gcc-c++ make automake autoconf libtool;

yum install -y libxml2-devel libpqxx-devel fcgi-devel \
  boost-devel libmemcached-devel;

# following stuff is for testing only
yum install -y postgresql postgresql-contrib postgis \
  postgresql-server

sudo systemctl enable postgresql
sudo postgresql-setup initdb
sudo systemctl start postgresql
sudo -u postgres -i createuser -s vagrant
