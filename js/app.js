/**
 * Gatito DTB-Patcher Web App
 * Frontend for the minimal Emscripten/WASM bridge.
 *
 * Important:
 * - Receptor is always the base.
 * - Doador is the source of selected transfers.
 * - Originals never leave the browser and are never modified.
 */

let Module = null;
let analysisData = [];
let uploadedFiles = { donor: null, receiver: null };
let currentTab = 0;
const selectedKeys = new Set();

const CATEGORIES = ['controls', 'audio', 'display', 'power', 'other'];
const PAGE_IDS = ['controles', 'audio', 'display', 'energia', 'outros'];

document.addEventListener('DOMContentLoaded', async () => {
    log('[INFO] Carregando aplicação...');

    try {
        if (typeof window.GatitoDTB !== 'function') {
            throw new Error('O módulo WASM não foi carregado. Verifique wasm/gatito_dtb_patcher.js.');
        }

        Module = await window.GatitoDTB();

        if (!Module || !Module._gatitoReady || !Module.FS || typeof Module.ccall !== 'function') {
            throw new Error('O WASM carregou, mas a API web não foi inicializada (FS/ccall ausentes).');
        }

        log('[OK] Módulo WASM carregado');
        log('[OK] DTC/WASM pronto para receber DTBs');
        setupEventListeners();
        log('[OK] Eventos conectados');
        log('[INFO] Selecione Doador e Receptor e clique em Analisar');
        setStatus('Pronto');
    } catch (error) {
        console.error(error);
        log('[ERRO] Falha ao inicializar: ' + (error?.message || error));
        setStatus('Erro no WASM');
    }
});

function setupEventListeners() {
    document.getElementById('donorSelectBtn').addEventListener('click', () => selectFile('donor'));
    document.getElementById('receiverSelectBtn').addEventListener('click', () => selectFile('receiver'));

    document.getElementById('swapBtn').addEventListener('click', swapFiles);
    document.getElementById('analyzeBtn').addEventListener('click', analyzeFiles);
    document.getElementById('buildBtn').addEventListener('click', buildDTB);

    document.querySelectorAll('.tab').forEach((btn, idx) => {
        btn.addEventListener('click', () => switchTab(idx));
    });
}

function setStatus(text) {
    const el = document.getElementById('status');
    if (el) el.textContent = text;
}

function hasFile(path) {
    try { return !!Module?.FS?.stat?.(path); } catch (_) { return false; }
}

function writeFile(path, bytes) {
    if (!Module?.FS?.writeFile) {
        throw new Error('O armazenamento temporário do WASM ainda não está disponível.');
    }
    if (hasFile(path)) {
        try { Module.FS.unlink(path); } catch (_) {}
    }
    Module.FS.writeFile(path, bytes);
}

function removeFile(path) {
    try { if (Module?.FS && hasFile(path)) Module.FS.unlink(path); } catch (_) {}
}

function storeUploadedFile(type, file, bytes) {
    uploadedFiles[type] = {
        name: file.name,
        size: bytes.byteLength,
        bytes: new Uint8Array(bytes)
    };
}

async function selectFile(type) {
    if (!Module) {
        log('[ERRO] WASM não carregado');
        return;
    }

    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.dtb,application/octet-stream';
    input.style.display = 'none';

    input.addEventListener('change', async (event) => {
        const file = event.target.files?.[0];
        input.remove();

        if (!file) return;

        try {
            log('[...] Lendo ' + (type === 'donor' ? 'Doador' : 'Receptor') + ': ' + file.name);
            const data = new Uint8Array(await file.arrayBuffer());
            const path = type === 'donor' ? '/tmp/donor.dtb' : '/tmp/receiver.dtb';

            storeUploadedFile(type, file, data);

            try {
                if (Module?.FS?.writeFile) writeFile(path, data);
            } catch (_) {
                log('[AVISO] FS do WASM indisponível; usando armazenamento temporário do navegador.');
            }

            const inputEl = type === 'donor'
                ? document.getElementById('donorFile')
                : document.getElementById('receiverFile');

            inputEl.value = file.name;
            log('[OK] ' + file.name + ' carregado temporariamente (' + (data.byteLength / 1024).toFixed(1) + ' KB)');

            // A seleção de arquivos invalida a análise anterior.
            invalidateAnalysis();
        } catch (error) {
            log('[ERRO] ' + (error?.message || error));
        }
    });

    document.body.appendChild(input);
    input.click();
}

