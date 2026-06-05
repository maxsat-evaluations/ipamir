#!/bin/bash -l
#SBATCH -J mse26incr:!solver!:!app!
#SBATCH -o /wrk-kappa/users/chrisjab/experiment-runs/mse26incr/out/!solver!_!app!_%a.out
#SBATCH -e /wrk-kappa/users/chrisjab/experiment-runs/mse26incr/err/!solver!_!app!_%a.err
#SBATCH -t 70
#SBATCH -p short
#SBATCH --account=cs_ukko2
#SBATCH --exclusive=user
#SBATCH --mem=32768
#SBATCH --cpus-per-task=10
#SBATCH --array=1-!ninsts!
#SBATCH --chdir=/wrk-kappa/users/chrisjab
#SBATCH --hint=nomultithread
#SBATCH --constraint=k8

module purge
module load \
  GCC/14.3.0 \
  GMP/6.3.0-GCCcore-14.3.0 \
  zlib/1.3.1-GCCcore-14.3.0 \
  Autoconf/2.72-GCCcore-14.3.0 \
  CMake/4.3.0-GCCcore-14.3.0 \
  MPFR/4.2.2-GCCcore-14.3.0

file=$(sed -n "${SLURM_ARRAY_TASK_ID}"p "/home/chrisjab/ipamir/mse26/!app!-inputs")
inst=$(basename "$file")

LOG_DIR="/wrk-kappa/users/chrisjab/experiment-runs/mse26incr/results/"
mkdir -p "${LOG_DIR}"
mkdir -p "${TMPDIR}/${USER}"

LOG_FILE="${LOG_DIR}/!solver!.!app!.${inst}"
TMP_FILE="${TMPDIR}/${USER}/!solver!.!app!.${inst}"

RUNSOLVER=/home/group/grp-cs-coreo/bin/runsolver
BIN=/home/chrisjab/ipamir/bin/!app!-!solver!

if [ ! -f ${LOG_FILE}.out ]; then
    echo "running '${BIN} ${file}'"
    ${RUNSOLVER} -d 300 -o "${TMP_FILE}.out" -v "${TMP_FILE}.var" -w "${TMP_FILE}.wat" -C 3600 -M 32768 "${BIN}" "${file}"
    echo "# generate-jobs.py info" >> ${TMP_FILE}.var
    echo "HOSTNAME=$(hostname)" >> ${TMP_FILE}.var
    echo "SLURM_ARRAY_TASK_ID=${SLURM_ARRAY_TASK_ID}" >> ${TMP_FILE}.var
    mv ${TMP_FILE}.out ${LOG_FILE}.out
    mv ${TMP_FILE}.var ${LOG_FILE}.var
    mv ${TMP_FILE}.wat ${LOG_FILE}.wat
fi
