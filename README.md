# bdump

A windbg-js plugin to dump the cpu state.

## Usage

Only tested with Hyper-V. You'll probably want the VM to have 1 vCPU. Sometimes
kd gets confused about which CPU scripts run on. Additionally, once you hit your
bp you should verify that `GSBASE` and `KERNEL_GSBASE` are different:

```
kd> rdmsr c0000101
msr[c0000101] = 0x41414141
kd> rdmsr c0000102
msr[c0000102] = 0x42424242
```

If they are the same, youll need to hit the bp again. I have no idea why this
happens.

```
kd> .scriptload c:\path\to\bdump.js
kd> !bdump "c:\\path\\to\\dump"
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
