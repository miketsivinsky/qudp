//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#include <QtGlobal>
#include <QDebug>

//------------------------------------------------------------------------------
// for TSocketTx::TSocketTx due to the QTBUG-30478 (https://bugreports.qt.io/browse/QTBUG-30478)
#if defined(Q_OS_WIN) && defined(ENA_WIN_API)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <winsock2.h>
#endif
//------------------------------------------------------------------------------

#include "qudp.h"

//#define DEBUG_SOCKET_RX
//#define DEBUG_SOCKET_TX


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#if 0 // deleter implementation as a class, really used implementation as a lambda
class TDeleter
{
    public:
        void operator()(uint8_t*  obj) {
            delete obj;
             printf("TDeleter\n");
        }
};
#endif

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static inline unsigned long convertIpAddr(unsigned long addr) {
    #if defined(QUDP_LIB_ADDR_FORMAT)
        return ((addr & 0xFF000000) >> 24) | ((addr & 0x00FF0000) >> 8) | ((addr & 0x0000FF00) << 8) | ((addr & 0x000000FF) << 24);
    #else
        return addr;
    #endif
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static inline int qTimeout(unsigned timeout) {
    if(timeout == INFINITE) {
        return -1;
    } else {
        return timeout;
    }
}

//------------------------------------------------------------------------------
TSocket::TSocket(unsigned long hostAddr, unsigned hostPort, const UDP_LIB::TParams& params) :
                                mExit(false),
                                mStatus(UDP_LIB::NotInitialized),
                                mHostAddr(hostAddr),
                                mHostPort(hostPort),
                                mSocket(0),
                                mParams(params),
                                mDrvSem(),
                                mAppSem(),
                                mSubmitQueue(),
                                mReadyQueue(),
                                mColdStartCompleted(false),
                                mColdStartCounter(0),
                                #if defined(QUDP_PRINT_DEBUG_INFO)
                                    mNoTranserErrorCounter(0),
                                    mTimeoutCounter(0),
                                #endif
                                mBufPool(params.numBundles),
                                mTransferPool(params.numBundles)
{
  for(unsigned i = 0; i < getBundlesNum(); ++i) {
    #if defined(CHECK_UNIQUE_PTR_DELETER)
        mBufPool[i] = std::unique_ptr<uint8_t [],decltype(Deleter)>(new uint8_t[getBufLength()], Deleter);
    #else
        mBufPool[i] = std::unique_ptr<uint8_t []>(new uint8_t[getBufLength()]);
    #endif

    mTransferPool[i].bundleId  = i;
    mTransferPool[i].direction = UDP_LIB::NotInit;
    mTransferPool[i].status    = UDP_LIB::NotInitialized;
    mTransferPool[i].length    = 0;
    mTransferPool[i].bufLength = getBufLength();
    mTransferPool[i].buf       = mBufPool[i].get();
    mTransferPool[i].isStream  = isStream();
  }
}

//------------------------------------------------------------------------------
TSocket::~TSocket()
{

}

//------------------------------------------------------------------------------
bool TSocket::socketInit()
{
    if(!mSocket) {
        mSocket = new QUdpSocket;

        #if defined(QUDP_SO_REUSEADDR)
            #if defined(Q_OS_WIN)
                QAbstractSocket::BindMode bindMode = QAbstractSocket::ReuseAddressHint;
            #else
                QAbstractSocket::BindMode bindMode = QAbstractSocket::ShareAddress;
            #endif
            if(mSocket->bind(QHostAddress(convertIpAddr(mHostAddr)),mHostPort,bindMode) == false) {
                #if defined(QUDP_PRINT_DEBUG_ERROR)
                    qDebug() << "[ERROR] [TSocket::socketInit]" << qPrintable(TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort)) << qPrintable(mSocket->errorString());
                #endif
                mStatus = UDP_LIB::SocketBindError;
                return false;
            }
        #endif
    }
    mStatus = UDP_LIB::Ok;
    return true;
}

