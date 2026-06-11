#!/bin/bash

source /opt/homebrew/Caskroom/miniforge/base/etc/profile.d/conda.sh
conda activate g4env

echo "Starting NIRS batch (1M)..."
mkdir -p sim/run_nirs && cd sim/run_nirs
../build/sim ../macros/batch_nirs_1M.mac > log.txt 2>&1 &
PID1=$!
cd ../..

echo "Starting CD neutron (1M)..."
mkdir -p sim/run_cd && cd sim/run_cd
../build/sim ../macros/neutron_cd_1M.mac > log.txt 2>&1 &
PID2=$!
cd ../..

echo "Starting GD neutron (1M)..."
mkdir -p sim/run_gd && cd sim/run_gd
../build/sim ../macros/neutron_gd_1M.mac > log.txt 2>&1 &
PID3=$!
cd ../..

echo "3 jobs launched (IBD/Cd/Gd). Waiting for them to complete..."
wait $PID1 $PID2 $PID3

echo "Copying output files back to build directory..."
mkdir -p sim/build
cp sim/run_nirs/pe_asymmetry.txt sim/build/pe_asymmetry.txt
cp sim/run_nirs/ibd_positron_spectrum.txt sim/build/ibd_positron_spectrum.txt || true
cp sim/run_cd/pe_asymmetry.txt sim/build/pe_cd.txt
cp sim/run_cd/neutron_lifetime_Cd.txt sim/build/neutron_lifetime_Cd.txt
cp sim/run_gd/pe_asymmetry.txt sim/build/pe_gd.txt
cp sim/run_gd/neutron_lifetime_Gd.txt sim/build/neutron_lifetime_Gd.txt

echo "Running data and plot generation scripts..."
python NIRS/make_plots_data.py
python NIRS/make_pngs.py

echo "ALL DONE!"
