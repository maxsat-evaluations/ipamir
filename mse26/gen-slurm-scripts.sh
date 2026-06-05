#!/usr/bin/env bash

set -euo pipefail

SOLVERS=("Aperture" "core-trail" "share-trail" "uwrmaxsat20" "uwrmaxsat20scip")
APPS=("ipamiradaboost" "ipamirextenf" "ipamirbioptsat" "ipamiric3ref" "ipamirwcnfi" "ipamirsibyltrace")

mkdir -p slurm-scripts
for app in "${APPS[@]}"; do
  for slv in "${SOLVERS[@]}"; do
    ninsts=$(cat "$app-inputs" | wc -l)
    sed \
      -e "s/!app!/$app/g" \
      -e "s/!solver!/$slv/g" \
      -e "s/!solver!/$slv/g" \
      -e "s/!ninsts!/$ninsts/g" \
      slurm-template.sh > slurm-scripts/"$app-$slv.sh"
  done
done
