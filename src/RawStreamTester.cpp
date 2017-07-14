
#include <cstdio>
#include <cmath>

#include "RawStreamTester.h"

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
void TStreamTester::fillBuf(uint8_t* buf, unsigned len)
{
    ++mTxBufs;
    len /= sizeof(TData);
    TData* dataBuf = reinterpret_cast<TData*>(buf);
    for(unsigned i = 0; i < len; ++i) {
        *dataBuf++ = mValue;
        mValue += mIncrement;
    }
}

//------------------------------------------------------------------------------
unsigned TStreamTester::checkBuf(uint8_t* buf, unsigned len, bool printDiff)
{
    ++mRxBufs;
    len /= sizeof(TData);
    TData* dataBuf = reinterpret_cast<TData*>(buf);
    unsigned errNum = 0;
    for(unsigned i = 0; i < len; ++i,++dataBuf) {
        if(*dataBuf != mValue) {
            ++errNum;
			if (printDiff) {
                                TData delta = (TData)(std::abs((int64_t)(mValue)-(int64_t)(*dataBuf)));
                printf("[DATA CHECK ERROR] idx: %4u, expected: %10u, received: %10u, delta: %10u, errNum: %5d\n", i, mValue, *dataBuf, delta, errNum);
			}
            mValue = *dataBuf;
        }
        mValue += mIncrement;
    }
    return errNum;
}
