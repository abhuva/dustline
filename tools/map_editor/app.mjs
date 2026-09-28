import {compile, makeNode, MAX_NODES, LUT_MAX_POINTS, defaultLutColor, normalizeSpawnProfiles, normalizePlacements} from './recipe.mjs';

const $ = id => document.getElementById(id);
const element = (tag, className, text) => {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text !== undefined) node.textContent = text;
  return node;
};
const clone = value => structuredClone(value);
const schema = await fetch('./schema.json').then(r => r.json());
const artData=await fetch('./generated/art.json').then(r=>r.json());
const assetCatalog=artData.catalog;
const materialAssets=new Map(assetCatalog.materials.map(asset=>[asset.key,asset]));
const decorationAssets=new Map(assetCatalog.decorations.map(asset=>[asset.key,asset]));
let materialCatalog=[];
function normalizedArtProfile(value){
  if(typeof value==='string')value=assetCatalog.presets[value];
  value=value&&typeof value==='object'?clone(value):clone(assetCatalog.presets.sun);
  value.materials=Array.isArray(value.materials)&&value.materials.length===4?value.materials:clone(assetCatalog.presets.sun.materials);
  value.decorations=Array.isArray(value.decorations)&&value.decorations.length===4?value.decorations:clone(assetCatalog.presets.sun.decorations);
  value.materials=value.materials.map((binding,index)=>({id:Number.isInteger(binding.id)?binding.id:index,asset:materialAssets.has(binding.asset)?binding.asset:assetCatalog.presets.sun.materials[index].asset,enabled:binding.enabled!==false}));
  value.decorations=value.decorations.map((binding,index)=>({asset:decorationAssets.has(binding.asset)?binding.asset:assetCatalog.presets.sun.decorations[index].asset,enabled:binding.enabled!==false}));
  if(!assetCatalog.wallSets.some(asset=>asset.key===value.wallSet))value.wallSet=assetCatalog.presets.sun.wallSet;
  if(!assetCatalog.townSets.some(asset=>asset.key===value.townSet))value.townSet=assetCatalog.presets.sun.townSet;
  return value;
}
const defaultShopProfile=()=>({tierFloor:1,townTierRange:[1,1],stockSize:9,
  mixWeights:{upgrade:1,front:1,side:1,top:1},townModifiers:[]});
