#if !defined(RAW_STREAM_TESTER_H)
#define RAW_STREAM_TESTER_H

#include <cstdint>

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
class TStreamTester
{
	public:
		typedef uint32_t TData;

        TStreamTester(TData  increment = 1, TData startValue = 0) : mIncrement(increment), mValue(startValue), mRxBufs(0), mTxBufs(0) {}
		void fillBuf(uint8_t* buf, unsigned len);
		unsigned checkBuf(uint8_t* buf, unsigned len, bool printDiff = false);
        uint64_t getRxBufs() const { return mRxBufs; }
        uint64_t getTxBufs() const { return mTxBufs; }

	private:
		const TData mIncrement;
        TData	 	mValue;
        uint64_t    mRxBufs;
        uint64_t    mTxBufs;
};

#endif	// RAW_STREAM_TESTER_H
