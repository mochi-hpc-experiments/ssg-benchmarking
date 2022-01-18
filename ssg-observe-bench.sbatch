#!/bin/bash
#SBATCH -A csc332
#SBATCH -J ssg-observe
#SBATCH -o %x-%j.out
#SBATCH -t 00:40:00
#SBATCH -N 4


set -euo pipefail

WORKDIR=$(pwd)
OUTPUTDIR=/gpfs/alpine/scratch/${USER}/csc332
SSG_STATE=${OUTPUTDIR}/ssg-bench.out
cd $OUTPUTDIR

srun -N 1 -n 1 ${WORKDIR}/ssg-launch-group -s $((60)) -f ${SSG_STATE} -n scale-test 'ofi+cxi://' mpi &

echo "observing group"
srun -N 3 -n 30 --ntasks-per-node=10 ${WORKDIR}/ssg-observe-group 'ofi+cxi://' ${SSG_STATE}