function normalizedShopProfile(value){
  const result=value&&typeof value==='object'?clone(value):defaultShopProfile();
  result.tierFloor=Number.isInteger(result.tierFloor)?result.tierFloor:1;
  result.townTierRange=Array.isArray(result.townTierRange)&&result.townTierRange.length===2?result.townTierRange.map(Number):[1,1];
  result.stockSize=Number.isInteger(result.stockSize)?result.stockSize:9;
  result.mixWeights={...defaultShopProfile().mixWeights,...(result.mixWeights??{})};
  result.townModifiers=Array.isArray(result.townModifiers)?result.townModifiers:[];
  return result;
}
const libraryResponse=await fetch('/api/library');
if(!libraryResponse.ok)throw new Error(`Could not load the map library (${libraryResponse.status}).`);
const libraryEnvelope=await libraryResponse.json();
let library=libraryEnvelope.library,libraryRevision=libraryEnvelope.revision;
let worldDraft=null,worldPending=null,worldDirty=false,worldUndo=[],worldRedo=[];
const ops = new Map(schema.operations.map(o => [o.id,o]));
const savedSelection=localStorage.getItem('dustline.map-selection.v1');
let currentId=library.maps.some(entry=>entry.id===savedSelection)?savedSelection:library.maps[0].id;
let currentEntry=library.maps.find(entry=>entry.id===currentId);
let recipe=normalizePlacements(clone(currentEntry.recipe)),includeInGame=currentEntry.includeInGame;recipe.artProfile=normalizedArtProfile(recipe.artProfile);recipe.spawnProfiles=normalizeSpawnProfiles(recipe.spawnProfiles);recipe.shopProfile=normalizedShopProfile(recipe.shopProfile);
let selected=recipe.output??recipe.nodes[0]?.id??null,pending=null,dirty=false;
let undo = [], redo = [], sequence = 0, ready = false, timer, lastResponse, iteration = null;
let zoom=1,panX=0,panY=0;
let placementSelection=null,placementZoom=1,placementPanX=0,placementPanY=0,placementDrag=null,placementAddKind=null;
let pinnedSettings = null;
let lutHistogram=null,lutHistogramNode=null,lutPointSelection=null;
let renderRequest=0,renderUrl=null,renderBlob=null,renderSeed=0,renderBusy=false;
let cancelConnectionDrag = null, suppressPortClick = false;
try {
  const saved=JSON.parse(localStorage.getItem('dustline.map-draft.v1'));
  if(saved?.id===currentId&&saved.revision===libraryRevision&&saved.recipe){
    recipe=normalizePlacements(saved.recipe);recipe.artProfile=normalizedArtProfile(recipe.artProfile);recipe.spawnProfiles=normalizeSpawnProfiles(recipe.spawnProfiles);recipe.shopProfile=normalizedShopProfile(recipe.shopProfile);includeInGame=Boolean(saved.includeInGame);selected=recipe.output??recipe.nodes[0]?.id??null;dirty=true;
  }
} catch { /* A broken draft must never prevent opening the workshop. */ }
const worker = new Worker('./worker.mjs', {type:'module'});
worker.onerror = event => message(`Engine could not load: ${event.message}. Run ./map-editor.ps1 to rebuild it.`, 'error');
worker.onmessage = ({data}) => {
  if(data.render){
    if(data.id!==renderRequest)return;
    if(data.error){renderBusy=false;$('render-status').textContent=data.error;$('render-map').disabled=false;return;}
    if(data.progress!==undefined){$('render-status').textContent=`Rendering full map: ${data.progress}%`;return;}
    if(renderUrl)URL.revokeObjectURL(renderUrl);
    renderBlob=data.blob;renderSeed=data.seed;renderUrl=URL.createObjectURL(data.blob);
    renderBusy=false;$('render-image').src=renderUrl;$('render-status').textContent=`8192 × 8192 pixels · seed ${data.seed}`;
    $('save-render').disabled=false;$('render-map').disabled=false;return;
  }
  if (data.ready) { ready = true; $('engine-status').textContent = 'ENGINE READY · SHARED C++ / WASM'; schedule(); return; }
  if (data.id !== sequence) return;
  if (data.error) { $('preview').setAttribute('aria-busy','false'); message(data.error,'error'); return; }
  $('preview').setAttribute('aria-busy','false'); $('download-png').disabled=false;
  if(Object.hasOwn(data,'histogramNode')){lutHistogram=data.histogram;lutHistogramNode=data.histogramNode;updateVisibleLut();}
  lastResponse = data; draw(data);
};
function message(text, type='') { $('notice').textContent = text; $('notice').className = type; }
function syncMaterialCatalog(){
  recipe.artProfile=normalizedArtProfile(recipe.artProfile);
  materialCatalog=recipe.artProfile.materials.filter(binding=>binding.enabled).map(binding=>{
    const asset=materialAssets.get(binding.asset);return {...asset,id:binding.id};
  });
  const list=$('material-ids');
  if(list)list.replaceChildren(...materialCatalog.map(material=>{const option=element('option','',material.name);option.value=material.id;return option;}));
}
function remember() { undo.push(clone(recipe)); if (undo.length > 80) undo.shift(); redo = []; }
function save() {
  dirty=true;
  try {
    localStorage.setItem('dustline.recipe.v1', JSON.stringify(recipe));
    localStorage.setItem('dustline.map-draft.v1',JSON.stringify({id:currentId,revision:libraryRevision,recipe,includeInGame}));
  } catch { /* The repository Save action remains available. */ }
  $('undo').disabled = !undo.length; $('redo').disabled = !redo.length;
  updateMapControls();
}
function change(fn, repaint=true) { remember(); fn(); iteration = null; save(); if (repaint) render(); else { renderGraph(); schedule(); } }
function restore(from, to) {
  if (!from.length) return;
  to.push(clone(recipe)); recipe = from.pop();
  if (!recipe.nodes.some(n => n.id === selected)) selected = recipe.output??recipe.nodes[0]?.id??null;
  pending = null; iteration = null; save(); render();
}
function select(id) {
  selected = id; iteration = null; $('view').value = 'selected';
  renderGraph(); renderInspector(); schedule();
}
function settingsNodeId() { return pinnedSettings ?? selected; }
function arrange() {
  const levels = new Map(), visiting = new Set(), rows = new Map();
  function level(n) {
    if (levels.has(n.id)) return levels.get(n.id);
    if (visiting.has(n.id)) return 0;
    visiting.add(n.id);
    const sources = Object.values(n.inputs).map(id => recipe.nodes.find(n => n.id === id)).filter(Boolean);
    const depth = sources.length ? 1 + Math.max(...sources.map(level)) : 0;
    visiting.delete(n.id); levels.set(n.id, depth); return depth;
  }
  for (const n of recipe.nodes) {
    const depth = level(n), row = rows.get(depth) ?? 0;
    n.x = 35 + depth*235; n.y = 45 + row*190; rows.set(depth,row+1);
  }
}
function render() {
  syncMaterialCatalog();
  if(pinnedSettings!==null && !recipe.nodes.some(n=>n.id===pinnedSettings))pinnedSettings=null;
  $('recipe-name').value = recipe.name ?? 'Untitled recipe'; $('seed').value = recipe.seed;
  $('include-in-game').checked=includeInGame;updateMapControls();
  renderGraph(); renderInspector();renderPlacementControls(); schedule();
}
function summary(n) {
  if (n.type === 'random') return `${n.p[0]}% walls · border ${n.p[1]}`;
  if (n.type === 'cellular') return `${n.p[0]} passes · ${n.p[4]} neighbours`;
  if (n.type === 'world') return 'Connect · clear · place';
  if (n.type === 'roads') return `${n.p[0]} px / material ${n.p[1]}`;
  if (n.type === 'largest') return '4-connected floor';
  if (n.type === 'combine') return ['A ∪ B','A ∩ B','A − B','A ⊕ B'][n.p[0]];
  if (n.type === 'field_lut') return `${n.p[0]+1} bands · ${n.p[1]} initial`;
  return ops.get(n.type).parameters.map((p,i) => n.p[i]).join(' / ') || 'Binary mask';
}
function connectionError(sourceId, targetId, port) {
  const source=recipe.nodes.find(n=>n.id===sourceId), target=recipe.nodes.find(n=>n.id===targetId);
  const expected=target && ops.get(target.type).inputs[port]?.replace('?','');
  if (!source || !expected || ops.get(source.type).kind!==expected) return `This input needs a ${expected || 'compatible output'}.`;
  const visited=new Set();
  function reachesTarget(id) {
    if(id===targetId)return true;
    if(visited.has(id))return false;
    visited.add(id);
    return Object.values(recipe.nodes.find(n=>n.id===id)?.inputs ?? {}).some(reachesTarget);
  }
  return reachesTarget(sourceId)?'This connection would create a cycle.':null;
}
function connect(sourceId, targetId, port, selectTarget=true) {
  const error=connectionError(sourceId,targetId,port);
  if(error){message(error,'error');return;}
  const target=recipe.nodes.find(n=>n.id===targetId);
  pending=null;
  if(target.inputs[port]===sourceId){if(selectTarget)select(targetId);return;}
  change(()=>{target.inputs[port]=sourceId;if(selectTarget)selected=targetId;});
}
function disconnect(targetId, port, selectTarget=true) {
  const target=recipe.nodes.find(n=>n.id===targetId);
  if(!target || target.inputs[port]===undefined)return;
  pending=null;
  change(()=>{delete target.inputs[port];if(selectTarget)selected=targetId;});
}
function wirePath(x1,y1,x2,y2) {
  const bend=Math.max(45,Math.abs(x2-x1)/2);
  return `M${x1},${y1} C${x1+bend},${y1} ${x2-bend},${y2} ${x2},${y2}`;
}
function portPointerDown(event) {
  event.stopPropagation();
  if(event.button!==0)return;
  const start=event.currentTarget, pointerId=event.pointerId;
  const origin=start.getBoundingClientRect(), startX=event.clientX, startY=event.clientY;
  let moved=false, hover=null, preview=null;
  function endpoints(other) {
    if(!other || start.classList.contains('out')===other.classList.contains('out'))return null;
    const output=start.classList.contains('out')?start:other, input=output===start?other:start;
    return [Number(output.dataset.nodeId),Number(input.dataset.nodeId),input.dataset.port];
  }
  function cleanup() {
    window.removeEventListener('pointermove',move);
    window.removeEventListener('pointerup',up);
    window.removeEventListener('pointercancel',cancel);
    window.removeEventListener('blur',cancel);
    hover?.classList.remove('drop-valid','drop-invalid'); preview?.remove();
    start.classList.remove('pending'); cancelConnectionDrag=null;
  }
  function cancel() {cleanup();pending=null;renderGraph();}
  function move(e) {
    if(e.pointerId!==pointerId)return;
    if(!moved && Math.hypot(e.clientX-startX,e.clientY-startY)<4)return;
    e.preventDefault();
    if(!moved){
      moved=true;pending=null;start.classList.add('pending');
      preview=document.createElementNS('http://www.w3.org/2000/svg','path');
      preview.classList.add('wire-preview');$('wires').append(preview);
      $('graph-hint').textContent='Drop on a matching port to connect or replace. Escape cancels.';
    }
    hover?.classList.remove('drop-valid','drop-invalid');
    hover=document.elementFromPoint(e.clientX,e.clientY)?.closest('.port');
    const pair=endpoints(hover), valid=pair && !connectionError(...pair);
    hover?.classList.add(valid?'drop-valid':'drop-invalid');
    const rect=$('graph-plane').getBoundingClientRect();
    const x1=(origin.left+origin.width/2-rect.left)/zoom, y1=(origin.top+origin.height/2-rect.top)/zoom;
    const x2=(e.clientX-rect.left)/zoom,y2=(e.clientY-rect.top)/zoom;
    preview.setAttribute('d',start.classList.contains('out')?wirePath(x1,y1,x2,y2):wirePath(x2,y2,x1,y1));
  }
  function up(e) {
    if(e.pointerId!==pointerId)return;
    const pair=endpoints(document.elementFromPoint(e.clientX,e.clientY)?.closest('.port'));
    cleanup();
    if(!moved)return; // Ordinary clicks retain the keyboard-accessible connection flow.
    suppressPortClick=true;setTimeout(()=>{suppressPortClick=false;},0);
    if(pair)connect(...pair);
    else message('Connection unchanged. Drop on an input/output port of the matching type.');
    renderGraph();
  }
  cancelConnectionDrag=cancel;
  window.addEventListener('pointermove',move,{passive:false});
  window.addEventListener('pointerup',up);
  window.addEventListener('pointercancel',cancel);
  window.addEventListener('blur',cancel);
}
window.addEventListener('click',event=>{
  if(suppressPortClick){event.preventDefault();event.stopImmediatePropagation();}
},true);
function applyGraphView() {
  $('graph-plane').style.transform=`translate(${panX}px, ${panY}px) scale(${zoom})`;
  $('graph').style.backgroundPosition=`${panX}px ${panY}px`;$('graph').style.backgroundSize=`${18*zoom}px ${18*zoom}px`;
  $('zoom-label').textContent=`${Math.round(zoom*100)}%`;
}
function renderGraph() {
  $('nodes').replaceChildren(); $('wires').replaceChildren();
  const width = Math.max($('graph').clientWidth/zoom, ...recipe.nodes.map(n => (Number(n.x)||0)+230));
  const height = Math.max($('graph').clientHeight/zoom, ...recipe.nodes.map(n => (Number(n.y)||0)+210));
  $('graph-plane').style.width = `${width}px`; $('graph-plane').style.height = `${height}px`;
  applyGraphView();
  for (const n of recipe.nodes) {
    const op = ops.get(n.type), card = element('div',`node ${op.kind}${n.id===selected?' selected':''}`);
    card.dataset.nodeId = n.id; card.style.left = `${Number(n.x)||0}px`; card.style.top = `${Number(n.y)||0}px`;
    const head = element('div','node-heading');
    head.tabIndex=0;head.setAttribute('role','button');head.setAttribute('aria-label',`Inspect ${n.label}`);
    head.onkeydown=event=>{if(event.key==='Enter'||event.key===' '){event.preventDefault();select(n.id);}};
    head.append(element('div','node-label',n.label || op.name), element('div','node-type',`${String(n.id).padStart(2,'0')} / ${op.name}`));
    const out = element('button',`port out ${op.kind}${pending===n.id?' pending':''}`);
    out.dataset.nodeId=n.id;
    out.title = `Connect ${n.label} output`; out.setAttribute('aria-label',out.title);
    out.onclick = event => { event.stopPropagation(); pending = pending===n.id?null:n.id; renderGraph(); };
    out.onpointerdown = portPointerDown; head.append(out);
    head.onpointerdown = event => {
      if (event.button !== 0) return;
      event.preventDefault();
      const startX=event.clientX,startY=event.clientY,originalX=Number(n.x)||0,originalY=Number(n.y)||0;
      let moved=false;
      const move = e => {
        if (!moved && Math.abs(e.clientX-startX)+Math.abs(e.clientY-startY)<4) return;
        if (!moved) { remember(); moved=true; }
        n.x=Math.max(0,originalX+(e.clientX-startX)/zoom); n.y=Math.max(0,originalY+(e.clientY-startY)/zoom);
        renderGraph();
      };
      const up = () => {
        window.removeEventListener('pointermove',move); window.removeEventListener('pointerup',up);
        if (moved) save(); else select(n.id);
      };
      window.addEventListener('pointermove',move); window.addEventListener('pointerup',up,{once:true});
    };
    card.append(head);
    Object.entries(op.inputs).forEach(([port,expected], index) => {
      const row=element('div','node-port',`${port==='mask'?'Mask':port.toUpperCase()}${expected.endsWith('?')?' · optional':''}`);
      const button=element('button',`port ${expected.replace('?','')}${n.inputs[port]!==undefined?' connected':''}`);
      button.dataset.nodeId=n.id;button.dataset.port=port;button.onpointerdown=portPointerDown;
      button.title=`Connect ${n.label} input ${port}`; button.setAttribute('aria-label',button.title);
      button.onclick=() => {
        if (pending===null) { select(n.id); message('Choose an output dot first, or use the input selector below.'); return; }
        connect(pending,n.id,port);
      };
      row.append(button); card.append(row);
      const source=recipe.nodes.find(s=>s.id===n.inputs[port]);
      if (source) {
        const remove=element('button','disconnect','×');
        remove.title=`Disconnect ${n.label} input ${port}`;remove.setAttribute('aria-label',remove.title);
        remove.onclick=()=>disconnect(n.id,port);row.append(remove);
        const x1=(Number(source.x)||0)+190,y1=(Number(source.y)||0)+33;
        const x2=Number(n.x)||0,y2=(Number(n.y)||0)+75+index*24;
        const path=document.createElementNS('http://www.w3.org/2000/svg','path');
        const bend=Math.max(45,Math.abs(x2-x1)/2);
        path.setAttribute('d',`M${x1},${y1} C${x1+bend},${y1} ${x2-bend},${y2} ${x2},${y2}`);
        path.setAttribute('fill','none'); path.setAttribute('stroke',expected.startsWith('field')?'#7d97bb':expected.startsWith('material')?'#d8a576':'#899e62'); path.setAttribute('stroke-width','2');
        $('wires').append(path);
      }
    });
    const foot=element('div','node-summary',summary(n)); foot.onclick=()=>select(n.id); card.append(foot);
    if(n.id===recipe.output) card.append(element('div','node-output','ROM OUTPUT'));
    if(n.id===recipe.materialOutput) card.append(element('div','node-output','GROUND OUTPUT'));
    if(n.id===recipe.spawnOutput)card.append(element('div','node-output','SPAWN OUTPUT'));
    if(n.id===recipe.decorationOutput)card.append(element('div','node-output','DECORATION OUTPUT'));
    if(n.id===pinnedSettings) card.append(element('div','node-pinned-label','PINNED'));
    $('nodes').append(card);
  }
  $('graph-hint').textContent=pending===null?'Drag empty space to pan · wheel to zoom · drag ports to connect.':'Choose any matching input to connect or replace. Escape cancels.';
}
function setting(parent,label,control,full=false) {
  const row=element('label',`setting${full?' full':''}`); row.append(element('span','',label),control); parent.append(row); return row;
}
function lutPoints(n) {
  return Array.from({length:n.p[0]},(_,slot)=>({slot,position:n.p[2+slot*2],output:n.p[3+slot*2],color:n.colors[slot+1]}));
}
function lutEntryAt(n,position) {
  let entry={output:n.p[1],color:n.colors[0]};
  for(const point of lutPoints(n).sort((a,b)=>a.position-b.position))if(point.position<=position)entry=point;
  return entry;
}
function setLutPoints(n,points) {
  n.p=[points.length,n.p[1],...points.flatMap(point=>[point.position,point.output])];
  n.colors=[n.colors[0],...points.map(point=>point.color)];
}
function freeLutPosition(n,wanted,ignore=-1) {
  const occupied=new Set(lutPoints(n).filter(p=>p.slot!==ignore).map(p=>p.position));
  wanted=Math.max(1,Math.min(255,Math.round(wanted)));
  if(!occupied.has(wanted))return wanted;
  for(let distance=1;distance<255;++distance)for(const candidate of [wanted+distance,wanted-distance])
    if(candidate>=1 && candidate<=255 && !occupied.has(candidate))return candidate;
  return null;
}
function lutPreviewColors(n) {
  const colors=Array(256).fill(null),entries=[{output:n.p[1],color:n.colors[0]},...lutPoints(n).sort((a,b)=>a.position-b.position)];
  for(const entry of entries)colors[entry.output]=entry.color;
  return colors;
}
function readableText(color) {
  const rgb=[1,3,5].map(at=>parseInt(color.slice(at,at+2),16));
  return rgb[0]*299+rgb[1]*587+rgb[2]*114>145000?'#172419':'#f2f5e8';
}
function svgNode(tag,attributes={}) {
  const node=document.createElementNS('http://www.w3.org/2000/svg',tag);
  for(const [name,value] of Object.entries(attributes))node.setAttribute(name,value);
  return node;
}
function renderLutDiagram(n,chart) {
  const points=lutPoints(n).sort((a,b)=>a.position-b.position),entries=[{position:0,output:n.p[1],color:n.colors[0]},...points];
  const bands=chart.querySelector('.lut-bands');bands.replaceChildren();
  entries.forEach((entry,index)=>{
    const end=entries[index+1]?.position??256;
    bands.append(svgNode('rect',{x:entry.position,y:0,width:end-entry.position,height:100,fill:entry.color}));
  });
  const histogram=lutHistogramNode===n.id?lutHistogram:null,max=histogram?Math.max(1,...histogram):1;
  let path='M0 96';
  for(let value=0;value<256;++value){const y=96-(histogram?.[value]??0)/max*82;path+=`L${value} ${y}L${value+1} ${y}`;}
  path+='L256 96Z';chart.querySelector('.lut-histogram').setAttribute('d',path);
  chart.querySelector('.lut-frequency').textContent=histogram?`${max} max / 4096 cells`:'Connect an input to see its distribution';
  const fixed=chart.querySelector('.lut-fixed');fixed.textContent=n.p[1];fixed.style.setProperty('--point-color',n.colors[0]);fixed.style.color=readableText(n.colors[0]);
  for(const handle of chart.querySelectorAll('.lut-point')) {
    const point=lutPoints(n).find(p=>p.slot===Number(handle.dataset.slot));
    if(point){handle.style.left=`${point.position/255*100}%`;handle.textContent=point.output;handle.title=`Input ${point.position} and above outputs ${point.output}`;handle.style.setProperty('--point-color',point.color);handle.style.color=readableText(point.color);}
  }
  const popup=chart.querySelector('.lut-popover');
  if(popup){const slot=Number(popup.dataset.slot),position=slot<0?0:n.p[2+slot*2];popup.style.left=`${Math.max(18,Math.min(82,position/255*100))}%`;}
}
function updateVisibleLut() {
  const chart=document.querySelector('.lut-chart'),n=recipe.nodes.find(n=>n.id===settingsNodeId());
  if(chart && n?.type==='field_lut')renderLutDiagram(n,chart);
}
function deleteLutPoint(n,slot) {
  const points=lutPoints(n).filter(point=>point.slot!==slot).map(({position,output,color})=>({position,output,color}));
  setLutPoints(n,points);lutPointSelection=null;
}
function addLutPoint(n,position) {
  if(n.p[0]>=LUT_MAX_POINTS)return false;
  position=freeLutPosition(n,position);
  if(position===null)return false;
  const points=lutPoints(n).map(({position,output,color})=>({position,output,color}));
  const source=lutEntryAt(n,position);points.push({position,output:source.output,color:source.color});setLutPoints(n,points);
  lutPointSelection={nodeId:n.id,slot:points.length-1};
  return true;
}
function renderLutPopup(n,chart) {
  if(lutPointSelection?.nodeId!==n.id)return;
  const slot=lutPointSelection.slot,fixed=slot===-1,point=fixed?{position:0,output:n.p[1],color:n.colors[0]}:lutPoints(n).find(point=>point.slot===slot);
  if(!point){lutPointSelection=null;return;}
  const popup=element('div','lut-popover');popup.dataset.slot=slot;popup.onclick=event=>event.stopPropagation();popup.onpointerdown=event=>event.stopPropagation();
  const heading=element('div','lut-popup-heading');heading.append(element('strong','',fixed?'Input 0 · fixed':`Input ${point.position}`));
  const close=element('button','lut-popup-close','×');close.setAttribute('aria-label','Close point editor');close.onclick=()=>{lutPointSelection=null;renderInspector();};heading.append(close);popup.append(heading);
  if(!fixed){
    const position=element('input');position.type='number';position.min=1;position.max=255;position.value=point.position;position.setAttribute('aria-label','Point position');
    position.onchange=()=>{const value=Number(position.value);if(position.validity.valid&&position.value!==''&&!lutPoints(n).some(p=>p.slot!==slot&&p.position===value))change(()=>{n.p[2+slot*2]=value;});else position.value=n.p[2+slot*2];};
    setting(popup,'Input position',position);
  }
  const output=element('input');output.type='number';output.min=0;output.max=255;output.value=point.output;output.setAttribute('aria-label',fixed?'Initial output value':'Point output value');
  output.onchange=()=>{if(output.validity.valid&&output.value!=='')change(()=>{if(fixed)n.p[1]=Number(output.value);else n.p[3+slot*2]=Number(output.value);});else output.value=fixed?n.p[1]:n.p[3+slot*2];};
  setting(popup,'Value / ID',output);
  const color=element('input');color.type='color';color.value=point.color;color.setAttribute('aria-label','Point color');color.onchange=()=>change(()=>{n.colors[slot+1]=color.value;});
  setting(popup,'Preview color',color);
  if(!fixed){const remove=element('button','lut-popup-delete','Delete point');remove.setAttribute('aria-label','Delete LUT point');remove.onclick=()=>change(()=>deleteLutPoint(n,slot));popup.append(remove);}
  chart.append(popup);
}
function renderLutEditor(settings,n) {
  const editor=element('div','lut-editor setting full');
  const header=element('div','lut-header');header.append(element('span','','INPUT DISTRIBUTION / STEPPED OUTPUT'),element('span','lut-count',`${n.p[0]+1} points`));editor.append(header);
  const chart=element('div','lut-chart');chart.setAttribute('aria-label','Input value histogram. Click to add a LUT point.');
  const svg=svgNode('svg',{viewBox:'0 0 256 100',preserveAspectRatio:'none','aria-hidden':'true'});
  svg.append(svgNode('g',{class:'lut-bands'}),svgNode('path',{class:'lut-histogram'}),svgNode('line',{class:'lut-axis',x1:0,y1:96,x2:256,y2:96}));chart.append(svg);
  const fixed=element('button',`lut-fixed${lutPointSelection?.nodeId===n.id&&lutPointSelection.slot===-1?' selected':''}`);fixed.style.left='0%';fixed.title='Position 0 is fixed';fixed.setAttribute('aria-label',`Fixed LUT point at 0, output ${n.p[1]}`);fixed.onclick=event=>{event.stopPropagation();lutPointSelection={nodeId:n.id,slot:-1};renderInspector();};chart.append(fixed);
  for(const point of lutPoints(n)) {
    const handle=element('button',`lut-point${lutPointSelection?.nodeId===n.id&&lutPointSelection.slot===point.slot?' selected':''}`);
    handle.dataset.slot=point.slot;handle.setAttribute('aria-label',`LUT point at ${point.position}, output ${point.output}`);
    handle.onpointerdown=event=>{
      if(event.button!==0)return;event.preventDefault();event.stopPropagation();
      const slot=Number(handle.dataset.slot),startX=event.clientX;let remembered=false,moved=false;
      lutPointSelection={nodeId:n.id,slot};
      const move=e=>{
        if(Math.abs(e.clientX-startX)>=2)moved=true;if(!moved)return;
        if(!remembered){remember();remembered=true;}
        const rect=chart.getBoundingClientRect(),position=freeLutPosition(n,(e.clientX-rect.left)/rect.width*255,slot);
        if(position!==null)n.p[2+slot*2]=position;iteration=null;save();renderLutDiagram(n,chart);schedule();
      };
      const up=()=>{window.removeEventListener('pointermove',move);window.removeEventListener('pointerup',up);renderGraph();renderInspector();};
      window.addEventListener('pointermove',move);window.addEventListener('pointerup',up,{once:true});
    };
    handle.onclick=event=>{event.stopPropagation();lutPointSelection={nodeId:n.id,slot:point.slot};renderInspector();};
    handle.onkeydown=event=>{
      if(event.key==='Delete'||event.key==='Backspace'){event.preventDefault();change(()=>deleteLutPoint(n,point.slot));return;}
      if(!['ArrowLeft','ArrowRight'].includes(event.key))return;
      event.preventDefault();const delta=(event.key==='ArrowLeft'?-1:1)*(event.shiftKey?8:1);
      change(()=>{n.p[2+point.slot*2]=freeLutPosition(n,n.p[2+point.slot*2]+delta,point.slot);});
    };
    chart.append(handle);
  }
  chart.onclick=event=>{
    if(n.p[0]>=LUT_MAX_POINTS||event.target.closest('.lut-point,.lut-fixed,.lut-popover'))return;
    const rect=chart.getBoundingClientRect();change(()=>addLutPoint(n,(event.clientX-rect.left)/rect.width*255));
  };
  chart.append(element('span','lut-zero','0'),element('span','lut-max','255'),element('span','lut-frequency'));
  renderLutPopup(n,chart);
  editor.append(chart,element('p','setting-help','Click empty graph space to add a point. Select a point to edit its preview color and value/ID. Drag movable points through one another to reorder them; position 0 stays fixed.'));
  settings.append(editor);renderLutDiagram(n,chart);
}
function renderInspector() {
  const root=$('inspector'); root.replaceChildren();
  const n=recipe.nodes.find(n=>n.id===settingsNodeId());
  const pinned=pinnedSettings!==null;
  $('pin-settings').textContent=pinned?'Unpin settings':'Pin settings';
  $('pin-settings').setAttribute('aria-pressed',String(pinned));
  $('pin-settings').disabled=!n;
  $('pin-settings').title=pinned?'Let settings follow the selected node':'Keep these controls while selecting another node to preview';
  $('inspector-node-name').textContent=n?`${pinned?'Pinned: ':''}${n.label}`:'';
  $('delete').disabled=!n || recipe.nodes.length===1;
  $('duplicate').disabled=!n || recipe.nodes.length>=MAX_NODES;
  $('delete').title=n?`Delete ${n.label}`:'';
  $('duplicate').title=n?`Duplicate ${n.label}`:'';
  const selectedType=recipe.nodes.find(n=>n.id===selected)?.type;
  $('set-output').disabled=!['world','roads','materials','spawns','decoration'].includes(selectedType);
  $('set-output').textContent=selectedType==='spawns'?'Use as spawn output':selectedType==='decoration'?'Use as decoration output':selectedType==='materials'?'Use as ground output':'Use as wall/floor output';
  if (!n) return;
  const op=ops.get(n.type),settings=element('div','settings'),lutTop=n.type==='field_lut'?element('div','lut-node-row'):settings;
  if(n.type==='field_lut')settings.append(lutTop);
  const label=element('input'); label.value=n.label; label.maxLength=80;
  label.onchange=()=>change(()=>{n.label=label.value;}); setting(lutTop,'Node label',label,n.type!=='field_lut');
  for(const [port,expected] of Object.entries(op.inputs)) {
    const input=element('select'); input.setAttribute('aria-label',`Input ${port}`);
    const empty=element('option','',expected.endsWith('?')?'None · apply everywhere':'Choose a source…'); empty.value=''; input.append(empty);
    recipe.nodes.filter(source=>source.id!==n.id && ops.get(source.type).kind===expected.replace('?','')).forEach(source=>{
      const option=element('option','',`${source.id} · ${source.label}`); option.value=source.id; input.append(option);
    });
    input.value=n.inputs[port]??'';
    input.onchange=()=>{if(input.value)connect(Number(input.value),n.id,port,false);else disconnect(n.id,port,false);input.value=n.inputs[port]??'';};
    setting(lutTop,`${port==='mask'?'Apply within mask':`Input ${port.toUpperCase()}`} · ${expected}`,input);
  }
  const parameterOrder=op.parameters.map((_,i)=>i);
  if(n.type==='field_lut')renderLutEditor(settings,n);
  else parameterOrder.forEach(index=>{
    const param=op.parameters[index];
    const [name,,min,max,mode]=param;
    if(mode==='rule') {
      const row=element('div','rule-buttons');
      for(let bit=0;bit<=8;++bit) {
        const button=element('button',n.p[index]&(1<<bit)?'active':'',String(bit));
        button.setAttribute('aria-label',`${name}: ${bit}`); button.setAttribute('aria-pressed',String(Boolean(n.p[index]&(1<<bit))));
        button.onclick=()=>change(()=>{n.p[index]^=1<<bit;}); row.append(button);
      }
      setting(settings,name,row,true);
    } else if(mode==='material') {
      const input=element('input');input.type='number';input.min=0;input.max=255;input.step=1;input.value=n.p[index];
      input.setAttribute('aria-label',name);input.setAttribute('list','material-ids');
      input.onchange=()=>{if(input.value!=='' && input.validity.valid)change(()=>{n.p[index]=Number(input.value);});else input.value=n.p[index];};
      const row=setting(settings,name,input);row.append(element('span','material-name',materialCatalog.find(m=>m.id===n.p[index])?.name??'Unassigned ID · renders as sand'));
    } else if(mode) {
      const choices={toggle:[[0,'No'],[1,'Yes']],neighbours:[[4,'4 · cardinal'],[8,'8 · surrounding']],combine:[[0,'Union · A OR B'],[1,'Intersection · A AND B'],[2,'Subtract · A minus B'],[3,'Exclusive OR']],voronoi:[[0,'Nearest site (squared)'],[1,'F2 − F1 (squared)']]};
      const input=element('select'); input.setAttribute('aria-label',name);
      choices[mode].forEach(([value,text])=>{const o=element('option','',text);o.value=value;input.append(o);});
      input.value=n.p[index]; input.onchange=()=>change(()=>{n.p[index]=Number(input.value);}); setting(settings,name,input);
    } else {
      const row=element('div','range-row'), slider=element('input'), number=element('input');
      slider.type='range'; number.type='number';
      for(const input of [slider,number]) {input.min=min;input.max=max;input.step=1;input.value=n.p[index];input.setAttribute('aria-label',name+(input===slider?' slider':''));}
      let remembered=false;
      slider.onpointerdown=()=>{remember();remembered=true;};
      slider.oninput=()=>{
        if(!remembered) remember();
        n.p[index]=Number(slider.value); number.value=slider.value; iteration=null; save(); renderGraph(); schedule();
      };
      slider.onchange=()=>{remembered=false;};
      number.onchange=()=>{ const value=Number(number.value); if(!number.validity.valid || number.value===''){number.value=n.p[index];return;} change(()=>{n.p[index]=value;}); };
      row.append(slider,number); setting(settings,name,row);
    }
  });
  if(op.random) {
    const input=element('input'); input.type='number'; input.min=0;input.max=4294967295;input.step=1;input.value=n.stream;
    input.onchange=()=>{ if(input.validity.valid && input.value!=='') change(()=>{n.stream=Number(input.value);});else input.value=n.stream;};
    setting(settings,'Random stream · 0 preserves the original seed',input,true);
  }
  if(n.type==='cellular') {
    const range=element('input');range.type='range';range.min=0;range.max=n.p[0];range.value=iteration??n.p[0];
    range.setAttribute('aria-label','Preview through iteration');
    const row=setting(settings,`Preview through iteration ${iteration??n.p[0]} / ${n.p[0]} · preview only`,range,true);
    range.oninput=()=>{iteration=Number(range.value); row.firstChild.textContent=`Preview through iteration ${iteration} / ${n.p[0]} · preview only`;if(pinnedSettings===null)$('view').value='selected';schedule();};
    settings.append(element('p','setting-help','Rule counts exclude the center. The optional mask limits updates; the forced solid border always wins.'));
  }
  if(n.type==='roads') settings.append(element('p','setting-help','One breadth-first search connects all towns to the starting town along shortest floor paths. Road width is constant, in world pixels (up to 256), rendered on the 8-pixel tile grid. Wide roads are clipped by walls and settlement artwork.'));
  if(n.type==='world') settings.append(element('p','setting-help','Enforces a 2-cell border, retains connected floor, creates a fallback clearing when needed, then carves the spawn and six outposts. Compare with its input to inspect these changes.'));
  if(n.type==='largest') settings.append(element('p','setting-help','Retains the largest four-connected floor component. Equal sizes choose the first in row-major order. An all-wall input stays all walls; final world placement handles fallback.'));
  if(n.type==='field_paint')settings.append(element('p','setting-help','Replace the field value wherever the mask is 1. All other cells keep their input value.'));
  if(n.type==='materials'){
    settings.append(element('p','setting-help','Interpret each input field value as a ground material ID for one 128 × 128 world-pixel cell. This output controls ground art and grip independently of wall/floor.'));
    const legacy=element('button','','Use original ground rules');legacy.onclick=()=>change(()=>{delete recipe.materialOutput;if(recipe.version<3)recipe.version=1;});settings.append(legacy);
  }
  if(n.type==='spawns')settings.append(element('p','placement-help','Applied to the final world. Input A controls density: 0 forbids spawning and higher values increase likelihood. Input B is categorical: its byte value selects a Population profile for each anchor. Build regions with Constant field, Stepped LUT, and Paint field value.'));
  if(n.type==='decoration')settings.append(element('p','placement-help','Applied to the final world. Input A multiplies density (0 = none, 255 = full). Four type weights are normalized automatically; all zero disables decoration. Positions stay fixed when type weights change.'));
  root.append(settings);
}
function schedule() {
  clearTimeout(timer); ++sequence; $('preview').setAttribute('aria-busy','true');
  $('download-png').disabled=true; $('export').disabled=true;
  $('render-map').disabled=true;
  message(ready?'Generating…':'Loading the shared generation engine…');
  timer=setTimeout(generate,70);
}
function generate() {
  if(!ready) return;
  try {
    let final=null;
    try {final=compile(recipe,schema,recipe.output,false);} catch { /* Independent stages can still be previewed. */ }
    const view=$('view').value, requestedTarget=['selected','compare'].includes(view)?selected:recipe.output;
    const requestedNode=recipe.nodes.find(n=>n.id===requestedTarget);
    const target=requestedTarget;
    const previewRecipe=clone(recipe), node=previewRecipe.nodes.find(n=>n.id===target);
    const iterationNode=previewRecipe.nodes.find(n=>n.id===settingsNodeId()),settingsNode=iterationNode;
    if(iteration!==null && iterationNode?.type==='cellular') iterationNode.p[0]=iteration;
    let compiled=compile(previewRecipe,schema,target,false);
    const placement=['spawns','decoration'].includes(compiled.kind);
    const spawnTarget=node.type==='spawns'?node.id:recipe.spawnOutput,decorTarget=node.type==='decoration'?node.id:recipe.decorationOutput;
    let spawnProgram=null,decorationProgram=null;
    if(compiled.kind==='world' || placement){
      if(spawnTarget!=null)spawnProgram=compile(previewRecipe,schema,spawnTarget,false).program;
      if(decorTarget!=null)decorationProgram=compile(previewRecipe,schema,decorTarget,false).program;
      if(placement)compiled=compile(previewRecipe,schema,recipe.output,false);
    }
    let materialProgram=null,materialError='';
    if(recipe.materialOutput!=null){try{materialProgram=compile(previewRecipe,schema,recipe.materialOutput,false).program;}catch(error){materialError=error.message;}}
    const histogramProgram=settingsNode?.type==='field_lut'&&settingsNode.inputs?.a!=null?compile(previewRecipe,schema,settingsNode.inputs.a,false).program:null;
    if(compiled.kind==='world' && materialError)throw new Error(materialError);
    let exportError='';
    try { const all=compile(recipe,schema); if(all.kind!=='world') exportError='Choose a Playable world node as the ROM output.'; } catch(error) {exportError=error.message;}
    $('export').disabled=Boolean(exportError); $('export').title=exportError||'Download the procedural recipe';
    $('recipe-status').textContent=(final?`${recipe.nodes.length} NODES · WALL/FLOOR + ${materialProgram?'GROUND IDS':'ORIGINAL GROUND'} · v${recipe.version}`:`${recipe.nodes.length} NODES · OUTPUT NEEDS A CONNECTION`)+` · ${dirty?'UNSAVED':'SAVED'}`;
    $('render-map').disabled=renderBusy || Boolean(exportError) || !final;
    $('preview-title').textContent=view==='seeds'?'Nine possibilities':view==='placements'?'Portal & spawn layout':node.label;
    $('preview-label').textContent=view==='seeds'?'CLICK A SEED TO EXPLORE':compiled.kind==='field'?'FIELD · 0—255':compiled.kind==='material'?'GROUND MATERIAL IDS · 0—255':$('layer').value==='textures'?'GAME TEXTURES · 8192 × 8192':'64 × 64 / 8192 px';
    const compare=!placement && view==='compare' && node.inputs?.a ? compile(previewRecipe,schema,node.inputs.a,false).program : null;
    const seeds=view==='seeds'?Array.from({length:9},(_,i)=>(recipe.seed+i)>>>0):null;
    message(exportError || 'Generating…',exportError?'warning':'');
    const categoricalColors=node?.type==='field_lut'?lutPreviewColors(node):null;
    const schematic=view==='placements';
    worker.postMessage({id:sequence,mapId:currentId,program:compiled.program,materialProgram,spawnProgram,decorationProgram,spawnProfiles:recipe.spawnProfiles,portals:recipe.portals,playerSpawns:recipe.playerSpawns,histogramProgram,histogramNode:histogramProgram?settingsNode.id:null,categorical:!!categoricalColors,categoricalColors,showSpawns:$('show-spawns').checked,seed:recipe.seed,schematic,collision:(schematic||$('layer').value==='collision')&&!seeds,textures:!schematic&&$('layer').value==='textures',seeds,compare});
  } catch(error) {
    $('export').disabled=true; $('preview').setAttribute('aria-busy','false');
    $('preview-label').textContent='INVALID GRAPH · PREVIOUS PREVIEW'; message(error.message,'error');
  }
}
function regions(cells) {
  const labels=new Int32Array(4096),sizes=[]; let label=0;
  for(let i=0;i<4096;++i) if(!cells[i] && !labels[i]) {
    ++label; const queue=[i];labels[i]=label;
    for(let head=0;head<queue.length;++head) {
      const at=queue[head],x=at%64,y=at>>6;
      for(const next of [x?at-1:-1,x<63?at+1:-1,y?at-64:-1,y<63?at+64:-1])
        if(next>=0 && !cells[next] && !labels[next]) {labels[next]=label;queue.push(next);}
    }
    sizes.push(queue.length);
  }
  return {labels,count:label,largest:Math.max(0,...sizes)};
}
function paint(output,regionView=false,refined=true,categoricalColors=null) {
  if(output.texture){const canvas=document.createElement('canvas');canvas.width=canvas.height=output.texture.side;const ctx=canvas.getContext('2d');const pixels=ctx.createImageData(canvas.width,canvas.height);pixels.data.set(output.texture.pixels);ctx.putImageData(pixels,0,0);return canvas;}
  const source=refined && output.refined?output.refined:output.cells, side=source.length===4096?64:1024;
  const canvas=document.createElement('canvas');canvas.width=canvas.height=side;
  const ctx=canvas.getContext('2d'), pixels=ctx.createImageData(side,side), field=output.meta[1]===1;
  const labels=regionView && !field && side===64?regions(output.cells).labels:null;
  const palette=[[215,184,126],[129,175,148],[153,156,209],[213,144,110],[194,191,127],[142,191,206]];
  for(let i=0;i<source.length;++i) {
    let color;
    if(output.meta[1]===3){const hex=materialCatalog.find(m=>m.id===source[i])?.color??'#f04fbc';color=[1,3,5].map(at=>parseInt(hex.slice(at,at+2),16));}
    else if(field && categoricalColors){const hex=categoricalColors[source[i]]??defaultLutColor(source[i]);color=[1,3,5].map(at=>parseInt(hex.slice(at,at+2),16));}
    else if(field) {const t=source[i]/255;color=[31+t*203,55+t*168,48+t*119];}
    else color=source[i]?[37,66,55]:labels?palette[(labels[i]-1)%palette.length]:[215,184,126];
    pixels.data.set([...color,255],i*4);
  }
  ctx.putImageData(pixels,0,0);return canvas;
}
function markers(ctx,output,x,y,size) {
  if(output.meta[1]!==2) return;
  if($('show-spawns').checked && output.spawns){
    for(let i=0;i<output.spawns.length;i+=2){ctx.fillStyle=recipe.spawnProfiles.find(profile=>profile.id===output.spawnTypes?.[i/2])?.color??'#f04040';ctx.fillRect(x+output.spawns[i]/8192*size-1,y+output.spawns[i+1]/8192*size-1,3,3);}
  }
  for(let i=0;i<7;++i) {
    const px=output.meta[6+i*2]/8192*size+x,py=output.meta[7+i*2]/8192*size+y;
    ctx.fillStyle=i?'#effbc6':'#f7814e';ctx.strokeStyle='#253b2b';ctx.lineWidth=1.5;
    if(i) {ctx.fillRect(px-3.5,py-3.5,7,7);ctx.strokeRect(px-3.5,py-3.5,7,7);}
    else {ctx.beginPath();ctx.arc(px,py,5,0,Math.PI*2);ctx.fill();ctx.stroke();}
  }
}
function placementItems(){return [...recipe.portals.map(item=>({kind:'portal',item})),...recipe.playerSpawns.map(item=>({kind:'spawn',item}))];}
function placementKey(kind,id){return `${kind}:${id}`;}
function selectedPlacement(){return placementItems().find(value=>placementKey(value.kind,value.item.id)===placementSelection)??null;}
function placementScreen(x,y){const size=640*placementZoom;return {x:placementPanX+x/8192*size,y:placementPanY+y/8192*size};}
function placementWorld(x,y){const size=640*placementZoom;return {x:Math.max(0,Math.min(8191,Math.round((x-placementPanX)/size*8192/8)*8)),y:Math.max(0,Math.min(8191,Math.round((y-placementPanY)/size*8192/8)*8))};}
function schematicCanvas(output){
  const canvas=document.createElement('canvas');canvas.width=canvas.height=1024;const ctx=canvas.getContext('2d'),pixels=ctx.createImageData(1024,1024);
  const collision=output.refined??new Uint8Array(1024*1024),roads=output.roads??new Uint8Array(1024*1024);
  for(let i=0;i<collision.length;++i){const color=collision[i]?[34,57,47]:roads[i]?[177,137,80]:[202,187,145];pixels.data.set([...color,255],i*4);}
  ctx.putImageData(pixels,0,0);return canvas;
}
function drawPlacementView(ctx,output){
  const size=640*placementZoom;ctx.drawImage(schematicCanvas(output),placementPanX,placementPanY,size,size);
  for(let index=0;index<6;++index){const town=placementScreen(output.meta[8+index*2],output.meta[9+index*2]-32);ctx.fillStyle='#e7f6bcbb';ctx.strokeStyle='#253b2b';ctx.lineWidth=1.5;ctx.fillRect(town.x-5,town.y-5,10,10);ctx.strokeRect(town.x-5,town.y-5,10,10);ctx.fillStyle='#17211b';ctx.font='8px Consolas';ctx.fillText(`T${index+1}`,town.x-4,town.y+3);}
  const invalid=new Set(output.invalidPortals??[]),invalidSpawns=new Set(output.invalidPlayerSpawns??[]);
  for(const {kind,item} of placementItems()){
    const point=placementScreen(item.x,item.y),isSelected=placementSelection===placementKey(kind,item.id);ctx.save();
    if(kind==='portal'){
      const width=Math.max(12,item.width/8192*size),height=Math.max(12,item.height/8192*size);ctx.strokeStyle=invalid.has(item.id)?'#f06f58':isSelected?'#fff8cf':'#efad43';ctx.fillStyle=invalid.has(item.id)?'#f06f5838':'#efad432c';ctx.lineWidth=isSelected?3:2;ctx.fillRect(point.x-width/2,point.y-height/2,width,height);ctx.strokeRect(point.x-width/2,point.y-height/2,width,height);
    }else{
      const angle=(item.heading-90)*Math.PI/180,tip={x:point.x+Math.cos(angle)*22,y:point.y+Math.sin(angle)*22};ctx.strokeStyle=invalidSpawns.has(item.id)?'#f06f58':isSelected?'#ffffff':'#55d8df';ctx.fillStyle=invalidSpawns.has(item.id)?'#f06f58':'#55d8df';ctx.lineWidth=isSelected?3:2;ctx.beginPath();ctx.arc(point.x,point.y,isSelected?6:5,0,Math.PI*2);ctx.fill();ctx.stroke();ctx.beginPath();ctx.moveTo(point.x,point.y);ctx.lineTo(tip.x,tip.y);ctx.stroke();ctx.beginPath();ctx.arc(tip.x,tip.y,isSelected?4:3,0,Math.PI*2);ctx.fill();
    }
    ctx.fillStyle=isSelected?'#fff8cf':kind==='portal'?'#efc46c':'#9eeef1';ctx.font='10px Consolas';ctx.fillText(item.id,point.x+8,point.y-7);ctx.restore();
  }
}
function draw(data) {
  const canvas=$('preview'),ctx=canvas.getContext('2d'),layer=$('layer').value;
  ctx.imageSmoothingEnabled=false;ctx.fillStyle='#111c17';ctx.fillRect(0,0,640,640);
  const first=data.outputs[0],grid=data.outputs.length>1;
  const categoricalColors=data.categorical?data.categoricalColors??[]:null;
  const placementView=$('view').value==='placements';
  const legend=placementView?[['Floor','#cabb91'],['Wall','#22392f'],['Road','#b18950'],['Portal','#efad43'],['Player Spawn','#55d8df']]:categoricalColors?[...new Set(first.cells)].sort((a,b)=>a-b).map(value=>[`Output ${value}`,categoricalColors[value]??defaultLutColor(value)]):
    first.meta[1]===3 || first.texture ? materialCatalog.map(m=>[m.name,m.color]) : [['Floor','#d8b77b'],['Wall','#28483d'],['Spawn','#f08b5b'],['Outpost','#e7f6bc']];
  if(first.spawns && $('show-spawns').checked)for(const profile of recipe.spawnProfiles)legend.push([profile.name,profile.color]);
  $('legend').replaceChildren(...legend.map(([name,color])=>{const item=element('span'),swatch=element('i');swatch.style.background=color;item.append(swatch,document.createTextNode(name));return item;}));
  if(placementView){drawPlacementView(ctx,first);}
  else if(grid) {
    data.outputs.forEach((output,i)=>{
      const x=(i%3)*216,y=Math.floor(i/3)*216;
      ctx.drawImage(paint(output,layer==='regions',false,categoricalColors),x,y,208,186);
      ctx.fillStyle='#ccd8bc';ctx.font='13px Consolas,monospace';ctx.fillText(String(output.seed),x+5,y+204);
    });
  } else if(data.before) {
    const before=paint(data.before,layer==='regions',false),after=paint(first,layer==='regions',true,categoricalColors);
    ctx.drawImage(before,0,0,before.width/2,before.height,0,0,320,640);
    ctx.drawImage(after,after.width/2,0,after.width/2,after.height,320,0,320,640);
    ctx.fillStyle='#e6efcc';ctx.fillRect(319,0,2,640);
    ctx.fillStyle='#10251de8';ctx.fillRect(8,8,180,28);ctx.fillRect(328,8,180,28);
    ctx.fillStyle='#e3eccd';ctx.font='14px Consolas';ctx.fillText('INPUT / BEFORE',18,27);ctx.fillText('OUTPUT / AFTER',338,27);
  } else {ctx.drawImage(paint(first,layer==='regions',true,categoricalColors),0,0,640,640);if(!first.texture)markers(ctx,first,0,0,640);}
  const isField=first.meta[1]===1,stats=regions(first.cells);
  let changed=0;if(data.before) first.cells.forEach((v,i)=>{changed+=v!==data.before.cells[i];});
  const min=Math.min(...first.cells),max=Math.max(...first.cells);
  const items=[
    [isField?'Value range':'Floor coverage',isField?`${min}—${max}`:`${(first.meta[5]/4096*100).toFixed(1)}%`,''],
    [isField?'Output':'Floor regions',isField?'Field':String(stats.count),!isField?`${stats.largest} largest`:'0—255'],
    ['Live buffers',`${first.meta[2]} / 6`,`${first.meta[2]*4} KiB grids`],
    [data.before?'Changed cells':'Browser generation',data.before?String(changed):data.ms.toFixed(1),data.before?'of 4096':'ms']
  ];
  if(first.meta[1]===3){items[0]=['Material IDs',String(new Set(first.cells).size),'distinct'];items[1]=['ID range',`${min}—${max}`,''];}
  if(first.spawns){items[0]=['Spawn locations',String(first.spawns.length/2),first.requestedSpawns==null?'legacy':`/ ${first.requestedSpawns} requested`];items[1]=['Decoration patches',String(first.patches??0),'cosmetic'];}
  if(placementView){items[0]=['Portals',String(recipe.portals.length),`${first.invalidPortals?.length??0} invalid`];items[1]=['Player spawns',String(recipe.playerSpawns.length),`${first.invalidPlayerSpawns?.length??0} invalid`];}
  $('metrics').replaceChildren(...items.map(([name,value,unit])=>{
    const metric=element('div','metric');metric.append(element('span','',name));
    const strong=element('strong','',value);strong.append(element('small','',unit));metric.append(strong);return metric;
  }));
  const fallback=data.outputs.filter(o=>o.meta[3]).length;
  let text=`Signature ${first.meta[4].toString(16).padStart(8,'0')} · seed ${first.seed}.`;
  if(fallback) text+=` Fallback clearing used${grid?` in ${fallback} of 9 seeds`:''}.`;
  else if(first.meta[1]===2) text+=' Connected floor; spawn and outposts placed.';
  if(layer==='collision' && first.meta[1]!==2) text+=' Refined collision is available on Playable world outputs.';
  if($('view').value==='compare' && !data.before) text+=' This source node has no input to compare.';
  if(layer==='textures' && first.meta[1]!==2)text+=' Select a Playable world output for the textured map.';
  const unknown=[...new Set(data.outputs.flatMap(o=>o.unknown??[]))];
  if(first.requestedSpawns!=null && first.spawns.length/2<first.requestedSpawns)text+=` Placed ${first.spawns.length/2}/${first.requestedSpawns} spawns: floor, probability field or spacing limits available anchors.`;
  if(unknown.length)text+=` Unassigned material IDs: ${unknown.join(', ')}. IDs are preserved; game art and grip fall back to sand.`;
  if(placementView && first.invalidPortals?.length)text+=` Portal access needs drivable floor: ${first.invalidPortals.join(', ')}.`;
  if(placementView && first.invalidPlayerSpawns?.length)text+=` Player Spawns need drivable floor: ${first.invalidPlayerSpawns.join(', ')}.`;
  try {const valid=compile(recipe,schema);if(valid.kind!=='world')text+=' Choose a Playable world output to export.';}catch(error){text+=` Export unavailable: ${error.message}`;}
  message(text,fallback||unknown.length?'warning':'');
}

