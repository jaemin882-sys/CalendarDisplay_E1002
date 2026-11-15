import copy,json,subprocess,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from build_host import build_host

class WeatherTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.temp=tempfile.TemporaryDirectory();cls.build=Path(cls.temp.name)
  app=(ROOT/'src/firmware_app.inc').read_text(encoding='utf8')
  (cls.build/'weather_store_under_test.inc').write_text(app[app.index('void loadWeatherCache()'):app.index('void setup()')],encoding='utf8')
  cls.binary=build_host(cls.build,'host_weather.cpp',sanitize=True)
 @classmethod
 def tearDownClass(cls):cls.temp.cleanup()
 def test_storage_rollover_failure_hash_and_layout(self):subprocess.run([str(self.binary)],check=True)
 def test_api_validation_atomicity_and_missing_days(self):
  from datetime import date,timedelta
  valid={'timezone':'Asia/Seoul','utc_offset_seconds':32400,'daily_units':{'temperature_2m_min':'°C','temperature_2m_max':'°C'},
   'daily':{'time':[(date(2026,11,1)+timedelta(days=i)).isoformat() for i in range(16)],
   'temperature_2m_min':[-12.4]*16,'temperature_2m_max':[17.3]*16,'weather_code':[0,1,2,3,45,48,51,56,61,67,71,77,82,85,95,99]}}
  def run(payload,ok=True,raw=False):
   f=self.build/'response.json';f.write_text(payload if raw else json.dumps(payload),encoding='utf8')
   subprocess.run([str(self.binary),str(f),'1' if ok else '0'],check=True)
  run(valid)
  for field in ('temperature_2m_min','temperature_2m_max','weather_code'):
   p=copy.deepcopy(valid);p['daily'][field][5]=None;run(p)
   p=copy.deepcopy(valid);p['daily'][field].pop();run(p,False)
   p=copy.deepcopy(valid);p['daily'][field][0]='10';run(p,False)
  for path,value in [('time','2026-10-31'),('time','2026-11-02'),('time','2026-02-30'),('weather_code',4),('weather_code',1.5),('temperature_2m_min',101),('temperature_2m_max',-99)]:
   p=copy.deepcopy(valid);p['daily'][path][0]=value;run(p,False)
  for field,value in [('timezone','UTC'),('utc_offset_seconds',0),('error',True)]:
   p=copy.deepcopy(valid);p[field]=value;run(p,False)
  p=copy.deepcopy(valid);p['daily_units']['temperature_2m_min']='°F';run(p,False)
  p=copy.deepcopy(valid)
  for values in p['daily'].values():values.pop()
  run(p) # A shorter supported horizon is valid; no fake extra days.
  run(json.dumps(valid)[:-2],False,True);run('<html>upstream error</html>',False,True)

if __name__=='__main__':unittest.main()
