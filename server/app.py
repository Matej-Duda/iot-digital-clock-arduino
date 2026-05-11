#!/usr/bin/env python3
"""
Multifunkcni digitalni hodiny - Flask server v2
================================================
Maturitni projekt - 2026

Funkce:
  - Webove rozhrani s casem, teplotou, tydennimy budiky
  - Open-Meteo API (Liberec) kazdych 15 min
  - POST /api/notification pro iPhone Shortcuts (notifikace z Instagramu atd.)
  - GET /api/arduino - plain-text pro Arduino
"""

import threading
import time
from datetime import datetime
from zoneinfo import ZoneInfo

import requests
from flask import Flask, render_template_string, request, jsonify, redirect

# ------------- Konfigurace -------------
# Liberec: 50.7671 N, 15.0562 E
OPEN_METEO_URL = (
    "https://api.open-meteo.com/v1/forecast"
    "?latitude=50.7671&longitude=15.0562"
    "&current_weather=true"
    "&timezone=Europe%2FPrague"
)
TEMP_FETCH_INTERVAL = 900  # 15 minut
PRAGUE_TZ = ZoneInfo("Europe/Prague")

# ------------- Globalni stav -------------
current_temperature = None

# Rychly budik (jednorazovy)
quick_alarm_time = ""

# Tydenni budiky: { "po": {"time": "07:00", "enabled": True}, ... }
DAY_KEYS = ["po", "ut", "st", "ct", "pa", "so", "ne"]
DAY_NAMES = {
    "po": "Pondeli", "ut": "Utery", "st": "Streda", "ct": "Ctvrtek",
    "pa": "Patek", "so": "Sobota", "ne": "Nedele"
}
# Python weekday(): 0=Monday ... 6=Sunday -> mapovani na nase klice
WEEKDAY_TO_KEY = {0: "po", 1: "ut", 2: "st", 3: "ct", 4: "pa", 5: "so", 6: "ne"}

weekly_alarms = {day: {"time": "", "enabled": False} for day in DAY_KEYS}

# Notifikace (z iPhone Shortcuts)
notification_title = ""
notification_body = ""

app = Flask(__name__)

# ------------- Stahovani teploty -------------
def fetch_temperature():
    global current_temperature
    try:
        resp = requests.get(OPEN_METEO_URL, timeout=10)
        resp.raise_for_status()
        data = resp.json()
        current_temperature = data["current_weather"]["temperature"]
        print(f"[OK] Teplota: {current_temperature} C")
    except Exception as e:
        print(f"[CHYBA] Teplota: {e}")


def temperature_worker():
    while True:
        fetch_temperature()
        time.sleep(TEMP_FETCH_INTERVAL)


# ------------- Kontrola budiku -------------
def is_alarm_active():
    """Vraci True pokud ma zustat budik aktivni v tuto minutu."""
    now = datetime.now(PRAGUE_TZ)
    time_str = now.strftime("%H:%M")

    # 1) Rychly budik
    if quick_alarm_time and time_str == quick_alarm_time:
        return True

    # 2) Tydenni budik pro dnesni den
    today_key = WEEKDAY_TO_KEY[now.weekday()]
    day_alarm = weekly_alarms[today_key]
    if day_alarm["enabled"] and day_alarm["time"] and time_str == day_alarm["time"]:
        return True

    return False


