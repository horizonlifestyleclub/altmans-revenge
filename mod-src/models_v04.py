from PIL import Image
exec((SRC/"shading_v04.py").read_text())
pal=list((SRC/'sprites/unittem.pal').read_bytes())
def color(rgb):
 return min(range(32,256),key=lambda i:sum((pal[i*3+j]*4-rgb[j])**2 for j in range(3)))
metal=color((85,105,125));light=color((100,115,130));dark=color((35,45,60));blue=18;olive=color((100,115,70))
def carrier(x,y,z,n):
 # Broad continuous swept hull, three recessed launch bays and paired engine pods.
 width=.88-.48*abs(y+.05)
 if abs(x)<width and -.82<y<.72 and 4<=z<=8:
  if y>.32 and any(abs(x-i)<.09 for i in [-.29,0,.29]) and z==4:return blue
  return light if z==8 else metal
 if abs(x)<.19 and -.68<y<.5 and 8<z<=11:return blue if y<-.48 else dark
 if .57<abs(x)<.78 and -.1<y<.83 and 3<=z<=9:return blue if y>.68 else light
 if abs(x)<.45 and -.25<y<.4 and z==3:return dark
 return 0
def fighter(x,y,z,n):
 if 2<=z<=4 and -.85<y<.7 and abs(x)<(.65*(y+.9)/1.6+.12):return blue if abs(x)<.10 else metal
 return 0
def quad(team):
 def shape(x,y,z,n):
  if abs(x)<.25 and abs(y)<.38 and 2<=z<=5:return team if z==5 else metal
  if z==3 and abs(abs(x)-abs(y))<.12 and abs(x)<.7:return light
  if z==4 and any(.12<math.hypot(x-i,y-j)<.26 for i in [-.58,.58] for j in [-.58,.58]):return dark
  if z==2 and abs(x)<.14 and -.5<y<-.3:return team
  return 0
 return shape
voxel('sgseraph',64,56,14,carrier,1)
voxel('sginter',22,26,7,fighter,.85)
voxel('sgneedle',22,28,7,lambda x,y,z,n:18 if fighter(x,y,z,n) and abs(x)<.12 else fighter(x,y,z,n),.85)
voxel('sgscout',24,24,8,quad(blue),.85)
voxel('sgnest',24,24,8,quad(18),.85)
voxel('sglocust',26,26,8,quad(olive),.9)
# Compile painted portraits into the game's native 60x48 indexed SHP format.
atlas=Image.open(ROOT/'outputs/singularity-build/v04/icon-atlas.png').convert('RGB')
W,H=atlas.size
rows=[(0,310,[0,507,861,1238,1619]),(310,600,[0,470,829,1238,1619]),(600,971,[0,403,809,1238,1619])]
names=['sgseicon','sglnicon','sgopicon','sgneicon','sgloicon','sghdicon','sgcoicon','sgnoicon','sgreicon','sgbaicon','sgfaicon','sgemicon']
palette=Image.new('P',(1,1));palette.putpalette([min(255,v*4) for v in (SRC/'sprites/cameo.pal').read_bytes()])
header=(SRC/'sprites/schpicon.shp').read_bytes()[:32]
preview=Image.new('RGB',(480,288))
for row,(top,bottom,edges) in enumerate(rows):
 for col in range(4):
  i=row*4+col
  crop=atlas.crop((round(edges[col]*W/1619)+3,round(top*H/971)+3,round(edges[col+1]*W/1619)-3,round(bottom*H/971)-3))
  tile=crop.resize((60,48),Image.Resampling.LANCZOS).quantize(palette=palette,dither=Image.Dither.FLOYDSTEINBERG)
  raw=bytearray(tile.tobytes());raw=bytes(1 if x==0 else x for x in raw)
  (MOD/(names[i]+'.shp')).write_bytes(header+raw)
  tile.frombytes(raw);preview.paste(tile.convert('RGB').resize((120,96)),(col*120,row*96))
preview.save(ROOT/'outputs/singularity-build/v04/installed-icons.png')
print('Installed six revised voxel models and twelve native menu portraits.')
