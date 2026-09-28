export const MAX_NODES = 64;
export const LUT_MAX_POINTS = 255;
export const PARAM_WORDS = 64;
export const DEFAULT_SPAWN_PROFILES=[{id:0,name:'Raider',color:'#ef6c5b',enemy:'raider',respawnSeconds:30,scrapChance:70,scrapMin:1,scrapMax:3,energyChance:25,energyMin:8,energyMax:16}];
export function normalizeSpawnProfiles(value) {
  const source=Array.isArray(value)&&value.length?value:DEFAULT_SPAWN_PROFILES;
  return source.map(profile=>({...DEFAULT_SPAWN_PROFILES[0],...profile}));
}
export function normalizePlacements(recipe) {
  const legacy=Array.isArray(recipe.nodes)?recipe.nodes.filter(node=>node.type==='gate'):[];
  if(!Array.isArray(recipe.portals))recipe.portals=legacy.map(node=>({id:node.gateId,x:node.p[0],y:node.p[1],width:node.p[2],height:node.p[3]}));
  if(!Array.isArray(recipe.playerSpawns))recipe.playerSpawns=legacy.map(node=>({id:`${node.gateId}_arrival`,x:node.p[4],y:node.p[5],heading:node.p[6]}));
  if(legacy.length){recipe.nodes=recipe.nodes.filter(node=>node.type!=='gate');recipe.version=6;}
  return recipe;
}
const LUT_COLORS=['#759bc7','#d6a466','#83ad79','#bd7f9f','#9a8ac7','#63aaa2','#c58a67','#a4a766'];
export const defaultLutColor=value=>LUT_COLORS[((value*37)^(value>>2))%LUT_COLORS.length];
function normalizeLut(node) {
  if(!Array.isArray(node.p) || !Number.isInteger(node.p[0]) || node.p[0]<0 || node.p[0]>LUT_MAX_POINTS)return;
  const count=node.p[0];
  if(node.p.length>=2+count*2)node.p=node.p.slice(0,2+count*2);
  const outputs=[node.p[1],...Array.from({length:count},(_,i)=>node.p[3+i*2])];
  node.colors=outputs.map((output,i)=>/^#[0-9a-f]{6}$/i.test(node.colors?.[i]??'')?node.colors[i]:defaultLutColor(Number.isInteger(output)?output:0));
}
function upgradeNode(node) {
  if(node.type==='material_fill') {
    node.type='field_fill';
    if(node.label==='Fill material')node.label='Constant field';
  }
  if(node.type==='material_paint') {
    node.type='field_paint';
    if(node.label==='Paint material')node.label='Paint field value';
  }
  if(node.type==='material_bands') {
    if(Array.isArray(node.p) && node.p.length===7) {
      const boundaries=node.p.slice(0,3),values=node.p.slice(3),points=[];
      let initial=values[0];
      boundaries.forEach((position,index)=>{
        const output=values[index+1];
        if(position===0)initial=output;
        else if(points.at(-1)?.position===position)points.at(-1).output=output;
        else points.push({position,output});
      });
      node.p=[points.length,initial,...points.flatMap(point=>[point.position,point.output])];
    }
    node.type='field_lut';
    if(node.label==='Field to materials')node.label='Stepped LUT';
  }
  if(node.type==='field_lut')normalizeLut(node);
}
function compiledParameters(node) {
  if(node.type!=='field_lut')return node.p;
  const pairs=Array.from({length:node.p[0]},(_,i)=>[node.p[2+i*2],node.p[3+i*2]]).sort((a,b)=>a[0]-b[0]);
  const lookup=new Uint8Array(256);let output=node.p[1],point=0;
  for(let input=0;input<256;++input){while(point<pairs.length&&pairs[point][0]<=input)output=pairs[point++][1];lookup[input]=output;}
  return Array.from({length:PARAM_WORDS},(_,word)=>(lookup[word*4]|lookup[word*4+1]<<8|lookup[word*4+2]<<16|lookup[word*4+3]<<24));
}
export function compile(recipe, schema, target = recipe.output, validateAll = true) {
  normalizePlacements(recipe);
  const fail = message => { throw new Error(message); };
  const integer = (value, lo, hi, name) => {
    if (!Number.isInteger(value) || value < lo || value > hi) fail(`${name}: expected ${lo}–${hi}.`);
  };
  if (![1,2,3,4,5,6].includes(recipe.version) || !Array.isArray(recipe.nodes)) fail('Unsupported recipe format.');
  integer(recipe.seed, 0, 0xffffffff, 'Seed');
  if (!recipe.nodes.length || recipe.nodes.length > MAX_NODES) fail(`Use between 1 and ${MAX_NODES} nodes.`);
  const ops = new Map(schema.operations.map((op, index) => [op.id, { ...op, index }]));
  const nodes = new Map();
  for (const node of recipe.nodes) {
    upgradeNode(node);
    integer(node.id, 1, 0x7fffffff, 'Node ID');
    if (nodes.has(node.id)) fail('Node IDs must be unique.');
    const op = ops.get(node.type);
    if (!op) fail(`Unknown operation: ${node.type}`);
    integer(node.stream ?? 0, 0, 0xffffffff, 'Random stream');
    // Import the retired min/max format at its midpoint, rounded to an 8px tile.
    if(node.type==='roads' && Array.isArray(node.p) && node.p.length===3) {
      integer(node.p[0],8,256,'Minimum road width');integer(node.p[2],node.p[0],256,'Maximum road width');
      node.p=[node.p[0]===node.p[2]?node.p[0]:Math.floor((node.p[0]+node.p[2]+8)/16)*8,node.p[1]];
    }
    if(node.type==='field_lut') {
      if(!Array.isArray(node.p) || !Number.isInteger(node.p[0]) || node.p.length!==2+node.p[0]*2)fail(`Invalid LUT data on node ${node.id}.`);
      integer(node.p[0],0,LUT_MAX_POINTS,'Point count');integer(node.p[1],0,255,'Initial output');
      for(let i=0;i<node.p[0];++i){integer(node.p[2+i*2],1,255,`Point ${i+1} position`);integer(node.p[3+i*2],0,255,`Point ${i+1} output`);}
      const positions=Array.from({length:node.p[0]},(_,i)=>node.p[2+i*2]);
      if(new Set(positions).size!==positions.length)fail('Stepped LUT points need unique positions.');
    } else {
      if (!Array.isArray(node.p) || node.p.length !== op.parameters.length) fail(`Invalid parameters on node ${node.id}.`);
      op.parameters.forEach((p, i) => {
        integer(node.p[i], p[2], p[3], p[0]);
        if (p[4] === 'neighbours' && ![4, 8].includes(node.p[i])) fail('Choose 4 or 8 neighbours.');
      });
    }
    nodes.set(node.id, node);
  }
  if(recipe.version===2 && nodes.get(recipe.materialOutput)?.type!=='materials') fail('Choose a Ground materials output.');
  if(recipe.version===1 && recipe.materialOutput!=null) fail('Material outputs require recipe version 2.');
  for(const [key,type] of [['materialOutput','materials'],['spawnOutput','spawns'],['decorationOutput','decoration']])
    if(recipe[key]!=null && nodes.get(recipe[key])?.type!==type)fail(`Invalid ${key}.`);
  if(recipe.version<3 && (recipe.spawnOutput!=null || recipe.decorationOutput!=null))fail('Placement outputs require recipe version 3.');
  const profiles=normalizeSpawnProfiles(recipe.spawnProfiles);
  if(profiles.length>8)fail('Use at most 8 spawn profiles.');
  const profileIds=new Set();
  for(const profile of profiles) {
    integer(profile.id,0,255,'Spawn profile ID');
    if(profileIds.has(profile.id))fail('Spawn profile IDs must be unique.');profileIds.add(profile.id);
    if(!['scout','raider','heavy'].includes(profile.enemy))fail('Unknown enemy type.');
    integer(profile.respawnSeconds,1,600,'Respawn seconds');integer(profile.scrapChance,0,100,'Scrap chance');
    integer(profile.scrapMin,0,15,'Minimum scrap');integer(profile.scrapMax,profile.scrapMin,15,'Maximum scrap');
    integer(profile.energyChance,0,100,'Energy chance');integer(profile.energyMin,0,100,'Minimum energy');
    integer(profile.energyMax,profile.energyMin,100,'Maximum energy');
  }
  const placementId=/^[a-z0-9]+(?:_[a-z0-9]+)*$/;
  for(const [label,items,limit,fields] of [
    ['Portal',recipe.portals,8,[['x',0,8191],['y',0,8191],['width',16,512],['height',16,512]]],
    ['Player spawn',recipe.playerSpawns,16,[['x',0,8191],['y',0,8191],['heading',0,359]]]]) {
    if(!Array.isArray(items)||items.length>limit)fail(`${label}s: use at most ${limit}.`);
    const ids=new Set();
    for(const item of items) {
      if(!item||typeof item.id!=='string'||item.id.length>24||!placementId.test(item.id))fail(`${label} needs a lowercase underscore ID.`);
      if(ids.has(item.id))fail(`Duplicate ${label.toLowerCase()} ID: ${item.id}.`);ids.add(item.id);
      for(const [field,low,high] of fields)integer(item[field],low,high,`${label} ${item.id} ${field}`);
    }
  }
  const ordered = [], done = new Set(), visiting = new Set();
  function visit(id) {
    const node = nodes.get(id);
    if (!node) fail('Connect all required inputs and choose an output.');
    if (visiting.has(id)) fail('This connection creates a cycle.');
    if (done.has(id)) return;
    visiting.add(id);
    const op = ops.get(node.type), inputs = node.inputs ?? {};
    if (Object.keys(inputs).some(port => !(port in op.inputs))) fail(`Unknown input on node ${id}.`);
    for (const [port, expected] of Object.entries(op.inputs)) {
      const source = inputs[port];
      if (source == null && expected.endsWith('?')) continue;
      visit(source);
      if (ops.get(nodes.get(source).type).kind !== expected.replace('?', '')) fail(`Node ${id}, ${port}: needs ${expected.replace('?', '')}.`);
    }
    visiting.delete(id); done.add(id); ordered.push(id);
  }
  if (validateAll) for (const id of nodes.keys()) visit(id);
  ordered.length = 0; done.clear(); visit(target);
  const indices = new Map(ordered.map((id, i) => [id, i]));
  const program = ordered.map(id => {
    const n = nodes.get(id),params=compiledParameters(n);
    return [ops.get(n.type).index, ...['a', 'b', 'mask'].map(p => indices.get(n.inputs?.[p]) ?? -1),
      n.stream ?? 0, ...params, ...Array(PARAM_WORDS - params.length).fill(0)];
  });
  const last = program.map((_, i) => i);
  program.forEach((n, i) => n.slice(1, 4).forEach(source => { if (source >= 0) last[source] = i; }));
  const peak = Math.max(...program.map((_, i) => last.slice(0, i).filter(end => end >= i).length + 1));
  if (peak > 6) fail(`This graph needs ${peak} live buffers. The GBA limit is 6; simplify its branches.`);
  return { program, peak, kind: ops.get(nodes.get(target).type).kind, ordered };
}

export function makeNode(schema, type, id, x = 40, y = 40) {
  const op = schema.operations.find(op => op.id === type);
  const node={ id, type, label: op.name, inputs: {}, stream: op.random ? id : 0, p: op.parameters.map(p => p[1]), x, y };
  if(type==='field_lut'){node.p=[3,0,96,1,144,2,192,3];normalizeLut(node);}
  return node;
}

export function presets(schema, original) {
  const create = (name, types, configure) => {
    const nodes = types.map((type, i) => makeNode(schema, type, i + 1, 40 + (i % 3) * 235, 45 + Math.floor(i / 3) * 200));
    configure(nodes);
    return { version: 1, name, seed: original.seed, output: nodes.at(-1).id, nodes };
  };
  const result = [original,
    create('Two rules + radial mask', ['random','cellular','cellular','radial','threshold','combine','largest','world'], n => {
      n[0].stream = 0;
      n[1].inputs.a = 1; n[1].p[0] = 2;
      n[2].inputs.a = 2; n[2].p = [3, 496, 504, 2, 8];
      n[3].p = [32,32,35,0]; n[4].inputs.a = 4; n[4].p[0] = 210;
      n[5].inputs = { a: 3, b: 5 }; n[6].inputs.a = 6; n[7].inputs.a = 7;
    }),
    create('Plasma basin', ['plasma','radial','blend','threshold','cellular','world'], n => {
      n[2].inputs = {a:1,b:2}; n[2].p[0] = 105;
      n[3].inputs.a = 3; n[3].p[0] = 142;
      n[4].inputs.a = 4; n[4].p[0] = 2; n[5].inputs.a = 5;
    }),
    create('Voronoi passages', ['voronoi','threshold','noise','threshold','combine','cellular','world'], n => {
      n[0].p = [18,1,20]; n[1].inputs.a = 1; n[1].p[0] = 50;
      n[2].p = [20,2]; n[3].inputs.a = 3; n[3].p = [160,1];
      n[4].inputs = {a:2,b:4}; n[4].p[0] = 1;
      n[5].inputs.a = 5; n[5].p[0] = 2; n[6].inputs.a = 6;
    }),
    (()=>{
      const r=structuredClone(original);r.name='Natural ground';r.version=2;
      const first=Math.max(...r.nodes.map(n=>n.id))+1;
      const noise=makeNode(schema,'noise',first,40,280);noise.p=[12,3];
      const bands=makeNode(schema,'field_lut',first+1,280,280);bands.inputs.a=first;
      const output=makeNode(schema,'materials',first+2,520,280);output.inputs.a=first+1;
      r.nodes.push(noise,bands,output);r.materialOutput=output.id;return r;
    })()];
  const roads=structuredClone(result.at(-1));roads.name="Natural ground + town roads";
  const node=makeNode(schema,"roads",Math.max(...roads.nodes.map(n=>n.id))+1,760,45);
  node.inputs.a=roads.output;roads.nodes.push(node);roads.output=node.id;result.push(roads);
  const populated=structuredClone(roads);populated.name='Populated wasteland';populated.version=4;populated.spawnProfiles=normalizeSpawnProfiles();
  const first=Math.max(...populated.nodes.map(n=>n.id))+1;
  const spawn=makeNode(schema,'spawns',first,760,280),decor=makeNode(schema,'decoration',first+1,760,480);
  const density=makeNode(schema,'noise',first+2,280,480);density.p=[10,2];decor.inputs.a=density.id;
  populated.nodes.push(spawn,decor,density);populated.spawnOutput=spawn.id;populated.decorationOutput=decor.id;result.push(populated);
  return result;
}
