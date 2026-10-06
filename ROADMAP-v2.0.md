# DTB-Patcher v2.0 — Roadmap

## Objetivo
Transformar o DTB-Patcher numa plataforma profissional de comparação e transferência de blocos DTB com banco de dados integrado, interface refinada e suporte a múltiplos hardwares.

---

## 📊 Fase 1: Banco de Dados de Blocos Funcionais

### Escopo
- [x] Estrutura `DtbFunctionalBlock` para catalogar blocos
- [x] Coletor automático de blocos a partir de DTBs reais
- [x] Persistência JSON para reutilização
- [x] Sincronização com repositórios online

### Arquivos Novos
- `native/dtb_database.hpp` — Estruturas de banco de dados
- `native/dtb_database.cpp` — Implementação do coletor
- `data/functional-blocks.json` — Banco de dados padrão
- `data/hardware-profiles.json` — Perfis de hardware (R36S, E6, Pixel2, K36, etc.)

### Dados a Catalogar

#### Controles
```
- Play Joystick (K36, E6)
- RockNix Single ADC Joypad
- GPIO Keys
- ADC Keys  
- Rockers (rocker0, rocker1)
```

#### Áudio
```
- RK817 Codec
- I2S Routing (i2s0, i2s1, i2s2)
- Headphone Detect
- Speaker Routing
```

#### Display
```
- MIPI DSI Panels
- Backlight PWM
- Resolution/Timing
- ST7703, Simple Panel
```

#### Energia
```
- Battery Management
- Charger GPIOs
- ADC Channels
- Power Domains
```

---

## 🎨 Fase 2: Interface Refinada (UI/UX v2.0)

### Ajustes Visuais
- [ ] Paleta de cores conforme mock (fundo 244,241,235 / cards 255,253,249)
- [ ] Painéis Doador/Receptor lado-a-lado com cores distintas (marrom/azul)
- [ ] Status colorido: ✅ Compatível (verde) | ❌ Incompatível (vermelho)
- [ ] Ícones melhorados por categoria

### Novos Painéis
- [ ] **Detalhes da Comparação** — mostra propriedades lado-a-lado
- [ ] **Diferenças Encontradas** — lista colorida (verde/vermelho) com timestamps
- [ ] **CSV Export/Import** — salva/carrega configurações de blocos
- [ ] **Base de Dados** — consulta blocos conhecidos, compatibilidades

### Reorganização de Layout
```
[Header]
├─ Logo Gatito + Título + Descrição
├─ Botões: Trocar | Analisar | Gerar
│
[Seleção de DTBs]
├─ Doador (origem)  |  Receptor (base)
│
[Categorias - Grid 3x2]
├─ Controles  |  Áudio      |  Display
├─ Energia    |  Outros     |  [Novo] Base de Dados
│
[Detalhes]
├─ Comparação Lado-a-Lado (Doador vs Receptor)
│
[Diferenças Encontradas]
├─ Log colorido com status
│
[CSV/Exportação]
├─ Importar | Exportar | Sincronizar Base
```

---

## 🔧 Fase 3: Model de Comparação Inteligente

### Mudanças em `dtb_model.cpp`

**Antes:**
```
Compara por path + nome
Mistura dados de fontes diferentes
```

**Depois:**
```
1. Extrai blocos funcionais (independente de path)
2. Cataloga por tipo + propriedades essenciais
3. Compara função com função
4. Classifica compatibilidade com score (0-100%)
5. Retorna mapeamento inteligente
```

### Exemplo: Controles
```
K36:
  bloco: adc-keys
  gpio-pins: [66/11, 66/12, ...]
  
E6:
  bloco: rocknix-singleadc-joypad
  gpio-pins: [bb/01, bb/00, ...]
  
Comparação:
  - Tipo diferente (adc-keys vs rocknix)
  - GPIOs diferentes → incompatível para K36
  - Score: 15% (muito diferente)
  
Transferência:
  - Não recomendada
  - Requer remapeamento manual
```

---

## 📁 Fase 4: Base de Dados Centralizada

### Estrutura
```
data/
├─ functional-blocks.json      # Catálogo de 100+ blocos
├─ hardware-profiles.json      # 10+ hardwares (RK3326, RK3399, etc.)
├─ compatibility-matrix.json   # Matriz de compatibilidades
└─ examples/
    ├─ r36s-v20-base.dtb
    ├─ gamemt-e6-base.dtb
    ├─ gkd-pixel2-base.dtb
    └─ [...]
```

### Sincronização Online
```
- GitHub: OlhaGatito/DTB-Patcher (repo principal)
- Mirrors: hardware-specific branches
- Update Check: botão "Sincronizar Base" na UI
```

---

## 📋 Fase 5: Exportação/Importação

### Formatos Suportados
1. **CSV** — relatório de blocos, compatibilidades, transferências
2. **JSON** — configuração completa (blocos selecionados, mapeamentos)
3. **XML** — suporte futuro para ferramentas externas
4. **DTS** — preview renderizado do novo DTB

### Funcionalidades
- [ ] Salvar comparação como projeto
- [ ] Carregar projeto anterior
- [ ] Gerar relatório de compatibilidade
- [ ] Exportar blocos selecionados para reutilização

---

## 🚀 Timeline

| Fase | Duração | Dependências |
|------|---------|--------------|
| 1. Banco de Dados | 2-3 dias | ✓ Completo |
| 2. Interface | 3-4 dias | Fase 1 |
| 3. Model Inteligente | 2-3 dias | Fases 1, 2 |
| 4. Base Centralizada | 1-2 dias | Fase 1 |
| 5. Export/Import | 2-3 dias | Fases 1, 3, 4 |
| **Total** | **11-15 dias** | |

---

## ✅ Métricas de Sucesso

- [x] Compilação sem erros
- [x] Teste semântico passando ✅ (6/6 blocos)
- [ ] 50+ blocos catalogados
- [ ] Compatibilidade 80%+ entre K36 ↔ E6
- [ ] Interface conforme mock
- [ ] CSV export funcional
- [ ] Base de dados sincronizada

---

## 📝 Notas

- **Foco inicial:** R36S V20 + GameMT E6 + GKD Pixel2 + K36
- **Linguagem:** Português (interface, mensagens, logs)
- **Licença:** Mesma do projeto (check LICENSE)
- **Contribuições:** Community-driven via GitHub Issues