//------------------------------------------------------------------------------
void TSocket::run()
{
    if(!socketInit()) {
        #if defined(QUDP_PRINT_DEBUG_ERROR)
            qDebug() << "[ERROR] [TSocket::run] socketInit() error. Thread finished" << TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort) << (getDir() == UDP_LIB::Receive ? "Rx" : "Tx");
        #endif
        return;
    }
    while(!mExit && (mStatus == UDP_LIB::Ok) && onExec()) {
        // main thread loop
    }

    mStatus = UDP_LIB::NotInitialized;
    mSocket->close();
    delete mSocket;
    mSocket = 0;
    #if defined(QUDP_PRINT_DEBUG_INFO)
        qDebug() << "[INFO] [TSocket::run] finished" << TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort) << (getDir() == UDP_LIB::Receive ? "Rx" : "Tx");
    #endif
}

//------------------------------------------------------------------------------
bool TSocket::threadFinish(unsigned timeout)
{
    mExit = true;
    return wait(timeout);
}

//------------------------------------------------------------------------------
bool TSocket::checkTransfer(const UDP_LIB::Transfer& transfer)
{
    //--- check bundleId
    if((transfer.bundleId < 0) || (transfer.bundleId >= static_cast<int>(getBundlesNum()))) {
        return false;
    }
    //--- check direction
    if(transfer.direction != getDir()) {
        return false;
    }

    //--- check length 1
    if((transfer.direction == UDP_LIB::Transmit) && ((transfer.length == 0) || (transfer.length > getBufLength()))) {
        return false;
    }

    //--- check length 2
    if((transfer.direction == UDP_LIB::Transmit) && isStream() && (transfer.length != getBufLength())) {
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocket::submitTransfer(UDP_LIB::Transfer& transfer)
{
    if(!checkTransfer(transfer)) {
        return UDP_LIB::SubmitError;
    }

    transfer.bufLength = getBufLength();                     // set bufLength
    transfer.isStream  = isStream();                         // set isStream
    transfer.buf       = mBufPool[transfer.bundleId].get();  // set buf
    transfer.status    = UDP_LIB::Ok;                        // set status Ok

    if(isStream() && (getDir() == UDP_LIB::Transmit)) {
        transfer.length = getBufLength();
    }
    // in original UDP_LIB now we send transfer request to socket
    // but in Qt implementation we can't work with QUdpSocket in different threads
    // and thus we use another algorithm
    sendToSubmitQueue(transfer);
    return UDP_LIB::Ok;
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocket::getTransfer(UDP_LIB::Transfer& transfer, unsigned timeout)
{
    if(mAppSem.tryAcquire(1,qTimeout(timeout))) {
        if(mReadyQueue.get(transfer)) {
            return UDP_LIB::Ok;
        } else {
            return UDP_LIB::SocketTransferError;
        }
    } else {
        return UDP_LIB::SocketWaitTimeout;
    }
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocket::tryGetTransfer(UDP_LIB::Transfer& transfer)
{
    if(!getReadyTransferNum())
        return UDP_LIB::NoReadyTransfers;
    return getTransfer(transfer,0);
}

//------------------------------------------------------------------------------
void TSocket::sendToSubmitQueue(const UDP_LIB::Transfer& transfer)
{
    mSubmitQueue.put(transfer);
    mDrvSem.release();
}

//------------------------------------------------------------------------------
void TSocket::sendToReadyQueue(const UDP_LIB::Transfer& transfer, bool bufUsedFlag)
{
    if(bufUsedFlag && !mColdStartCompleted) {
        const unsigned COLD_START_BORDER = getSocketBufSize() ? getSocketBufSize()/getBufLength() : getBundlesNum();
        if(++mColdStartCounter >= COLD_START_BORDER*8) {
            mColdStartCompleted = true;
        }
    }
    mReadyQueue.put(transfer);
    mAppSem.release();
    if(mParams.onTransferReady) {
        UDP_LIB::TNetAddr hostAddr = { getHostAddr(), getHostPort() };
        UDP_LIB::TNetAddr peerAddr = { getPeerAddr(), getPeerPort() };
        (*mParams.onTransferReady)(hostAddr, peerAddr, getDir());
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
TSocketRx::TSocketRx(unsigned long hostAddr, unsigned hostPort, const UDP_LIB::TParams& params)
    : TSocket(hostAddr, hostPort, params)
{
    for(auto& transfer : mTransferPool) {
        transfer.direction = UDP_LIB::Receive;
    }
}

//------------------------------------------------------------------------------
TSocketRx::~TSocketRx()
{
    mDrvSem.release(getBundlesNum());
    mAppSem.release(getBundlesNum());

    if(!threadFinish(WAIT_THREAD_FINISH_RX)) {
        #if defined(QUDP_PRINT_DEBUG_INFO)
            qDebug() << "[WARN] [TSocketRx::~TSocketRx] thread does not finished and terminated" << TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort);
        #endif
        terminate();
        if(mSocket) {
            delete mSocket;
            mSocket = 0;
        }
    } else {
        #if defined(QUDP_PRINT_DEBUG_INFO)
            qDebug() << "[INFO] [TSocketRx::~TSocketRx] thread finished Ok" << TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort);
        #endif
    }
}

//------------------------------------------------------------------------------
bool TSocketRx::socketInit()
{
    if(!TSocket::socketInit()) {
        #if defined(QUDP_PRINT_DEBUG_ERROR)
            qDebug() << "[ERROR] [TSocketRx::socketInit] error in [TSocket::socketInit]" << TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort);
        #endif
        return false;
    }

    #if !defined(QUDP_SO_REUSEADDR)
        if(mSocket->bind(QHostAddress(convertIpAddr(mHostAddr)),mHostPort,QAbstractSocket::DefaultForPlatform) == false) {
            #if defined(QUDP_PRINT_DEBUG_ERROR)
                qDebug() << "[ERROR] [TSocketRx::socketInit]" << qPrintable(TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort)) << qPrintable(mSocket->errorString());
            #endif
            mStatus = UDP_LIB::SocketBindError;
            return false;
        }
    #endif

    if(getSocketBufSize()) {
        mSocket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption,getSocketBufSize());
        #if defined(QUDP_PRINT_DEBUG_ERROR) || defined(QUDP_PRINT_DEBUG_INFO)
            int socketBufSize = mSocket->socketOption(QAbstractSocket::ReceiveBufferSizeSocketOption).toInt();
        #endif
        #if defined(QUDP_PRINT_DEBUG_INFO)
            qDebug() << "[INFO] [TSocketRx::socketInit] [ReceiveBufferSizeSocketOption]" << TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort) << socketBufSize << getSocketBufSize();
        #endif
        #if defined(QUDP_PRINT_DEBUG_ERROR)
          #if defined(Q_OS_WIN)
            if(socketBufSize != getSocketBufSize()) {
          #else
            if(socketBufSize != getSocketBufSize()*2) {
          #endif
                qDebug() << "[ERROR] [TSocketRx::socketInit] [ReceiveBufferSizeSocketOption]" << TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort) << socketBufSize << getSocketBufSize();
            }
        #endif
    }

    mStatus = UDP_LIB::Ok;
    return true;
}

