# DTB-Patcher — Native C++ edition

O DTB-Patcher é uma ferramenta Windows para analisar Device Tree Blobs e gerar novos DTBs a partir do fluxo Doador -> Receptor.

## Objetivo

A versão de produção está sendo migrada para C++ nativo para que o usuário final receba um executável pronto: DTB-Patcher.exe.

O programa não depende de Python, PyInstaller, MSYS2, GCC ou de um dtc.exe externo para funcionar.

O Device Tree Compiler (DTC) oficial é incorporado ao processo durante a compilação. O executável final chama o código do DTC no mesmo processo, em vez de iniciar um programa externo.

## Arquitetura

DTB-Patcher.exe -> GUI Win32/C++ -> motor de transferência C++ -> DTC nativo integrado

## Fluxo

1. Selecionar DTB Doador.
2. Selecionar DTB Receptor.
3. Analisar as diferenças.
4. Marcar as transferências desejadas.
5. Gerar um novo DTB.
6. O Doador e o Receptor originais permanecem intactos.
7. Resultados em Documents\DTB-Patcher\New dtb\patch-NNN.dtb.

## DTC

Projeto oficial: https://github.com/dgibson/dtc

O DTC é distribuído sob GPL-2.0-or-later. Os avisos/licença correspondentes permanecem no projeto.

## Build Windows

O workflow .github/workflows/build-native-windows.yml baixa o código-fonte oficial do DTC, gera seus componentes, compila o bridge, o motor C++ e a GUI Win32 e produz DTB-Patcher-Windows-Native.zip.

MSYS2/GCC/Flex/Bison são dependências de build, não dependências do usuário final.

## Migração

A implementação Python permanece temporariamente para comparação e rollback.

A implementação nativa fica em native/main.cpp, native/dtb_model.cpp, native/dtb_model.hpp, native/dtc_bridge.c e native/dtc_bridge.h.

A implementação nativa será validada com DTBs reais antes de substituir definitivamente a versão Python.

## Validação obrigatória

- phandles
- GPIO/pinctrl
- audio-routing
- display/backlight
- propriedades binárias
- strings múltiplas
- /bits/
- /memreserve/
- nós com @
- referências &label e &{/path}
- geração repetida de patch-NNN.dtb

Nenhum resultado deve substituir o Doador ou o Receptor original.
