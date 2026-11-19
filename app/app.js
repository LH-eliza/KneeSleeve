/* uOBionics Smart Knee Sleeve — interactive app.
 * Sensor data is simulated in the browser (no sleeve hardware is read). */
(function () {
  "use strict";

  /* ---------- helpers ---------- */
  var $ = function (s, r) { return (r || document).querySelector(s); };
  var $$ = function (s, r) { return Array.prototype.slice.call((r || document).querySelectorAll(s)); };
  function icon(name, size, cls) {
    var svg = window.ICONS[name].replace(/^<svg[^>]*>/, function (root) { return root.replace(/\s(width|height)="\d+"/g, ""); });
    return svg.replace("<svg", '<svg aria-hidden="true"' + (size ? ' width="' + size + '" height="' + size + '"' : "") + (cls ? ' class="' + cls + '"' : ""));
  }
  function pad(n) { return (n < 10 ? "0" : "") + n; }
  function fmtTime(sec) { sec = Math.max(0, Math.floor(sec)); return pad(Math.floor(sec / 60)) + ":" + pad(sec % 60); }
  function sgn(v) { var s = v.toFixed(2); return v < 0 ? "−" + s.slice(1) : "+" + s; }
  function clamp(v, a, b) { return Math.min(b, Math.max(a, v)); }
  function rnd(a) { return (Math.random() - 0.5) * 2 * a; }
  var toastTimer;
  function toast(msg) {
    var t = $("#toast"); t.textContent = msg; t.hidden = false;
    clearTimeout(toastTimer); toastTimer = setTimeout(function () { t.hidden = true; }, 2800);
  }

  /* ---------- state ---------- */
  var S = {
    connected: false, connecting: false,
    calibrated: false, reference: 180,
    f: 0, raw: 180, vel: 0, phase: 0, stage: "down",
    minF: null, maxF: null,
    reps: 0, flexEv: 0, extEv: 0, goal: 10,
    sessionStart: null,
    recording: false, recStart: 0, recSamples: [], recSeconds: 0, hasRecording: false,
    hist: [], advancedPref: false, watch: false
  };
  try { S.advancedPref = localStorage.getItem("uob.advanced") === "1"; } catch (e) {}
  var T_CYCLE = 5.2;   // seconds per simulated bend-and-straighten cycle

  /* ---------- routes ---------- */
  var ROUTES = {
    welcome:   { title: "Welcome", nav: "welcome", badge: false },
    straighten:{ title: "Calibration · Straighten leg", nav: "setup", badge: false },
    zero:      { title: "Calibration · Set zero point", nav: "setup", badge: false },
    complete:  { title: "Calibration complete", nav: "setup", badge: false },
    mode:      { title: "Choose your mode", nav: "mode", badge: true, guard: true },
    exercise:  { title: "Knee Exercise", nav: "exercise", badge: true, guard: true },
    advanced:  { title: "Advanced monitor", nav: "advanced", badge: true, guard: true },
    settings:  { title: "Settings", nav: null, badge: true }
  };
  var NAV = [
    { id: "welcome", label: "Device connection", icon: "bluetooth", href: "#/welcome" },
    { id: "setup", label: "Setup & Calibration", icon: "sliders-horizontal", href: "#/straighten" },
    { id: "mode", label: "Choose your mode", icon: "layout-grid", href: "#/mode" },
    { id: "exercise", label: "Knee Exercise", icon: "activity", href: "#/exercise" },
    { id: "advanced", label: "Advanced monitor", icon: "chart-no-axes-combined", href: "#/advanced" }
  ];
  var current = "welcome";
  var live = [];           // per-frame updaters for the mounted view
  var cleanups = [];       // timers etc. to clear on navigation

  function go(route) { location.hash = "#/" + route; }
  function currentRoute() {
    var h = (location.hash || "").replace(/^#\/?/, "");
    return ROUTES[h] ? h : "welcome";
  }

  /* ---------- shell rendering ---------- */
  function badge(text, kind) { return '<span class="badge ' + (kind || "") + '">' + text + "</span>"; }

  function renderShell() {
    var r = ROUTES[current];
    $("#nav").innerHTML = NAV.map(function (n) {
      return '<a href="' + n.href + '"' + (n.id === r.nav ? ' aria-current="page"' : "") + ">" + icon(n.icon) + "<span>" + n.label + "</span></a>";
    }).join("") + '<a href="#/settings"' + (current === "settings" ? ' aria-current="page"' : "") + ">" + icon("sliders-horizontal") + "<span>Settings</span></a>";
    $("#deviceSummary").innerHTML = icon("bluetooth") +
      '<p class="name">Smart Knee Sleeve</p>' +
      (S.connected ? badge("Connected · demo") : S.connecting ? badge("Connecting…", "neutral") : badge("Not connected", "danger")) +
      '<p class="sub">Thigh + shin sensors</p>';
    $("#pageName").textContent = r.title;
    document.title = r.title + " · uOBionics";
    $("#topStatus").innerHTML = r.badge ? '<span class="full">' + badge("Device connected") + '</span><span class="short">' + badge("Connected") + "</span>" : "";
    $("#menuBtn").innerHTML = icon("menu");
  }

  function setNav(open) {
    document.body.classList.toggle("nav-open", open);
    $("#backdrop").hidden = !open;
    $("#menuBtn").setAttribute("aria-expanded", open ? "true" : "false");
    if (open) { var a = $("#nav a"); if (a) a.focus(); }
  }

  /* ---------- sensor simulation ---------- */
  var last = performance.now(), histAcc = 0, recAcc = 0;
  function isMoving() { return (S.watch && (current === "straighten" || current === "zero")) || (current === "exercise" || current === "advanced"); }

  function derived() {
    var rawR = Math.round(S.raw), refR = Math.round(S.reference);
    return { rawR: rawR, refR: refR, flex: Math.max(0, refR - rawR) };
  }

  function tick() {
    var now = performance.now();
    var dt = Math.min(0.25, (now - last) / 1000); last = now;
    var moving = isMoving();
    if (moving) S.phase += dt;
    var target = moving ? 3 + 105 * (0.5 - 0.5 * Math.cos(2 * Math.PI * S.phase / T_CYCLE)) : 0;
    var prev = S.f;
    S.f += (target - S.f) * Math.min(1, dt * 8);
    S.raw = 180 - S.f + (moving ? rnd(0.25) : 0);
    var v = dt > 0 ? Math.abs(S.f - prev) / dt : 0;
    S.vel += (v - S.vel) * Math.min(1, dt * 6);
    var d = derived();

    if (moving) {
      if (S.minF === null || d.flex < S.minF) S.minF = d.flex;
      if (S.maxF === null || d.flex > S.maxF) S.maxF = d.flex;
      if (S.stage === "down" && d.flex > 95) { S.stage = "up"; S.flexEv++; }
      else if (S.stage === "up" && d.flex < 15) { S.stage = "down"; S.extEv++; S.reps++; }
    }
    histAcc += dt;
    while (histAcc >= 0.05) { histAcc -= 0.05; S.hist.push(S.f); if (S.hist.length > 100) S.hist.shift(); }

    if (S.recording) {
      S.recSeconds = (now - S.recStart) / 1000;
      recAcc += dt;
      while (recAcc >= 0.1) { recAcc -= 0.1; S.recSamples.push(sampleRow(S.recSeconds, d)); }
    }
    for (var i = 0; i < live.length; i++) live[i](d);
  }

  function imu(d) {
    var rad = d.flex * Math.PI / 180;
    return {
      t: [0.01 + rnd(0.01), -0.02 + rnd(0.01), 1.0 + rnd(0.01)],
      s: [Math.cos(rad) + rnd(0.01), 0.03 + rnd(0.01), Math.sin(rad) + rnd(0.01)]
    };
  }
  function sampleRow(t, d) {
    var m = imu(d);
    return [t.toFixed(2), S.raw.toFixed(2), d.flex, d.refR, S.vel.toFixed(1)]
      .concat(m.t.map(function (x) { return x.toFixed(3); }), m.s.map(function (x) { return x.toFixed(3); })).join(",");
  }
  function exportCsv() {
    var header = "time_s,raw_angle_deg,knee_flexion_deg,reference_deg,velocity_deg_s,thigh_x_g,thigh_y_g,thigh_z_g,shin_x_g,shin_y_g,shin_z_g";
    var blob = new Blob([header + "\n" + S.recSamples.join("\n") + "\n"], { type: "text/csv" });
    var n = new Date(), name = "kneesleeve_session_" + n.getFullYear() + pad(n.getMonth() + 1) + pad(n.getDate()) + "_" + pad(n.getHours()) + pad(n.getMinutes()) + pad(n.getSeconds()) + ".csv";
    var a = document.createElement("a"); a.href = URL.createObjectURL(blob); a.download = name;
    document.body.appendChild(a); a.click(); a.remove();
    setTimeout(function () { URL.revokeObjectURL(a.href); }, 2000);
    toast("Exported " + name);
  }
  function sessionSeconds() { return S.sessionStart ? (performance.now() - S.sessionStart) / 1000 : 0; }

  /* ---------- shared components ---------- */
  var NS = "http://www.w3.org/2000/svg";
  var GAUGE_MAX = 135, CX = 130, CY = 116, R = 104;
  function pt(phi) { var a = phi * Math.PI / 180; return [CX + R * Math.sin(a), CY - R * Math.cos(a)]; }
  function arc(f0, f1) {
    var p0 = -135 + f0 * 2, p1 = -135 + f1 * 2, a = pt(p0), b = pt(p1);
    return "M" + a[0].toFixed(2) + " " + a[1].toFixed(2) + " A" + R + " " + R + " 0 " + ((p1 - p0) > 180 ? 1 : 0) + " 1 " + b[0].toFixed(2) + " " + b[1].toFixed(2);
  }
  function gaugeCard(id) {
    return '<div class="gauge-card"><div class="hd"><b>Knee flexion</b><span>Demo live</span></div>' +
      '<div class="gauge" role="img" aria-label="Knee flexion gauge" id="' + id + '">' +
      '<svg viewBox="0 0 260 234"><path d="' + arc(0, GAUGE_MAX) + '" fill="none" stroke="#eff6fa" stroke-width="8" stroke-linecap="round"/>' +
      '<path class="gf" d="" fill="none" stroke="#16587b" stroke-width="8" stroke-linecap="round"/></svg>' +
      '<span class="lbl" style="left:36px;top:208px">0°</span><span class="lbl" style="left:196px;top:208px">135°</span>' +
      '<div class="val gv">0°</div><div class="cap">Current knee bend</div></div></div>';
  }
  function updGauge(id, flex) {
    var root = $("#" + id); if (!root) return;
    var f = clamp(flex, 0, GAUGE_MAX);
    var p = $(".gf", root);
    p.setAttribute("d", f < 0.6 ? "" : arc(0, f));
    $(".gv", root).textContent = flex + "°";
  }
  function legSvg() { return '<svg viewBox="0 0 400 144" preserveAspectRatio="xMidYMid meet" aria-hidden="true"><g id="legG"></g></svg>'; }
  function updLeg(flex) {
    var g = $("#legG"); if (!g) return;
    var jx = 170, jy = 74, thigh = 66, shin = 76, rad = flex * Math.PI / 180, half = rad / 2;
    var sx = jx + shin * Math.sin(rad), sy = jy + shin * Math.cos(rad);
    var ar = 28, ax = jx + ar * Math.sin(rad), ay = jy + ar * Math.cos(rad);
    var lx = jx + 50 * Math.sin(half), ly = jy + 50 * Math.cos(half);
    g.innerHTML =
      '<line x1="' + jx + '" y1="' + jy + '" x2="' + jx + '" y2="' + (jy + 56) + '" stroke="#9ea2ae" stroke-width="1.5" stroke-dasharray="3 3"/>' +
      '<line x1="' + jx + '" y1="' + (jy - thigh) + '" x2="' + jx + '" y2="' + jy + '" stroke="#16587b" stroke-width="5" stroke-linecap="round"/>' +
      '<line x1="' + jx + '" y1="' + jy + '" x2="' + sx.toFixed(1) + '" y2="' + sy.toFixed(1) + '" stroke="#16587b" stroke-width="5" stroke-linecap="round"/>' +
      '<path d="M' + jx + " " + (jy + ar) + " A" + ar + " " + ar + " 0 0 0 " + ax.toFixed(1) + " " + ay.toFixed(1) + '" fill="none" stroke="#16587b" stroke-width="1.5"/>' +
      '<circle cx="' + jx + '" cy="' + jy + '" r="7" fill="#fff" stroke="#16587b" stroke-width="3"/>' +
      '<text x="' + (jx + 16) + '" y="' + (jy - thigh + 12) + '" font-size="13" fill="#4d5461" font-family="inherit">Thigh IMU</text>' +
      '<text x="' + (sx + 12).toFixed(1) + '" y="' + (sy + 5).toFixed(1) + '" font-size="13" fill="#4d5461" font-family="inherit">Shin IMU</text>' +
      '<text x="' + lx.toFixed(1) + '" y="' + (ly + 6).toFixed(1) + '" font-size="16" font-weight="500" text-anchor="middle" fill="#16587b" font-family="inherit">' + flex + '\u00b0</text>';
  }
  var ILLUS = "<svg class=\"illus\" role=\"img\" aria-label=\"Seated person with the right leg extended straight in front\" viewBox=\"0 0 300 190\" fill=\"none\" xmlns=\"http://www.w3.org/2000/svg\"><g clip-path=\"url(#legClip)\"><path d=\"M72 68C72 66 73 65 75 65H81C83 65 84 66 84 68V119H76L72 68Z\" fill=\"#E6EBEF\" stroke-width=\"1.2\" stroke-linejoin=\"round\" stroke=\"#354D65\"/><path d=\"M78 125H84L81 177H75L78 125Z\" fill=\"#E6EBEF\" stroke-width=\"1.2\" stroke-linejoin=\"round\" stroke=\"#354D65\"/><path d=\"M117 125H123L129 177H123L117 125Z\" fill=\"#E6EBEF\" stroke-width=\"1.2\" stroke-linejoin=\"round\" stroke=\"#354D65\"/><path d=\"M74 119H128C130 119 131 120 131 122V126H74V119Z\" fill=\"#D6E0E8\" stroke-width=\"1.2\" stroke-linejoin=\"round\" stroke=\"#354D65\"/><path d=\"M105 108C119 107 135 114 143 123C146.333 126.333 148 131 148 137V165H137L133 139L110 133C102 131 98 126.667 98 120L105 108Z\" fill=\"#E2E7EB\" stroke-width=\"1.2\" stroke-linejoin=\"round\" stroke=\"#354D65\"/><path d=\"M137 164H148L150 169L163 173C165.667 173.667 166.667 175.333 166 178H136C134 176.667 133.667 174.333 135 171L137 164Z\" fill=\"#EEF1F3\" stroke-width=\"1.2\" stroke-linejoin=\"round\" stroke=\"#354D65\"/><path d=\"M94 57C99.3333 55 105 56 111 60C114.333 63.3333 115.667 69.3333 115 78L112 101C113.333 104.333 116.333 107 121 109L119 122L96 123C90 121 87 117 87 111L88 78C87.3333 69.3333 89.3333 62.3333 94 57Z\" fill=\"#ACC8DD\" stroke-width=\"1.2\" stroke-linejoin=\"round\" stroke=\"#354D65\"/><path d=\"M91 106C97 108 103.667 108 111 106\" stroke-linecap=\"round\" stroke=\"#779BB7\"/><path d=\"M96 45L108 46V58C106 61.3333 103 62.3333 99 61L94 57L96 45Z\" fill=\"#EEF1F3\" stroke-width=\"1.2\" stroke-linejoin=\"round\" stroke=\"#354D65\"/><path d=\"M88 28C88.6667 20 93.3333 15.6667 102 15C111.333 14.3334 117.333 18.3334 120 27L119 34L124 40C124 41.3334 122.333 42.3334 119 43L118 49C116.667 52.3334 113.667 53.6667 109 53L100 50C94 47.3334 90.3333 43.3334 89 38L88 28Z\" fill=\"#EEF1F3\" stroke-width=\"1.2\" stroke-linejoin=\"round\" stroke=\"#354D65\"/><path d=\"M89 38C85.6667 32.6667 85.6667 27.3334 89 22C92.3333 15.3334 97.6667 13 105 15C112.333 15 117 18.3334 119 25C113.667 27 108 26.6667 102 24L98 33H95V42L89 38Z\" fill=\"#CFD9E2\" stroke-width=\"1.2\" stroke-linejoin=\"round\" stroke=\"#354D65\"/><path d=\"M99 36C96.3333 33.3333 95 33.6667 95 37C95 39 96 40 98 40M115 34H117M114 46H118\" stroke-linecap=\"round\" stroke-linejoin=\"round\" stroke=\"#354D65\"/><path d=\"M96 77H108L109 93L120 104L132 107C134.667 107.667 135.667 109.333 135 112H121C118.333 112 116 111 114 109L100 100C98 98.6667 97 96.3333 97 93L96 77Z\" fill=\"#EEF1F3\" stroke-width=\"1.2\" stroke-linejoin=\"round\" stroke=\"#354D65\"/><path d=\"M96 63C100.667 60.3333 104.333 61.3333 107 66L110 81L96 83L93 72C92.3333 68 93.3333 65 96 63Z\" fill=\"#ACC8DD\" stroke-width=\"1.2\" stroke-linejoin=\"round\" stroke=\"#354D65\"/><path d=\"M125 109L132 110\" stroke-width=\"0.9\" stroke-linecap=\"round\" stroke-linejoin=\"round\" stroke=\"#8498A9\"/><path d=\"M104.418 130.25H148.627L166.454 129.25L194.263 127.25L197.829 125.25V116.25L192.837 114.25L166.454 112.25L148.627 111.25H106.557C100.852 111.25 98 114.25 98 120.25C98 125.583 100.139 128.917 104.418 130.25Z\" fill=\"#EEF1F3\" stroke-width=\"1.2\" stroke-linejoin=\"round\" stroke=\"#354D65\"/><path d=\"M194.263 126.25C196.64 128.25 198.542 127.583 199.968 124.25L202.107 116.25L208.525 104.25C209.475 101.583 209 99.5833 207.098 98.25C205.672 97.5833 204.246 98.25 202.82 100.25L195.69 111.25C194.263 113.25 193.788 115.917 194.263 119.25V126.25Z\" fill=\"#EEF1F3\" stroke-width=\"1.2\" stroke-linejoin=\"round\" stroke=\"#354D65\"/><path d=\"M144.349 130.25L160.036 129.25C158.61 123.917 158.61 118.25 160.036 112.25L144.349 111.25C145.3 117.917 145.3 124.25 144.349 130.25Z\" fill=\"#294F76\" stroke-width=\"1.2\" stroke-linejoin=\"round\" stroke=\"#354D65\"/><path d=\"M147.914 129.25C148.865 123.917 148.865 118.25 147.914 112.25M156.471 128.25C155.52 123.583 155.52 118.583 156.471 113.25\" stroke-linejoin=\"round\" stroke=\"#7294B2\"/></g><defs><clipPath id=\"legClip\"><rect width=\"300\" height=\"190\" fill=\"white\"/></clipPath></defs></svg>";

  function legCard(style) {
    return '<div class="guide-card"' + (style ? ' style="' + style + '"' : "") + ">" + ILLUS +
      '<p id="legCap">Straight leg = 0\u00b0 knee flexion</p>' +
      '<div class="watch"><span>Watch the leg move (demo)</span><button class="toggle" role="switch" id="watch" aria-label="Watch the leg move" aria-checked="' + S.watch + '"></button></div></div>';
  }
  function wireLeg(root) {
    var svg = $("svg.illus", root), ps = svg.querySelectorAll("path");
    var leg = ps[15], foot = ps[16], sleeve = ps[17];
    var defs = svg.querySelector("defs");
    ["thighClip:0:152", "shinClip:152:148"].forEach(function (s) {
      var a = s.split(":"), cp = document.createElementNS(NS, "clipPath"), r = document.createElementNS(NS, "rect");
      cp.setAttribute("id", a[0]); r.setAttribute("x", a[1]); r.setAttribute("y", 0); r.setAttribute("width", a[2]); r.setAttribute("height", 190);
      cp.appendChild(r); defs.appendChild(cp);
    });
    var shin = leg.cloneNode(true);
    leg.setAttribute("clip-path", "url(#thighClip)"); shin.setAttribute("clip-path", "url(#shinClip)");
    var g = document.createElementNS(NS, "g"); g.appendChild(shin); g.appendChild(foot);
    sleeve.parentNode.insertBefore(g, sleeve);
    var tg = $("#watch", root), cap = $("#legCap", root);
    tg.addEventListener("click", function () {
      S.watch = !S.watch; tg.setAttribute("aria-checked", S.watch ? "true" : "false");
    });
    live.push(function () {
      var a = S.watch || S.f > 0.5 ? clamp(S.f, 0, 100) : 0;
      g.setAttribute("transform", "rotate(" + a.toFixed(2) + " 152 121)");
      cap.textContent = a > 1 ? "Knee flexion: " + Math.round(a) + "\u00b0" : "Straight leg = 0\u00b0 knee flexion";
    });
    cleanups.push(function () { S.watch = false; });
  }

  function infoBtn() { return ""; }

  /* ---------- views ---------- */
  var views = {};

  views.welcome = function (root) {
    var timer;
    function draw(progress) {
      var connected = S.connected;
      root.innerHTML = '<div class="center"><div class="guided">' +
        '<p class="brandmark">uOBionics</p>' +
        '<h2 class="l">Smart Knee Assessment System</h2>' +
        '<p class="lede">Connect your sleeve. Calibrate your knee. Understand your movement.</p>' +
        '<div class="pairing' + (connected ? "" : " busy") + '">' + icon("bluetooth") + "</div>" +
        '<div class="stack">' +
        (connected
          ? '<h2 class="l">Device connected</h2><p class="lede">Your knee sleeve is paired and ready to calibrate.</p>' + badge("Ready to calibrate")
          : '<h2 class="l">Connecting…</h2><div class="searching">' + icon("loader-circle", 20, "spin") + "<span>Searching for your knee sleeve…</span></div>" +
            '<div class="connecting-bar" role="progressbar" aria-valuemin="0" aria-valuemax="100" aria-valuenow="' + Math.round(progress || 0) + '"><i style="width:' + (progress || 0) + '%"></i></div>') +
        "</div>" +
        '<div class="actions"><button class="btn btn-primary" id="start"' + (connected ? "" : " disabled") + ">Get Started</button>" +
        '<p class="hint">' + (connected ? "First, we’ll set your straight-leg reference." : "Start is unavailable until the sleeve connects.") + "</p></div>" +
        "</div></div>";
      var b = $("#start");
      if (b) b.addEventListener("click", function () { go(S.calibrated ? "mode" : "straighten"); });
      if (connected && b && document.activeElement === document.body) b.focus({ preventScroll: true });
    }
    if (S.connected) { draw(); return; }
    S.connecting = true; renderShell();
    var p = 0; draw(0);
    timer = setInterval(function () {
      p += 100 / 12;               // ~2.4 s total
      if (p >= 100) {
        clearInterval(timer); S.connected = true; S.connecting = false; renderShell();
        if (current === "welcome") draw();
        return;
      }
      var bar = $(".connecting-bar i"); if (bar) { bar.style.width = p + "%"; bar.parentNode.setAttribute("aria-valuenow", Math.round(p)); }
    }, 200);
    cleanups.push(function () { clearInterval(timer); if (!S.connected) { S.connecting = false; } });
  };

  function progressHTML(step) {
    var names = ["Straighten", "Set zero", "Ready"];
    return '<div class="progress"><div class="row"><span>Setup &amp; Calibration</span><b>Step ' + step + " of 3</b></div>" +
      '<div class="track">' + names.map(function (n, i) { return '<i class="' + (i < step ? "on" : "") + '"></i>'; }).join("") + "</div>" +
      '<div class="names">' + names.map(function (n, i) { return '<span class="' + (i === step - 1 ? "on" : "") + '">' + n + "</span>"; }).join("") + "</div></div>";
  }

  views.straighten = function (root) {
    root.innerHTML = '<div class="center"><div class="guided">' + progressHTML(1) +
      '<h2 class="xl">Step 1: Straighten Your Leg</h2>' +
      '<p class="lede">Sit down and extend your leg fully straight out in front of you. Keep your knee as flat as possible.</p>' +
      legCard() +
      '<div class="read-card"><div class="m"><span>Raw sensor angle · live</span><b id="rawLive">180.0°</b></div>' +
      "<span>This is the sensor reference, not knee flexion. The next step saves this position as 0°.</span></div>" +
      '<div class="actions"><button class="btn btn-primary" id="next">My Leg is Straight</button>' +
      '<p class="hint">Stay seated and keep the sleeve comfortably in place.</p></div></div></div>';
    wireLeg(root);
    live.push(function () { $("#rawLive").textContent = S.raw.toFixed(1) + "°"; });
    $("#next").addEventListener("click", function () { go("zero"); });
  };

  views.zero = function (root) {
    root.innerHTML = '<div class="center"><div class="guided">' + progressHTML(2) +
      '<h2 class="xl">Step 2: Set Your Zero Point</h2>' +
      '<p class="lede">Hold completely still in the straightened position. We’ll use this reading as your straight-leg reference.</p>' +
      legCard() +
      '<div class="bigread"><span>Current raw sensor angle</span><b id="rawBig">180.0°</b><span>Raw reference ≈ 180° · This will become 0° knee flexion.</span></div>' +
      '<div id="zoneMsg" style="width:100%"><div class="warn">' + icon("triangle-alert") + "<div><b>Don’t move during calibration</b><span>Keep your leg straight until calibration is complete.</span></div></div></div>" +
      '<div class="actions"><button class="btn btn-primary" id="cal">Calibrate Now</button>' +
      '<button class="btn btn-secondary" id="back">Back</button></div></div></div>';
    var busy = false;
    wireLeg(root);
    live.push(function () { $("#rawBig").textContent = S.raw.toFixed(1) + "°"; });
    $("#back").addEventListener("click", function () { if (!busy) go("straighten"); });
    $("#cal").addEventListener("click", function () {
      if (busy) return; busy = true;
      S.watch = false; var wt = $("#watch"); wt.setAttribute("aria-checked", "false"); wt.disabled = true;
      var b = $("#cal"); b.disabled = true; b.textContent = "Calibrating…";
      $("#back").disabled = true;
      $("#zoneMsg").innerHTML = '<div class="calib-busy" role="status">' + icon("loader-circle", 24, "spin") + "<span>Saving your straight-leg reference…</span></div>";
      var t = setTimeout(function () {
        S.reference = S.raw; S.calibrated = true;
        S.reps = 0; S.flexEv = 0; S.extEv = 0; S.stage = "down"; S.minF = S.maxF = null; S.phase = 0;
        S.sessionStart = performance.now();
        go("complete");
      }, 2000);
      cleanups.push(function () { clearTimeout(t); });
    });
  };

  views.complete = function (root) {
    root.innerHTML = '<div class="center"><div class="guided">' + progressHTML(3) +
      '<div class="success-orb">' + icon("circle-check") + "</div>" +
      '<h2 class="xl">All Set!</h2>' +
      '<p class="lede">Your sleeve is calibrated and ready. Your straight-leg position is now 0° knee flexion.</p>' +
      '<div class="success-box" role="status"><b>Calibration complete</b><span>Straight-leg reference saved successfully.</span></div>' +
      '<div class="read-card"><div class="m"><span>Reference angle stored</span><b>' + S.reference.toFixed(1) + "°</b></div>" +
      "<span>You can recalibrate anytime if your readings feel off or you adjust the sleeve.</span></div>" +
      '<div class="actions"><button class="btn btn-primary" id="mode">Choose Your Mode</button>' +
      '<button class="btn btn-secondary" id="recal">Recalibrate</button></div></div></div>';
    $("#mode").addEventListener("click", function () { go("mode"); });
    $("#recal").addEventListener("click", function () { go("straighten"); });
  };

  views.mode = function (root) {
    var adv = S.advancedPref;
    function opt(o) {
      return '<div class="mode' + (o.pick ? " pick" : "") + '"><div class="ico">' + icon(o.icon) + "</div>" +
        "<div><h3>" + o.title + "</h3><p>" + o.desc + "</p></div><ul>" +
        o.feats.map(function (f) { return "<li>" + icon("check") + f + "</li>"; }).join("") + "</ul>" +
        '<button class="btn ' + (o.pick ? "btn-primary" : "btn-secondary") + '" data-go="' + o.go + '">' + o.cta + "</button></div>";
    }
    root.innerHTML = '<div class="center"><div class="guided wide">' +
      '<div class="stack" style="gap:12px"><p class="eyebrow">Calibrated and ready</p><h2 class="xl">How do you want to use it?</h2>' +
      '<p class="lede">Choose the view that works for you. You can switch anytime.</p></div>' +
      '<div class="modes">' +
      opt({ icon: "activity", title: "Simple Mode", desc: "An easy home exercise guide to follow your movement at your own pace.", feats: ["Guided Exercise", "Rep Counter", "Beginner Friendly"], cta: "Choose Simple Mode", go: "exercise", pick: !adv }) +
      opt({ icon: "chart-no-axes-combined", title: "Advanced Mode", desc: "A detailed data dashboard for clinicians and researchers.", feats: ["Live Graph", "IMU Sensors", "CSV Export"], cta: "Choose Advanced Mode", go: "advanced", pick: adv }) +
      '</div><p class="hint">Movement insights for demonstration, not medical advice or a clinically validated assessment.</p></div></div>';
    $$("[data-go]", root).forEach(function (b) {
      b.addEventListener("click", function () {
        S.advancedPref = b.getAttribute("data-go") === "advanced";
        try { localStorage.setItem("uob.advanced", S.advancedPref ? "1" : "0"); } catch (e) {}
        go(b.getAttribute("data-go"));
      });
    });
  };

  function guidanceFor(flex, done) {
    if (done) return ["Goal reached — great work!", "You completed all " + S.goal + " cycles. Reset reps to go again."];
    if (flex < 15) return ["Ready — bend slowly", "Gently bend your knee and lift your foot."];
    if (flex < 60) return ["Nice start — keep going", "Keep the movement smooth and comfortable."];
    if (flex < 100) return ["Good bend — move slowly", "Keep the movement smooth and comfortable."];
    return ["Great bend — hold, then lower", "Hold for 2 seconds, then slowly straighten."];
  }

  views.exercise = function (root) {
    var segs = ""; for (var i = 0; i < S.goal; i++) segs += "<i></i>";
    root.innerHTML = '<div class="dash"><div class="dash-head col"><h2>Knee Exercise</h2><p>A little movement, at your own pace.</p>' +
      '<div class="switch"><button class="btn btn-secondary" id="toAdv">Switch to Advanced</button></div></div>' +
      '<div class="ex-grid">' +
      '<div class="feedback"><div class="panel-angle">' + gaugeCard("g1") +
      '<div class="bend-range"><div class="l"><span>Straight</span><span>Fully bent</span></div>' +
      '<div class="track"><div class="bg"></div><div class="fill" id="rFill"></div><div class="dot" id="rDot"></div></div>' +
      '<div class="e"><span>0°</span><span class="mid" id="rMid">0°</span><span class="r">135°</span></div></div></div>' +
      '<div class="guidance" role="status">' + icon("circle-check") + '<div><b id="gTitle"></b><span id="gText"></span></div></div></div>' +
      '<div class="card reps"><h3>Reps completed</h3><div class="reps-total"><b id="repN">0</b><span>/ ' + S.goal + '</span></div>' +
      '<div class="segs" id="segs">' + segs + "</div>" +
      '<p class="note">One rep is one full bend-and-straighten cycle, not each movement.</p><p class="note" id="repLeft"></p></div>' +
      '<div class="actions-col"><div class="row2" style="display:flex;flex-direction:column;gap:12px"><button class="btn btn-secondary" id="reset">Reset Reps</button><button class="btn btn-secondary" id="recal">Recalibrate</button></div>' +
      '<p class="note" id="sess">Session time 00:00 · Seated knee movement</p></div>' +
      '<div class="card guide"><h3>Seated knee exercise</h3><div class="steps">' +
      [["Get comfortable", "Sit upright with your feet flat on the floor and your sleeve comfortable."],
       ["Lift and hold", "Slowly lift and extend your leg as far as comfortable. Hold for 2 seconds."],
       ["Lower and repeat", "Slowly lower your foot back to the floor. Rest, then repeat."]].map(function (s, i) {
        return '<div class="step"><span class="n">' + (i + 1) + "</span><div><b>" + s[0] + "</b><span>" + s[1] + "</span></div></div>";
      }).join("") + '</div><p class="note">Move within your comfort. Stop if it hurts. This guide is not a medical prescription.</p></div></div></div>';
    var segEls = $$("#segs i", root), lastShown = -1;
    live.push(function (d) {
      updGauge("g1", d.flex);
      var pct = clamp(d.flex / GAUGE_MAX, 0, 1) * 100;
      $("#rFill").style.width = pct + "%"; $("#rDot").style.left = pct + "%";
      var mid = $("#rMid"); mid.textContent = d.flex + "°"; mid.style.left = pct + "%";
      var shown = Math.min(S.reps, S.goal), done = S.reps >= S.goal;
      if (shown !== lastShown) {
        lastShown = shown;
        $("#repN").textContent = shown;
        segEls.forEach(function (s, i) { s.classList.toggle("on", i < shown); });
        $("#repLeft").textContent = done ? "Demo goal reached." : (S.goal - shown) + " more cycle" + (S.goal - shown === 1 ? "" : "s") + " to your demo goal.";
      }
      var g = guidanceFor(d.flex, done);
      if ($("#gTitle").textContent !== g[0]) { $("#gTitle").textContent = g[0]; $("#gText").textContent = g[1]; }
      $("#sess").textContent = "Session time " + fmtTime(sessionSeconds()) + " · Seated knee movement";
    });
    $("#toAdv").addEventListener("click", function () { go("advanced"); });
    $("#reset").addEventListener("click", function () { S.reps = 0; S.flexEv = 0; S.extEv = 0; S.stage = "down"; lastShown = -1; toast("Reps reset"); });
    $("#recal").addEventListener("click", function () { go("straighten"); });
  };

  views.advanced = function (root) {
    root.innerHTML = '<div class="dash"><div class="adv">' +
      '<div class="dash-head"><div class="intro"><h2>Advanced monitor</h2><p>Live movement, sensor measurements and session insights.</p></div>' +
      '<div class="switch"><button class="btn btn-secondary" id="toSimple">Switch to Simple</button></div></div>' +
      '<div class="adv-bar" id="bar"></div>' +
      '<div class="live-grid">' +
      '<div class="live-col">' + gaugeCard("g2") +
      '<div class="rom-strip"><div>Min<b id="mMin">—</b></div><div>Max<b id="mMax">—</b></div><div>ROM<b id="mRom">—</b></div></div>' +
      '<p class="note">ROM is the maximum minus the minimum flexion in this session.</p></div>' +
      '<div class="live-col"><div class="card tight"><div class="hd"><b>Live leg tracking</b><span class="acc">Tracking</span></div><div class="leg">' + legSvg() + "</div></div>" +
      '<div class="tiles"><div class="tile">Raw angle<b id="tRaw">180°</b></div><div class="tile">Velocity<b id="tVel">0.0 °/s</b></div>' +
      '<div class="tile">Reference<b id="tRef">180°</b></div><div class="tile">Session time<b id="tSess">00:00</b></div></div></div>' +
      '<div class="live-col col-detect"><div class="card tight pad20"><div class="detect"><div class="row"><h3 class="ct" style="font-weight:500">Exercise detection</h3>' + badge("Detected") + "</div>" +
      '<div class="row"><div><h4>Seated knee exercise</h4><small>Bend + straighten</small></div><div class="count"><b id="dN">0</b>full cycles</div></div></div></div>' +
      '<div class="card"><h3>Movement events</h3><div class="meas"><span>Extension events</span><b id="eE">0</b></div><div class="meas"><span>Flexion events</span><b id="eF">0</b></div>' +
      '<p class="note" id="eNote">Events mark each direction. A full repetition includes both, so 0 events = 0 full cycles.</p></div></div>' +
      "</div>" +
      '<div class="hist-grid"><div style="display:flex;flex-direction:column;gap:16px;min-width:0">' +
      '<div class="card tight"><div class="hd"><b>Angle history</b><span class="acc">Flexion (°)</span></div>' +
      '<div class="plot"><div class="gl" style="top:20px"></div><div class="gl" style="top:100px"></div><div class="gl" style="top:180px"></div>' +
      '<span class="gt" style="top:20px">120</span><span class="gt" style="top:100px">60</span><span class="gt" style="top:180px">0</span>' +
      '<div class="area"><svg viewBox="0 0 100 196" preserveAspectRatio="none" aria-hidden="true"><path id="hPath" d="" fill="none" stroke="#16587b" stroke-width="2.5" stroke-linejoin="round" stroke-linecap="round" vector-effect="non-scaling-stroke"/></svg></div>' +
      '<span class="last" id="hDot" style="top:180px"></span>' +
      '<span class="tk" style="left:32px">−5 s</span><span class="tk" style="left:50%;transform:translateX(-50%);margin-left:12px">−2.5 s</span><span class="tk" style="right:8px">Now</span></div>' +
      '<div class="gsum"><span id="hSum">Last 5 seconds · Current 0°</span><span>Time · s</span></div></div>' +
      '<div class="help">' + icon("info") + '<div><b>How is flexion calculated?</b><span id="calcNote">180° reference − 180° raw angle = 0° flexion. Straight is 0°. Raw angle and knee flexion are different measurements.</span></div></div></div>' +
      '<div><div class="imu-head"><b>IMU sensor readings</b><span>Acceleration · g</span></div><div class="imu-pair">' +
      ["Thigh", "Shin"].map(function (n, i) {
        return '<div class="imu"><b>' + n + ' IMU</b><em>● Connected</em>' + ["X", "Y", "Z"].map(function (a, j) { return '<div class="ax"><span>' + a + '</span><b id="i' + i + j + '">+0.00</b></div>'; }).join("") + "</div>";
      }).join("") + '</div><p class="note imu-note">X, Y and Z show acceleration in g, including gravity, for each sleeve sensor.</p></div></div>' +
      "</div></div>";

    function bar() {
      var el = $("#bar"), rec = S.recording;
      var stateText, desc;
      if (rec) { stateText = '<span class="badge rec" id="recBadge">Recording · ' + fmtTime(S.recSeconds) + "</span>"; desc = "CSV export becomes available after stopping."; }
      else if (S.hasRecording) { stateText = "<b>Stopped · export ready</b>"; desc = "Recording saved · " + fmtTime(S.recSeconds) + " · Demo session"; }
      else { stateText = '<b id="stState">Ready to record · Session ' + fmtTime(sessionSeconds()) + "</b>"; desc = "Record a session, then stop to export CSV."; }
      el.innerHTML = '<div class="rec">' +
        (rec ? '<button class="btn btn-danger btn-lg" id="stop">Stop recording</button>' : '<button class="btn btn-primary btn-lg" id="record">' + (S.hasRecording ? "Record again" : "Record") + "</button>") + "</div>" +
        '<div class="status">' + stateText + "<span>" + desc + "</span></div>" +
        '<div class="recal">' + (S.hasRecording && !rec ? '<button class="btn btn-secondary" id="export">Export CSV</button>' : "") +
        '<button class="btn btn-secondary" id="recal"' + (rec ? " disabled" : "") + ">Recalibrate</button></div>";
      var r = $("#record"), s = $("#stop"), e = $("#export"), c = $("#recal");
      if (r) r.addEventListener("click", function () {
        S.recording = true; S.hasRecording = false; S.recSamples = []; S.recStart = performance.now(); S.recSeconds = 0; recAcc = 0; bar(); var b = $("#stop"); if (b) b.focus();
      });
      if (s) s.addEventListener("click", function () {
        S.recSeconds = (performance.now() - S.recStart) / 1000; S.recording = false; S.hasRecording = true; bar(); var b = $("#export"); if (b) b.focus();
        toast("Recording stopped · export ready");
      });
      if (e) e.addEventListener("click", exportCsv);
      if (c) c.addEventListener("click", function () { go("straighten"); });
    }
    bar();
    $("#toSimple").addEventListener("click", function () { go("exercise"); });

    live.push(function (d) {
      updGauge("g2", d.flex); updLeg(d.flex);
      $("#mMin").textContent = S.minF === null ? "—" : S.minF + "°";
      $("#mMax").textContent = S.maxF === null ? "—" : S.maxF + "°";
      $("#mRom").textContent = S.minF === null ? "—" : (S.maxF - S.minF) + "°";
      $("#tRaw").textContent = d.rawR + "°"; $("#tVel").textContent = S.vel.toFixed(1) + " °/s";
      $("#tRef").textContent = d.refR + "°"; $("#tSess").textContent = fmtTime(sessionSeconds());
      var cycles = Math.min(S.flexEv, S.extEv);
      $("#dN").textContent = cycles; $("#eE").textContent = S.extEv; $("#eF").textContent = S.flexEv;
      $("#eNote").textContent = "Events mark each direction. A full repetition includes both, so " + (S.flexEv + S.extEv) + " events = " + cycles + " full cycle" + (cycles === 1 ? "" : "s") + ".";
      $("#calcNote").textContent = d.refR + "° reference − " + d.rawR + "° raw angle = " + d.flex + "° flexion. Straight is 0°. Raw angle and knee flexion are different measurements.";
      // history plot
      var n = S.hist.length, pts = [];
      for (var i = 0; i < n; i++) {
        var x = n < 100 ? 100 - (n - i) * (100 / 99) : i * (100 / 99);
        pts.push((x < 0 ? 0 : x).toFixed(2) + " " + (180 - clamp(S.hist[i], 0, 135) * (160 / 120)).toFixed(1));
      }
      $("#hPath").setAttribute("d", n ? "M" + pts.join(" L") : "");
      $("#hDot").style.top = (180 - clamp(S.f, 0, 135) * (160 / 120)) + "px";
      $("#hSum").textContent = "Last 5 seconds · Current " + d.flex + "°";
      var m = imu(d), a = ["X", "Y", "Z"];
      for (var j = 0; j < 3; j++) { $("#i0" + j).textContent = sgn(m.t[j]); $("#i1" + j).textContent = sgn(m.s[j]); }
      // recording status
      var rb = $("#recBadge"); if (rb) rb.textContent = "Recording · " + fmtTime(S.recSeconds);
      var st = $("#stState"); if (st) st.textContent = "Ready to record · Session " + fmtTime(sessionSeconds());
    });
  };

  views.settings = function (root) {
    var on = S.advancedPref;
    root.innerHTML = '<div class="dash"><div class="settings"><div><h2>Settings</h2><p class="note" style="margin-top:8px">Choose how your exercise experience behaves.</p></div>' +
      '<div class="card"><h3>Exercise mode</h3><div class="toggle-row"><div><b>Advanced mode</b><span class="note">Show more detailed tracking, sensor panels and movement events.</span></div>' +
      '<button class="toggle" role="switch" id="tg" aria-label="Advanced mode" aria-checked="' + on + '"></button></div>' +
      '<div class="help">' + icon("info") + '<div><b id="tgT"></b><span id="tgS"></span></div></div>' +
      '<button class="btn btn-secondary" id="save">Save changes</button></div></div></div>';
    function note() {
      var v = $("#tg").getAttribute("aria-checked") === "true";
      $("#tgT").textContent = v ? "Advanced mode is on" : "Advanced mode is off by default";
      $("#tgS").textContent = v ? "You’ll see the detailed monitor first. Turn it off for a simpler everyday view." : "Keep this screen simple for everyday exercise. Turn it on only when you want more detail.";
    }
    note();
    $("#tg").addEventListener("click", function () { var t = $("#tg"); t.setAttribute("aria-checked", t.getAttribute("aria-checked") === "true" ? "false" : "true"); note(); });
    $("#save").addEventListener("click", function () {
      S.advancedPref = $("#tg").getAttribute("aria-checked") === "true";
      try { localStorage.setItem("uob.advanced", S.advancedPref ? "1" : "0"); } catch (e) {}
      toast("Settings saved");
    });
  };

  /* ---------- router ---------- */
  function render() {
    cleanups.forEach(function (c) { c(); }); cleanups = []; live = [];
    var name = currentRoute();
    // Free navigation: opening a screen directly treats the sleeve as connected.
    if (name !== "welcome" && !S.connected) { S.connected = true; S.connecting = false; }
    if (!S.sessionStart) S.sessionStart = performance.now();
    current = name;
    setNav(false);
    renderShell();
    var root = $("#view"); root.innerHTML = "";
    views[name](root);
    window.scrollTo(0, 0);
    root.focus({ preventScroll: true });
    tick();
  }

  /* ---------- boot ---------- */
  $("#menuBtn").addEventListener("click", function () { setNav(!document.body.classList.contains("nav-open")); });
  $("#backdrop").addEventListener("click", function () { setNav(false); });
  document.addEventListener("keydown", function (e) { if (e.key === "Escape") setNav(false); });
  var nav = $("#nav");
  nav.addEventListener("click", function () { setNav(false); });
  window.addEventListener("hashchange", render);
  window.addEventListener("resize", function () { if (window.innerWidth >= 1024) setNav(false); });
  if (!location.hash) location.replace("#/welcome");
  render();
  setInterval(tick, 50);
})();
