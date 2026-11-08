from pathlib import Path
import shlex,subprocess
root=Path.cwd();b=root/'outputs/redalert2-recomp/build-macos';work=root/'work/singularity';cmds=(work/'build-commands.txt').read_text().splitlines();obj=str(work/'bridge-v07/main-lab.o')
compile=shlex.split(next(c for c in cmds if ' -c ' in c and '/src/linux/main.c' in c))
compile[compile.index('-o')+1]=obj;compile[compile.index('-MF')+1]=obj+'.d';compile+=['-DSG_V07_LAB=1','-DSG_BRIDGE_LAB=1']
subprocess.run(compile,cwd=b,check=True)
link=shlex.split(cmds[-1].removeprefix(': && ').removesuffix(' && :'))
link=[obj if a=='CMakeFiles/ra2.dir/src/linux/main.c.o' else a for a in link];link[link.index('-o')+1]=str(work/'bridge-v07/build/ra2-lab')
subprocess.run(link,cwd=b,check=True)
print('Isolated drone combat lab binary; production research timing unchanged.')
