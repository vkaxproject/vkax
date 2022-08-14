#!/usr/bin/env bash
# use testnet settings,  if you need mainnet,  use ~/.jagoancore/jgcd.pid file instead
export LC_ALL=C

dash_pid=$(<~/.jagoancore/testnet3/jgcd.pid)
sudo gdb -batch -ex "source debug.gdb" jgcd ${dash_pid}
