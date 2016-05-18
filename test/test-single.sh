#!/usr/bin/env bash

set -e
TMPDIR=`mktemp -d`
CGIMAP_PIDFILE=$TMPDIR/cgimap.pid
CGIMAP_SOCKET=$TMPDIR/cgimap.sock
MAPSCRIPT=$TMPDIR/map.sh
BUILD_DIR=`dirname \`dirname $0\``
TMPDB="openstreetmap_${RANDOM}_$$"

# function to clean up temporary stuff created for this test
# and reset everything back to a usable level.
function clean_up() {
    if [[ -d "$TMPDIR" ]]; then
	if [[ -f "$CGIMAP_PIDFILE" ]]; then
	    # cgimap traps SIGTERM, but would need another
	    # request to break it out of the loop, so we
	    # just kill it with extreme prejudice.
	    kill -KILL `cat $CGIMAP_PIDFILE`
	else
	    echo "pidfile $CGIMAP_PIDFILE doesn't exist!"
	    killall openstreetmap-cgimap
	fi
	rm -f "$CGIMAP_PIDFILE"
	rm -f "$CGIMAP_SOCKET"

	db_count=`psql -t -c "\\l" postgres | grep -c "$TMPDB"`
	if [[ $db_count -ne 0 ]]; then
	    dropdb "$TMPDB"
	fi

	if [[ $1 -ne 0 ]]; then
	    echo "== map.log =="
	    cat "$TMPDIR/map.log"
	    echo "== out.log =="
	    cat "$TMPDIR/out.log"
	fi
	rm -rf "$TMPDIR"
    fi
}

# function to clean up, when cleanup was unexpected. this returns 
# with a status code that automake's system will interpret as a
# "hard error".
function trapped_clean_up() {
    clean_up 1
    exit 99
}

# set up a trap to clean up when the script exits unexpectedly.
trap trapped_clean_up EXIT

# create a script which will run cgimap with all the arguments
# we want, as cgi-fcgi isn't capable of passing extra arguments
# (or indeed preserving the environment) for cgimap to get its
# parameters.
cat <<EOF > $MAPSCRIPT
#!/bin/bash
ulimit -c unlimited
cd "$TMPDIR"
"$PWD/$BUILD_DIR/openstreetmap-cgimap" $2 --dbname "$TMPDB" --pidfile "$CGIMAP_PIDFILE" --logfile "$TMPDIR/map.log" > "$TMPDIR/out.log" 2>&1
echo "Return value: \$?" >> "$TMPDIR/out.log"
EOF
chmod +x "$MAPSCRIPT"

# create a temporary database and fill it with the development
# structure of the current rails_port code.
createdb -O $USER -E UTF-8 "$TMPDB" "A test database for CGImap."
psql -q -f "$BUILD_DIR/test/structure.sql" "$TMPDB"

# start the map program with fcgi - this will leave the map
# handler running, but we have to get its PID to be able to
# shut it down.
cgi-fcgi -start -connect "$CGIMAP_SOCKET" "$TMPDIR/map.sh"

# call the test program with the socket and temporary database
# as arguments
$1 "$CGIMAP_SOCKET" "$TMPDB"

# if the test program didn't exit with a bad status then i
# guess we passed the test.
trap - EXIT
clean_up 0
exit 0
