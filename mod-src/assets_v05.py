# Native indexed pixel assets. No painted/image-generated atlas in this build.
from PIL import Image,ImageDraw,ImageFont
OUT=ROOT/'outputs/singularity-build/v05';OUT.mkdir(exist_ok=True)
UP=[min(255,v*4) for v in (SRC/'sprites/unittem.pal').read_bytes()]
CP=[min(255,v*4) for v in (SRC/'sprites/cameo.pal').read_bytes()]
metal=92;light=87;dark=95;blue=18
# Detailed geometry: segmented wings, intake lips, raised spine, exhausts, panel seams.
def carrier5(x,y,z,n):
 ax=abs(x)
 if -.88<y<.78:
  width=.24+(.65*(1-abs(y+.05)))
  top=5+int(3*(1-ax))
  if ax<width and 3<=z<=top:
   if z==top:
    if abs((ax*10)%1)<.13:return 95
    if ax>.48 and -.18<y<.40:return 89
    return 91 if ax>.3 else 86
   return 95
 if ax<.15 and -.8<y<.5 and 7<=z<=12:return 18 if z==12 and y<-.5 else 90
 if .49<ax<.73 and -.2<y<.9 and 2<=z<=10:
  if y>.75:return 18 if 4<=z<=7 else 98
  if z==10:return 87 if ax<.54 or ax>.69 else 94
  return 92
 if .27<ax<.41 and -.25<y<.3 and z==2:return 18
 return 0
def bomber(x,y,z,n):
 # B-2 flying wing, no tail fins, serrated trailing edge.
 ax=abs(x);leading=-.85+ax*.95;trailing=.65-ax*.25
 if .28<ax<.55:trailing-=.22
 if leading<y<trailing and ax<.96 and 2<=z<=4:
  if z==4 and (abs(y-leading)<.09 or abs(y-trailing)<.07):return 87
  if z==4 and .18<ax<.32 and -.22<y<.2:return 95
  return 95 if int(ax*18)%5 else 92
 if ax<.15 and -.64<y<.4 and 4<z<=7:return 18 if y<-.42 else 90
 return 0
def jet5(kind):
 def shape(x,y,z,n):
  ax=abs(x)
  if kind==2:
   if -.25<y<.35 and ax<.88 and 2<=z<=4:return 90 if z==4 else 94
   if .42<ax<.58 and -.5<y<.65 and 1<=z<=5:return 18 if y>.5 else 92
  if kind==1 and -.6<y<.65 and ax<(y+.8)*.55 and 2<=z<=3:return 92
  if ax<.13 and -.95<y<.9 and 2<=z<=6:return 18 if z==6 and -.5<y<-.25 else 90
  if -.1<y<.7 and ax<(.84 if kind==0 else .65)*(y+.35) and 2<=z<=3:return 87 if abs(y-.1)<.08 else 93
  if .09<ax<.17 and .55<y<.9 and 4<=z<=9:return 90
  if .3<ax<.5 and .2<y<.65 and z==1:return 18
  return 0
 return shape
voxel('sgseraph',76,68,16,carrier5,.95)
voxel('sgb2',72,46,10,bomber,1)
for name,kind in [('sgraven',0),('sgaurora',1),('sgtempest',2)]:voxel(name,40,56,12,jet5(kind),.9)
# Read TS/RA2 SHP and compile uncompressed indexed frames (supported by the engine).
def shp_read(path):
 b=path.read_bytes();_,w,h,n=struct.unpack_from('<4H',b);frames=[]
 for i in range(n):
  x,y,fw,fh,comp,avg,zero,off=struct.unpack_from('<4H4I',b,8+24*i);im=Image.new('P',(w,h));im.putpalette(UP)
  if not fw or not fh or not off:frames.append(im);continue
  raw=bytearray();pos=off
  if comp==3:
   for row in range(fh):
    size=struct.unpack_from('<H',b,pos)[0];end=pos+size;pos+=2;line=bytearray()
    while pos<end:
     v=b[pos];pos+=1
     if v:line.append(v)
     else:count=b[pos];pos+=1;line.extend(bytes(count))
    raw.extend(line[:fw]+bytes(max(0,fw-len(line))))
  elif comp==2:
   for row in range(fh):
    size=struct.unpack_from('<H',b,pos)[0];raw.extend(b[pos+2:pos+2+fw]);pos+=size
  else:raw.extend(b[pos:pos+fw*fh])
  tile=Image.frombytes('P',(fw,fh),bytes(raw));im.paste(tile,(x,y));frames.append(im)
 return frames

