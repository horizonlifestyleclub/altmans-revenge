from pathlib import Path
import configparser,shutil
root=Path.cwd();src=root/'outputs/redalert2-recomp';lab=root/'work/singularity/lab';mod=lab/'mods/yr/Singularity';mod.mkdir(parents=True,exist_ok=True);(lab/'build').mkdir(exist_ok=True)
for p in (src/'mods/yr/Singularity').iterdir():
 if p.is_file():shutil.copy2(p,mod/p.name)
c=configparser.ConfigParser(interpolation=None,strict=False,delimiters=('=',));c.optionxform=str
c.read(root/'work/singularity/maps/xmp29u2.map',encoding='cp1252')
for group in ['Structures','Units','Infantry']:
 if group not in c:c[group]={}
for i,(typ,x,y,owner) in enumerate([('SGCOLO',49,94,'<Player @ A>'),('NAPOWR',45,94,'<Player @ A>'),('NAPOWR',42,94,'<Player @ A>'),('NAPOWR',45,90,'<Player @ A>'),('NAPOWR',42,90,'<Player @ A>'),('NAPOWR',39,94,'<Player @ A>'),('NAPOWR',39,90,'<Player @ A>'),('NAPOWR',42,86,'<Player @ A>'),('NAPOWR',39,86,'<Player @ A>'),('NAPOWR',39,82,'<Player @ A>'),('GAPOWR',56,94,'<Player @ B>')]):
 c['Structures'][str(99000+i)]=f'{owner},{typ},256,{x},{y},0,None,1,0,1,0,0,None,None,None,0,0'
for i,(typ,x,y) in enumerate([('SGLINK',50,96),('SGSERAPH',52,96),('SGOP',54,96),('SGHOUND',52,98)]):
 group='Units' if typ=='SGSERAPH' else 'Infantry'
 c[group][str(99000+i)]=f'<Player @ A>,{typ},256,{x},{y},'+('0,Guard,0,None,-1,0,0' if group=='Units' else '0,Guard,0,None,-1,0,0')
with (mod/'xmp29u2.map').open('w',encoding='cp1252') as f:c.write(f,space_around_delimiters=False)
p=mod/'RA2MD.INI';c=configparser.ConfigParser();c.optionxform=str;c.read(p);c['Skirmish']['ScenIndex']='0';c['Skirmish']['GameSpeed']='1';c['MultiPlayer']['Side']='Americans'
with p.open('w') as f:c.write(f,space_around_delimiters=False)
print('Created isolated lab overlay; normal mod settings untouched.')
