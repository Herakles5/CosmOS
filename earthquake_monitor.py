#!/usr/bin/env python3
"""
AION NEXUS - Autonomous Earthquake & Schumann Resonance Monitor
================================================================
Monitors live seismic events (M5.0 - M9.0+) and geomagnetic / Schumann resonance
disruptions based on Tobias' Earthquake App:
https://herakles5.github.io/earthquake-app/

Data Sources:
- USGS GeoJSON Feed (all_day)
- EMSC Seismic Portal API
- NOAA SWPC Planetary K-Index (Schumann Resonance / Gaia Charge)

Behaviors:
1. Detects new M5.0+ earthquakes worldwide and posts alerts to X & Facebook.
2. Calculates Gaia Charge Multiplier & Resonance Impact (Normal, Elevated, High Tension).
3. If Resonance Impact is ELEVATED or HIGH TENSION:
   - Attaches a critical resonance warning to earthquake alerts.
   - Emits standalone CRITICAL RESONANCE WARNINGS upon entering elevated states.
4. Always includes direct links to https://herakles5.github.io/earthquake-app/
"""

import os
import sys
import time
import json
import urllib.request
import urllib.error
from datetime import datetime, timezone

LOG_FILE = "/root/earthquake_monitor.log"
POSTED_IDS_FILE = "/root/earthquake_posted_ids.json"
RESONANCE_STATE_FILE = "/root/earthquake_resonance_state.json"
X_QUEUE_FILE = "/root/x_post_queue.json"
FB_QUEUE_FILE = "/root/fb_post_queue.json"
APP_URL = "https://herakles5.github.io/earthquake-app/"

def log(msg: str):
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    entry = f"[{timestamp}] [AION SEISMIC-MONITOR] {msg}"
    print(entry, flush=True)
    try:
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(entry + "\n")
    except Exception:
        pass

def load_posted_ids() -> set:
    if os.path.exists(POSTED_IDS_FILE):
        try:
            with open(POSTED_IDS_FILE, "r", encoding="utf-8") as f:
                return set(json.load(f))
        except Exception:
            pass
    return set()

def save_posted_ids(posted: set):
    try:
        # Keep recent 2000 IDs to avoid unbounded file growth
        data = list(posted)[-2000:]
        with open(POSTED_IDS_FILE, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
    except Exception as e:
        log(f"Fehler beim Speichern von {POSTED_IDS_FILE}: {e}")

def load_resonance_state() -> dict:
    if os.path.exists(RESONANCE_STATE_FILE):
        try:
            with open(RESONANCE_STATE_FILE, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            pass
    return {"last_impact": "Normal", "last_alert_time": 0}

def save_resonance_state(state: dict):
    try:
        with open(RESONANCE_STATE_FILE, "w", encoding="utf-8") as f:
            json.dump(state, f, indent=2)
    except Exception as e:
        log(f"Fehler beim Speichern von {RESONANCE_STATE_FILE}: {e}")

def push_to_queue(queue_file: str, item: dict):
    queue = []
    if os.path.exists(queue_file):
        try:
            with open(queue_file, "r", encoding="utf-8") as f:
                queue = json.load(f)
        except Exception:
            queue = []
    # Dringende Erdbeben- & Resonanz-Meldungen haben absolute Prioritaet (Vorne einfuegen)
    queue.insert(0, item)
    try:
        with open(queue_file, "w", encoding="utf-8") as f:
            json.dump(queue, f, indent=2, ensure_ascii=False)
    except Exception as e:
        log(f"Fehler beim Schreiben in {queue_file}: {e}")

def fetch_schumann_resonance():
    """
    Berechnet den Gaia Charge Multiplier und Resonance Impact Level
    analog zu EarthquakeApp/app.js aus dem NOAA Space Weather Kp-Index.
    """
    url = "https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json"
    kp_val = 1.0
    time_tag = ""
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "CosmOS-Earthquake-Monitor/1.0"})
        with urllib.request.urlopen(req, timeout=10) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            if data and len(data) > 0:
                last_entry = data[-1]
                kp_val = float(last_entry.get("Kp", 1.0))
                time_tag = last_entry.get("time_tag", "")
    except Exception as e:
        log(f"Hinweis beim Abruf von NOAA SWPC K-Index: {e}")

    # Exakte Formel aus EarthquakeApp app.js:
    if kp_val <= 3.0:
        charge = 0.79 + (kp_val / 3.0) * 0.21
    elif kp_val <= 5.0:
        charge = 1.0 + ((kp_val - 3.0) / 2.0) * 0.37
    else:
        charge = 1.37 + ((kp_val - 5.0) / 4.0) * 0.63

    if charge > 1.5:
        impact = "High Tension"
    elif charge > 1.1:
        impact = "Elevated"
    else:
        impact = "Normal"

    return {
        "kp": kp_val,
        "charge": charge,
        "impact": impact,
        "time_tag": time_tag
    }

