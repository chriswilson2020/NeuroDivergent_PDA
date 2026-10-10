#pragma once
#include "MessagingProtocol.h"
#include <stdint.h>
class HardwareManager; class MessageStore; class MessagingSettingsStore; class RTCService; class Shell;

struct RadioRuntimeStats { uint64_t receiveMs=0,transmitMs=0,sleepMs=0; uint32_t packetsReceived=0,packetsSent=0; };
class MessagingService {
public:
    using OpenConversationCallback=void(*)(void*context,uint64_t contactId);
    bool begin(HardwareManager&,MessageStore&,MessagingSettingsStore&,RTCService&,Shell&);
    void update(); void suspend(); void resume();
    bool send(uint64_t contactId,const char*text);
    bool retry(uint32_t localId);
    bool startPairing(const char*code); void stopPairing(); bool pairing()const{return pairing_;}
    const char*status()const{return status_;} RadioRuntimeStats stats()const;
    void configurationChanged();
    bool processorSleepReady() const;
    void latchProcessorWake();
    void setOpenConversationCallback(OpenConversationCallback callback,void*context){openCallback_=callback;openContext_=context;}
private:
    enum class State:uint8_t{Sleeping,Receiving,Transmitting,WaitingAck};
    static void radioIrq(); static void openMessages(void*context);
    void onIrq(); void receivePacket(); bool transmit(const uint8_t*packet,size_t length,bool ackPacket=false);
    bool transmitPending(); void queueAck(uint64_t contact,uint32_t messageId);
    void processPacket(const uint8_t*packet,size_t length); void notifyIncoming(uint64_t contact,const char*text);
    void setState(State state); void startReceive(); uint32_t nowEpoch();
    static volatile bool irq_; static MessagingService*active_;
    HardwareManager*hardware_=nullptr;MessageStore*messages_=nullptr;MessagingSettingsStore*settings_=nullptr;RTCService*rtc_=nullptr;Shell*shell_=nullptr;
    State state_=State::Sleeping;uint32_t stateSince_=0,nextTxMs_=0,ackDeadline_=0,lastPairHello_=0,pairUntil_=0,currentLocalId_=0;uint8_t currentAttempts_=0;
    bool suspended_=false,pairing_=false,sendingAck_=false;char pairCode_[17]{};char status_[64]="Messaging disabled";
    uint64_t ackContact_=0;uint32_t ackMessageId_=0;bool ackPending_=false;
    uint64_t notificationContact_=0;OpenConversationCallback openCallback_=nullptr;void*openContext_=nullptr;
    uint8_t tx_[255]{},rx_[255]{};size_t txLength_=0;RadioRuntimeStats stats_{};
};
