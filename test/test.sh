#!/usr/bin/env bash

set -e
HERE=`dirname $0`

# test read-write backend
echo "=== Testing read-write ==="
$HERE/test-single.sh $1

# test read-only backend
echo "=== Testing read-only ==="
$HERE/test-single.sh $1 --readonly