function swapFiles() {
    if (!Module) {
        log('[ERRO] WASM não carregado');
        return;
    }

    try {
        const donorName = document.getElementById('donorFile').value;
        const receiverName = document.getElementById('receiverFile').value;

        if (!donorName && !receiverName) {
            log('[INFO] Nenhum arquivo selecionado');
            return;
        }

        // Troca os bytes no FS virtual, não apenas os nomes exibidos.
        const donorBytes = uploadedFiles.donor ? new Uint8Array(uploadedFiles.donor.bytes) : null;
        const receiverBytes = uploadedFiles.receiver ? new Uint8Array(uploadedFiles.receiver.bytes) : null;

        if (receiverBytes) writeFile('/tmp/donor.dtb', receiverBytes);
        else removeFile('/tmp/donor.dtb');

        if (donorBytes) writeFile('/tmp/receiver.dtb', donorBytes);
        else removeFile('/tmp/receiver.dtb');

        document.getElementById('donorFile').value = receiverName;
        document.getElementById('receiverFile').value = donorName;

        invalidateAnalysis();
        log('[OK] Doador e Receptor trocados (arquivos e interface)');
    } catch (error) {
        log('[ERRO] Falha ao trocar arquivos: ' + (error?.message || error));
    }
}

function invalidateAnalysis() {
    analysisData = [];
    selectedKeys.clear();
    clearRenderedLists();
    for (let i = 0; i < 5; i++) {
        document.getElementById('donorCount' + i).textContent = '—';
        document.getElementById('receiverCount' + i).textContent = '—';
        document.getElementById('selectedCount' + i).textContent = '—';
    }
    setStatus('Aguardando análise');
}

async function analyzeFiles() {
    if (!Module) {
        log('[ERRO] WASM não carregado');
        return;
    }

    if (!uploadedFiles.donor || !uploadedFiles.receiver) {
        log('[ERRO] Selecione ambos os DTBs');
        return;
    }

    try {
        writeFile('/tmp/donor.dtb', uploadedFiles.donor.bytes);
        writeFile('/tmp/receiver.dtb', uploadedFiles.receiver.bytes);
    } catch (error) {
        log('[ERRO] Não foi possível preparar os DTBs para o WASM: ' + (error?.message || error));
        return;
    }

    try {
        selectedKeys.clear();
        log('[...] Analisando DTBs com DTC/WASM...');
        setStatus('Analisando...');

        const result = Module.ccall(
            'analyze',
            'string',
            ['string', 'string'],
            ['/tmp/donor.dtb', '/tmp/receiver.dtb']
        );

        if (result !== 'OK') {
            const detail = getNativeError();
            throw new Error(detail || result || 'O WASM recusou a análise.');
        }

        const jsonStr = Module.ccall('get_analysis', 'string', [], []);
        const parsed = JSON.parse(jsonStr);

        if (!Array.isArray(parsed)) {
            throw new Error('get_analysis não retornou uma lista JSON válida.');
        }

        analysisData = parsed;
        log('[OK] Análise concluída: ' + analysisData.length + ' itens');
        setStatus('Análise concluída');

        renderAllTabs();
    } catch (error) {
        console.error(error);
        log('[ERRO] ' + (error?.message || error));
        setStatus('Erro na análise');
    }
}

function getNativeError() {
    try {
        if (!Module?._get_error) return '';
        return Module.ccall('get_error', 'string', [], []) || '';
    } catch (_) {
        return '';
    }
}

function renderAllTabs() {
    for (let tabIdx = 0; tabIdx < 5; tabIdx++) {
        const category = CATEGORIES[tabIdx];
        const items = analysisData.filter(item => item.category === category);

        let donorCnt = 0;
        let receiverCnt = 0;
        let selectedCnt = 0;

        items.forEach(item => {
            if (item.donorValue !== undefined && item.donorValue !== null && item.donorValue !== '') donorCnt++;
            if (item.receiverValue !== undefined && item.receiverValue !== null && item.receiverValue !== '') receiverCnt++;
            if (selectedKeys.has(item.key)) selectedCnt++;
        });

        document.getElementById('donorCount' + tabIdx).textContent = donorCnt;
        document.getElementById('receiverCount' + tabIdx).textContent = receiverCnt;
        document.getElementById('selectedCount' + tabIdx).textContent = selectedCnt;

        renderList('list' + tabIdx + '-donor', items, 'donor');
        renderList('list' + tabIdx + '-receiver', items, 'receiver');
    }
}