def fetch_live_earthquakes():
    """
    Holt weltweite Erdbeben der letzten 24h von USGS und EMSC.
    Filtert strikt auf M5.0 bis M9.9.
    """
    raw_quakes = []
    
    # 1. USGS GeoJSON Feed (all_day)
    try:
        url = "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/all_day.geojson"
        req = urllib.request.Request(url, headers={"User-Agent": "CosmOS-Earthquake-Monitor/1.0"})
        with urllib.request.urlopen(req, timeout=12) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            for f in data.get("features", []):
                p = f.get("properties", {})
                mag = p.get("mag")
                if mag is not None and mag >= 5.0:
                    coords = f.get("geometry", {}).get("coordinates", [0, 0, 0])
                    raw_quakes.append({
                        "id": f.get("id"),
                        "mag": round(float(mag), 1),
                        "place": p.get("place", "Unbekannte Region"),
                        "lon": coords[0],
                        "lat": coords[1],
                        "depth": round(coords[2] if len(coords) > 2 else 0, 1),
                        "time": p.get("time", int(time.time() * 1000)),
                        "source": "USGS"
                    })
    except Exception as e:
        log(f"USGS Fetch Fehler: {e}")

    # 2. EMSC FDSNWS
    try:
        url = "https://www.seismicportal.eu/fdsnws/event/1/query?format=json&minmag=5.0&limit=50"
        req = urllib.request.Request(url, headers={"User-Agent": "CosmOS-Earthquake-Monitor/1.0"})
        with urllib.request.urlopen(req, timeout=12) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            for f in data.get("features", []):
                p = f.get("properties", {})
                mag = p.get("mag")
                if mag is not None and mag >= 5.0:
                    coords = f.get("geometry", {}).get("coordinates", [0, 0, 0])
                    raw_quakes.append({
                        "id": p.get("unid") or f.get("id"),
                        "mag": round(float(mag), 1),
                        "place": p.get("flynn_region") or "EMSC Region",
                        "lon": coords[0],
                        "lat": coords[1],
                        "depth": round(float(p.get("depth", coords[2] if len(coords) > 2 else 0)), 1),
                        "time": int(datetime.fromisoformat(p.get("time").replace("Z", "+00:00")).timestamp() * 1000),
                        "source": "EMSC"
                    })
    except Exception as e:
        log(f"EMSC Fetch Fehler: {e}")

    # Sortiere nach Aktualitaet (neueste zuerst)
    raw_quakes.sort(key=lambda q: q["time"], reverse=True)

    # Duplikatsbereinigung nach Zeit & Distanz (< 5 min & < 2 Grad)
    deduped = []
    for q in raw_quakes:
        is_dup = False
        for d in deduped:
            time_diff = abs(q["time"] - d["time"])
            coord_diff = ((q["lat"] - d["lat"])**2 + (q["lon"] - d["lon"])**2)**0.5
            if time_diff < 300000 and coord_diff < 2.0:
                is_dup = True
                if q["mag"] > d["mag"]:
                    d["mag"] = q["mag"]
                break
        if not is_dup:
            # Nur letzte 24h
            if (time.time() * 1000) - q["time"] < 86400000:
                deduped.append(q)

    return deduped

