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
KOREA_HOLIDAY_ICS = (
    "https://calendar.google.com/calendar/ical/"
    "ko.south_korea.official%23holiday%40group.v.calendar.google.com/public/basic.ics"
)


def cpp_string(name: str, text: str, default: str = "") -> str:
    m = re.search(rf'^\s*#define\s+{re.escape(name)}\s+"((?:\\.|[^"])*)"', text, re.M)
    if not m:
        return default
    value = m.group(1)
    return json.loads('"'+value+'"')


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

    feeds.append({
        "name": "공휴일",
        "url": KOREA_HOLIDAY_ICS,
        "color": "#d92828",
        "holiday": True,
    })

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
        data = resp.read(768*1024+1)
        if len(data)>768*1024: raise ValueError("ICS 768KiB 초과")
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
    zone = re.search(r'TZID="?([^;"\s]+)', prop)
    if zone and zone[1] not in {"Asia/Seoul", "Asia/Tokyo", "UTC", "Etc/UTC", "GMT"}:
        raise ValueError("지원하지 않는 TZID")
    tz = timezone.utc if value.endswith("Z") or (zone and zone[1] in {"UTC", "Etc/UTC", "GMT"}) else KST
    all_day = len(value) == 8
    fmt = "%Y%m%d" if all_day else "%Y%m%dT%H%M%S"
    dt = datetime.strptime(value.removesuffix("Z"), fmt).replace(tzinfo=tz)
    return dt, all_day


