FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -qq && \
    apt-get install -y gcc g++ make cmake cmake-curses-gui gdb less ssh \
       gnupg2 ca-certificates software-properties-common \
       build-essential wget git nano jq valgrind linux-perf \
       gcovr lcov strace ltrace rsync zip sudo \
       tar curl unzip pkg-config bash-completion aria2 \
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
   echo ${REMOTE_USER} ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/${REMOTE_USER}
   chmod 0440 /etc/sudoers.d/${REMOTE_USER}
EOF

ENV HOME /home/${REMOTE_USER}

ENV LANG C.UTF-8
ENV LC_ALL C.UTF-8

USER ${REMOTE_USER}

