/**
 * Gatito DTB-Patcher Web App
 * Conecta UI ao módulo WASM
 */

let Module = null;
let analysisData = [];
let currentTab = 0;

const TAB_NAMES = ['Controles', 'Áudio', 'Display', 'Energia', 'Outros'];
const CATEGORIES = ['controls', 'audio', 'display', 'power', 'other'];

// ====== INICIALIZAÇÃO ======
document.addEventListener('DOMContentLoaded', async () => {
    log('[INFO] Carregando aplicação...');
    
    try {
        // Aguardar carregamento do WASM
        Module = await window.GatitoDTB();
        log('[OK] Módulo WASM carregado');
        
        setupEventListeners();
        log('[OK] Eventos conectados');
        log('[INFO] Selecione dois DTBs e clique em Analisar');
    } catch (error) {
        log('[ERRO] Falha ao inicializar: ' + error.message);
    }
});

// ====== EVENT LISTENERS ======
function setupEventListeners() {
    // Botões de seleção de arquivo
    document.getElementById('donorSelectBtn').addEventListener('click', () => selectFile('donor'));
    document.getElementById('receiverSelectBtn').addEventListener('click', () => selectFile('receiver'));
    
    // Botões de ação
    document.getElementById('swapBtn').addEventListener('click', swapFiles);
    document.getElementById('analyzeBtn').addEventListener('click', analyzeFiles);
    document.getElementById('buildBtn').addEventListener('click', buildDTB);
    
    // Abas
    document.querySelectorAll('.tab').forEach((btn, idx) => {
        btn.addEventListener('click', () => switchTab(idx));
    });
}

// ====== ARQUIVO ======
async function selectFile(type) {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.dtb';
    
    input.onchange = async (e) => {
        const file = e.target.files[0];
        if (!file) return;
        
        try {
            log(`[...] Lendo ${type === 'donor' ? 'Doador' : 'Receptor'}: ${file.name}`);
            
            const data = await file.arrayBuffer();
            const view = new Uint8Array(data);
            
            // Salvar em FS virtual do WASM
            const path = `/tmp/${type}.dtb`;
            Module.FS.writeFile(path, view);
            
            // Atualizar UI
            const inputEl = type === 'donor' 
                ? document.getElementById('donorFile')
                : document.getElementById('receiverFile');
            inputEl.value = file.name;
            
            log(`[OK] ${file.name} carregado (${(data.byteLength / 1024).toFixed(1)} KB)`);
        } catch (error) {
            log(`[ERRO] ${error.message}`);
        }
    };
    
    input.click();
}

function swapFiles() {
    const donor = document.getElementById('donorFile').value;
    const receiver = document.getElementById('receiverFile').value;
    
    document.getElementById('donorFile').value = receiver;
    document.getElementById('receiverFile').value = donor;
    
    log('[INFO] Arquivos trocados');
}

// ====== ANÁLISE ======
async function analyzeFiles() {
    if (!Module) {
        log('[ERRO] WASM não carregado');
        return;
    }
    
    try {
        Module.FS.stat('/tmp/donor.dtb');
        Module.FS.stat('/tmp/receiver.dtb');
    } catch {
        log('[ERRO] Selecione ambos os DTBs');
        return;
    }
    
    try {
        log('[...] Analisando DTBs...');
        document.getElementById('status').textContent = 'Analisando...';
        
        const result = Module.ccall('analyze', 'string', 
            ['string', 'string'],
            ['/tmp/donor.dtb', '/tmp/receiver.dtb']);
        
        if (result !== 'OK') {
            log('[ERRO] ' + result);
            return;
        }
        
        // Obter análise como JSON
        const jsonStr = Module.ccall('get_analysis', 'string', [], []);
        analysisData = JSON.parse(jsonStr);
        
        log(`[OK] Análise concluída: ${analysisData.length} itens`);
        document.getElementById('status').textContent = 'Pronto';
        
        renderAllTabs();
        
    } catch (error) {
        log('[ERRO] ' + error.message);
        document.getElementById('status').textContent = 'Erro';
    }
}