function uniquePlacementId(prefix,items){const ids=new Set(items.map(item=>item.id));let index=1,id=`${prefix}_${index}`;while(ids.has(id))id=`${prefix}_${++index}`;return id;}
function nearestDrivable(point){
  const collision=lastResponse?.outputs?.[0]?.refined;if(!collision)return point;
  const tx=Math.max(0,Math.min(1023,Math.round(point.x/8))),ty=Math.max(0,Math.min(1023,Math.round(point.y/8)));
  for(let radius=0;radius<96;++radius)for(let y=Math.max(0,ty-radius);y<=Math.min(1023,ty+radius);++y)for(const x of [tx-radius,tx+radius])if(x>=0&&x<1024&&!collision[y*1024+x])return {x:x*8+4,y:y*8+4};
  return point;
}
function addPlacement(kind,point){
  point=nearestDrivable(point);change(()=>{
    if(kind==='portal'){const item={id:uniquePlacementId('portal',recipe.portals),x:point.x,y:point.y,width:128,height:48};recipe.portals.push(item);placementSelection=placementKey('portal',item.id);}
    else {const item={id:uniquePlacementId('spawn',recipe.playerSpawns),x:point.x,y:point.y,heading:0};recipe.playerSpawns.push(item);placementSelection=placementKey('spawn',item.id);}
    recipe.version=Math.max(6,recipe.version);
  });renderPlacementControls();
}
function focusPlacement(){const selected=selectedPlacement();if(!selected)return;placementZoom=4;const point=placementScreen(selected.item.x,selected.item.y);placementPanX+=320-point.x;placementPanY+=320-point.y;if(lastResponse)draw(lastResponse);}
function renderPlacementControls(){
  const active=$('view').value==='placements';$('placement-tools').hidden=!active;$('placement-inspector').hidden=!active;$('placement-viewport').classList.toggle('placement-active',active);$('placement-viewport').closest('.preview-panel').classList.toggle('placement-mode',active);if(!active)return;
  const items=placementItems();if(!items.some(value=>placementKey(value.kind,value.item.id)===placementSelection))placementSelection=items.length?placementKey(items[0].kind,items[0].item.id):null;
  const list=$('placement-list');list.replaceChildren(...items.map(({kind,item})=>{const option=element('option','',`${kind==='portal'?'PORTAL':'SPAWN'} · ${item.id}`);option.value=placementKey(kind,item.id);return option;}));list.value=placementSelection??'';
  const selected=selectedPlacement(),root=$('placement-inspector');root.replaceChildren();if(!selected){root.append(element('span','muted','Add a Portal or Player Spawn, then click the map to place it.'));return;}
  const field=(label,key,min,max,className='')=>{const wrapper=element('label',className,label),input=element('input');input.type=key==='id'?'text':'number';input.value=selected.item[key];if(key==='id'){input.maxLength=24;input.pattern='[a-z0-9]+(?:_[a-z0-9]+)*';}else{input.min=min;input.max=max;input.step=1;}input.setAttribute('aria-label',`${selected.kind==='portal'?'Portal':'Player spawn'} ${label}`);input.onchange=()=>{let value=key==='id'?input.value.trim():Number(input.value);const siblings=selected.kind==='portal'?recipe.portals:recipe.playerSpawns;if(!input.validity.valid||value===''||(key==='id'&&siblings.some(item=>item!==selected.item&&item.id===value))){input.value=selected.item[key];return;}change(()=>{selected.item[key]=value;if(key==='id')placementSelection=placementKey(selected.kind,value);recipe.version=Math.max(6,recipe.version);});renderPlacementControls();};wrapper.append(input);root.append(wrapper);};
  field('ID','id',0,0,'placement-id');field('X','x',0,8191);field('Y','y',0,8191);if(selected.kind==='portal'){field('Width','width',16,512);field('Height','height',16,512);}else field('Heading','heading',0,359);
  const actions=element('div','placement-actions'),duplicate=element('button','','Duplicate'),remove=element('button','danger','Delete');duplicate.onclick=()=>change(()=>{const items=selected.kind==='portal'?recipe.portals:recipe.playerSpawns,item=clone(selected.item);item.id=uniquePlacementId(selected.kind==='portal'?'portal':'spawn',items);item.x=Math.min(8191,item.x+64);item.y=Math.min(8191,item.y+64);items.push(item);placementSelection=placementKey(selected.kind,item.id);});remove.onclick=()=>change(()=>{const items=selected.kind==='portal'?recipe.portals:recipe.playerSpawns,index=items.indexOf(selected.item);if(index>=0)items.splice(index,1);placementSelection=null;});actions.append(duplicate);
  if(selected.kind==='portal'){const arrival=element('button','','Create arrival spawn');arrival.onclick=()=>{const dx=4096-selected.item.x,dy=4096-selected.item.y,length=Math.max(1,Math.hypot(dx,dy)),distance=Math.max(selected.item.width,selected.item.height)/2+80,point=nearestDrivable({x:Math.round((selected.item.x+dx/length*distance)/8)*8,y:Math.round((selected.item.y+dy/length*distance)/8)*8});change(()=>{const item={id:uniquePlacementId(`${selected.item.id}_arrival`,recipe.playerSpawns),x:point.x,y:point.y,heading:(Math.round(Math.atan2(dy,dx)*180/Math.PI)+360)%360};recipe.playerSpawns.push(item);placementSelection=placementKey('spawn',item.id);recipe.version=Math.max(6,recipe.version);});renderPlacementControls();};actions.append(arrival);}
  actions.append(remove);root.append(actions);
}

