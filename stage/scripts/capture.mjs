import { chromium } from 'playwright';
import { spawn } from 'node:child_process';
import fs from 'node:fs';

const fps = 15;
const seconds = 20;
const totalFrames = fps * seconds;

fs.rmSync('output/frames', { recursive: true, force: true });
fs.mkdirSync('output/frames', { recursive: true });

const browser = await chromium.launch({ headless: true });
const page = await browser.newPage({ viewport: { width: 1280, height: 720 } });
page.on('console', message => console.log('[browser]', message.type(), message.text()));
page.on('pageerror', error => console.error('[browser error]', error));

await page.goto('http://127.0.0.1:4173/?capture=1', { waitUntil: 'networkidle' });
await page.waitForFunction(() => typeof window.__MASON_RENDER_AT__ === 'function');

for (let i = 0; i < totalFrames; i++) {
  const t = i / fps;
  await page.evaluate(time => window.__MASON_RENDER_AT__(time), t);
  await page.screenshot({
    path: `output/frames/frame-${String(i).padStart(4, '0')}.png`,
    type: 'png'
  });
  if ((i + 1) % fps === 0) console.log(`captured ${i + 1}/${totalFrames} frames`);
}
await browser.close();

const frameCount = fs.readdirSync('output/frames').filter(name => name.endsWith('.png')).length;
if (frameCount !== totalFrames) {
  throw new Error(`expected ${totalFrames} frames, found ${frameCount}`);
}

await new Promise((resolve, reject) => {
  const ff = spawn('ffmpeg', [
    '-y',
    '-framerate', String(fps),
    '-i', 'output/frames/frame-%04d.png',
    '-c:v', 'libx264',
    '-preset', 'medium',
    '-crf', '20',
    '-pix_fmt', 'yuv420p',
    '-movflags', '+faststart',
    'output/mason-stage.mp4'
  ], { stdio: 'inherit' });

  ff.on('error', reject);
  ff.on('exit', code => code === 0 ? resolve() : reject(new Error(`ffmpeg failed with exit code ${code}`)));
});
