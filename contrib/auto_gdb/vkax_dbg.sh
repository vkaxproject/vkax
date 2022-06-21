#!/usr/bin/env bash
# use testnet settings,  if you need mainnet,  use ~/.vkaxcore/vkaxd.pid file instead
export LC_ALL=C

dash_pid=$(<~/.vkaxcore/testnet3/vkaxd.pid)
sudo gdb -batch -ex "source debug.gdb" vkaxd ${dash_pid}