function renderMapOptions(){
  const options=library.maps.map(entry=>{
    const option=element('option','',`${entry.recipe.name}${entry.includeInGame?'':' · DRAFT'}`);option.value=entry.id;return option;
  });
  if(currentId===null){const option=element('option','','NEW MAP · UNSAVED');option.value='';options.push(option);}
  $('preset').replaceChildren(...options);$('preset').value=currentId??'';
}
function updateMapControls(){
  renderMapOptions();
  $('save-map').disabled=!dirty;
  $('save-map').textContent=dirty?'Save *':'Save';
  $('delete-map').disabled=currentId===null||library.maps.length<=1;
  $('recipe-status').classList.toggle('dirty',dirty);
}
function resetEditorState(){
  selected=recipe.output??recipe.nodes[0]?.id??null;pending=null;pinnedSettings=null;lutPointSelection=null;
  undo=[];redo=[];iteration=null;panX=panY=0;zoom=1;placementZoom=1;placementPanX=placementPanY=0;placementDrag=null;placementSelection=recipe.portals.length?placementKey('portal',recipe.portals[0].id):recipe.playerSpawns.length?placementKey('spawn',recipe.playerSpawns[0].id):null;$('view').value='placements';
  $('undo').disabled=true;$('redo').disabled=true;
}
function discardAllowed(){return !dirty||window.confirm('Discard unsaved changes to this map?');}
function loadMap(id){
  const entry=library.maps.find(entry=>entry.id===id);if(!entry)return;
  currentId=id;currentEntry=entry;recipe=normalizePlacements(clone(entry.recipe));recipe.artProfile=normalizedArtProfile(recipe.artProfile);recipe.spawnProfiles=normalizeSpawnProfiles(recipe.spawnProfiles);recipe.shopProfile=normalizedShopProfile(recipe.shopProfile);includeInGame=entry.includeInGame;dirty=false;
  localStorage.setItem('dustline.map-selection.v1',id);localStorage.setItem('dustline.recipe.v1',JSON.stringify(recipe));localStorage.removeItem('dustline.map-draft.v1');
  resetEditorState();render();message(`Loaded ${recipe.name}.`);
}
function slug(name){
  const base=name.toLowerCase().replace(/[^a-z0-9]+/g,'-').replace(/^-|-$/g,'').slice(0,40)||'map';
  let id=base,suffix=2;while(library.maps.some(entry=>entry.id===id))id=`${base.slice(0,43-String(suffix).length)}-${suffix++}`;return id;
}
async function writeLibrary(next,nextId,success){
  try{
    reconcileWorld(next);
    const response=await fetch('/api/library',{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify({revision:libraryRevision,library:next})});
    const result=await response.json();if(!response.ok)throw new Error(result.error||`Save failed (${response.status}).`);
    library=result.library;libraryRevision=result.revision;currentId=nextId;currentEntry=library.maps.find(entry=>entry.id===currentId);
    recipe=normalizePlacements(clone(currentEntry.recipe));recipe.artProfile=normalizedArtProfile(recipe.artProfile);recipe.spawnProfiles=normalizeSpawnProfiles(recipe.spawnProfiles);recipe.shopProfile=normalizedShopProfile(recipe.shopProfile);includeInGame=currentEntry.includeInGame;dirty=false;
    localStorage.setItem('dustline.map-selection.v1',currentId);localStorage.setItem('dustline.recipe.v1',JSON.stringify(recipe));localStorage.removeItem('dustline.map-draft.v1');
    resetEditorState();render();message(success);
  }catch(error){message(error.message,'error');updateMapControls();}
}
async function saveMap(asCopy=false){
  let name=String(recipe.name??'').trim();
  if(asCopy){name=window.prompt('Name for the copied map',`${name||'Untitled map'} copy`)?.trim();if(!name)return;}
  recipe.name=name;
  if(includeInGame){
    try{const result=compile(recipe,schema);if(result.kind!=='world')throw new Error('Choose a Playable world output.');
      for(const key of ['materialOutput','spawnOutput','decorationOutput'])if(recipe[key]!=null)compile(recipe,schema,recipe[key]);
    }catch(error){message(`Cannot include this map in the game: ${error.message}`,'error');return;}
  }
  const next=clone(library);let id=currentId;
  if(asCopy||id===null){id=slug(name);next.maps.push({id,includeInGame,recipe:clone(recipe)});}
  else {const index=next.maps.findIndex(entry=>entry.id===id);next.maps[index]={id,includeInGame,recipe:clone(recipe)};}
  await writeLibrary(next,id,`${name} saved to maps/map-library.json.`);
}
$('preset').onchange=()=>{const id=$('preset').value;if(discardAllowed())loadMap(id);else $('preset').value=currentId??'';};
$('new-map').onclick=()=>{
  if(!discardAllowed())return;
  const name=window.prompt('Name for the new map','Untitled map')?.trim();if(!name)return;
  currentId=null;recipe={version:7,name,seed:crypto.getRandomValues(new Uint32Array(1))[0],nodes:[],portals:[],playerSpawns:[],artProfile:normalizedArtProfile('sun'),spawnProfiles:normalizeSpawnProfiles(),shopProfile:defaultShopProfile()};includeInGame=false;dirty=true;
  resetEditorState();save();render();message('New draft. Add nodes, then Save to add it to the shared library.');
};
$('save-map').onclick=()=>saveMap(false);$('save-as').onclick=()=>saveMap(true);
$('include-in-game').onchange=()=>{includeInGame=$('include-in-game').checked;save();message(includeInGame?'This map will appear in the next ROM build.':'Saved as an editor draft; it will not appear in the ROM.');};
$('delete-map').onclick=async()=>{
  if(currentId===null||!window.confirm(`Delete ${recipe.name} from the shared map library?`))return;
  const index=library.maps.findIndex(entry=>entry.id===currentId),next=clone(library);next.maps.splice(index,1);
  const replacement=next.maps[Math.min(index,next.maps.length-1)].id;
  await writeLibrary(next,replacement,`${recipe.name} deleted from the shared library.`);
};
function assetSelect(items,value,onchange){
  const select=element('select');
  for(const item of items){const option=element('option','',item.name);option.value=item.key;select.append(option);}
  select.value=value;select.onchange=()=>onchange(select.value);return select;
}
function previewImage(key,name){
  const image=element('img');image.src=`generated/previews/${key}.png`;image.alt=name;return image;
}
function artProfileError(){
  const enabled=recipe.artProfile.materials.filter(binding=>binding.enabled);
  if(!enabled.length)return 'Enable at least one ground material.';
  const ids=enabled.map(binding=>binding.id);
  if(new Set(ids).size!==ids.length)return 'Enabled ground materials need unique IDs.';
  return '';
}
function renderArtDialog(){
  syncMaterialCatalog();
  const profile=recipe.artProfile,bankIndex=currentId==null?undefined:artData.mapBanks[currentId],bank=bankIndex==null?null:artData.banks[bankIndex];
  const error=artProfileError();
  $('art-bank-status').textContent=error||`${bank?`${bank.uniqueTiles} unique terrain tiles / ${bank.tileSlots} reserved`:'Bank not built yet'} · ${profile.materials.filter(binding=>binding.enabled).length}/4 materials · ${profile.decorations.filter(binding=>binding.enabled).length}/4 decorations${dirty?' · rebuild required':''}`;
  $('art-bank-status').classList.toggle('error',Boolean(error));
  const materialRows=profile.materials.map((binding,index)=>{
    const asset=materialAssets.get(binding.asset),row=element('div','asset-binding');
    const enabled=element('input');enabled.type='checkbox';enabled.checked=binding.enabled;
    enabled.onchange=()=>{change(()=>{binding.enabled=enabled.checked;});renderArtDialog();};
    const id=element('input');id.type='number';id.min=0;id.max=255;id.step=1;id.value=binding.id;id.setAttribute('aria-label',`Material slot ${index+1} ID`);
    id.onchange=()=>{if(id.validity.valid&&id.value!=='')change(()=>{binding.id=Number(id.value);});renderArtDialog();};
    const select=assetSelect(assetCatalog.materials,binding.asset,value=>{change(()=>{binding.asset=value;});renderArtDialog();});
    const meta=element('span','asset-meta',`${asset.surface===1?'firm grip':'loose grip'} · 16 source tiles`);
    const toggle=element('label','');toggle.append(enabled,document.createTextNode(`Slot ${index+1}`));
    meta.prepend(toggle);row.append(previewImage(asset.key,asset.name),select,id,meta);return row;
  });
  $('material-bindings').replaceChildren(...materialRows);
  const decorationRows=profile.decorations.map((binding,index)=>{
    const asset=decorationAssets.get(binding.asset),row=element('div','asset-binding');
    const enabled=element('input');enabled.type='checkbox';enabled.checked=binding.enabled;
    enabled.onchange=()=>{change(()=>{binding.enabled=enabled.checked;});renderArtDialog();};
    const select=assetSelect(assetCatalog.decorations,binding.asset,value=>{change(()=>{binding.asset=value;});renderArtDialog();});
    const toggle=element('label','');toggle.append(enabled,document.createTextNode(`Slot ${index+1}`));
    row.append(previewImage(asset.key,asset.name),select,toggle,element('span','asset-meta',`Decoration type ${index+1} · 4 tiles`));return row;
  });
  $('decoration-bindings').replaceChildren(...decorationRows);
  const worldRows=[['Walls','wallSet',assetCatalog.wallSets],['Towns','townSet',assetCatalog.townSets]].map(([label,key,items])=>{
    const asset=items.find(item=>item.key===profile[key]),row=element('div','asset-binding world');
    const select=assetSelect(items,profile[key],value=>{change(()=>{profile[key]=value;});renderArtDialog();});
    row.append(previewImage(asset.key,asset.name),element('span','asset-meta',label),select);return row;
  });
  $('world-bindings').replaceChildren(...worldRows);
}
$('art-bank').onclick=()=>{renderArtDialog();$('art-dialog').showModal();};
$('close-art').onclick=()=>$('art-dialog').close();
function populationField(label,control){const wrapper=element('label','',label);wrapper.append(control);return wrapper;}
function renderPopulationDialog(){
  recipe.spawnProfiles=normalizeSpawnProfiles(recipe.spawnProfiles);
  const ids=recipe.spawnProfiles.map(profile=>profile.id),duplicate=new Set(ids).size!==ids.length;
  $('population-status').textContent=duplicate?'Profile IDs must be unique.':`${recipe.spawnProfiles.length}/8 profiles · field values without a matching profile use the first profile`;
  $('population-status').classList.toggle('error',duplicate);
  const rows=recipe.spawnProfiles.map((profile,index)=>{
    const row=element('div','population-binding');
    const color=element('input','profile-color');color.type='color';color.value=profile.color;
    color.oninput=()=>change(()=>{profile.color=color.value;recipe.version=Math.max(4,recipe.version);});
    const name=element('input');name.maxLength=18;name.value=profile.name;name.onchange=()=>change(()=>{profile.name=name.value.trim();recipe.version=Math.max(4,recipe.version);});
    const number=(value,min,max,apply)=>{const input=element('input');input.type='number';input.min=min;input.max=max;input.value=value;input.onchange=()=>{if(input.validity.valid&&input.value!=='')change(()=>{apply(Number(input.value));recipe.version=Math.max(4,recipe.version);});};return input;};
    const enemy=element('select');for(const [value,label] of [['scout','Scout'],['raider','Raider'],['heavy','Heavy']]){const option=element('option','',label);option.value=value;enemy.append(option);}enemy.value=profile.enemy;enemy.onchange=()=>change(()=>{profile.enemy=enemy.value;recipe.version=Math.max(4,recipe.version);});
    const remove=element('button','','Delete');remove.disabled=recipe.spawnProfiles.length===1;remove.onclick=()=>{change(()=>{recipe.spawnProfiles.splice(index,1);recipe.version=Math.max(4,recipe.version);});renderPopulationDialog();};
    row.append(populationField('Color',color),populationField('Name',name),populationField('ID',number(profile.id,0,255,value=>profile.id=value)),populationField('Enemy',enemy),populationField('Respawn s',number(profile.respawnSeconds,1,600,value=>profile.respawnSeconds=value)),populationField('Scrap %',number(profile.scrapChance,0,100,value=>profile.scrapChance=value)),populationField('Scrap min',number(profile.scrapMin,0,15,value=>profile.scrapMin=value)),populationField('Scrap max',number(profile.scrapMax,0,15,value=>profile.scrapMax=value)),populationField('Energy %',number(profile.energyChance,0,100,value=>profile.energyChance=value)),populationField('Energy min',number(profile.energyMin,0,100,value=>profile.energyMin=value)),populationField('Energy max',number(profile.energyMax,0,100,value=>profile.energyMax=value)),remove);
    return row;
  });
  $('population-bindings').replaceChildren(...rows);$('add-population').disabled=recipe.spawnProfiles.length>=8;
}
$('population-bank').onclick=()=>{renderPopulationDialog();$('population-dialog').showModal();};
$('close-population').onclick=()=>$('population-dialog').close();
$('add-population').onclick=()=>{if(recipe.spawnProfiles.length>=8)return;const ids=new Set(recipe.spawnProfiles.map(profile=>profile.id));let id=0;while(ids.has(id))++id;const colors=['#ef6c5b','#65b9dc','#e4bd57','#a889d6','#79bc7b','#d77faa','#8ac6b1','#d68e5d'];change(()=>{recipe.spawnProfiles.push({id,name:`Profile ${id}`,color:colors[recipe.spawnProfiles.length],enemy:'raider',respawnSeconds:30,scrapChance:70,scrapMin:1,scrapMax:3,energyChance:25,energyMin:8,energyMax:16});recipe.version=Math.max(4,recipe.version);});renderPopulationDialog();};

