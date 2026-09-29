from __future__ import annotations

from pathlib import Path
from datetime import datetime, timedelta, timezone
from http.server import BaseHTTPRequestHandler, HTTPServer
import calendar
import json
import re
import threading
import urllib.request
import webbrowser

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "src" / "config.h"
PORT = 8765
KST = timezone(timedelta(hours=9))

COLORS = ["#d92828", "#2455d6", "#3f8f45", "#dc7c25"]


def cpp_string(name: str, text: str, default: str = "") -> str:
    m = re.search(rf'^\s*#define\s+{re.escape(name)}\s+"((?:\\.|[^"])*)"', text, re.M)
    if not m:
        return default
    value = m.group(1)
    return bytes(value, "utf-8").decode("unicode_escape") if "\\u" in value else value.replace(r"\"", '"').replace(r"\\", "\\")


def load_config():
    if not CONFIG.exists():
        raise RuntimeError("src/config.h 가 없습니다. 먼저 python tools/setup_config.py 를 실행하세요.")

    text = CONFIG.read_text(encoding="utf-8")
    feeds = []
    for i in range(1, 5):
        name = cpp_string(f"CALENDAR_{i}_NAME", text, f"캘린더 {i}")
        url = cpp_string(f"CALENDAR_{i}_URL", text, "").strip()
        if url:
            feeds.append({"name": name or f"캘린더 {i}", "url": url, "color": COLORS[(i - 1) % len(COLORS)]})

    if not feeds:
        raise RuntimeError("config.h 에 캘린더 주소가 없습니다.")

    return {
        "memo": cpp_string("CUSTOM_MEMO_TEXT", text, ""),
        "feeds": feeds,
    }


def fetch_ics(url: str) -> str:
    if url.startswith("webcal://"):
        url = "https://" + url[len("webcal://"):]
    req = urllib.request.Request(
        url,
        headers={
            "User-Agent": "CalendarDisplay-E1002-Preview/0.1",
            "Accept": "text/calendar,*/*",
        },
    )
    with urllib.request.urlopen(req, timeout=20) as resp:
        data = resp.read()
    return data.decode("utf-8", errors="replace")


def unfold_ics(text: str) -> list[str]:
    raw = text.replace("\r\n", "\n").replace("\r", "\n").split("\n")
    out: list[str] = []
    for line in raw:
        if line.startswith((" ", "\t")) and out:
            out[-1] += line[1:]
        else:
            out.append(line)
    return out


def unescape_ics(value: str) -> str:
    return (
        value.replace(r"\N", "\n")
        .replace(r"\n", "\n")
        .replace(r"\,", ",")
        .replace(r"\;", ";")
        .replace(r"\\", "\\")
    )


def parse_ics_dt(prop: str, value: str):
    params = prop.split(";")[1:]
    all_day = any(p.upper() == "VALUE=DATE" for p in params) or (len(value) == 8 and "T" not in value)

    if all_day:
        dt = datetime.strptime(value[:8], "%Y%m%d").replace(tzinfo=KST)
        return dt, True

    v = value.strip()
    if v.endswith("Z"):
        dt = datetime.strptime(v, "%Y%m%dT%H%M%SZ").replace(tzinfo=timezone.utc).astimezone(KST)
    else:
        fmt = "%Y%m%dT%H%M%S" if len(v) >= 15 else "%Y%m%dT%H%M"
        dt = datetime.strptime(v[:15] if fmt.endswith("%S") else v[:13], fmt).replace(tzinfo=KST)
    return dt, False


def parse_until(value: str):
    try:
        if value.endswith("Z"):
            return datetime.strptime(value, "%Y%m%dT%H%M%SZ").replace(tzinfo=timezone.utc).astimezone(KST)
        if "T" in value:
            return datetime.strptime(value[:15], "%Y%m%dT%H%M%S").replace(tzinfo=KST)
        return datetime.strptime(value[:8], "%Y%m%d").replace(tzinfo=KST)
    except Exception:
        return None


def parse_rule(rule: str) -> dict:
    parts = {}
    for item in rule.split(";"):
        if "=" in item:
            k, v = item.split("=", 1)
            parts[k.upper()] = v
    return parts


def add_months(dt: datetime, months: int) -> datetime:
    y = dt.year + (dt.month - 1 + months) // 12
    m = (dt.month - 1 + months) % 12 + 1
    d = min(dt.day, calendar.monthrange(y, m)[1])
    return dt.replace(year=y, month=m, day=d)


