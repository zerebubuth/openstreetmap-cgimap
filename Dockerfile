# vim:set ft=dockerfile:
FROM ubuntu:20.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -qq && \
    apt-get install -y gcc g++ make autoconf automake libtool \
       libfcgi-dev libxml2-dev libmemcached-dev \
       libboost-program-options-dev libboost-system-dev \
       libboost-filesystem-dev libboost-locale-dev libcrypto++-dev libyajl-dev \
       libpqxx-dev zlib1g-dev libargon2-dev \
       postgresql-12 postgresql-server-dev-all \
       --no-install-recommends && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the main application.
COPY . ./

# Compile, install and remove source
RUN ./autogen.sh && \
    ./configure --enable-static --disable-shared --enable-yajl && \
    make && \
    make check && \    
    strip openstreetmap-cgimap

FROM ubuntu:20.04

RUN apt-get update -qq && \
    apt-get install -y \
       libfcgi-bin libmemcached11 libboost-locale1.71.0 libboost-program-options1.71.0 \
       libxml2 libcrypto++6 libyajl2 libpqxx-6.4 zlib1g argon2 \
       --no-install-recommends && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

COPY --from=builder /app/openstreetmap-cgimap /usr/local/bin

RUN groupadd -g 61000 cgimap && \
    useradd -g 61000 -l -M -s /bin/false -u 61000 cgimap

USER cgimap

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
