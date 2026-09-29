"""Compile and exercise the actual firmware parser, not a Python reimplementation."""
import os, subprocess, tempfile, unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BIN=Path(tempfile.gettempdir())/'e1002_parser_test'
def event(uid='a', start='20260929', end=None, extra='', summary='일정'):
    return f'BEGIN:VEVENT\nUID:{uid}\nDTSTART:{start}\n'+(f'DTEND:{end}\n' if end else '')+f'SUMMARY:{summary}\n{extra}\nEND:VEVENT\n'
def calendar(*events):return 'BEGIN:VCALENDAR\nVERSION:2.0\n'+''.join(events)+'END:VCALENDAR\n'
class ParserTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-fsanitize=address,undefined','-g',str(ROOT/'tests/host_parser.cpp'),'-o',str(BIN)],check=True)
 def parse(self,raw,success=True,ws='20260913',we='20261025'):
  with tempfile.NamedTemporaryFile('w',suffix='.ics',encoding='utf8') as f:
   f.write(raw);f.flush();r=subprocess.run([str(BIN),f.name,ws,we],capture_output=True,text=True)
  self.assertEqual(r.returncode,0 if success else 1,r.stderr)
  return [x.split('\t') for x in r.stdout.splitlines()]
 def test_folding_escape_alarm(self):
  rows=self.parse(calendar(event(summary='세탁기\\, 정수기\n 이전설치',extra='BEGIN:VALARM\nSUMMARY:alarm\nEND:VALARM')))
  self.assertEqual(rows[0][2],'세탁기, 정수기이전설치')
 def test_history_does_not_consume_capacity(self):
  rows=self.parse(calendar(*(event(str(i),'20200101') for i in range(600)),event()))
  self.assertEqual(len(rows),1)
 def test_old_daily_count_exdate(self):
  self.assertEqual(len(self.parse(calendar(event(start='20000101',extra='RRULE:FREQ=DAILY')))),42)
  self.assertEqual(len(self.parse(calendar(event(start='20260913',extra='RRULE:FREQ=DAILY;COUNT=4\nEXDATE:20260914')))),3)
 def test_weekly_interval_wkst(self):
  rows=self.parse(calendar(event(start='20260916',extra='RRULE:FREQ=WEEKLY;INTERVAL=2;BYDAY=MO,WE;COUNT=5')))
  self.assertEqual(len(rows),5)
  # Sep 16, 28, 30, Oct 12, 14. Original implementation incorrectly included Sep 21.
  self.assertEqual(int(rows[1][0])-int(rows[0][0]),12*86400)
 def test_monthly_ordinal_and_negative(self):
  self.assertEqual(len(self.parse(calendar(event(start='20260112',extra='RRULE:FREQ=MONTHLY;BYDAY=2MO')))),2)
  self.assertEqual(len(self.parse(calendar(event(start='20260131',extra='RRULE:FREQ=MONTHLY;BYMONTHDAY=-1')))),1)
 def test_until_and_multiday_overlap(self):
  self.assertEqual(len(self.parse(calendar(event(start='20260910',end='20260915',extra='RRULE:FREQ=DAILY;UNTIL=20260913')))),4)
 def test_override_move_and_cancel_both_orders(self):
  master=event(extra='RRULE:FREQ=DAILY;COUNT=3')
  moved=event(start='20261002',extra='RECURRENCE-ID:20260930',summary='변경')
  cancel='BEGIN:VEVENT\nUID:a\nRECURRENCE-ID:20261001\nSTATUS:CANCELLED\nEND:VEVENT\n'
  for items in [(master,moved,cancel),(cancel,moved,master)]:
   rows=self.parse(calendar(*items));self.assertEqual([r[2] for r in rows],['일정','변경'])
 def test_utc_exdate(self):
  self.assertEqual(len(self.parse(calendar(event(start='20260928T150000Z',extra='RRULE:FREQ=DAILY;COUNT=2\nEXDATE;TZID=Asia/Seoul:20260930T000000')))),1)
 def test_invalid_and_unsupported_atomic(self):
  for bad in ['<html>error</html>',calendar(event()).replace('END:VCALENDAR',''),calendar(event(start='20260230')),calendar(event(extra='RRULE:FREQ=MONTHLY;BYSETPOS=-1')),
              calendar(event()).replace('DTSTART:','DTSTART;TZID=America/New_York:')]:
   self.parse(bad,False)
 def test_valid_empty_and_cancelled(self):
  self.assertEqual(self.parse(calendar()),[])
  self.assertEqual(self.parse(calendar(event(extra='STATUS:CANCELLED'))),[])
 def test_leap_and_month_end(self):
  self.assertEqual(len(self.parse(calendar(event(start='20240131',extra='RRULE:FREQ=MONTHLY')),ws='20240201',we='20240401')),1)
  self.assertEqual(len(self.parse(calendar(event(start='20240229',extra='RRULE:FREQ=YEARLY')),ws='20250201',we='20250301')),0)
 def test_capacity_failure(self):self.parse(calendar(*(event(str(i)) for i in range(513))),False)
if __name__=='__main__':unittest.main()
