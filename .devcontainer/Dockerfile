FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -qq && \
    apt-get install -y gcc g++ make cmake cmake-curses-gui gdb less ssh \
       libfcgi-dev libxml2-dev libmemcached-dev libbrotli-dev \
       libboost-program-options-dev libcrypto++-dev libyajl-dev \
       libpqxx-dev zlib1g-dev libfmt-dev \
       postgresql-15 postgresql-server-dev-all dpkg-dev file \
       clang-tidy cppcheck ninja-build locales \
       --no-install-recommends && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

ARG REMOTE_USER
ARG REMOTE_UID
ARG REMOTE_GID
RUN <<EOF
   addgroup --gid ${REMOTE_GID} ${REMOTE_USER}
   adduser --disabled-password --uid ${REMOTE_UID} --gid ${REMOTE_GID} ${REMOTE_USER}
EOF

ENV HOME /home/${REMOTE_USER}

ENV LANG C.UTF-8
ENV LC_ALL C.UTF-8

USER ${REMOTE_USER}