def add_years(dt: datetime, years: int) -> datetime:
    y = dt.year + years
    d = min(dt.day, calendar.monthrange(y, dt.month)[1])
    return dt.replace(year=y, day=d)


DAYMAP = {"MO": 0, "TU": 1, "WE": 2, "TH": 3, "FR": 4, "SA": 5, "SU": 6}


def expand_event(base: dict, month_start: datetime, month_end: datetime):
    start = base["start"]
    end = base["end"]
    duration = max(end - start, timedelta(seconds=1))
    rule = base.get("rrule")
    exclusions = base.get("exdates", set())

    def emit(s):
        key = s.strftime("%Y%m%d")
        key2 = s.strftime("%Y%m%dT%H%M%S")
        if key in exclusions or key2 in exclusions:
            return None
        e = s + duration
        if s < month_end and e > month_start:
            item = dict(base)
            item["start"] = s
            item["end"] = e
            return item
        return None

    if not rule:
        one = emit(start)
        return [one] if one else []

    r = parse_rule(rule)
    freq = r.get("FREQ", "").upper()
    interval = max(int(r.get("INTERVAL", "1") or "1"), 1)
    count_limit = int(r["COUNT"]) if r.get("COUNT", "").isdigit() else None
    until = parse_until(r["UNTIL"]) if r.get("UNTIL") else None
    out = []
    generated = 0

    def allowed(s):
        return (until is None or s <= until) and (count_limit is None or generated < count_limit)

    if freq == "WEEKLY" and r.get("BYDAY"):
        wanted = [DAYMAP[d] for d in r["BYDAY"].split(",") if d in DAYMAP]
        week0 = start - timedelta(days=start.weekday())
        wk = 0
        while wk < 600:
            base_week = week0 + timedelta(weeks=wk * interval)
            if base_week >= month_end and base_week > start:
                break
            for wd in wanted:
                s = base_week + timedelta(days=wd)
                s = s.replace(hour=start.hour, minute=start.minute, second=start.second, microsecond=0)
                if s < start:
                    continue
                if not allowed(s):
                    return out
                generated += 1
                item = emit(s)
                if item:
                    out.append(item)
            wk += 1
        return out

    cur = start
    guard = 0
    while guard < 5000 and allowed(cur):
        generated += 1
        item = emit(cur)
        if item:
            out.append(item)

        if freq == "DAILY":
            cur += timedelta(days=interval)
        elif freq == "WEEKLY":
            cur += timedelta(weeks=interval)
        elif freq == "MONTHLY":
            cur = add_months(cur, interval)
        elif freq == "YEARLY":
            cur = add_years(cur, interval)
        else:
            break

        if cur >= month_end and cur > start and freq in {"DAILY", "WEEKLY", "MONTHLY", "YEARLY"}:
            if freq in {"DAILY", "WEEKLY"}:
                break
            if cur > month_end + timedelta(days=370):
                break
        guard += 1

    return out


def parse_ics(text: str, cal_name: str, color: str, month_start: datetime, month_end: datetime):
    lines = unfold_ics(text)
    events = []
    current = None

    for line in lines:
        if line == "BEGIN:VEVENT":
            current = {"summary": "(제목 없음)", "calendar": cal_name, "color": color, "exdates": set()}
            continue
        if line == "END:VEVENT":
            if current and current.get("start"):
                if "end" not in current:
                    current["end"] = current["start"] + (timedelta(days=1) if current.get("allDay") else timedelta(hours=1))
                events.extend(expand_event(current, month_start, month_end))
            current = None
            continue
        if current is None or ":" not in line:
            continue

        prop, value = line.split(":", 1)
        key = prop.split(";", 1)[0].upper()

        try:
            if key == "DTSTART":
                current["start"], current["allDay"] = parse_ics_dt(prop, value)
            elif key == "DTEND":
                current["end"], _ = parse_ics_dt(prop, value)
            elif key == "SUMMARY":
                current["summary"] = unescape_ics(value)
            elif key == "RRULE":
                current["rrule"] = value
            elif key == "EXDATE":
                for v in value.split(","):
                    v = v.strip()
                    if v:
                        current["exdates"].add(v.rstrip("Z"))
            elif key == "STATUS" and value.upper() == "CANCELLED":
                current["cancelled"] = True
        except Exception:
            pass

    return [e for e in events if not e.get("cancelled")]