let shopPreviewRequest=0;
function shopNumber(label,value,min,max,apply,disabled=false){
  const input=element('input');input.type='number';input.min=min;input.max=max;input.step=1;input.value=value;input.disabled=disabled;input.setAttribute('aria-label',label);
  input.onchange=()=>{if(input.validity.valid&&input.value!==''){change(()=>{apply(Number(input.value));recipe.version=7;});renderShopDialog();}};
  return populationField(label,input);
}
async function requestShopPreview(){
  const request=++shopPreviewRequest;
  if(currentId===null){$('shop-status').textContent='Save this draft before previewing its stable region ID.';$('shop-preview').replaceChildren();return;}
  $('shop-status').textContent='Resolving exact inventoriesâ€¦';$('shop-status').classList.remove('error');
  try{
    const response=await fetch('/api/shop-preview',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({mapId:currentId,seed:recipe.seed,shopProfile:recipe.shopProfile})});
    const result=await response.json();if(request!==shopPreviewRequest)return;if(!response.ok)throw new Error(result.error||`Preview failed (${response.status}).`);
    $('shop-status').textContent=`${result.towns.length} towns Â· fixed for seed ${recipe.seed} Â· generation v1`;
    const rows=result.towns.map(town=>{const row=element('section','shop-preview-town'),heading=element('header');heading.append(element('strong','',`${town.index} Â· ${town.name}`),element('span','',`TIER ${town.tierCap}`));const list=element('ol');for(const item of town.items)list.append(element('li','',`${item.name} Â· ${item.family} T${item.tier}`));row.append(heading,list);return row;});
    $('shop-preview').replaceChildren(...rows);
  }catch(error){if(request!==shopPreviewRequest)return;$('shop-status').textContent=error.message;$('shop-status').classList.add('error');$('shop-preview').replaceChildren();}
}
function renderShopDialog(){
  recipe.shopProfile=normalizedShopProfile(recipe.shopProfile);const profile=recipe.shopProfile,families=['upgrade','front','side','top'];
  const base=element('div','shop-profile-grid');
  base.append(shopNumber('Shop tier floor',profile.tierFloor,1,255,value=>profile.tierFloor=value),shopNumber('Shop tier minimum',profile.townTierRange[0],1,255,value=>profile.townTierRange[0]=value),shopNumber('Shop tier maximum',profile.townTierRange[1],1,255,value=>profile.townTierRange[1]=value),shopNumber('Shop stock size',profile.stockSize,1,9,value=>profile.stockSize=value),...families.map(family=>shopNumber(`Shop ${family} weight`,profile.mixWeights[family],0,100,value=>profile.mixWeights[family]=value)));
  $('shop-profile-fields').replaceChildren(base);
  const modifiers=[];
  for(let town=0;town<6;++town){
    const row=element('div','shop-modifier-row'),existing=profile.townModifiers.find(value=>value.town===town),enabled=element('input');enabled.type='checkbox';enabled.checked=Boolean(existing);enabled.setAttribute('aria-label',`Town ${town} override`);
    enabled.onchange=()=>{change(()=>{if(enabled.checked)profile.townModifiers.push({town,tierOffset:0,stockDelta:0,mixWeights:{...profile.mixWeights}});else profile.townModifiers=profile.townModifiers.filter(value=>value.town!==town);recipe.version=7;});renderShopDialog();};
    const toggle=element('label','',`Town ${town}`);toggle.append(enabled);row.append(toggle,shopNumber(`Town ${town} tier offset`,existing?.tierOffset??0,-254,254,value=>existing.tierOffset=value,!existing),shopNumber(`Town ${town} stock delta`,existing?.stockDelta??0,-9,9,value=>existing.stockDelta=value,!existing),...families.map(family=>shopNumber(`Town ${town} ${family} weight`,existing?.mixWeights?.[family]??profile.mixWeights[family],0,100,value=>{existing.mixWeights??={};existing.mixWeights[family]=value;},!existing)));modifiers.push(row);
  }
  $('shop-modifiers').replaceChildren(...modifiers);requestShopPreview();
}
$('shop-profile').onclick=()=>{renderShopDialog();$('shop-dialog').showModal();};
$('close-shop').onclick=()=>$('shop-dialog').close();

