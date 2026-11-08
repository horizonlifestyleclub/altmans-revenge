# Recover the engine's normal directions from a shipped voxel's surface.
b=template;bl=struct.unpack_from('<I',b,28)[0];ft=830+bl
sx,sy,sz,_=b[ft+88:ft+92];data=830+sx*sy*8;cloud={}
for yy in range(sy):
 for xx in range(sx):
  off=struct.unpack_from('<i',b,830+4*(yy*sx+xx))[0]
  if off<0:continue
  j=data+off;zz=0
  while zz<sz:
   skip,count=b[j:j+2];j+=2;zz+=skip
   for k in range(count):cloud[xx,yy,zz+k]=b[j+2*k+1]
   zz+=count;j+=2*count+1
vectors={}
def gradient(grid,x,y,z):
 return tuple(sum((1 if (x+dx*d,y+dy*d,z+dz*d) not in grid else 0)-(1 if (x-dx*d,y-dy*d,z-dz*d) not in grid else 0) for d in (1,2)) for dx,dy,dz in [(1,0,0),(0,1,0),(0,0,1)])
for xyz,norm in cloud.items():
 g=gradient(cloud,*xyz);v=vectors.setdefault(norm,[0,0,0])
 for i in range(3):v[i]+=g[i]
vectors={k:tuple(q/(sum(t*t for t in v)**.5) for q in v) for k,v in vectors.items() if any(v)}
def voxel(name,nx,ny,nz,shape,scale):
 grid={(x,y,z):shape((x-nx/2)/(nx/2),(y-ny/2)/(ny/2),z,nz) for y in range(ny) for x in range(nx) for z in range(nz)}
 grid={k:v for k,v in grid.items() if v}
 starts=[];ends=[];span=bytearray()
 for y in range(ny):
  for x in range(nx):
   if not any((x,y,z) in grid for z in range(nz)):starts.append(-1);ends.append(-1);continue
   starts.append(len(span));z=0
   while z<nz:
    skip=0
    while z<nz and (x,y,z) not in grid:skip+=1;z+=1
    run=[]
    while z<nz and (x,y,z) in grid:
     g=gradient(grid,x,y,z)
     if not any(g):g=(0,0,1)
     normal=max(vectors,key=lambda k:sum(g[i]*vectors[k][i] for i in range(3)))
     run.extend((grid[x,y,z],normal));z+=1
    count=len(run)//2;span+=bytes([skip,count]+run+[count])
   ends.append(len(span)-1)
 n=nx*ny;body=struct.pack('<%di'%n,*starts)+struct.pack('<%di'%n,*ends)+span
 head=b'Voxel Animation\0'+struct.pack('<4I',1,1,1,len(body))+template[32:802]+name.encode().ljust(16,b'\0')+struct.pack('<3I',0,1,2)
 footer=struct.pack('<3If',0,n*4,n*8,1/12)+ident+struct.pack('<6f',-nx*scale/2,-ny*scale/2,0,nx*scale/2,ny*scale/2,nz*scale)+bytes([nx,ny,nz,4])
 (MOD/(name+'.vxl')).write_bytes(head+body+footer)
 (MOD/(name+'.hva')).write_bytes(b'\0'*16+struct.pack('<II',1,1)+name.encode().ljust(16,b'\0')+ident)