def event_days(event, window_start, window_end):
    start = event["start"]
    end = event["end"]
    if event.get("allDay"):
        end = end - timedelta(seconds=1)
    else:
        end = max(end - timedelta(seconds=1), start)

    first = max(start.date(), window_start.date())
    last = min(end.date(), (window_end - timedelta(seconds=1)).date())
    d = first
    while d <= last:
        yield d.isoformat()
        d += timedelta(days=1)


def build_payload():
    cfg = load_config()
    now = datetime.now(KST)

    # Sunday-based 6-week rolling view:
    # previous 2 full weeks + current week + next 3 full weeks.
    days_since_sunday = (now.weekday() + 1) % 7
    this_sunday = datetime(now.year, now.month, now.day, tzinfo=KST) - timedelta(days=days_since_sunday)
    window_start = this_sunday - timedelta(weeks=2)
    window_end = window_start + timedelta(weeks=6)

    all_events = []
    errors = []
    for feed in cfg["feeds"]:
        try:
            ics = fetch_ics(feed["url"])
            all_events.extend(parse_ics(ics, feed["name"], feed["color"], window_start, window_end))
        except Exception as exc:
            errors.append(f'{feed["name"]}: {exc}')

    by_day = {}
    d = window_start
    while d < window_end:
        by_day[d.date().isoformat()] = []
        d += timedelta(days=1)

    for e in sorted(all_events, key=lambda x: x["start"]):
        for key in event_days(e, window_start, window_end):
            if e.get("allDay"):
                label = e["summary"]
            else:
                label = e["start"].strftime("%H:%M ") + e["summary"]
            by_day[key].append({
                "text": label,
                "color": e["color"],
                "calendar": e["calendar"],
            })

    last_day = window_end - timedelta(days=1)
    return {
        "start": window_start.date().isoformat(),
        "end": last_day.date().isoformat(),
        "today": now.date().isoformat(),
        "memo": cfg["memo"],
        "days": by_day,
        "errors": errors,
    }