//------------------------------------------------------------------------------
void TSocketRx::threadStart()
{
    for(auto& transfer : mTransferPool) {
        submitTransfer(transfer);
    }
    TSocket::threadStart();
}

//------------------------------------------------------------------------------
bool TSocketRx::onExec()
{
    unsigned tryXmit  = TSocketRx::MAX_RCV_BUFFERS;
    UDP_LIB::Transfer transfer;
    qint64 xmitLength;
    int rxSocketTimeout = (mParams.timeout == INFINITE) ? TSocketRx::SOCKET_TIMEOUT_RX : qTimeout(mParams.timeout);

    while(tryXmit--) {
        mDrvSem.acquire();
        xmitLength = 0;
        if(mSubmitQueue.readFront(transfer)) {
            if(mParams.timeout != INFINITE) {
                mSubmitQueue.pop();
            }
            #if defined(DEBUG_SOCKET_RX)
                qDebug() << "[INFO] [TSocketRx::onExec] get(transfer):" << TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort) << "threadId" << currentThreadId();
            #endif
            transfer.status = UDP_LIB::Ok; // ?! not need to set in there place preset in TSocket::submitTransfer
            for(auto nPacket = 0; nPacket < getPacketsNum(); ++nPacket) {
                bool dataReady = mSocket->waitForReadyRead(rxSocketTimeout);
                if(dataReady) {
                    qint64 packetLen = mSocket->readDatagram(getPacketAddr(transfer,nPacket),getPacketSize());
                    xmitLength += packetLen;
                    if((packetLen != getPacketSize()) && isStream()) {
                        // data len error - when socket Rx and isStream() == true
                        #if defined(QUDP_PRINT_DEBUG_ERROR)
                            qDebug() << "[ERROR] [TSocketRx::onExec] packetLen error" << TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort) << packetLen;
                        #endif
                        transfer.status = UDP_LIB::XmitLenError;
                        break;
                    }
                } else {
                    // timeout
                    #if defined(QUDP_PRINT_DEBUG_ERROR)
                        const unsigned TimeoutFactor = TSocket::TIMEOUT_DEBUG_PERIOD/rxSocketTimeout;
                        if((++mTimeoutCounter % TimeoutFactor) == 0) {
                            //qDebug() << "[WARN] [TSocketRx::onExec] timeout" << transfer.bundleId << TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort);
                        }
                    #endif
                    transfer.status = UDP_LIB::SocketWaitTimeout;
                    break;
                }
            }
        } else {
            // unknown error
            #if defined(QUDP_PRINT_DEBUG_ERROR)
                qDebug() << "[WARN] [TSocketRx::onExec] it's probably thread finishing" << TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort) << ++mNoTranserErrorCounter;
            #endif
            transfer.status = UDP_LIB::SocketTransferError;
            return true;
        }
        if(transfer.status != UDP_LIB::Ok) {
            tryXmit = 0;
        } else {
            if(mSocket->hasPendingDatagrams() && mDrvSem.available()) {
                #if defined(DEBUG_SOCKET_RX)
                    qDebug() << "[INFO] [TSocketRx::onExec] transfer completed (retry)" << TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort);
                #endif
                transfer.length = xmitLength;
                if(mParams.timeout == INFINITE) {
                    mSubmitQueue.pop();
                }
                sendToReadyQueue(transfer);
            } else {
                tryXmit = 0;
            }
        }
    }
    #if defined(DEBUG_SOCKET_RX)
        if(transfer.status == UDP_LIB::Ok) {
            qDebug() << "[INFO] [TSocketRx::onExec] transfer completed (final)" << TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort);
        }
    #endif
    transfer.length = xmitLength;
    if(mParams.timeout != INFINITE) {
        sendToReadyQueue(transfer);
    } else {
        if(transfer.status != UDP_LIB::SocketWaitTimeout) {
            mSubmitQueue.pop();
            sendToReadyQueue(transfer);
        } else {
            mDrvSem.release();
        }
    }
    return true;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
