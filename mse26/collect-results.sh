#!/usr/bin/env bash

set -euo pipefail

SOLVERS=("Aperture" "core-trail" "share-trail" "uwrmaxsat20" "uwrmaxsat20scip")
APPS=("ipamiradaboost" "ipamirextenf" "ipamirbioptsat" "ipamiric3ref" "ipamirwcnfi" "ipamirsibyltrace")

INPUTS_DIR="/home/chrisjab/ipamir/mse26"
LOG_DIR="/wrk-kappa/users/chrisjab/experiment-runs/mse26incr/results"

# CSV Header
echo -n "app,input"
for slv in "${SOLVERS[@]}"; do
  echo -n ",$slv"
done
echo

for app in "${APPS[@]}"; do
  for inst in $(cat "$INPUTS_DIR/$app-inputs"); do
    inst="$(basename "$inst")"
    echo -n "$app,$inst"
    for slv in "${SOLVERS[@]}"; do
      LOG="$LOG_DIR/$slv.$app.$inst"
      if [ ! -f "$LOG.wat" ]; then
        # hard slurm timeout causes no log files to be present
        echo -n ",timeout"
      elif grep -q "Maximum VSize exceeded" "$LOG.wat"; then
        echo -n ",memout"
      elif grep -q "OutOfMemoryException" "$LOG.out"; then
        echo -n ",memout"
      elif grep -q "Child dumped core" "$LOG.wat"; then
        echo -n ",error"
      elif grep -q "Maximum CPU time exceeded: sending SIGTERM then SIGKILL" "$LOG.wat"; then
        echo -n ",timeout"
      else
        echo -n ",$(grep "^CPUTIME=" "$LOG.var" | cut -d'=' -f2)"
      fi
    done
    echo
  done
done