function effectiveWorldMaps(){
  return library.maps.map(entry=>entry.id===currentId&&dirty?{...entry,includeInGame,recipe}:entry).filter(entry=>entry.includeInGame);
}
function mapPlayerSpawnIds(entry){return entry.recipe.playerSpawns.map(item=>item.id);}
function resetWorldDraft(){
  const snapshot={maps:effectiveWorldMaps().map(clone),world:clone(library.world??{})};reconcileWorld(snapshot);worldDraft=snapshot.world;
  worldPending=null;worldDirty=false;worldUndo=[];worldRedo=[];
}
function legacyWorldValidation(){
  const maps=effectiveWorldMaps();if(!maps.length)return 'Enable at least one map.';
  const used=new Set();for(const link of worldDraft.connections){const key=portalKey(link.from);if(used.has(key))return `${link.from.map} / ${link.from.portal} has more than one destination.`;used.add(key);}
  return '';
}
function legacyRenderWorldGraph(){
  const maps=effectiveWorldMaps(),byId=new Map(maps.map(entry=>[entry.id,entry])),used=new Set(worldDraft.connections.map(link=>portalKey(link.from)));
  const maxX=Math.max(1200,...worldDraft.nodes.map(node=>node.x+220)),maxY=Math.max(760,...worldDraft.nodes.map(node=>node.y+190));
  for(const id of ['world-wires','world-nodes']){const target=$(id);target.style.width=`${maxX}px`;target.style.height=`${maxY}px`;}
  const svg=$('world-wires');svg.setAttribute('viewBox',`0 0 ${maxX} ${maxY}`);svg.replaceChildren();
  const defs=document.createElementNS('http://www.w3.org/2000/svg','defs');
  for(const [id,orient] of [['world-arrow-end','auto'],['world-arrow-start','auto-start-reverse']]){const marker=document.createElementNS('http://www.w3.org/2000/svg','marker');marker.id=id;marker.setAttribute('viewBox','0 0 10 10');marker.setAttribute('refX','8');marker.setAttribute('refY','5');marker.setAttribute('markerWidth','7');marker.setAttribute('markerHeight','7');marker.setAttribute('orient',orient);const arrow=document.createElementNS('http://www.w3.org/2000/svg','path');arrow.setAttribute('d','M 0 0 L 10 5 L 0 10 z');arrow.setAttribute('fill','#eac35b');marker.append(arrow);defs.append(marker);}svg.append(defs);
  const endpointPoint=(endpoint,kind)=>{const layout=worldDraft.nodes.find(node=>node.map===endpoint.map),entry=byId.get(endpoint.map),portals=mapPortalIds(entry),index=kind==='portal'?portals.indexOf(endpoint.portal):mapPlayerSpawnIds(entry).indexOf(endpoint.spawn);return {x:layout.x+(kind==='portal'?8:168),y:layout.y+72+(kind==='portal'?index:portals.length+index)*25};};
  for(const link of worldDraft.connections){const a=endpointPoint(link.from,'portal'),b=endpointPoint(link.to,'spawn'),path=document.createElementNS('http://www.w3.org/2000/svg','path');path.setAttribute('d',`M${a.x},${a.y} C${a.x+(b.x-a.x)*.45},${a.y} ${b.x-(b.x-a.x)*.45},${b.y} ${b.x},${b.y}`);path.classList.add('world-wire','one-way');path.setAttribute('marker-end','url(#world-arrow-end)');svg.append(path);}
  const nodeElements=worldDraft.nodes.map(layout=>{
    const entry=byId.get(layout.map),node=element('div',`world-node${layout.map===worldDraft.start.map?' start':''}`);node.style.left=`${layout.x}px`;node.style.top=`${layout.y}px`;
    const heading=element('div','world-node-heading');heading.append(element('strong','',entry.recipe.name),element('small','',entry.id));
    heading.onpointerdown=event=>{if(event.button!==0)return;event.preventDefault();const startX=event.clientX,startY=event.clientY,originX=layout.x,originY=layout.y,pointer=event.pointerId;
      const move=e=>{if(e.pointerId!==pointer)return;node.style.left=`${Math.max(0,originX+e.clientX-startX)}px`;node.style.top=`${Math.max(0,originY+e.clientY-startY)}px`;};
      const end=e=>{if(e.pointerId!==pointer)return;window.removeEventListener('pointermove',move);window.removeEventListener('pointerup',end);window.removeEventListener('pointercancel',end);layout.x=Math.round(Math.max(0,originX+e.clientX-startX));layout.y=Math.round(Math.max(0,originY+e.clientY-startY));worldDirty=true;renderWorldGraph();};
      window.addEventListener('pointermove',move);window.addEventListener('pointerup',end);window.addEventListener('pointercancel',end);};
    const ports=element('div','world-gates');for(const portal of mapPortalIds(entry)){const endpoint={map:entry.id,portal},key=portalKey(endpoint),button=element('button',`world-gate portal${used.has(key)?' used':''}${worldPending===key?' pending':''}`,`PORTAL · ${portal}`);button.onclick=()=>{
      if(used.has(key)){$('world-status').textContent='That Portal already has a destination. Remove its transition below first.';return;}
      if(worldPending===key){worldPending=null;renderWorldGraph();return;}
      worldPending=key;renderWorldGraph();};ports.append(button);}
    for(const spawn of entry.recipe.playerSpawns){const button=element('button','world-gate spawn',`SPAWN · ${spawn.id} @ ${spawn.x},${spawn.y}`);button.onclick=()=>{if(!worldPending){$('world-status').textContent='Choose a source Portal first.';return;}const [map,...parts]=worldPending.split(':');worldDraft.connections.push({from:{map,portal:parts.join(':')},to:{map:entry.id,spawn:spawn.id}});worldPending=null;worldDirty=true;renderWorldGraph();};ports.append(button);}
    node.append(heading,ports);return node;
  });$('world-nodes').replaceChildren(...nodeElements);
  const start=$('world-start');start.replaceChildren(...maps.map(entry=>{const option=element('option','',entry.recipe.name);option.value=entry.id;return option;}));start.value=worldDraft.start.map??'';const startEntry=byId.get(worldDraft.start.map),startSpawn=$('world-start-spawn');startSpawn.replaceChildren(...startEntry.recipe.playerSpawns.map(spawn=>{const option=element('option','',`${spawn.id} @ ${spawn.x},${spawn.y}`);option.value=spawn.id;return option;}));startSpawn.value=worldDraft.start.spawn;
  const links=worldDraft.connections.map((link,index)=>{const destination=byId.get(link.to.map)?.recipe.playerSpawns.find(spawn=>spawn.id===link.to.spawn),position=destination?` @ ${destination.x},${destination.y}`:'',row=element('div','world-link'),label=element('span','',`${link.from.map} / ${link.from.portal}  →  ${link.to.map} / ${link.to.spawn}${position}`),remove=element('button','','Remove');remove.onclick=()=>{worldDraft.connections.splice(index,1);worldDirty=true;renderWorldGraph();};row.append(label,remove);return row;});
  $('world-links').replaceChildren(...links);const error=worldValidation(),portalCount=maps.reduce((total,entry)=>total+mapPortalIds(entry).length,0),unconnected=portalCount-used.size;$('world-status').textContent=worldPending?`Choose a destination Player Spawn for ${worldPending}.`:error||`${maps.length} regions · ${worldDraft.connections.length} transitions${unconnected?` · ${unconnected} unconnected Portal${unconnected===1?'':'s'}`:' · all Portals connected'}`;$('save-world').disabled=Boolean(error);
}
const cardinalSides=['north','east','south','west'];
const oppositeSide={north:'south',east:'west',south:'north',west:'east'};
const cardinalDelta={north:[0,-1],east:[1,0],south:[0,1],west:[-1,0]};
const sideKey=endpoint=>`${endpoint.map}:${endpoint.side}`;

