# vim:set ft=dockerfile:
FROM ubuntu:14.04

MAINTAINER operations@osmfoundation.org

RUN apt-get update -qq && apt-get install -y gcc g++ make autoconf automake libtool \
 libfcgi-dev libxml2-dev libmemcached-dev \
 libboost-regex-dev libboost-program-options-dev libboost-system-dev \
 libboost-date-time-dev libboost-filesystem-dev \
 libpqxx-dev zlib1g-dev --no-install-recommends

WORKDIR /app

# Copy the main application.
COPY . ./

# Compile, install and remove source
RUN ./autogen.sh && ./configure && make install && ldconfig && rm -rf /app

ENV CGIMAP_HOST db
ENV CGIMAP_DBNAME openstreetmap
ENV CGIMAP_USERNAME openstreetmap
ENV CGIMAP_PASSWORD openstreetmap
ENV CGIMAP_PIDFILE /dev/null
ENV CGIMAP_LOGFILE /dev/stdout
ENV CGIMAP_MEMCACHE memcached
ENV CGIMAP_RATELIMIT 204800
ENV CGIMAP_MAXDEBT 250

EXPOSE 8000

CMD ["/usr/local/bin/openstreetmap-cgimap", "--port=8000", "--instances=30"]
