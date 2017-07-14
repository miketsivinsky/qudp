#include <QDebug>
#include <QThread>
#include <QHostAddress>
#include <QElapsedTimer>

#include <cstdlib>

#include "../common/RawStreamTester.h"
#include "qudp_lib.h"

//------------------------------------------------------------------------------
// max UDP payload = 1472 (octets)
// UDP overhead    = 66 (octets)
//
// max relative speed (for UDP palyload = 1472) = 0.9571
// max relative speed (for UDP palyload = 1024) = 0.9394
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
#define UDP_LIB_ADDR_FORMAT

QThread::Priority MainThreadPriority   = QThread::NormalPriority;
QThread::Priority WorkerThreadPriority = QThread::NormalPriority;

const unsigned BundleNum = 128;
const unsigned Timeout   = INFINITE /*2000*/;
const int SocketBufSize  = 8*1024*1024;

const unsigned TypeInterval = 10000;

//------------------------------------------------------------------------------
const int PrgParams = 7;
// argv[0] - program name
// argv[1] - PacketNum
// argv[2] - host IP
// argv[3] - Port
// argv[4] - PacketSize
// argv[5] - PacketsInBuf
// argv[6] - PacketGenType
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
static unsigned long convertIpAddr(unsigned long addr);
static bool typeInfo(unsigned nBuf, UDP_LIB::TStatus status, UDP_LIB::Transfer& transfer);

