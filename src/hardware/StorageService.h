#pragma once
class SPIBusManager;

class StorageService {
public:
    bool mount(SPIBusManager &bus);
    void unmount();
    bool mounted() const { return mounted_ && !hostOwned_; }
    bool beginHostAccess();
    void endHostAccess() { hostOwned_ = false; }
private:
    bool mounted_ = false;
    bool hostOwned_ = false;
};
