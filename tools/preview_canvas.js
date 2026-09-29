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
const eventAdvance=c=>Math.max(1,glyph(c)[0]-2);
function eventWidth(s){return Array.from(s).reduce((n,c)=>n+eventAdvance(c),0);}
function eventText(s,x,y,color=black){
 ctx.fillStyle=color;
 for(const c of s){const g=glyph(c);for(let row=0;row<32;row++)for(let col=0;col<24;col++)if(g[row+1]&(1<<col))ctx.fillRect(x+col,y-24+row,1,1);x+=eventAdvance(c);}
}
function take(s,w){let out='',used=0;for(const c of s){const n=glyph(c)[0];if(used+n>w)break;out+=c;used+=n;}return out;}
function fit(s,w){if(width(s)<=w)return s;return take(s,w-24)+'...';}
function takeEvent(s,w){let out='',used=0;for(const c of s){const n=eventAdvance(c);if(used+n>w)break;out+=c;used+=n;}return out;}
function fitEvent(s,w){if(eventWidth(s)<=w)return s;return takeEvent(s,w-18)+'...';}
function rect(x,y,w,h,color){ctx.fillStyle=color;ctx.fillRect(x,y,w,1);ctx.fillRect(x,y+h-1,w,1);ctx.fillRect(x,y,1,h);ctx.fillRect(x+w-1,y,1,h);}
const left=layout.UI_LEFT,right=layout.UI_RIGHT,cw=(right-left)/7,top=layout.UI_GRID_TOP+layout.UI_HEADER_H,rh=(layout.UI_GRID_BOTTOM-top)/6;
const title=p.start.slice(5).replace('-','.')+' - '+p.end.slice(5).replace('-','.');
text(title,10,23,black,'title');let tw=width(title,'title');text(fit(p.memo,740-tw-18),10+tw+18,23);
['SUN','MON','TUE','WED','THU','FRI','SAT'].forEach((d,c)=>text(d,left+c*cw+(cw-width(d,'day'))/2|0,layout.UI_GRID_TOP+15,c===0?red:c===6?blue:black,'day'));
ctx.fillStyle=black;
for(let c=0;c<=7;c++)ctx.fillRect(left+c*cw,top,1,layout.UI_GRID_BOTTOM-top+1);
for(let r=0;r<=6;r++)ctx.fillRect(left,top+r*rh,right-left+1,1);
for(let slot=0;slot<42;slot++){
 let date=new Date(p.start+'T00:00:00Z');date.setUTCDate(date.getUTCDate()+slot);
 const key=date.toISOString().slice(0,10),col=slot%7,x=left+col*cw,y=top+Math.floor(slot/7)*rh;
 if(key===p.today){rect(x+2,y+2,cw-4,rh-4,red);rect(x+3,y+3,cw-6,rh-6,red);}
 const hs=(p.holidays[key]||[]).join('·'),day=date.getUTCDate(),ds=(day===1||slot===0)?`${date.getUTCMonth()+1}/${day}`:String(day);
 text(ds,x+5,y+15,hs||col===0?red:col===6?blue:black,'day');
 if(hs){let hx=x+5+width(ds,'day')+4;text(fit(hs,cw-(hx-x)-5),hx,y+15,red);}
 const es=p.days[key]||[],shown=Math.min(es.length,3),hidden=Math.max(0,es.length-3),more='+'+hidden,mw=hidden?width(more,'more')+5:0;
 let line=0;
 for(let i=0;i<shown;i++){
  const label=compact(es[i].text);let lines=es.length===1?3:1;
  if(es.length===2){let firstNeedsTwo=eventWidth(compact(es[0].text))>cw-10;lines=(i===0?firstNeedsTwo:!firstNeedsTwo)?2:1;}
  let rest=label;
  for(let part=0;part<lines&&rest.length;part++){
   const w=cw-10-(line===2?mw:0),piece=takeEvent(rest,w),draw=part===lines-1?fitEvent(rest,w):piece;
   eventText(draw,x+5,y+layout.UI_EVENT_BASELINE+line*layout.UI_EVENT_STEP);line++;rest=rest.slice(piece.length);
  }
 }
 if(hidden)text(more,x+cw-mw,y+65,black,'more');
}
if(p.errors.length){ctx.fillStyle=red;ctx.beginPath();ctx.arc(780,16,3,0,Math.PI*2);ctx.fill();}
document.querySelector('#message').textContent=p.errors.length?p.errors.join(' / '):'펌웨어와 동일한 비트맵 폰트·픽셀 배치. 실제 패널의 색감과 refresh는 기기에서 확인하세요.';
