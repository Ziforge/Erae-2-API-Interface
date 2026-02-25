# Erae API SysEx

The ERAE API is a custom sysex library with messages enabling you to take full control of the ERAE product
point detection and LEDs states.

This repository provides a short example on how to use the API sysex interface with the Erae device.

The ERAE API V2 is defined here:
[Erae_API_V2.pdf](Erae_API_V2.pdf)

## Setup with virtual environment:

**Linux / MacOS:**

Prerequisites:
- `python3`
- `venv`

Create virtual environment and install dependencies:
```
python3 -m venv ve
source ve/bin/activate
pip install -e .
```

**Note:**
On ubuntu, you might need to install the `python3-dev` package

## How to use

Use the Erae Lab to create a layout with an API Zone element:
- `Zone index=0`
- `width => 24` and `height => 24` (for image rendering quality)

Activate virtual envirnonment (Linux / MacOS)
```
source ve/bin/activate
```

Usage:
```
python run_erae_api.py
```

**Note:**
- Select MIDI Input: `Erae 2 MIDI`
- Select MIDI Output: `Erae 2 MIDI`
