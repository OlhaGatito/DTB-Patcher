# DTB-Patcher

A GUI tool for safely comparing and transferring selected components between Device Tree Blobs (DTB).

## Current status

**Phase 1 — GUI foundation**

The first version provides:

- Source, destination and output DTB selection
- Transfer-category selection
- Compare and Build workflow placeholders
- Operation log
- Progress bar
- Separation between GUI and future DTB backend

The DTB parser, diff engine, transfer engine, validation, and automatic dtc compilation/descompilation will be implemented in the next phase.

## Architecture

    DTB
     ↓
    dtc -I dtb -O dts
     ↓
    DTS parser
     ↓
    Structural comparison
     ↓
    Selected component transfer
     ↓
    Validation
     ↓
    dtc -I dts -O dtb
     ↓
    Patched DTB

## Run

Linux/WSL:

    python3 main.py

Windows:

    python main.py

## Design rule

The GUI must never directly manipulate DTS text. All DTB/DTS parsing and patching belongs to the backend layer. This keeps the transfer engine independently testable and prevents UI code from becoming coupled to device-tree internals.