def shp_write(path,frames):
 w,h=frames[0].size;n=len(frames);head=bytearray(struct.pack('<4H',0,w,h,n));body=bytearray();off=8+n*24
 for im in frames:
  # Compact frame crops preserve the original canvas coordinates.
  box=im.getbbox()
  if not box:head+=struct.pack('<4H4I',0,0,0,0,1,0,0,0);continue
  x,y,right,bottom=box;tile=im.crop(box);raw=tile.tobytes()
  head+=struct.pack('<4H4I',x,y,right-x,bottom-y,1,0,0,off+len(body));body+=raw
 path.write_bytes(head+body)
# Bespoke electronics packs/visors on the original animated bodies.
for dest,accent in [('sglink',18),('sgop',18)]:
 frames=shp_read(SRC/'sprites'/('engineer.shp' if dest=='sglink' else 'ggi.shp'))
 for i,im in enumerate(frames):
  if i>=len(frames)//2:continue # retain original shadow frames
  box=im.getbbox()
  if not box:continue
  x,y,r,b=box;cx=(x+r)//2;d=ImageDraw.Draw(im)
  # Compact hard-shell radio pack and antenna, readable at game scale.
  d.rectangle((cx-7,y+8,cx-3,y+18),fill=95,outline=89)
  d.line((cx-6,y+8,cx-7,y+1),fill=91)
  d.rectangle((cx-6,y+10,cx-4,y+12),fill=accent)
  d.line((cx-1,y+6,cx+3,y+6),fill=accent)
  # Dark tactical uniforms replace the original light GI trousers/armour.
  pix=im.load()
  for yy in range(y,b):
   for xx in range(x,r):
    if 40<=pix[xx,yy]<=48:pix[xx,yy]=90+(pix[xx,yy]-40)//2
 shp_write(MOD/(dest+'.shp'),frames)
