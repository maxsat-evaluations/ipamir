#!/usr/bin/env bash

set -eu

SOLVERS=("Aperture" "core-trail" "share-trail" "uwrmaxsat20" "uwrmaxsat20scip")
APPS=("ipamiradaboost" "ipamirextenf" "ipamirbioptsat" "ipamiric3ref" "ipamirwcnfi" "ipamirsibyltrace")

module purge
module load \
  GCC/14.3.0 \
  GMP/6.3.0-GCCcore-14.3.0 \
  zlib/1.3.1-GCCcore-14.3.0 \
  Autoconf/2.72-GCCcore-14.3.0 \
  CMake/4.3.0-GCCcore-14.3.0 \
  MPFR/4.2.2-GCCcore-14.3.0

make -C sat/minisat220

mkdir -p bin/
for app in "${APPS[@]}"; do
  for slv in "${SOLVERS[@]}"; do
    make -C maxsat/"$slv"
    rm app/"$app"/"$app"
    IPAMIRSOLVER="$slv" make -C app/"$app"
    cp app/"$app"/"$app" bin/"$app-$slv"
  done
done