# ------------- HTML sablona -------------
HTML_PAGE = r"""
<!DOCTYPE html>
<html lang="cs">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Multifunkcni hodiny</title>
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600;700&family=JetBrains+Mono:wght@500&display=swap" rel="stylesheet">
  <style>
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

    body {
      font-family: 'Inter', sans-serif;
      min-height: 100vh;
      display: flex;
      justify-content: center;
      background: linear-gradient(135deg, #0f0c29, #302b63, #24243e);
      color: #e0e0e0;
      padding: 1.2rem;
      padding-top: 2rem;
    }

    .container { width: 100%; max-width: 520px; }

    /* -- Karta -- */
    .card {
      background: rgba(255,255,255,0.06);
      backdrop-filter: blur(18px);
      -webkit-backdrop-filter: blur(18px);
      border: 1px solid rgba(255,255,255,0.12);
      border-radius: 20px;
      padding: 1.5rem 1.6rem;
      margin-bottom: 1rem;
      box-shadow: 0 8px 32px rgba(0,0,0,0.35);
    }

    h1 {
      text-align: center;
      font-size: 1.4rem;
      font-weight: 700;
      margin-bottom: 1rem;
      background: linear-gradient(90deg, #a78bfa, #818cf8);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
    }

    /* -- Hodiny -- */
    #clock {
      font-family: 'JetBrains Mono', monospace;
      font-size: clamp(2.4rem, 10vw, 3.8rem);
      font-weight: 500;
      text-align: center;
      letter-spacing: .03em;
      color: #f1f5f9;
      text-shadow: 0 0 20px rgba(167,139,250,.45);
      overflow: hidden;
      white-space: nowrap;
    }
    #date-line {
      text-align: center;
      font-size: .85rem;
      color: #9ca3af;
      margin-top: .3rem;
    }

    /* -- Teplota -- */
    .temp-wrap { text-align: center; margin-top: .8rem; }
    .temp-badge {
      display: inline-flex;
      align-items: center;
      gap: .4rem;
      background: rgba(56,189,248,0.12);
      border: 1px solid rgba(56,189,248,0.25);
      border-radius: 999px;
      padding: .4rem 1rem;
      font-size: 1.05rem;
      font-weight: 600;
      color: #38bdf8;
    }

    /* -- Sekce nadpis -- */
    .section-title {
      font-size: .82rem;
      font-weight: 600;
      text-transform: uppercase;
      letter-spacing: .08em;
      color: #a78bfa;
      margin-bottom: .7rem;
    }

    /* -- Formulare -- */
    .form-row { display: flex; gap: .5rem; }

    input[type="time"],
    input[type="text"] {
      flex: 1;
      padding: .55rem .75rem;
      border-radius: 10px;
      border: 1px solid rgba(255,255,255,.12);
      background: rgba(255,255,255,.06);
      color: #f1f5f9;
      font-size: .9rem;
      font-family: inherit;
      outline: none;
      transition: border-color .2s;
    }
    input:focus { border-color: #818cf8; }

    button, .btn {
      padding: .55rem 1.1rem;
      border-radius: 10px;
      border: none;
      background: linear-gradient(135deg, #7c3aed, #6366f1);
      color: #fff;
      font-size: .85rem;
      font-weight: 600;
      cursor: pointer;
      transition: opacity .2s, transform .15s;
      white-space: nowrap;
      text-decoration: none;
      display: inline-flex;
      align-items: center;
    }
    button:hover, .btn:hover { opacity: .88; transform: scale(1.03); }

    /* -- Flash -- */
    .flash {
      margin-top: .6rem;
      padding: .45rem .75rem;
      border-radius: 8px;
      font-size: .78rem;
      text-align: center;
      animation: fadeIn .3s ease;
      background: rgba(52,211,153,.12);
      color: #34d399;
      border: 1px solid rgba(52,211,153,.25);
    }
    @keyframes fadeIn { from { opacity:0; transform:translateY(-5px); } to { opacity:1; transform:translateY(0); } }

    /* -- Tydenni budiky -- */
    .weekly-grid { display: flex; flex-direction: column; gap: .5rem; }
    .day-row {
      display: flex;
      align-items: center;
      gap: .5rem;
      padding: .45rem .65rem;
      background: rgba(255,255,255,.04);
      border: 1px solid rgba(255,255,255,.08);
      border-radius: 12px;
    }
    .day-row.active { border-color: rgba(129,140,248,.35); background: rgba(129,140,248,.08); }
    .day-label {
      width: 55px;
      font-size: .82rem;
      font-weight: 600;
      color: #c4b5fd;
      flex-shrink: 0;
    }
    .day-row input[type="time"] {
      flex: 1;
      padding: .4rem .55rem;
      font-size: .82rem;
      min-width: 0;
    }

    /* Toggle switch */
    .toggle { position: relative; width: 42px; height: 24px; flex-shrink: 0; }
    .toggle input { opacity: 0; width: 0; height: 0; }
    .toggle .slider {
      position: absolute; inset: 0;
      background: rgba(255,255,255,.12);
      border-radius: 24px;
      cursor: pointer;
      transition: background .25s;
    }
    .toggle .slider::before {
      content: "";
      position: absolute;
      width: 18px; height: 18px;
      left: 3px; bottom: 3px;
      background: #e0e0e0;
      border-radius: 50%;
      transition: transform .25s;
    }
    .toggle input:checked + .slider { background: #7c3aed; }
    .toggle input:checked + .slider::before { transform: translateX(18px); background: #fff; }

    /* -- Notifikace karta -- */
    .notif-card {
      display: flex;
      align-items: flex-start;
      gap: .7rem;
      padding: .6rem;
    }
    .notif-icon {
      width: 36px; height: 36px;
      border-radius: 10px;
      background: linear-gradient(135deg, #f09433, #e6683c, #dc2743, #cc2366, #bc1888);
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: 1.1rem;
      flex-shrink: 0;
    }
    .notif-content { flex: 1; min-width: 0; }
    .notif-from { font-size: .82rem; font-weight: 600; color: #e0e0e0; }
    .notif-msg { font-size: .78rem; color: #9ca3af; margin-top: .15rem; word-break: break-word; }
    .notif-empty { font-size: .82rem; color: #6b7280; text-align: center; padding: .5rem; }

    /* -- Footer info -- */
    .info {
      text-align: center;
      font-size: .72rem;
      color: #4b5563;
      margin-top: .3rem;
    }
    .info code {
      background: rgba(255,255,255,.08);
      padding: .12rem .4rem;
      border-radius: 5px;
      font-family: 'JetBrains Mono', monospace;
      font-size: .7rem;
    }
  </style>
</head>
<body>
  <div class="container">

    <!-- === Hodiny + teplota === -->
    <div class="card">
      <h1>Multifunkcni hodiny</h1>
      <div id="clock">00:00:00</div>
      <div id="date-line"></div>
      <div class="temp-wrap">
        <div class="temp-badge">
          <span>🌡️</span>
          <span id="temp-value">{{ temp }}</span>
        </div>
      </div>
    </div>

    <!-- === Rychly budik === -->
    <div class="card">
      <div class="section-title">⏰ Rychly budik</div>
      <form method="POST" action="/set_alarm">
        <div class="form-row">
          <input type="time" name="alarm" value="{{ quick_alarm }}" required>
          <button type="submit">Nastavit</button>
        </div>
      </form>
      {% if alarm_flash %}
        <div class="flash">{{ alarm_flash }}</div>
      {% endif %}
    </div>

    <!-- === Tydenni budiky === -->
    <div class="card">
      <div class="section-title">📅 Tydenni budiky</div>
      <form method="POST" action="/set_weekly">
        <div class="weekly-grid">
          {% for day in day_keys %}
          <div class="day-row {{ 'active' if weekly[day].enabled else '' }}">
            <span class="day-label">{{ day_names[day] }}</span>
            <input type="time" name="time_{{ day }}" value="{{ weekly[day].time }}">
            <label class="toggle">
              <input type="checkbox" name="on_{{ day }}" {{ 'checked' if weekly[day].enabled else '' }}>
              <span class="slider"></span>
            </label>
          </div>
          {% endfor %}
        </div>
        <div style="margin-top:.7rem; text-align:center;">
          <button type="submit">Ulozit rozvrh</button>
        </div>
      </form>
      {% if weekly_flash %}
        <div class="flash">{{ weekly_flash }}</div>
      {% endif %}
    </div>

    <!-- === Posledni notifikace === -->
    <div class="card">
      <div class="section-title">📱 Posledni notifikace</div>
      {% if notif_title %}
        <div class="notif-card">
          <div class="notif-icon">
            <svg viewBox="0 0 24 24" width="20" height="20" fill="white">
              <path d="M12 2.163c3.204 0 3.584.012 4.85.07 3.252.148 4.771 1.691 4.919 4.919.058 1.265.069 1.645.069 4.849 0 3.205-.012 3.584-.069 4.849-.149 3.225-1.664 4.771-4.919 4.919-1.266.058-1.644.07-4.85.07-3.204 0-3.584-.012-4.849-.07-3.26-.149-4.771-1.699-4.919-4.92-.058-1.265-.07-1.644-.07-4.849 0-3.204.013-3.583.07-4.849.149-3.227 1.664-4.771 4.919-4.919 1.266-.057 1.645-.069 4.849-.069zM12 0C8.741 0 8.333.014 7.053.072 2.695.272.273 2.69.073 7.052.014 8.333 0 8.741 0 12c0 3.259.014 3.668.072 4.948.2 4.358 2.618 6.78 6.98 6.98C8.333 23.986 8.741 24 12 24c3.259 0 3.668-.014 4.948-.072 4.354-.2 6.782-2.618 6.979-6.98.059-1.28.073-1.689.073-4.948 0-3.259-.014-3.667-.072-4.947-.196-4.354-2.617-6.78-6.979-6.98C15.668.014 15.259 0 12 0zm0 5.838a6.162 6.162 0 100 12.324 6.162 6.162 0 000-12.324zM12 16a4 4 0 110-8 4 4 0 010 8zm6.406-11.845a1.44 1.44 0 100 2.881 1.44 1.44 0 000-2.881z"/>
            </svg>
          </div>
          <div class="notif-content">
            <div class="notif-from">{{ notif_title }}</div>
            <div class="notif-msg">{{ notif_body }}</div>
          </div>
        </div>
      {% else %}
        <div class="notif-empty">Zatim zadna notifikace</div>
      {% endif %}
    </div>

    <p class="info">Arduino: <code>GET /api/arduino</code> · Notifikace: <code>POST /api/notification</code></p>
  </div>

  <script>
    const DAYS = ['Nedele','Pondeli','Utery','Streda','Ctvrtek','Patek','Sobota'];
    const MONTHS = ['ledna','unora','brezna','dubna','kvetna','cervna',
                    'cervence','srpna','zari','rijna','listopadu','prosince'];
    function tick() {
      const now = new Date();
      const hh = String(now.getHours()).padStart(2,'0');
      const mm = String(now.getMinutes()).padStart(2,'0');
      const ss = String(now.getSeconds()).padStart(2,'0');
      document.getElementById('clock').textContent = hh + ':' + mm + ':' + ss;
      document.getElementById('date-line').textContent =
        DAYS[now.getDay()] + ', ' + now.getDate() + '. ' + MONTHS[now.getMonth()] + ' ' + now.getFullYear();
    }
    tick();
    setInterval(tick, 1000);
  </script>
</body>
</html>
"""

