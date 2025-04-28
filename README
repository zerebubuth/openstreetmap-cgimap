# openstreetmap-cgimap

[![CI](https://github.com/zerebubuth/openstreetmap-cgimap/actions/workflows/docker_bookworm.yml/badge.svg)](https://github.com/zerebubuth/openstreetmap-cgimap/actions/workflows/main.yml) [![CodeQL](https://github.com/zerebubuth/openstreetmap-cgimap/actions/workflows/codeql.yml/badge.svg)](https://github.com/zerebubuth/openstreetmap-cgimap/actions/workflows/codeql.yml)

## Introduction

openstreetmap-cgimap is a C++ application designed to improve the performance of key parts of the OpenStreetMap (OSM) API. It directly interacts with the PostgreSQL backend (APIDB) and bypasses the Rails-based OSM website.

This tool is useful for replacing certain API endpoints with optimized versions to enhance performance. For more details, refer to the [cgimap wiki page](https://wiki.openstreetmap.org/wiki/CGImap) wiki page.

## Key Features

* **Optimized API Endpoints**: cgimap provides 25 performance-enhanced endpoints for interacting with OSM data, including fetching nodes, ways, relations, uploading changesets, and more.

* **Efficient Interaction with PostgreSQL Database**: It connects directly to the PostgreSQL-backed OSM database (APIDB) without requiring the OSM website application.

* **Bulk-enabled database operations**: Mass database operations avoid unnecessary roundtrips for improved performance.


## Prerequisites

Before installing and using cgimap, make sure your system meets the following requirements:
Software Requirements:

* **Operating System**: Linux (tested on Ubuntu 22.04, 24.04, Debian 12, and 13)

* **C++20**: cgimap requires a compiler that supports C++20.

* **PostgreSQL**: A running PostgreSQL server with an APIDB backend. Follow [the Rails website instructions](https://github.com/openstreetmap/openstreetmap-website/blob/master/INSTALL.md) to set up an APIDB backend.

* **Dependencies**: Install the following packages on Ubuntu/Debian:

```bash
    sudo apt-get install libxml2-dev libpqxx-dev libfcgi-dev zlib1g-dev libbrotli-dev \
         libboost-program-options-dev libfmt-dev libmemcached-dev libyajl-dev
```

Note that the full set of packages needed from a fresh install (tested
with Ubuntu 22.04, Debian 12 and 13) - you may already have many or all of these - is:

```bash
    sudo apt-get install git build-essential cmake make
```

For Ubuntu 24.04 users, you might need to build and install libpqxx version
7.10.0 manually due to compatibility issues with C++20. More details can be
found in `docker/ubuntu/Dockerfile2404`.

## Installation

### Clone the Repository

First, clone the cgimap repository to your local machine:

```bash
    git clone https://github.com/zerebubuth/openstreetmap-cgimap.git
    cd openstreetmap-cgimap/
```

### Build openstreetmap-cgimap

Create a build directory and use `cmake` to compile the program:

```bash
    mkdir build
    cd build
    cmake ..
    cmake --build .
```

After the build is complete, you'll have a openstreetmap-cgimap executable.

### Install openstreetmap-cgimap (Optional)

    sudo make install

## Configuration

### Running openstreetmap-cgimap

Once cgimap is built, you can run it with the following command (adjust parameters as needed):

```bash
    ./openstreetmap-cgimap --dbname=openstreetmap --username=user --password=pass \
         --dbport=5432 --socket=:54321 --logfile=/tmp/logfile \
         --daemon --instances=10
```

You can limit access to the FastCGI server by specifying a local IP address or using Unix domain sockets.

### Environment Variables and Config File

You can configure cgimap using environment variables or an INI-style config file.
For example, to set the database parameters, add them to your cgimap.config file:

```ini
dbname=openstreetmap
host=localhost
username=user
password=pass

#update-host=127.0.0.1
#update-dbname=openstreetmap
#ratelimit=100000

# Expert settings (should be left to their default values in most cases)
# see --help for further details
#
#map-area=10
#disable-api-write=
#max-payload=50000000

```

Typically you will need to modify the database connection parameters and path
to the executable. See `./openstreetmap-cgimap --help` for a list of options.
Besides, rate limiting parameters need to be configured according to your needs.

To convert a command line option to an environment variable prepend `CGIMAP_` to
the option, convert hyphens to underscores, and capitalize it.
For example, the option `--moderator-ratelimit` becomes the environment variable
`CGIMAP_MODERATOR_RATELIMIT`.

Run cgimap with the config file:

    ./openstreetmap-cgimap --configfile=/path/to/cgimap.config

### Daemon Mode

To run cgimap as a background service, you can use systemd (for modern systems). See the relevant scripts in the scripts directory.

For systemd:

```bash
sudo cp scripts/cgimap.service /etc/systemd/system/cgimap.service
sudo systemctl enable cgimap
sudo systemctl start cgimap
```

An example of this can be found in
[OSM Chef](https://github.com/openstreetmap/chef/blob/master/cookbooks/web/recipes/cgimap.rb).

### Database Permissions

Ensure the database user has proper permissions to read and update APIDB tables. Refer to the [OSM chef repo](https://github.com/openstreetmap/chef/blob/master/cookbooks/db/recipes/master.rb) for a list of required permissions for cgimap. We recommend using a dedicated database user for cgimap, as well as a dedicated OS user to run cgimap.

## Use with a Web Server

cgimap has to be used with a FastCGI enabled HTTP server like lighttpd, apache2 etc. See the instructions below to use cgimap with lighttpd or apache2.

### Configuring lighttpd as FastCGI proxy

A sample lighttd.conf file is provided, which can be used for testing purposes only. To test cgimap with lighttpd you will need to install lighttpd:

	sudo apt-get install lighttpd

Edit the supplied lighttpd.config file to include your cgimap path and run it with the lighttpd, like:

	/usr/sbin/lighttpd -f lighttpd.conf

You can then access the running instance at `http://localhost:31337/api/0.6/map?bbox=...`


### Configuring Apache as FastCGI proxy

FCGI programs can be deployed with Apache using `mod_fastcgi_handler`,
`mod_fcgid`, `mod_fastcgi`, and on recent versions `mod_proxy_fcgi`.

A sample Apache configuration file that will work in conjunction with cgimap as a
daemon is supplied in `scripts/cgimap.conf`. To use this on a Debian- or Ubuntu-based
system you need to copy the configuration to where Apache will read it and
create an api directory:

```bash
    sudo cp scripts/cgimap.conf /etc/apache2/sites-available/cgimap
    sudo chmod 644 /etc/apache2/sites-available/cgimap
    sudo chown root:root /etc/apache2/sites-available/cgimap
    sudo mkdir /var/www/api
    sudo a2ensite cgimap
    sudo service apache2 restart
```

The apache modules mod_proxy and mod_fastcgi_handler must also be enabled.

By the way, the OSMF _api.openstreetmap.org_ instance runs cgimap as a daemon and Apache with
[mod_proxy_fcgi](https://httpd.apache.org/docs/trunk/mod/mod_proxy_fcgi.html).

## Docker Support

You can build a Docker image using one of the provided Dockerfiles underneath docker/

    docker build -f docker/debian/Dockerfile_bookworm . -t cgimap:bookworm

Once the image is built, you can run cgimap in a container. For more details, refer to the Docker documentation.

https://github.com/zerebubuth/openstreetmap-cgimap/pull/213 has additional configuration details on how to use the cgimap image in a complete development environment, which includes the Rails port, a PostgreSQL DB, lighttpd as reverse proxy, and openstreetmap-cgimap.

## Visual Studio DevContainer Support

If you are using Visual Studio Code, you can set up a DevContainer to develop, build, test and run cgimap in an isolated development environment without worrying about your local dependencies.

### How to Use:

* **Install VS Code**: If you haven't already, install Visual Studio Code.

* **Install Docker**: Make sure Docker is installed and running on your machine, as DevContainers rely on Docker.

* **Open the Repository in VS Code**:

    - Open the cgimap repository in VS Code.
    - If prompted, open the project in a DevContainer.
    - Alternatively, use the _Remote - Containers_ extension to reopen the project inside the DevContainer.

* **Build and Run cgimap**: Once inside the DevContainer, you can follow the usual build steps:

```bash
    mkdir build
    cd build
    cmake ..
    cmake --build .
```

This setup running on Debian Bookworm (like the official OSMF instance) ensures that you have all necessary dependencies inside the container. You can also build and deploy Debian packages using dpkg-buildpackage inside the DevContainer.

Developing on WSL2 is also supported. Start vscode inside WSL2, and continue with opening the repository as described earlier.

## Running Tests

If you'd like to run the test suite, install the required packages:

    sudo apt-get install postgresql-all postgresql-common postgresql-server-dev-all

Then, configure the build to include tests:

    cmake .. -DCMAKE_BUILD_TYPE=Debug

or

    cmake .. -DBUILD_TESTING=ON

Run the tests:

    ctest

Test cases are executed under `pg_virtualenv`, i.e. no further steps are needed to create databases as your user.

To speed up test runs, you can use cmake option `-DENABLE_PGVIRTUALENV=OFF`, then run the tests using:

```
pg_virtualenv ctest
```

This way, the virtual PostgreSQL cluster can be reused across test cases.

<!--
## Code coverage

(TBD)


## Formatting

The cgimap code is formatted using
[clang-format](http://clang.llvm.org/docs/ClangFormat.html) in a style
which is based on the "LLVM style" which ships with
`clang-format`.
<!--
To enable an automatic reformatting option, provide the `--with-clang-format`
option to `configure` and then reformatting can be done across the
whole set of source files by running:

    make clang-format

Ideally, this should be done before committing each set of changes.
-->

## Acknowledgements

cgimap contains code from and is partly based on the following:

* [quad_tile.c](https://github.com/openstreetmap/openstreetmap-website/blob/master/db/functions/quadtile.c)
  by TomH.
* See contrib directory for more details on:
   - [libxml++](https://gitlab.gnome.org/GNOME/libxmlplusplus/) by The libxml++ development team
  - [Catch2](https://github.com/catchorg/Catch2) by Catch2 Authors
  - [SJParser](https://gitlab.com/dhurum/sjparser) by Denis Tikhomirov




