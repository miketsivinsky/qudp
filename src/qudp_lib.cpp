//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#define QUDP_LIB_EXPORT

#include "QUDP_Lib.h"
#include "QUDP.h"

namespace UDP_LIB
{
	//------------------------------------------------------------------------------
	QUDP_DLL_API UDP_LIB::TStatus init() {
		return TSocketPool::init();
	}

	//------------------------------------------------------------------------------
	QUDP_DLL_API UDP_LIB::TStatus cleanUp()	{
		return TSocketPool::cleanUp();
	}

	//------------------------------------------------------------------------------
	QUDP_DLL_API UDP_LIB::TStatus getStatus() {
		return TSocketPool::getStatus();
	}

	//------------------------------------------------------------------------------
	QUDP_DLL_API bool isSocketExist(unsigned long hostAddr, unsigned hostPort) {
		return TSocketPool::isSocketExist(hostAddr,hostPort);
	}

	//------------------------------------------------------------------------------
	QUDP_DLL_API bool isDirectionExist(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir) {
		return TSocketPool::isSocketExist(hostAddr,hostPort, dir);
	}

	//------------------------------------------------------------------------------
	QUDP_DLL_API UDP_LIB::TStatus createSocket(unsigned long hostAddr, unsigned hostPort, const UDP_LIB::TParams* rxParams, const UDP_LIB::TParams* txParams) {
		return TSocketPool::createSocket(hostAddr,hostPort,rxParams,txParams);
	}

	//------------------------------------------------------------------------------
	QUDP_DLL_API UDP_LIB::TStatus submitTransfer(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer) {
		return TSocketPool::submitTransfer(hostAddr,hostPort,dir,transfer);
	}

	//------------------------------------------------------------------------------
	QUDP_DLL_API UDP_LIB::TStatus getTransfer(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer, unsigned timeout) {
        return TSocketPool::getTransfer(hostAddr,hostPort,dir,transfer,static_cast<unsigned>(timeout));
	}

	//------------------------------------------------------------------------------
	QUDP_DLL_API unsigned getReadyTransferNum(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir) {
		return TSocketPool::getReadyTransferNum(hostAddr,hostPort,dir);
	}
	
	//------------------------------------------------------------------------------
	QUDP_DLL_API UDP_LIB::TStatus tryGetTransfer(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer) {
		return TSocketPool::tryGetTransfer(hostAddr,hostPort,dir,transfer);
	}
}
