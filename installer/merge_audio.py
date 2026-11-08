"""Merge the mod's new sounds into the player's own Yuri's Revenge sound bank.

  python3 merge_audio.py <game folder> <patch.idx> <out folder>

The package ships only the mod's own clips (patch.idx + patch.bag). The game's
sound bank (audio.idx/audio.bag) is read from the player's own copy - from a
loose audio.idx/bag in the game folder, else from inside langmd.mix (an
unencrypted MIX holding audiomd.mix) - and the merged bank is written to the
mod folder. Clips with a name already in the bank replace it.

MIX (unencrypted): uint32 flags | uint16 count | uint32 body size |
count x (uint32 id, uint32 offset, uint32 size) | body. The sound bank is found
by content (audio.idx starts with 'GABA', version 2), so no file-name hashes
are needed.
"""
from pathlib import Path
import struct
import sys

ENTRY = 36   # name[16], offset, size, rate, flags, chunk


def mix_files(blob, depth=0):
    """Every file in an unencrypted MIX, and in MIXes inside it."""
    if len(blob) < 10 or depth > 3:
        return
    flags, = struct.unpack_from('<I', blob)
    pos = 4
    if flags & 0xFFFF:             # old format: no flags word
        pos = 0
    elif flags & 0x20000:          # encrypted header: not one of ours
        return
    count, body = struct.unpack_from('<HI', blob, pos)
    pos += 6
    start = pos + 12 * count
    if count == 0 or start + body > len(blob) + 20:
        return
    for i in range(count):
        _, off, size = struct.unpack_from('<III', blob, pos + 12 * i)
        if start + off + size > len(blob):
            return
        data = blob[start + off:start + off + size]
        yield data
        if size > 10:
            yield from mix_files(data, depth + 1)


def is_idx(data):
    return len(data) >= 12 and data[:4] == b'GABA' and struct.unpack_from('<I', data, 4)[0] == 2 \
        and len(data) == 12 + ENTRY * struct.unpack_from('<I', data, 8)[0]


def bag_size(idx):
    n = struct.unpack_from('<I', idx, 8)[0]
    return max((sum(struct.unpack_from('<II', idx, 12 + ENTRY * i + 16)) for i in range(n)), default=0)


def find_bank(game):
    loose = {p.name.lower(): p for p in game.iterdir()}
    if 'audio.idx' in loose and 'audio.bag' in loose:
        return loose['audio.idx'].read_bytes(), loose['audio.bag'].read_bytes()
    for name in ('langmd.mix', 'audiomd.mix'):
        if name not in loose:
            continue
        files = list(mix_files(loose[name].read_bytes()))
        for idx in filter(is_idx, files):
            need = bag_size(idx)
            # the bag is the file of the right size; its first clip's bytes start it
            for bag in files:
                if len(bag) >= need and len(bag) - need < 16 and not is_idx(bag):
                    return idx, bag
    raise SystemExit('Could not find the sound bank (audio.idx) in ' + str(game))


def entries(idx):
    return [idx[12 + ENTRY * i:12 + ENTRY * (i + 1)] for i in range(struct.unpack_from('<I', idx, 8)[0])]


def merge(game, patch_idx, out):
    idx, bag = find_bank(Path(game))
    p_idx = Path(patch_idx).read_bytes()
    p_bag = Path(patch_idx).with_suffix('.bag').read_bytes()
    new = {e[:16]: e for e in entries(p_idx)}
    kept = [e for e in entries(idx) if e[:16] not in new]
    bag = bytearray(bag)
    for name, e in new.items():
        off, size, rate, fl, chunk = struct.unpack_from('<5I', e, 16)
        kept.append(name + struct.pack('<5I', len(bag), size, rate, fl, chunk))
        bag += p_bag[off:off + size]
    out = Path(out)
    (out / 'audio.bag').write_bytes(bytes(bag))
    (out / 'audio.idx').write_bytes(idx[:8] + struct.pack('<I', len(kept)) + b''.join(kept))
    return len(new), len(kept)


def make_patch(original_idx, modded_idx, out_idx):
    """Packaging side: the clips the mod adds or changes, as a small bank."""
    o_idx = Path(original_idx).read_bytes()
    o_bag = Path(original_idx).with_suffix('.bag').read_bytes()
    m_idx = Path(modded_idx).read_bytes()
    m_bag = Path(modded_idx).with_suffix('.bag').read_bytes()
    orig = {}
    for e in entries(o_idx):
        off, size = struct.unpack_from('<II', e, 16)
        orig[e[:16]] = (e[24:], o_bag[off:off + size])
    out, data = [], bytearray()
    for e in entries(m_idx):
        off, size = struct.unpack_from('<II', e, 16)
        clip = m_bag[off:off + size]
        if orig.get(e[:16]) == (e[24:], clip):
            continue
        out.append(e[:16] + struct.pack('<II', len(data), size) + e[24:])
        data += clip
    Path(out_idx).write_bytes(m_idx[:8] + struct.pack('<I', len(out)) + b''.join(out))
    Path(out_idx).with_suffix('.bag').write_bytes(bytes(data))
    return len(out), len(data)


if __name__ == '__main__':
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    added, total = merge(*sys.argv[1:])
    print('Sound bank: %d mod clips merged into your own %d-clip bank' % (added, total))
