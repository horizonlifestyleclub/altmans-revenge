"""Compile original cameo paintings into the native 60x48 RA2 sidebar format.

Artwork is generated separately with image_gen. This step only scales it for the
engine, applies the shipped cameo palette, and adds the standard caption/frame.
Run without --install to prepare and inspect. No gameplay rules are changed.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
import struct
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = __import__('sgpaths').ROOT
OUT = ROOT / 'outputs/singularity-build/v08'
MAIN = ROOT / 'outputs/redalert2-recomp/mods/yr/Singularity'
PALETTE = [min(255, v * 4) for v in (ROOT / 'work/singularity/sprites/cameo.pal').read_bytes()]
# source, native filename, in-icon caption, art section
ITEMS = [
    ('link', 'sglnicon', 'LINK', 'sglink'),
    ('nest', 'sgopicon', 'NEST', 'sgop'),
    ('bastion', 'sgtankico', 'BASTION', 'sgtank'),
    ('hound', 'sghdicon', 'HOUND', 'sghound'),
    ('needle', 'sgneicon', 'NEEDLE', 'sgneedle'),
    ('locust', 'sgloicon', 'LOCUST', 'sglocust'),
    ('pilot', 'sgpdicon', 'DRONE', 'sgscout'),
    ('b2', 'sgb2ico', 'B-2 BOMBER', 'sgb2'),
    ('raven', 'sgravenico', 'RAVEN', 'sgraven'),
    ('aurora', 'sgauroraico', 'AURORA', 'sgaurora'),
    ('tempest', 'sgtempestico', 'TEMPEST', 'sgtempest'),
    ('node', 'sgnoicon', 'NODE', 'NASGN'),
    ('reactor', 'sgreicon', 'REACTOR', 'NAPOWR'),
    ('battery', 'sgbaicon', 'BATTERY', 'GASGB'),
    ('factory', 'sgfaicon', 'FACTORY', 'NASGF'),
    ('aegis', 'sgemicon', 'AEGIS', 'GASGE'),
    ('interference', 'sgjminicon', 'JAMMER', 'NASGJ'),
    ('colossus', 'sgcoicon', 'COLOSSUS', 'GASGC'),
    ('area51', 'sga51ico', 'AREA 51', 'GASGA'),
    # v09 Singularity sidebar cards (superweapon SidebarImage, no art section).
    ('node', 'sgresico', 'RESEARCH', None),
    ('colossus', 'sgsingico', 'SINGULARITY', None),
    ('pilot', 'sgswicon', 'SWARM', 'SGSWARMART'),
]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compile_icons():
    compiled = OUT / 'compiled'
    compiled.mkdir(exist_ok=True)
    palette = Image.new('P', (1, 1))
    palette.putpalette(PALETTE)
    icons = {}
    records = []
    for key, stem, label, section in ITEMS:
        source = OUT / 'sources' / (key + '.png')
        # v09: classic RA2 cameo layout. The picture fills the whole icon and the
        # caption sits on it in light capitals with a black outline, no black bar.
        src = Image.open(source).convert('RGB')
        w, h = src.size
        cw, ch = (w, w * 48 // 60) if w * 48 // 60 <= h else (h * 60 // 48, h)
        src = src.crop(((w - cw) // 2, (h - ch) // 2, (w - cw) // 2 + cw, (h - ch) // 2 + ch))
        canvas = src.resize((60, 48), Image.Resampling.LANCZOS)
        px = canvas.load()
        for y in range(37, 48):
            for x in range(60):
                r_, g_, b_ = px[x, y]
                px[x, y] = (r_ * 3 // 5, g_ * 3 // 5, b_ * 3 // 5)
        # Caption drawn 4x larger, then reduced to crisp one-colour pixels.
        fontfile = '/System/Library/Fonts/Supplemental/Arial Narrow Bold.ttf'
        size = 10
        while size > 7 and ImageDraw.Draw(canvas).textbbox((0, 0), label, font=ImageFont.truetype(fontfile, size))[2] > 56:
            size -= 1
        big = Image.new('L', (240, 64), 0)
        bdraw = ImageDraw.Draw(big)
        font4 = ImageFont.truetype(fontfile, size * 4)
        bbox = bdraw.textbbox((0, 0), label, font=font4)
        assert bbox[2] - bbox[0] <= 232, label
        bdraw.text(((240 - (bbox[2] - bbox[0])) // 2 - bbox[0], 60 - bbox[3]), label, font=font4, fill=255)
        mask = big.resize((60, 16), Image.Resampling.LANCZOS).point(lambda v: 255 if v > 100 else 0)
        outline = mask.filter(ImageFilter.MaxFilter(3))
        canvas.paste((0, 0, 0), (0, 31), outline)
        canvas.paste((238, 238, 238), (0, 31), mask)
        indexed = canvas.quantize(palette=palette, dither=Image.Dither.NONE)
        # Index zero is transparent in SHP; no holes in opaque menu portraits.
        indexed.frombytes(bytes(1 if v == 0 else v for v in indexed.tobytes()))
        data = struct.pack('<4H', 0, 60, 48, 1)
        data += struct.pack('<4H4I', 0, 0, 60, 48, 1, 0, 0, 32)
        data += indexed.tobytes()
        dest = compiled / (stem + '.shp')
        dest.write_bytes(data)
        indexed.save(compiled / (stem + '.png'))
        icons.setdefault(key, indexed.convert('RGB'))
        icons[stem] = indexed.convert('RGB')
        records.append(dict(key=key, file=dest.name, caption=label, section=section,
                            source=str(source.relative_to(OUT)), source_sha256=digest(source), sha256=digest(dest)))
    (OUT / 'cameo-manifest.json').write_text(json.dumps(records, indent=2) + '\n')
    make_preview(icons)
    return records


def make_preview(icons):
    fontpath = '/System/Library/Fonts/Supplemental/Arial.ttf'
    boldpath = '/System/Library/Fonts/Supplemental/Arial Bold.ttf'
    titlefont = ImageFont.truetype(boldpath, 28)
    subfont = ImageFont.truetype(fontpath, 17)
    rows = [
        ('ALLIES / US  |  FIELD UNITS', ['link', 'hound', 'bastion', 'pilot']),
        ('ALLIES / US  |  EXPERIMENTAL AIRCRAFT', ['b2', 'raven', 'aurora', 'tempest']),
        ('ALLIES / US  |  INFRASTRUCTURE', ['colossus', 'area51', 'aegis']),
        ('CHINA / SOVIETS  |  DRONES AND CONTROL', ['nest', 'needle', 'locust', 'interference']),
        ('CHINA / SOVIETS  |  INDUSTRIAL NETWORK', ['factory', 'reactor', 'battery', 'node']),
    ]
    sheet = Image.new('RGB', (1080, 1290), '#131b20')
    draw = ImageDraw.Draw(sheet)
    draw.text((30, 20), 'SINGULARITY  /  ORIGINAL CAMEO ART', font=titlefont, fill='#eee9d6')
    draw.text((30, 61), 'Native 60 x 48 icons enlarged 4x. Every portrait below is newly designed.', font=subfont, fill='#b8c5cd')
    for n, (heading, keys) in enumerate(rows):
        y = 105 + n * 232
        colour = '#83c8e7' if n < 3 else '#ed9c84'
        draw.text((30, y), heading, font=subfont, fill=colour)
        for j, key in enumerate(keys):
            sheet.paste(icons[key].resize((240, 192), Image.Resampling.NEAREST), (30 + j * 260, y + 29))
    sheet.save(OUT / 'original-cameos.png')
    actual = Image.new('RGB', (60 * len(ITEMS), 48), '#000000')
    for n, (_, stem, _, _) in enumerate(ITEMS):
        actual.paste(icons[stem], (n * 60, 0))
    actual.save(OUT / 'actual-size-cameos.png')


def install(records, target):
    assert target.is_dir(), target
    backup = ROOT / 'work/singularity/v08-cameo-backup' / target.name
    backup.mkdir(parents=True, exist_ok=True)
    allowed = {r['file'] for r in records} | {'artmd.ini'}
    before = {p.name: digest(p) for p in target.iterdir() if p.is_file()}
    # Preserve originals once, including the old shared portrait bindings.
    for name in allowed:
        old = target / name
        if old.exists() and not (backup / name).exists():
            shutil.copyfile(old, backup / name)
    if not (backup / 'before.json').exists():
        (backup / 'before.json').write_text(json.dumps(before, indent=2) + '\n')
    for record in records:
        src = OUT / 'compiled' / record['file']
        dst = target / record['file']
        if dst.exists() and digest(dst) == record['sha256']:
            continue
        if target != MAIN and not dst.exists() and (MAIN / dst.name).exists():
            dst.symlink_to(MAIN / dst.name)
        else:
            dst.write_bytes(src.read_bytes())
    path = target / 'artmd.ini'
    text = path.read_text(encoding='cp1252')
    bindings = {r['section']: r['file'][:-4] for r in records if r['section']}
    active = None
    seen = set()
    lines = []
    for line in text.splitlines(keepends=True):
        match = re.match(r'^\[([^]]+)\]', line)
        if match:
            active = match.group(1)
        match = re.match(r'^(Cameo|AltCameo)\s*=', line)
        if active in bindings and match:
            line = match.group(1) + '=' + bindings[active] + '\n'
            seen.add((active, match.group(1)))
        lines.append(line)
    assert all((s, 'Cameo') in seen and (s, 'AltCameo') in seen for s in bindings)
    updated = ''.join(lines)
    if updated != text:
        path.write_text(updated, encoding='cp1252')
    after = {p.name: digest(p) for p in target.iterdir() if p.is_file()}
    assert all(after.get(n) == h for n, h in before.items() if n not in allowed)
    assert not (set(after) - set(before) - allowed)
    for record in records:
        assert digest(target / record['file']) == record['sha256']
    return dict(profile=target.name, directory=str(target), changed=[n for n in after if before.get(n) != after[n]],
                preserved_gameplay_files=True, backup=str(backup))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--install', action='append', type=Path, default=[])
    args = parser.parse_args()
    records = compile_icons()
    # Snapshot both profiles before writing through their shared asset symlinks.
    for target in args.install:
        backup = ROOT / 'work/singularity/v08-cameo-backup' / target.name
        backup.mkdir(parents=True, exist_ok=True)
        for name in {r['file'] for r in records} | {'artmd.ini'}:
            old = target / name
            if old.exists() and not (backup / name).exists():
                shutil.copyfile(old, backup / name)
        if not (backup / 'before.json').exists():
            before = {p.name: digest(p) for p in target.iterdir() if p.is_file()}
            (backup / 'before.json').write_text(json.dumps(before, indent=2) + '\n')
    report = [install(records, p.resolve()) for p in args.install]
    if report:
        (OUT / 'installation.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Compiled', len(records), 'original 60x48 portraits (classic captions) with native cameo palette; installed profiles:', len(report))