function reconcileWorld(target){
  const maps=target.maps.filter(entry=>entry.includeInGame),existing=target.world?.version===3?target.world:{},validMapIds=new Set(maps.map(entry=>entry.id));
  const positioned=new Map();for(const node of Array.isArray(existing.nodes)?existing.nodes:[])if(validMapIds.has(node?.map)&&!positioned.has(node.map))positioned.set(node.map,node);
  const nodes=maps.map((entry,index)=>{const old=positioned.get(entry.id);return {map:entry.id,x:old?.x??80+(index%4)*250,y:old?.y??70+Math.floor(index/4)*210};});
  const used=new Set(),connections=[];
  for(const link of Array.isArray(existing.connections)?existing.connections:[]){const a=link?.a,b=link?.b,keyA=a?sideKey(a):'',keyB=b?sideKey(b):'';
    if(validMapIds.has(a?.map)&&validMapIds.has(b?.map)&&a.map!==b.map&&cardinalSides.includes(a.side)&&b.side===oppositeSide[a.side]&&!used.has(keyA)&&!used.has(keyB)){used.add(keyA);used.add(keyB);connections.push(clone(link));if(connections.length===128)break;}}
  const startMap=validMapIds.has(existing.start?.map)?existing.start.map:maps[0]?.id,startEntry=maps.find(entry=>entry.id===startMap),spawnIds=startEntry?mapPlayerSpawnIds(startEntry):[];
  const startSpawn=spawnIds.includes(existing.start?.spawn)?existing.start.spawn:spawnIds[0];
  target.world={version:3,start:{map:startMap,spawn:startSpawn},nodes,connections};return target;
}
function derivedWorldGrid(maps){
  const used=new Set(),coordinates=new Map(),adjacency=new Map(maps.map(entry=>[entry.id,[]]));
  for(const link of worldDraft.connections){if(!link?.a||!link?.b||link.a.map===link.b.map||link.b.side!==oppositeSide[link.a.side])return {error:'Connections must join complementary sides on different regions.'};for(const endpoint of [link.a,link.b]){const key=sideKey(endpoint);if(used.has(key))return {error:`${endpoint.map} / ${endpoint.side} has more than one connection.`};used.add(key);}const [dx,dy]=cardinalDelta[link.a.side];adjacency.get(link.a.map)?.push([link.b.map,dx,dy]);adjacency.get(link.b.map)?.push([link.a.map,-dx,-dy]);}
  let component=0;for(const root of [worldDraft.start.map,...maps.map(entry=>entry.id)]){if(coordinates.has(root)||!adjacency.has(root))continue;coordinates.set(root,[0,0,component]);const owners=new Map([['0,0',root]]),queue=[root];while(queue.length){const source=queue.shift(),[x,y]=coordinates.get(source);for(const [target,dx,dy] of adjacency.get(source)){const expected=[x+dx,y+dy],old=coordinates.get(target);if(old){if(old[2]===component&&(old[0]!==expected[0]||old[1]!==expected[1]))return {error:'Connections create an inconsistent cardinal cycle.'};continue;}const key=expected.join(','),owner=owners.get(key);if(owner&&owner!==target)return {error:`${owner} and ${target} overlap on the derived grid.`};coordinates.set(target,[...expected,component]);owners.set(key,target);queue.push(target);}}component++;}
  return {used,coordinates,components:component};
}
function worldValidation(){const maps=effectiveWorldMaps();if(!maps.length)return 'Enable at least one map.';return derivedWorldGrid(maps).error??'';}
function rememberWorld(){worldUndo.push(clone(worldDraft));if(worldUndo.length>64)worldUndo.shift();worldRedo=[];worldDirty=true;}
function restoreWorld(source,destination){if(!source.length)return;destination.push(clone(worldDraft));worldDraft=source.pop();worldPending=null;worldDirty=true;renderWorldGraph();}
function renderWorldGraph(){
  const maps=effectiveWorldMaps(),byId=new Map(maps.map(entry=>[entry.id,entry])),derived=derivedWorldGrid(maps),used=derived.used??new Set();
  const maxX=Math.max(1200,...worldDraft.nodes.map(node=>node.x+220)),maxY=Math.max(760,...worldDraft.nodes.map(node=>node.y+190));
  for(const id of ['world-wires','world-nodes']){const target=$(id);target.style.width=`${maxX}px`;target.style.height=`${maxY}px`;}
  const svg=$('world-wires');svg.setAttribute('viewBox',`0 0 ${maxX} ${maxY}`);svg.replaceChildren();
  const endpointPoint=endpoint=>{const layout=worldDraft.nodes.find(node=>node.map===endpoint.map),points={north:[88,0],east:[176,55],south:[88,110],west:[0,55]},point=points[endpoint.side];return {x:layout.x+point[0],y:layout.y+point[1]};};
  for(const link of worldDraft.connections){const a=endpointPoint(link.a),b=endpointPoint(link.b),path=document.createElementNS('http://www.w3.org/2000/svg','path');path.setAttribute('d',`M${a.x},${a.y} C${a.x+(b.x-a.x)*.45},${a.y} ${b.x-(b.x-a.x)*.45},${b.y} ${b.x},${b.y}`);path.classList.add('world-wire');svg.append(path);}
  const nodeElements=worldDraft.nodes.map(layout=>{const entry=byId.get(layout.map),node=element('div',`world-node${layout.map===worldDraft.start.map?' start':''}`);node.style.left=`${layout.x}px`;node.style.top=`${layout.y}px`;
    const heading=element('div','world-node-heading'),grid=derived.coordinates?.get(entry.id);heading.append(element('strong','',entry.recipe.name),element('small','',grid?`${entry.id} [${grid[0]},${grid[1]}]`:entry.id));
    heading.onpointerdown=event=>{if(event.button!==0)return;event.preventDefault();const startX=event.clientX,startY=event.clientY,originX=layout.x,originY=layout.y,pointer=event.pointerId;const move=e=>{if(e.pointerId!==pointer)return;node.style.left=`${Math.max(0,originX+e.clientX-startX)}px`;node.style.top=`${Math.max(0,originY+e.clientY-startY)}px`;};const end=e=>{if(e.pointerId!==pointer)return;window.removeEventListener('pointermove',move);window.removeEventListener('pointerup',end);window.removeEventListener('pointercancel',end);const nextX=Math.round(Math.max(0,originX+e.clientX-startX)),nextY=Math.round(Math.max(0,originY+e.clientY-startY));if(nextX!==originX||nextY!==originY){rememberWorld();layout.x=nextX;layout.y=nextY;}renderWorldGraph();};window.addEventListener('pointermove',move);window.addEventListener('pointerup',end);window.addEventListener('pointercancel',end);};
    const ports=element('div','world-gates');for(const side of cardinalSides){const endpoint={map:entry.id,side},key=sideKey(endpoint),button=element('button',`world-gate cardinal ${side}${used.has(key)?' used':''}${worldPending===key?' pending':''}`,side[0].toUpperCase());button.title=`${entry.id} ${side}`;button.onclick=()=>{if(used.has(key)){$('world-status').textContent='That side is already connected. Remove its connection below first.';return;}if(worldPending===key){worldPending=null;renderWorldGraph();return;}if(!worldPending){worldPending=key;renderWorldGraph();return;}const split=worldPending.lastIndexOf(':'),map=worldPending.slice(0,split),pendingSide=worldPending.slice(split+1);if(map===entry.id){$('world-status').textContent='Choose a different region.';return;}if(side!==oppositeSide[pendingSide]){$('world-status').textContent=`Choose the ${oppositeSide[pendingSide]} side of another region.`;return;}const id=`${map}_${pendingSide}_${entry.id}_${side}`.replace(/[^a-z0-9_-]+/g,'-');rememberWorld();worldDraft.connections.push({id,a:{map,side:pendingSide},b:{map:entry.id,side},requirement:null});worldPending=null;renderWorldGraph();};ports.append(button);}node.append(heading,ports);return node;});
  $('world-nodes').replaceChildren(...nodeElements);
  const start=$('world-start');start.replaceChildren(...maps.map(entry=>{const option=element('option','',entry.recipe.name);option.value=entry.id;return option;}));start.value=worldDraft.start.map??'';const startEntry=byId.get(worldDraft.start.map),startSpawn=$('world-start-spawn');startSpawn.replaceChildren(...(startEntry?.recipe.playerSpawns??[]).map(spawn=>{const option=element('option','',`${spawn.id} @ ${spawn.x},${spawn.y}`);option.value=spawn.id;return option;}));startSpawn.value=worldDraft.start.spawn;
  const links=worldDraft.connections.map((link,index)=>{const row=element('div','world-link'),label=element('span','',`${link.a.map} / ${link.a.side}  ↔  ${link.b.map} / ${link.b.side}`),remove=element('button','','Remove');remove.onclick=()=>{rememberWorld();worldDraft.connections.splice(index,1);renderWorldGraph();};row.append(label,remove);return row;});$('world-links').replaceChildren(...links);
  const error=derived.error??'',unconnected=maps.length*4-used.size;$('world-status').textContent=worldPending?`Choose the complementary side for ${worldPending}.`:error||`${maps.length} regions · ${worldDraft.connections.length} reciprocal connections · ${unconnected} free sides`;$('save-world').disabled=Boolean(error);$('world-undo').disabled=!worldUndo.length;$('world-redo').disabled=!worldRedo.length;
}

