/*
 * S3 IMU+Encoder — LEG 2 | WROOM: 08:3A:F2:52:88:E8
 * Board: ESP32S3 Dev Module | USB CDC On Boot: Enabled
 */
#include <esp_now.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <Wire.h>
#include <math.h>

uint8_t wroomMAC[] = {0x08, 0x3A, 0xF2, 0x52, 0x88, 0xE8};
#define ENC_DT 10
#define ENC_CLK 11
#define IMU1_SDA 39
#define IMU1_SCL 35
#define IMU2_SDA 18
#define IMU2_SCL 15
#define IMU_ADDR 0x6A
#define LED_PIN 2

TwoWire I2C_1=TwoWire(0); TwoWire I2C_2=TwoWire(1);
float calibrationAngle=180.0,smoothedAngle=180;
volatile long encCount=0; volatile uint8_t encLast=0; float encAngle=0.0;
#define ENC_DEG_PER_DETENT 15.0f

void IRAM_ATTR encoderISR(){uint8_t c=digitalRead(ENC_CLK);uint8_t d=digitalRead(ENC_DT);uint8_t s=(c<<1)|d;if(encLast==0b10&&s==0b00)encCount++;else if(encLast==0b00&&s==0b10)encCount--;else if(encLast==0b01&&s==0b11)encCount++;else if(encLast==0b11&&s==0b01)encCount--;encLast=s;}

typedef struct{float knee,raw,enc;long encCount;float a1x,a1y,a1z,a2x,a2y,a2z;uint8_t calDone;float calOffset;uint8_t encReset;}ImuPacket;
ImuPacket imuPkt;
typedef struct{uint8_t cmd;}CmdPacket;

void i2cWrite(TwoWire &w,uint8_t r,uint8_t v){w.beginTransmission(IMU_ADDR);w.write(r);w.write(v);w.endTransmission();}
uint8_t i2cRead(TwoWire &w,uint8_t r){w.beginTransmission(IMU_ADDR);w.write(r);w.endTransmission(false);w.requestFrom((uint8_t)IMU_ADDR,(uint8_t)1);return w.read();}
bool initIMU(TwoWire &w,int sda,int scl,const char* label){
  Serial.printf("[%s] Init SDA=%d SCL=%d\n",label,sda,scl);
  w.begin(sda,scl);w.setClock(400000);delay(100);
  w.beginTransmission(IMU_ADDR);uint8_t err=w.endTransmission();
  Serial.printf("[%s] Probe 0x%02X: %s\n",label,IMU_ADDR,err==0?"ACK":"NACK");
  if(err!=0)return false;
  uint8_t who=i2cRead(w,0x0F);Serial.printf("[%s] WHO_AM_I=0x%02X\n",label,who);
  if(who!=0x6C)return false;
  i2cWrite(w,0x12,0x01);delay(50);i2cWrite(w,0x10,0x40);delay(10);
  Serial.printf("[%s] OK!\n",label);return true;}
void readAccel(TwoWire &w,float*ax,float*ay,float*az){uint8_t b[6];w.beginTransmission(IMU_ADDR);w.write(0x28);w.endTransmission(false);w.requestFrom((uint8_t)IMU_ADDR,(uint8_t)6);for(int i=0;i<6&&w.available();i++)b[i]=w.read();*ax=(int16_t)(b[1]<<8|b[0])*0.000122f;*ay=(int16_t)(b[3]<<8|b[2])*0.000122f;*az=(int16_t)(b[5]<<8|b[4])*0.000122f;}
float calcAngle(float x1,float y1,float z1,float x2,float y2,float z2){float m1=sqrt(x1*x1+y1*y1+z1*z1);float m2=sqrt(x2*x2+y2*y2+z2*z2);return acos(constrain((x1*x2+y1*y2+z1*z2)/(m1*m2),-1.0,1.0))*180.0/PI;}

void onRecv(const esp_now_recv_info_t* info,const uint8_t* data,int len){
  if(len==sizeof(CmdPacket)){CmdPacket*cmd=(CmdPacket*)data;
    if(cmd->cmd==1){digitalWrite(LED_PIN,HIGH);float sum=0;for(int i=0;i<20;i++){float ax1,ay1,az1,ax2,ay2,az2;readAccel(I2C_1,&ax1,&ay1,&az1);readAccel(I2C_2,&ax2,&ay2,&az2);sum+=calcAngle(ax1,ay1,az1,ax2,ay2,az2);delay(50);}calibrationAngle=sum/20.0;digitalWrite(LED_PIN,LOW);imuPkt.calDone=1;imuPkt.calOffset=calibrationAngle;Serial.printf("[CAL] %.1f\n",calibrationAngle);}
    else if(cmd->cmd==2){encCount=0;encAngle=0;imuPkt.encReset=1;}}}

void setup(){Serial.begin(115200);delay(3000);
Serial.println("=== LEG 2 IMU+Enc ===");pinMode(LED_PIN,OUTPUT);
pinMode(ENC_CLK,INPUT_PULLUP);pinMode(ENC_DT,INPUT_PULLUP);encLast=(digitalRead(ENC_CLK)<<1)|digitalRead(ENC_DT);
attachInterrupt(digitalPinToInterrupt(ENC_CLK),encoderISR,CHANGE);attachInterrupt(digitalPinToInterrupt(ENC_DT),encoderISR,CHANGE);Serial.println("Encoder OK");
bool i1=initIMU(I2C_1,IMU1_SDA,IMU1_SCL,"IMU1");bool i2=initIMU(I2C_2,IMU2_SDA,IMU2_SCL,"IMU2");
if(!i1||!i2)Serial.println("WARNING: IMU issue");
nvs_flash_init();esp_netif_init();esp_event_loop_create_default();esp_netif_create_default_wifi_sta();
wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();esp_wifi_init(&cfg);esp_wifi_set_mode(WIFI_MODE_STA);esp_wifi_start();esp_wifi_set_channel(1,WIFI_SECOND_CHAN_NONE);
if(esp_now_init()!=ESP_OK){Serial.println("ESP-NOW FAIL");}else{esp_now_peer_info_t peer={};memcpy(peer.peer_addr,wroomMAC,6);peer.channel=1;peer.encrypt=false;esp_now_add_peer(&peer);esp_now_register_recv_cb(onRecv);Serial.println("ESP-NOW OK");}
memset(&imuPkt,0,sizeof(imuPkt));Serial.println("Ready!");}

unsigned long lastSample=0;
void loop(){if(millis()-lastSample>=50){lastSample=millis();float a1x,a1y,a1z,a2x,a2y,a2z;readAccel(I2C_1,&a1x,&a1y,&a1z);readAccel(I2C_2,&a2x,&a2y,&a2z);float raw=calcAngle(a1x,a1y,a1z,a2x,a2y,a2z);smoothedAngle=smoothedAngle*0.5+raw*0.5;float knee=abs(calibrationAngle-smoothedAngle);encAngle=(float)encCount*ENC_DEG_PER_DETENT;
imuPkt.knee=knee;imuPkt.raw=smoothedAngle;imuPkt.enc=encAngle;imuPkt.encCount=encCount;imuPkt.a1x=a1x;imuPkt.a1y=a1y;imuPkt.a1z=a1z;imuPkt.a2x=a2x;imuPkt.a2y=a2y;imuPkt.a2z=a2z;
esp_now_send(wroomMAC,(uint8_t*)&imuPkt,sizeof(imuPkt));imuPkt.calDone=0;imuPkt.encReset=0;}}