TSocketTx::TSocketTx(unsigned long hostAddr, unsigned hostPort, const UDP_LIB::TParams& params)
    : TSocket(hostAddr, hostPort, params),
      mBytesSent(0)
{
    for(auto& transfer : mTransferPool) {
        transfer.direction = UDP_LIB::Transmit;
        transfer.status    = UDP_LIB::Ok;
    }
}

//------------------------------------------------------------------------------
TSocketTx::~TSocketTx()
{
    mDrvSem.release(getBundlesNum());
    mAppSem.release(getBundlesNum());
    mTxSem.release();

    if(!threadFinish(WAIT_THREAD_FINISH_TX)) {
        #if defined(QUDP_PRINT_DEBUG_INFO)
            qDebug() << "[WARN] [TSocketTx::~TSocketTx] thread does not finished and terminated" << TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort);
        #endif
        terminate();
        if(mSocket) {
            delete mSocket;
            mSocket = 0;
        }
    } else {
        #if defined(QUDP_PRINT_DEBUG_INFO)
            qDebug() << "[INFO] [TSocketTx::~TSocketTx] thread finished Ok" << TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort);
        #endif
    }
}

//------------------------------------------------------------------------------
bool TSocketTx::socketInit()
{
    if(!TSocket::socketInit()) {
        #if defined(QUDP_PRINT_DEBUG_ERROR)
            qDebug() << "[ERROR] [TSocketTx::socketInit] error in [TSocket::socketInit]";
        #endif
        return false;
    }

    #if !defined(QUDP_SO_REUSEADDR)
        if(mSocket->bind(QHostAddress(convertIpAddr(mHostAddr))) == false) {
            #if defined(QUDP_PRINT_DEBUG_ERROR)
                qDebug() << "[ERROR] [TSocketTx::socketInit]" << qPrintable(TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort)) << qPrintable(mSocket->errorString());
            #endif
            mStatus = UDP_LIB::SocketBindError;
            return false;
    }
    #endif

    if(getSocketBufSize()) {
        #if defined(Q_OS_WIN) && defined(ENA_WIN_API)
            int optValue = getSocketBufSize();
            int optLength = sizeof(optValue);
            qintptr hSocket = mSocket->socketDescriptor();
            ::setsockopt(hSocket,SOL_SOCKET,SO_SNDBUF,reinterpret_cast<char*>(&optValue),optLength);
        #else
            mSocket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption,getSocketBufSize());
        #endif

        #if defined(QUDP_PRINT_DEBUG_ERROR) || defined(QUDP_PRINT_DEBUG_INFO)
            int socketBufSize = mSocket->socketOption(QAbstractSocket::SendBufferSizeSocketOption).toInt();
        #endif

        #if defined(QUDP_PRINT_DEBUG_INFO)
            qDebug() << "[INFO] [TSocketTx] [SendBufferSizeSocketOption]" << TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort) << socketBufSize << getSocketBufSize();
        #endif
        #if defined(QUDP_PRINT_DEBUG_ERROR)
          #if defined(Q_OS_WIN)
            if(socketBufSize != getSocketBufSize()) {
          #else
            if(socketBufSize != getSocketBufSize()*2) {
          #endif
                qDebug() << "[ERROR] [TSocketTx] [SendBufferSizeSocketOption]" << TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort) << socketBufSize << getSocketBufSize();
            }
        #endif
    }
    /* try use write() instead sendDatagram, but result the same */ //mSocket->connectToHost(QHostAddress(mParams.peerAddr), mParams.peerPort);
    connect(mSocket,&QIODevice::bytesWritten,this,&TSocketTx::bytesWritten,Qt::DirectConnection);
    mStatus = UDP_LIB::Ok;
    return true;
}

