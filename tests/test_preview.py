import json,re,subprocess,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import preview
class PreviewTests(unittest.TestCase):
 def test_actual_firmware_pixels_equal_canvas(self):
  html=(ROOT/'preview.html').read_text(encoding='utf8')
  p=json.loads(re.search(r'const p=(.*?),fonts=',html).group(1))
  # Ensure a future renderer edit cannot silently leave the offline example stale.
  self.assertIn((ROOT/'tools/preview_canvas.js').read_text(),html)
  with tempfile.TemporaryDirectory() as td:
   td=Path(td);events=[];holidays=[];uid=0
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
   subprocess.run([sys.executable,str(ROOT/'tools/render_firmware_preview.py'),str(td/'sample.ics'),str(td/'native.ppm'),str(td/'holiday.ics')],check=True)
   subprocess.run(['node',str(ROOT/'tests/verify_preview.cjs'),str(ROOT/'preview.html'),str(td/'canvas.ppm')],check=True)
   self.assertEqual((td/'native.ppm').read_bytes(),(td/'canvas.ppm').read_bytes())
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
