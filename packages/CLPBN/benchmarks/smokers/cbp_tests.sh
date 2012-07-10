#!/bin/bash

source sm.sh
source ../benchs.sh

SOLVER="cbp"

function run_all_graphs
{
  write_header $1
  run_solver pop100   $2
  run_solver pop200   $2
  run_solver pop300   $2
  run_solver pop400   $2
  run_solver pop500   $2
  run_solver pop600   $2
  run_solver pop700   $2
  run_solver pop800   $2
  run_solver pop900   $2
  run_solver pop1000  $2
  run_solver pop1100  $2
  run_solver pop1200  $2
  run_solver pop1300  $2
  run_solver pop1400  $2
  run_solver pop1500  $2
}

prepare_new_run
run_all_graphs "cbp(shedule=seq_fixed)                " seq_fixed

