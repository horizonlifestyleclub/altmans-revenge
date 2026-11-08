from pathlib import Path
import shlex,subprocess
root=Path.cwd();b=root/'outputs/redalert2-recomp/build-macos';work=root/'work/singularity';cmds=(work/'build-commands.txt').read_text().splitlines();obj=str(work/'lab/main-lab.o')
compile=shlex.split(next(c for c in cmds if ' -c ' in c and '/src/linux/main.c' in c))
compile[compile.index('-o')+1]=obj;compile[compile.index('-MF')+1]=obj+'.d';compile+=['-DSG_LAB=1','-DSG_RESEARCH_TICKS=90','-DSG_INTEGRATION_TICKS=60']
subprocess.run(compile,cwd=b,check=True)
link=shlex.split(cmds[-1].removeprefix(': && ').removesuffix(' && :'))
link=[obj if a=='CMakeFiles/ra2.dir/src/linux/main.c.o' else a for a in link];link[link.index('-o')+1]=str(work/'lab/build/ra2-ui')
subprocess.run(link,cwd=b,check=True)
print('Isolated lab binary: 90-tick research + 60-tick integration. Shipping binary remains 5400 + 1800.')
