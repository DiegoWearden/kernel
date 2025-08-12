#include "utils.h"
#include "core.h"

template<class T>
class PerCPU {
private:
    T data[CORE_COUNT];
public:
    inline T& forCPU(int id) {
        return data[id];
    }

    inline T& mine() {
        return forCPU(getCoreID());
    }
};