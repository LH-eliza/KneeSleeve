/*
 * uOBionics Web Server (WROOM)
 * Receives data from S3 via ESP-NOW, serves webpage+WebSocket
 * Board: ESP32 Dev Module
 */

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <esp_now.h>
#include <esp_wifi.h>

const char* WIFI_SSID = "uOBionics";
const char* WIFI_PASS = "knee2026";

WebServer server(80);
WebSocketsServer webSocket(81);

// S3 MACs for ESP-NOW
// All 4 S3 MACs
uint8_t s3emgL1[] = {0xDC, 0xB4, 0xD9, 0x0C, 0xD1, 0x04};
uint8_t s3imuL1[] = {0xDC, 0xB4, 0xD9, 0x0C, 0x64, 0x58};
uint8_t s3emgL2[] = {0xDC, 0xB4, 0xD9, 0x0C, 0x63, 0xAC};
uint8_t s3imuL2[] = {0xDC, 0xB4, 0xD9, 0x05, 0x2B, 0xB0};

typedef struct { float emg1,emg2; uint8_t conn1,conn2; } EmgPacket;
typedef struct { float knee,raw,enc; long encCount; float a1x,a1y,a1z,a2x,a2y,a2z; uint8_t calDone; float calOffset; uint8_t encReset; } ImuPacket;
typedef struct { uint8_t cmd; } CmdPacket;

// Leg 1 data
float g1Knee=0,g1Raw=180,g1Emg1=0,g1Emg2=0,g1Enc=0;
long g1EncCnt=0;
bool g1Ec1=false,g1Ec2=false;
float g1a1x=0,g1a1y=0,g1a1z=0,g1a2x=0,g1a2y=0,g1a2z=0;
// Leg 2 data
float g2Knee=0,g2Raw=180,g2Emg1=0,g2Emg2=0,g2Enc=0;
long g2EncCnt=0;
bool g2Ec1=false,g2Ec2=false;
float g2a1x=0,g2a1y=0,g2a1z=0,g2a2x=0,g2a2y=0,g2a2z=0;
bool gNewData=false;

bool macEq(const uint8_t* a, const uint8_t* b) { for(int i=0;i<6;i++) if(a[i]!=b[i]) return false; return true; }

void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  const uint8_t* mac = info->src_addr;
  bool isL1 = macEq(mac, s3emgL1) || macEq(mac, s3imuL1);

  if (len == sizeof(EmgPacket)) {
    EmgPacket* p=(EmgPacket*)data;
    if(isL1){ g1Emg1=p->emg1; g1Emg2=p->emg2; g1Ec1=p->conn1; g1Ec2=p->conn2; }
    else    { g2Emg1=p->emg1; g2Emg2=p->emg2; g2Ec1=p->conn1; g2Ec2=p->conn2; }
  } else if (len == sizeof(ImuPacket)) {
    ImuPacket* p=(ImuPacket*)data;
    if(isL1){
      g1Knee=p->knee; g1Raw=p->raw; g1Enc=p->enc; g1EncCnt=p->encCount;
      g1a1x=p->a1x; g1a1y=p->a1y; g1a1z=p->a1z;
      g1a2x=p->a2x; g1a2y=p->a2y; g1a2z=p->a2z;
      if(p->calDone) webSocket.broadcastTXT("{\"calDone\":true,\"offset\":"+String(p->calOffset,1)+",\"leg\":1}");
      if(p->encReset) webSocket.broadcastTXT("{\"encReset\":true,\"leg\":1}");
    } else {
      g2Knee=p->knee; g2Raw=p->raw; g2Enc=p->enc; g2EncCnt=p->encCount;
      g2a1x=p->a1x; g2a1y=p->a1y; g2a1z=p->a1z;
      g2a2x=p->a2x; g2a2y=p->a2y; g2a2z=p->a2z;
      if(p->calDone) webSocket.broadcastTXT("{\"calDone\":true,\"offset\":"+String(p->calOffset,1)+",\"leg\":2}");
      if(p->encReset) webSocket.broadcastTXT("{\"encReset\":true,\"leg\":2}");
    }
    gNewData=true;
  }
}

