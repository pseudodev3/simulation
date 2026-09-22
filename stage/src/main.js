import * as THREE from 'three';

const root=document.querySelector('#app');
const scene=new THREE.Scene();
scene.background=new THREE.Color(0x9bc9df);
scene.fog=new THREE.Fog(0x9bc9df,35,85);

const camera=new THREE.PerspectiveCamera(38,innerWidth/innerHeight,.1,160);
camera.position.set(20,19,26);
camera.lookAt(0,0,0);

const renderer=new THREE.WebGLRenderer({antialias:true,preserveDrawingBuffer:true});
renderer.setPixelRatio(Math.min(devicePixelRatio,2));
renderer.setSize(innerWidth,innerHeight);
renderer.shadowMap.enabled=true;
renderer.shadowMap.type=THREE.PCFSoftShadowMap;
renderer.outputColorSpace=THREE.SRGBColorSpace;
renderer.toneMapping=THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure=1.1;
root.appendChild(renderer.domElement);

scene.add(new THREE.HemisphereLight(0xfff3d4,0x42536b,2.1));
const sun=new THREE.DirectionalLight(0xffe0a0,3.2);sun.position.set(-18,28,12);sun.castShadow=true;sun.shadow.mapSize.set(2048,2048);scene.add(sun);

const ground=new THREE.Mesh(new THREE.PlaneGeometry(80,55),new THREE.MeshStandardMaterial({color:0x79976a,roughness:1}));
ground.rotation.x=-Math.PI/2;ground.receiveShadow=true;scene.add(ground);
function box(x,y,z,color,px,py,pz){const m=new THREE.Mesh(new THREE.BoxGeometry(x,y,z),new THREE.MeshStandardMaterial({color,roughness:.88}));m.position.set(px,py,pz);m.castShadow=m.receiveShadow=true;scene.add(m);return m}
box(66,.18,8,0x555b62,0,.10,0); box(7,.2,48,0x555b62,7,.11,0);
box(66,.08,1.5,0xc9c1aa,0,.21,-4.8);box(66,.08,1.5,0xc9c1aa,0,.21,4.8);box(1.5,.08,48,0xc9c1aa,3,.21,0);box(1.5,.08,48,0xc9c1aa,11,.21,0);
for(let x=-27;x<30;x+=5)box(2.3,.025,.16,0xe8d99d,x,.23,0);
for(let z=-20;z<22;z+=5)box(.16,.025,2.3,0xe8d99d,7,.23,z);

function building(x,z,w,d,h,wall,roof){
 const body=box(w,h,d,wall,x,h/2,z);
 const r=new THREE.Mesh(new THREE.ConeGeometry(Math.max(w,d)*.72,2.2,4),new THREE.MeshStandardMaterial({color:roof,roughness:1}));
 r.position.set(x,h+1,z);r.rotation.y=Math.PI/4;r.castShadow=true;scene.add(r);
 box(w*.22,h*.48,.16,0x573d2b,x,h*.24,z+d/2+.09);
 for(const wx of [-w*.24,w*.24]) box(w*.2,h*.2,.12,0xb8e7f1,x+wx,h*.58,z+d/2+.1);
}
const houseColors=[0xd98f70,0xe4c174,0x91b6a2,0xb59bc5];
for(let i=0;i<6;i++)building(-26+i*9,-12,6,6,4.2,houseColors[i%4],0x704b43);
for(let i=0;i<4;i++)building(-26+i*9,13,6,6,4.2,houseColors[(i+2)%4],0x704b43);
building(19,-14,10,7,5.4,0xb45d4c,0x5d4140);building(29,-14,8,7,4.8,0xd1aa62,0x5d4140);
building(20,14,10,7,4.6,0x7193a7,0x4b5961);building(31,14,8,7,4.3,0x8eaa70,0x4b5961);

for(const [x,z] of [[-14,7],[-5,-7],[15,7],[25,7],[-22,-4],[34,2]]){
 const trunk=box(.45,2,.45,0x765338,x,1,z);
 const crown=new THREE.Mesh(new THREE.SphereGeometry(1.7,8,7),new THREE.MeshStandardMaterial({color:0x527c4d,roughness:1}));crown.position.set(x,3,z);crown.castShadow=true;scene.add(crown);
}

