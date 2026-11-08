# v11 "Altman's Revenge": naval kamikazes.
# Both are one-way units: the runtime (sg_kamikaze in src/linux/singularity.h)
# removes them, with a blast, the frame after they fire, like the Shahed.
# Suicide=no on purpose: the engine's own Suicide=yes deletes the unit before
# its warhead lands, so the hit would do nothing.

# Shared warhead: a ship-killing splash, full damage against every armour.
add('SGSEABOOMWH','APSplash',None,None,CellSpread='1.5',PercentAtMax='.5',
    Verses='100%,100%,100%,100%,100%,100%,100%,100%,100%,100%,100%',
    AnimList='TWLT070',Wall='yes',Wood='yes')

# ---- Allies: Kamikaze Dolphin ----
# A trained dolphin carrying a limpet charge: swims under the target and
# detonates. Cheap, cloaked under water, gone after one hit.
add('SGDOLPHBOOM','SubTorpedo',None,None,Damage='700',ROF='60',Range='1.5',Speed='40',
    Warhead='SGSEABOOMWH',Report='DolphinAttack',Suicide='no')
add('SGDOLPH','DLPH','VehicleTypes','Kamikaze Dolphin',Image='DLPH',
    Prerequisite='GAYARD',Cost='400',Soylent='200',Strength='150',Speed='10',TechLevel='3',
    Primary='SGDOLPHBOOM',ElitePrimary='SGDOLPHBOOM',GuardRange='7',Sight='6',
    AIBuildThis='yes',AllowedToStartInMultiplayer='no',Points='15')
labels['Name:SGDOLPH']='Kamikaze Dolphin ($400: one hit sinks a destroyer, then it is gone)'

# ---- China: Sea Drone (unmanned surface vessel) ----
# A fast, low explosive boat with no crew: rams ships, naval yards and
# shore buildings, then explodes.
add('SGSEABOOM','FlakTrackGun',None,None,Damage='700',ROF='60',Range='1.2',
    Projectile='InvisibleLow',Warhead='SGSEABOOMWH',Report='GenLargeExplosion',Anim='',Suicide='no')
add('SGSEADRONE','HYD','VehicleTypes','Sea Drone',Image='HYD',
    Owner='Russians,Confederation,Africans,Arabs',Prerequisite='NAYARD',Cost='350',Soylent='175',
    Strength='150',Armor='light',Speed='12',TechLevel='3',
    Primary='SGSEABOOM',ElitePrimary='SGSEABOOM',Secondary='',EliteSecondary='',
    GuardRange='8',Sight='7',AIBuildThis='yes',AllowedToStartInMultiplayer='no',Points='15',
    VoiceSelect='SGDroneSelect',VoiceMove='SGDroneMove',VoiceAttack='SGDroneDive')
labels['Name:SGSEADRONE']='Sea Drone ($350: one-way explosive boat, rams ships and shore buildings)'
