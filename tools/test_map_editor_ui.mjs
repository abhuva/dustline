// DOM integration check. Install test dependency: npm install --prefix build/map-editor-ui --no-save --package-lock=false jsdom@26.1.0
import {JSDOM} from '../build/map-editor-ui/node_modules/jsdom/lib/api.js';
import {readFile} from 'node:fs/promises';
import assert from 'node:assert/strict';
import {compile,makeNode,presets,MAX_NODES,LUT_MAX_POINTS,defaultLutColor,normalizeSpawnProfiles,normalizePlacements} from './map_editor/recipe.mjs';
const read = path => readFile(new URL('../'+path,import.meta.url),'utf8');
const art=JSON.parse(await read('tools/map_editor/generated/art.json'));
const schema=JSON.parse(await read('tools/map_editor/schema.json'));
const original=JSON.parse(await read('maps/recipes/wasteland.json'));
let mapLibrary=JSON.parse(await read('maps/map-library.json')),libraryRevision='test-revision';
const dom=new JSDOM(await read('tools/map_editor/index.html'),{url:'http://localhost:8765',runScripts:'outside-only',pretendToBeVisual:true});
const w=dom.window;
// jsdom has no native PointerEvent handler properties; bridge to its event dispatcher.
Object.defineProperty(w.HTMLElement.prototype,'onpointerdown',{set(fn){if(this._pointerDown)this.removeEventListener('pointerdown',this._pointerDown);this._pointerDown=fn;this.addEventListener('pointerdown',fn);},get(){return this._pointerDown;}});
Object.assign(w,{structuredClone,compile,makeNode,presets,MAX_NODES,LUT_MAX_POINTS,defaultLutColor,normalizeSpawnProfiles,normalizePlacements,Worker:class {constructor(){w.testWorker=this;}postMessage(data){w.lastProgram=data;}},fetch:async(url,options={})=>{
  const path=String(url);
  if(path.includes('schema'))return {ok:true,status:200,json:async()=>structuredClone(schema)};
  if(path.includes('art.json'))return {ok:true,status:200,json:async()=>structuredClone(art)};
  if(path==='/api/shop-preview'&&options.method==='POST'){
    const request=JSON.parse(options.body),count=request.shopProfile.stockSize;
    return {ok:true,status:200,json:async()=>({profile:structuredClone(request.shopProfile),towns:Array.from({length:6},(_,index)=>({index,name:`TOWN ${index}`,tierCap:request.shopProfile.townTierRange[1],items:Array.from({length:count},(_item,saveId)=>({saveId,id:`item_${saveId}`,name:`ITEM ${saveId}`,family:'front',tier:1}))}))})};
  }
  if(path==='/api/library'&&options.method==='PUT'){
    const request=JSON.parse(options.body);assert.equal(request.revision,libraryRevision);mapLibrary=structuredClone(request.library);libraryRevision+='x';
    return {ok:true,status:200,json:async()=>({library:structuredClone(mapLibrary),revision:libraryRevision})};
  }
  if(path==='/api/library')return {ok:true,status:200,json:async()=>({library:structuredClone(mapLibrary),revision:libraryRevision})};
  return {ok:true,status:200,json:async()=>structuredClone(original)};
}});
w.HTMLElement.prototype.scrollTo=()=>{};
w.HTMLDialogElement.prototype.showModal=function(){this.setAttribute('open','');};
w.HTMLDialogElement.prototype.close=function(){this.removeAttribute('open');};
w.HTMLAnchorElement.prototype.click=function(){};
w.Blob=Blob;w.URL.createObjectURL=blob=>{w.lastBlob=blob;return 'blob:test';};w.URL.revokeObjectURL=()=>{};
w.confirm=()=>true;w.prompt=(_message,value)=>value;
w.document.addEventListener('click',e=>{if(e.target.tagName==='A')e.preventDefault();});
w.HTMLCanvasElement.prototype.getContext=()=>({fillRect(){},drawImage(){},fillText(){},strokeRect(){},beginPath(){},moveTo(){},lineTo(){},arc(){},fill(){},stroke(){},save(){},restore(){},putImageData(){},createImageData:(width,height)=>({data:new Uint8ClampedArray(width*height*4)})});
await w.eval('(async()=>{'+(await read('tools/map_editor/app.mjs')).replace(/^import .*;\r?\n/,'')+'})()');
const $=id=>w.document.getElementById(id);
const node=id=>JSON.parse(w.localStorage.getItem('dustline.recipe.v1')).nodes.find(n=>n.id===id);
const port=(id,input)=>w.document.querySelector(`.port[data-node-id="${id}"]${input?`[data-port="${input}"]`:'.out'}`);
const click=e=>{assert.ok(e);e.click();};
const key=(key,extra={})=>w.dispatchEvent(new w.KeyboardEvent('keydown',{key,bubbles:true,...extra}));


