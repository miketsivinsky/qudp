//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#if !defined(QUDP_H)
#define QUDP_H


#include <string>
#include <vector>
#include <memory>
#include <map>

#include <QHostAddress>
#include <QUdpSocket>
#include <QSemaphore>
#include <QThread>

#include "SysUtilsQt.h"
#include "tqueue.h"
#include "UDP_Defs.h"

#define QUDP_LIB_ADDR_FORMAT
//#define QUDP_SO_REUSEADDR

#define QUDP_PRINT_DEBUG_INFO
#define QUDP_PRINT_DEBUG_ERROR

#define QUDP_THREAD_SAFE

//#define CHECK_UNIQUE_PTR_DELETER

#if defined(CHECK_UNIQUE_PTR_DELETER)
    #include <functional>
#endif

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
class TSocket : public QThread
{
    Q_OBJECT

    friend class TSocketWrapper;

    public:
        TSocket(unsigned long hostAddr, unsigned hostPort, const UDP_LIB::TParams& params);
        virtual ~TSocket() = 0;
        virtual UDP_LIB::TDirection getDir() const = 0;
        UDP_LIB::TStatus getStatus() const { return mStatus; }
        int getPacketsNum() const { return mParams.numPacketsInBundle; }
        int getPacketSize() const { return mParams.netPacketSize; }
        int getBufLength() const { return getPacketsNum()*getPacketSize(); }
        unsigned getBundlesNum() const { return mParams.numBundles; }
        int getSocketBufSize() const { return mParams.socketBufSize; }
        int isStream() const { return (getPacketsNum() != 1); }
        char* getPacketAddr(UDP_LIB::Transfer& transfer, unsigned nPacket) { return reinterpret_cast<char*>(transfer.buf + nPacket*getPacketSize()); }
        unsigned long getHostAddr() const { return mHostAddr; }
        unsigned getHostPort() const { return mHostPort; }
        unsigned long getPeerAddr() const { return mParams.peerAddr; }
        unsigned getPeerPort() const { return mParams.peerPort; }

    protected:
        //---
        typedef TQueue<UDP_LIB::Transfer,TQueueSl,TQtReadWriteLockGuard> TJobQueue;

        //---
        volatile bool      mExit;        // TODO: check - is it need to do it atomic?
        UDP_LIB::TStatus   mStatus;
        unsigned long      mHostAddr;
        unsigned           mHostPort;
        QUdpSocket*        mSocket;
        UDP_LIB::TParams   mParams;
        QSemaphore         mDrvSem;
        QSemaphore         mAppSem;
        TJobQueue          mSubmitQueue;
        TJobQueue          mReadyQueue;
        bool               mColdStartCompleted;
        unsigned           mColdStartCounter;
        #if defined(QUDP_PRINT_DEBUG_INFO)
            unsigned mNoTranserErrorCounter;
            unsigned mTimeoutCounter;
            static const unsigned TIMEOUT_DEBUG_PERIOD = 10000;
        #endif

        #if defined(CHECK_UNIQUE_PTR_DELETER)
            std::function<void(uint8_t* obj)> Deleter = [](uint8_t* obj) { static auto n = 0; delete [] obj; printf("Deleter [uint8_t] %d\n",++n);  };
            std::vector<std::unique_ptr<uint8_t [],decltype(Deleter)>> mBufPool;
        #else
            std::vector<std::unique_ptr<uint8_t []>> mBufPool;
        #endif
        std::vector<UDP_LIB::Transfer>   mTransferPool;

        //---
        virtual void threadStart() { QThread::start(QThread::Priority(mParams.threadPriority)); }
        virtual void run() Q_DECL_OVERRIDE;
        virtual bool onExec() { return false; }
        bool threadFinish(unsigned timeout);
        virtual bool socketInit();
        bool checkTransfer(const UDP_LIB::Transfer& transfer);
        void sendToSubmitQueue(const UDP_LIB::Transfer& transfer);
        void sendToReadyQueue(const UDP_LIB::Transfer& transfer, bool bufUsedFlag = true);
        unsigned getReadyTransferNum() const { return static_cast<unsigned>(mReadyQueue.size()); }
        UDP_LIB::TStatus submitTransfer(UDP_LIB::Transfer& transfer);
        UDP_LIB::TStatus getTransfer(UDP_LIB::Transfer& transfer, unsigned timeout = INFINITE);
        UDP_LIB::TStatus tryGetTransfer(UDP_LIB::Transfer& transfer);
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
class TSocketRx : public TSocket
{
    public:
        TSocketRx(unsigned long hostAddr, unsigned hostPort, const UDP_LIB::TParams& params);
        ~TSocketRx();
        virtual UDP_LIB::TDirection getDir() const { return UDP_LIB::Receive; }

