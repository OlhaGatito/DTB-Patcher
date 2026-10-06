/* Gatito DTB-Patcher - browser FDT parser fallback.
 * Parses standard Flattened Device Tree binaries locally when the minimal
 * WASM bridge cannot decompile a DTB. No file leaves the browser.
 */
(function () {
  const MAGIC = 0xd00dfeed;
  const BEGIN_NODE = 1, END_NODE = 2, PROP = 3, NOP = 4, END = 9;

  const lower = s => String(s || '').toLowerCase();
  const has = (s, q) => lower(s).includes(lower(q));
  const ends = (s, q) => lower(s).endsWith(lower(q));

  function be32(v, o) {
    if (o < 0 || o + 4 > v.length) throw new Error('DTB truncado ao ler u32.');
    return (((v[o] << 24) >>> 0) | (v[o + 1] << 16) | (v[o + 2] << 8) | v[o + 3]) >>> 0;
  }
  function align4(n) { return (n + 3) & ~3; }

  function cString(v, start, end) {
    let p = start;
    while (p < end && v[p] !== 0) p++;
    if (p >= end) throw new Error('DTB inválido: string sem terminador.');
    return new TextDecoder('utf-8', { fatal: false }).decode(v.subarray(start, p));
  }

  function printable(bytes) {
    if (!bytes.length) return '';
    let good = 0;
    for (const b of bytes) if (b === 9 || b === 10 || b === 13 || (b >= 32 && b < 127)) good++;
    return good / bytes.length > 0.90;
  }

  function valueText(bytes) {
    if (!bytes.length) return '';
    if (bytes[bytes.length - 1] === 0) {
      let parts = [], p = 0;
      while (p < bytes.length) {
        let e = p;
        while (e < bytes.length && bytes[e] !== 0) e++;
        if (e > p) parts.push(new TextDecoder().decode(bytes.subarray(p, e)));
        p = e + 1;
      }
      if (parts.length) return parts.map(x => '"' + x.replace(/\\/g, '\\\\').replace(/"/g, '\\"') + '"').join(', ');
    }
    if (printable(bytes)) return '"' + new TextDecoder().decode(bytes).replace(/\0/g, '').replace(/"/g, '\\"') + '"';
    if (bytes.length % 4 === 0) {
      const cells = [];
      for (let i = 0; i < bytes.length; i += 4) cells.push('0x' + be32(bytes, i).toString(16).padStart(8, '0'));
      return '<' + cells.join(' ') + '>';
    }
    return '[' + Array.from(bytes, b => b.toString(16).padStart(2, '0')).join(' ') + ']';
  }

  function pretty(s) {
    s = String(s || '').replace(/[_-]+/g, ' ').trim();
    return s.replace(/(^| )([a-z])/g, (_, a, b) => a + b.toUpperCase());
  }

  function codeName(value) {
    const m = String(value || '').match(/<\\s*(?:0x)?([0-9a-fA-F]+)(?:\\s|>)/);
    if (!m) return '';
    const n = parseInt(m[1], 16);
    return ({304:'A',305:'B',307:'X',308:'Y',310:'L1',311:'R1',312:'L2',313:'R2',
      314:'SELECT',315:'START',317:'L3',318:'R3',544:'UP',545:'DOWN',546:'LEFT',547:'RIGHT',316:'MODE'})[n] || '';
  }

  function gpioSummary(value) {
    const m = String(value || '').match(/<\\s*([^\\s>]+)\\s+([^\\s>]+)/);
    if (!m) return value || '—';
    let n = m[2];
    if (/^0x/i.test(n)) n = String(parseInt(n, 16));
    return 'GPIO ' + n;
  }

  function parse(bytes) {
    const v = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
    if (v.length < 40) throw new Error('DTB inválido: arquivo menor que o cabeçalho FDT.');
    const magic = be32(v, 0);
    if (magic !== MAGIC) throw new Error('DTB inválido: magic FDT não encontrado (0x' + magic.toString(16) + ').');
    const total = be32(v, 4), structOff = be32(v, 8), stringsOff = be32(v, 12);
    const structSize = be32(v, 36), stringsSize = be32(v, 32);
    if (total > v.length || structOff + structSize > v.length || stringsOff + stringsSize > v.length)
      throw new Error('DTB inválido: áreas do FDT ultrapassam o tamanho do arquivo.');

    const strings = v.subarray(stringsOff, stringsOff + stringsSize);
    const root = { name:'', path:'/', props:{}, children:[] };
    const stack = [root];
    let p = structOff, ended = false;

    while (p < structOff + structSize) {
      const token = be32(v, p); p += 4;
      if (token === BEGIN_NODE) {
        const start = p;
        while (p < structOff + structSize && v[p] !== 0) p++;
        if (p >= structOff + structSize) throw new Error('DTB inválido: nó sem terminador.');
        const name = new TextDecoder().decode(v.subarray(start, p));
        p = structOff + align4(p - structOff);
        const parent = stack[stack.length - 1];
        const path = parent.path === '/' ? '/' + name : parent.path + '/' + name;
        const node = { name, path, props:{}, children:[] };
        parent.children.push(node);
        stack.push(node);
      } else if (token === END_NODE) {
        if (stack.length <= 1) throw new Error('DTB inválido: END_NODE sem BEGIN_NODE.');
        stack.pop();
      } else if (token === PROP) {
        if (p + 8 > structOff + structSize) throw new Error('DTB inválido: propriedade truncada.');
        const len = be32(v, p), nameOff = be32(v, p + 4); p += 8;
        if (nameOff >= strings.length || p + len > structOff + structSize)
          throw new Error('DTB inválido: propriedade fora dos limites.');
        const name = cString(strings, nameOff, strings.length);
        const raw = v.slice(p, p + len); p = structOff + align4((p + len) - structOff);
        stack[stack.length - 1].props[name] = valueText(raw);
      } else if (token === NOP) {
        continue;
      } else if (token === END) {
        ended = true; break;
      } else {
        throw new Error('DTB inválido: token FDT desconhecido 0x' + token.toString(16) + '.');
      }
    }
    if (!ended || stack.length !== 1) throw new Error('DTB inválido: estrutura FDT incompleta.');
    return root;
  }

  function walk(root) {
    const out = [];
    (function visit(n){ out.push(n); n.children.forEach(visit); })(root);
    return out;
  }

  function controlProps(n) {
    return Object.keys(n.props).filter(p => p === 'gpio' || p === 'gpios' || ends(p, '-gpios'));
  }

  function isControl(n) {
    const x = lower(n.path + ' ' + n.name);
    if (has(x, 'pinctrl') || has(x, 'gpio@')) return false;
    const named = ['button','key','joy','gamepad','rocker','input','retrogame'].some(q => has(x,q));
    return named && controlProps(n).length > 0;
  }

  function controlKey(n) {
    const label = n.props.label || n.props['key-label'] || n.props['button-label'] || n.props.name;
    if (label && !/^</.test(label)) return label.replace(/^"|"$/g,'').replace(/[_-]+/g,' ').toLowerCase();
    const code = codeName(n.props['linux,code']);
    if (code) return code.toLowerCase();
    const x = lower(n.name).replace(/[_-]+/g,' ');
    return x;
  }

  function relevantNode(n, category) {
    const x = lower(n.path + ' ' + n.name);
    const groups = {
      audio:['audio','sound','codec','i2s','dai'],
      display:['display','panel','backlight','lcd','mipi','dsi'],
      power:['battery','charger','power','fuel','adc']
    };
    return (groups[category] || []).some(q => has(x,q));
  }

  function relevantProperty(category, p) {
    const x = lower(p);
    const groups = {
      audio:['audio','sound','codec','dai','i2s','routing','format','mclk','clock'],
      display:['display','panel','backlight','lcd','timing','width','height','format','reset','enable','power','remote'],
      power:['battery','charger','charge','voltage','current','capacity','adc','channel','gpio']
    };
    return (groups[category] || []).some(q => has(x,q));
  }

  function blockSummary(n, props) {
    return props.map(p => p + ': ' + (n.props[p] || '—')).join(' | ').slice(0, 220);
  }

  function buildAnalysis(donor, receiver) {
    const dn = walk(donor), rn = walk(receiver), out = [];
    const rControls = new Map();
    for (const n of rn) if (isControl(n)) for (const p of controlProps(n)) {
      const k = controlKey(n); if (!rControls.has(k)) rControls.set(k, n);
    }
    const seenControls = new Set();
    for (const n of dn) if (isControl(n)) for (const p of controlProps(n)) {
      const key = controlKey(n);
      if (seenControls.has(key)) continue;
      seenControls.add(key);
      const r = rControls.get(key);
      const dv = gpioSummary(n.props[p]);
      if (!r) out.push({category:'controls',key,label:pretty(key),donorValue:dv,receiverValue:'Função não encontrada no receptor',compatible:false});
      else {
        const rp = r.props[p] !== undefined ? p : '';
        if (!rp) out.push({category:'controls',key,label:pretty(key),donorValue:dv,receiverValue:'Propriedade GPIO não encontrada',compatible:false});
        else if (n.props[p] !== r.props[rp]) out.push({category:'controls',key,label:pretty(key),donorValue:dv,receiverValue:gpioSummary(r.props[rp]),compatible:true});
      }
    }

    for (const category of ['audio','display','power']) {
      const rmap = new Map();
      for (const n of rn) if (relevantNode(n,category)) {
        const k = category + ':' + (n.props.label || n.name || n.path);
        if (!rmap.has(k)) rmap.set(k,n);
      }
      const seen = new Set();
      for (const n of dn) if (relevantNode(n,category)) {
        const props = Object.keys(n.props).filter(p => relevantProperty(category,p));
        if (!props.length) continue;
        const k = category + ':' + (n.props.label || n.name || n.path);
        if (seen.has(k)) continue; seen.add(k);
        const r = rmap.get(k);
        const common = r ? props.filter(p => r.props[p] !== undefined && r.props[p] !== n.props[p]) : [];
        out.push({category,key:k,label:pretty(n.props.label || n.name || n.path),donorValue:blockSummary(n,props),receiverValue:r ? (common.length ? blockSummary(r,common) : 'Sem diferença transferível') : 'Bloco correspondente não encontrado',compatible:!!common.length});
      }
    }

    return out;
  }

  window.GatitoFdt = { parse, buildAnalysis, walk };
})();