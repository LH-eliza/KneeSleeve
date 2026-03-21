/*
 * S3 EMG — LEG 1 | WROOM: 08:3A:F2:52:88:E8
 * Board: ESP32S3 Dev Module | USB CDC On Boot: Enabled
 */
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

static const char* MAC_A = "88:13:bf:14:e3:76";
static const char* MAC_B = "88:13:bf:55:33:0e";
uint8_t wroomMAC[] = {0x08, 0x3A, 0xF2, 0x52, 0x88, 0xE8};
#define SAMPLE_RATE 200.0

typedef struct { float emg1,emg2; uint8_t conn1,conn2; } EmgPacket;
EmgPacket emgPkt;
static BLEUUID serviceUUID("ec3af789-2154-49f4-a9fc-bc6c88e9e930");
struct OnePole { float alpha,prev; void setup(float fc,float fs){float dt=1.0f/fs;alpha=dt/(dt+(1.0f/(2.0f*3.1415926f*fc)));prev=0;} float lowpass(float x){prev=prev+alpha*(x-prev);return prev;} float highpass(float x){float y=x-prev+alpha*prev;prev=x;return y;} };
OnePole hpA,lpA,hpB,lpB;
BLEScan* pBLEScan=nullptr; BLEClient* clientA=nullptr; BLEClient* clientB=nullptr;
BLERemoteCharacteristic* charA=nullptr; BLERemoteCharacteristic* charB=nullptr;
volatile float valueA=0,valueB=0; volatile bool connectedA=false,connectedB=false;
BLEAdvertisedDevice* foundDeviceA=nullptr; BLEAdvertisedDevice* foundDeviceB=nullptr;

void parseNotifyData(uint8_t* data,size_t length,volatile float* dest){if(length==0)return;bool isString=true;for(size_t i=0;i<length;i++){if(data[i]==0)break;if(data[i]!='.'&&data[i]!='-'&&(data[i]<'0'||data[i]>'9')){isString=false;break;}}if(isString&&length<20){char buf[21];size_t cl=(length<20)?length:20;memcpy(buf,data,cl);buf[cl]='\0';*dest=atof(buf);}else if(length>=2){*dest=(float)(int16_t)((data[1]<<8)|data[0]);}else if(length==1){*dest=(float)data[0];}}
static void notifyA(BLERemoteCharacteristic*,uint8_t* d,size_t l,bool){parseNotifyData(d,l,&valueA);}
static void notifyB(BLERemoteCharacteristic*,uint8_t* d,size_t l,bool){parseNotifyData(d,l,&valueB);}
class ClientCallbackA:public BLEClientCallbacks{void onConnect(BLEClient*){connectedA=true;}void onDisconnect(BLEClient*){connectedA=false;Serial.println("[A] Disconnected!");}};
class ClientCallbackB:public BLEClientCallbacks{void onConnect(BLEClient*){connectedB=true;}void onDisconnect(BLEClient*){connectedB=false;Serial.println("[B] Disconnected!");}};
bool macMatch(const char* f,const char* t){String a=String(f);String b=String(t);a.toLowerCase();b.toLowerCase();return a==b;}
void scanForDevices(){Serial.println("Scanning (10s)...");Serial.flush();pBLEScan->setActiveScan(true);pBLEScan->setInterval(100);pBLEScan->setWindow(99);BLEScanResults* r=pBLEScan->start(10,false);if(!r){Serial.println("Scan failed!");return;}int c=r->getCount();Serial.printf("Found %d devices\n",c);for(int i=0;i<c;i++){BLEAdvertisedDevice d=r->getDevice(i);String addr=String(d.getAddress().toString().c_str());if(!foundDeviceA&&macMatch(addr.c_str(),MAC_A)){foundDeviceA=new BLEAdvertisedDevice(d);Serial.printf("  EMG A: %s\n",addr.c_str());}if(!foundDeviceB&&macMatch(addr.c_str(),MAC_B)){foundDeviceB=new BLEAdvertisedDevice(d);Serial.printf("  EMG B: %s\n",addr.c_str());}}pBLEScan->clearResults();}
BLERemoteCharacteristic* subscribeToFirstNotifiable(BLEClient* client,void(*cb)(BLERemoteCharacteristic*,uint8_t*,size_t,bool),const char* label){std::map<std::string,BLERemoteService*>* svcs=client->getServices();if(!svcs)return nullptr;for(auto& sp:*svcs){String u=String(sp.second->getUUID().toString().c_str());if(u.length()<=8)continue;std::map<std::string,BLERemoteCharacteristic*>* chars=sp.second->getCharacteristics();if(!chars)continue;for(auto& cp2:*chars){BLERemoteCharacteristic* c=cp2.second;if(c->canNotify()){c->registerForNotify(cb);BLERemoteDescriptor* desc=c->getDescriptor(BLEUUID((uint16_t)0x2902));if(desc){uint8_t en[]={0x01,0x00};desc->writeValue(en,2);}Serial.printf("[%s] Subscribed!\n",label);return c;}}}return nullptr;}
bool connectAndSubscribe(BLEClient* client,BLEAdvertisedDevice* device,BLERemoteCharacteristic** outChar,void(*cb)(BLERemoteCharacteristic*,uint8_t*,size_t,bool),const char* label){Serial.printf("[%s] Connecting...\n",label);Serial.flush();if(!client->connect(device)){Serial.printf("[%s] FAILED!\n",label);return false;}Serial.printf("[%s] Connected!\n",label);Serial.flush();delay(500);*outChar=subscribeToFirstNotifiable(client,cb,label);if(!*outChar){client->disconnect();return false;}Serial.printf("[%s] Ready!\n",label);Serial.flush();return true;}