def format_earthquake_post_x(eq: dict, sr: dict) -> str:
    """
    Formatiert Tweet fuer X strikt unter 280 Zeichen.
    """
    time_str = datetime.fromtimestamp(eq["time"] / 1000, tz=timezone.utc).strftime("%H:%M UTC")
    is_elevated = sr["impact"] in ["Elevated", "High Tension"]
    
    header = "🚨 CRITICAL SEISMIC ALERT" if (eq["mag"] >= 6.5 or is_elevated) else "⚠️ SEISMIC ALERT"
    
    place = eq["place"]
    if len(place) > 40:
        place = place[:37] + "..."

    warn_line = ""
    if is_elevated:
        warn_line = f"\n⚡ KRITISCHE WARNUNG: Resonanz {sr['impact'].upper()} ({sr['charge']:.2f}x)!"

    # Template
    msg = (
        f"{header} [M{eq['mag']}]\n"
        f"📍 {place}\n"
        f"Depth: {eq['depth']}km | {time_str}\n"
        f"Resonance Impact: {sr['impact']} (Kp: {sr['kp']:.1f}){warn_line}\n"
        f"Live: {APP_URL}\n"
        f"#Earthquake #Erdbeben #CosmOS"
    )
    
    if len(msg) > 280:
        msg = (
            f"{header} [M{eq['mag']}]\n"
            f"📍 {place}\n"
            f"Depth: {eq['depth']}km | {time_str}\n"
            f"Impact: {sr['impact']}{warn_line}\n"
            f"{APP_URL}\n"
            f"#Earthquake"
        )
    return msg[:280]

def format_earthquake_post_fb(eq: dict, sr: dict) -> str:
    """
    Formatiert ausfuehrlichen Facebook-Beitrag mit vollstaendigem wissenschaftlichem Kontext.
    """
    time_str = datetime.fromtimestamp(eq["time"] / 1000, tz=timezone.utc).strftime("%d.%m.%Y um %H:%M UTC")
    is_elevated = sr["impact"] in ["Elevated", "High Tension"]

    warn_block = ""
    if is_elevated:
        warn_block = (
            f"🚨 KRITISCHE RESONANZ-WARNUNG (GAIA MATRIX) 🚨\n"
            f"Der aktuelle Resonanz-Impact befindet sich im Status: {sr['impact'].upper()}!\n"
            f"Gaia Charge Multiplier: {sr['charge']:.2f}x (Kp-Index: {sr['kp']:.2f}).\n"
            f"Erhöhte elektromagnetische Feldspannung und geomagnetische Störungen im "
            f"Schumann-Resonanzraum der Erde beschleunigen statistisch die tektonische Entladung!\n\n"
        )

    msg = (
        f"🌍 SEISMISCHER EILBERICHT [M{eq['mag']}] – CosmOS Earthquake Monitor\n"
        f"━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n"
        f"📍 Region: {eq['place']}\n"
        f"💥 Magnitude: M{eq['mag']} ({eq['source']})\n"
        f"📏 Herdtiefe: {eq['depth']} km\n"
        f"⏰ Zeitpunkt: {time_str}\n"
        f"🌐 Koordinaten: {eq['lat']:.2f}° N/S, {eq['lon']:.2f}° E/W\n\n"
        f"📊 GEOPHYSIKALISCHER STATUS:\n"
        f"• Schumann Resonance / Gaia Charge: {sr['charge']:.2f}x\n"
        f"• Geomagnetischer Kp-Index: {sr['kp']:.2f}\n"
        f"• Resonanz-Impact: {sr['impact']}\n\n"
        f"{warn_block}"
        f"🔎 Live-Tracking, 3D Gaia Matrix & Taktungsrhythmen:\n"
        f"{APP_URL}\n\n"
        f"#Earthquake #Erdbeben #SeismicAlert #EarthquakeApp #GaiaMatrix #SchumannResonance #CosmOS #AionNEXUS"
    )
    return msg

def format_resonance_alert_x(sr: dict) -> str:
    msg = (
        f"🚨 KRITISCHE RESONANZ-WARNUNG 🚨\n"
        f"Schumann / Space Weather Impact: {sr['impact'].upper()}\n"
        f"Gaia Charge Multiplier: {sr['charge']:.2f}x | Kp: {sr['kp']:.2f}\n"
        f"⚡ Erhöhte geomagnetische Feldspannung detektiert. Akute Erhöhung tektonischer Entladungsrisiken (M5.0+)!\n"
        f"Live-Monitor: {APP_URL}\n"
        f"#SchumannResonance #GaiaMatrix #CosmOS"
    )
    return msg[:280]