const wait=()=>new Promise(resolve=>setTimeout(resolve,100));
const select=id=>w.document.querySelector(`.node[data-node-id="${id}"] .node-heading`).dispatchEvent(new w.KeyboardEvent('keydown',{key:'Enter',bubbles:true}));
const edit=(name,value)=>{const input=w.document.querySelector(`[aria-label="${name}"]`);assert.ok(input,name);input.value=String(value);input.dispatchEvent(new w.Event('change'));};
const lutValue=(row,value)=>(row[5+(value>>2)]>>>((value&3)*8))&255;
w.testWorker.onmessage({data:{ready:true}});
Object.defineProperties($('graph'),{clientWidth:{configurable:true,value:600},clientHeight:{configurable:true,value:400}});$('graph').getBoundingClientRect=()=>({left:10,top:20,width:600,height:400,right:610,bottom:420});
$('graph').dispatchEvent(new w.WheelEvent('wheel',{deltaY:-100,clientX:310,clientY:220,bubbles:true,cancelable:true}));assert.equal($('zoom-label').textContent,'110%');let transform=$('graph-plane').style.transform.match(/translate\(([-+\de.]+)px, ([-+\de.]+)px\) scale\(([-+\de.]+)\)/);assert.ok(Math.abs(Number(transform[1])+30)<.001&&Math.abs(Number(transform[2])+20)<.001&&Math.abs(Number(transform[3])-1.1)<.001);
$('graph').dispatchEvent(new w.MouseEvent('pointerdown',{button:0,clientX:100,clientY:100,bubbles:true}));w.dispatchEvent(new w.MouseEvent('pointermove',{clientX:130,clientY:145,bubbles:true}));transform=$('graph-plane').style.transform.match(/translate\(([-+\de.]+)px, ([-+\de.]+)px\)/);assert.ok(Math.abs(Number(transform[1]))<.001&&Math.abs(Number(transform[2])-25)<.001);w.dispatchEvent(new w.MouseEvent('pointerup',{clientX:130,clientY:145,bubbles:true}));assert.equal($('graph').classList.contains('panning'),false);
$('view').value='final';$('view').dispatchEvent(new w.Event('change'));$('layer').value='textures';$('layer').dispatchEvent(new w.Event('change'));
$('preset').value='natural-ground';$('preset').dispatchEvent(new w.Event('change'));await wait();
let recipe=JSON.parse(w.localStorage.getItem('dustline.recipe.v1'));
assert.equal(recipe.version,6);assert.equal(recipe.materialOutput,6);assert.equal(recipe.output,3);
assert.equal(w.lastProgram.schematic,true);assert.equal(w.lastProgram.materialProgram.at(-1)[0],14);
$('view').value='final';$('view').dispatchEvent(new w.Event('change'));await wait();assert.equal(w.lastProgram.textures,true);
click($('art-bank'));assert.equal($('material-bindings').children.length,4);assert.equal($('decoration-bindings').children.length,4);assert.equal($('world-bindings').children.length,2);
edit('Material slot 2 ID',7);assert.equal(JSON.parse(w.localStorage.getItem('dustline.recipe.v1')).artProfile.materials[1].id,7);
edit('Material slot 2 ID',1);click($('close-art'));
click($('shop-profile'));await wait();assert.equal($('shop-preview').children.length,6);assert.equal(w.document.querySelectorAll('.shop-modifier-row').length,6);edit('Shop stock size',8);await wait();recipe=JSON.parse(w.localStorage.getItem('dustline.recipe.v1'));assert.equal(recipe.version,7);assert.equal(recipe.shopProfile.stockSize,8);assert.equal($('shop-preview').querySelectorAll('li').length,48);click(w.document.querySelector('[aria-label="Town 0 override"]'));assert.equal(JSON.parse(w.localStorage.getItem('dustline.recipe.v1')).shopProfile.townModifiers.length,1);click($('close-shop'));
console.log('PASS shop profile DOM: bounded regional fields, town overrides and exact six-town preview.');
select(5);await wait();assert.ok(w.document.querySelector('.lut-chart'));assert.ok(w.lastProgram.histogramProgram);assert.equal(w.document.querySelector('.lut-node-row').children.length,2);assert.equal(w.document.querySelector('.lut-node-row .setting.full'),null);
assert.equal(w.document.querySelector('.lut-chart svg').getAttribute('preserveAspectRatio'),'none');
const histogram=new Uint16Array(256);histogram[42]=4096;
w.testWorker.onmessage({data:{id:w.lastProgram.id,ms:1,histogram,histogramNode:5,categorical:true,outputs:[{meta:new Uint32Array([0,1,2,0,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]),cells:new Uint8Array(4096).fill(1),seed:42}]}});
assert.match(w.document.querySelector('.lut-frequency').textContent,/4096 max/);assert.match(w.document.querySelector('.lut-histogram').getAttribute('d'),/L42 14/);
click(w.document.querySelector('.lut-point'));assert.ok(w.document.querySelector('.lut-popover'));edit('Point color','#123456');assert.equal(node(5).colors[1],'#123456');await wait();assert.equal(w.lastProgram.categoricalColors[1],'#123456');
click(w.document.querySelector('.lut-point'));click(w.document.querySelector('[aria-label="Delete LUT point"]'));assert.equal(node(5).p[0],2);
const chart=w.document.querySelector('.lut-chart');chart.getBoundingClientRect=()=>({left:0,width:255});chart.dispatchEvent(new w.MouseEvent('click',{clientX:220,bubbles:true}));assert.equal(node(5).p[0],3);chart.dispatchEvent(new w.MouseEvent('click',{clientX:210,bubbles:true}));assert.equal(node(5).p[0],4);
click($('pin-settings'));select(3);click(w.document.querySelector('.lut-fixed'));edit('Point color','#abcdef');edit('Initial output value',255);await wait();
assert.equal(Number(w.document.querySelector('.node.selected').dataset.nodeId),3);
assert.equal(lutValue(w.lastProgram.materialProgram[1],0),255);
// Both roots and categorical IDs survive JSON export/import.
click($('export'));const exported=JSON.parse(await w.lastBlob.text());assert.equal(exported.materialOutput,6);assert.equal(exported.nodes.find(n=>n.id===5).p[1],255);assert.equal(exported.nodes.find(n=>n.id===5).colors[0],'#abcdef');
Object.defineProperty($('import-file'),'files',{configurable:true,value:[{size:1000,text:async()=>JSON.stringify(exported)}]});
await $('import-file').onchange();await wait();assert.equal(lutValue(w.lastProgram.materialProgram[1],0),255);
// Render action sends both outputs and displays its asynchronous full-resolution result.
click($('render-map'));assert.equal(w.lastProgram.render,true);assert.equal(w.lastProgram.materialProgram.at(-1)[0],14);assert.ok($('render-dialog').open);
w.testWorker.onmessage({data:{render:true,id:w.lastProgram.id,blob:new Blob(['png']),seed:exported.seed}});
assert.equal($('save-render').disabled,false);assert.match($('render-status').textContent,/8192/);
$('render-zoom').value='1';$('render-zoom').dispatchEvent(new w.Event('change'));assert.equal($('render-image').style.width,'8192px');click($('close-render'));
// Simulate a generated material result, including reserved-ID warning/legend.
select(6);await wait();
w.testWorker.onmessage({data:{id:w.lastProgram.id,ms:1,outputs:[{meta:new Uint32Array([0,3,2,0,123,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]),cells:new Uint8Array(4096).fill(255),seed:42,unknown:[255]}]}});
assert.match($('notice').textContent,/Unassigned material IDs: 255/);assert.match($('legend').textContent,/Sand.*Gravel/);
// Deleting the material root restores the explicit legacy fallback and undo restores both roots.
click($('delete'));recipe=JSON.parse(w.localStorage.getItem('dustline.recipe.v1'));assert.equal(recipe.version,7);assert.equal(recipe.materialOutput,undefined);
click($('undo'));recipe=JSON.parse(w.localStorage.getItem('dustline.recipe.v1'));assert.equal(recipe.version,7);assert.equal(recipe.materialOutput,6);
// Changing outputs never replaces the wall/floor root with a material root.
select(6);click($('set-output'));recipe=JSON.parse(w.localStorage.getItem('dustline.recipe.v1'));assert.equal(recipe.output,3);assert.equal(recipe.materialOutput,6);
console.log('PASS material editor DOM: independent roots, pinned edits, JSON roundtrip, full-render request/zoom, warnings/legend, deletion/undo.');
$('preset').value='town-roads';$('preset').dispatchEvent(new w.Event('change'));await wait();
recipe=JSON.parse(w.localStorage.getItem('dustline.recipe.v1'));
assert.equal(recipe.nodes.find(n=>n.id===recipe.output).type,'roads');
assert.equal(w.lastProgram.program.at(-1)[0],16);assert.equal(w.lastProgram.materialProgram.at(-1)[0],14);
edit('Road width (pixels)',256);edit('Road material',1);await wait();
assert.deepEqual(Array.from(w.lastProgram.program.at(-1).slice(5,8)),[256,1,0]);
select(3);click($('set-output'));await wait();assert.equal(w.lastProgram.program.at(-1)[0],11);
select(7);click($('set-output'));await wait();assert.equal(w.lastProgram.program.at(-1)[0],16);
click($('render-map'));assert.equal(w.lastProgram.render,true);assert.equal(w.lastProgram.program.at(-1)[0],16);
click($('close-render'));click($('export'));const roadExport=JSON.parse(await w.lastBlob.text());
assert.equal(roadExport.output,7);assert.equal(roadExport.materialOutput,6);
// Retired min/max exports import as one width, rounded to an 8px tile.
const oldRoads=structuredClone(roadExport);oldRoads.nodes.find(n=>n.type==='roads').p=[153,1,170];
Object.defineProperty($('import-file'),'files',{configurable:true,value:[{size:1000,text:async()=>JSON.stringify(oldRoads)}]});
await $('import-file').onchange();await wait();
assert.deepEqual(Array.from(w.lastProgram.program.at(-1).slice(5,8)),[160,1,0]);
assert.equal(w.document.querySelector('[aria-label="Road width (pixels)"]').value,'160');
assert.equal(w.document.querySelector('[aria-label="Maximum width (pixels)"]'),null);
console.log('PASS road editor DOM: preset, width/material edits, output switching, full render and JSON export.');
$('preset').value='populated-wasteland';$('preset').dispatchEvent(new w.Event('change'));await wait();
recipe=JSON.parse(w.localStorage.getItem('dustline.recipe.v1'));
assert.equal(recipe.version,6);assert.equal(w.lastProgram.spawnProgram.at(-1)[0],17);assert.equal(w.lastProgram.decorationProgram.at(-1)[0],18);
click($('population-bank'));assert.equal($('population-bindings').children.length,1);click($('add-population'));assert.equal($('population-bindings').children.length,2);assert.equal(JSON.parse(w.localStorage.getItem('dustline.recipe.v1')).version,6);click($('close-population'));
select(recipe.spawnOutput);edit('Target count',24);await wait();
assert.equal(w.lastProgram.program.at(-1)[0],16);assert.equal(w.lastProgram.spawnProgram.at(-1)[5],24);
click($('pin-settings'));select(recipe.output);edit('Minimum spacing (pixels)',384);await wait();
assert.equal(w.lastProgram.spawnProgram.at(-1)[6],384);click($('pin-settings'));
select(recipe.decorationOutput);edit('Density %',80);edit('Dry grass weight',0);await wait();
assert.equal(w.lastProgram.decorationProgram.at(-1)[5],80);assert.equal(w.lastProgram.decorationProgram.at(-1)[6],0);
$('show-spawns').checked=false;$('show-spawns').dispatchEvent(new w.Event('change'));await wait();assert.equal(w.lastProgram.showSpawns,false);
click($('export'));const populated=JSON.parse(await w.lastBlob.text());assert.ok(populated.spawnOutput && populated.decorationOutput);
click($('set-output'));assert.equal(JSON.parse(w.localStorage.getItem('dustline.recipe.v1')).output,populated.output);
click($('delete'));await wait();assert.equal(JSON.parse(w.localStorage.getItem('dustline.recipe.v1')).decorationOutput,undefined);
click($('undo'));await wait();assert.equal(w.lastProgram.decorationProgram.at(-1)[5],80);
console.log('PASS placement editor DOM: independent outputs, pinned spawn controls, density/type weights, marker toggle, export and deletion/undo.');
$('preset').value='wasteland';$('preset').dispatchEvent(new w.Event('change'));await wait();$('view').value='placements';$('view').dispatchEvent(new w.Event('change'));await wait();assert.equal(w.lastProgram.schematic,true);assert.equal(w.lastProgram.exitMask,9);assert.equal(w.lastProgram.portals.length,2);assert.equal(w.lastProgram.playerSpawns.length,3);assert.equal($('placement-list').children.length,5);assert.ok(w.document.querySelector('[aria-label="Portal X"]'));assert.ok(w.document.querySelector('.preview-panel').classList.contains('placement-mode'));edit('Portal X',4096);click($('undo'));await wait();$('view').value='final';$('view').dispatchEvent(new w.Event('change'));await wait();assert.ok(!w.document.querySelector('.preview-panel').classList.contains('placement-mode'));$('view').value='placements';$('view').dispatchEvent(new w.Event('change'));await wait();
console.log('PASS authored placement DOM: schematic request, separate Portal/Player Spawn lists, exact coordinate editing and undo.');
const worldMaps=mapLibrary.maps.filter(entry=>entry.includeInGame),worldConnections=mapLibrary.world.connections.length;
click($('world-map'));assert.ok($('world-dialog').open);assert.equal(w.document.querySelectorAll('.world-node').length,worldMaps.length);assert.equal(w.document.querySelectorAll('.world-gate.cardinal').length,worldMaps.length*4);assert.equal(w.document.querySelectorAll('.world-link').length,worldConnections);assert.match(w.document.querySelector('.world-node small').textContent,/\[-?\d+,-?\d+\]/);assert.match(w.document.querySelector('.world-link span').textContent,/↔/);
click($('open-atlas'));assert.ok($('atlas-dialog').open);assert.equal(w.lastProgram.atlas,true);assert.equal(w.lastProgram.maps.length,worldMaps.length);assert.deepEqual(w.lastProgram.maps.map(map=>map.exitMask).sort((a,b)=>a-b),[2,4,9]);const atlasMessage=w.lastProgram;w.testWorker.onmessage({data:{id:atlasMessage.id,atlas:true,maps:atlasMessage.maps.map(map=>({id:map.id,grid:map.grid,exitMask:map.exitMask,cells:new Uint8Array(4096),roads:new Uint8Array(16384),meta:new Uint32Array(20)}))}});assert.ok($('atlas-canvas').width>=256);assert.match($('atlas-status').textContent,/3 regions/);click($('close-atlas'));
click(w.document.querySelector('.world-link button'));assert.equal(w.document.querySelectorAll('.world-link').length,worldConnections-1);assert.equal($('save-world').disabled,false);assert.match($('world-status').textContent,/free sides/);
const east=w.document.querySelector('[title="twin-cities east"]'),west=w.document.querySelector('[title="wasteland west"]');click(east);assert.match($('world-status').textContent,/complementary/);click(west);assert.equal(w.document.querySelectorAll('.world-link').length,worldConnections);
click($('world-undo'));assert.equal(w.document.querySelectorAll('.world-link').length,worldConnections-1);click($('world-redo'));assert.equal(w.document.querySelectorAll('.world-link').length,worldConnections);
click($('save-world'));await wait();assert.equal(mapLibrary.world.version,3);assert.equal(mapLibrary.world.connections.length,worldConnections);assert.match($('world-status').textContent,/saved/);click($('close-world'));
$('preset').value='two-rules-radial';$('preset').dispatchEvent(new w.Event('change'));await wait();$('include-in-game').checked=false;$('include-in-game').dispatchEvent(new w.Event('change'));click($('save-map'));await wait();
assert.equal(mapLibrary.maps.find(entry=>entry.id==='two-rules-radial').includeInGame,false);assert.ok(!mapLibrary.world.nodes.some(node=>node.map==='two-rules-radial'));assert.ok(mapLibrary.world.connections.every(link=>link.a.map!=='two-rules-radial'&&link.b.map!=='two-rules-radial'));
console.log('PASS world-map DOM: reciprocal cardinal ports, derived coordinates, disabled-map pruning and atomic library save.');
const initialMaps=mapLibrary.maps.length;
click($('new-map'));assert.equal(JSON.parse(w.localStorage.getItem('dustline.recipe.v1')).nodes.length,0);assert.equal($('include-in-game').checked,false);
click($('save-map'));await wait();assert.equal(mapLibrary.maps.length,initialMaps+1);assert.match($('preset').value,/untitled-map/);
$('preset').value='natural-ground';$('preset').dispatchEvent(new w.Event('change'));await wait();
click($('save-as'));await wait();assert.equal(mapLibrary.maps.length,initialMaps+2);assert.match($('preset').value,/natural-ground-copy/);
console.log('PASS shared library UI: new draft, repository Save, Save As and automatic reload selection.');
dom.window.close();