def html_page(payload):
    data = json.dumps(payload, ensure_ascii=False).replace("</", "<\\/")
    return f"""<!doctype html>
<html lang="ko">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>E1002 실제 캘린더 미리보기</title>
<style>
:root{{--black:#111;--white:#fff;--red:#d92828;--blue:#2455d6;--green:#3f8f45;--line:#222;--bg:#ececec}}
*{{box-sizing:border-box}}
body{{margin:0;padding:26px;background:var(--bg);font-family:"Noto Sans KR","Malgun Gothic","Apple SD Gothic Neo",sans-serif;color:#111}}
.top{{width:800px;margin:0 auto 12px;display:flex;justify-content:space-between;align-items:center;font-size:13px}}
.screen{{width:800px;height:480px;margin:auto;background:white;box-shadow:0 3px 16px rgba(0,0,0,.18);position:relative;overflow:hidden}}
.title{{position:absolute;left:18px;top:8px;font:700 25px/1 Arial,sans-serif}}
.memo{{position:absolute;left:180px;top:10px;width:565px;font-size:16px;line-height:24px;white-space:nowrap;overflow:hidden}}
.status{{position:absolute;right:25px;top:19px;width:10px;height:10px;border-radius:50%;background:var(--green)}}
.status.bad{{background:var(--red)}}
.weekdays{{position:absolute;left:16px;top:86px;width:768px;height:24px;display:grid;grid-template-columns:repeat(7,1fr);align-items:center;text-align:center;font-size:14px}}
.weekdays div:first-child{{color:var(--red)}} .weekdays div:last-child{{color:var(--blue)}}
.grid{{position:absolute;left:16px;top:110px;width:768px;height:360px;display:grid;grid-template-columns:repeat(7,1fr);grid-template-rows:repeat(6,60px);border-left:1px solid var(--line);border-top:1px solid var(--line)}}
.cell{{position:relative;border-right:1px solid var(--line);border-bottom:1px solid var(--line);padding:3px 4px;overflow:hidden;background:#fff}}
.cell.today:after{{content:"";position:absolute;inset:2px;border:2px solid var(--red);pointer-events:none}}
.day{{height:17px;font:700 14px/16px Arial,sans-serif}} .sun .day{{color:var(--red)}} .sat .day{{color:var(--blue)}}
.event{{height:15px;line-height:15px;font-size:11px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;padding-left:10px;position:relative}}
.event:before{{content:"";position:absolute;left:1px;top:5px;width:5px;height:5px;background:var(--c,#111)}}
.more{{position:absolute;left:14px;bottom:2px;font:11px/12px Arial,sans-serif}}
.msg{{width:800px;margin:10px auto 0;font-size:12px;color:#555}} .err{{color:#a51616}}
</style>
</head>
<body>
<div class="top"><b>실제 iCloud 일정 미리보기</b><span>브라우저 새로고침 = iCloud 다시 불러오기</span></div>
<div class="screen">
  <div class="title" id="title"></div><div class="memo" id="memo"></div>
  <div class="status" id="status"></div>
  <div class="weekdays"><div>일</div><div>월</div><div>화</div><div>수</div><div>목</div><div>금</div><div>토</div></div>
  <div class="grid" id="grid"></div>
</div>
<div class="msg" id="msg"></div>
<script>
const p={data};
const start=new Date(p.start+"T00:00:00");
const end=new Date(p.end+"T00:00:00");
const mmdd=d=>String(d.getMonth()+1).padStart(2,"0")+"."+String(d.getDate()).padStart(2,"0");
document.getElementById("title").textContent=mmdd(start)+" - "+mmdd(end);
document.getElementById("memo").textContent=p.memo;
if(p.errors.length) document.getElementById("status").classList.add("bad");
document.getElementById("msg").innerHTML=p.errors.length
 ? '<span class="err">일부 캘린더 불러오기 실패: '+p.errors.map(x=>x.replace(/[<>&]/g,"")).join(" / ")+'</span>'
 : '현재 주 기준 이전 2주 + 현재 주 + 이후 3주를 표시합니다. 공개 iCloud 주소는 PC의 src/config.h에서만 읽습니다.';
const grid=document.getElementById("grid");
for(let slot=0;slot<42;slot++){{
  const cell=document.createElement("div"), col=slot%7;
  cell.className="cell "+(col===0?"sun ":"")+(col===6?"sat ":"");
  const d=new Date(start); d.setDate(start.getDate()+slot);
  const key=d.getFullYear()+"-"+String(d.getMonth()+1).padStart(2,"0")+"-"+String(d.getDate()).padStart(2,"0");
  if(key===p.today) cell.classList.add("today");
  const dn=document.createElement("div"); dn.className="day";
  dn.textContent=(d.getDate()===1||slot===0)?(d.getMonth()+1)+"/"+d.getDate():d.getDate();
  cell.appendChild(dn);
  const es=p.days[key]||[];
  es.slice(0,3).forEach(e=>{{
    const r=document.createElement("div"); r.className="event"; r.style.setProperty("--c",e.color); r.textContent=e.text; r.title=e.calendar+" · "+e.text; cell.appendChild(r);
  }});
  if(es.length>3){{const more=document.createElement("div");more.className="more";more.textContent="+"+(es.length-3);cell.appendChild(more);}}
  grid.appendChild(cell);
}}
</script>
</body></html>"""


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        try:
            payload = build_payload()
            body = html_page(payload).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        except Exception as exc:
            body = f"<meta charset='utf-8'><h2>미리보기 오류</h2><pre>{str(exc)}</pre>".encode("utf-8")
            self.send_response(500)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

    def log_message(self, format, *args):
        pass


def main():
    print("E1002 실제 iCloud 캘린더 미리보기")
    print(f"config: {CONFIG}")
    print("iCloud 일정을 불러오는 중입니다...")
    # First fetch here so errors show in the terminal before opening the browser.
    payload = build_payload()
    if payload["errors"]:
        print("일부 캘린더 오류:")
        for e in payload["errors"]:
            print(" -", e)
    else:
        print("iCloud 일정 불러오기 성공")

    server = HTTPServer(("127.0.0.1", PORT), Handler)
    url = f"http://127.0.0.1:{PORT}/"
    print(f"브라우저 열기: {url}")
    print("종료하려면 이 터미널에서 Ctrl+C")
    threading.Timer(0.5, lambda: webbrowser.open(url)).start()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n미리보기 종료")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print()
        print("오류:", exc)
        input("엔터를 누르면 종료합니다...")
