#!/usr/bin/env bash
# use testnet settings,  if you need mainnet,  use ~/.dashcore/vkaxd.pid file instead
export LC_ALL=C

dash_pid=$(<~/.dashcore/testnet3/vkaxd.pid)
sudo gdb -batch -ex "source debug.gdb" vkaxd ${dash_pid}
