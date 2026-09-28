import createEngine from './generated/engine.mjs';
import {renderOverview,renderFullPng} from './render.mjs';
const engine = await createEngine();
const art = await fetch('./generated/art.json').then(r=>{if(!r.ok)throw new Error('Run ./build.ps1 to generate terrain art.');return r.json();});
const messages = ['OK','Invalid node count','Unknown operation','Invalid input','Wrong port type','Invalid parameter','GBA buffer budget exceeded'];
function selectBank(mapId){return art.banks?.[art.mapBanks?.[mapId]??0]??art;}
function configureMaterials(bank){
  if(engine._recipe_material_textures)engine.HEAPU8.set(bank.materialTextures,engine._recipe_material_textures());
  if(engine._recipe_material_surfaces)engine.HEAPU8.set(bank.materialSurfaces,engine._recipe_material_surfaces());
}
function execute(program,seed){
  engine.HEAPU32.set(new Uint32Array(program.flat()),engine._recipe_input()>>>2);
  const status=engine._recipe_run(program.length,seed>>>0);
  if(status)throw new Error(messages[status]??`Engine error ${status}`);
}
function run(program, seed, {collision=false,textures=false,schematic=false,portals=[],playerSpawns=[],materialProgram=null,spawnProgram=null,decorationProgram=null,spawnProfiles=null,showSpawns=true,full=false,small=false,bank=art}={}) {
  configureMaterials(bank);
  execute(program,seed);
  const meta = engine.HEAPU32.slice(engine._recipe_meta() >>> 2, (engine._recipe_meta() >>> 2) + 20);
  const cells = engine.HEAPU8.slice(engine._recipe_cells(), engine._recipe_cells() + 4096);
  let refined=null,roads=null,texture=null,ground=null,tileMap=null,decorations=null,spawns=null,spawnTypes=null,patches=0,invalidPortals=[],invalidPlayerSpawns=[];
  if(meta[1]===2){
    for(const portal of portals)if(engine._recipe_connect_portal(portal.x,portal.y))invalidPortals.push(portal.id);
    if(materialProgram){execute(materialProgram,seed);if(engine._recipe_apply_materials())throw new Error('Ground output must contain material IDs.');}
    if(spawnProgram){execute(spawnProgram,seed);if(engine._recipe_apply_spawns())throw new Error('Choose an Enemy spawns output.');}
    if(decorationProgram){
      const effective=decorationProgram.map(node=>[...node]),last=effective.at(-1),mask=bank.decorationEnabledMask??15;
      for(let slot=0;slot<4;++slot)if(!(mask&(1<<slot)))last[6+slot]=0;
      execute(effective,seed);if(engine._recipe_apply_decoration())throw new Error('Choose a Decoration output.');
    }
    const count=engine._recipe_spawn_count(),points=engine._recipe_spawns()>>>1;
    spawns=engine.HEAPU16.slice(points,points+count*2);
    if(engine._recipe_spawn_types){const types=engine._recipe_spawn_types();spawnTypes=engine.HEAPU8.slice(types,types+count);}
    if(decorationProgram){const ptr=engine._recipe_decorations();decorations=engine.HEAPU8.slice(ptr,ptr+1024*1024);patches=decorations.reduce((n,v)=>n+(v>0 && (v-1)%4===0),0);}
    const ptr=engine._recipe_ground();ground=engine.HEAPU8.slice(ptr,ptr+4096);
    if(collision){const ptr=engine._recipe_collision();refined=engine.HEAPU8.slice(ptr,ptr+1024*1024);invalidPlayerSpawns=playerSpawns.filter(item=>refined[Math.min(1023,item.y>>3)*1024+Math.min(1023,item.x>>3)]).map(item=>item.id);}
    if(schematic){const ptr=engine._recipe_roads();roads=engine.HEAPU8.slice(ptr,ptr+1024*1024);}
    if(textures || full){
      const ptr=engine._recipe_tiles()>>>1;tileMap=engine.HEAPU16.slice(ptr,ptr+1024*1024);
      if(textures)texture=renderOverview(tileMap,bank,small?8:4,decorations,showSpawns?spawns:null,spawnTypes,spawnProfiles);
    }
  }
  const registered=new Set(bank.materialIds??[0,1,2,3]);
  const unknown=[...new Set(meta[1]===3?cells:ground??[])].filter(id=>!registered.has(id));
  return {meta,cells,refined,roads,texture,ground,unknown,seed,spawns,spawnTypes,patches,invalidPortals,invalidPlayerSpawns,requestedSpawns:spawnProgram?.at(-1)[5],decorations:full?decorations:null,tileMap:full?tileMap:null};
}
self.onmessage = async ({data}) => {
  const {id,mapId,program,seed,collision,textures,schematic,portals,playerSpawns,seeds,compare,materialProgram,spawnProgram,decorationProgram,spawnProfiles,histogramProgram,histogramNode,categorical,categoricalColors,showSpawns,render} = data;
  const bank=selectBank(mapId),options={materialProgram,spawnProgram,decorationProgram,spawnProfiles,showSpawns,schematic,portals,playerSpawns,bank};
  try {
    if(render){
      const output=run(program,seed,{...options,full:true});
      if(!output.tileMap)throw new Error('Choose a Playable world to render.');
      const blob=await renderFullPng(output.tileMap,bank,progress=>self.postMessage({id,render:true,progress}),output.decorations,showSpawns?output.spawns:null,output.spawnTypes,spawnProfiles);
      self.postMessage({id,render:true,blob,seed});return;
    }
    const start = performance.now();
    const outputs = (seeds ?? [seed]).map(s => run(program,s,{...options,collision,textures,small:!!seeds}));
    const before = compare ? run(compare,seed,{...options,textures}) : null;
    let histogram=null;
    if(histogramProgram){execute(histogramProgram,seed);histogram=new Uint16Array(256);for(const value of engine.HEAPU8.slice(engine._recipe_cells(),engine._recipe_cells()+4096))++histogram[value];}
    const response={id,outputs,before,histogram,histogramNode,categorical,categoricalColors,ms:performance.now()-start};
    const transfer=outputs.flatMap(o=>[o.cells.buffer,...[o.refined,o.roads,o.ground,o.texture?.pixels].filter(Boolean).map(a=>a.buffer)]);
    if(histogram)transfer.push(histogram.buffer);
    self.postMessage(response,transfer);
  } catch (error) { self.postMessage({id,render:!!render,error:error.message}); }
};
self.postMessage({ready:true});
