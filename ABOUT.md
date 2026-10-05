# About Gatito Dtb Pacher

Gatito Dtb Pacher is a Windows-native tool created to make Device Tree Blob work safer and easier to inspect.

## What it does

The application compares two DTBs using the terminology:

- Doador — the DTB that contains the property or configuration we want to study.
- Receptor — the DTB that receives a selected change.
- Patch — a newly generated DTB. The original files are never overwritten.

The project is designed around a simple rule:

> inspect first, select explicitly, generate a new file, never modify the source DTBs.

## Native architecture

The production implementation is C++/Win32. The Device Tree Compiler (DTC) is compiled from its official source and linked into the native application instead of being launched as an external dtc.exe process.

The final distribution is intended to be a ready-to-use Windows executable.

## Branding

The application uses the Saruê mascot from the Gatito-Ports project as project branding. The source vector asset is kept in assets/sarue.svg.

## Third-party software

Gatito Dtb Pacher incorporates source code from the Device Tree Compiler project.

DTC upstream:
https://github.com/dgibson/dtc

DTC is licensed under GPL-2.0-or-later. See tools/DTC-LICENSE.txt and THIRD-PARTY-NOTICES.md.

## Project status

The native C++ implementation is under active validation. The old Python implementation and legacy Python build files have been removed from the active repository.

The native implementation must be validated with real DTBs before the application is considered ready for hardware use.
