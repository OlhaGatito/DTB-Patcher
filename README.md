# 🚀 Gatito DTB‑Patcher

> **Analise, compare e gere novos Device Tree Blobs** com total controle sobre cada transferência.

![Saruê mascot](assets/sarue.svg)

---

## ✨ Principais recursos

| ✅ | Recurso |
|----|---------|
| 🪟 **GUI nativa Windows** (Win32 + GDI+) |
| 🛠️ **Device Tree Compiler embutido** – nada de `dtc.exe` externo |
| 🔍 **Comparação visual por categoria** (Controles, Áudio, Display, Energia, Outros) |
| 📄 **Preview completo do DTS** antes da compilação |
| 📁 **Logs permanentes** em `Documents/Gatito Dtb Patcher/Logs/` |
| 📦 **Geração automática de patches numerados** (`patch‑001.dtb`, `patch‑002.dtb`, …) |
| 🎯 **Transferência auditável** – veja caminho, tipo, categoria e motivo antes de aplicar |

---

## 📦 Instalação (usuário final)

1. Baixe o **executável** da última *release* (arquivo `.exe`).
2. Execute‑o – não precisa instalar **Python, MinGW, GCC, MSYS2, Flex/Bison** ou `dtc`.
3. Na primeira execução, o programa cria `Documents/Gatito Dtb Patcher/` com as pastas de **logs**, **novos DTBs** e **DTBs de referência**.

> 📋 Todos os detalhes técnicos (build, contribuição, segurança, licenças) estão no repositório **`Main`**.

---

## 🛠️ Como funciona (resumido)

```
DTB Doador → DTC nativo → DTS (descompactado)
         → comparação por blocos funcionais →
DTB Receptor (base) → substituição de props →
preview DTS → compilação → novo DTB
```

- **Nenhum arquivo original é sobrescrito** – Doador e Receptor permanecem intactos.
- **Geração automática** de patches numerados.
- **Validação round‑trip** (DTS → DTB → DTS idêntico?).

---

## 🎯 Fluxo típico

1. Abra a GUI.
2. Selecione **DTB Doador** (onde você busca as props novas).
3. Selecione **DTB Receptor** (a base que será alterada).
4. O programa lista as diferenças por **categoria** (GPIO, Áudio, Display, etc).
5. **Selecione explicitamente** quais blocos do Doador você quer transferir para o Receptor.
6. **Preview** do DTS final.
7. **Gere** o novo DTB com um clique.
8. Resultado salvo em `Documents/Gatito Dtb Patcher/New dtb/patch‑001.dtb`.

---

## 📚 Documentação e contribuição

| 📖 Documento | Localização |
|-------------|------------|
| 🏗️ Arquitetura nativa | `Main/docs/DTB-PATCHER-ARCHITECTURE.md` |
| 📖 Guia de contribuição | `Main/CONTRIBUTING.md` |
| 🔐 Segurança e boas práticas | `Main/SECURITY.md` |
| 📦 Licença do DTC (GPL‑2.0‑or‑later) | `Main/tools/DTC-LICENSE.txt` |
| 📄 Histórico de mudanças | `Main/CHANGELOG.md` |

---

## 🧑‍💻 Como contribuir

1. **Fork** do repositório **`Main`** (código‑fonte).
2. Implemente correções ou novas funcionalidades.
3. Abra *pull‑request* descrevendo as mudanças.
4. Se o CI validar, uma nova *release* será publicada aqui, pronta para download.

---

## ⚖️ Licença

O código incorpora o **Device Tree Compiler (DTC)** sob **GPL‑2.0‑or‑later**.  
Consulte `Main/tools/DTC-LICENSE.txt` para detalhes.

---