//------------------------------------------------------------------------------
void TSocketTx::threadStart()
{
    for(auto& transfer : mTransferPool) {
        sendToReadyQueue(transfer,false);
    }
    TSocket::threadStart();
}

//------------------------------------------------------------------------------
void TSocketTx::bytesWritten(qint64 nBytes)
{
    mBytesSent = nBytes;
    mTxSem.release();
}

//------------------------------------------------------------------------------
bool TSocketTx::onExec()
{
    unsigned tryXmit = TSocketTx::MAX_SENT_BUFFERS;
    UDP_LIB::Transfer transfer;
    const QHostAddress peerAddr(convertIpAddr(mParams.peerAddr));
    const quint16 peerPort = mParams.peerPort;
    qint64 xmitLength;
    UDP_LIB::TStatus txStatus;

    while(tryXmit) {
        mDrvSem.acquire();
        xmitLength = 0;
        if(mSubmitQueue.get(transfer)) {
            #if defined(DEBUG_SOCKET_TX)
                qDebug() << "[INFO] [TSocketTx::onExec] get(transfer) bundleId:" << transfer.bundleId << "threadId" << currentThreadId();
            #endif
            transfer.status = UDP_LIB::Ok; // ?! not need to set in there place preset in TSocket::submitTransfer
            const qint64 packetLen = isStream() ? getPacketSize() : transfer.length;

            #if 0 // DEBUG
                static unsigned bufCntr = 0;
                if((++bufCntr <= 2*getBundlesNum())) {
                    uint32_t* bufPtr = (uint32_t*)transfer.buf;
                    for(int k = 0; k < 8; ++k) {
                        printf("[udp2] %2d-0x%08x: buf[%2d] = %5d\n",transfer.bundleId,transfer.buf,k,bufPtr[k]);
                    }
                    printf("-------------------\n");
                }
            #endif

            for(auto nPacket = 0; nPacket < getPacketsNum(); ++nPacket) {
                unsigned retryTx = TSocketTx::MAX_RETRY_PACKET_TX;
                while(retryTx--) {
                    if(mExit) {
                        #if defined(DEBUG_SOCKET_TX)
                            qDebug() << "TSocketTx::onExec() exit";
                        #endif
                        return false;
                    }
                    /*int bytesSent =*/ mSocket->writeDatagram(getPacketAddr(transfer,nPacket), packetLen, peerAddr, peerPort);
                    /* try use write() instead sendDatagram, but result the same */ //int bytesSent = mSocket->write(getPacketAddr(transfer,nPacket), packetLen);
                    bool xmitComplete = mTxSem.tryAcquire(1,/*SOCKET_TIMEOUT_TX*/timeout());

                    if(xmitComplete) {
                        xmitLength += mBytesSent;
                        if(mBytesSent != packetLen) {
                            // data len error
                            txStatus = UDP_LIB::XmitLenError;
                        } else {
                            txStatus = UDP_LIB::Ok;
                            break;
                        }
                    } else {
                        // timeout error
                        txStatus = UDP_LIB::SocketWaitTimeout;
                    }
                    mBytesSent = 0;
                }
                if(txStatus != UDP_LIB::Ok) {
                    transfer.status = txStatus;
                    #if defined(QUDP_PRINT_DEBUG_ERROR)
                        qDebug() << "[ERROR] [TSocketTx::onExec]" << txStatus << TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort);
                    #endif
                    break;
                }
            }
        } else {
            // unknown error
            #if defined(QUDP_PRINT_DEBUG_ERROR)
                qDebug() << "[WARN] [TSocketTx::onExec] it's probably thread finishing" << TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort)  << ++mNoTranserErrorCounter;
            #endif
            transfer.status = UDP_LIB::SocketTransferError;
            return true;
        }
        if(transfer.status != UDP_LIB::Ok) {
            tryXmit = 0;
        } else {
            if(mDrvSem.available() && --tryXmit) {
                #if defined(DEBUG_SOCKET_TX)
                    qDebug() << "[INFO] [TSocketTx::onExec] transfer completed (retry)" << TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort) << tryXmit;
                #endif
                transfer.length = xmitLength;
                sendToReadyQueue(transfer);
            } else {
                tryXmit = 0;
            }
        }
    }
    #if defined(DEBUG_SOCKET_TX)
        qDebug() << "[INFO] [TSocketTx::onExec] transfer completed (final)" << TSocketWrapper::fullAddrTxt(mHostAddr,mHostPort);
    #endif
    transfer.length = xmitLength;
    sendToReadyQueue(transfer);
    return true;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
