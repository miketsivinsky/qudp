#!/bin/sh

#@echo off

#******  test config
device_emulator=1
useRx=1
useTx=0

#****** IP list
LocalHost=127.0.0.1
HostIP=192.168.10.2
DeviceIP=192.168.10.1

#****** common part
PacketSize=1472
PacketInBuf=64
PacketGenType=1

#****** rx/tx part dependent from device_emulator mode
if [[ $device_emulator -eq 1 ]]; then 
	TxHostIP=$HostIP
	TxPeer=$DeviceIP
	RxHostIP=$HostIP
else 
	TxHostIP=$DeviceIP
	TxPeer=$HostIP
	RxHostIP=$DeviceIP
fi

#****** tx part
TxBufNum=1000000
TxDelay=0
TxPort=50012

#****** rx part
RxBufNum=100000
RxPort=50012


#********************************************

killall test_tx
killall test_rx

TxPrg=../../bin/release/test_tx
RxPrg=../../bin/release/test_rx

if [[  $useRx -eq 1 ]]; then
    echo "Rx..."
	($RxPrg $RxBufNum $RxHostIP $RxPort $PacketSize $PacketInBuf $PacketGenType)
fi

if [[  $useTx -eq 1 ]]; then
    echo "Tx..."
	($TxPrg $TxBufNum $TxHostIP $TxPort $PacketSize $PacketInBuf $PacketGenType $TxDelay $TxPeer)
fi