# Colossus uses a native isometric machine hall and animated illuminated core.
base=shp_read(SRC/'sprites/ggtech.shp');w,h=base[0].size
# Preserve foundation/damage/shadow frame counts, replace visible upper structure.
def core_frame(damaged=False,phase=0,transparent=False):
 im=Image.new('P',(w,h));im.putpalette(UP);d=ImageDraw.Draw(im)
 cx=w//2;cy=h//2+15
 def block(x,y,rx,ry,height,top=89):
  d.polygon([(x-rx,y),(x,y+ry),(x,y+ry-height),(x-rx,y-height)],fill=94)
  d.polygon([(x,y+ry),(x+rx,y),(x+rx,y-height),(x,y+ry-height)],fill=94)
  d.polygon([(x-rx,y-height),(x,y-ry-height),(x+rx,y-height),(x,y+ry-height)],fill=top)
  d.line([(x-rx,y-height),(x,y-ry-height),(x+rx,y-height)],fill=86)
  # Individual steel wall panels and small vents, not blank polygons.
  for q in range(5,rx,7):
   ly=y+int(q*ry/rx)
   d.line((x-rx+q,ly-height+3,x-rx+q,ly-2),fill=92)
   d.line((x+q,y+ry-int(q*ry/rx)-height+3,x+q,y+ry-int(q*ry/rx)-2),fill=95)
  for q in range(4,height-3,7):
   d.line((x-rx+3,y-height+q+2,x-3,y+ry-height+q-2),fill=93)
  # Roof cooling grille and raised lip.
  if rx>20:
   for q in range(-10,11,4):d.line((x-10+q,y-height-5,x+q,y-height),fill=95)

 if not transparent:
  block(cx,cy+28,88,43,12,92)
  for x,y in [(cx-43,cy+6),(cx+43,cy+6),(cx-24,cy+37),(cx+24,cy+37)]:
   block(x,y,22,11,38,91 if not damaged else 94)
   for j in range(5):d.line((x-18,y-30+j*5,x-3,y-22+j*5),fill=95)
  block(cx,cy+3,24,12,99,88)
  for j in range(8):
   yy=cy-80+j*10
   d.line((cx-20,yy,cx-3,yy+8),fill=18+(j%4))
   d.line((cx+3,yy+8,cx+20,yy),fill=18+(j%4))
  # Recessed entrance, two coolant risers and roof capacitors.
  d.polygon([(cx-10,cy+26),(cx+1,cy+31),(cx+1,cy+52),(cx-10,cy+47)],fill=95)
  for yy in range(cy+28,cy+47,4):d.line((cx-9,yy,cx,yy+4),fill=90)
  for xx in [cx-27,cx+27]:
   d.line((xx,cy-10,xx,cy-75),fill=86,width=3)
   for yy in range(cy-70,cy-10,10):d.line((xx-2,yy,xx+2,yy),fill=95)
  for xx in [cx-55,cx+55]:
   d.ellipse((xx-8,cy-34,xx+8,cy-24),fill=95,outline=87)
   d.line((xx-5,cy-29,xx+5,cy-29),fill=90)
  d.line((cx,cy-100,cx,cy-126),fill=87)
  d.ellipse((cx-3,cy-130,cx+3,cy-124),fill=18)
 else:
  yy=cy-80+(phase%8)*10
  d.line((cx-20,yy,cx-3,yy+8),fill=20,width=2)
  d.line((cx+3,yy+8,cx+20,yy),fill=20,width=2)
  for j in range(4):
   ang=phase*math.pi/6+j*math.pi/2;x=int(cx+65*math.cos(ang));y=int(cy-45+24*math.sin(ang))
   d.line((x-4,y,x+4,y),fill=87);d.line((x,y-3,x,y+3),fill=87);d.point((x,y),fill=18)
 return im
for i in range(min(3,len(base))):base[i]=core_frame(i>0)
shp_write(MOD/'ggsgc.shp',base)
shp_write(MOD/'sgcorean.shp',[core_frame(phase=i,transparent=True) for i in range(12)])
# Aircraft airfield gets restrained hazard striping and classified hangar markings.
frames=shp_read(SRC/'sprites/ggairc.shp')
for im in frames[:3]:
 d=ImageDraw.Draw(im);x,y=im.width//2,im.height//2
 for j in range(6):d.line((x-16+j*4,y+19,x-12+j*4,y+21),fill=18)
 d.rectangle((x-5,y-19,x+6,y-12),fill=94);d.text((x-5,y-20),'51',fill=87,font=ImageFont.load_default())