TSocketWrapper* TSocketWrapper::createSocket(unsigned long hostAddr, unsigned hostPort, const UDP_LIB::TParams* rxParams, const UDP_LIB::TParams* txParams, UDP_LIB::TStatus& status)
{
     status = UDP_LIB::Ok;
    TSocketWrapper* wrapper = new TSocketWrapper(hostAddr,hostPort,rxParams,txParams);
    if(wrapper->mSocketRx && (wrapper->mSocketRx->getStatus() != UDP_LIB::Ok))
        status = wrapper->mSocketRx->getStatus();
    if(wrapper->mSocketTx && (wrapper->mSocketTx->getStatus() != UDP_LIB::Ok))
        status = wrapper->mSocketTx->getStatus();
    return wrapper;
}

//------------------------------------------------------------------------------
TSocketWrapper::TSocketWrapper(unsigned long hostAddr, unsigned hostPort, const UDP_LIB::TParams* rxParams, const UDP_LIB::TParams* txParams) :
                                                                                         mHostAddr(hostAddr),
                                                                                         mHostPort(hostPort),
                                                                                         mSocketRx(0),
                                                                                         mSocketTx(0)
{
    #if defined(QUDP_PRINT_DEBUG_INFO)
        qDebug() << "[INFO] [TSocketWrapper::TSocketWrapper]" << qPrintable(fullAddrTxt());
    #endif

    if(rxParams) {
        mSocketRx = new TSocketRx(mHostAddr, mHostPort, *rxParams);
    }
    if(txParams) {
        mSocketTx = new TSocketTx(mHostAddr, mHostPort, *txParams);
    }
    //socketStart();  // moved at the end of TSocketPool::createSocket to prevent calling Tx callback before inserting socket in pool
}

//------------------------------------------------------------------------------
TSocketWrapper::~TSocketWrapper()
{
    delete mSocketRx;
    delete mSocketTx;

    #if defined(QUDP_PRINT_DEBUG_INFO)
        qDebug() << "[INFO] [TSocketWrapper::~TSocketWrapper]" << qPrintable(fullAddrTxt());
    #endif
}

