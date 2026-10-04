# DTB-Patcher

Ferramenta Windows para analisar e gerar Device Tree Blobs (DTB) usando um fluxo Doador -> Receptor.

## Objetivos

- nunca altera o DTB Doador;
- nunca sobrescreve o DTB Receptor;
- decompila DTB -> DTS e compila DTS -> DTB automaticamente;
- mostra diferenças estruturais e permite selecionar transferências;
- grava resultados em `Documents\\DTB-Patcher\\New dtb`;
- gera relatório JSON junto do resultado;
- mantém `Work`, `Backups`, `Projects` e `Logs`;
- usa o Device Tree Compiler (dtc) open source;
- os builds Windows incluem `dtc.exe` junto do programa.

## DTC

O DTB-Patcher usa o **Device Tree Compiler (dtc)** do projeto oficial:

https://github.com/dgibson/dtc

O upstream é distribuído sob GPL v2. O arquivo `tools/DTC-LICENSE.txt` documenta a origem e a licença do componente.

O workflow de Windows compila o `dtc.exe` a partir do código-fonte upstream e o coloca em `tools\\dtc.exe`. O mesmo arquivo é incluído no pacote do DTB-Patcher.

## Estrutura

```
DTB-Patcher/
  dtb_patcher/
  tests/
  tools/
    dtc.exe
    DTC-LICENSE.txt
  .github/workflows/
    build-windows.yml
  main.py
  run_windows.bat
  build_windows.bat
```

## Instalação e teste no Windows

```bat
py -3 -m venv .venv
.venv\\Scripts\\activate
python -m pip install --upgrade pip
python -m pip install pytest pyinstaller
tools\\dtc.exe --version
python -m pytest -q
python main.py
```

## Build local

```bat
build_windows.bat
```

O resultado fica em `dist\\DTB-Patcher\\DTB-Patcher.exe`.

Para um build totalmente reproduzível com o `dtc.exe` compilado automaticamente, use o workflow **Build Windows** em GitHub Actions. O artefato gerado é `DTB-Patcher-Windows.zip` e contém o DTB-Patcher e o DTC.

## Segurança do fluxo

O parser é deliberadamente conservador. Transferências com dependências ou referências ambíguas devem ser recusadas antes de virar uma etapa mais avançada de resolução de phandles.

O Doador permanece somente leitura durante o fluxo. O Receptor original também não é sobrescrito; cada resultado é salvo como um novo DTB numerado.