shp_write(MOD/'ggsga.shp',frames)
# Menu icons: render actual native geometry at low resolution, restrained palette,
# single-pixel highlights, stock-style dark frame and a short unit label.
def voxel_icon(shape,nx,ny,nz):
 im=Image.new('RGB',(120,82),(35,45,43));d=ImageDraw.Draw(im)
 for yy in range(82):d.line((0,yy,120,yy),fill=(28+yy//5,36+yy//5,35+yy//6))
 points=[]
 for y in range(ny):
  for x in range(nx):
   for z in range(nz):
    c=shape((x-nx/2)/(nx/2),(y-ny/2)/(ny/2),z,nz)
    if c:points.append((x+y,z,x,y,c))
 scale=min(95/(nx+ny),1.6)
 for _,z,x,y,c in sorted(points):
  px=int(60+(x-y)*scale);py=int(30+(x+y-(nx+ny)/2)*scale*.5-z*1.1)
  rgb=tuple(UP[c*3:c*3+3]);d.rectangle((px,py,px+1,py+1),fill=rgb)
 return im.crop((15,0,105,56)).resize((60,35),Image.Resampling.NEAREST)
def tile_icon(icon,label,name):
 im=Image.new('RGB',(60,48),(10,12,14));im.paste(icon.resize((60,35),Image.Resampling.NEAREST),(0,0));d=ImageDraw.Draw(im)
 d.rectangle((1,35,58,46),fill=(10,12,14));d.rectangle((0,0,59,47),outline=(110,115,110));d.line((1,1,58,1),fill=(170,175,165))
 font=ImageFont.load_default();d.text((3,37),label,fill=(235,227,200),font=font,stroke_width=0)
 pp=Image.new('P',(1,1));pp.putpalette(CP);idx=im.quantize(palette=pp,dither=Image.Dither.NONE)
 idx.frombytes(bytes(1 if v==0 else v for v in idx.tobytes()));shp_write(MOD/(name+'.shp'),[idx]);return im
icons=[]
for name,label,shape,dims in [('sgseicon','SERAPH',carrier5,(76,68,16)),('sgb2ico','B-2',bomber,(72,46,10)),('sgravenico','RAVEN',jet5(0),(40,56,12)),('sgauroraico','AURORA',jet5(1),(40,56,12)),('sgtempestico','TEMPEST',jet5(2),(40,56,12)),('sgneicon','NEEDLE',fighter,(22,28,7)),('sgloicon','LOCUST',quad(18),(26,26,8))]:
 icons.append(tile_icon(voxel_icon(shape,*dims),label,name))
# Native stock cameos supply the same artistic language for adapted structures/personnel.
for name,label,source in [('sglnicon','LINK','aengicon'),('sgopicon','NEST','flkticon'),('sghdicon','HOUND','adogicon'),('sgnoicon','NODE','ntchicon'),('sgreicon','REACTOR','npwricon'),('sgbaicon','BATTERY','powricon'),('sgfaicon','FACTORY','nwepicon'),('sgemicon','EMP','prisicon'),('sga51ico','AREA 51','heliicon')]:
 p=SRC/'sprites'/(source+'.shp')
 assert p.exists(),p
 src=shp_read(p)[0];src.putpalette(CP);icons.append(tile_icon(src.convert('RGB').crop((0,0,60,41)),label,name))
core=core_frame().convert('RGBA');core.putalpha(core_frame().point(lambda x:255 if x else 0).convert('L'))
bg=Image.new('RGBA',core.size,(34,42,40,255));bg.alpha_composite(core)
icons.append(tile_icon(bg.convert('RGB').crop((w//2-95,h//2-125,w//2+95,h//2+65)).resize((60,41),Image.Resampling.NEAREST),'COLOSSUS','sgcoicon'))
preview=Image.new('RGB',(6*180,3*144),(15,20,22))
for i,im in enumerate(icons):preview.paste(im.resize((180,144),Image.Resampling.NEAREST),((i%6)*180,(i//6)*144))
preview.save(OUT/'native-icons.png')
print('v05: native pixel icons, animated Colossus, operator equipment, carrier and four aircraft compiled.')
# Reviewable preview of the exact indexed building frames compiled into the mod.
review=[]
for phase in range(12):
 im=Image.new('RGBA',(w,h),(31,40,37,255))
 for layer in [core_frame(),core_frame(phase=phase,transparent=True)]:
  rgba=layer.convert('RGBA');rgba.putalpha(Image.frombytes('L',layer.size,bytes(255 if v else 0 for v in layer.tobytes())));im.alpha_composite(rgba)
 review.append(im.crop((w//2-105,h//2-135,w//2+105,h//2+90)).resize((420,450),Image.Resampling.NEAREST).convert('RGB'))
review[0].save(OUT/'colossus-core.gif',save_all=True,append_images=review[1:],duration=120,loop=0)