//------------------------------------------------------------------------------
void TSocketWrapper::socketStart()
{
    //---
    if(mSocketRx) {
        mSocketRx->threadStart();
    }
    if(mSocketTx) {
        mSocketTx->threadStart();
    }
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocketWrapper::submitTransfer(UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer)
{
    if(dir == UDP_LIB::Transmit && mSocketTx) {
        return mSocketTx->submitTransfer(transfer);
    }
    if(dir == UDP_LIB::Receive && mSocketRx) {
        return mSocketRx->submitTransfer(transfer);
    }
    return UDP_LIB::SocketXmitNotExist;
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocketWrapper::getTransfer(UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer, unsigned timeout)
{
    if(dir == UDP_LIB::Transmit && mSocketTx) {
        return mSocketTx->getTransfer(transfer,timeout);
    }
    if(dir == UDP_LIB::Receive && mSocketRx) {
        return mSocketRx->getTransfer(transfer,timeout);
    }
    return UDP_LIB::SocketXmitNotExist;
}

//------------------------------------------------------------------------------
unsigned TSocketWrapper::getReadyTransferNum(UDP_LIB::TDirection dir) const
{
    if(dir == UDP_LIB::Transmit && mSocketTx) {
        return mSocketTx->getReadyTransferNum();
    }
    if(dir == UDP_LIB::Receive && mSocketRx) {
        return mSocketRx->getReadyTransferNum();
    }
    return 0;
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocketWrapper::tryGetTransfer(UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer)
{
    if(dir == UDP_LIB::Transmit && mSocketTx) {
        return mSocketTx->tryGetTransfer(transfer);
    }
    if(dir == UDP_LIB::Receive && mSocketRx) {
        return mSocketRx->tryGetTransfer(transfer);
    }
    return UDP_LIB::SocketXmitNotExist;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
TSocketPool* TSocketPool::mInstance = 0;
#if defined(QUDP_THREAD_SAFE)
    TQtMutexGuard TSocketPool::mInstanceGuard;
#endif


//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocketPool::init()
{
    #if defined(QUDP_THREAD_SAFE)
        TQtMutexGuard::TLocker lock(mInstanceGuard);
    #endif
    if(mInstance == 0) {
        mInstance = new TSocketPool;
    }
    return UDP_LIB::Ok;
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocketPool::cleanUp()
{
    #if defined(QUDP_THREAD_SAFE)
        TQtMutexGuard::TLocker lock(mInstanceGuard);
    #endif
    delete mInstance;
    mInstance = 0;
    return UDP_LIB::Ok;
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocketPool::getStatus()
{
    #if defined(QUDP_THREAD_SAFE)
        TQtMutexGuard::TLocker lock(mInstanceGuard);
    #endif
    return getStatusImpl();
}

//------------------------------------------------------------------------------
TSocketPool::TSocketPool() : mNetStatus(UDP_LIB::Ok), mSocketPoolMap()
{
    #if defined(QUDP_PRINT_DEBUG_INFO)
        qDebug() << "[INFO] [TSocketPool::::TSocketPool]";
    #endif
}

//------------------------------------------------------------------------------
TSocketPool::~TSocketPool()
{
    for(TPoolMap::iterator sock = getPool().begin(); sock != getPool().end(); ++sock) {
        delete sock->second;
        #if defined(QUDP_PRINT_DEBUG_INFO)
            uint32_t hostAddr = uint32_t(sock->first >> 32);
            uint32_t hostPort = uint32_t(sock->first & 0xFFFFFFFF);
            qDebug() << "[INFO] [TSocketPool::~TSocketPool] socket deleted:" << qPrintable(TSocketWrapper::fullAddrTxt(hostAddr, hostPort));
        #endif
    }
    getPool().clear();

    #if defined(QUDP_PRINT_DEBUG_INFO)
        qDebug() << "[INFO] [TSocketPool::~TSocketPool] finished";
    #endif
}

//------------------------------------------------------------------------------
TSocketWrapper* TSocketPool::getSocketWrapper(unsigned long hostAddr, unsigned hostPort)
{
    //---
    if (getStatusImpl() != UDP_LIB::Ok) {
        return 0;
    }

    TPoolMap::iterator sock = getPool().find(getHostAddr64(hostAddr,hostPort));
    if(sock != getPool().end()) {
        return sock->second;
    } else {
        return 0;
    }
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocketPool::getStatusImpl()
{
    if (getInstance() == 0) {
        return UDP_LIB::NotInitialized;
    }
    else {
        return getInstance()->mNetStatus;
    }
}

//------------------------------------------------------------------------------
bool TSocketPool::isSocketExist(unsigned long hostAddr, unsigned hostPort)
{
    #if defined(QUDP_THREAD_SAFE)
        TQtMutexGuard::TLocker lock(mInstanceGuard);
    #endif
    return isSocketExistImpl(hostAddr,hostPort);
}

//------------------------------------------------------------------------------
bool TSocketPool::isSocketExist(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir)
{
    #if defined(QUDP_THREAD_SAFE)
        TQtMutexGuard::TLocker lock(mInstanceGuard);
    #endif
    return isSocketExistImpl(hostAddr,hostPort,dir);
}

//------------------------------------------------------------------------------
bool TSocketPool::isSocketExistImpl(unsigned long hostAddr, unsigned hostPort)
{
    //---
    if (getStatusImpl() != UDP_LIB::Ok) {
        return false;
    }
    TPoolMap::iterator sock = getPool().find(getHostAddr64(hostAddr,hostPort));
    return (sock != getPool().end());
}

//------------------------------------------------------------------------------
bool TSocketPool::isSocketExistImpl(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir)
{
    //---
    if (getStatusImpl() != UDP_LIB::Ok) {
        return false;
    }
    TPoolMap::iterator sock = getPool().find(getHostAddr64(hostAddr,hostPort));
    if(sock == getPool().end()) {
        return false;
    } else {
        return sock->second->isSocketExist(dir);
    }
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocketPool::createSocket(unsigned long hostAddr, unsigned hostPort, const UDP_LIB::TParams* rxParams, const UDP_LIB::TParams* txParams)
{
    #if defined(QUDP_THREAD_SAFE)
        TQtMutexGuard::TLocker lock(mInstanceGuard);
    #endif

    //---
    if(getStatusImpl() != UDP_LIB::Ok) {
        return getStatus();
    }

    if(isSocketExistImpl(hostAddr,hostPort)) {
        return UDP_LIB::SocketAlreadyExist;
    }

    //---
    UDP_LIB::TStatus status;
    TSocketWrapper* socketWrapper = TSocketWrapper::createSocket(hostAddr, hostPort, rxParams, txParams, status);
    if(!socketWrapper) {
        return status;
    }

    //---
    getPool().insert(std::pair<uint64_t,TSocketWrapper*>(getHostAddr64(hostAddr,hostPort),socketWrapper));
    socketWrapper->socketStart(); // to prevent calling Tx callback before inserting socket in pool
    return UDP_LIB::Ok;
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocketPool::submitTransfer(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer)
{
    #if defined(QUDP_THREAD_SAFE)
        TQtMutexGuard::TLocker lock(mInstanceGuard);
    #endif

    if (getStatusImpl() != UDP_LIB::Ok) {
        return getStatus();
    }

    TSocketWrapper* socketWrapper = getSocketWrapper(hostAddr,hostPort);
    if(socketWrapper) {
        return socketWrapper->submitTransfer(dir,transfer);
    } else {
        return UDP_LIB::SocketNotExist;
    }
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocketPool::getTransfer(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer, unsigned timeout)
{
    #if defined(QUDP_THREAD_SAFE)
        TQtMutexGuard::TLocker lock(mInstanceGuard);
    #endif

    if (getStatusImpl() != UDP_LIB::Ok) {
        return getStatus();
    }

    TSocketWrapper* socketWrapper = getSocketWrapper(hostAddr,hostPort);
    if(socketWrapper) {
        return socketWrapper->getTransfer(dir,transfer,timeout);
    } else {
        return UDP_LIB::SocketNotExist;
    }
}

//------------------------------------------------------------------------------
unsigned TSocketPool::getReadyTransferNum(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir)
{
    #if defined(QUDP_THREAD_SAFE)
        TQtMutexGuard::TLocker lock(mInstanceGuard);
    #endif

    if (getStatusImpl() != UDP_LIB::Ok) {
        return 0;
    }
    TSocketWrapper* socketWrapper = getSocketWrapper(hostAddr,hostPort);
    if(socketWrapper) {
        return socketWrapper->getReadyTransferNum(dir);
    } else {
        return 0;
    }
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocketPool::tryGetTransfer(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer)
{
    #if defined(QUDP_THREAD_SAFE)
        TQtMutexGuard::TLocker lock(mInstanceGuard);
    #endif

    if (getStatusImpl() != UDP_LIB::Ok) {
        return getStatus();
    }

    TSocketWrapper* socketWrapper = getSocketWrapper(hostAddr,hostPort);
    if(socketWrapper) {
        return socketWrapper->tryGetTransfer(dir,transfer);
    } else {
        return UDP_LIB::SocketNotExist;
    }
}