void setup(){Serial.begin(115200);while(!Serial){delay(10);}delay(2000);
Serial.println("=== LEG 1 EMG ===");
WiFi.mode(WIFI_STA);WiFi.disconnect();esp_wifi_set_channel(1,WIFI_SECOND_CHAN_NONE);
if(esp_now_init()!=ESP_OK){Serial.println("ESP-NOW FAIL");}else{esp_now_peer_info_t peer={};memcpy(peer.peer_addr,wroomMAC,6);peer.channel=1;peer.encrypt=false;esp_now_add_peer(&peer);Serial.println("ESP-NOW OK");}
hpA.setup(20.0f,SAMPLE_RATE);lpA.setup(500.0f,SAMPLE_RATE);hpB.setup(20.0f,SAMPLE_RATE);lpB.setup(500.0f,SAMPLE_RATE);
BLEDevice::init("LEG1_EMG");pBLEScan=BLEDevice::getScan();
int sa=0;while(!foundDeviceA||!foundDeviceB){scanForDevices();sa++;if(sa>=3)Serial.println("Check shields ON");if(!foundDeviceA||!foundDeviceB){Serial.println("Retry 3s...");delay(3000);}}
Serial.println("Both found!");clientA=BLEDevice::createClient();clientA->setClientCallbacks(new ClientCallbackA());
if(!connectAndSubscribe(clientA,foundDeviceA,&charA,notifyA,"A")){Serial.println("FATAL A");while(1)delay(1000);}
delay(2000);clientB=BLEDevice::createClient();clientB->setClientCallbacks(new ClientCallbackB());
if(!connectAndSubscribe(clientB,foundDeviceB,&charB,notifyB,"B")){Serial.println("FATAL B");while(1)delay(1000);}
Serial.println("Leg 1 EMG ready!");}

uint32_t lastS=0,lastN=0;
void loop(){uint32_t n=millis();if(n-lastS<(uint32_t)(1000.0f/SAMPLE_RATE))return;lastS=n;
float eA=lpA.lowpass(hpA.highpass(valueA));float eB=lpB.lowpass(hpB.highpass(valueB));
if(n-lastN>=50){lastN=n;emgPkt.emg1=eA;emgPkt.emg2=eB;emgPkt.conn1=connectedA?1:0;emgPkt.conn2=connectedB?1:0;esp_now_send(wroomMAC,(uint8_t*)&emgPkt,sizeof(emgPkt));}}