def format_resonance_alert_fb(sr: dict) -> str:
    msg = (
        f"🚨 KRITISCHE RESONANZ-WARNUNG (NOAA SWPC & GAIA MATRIX) 🚨\n"
        f"━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n"
        f"Der geomagnetische Resonanz-Impact der Erde hat soeben ein erhöhtes Niveau erreicht:\n\n"
        f"⚡ Status: {sr['impact'].upper()}\n"
        f"📈 Gaia Charge Multiplier: {sr['charge']:.2f}x\n"
        f"🧲 Kp-Index (Planetary K): {sr['kp']:.2f}\n\n"
        f"WISSENSCHAFTLICHER HINTERGRUND:\n"
        f"Wenn das elektromagnetische Feld der Erde und der Schumann-Resonanzraum "
        f"gestört werden, baut sich intensive energetische Spannung auf. "
        f"Im Rhythmus-Modell der Gaia Matrix beschleunigt der Multiplikator von {sr['charge']:.2f}x "
        f"die Poisson-Wahrscheinlichkeiten für signifikante Beben (M5.0 - M9.0+) weltweit.\n\n"
        f"📊 Verfolge die aktuellen Echtzeit-Daten und tektonischen Aktivitätskurven live auf:\n"
        f"{APP_URL}\n\n"
        f"#SchumannResonance #GaiaMatrix #SpaceWeather #GeomagneticStorm #EarthquakeApp #CosmOS #AionNEXUS"
    )
    return msg

def check_and_publish():
    sr = fetch_schumann_resonance()
    log(f"Schumann-Resonanz Status: Kp={sr['kp']:.2f}, GaiaCharge={sr['charge']:.2f}x, Impact={sr['impact']}")

    # 1. Pruefe Resonanz-Impact Uebergang (Kritische Warnung sobald ELEVATED / HIGH TENSION)
    res_state = load_resonance_state()
    current_impact = sr["impact"]
    last_impact = res_state.get("last_impact", "Normal")
    last_alert_time = res_state.get("last_alert_time", 0)
    now = time.time()

    # Sende kritische Warnung wenn neu in ELEVATED / HIGH TENSION oder alle 6h bei dauerhaftem Hochstand
    if current_impact in ["Elevated", "High Tension"]:
        if last_impact == "Normal" or (now - last_alert_time > 21600):
            log(f"🚨 Ausloesen einer KRITISCHEN RESONANZ-WARNUNG! Status: {current_impact}")
            x_msg = format_resonance_alert_x(sr)
            fb_msg = format_resonance_alert_fb(sr)
            push_to_queue(X_QUEUE_FILE, {"type": "ResonanceAlert", "message": x_msg, "created_at": now})
            push_to_queue(FB_QUEUE_FILE, {"type": "ResonanceAlert", "message": fb_msg, "created_at": now})
            res_state["last_alert_time"] = now

    res_state["last_impact"] = current_impact
    save_resonance_state(res_state)

    # 2. Pruefe Erdbeben M5.0+
    posted_ids = load_posted_ids()
    quakes = fetch_live_earthquakes()
    log(f"Gefundene M5.0+ Erdbeben in letzten 24h: {len(quakes)}")

    new_posts = 0
    # Bei Erstinitialisierung nur das allerneueste Beben posten um Spam zu vermeiden
    unposted_quakes = [q for q in quakes if q["id"] not in posted_ids]
    if len(posted_ids) == 0 and len(unposted_quakes) > 1:
        # Markiere aeltere bereits als gesehen
        for q in unposted_quakes[1:]:
            posted_ids.add(q["id"])
        unposted_quakes = unposted_quakes[:1]

    for q in unposted_quakes:
        log(f"Neues M5.0+ Erdbeben detektiert: M{q['mag']} - {q['place']} ({q['id']})")
        x_post = format_earthquake_post_x(q, sr)
        fb_post = format_earthquake_post_fb(q, sr)

        push_to_queue(X_QUEUE_FILE, {"type": "EarthquakeAlert", "quake_id": q["id"], "message": x_post, "created_at": now})
        push_to_queue(FB_QUEUE_FILE, {"type": "EarthquakeAlert", "quake_id": q["id"], "message": fb_post, "created_at": now})

        posted_ids.add(q["id"])
        new_posts += 1

    save_posted_ids(posted_ids)
    if new_posts > 0:
        log(f"{new_posts} neue Erdbeben-Meldungen in X- & FB-Live-Queues eingereiht.")

def main():
    log("=== AION EARTHQUAKE & RESONANCE MONITOR GESTARTET ===")
    log(f"Ueberwachende App: {APP_URL}")
    log("Schwellenwert: M5.0 bis M9.9+ | Kritische Resonanz: Elevated / High Tension")

    if "--once" in sys.argv:
        check_and_publish()
        return

    interval = 60
    while True:
        try:
            check_and_publish()
        except Exception as e:
            log(f"Unerwarteter Fehler im Monitor-Zyklus: {e}")
        time.sleep(interval)

if __name__ == "__main__":
    main()