# ------------- Routes -------------

@app.route("/")
def index():
    temp_str = f"{current_temperature} C" if current_temperature is not None else "nacitani..."
    return render_template_string(
        HTML_PAGE,
        temp=temp_str,
        quick_alarm=quick_alarm_time,
        weekly=weekly_alarms,
        day_keys=DAY_KEYS,
        day_names=DAY_NAMES,
        notif_title=notification_title,
        notif_body=notification_body,
        alarm_flash=request.args.get("alarm_flash"),
        weekly_flash=request.args.get("weekly_flash"),
    )


@app.route("/set_alarm", methods=["POST"])
def set_alarm():
    """Nastavi rychly (jednorazovy) budik."""
    global quick_alarm_time
    value = request.form.get("alarm", "").strip()
    if value:
        quick_alarm_time = value
        return redirect(f"/?alarm_flash=Budik+nastaven+na+{quick_alarm_time}")
    return redirect("/?alarm_flash=Neplatny+cas!")


@app.route("/set_weekly", methods=["POST"])
def set_weekly():
    """Ulozi tydenni rozvrh budiku."""
    global weekly_alarms
    for day in DAY_KEYS:
        t = request.form.get(f"time_{day}", "").strip()
        enabled = request.form.get(f"on_{day}") is not None
        weekly_alarms[day] = {"time": t, "enabled": enabled}
    print(f"[OK] Tydenni budiky aktualizovany: {weekly_alarms}")
    return redirect("/?weekly_flash=Rozvrh+ulozen!")