const palette=[0xf0a36d,0x5d91c6,0xd66f75,0x6ca77b,0xc69bd3,0xe3bd58,0x7c6ac2,0x53aeb0];
const people=[];
function person(i,x,z){
 const g=new THREE.Group();
 const body=new THREE.Mesh(new THREE.CapsuleGeometry(.42,.95,4,8),new THREE.MeshStandardMaterial({color:palette[i%palette.length],roughness:.8}));
 body.position.y=1.15;body.castShadow=true;g.add(body);
 const head=new THREE.Mesh(new THREE.SphereGeometry(.42,12,10),new THREE.MeshStandardMaterial({color:[0x6d402c,0x9a6545,0xc58a63,0x4b2c22][i%4],roughness:.9}));
 head.position.y=2.08;head.castShadow=true;g.add(head);
 const hair=new THREE.Mesh(new THREE.SphereGeometry(.44,10,6,0,Math.PI*2,0,Math.PI*.55),new THREE.MeshStandardMaterial({color:[0x251b18,0x3b241b,0x1c1716][i%3]}));hair.position.y=2.18;g.add(hair);
 const legMat=new THREE.MeshStandardMaterial({color:0x343944,roughness:.9});
 for(const sx of [-.19,.19]){const leg=new THREE.Mesh(new THREE.BoxGeometry(.22,.72,.25),legMat);leg.position.set(sx,.4,0);leg.castShadow=true;g.add(leg)}
 g.position.set(x,0,z);scene.add(g);people.push({g,phase:i*.7,speed:.45+i*.025,lane:i%2?3.8:-3.8});
}
for(let i=0;i<12;i++)person(i,-29+i*2.4,i%2?3.8:-3.8);

const clock=new THREE.Clock();
function renderAt(t){
 people.forEach((p,i)=>{
   const x=((t*p.speed+i*4+60)%60)-30;p.g.position.x=x;p.g.position.z=p.lane+Math.sin(t*.55+p.phase)*.12;
   p.g.position.y=Math.abs(Math.sin(t*5*p.speed+p.phase))*.06;
   p.g.rotation.y=Math.PI/2;
 });
 const cycle=(Math.sin(t*.045)+1)/2;
 scene.background.setHSL(.56,.42,.18+cycle*.48);scene.fog.color.copy(scene.background);
 sun.intensity=.35+cycle*3.1;sun.position.x=Math.cos(t*.045)*25;sun.position.y=5+cycle*25;
 const hero=people[0].g.position;
 if(t<7){
   camera.position.set(20+Math.sin(t*.09)*4,19,26+Math.cos(t*.075)*3);
   camera.lookAt(1,1.4,0);
 }else if(t<15){
   const follow=(t-7)/8;
   const targetX=hero.x+6.5;
   const targetZ=hero.z+9.5;
   camera.position.x=THREE.MathUtils.lerp(20,targetX,Math.min(1,follow*1.8));
   camera.position.y=THREE.MathUtils.lerp(19,6.8,Math.min(1,follow*1.8));
   camera.position.z=THREE.MathUtils.lerp(26,targetZ,Math.min(1,follow*1.8));
   camera.lookAt(hero.x+2.2,1.5,hero.z);
 }else{
   const out=Math.min(1,(t-15)/3);
   camera.position.x=THREE.MathUtils.lerp(hero.x+6.5,18,out);
   camera.position.y=THREE.MathUtils.lerp(6.8,17,out);
   camera.position.z=THREE.MathUtils.lerp(hero.z+9.5,24,out);
   camera.lookAt(2,1.3,0);
 }
 renderer.render(scene,camera);
}
window.__MASON_RENDER_AT__=renderAt;
const captureMode=new URLSearchParams(location.search).has('capture');
function animate(){
 requestAnimationFrame(animate);
 renderAt(clock.getElapsedTime());
}
if(captureMode) renderAt(0); else animate();
addEventListener('resize',()=>{camera.aspect=innerWidth/innerHeight;camera.updateProjectionMatrix();renderer.setSize(innerWidth,innerHeight);if(captureMode)renderAt(0)});
