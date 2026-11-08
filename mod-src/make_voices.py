from pathlib import Path
import subprocess,struct,wave,json
ROOT=Path.cwd(); out=ROOT/'outputs/redalert2-recomp/mods/yr/Singularity';tmp=ROOT/'work/singularity/voices'
lines={
'SGLinkSelect':('Samantha',["Eyes up. I've got the little guy.","Remote work. Hostile office.","Pilot here. Where's the problem?"]),
'SGLinkMove':('Samantha',["Better signal. Better angle.","I'll walk. It flies."]),
'SGLinkAttack':('Samantha',["Nice tank. Shame about the roof.","Sending a very small problem.","Drone out. Heads down."]),
'SGSeraphSelect':('Alex',["Seraph actual. The sky is spoken for.","Three birds. One bad day.","Big wings. Small problems."]),
'SGSeraphMove':('Alex',["Taking the high road.","Moving the whole neighborhood."]),
'SGSeraphAttack':('Alex',["Open the bays. Let the kids out.","Swarm, meet problem.","Air superiority. Delivered."]),
'SGHoundSelect':('Alex',["Good dog. Bad intentions.","I can hear your firewall."]),
'SGHoundMove':('Alex',["No leash. No noise.","Following the wrong footsteps."]),
'SGHoundAttack':('Alex',["Fetch. Detonate.","This one bites back."]),
'SGChinaSelect':('Tingting',["One operator. Many possibilities.","The nest is ready.","Small machines. Large numbers."]),
'SGChinaMove':('Tingting',["New ground. New nest.","Keep the signal strong."]),
'SGChinaAttack':('Tingting',["One drone is a warning. Two is a plan.","The sky has reinforcements.","They cannot watch every angle."]),
'SGNeedleSelect':('Tingting',["Small target. Big inconvenience.","Built for one important delivery."]),
'SGNeedleMove':('Tingting',["Taking the scenic route.","Cheap does not mean slow."]),
'SGNeedleAttack':('Tingting',["No return ticket.","Delivery confirmed. Almost."]),
'SGLocustSelect':('Tingting',["There are more where I came from.","Economy of force. Literally."]),
'SGLocustMove':('Tingting',["The swarm shifts.","Another angle. Another problem."]),
'SGLocustAttack':('Tingting',["We brought enough for everyone.","Again. And again."]),
'SGCoreReady':('Samantha',["Colossus online. Shall we think bigger?"]),
'SGCoreActive':('Samantha',["Singularity achieved. Their odds have been revised."]),
'SGCoreLost':('Samantha',["Core connection lost. Back to doing this the hard way."]),
}
manifest={}; samples=[]
for group,(voice,texts) in lines.items():
 manifest[group]=[]
 for i,text in enumerate(texts):
  name='sgv%03d'%len(samples);aiff=tmp/(name+'.aiff');wav=out/(name+'.wav')
  subprocess.run(['/usr/bin/say','-v',voice,'-r','178' if voice=='Alex' else '172','-o',str(aiff),text],check=True)
  subprocess.run(['/usr/bin/afconvert','-f','WAVE','-d','LEI16@22050','-c','1',str(aiff),str(wav)],check=True)
  with wave.open(str(wav),'rb') as w:
   assert w.getnchannels()==1 and w.getsampwidth()==2 and w.getframerate()==22050
   data=w.readframes(w.getnframes())
  samples.append((name,data,voice,text));manifest[group].append(name)
idx=(ROOT/'work/singularity/audio/audio.idx').read_bytes();magic,version,count=struct.unpack_from('<III',idx)
assert len(idx)==12+36*count
entries=[idx[12+36*i:12+36*(i+1)] for i in range(count)]
bag=bytearray((ROOT/'work/singularity/audio/audio.bag').read_bytes())
for name,data,voice,text in samples:
 entries.append(struct.pack('<16s5I',name.encode(),len(bag),len(data),22050,4,0));bag+=data
entries.sort(key=lambda b:b[:16].split(b'\0')[0].lower())
(out/'audio.idx').write_bytes(struct.pack('<III',magic,version,len(entries))+b''.join(entries))
(out/'audio.bag').write_bytes(bag)
(tmp/'manifest.json').write_text(json.dumps(manifest,indent=2))
preview=ROOT/'outputs/singularity-build/voice-preview.wav'
with wave.open(str(preview),'wb') as w:
 w.setnchannels(1);w.setsampwidth(2);w.setframerate(22050)
 for name,data,voice,text in [samples[i] for i in [1,5,8,13,16,22,27,33]]:w.writeframes(data+b'\0'*22050)
(ROOT/'outputs/singularity-build/VOICE-LINES.md').write_text('# Synthetic prototype voices\n\nGenerated locally using macOS voices. US: Samantha/Alex; China: Tingting reading English. These are synthetic placeholder performances, not human voice acting.\n\n'+'\n'.join(f'- **{name} / {voice}:** {text}' for name,data,voice,text in samples))
print(f'Generated and packed {len(samples)} new voice clips; preserved {count} original audio samples.')
