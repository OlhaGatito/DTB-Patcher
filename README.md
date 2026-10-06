# 🐱 Gatito DTB-Patcher

**Ferramenta web para comparar e transferir blocos funcionais entre arquivos Device Tree.**

Acesse agora: **https://olhagatito.github.io/DTB-Patcher/**

---

## 🎯 O que faz

- 📤 **Upload** de dois arquivos DTS (ou DTB convertido para DTS)
- 📊 **Análise automática** de diferenças categorizadas
- ✅ **Seleção granular** de itens a transferir
- 📥 **Download** do novo DTB/DTS modificado
- 🌐 **100% Online** — sem backend, sem instalação

## 🚀 Como usar

### 1️⃣ Abra o site
```
https://olhagatito.github.io/DTB-Patcher/
```

### 2️⃣ Prepare seus arquivos

**Opção A: Usar arquivos DTS (recomendado)**
- Faça upload direto

**Opção B: Converter DTB para DTS**
```bash
dtc -I dtb -O dts seu_arquivo.dtb -o seu_arquivo.dts
```

### 3️⃣ Selecione e analise

1. Clique **Selecionar** para carregar o DTB Doador
2. Clique **Selecionar** para carregar o DTB Receptor
3. Clique **Analisar DTBs**
4. Marque/desmarque itens conforme necessário
5. Clique **Gerar** para download do resultado

## 📋 Categorias

- **🎮 Controles** — Joysticks, botões, GPIO
- **🔊 Áudio** — Codecs, I2S, DAI
- **🖥 Display** — Painel, backlight, DSI
- **🔋 Energia** — Bateria, carregador, ADC
- **🛠 Outros** — Diferenças diversas

## 🛠 Tecnologia

| Componente | Detalhe |
|-----------|--------|
| **Frontend** | HTML5 + CSS3 + JavaScript ES6+ |
| **Backend** | C++ compilado para WebAssembly (Emscripten) |
| **Biblioteca** | libfdt (Device Tree) |
| **Deploy** | GitHub Pages (automático) |

**Totalmente client-side** — nenhum dado sai do seu navegador.

## 📦 Arquivos do Repositório

```
DTB-Patcher/
├── index.html                      # Interface web
├── js/app.js                       # Lógica JavaScript
├── wasm/
│   ├── gatito_dtb_patcher.js      # Glue Emscripten
│   └── gatito_dtb_patcher.wasm    # Binário (172 KB)
├── native/
│   ├── dtb_model.hpp/cpp          # Estruturas DTB
│   └── dtb_database.hpp/cpp       # Análise de blocos
├── .github/workflows/static.yml   # Deploy automático
├── README.md                       # Este arquivo
├── LICENSE                         # GPL-2.0+
└── CHANGELOG.md                    # Histórico de versões
```

## ⚙️ Build (Dev)

Para recompilar o WASM após mudanças:

```bash
# Requer Emscripten SDK
emcc -O3 -s WASM=1 ... dtc-src/libfdt/*.c native/*.cpp -o wasm/gatito_dtb_patcher.js
```

## 📝 Créditos

Desenvolvido para **ROCKNIX** / **Aurknix** / **ArkOS** community.

## 📄 Licença

GPL-2.0+ — compatível com DTC oficial

---

**Problemas?** Abra uma [issue](https://github.com/OlhaGatito/DTB-Patcher/issues)