function renderList(elementId, items, side) {
    const el = document.getElementById(elementId);
    el.innerHTML = '';

    if (!items.length) {
        const empty = document.createElement('div');
        empty.className = 'row';
        empty.textContent = 'Nenhum bloco encontrado.';
        empty.style.color = '#8198ad';
        el.appendChild(empty);
        return;
    }

    items.forEach(item => {
        const row = document.createElement('label');
        row.className = 'row';

        if (item.compatible && selectedKeys.has(item.key)) {
            row.classList.add('selected');
        } else if (item.donorValue && item.receiverValue) {
            row.classList.add('diff');
        } else {
            row.classList.add('bad');
        }

        const checkbox = document.createElement('input');
        checkbox.type = 'checkbox';
        checkbox.className = 'check';
        checkbox.dataset.key = item.key || '';
        checkbox.disabled = side !== 'donor' || !item.compatible;
        checkbox.checked = side === 'donor' && selectedKeys.has(item.key);

        if (side === 'donor' && item.compatible) {
            checkbox.addEventListener('change', () => {
                setSelection(item.key, checkbox.checked);
            });
        }

        const span = document.createElement('span');
        span.textContent = item.label || item.key || 'Sem nome';

        const sub = document.createElement('span');
        sub.className = 'sub';
        sub.textContent = side === 'donor'
            ? (item.donorValue || '—')
            : (item.receiverValue || '—');

        span.appendChild(sub);
        row.appendChild(checkbox);
        row.appendChild(span);

        if (item.compatible && selectedKeys.has(item.key)) {
            const check = document.createElement('b');
            check.textContent = '✓';
            row.appendChild(check);
        }

        el.appendChild(row);
    });
}

function setSelection(key, enabled) {
    if (!key) return;

    try {
        Module.ccall(
            'set_selection',
            'number',
            ['string', 'number'],
            [key, enabled ? 1 : 0]
        );

        if (enabled) selectedKeys.add(key);
        else selectedKeys.delete(key);

        renderAllTabs();
        log('[INFO] ' + (enabled ? 'Selecionado: ' : 'Removido: ') + key);
    } catch (error) {
        log('[ERRO] Falha ao alterar seleção: ' + (error?.message || error));
    }
}

function clearRenderedLists() {
    for (let i = 0; i < 5; i++) {
        const donor = document.getElementById('list' + i + '-donor');
        const receiver = document.getElementById('list' + i + '-receiver');
        if (donor) donor.innerHTML = '';
        if (receiver) receiver.innerHTML = '';
    }
}

function switchTab(idx) {
    currentTab = idx;

    document.querySelectorAll('.tab').forEach((tab, i) => {
        tab.classList.toggle('active', i === idx);
    });

    document.querySelectorAll('.page').forEach(page => {
        page.classList.remove('active');
    });

    const page = document.getElementById(PAGE_IDS[idx]);
    if (page) page.classList.add('active');
}

async function buildDTB() {
    if (!Module) {
        log('[ERRO] WASM não carregado');
        return;
    }

    if (!analysisData.length) {
        log('[ERRO] Analise os DTBs primeiro');
        return;
    }

    if (!selectedKeys.size) {
        log('[ERRO] Selecione pelo menos um bloco compatível no lado Doador');
        setStatus('Nenhuma transferência selecionada');
        return;
    }

    try {
        log('[...] Gerando patch a partir do Receptor...');
        setStatus('Gerando...');

        const result = Module.ccall(
            'build',
            'string',
            ['string'],
            ['/tmp/patched.dts']
        );

        if (!String(result).startsWith('OK')) {
            throw new Error(getNativeError() || result || 'Falha ao gerar o patch.');
        }

        // Algumas versões da ponte geram DTB diretamente; outras geram DTS.
        // Preferimos o DTB e mantemos DTS como fallback para inspeção.
        let outputPath = null;
        let outputName = null;

        if (hasFile('/tmp/patched.dtb')) {
            outputPath = '/tmp/patched.dtb';
            outputName = 'gatito-patched.dtb';
        } else if (hasFile('/tmp/patched.dts')) {
            outputPath = '/tmp/patched.dts';
            outputName = 'gatito-patched.dts';
        }

        if (!outputPath) {
            throw new Error('A ponte WASM retornou sucesso, mas não criou /tmp/patched.dtb nem /tmp/patched.dts.');
        }

        const bytes = new Uint8Array(Module.FS.readFile(outputPath));
        downloadBytes(bytes, outputName);

        log('[OK] Arquivo gerado: ' + outputName + ' (' + (bytes.byteLength / 1024).toFixed(1) + ' KB)');
        setStatus('Pronto');
    } catch (error) {
        console.error(error);
        log('[ERRO] ' + (error?.message || error));
        setStatus('Erro na geração');
    }
}

function downloadBytes(bytes, filename) {
    const blob = new Blob([bytes], { type: 'application/octet-stream' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');

    a.href = url;
    a.download = filename;
    a.style.display = 'none';

    document.body.appendChild(a);
    a.click();
    a.remove();

    setTimeout(() => URL.revokeObjectURL(url), 1000);
}

function log(msg) {
    const logEl = document.getElementById('log');
    if (!logEl) return;

    const time = new Date().toLocaleTimeString('pt-BR');
    logEl.textContent += '[' + time + '] ' + msg + '\n';
    logEl.scrollTop = logEl.scrollHeight;
}
