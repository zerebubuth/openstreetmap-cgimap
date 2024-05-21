#!/bin/bash

if [ $# -ne 1 ]; then
  exit 1
fi

cut=0
while read line; do
  if [ "$line" == "[//]: # (start nodoc)" ]; then
    cut=1
  fi
  if [ $cut -eq 0 ]; then
    echo $line
  fi
  if [ "$line" == "[//]: # (end nodoc)" ]; then
    cut=0
  fi
done < $1