function renderAllTabs() {
    for (let tabIdx = 0; tabIdx < 5; tabIdx++) {
        const category = CATEGORIES[tabIdx];
        const items = analysisData.filter(item => item.category === category);
        
        // Contar
        let donorCnt = 0, receiverCnt = 0, selectedCnt = 0;
        items.forEach(item => {
            if (item.donorValue) donorCnt++;
            if (item.receiverValue) receiverCnt++;
            if (item.compatible) selectedCnt++;
        });
        
        document.getElementById(`donorCount${tabIdx}`).textContent = donorCnt;
        document.getElementById(`receiverCount${tabIdx}`).textContent = receiverCnt;
        document.getElementById(`selectedCount${tabIdx}`).textContent = selectedCnt;
        
        // Renderizar listas
        renderList(`list${tabIdx}-donor`, items, 'donor');
        renderList(`list${tabIdx}-receiver`, items, 'receiver');
    }
}

function renderList(elementId, items, side) {
    const el = document.getElementById(elementId);
    el.innerHTML = '';
    
    items.forEach(item => {
        const row = document.createElement('label');
        row.className = 'row';
        
        if (item.compatible) {
            row.classList.add('selected');
        } else if (item.donorValue && item.receiverValue) {
            row.classList.add('diff');
        } else {
            row.classList.add('bad');
        }
        
        const checkbox = document.createElement('input');
        checkbox.type = 'checkbox';
        checkbox.className = 'check';
        checkbox.checked = item.compatible;
        checkbox.dataset.key = item.key;
        checkbox.addEventListener('change', (e) => {
            if (Module && Module._set_selection) {
                const key = Module.stringToUTF8(e.target.dataset.key, 
                    Module._malloc(e.target.dataset.key.length + 1), 
                    e.target.dataset.key.length + 1);
                Module._set_selection(key, e.target.checked ? 1 : 0);
                Module._free(key);
            }
        });
        
        const span = document.createElement('span');
        span.textContent = item.label;
        const sub = document.createElement('span');
        sub.className = 'sub';
        sub.textContent = side === 'donor' ? (item.donorValue || '—') : (item.receiverValue || '—');
        span.appendChild(sub);
        
        row.appendChild(checkbox);
        row.appendChild(span);
        
        if (item.compatible) {
            const check = document.createElement('b');
            check.textContent = '✓';
            row.appendChild(check);
        }
        
        el.appendChild(row);
    });
}

// ====== ABAS ======
function switchTab(idx) {
    currentTab = idx;
    
    document.querySelectorAll('.tab').forEach((tab, i) => {
        tab.classList.toggle('active', i === idx);
    });
    
    document.querySelectorAll('.page').forEach(page => {
        page.classList.remove('active');
    });
    
    const pageId = ['controles', 'audio', 'display', 'energia', 'outros'][idx];
    document.getElementById(pageId).classList.add('active');
}

// ====== BUILD ======
async function buildDTB() {
    if (!Module) {
        log('[ERRO] WASM não carregado');
        return;
    }
    
    if (analysisData.length === 0) {
        log('[ERRO] Analise os DTBs primeiro');
        return;
    }
    
    try {
        log('[...] Gerando novo DTB...');
        document.getElementById('status').textContent = 'Gerando...';
        
        const result = Module.ccall('build', 'string', 
            ['string'],
            ['/tmp/patched.dts']);
        
        if (!result.startsWith('OK')) {
            log('[ERRO] ' + result);
            return;
        }
        
        log('[OK] DTB gerado: ' + result);
        
        // Download
        const dtsData = Module.FS.readFile('/tmp/patched.dts');
        const blob = new Blob([dtsData], { type: 'application/octet-stream' });
        
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = 'patched.dts';
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
        
        log('[INFO] Download iniciado');
        document.getElementById('status').textContent = 'Pronto';
        
    } catch (error) {
        log('[ERRO] ' + error.message);
        document.getElementById('status').textContent = 'Erro';
    }
}

// ====== LOG ======
function log(msg) {
    const logEl = document.getElementById('log');
    const time = new Date().toLocaleTimeString('pt-BR');
    logEl.textContent += `[${time}] ${msg}\n`;
    logEl.scrollTop = logEl.scrollHeight;
}
