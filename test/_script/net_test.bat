@echo off

rem ******  test config
set device_emulator=1
set useRx=0
set useTx=1

rem ****** IP list
set LocalHost=127.0.0.1
set HostIP=192.168.10.3
set DeviceIP=192.168.10.1

rem ****** common part
set PacketSize=1472
set PacketInBuf=64
set PacketGenType=1

rem ****** rx/tx part dependent from device_emulator mode
if %device_emulator%==1 (
	set TxHostIP=%HostIP%
	set TxPeer=%DeviceIP%
	set RxHostIP=%HostIP%
) else (
	set TxHostIP=%DeviceIP%
	set TxPeer=%HostIP%
	set RxHostIP=%DeviceIP%
)

rem ****** tx part
set TxBufNum=1000000
set TxDelay=0
set TxPort=50012

rem ****** rx part
set RxBufNum=100000
set RxPort=50012

rem ********************************************

taskkill /FI "WINDOWTITLE eq netTestTx" > nul
taskkill /FI "WINDOWTITLE eq netTestRx" > nul

set TxPrg=..\..\bin\release\test_tx.exe
set RxPrg=..\..\bin\release\test_rx.exe

if %useRx%==1 (
	cmd.exe /C "start "netTestRx" srv\net_rx1.bat %RxPrg% %RxBufNum% %RxHostIP% %RxPort% %PacketSize% %PacketInBuf% %PacketGenType%" 
)
if %useTx%==1 (
	cmd.exe /C "start "netTestTx" srv\net_tx1.bat %TxPrg% %TxBufNum% %TxHostIP% %TxPort% %PacketSize% %PacketInBuf% %PacketGenType% %TxDelay% %TxPeer%" 
)


