import { chromium } from 'playwright';
import { spawn } from 'node:child_process';
import fs from 'node:fs';

fs.mkdirSync('output/frames',{recursive:true});
const browser=await chromium.launch({headless:true});
const page=await browser.newPage({viewport:{width:1280,height:720}});
await page.goto('http://127.0.0.1:4173',{waitUntil:'networkidle'});
for(let i=0;i<300;i++){
  await page.screenshot({path:`output/frames/frame-${String(i).padStart(4,'0')}.png`});
  await page.waitForTimeout(1000/15);
}
await browser.close();
await new Promise((resolve,reject)=>{
 const ff=spawn('ffmpeg',['-y','-framerate','15','-i','output/frames/frame-%04d.png','-c:v','libx264','-pix_fmt','yuv420p','-crf','20','output/mason-stage.mp4'],{stdio:'inherit'});
 ff.on('exit',c=>c===0?resolve():reject(new Error('ffmpeg failed '+c)));
});