const char WEBPAGE[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>uOBionics</title>
<style>
*{margin:0;padding:0;box-sizing:border-box;-webkit-tap-highlight-color:transparent}
:root{
  --bg:#03070f;
  --card:#080f1a;
  --card2:#0c1520;
  --border:#1a2840;
  --border2:#243550;
  --text:#e8f0f8;
  --dim:#5a7090;
  --muted:#1a2840;
  --accent:#00d4f0;
  --accent2:#0099b8;
  --garnet:#c41e3a;
  --success:#00e676;
  --warning:#ffab40;
  --danger:#ff5252;
  --glow:rgba(0,212,240,0.12);
}
html,body{height:100%;overflow:hidden}
body{font-family:-apple-system,system-ui,sans-serif;background:var(--bg);color:var(--text)}
.screen{display:none!important;flex-direction:column;height:100%;overflow-y:auto;position:absolute;inset:0;background:var(--bg)}
.screen.active{display:flex!important}

/* WELCOME */
#welcome{justify-content:center;align-items:center;text-align:center;padding:32px 24px;background:radial-gradient(ellipse at 50% 20%,#0a1830 0%,var(--bg) 65%)}
.logo{width:88px;height:88px;background:linear-gradient(145deg,var(--garnet),#e63950);border-radius:24px;display:flex;align-items:center;justify-content:center;font-size:2.6rem;box-shadow:0 16px 48px rgba(196,30,58,0.35);margin-bottom:20px}
.w-title{font-size:1.9rem;font-weight:700;letter-spacing:-0.5px;margin-bottom:6px}
.w-title span{color:var(--garnet)}
.w-sub{color:var(--dim);font-size:0.85rem;margin-bottom:32px}
.w-status{display:inline-flex;align-items:center;gap:8px;padding:10px 20px;background:var(--card);border:1px solid var(--border);border-radius:24px;font-size:0.8rem;margin-bottom:32px}
.sdot{width:8px;height:8px;border-radius:50%;background:var(--danger);transition:all 0.3s}
.sdot.ok{background:var(--success);box-shadow:0 0 10px var(--success)}
.w-btn{width:100%;max-width:280px;padding:16px;background:linear-gradient(135deg,var(--accent),var(--accent2));color:#020d16;border:none;border-radius:14px;font-family:inherit;font-size:1rem;font-weight:700;cursor:pointer;box-shadow:0 8px 32px rgba(0,212,240,0.25)}
.w-btn:disabled{opacity:0.4;cursor:default}

/* CALIBRATION */
#calibrate{padding:0;justify-content:flex-start}
.cal-header{padding:20px 20px 0;display:flex;align-items:center;gap:12px}
.cal-back{width:34px;height:34px;background:var(--card);border:1px solid var(--border);border-radius:10px;display:flex;align-items:center;justify-content:center;cursor:pointer;flex-shrink:0}
.cal-header-title{font-size:1rem;font-weight:600}
.cal-steps{display:flex;gap:6px;padding:16px 20px 0}
.cal-step{flex:1;height:3px;border-radius:2px;background:var(--muted);transition:background 0.4s}
.cal-step.done{background:var(--success)}
.cal-step.active{background:var(--accent)}
.cal-body{flex:1;display:flex;flex-direction:column;align-items:center;justify-content:center;padding:20px;text-align:center}
.cal-icon{width:64px;height:64px;border-radius:18px;display:flex;align-items:center;justify-content:center;font-size:2rem;margin:0 auto 16px;border:1px solid var(--border);background:linear-gradient(135deg,#0a1830,#102040)}
.cal-step-title{font-size:1.3rem;font-weight:600;margin-bottom:8px}
.cal-step-desc{font-size:0.85rem;color:var(--dim);line-height:1.6;max-width:260px;margin:0 auto 24px}
.cal-step-desc strong{color:var(--text)}
.cal-visual{width:120px;height:180px;margin:0 auto 24px}
.cal-visual svg{width:100%;height:100%;overflow:visible}
.cal-thigh{stroke:var(--accent);stroke-width:12;stroke-linecap:round}
.cal-shin-line{stroke:var(--accent);stroke-width:12;stroke-linecap:round;transition:all 0.1s}
.cal-jt{fill:var(--bg);stroke:var(--accent);stroke-width:3}
.cal-live{font-size:3rem;font-weight:300;color:var(--accent);font-family:monospace;line-height:1;margin-bottom:4px}
.cal-live-label{font-size:0.7rem;color:var(--dim);text-transform:uppercase;letter-spacing:1.5px;margin-bottom:24px}
.cal-tip{background:var(--card2);border:1px solid var(--border);border-radius:12px;padding:12px 16px;font-size:0.78rem;color:var(--dim);line-height:1.5;max-width:280px;margin:0 auto 24px;text-align:left}
.cal-btn{padding:16px 48px;background:var(--accent);color:#020d16;border:none;border-radius:12px;font-family:inherit;font-size:0.95rem;font-weight:700;cursor:pointer}
.cal-btn:disabled{opacity:0.4}
.cal-btn.done{background:var(--success);color:#020d16}
.cal-progress{width:240px;height:4px;background:var(--muted);border-radius:2px;overflow:hidden;margin-top:16px;display:none}
.cal-progress-fill{height:100%;width:0%;background:var(--accent);transition:width 2s linear}

/* MODE PICKER */
#modepick{justify-content:center;align-items:center;padding:32px 24px;text-align:center}
.mode-title{font-size:1.5rem;font-weight:700;margin-bottom:8px}
.mode-sub{font-size:0.85rem;color:var(--dim);margin-bottom:32px;line-height:1.5;max-width:280px}
.mode-cards{display:flex;flex-direction:column;gap:14px;width:100%;max-width:340px}
.mode-card{background:var(--card);border:2px solid var(--border);border-radius:18px;padding:22px 20px;cursor:pointer;text-align:left;transition:border-color 0.2s,background 0.2s}
.mode-card:active{transform:scale(0.98)}
.mode-card.simple-card:hover,.mode-card.simple-card.selected{border-color:var(--success);background:#0a1a0f}
.mode-card.advanced-card:hover,.mode-card.advanced-card.selected{border-color:var(--accent);background:#081520}
.mode-card-header{display:flex;align-items:center;gap:12px;margin-bottom:8px}
.mode-card-icon{font-size:1.8rem}
.mode-card-title{font-size:1rem;font-weight:700}
.mode-card-desc{font-size:0.78rem;color:var(--dim);line-height:1.5}
.mode-card-tags{display:flex;flex-wrap:wrap;gap:6px;margin-top:10px}
.mode-tag{font-size:0.62rem;padding:3px 8px;border-radius:20px;background:var(--muted);color:var(--dim)}
.mode-tag.green{background:#0a2010;color:var(--success)}
.mode-tag.blue{background:#081520;color:var(--accent)}

/* SIMPLE MONITOR */
#simple{padding:0 0 88px;overflow-y:auto}

/* ADVANCED MONITOR */
#monitor{padding:0 0 88px;overflow-y:auto}

/* Shared nav */
.mon-nav{display:flex;justify-content:space-between;align-items:center;padding:16px 16px 12px;position:sticky;top:0;background:var(--bg);z-index:10;border-bottom:1px solid var(--border)}
.mon-back{width:34px;height:34px;background:var(--card);border:1px solid var(--border);border-radius:10px;display:flex;align-items:center;justify-content:center;cursor:pointer}
.mon-nav-title{font-size:0.9rem;font-weight:600}
.live-badge{display:flex;align-items:center;gap:5px;font-size:0.65rem;color:var(--success);font-weight:600;letter-spacing:1px}
.live-dot{width:6px;height:6px;background:var(--success);border-radius:50%;animation:pulse 1.5s infinite}
@keyframes pulse{0%,100%{opacity:1;transform:scale(1)}50%{opacity:0.4;transform:scale(0.8)}}
.mode-switch-btn{font-size:0.65rem;color:var(--accent);background:none;border:1px solid var(--border);border-radius:8px;padding:4px 8px;cursor:pointer;font-family:inherit}

.section{padding:0 12px;margin-bottom:10px}
.card{background:var(--card);border:1px solid var(--border);border-radius:16px;padding:16px}
.card-title{font-size:0.58rem;color:var(--dim);text-transform:uppercase;letter-spacing:1.2px;margin-bottom:12px;display:flex;align-items:center;justify-content:space-between}
.card-title-info{font-size:0.58rem;color:var(--dim);cursor:pointer;text-decoration:underline;text-underline-offset:2px}

/* ── SIMPLE MODE STYLES ── */
.simple-angle-wrap{text-align:center;padding:28px 16px 20px}
.simple-angle-label{font-size:0.75rem;color:var(--dim);text-transform:uppercase;letter-spacing:2px;margin-bottom:8px}
.simple-angle-val{font-size:5.5rem;font-weight:200;font-family:monospace;line-height:1;color:var(--text)}
.simple-angle-desc{font-size:0.9rem;color:var(--dim);margin-top:8px;font-weight:500}

.simple-rom-wrap{padding:0 8px}
.simple-rom-label-row{display:flex;justify-content:space-between;margin-bottom:10px}
.simple-rom-label{font-size:0.75rem;font-weight:600}
.simple-rom-label.ext{color:var(--success)}
.simple-rom-label.flex{color:var(--accent)}
.simple-track{height:16px;background:var(--muted);border-radius:8px;position:relative;overflow:visible;margin-bottom:6px}
.simple-fill{height:100%;background:linear-gradient(90deg,var(--success),var(--accent));border-radius:8px;transition:width 0.15s ease;position:relative}
.simple-thumb{position:absolute;right:-8px;top:50%;transform:translateY(-50%);width:16px;height:16px;background:white;border-radius:50%;box-shadow:0 0 10px var(--accent)}
.simple-ticks{display:flex;justify-content:space-between;margin-top:4px}
.simple-tick{font-size:0.65rem;color:var(--dim)}

.simple-status-card{text-align:center;padding:20px}
.simple-status-icon{font-size:3.5rem;margin-bottom:10px;display:block}
.simple-status-title{font-size:1.3rem;font-weight:700;margin-bottom:6px}
.simple-status-desc{font-size:0.85rem;color:var(--dim);line-height:1.5;max-width:240px;margin:0 auto}

.simple-rep-card{display:flex;align-items:center;gap:20px;padding:20px}
.simple-rep-big{font-size:4.5rem;font-weight:200;font-family:monospace;color:var(--accent);line-height:1}
.simple-rep-info{flex:1}
.simple-rep-title{font-size:1rem;font-weight:700;margin-bottom:4px}
.simple-rep-sub{font-size:0.8rem;color:var(--dim);line-height:1.4}
.simple-rep-goal{margin-top:12px}
.simple-rep-goal-bar{height:8px;background:var(--muted);border-radius:4px;overflow:hidden;margin-top:6px}
.simple-rep-goal-fill{height:100%;background:linear-gradient(90deg,var(--success),var(--accent));border-radius:4px;transition:width 0.3s}
.simple-rep-goal-label{font-size:0.65rem;color:var(--dim);display:flex;justify-content:space-between;margin-top:4px}

.simple-steps-card{padding:16px}
.simple-step-row{display:flex;align-items:flex-start;gap:12px;padding:10px 0;border-bottom:1px solid var(--border)}
.simple-step-row:last-child{border-bottom:none}
.simple-step-num{width:28px;height:28px;border-radius:50%;display:flex;align-items:center;justify-content:center;font-size:0.75rem;font-weight:700;flex-shrink:0;background:var(--muted);color:var(--dim)}
.simple-step-num.active{background:var(--accent);color:#020d16}
.simple-step-num.done{background:var(--success);color:#020d16}
.simple-step-text{flex:1}
.simple-step-title{font-size:0.85rem;font-weight:600;margin-bottom:2px}
.simple-step-desc{font-size:0.75rem;color:var(--dim);line-height:1.4}

.simple-controls{position:fixed;bottom:0;left:0;right:0;background:linear-gradient(transparent,var(--bg) 25%);padding:20px 12px 16px;display:flex;gap:8px}
.simple-ctrl{flex:1;padding:16px;border:none;border-radius:12px;font-family:inherit;font-size:0.9rem;font-weight:700;cursor:pointer}
.simple-ctrl-start{background:var(--success);color:#020d16}
.simple-ctrl-reset{background:var(--card);color:var(--text);border:1px solid var(--border)}

/* ── ADVANCED MODE STYLES ── */
.hero-card{padding:20px 16px 16px}
.angle-ring-wrap{position:relative;width:160px;height:160px;margin:0 auto 16px}
.angle-svg{width:100%;height:100%;transform:rotate(-90deg)}
.ring-bg{fill:none;stroke:var(--muted);stroke-width:8}
.ring-fill{fill:none;stroke:var(--accent);stroke-width:8;stroke-linecap:round;stroke-dasharray:439;stroke-dashoffset:439;transition:stroke-dashoffset 0.15s ease;filter:drop-shadow(0 0 10px rgba(0,212,240,0.4))}
.ring-inner{position:absolute;inset:0;display:flex;flex-direction:column;align-items:center;justify-content:center}
.ring-label{font-size:0.5rem;color:var(--accent);text-transform:uppercase;letter-spacing:2px;margin-bottom:2px}
.ring-val{font-size:3.2rem;font-weight:300;font-family:monospace;line-height:1}
.ring-unit{font-size:0.6rem;color:var(--dim);margin-top:4px;text-transform:uppercase;letter-spacing:1px}
.rom-axis{margin-bottom:4px}
.rom-axis-label{display:flex;justify-content:space-between;font-size:0.6rem;color:var(--dim);margin-bottom:6px}
.rom-track-wrap{position:relative;height:10px;background:var(--muted);border-radius:5px;margin-bottom:2px}
.rom-track-fill{height:100%;background:linear-gradient(90deg,var(--success),var(--accent));border-radius:5px;transition:width 0.15s ease;position:relative}
.rom-thumb{position:absolute;right:-5px;top:50%;transform:translateY(-50%);width:10px;height:10px;background:white;border-radius:50%;box-shadow:0 0 8px var(--accent)}
.rom-ticks{display:flex;justify-content:space-between;margin-top:4px}
.rom-tick{font-size:0.55rem;color:var(--dim)}
.stats3{display:grid;grid-template-columns:repeat(3,1fr);gap:1px;background:var(--border);border-radius:10px;overflow:hidden;margin-top:12px}
.stat{background:var(--card);padding:12px 8px;text-align:center;position:relative}
.stat-val{font-size:1.25rem;font-weight:300;font-family:monospace}
.stat-label{font-size:0.5rem;color:var(--dim);text-transform:uppercase;letter-spacing:0.8px;margin-top:3px}
.stat-info{position:absolute;top:6px;right:6px;width:14px;height:14px;border-radius:50%;background:var(--muted);font-size:0.5rem;color:var(--dim);display:flex;align-items:center;justify-content:center;cursor:pointer}
.leg-card{display:flex;flex-direction:column;gap:10px;align-items:center}
.leg-svg-wrap{flex-shrink:0;width:75px}
.leg-svg{width:75px;height:115px}
.leg-right{flex:1}
.leg-metric{padding:5px 8px;background:var(--bg);border-radius:6px;margin-bottom:4px;display:flex;justify-content:space-between;align-items:center}
.leg-metric-label{font-size:0.55rem;color:var(--dim);display:flex;align-items:center;gap:3px}
.leg-metric-val{font-size:0.72rem;font-family:monospace;font-weight:500}
.tip-icon{width:14px;height:14px;border-radius:50%;background:var(--muted);font-size:0.48rem;color:var(--dim);display:inline-flex;align-items:center;justify-content:center;cursor:pointer;flex-shrink:0}
.ex-card{display:flex;align-items:center;gap:16px}
.ex-big{font-size:3rem;font-weight:200;color:var(--accent);font-family:monospace;line-height:1}
.ex-detail{flex:1}
.ex-name{font-size:0.9rem;font-weight:500;margin-bottom:2px}
.ex-sub{font-size:0.65rem;color:var(--dim)}
.ex-mini{display:grid;grid-template-columns:1fr 1fr;gap:6px;margin-top:10px}
.ex-mini-item{background:var(--bg);border-radius:6px;padding:8px;text-align:center}
.ex-mini-label{font-size:0.5rem;color:var(--dim);text-transform:uppercase}
.ex-mini-val{font-size:0.85rem;font-family:monospace;margin-top:2px}
.graph-container{position:relative;height:100px;background:var(--bg);border-radius:10px;overflow:hidden}
#graphCanvas{position:absolute;inset:0;width:100%;height:100%}
.graph-y-labels{position:absolute;left:4px;top:0;bottom:0;display:flex;flex-direction:column;justify-content:space-between;padding:4px 0;pointer-events:none}
.graph-y-label{font-size:0.48rem;color:var(--dim);font-family:monospace}
.graph-x-labels{display:flex;justify-content:space-between;padding:4px 2px 0;font-size:0.5rem;color:var(--dim);font-family:monospace}
.imu-section{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.imu-box{background:var(--bg);border-radius:10px;padding:12px}
.imu-title{font-size:0.6rem;text-transform:uppercase;letter-spacing:1px;margin-bottom:8px;display:flex;align-items:center;gap:6px}
.imu-dot{width:6px;height:6px;border-radius:50%}
.imu-axes{display:grid;grid-template-columns:repeat(3,1fr);gap:4px}
.imu-axis{background:var(--card);border-radius:6px;padding:6px 4px;text-align:center}
.imu-axis-label{font-size:0.48rem;color:var(--dim);text-transform:uppercase;letter-spacing:0.5px}
.imu-axis-val{font-size:0.72rem;font-family:monospace;margin-top:2px}
.controls{position:fixed;bottom:0;left:0;right:0;background:linear-gradient(transparent,var(--bg) 25%);padding:20px 12px 16px;display:flex;gap:8px}
.ctrl{flex:1;padding:14px;border:none;border-radius:12px;font-family:inherit;font-size:0.85rem;font-weight:600;cursor:pointer}
.ctrl-rec{background:var(--card);color:var(--text);border:1px solid var(--border)}
.ctrl-rec.on{background:var(--danger);border-color:var(--danger);color:white}
.ctrl-cal{background:var(--accent);color:#020d16}

/* PONG TEST */
#test{display:flex;flex-direction:column}
.t-head{display:flex;justify-content:space-between;align-items:center;padding:12px 16px;background:var(--card);border-bottom:1px solid var(--border)}
.t-back{width:34px;height:34px;background:var(--muted);border-radius:8px;display:flex;align-items:center;justify-content:center;cursor:pointer}
.t-info{text-align:center}.t-phase{font-size:0.55rem;color:var(--dim);text-transform:uppercase}.t-target{font-size:0.9rem;font-weight:600;color:var(--accent)}
.t-trials{text-align:right}.t-num{font-size:1.1rem;font-weight:600}.t-label{font-size:0.5rem;color:var(--dim)}
.t-game{flex:1;position:relative;background:#050810}.t-canvas{width:100%;height:100%}
.t-overlay{position:absolute;inset:0;background:rgba(5,8,16,0.97);display:flex;flex-direction:column;align-items:center;justify-content:center;padding:24px;text-align:center}
.t-overlay.hidden{display:none}
.t-ov-title{font-size:1.4rem;font-weight:700;color:var(--accent);margin-bottom:8px}
.t-ov-desc{color:var(--dim);font-size:0.85rem;line-height:1.5;max-width:280px;margin-bottom:16px}
.t-ov-zones{display:flex;gap:8px;margin-bottom:20px}
.t-zone{background:var(--card);border:1px solid var(--border);border-radius:10px;padding:12px 16px;text-align:center}
.t-zone-num{font-size:1.2rem;font-weight:600}.t-zone-num.z1{color:var(--success)}.t-zone-num.z2{color:var(--warning)}.t-zone-num.z3{color:var(--danger)}
.t-zone-range{font-size:0.55rem;color:var(--dim)}
.t-ov-btn{padding:16px 40px;background:var(--accent);color:#020d16;border:none;border-radius:12px;font-size:1rem;font-weight:600;cursor:pointer}
.t-ov-hint{font-size:0.65rem;color:var(--muted);margin-top:14px}
.t-hud{position:absolute;bottom:0;left:0;right:0;padding:12px 16px;background:linear-gradient(transparent,rgba(5,8,16,0.95))}
.t-bar{height:8px;background:var(--muted);border-radius:4px;position:relative;margin-bottom:10px}
.t-bar-fill{position:absolute;top:0;left:0;height:100%;background:var(--accent);border-radius:4px;box-shadow:0 0 10px rgba(0,212,240,0.15)}
.t-bar-target{position:absolute;top:-4px;bottom:-4px;width:3px;background:var(--warning);border-radius:2px}
.t-row{display:flex;justify-content:space-between;align-items:center}
.t-angle{font-size:1.6rem;font-weight:300;color:var(--accent)}
.t-feedback{padding:6px 14px;border-radius:20px;font-size:0.7rem;font-weight:600;background:var(--muted);color:var(--dim)}
.t-feedback.good{background:rgba(0,230,118,0.2);color:var(--success)}
.t-feedback.close{background:rgba(255,171,64,0.2);color:var(--warning)}
.t-feedback.far{background:rgba(255,82,82,0.2);color:var(--danger)}
.t-score{text-align:right}.t-score-val{font-size:1.2rem;font-weight:600}.t-score-label{font-size:0.5rem;color:var(--dim)}
.toast{position:fixed;top:50%;left:50%;transform:translate(-50%,-50%);padding:20px 40px;background:var(--success);color:#020d16;border-radius:16px;font-size:1.3rem;font-weight:700;opacity:0;transition:opacity 0.15s;z-index:99;pointer-events:none}
.toast.show{opacity:1}.toast.miss{background:var(--danger)}
#results{padding:24px;overflow-y:auto}
.r-main{text-align:center;margin-bottom:24px}
.r-icon{font-size:3rem;margin-bottom:8px}
.r-score-big{font-size:5rem;font-weight:200;color:var(--accent);text-shadow:0 0 40px rgba(0,212,240,0.15)}
.r-label2{font-size:0.7rem;color:var(--dim);text-transform:uppercase;letter-spacing:1px;margin-top:4px}
.r-verdict{font-size:1rem;color:var(--dim);margin-top:14px;line-height:1.5}.r-verdict strong{color:var(--text)}
.r-zones{display:flex;gap:10px;margin-bottom:16px}
.r-zone{flex:1;background:var(--card);border:1px solid var(--border);border-radius:12px;padding:12px;text-align:center}
.r-zone-bar{height:4px;border-radius:2px;margin-bottom:8px}.r-zone-bar.z1{background:var(--success)}.r-zone-bar.z2{background:var(--warning)}.r-zone-bar.z3{background:var(--danger)}
.r-zone-score{font-size:1.3rem;font-weight:500}.r-zone-name{font-size:0.5rem;color:var(--dim);margin-top:2px}
.r-metrics{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:16px}
.r-metric{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:14px;text-align:center}
.r-metric-val{font-size:1.2rem;font-weight:500}.r-metric-label{font-size:0.5rem;color:var(--dim);margin-top:2px;text-transform:uppercase}
.r-finding{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:14px;margin-bottom:16px}
.r-finding-title{font-size:0.6rem;color:var(--accent);text-transform:uppercase;margin-bottom:8px}
.r-finding-text{font-size:0.85rem;color:var(--dim);line-height:1.6}.r-finding-text strong{color:var(--text)}
.r-actions{display:flex;gap:12px}
.r-btn{flex:1;padding:14px;border-radius:12px;font-size:0.9rem;font-weight:600;cursor:pointer;border:none}
.r-btn-done{background:var(--card);color:var(--text);border:1px solid var(--border)}.r-btn-retry{background:var(--accent);color:#020d16}

/* Encoder card */
.enc-card{display:flex;align-items:center;gap:16px;padding:4px 0 8px}
.enc-big{font-size:3rem;font-weight:200;color:var(--warning);font-family:monospace;line-height:1;min-width:90px;text-align:right}
.enc-detail{flex:1}
.enc-name{font-size:0.9rem;font-weight:600;margin-bottom:2px}
.enc-sub{font-size:0.65rem;color:var(--dim);line-height:1.4}
.enc-mini{display:grid;grid-template-columns:1fr 1fr;gap:6px;margin-top:10px}
.enc-mini-item{background:var(--bg);border-radius:6px;padding:8px;text-align:center}
.enc-mini-label{font-size:0.5rem;color:var(--dim);text-transform:uppercase}
.enc-mini-val{font-size:0.85rem;font-family:monospace;margin-top:2px;color:var(--warning)}
.enc-reset-btn{margin-top:12px;width:100%;padding:10px;background:var(--card2);border:1px solid var(--border2);border-radius:10px;color:var(--warning);font-family:inherit;font-size:0.8rem;font-weight:600;cursor:pointer}
/* EMG */
.emg-bars{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.emg-bar-wrap{text-align:center}
.emg-bar-label{font-size:0.65rem;color:var(--dim);text-transform:uppercase;letter-spacing:1px;margin-bottom:6px}
.emg-bar-outer{height:120px;background:var(--muted);border-radius:8px;position:relative;overflow:hidden}
.emg-bar-fill{position:absolute;bottom:0;left:0;right:0;border-radius:8px;transition:height 0.1s}
.emg-bar-fill.emg1{background:linear-gradient(to top,rgba(255,107,157,0.3),#ff6b9d)}
.emg-bar-fill.emg2{background:linear-gradient(to top,rgba(192,132,252,0.3),#c084fc)}
.emg-bar-val{font-size:1.4rem;font-weight:300;font-family:monospace;margin-top:6px}
.emg-bar-val.c1{color:#ff6b9d}.emg-bar-val.c2{color:#c084fc}
.emg-status-row{display:flex;justify-content:center;gap:16px;margin-top:10px}
.emg-status-dot{width:8px;height:8px;border-radius:50%;display:inline-block;margin-right:4px}
.emg-status-item{font-size:0.65rem;color:var(--dim);display:flex;align-items:center}
.emg-graph-wrap{position:relative;height:80px;background:var(--bg);border-radius:10px;overflow:hidden;margin-top:10px}
#emgGraphCanvas{position:absolute;inset:0;width:100%;height:100%}
.tooltip{display:none;position:fixed;z-index:100;background:#0f1f30;border:1px solid var(--border2);border-radius:10px;padding:10px 14px;font-size:0.75rem;color:var(--text);line-height:1.5;max-width:220px;box-shadow:0 8px 32px rgba(0,0,0,0.5)}
.tooltip.show{display:block}
</style>
</head>
<body>

<!-- WELCOME -->
<div id="welcome" class="screen active">
  <div class="logo">&#x1F9BF;</div>
  <div class="w-title">u<span>O</span>Bionics</div>
  <div class="w-sub">Smart Knee Assessment System</div>
  <div class="w-status">
    <div class="sdot" id="statusDot"></div>
    <span id="statusText">Connecting to device...</span>
  </div>
  <div class="w-status"><div class="sdot" id="emgDot1"></div><span id="emgStat1">EMG 1: --</span></div>
  <div class="w-status"><div class="sdot" id="emgDot2"></div><span id="emgStat2">EMG 2: --</span></div>
  <button class="w-btn" id="startBtn" onclick="goTo('calibrate')" disabled>Get Started</button>
</div>

<!-- CALIBRATION -->
<div id="calibrate" class="screen">
  <div class="cal-header">
    <div class="cal-back" onclick="goTo('welcome')">&larr;</div>
    <div class="cal-header-title">Setup &amp; Calibration</div>
  </div>
  <div class="cal-steps">
    <div class="cal-step active" id="cstep1"></div>
    <div class="cal-step" id="cstep2"></div>
    <div class="cal-step" id="cstep3"></div>
  </div>
  <div class="cal-body" id="calStep1">
    <div class="cal-icon">&#x1F9CD;</div>
    <div class="cal-step-title">Step 1: Straighten Your Leg</div>
    <div class="cal-step-desc">Sit down and extend your leg <strong>fully straight out in front of you</strong>. Keep your knee as flat as possible.</div>
    <div class="cal-visual">
      <svg viewBox="0 0 120 180">
        <line class="cal-thigh" x1="60" y1="10" x2="60" y2="90"/>
        <line class="cal-shin-line" id="calShin" x1="60" y1="90" x2="60" y2="170"/>
        <circle class="cal-jt" cx="60" cy="10" r="7"/>
        <circle class="cal-jt" cx="60" cy="90" r="9"/>
        <circle class="cal-jt" id="calAnkle" cx="60" cy="170" r="6"/>
      </svg>
    </div>
    <div class="cal-live" id="calAngleDisplay">--&deg;</div>
    <div class="cal-live-label">Live angle reading</div>
    <div class="cal-tip">&#x1F4A1; The diagram above shows your leg in real-time. When straight, the two lines should align and the reading should be near 0&deg;.</div>
    <button class="cal-btn" onclick="showCalStep(2)">My Leg is Straight &rarr;</button>
  </div>
  <div class="cal-body" id="calStep2" style="display:none">
    <div class="cal-icon">&#x1F4D0;</div>
    <div class="cal-step-title">Step 2: Set Your Zero Point</div>
    <div class="cal-step-desc">Hold your leg <strong>completely still</strong> in the straightened position and press the button below. This tells the sleeve what &quot;straight&quot; feels like.</div>
    <div class="cal-live" id="calAngleDisplay2">--&deg;</div>
    <div class="cal-live-label">Hold still &mdash; current reading</div>
    <div class="cal-tip">&#x26A0; Do not move during calibration. Any movement will cause incorrect angle measurements during your session.</div>
    <button class="cal-btn" id="calBtn" onclick="doCalibrate()">Calibrate Now</button>
    <div class="cal-progress" id="calProgress"><div class="cal-progress-fill" id="calProgressFill"></div></div>
  </div>
  <div class="cal-body" id="calStep3" style="display:none">
    <div class="cal-icon">&#x2705;</div>
    <div class="cal-step-title">All Set!</div>
    <div class="cal-step-desc">Your sleeve is calibrated and ready to track your knee movement accurately.</div>
    <div class="cal-live" id="calOffsetDisplay">--&deg;</div>
    <div class="cal-live-label">Reference angle stored</div>
    <div class="cal-tip">&#x2705; You can recalibrate at any time from the monitor screen if readings feel off.</div>
    <button class="cal-btn done" onclick="goTo('modepick')">Choose Your Mode &rarr;</button>
  </div>
</div>

<!-- MODE PICKER -->
<div id="modepick" class="screen">
  <div style="padding:32px 24px;flex:1;display:flex;flex-direction:column;justify-content:center;align-items:center;text-align:center">
    <div class="mode-title">How do you want to use it?</div>
    <div class="mode-sub">Choose the experience that works best for you. You can switch anytime from the monitor.</div>
    <div class="mode-cards">
      <div class="mode-card simple-card" onclick="startMode('simple')">
        <div class="mode-card-header">
          <span class="mode-card-icon">&#x1F3C3;</span>
          <div class="mode-card-title">Simple Mode</div>
        </div>
        <div class="mode-card-desc">Easy to follow exercise guide with clear instructions. Perfect for doing your knee exercises at home. No technical knowledge needed.</div>
        <div class="mode-card-tags">
          <span class="mode-tag green">Guided Exercise</span>
          <span class="mode-tag green">Rep Counter</span>
          <span class="mode-tag green">Beginner Friendly</span>
        </div>
      </div>
      <div class="mode-card advanced-card" onclick="startMode('advanced')">
        <div class="mode-card-header">
          <span class="mode-card-icon">&#x1F4CA;</span>
          <div class="mode-card-title">Advanced Mode</div>
        </div>
        <div class="mode-card-desc">Full data dashboard with live angle graphs, sensor readings, velocity tracking, and CSV export. For clinicians and researchers.</div>
        <div class="mode-card-tags">
          <span class="mode-tag blue">Live Graph</span>
          <span class="mode-tag blue">IMU Sensors</span>
          <span class="mode-tag blue">CSV Export</span>
        </div>
      </div>
      <div class="mode-card advanced-card" onclick="startMode('pong')">
        <div class="mode-card-header">
          <span class="mode-card-icon">&#x1F3AF;</span>
          <div class="mode-card-title">Pong Assessment</div>
        </div>
        <div class="mode-card-desc">Clinical proprioception test disguised as a game. Hit the ball at target angles to assess joint position sense.</div>
        <div class="mode-card-tags">
          <span class="mode-tag blue">Proprioception</span>
          <span class="mode-tag blue">3 Zones</span>
          <span class="mode-tag blue">Clinical Score</span>
        </div>
      </div>
    </div>
  </div>
</div>

<!-- SIMPLE MONITOR -->
<div id="simple" class="screen">
  <div class="mon-nav">
    <div class="mon-back" onclick="goTo('modepick')">&larr;</div>
    <div class="mon-nav-title">Knee Exercise</div>
    <button class="mode-switch-btn" onclick="startMode('advanced')">Advanced &rarr;</button>
  </div>

  <div class="section" style="margin-top:10px">
    <div class="card">
      <div class="simple-angle-wrap">
        <div class="simple-angle-label">Current Knee Bend</div>
        <div class="simple-angle-val" id="simpleAngle">--</div>
        <div class="simple-angle-desc" id="simpleAngleDesc">Waiting for movement...</div>
      </div>
      <div class="simple-rom-wrap">
        <div class="simple-rom-label-row">
          <span class="simple-rom-label ext">Straight (0&deg;)</span>
          <span class="simple-rom-label flex">Fully Bent (135&deg;)</span>
        </div>
        <div class="simple-track">
          <div class="simple-fill" id="simpleBar" style="width:0%">
            <div class="simple-thumb"></div>
          </div>
        </div>
        <div class="simple-ticks">
          <span class="simple-tick">0&deg;</span>
          <span class="simple-tick">45&deg;</span>
          <span class="simple-tick">90&deg;</span>
          <span class="simple-tick">135&deg;</span>
        </div>
      </div>
    </div>
  </div>


  <div class="section">
    <div class="card">
      <div class="card-title">Muscle Activity</div>
      <div class="emg-bars">
        <div class="emg-bar-wrap"><div class="emg-bar-label">Muscle 1</div><div class="emg-bar-outer"><div class="emg-bar-fill emg1" id="sEmgBar1" style="height:0%"></div></div><div class="emg-bar-val c1" id="sEmgVal1">0</div></div>
        <div class="emg-bar-wrap"><div class="emg-bar-label">Muscle 2</div><div class="emg-bar-outer"><div class="emg-bar-fill emg2" id="sEmgBar2" style="height:0%"></div></div><div class="emg-bar-val c2" id="sEmgVal2">0</div></div>
      </div>
      <div class="emg-status-row">
        <div class="emg-status-item"><div class="emg-status-dot" id="sEmgDot1" style="background:var(--danger)"></div>EMG 1</div>
        <div class="emg-status-item"><div class="emg-status-dot" id="sEmgDot2" style="background:var(--danger)"></div>EMG 2</div>
      </div>
    </div>
  </div>
  <div class="section">
    <div class="card">
      <div class="simple-status-card">
        <span class="simple-status-icon" id="simpleStatusIcon">&#x1F9D8;</span>
        <div class="simple-status-title" id="simpleStatusTitle">Ready to Start</div>
        <div class="simple-status-desc" id="simpleStatusDesc">Follow the exercise steps below. Bend your knee slowly and bring it back down.</div>
      </div>
    </div>
  </div>

  <div class="section">
    <div class="card">
      <div class="simple-rep-card">
        <div class="simple-rep-big" id="simpleReps">0</div>
        <div class="simple-rep-info">
          <div class="simple-rep-title">Reps Completed</div>
          <div class="simple-rep-sub">Each rep = one full bend and straighten of your knee</div>
          <div class="simple-rep-goal">
            <div class="simple-rep-goal-label">
              <span>Progress toward goal</span>
              <span id="simpleGoalText">0 / 10</span>
            </div>
            <div class="simple-rep-goal-bar">
              <div class="simple-rep-goal-fill" id="simpleGoalFill" style="width:0%"></div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>

  <div class="section">
    <div class="card">
      <div class="card-title">Seated Knee Bend &mdash; How To</div>
      <div class="simple-steps-card">
        <div class="simple-step-row">
          <div class="simple-step-num" id="sstep1">1</div>
          <div class="simple-step-text">
            <div class="simple-step-title">Sit upright in your chair</div>
            <div class="simple-step-desc">Keep your back straight and both feet flat on the floor. Make sure the sleeve is comfortable on your knee.</div>
          </div>
        </div>
        <div class="simple-step-row">
          <div class="simple-step-num" id="sstep2">2</div>
          <div class="simple-step-text">
            <div class="simple-step-title">Slowly lift and extend your leg</div>
            <div class="simple-step-desc">Raise your foot off the ground and straighten your knee as much as comfortable. Hold for 2 seconds.</div>
          </div>
        </div>
        <div class="simple-step-row">
          <div class="simple-step-num" id="sstep3">3</div>
          <div class="simple-step-text">
            <div class="simple-step-title">Slowly lower your leg back down</div>
            <div class="simple-step-desc">Bend your knee and bring your foot back to the floor. That is one full rep. Rest briefly, then repeat.</div>
          </div>
        </div>
      </div>
    </div>
  </div>

  <div class="simple-controls">
    <button class="simple-ctrl simple-ctrl-reset" onclick="resetSimple()">Reset Reps</button>
    <button class="simple-ctrl simple-ctrl-start" onclick="goTo('calibrate')">Recalibrate</button>
  </div>
</div>

<!-- ADVANCED MONITOR -->
<div id="monitor" class="screen">
  <div class="mon-nav">
    <div class="mon-back" onclick="goTo('modepick')">&larr;</div>
    <div class="mon-nav-title">Advanced Monitor</div>
    <button class="mode-switch-btn" onclick="startMode('simple')">Simple &rarr;</button>
  </div>

  <div class="section" style="margin-top:10px">
    <div class="card hero-card">
      <div class="angle-ring-wrap">
        <svg class="angle-svg" viewBox="0 0 160 160">
          <circle class="ring-bg" cx="80" cy="80" r="70"/>
          <circle class="ring-fill" id="ring" cx="80" cy="80" r="70"/>
        </svg>
        <div class="ring-inner">
          <div class="ring-label">Knee Flexion</div>
          <div class="ring-val" id="monAngle">--</div>
          <div class="ring-unit">degrees</div>
        </div>
      </div>
      <div class="rom-axis">
        <div class="rom-axis-label">
          <span>0&deg; Extension</span>
          <span id="romCurrent" style="color:var(--accent)">--&deg;</span>
          <span>135&deg; Flexion</span>
        </div>
        <div class="rom-track-wrap">
          <div class="rom-track-fill" id="romBar" style="width:0%">
            <div class="rom-thumb"></div>
          </div>
        </div>
        <div class="rom-ticks">
          <span class="rom-tick">0&deg;</span>
          <span class="rom-tick">45&deg;</span>
          <span class="rom-tick">90&deg;</span>
          <span class="rom-tick">135&deg;</span>
        </div>
      </div>
      <div class="stats3">
        <div class="stat">
          <div class="stat-info" onclick="showTip(this,'Minimum angle reached this session. Lower = more leg extension.')">?</div>
          <div class="stat-val" id="romMin">--</div>
          <div class="stat-label">Min &deg;</div>
        </div>
        <div class="stat">
          <div class="stat-info" onclick="showTip(this,'Maximum angle reached this session. Higher = more knee flexion.')">?</div>
          <div class="stat-val" id="romMax">--</div>
          <div class="stat-label">Max &deg;</div>
        </div>
        <div class="stat">
          <div class="stat-info" onclick="showTip(this,'Range of Motion: the total angular range achieved (Max minus Min).')">?</div>
          <div class="stat-val" id="romRange">--</div>
          <div class="stat-label">ROM &deg;</div>
        </div>
      </div>
    </div>
  </div>

  <div class="section">
    <div class="card">
      <div class="card-title">EMG Muscle Activity
        <span class="card-title-info" onclick="showTip(this,'Filtered EMG signals from two MyoWare 2.0 sensors measuring muscle electrical activity.')">What is this?</span>
      </div>
      <div class="emg-bars">
        <div class="emg-bar-wrap"><div class="emg-bar-label">Muscle 1</div><div class="emg-bar-outer"><div class="emg-bar-fill emg1" id="aEmgBar1" style="height:0%"></div></div><div class="emg-bar-val c1" id="aEmgVal1">0</div></div>
        <div class="emg-bar-wrap"><div class="emg-bar-label">Muscle 2</div><div class="emg-bar-outer"><div class="emg-bar-fill emg2" id="aEmgBar2" style="height:0%"></div></div><div class="emg-bar-val c2" id="aEmgVal2">0</div></div>
      </div>
      <div class="emg-graph-wrap"><canvas id="emgGraphCanvas"></canvas></div>
      <div class="emg-status-row">
        <div class="emg-status-item"><div class="emg-status-dot" id="aEmgDot1" style="background:var(--danger)"></div>EMG 1</div>
        <div class="emg-status-item"><div class="emg-status-dot" id="aEmgDot2" style="background:var(--danger)"></div>EMG 2</div>
      </div>
    </div>
  </div>

  <div class="section">
    <div class="card">
      <div class="card-title">Live Sensor Tracking</div>
      <div class="leg-card">
        <div class="leg-svg-wrap">
          <svg class="leg-svg" viewBox="0 0 75 115">
            <circle cx="38" cy="10" r="5" fill="#0c1a2a"/>
            <line x1="38" y1="10" x2="38" y2="55" stroke="#2a4060" stroke-width="7" stroke-linecap="round"/>
            <circle cx="38" cy="55" r="7" fill="#080f1a" stroke="#00d4f0" stroke-width="2.5"/>
            <line id="monShin" x1="38" y1="55" x2="38" y2="108" stroke="#00d4f0" stroke-width="7" stroke-linecap="round"/>
            <circle id="monAnkle" cx="38" cy="108" r="5" fill="#00d4f0"/>
          </svg>
        </div>
        <div class="leg-right">
          <div class="leg-metric">
            <div class="leg-metric-label">Raw Angle <div class="tip-icon" onclick="showTip(this,'The unprocessed angle from the sensors before your calibration offset is applied.')">?</div></div>
            <div class="leg-metric-val" id="rawAngle">--&deg;</div>
          </div>
          <div class="leg-metric">
            <div class="leg-metric-label">Angular Velocity <div class="tip-icon" onclick="showTip(this,'How fast your knee angle is changing in degrees per second. High values mean rapid movement.')">?</div></div>
            <div class="leg-metric-val" id="velocity">--&deg;/s</div>
          </div>
          <div class="leg-metric">
            <div class="leg-metric-label">Cal Offset <div class="tip-icon" onclick="showTip(this,'The reference angle recorded during calibration when your leg was straight.')">?</div></div>
            <div class="leg-metric-val" id="calVal">--&deg;</div>
          </div>
          <div class="leg-metric">
            <div class="leg-metric-label">Session Time</div>
            <div class="leg-metric-val" id="sessionTime">00:00</div>
          </div>
        </div>
      </div>
    </div>
  </div>

  <div class="section">
    <div class="card">
      <div class="card-title">
        Exercise Detection
        <span class="card-title-info" onclick="showTip(this,'A rep is counted each time the knee goes below 15 deg then above 50 deg.')">How are reps counted?</span>
      </div>
      <div class="ex-card">
        <div class="ex-big" id="reps">0</div>
        <div class="ex-detail">
          <div class="ex-name" id="exName">Ready</div>
          <div class="ex-sub">Bend knee past 50&deg; to begin counting</div>
        </div>
      </div>
      <div class="ex-mini">
        <div class="ex-mini-item"><div class="ex-mini-label">Extensions</div><div class="ex-mini-val" id="exExtCount">0</div></div>
        <div class="ex-mini-item"><div class="ex-mini-label">Flexions</div><div class="ex-mini-val" id="exFlexCount">0</div></div>
      </div>
    </div>
  </div>

  <div class="section">
    <div class="card">
      <div class="card-title">Angle History (last 5s)</div>
      <div class="graph-container">
        <canvas id="graphCanvas"></canvas>
        <div class="graph-y-labels">
          <div class="graph-y-label">135&deg;</div>
          <div class="graph-y-label">90&deg;</div>
          <div class="graph-y-label">45&deg;</div>
          <div class="graph-y-label">0&deg;</div>
        </div>
      </div>
      <div class="graph-x-labels">
        <span>-5s</span><span>-4s</span><span>-3s</span><span>-2s</span><span>-1s</span><span>now</span>
      </div>
    </div>
  </div>

  <div class="section">
    <div class="card">
      <div class="card-title">
        IMU Sensor Data (g-force)
        <span class="card-title-info" onclick="showTip(this,'Raw accelerometer readings from each sensor. X/Y/Z represent the three axes of movement.')">What is this?</span>
      </div>
      <div class="imu-section">
        <div class="imu-box">
          <div class="imu-title"><div class="imu-dot" style="background:var(--garnet)"></div>Thigh Sensor</div>
          <div class="imu-axes">
            <div class="imu-axis"><div class="imu-axis-label">X</div><div class="imu-axis-val" id="a1x">--</div></div>
            <div class="imu-axis"><div class="imu-axis-label">Y</div><div class="imu-axis-val" id="a1y">--</div></div>
            <div class="imu-axis"><div class="imu-axis-label">Z</div><div class="imu-axis-val" id="a1z">--</div></div>
          </div>
        </div>
        <div class="imu-box">
          <div class="imu-title"><div class="imu-dot" style="background:var(--accent)"></div>Shin Sensor</div>
          <div class="imu-axes">
            <div class="imu-axis"><div class="imu-axis-label">X</div><div class="imu-axis-val" id="a2x">--</div></div>
            <div class="imu-axis"><div class="imu-axis-label">Y</div><div class="imu-axis-val" id="a2y">--</div></div>
            <div class="imu-axis"><div class="imu-axis-label">Z</div><div class="imu-axis-val" id="a2z">--</div></div>
          </div>
        </div>
      </div>
    </div>
  </div>

  <div class="section">
    <div class="card">
      <div class="card-title">
        Rotary Encoder (PEC11R)
        <span class="card-title-info" onclick="showTip(this,'Quadrature encoder on GPIO 10/11. Tracks cumulative rotation in degrees and raw detent count.')">What is this?</span>
      </div>
      <div class="enc-card">
        <div class="enc-big" id="encAngle">0.0°</div>
        <div class="enc-detail">
          <div class="enc-name">Encoder Angle</div>
          <div class="enc-sub">Cumulative rotation since last reset. Each detent = 15°.</div>
          <div class="enc-mini">
            <div class="enc-mini-item"><div class="enc-mini-label">Raw Count</div><div class="enc-mini-val" id="encCount">0</div></div>
            <div class="enc-mini-item"><div class="enc-mini-label">Direction</div><div class="enc-mini-val" id="encDir">—</div></div>
          </div>
          <button class="enc-reset-btn" onclick="resetEncoder()">↺ Reset Encoder</button>
        </div>
      </div>
    </div>
  </div>

  <div class="controls">
    <button class="ctrl ctrl-rec" id="recBtn" onclick="toggleRec()">Record</button>
    <button class="ctrl ctrl-cal" onclick="goTo('calibrate')">Recalibrate</button>
  </div>
</div>


<!-- PONG TEST -->
<div id="test" class="screen">
  <div class="t-head">
    <div class="t-back" onclick="endTest();goTo('modepick')">&larr;</div>
    <div class="t-info"><div class="t-phase" id="tPhase">Phase 1 of 3</div><div class="t-target" id="tTarget">Target: 25&deg;</div></div>
    <div class="t-trials"><div class="t-num" id="tTrials">0/15</div><div class="t-label">Trials</div></div>
  </div>
  <div class="t-game">
    <canvas class="t-canvas" id="tCanvas"></canvas>
    <div class="t-overlay" id="tOverlay">
      <div class="t-ov-title">The Pong Test&trade;</div>
      <div class="t-ov-desc">Move your knee to hit the ball at specific target angles. The ball bounces back &mdash; keep the rally going!</div>
      <div class="t-ov-zones">
        <div class="t-zone"><div class="t-zone-num z1">1</div><div class="t-zone-range">15-35&deg;</div></div>
        <div class="t-zone"><div class="t-zone-num z2">2</div><div class="t-zone-range">40-60&deg;</div></div>
        <div class="t-zone"><div class="t-zone-num z3">3</div><div class="t-zone-range">65-90&deg;</div></div>
      </div>
      <button class="t-ov-btn" onclick="startPongTest()">Begin Assessment</button>
      <div class="t-ov-hint">15 trials &bull; ~60 seconds</div>
    </div>
    <div class="t-hud" id="tHud" style="display:none">
      <div class="t-bar"><div class="t-bar-fill" id="tFill"></div><div class="t-bar-target" id="tTargetBar"></div></div>
      <div class="t-row">
        <div class="t-angle" id="tAngle">0&deg;</div>
        <div class="t-feedback" id="tFeedback">Ready</div>
        <div class="t-score"><div class="t-score-val" id="tScore">0</div><div class="t-score-label">Hits</div></div>
      </div>
    </div>
  </div>
</div>

<!-- PONG RESULTS -->
<div id="results" class="screen">
  <div class="r-main">
    <div class="r-icon" id="rIcon">&#x1F4CA;</div>
    <div class="r-score-big" id="rScore">--</div>
    <div class="r-label2">Joint Position Sense Score</div>
    <div class="r-verdict" id="rVerdict">Analyzing...</div>
  </div>
  <div class="r-zones">
    <div class="r-zone"><div class="r-zone-bar z1"></div><div class="r-zone-score" id="rZ1">--</div><div class="r-zone-name">Low (15-35&deg;)</div></div>
    <div class="r-zone"><div class="r-zone-bar z2"></div><div class="r-zone-score" id="rZ2">--</div><div class="r-zone-name">Mid (40-60&deg;)</div></div>
    <div class="r-zone"><div class="r-zone-bar z3"></div><div class="r-zone-score" id="rZ3">--</div><div class="r-zone-name">High (65-90&deg;)</div></div>
  </div>
  <div class="r-metrics">
    <div class="r-metric"><div class="r-metric-val" id="rAccuracy">--</div><div class="r-metric-label">Accuracy</div></div>
    <div class="r-metric"><div class="r-metric-val" id="rReaction">--</div><div class="r-metric-label">Avg Reaction</div></div>
    <div class="r-metric"><div class="r-metric-val" id="rConsistency">--</div><div class="r-metric-label">Consistency</div></div>
    <div class="r-metric"><div class="r-metric-val" id="rSmoothness">--</div><div class="r-metric-label">Smoothness</div></div>
  </div>
  <div class="r-finding"><div class="r-finding-title">Clinical Interpretation</div><div class="r-finding-text" id="rFinding">...</div></div>
  <div class="r-actions">
    <button class="r-btn r-btn-done" onclick="goTo('modepick')">Done</button>
    <button class="r-btn r-btn-retry" onclick="goTo('test')">Retest</button>
  </div>
</div>

<div class="toast" id="toast">HIT!</div>

<!-- Tooltip -->
<div class="tooltip" id="globalTip"></div>

<script>
var ws,connected=false,calOffset=180;
var angle=0,rawAngle=180,vel=0,lastA=0,lastT=Date.now();
var minA=null,maxA=null,start=Date.now();
var ext=0,flex=0,wasExt=true;
var rec=false,recD=[],gData=[];
var simpleExt=0,simpleFlex=0,simpleWasExt=true;
var GOAL=10;
var currentMode='simple';
var encAngleDeg=0,encCountVal=0,encPrevCount=0;
var emgA=0,emgB=0,emgConnA=false,emgConnB=false,emgDataA=[],emgDataB=[];

var tipTimeout;
function showTip(el,msg){
  var tip=document.getElementById('globalTip');
  var rect=el.getBoundingClientRect();
  tip.textContent=msg;
  tip.style.left=Math.min(rect.left,window.innerWidth-240)+'px';
  tip.style.top=(rect.bottom+8)+'px';
  tip.classList.add('show');
  clearTimeout(tipTimeout);
  tipTimeout=setTimeout(function(){tip.classList.remove('show');},3500);
}
document.addEventListener('click',function(e){
  if(!e.target.closest('.tip-icon')&&!e.target.closest('.stat-info')&&!e.target.closest('.card-title-info')){
    document.getElementById('globalTip').classList.remove('show');
  }
});

function goTo(id){
  document.querySelectorAll('.screen').forEach(function(s){s.classList.remove('active');});
  document.getElementById(id).classList.add('active');
  if(id==='monitor'||id==='simple'){start=Date.now();minA=maxA=null;ext=0;flex=0;gData=[];emgDataA=[];emgDataB=[];}
  if(id==='test')initPong();
  if(id==='calibrate'){showCalStep(1);}
}

function startMode(mode){
  currentMode=mode;
  if(mode==='simple'){resetSimple();goTo('simple');}
  else if(mode==='pong'){goTo('test');initPong();}
  else{goTo('monitor');}
}

function resetSimple(){
  simpleExt=0;simpleFlex=0;simpleWasExt=true;
  document.getElementById('simpleReps').textContent='0';
  document.getElementById('simpleGoalText').textContent='0 / '+GOAL;
  document.getElementById('simpleGoalFill').style.width='0%';
  document.getElementById('simpleStatusIcon').textContent='&#x1F9D8;';
  document.getElementById('simpleStatusTitle').textContent='Ready to Start';
  document.getElementById('simpleStatusDesc').textContent='Follow the exercise steps below. Bend your knee slowly and bring it back down.';
  [1,2,3].forEach(function(i){
    var s=document.getElementById('sstep'+i);
    s.classList.remove('active','done');
  });
}

function showCalStep(n){
  ['calStep1','calStep2','calStep3'].forEach(function(id,i){
    document.getElementById(id).style.display=i===n-1?'flex':'none';
  });
  [1,2,3].forEach(function(i){
    var s=document.getElementById('cstep'+i);
    s.classList.remove('active','done');
    if(i<n)s.classList.add('done');
    else if(i===n)s.classList.add('active');
  });
}

function connect(){
  ws=new WebSocket('ws://'+location.hostname+':81/');
  ws.onopen=function(){
    connected=true;
    document.getElementById('statusDot').classList.add('ok');
    document.getElementById('statusText').textContent='Device connected';
    document.getElementById('startBtn').disabled=false;
  };
  ws.onclose=function(){
    connected=false;
    document.getElementById('statusDot').classList.remove('ok');
    document.getElementById('statusText').textContent='Reconnecting...';
    document.getElementById('startBtn').disabled=true;
    setTimeout(connect,2000);
  };
  ws.onmessage=function(e){
    var d=JSON.parse(e.data);
    if(d.calDone){onCalDone(d);return;}
    if(d.encReset){encAngleDeg=0;encCountVal=0;encPrevCount=0;return;}
    if(d.knee!==undefined)onData(d);
  };
}

function onCalDone(d){
  calOffset=d.offset;
  document.getElementById('calProgress').style.display='none';
  document.getElementById('calOffsetDisplay').textContent=d.offset.toFixed(1)+'°';
  document.getElementById('calVal').textContent=d.offset.toFixed(1)+'°';
  showCalStep(3);
}

function onData(d){
  var now=Date.now(),dt=(now-lastT)/1000;lastT=now;
  rawAngle=d.raw;angle=d.knee;
  vel=dt>0?Math.abs(angle-lastA)/dt:0;lastA=angle;
  if(d.enc!==undefined){encAngleDeg=d.enc;encCountVal=d.encCount;}
  if(d.e1!==undefined){emgA=Math.abs(d.e1);emgConnA=true;}
  if(d.e2!==undefined){emgB=Math.abs(d.e2);emgConnB=true;}
  if(d.ec1!==undefined)emgConnA=d.ec1;
  if(d.ec2!==undefined)emgConnB=d.ec2;
  updateEMG();
  if(testOn)pVelLog.push(vel);
  updateCalLeg(d.raw);
  updateSimple();
  updateMonitor(d);
}

function updateCalLeg(raw){
  var a=Math.min(Math.abs(180-raw),135);
  var rad=a*Math.PI/180,len=80;
  var x=60+Math.sin(rad)*len,y=90+Math.cos(rad)*len;
  var shin=document.getElementById('calShin');
  if(shin){shin.setAttribute('x2',x);shin.setAttribute('y2',y);}
  var ankle=document.getElementById('calAnkle');
  if(ankle){ankle.setAttribute('cx',x);ankle.setAttribute('cy',y);}
  var disp=raw.toFixed(0)+'°';
  var d1=document.getElementById('calAngleDisplay');if(d1)d1.textContent=disp;
  var d2=document.getElementById('calAngleDisplay2');if(d2)d2.textContent=disp;
}

function doCalibrate(){
  if(!connected)return;
  var btn=document.getElementById('calBtn');
  btn.textContent='Calibrating...';btn.disabled=true;
  var prog=document.getElementById('calProgress');
  prog.style.display='block';
  document.getElementById('calProgressFill').style.width='100%';
  ws.send('CALIBRATE');
}

function updateSimple(){
  if(!document.getElementById('simple').classList.contains('active'))return;
  document.getElementById('simpleAngle').textContent=angle.toFixed(0);
  var pct=Math.min(angle/135,1);
  document.getElementById('simpleBar').style.width=(pct*100)+'%';

  var desc,icon,title;
  if(angle<10){
    desc='Your leg is straight. Good starting position!';
    icon='&#x1F9B5;';title='Leg Straight';
    document.getElementById('sstep1').classList.add('active');
    document.getElementById('sstep2').classList.remove('active','done');
    document.getElementById('sstep3').classList.remove('active','done');
  } else if(angle<50){
    desc='Keep bending your knee further up.';
    icon='&#x1F4C8;';title='Bending...';
    document.getElementById('sstep1').classList.add('done');
    document.getElementById('sstep2').classList.add('active');
    document.getElementById('sstep3').classList.remove('active','done');
  } else if(angle<100){
    desc='Great bend! Hold it for a moment.';
    icon='&#x1F44D;';title='Good Bend!';
    document.getElementById('sstep1').classList.add('done');
    document.getElementById('sstep2').classList.add('done');
    document.getElementById('sstep3').classList.add('active');
  } else {
    desc='Excellent! That is a full knee bend.';
    icon='&#x1F3C6;';title='Full Bend!';
    document.getElementById('sstep1').classList.add('done');
    document.getElementById('sstep2').classList.add('done');
    document.getElementById('sstep3').classList.add('done');
  }
  document.getElementById('simpleAngleDesc').textContent=desc;
  document.getElementById('simpleStatusIcon').innerHTML=icon;
  document.getElementById('simpleStatusTitle').textContent=title;
  document.getElementById('simpleStatusDesc').textContent=desc;

  // Rep counting
  if(angle>50&&simpleWasExt){simpleFlex++;simpleWasExt=false;}
  if(angle<10&&!simpleWasExt){
    simpleExt++;simpleWasExt=true;
    var total=Math.min(simpleExt,simpleFlex);
    document.getElementById('simpleReps').textContent=total;
    var pctGoal=Math.min(total/GOAL*100,100);
    document.getElementById('simpleGoalFill').style.width=pctGoal+'%';
    document.getElementById('simpleGoalText').textContent=total+' / '+GOAL;
  }
}

function resetEncoder(){if(connected)ws.send('RESET_ENC');}

function updateMonitor(d){
  if(!document.getElementById('monitor').classList.contains('active'))return;
  document.getElementById('monAngle').textContent=angle.toFixed(0);
  document.getElementById('romCurrent').textContent=angle.toFixed(0)+'°';
  var pct=Math.min(angle/135,1);
  document.getElementById('ring').style.strokeDashoffset=439.8*(1-pct);
  document.getElementById('romBar').style.width=(pct*100)+'%';
  document.getElementById('rawAngle').textContent=rawAngle.toFixed(1)+'°';
  document.getElementById('velocity').textContent=vel.toFixed(0)+'°/s';
  var el=Math.floor((Date.now()-start)/1000);
  document.getElementById('sessionTime').textContent=String(Math.floor(el/60)).padStart(2,'0')+':'+String(el%60).padStart(2,'0');
  if(minA===null||angle<minA){minA=angle;document.getElementById('romMin').textContent=minA.toFixed(0);}
  if(maxA===null||angle>maxA){maxA=angle;document.getElementById('romMax').textContent=maxA.toFixed(0);}
  if(minA!==null&&maxA!==null)document.getElementById('romRange').textContent=(maxA-minA).toFixed(0);
  var rad=angle*Math.PI/180,len=53;
  var sx=38+Math.sin(rad)*len,sy=55+Math.cos(rad)*len;
  document.getElementById('monShin').setAttribute('x2',sx);
  document.getElementById('monShin').setAttribute('y2',sy);
  document.getElementById('monAnkle').setAttribute('cx',sx);
  document.getElementById('monAnkle').setAttribute('cy',sy);
  if(angle<15&&!wasExt){ext++;wasExt=true;document.getElementById('exName').textContent='Extension';document.getElementById('exExtCount').textContent=ext;}
  if(angle>50&&wasExt){flex++;wasExt=false;document.getElementById('exName').textContent='Flexion detected';document.getElementById('exFlexCount').textContent=flex;}
  document.getElementById('reps').textContent=ext+flex;
  if(d.a1x!==undefined){
    document.getElementById('a1x').textContent=d.a1x.toFixed(2);
    document.getElementById('a1y').textContent=d.a1y.toFixed(2);
    document.getElementById('a1z').textContent=d.a1z.toFixed(2);
    document.getElementById('a2x').textContent=d.a2x.toFixed(2);
    document.getElementById('a2y').textContent=d.a2y.toFixed(2);
    document.getElementById('a2z').textContent=d.a2z.toFixed(2);
  }
  gData.push(angle);if(gData.length>100)gData.shift();
  drawGraph();
  drawEMGGraph();
  if(rec)recD.push({t:Date.now(),angle:angle,rawAngle:rawAngle,vel:vel,emg1:emgA,emg2:emgB,enc:encAngleDeg,encCount:encCountVal,a1x:d.a1x,a1y:d.a1y,a1z:d.a1z,a2x:d.a2x,a2y:d.a2y,a2z:d.a2z});
  // Encoder display
  var eAngle=document.getElementById('encAngle');
  if(eAngle){eAngle.textContent=encAngleDeg.toFixed(1)+'°';}
  var eCnt=document.getElementById('encCount');
  if(eCnt){eCnt.textContent=encCountVal;}
  var eDir=document.getElementById('encDir');
  if(eDir){var delta=encCountVal-encPrevCount;eDir.textContent=delta>0?'CW ↻':delta<0?'CCW ↺':'—';encPrevCount=encCountVal;}
}


function updateEMG(){
  var mx=1000,p1=Math.min(emgA/mx*100,100),p2=Math.min(emgB/mx*100,100);
  var d1=document.getElementById('emgDot1'),d2=document.getElementById('emgDot2');
  if(d1){if(emgConnA){d1.classList.add('ok');document.getElementById('emgStat1').textContent='EMG 1: Active';}else{d1.classList.remove('ok');document.getElementById('emgStat1').textContent='EMG 1: --';}}
  if(d2){if(emgConnB){d2.classList.add('ok');document.getElementById('emgStat2').textContent='EMG 2: Active';}else{d2.classList.remove('ok');document.getElementById('emgStat2').textContent='EMG 2: --';}}
  ['s','a'].forEach(function(px){
    var b1=document.getElementById(px+'EmgBar1'),b2=document.getElementById(px+'EmgBar2');
    if(b1)b1.style.height=p1+'%';if(b2)b2.style.height=p2+'%';
    var v1=document.getElementById(px+'EmgVal1'),v2=document.getElementById(px+'EmgVal2');
    if(v1)v1.textContent=emgA.toFixed(0);if(v2)v2.textContent=emgB.toFixed(0);
    var dd1=document.getElementById(px+'EmgDot1'),dd2=document.getElementById(px+'EmgDot2');
    if(dd1)dd1.style.background=emgConnA?'var(--success)':'var(--danger)';
    if(dd2)dd2.style.background=emgConnB?'var(--success)':'var(--danger)';
  });
  emgDataA.push(emgA);emgDataB.push(emgB);
  if(emgDataA.length>100){emgDataA.shift();emgDataB.shift();}
}
function drawEMGGraph(){
  var c=document.getElementById('emgGraphCanvas');if(!c)return;
  var w=c.parentElement.clientWidth,h=c.parentElement.clientHeight;c.width=w;c.height=h;
  var ctx=c.getContext('2d');ctx.fillStyle='#080f1a';ctx.fillRect(0,0,w,h);
  if(emgDataA.length<2)return;
  var mx=Math.max(100,Math.max.apply(null,emgDataA),Math.max.apply(null,emgDataB));
  ctx.strokeStyle='#ff6b9d';ctx.lineWidth=1.5;ctx.beginPath();
  emgDataA.forEach(function(v,i){var x=i/(emgDataA.length-1)*w,y=h-(v/mx)*h*0.9-h*0.05;i===0?ctx.moveTo(x,y):ctx.lineTo(x,y);});ctx.stroke();
  ctx.strokeStyle='#c084fc';ctx.beginPath();
  emgDataB.forEach(function(v,i){var x=i/(emgDataB.length-1)*w,y=h-(v/mx)*h*0.9-h*0.05;i===0?ctx.moveTo(x,y):ctx.lineTo(x,y);});ctx.stroke();
}
function drawGraph(){
  var c=document.getElementById('graphCanvas');
  var w=c.parentElement.clientWidth,h=c.parentElement.clientHeight;
  c.width=w;c.height=h;
  var ctx=c.getContext('2d');
  ctx.fillStyle='#080f1a';ctx.fillRect(0,0,w,h);
  [0,45,90,135].forEach(function(deg){
    var y=h-(deg/135)*h;
    ctx.strokeStyle=deg===0?'#1a2840':'#111d2e';
    ctx.lineWidth=1;ctx.setLineDash([3,4]);
    ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(w,y);ctx.stroke();
    ctx.setLineDash([]);
  });
  if(gData.length<2)return;
  ctx.strokeStyle='rgba(0,212,240,0.2)';ctx.lineWidth=8;ctx.beginPath();
  gData.forEach(function(v,i){var x=i/(gData.length-1)*w,y=h-(v/135)*h*0.92-h*0.04;i===0?ctx.moveTo(x,y):ctx.lineTo(x,y);});
  ctx.stroke();
  ctx.strokeStyle='#00d4f0';ctx.lineWidth=2;ctx.beginPath();
  gData.forEach(function(v,i){var x=i/(gData.length-1)*w,y=h-(v/135)*h*0.92-h*0.04;i===0?ctx.moveTo(x,y):ctx.lineTo(x,y);});
  ctx.stroke();
}

function toggleRec(){
  rec=!rec;var btn=document.getElementById('recBtn');
  if(rec){recD=[];btn.textContent='Stop';btn.classList.add('on');}
  else{btn.textContent='Record';btn.classList.remove('on');if(recD.length)exportCSV();}
}

function exportCSV(){
  var csv='Time(ms),Knee(deg),Raw(deg),Velocity(deg/s),EMG1,EMG2,Encoder(deg),EncoderCount,Thigh_X,Thigh_Y,Thigh_Z,Shin_X,Shin_Y,Shin_Z\n';
  var t0=recD[0].t;
  recD.forEach(function(r){
    csv+=(r.t-t0)+','+r.angle.toFixed(1)+','+r.rawAngle.toFixed(1)+','+r.vel.toFixed(1);
    csv+=','+(r.emg1!==undefined?r.emg1.toFixed(1):'')+','+(r.emg2!==undefined?r.emg2.toFixed(1):'')+','+(r.enc!==undefined?r.enc.toFixed(1):'')+','+(r.encCount!==undefined?r.encCount:'');
    if(r.a1x!==undefined)csv+=','+r.a1x.toFixed(3)+','+r.a1y.toFixed(3)+','+r.a1z.toFixed(3)+','+r.a2x.toFixed(3)+','+r.a2y.toFixed(3)+','+r.a2z.toFixed(3);
    csv+='\n';
  });
  var a=document.createElement('a');
  a.href=URL.createObjectURL(new Blob([csv],{type:'text/csv'}));
  a.download='uobionics_'+new Date().toISOString().slice(0,10)+'.csv';
  a.click();
}

// ── PONG TEST ──
var ZONES=[{min:15,max:35},{min:40,max:60},{min:65,max:90}];
var testOn=false,pPhase=0,pTrial=0,pHits=0,pTarget=0,pTrialStart=0,pRallies=0;
var pZoneData=[{h:0,e:[],t:[]},{h:0,e:[],t:[]},{h:0,e:[],t:[]}];
var pVelLog=[];
var tCanvas,tCtx,paddle={y:0,h:60},ball={x:0,y:0,vx:0,vy:0,r:10,on:false};

function initPong(){
  tCanvas=document.getElementById('tCanvas');
  tCtx=tCanvas.getContext('2d');
  tCanvas.width=tCanvas.parentElement.clientWidth;
  tCanvas.height=tCanvas.parentElement.clientHeight;
  paddle.h=tCanvas.height*0.18;
  document.getElementById('tOverlay').classList.remove('hidden');
  document.getElementById('tHud').style.display='none';
}

function startPongTest(){
  testOn=true;pPhase=0;pTrial=0;pHits=0;pRallies=0;
  pZoneData=[{h:0,e:[],t:[]},{h:0,e:[],t:[]},{h:0,e:[],t:[]}];
  pVelLog=[];
  document.getElementById('tOverlay').classList.add('hidden');
  document.getElementById('tHud').style.display='block';
  document.getElementById('tScore').textContent='0';
  document.getElementById('tTrials').textContent='0/15';
  pNextTrial();
  requestAnimationFrame(pGameLoop);
}

function pNextTrial(){
  if(pTrial>=15){endTest();showPongResults();return;}
  pPhase=Math.floor(pTrial/5);
  var z=ZONES[pPhase];
  pTarget=z.min+Math.random()*(z.max-z.min);
  document.getElementById('tPhase').textContent='Phase '+(pPhase+1)+' of 3';
  document.getElementById('tTarget').textContent='Target: '+pTarget.toFixed(0)+'°';
  document.getElementById('tTargetBar').style.left=(pTarget/135*100)+'%';
  setTimeout(function(){pLaunchBall();pTrialStart=Date.now();},500+Math.random()*300);
}

function pLaunchBall(){
  var h=tCanvas.height;
  ball.x=tCanvas.width-40;
  ball.y=h-(pTarget/135)*h+(Math.random()-0.5)*30;
  ball.vx=-5;ball.vy=(Math.random()-0.5)*2;ball.on=true;
}

function pGameLoop(){
  if(!testOn)return;
  pUpdate();pDraw();pUpdateHUD();
  requestAnimationFrame(pGameLoop);
}

function pUpdate(){
  var w=tCanvas.width,h=tCanvas.height;
  var targetY=h-(angle/135)*h-paddle.h/2;
  paddle.y+=(targetY-paddle.y)*0.2;
  paddle.y=Math.max(0,Math.min(h-paddle.h,paddle.y));
  if(!ball.on)return;
  ball.x+=ball.vx;ball.y+=ball.vy;
  if(ball.y<ball.r){ball.y=ball.r;ball.vy*=-1;}
  if(ball.y>h-ball.r){ball.y=h-ball.r;ball.vy*=-1;}
  if(ball.x>w-ball.r){ball.x=w-ball.r;ball.vx*=-1;}
  if(ball.x<50&&ball.vx<0&&ball.y>paddle.y-10&&ball.y<paddle.y+paddle.h+10){
    ball.vx=-ball.vx*1.05;ball.vy+=(ball.y-(paddle.y+paddle.h/2))*0.08;ball.x=50+ball.r;
    pRallies++;
    if(pRallies===1||(pRallies>1&&pTrial<15)){
      var rt=Date.now()-pTrialStart;var err=Math.abs(angle-pTarget);
      pHits++;pZoneData[pPhase].h++;pZoneData[pPhase].e.push(err);pZoneData[pPhase].t.push(rt);
      document.getElementById('tScore').textContent=pHits;
      pShowToast(true);
      if(pRallies>=3){ball.on=false;pTrial++;pRallies=0;document.getElementById('tTrials').textContent=pTrial+'/15';setTimeout(pNextTrial,300);}
    }
  }
  if(ball.x<-20){
    pZoneData[pPhase].e.push(Math.abs(angle-pTarget));pZoneData[pPhase].t.push(Date.now()-pTrialStart);
    pShowToast(false);ball.on=false;pTrial++;pRallies=0;
    document.getElementById('tTrials').textContent=pTrial+'/15';setTimeout(pNextTrial,300);
  }
  ball.vx=Math.max(-10,Math.min(10,ball.vx));ball.vy=Math.max(-6,Math.min(6,ball.vy));
}

function pDraw(){
  var w=tCanvas.width,h=tCanvas.height;
  tCtx.fillStyle='#050810';tCtx.fillRect(0,0,w,h);
  tCtx.strokeStyle='#0f1525';
  for(var i=1;i<5;i++){tCtx.beginPath();tCtx.moveTo(0,h/5*i);tCtx.lineTo(w,h/5*i);tCtx.stroke();}
  var z=ZONES[pPhase];var y1=h-(z.max/135)*h,y2=h-(z.min/135)*h;
  var colors=['rgba(0,230,118,0.15)','rgba(255,171,64,0.15)','rgba(255,82,82,0.15)'];
  tCtx.fillStyle=colors[pPhase];tCtx.fillRect(0,y1,w,y2-y1);
  var ty=h-(pTarget/135)*h;tCtx.strokeStyle='rgba(255,171,64,0.5)';tCtx.setLineDash([6,6]);
  tCtx.beginPath();tCtx.moveTo(0,ty);tCtx.lineTo(w,ty);tCtx.stroke();tCtx.setLineDash([]);
  var py=paddle.y,th=paddle.h*0.35,sh=paddle.h*0.65;
  var bend=Math.min(angle*0.4,35)*Math.PI/180;
  tCtx.shadowColor='#00d4f0';tCtx.shadowBlur=15;tCtx.strokeStyle='#00d4f0';tCtx.lineWidth=10;tCtx.lineCap='round';
  tCtx.beginPath();tCtx.moveTo(25,py);tCtx.lineTo(25,py+th);tCtx.stroke();
  tCtx.fillStyle='#050810';tCtx.beginPath();tCtx.arc(25,py+th,8,0,Math.PI*2);tCtx.fill();
  tCtx.strokeStyle='#00d4f0';tCtx.lineWidth=2;tCtx.stroke();
  var sx2=25+Math.sin(bend)*sh,sy2=py+th+Math.cos(bend)*sh;
  tCtx.lineWidth=10;tCtx.beginPath();tCtx.moveTo(25,py+th);tCtx.lineTo(sx2,sy2);tCtx.stroke();
  tCtx.fillStyle='#00d4f0';tCtx.beginPath();tCtx.arc(sx2,sy2,6,0,Math.PI*2);tCtx.fill();
  tCtx.shadowBlur=0;
  if(ball.on){tCtx.shadowColor='#fff';tCtx.shadowBlur=15;tCtx.fillStyle='#fff';tCtx.beginPath();tCtx.arc(ball.x,ball.y,ball.r,0,Math.PI*2);tCtx.fill();tCtx.shadowBlur=0;}
}

function pUpdateHUD(){
  document.getElementById('tAngle').textContent=angle.toFixed(0)+'°';
  document.getElementById('tFill').style.width=Math.min(angle/135*100,100)+'%';
  var err=Math.abs(angle-pTarget);var fb=document.getElementById('tFeedback');
  if(err<8){fb.textContent='Perfect!';fb.className='t-feedback good';}
  else if(err<18){fb.textContent='Close';fb.className='t-feedback close';}
  else{fb.textContent=angle<pTarget?'Bend more':'Straighten';fb.className='t-feedback far';}
}

function pShowToast(hit){
  var t=document.getElementById('toast');t.textContent=hit?'HIT!':'MISS';
  t.className='toast show'+(hit?'':' miss');setTimeout(function(){t.classList.remove('show');},250);
}

function endTest(){testOn=false;}

function showPongResults(){
  goTo('results');
  var totalHits=pZoneData.reduce(function(a,z){return a+z.h;},0);
  var allErr=[];var allRT=[];
  pZoneData.forEach(function(z){allErr=allErr.concat(z.e);allRT=allRT.concat(z.t);});
  var meanErr=allErr.length?allErr.reduce(function(a,b){return a+b;},0)/allErr.length:30;
  var accuracy=Math.max(0,Math.min(100,100-meanErr*2));
  var consistency=100;
  if(allErr.length>1){var v2=allErr.map(function(e){return(e-meanErr)*(e-meanErr);}).reduce(function(a,b){return a+b;},0)/allErr.length;consistency=Math.max(0,100-Math.sqrt(v2)*2.5);}
  var meanRT=allRT.length?allRT.reduce(function(a,b){return a+b;},0)/allRT.length:1000;
  var reaction=Math.max(0,Math.min(100,120-meanRT/12));
  var smoothness=100;
  if(pVelLog.length>10){var avgV=pVelLog.reduce(function(a,b){return a+b;},0)/pVelLog.length;var varV=pVelLog.map(function(v){return(v-avgV)*(v-avgV);}).reduce(function(a,b){return a+b;},0)/pVelLog.length;smoothness=Math.max(0,Math.min(100,100-Math.sqrt(varV)*0.15));}
  var overall=Math.round(accuracy*0.35+consistency*0.25+reaction*0.2+smoothness*0.2);
  document.getElementById('rScore').textContent=overall;
  var icon,verdict;
  if(overall>=80){icon='&#x1F3C6;';verdict='<strong>Excellent</strong> joint position sense. Proprioception within normal limits.';}
  else if(overall>=60){icon='&#x1F44D;';verdict='<strong>Good</strong> function with minor deficits. Targeted exercises may help.';}
  else if(overall>=40){icon='&#x1F4CB;';verdict='<strong>Fair</strong> performance. Proprioceptive training recommended.';}
  else{icon='&#x26A0;';verdict='<strong>Below normal</strong>. Consider rehabilitation program.';}
  document.getElementById('rIcon').innerHTML=icon;document.getElementById('rVerdict').innerHTML=verdict;
  [0,1,2].forEach(function(i){document.getElementById('rZ'+(i+1)).textContent=Math.round(pZoneData[i].h/5*100)+'%';});
  document.getElementById('rAccuracy').textContent=accuracy.toFixed(0)+'%';
  document.getElementById('rReaction').textContent=meanRT.toFixed(0)+'ms';
  document.getElementById('rConsistency').textContent=consistency.toFixed(0)+'%';
  document.getElementById('rSmoothness').textContent=smoothness.toFixed(0)+'%';
  var weakest=0;pZoneData.forEach(function(z,i){if(z.h<pZoneData[weakest].h)weakest=i;});
  var ranges=['near extension (15-35°)','mid-range (40-60°)','deep flexion (65-90°)'];
  var finding;
  if(totalHits>=12){finding='<strong>Strong proprioceptive function</strong> across all tested ranges.';}
  else if(pZoneData[weakest].h<=2){finding='<strong>Deficit detected</strong> in '+ranges[weakest]+'. ';if(weakest===0)finding+='Common after ACL injuries or early post-surgical recovery.';else if(weakest===1)finding+='Mid-range deficits may indicate general proprioceptive decline.';else finding+='Deep flexion difficulties may relate to posterior capsule tightness.';}
  else{finding='<strong>Moderate performance</strong>. Regular proprioceptive exercises recommended.';}
  document.getElementById('rFinding').innerHTML=finding;
}


connect();
</script>
</body>
</html>
)=====";


const char COMPARE_PAGE[] PROGMEM = R"=====(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>uOBionics — Bilateral Analysis</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
:root{--bg:#03070f;--card:#080f1a;--border:#1a2840;--text:#e8f0f8;--dim:#5a7090;--muted:#1a2840;--accent:#00d4f0;--garnet:#c41e3a;--success:#00e676;--warning:#ffab40;--danger:#ff5252;--L:#00d4f0;--R:#c084fc;--emg1:#ff6b9d;--emg2:#c084fc}
html,body{height:100%;overflow-y:auto;background:var(--bg);font-family:-apple-system,system-ui,sans-serif;color:var(--text)}
.hdr{padding:16px;text-align:center;border-bottom:1px solid var(--border);position:sticky;top:0;background:var(--bg);z-index:10}
.hdr h1{font-size:1.2rem;font-weight:700}.hdr h1 span{color:var(--garnet)}
.hdr-sub{font-size:0.7rem;color:var(--dim);margin-top:4px}
.conn-row{display:flex;justify-content:center;gap:16px;margin-top:8px}
.conn-item{font-size:0.65rem;display:flex;align-items:center;gap:5px}
.conn-dot{width:8px;height:8px;border-radius:50%;background:var(--danger)}.conn-dot.ok{background:var(--success);box-shadow:0 0 8px var(--success)}
.setup{padding:24px;text-align:center;display:none}
.setup.show{display:block}
.setup input{width:100%;max-width:260px;padding:12px;background:var(--card);border:1px solid var(--border);border-radius:10px;color:var(--text);font-family:inherit;font-size:0.9rem;margin:6px 0;text-align:center}
.setup-btn{margin-top:12px;padding:14px 40px;background:var(--accent);color:#020d16;border:none;border-radius:12px;font-weight:700;font-size:1rem;cursor:pointer}
.dash{padding:12px;display:none}
.dash.show{display:block}
.sec{margin-bottom:12px}
.cd{background:var(--card);border:1px solid var(--border);border-radius:14px;padding:16px}
.ct{font-size:0.55rem;color:var(--dim);text-transform:uppercase;letter-spacing:1.2px;margin-bottom:12px}

/* Bilateral angle hero */
.bi-hero{display:flex;align-items:center;justify-content:center;gap:20px;padding:12px 0}
.bi-leg{text-align:center}
.bi-leg-label{font-size:0.6rem;text-transform:uppercase;letter-spacing:1px;margin-bottom:4px}
.bi-leg-label.l{color:var(--L)}.bi-leg-label.r{color:var(--R)}
.bi-val{font-size:3.5rem;font-weight:200;font-family:monospace;line-height:1}
.bi-val.l{color:var(--L)}.bi-val.r{color:var(--R)}
.bi-vs{font-size:0.8rem;color:var(--dim);font-weight:600}

/* Asymmetry */
.asym-wrap{text-align:center;padding:8px 0}
.asym-bar-outer{height:24px;background:var(--muted);border-radius:12px;position:relative;overflow:hidden;margin:8px 0}
.asym-bar-l{position:absolute;top:0;right:50%;height:100%;background:var(--L);border-radius:12px 0 0 12px;transition:width 0.2s}
.asym-bar-r{position:absolute;top:0;left:50%;height:100%;background:var(--R);border-radius:0 12px 12px 0;transition:width 0.2s}
.asym-center{position:absolute;top:0;left:50%;width:2px;height:100%;background:var(--text);transform:translateX(-1px);z-index:1}
.asym-labels{display:flex;justify-content:space-between;font-size:0.6rem;color:var(--dim)}
.asym-score{font-size:2.5rem;font-weight:200;font-family:monospace;color:var(--accent)}
.asym-desc{font-size:0.75rem;color:var(--dim);margin-top:4px}
.asym-badge{display:inline-block;padding:4px 12px;border-radius:20px;font-size:0.7rem;font-weight:600;margin-top:8px}
.asym-badge.good{background:rgba(0,230,118,0.15);color:var(--success)}
.asym-badge.mod{background:rgba(255,171,64,0.15);color:var(--warning)}
.asym-badge.bad{background:rgba(255,82,82,0.15);color:var(--danger)}

/* ROM comparison */
.rom-comp{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.rom-item{background:var(--bg);border-radius:8px;padding:10px;text-align:center}
.rom-label{font-size:0.5rem;color:var(--dim);text-transform:uppercase}
.rom-val{font-size:1.3rem;font-weight:300;font-family:monospace;margin-top:2px}
.rom-val.l{color:var(--L)}.rom-val.r{color:var(--R)}

/* EMG comparison */
.emg-comp{display:flex;align-items:flex-end;justify-content:center;gap:12px;height:100px;padding:8px 0}
.emg-col{display:flex;flex-direction:column;align-items:center;gap:4px;width:40px}
.emg-col-bar{width:100%;background:var(--muted);border-radius:6px;position:relative;overflow:hidden}
.emg-col-fill{position:absolute;bottom:0;left:0;right:0;border-radius:6px;transition:height 0.15s}
.emg-col-fill.l1{background:var(--L)}.emg-col-fill.l2{background:rgba(0,212,240,0.5)}
.emg-col-fill.r1{background:var(--R)}.emg-col-fill.r2{background:rgba(192,132,252,0.5)}
.emg-col-label{font-size:0.5rem;color:var(--dim)}

/* Dual graph */
.graph-dual{position:relative;height:100px;background:var(--bg);border-radius:10px;overflow:hidden}
#dualGraph{position:absolute;inset:0;width:100%;height:100%}
.graph-legend{display:flex;justify-content:center;gap:16px;margin-top:6px}
.gl-item{font-size:0.6rem;display:flex;align-items:center;gap:4px}
.gl-dot{width:8px;height:8px;border-radius:50%}

/* Clinical card */
.clin{background:var(--card);border:1px solid var(--border);border-radius:14px;padding:16px}
.clin-title{font-size:0.6rem;color:var(--accent);text-transform:uppercase;letter-spacing:1px;margin-bottom:10px}
.clin-text{font-size:0.85rem;color:var(--dim);line-height:1.6}.clin-text strong{color:var(--text)}
.clin-metrics{display:grid;grid-template-columns:repeat(3,1fr);gap:6px;margin-top:10px}
.clin-m{background:var(--bg);border-radius:8px;padding:8px;text-align:center}
.clin-m-val{font-size:1.1rem;font-weight:500;font-family:monospace}
.clin-m-label{font-size:0.45rem;color:var(--dim);text-transform:uppercase;margin-top:2px}

/* Legs SVG */
.legs-wrap{display:flex;justify-content:center;gap:30px;padding:12px 0}
.leg-vis{text-align:center}
.leg-vis svg{width:50px;height:90px;overflow:visible}
.leg-vis-label{font-size:0.6rem;margin-top:4px}
.leg-vis-label.l{color:var(--L)}.leg-vis-label.r{color:var(--R)}
</style>
</head>
<body>

<div class="hdr">
  <h1>u<span>O</span>Bionics — Bilateral Analysis</h1>
  <div class="hdr-sub">Real-time bilateral knee comparison</div>
  <div class="conn-row">
    <div class="conn-item"><div class="conn-dot" id="cL"></div>Leg 1</div>
    <div class="conn-item"><div class="conn-dot" id="cR"></div>Leg 2</div>
  </div>
</div>

<div class="setup show" id="setup">
  <p style="color:var(--dim);font-size:0.85rem;margin-bottom:12px">Enter the IP addresses of each leg's device</p>
  <input id="ipL" placeholder="Leg 1 IP (e.g. 192.168.4.1)" value="192.168.4.1">
  <input id="ipR" placeholder="Leg 2 IP (e.g. 192.168.4.1)" value="192.168.4.1">
  <p style="color:var(--dim);font-size:0.65rem;margin-top:8px">Connect your phone to each WiFi network first.<br>Leg 1 = uOBionics, Leg 2 = uOBionics-L2</p>
  <button class="setup-btn" onclick="startCompare()">Start Comparison</button>
</div>

<div class="dash" id="dash">

  <div class="sec"><div class="cd">
    <div class="ct">Live Knee Angle</div>
    <div class="legs-wrap">
      <div class="leg-vis"><svg viewBox="0 0 50 90"><line x1="25" y1="5" x2="25" y2="40" stroke="#2a4060" stroke-width="5" stroke-linecap="round"/><circle cx="25" cy="40" r="5" fill="#080f1a" stroke="var(--L)" stroke-width="2"/><line id="shinL" x1="25" y1="40" x2="25" y2="82" stroke="var(--L)" stroke-width="5" stroke-linecap="round"/><circle id="ankleL" cx="25" cy="82" r="4" fill="var(--L)"/></svg><div class="leg-vis-label l">Left</div></div>
      <div class="leg-vis"><svg viewBox="0 0 50 90"><line x1="25" y1="5" x2="25" y2="40" stroke="#2a4060" stroke-width="5" stroke-linecap="round"/><circle cx="25" cy="40" r="5" fill="#080f1a" stroke="var(--R)" stroke-width="2"/><line id="shinR" x1="25" y1="40" x2="25" y2="82" stroke="var(--R)" stroke-width="5" stroke-linecap="round"/><circle id="ankleR" cx="25" cy="82" r="4" fill="var(--R)"/></svg><div class="leg-vis-label r">Right</div></div>
    </div>
    <div class="bi-hero">
      <div class="bi-leg"><div class="bi-leg-label l">Left Knee</div><div class="bi-val l" id="angL">--</div></div>
      <div class="bi-vs">vs</div>
      <div class="bi-leg"><div class="bi-leg-label r">Right Knee</div><div class="bi-val r" id="angR">--</div></div>
    </div>
  </div></div>

  <div class="sec"><div class="cd">
    <div class="ct">Symmetry Index</div>
    <div class="asym-wrap">
      <div class="asym-score" id="asymVal">--%</div>
      <div class="asym-bar-outer">
        <div class="asym-bar-l" id="asymBarL" style="width:0"></div>
        <div class="asym-bar-r" id="asymBarR" style="width:0"></div>
        <div class="asym-center"></div>
      </div>
      <div class="asym-labels"><span>Left dominant</span><span>Symmetrical</span><span>Right dominant</span></div>
      <div class="asym-badge good" id="asymBadge">Normal</div>
      <div class="asym-desc" id="asymDesc">Bilateral symmetry within normal limits</div>
    </div>
  </div></div>

  <div class="sec"><div class="cd">
    <div class="ct">Range of Motion Comparison</div>
    <div class="rom-comp">
      <div class="rom-item"><div class="rom-label">Left Min</div><div class="rom-val l" id="minL">--</div></div>
      <div class="rom-item"><div class="rom-label">Right Min</div><div class="rom-val r" id="minR">--</div></div>
      <div class="rom-item"><div class="rom-label">Left Max</div><div class="rom-val l" id="maxL">--</div></div>
      <div class="rom-item"><div class="rom-label">Right Max</div><div class="rom-val r" id="maxR">--</div></div>
      <div class="rom-item"><div class="rom-label">Left ROM</div><div class="rom-val l" id="romL">--</div></div>
      <div class="rom-item"><div class="rom-label">Right ROM</div><div class="rom-val r" id="romR">--</div></div>
    </div>
  </div></div>

  <div class="sec"><div class="cd">
    <div class="ct">EMG Muscle Balance</div>
    <div class="emg-comp">
      <div class="emg-col"><div class="emg-col-bar" style="height:80px"><div class="emg-col-fill l1" id="eL1" style="height:0%"></div></div><div class="emg-col-label">L-M1</div></div>
      <div class="emg-col"><div class="emg-col-bar" style="height:80px"><div class="emg-col-fill l2" id="eL2" style="height:0%"></div></div><div class="emg-col-label">L-M2</div></div>
      <div style="width:1px;height:80px;background:var(--border)"></div>
      <div class="emg-col"><div class="emg-col-bar" style="height:80px"><div class="emg-col-fill r1" id="eR1" style="height:0%"></div></div><div class="emg-col-label">R-M1</div></div>
      <div class="emg-col"><div class="emg-col-bar" style="height:80px"><div class="emg-col-fill r2" id="eR2" style="height:0%"></div></div><div class="emg-col-label">R-M2</div></div>
    </div>
  </div></div>

  <div class="sec"><div class="cd">
    <div class="ct">Angle Overlay (5s)</div>
    <div class="graph-dual"><canvas id="dualGraph"></canvas></div>
    <div class="graph-legend">
      <div class="gl-item"><div class="gl-dot" style="background:var(--L)"></div>Left</div>
      <div class="gl-item"><div class="gl-dot" style="background:var(--R)"></div>Right</div>
    </div>
  </div></div>

  <div class="sec"><div class="clin">
    <div class="clin-title">Clinical Assessment</div>
    <div class="clin-text" id="clinText">Collecting data...</div>
    <div class="clin-metrics">
      <div class="clin-m"><div class="clin-m-val" id="clinSI">--</div><div class="clin-m-label">Symmetry Index</div></div>
      <div class="clin-m"><div class="clin-m-val" id="clinROM">--</div><div class="clin-m-label">ROM Deficit</div></div>
      <div class="clin-m"><div class="clin-m-val" id="clinEMG">--</div><div class="clin-m-label">EMG Balance</div></div>
    </div>
  </div></div>

</div>

<script>
var wsL,wsR,dL={knee:0,e1:0,e2:0},dR={knee:0,e1:0,e2:0};
var connL=false,connR=false;
var minL=null,maxL=null,minR=null,maxR=null;
var gL=[],gR=[];
var MX=1000; // EMG max for normalization

function startCompare(){
  var ipL=document.getElementById('ipL').value.trim();
  var ipR=document.getElementById('ipR').value.trim();
  document.getElementById('setup').classList.remove('show');
  document.getElementById('dash').classList.add('show');
  connectWS('L',ipL);
  connectWS('R',ipR);
  setInterval(updateAll,50);
}

function connectWS(leg,ip){
  var ws=new WebSocket('ws://'+ip+':81/');
  ws.onopen=function(){
    if(leg==='L'){connL=true;document.getElementById('cL').classList.add('ok');}
    else{connR=true;document.getElementById('cR').classList.add('ok');}
  };
  ws.onclose=function(){
    if(leg==='L'){connL=false;document.getElementById('cL').classList.remove('ok');}
    else{connR=false;document.getElementById('cR').classList.remove('ok');}
    setTimeout(function(){connectWS(leg,ip);},2000);
  };
  ws.onmessage=function(e){
    var d=JSON.parse(e.data);
    if(d.knee===undefined)return;
    if(leg==='L')dL=d; else dR=d;
  };
  if(leg==='L')wsL=ws;else wsR=ws;
}

function updateAll(){
  var aL=dL.knee||0,aR=dR.knee||0;

  // Angles
  document.getElementById('angL').textContent=aL.toFixed(0)+'°';
  document.getElementById('angR').textContent=aR.toFixed(0)+'°';

  // Leg animations
  updateLeg('shinL','ankleL',aL);
  updateLeg('shinR','ankleR',aR);

  // ROM tracking
  if(connL){
    if(minL===null||aL<minL)minL=aL;
    if(maxL===null||aL>maxL)maxL=aL;
  }
  if(connR){
    if(minR===null||aR<minR)minR=aR;
    if(maxR===null||aR>maxR)maxR=aR;
  }
  document.getElementById('minL').textContent=minL!==null?minL.toFixed(0)+'°':'--';
  document.getElementById('minR').textContent=minR!==null?minR.toFixed(0)+'°':'--';
  document.getElementById('maxL').textContent=maxL!==null?maxL.toFixed(0)+'°':'--';
  document.getElementById('maxR').textContent=maxR!==null?maxR.toFixed(0)+'°':'--';
  var rL=minL!==null&&maxL!==null?maxL-minL:0;
  var rR=minR!==null&&maxR!==null?maxR-minR:0;
  document.getElementById('romL').textContent=rL.toFixed(0)+'°';
  document.getElementById('romR').textContent=rR.toFixed(0)+'°';

  // Symmetry Index (Limb Symmetry Index - standard clinical metric)
  // LSI = (involved/uninvolved) × 100
  // Asymmetry = |L-R| / max(L,R) × 100
  var maxA=Math.max(aL,aR,1);
  var asym=Math.abs(aL-aR)/maxA*100;
  document.getElementById('asymVal').textContent=asym.toFixed(0)+'%';
  var pL=Math.min(aL/135*50,50),pR=Math.min(aR/135*50,50);
  document.getElementById('asymBarL').style.width=pL+'%';
  document.getElementById('asymBarR').style.width=pR+'%';
  var badge=document.getElementById('asymBadge');
  var desc=document.getElementById('asymDesc');
  if(asym<10){badge.textContent='Normal';badge.className='asym-badge good';desc.textContent='Bilateral symmetry within normal limits (<10%)';}
  else if(asym<20){badge.textContent='Mild Asymmetry';badge.className='asym-badge mod';desc.textContent='Minor bilateral difference detected (10-20%)';}
  else{badge.textContent='Significant Asymmetry';badge.className='asym-badge bad';desc.textContent='Significant bilateral deficit (>20%) — clinical attention recommended';}

  // EMG bars
  var e1L=Math.abs(dL.e1||0),e2L=Math.abs(dL.e2||0);
  var e1R=Math.abs(dR.e1||0),e2R=Math.abs(dR.e2||0);
  document.getElementById('eL1').style.height=Math.min(e1L/MX*100,100)+'%';
  document.getElementById('eL2').style.height=Math.min(e2L/MX*100,100)+'%';
  document.getElementById('eR1').style.height=Math.min(e1R/MX*100,100)+'%';
  document.getElementById('eR2').style.height=Math.min(e2R/MX*100,100)+'%';

  // Graph
  gL.push(aL);gR.push(aR);
  if(gL.length>100){gL.shift();gR.shift();}
  drawDual();

  // Clinical assessment
  updateClinical(aL,aR,rL,rR,e1L+e2L,e1R+e2R,asym);
}

function updateLeg(shinId,ankleId,angle){
  var rad=Math.min(angle,135)*Math.PI/180,len=42;
  var x=25+Math.sin(rad)*len,y=40+Math.cos(rad)*len;
  document.getElementById(shinId).setAttribute('x2',x);
  document.getElementById(shinId).setAttribute('y2',y);
  document.getElementById(ankleId).setAttribute('cx',x);
  document.getElementById(ankleId).setAttribute('cy',y);
}

function drawDual(){
  var c=document.getElementById('dualGraph');if(!c)return;
  var w=c.parentElement.clientWidth,h=c.parentElement.clientHeight;
  c.width=w;c.height=h;
  var ctx=c.getContext('2d');
  ctx.fillStyle='#080f1a';ctx.fillRect(0,0,w,h);
  // Grid
  ctx.strokeStyle='#111d2e';ctx.lineWidth=1;ctx.setLineDash([3,4]);
  for(var i=1;i<4;i++){ctx.beginPath();ctx.moveTo(0,h/4*i);ctx.lineTo(w,h/4*i);ctx.stroke();}
  ctx.setLineDash([]);
  if(gL.length<2)return;
  // Left
  ctx.strokeStyle='#00d4f0';ctx.lineWidth=2;ctx.beginPath();
  gL.forEach(function(v,i){var px=i/(gL.length-1)*w,py=h-(v/135)*h*0.9-h*0.05;i===0?ctx.moveTo(px,py):ctx.lineTo(px,py);});
  ctx.stroke();
  // Right
  ctx.strokeStyle='#c084fc';ctx.lineWidth=2;ctx.beginPath();
  gR.forEach(function(v,i){var px=i/(gR.length-1)*w,py=h-(v/135)*h*0.9-h*0.05;i===0?ctx.moveTo(px,py):ctx.lineTo(px,py);});
  ctx.stroke();
}

function updateClinical(aL,aR,romL,romR,emgL,emgR,asym){
  var maxROM=Math.max(romL,romR,1);
  var romDeficit=Math.abs(romL-romR)/maxROM*100;
  var maxEMG=Math.max(emgL,emgR,1);
  var emgBalance=100-Math.abs(emgL-emgR)/maxEMG*100;

  document.getElementById('clinSI').textContent=asym.toFixed(0)+'%';
  document.getElementById('clinROM').textContent=romDeficit.toFixed(0)+'%';
  document.getElementById('clinEMG').textContent=emgBalance.toFixed(0)+'%';

  var txt='';
  var weaker=romL<romR?'left':'right';
  if(asym<10&&romDeficit<15){
    txt='<strong>Bilateral function appears symmetric.</strong> Both knees demonstrate comparable range of motion and muscle activation patterns. No significant side-to-side deficits detected. This is consistent with normal bilateral knee function.';
  } else if(asym<20){
    txt='<strong>Mild bilateral asymmetry detected.</strong> The '+weaker+' knee shows slightly reduced ROM ('+romDeficit.toFixed(0)+'% deficit). This may indicate early-stage muscle weakness or guarding behavior. Targeted strengthening exercises for the '+weaker+' side are recommended.';
  } else {
    txt='<strong>Significant bilateral asymmetry ('+asym.toFixed(0)+'%).</strong> The '+weaker+' knee demonstrates a '+romDeficit.toFixed(0)+'% ROM deficit compared to the contralateral side. ';
    if(emgBalance<70) txt+='EMG data shows muscle activation imbalance ('+emgBalance.toFixed(0)+'% balance), suggesting neuromuscular compensation. ';
    txt+='Clinical assessment recommended. This pattern may indicate ligamentous laxity, post-surgical deficit, or proprioceptive impairment requiring structured rehabilitation.';
  }
  document.getElementById('clinText').innerHTML=txt;
}
</script>
</body>
</html>
)=====";
void onWsEvent(uint8_t n,WStype_t t,uint8_t*p,size_t l){
  if(t!=WStype_TEXT)return;
  CmdPacket cmd;
  // Default to leg 1, but check for leg-specific commands
  if(strcmp((char*)p,"CALIBRATE")==0||strcmp((char*)p,"CALIBRATE_L1")==0){cmd.cmd=1;esp_now_send(s3imuL1,(uint8_t*)&cmd,sizeof(cmd));}
  else if(strcmp((char*)p,"CALIBRATE_L2")==0){cmd.cmd=1;esp_now_send(s3imuL2,(uint8_t*)&cmd,sizeof(cmd));}
  else if(strcmp((char*)p,"RESET_ENC")==0||strcmp((char*)p,"RESET_ENC_L1")==0){cmd.cmd=2;esp_now_send(s3imuL1,(uint8_t*)&cmd,sizeof(cmd));}
  else if(strcmp((char*)p,"RESET_ENC_L2")==0){cmd.cmd=2;esp_now_send(s3imuL2,(uint8_t*)&cmd,sizeof(cmd));}
}

void setup(){
  Serial.begin(115200);delay(1000);
  Serial.println("\n=== uOBionics WROOM ===");
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(WIFI_SSID,WIFI_PASS,1);
  if(esp_now_init()!=ESP_OK){Serial.println("ESP-NOW FAIL!");}
  else{
    esp_now_peer_info_t p1={};memcpy(p1.peer_addr,s3emgL1,6);p1.channel=1;p1.encrypt=false;esp_now_add_peer(&p1);
    esp_now_peer_info_t p2={};memcpy(p2.peer_addr,s3imuL1,6);p2.channel=1;p2.encrypt=false;esp_now_add_peer(&p2);
    esp_now_peer_info_t p3={};memcpy(p3.peer_addr,s3emgL2,6);p3.channel=1;p3.encrypt=false;esp_now_add_peer(&p3);
    esp_now_peer_info_t p4={};memcpy(p4.peer_addr,s3imuL2,6);p4.channel=1;p4.encrypt=false;esp_now_add_peer(&p4);
    esp_now_register_recv_cb(onRecv);
    Serial.println("ESP-NOW OK");
  }
  server.on("/",[](){server.send_P(200,"text/html",WEBPAGE);});
  server.on("/compare",[](){server.send_P(200,"text/html",COMPARE_PAGE);});
  // CORS headers for cross-origin comparison page
  server.on("/data",[](){
    server.sendHeader("Access-Control-Allow-Origin","*");
    String j="{\"l1k\":"+String(g1Knee,1)+",\"l1e1\":"+String(g1Emg1,1)+",\"l1e2\":"+String(g1Emg2,1)+
      ",\"l2k\":"+String(g2Knee,1)+",\"l2e1\":"+String(g2Emg1,1)+",\"l2e2\":"+String(g2Emg2,1)+"}";
    server.send(200,"application/json",j);
  });
  server.begin();webSocket.begin();webSocket.onEvent(onWsEvent);
  Serial.printf("WiFi: %s Pass: %s\nhttp://%s\n\n",WIFI_SSID,WIFI_PASS,WiFi.softAPIP().toString().c_str());
}

unsigned long lastBC=0;
void loop(){
  webSocket.loop();server.handleClient();
  if(gNewData&&millis()-lastBC>=50){
    lastBC=millis();gNewData=false;
    String j="{\"knee\":"+String(g1Knee,1)+",\"raw\":"+String(g1Raw,1)+
      ",\"e1\":"+String(g1Emg1,1)+",\"e2\":"+String(g1Emg2,1)+
      ",\"ec1\":"+String(g1Ec1?"true":"false")+",\"ec2\":"+String(g1Ec2?"true":"false")+
      ",\"enc\":"+String(g1Enc,1)+",\"encCount\":"+String(g1EncCnt)+
      ",\"a1x\":"+String(g1a1x,3)+",\"a1y\":"+String(g1a1y,3)+",\"a1z\":"+String(g1a1z,3)+
      ",\"a2x\":"+String(g1a2x,3)+",\"a2y\":"+String(g1a2y,3)+",\"a2z\":"+String(g1a2z,3)+
      ",\"l2k\":"+String(g2Knee,1)+",\"l2r\":"+String(g2Raw,1)+
      ",\"l2e1\":"+String(g2Emg1,1)+",\"l2e2\":"+String(g2Emg2,1)+
      ",\"l2ec1\":"+String(g2Ec1?"true":"false")+",\"l2ec2\":"+String(g2Ec2?"true":"false")+
      ",\"l2enc\":"+String(g2Enc,1)+",\"l2encCount\":"+String(g2EncCnt)+
      ",\"l2a1x\":"+String(g2a1x,3)+",\"l2a1y\":"+String(g2a1y,3)+",\"l2a1z\":"+String(g2a1z,3)+
      ",\"l2a2x\":"+String(g2a2x,3)+",\"l2a2y\":"+String(g2a2y,3)+",\"l2a2z\":"+String(g2a2z,3)+"}";
    webSocket.broadcastTXT(j);
  }
}