# ------------- Notification API (pro iPhone Shortcuts) -------------

@app.route("/api/notification", methods=["POST"])
def api_notification():
    """
    Prijima notifikace z iPhone Shortcuts.
    Podporuje JSON, form-data i plain text.
    """
    global notification_title, notification_body

    title = ""
    body = ""

    # Debug: vypis co prislo
    print(f"[NOTIF DEBUG] Content-Type: {request.content_type}")
    print(f"[NOTIF DEBUG] Data: {request.get_data(as_text=True)[:500]}")

    # 1) Zkusime JSON (force=True ignoruje Content-Type)
    try:
        data = request.get_json(force=True, silent=True)
        if data and isinstance(data, dict):
            # a) iPhone Shortcuts format: {"Instagram": "zprava text"}
            #    = klic je nazev appky, hodnota je zprava
            if "title" not in data and "Title" not in data and "body" not in data:
                # Vezmeme prvni klic-hodnotu
                first_key = next(iter(data))
                title = str(first_key)
                body = str(data[first_key])
            else:
                # b) Standardni format: {"title": "...", "body": "..."}
                title = str(data.get("title", data.get("Title", "")))
                body = str(data.get("body", data.get("Body",
                           data.get("message", data.get("Message",
                           data.get("text", data.get("Text", "")))))))
    except Exception:
        pass

    # 2) Zkusime form-data
    if not title and not body:
        title = request.form.get("title", request.form.get("Title", ""))
        body = request.form.get("body", request.form.get("Body",
               request.form.get("message", request.form.get("Message", ""))))

    # 3) Fallback: raw text jako body
    if not title and not body:
        raw = request.get_data(as_text=True).strip()
        if raw:
            body = raw
            title = "Notifikace"

    # Ulozime cokoliv co prislo
    if title or body:
        notification_title = (title or "Notifikace").strip()
        notification_body = (body or "").strip()
        print(f"[NOTIF OK] {notification_title}: {notification_body}")
        return jsonify({"status": "ok", "title": notification_title, "body": notification_body}), 200
    else:
        print("[NOTIF CHYBA] Prazdny request")
        return jsonify({"status": "ok", "message": "Prazdna notifikace, ale OK"}), 200


