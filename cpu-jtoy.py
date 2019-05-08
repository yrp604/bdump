#!/usr/bin/env python

import json
import sys
import yaml

"""
This is an annoying hack -- Right now the windbg js dumps integer values as
hex strings, e.g.:
    { "rax": "0x4141" }

This is because js ints are 53 bit, so we can't actually store full 64b
registers. Writing a decoder for this in rust kind sucks for two reasons:
    - We'd need to create a separate decode function for each bitsize we want
    to decode, e.g. one for u16, u32, and u64
    - We'd need to create a separate decode function for each array size we
    want to decide, e.g. [u64; 4 vs [u64; 6]

While this is definitely doable, I'd rather just bang out a json to yaml
converter and have the mofo load yaml instead.
"""

def usage():
    print('Usage: %s <cpu-state.json>' % sys.argv[0])
    sys.exit(-1)

# json hex string decoder class
# https://stackoverflow.com/questions/45068797/how-to-convert-string-int-json-into-real-int-with-json-loads
class HexStr(json.JSONDecoder):
    def decode(self, s):
        result = super(HexStr, self).decode(s)
        return self._decode(result)

    def _decode(self, o):
        if isinstance(o, str) or isinstance(o, unicode):
            try:
                return int(o, 0)
            except ValueError:
                try:
                    return float(o)
                except ValueError:
                    return o
        elif isinstance(o, dict):
            return {k: self._decode(v) for k, v in o.items()}
        elif isinstance(o, list):
            return [self._decode(v) for v in o]
        else:
            return o

# yaml hex encoder functions
# https://stackoverflow.com/questions/9100662/how-to-print-integers-as-hex-strings-using-json-dumps-in-python/9101562#9101562
def hexint_presenter(dumper, data):
    return dumper.represent_int('%#x' % data)

def unicode_representer(dumper, uni):
    node = yaml.ScalarNode(tag=u'tag:yaml.org,2002:str', value=uni)
    return node

yaml.add_representer(int, hexint_presenter)
yaml.add_representer(long, hexint_presenter)
yaml.add_representer(unicode, unicode_representer)

def main():
    if len(sys.argv) != 2: usage()

    json_cpu_file = sys.argv[1]

    with open(json_cpu_file, 'rb') as h:
        j = json.load(h, cls=HexStr)

    # HACK HACK HACK
    # remove when I acutally lift zmm state from windbg
    j['zmm'] = []
    for ii in range(32):
        j['zmm'].append({ 'q': [0 for _ in range(8)] })

    y = yaml.dump(j)

    print(y)

if __name__ == '__main__':
    main()
