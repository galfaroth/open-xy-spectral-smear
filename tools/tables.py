"""Regenerate original Q31 FFT/Hann constants; no runtime floating point."""
import math
from pathlib import Path
N = 128
q31 = lambda x: round(x * 2147483647)
arrays = {
    'cosine': [q31(math.cos(2 * math.pi * k / N)) for k in range(N // 2)],
    'sine': [q31(math.sin(2 * math.pi * k / N)) for k in range(N // 2)],
    'window': [q31((1 - math.cos(2 * math.pi * k / N)) / 2) for k in range(N)],
    'reverse': [int(f'{k:07b}'[::-1], 2) for k in range(N)],
}
text = '#pragma once\n#include <opxy/types.hpp>\nnamespace spectralsmear { namespace tables {\n'
for name, values in arrays.items():
    kind = 'u8' if name == 'reverse' else 'i32'
    text += f'const opxy::{kind} {name}[{len(values)}] = {{\n'
    for i in range(0, len(values), 8):
        text += '    ' + ', '.join(map(str, values[i:i+8])) + ',\n'
    text += '};\n'
text += '}}\n'
(Path(__file__).resolve().parents[1] / 'tables.h').write_text(text)
