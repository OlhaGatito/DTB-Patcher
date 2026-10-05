# DTB-Patcher

**Analise, compare e gere novos Device Tree Blobs com controle explicito sobre cada transferencia.**

DTB-Patcher nasceu para evitar o fluxo perigoso de editar DTBs no escuro. O projeto trabalha com **Doador -> Receptor -> Patch**, mantendo os arquivos originais intactos.

![Saruê](assets/sarue.svg)

## ✨ Destaques

- GUI nativa para Windows.
- Migracao para C++/Win32.
- Device Tree Compiler integrado ao processo.
- Nenhum dtc.exe externo exigido pelo aplicativo final.
- Comparacao funcional entre DTBs, organizada por Controles, Audio, Display, Energia e Outros.
- Doador e Receptor exibidos lado a lado em cada bloco.
- Selecao individual somente do lado do Doador.
- Transferencia limitada a propriedades existentes e compativeis no Receptor.
- Doador e Receptor nunca sao sobrescritos.
- Geracao automatica de patch-001.dtb, patch-002.dtb e assim por diante.
- Mascote Saruê do Gatito-Ports.
- Documentacao de arquitetura, seguranca e contribuicao.

## 🧭 Como funciona

    DTB Doador
         |
         v
    DTC nativo -> estrutura DTS
         |
         +---- comparar ----+
         |                  |
         v                  v
    Blocos funcionais  DTB Receptor
         |                   |
         +-- selecao --------+
                  |
                  v
          Receptor como base
                  |
                  v
          Substituicao dos blocos
                  |
                  v
              Novo DTB
         |
         v
    Documents/DTB-Patcher/New dtb/

O objetivo e tornar a transferencia auditavel: o usuario consegue ver o caminho, tipo, categoria e motivo de cada diferenca antes de gerar o arquivo.

## 🖥️ Distribuicao

A versao final sera distribuida como um executavel Windows pronto para uso.

O usuario final nao deve precisar instalar:

- Python;
- PyInstaller;
- GCC;
- MinGW;
- MSYS2;
- Flex/Bison;
- dtc.exe.

Essas ferramentas pertencem ao ambiente de build.

## 🧩 Arquitetura nativa

A implementacao nativa concentra a migracao:

    native/
    ├── main.cpp
    ├── dtb_model.cpp
    ├── dtb_model.hpp
    ├── dtc_bridge.c
    └── dtc_bridge.h

O DTC oficial e compilado durante o build e ligado ao aplicativo. A aplicacao chama o codigo do compilador no mesmo processo.

A interface usa Win32 e GDI+ para manter a distribuicao simples e nativa.

## 🎨 Saruê / Gatito-Ports

A identidade visual usa o Saruê do projeto Gatito-Ports.

Gatito-Ports:
https://github.com/OlhaGatito/Gatito-Ports

O vetor original utilizado pelo DTB-Patcher esta em:

    assets/sarue.svg

## 🛡️ Filosofia de seguranca

**Nunca editar sem entender.**

O projeto segue estas regras:

1. Inspecionar antes de modificar.
2. Comparar estruturalmente.
3. Exigir selecao explicita.
4. Gerar um novo DTB.
5. Preservar Doador e Receptor.
6. Manter backups e resultados numerados.
7. Validar o resultado antes de usa-lo no hardware.

Um DTB incorreto pode impedir o boot ou alterar configuracoes de hardware. Por isso, resultados ainda devem ser considerados artefatos de teste ate serem validados no dispositivo.

## 🔬 Validacao

As areas prioritarias sao:

- phandles;
- GPIO/pinctrl;
- audio-routing;
- display/backlight;
- propriedades binarias;
- strings multiplas;
- /bits/;
- /memreserve/;
- nos com unit address;
- labels;
- referencias por caminho;
- round-trip DTS -> DTB -> DTS;
- geracao repetida de patches.

## 📚 Documentacao

- ABOUT.md — historia, arquitetura e identidade do projeto.
- CONTRIBUTING.md — regras para contribuicao.
- SECURITY.md — seguranca e cuidados com DTBs.
- THIRD-PARTY-NOTICES.md — componentes e ativos de terceiros.
- CHANGELOG.md — historico de mudancas.
- tools/DTC-LICENSE.txt — licenca do DTC.

## 🏗️ Build

GitHub Actions:

    .github/workflows/build-native-windows.yml

Build local:

    build_native_windows.bat

O build de producao baixa o codigo-fonte oficial do DTC, compila os componentes nativos e produz o executavel.

## ⚖️ Licenciamento

O DTB-Patcher incorpora codigo do Device Tree Compiler (DTC), distribuido sob GPL-2.0-or-later.

Consulte THIRD-PARTY-NOTICES.md e tools/DTC-LICENSE.txt antes de redistribuir builds que incorporem o DTC.

## 📌 Status

**Implementacao nativa em C++ em validacao.**

A implementacao Python foi removida do codigo ativo. O fluxo suportado e o executavel nativo, com o DTC integrado no mesmo processo. O executavel ainda deve ser validado com DTBs reais antes de ser considerado pronto para uso em hardware.
