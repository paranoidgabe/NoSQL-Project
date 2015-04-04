
#ifndef OS_CONSTANTS_H_
#define OS_CONSTANTS_H_

#include <cstdint>

namespace os {

    enum FilePosition { BEG , CUR , END };
    enum FileStatus { OPEN , CLOSED , DELETED };
    enum LockType { READ , WRITE };

    static const uint64_t KB = 1024;
    static const uint64_t MB = 1024 * KB;
    static const uint64_t GB = 1024 * MB;
    static const uint64_t TB = 1024 * GB;

    template<uint64_t V>
    uint64_t round( uint64_t t ) {
        return V * ( (t + V - 1) / V);
    }

}

#endif
