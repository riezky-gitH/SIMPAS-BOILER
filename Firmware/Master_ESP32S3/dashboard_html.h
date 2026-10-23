#ifndef DASHBOARD_HTML_H
#define DASHBOARD_HTML_H

#include <pgmspace.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="id" data-theme="dark">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>SCADA Boiler PLTU - Interactive React-Style Console</title>
  <style>
    /* ==========================================================================
       DESIGN SYSTEM (TAILWIND / REACT UI INSPIRED)
       ========================================================================== */
    :root {
      --bg: #090d16;
      --bg-surface: #0f172a;
      --bg-card: #1e293b;
      --bg-card-sub: #172033;
      --bg-card-hover: #334155;
      --border: #334155;
      --border-focus: #38bdf8;
      
      --text-main: #f8fafc;
      --text-muted: #94a3b8;
      --text-dim: #64748b;
      
      --primary: #991b1b;
      --primary-hover: #b91c1c;
      --primary-glow: rgba(185, 28, 28, 0.4);
      --cyan: #06b6d4;
      --cyan-glow: rgba(6, 182, 212, 0.25);
      --emerald: #10b981;
      --emerald-glow: rgba(16, 185, 129, 0.25);
      --amber: #f59e0b;
      --amber-glow: rgba(245, 158, 11, 0.25);
      --crimson: #ef4444;
      --crimson-glow: rgba(239, 68, 68, 0.35);
      --violet: #8b5cf6;
      
      --radius-xl: 18px;
      --radius-lg: 12px;
      --radius-md: 8px;
      --radius-sm: 6px;
      --shadow-sm: 0 2px 8px rgba(0,0,0,0.3);
      --shadow-card: 0 10px 25px -5px rgba(0,0,0,0.5), 0 0 0 1px var(--border);
    }

    [data-theme="light"] {
      --bg: #f8fafc;
      --bg-surface: #ffffff;
      --bg-card: #ffffff;
      --bg-card-sub: #f1f5f9;
      --bg-card-hover: #e2e8f0;
      --border: #e2e8f0;
      --border-focus: #0284c7;
      
      --text-main: #0f172a;
      --text-muted: #475569;
      --text-dim: #94a3b8;
      
      --primary: #800000;
      --primary-hover: #990000;
      --primary-glow: rgba(128, 0, 0, 0.2);
      --cyan: #0284c7;
      --emerald: #16a34a;
      --amber: #d97706;
      --crimson: #dc2626;
      --violet: #7c3aed;
      
      --shadow-sm: 0 1px 3px rgba(0,0,0,0.06);
      --shadow-card: 0 4px 20px -2px rgba(0,0,0,0.08), 0 0 0 1px var(--border);
    }

    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif; }
    body {
      background-color: var(--bg);
      color: var(--text-main);
      padding: 1rem 1.25rem 3rem;
      min-height: 100vh;
      line-height: 1.5;
      transition: background-color 0.25s, color 0.25s;
    }
    .mono { font-family: "SF Mono", "Cascadia Code", Consolas, Menlo, Monaco, monospace; }

    /* Container */
    .app-container { max-width: 1440px; margin: 0 auto; display: flex; flex-direction: column; gap: 1.25rem; }

    /* ==========================================================================
       HEADER & NAVIGATION BAR (REACT NAVBAR PATTERN)
       ========================================================================== */
    .navbar {
      background: var(--bg-surface);
      border: 1px solid var(--border);
      border-radius: var(--radius-xl);
      padding: 0.85rem 1.5rem;
      display: flex;
      justify-content: space-between;
      align-items: center;
      box-shadow: var(--shadow-card);
      flex-wrap: wrap;
      gap: 1rem;
    }
    .nav-brand { display: flex; align-items: center; gap: 0.85rem; }
    .brand-icon {
      width: 44px; height: 44px;
      background: linear-gradient(135deg, #800000, #dc2626);
      border-radius: var(--radius-lg);
      display: flex; align-items: center; justify-content: center;
      color: white; font-weight: 800; font-size: 1.2rem;
      box-shadow: 0 4px 14px var(--primary-glow);
    }
    .brand-title { font-size: 1.15rem; font-weight: 800; letter-spacing: -0.3px; color: var(--text-main); }
    .brand-sub { font-size: 0.75rem; color: var(--text-muted); font-weight: 500; }

    /* Tabs (React-Like Component) */
    .nav-tabs {
      display: flex;
      background: var(--bg-card-sub);
      border: 1px solid var(--border);
      padding: 0.25rem;
      border-radius: var(--radius-lg);
      gap: 0.25rem;
    }
    .tab-item {
      padding: 0.45rem 1rem;
      border-radius: var(--radius-md);
      font-size: 0.75rem;
      font-weight: 700;
      color: var(--text-muted);
      cursor: pointer;
      display: inline-flex; align-items: center; gap: 0.45rem;
      border: none; background: transparent;
      transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
      user-select: none;
    }
    .tab-item:hover { color: var(--text-main); background: rgba(255,255,255,0.05); }
    .tab-item.active {
      color: white;
      background: var(--primary);
      box-shadow: 0 2px 8px var(--primary-glow);
    }
    [data-theme="light"] .tab-item.active { color: white; background: var(--primary); }

    .nav-tools { display: flex; align-items: center; gap: 0.65rem; }
    .status-badge {
      display: inline-flex; align-items: center; gap: 0.45rem;
      padding: 0.35rem 0.75rem; border-radius: 9999px;
      font-size: 0.75rem; font-weight: 700;
      background: var(--bg-card-sub); border: 1px solid var(--border);
      color: var(--text-main);
    }
    .pulse-dot {
      width: 8px; height: 8px; border-radius: 50%;
      background: var(--emerald); box-shadow: 0 0 8px var(--emerald);
      animation: pulse-ring 1.8s infinite;
    }
    .pulse-dot.offline { background: var(--crimson); box-shadow: 0 0 8px var(--crimson); }
    @keyframes pulse-ring { 0%, 100% { opacity: 1; transform: scale(1); } 50% { opacity: 0.4; transform: scale(0.85); } }

    .btn-icon {
      background: var(--bg-card-sub); border: 1px solid var(--border);
      color: var(--text-main); border-radius: var(--radius-md);
      padding: 0.4rem 0.75rem; font-size: 0.75rem; font-weight: 600;
      cursor: pointer; display: inline-flex; align-items: center; gap: 0.4rem;
      transition: all 0.2s;
    }
    .btn-icon:hover { background: var(--bg-card-hover); border-color: var(--border-focus); transform: translateY(-1px); }

    /* ==========================================================================
       HERO KPI RIBBON (QUICK TELEMETRY CHIPS)
       ========================================================================== */
    .kpi-row {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
      gap: 0.85rem;
    }
    .kpi-chip {
      background: var(--bg-surface);
      border: 1px solid var(--border);
      border-radius: var(--radius-lg);
      padding: 0.85rem 1.15rem;
      box-shadow: var(--shadow-sm);
      display: flex; justify-content: space-between; align-items: center;
      position: relative; overflow: hidden;
      transition: transform 0.2s, box-shadow 0.2s;
    }
    .kpi-chip:hover { transform: translateY(-2px); box-shadow: var(--shadow-card); }
    .kpi-chip::after {
      content: ""; position: absolute; left: 0; top: 0; bottom: 0; width: 4px;
      background: var(--primary);
    }
    .kpi-chip.kpi-cyan::after { background: var(--cyan); }
    .kpi-chip.kpi-emerald::after { background: var(--emerald); }
    .kpi-chip.kpi-amber::after { background: var(--amber); }

    .kpi-title { font-size: 0.7rem; font-weight: 700; text-transform: uppercase; color: var(--text-muted); letter-spacing: 0.5px; }
    .kpi-value { font-size: 1.25rem; font-weight: 800; color: var(--text-main); margin-top: 0.15rem; }
    .kpi-pill { font-size: 0.65rem; padding: 0.2rem 0.5rem; border-radius: 4px; font-weight: 700; }

    /* ==========================================================================
       TAB 1: LIVE MONITORING (DUAL CHAMBER CONSOLE)
       ========================================================================== */
    .chamber-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(480px, 1fr));
      gap: 1.25rem;
    }
    @media (max-width: 640px) { .chamber-grid { grid-template-columns: 1fr; } }

    .chamber-card {
      background: var(--bg-surface);
      border: 1px solid var(--border);
      border-radius: var(--radius-xl);
      padding: 1.35rem;
      box-shadow: var(--shadow-card);
      position: relative;
      overflow: hidden;
      display: flex; flex-direction: column; gap: 1.25rem;
      transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
    }
    .chamber-card.is-alarm {
      border-color: var(--crimson);
      box-shadow: 0 0 25px rgba(239, 68, 68, 0.35);
    }
    .chamber-card.is-alarm::before {
      content: ""; position: absolute; top: 0; left: 0; right: 0; height: 4px;
      background: var(--crimson);
      animation: pulse-ring 1s infinite;
    }

    .card-header {
      display: flex; justify-content: space-between; align-items: center;
      border-bottom: 1px solid var(--border);
      padding-bottom: 0.85rem;
    }
    .card-title-group { display: flex; align-items: center; gap: 0.75rem; }
    .unit-tag {
      width: 32px; height: 32px; border-radius: var(--radius-md);
      background: var(--bg-card-sub); border: 1px solid var(--border);
      display: flex; align-items: center; justify-content: center;
      font-size: 0.85rem; font-weight: 800; color: var(--cyan);
    }
    .card-title-text { font-size: 1.05rem; font-weight: 800; letter-spacing: -0.2px; }
    .card-subtitle-text { font-size: 0.7rem; color: var(--text-dim); }

    .state-pill {
      padding: 0.3rem 0.85rem; border-radius: var(--radius-sm);
      font-size: 0.75rem; font-weight: 800; text-transform: uppercase;
      letter-spacing: 0.5px;
    }
    .state-normal  { background: rgba(16, 185, 129, 0.15); color: var(--emerald); border: 1px solid rgba(16, 185, 129, 0.3); }
    .state-warning { background: rgba(245, 158, 11, 0.15); color: var(--amber); border: 1px solid rgba(245, 158, 11, 0.3); }
    .state-alarm   { background: rgba(239, 68, 68, 0.2); color: var(--crimson); border: 1px solid rgba(239, 68, 68, 0.4); animation: pulse-scale 1s infinite; }
    .state-offline { background: rgba(148, 163, 184, 0.15); color: var(--text-muted); border: 1px solid rgba(148, 163, 184, 0.3); }
    @keyframes pulse-scale { 0%, 100% { transform: scale(1); } 50% { transform: scale(1.05); } }

    /* Interactive Gauge Section */
    .telemetry-row {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 1rem;
    }
    .gauge-box {
      background: var(--bg-card-sub);
      border: 1px solid var(--border);
      border-radius: var(--radius-lg);
      padding: 1rem;
      display: flex; flex-direction: column; align-items: center;
      position: relative;
    }
    .gauge-top { width: 100%; display: flex; justify-content: space-between; align-items: center; margin-bottom: 0.5rem; }
    .gauge-label { font-size: 0.7rem; font-weight: 700; text-transform: uppercase; color: var(--text-muted); }
    .source-badge {
      font-size: 0.6rem; font-weight: 700; padding: 0.15rem 0.45rem; border-radius: 4px;
      background: rgba(6, 182, 212, 0.1); color: var(--cyan); border: 1px solid rgba(6, 182, 212, 0.25);
    }
    .source-badge.sim { background: rgba(168, 85, 247, 0.1); color: #c084fc; border-color: rgba(168, 85, 247, 0.25); }

    /* Circular SVG Gauge */
    .radial-gauge { width: 150px; height: 110px; position: relative; }
    .radial-gauge svg { width: 100%; height: 100%; overflow: visible; }
    .gauge-bg { fill: none; stroke: var(--border); stroke-width: 10; stroke-linecap: round; }
    .gauge-fill {
      fill: none; stroke: var(--cyan); stroke-width: 10; stroke-linecap: round;
      stroke-dasharray: 235.6; stroke-dashoffset: 235.6;
      transition: stroke-dashoffset 0.4s ease, stroke 0.3s ease;
    }
    .gauge-fill.warn { stroke: var(--amber); }
    .gauge-fill.danger { stroke: var(--crimson); filter: drop-shadow(0 0 6px rgba(239, 68, 68, 0.5)); }

    .gauge-center-text {
      position: absolute; bottom: 8px; left: 0; right: 0; text-align: center;
    }
    .gauge-value { font-size: 1.65rem; font-weight: 800; letter-spacing: -1px; }
    .gauge-unit { font-size: 0.75rem; font-weight: 700; color: var(--text-dim); }

    .gauge-footer-text {
      margin-top: 0.4rem; font-size: 0.65rem; color: var(--text-dim);
      width: 100%; display: flex; justify-content: space-between;
    }

    /* Solenoid Valve & Pipe Schematic (Interactive Visual) */
    .pipe-schematic-card {
      background: var(--bg-card-sub);
      border: 1px solid var(--border);
      border-radius: var(--radius-lg);
      padding: 0.85rem 1.15rem;
      display: flex; justify-content: space-between; align-items: center;
      gap: 1rem;
    }
    .valve-visual-wrap { display: flex; align-items: center; gap: 0.85rem; }
    .valve-icon-badge {
      width: 44px; height: 44px; border-radius: var(--radius-md);
      display: flex; align-items: center; justify-content: center;
      transition: all 0.3s;
    }
    .valve-closed {
      background: rgba(16, 185, 129, 0.15); border: 1px solid rgba(16, 185, 129, 0.3);
      color: var(--emerald);
    }
    .valve-open {
      background: rgba(239, 68, 68, 0.2); border: 1px solid rgba(239, 68, 68, 0.4);
      color: var(--crimson);
      animation: pulse-ring 1s infinite;
    }
    .valve-details-title { font-size: 0.7rem; font-weight: 700; text-transform: uppercase; color: var(--text-muted); }
    .valve-details-state { font-size: 0.9rem; font-weight: 800; }

    .buzzer-indicator { display: flex; align-items: center; gap: 0.5rem; font-size: 0.75rem; font-weight: 700; }
    .buzzer-icon {
      width: 32px; height: 32px; border-radius: var(--radius-sm);
      display: flex; align-items: center; justify-content: center;
      background: var(--bg-surface); border: 1px solid var(--border);
      color: var(--text-dim);
    }
    .buzzer-icon.active {
      background: rgba(239, 68, 68, 0.2); border-color: var(--crimson);
      color: var(--crimson); animation: pulse-ring 0.8s infinite;
    }

    /* Buttons (React/Tailwind Style Button Component) */
    .action-button-group { display: flex; gap: 0.5rem; flex-wrap: wrap; }
    .app-btn {
      flex: 1; min-width: 120px; padding: 0.65rem 0.85rem;
      border-radius: var(--radius-md); font-size: 0.75rem; font-weight: 700;
      cursor: pointer; display: inline-flex; align-items: center; justify-content: center; gap: 0.45rem;
      transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
      border: 1px solid transparent; user-select: none;
    }
    .app-btn:active { transform: scale(0.96); }
    .btn-maroon {
      background: var(--primary); color: white;
      box-shadow: 0 2px 8px var(--primary-glow);
    }
    .btn-maroon:hover { background: var(--primary-hover); transform: translateY(-1px); }
    .btn-slate {
      background: var(--bg-card-sub); border-color: var(--border);
      color: var(--text-main);
    }
    .btn-slate:hover { background: var(--bg-card-hover); border-color: var(--border-focus); transform: translateY(-1px); }

    /* ==========================================================================
       TAB 2: REAL-TIME CHART CANVAS (ZERO-CDN 60 FPS)
       ========================================================================== */
    .chart-card {
      background: var(--bg-surface);
      border: 1px solid var(--border);
      border-radius: var(--radius-xl);
      padding: 1.5rem;
      box-shadow: var(--shadow-card);
      display: flex; flex-direction: column; gap: 1rem;
    }
    .chart-header { display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 0.75rem; }
    .chart-legend { display: flex; align-items: center; gap: 1.25rem; font-size: 0.75rem; font-weight: 700; }
    .legend-item { display: flex; align-items: center; gap: 0.4rem; }
    .legend-box { width: 12px; height: 12px; border-radius: 3px; }

    .canvas-container {
      width: 100%; height: 320px; position: relative;
      background: var(--bg-card-sub); border: 1px solid var(--border);
      border-radius: var(--radius-lg); overflow: hidden;
    }
    canvas { width: 100%; height: 100%; display: block; }

    /* ==========================================================================
       TAB 3: ALARM & AUDIT EVENT LOGS
       ========================================================================== */
    .log-card {
      background: var(--bg-surface);
      border: 1px solid var(--border);
      border-radius: var(--radius-xl);
      padding: 1.25rem;
      box-shadow: var(--shadow-card);
      display: flex; flex-direction: column; gap: 0.85rem;
    }
    .log-table-wrap {
      max-height: 420px; overflow-y: auto;
      border: 1px solid var(--border); border-radius: var(--radius-lg);
    }
    .log-table {
      width: 100%; border-collapse: collapse; font-size: 0.75rem; text-align: left;
    }
    .log-table th {
      background: var(--bg-card-sub); padding: 0.75rem 1rem;
      font-weight: 700; color: var(--text-muted); position: sticky; top: 0; z-index: 2;
      border-bottom: 1px solid var(--border);
    }
    .log-table td {
      padding: 0.65rem 1rem; border-bottom: 1px solid var(--border);
    }
    .log-table tr:hover td { background: rgba(255,255,255,0.02); }

    /* ==========================================================================
       TAB 4: SYSTEM DIAGNOSTICS & TELEMETRY DEBUGGER
       ========================================================================== */
    .diag-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
      gap: 1.25rem;
    }
    .diag-card {
      background: var(--bg-surface);
      border: 1px solid var(--border);
      border-radius: var(--radius-xl);
      padding: 1.25rem;
      box-shadow: var(--shadow-card);
      display: flex; flex-direction: column; gap: 0.75rem;
    }
    .diag-item {
      display: flex; justify-content: space-between; align-items: center;
      padding: 0.5rem 0; border-bottom: 1px solid var(--border); font-size: 0.8rem;
    }
    .diag-item:last-child { border-bottom: none; }

    /* ==========================================================================
       EMERGENCY MASTER OVERRIDE FOOTER CONSOLE
       ========================================================================== */
    .emergency-console {
      background: linear-gradient(135deg, rgba(239, 68, 68, 0.08), rgba(185, 28, 28, 0.18));
      border: 1.5px solid rgba(239, 68, 68, 0.4);
      border-radius: var(--radius-xl);
      padding: 1.25rem 1.5rem;
      display: flex; justify-content: space-between; align-items: center;
      gap: 1.25rem; flex-wrap: wrap;
      box-shadow: var(--shadow-card);
    }
    .emergency-text-group { display: flex; align-items: center; gap: 1rem; }
    .emergency-badge-icon {
      width: 48px; height: 48px; border-radius: var(--radius-lg);
      background: rgba(239, 68, 68, 0.2); border: 1px solid var(--crimson);
      display: flex; align-items: center; justify-content: center;
      color: var(--crimson); font-size: 1.4rem;
      animation: pulse-ring 1.5s infinite;
    }
    .emergency-heading { font-size: 1.05rem; font-weight: 800; color: var(--crimson); }
    .emergency-subheading { font-size: 0.75rem; color: var(--text-muted); max-width: 550px; margin-top: 0.15rem; }

    .emergency-btn-group { display: flex; gap: 0.75rem; flex-wrap: wrap; }
    .btn-emergency-action {
      background: linear-gradient(135deg, #b91c1c, #ef4444);
      color: white; font-weight: 800; font-size: 0.85rem;
      padding: 0.8rem 1.5rem; border-radius: var(--radius-md);
      border: none; cursor: pointer;
      box-shadow: 0 4px 15px rgba(239, 68, 68, 0.4);
      display: inline-flex; align-items: center; gap: 0.5rem;
      transition: all 0.2s;
    }
    .btn-emergency-action:hover {
      background: linear-gradient(135deg, #991b1b, #dc2626);
      transform: translateY(-2px); box-shadow: 0 6px 20px rgba(239, 68, 68, 0.6);
    }
    .btn-emergency-action:active { transform: scale(0.96); }

    /* ==========================================================================
       MODAL & TOAST DIALOGS (REACT PORTAL INSPIRED)
       ========================================================================== */
    .modal-overlay {
      position: fixed; inset: 0; background: rgba(0,0,0,0.7);
      backdrop-filter: blur(4px); z-index: 999;
      display: none; align-items: center; justify-content: center;
      padding: 1rem;
    }
    .modal-overlay.open { display: flex; }
    .modal-dialog {
      background: var(--bg-surface); border: 1px solid var(--border);
      border-radius: var(--radius-xl); max-width: 440px; width: 100%;
      padding: 1.5rem; box-shadow: 0 20px 40px rgba(0,0,0,0.6);
      animation: modal-pop 0.25s cubic-bezier(0.175, 0.885, 0.32, 1.275);
    }
    @keyframes modal-pop { from { transform: scale(0.9); opacity: 0; } to { transform: scale(1); opacity: 1; } }

    .toast-container {
      position: fixed; bottom: 20px; right: 20px; z-index: 1000;
      display: flex; flex-direction: column; gap: 0.5rem;
    }
    .toast-card {
      background: var(--bg-surface); border: 1px solid var(--border-focus);
      color: var(--text-main); padding: 0.75rem 1.15rem; border-radius: var(--radius-md);
      font-size: 0.75rem; font-weight: 600; box-shadow: 0 10px 25px rgba(0,0,0,0.5);
      display: flex; align-items: center; gap: 0.65rem;
      animation: toast-slide 0.25s ease-out;
    }
    @keyframes toast-slide { from { transform: translateX(100%); opacity: 0; } to { transform: translateX(0); opacity: 1; } }
  </style>
</head>
<body>
  <div class="app-container">

    <!-- ================= NAVBAR ================= -->
    <header class="navbar">
      <div class="nav-brand">
        <div class="brand-icon">&#9889;</div>
        <div>
          <h1 class="brand-title">PLTU STEAM BOILER SCADA</h1>
          <p class="brand-sub">Enterprise Multi-Chamber Safety Controller &bull; WMLTA</p>
        </div>
      </div>

      <!-- React Style Tab Switcher -->
      <nav class="nav-tabs">
        <button class="tab-item active" onclick="switchTab('tab-monitoring')">
          <span>&#127915;</span> Monitoring
        </button>
        <button class="tab-item" onclick="switchTab('tab-analytics')">
          <span>&#128200;</span> Grafik Realtime
        </button>
        <button class="tab-item" onclick="switchTab('tab-events')">
          <span>&#128203;</span> Log Kejadian
        </button>
        <button class="tab-item" onclick="switchTab('tab-diag')">
          <span>&#9881;</span> Diagnostik
        </button>
      </nav>

      <div class="nav-tools">
        <div class="status-badge mono">
          <span class="pulse-dot" id="liveDot"></span>
          <span id="netStatusText">ONLINE</span>
        </div>
        <button class="btn-icon mono" onclick="toggleTheme()" id="themeBtn" title="Ganti Tema">
          <span id="themeIcon">&#9728;</span> <span id="themeLabel">Light</span>
        </button>
        <button class="btn-icon mono" onclick="toggleAudio()" id="audioBtn" title="Suara Alarm">
          <span id="audioIcon">&#128266;</span> <span id="audioLabel">Audio: ON</span>
        </button>
      </div>
    </header>

    <!-- ================= HERO KPI STATS ================= -->
    <section class="kpi-row">
      <div class="kpi-chip kpi-emerald">
        <div>
          <div class="kpi-title">Status Sistem</div>
          <div class="kpi-value" id="kpiStatusVal" style="color:var(--emerald);">NORMAL</div>
        </div>
        <span class="kpi-pill state-normal mono" id="kpiStatusPill">FAILSAFE READY</span>
      </div>

      <div class="kpi-chip kpi-amber">
        <div>
          <div class="kpi-title">Ambang Batas (Master RV5)</div>
          <div class="kpi-value mono" id="kpiSetpointVal">4.0 Bar / 160&deg;C</div>
        </div>
        <span class="kpi-pill mono" style="background:rgba(245, 158, 11, 0.15); color:var(--amber);">SYNC 100%</span>
      </div>

      <div class="kpi-chip kpi-cyan">
        <div>
          <div class="kpi-title">Koneksi Slave (UART)</div>
          <div class="kpi-value mono" id="kpiLinkVal">2 / 2 Active</div>
        </div>
        <span class="kpi-pill mono" style="background:rgba(6, 182, 212, 0.15); color:var(--cyan);">9600 BAUD</span>
      </div>

      <div class="kpi-chip">
        <div>
          <div class="kpi-title">Siklus Polling & Klien</div>
          <div class="kpi-value mono" id="kpiPollVal">600 ms &bull; 1 Klien</div>
        </div>
        <span class="kpi-pill state-normal mono">REST API OK</span>
      </div>
    </section>

    <!-- ================= TAB CONTENT 1: MONITORING ================= -->
    <main id="tab-monitoring" class="chamber-grid">

      <!-- BOILER CHAMBER 1 -->
      <article class="chamber-card" id="cardB1">
        <div class="card-header">
          <div class="card-title-group">
            <div class="unit-tag">01</div>
            <div>
              <h2 class="card-title-text">BOILER CHAMBER 1</h2>
              <span class="card-subtitle-text mono">UART 1 &bull; ARDUINO NANO 1 (SLAVE 1)</span>
            </div>
          </div>
          <span class="state-pill state-normal mono" id="b1StatePill">NORMAL</span>
        </div>

        <div class="telemetry-row">
          <!-- Tekanan Radial Gauge -->
          <div class="gauge-box">
            <div class="gauge-top">
              <span class="gauge-label">Tekanan Uap</span>
              <span class="source-badge mono" id="b1SrcP">TRANSDUCER (REAL)</span>
            </div>
            <div class="radial-gauge">
              <svg viewBox="0 0 100 65">
                <path class="gauge-bg" d="M 15 55 A 35 35 0 0 1 85 55" />
                <path class="gauge-fill" id="b1PressArc" d="M 15 55 A 35 35 0 0 1 85 55" />
              </svg>
              <div class="gauge-center-text">
                <div class="gauge-value mono" id="b1PressVal">0.00</div>
                <div class="gauge-unit">BAR (0 - 8.0)</div>
              </div>
            </div>
            <div class="gauge-footer-text mono">
              <span>0.0 Bar</span>
              <span id="b1PressPct">0% TH</span>
              <span>8.0 Bar</span>
            </div>
          </div>

          <!-- Suhu Radial Gauge -->
          <div class="gauge-box">
            <div class="gauge-top">
              <span class="gauge-label">Suhu Boiler</span>
              <span class="source-badge mono" id="b1SrcT">MAX6675 (REAL)</span>
            </div>
            <div class="radial-gauge">
              <svg viewBox="0 0 100 65">
                <path class="gauge-bg" d="M 15 55 A 35 35 0 0 1 85 55" />
                <path class="gauge-fill" id="b1TempArc" style="stroke:var(--amber);" d="M 15 55 A 35 35 0 0 1 85 55" />
              </svg>
              <div class="gauge-center-text">
                <div class="gauge-value mono" id="b1TempVal">0.0</div>
                <div class="gauge-unit">&deg;CELCIUS</div>
              </div>
            </div>
            <div class="gauge-footer-text mono">
              <span>0 &deg;C</span>
              <span id="b1TempPct">0% TH</span>
              <span>200 &deg;C</span>
            </div>
          </div>
        </div>

        <!-- Katup Solenoid & Buzzer Widget -->
        <div class="pipe-schematic-card">
          <div class="valve-visual-wrap">
            <div class="valve-icon-badge valve-closed" id="b1ValveBadge">
              <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5">
                <circle cx="12" cy="12" r="3"></circle>
                <path d="M3 12h6m6 0h6"></path>
                <path d="M12 3v6m0 6v6"></path>
              </svg>
            </div>
            <div>
              <div class="valve-details-title">Katup Solenoid Relief</div>
              <div class="valve-details-state mono" id="b1ValveState" style="color:var(--emerald);">TERTUTUP (0&deg;) - SEALED</div>
            </div>
          </div>

          <div class="buzzer-indicator">
            <div class="buzzer-icon" id="b1BuzzerIcon">&#128276;</div>
            <div>
              <div style="font-size:0.65rem; color:var(--text-dim);">BUZZER</div>
              <div class="mono" id="b1BuzzerText" style="color:var(--text-muted);">STANDBY</div>
            </div>
          </div>
        </div>

        <!-- Action Controls -->
        <div class="action-button-group">
          <button class="app-btn btn-maroon" onclick="sendCmd(1, 'PURGE,1')">
            <span>&#128168;</span> Manual Purge
          </button>
          <button class="app-btn btn-slate" onclick="sendCmd(1, 'PURGE,0')">
            <span>&#128274;</span> Tutup Katup
          </button>
          <button class="app-btn btn-slate" onclick="sendCmd(1, 'MUTE,1')">
            <span>&#128277;</span> Mute Buzzer
          </button>
        </div>
      </article>

      <!-- BOILER CHAMBER 2 -->
      <article class="chamber-card" id="cardB2">
        <div class="card-header">
          <div class="card-title-group">
            <div class="unit-tag" style="color:var(--amber);">02</div>
            <div>
              <h2 class="card-title-text">BOILER CHAMBER 2</h2>
              <span class="card-subtitle-text mono">UART 2 &bull; ARDUINO NANO 2 (SLAVE 2)</span>
            </div>
          </div>
          <span class="state-pill state-normal mono" id="b2StatePill">NORMAL</span>
        </div>

        <div class="telemetry-row">
          <!-- Tekanan Radial Gauge -->
          <div class="gauge-box">
            <div class="gauge-top">
              <span class="gauge-label">Tekanan Uap</span>
              <span class="source-badge mono" id="b2SrcP">TRANSDUCER (REAL)</span>
            </div>
            <div class="radial-gauge">
              <svg viewBox="0 0 100 65">
                <path class="gauge-bg" d="M 15 55 A 35 35 0 0 1 85 55" />
                <path class="gauge-fill" id="b2PressArc" d="M 15 55 A 35 35 0 0 1 85 55" />
              </svg>
              <div class="gauge-center-text">
                <div class="gauge-value mono" id="b2PressVal">0.00</div>
                <div class="gauge-unit">BAR (0 - 8.0)</div>
              </div>
            </div>
            <div class="gauge-footer-text mono">
              <span>0.0 Bar</span>
              <span id="b2PressPct">0% TH</span>
              <span>8.0 Bar</span>
            </div>
          </div>

          <!-- Suhu Radial Gauge -->
          <div class="gauge-box">
            <div class="gauge-top">
              <span class="gauge-label">Suhu Boiler</span>
              <span class="source-badge mono" id="b2SrcT">MAX6675 (REAL)</span>
            </div>
            <div class="radial-gauge">
              <svg viewBox="0 0 100 65">
                <path class="gauge-bg" d="M 15 55 A 35 35 0 0 1 85 55" />
                <path class="gauge-fill" id="b2TempArc" style="stroke:var(--amber);" d="M 15 55 A 35 35 0 0 1 85 55" />
              </svg>
              <div class="gauge-center-text">
                <div class="gauge-value mono" id="b2TempVal">0.0</div>
                <div class="gauge-unit">&deg;CELCIUS</div>
              </div>
            </div>
            <div class="gauge-footer-text mono">
              <span>0 &deg;C</span>
              <span id="b2TempPct">0% TH</span>
              <span>200 &deg;C</span>
            </div>
          </div>
        </div>

        <!-- Katup Solenoid & Buzzer Widget -->
        <div class="pipe-schematic-card">
          <div class="valve-visual-wrap">
            <div class="valve-icon-badge valve-closed" id="b2ValveBadge">
              <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5">
                <circle cx="12" cy="12" r="3"></circle>
                <path d="M3 12h6m6 0h6"></path>
                <path d="M12 3v6m0 6v6"></path>
              </svg>
            </div>
            <div>
              <div class="valve-details-title">Katup Solenoid Relief</div>
              <div class="valve-details-state mono" id="b2ValveState" style="color:var(--emerald);">TERTUTUP (0&deg;) - SEALED</div>
            </div>
          </div>

          <div class="buzzer-indicator">
            <div class="buzzer-icon" id="b2BuzzerIcon">&#128276;</div>
            <div>
              <div style="font-size:0.65rem; color:var(--text-dim);">BUZZER</div>
              <div class="mono" id="b2BuzzerText" style="color:var(--text-muted);">STANDBY</div>
            </div>
          </div>
        </div>

        <!-- Action Controls -->
        <div class="action-button-group">
          <button class="app-btn btn-maroon" onclick="sendCmd(2, 'PURGE,1')">
            <span>&#128168;</span> Manual Purge
          </button>
          <button class="app-btn btn-slate" onclick="sendCmd(2, 'PURGE,0')">
            <span>&#128274;</span> Tutup Katup
          </button>
          <button class="app-btn btn-slate" onclick="sendCmd(2, 'MUTE,1')">
            <span>&#128277;</span> Mute Buzzer
          </button>
        </div>
      </article>

    </main>

    <!-- ================= TAB CONTENT 2: ANALYTICS & LIVE CHART ================= -->
    <section id="tab-analytics" class="chart-card" style="display:none;">
      <div class="chart-header">
        <div>
          <h2 style="font-size:1.1rem; font-weight:800;">TREN TELEMETRI TEKANAN REAL-TIME</h2>
          <p style="font-size:0.75rem; color:var(--text-muted);">Live Canvas 60 FPS &bull; Interval Update 600ms &bull; Zero-Lag Local Rendering</p>
        </div>
        <div class="chart-legend mono">
          <div class="legend-item"><div class="legend-box" style="background:var(--cyan);"></div> Boiler 1 (Bar)</div>
          <div class="legend-item"><div class="legend-box" style="background:var(--violet);"></div> Boiler 2 (Bar)</div>
          <div class="legend-item"><div class="legend-box" style="background:var(--crimson);"></div> Ambang Batas Setpoint</div>
        </div>
      </div>
      <div class="canvas-container">
        <canvas id="liveChartCanvas"></canvas>
      </div>
    </section>

    <!-- ================= TAB CONTENT 3: EVENT LOGS ================= -->
    <section id="tab-events" class="log-card" style="display:none;">
      <div style="display:flex; justify-content:space-between; align-items:center;">
        <div>
          <h2 style="font-size:1.1rem; font-weight:800;">RIWAYAT KEJADIAN & PERINGATAN SISTEM</h2>
          <p style="font-size:0.75rem; color:var(--text-muted);">Audit log operasional, perubahan katup, dan pemicu alarm</p>
        </div>
        <button class="btn-icon mono" onclick="clearEventLogs()">Bersihkan Log</button>
      </div>
      <div class="log-table-wrap">
        <table class="log-table mono">
          <thead>
            <tr>
              <th>WAKTU</th>
              <th>NODE</th>
              <th>EVENT / STATUS</th>
              <th>PARAMETER</th>
              <th>KATUP</th>
            </tr>
          </thead>
          <tbody id="logTableBody">
            <tr><td colspan="5" style="text-align:center; color:var(--text-dim); padding:1.5rem;">Belum ada log tercatat...</td></tr>
          </tbody>
        </table>
      </div>
    </section>

    <!-- ================= TAB CONTENT 4: DIAGNOSTICS ================= -->
    <section id="tab-diag" class="diag-grid" style="display:none;">
      <div class="diag-card">
        <h3 style="font-weight:800; font-size:1rem; border-bottom:1px solid var(--border); padding-bottom:0.5rem;">HARDWARE MASTER (ESP32-S3)</h3>
        <div class="diag-item"><span>Chip Controller</span><span class="mono">ESP32-S3-WROOM-1 (N8R2)</span></div>
        <div class="diag-item"><span>Flash / PSRAM</span><span class="mono">8MB Flash / 2MB Quad PSRAM</span></div>
        <div class="diag-item"><span>Display SPI</span><span class="mono">ST7789 1.47" (320x172 Horizontal)</span></div>
        <div class="diag-item"><span>SPI Clock Bus</span><span class="mono">40 MHz Hardware SPI</span></div>
        <div class="diag-item"><span>Master Uptime</span><span class="mono" id="diagUptime">--:--:--</span></div>
      </div>

      <div class="diag-card">
        <h3 style="font-weight:800; font-size:1rem; border-bottom:1px solid var(--border); padding-bottom:0.5rem;">JARINGAN & KOMUNIKASI</h3>
        <div class="diag-item"><span>Mode WiFi</span><span class="mono">Standalone SoftAP (192.168.4.1)</span></div>
        <div class="diag-item"><span>SSID Hotspot</span><span class="mono">SCADA_BOILER_PLTU</span></div>
        <div class="diag-item"><span>Klien Terhubung</span><span class="mono" id="diagClients">1 Perangkat</span></div>
        <div class="diag-item"><span>Protokol Slave</span><span class="mono">Dual Hardware UART (9600 8N1)</span></div>
        <div class="diag-item"><span>Heartbeat Sync</span><span class="mono">1000 ms (Setpoint Auto-Broadcast)</span></div>
      </div>

      <div class="diag-card">
        <h3 style="font-weight:800; font-size:1rem; border-bottom:1px solid var(--border); padding-bottom:0.5rem;">FAILSAFE & PROTEKSI OTOMATIS</h3>
        <div class="diag-item"><span>Safety Latched Buzzer</span><span class="mono" style="color:var(--emerald);">ENABLED (Kontinu)</span></div>
        <div class="diag-item"><span>Pressure Hysteresis</span><span class="mono">0.20 Bar (Anti-Chatter)</span></div>
        <div class="diag-item"><span>Temperature Hysteresis</span><span class="mono">3.0 &deg;C</span></div>
        <div class="diag-item"><span>Watchdog Link Slave</span><span class="mono">3000 ms Timeout</span></div>
      </div>
    </section>

    <!-- ================= EMERGENCY MASTER SAFETY OVERRIDE ================= -->
    <section class="emergency-console">
      <div class="emergency-text-group">
        <div class="emergency-badge-icon">&#9888;</div>
        <div>
          <h3 class="emergency-heading">MASTER SAFETY OVERRIDE CONSOLE</h3>
          <p class="emergency-subheading">
            Protokol Darurat PLTU: Buka serentak seluruh katup relief pembuang uap dan bypass kontrol otomatis untuk meredakan overpressure seketika.
          </p>
        </div>
      </div>

      <div class="emergency-btn-group">
        <button class="btn-emergency-action" onclick="openEmergencyModal()">
          <span>&#128680;</span> EMERGENCY ALL-PURGE
        </button>
        <button class="app-btn btn-slate" style="padding:0.75rem 1.25rem;" onclick="sendCmd(0, 'PURGE,0')">
          <span>&#128274;</span> TUTUP SEMUA KATUP
        </button>
        <button class="app-btn btn-slate" style="padding:0.75rem 1.25rem;" onclick="sendCmd(0, 'MUTE,1')">
          <span>&#128277;</span> SILENCE ALARMS
        </button>
      </div>
    </section>

  </div>

  <!-- ================= REACT-LIKE CONFIRMATION MODAL ================= -->
  <div class="modal-overlay" id="confirmModal">
    <div class="modal-dialog">
      <div style="font-size:2rem; margin-bottom:0.5rem;">&#9888;</div>
      <h3 style="font-size:1.15rem; font-weight:800; color:var(--crimson); margin-bottom:0.5rem;">
        Konfirmasi Emergency All-Purge?
      </h3>
      <p style="font-size:0.8rem; color:var(--text-muted); line-height:1.5; margin-bottom:1.25rem;">
        Perintah ini akan membuka seluruh katup solenoid servo di <strong>Boiler 1 dan Boiler 2</strong> secara serentak untuk membuang uap. Anda yakin ingin melanjutkan?
      </p>
      <div style="display:flex; gap:0.75rem;">
        <button class="app-btn btn-maroon" style="flex:1;" onclick="confirmEmergencyPurge()">
          Ya, Buka Semua Katup
        </button>
        <button class="app-btn btn-slate" style="flex:1;" onclick="closeEmergencyModal()">
          Batal
        </button>
      </div>
    </div>
  </div>

  <!-- ================= FLOATING TOAST STACK ================= -->
  <div class="toast-container" id="toastBox"></div>

  <!-- ==========================================================================
       VANILLA REACT-STYLE APPLICATION STATE & CONTROLLER
       ========================================================================== -->
  <script>
    // --------------------------------------------------------------------------
    // STATE STORE
    // --------------------------------------------------------------------------
    const State = {
      theme: 'dark',
      sound: true,
      activeTab: 'tab-monitoring',
      currentSetP: 4.0,
      currentSetT: 160.0,
      audioCtx: null,
      lastAlarmB1: false,
      lastAlarmB2: false,
      history: {
        times: [],
        p1: [],
        p2: [],
        sp: []
      },
      events: []
    };

    // --------------------------------------------------------------------------
    // TAB CONTROLLER
    // --------------------------------------------------------------------------
    function switchTab(tabId) {
      State.activeTab = tabId;
      document.querySelectorAll('.tab-item').forEach(el => el.classList.remove('active'));
      const activeBtn = Array.from(document.querySelectorAll('.tab-item')).find(b => b.getAttribute('onclick').includes(tabId));
      if (activeBtn) activeBtn.classList.add('active');

      ['tab-monitoring', 'tab-analytics', 'tab-events', 'tab-diag'].forEach(id => {
        const sec = document.getElementById(id);
        if (sec) sec.style.display = (id === tabId) ? (id === 'tab-monitoring' ? 'grid' : 'flex') : 'none';
      });

      if (tabId === 'tab-analytics') drawChart();
      playBeep(700, 0.04);
    }

    // --------------------------------------------------------------------------
    // THEME & SOUND CONTROLLERS
    // --------------------------------------------------------------------------
    function toggleTheme() {
      State.theme = State.theme === 'dark' ? 'light' : 'dark';
      document.documentElement.setAttribute('data-theme', State.theme);
      document.getElementById('themeIcon').innerText = State.theme === 'dark' ? '&#9728;' : '&#9790;';
      document.getElementById('themeLabel').innerText = State.theme === 'dark' ? 'Light' : 'Dark';
      showToast(`Tema beralih ke Mode ${State.theme.toUpperCase()}`);
      if (State.activeTab === 'tab-analytics') drawChart();
    }

    function toggleAudio() {
      State.sound = !State.sound;
      document.getElementById('audioIcon').innerText = State.sound ? '&#128266;' : '&#128263;';
      document.getElementById('audioLabel').innerText = State.sound ? 'Audio: ON' : 'Audio: OFF';
      showToast(`Audio alarm ${State.sound ? 'diaktifkan' : 'dimatikan'}`);
    }

    function playBeep(freq = 880, duration = 0.12) {
      if (!State.sound) return;
      try {
        if (!State.audioCtx) State.audioCtx = new (window.AudioContext || window.webkitAudioContext)();
        if (State.audioCtx.state === 'suspended') State.audioCtx.resume();
        const osc = State.audioCtx.createOscillator();
        const gain = State.audioCtx.createGain();
        osc.type = 'sine';
        osc.frequency.value = freq;
        gain.gain.setValueAtTime(0.08, State.audioCtx.currentTime);
        gain.gain.exponentialRampToValueAtTime(0.001, State.audioCtx.currentTime + duration);
        osc.connect(gain);
        gain.connect(State.audioCtx.destination);
        osc.start();
        osc.stop(State.audioCtx.currentTime + duration);
      } catch(e) {}
    }

    // --------------------------------------------------------------------------
    // TOAST NOTIFICATIONS
    // --------------------------------------------------------------------------
    function showToast(msg, icon = '&#8505;') {
      const box = document.getElementById('toastBox');
      const toast = document.createElement('div');
      toast.className = 'toast-card';
      toast.innerHTML = `<span>${icon}</span> <span>${msg}</span>`;
      box.appendChild(toast);
      setTimeout(() => {
        toast.style.opacity = '0';
        toast.style.transform = 'translateY(10px)';
        toast.style.transition = 'all 0.3s ease';
        setTimeout(() => toast.remove(), 300);
      }, 2800);
    }

    // --------------------------------------------------------------------------
    // MODAL DIALOG CONTROLLER
    // --------------------------------------------------------------------------
    function openEmergencyModal() {
      document.getElementById('confirmModal').classList.add('open');
      playBeep(440, 0.2);
    }
    function closeEmergencyModal() {
      document.getElementById('confirmModal').classList.remove('open');
    }
    function confirmEmergencyPurge() {
      closeEmergencyModal();
      sendCmd(0, 'PURGE,1');
    }

    // --------------------------------------------------------------------------
    // EVENT LOGGER
    // --------------------------------------------------------------------------
    function addEventLog(node, status, param, valve) {
      const now = new Date();
      const timeStr = now.toTimeString().split(' ')[0];
      State.events.unshift({ time: timeStr, node, status, param, valve });
      if (State.events.length > 50) State.events.pop();
      renderEventLogs();
    }

    function renderEventLogs() {
      const tbody = document.getElementById('logTableBody');
      if (State.events.length === 0) {
        tbody.innerHTML = '<tr><td colspan="5" style="text-align:center; color:var(--text-dim); padding:1.5rem;">Belum ada log tercatat...</td></tr>';
        return;
      }
      tbody.innerHTML = State.events.map(ev => `
        <tr>
          <td style="color:var(--text-muted);">${ev.time}</td>
          <td style="font-weight:700;">${ev.node}</td>
          <td><span class="state-pill ${ev.status.includes('ALARM') ? 'state-alarm' : ev.status.includes('WARN') ? 'state-warning' : 'state-normal'}">${ev.status}</span></td>
          <td>${ev.param}</td>
          <td style="color:${ev.valve === 'OPEN' ? 'var(--crimson)' : 'var(--emerald)'}; font-weight:700;">${ev.valve === 'OPEN' ? 'TERBUKA' : 'TERTUTUP'}</td>
        </tr>
      `).join('');
    }

    function clearEventLogs() {
      State.events = [];
      renderEventLogs();
      showToast('Riwayat log berhasil dibersihkan');
    }

    // --------------------------------------------------------------------------
    // RADIAL GAUGE CALCULATOR (SVG Arc Math)
    // Semicircle perimeter for r=35 is Math.PI * 35 = 110.0
    // --------------------------------------------------------------------------
    function updateRadialGauge(arcId, value, maxVal, isWarn, isDanger) {
      const arc = document.getElementById(arcId);
      if (!arc) return;
      const arcLen = 110.0;
      const ratio = Math.min(Math.max(value / maxVal, 0), 1);
      const offset = arcLen * (1 - ratio);
      arc.style.strokeDasharray = `${arcLen}`;
      arc.style.strokeDashoffset = `${offset}`;

      arc.classList.remove('warn', 'danger');
      if (isDanger) arc.classList.add('danger');
      else if (isWarn) arc.classList.add('warn');
    }

    // --------------------------------------------------------------------------
    // TELEMETRY FETCHER & REACTION ENGINE
    // --------------------------------------------------------------------------
    async function updateTelemetry() {
      try {
        const res = await fetch('/api/data');
        if (!res.ok) throw new Error('HTTP ' + res.status);
        const d = await res.json();

        // 1. Connection status
        document.getElementById('liveDot').classList.remove('offline');
        document.getElementById('netStatusText').innerText = `ONLINE (${d.ap_clients || 1} Klien)`;

        // 2. Setpoint
        State.currentSetP = d.setpoint_p || 4.0;
        State.currentSetT = d.setpoint_t || 160.0;
        document.getElementById('kpiSetpointVal').innerText = `${State.currentSetP.toFixed(1)} Bar / ${Math.round(State.currentSetT)}&deg;C`;

        // 3. Update Chamber 1 & 2
        updateChamber(1, d.slave1);
        updateChamber(2, d.slave2);

        // 4. Update Global Status KPI
        const isAnyAlarm = (d.slave1 && d.slave1.state === 'ALARM') || (d.slave2 && d.slave2.state === 'ALARM');
        const isAnyWarn  = (d.slave1 && d.slave1.state === 'WARNING') || (d.slave2 && d.slave2.state === 'WARNING');
        const kpiVal = document.getElementById('kpiStatusVal');
        const kpiPill = document.getElementById('kpiStatusPill');

        if (isAnyAlarm) {
          kpiVal.innerText = '&#128680; ALARM DARURAT';
          kpiVal.style.color = 'var(--crimson)';
          kpiPill.innerText = 'OVERPRESSURE';
          kpiPill.className = 'kpi-pill state-alarm mono';
          playBeep(920, 0.35);
        } else if (isAnyWarn) {
          kpiVal.innerText = '&#9888; PERINGATAN';
          kpiVal.style.color = 'var(--amber)';
          kpiPill.innerText = 'MENDEKATI BATAS';
          kpiPill.className = 'kpi-pill state-warning mono';
        } else {
          kpiVal.innerText = 'NORMAL';
          kpiVal.style.color = 'var(--emerald)';
          kpiPill.innerText = 'SISTEM STABIL';
          kpiPill.className = 'kpi-pill state-normal mono';
        }

        // 5. Diagnostics & Klien
        const activeUnits = (d.slave1 && d.slave1.connected ? 1 : 0) + (d.slave2 && d.slave2.connected ? 1 : 0);
        document.getElementById('kpiLinkVal').innerText = `${activeUnits} / 2 Active`;
        if (d.uptime) {
          const sec = d.uptime;
          const upStr = `${String(Math.floor(sec/3600)).padStart(2,'0')}:${String(Math.floor((sec%3600)/60)).padStart(2,'0')}:${String(sec%60).padStart(2,'0')}`;
          document.getElementById('diagUptime').innerText = upStr;
        }
        if (d.ap_clients !== undefined) {
          document.getElementById('diagClients').innerText = `${d.ap_clients} Perangkat`;
          document.getElementById('kpiPollVal').innerText = `600 ms &bull; ${d.ap_clients} Klien`;
        }

        // 6. Push to historical buffer for Canvas Chart
        const nowStr = new Date().toTimeString().split(' ')[0];
        State.history.times.push(nowStr);
        State.history.p1.push(d.slave1 ? d.slave1.pressure : 0);
        State.history.p2.push(d.slave2 ? d.slave2.pressure : 0);
        State.history.sp.push(State.currentSetP);
        if (State.history.times.length > 30) {
          State.history.times.shift();
          State.history.p1.shift();
          State.history.p2.shift();
          State.history.sp.shift();
        }

        if (State.activeTab === 'tab-analytics') drawChart();

      } catch (err) {
        document.getElementById('liveDot').classList.add('offline');
        document.getElementById('netStatusText').innerText = 'OFFLINE (LOST LINK)';
      }
    }

    function updateChamber(id, data) {
      const prefix = `b${id}`;
      const cardEl = document.getElementById(`cardB${id}`);
      if (!data || data.connected === false) {
        // Offline Chamber Handling
        document.getElementById(`${prefix}StatePill`).innerText = 'OFFLINE';
        document.getElementById(`${prefix}StatePill`).className = 'state-pill state-offline mono';
        cardEl.classList.remove('is-alarm');
        document.getElementById(`${prefix}PressVal`).innerText = '--';
        document.getElementById(`${prefix}TempVal`).innerText = '--';
        document.getElementById(`${prefix}ValveState`).innerText = 'TIDAK TERHUBUNG';
        document.getElementById(`${prefix}ValveState`).style.color = 'var(--text-dim)';
        document.getElementById(`${prefix}ValveBadge`).className = 'valve-icon-badge';
        document.getElementById(`${prefix}BuzzerText`).innerText = 'OFFLINE';
        updateRadialGauge(`${prefix}PressArc`, 0, 8.0, false, false);
        updateRadialGauge(`${prefix}TempArc`, 0, 200.0, false, false);
        return;
      }

      const p = data.pressure || 0;
      const t = data.temp || 0;
      const isOverP = (p >= State.currentSetP);
      const isWarnP = (p >= State.currentSetP * 0.85);
      const isOverT = (t >= State.currentSetT);
      const isWarnT = (t >= State.currentSetT * 0.90);

      // Display Values
      document.getElementById(`${prefix}PressVal`).innerText = p.toFixed(2);
      document.getElementById(`${prefix}TempVal`).innerText = (t >= 900.0) ? 'ERR' : t.toFixed(1);

      // Gauge Percentages
      const pctP = Math.round((p / State.currentSetP) * 100);
      const pctT = Math.round((t / State.currentSetT) * 100);
      document.getElementById(`${prefix}PressPct`).innerText = `${pctP}% TH`;
      document.getElementById(`${prefix}TempPct`).innerText = `${pctT}% TH`;

      // Update Radials
      updateRadialGauge(`${prefix}PressArc`, p, 8.0, isWarnP, isOverP);
      updateRadialGauge(`${prefix}TempArc`, t, 200.0, isWarnT, isOverT);

      // Sensor Source Tags
      const srcPEl = document.getElementById(`${prefix}SrcP`);
      srcPEl.innerText = data.src_p === 'SIM' ? 'POTENSIO (SIM)' : 'TRANSDUCER (REAL)';
      srcPEl.className = 'source-badge mono' + (data.src_p === 'SIM' ? ' sim' : '');

      const srcTEl = document.getElementById(`${prefix}SrcT`);
      srcTEl.innerText = data.src_t === 'SIM' ? 'POTENSIO (SIM)' : 'MAX6675 (REAL)';
      srcTEl.className = 'source-badge mono' + (data.src_t === 'SIM' ? ' sim' : '');

      // Solenoid Valve State
      const isValveOpen = (data.valve === 'OPEN');
      const valveText = document.getElementById(`${prefix}ValveState`);
      const valveBadge = document.getElementById(`${prefix}ValveBadge`);
      if (isValveOpen) {
        valveText.innerText = 'TERBUKA (90&deg;) - EXHAUST ACTIVE';
        valveText.style.color = 'var(--crimson)';
        valveBadge.className = 'valve-icon-badge valve-open';
      } else {
        valveText.innerText = 'TERTUTUP (0&deg;) - SEALED';
        valveText.style.color = 'var(--emerald)';
        valveBadge.className = 'valve-icon-badge valve-closed';
      }

      // Buzzer State
      const bzIcon = document.getElementById(`${prefix}BuzzerIcon`);
      const bzText = document.getElementById(`${prefix}BuzzerText`);
      if (data.buzzer) {
        bzIcon.className = 'buzzer-icon active';
        bzText.innerText = 'BERBUNYI (ALARM)';
        bzText.style.color = 'var(--crimson)';
      } else {
        bzIcon.className = 'buzzer-icon';
        bzText.innerText = 'STANDBY';
        bzText.style.color = 'var(--text-muted)';
      }

      // Card Container Alarm State
      const statePill = document.getElementById(`${prefix}StatePill`);
      statePill.innerText = data.state || 'NORMAL';
      cardEl.classList.remove('is-alarm');

      if (data.state === 'ALARM') {
        statePill.className = 'state-pill state-alarm mono';
        cardEl.classList.add('is-alarm');
        if ((id === 1 && !State.lastAlarmB1) || (id === 2 && !State.lastAlarmB2)) {
          addEventLog(`BOILER ${id}`, '&#128680; ALARM DARURAT', `${p.toFixed(2)} Bar / ${t.toFixed(1)}&deg;C`, data.valve);
        }
      } else if (data.state === 'WARNING') {
        statePill.className = 'state-pill state-warning mono';
      } else {
        statePill.className = 'state-pill state-normal mono';
      }

      if (id === 1) State.lastAlarmB1 = (data.state === 'ALARM');
      if (id === 2) State.lastAlarmB2 = (data.state === 'ALARM');
    }

    // --------------------------------------------------------------------------
    // ACTION COMMAND SENDER (Optimistic Feedback)
    // --------------------------------------------------------------------------
    async function sendCmd(nodeId, action) {
      playBeep(650, 0.08);
      const targetLabel = nodeId === 0 ? 'Semua Boiler' : `Boiler ${nodeId}`;
      showToast(`Mengirim perintah [${action}] ke ${targetLabel}...`);
      try {
        await fetch(`/api/command?node=${nodeId}&action=${action}`);
        addEventLog(targetLabel, 'COMMAND EXECUTED', action, action.includes('PURGE,1') ? 'OPEN' : 'CLOSED');
        setTimeout(updateTelemetry, 150);
      } catch (err) {
        showToast(`&#10060; Gagal mengirim perintah ke ${targetLabel}`);
      }
    }

    // --------------------------------------------------------------------------
    // CANVAS CHART ENGINE (Native 60 FPS Canvas without external libs)
    // --------------------------------------------------------------------------
    function drawChart() {
      const canvas = document.getElementById('liveChartCanvas');
      if (!canvas) return;
      const ctx = canvas.getContext('2d');
      const w = canvas.parentElement.clientWidth;
      const h = canvas.parentElement.clientHeight;
      canvas.width = w;
      canvas.height = h;

      ctx.clearRect(0, 0, w, h);

      // Grid background
      const padding = { top: 25, right: 30, bottom: 35, left: 45 };
      const plotW = w - padding.left - padding.right;
      const plotH = h - padding.top - padding.bottom;

      ctx.strokeStyle = State.theme === 'dark' ? '#334155' : '#e2e8f0';
      ctx.lineWidth = 1;

      // Horizontal grid lines (0 to 8 Bar)
      for (let bar = 0; bar <= 8; bar += 2) {
        const y = padding.top + plotH - (bar / 8.0) * plotH;
        ctx.beginPath();
        ctx.moveTo(padding.left, y);
        ctx.lineTo(w - padding.right, y);
        ctx.stroke();

        ctx.fillStyle = '#64748b';
        ctx.font = '10px "SF Mono", monospace';
        ctx.textAlign = 'right';
        ctx.fillText(`${bar}B`, padding.left - 8, y + 3);
      }

      if (State.history.p1.length < 2) return;

      const n = State.history.p1.length;
      const xStep = plotW / (n - 1);

      // Helper to draw a line plot
      function plotLine(data, color, isDashed = false) {
        ctx.strokeStyle = color;
        ctx.lineWidth = isDashed ? 1.5 : 2.5;
        if (isDashed) ctx.setLineDash([4, 4]); else ctx.setLineDash([]);
        ctx.beginPath();
        for (let i = 0; i < n; i++) {
          const x = padding.left + i * xStep;
          const y = padding.top + plotH - (Math.min(data[i] / 8.0, 1.0)) * plotH;
          if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
        }
        ctx.stroke();
        ctx.setLineDash([]);
      }

      // Plot Setpoint Threshold (Dashed Red)
      plotLine(State.history.sp, '#ef4444', true);
      // Plot Boiler 1 (Cyan)
      plotLine(State.history.p1, '#06b6d4', false);
      // Plot Boiler 2 (Violet)
      plotLine(State.history.p2, '#8b5cf6', false);
    }

    // --------------------------------------------------------------------------
    // STARTUP
    // --------------------------------------------------------------------------
    window.addEventListener('resize', () => {
      if (State.activeTab === 'tab-analytics') drawChart();
    });

    setInterval(updateTelemetry, 600);
    updateTelemetry();
  </script>
</body>
</html>

)rawliteral";

#endif // DASHBOARD_HTML_H