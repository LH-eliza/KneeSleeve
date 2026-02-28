/*
 * uOBionics Smart Knee Sleeve
 * True North Biomedical Competition
 * University of Ottawa
 */

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Wire.h>
#include <math.h>

const char* WIFI_SSID = "uOBionics";
const char* WIFI_PASS = "knee2026";

#define IMU1_SDA 18
#define IMU1_SCL 19
#define IMU2_SDA 32
#define IMU2_SCL 33
#define IMU_ADDR 0x6A
#define LED_PIN 2

TwoWire I2C_1 = TwoWire(0);
TwoWire I2C_2 = TwoWire(1);
WebServer server(80);
WebSocketsServer webSocket(81);

float calibrationAngle = 180.0;
float smoothedAngle = 180;

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

/* Tooltip */
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

  <div class="controls">
    <button class="ctrl ctrl-rec" id="recBtn" onclick="toggleRec()">Record</button>
    <button class="ctrl ctrl-cal" onclick="goTo('calibrate')">Recalibrate</button>
  </div>
</div>

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
  if(id==='monitor'||id==='simple'){start=Date.now();minA=maxA=null;ext=0;flex=0;gData=[];}
  if(id==='calibrate'){showCalStep(1);}
}

function startMode(mode){
  currentMode=mode;
  if(mode==='simple'){resetSimple();goTo('simple');}
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
  if(rec)recD.push({t:Date.now(),angle:angle,rawAngle:rawAngle,vel:vel,a1x:d.a1x,a1y:d.a1y,a1z:d.a1z,a2x:d.a2x,a2y:d.a2y,a2z:d.a2z});
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
  var csv='Time(ms),Knee(deg),Raw(deg),Velocity(deg/s),Thigh_X,Thigh_Y,Thigh_Z,Shin_X,Shin_Y,Shin_Z\n';
  var t0=recD[0].t;
  recD.forEach(function(r){
    csv+=(r.t-t0)+','+r.angle.toFixed(1)+','+r.rawAngle.toFixed(1)+','+r.vel.toFixed(1);
    if(r.a1x!==undefined)csv+=','+r.a1x.toFixed(3)+','+r.a1y.toFixed(3)+','+r.a1z.toFixed(3)+','+r.a2x.toFixed(3)+','+r.a2y.toFixed(3)+','+r.a2z.toFixed(3);
    csv+='\n';
  });
  var a=document.createElement('a');
  a.href=URL.createObjectURL(new Blob([csv],{type:'text/csv'}));
  a.download='uobionics_'+new Date().toISOString().slice(0,10)+'.csv';
  a.click();
}

connect();
</script>
</body>
</html>
)=====";

void i2cWrite(TwoWire &wire,uint8_t reg,uint8_t val){wire.beginTransmission(IMU_ADDR);wire.write(reg);wire.write(val);wire.endTransmission();}
uint8_t i2cRead(TwoWire &wire,uint8_t reg){wire.beginTransmission(IMU_ADDR);wire.write(reg);wire.endTransmission(false);wire.requestFrom((uint8_t)IMU_ADDR,(uint8_t)1);return wire.read();}
bool initIMU(TwoWire &wire,int sda,int scl){wire.begin(sda,scl);wire.setClock(400000);delay(100);if(i2cRead(wire,0x0F)!=0x6C)return false;i2cWrite(wire,0x12,0x01);delay(50);i2cWrite(wire,0x10,0x40);delay(10);return true;}
void readAccel(TwoWire &wire,float*ax,float*ay,float*az){uint8_t b[6];wire.beginTransmission(IMU_ADDR);wire.write(0x28);wire.endTransmission(false);wire.requestFrom((uint8_t)IMU_ADDR,(uint8_t)6);for(int i=0;i<6&&wire.available();i++)b[i]=wire.read();*ax=(int16_t)(b[1]<<8|b[0])*0.000122f;*ay=(int16_t)(b[3]<<8|b[2])*0.000122f;*az=(int16_t)(b[5]<<8|b[4])*0.000122f;}
float calcAngle(float a1x,float a1y,float a1z,float a2x,float a2y,float a2z){float m1=sqrt(a1x*a1x+a1y*a1y+a1z*a1z);float m2=sqrt(a2x*a2x+a2y*a2y+a2z*a2z);float d=a1x*a2x+a1y*a2y+a1z*a2z;return acos(constrain(d/(m1*m2),-1.0,1.0))*180.0/PI;}
void doCalibrate(){digitalWrite(LED_PIN,HIGH);float sum=0;for(int i=0;i<20;i++){float a1x,a1y,a1z,a2x,a2y,a2z;readAccel(I2C_1,&a1x,&a1y,&a1z);readAccel(I2C_2,&a2x,&a2y,&a2z);sum+=calcAngle(a1x,a1y,a1z,a2x,a2y,a2z);delay(50);}calibrationAngle=sum/20.0;digitalWrite(LED_PIN,LOW);webSocket.broadcastTXT("{\"calDone\":true,\"offset\":"+String(calibrationAngle,1)+"}");}
void onWsEvent(uint8_t n,WStype_t t,uint8_t*p,size_t l){if(t==WStype_TEXT&&strcmp((char*)p,"CALIBRATE")==0)doCalibrate();}
void setup(){Serial.begin(115200);pinMode(LED_PIN,OUTPUT);if(!initIMU(I2C_1,IMU1_SDA,IMU1_SCL)||!initIMU(I2C_2,IMU2_SDA,IMU2_SCL)){while(1){digitalWrite(LED_PIN,!digitalRead(LED_PIN));delay(200);}}WiFi.mode(WIFI_AP);WiFi.softAP(WIFI_SSID,WIFI_PASS);server.on("/",[](){server.send_P(200,"text/html",WEBPAGE);});server.begin();webSocket.begin();webSocket.onEvent(onWsEvent);Serial.printf("\n\nuOBionics\nWiFi: %s\nPass: %s\nhttp://%s\n\n",WIFI_SSID,WIFI_PASS,WiFi.softAPIP().toString().c_str());}
unsigned long lastSample=0;
void loop(){webSocket.loop();server.handleClient();if(millis()-lastSample>=50){lastSample=millis();float a1x,a1y,a1z,a2x,a2y,a2z;readAccel(I2C_1,&a1x,&a1y,&a1z);readAccel(I2C_2,&a2x,&a2y,&a2z);float raw=calcAngle(a1x,a1y,a1z,a2x,a2y,a2z);smoothedAngle=smoothedAngle*0.5+raw*0.5;float knee=abs(calibrationAngle-smoothedAngle);webSocket.broadcastTXT("{\"knee\":"+String(knee,1)+",\"raw\":"+String(smoothedAngle,1)+",\"a1x\":"+String(a1x,3)+",\"a1y\":"+String(a1y,3)+",\"a1z\":"+String(a1z,3)+",\"a2x\":"+String(a2x,3)+",\"a2y\":"+String(a2y,3)+",\"a2z\":"+String(a2z,3)+"}");}}
