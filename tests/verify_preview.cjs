const fs=require('fs'),vm=require('vm');
const pixels=Buffer.alloc(800*480*3,255);const colors={'#fff':[255,255,255],'#000':[0,0,0],'#f00':[255,0,0],'#00f':[0,0,255]};
const ctx={fillStyle:'#fff',fillRect(x,y,w,h){if(![x,y,w,h].every(Number.isInteger))throw Error('Non integer geometry');const c=colors[this.fillStyle];for(let j=y;j<y+h;j++)for(let i=x;i<x+w;i++)if(i>=0&&i<800&&j>=0&&j<480){let p=(j*800+i)*3;pixels[p]=c[0];pixels[p+1]=c[1];pixels[p+2]=c[2];}}};
const document={querySelector(s){return s==='canvas'?{getContext(){return ctx}}:{};}};
const html=fs.readFileSync(process.argv[2],'utf8'),js=html.match(/<script>([\s\S]*)<\/script>/)[1];
vm.runInNewContext(js,{document,console});fs.writeFileSync(process.argv[3],Buffer.concat([Buffer.from('P6\n800 480\n255\n'),pixels]));
