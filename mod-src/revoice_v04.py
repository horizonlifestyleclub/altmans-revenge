from pathlib import Path
p=Path('work/singularity/make_voices.py');s=p.read_text();start=s.index('lines={');end=s.index('\nmanifest=',start)
s=s[:start]+'''lines={
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
}''' +s[end:]
s=s.replace("'-r','165'","'-r','178' if voice=='Alex' else '172'")
s=s.replace('samples[:2]+samples[10:13]',"[samples[i] for i in [1,5,8,13,16,22,27,33]]")
p.write_text(s)
