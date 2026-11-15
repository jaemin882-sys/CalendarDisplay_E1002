import copy,json,re,subprocess,sys,tempfile,unittest
from datetime import date,timedelta
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import preview
from build_host import build_host
from generate_preview import offline_html
class PreviewTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.temp=tempfile.TemporaryDirectory();cls.td=Path(cls.temp.name)
  cls.binary=build_host(cls.td)
 @classmethod
 def tearDownClass(cls):cls.temp.cleanup()
 def test_actual_firmware_pixels_equal_canvas(self):
  html=(ROOT/'preview.html').read_text(encoding='utf8')
  p=json.loads(re.search(r'const p=(.*?),fonts=',html).group(1))
  # Ensure a future renderer edit cannot silently leave the offline example stale.
  self.assertIn((ROOT/'tools/preview_canvas.js').read_text(encoding='utf8'),html)
  self.assertIn((ROOT/'tools/font_atlas.json').read_text(encoding='utf8'),html)
  self.compare_pixels(p)
 def test_weather_month_boundaries_holiday_and_three_events(self):
  for start,today in [('2026-10-18','2026-11-01'),('2026-11-15','2026-11-29')]:
   p={'start':start,'end':(date.fromisoformat(start)+timedelta(days=41)).isoformat(),'today':today,
      'memo':'온이네집','days':{},'holidays':{},'weather':{},'errors':[],'battery':85}
   codes=[0,1,2,3,45,48,51,56,61,67,71,77,82,85,95,99]
   for i,code in enumerate(codes):
    key=(date.fromisoformat(today)+timedelta(days=i)).isoformat()
    p['weather'][key]={'low':-120 if i%2 else 80,'high':-20 if i%2 else 170,'code':code}
    p['holidays'][key]=['매우긴공휴일이름']
    p['days'][key]=[{'text':s} for s in sorted(['첫번째 일정','두번째 일정','세번째 일정'])]
   # Recordless past and beyond day 16 stay empty; one saved past forecast stays visible.
   p['weather'][start]={'low':-1000,'high':-990,'code':71}
   self.compare_pixels(p)
   blank=copy.deepcopy(p);blank['weather']={}
   original=self.render(blank)
   actual=self.render(p)
   # Only cell headers may change; the three schedule lines and dividers are identical.
   for row in range(6):
    for y in range(50+row*71+19,50+(row+1)*71):
     self.assertEqual(actual[y*2400:(y+1)*2400],original[y*2400:(y+1)*2400])
 def render(self,p):
   td=self.td;events=[];holidays=[];uid=0
   for day,items in p['days'].items():
    for item in items:
     uid+=1;label=item['text'];start=day.replace('-','')
     if re.match(r'\d\d:\d\d ',label):start+='T'+label[:5].replace(':','')+'00';label=label[6:]
     events.append(f'BEGIN:VEVENT\nUID:{uid}\nDTSTART:{start}\nSUMMARY:{label}\nEND:VEVENT\n')
   for day,names in p['holidays'].items():
    for name in names:
     uid+=1;holidays.append(f'BEGIN:VEVENT\nUID:{uid}\nDTSTART:{day.replace("-","")}\nSUMMARY:{name}\nEND:VEVENT\n')
   for name,items in [('sample',events),('holiday',holidays)]:
    (td/f'{name}.ics').write_text('BEGIN:VCALENDAR\n'+''.join(items)+'END:VCALENDAR\n',encoding='utf8')
   epoch=date(1970,1,1)
   (td/'weather.txt').write_text('\n'.join(f'{(date.fromisoformat(k)-epoch).days} {v["low"]} {v["high"]} {v["code"]}' for k,v in p.get('weather',{}).items()),encoding='utf8')
   subprocess.run([str(self.binary),str(td/'sample.ics'),str(td/'native.ppm'),str(td/'holiday.ics'),p['start'].replace('-',''),p['today'].replace('-',''),str(td/'weather.txt'),str(p.get('battery',-1))],check=True)
   return (td/'native.ppm').read_bytes().split(b'\n',3)[3]
 def compare_pixels(self,p):
   actual=self.render(p)
   td=self.td;(td/'preview.html').write_text(offline_html(p),encoding='utf8')
   subprocess.run(['node',str(ROOT/'tests/verify_preview.cjs'),str(td/'preview.html'),str(td/'canvas.ppm')],check=True)
   expected=(td/'canvas.ppm').read_bytes().split(b'\n',3)[3]
   differences=[i//3 for i,(a,b) in enumerate(zip(actual,expected)) if a!=b]
   if differences:
    import shutil
    failed=ROOT/'.pio/preview-failure';failed.mkdir(parents=True,exist_ok=True)
    for name in ('native.ppm','canvas.ppm','sample.ics','holiday.ics','preview.html'):
     shutil.copyfile(td/name,failed/name)
   self.assertFalse(bool(differences),f'{len(set(differences))} differing pixels; first: {[(i%800,i//800) for i in differences[:10]]}')
 def test_python_parser_cases(self):
  from datetime import datetime,timezone,timedelta
  kst=timezone(timedelta(hours=9));start=datetime(2026,9,13,tzinfo=kst);end=datetime(2026,10,25,tzinfo=kst)
  from test_parser import calendar,event
  cases=[(calendar(event(start='20000101',extra='RRULE:FREQ=DAILY')),42),
   (calendar(event(start='20260916',extra='RRULE:FREQ=WEEKLY;INTERVAL=2;BYDAY=MO,WE;COUNT=5')),5),
   (calendar(event(start='20260112',extra='RRULE:FREQ=MONTHLY;BYDAY=2MO')),2),
   (calendar(event(extra='RRULE:FREQ=DAILY;COUNT=3'),event(start='20261002',extra='RECURRENCE-ID:20260930')),3)]
  for raw,count in cases:self.assertEqual(len(preview.parse_ics(raw,'test','black',start,end)),count)
if __name__=='__main__':unittest.main()