//------------------------------------------------------------------------------
class TNetworkTest
{
    public:
        TNetworkTest() { UDP_LIB::init(); }
        ~TNetworkTest() { UDP_LIB::cleanUp(); }
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int main(int argc, char *argv[]) {
    if(argc < PrgParams) {
        printf("No input parameters\n");
        return 0;
    }

    //---
    const unsigned BufNum           = atoi(argv[1]);
    const char*    HostAddrStr      = argv[2];
    const unsigned Port             = atoi(argv[3]);
    const unsigned PacketSize       = atoi(argv[4]);
    const unsigned PacketsInBuf     = atoi(argv[5]);
    const unsigned PacketGenType    = atoi(argv[6]);

    //---
    printf("--------------------------------------------\n");
    printf("[INFO] [%s start]\n\n",argv[0]);
    printf("[PARAM] BufNum:     %8d\n",BufNum);
    printf("[PARAM] PacketSize:    %5d\n",PacketSize);
    printf("[PARAM] PacketsInBuf:  %5d\n",PacketsInBuf);
    printf("[PARAM] host IP:       %s\n",HostAddrStr);
    printf("[PARAM] port:          %5d\n",Port);
    printf("[PARAM] PacketGenType: %5d\n",PacketGenType);
    printf("--------------------------------------------\n");
    printf("\n");

    //---
    unsigned long HostAddr              = convertIpAddr(QHostAddress(HostAddrStr).toIPv4Address());
    const UDP_LIB::TParams rxParams     = { PacketSize, PacketsInBuf, BundleNum, WorkerThreadPriority, Timeout, SocketBufSize, 0, 0, 0 };
    const UDP_LIB::TDirection Direction = UDP_LIB::Receive;

    //---
    UDP_LIB::TStatus status;
    unsigned rvdBuf = 0;
    uint64_t rvdBytes = 0;
    QElapsedTimer timer;

    //---
    QThread::currentThread()->setPriority(MainThreadPriority);
    TNetworkTest networkTest;

    status = UDP_LIB::createSocket(HostAddr,Port,&rxParams,0);
    if(status == UDP_LIB::Ok) {
        UDP_LIB::Transfer transfer;
        TStreamTester streamChecker;
        unsigned errCount = 0;
        unsigned outOfOrderCount = 0;

        unsigned bufId = 0;
        for(unsigned nBuf = 0; nBuf < BufNum; ++nBuf) {
            status = UDP_LIB::getTransfer(HostAddr, Port, Direction, transfer);
            if(!typeInfo(nBuf,status,transfer))
                continue;
            if(nBuf == 0) {
                timer.start();
            }

            #if 1
                if(transfer.bundleId != bufId) {
                    qDebug() << "[BUFID ERR]" << transfer.bundleId << bufId;
                }
                bufId = (transfer.bundleId + 1);
                bufId = (bufId == BundleNum) ? 0 : bufId;
            #endif

            ++rvdBuf;
            rvdBytes += transfer.length;
            if (PacketGenType) {
                unsigned errInCurrBuf = streamChecker.checkBuf(transfer.buf, transfer.length);
                errCount += errInCurrBuf;
                if (errInCurrBuf) {
                    //printf("[ERROR] %6d errors in received buf\n", errInCurrBuf);
                }
            }
            status = UDP_LIB::submitTransfer(HostAddr,Port,Direction,transfer);
            if(status != UDP_LIB::Ok) {
                printf("[ERROR][MAIN] [submitTransfer] [%8d]: status: %2d\n",nBuf,status);
                break;
            }
        }
        qint64 timeElapsed = timer.nsecsElapsed();

        //---
        printf("\n--------------------------------------------\n");
        printf("[INFO] buffers expected %20d\n",BufNum);
        printf("[INFO] buffers received %20d\n",rvdBuf);
        #if defined(Q_CC_GNU)
            printf("[INFO] bytes received   %20lu\n",rvdBytes);
        #else
            printf("[INFO] bytes received   %20I64u\n",rvdBytes);
        #endif
        printf("[INFO] corrupted bufs   %20d\n",errCount);
        printf("[INFO] out of order     %20d\n",outOfOrderCount);
        printf("[INFO] elapsed time     %20.5fs\n",double(timeElapsed)/1e9);
        printf("[INFO] transfer rate:   %20.1f MB/s\n",(((double)rvdBytes)/timeElapsed)*1000.0);
        printf("[INFO] transfer rate:   %20.1f Mb/s\n",(((double)rvdBytes*8)/timeElapsed)*1000.0);
        printf("--------------------------------------------\n");
        printf("\n");
    } else {
        printf("[ERROR][MAIN] [createSocket] status: %d\n",status);
    }

    printf("\n--------------------------------------------\n");
    QThread::msleep(500);
    return 0;
}

//------------------------------------------------------------------------------
unsigned long convertIpAddr(unsigned long addr) {
    #if defined(UDP_LIB_ADDR_FORMAT)
        return ((addr & 0xFF000000) >> 24) | ((addr & 0x00FF0000) >> 8) | ((addr & 0x0000FF00) << 8) | ((addr & 0x000000FF) << 24);
    #else
        return addr;
    #endif
}

//------------------------------------------------------------------------------
bool typeInfo(unsigned nBuf, UDP_LIB::TStatus status, UDP_LIB::Transfer& transfer)
{
    if(nBuf == 0) {
        printf("\n--------------------------------------------\n");
    }

    if(status != UDP_LIB::Ok) {
        printf("[ERROR][MAIN] [getTransfer] [%8d]: status: %2d, trStat: %2d\n",nBuf,status,transfer.status);
        return false;
    }
    if(transfer.status != UDP_LIB::Ok) {
        printf("[WARN][MAIN] [getTransfer] [%8d]: bufId: %3d, trStat: %2d\n",nBuf,transfer.bundleId,transfer.status);
    }

    bool enaTypeInfo = ((nBuf % TypeInterval) == 0);
    if(enaTypeInfo) {
            printf("[INFO][MAIN] [getTransfer] [%8d]: bufId: %3d, dir: %1d, trStat: %2d, len: %5d, bufLen: %5d, isStream: %1d\n",
                                                  nBuf,
                                                  transfer.bundleId,
                                                  transfer.direction,
                                                  transfer.status,
                                                  transfer.length,
                                                  transfer.bufLength,
                                                  transfer.isStream
                    );
    }
    return true;
}