$('world-start').onchange=()=>{rememberWorld();worldDraft.start.map=$('world-start').value;worldDraft.start.spawn=mapPlayerSpawnIds(effectiveWorldMaps().find(entry=>entry.id===worldDraft.start.map))[0];renderWorldGraph();};
$('world-start-spawn').onchange=()=>{rememberWorld();worldDraft.start.spawn=$('world-start-spawn').value;renderWorldGraph();};
$('world-undo').onclick=()=>restoreWorld(worldUndo,worldRedo);
$('world-redo').onclick=()=>restoreWorld(worldRedo,worldUndo);
$('world-map').onclick=()=>{resetWorldDraft();renderWorldGraph();$('world-dialog').showModal();};
$('close-world').onclick=()=>{if(!worldDirty||window.confirm('Discard unsaved world-map changes?'))$('world-dialog').close();};
$('save-world').onclick=async()=>{
  const error=worldValidation();if(error){$('world-status').textContent=error;return;}if(dirty&&currentId===null){$('world-status').textContent='Save this new map before adding it to the world.';return;}
  const next=clone(library);if(dirty&&currentId!==null){const index=next.maps.findIndex(entry=>entry.id===currentId);next.maps[index]={id:currentId,includeInGame,recipe:clone(recipe)};}next.world=clone(worldDraft);reconcileWorld(next);
  try{const response=await fetch('/api/library',{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify({revision:libraryRevision,library:next})}),result=await response.json();if(!response.ok)throw new Error(result.error||`Save failed (${response.status}).`);library=result.library;libraryRevision=result.revision;worldDirty=false;
    if(currentId!==null){currentEntry=library.maps.find(entry=>entry.id===currentId);recipe=normalizePlacements(clone(currentEntry.recipe));recipe.artProfile=normalizedArtProfile(recipe.artProfile);recipe.spawnProfiles=normalizeSpawnProfiles(recipe.spawnProfiles);recipe.shopProfile=normalizedShopProfile(recipe.shopProfile);includeInGame=currentEntry.includeInGame;dirty=false;localStorage.setItem('dustline.recipe.v1',JSON.stringify(recipe));localStorage.removeItem('dustline.map-draft.v1');resetEditorState();render();}
    resetWorldDraft();renderWorldGraph();$('world-status').textContent='World routes saved to maps/map-library.json.';
  }catch(saveError){$('world-status').textContent=saveError.message;}
};
document.querySelector('.section-label span').textContent=schema.operations.length;
syncMaterialCatalog();
schema.operations.forEach(op=>{
  const button=element('button',op.kind);button.append(element('span','',op.kind==='field'?'≈':op.id==='world'?'↗':'+'),document.createTextNode(op.name));
  button.onclick=()=>{
    if(recipe.nodes.length>=MAX_NODES){message(`This recipe already has ${MAX_NODES} nodes.`,'error');return;}
    change(()=>{
      const id=Math.max(0,...recipe.nodes.map(n=>n.id))+1,n=makeNode(schema,op.id,id,35+recipe.nodes.length%3*220,45+Math.floor(recipe.nodes.length/3)*190);
      for(const [port,expected] of Object.entries(op.inputs)) {
        if(expected.endsWith('?'))continue;
        const matches=recipe.nodes.filter(s=>ops.get(s.type).kind===expected);
        const source=matches.find(s=>s.id===selected)??matches.at(-1);if(source)n.inputs[port]=source.id;
      }
      recipe.nodes.push(n);selected=id;$('view').value='selected';
      if(op.kind==='world')recipe.output=id;
      if(op.id==='materials'){recipe.version=Math.max(2,recipe.version);recipe.materialOutput=id;}
      if(op.id==='spawns'){recipe.version=Math.max(4,recipe.version);recipe.spawnProfiles=normalizeSpawnProfiles(recipe.spawnProfiles);recipe.spawnOutput=id;}
      if(op.id==='decoration'){recipe.version=Math.max(3,recipe.version);recipe.decorationOutput=id;}
    });
    const added=recipe.nodes.at(-1);panX=$('graph').clientWidth/2-((Number(added.x)||0)+95)*zoom;panY=$('graph').clientHeight/2-((Number(added.y)||0)+70)*zoom;renderGraph();
  };
  $('library').append(button);
});
$('undo').onclick=()=>restore(undo,redo);$('redo').onclick=()=>restore(redo,undo);
$('arrange').onclick=()=>{change(arrange);fitGraph();};
function setZoom(value,anchorX=$('graph').clientWidth/2,anchorY=$('graph').clientHeight/2){
  const next=Math.max(.25,Math.min(1.5,value));if(next===zoom)return;
  const worldX=(anchorX-panX)/zoom,worldY=(anchorY-panY)/zoom;
  panX=anchorX-worldX*next;panY=anchorY-worldY*next;zoom=next;renderGraph();
}
$('zoom-in').onclick=()=>setZoom(zoom+.1);$('zoom-out').onclick=()=>setZoom(zoom-.1);
function fitGraph(){
  if(!recipe.nodes.length){zoom=1;panX=panY=0;renderGraph();return;}
  const minX=Math.min(...recipe.nodes.map(n=>Number(n.x)||0)),minY=Math.min(...recipe.nodes.map(n=>Number(n.y)||0));
  const maxX=Math.max(...recipe.nodes.map(n=>(Number(n.x)||0)+230)),maxY=Math.max(...recipe.nodes.map(n=>(Number(n.y)||0)+210));
  const width=maxX-minX,height=maxY-minY,padding=16;
  zoom=Math.max(.25,Math.min(1,($('graph').clientWidth-padding*2)/width,($('graph').clientHeight-padding*2)/height));
  panX=($('graph').clientWidth-width*zoom)/2-minX*zoom;panY=($('graph').clientHeight-height*zoom)/2-minY*zoom;renderGraph();
}
$('fit-graph').onclick=fitGraph;
$('graph').addEventListener('wheel',event=>{
  event.preventDefault();const rect=$('graph').getBoundingClientRect();
  setZoom(zoom*(event.deltaY<0?1.1:1/1.1),event.clientX-rect.left,event.clientY-rect.top);
},{passive:false});
$('graph').addEventListener('pointerdown',event=>{
  if(event.button!==0||event.target.closest('.node'))return;
  event.preventDefault();const pointerId=event.pointerId,startX=event.clientX,startY=event.clientY,originX=panX,originY=panY;
  $('graph').classList.add('panning');
  const move=e=>{if(e.pointerId!==pointerId)return;panX=originX+e.clientX-startX;panY=originY+e.clientY-startY;applyGraphView();};
  const end=e=>{if(e.pointerId!==pointerId)return;window.removeEventListener('pointermove',move);window.removeEventListener('pointerup',end);window.removeEventListener('pointercancel',end);$('graph').classList.remove('panning');};
  window.addEventListener('pointermove',move);window.addEventListener('pointerup',end);window.addEventListener('pointercancel',end);
});
$('recipe-name').onchange=()=>change(()=>{recipe.name=$('recipe-name').value.trim();},false);
$('seed').onchange=()=>{const input=$('seed');if(!input.validity.valid||input.value===''){input.value=recipe.seed;return;}change(()=>{recipe.seed=Number(input.value);},false);};
$('random-seed').onclick=()=>change(()=>{recipe.seed=crypto.getRandomValues(new Uint32Array(1))[0];});
$('view').onchange=()=>{renderPlacementControls();schedule();};$('layer').onchange=schedule;$('show-spawns').onchange=schedule;
$('pin-settings').onclick=()=>{pinnedSettings=pinnedSettings===null?selected:null;iteration=null;renderGraph();renderInspector();schedule();};
$('duplicate').onclick=()=>change(()=>{
  const n=clone(recipe.nodes.find(n=>n.id===settingsNodeId()));n.id=Math.max(...recipe.nodes.map(n=>n.id))+1;n.label+=' copy';n.x+=30;n.y+=160;
  if(ops.get(n.type).random)n.stream=n.id;recipe.nodes.push(n);
  if(pinnedSettings!==null)pinnedSettings=n.id;else {selected=n.id;$('view').value='selected';}
});
$('delete').onclick=()=>change(()=>{
  const removed=settingsNodeId();
  recipe.nodes=recipe.nodes.filter(n=>n.id!==removed);
  for(const n of recipe.nodes)for(const port of Object.keys(n.inputs))if(n.inputs[port]===removed)delete n.inputs[port];
  if(recipe.output===removed)recipe.output=recipe.nodes.findLast(n=>['world','roads'].includes(n.type))?.id??recipe.nodes.at(-1).id;
  if(recipe.materialOutput===removed){delete recipe.materialOutput;if(recipe.version<3)recipe.version=1;}
  for(const key of ['spawnOutput','decorationOutput'])if(recipe[key]===removed)delete recipe[key];
  if(selected===removed)selected=recipe.output;
  pending=null;
});
$('set-output').onclick=()=>change(()=>{const type=recipe.nodes.find(n=>n.id===selected).type;
  if(type==='materials'){recipe.materialOutput=selected;recipe.version=Math.max(2,recipe.version);}
  else if(type==='spawns'){recipe.spawnOutput=selected;recipe.version=Math.max(4,recipe.version);recipe.spawnProfiles=normalizeSpawnProfiles(recipe.spawnProfiles);}
  else if(type==='decoration'){recipe.decorationOutput=selected;recipe.version=Math.max(3,recipe.version);}
  else recipe.output=selected;$('view').value='final';});
function download(blob,name){const url=URL.createObjectURL(blob),a=element('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
$('export').onclick=()=>{
  try{const compiled=compile(recipe,schema);if(compiled.kind!=='world')throw new Error('Select a Playable world output.');
    for(const key of ['materialOutput','spawnOutput','decorationOutput'])if(recipe[key]!=null)compile(recipe,schema,recipe[key]);
    download(new Blob([JSON.stringify(recipe,null,2)+'\n'],{type:'application/json'}),`${currentId??slug(recipe.name)}.json`);
    message('Standalone recipe exported for backup or sharing. Save writes directly to the shared map library.');
  }catch(error){message(error.message,'error');}
};
$('import').onclick=()=>$('import-file').click();
$('import-file').onchange=async()=>{
  const file=$('import-file').files[0];if(!file)return;
  try{
    if(file.size>256*1024)throw new Error('Recipe file is too large (limit 256 KiB).');
    const imported=JSON.parse(await file.text());compile(imported,schema);
    for(const n of imported.nodes){n.x=Math.max(0,Math.min(10000,Number(n.x)||0));n.y=Math.max(0,Math.min(10000,Number(n.y)||0));n.inputs??={};n.label=String(n.label??ops.get(n.type).name).slice(0,80);}
    if(!discardAllowed())return;
    currentId=null;recipe=imported;recipe.artProfile=normalizedArtProfile(recipe.artProfile);recipe.spawnProfiles=normalizeSpawnProfiles(recipe.spawnProfiles);recipe.shopProfile=normalizedShopProfile(recipe.shopProfile);includeInGame=false;dirty=true;resetEditorState();save();render();
    message('Imported as a new draft. Use Save to add it to the shared library.');
  }catch(error){message(`Import failed: ${error.message}`,'error');}
  $('import-file').value='';
};
$('download-png').onclick=()=>$('preview').toBlob(blob=>download(blob,`dustline-${recipe.seed}.png`));
$('render-map').onclick=()=>{
  try {
    const previewRecipe=clone(recipe),iterationNode=previewRecipe.nodes.find(n=>n.id===settingsNodeId());
    if(iteration!==null && iterationNode?.type==='cellular')iterationNode.p[0]=iteration;
    const world=compile(previewRecipe,schema),materialProgram=recipe.materialOutput==null?null:compile(previewRecipe,schema,recipe.materialOutput).program;
    if(world.kind!=='world')throw new Error('Choose a Playable world output.');
    renderRequest++;renderBusy=true;renderBlob=null;$('save-render').disabled=true;$('render-map').disabled=true;
    $('render-image').removeAttribute('src');$('render-status').textContent='Rendering full map…';$('render-dialog').showModal();
    const spawnProgram=recipe.spawnOutput==null?null:compile(previewRecipe,schema,recipe.spawnOutput).program;
    const decorationProgram=recipe.decorationOutput==null?null:compile(previewRecipe,schema,recipe.decorationOutput).program;
    worker.postMessage({id:renderRequest,render:true,mapId:currentId,program:world.program,materialProgram,spawnProgram,decorationProgram,spawnProfiles:recipe.spawnProfiles,portals:recipe.portals,playerSpawns:recipe.playerSpawns,showSpawns:$('show-spawns').checked,seed:recipe.seed});
  }catch(error){message(error.message,'error');}
};
$('close-render').onclick=()=>$('render-dialog').close();
$('save-render').onclick=()=>{if(renderBlob)download(renderBlob,`dustline-world-${renderSeed}-8192.png`);};
$('render-zoom').onchange=()=>{$('render-image').style.width=$('render-zoom').value==='fit'?'':`${8192*Number($('render-zoom').value)}px`;$('render-image').classList.toggle('actual-size',$('render-zoom').value!=='fit');};
$('preview').onclick=event=>{
  const rect=$('preview').getBoundingClientRect(),x=(event.clientX-rect.left)/rect.width,y=(event.clientY-rect.top)/rect.height;
  if($('view').value==='placements')return;
  if($('view').value!=='seeds'||!lastResponse)return;
  const output=lastResponse.outputs[Math.min(2,Math.floor(y*3))*3+Math.min(2,Math.floor(x*3))];
  if(output)change(()=>{recipe.seed=output.seed;$('view').value='final';});
};
$('add-portal').onclick=()=>{placementAddKind=placementAddKind==='portal'?null:'portal';message(placementAddKind?'Click the schematic to place the Portal.':'Portal placement cancelled.');};
$('add-player-spawn').onclick=()=>{placementAddKind=placementAddKind==='spawn'?null:'spawn';message(placementAddKind?'Click the schematic to place the Player Spawn.':'Player Spawn placement cancelled.');};
$('placement-list').onchange=()=>{placementSelection=$('placement-list').value||null;renderPlacementControls();if(lastResponse)draw(lastResponse);};
$('placement-focus').onclick=focusPlacement;
$('placement-fit').onclick=()=>{placementZoom=1;placementPanX=placementPanY=0;if(lastResponse)draw(lastResponse);};
function previewPoint(event){const rect=$('preview').getBoundingClientRect();return {x:(event.clientX-rect.left)/rect.width*640,y:(event.clientY-rect.top)/rect.height*640};}
$('preview').addEventListener('wheel',event=>{
  if($('view').value!=='placements')return;event.preventDefault();const point=previewPoint(event),next=Math.max(1,Math.min(16,placementZoom*(event.deltaY<0?1.25:.8))),worldX=(point.x-placementPanX)/placementZoom,worldY=(point.y-placementPanY)/placementZoom;placementPanX=point.x-worldX*next;placementPanY=point.y-worldY*next;placementZoom=next;if(lastResponse)draw(lastResponse);
},{passive:false});
$('preview').addEventListener('pointerdown',event=>{
  if($('view').value!=='placements'||event.button!==0)return;event.preventDefault();const point=previewPoint(event);
  if(placementAddKind){const kind=placementAddKind;placementAddKind=null;addPlacement(kind,placementWorld(point.x,point.y));return;}
  let hit=null,mode='move';for(const value of placementItems().reverse()){
    const center=placementScreen(value.item.x,value.item.y),distance=Math.hypot(point.x-center.x,point.y-center.y);if(value.kind==='spawn'){
      const angle=(value.item.heading-90)*Math.PI/180,tip={x:center.x+Math.cos(angle)*22,y:center.y+Math.sin(angle)*22};if(Math.hypot(point.x-tip.x,point.y-tip.y)<9){hit=value;mode='rotate';break;}
    }if(distance<14){hit=value;break;}
  }
  if(hit){placementSelection=placementKey(hit.kind,hit.item.id);renderPlacementControls();placementDrag={mode,item:hit.item,kind:hit.kind,start:point,origin:{x:hit.item.x,y:hit.item.y},remembered:false};}
  else placementDrag={mode:'pan',start:point,origin:{x:placementPanX,y:placementPanY},remembered:false};
  $('placement-viewport').classList.add('dragging');if(lastResponse)draw(lastResponse);
});
window.addEventListener('pointermove',event=>{
  if(!placementDrag)return;const point=previewPoint(event),drag=placementDrag;if(drag.mode==='pan'){placementPanX=drag.origin.x+point.x-drag.start.x;placementPanY=drag.origin.y+point.y-drag.start.y;}
  else {if(!drag.remembered){remember();drag.remembered=true;}if(drag.mode==='move'){const world=placementWorld(point.x,point.y);drag.item.x=world.x;drag.item.y=world.y;}else {const center=placementScreen(drag.item.x,drag.item.y);drag.item.heading=(Math.round(Math.atan2(point.y-center.y,point.x-center.x)*180/Math.PI+90)+360)%360;}}
  if(lastResponse)draw(lastResponse);
});
function finishPlacementDrag(){if(!placementDrag)return;const changed=placementDrag.remembered;placementDrag=null;$('placement-viewport').classList.remove('dragging');if(changed){recipe.version=Math.max(6,recipe.version);save();renderPlacementControls();schedule();}}
window.addEventListener('pointerup',finishPlacementDrag);window.addEventListener('pointercancel',finishPlacementDrag);
$('help').onclick=()=>$('guide').showModal();$('close-guide').onclick=()=>$('guide').close();
window.addEventListener('keydown',event=>{
  if(event.key==='Escape'){if(cancelConnectionDrag)cancelConnectionDrag();else {pending=null;renderGraph();}}
  if((event.ctrlKey||event.metaKey)&&event.key.toLowerCase()==='z'){event.preventDefault();event.shiftKey?restore(redo,undo):restore(undo,redo);}
  if((event.ctrlKey||event.metaKey)&&event.key.toLowerCase()==='s'){event.preventDefault();saveMap(false);}
});
localStorage.setItem('dustline.recipe.v1',JSON.stringify(recipe));render();
