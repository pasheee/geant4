#!/bin/bash

source /opt/homebrew/Caskroom/miniforge/base/etc/profile.d/conda.sh
conda activate g4env

echo "Starting NIRS batch (100k)..."
mkdir -p sim/run_nirs && cd sim/run_nirs
../build/sim ../macros/batch_nirs.mac > log.txt 2>&1 &
PID1=$!
cd ../..

echo "Starting CD neutron (100k)..."
mkdir -p sim/run_cd && cd sim/run_cd
../build/sim ../macros/neutron_cd.mac > log.txt 2>&1 &
PID2=$!
cd ../..

echo "Starting GD neutron (100k)..."
mkdir -p sim/run_gd && cd sim/run_gd
../build/sim ../macros/neutron_gd.mac > log.txt 2>&1 &
PID3=$!
cd ../..

echo "Starting P10 positrons (100k)..."
mkdir -p sim/run_p10 && cd sim/run_p10
../build/sim ../macros/pos_p10.mac > log.txt 2>&1 &
PID4=$!
cd ../..

echo "Starting P50 positrons (100k)..."
mkdir -p sim/run_p50 && cd sim/run_p50
../build/sim ../macros/pos_p50.mac > log.txt 2>&1 &
PID5=$!
cd ../..

echo "Starting P90 positrons (100k)..."
mkdir -p sim/run_p90 && cd sim/run_p90
../build/sim ../macros/pos_p90.mac > log.txt 2>&1 &
PID6=$!
cd ../..

echo "All 6 jobs launched. Waiting for them to complete..."
wait $PID1 $PID2 $PID3 $PID4 $PID5 $PID6

echo "Copying output files back to build directory..."
mkdir -p sim/build
cp sim/run_nirs/pe_asymmetry.txt sim/build/pe_asymmetry.txt
cp sim/run_nirs/ibd_positron_spectrum.txt sim/build/ibd_positron_spectrum.txt || true
cp sim/run_nirs/ibd_kinematics.txt sim/build/ibd_kinematics.txt || true
cp sim/run_cd/pe_asymmetry.txt sim/build/pe_cd.txt
cp sim/run_cd/neutron_lifetime_Cd.txt sim/build/neutron_lifetime_Cd.txt
cp sim/run_gd/pe_asymmetry.txt sim/build/pe_gd.txt
cp sim/run_gd/neutron_lifetime_Gd.txt sim/build/neutron_lifetime_Gd.txt
cp sim/run_p10/photons_p10.txt sim/build/photons_p10.txt
cp sim/run_p50/photons_p50.txt sim/build/photons_p50.txt
cp sim/run_p90/photons_p90.txt sim/build/photons_p90.txt

echo "Running data and plot generation scripts..."
python NIRS/make_plots_data.py
python NIRS/make_pngs.py

echo "ALL DONE!"
