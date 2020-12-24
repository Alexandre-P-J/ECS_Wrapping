#include <vector>
#include <limits>
#include <algorithm>
#include <functional>
#include "Storage.h"

class Registry {
    // das bad. delete->UB
    std::vector<void*> storages;

public:
    ~Registry() {
        for (auto ptr : storages) {
            delete ptr;
        }
    }
}