def expand_event(base, month_start, month_end):
    start, end = base["start"], base["end"]
    duration = max(end-start, timedelta(seconds=1))
    rule = dict(item.split("=",1) for item in base.get("rrule", "").split(";") if item)
    allowed = {"FREQ","INTERVAL","COUNT","UNTIL","BYDAY","BYMONTHDAY","BYMONTH","WKST"}
    if set(rule)-allowed:
        raise ValueError("지원하지 않는 RRULE 속성")
    freq=rule.get("FREQ")
    if freq and freq not in {"DAILY","WEEKLY","MONTHLY","YEARLY"}:
        raise ValueError("지원하지 않는 반복 주기")
    interval=max(1,int(rule.get("INTERVAL",1)))
    count=int(rule.get("COUNT",0))
    until=parse_ics_dt("",rule["UNTIL"])[0] if "UNTIL" in rule else None
    if until and len(rule["UNTIL"])==8: until+=timedelta(seconds=86399)
    weekdays={"SU":6,"MO":0,"TU":1,"WE":2,"TH":3,"FR":4,"SA":5}
    wkst=weekdays[rule.get("WKST","MO")]
    byday=rule.get("BYDAY","").split(",") if rule.get("BYDAY") else []
    for token in byday:
        if not re.fullmatch(r"[+-]?[0-9]*(MO|TU|WE|TH|FR|SA|SU)",token):raise ValueError("BYDAY 형식 오류")
        if len(token)>2 and (freq in {"DAILY","WEEKLY"} or (freq=="YEARLY" and "BYMONTH" not in rule)):
            raise ValueError("지원하지 않는 순서 지정 BYDAY")
    def number_matches(key, value, maximum):
        return key not in rule or value in [int(n) if int(n)>0 else maximum+int(n)+1 for n in rule[key].split(",")]
    out=[];generated=0
    initial=max(0,int((month_start-start-duration).total_seconds()//86400)-1) if not count and freq else 0
    for offset in range(initial,80000):
        cur=start+timedelta(days=offset)
        if cur>=month_end or (until and cur>until):break
        mdiff=(cur.year-start.year)*12+cur.month-start.month
        dim=calendar.monthrange(cur.year,cur.month)[1]
        match=not freq and offset==0
        if freq=="DAILY":match=offset%interval==0
        if freq=="WEEKLY":match=((offset+(start.weekday()-wkst)%7)//7)%interval==0 and (byday or cur.weekday()==start.weekday())
        if freq=="MONTHLY":match=mdiff%interval==0 and (byday or "BYMONTHDAY" in rule or cur.day==start.day)
        if freq=="YEARLY":match=(cur.year-start.year)%interval==0 and ("BYMONTH" in rule or byday or "BYMONTHDAY" in rule or cur.month==start.month) and (byday or "BYMONTHDAY" in rule or cur.day==start.day)
        def day_matches(token):
            ordinal=int(token[:-2]) if len(token)>2 else 0
            return cur.weekday()==weekdays[token[-2:]] and (not ordinal or ordinal==(cur.day-1)//7+1 or ordinal==-((dim-cur.day)//7+1))
        match=match and number_matches("BYMONTH",cur.month,12) and number_matches("BYMONTHDAY",cur.day,dim) and (not byday or any(day_matches(t) for t in byday))
        if offset==0:match=True
        if match:
            generated+=1
            if count and generated>count:break
            if cur not in base.get("exdates",set()) and cur+duration>month_start:
                out.append(dict(base,start=cur.astimezone(KST),end=(cur+duration).astimezone(KST)))
        if not freq:break
    return out


def parse_ics(text, cal_name, color, month_start, month_end, holiday=False):
    lines=unfold_ics(text.lstrip("\ufeff"))
    if "BEGIN:VCALENDAR" not in lines or "END:VCALENDAR" not in lines:
        raise ValueError("완전한 VCALENDAR가 아닙니다")
    records=[];current=None;nested=0
    for line in lines:
        if len(line.encode("utf8"))>4096:raise ValueError("ICS 행이 너무 깁니다")
        if line=="BEGIN:VEVENT":
            current={"summary":"(제목없음)","calendar":cal_name,"color":color,"holiday":holiday,"exdates":set()};nested=0;continue
        if line=="END:VEVENT":
            if current is None or not current.get("uid"):raise ValueError("VEVENT UID 누락")
            records.append(current);current=None;continue
        if current is None:continue
        if line.startswith("BEGIN:"):nested+=1;continue
        if line.startswith("END:"):nested-=1;continue
        if nested or ":" not in line:continue
        prop,value=line.split(":",1);key=prop.split(";",1)[0]
        if key=="DTSTART":current["start"],current["allDay"]=parse_ics_dt(prop,value)
        elif key=="DTEND":current["end"],_=parse_ics_dt(prop,value)
        elif key=="UID":current["uid"]=value
        elif key=="SUMMARY":current["summary"]=unescape_ics(value).replace("\n"," ")
        elif key=="RRULE":current["rrule"]=value
        elif key=="STATUS":current["cancelled"]=value=="CANCELLED"
        elif key=="EXDATE":current["exdates"].update(parse_ics_dt(prop,v)[0] for v in value.split(","))
        elif key=="RECURRENCE-ID":
            if "RANGE=" in prop:raise ValueError("RANGE 예외 미지원")
            current["recurrence"],_=parse_ics_dt(prop,value)
        elif key in {"RDATE","EXRULE","DURATION"}:raise ValueError(f"{key} 미지원")
    if current:raise ValueError("잘린 VEVENT")
    overrides={}
    for r in records:
        if "recurrence" in r:
            key=(r["uid"],r["recurrence"])
            if key in overrides:raise ValueError("중복 수정 일정")
            overrides[key]=r
    if len(overrides)>128:raise ValueError("수정 예외 128개 초과")
    out=[]
    for r in records:
        if r.get("cancelled"):continue
        if "start" not in r:raise ValueError("DTSTART 누락")
        r.setdefault("end",r["start"]+(timedelta(days=1) if r["allDay"] else timedelta(seconds=1)))
        if r["end"]<r["start"]:raise ValueError("잘못된 DTEND")
        if "recurrence" in r:r.pop("rrule",None)
        else:r["exdates"].update(date for uid,date in overrides if uid==r["uid"])
        out.extend(expand_event(r,month_start,month_end))
    if len(out)>512:raise ValueError("표시 범위 일정 512개 초과")
    return out


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
    for feed_index,feed in enumerate(cfg["feeds"]):
        try:
            ics = fetch_ics(feed["url"])
            parsed=parse_ics(ics,feed["name"],feed["color"],window_start,window_end,feed.get("holiday",False))
            for e in parsed: e["calendarIndex"]=feed_index
            all_events.extend(parsed)
        except Exception as exc:
            errors.append(f'{feed["name"]}: 다운로드/파싱 실패 ({type(exc).__name__})')

    by_day = {}
    holidays = {}
    d = window_start
    while d < window_end:
        key = d.date().isoformat()
        by_day[key] = []
        holidays[key] = []
        d += timedelta(days=1)

    for e in sorted(all_events, key=lambda x: (x["start"],not x["allDay"],x["calendarIndex"],x["summary"])):
        for key in event_days(e, window_start, window_end):
            if e.get("holiday"):
                holidays[key].append(e["summary"])
                continue

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
        "holidays": holidays,
        "errors": errors,
    }


def html_page(payload):
    data=json.dumps(payload,ensure_ascii=False).replace("</","<\\/")
    fonts=(ROOT/"tools/font_atlas.json").read_text(encoding="utf8")
    layout=dict((name,int(value)) for name,value in re.findall(r"int (UI_\w+)=(\d+);",(ROOT/"src/ui_layout.h").read_text()))
    script=(ROOT/"tools/preview_canvas.js").read_text(encoding="utf8")
    return f'''<!doctype html><html lang="ko"><meta charset="utf-8">
<title>E1002 펌웨어 비트맵 미리보기</title>
<style>body{{background:#eee;font:14px sans-serif;padding:24px}} main{{width:800px;margin:auto}}canvas{{background:white;box-shadow:0 2px 12px #bbb;image-rendering:pixelated}}p{{color:#555}}</style>
<main><p>800 × 480 · 이전 2주 + 현재 주 + 이후 3주 · 새로고침으로 다시 동기화</p>
<canvas width="800" height="480"></canvas><p id="message"></p></main>
<script>const p={data},fonts={fonts},layout={json.dumps(layout)};
{script}</script></html>'''


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
            body = "<meta charset='utf-8'><h2>미리보기 오류: 설정 및 네트워크를 확인하세요.</h2>".encode("utf-8")
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
        raise SystemExit(1)
