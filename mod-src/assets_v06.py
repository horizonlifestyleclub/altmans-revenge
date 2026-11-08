# Compile the new facility art; preserve native palettes, shadows and team remap.
OUT=ROOT/'outputs/singularity-build/v06'
source=Image.open(OUT/'colossus-source.png').convert('RGBA');source=source.crop(source.getbbox());source.thumbnail((246,172),Image.Resampling.LANCZOS)
unitpal=Image.new('P',(1,1));unitpal.putpalette(UP)
# Restrict quantization to ordinary colours, reserving 0 for transparency and 16-31 for team colour.
fixed=UP[:]
for i in [0,*range(16,32)]:fixed[i*3:i*3+3]=[255,0,255]
qp=Image.new('P',(1,1));qp.putpalette(fixed)
tile=source.convert('RGB').quantize(palette=qp,dither=Image.Dither.NONE);px=tile.load();rgba=source.load()
for yy in range(tile.height):
 for xx in range(tile.width):
  rr,gg,bb,aa=rgba[xx,yy]
  if aa<128:px[xx,yy]=0
  elif bb>rr*1.35 and bb>gg*1.15 and bb>45:px[xx,yy]=min(31,max(16,31-bb//16))
  elif px[xx,yy]==0:px[xx,yy]=1
base=Image.new('P',(346,300));base.putpalette(UP);base.paste(tile,((346-tile.width)//2,236-tile.height))
damaged=base.copy();dp=damaged.load()
for yy in range(300):
 for xx in range(346):
  if dp[xx,yy] and ((xx*17+yy*31)%43)<3:dp[xx,yy]=94
shadow=Image.new('P',base.size);shadow.putpalette(UP);d=ImageDraw.Draw(shadow);d.polygon([(57,177),(173,117),(299,177),(183,237)],fill=1)
shp_write(MOD/'ggsgc.shp',[base,damaged,damaged,shadow,shadow,shadow])
# Subtle LEDs and orbiting defensive drone silhouettes, aligned with the low hall.
anims=[]
for phase in range(12):
 im=Image.new('P',base.size);im.putpalette(UP);d=ImageDraw.Draw(im)
 for j in range(5):
  x=95+j*27;y=181+int(j*4)
  d.point((x,y),fill=18+(phase+j)%6)
 for j in range(3):
  ang=(phase/12+j/3)*math.tau;x=int(173+105*math.cos(ang));y=int(155+38*math.sin(ang))
  d.line((x-3,y,x+3,y),fill=89);d.line((x,y-2,x,y+2),fill=89);d.point((x,y),fill=18)
 anims.append(im)
shp_write(MOD/'sgcorean.shp',anims)
# Reuse the detailed original Battle Fortress geometry, with its independent
# drones providing the carrier identity; retain the original chassis shading.
shutil.copy2(SRC/'local/bfrt.vxl',MOD/'sgtank.vxl');shutil.copy2(SRC/'local/bfrt.hva',MOD/'sgtank.hva')
# Every menu portrait now uses the native RA2 cameo language; custom low-detail
# voxel thumbnail icons are removed. These are adapted stock cameos.
items=[('sglnicon','LINK','aengicon'),('sgopicon','NEST','brisicon'),('sgtankico','BASTION','bfrticon'),('sgneicon','NEEDLE','dronicon'),('sgloicon','LOCUST','roboicon'),('sghdicon','HOUND','adogicon'),('sgnoicon','NODE','ntchicon'),('sgreicon','REACTOR','npwricon'),('sgbaicon','BATTERY','powricon'),('sgfaicon','FACTORY','nwepicon'),('sgemicon','EMP','prisicon'),('sga51ico','AREA 51','heliicon'),('sgb2ico','NIGHTFALL','bplnicon'),('sgravenico','RAVEN','beagicon'),('sgauroraico','AURORA','falcicon'),('sgtempestico','TEMPEST','spypicon')]
icons=[]
for name,label,stem in items:
 im=shp_read(SRC/'sprites'/(stem+'.shp'))[0];im.putpalette(CP)
 icons.append(tile_icon(im.convert('RGB').crop((0,0,60,35)),label,name))
# Facility cameo uses the actual new sprite, with sky backdrop and native palette.
bg=Image.new('RGB',(60,35),(73,99,117));small=source.copy();small.thumbnail((58,35),Image.Resampling.LANCZOS);bg.paste(small,((60-small.width)//2,35-small.height),small)
icons.append(tile_icon(bg,'COLOSSUS','sgcoicon'))
preview=Image.new('RGB',(6*180,3*144),(20,24,25))
for i,im in enumerate(icons):preview.paste(im.resize((180,144),Image.Resampling.NEAREST),(i%6*180,i//6*144))
preview.save(OUT/'installed-icons.png')
frames=[]
for anim in anims:
 im=Image.new('RGBA',base.size,(48,60,42,255))
 for layer in [base,anim]:
  rgba=layer.convert('RGBA');rgba.putalpha(Image.frombytes('L',layer.size,bytes(255 if v else 0 for v in layer.tobytes())));im.alpha_composite(rgba)
 frames.append(im.crop((43,45,303,245)).resize((520,400),Image.Resampling.NEAREST).convert('RGB'))
frames[0].save(OUT/'colossus-native.gif',save_all=True,append_images=frames[1:],duration=120,loop=0)
print('v06: Colossus facility, native cameo set and Battle Fortress-derived nest tank compiled.')