    protected:
        static unsigned const  WAIT_THREAD_FINISH_RX = 2000;
        static unsigned const  SOCKET_TIMEOUT_RX     =  200;
        static unsigned const  MAX_RCV_BUFFERS       = 1000;

        virtual bool socketInit();
        virtual void threadStart();
        virtual bool onExec();
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
class TSocketTx : public TSocket
{
    Q_OBJECT

    public slots:
        void bytesWritten(qint64 nBytes);

    public:
        TSocketTx(unsigned long hostAddr, unsigned hostPort, const UDP_LIB::TParams& params);
        ~TSocketTx();
        virtual UDP_LIB::TDirection getDir() const { return UDP_LIB::Transmit; }

    protected:
        static unsigned const  WAIT_THREAD_FINISH_TX   = 3000;
        static unsigned const  SOCKET_TIMEOUT_TX_START = 200;
        static unsigned const  SOCKET_TIMEOUT_TX       = 10;
        static unsigned const  MAX_SENT_BUFFERS        = 1;
        static unsigned const  MAX_RETRY_PACKET_TX     = 200;
        QSemaphore   mTxSem;
        quint64      mBytesSent;

        virtual bool socketInit();
        virtual void threadStart();
        virtual bool onExec();
        unsigned timeout() const { return mColdStartCompleted ? SOCKET_TIMEOUT_TX : SOCKET_TIMEOUT_TX_START; }
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
class TSocketWrapper
{
    public:
        static QString fullAddrTxt(unsigned long hostAddr, unsigned hostPort) { return QString::number(hostAddr,16) + QString(":") + QString::number(hostPort); }

        static TSocketWrapper* createSocket(unsigned long hostAddr, unsigned hostPort, const UDP_LIB::TParams* rxParams, const UDP_LIB::TParams* txParams, UDP_LIB::TStatus& status);
        ~TSocketWrapper();
        void socketStart();
        UDP_LIB::TStatus submitTransfer(UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer);
        UDP_LIB::TStatus getTransfer(UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer, unsigned timeout = INFINITE);
        unsigned getReadyTransferNum(UDP_LIB::TDirection dir) const;
        UDP_LIB::TStatus tryGetTransfer(UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer);
        bool isSocketExist(UDP_LIB::TDirection dir) const { return dir == (UDP_LIB::Receive) ? mSocketRx : mSocketTx; }

    private:
        TSocketWrapper(unsigned long hostAddr,unsigned hostPort, const UDP_LIB::TParams* rxParams, const UDP_LIB::TParams* txParams);
        QString fullAddrTxt() { return TSocketWrapper::fullAddrTxt(mHostAddr, mHostPort); }

        unsigned long mHostAddr;
        unsigned      mHostPort;
        TSocket*      mSocketRx;
        TSocket*      mSocketTx;
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
class TSocketPool
{
    public:
        static UDP_LIB::TStatus init();
        static UDP_LIB::TStatus cleanUp();
        static UDP_LIB::TStatus getStatus();
        static bool isSocketExist(unsigned long hostAddr, unsigned hostPort);
        static bool isSocketExist(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir);
        static UDP_LIB::TStatus createSocket(unsigned long hostAddr, unsigned hostPort, const UDP_LIB::TParams* rxParams, const UDP_LIB::TParams* txParams);
        static UDP_LIB::TStatus submitTransfer(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer);
        static UDP_LIB::TStatus getTransfer(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer, DWORD timeout = INFINITE);
        static unsigned getReadyTransferNum(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir);
        static UDP_LIB::TStatus tryGetTransfer(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer);

    private:
        static TSocketPool* mInstance;
        #if defined(QUDP_THREAD_SAFE)
            static TQtMutexGuard mInstanceGuard;
        #endif
        typedef std::map<uint64_t, TSocketWrapper*> TPoolMap;

        //---
        static TSocketPool* getInstance() { return mInstance; }
        static TPoolMap& getPool() { return getInstance()->mSocketPoolMap; }
        static uint64_t getHostAddr64(unsigned long hostAddr, unsigned hostPort) { return (static_cast<uint64_t>(hostAddr) << 32) | (hostPort & 0xFFFF); }
        TSocketPool();
        ~TSocketPool();
        static TSocketWrapper* getSocketWrapper(unsigned long hostAddr, unsigned hostPort);
        static UDP_LIB::TStatus getStatusImpl();
        static bool isSocketExistImpl(unsigned long hostAddr, unsigned hostPort);
        static bool isSocketExistImpl(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir);


        //---
        UDP_LIB::TStatus mNetStatus;
        TPoolMap         mSocketPoolMap;
};

#endif /* QUDP_H */

