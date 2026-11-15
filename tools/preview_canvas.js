// Bitmap glyphs generated from the same pinned U8g2 fonts as the firmware.
const canvas=document.querySelector('canvas'),ctx=canvas.getContext('2d');
const black='#000',red='#f00',blue='#00f';
ctx.fillStyle='#fff';ctx.fillRect(0,0,800,480);
const compact=s=>s.replace(/[ \t\r\n\u00a0\u3000]/g,'');
function glyph(c,font='body'){return fonts[font][c.codePointAt(0)]||fonts[font][63];}
function width(s,font='body'){const chars=Array.from(s);return chars.reduce((n,c,i)=>n+glyph(c,font)[font==='body'||i===chars.length-1?0:33],0);}
function text(s,x,y,color=black,font='body'){
 ctx.fillStyle=color;
 for(const c of s){const g=glyph(c,font);for(let row=0;row<32;row++)for(let col=0;col<24;col++)if(g[row+1]&(1<<col))ctx.fillRect(x+col,y-24+row,1,1);x+=g[font==='body'?0:33];}
}
const eventAdvance=c=>Math.max(1,Math.floor((glyph(c)[0]*14+8)/16)-1);
function eventWidth(s){return Array.from(s).reduce((n,c)=>n+eventAdvance(c),0);}
function eventText(s,x,y,color=black){
 ctx.fillStyle=color;
 for(const c of s){const g=glyph(c);for(let row=0;row<32;row++)for(let col=0;col<Math.min(24,g[0]);col++)if(g[row+1]&(1<<col))ctx.fillRect(x+Math.floor(col*14/16),y+Math.trunc((row-24)*14/16),1,1);x+=eventAdvance(c);}
}
function take(s,w){let out='',used=0;for(const c of s){const n=glyph(c)[0];if(used+n>w)break;out+=c;used+=n;}return out;}
function fit(s,w){if(width(s)<=w)return s;return take(s,w-24)+'...';}
function takeEvent(s,w){let out='',used=0;for(const c of s){const n=eventAdvance(c);if(used+n>w)break;out+=c;used+=n;}return out;}
function fitEvent(s,w){if(eventWidth(s)<=w)return s;return takeEvent(s,w-18)+'...';}
function rect(x,y,w,h,color){ctx.fillStyle=color;ctx.fillRect(x,y,w,1);ctx.fillRect(x,y+h-1,w,1);ctx.fillRect(x,y,1,h);ctx.fillRect(x+w-1,y,1,h);}
function pixel(x,y,color){ctx.fillStyle=color;ctx.fillRect(x,y,1,1);}
function line(x,y,x1,y1,color){
 const steep=Math.abs(y1-y)>Math.abs(x1-x);if(steep){[x,y]=[y,x];[x1,y1]=[y1,x1];}
 if(x>x1){[x,x1]=[x1,x];[y,y1]=[y1,y];}
 const dx=x1-x,dy=Math.abs(y1-y),step=y<y1?1:-1;let err=Math.floor(dx/2);
 for(;x<=x1;x++){pixel(steep?y:x,steep?x:y,color);err-=dy;if(err<0){y+=step;err+=dx;}}
}
function circle(x0,y0,r,color){
 ctx.fillStyle=color;ctx.fillRect(x0,y0-r,1,2*r+1);
 let f=1-r,dx=1,dy=-2*r,x=0,y=r,px=x,py=y;
 while(x<y){
  if(f>=0){y--;dy+=2;f+=dy;}x++;dx+=2;f+=dx;
  if(x<y+1){ctx.fillRect(x0+x,y0-y,1,2*y+1);ctx.fillRect(x0-x,y0-y,1,2*y+1);}
  if(y!==py){ctx.fillRect(x0+py,y0-px,1,2*px+1);ctx.fillRect(x0-py,y0-px,1,2*px+1);py=y;}px=x;
 }
}
function weatherKind(code){
 if([71,73,75,77,85,86].includes(code))return 'snow';
 if([51,53,55,56,57,61,63,65,66,67,80,81,82,95,96,97,99].includes(code))return 'rain';
 return code===0?'sun':[1,2].includes(code)?'partly':'cloud';
}
function weatherIcon(x,y,kind){
 if(kind==='snow'){
  [[6,1,6,11],[1,3,11,9],[1,9,11,3],[4,1,6,3],[6,3,8,1],[4,11,6,9],[6,9,8,11]].forEach(([a,b,c,d])=>line(x+a,y+b,x+c,y+d,blue));return;
 }
 if(kind==='sun'||kind==='partly'){
  const cx=x+(kind==='sun'?6:4),cy=y+4;circle(cx,cy,3,black);circle(cx,cy,2,'#ff0');
  [[0,-4],[0,4],[-4,0],[4,0],[-3,-3],[3,-3],[-3,3],[3,3]].forEach(([a,b])=>pixel(cx+a,cy+b,black));
  if(kind==='sun')return;
 }
 circle(x+3,y+6,2,black);circle(x+7,y+5,3,black);circle(x+10,y+7,2,black);
 ctx.fillStyle=black;ctx.fillRect(x+3,y+6,8,4);
 if(kind==='rain')for(let i=0;i<3;i++)line(x+3+i*4,y+10,x+2+i*4,y+12,blue);
}
const degrees=t=>(t<0?-1:1)*Math.floor((Math.abs(t)+5)/10);
function dayWeather(right,y,available,w){
 const label=`${degrees(w.low)}/${degrees(w.high)}°`,extra=layout.UI_WEATHER_ICON+layout.UI_WEATHER_GAP;
 let font='more';if(width(label,font)+extra>available)font='weatherSmall';if(width(label,font)+extra>available)font='weatherTiny';
 const left=right-width(label,font)-extra;
 text(label,right-width(label,font),y+14,black,font);weatherIcon(left,y+4,weatherKind(w.code));return left;
}
function holidayHeader(label,x,y,w){
 if(w<=0)return;
 if(eventWidth(label)<=w){eventText(label,x,y,red);return;}
 if(w<5)return;
 const prefix=takeEvent(label,w-6);eventText(prefix,x,y,red);
 for(let i=0;i<3;i++)pixel(x+eventWidth(prefix)+i*2,y,red);
}
const left=layout.UI_LEFT,right=layout.UI_RIGHT,cw=(right-left)/7,top=layout.UI_GRID_TOP+layout.UI_HEADER_H,rh=(layout.UI_GRID_BOTTOM-top)/6;
const title=p.start.slice(5).replace('-','.')+' - '+p.end.slice(5).replace('-','.');
text(title,10,23,black,'title');let tw=width(title,'title'),memoX=10+tw+12;
if(280-memoX>0)text(fit(p.memo,280-memoX),memoX,23);
const reminders=(p.reminders||[]).filter(r=>r.days>=0).slice(0,2).map(r=>r.label+' '+(r.days===0?'D-DAY':'D-'+r.days));
let reminderX=284+Math.trunc((232-reminders.reduce((n,s)=>n+width(s),0)-(reminders.length>1?12:0))/2);
for(const s of reminders){text(s,reminderX,23);reminderX+=width(s)+12;}
if(p.battery>=0){const s=p.battery+'%';text(s,790-width(s,'day'),23,black,'day');}
['SUN','MON','TUE','WED','THU','FRI','SAT'].forEach((d,c)=>text(d,left+c*cw+(cw-width(d,'day'))/2|0,layout.UI_GRID_TOP+15,c===0?red:c===6?blue:black,'day'));
ctx.fillStyle=black;
for(let c=0;c<=7;c++)ctx.fillRect(left+c*cw,top,1,layout.UI_GRID_BOTTOM-top+1);
for(let r=0;r<=6;r++)ctx.fillRect(left,top+r*rh,right-left+1,1);
for(let slot=0;slot<42;slot++){
 let date=new Date(p.start+'T00:00:00Z');date.setUTCDate(date.getUTCDate()+slot);
 const key=date.toISOString().slice(0,10),col=slot%7,x=left+col*cw,y=top+Math.floor(slot/7)*rh;
 if(slot>0&&date.getUTCDate()===1){ctx.fillStyle=black;if(col===0)ctx.fillRect(left,y-1,right-left+1,3);else ctx.fillRect(x-1,y,3,rh+1);}
 if(key===p.today){rect(x+2,y+2,cw-4,rh-4,red);rect(x+3,y+3,cw-6,rh-6,red);}
 const hs=(p.holidays[key]||[]).join('·'),day=date.getUTCDate(),ds=(day===1||slot===0)?`${date.getUTCMonth()+1}/${day}`:String(day);
 text(ds,x+5,y+15,hs||col===0?red:col===6?blue:black,'day');
 let headerRight=x+cw-layout.UI_WEATHER_RIGHT,hx=x+5+width(ds,'day')+layout.UI_HEADER_GAP;
 if(p.weather&&p.weather[key])headerRight=dayWeather(headerRight,y,headerRight-hx,p.weather[key])-layout.UI_HEADER_GAP;
 if(hs)holidayHeader(hs,hx,y+15,headerRight-hx);
 const es=p.days[key]||[],shown=Math.min(es.length,3),hidden=Math.max(0,es.length-3),more='+'+hidden,mw=hidden?width(more,'more')+5:0;
 let line=0;
 for(let i=0;i<shown;i++){
  const label=compact(es[i].text);let lines=es.length===1?3:1;
  if(es.length===2){let firstNeedsTwo=width(compact(es[0].text))>cw-10;lines=(i===0?firstNeedsTwo:!firstNeedsTwo)?2:1;}
  let rest=label;
  for(let part=0;part<lines&&rest.length;part++){
   const w=cw-10-(line===2?mw:0),piece=takeEvent(rest,w),draw=part===lines-1?fitEvent(rest,w):piece;
   eventText(draw,x+5,y+layout.UI_EVENT_BASELINE+line*layout.UI_EVENT_STEP);line++;rest=rest.slice(piece.length);
  }
 }
 if(hidden)text(more,x+cw-mw,y+65,black,'more');
}
if(p.errors.length)circle(780,16,3,red);
document.querySelector('#message').textContent=[...p.errors,p.weather_error||''].filter(Boolean).join(' / ')||'펌웨어와 동일한 비트맵 폰트·픽셀 배치. 실제 패널의 색감과 refresh는 기기에서 확인하세요.';