# ------------- Arduino API -------------

@app.route("/api/arduino")
def api_arduino():
    """
    Plain-text pro Arduino.
    Format: <TIME:HH:MM|TEMP:XX.X|ALARM:0|MSG:text>
    """
    now = datetime.now(PRAGUE_TZ)
    time_str = now.strftime("%H:%M")
    date_str = now.strftime("%d.%m.")
    temp_str = f"{round(current_temperature)}" if current_temperature is not None else "N/A"

    alarm_active = "1" if is_alarm_active() else "0"

    # Zprava = posledni notifikace, nebo "none"
    if notification_title and notification_body:
        msg = f"{notification_title}: {notification_body}"
    elif notification_body:
        msg = notification_body
    elif notification_title:
        msg = notification_title
    else:
        msg = "none"

    payload = f"<TIME:{time_str}|DATE:{date_str}|TEMP:{temp_str}|ALARM:{alarm_active}|MSG:{msg}>"
    print(f"[ARDUINO API] Odesílám: {payload}")
    return payload, 200, {"Content-Type": "text/plain; charset=utf-8"}


# ------------- JSON status (bonus) -------------

@app.route("/api/status")
def api_status():
    now = datetime.now(PRAGUE_TZ)
    return jsonify({
        "time": now.strftime("%H:%M:%S"),
        "date": now.strftime("%Y-%m-%d"),
        "temperature": current_temperature,
        "quick_alarm": quick_alarm_time or None,
        "weekly_alarms": weekly_alarms,
        "alarm_active": is_alarm_active(),
        "notification": {
            "title": notification_title or None,
            "body": notification_body or None,
        },
    })


# ------------- Spusteni -------------

if __name__ == "__main__":
    fetch_temperature()
    t = threading.Thread(target=temperature_worker, daemon=True)
    t.start()

    print("\n" + "=" * 50)
    print("  Multifunkcni digitalni hodiny v2")
    print("  Web:     http://0.0.0.0:5000")
    print("  Arduino: http://<IP>:5000/api/arduino")
    print("  Notif:   POST http://<IP>:5000/api/notification")
    print("=" * 50 + "\n")

    app.run(host="0.0.0.0", port=5000, debug=False)
