# DTB-Patcher

Ferramenta Windows para analisar e gerar Device Tree Blobs (DTB) usando um fluxo Doador -> Receptor.

## Objetivos
- nunca altera o DTB Doador;
- nunca sobrescreve o DTB Receptor;
- decompila DTB -> DTS e compila DTS -> DTB automaticamente;
- mostra diferenças estruturais e permite selecionar transferências;
- grava resultados em `Documents\DTB-Patcher\New dtb`;
- gera relatório JSON junto do resultado;
- mantém `Work`, `Backups`, `Projects` e `Logs`;
- usa `dtc.exe` local, sem download automático.

## Estrutura
```
DTB-Patcher/
  dtb_patcher/
  tests/
  tools/dtc.exe
  main.py
  run_windows.bat
  build_windows.bat
```

## Instalação e teste no Windows
```bat
py -3 -m venv .venv
.venv\Scripts\activate
python -m pip install --upgrade pip
python -m pip install pytest pyinstaller
tools\dtc.exe --version
python -m pytest -q
python main.py
```

## Build
```bat
build_windows.bat
```
O resultado fica em `dist\DTB-Patcher\DTB-Patcher.exe`.

> Nota: o parser é deliberadamente conservador. Transferências com dependências/refs ambíguas devem ser recusadas antes de virar uma etapa mais avançada de resolução de phandles.
