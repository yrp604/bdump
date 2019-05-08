# bdump

A windbg-js plugin to dump the cpu state.

## Usage

```
0: kd> .scriptload c:\path\to\bdump.js
0: kd> !bdump "c:\\path\\to\\dump"
```

This will create two files:
- `dump\mem.dmp` is a standard windows dumpfile. Note that this is _not_ a
minidump!
- `dump\regs.json` is a JSON object containing all the register values encoded
as hex strings. E.g

```json
{"rax": "0x41414141", "rbx": "0x42424242", ... }
```

There is also a `cpu-jtoy.py` script which converts the registers from JSON to
YAML. This file is vanilla YAML and is used to allow hex encoding of integers
and avoid the 53-bit limitation in some JSON implementations. Hopefully with
BigInt support in more JS engines soon I can drop this...

This script will also add in the AVX register state with all values zeroed.

## Caveats
- doesn't currently capture `xmm`/`ymm`/`zmm` register states
- doesn't currently capture `mxcsr_mask`
- capturing segments (especially `tr` and `es`) is kinda sketchy
