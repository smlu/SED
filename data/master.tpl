# DESC:   Class Actor
# BBOX: 0 0 0 0 0 0
_actor           none              orient=(0.000000/0.000000/0.000000) type=actor collide=1 move=physics thingflags=0x20000000 mass=150.000000 physflags=0x4a4f maxrotvel=200.000000 maxvel=1.000000 health=40.00 maxhealth=40.00 maxrotthrust=180.00 jumpspeed=1.50 eyeoffset=(0.000000/0.000000/0.037000) minheadpitch=-80.00 maxheadpitch=80.00 lightoffset=(0.000000/0.070000/0.040000) lightintensity=0.80
# DESC:   Player
# BBOX: -0.03730128 -0.01387423 -0.1184614 0.03811364 0.03983956 0.06454442
walkplayer       _actor            type=player thingflags=0x20000401 light=0.200000 model3d=ky.3do size=0.065000 movesize=0.065000 puppet=ky.pup soundclass=ky.snd cog=kyle.cog surfdrag=3.000000 airdrag=0.500000 staticdrag=0.300000 health=100.00 maxhealth=100.00 maxthrust=2.00 typeflags=0x1 error=0.50 fov=0.71 chance=1.00
# DESC:   Class Decor
# BBOX: 0 0 0 0 0 0
_decor           none              orient=(0.000000/0.000000/0.000000) type=cog collide=1 move=path
# DESC:   
# BBOX: 0 0 0 0 0 0
_structure       _decor            collide=3 thingflags=0x8
# DESC:   
# BBOX: 0 0 0 0 0 0
_walkstruct      _structure        thingflags=0x400048
# DESC:   
# BBOX: 0 0 0 0 0 0
_humanactor      _actor            size=0.065000 movesize=0.065000 surfdrag=3.000000 airdrag=0.500000 maxvel=0.500000 maxthrust=0.80 typeflags=0x80000
# DESC:   
# BBOX: 0 0 0 0 0 0
_weapon          none              orient=(0.000000/0.000000/0.000000) type=weapon collide=1 move=physics thingflags=0x20000000 timer=10.000000 mass=5.000000 physflags=0x200 maxrotvel=90.000000 damageclass=0x2 typeflags=0x1
# DESC:   
# BBOX: 0 0 0 0 0 0
_explosion       none              orient=(0.000000/0.000000/0.000000) type=explosion typeflags=0x1 damageclass=0x4
# DESC:   
# BBOX: 0 0 0 0 0 0
+laserhit        _explosion        thingflags=0x1 light=0.200000 timer=0.500000 sprite=bryx.spr soundclass=exp_laserhit.snd typeflags=0x33 blasttime=0.300000 maxlight=0.400000
# DESC:   
# BBOX: 0 0 0 0 0 0
+smflash         _explosion        thingflags=0x1 light=0.400000 timer=0.100000 typeflags=0x0
# DESC:   
# BBOX: 0 0 0 0 0 0
+laserfleshhit   +laserhit         soundclass=exp_fleshy.snd
# DESC:   Bryar Pistol
# BBOX: -0.007791 -0.090587 -0.006258 0.007791 0.017998 0.006258
+bryarbolt       _weapon           thingflags=0x20000001 light=0.400000 model3d=bry0.3do size=0.001000 movesize=0.001000 soundclass=bry.snd creatething=+smflash maxrotvel=0.000000 vel=(0.000000/4.000000/0.000000) explode=+laserhit fleshhit=+laserfleshhit damage=30.000000 mindamage=10.000000 typeflags=0x20440d rate=15.000000
# DESC:   Storm Trooper Rifle
# BBOX: -0.005653 -0.113701 -0.004853 0.005653 0.02259 0.004853
+stlaser         +bryarbolt        model3d=str0.3do soundclass=stlaser.snd vel=(0.000000/6.000000/0.000000) rate=10.000000
# DESC:   Enemy Rifle
# BBOX: -0.005653 -0.113701 -0.004853 0.005653 0.02259 0.004853
+elaser          +stlaser          vel=(0.000000/4.000000/0.000000) damage=12.000000 mindamage=5.000000
# DESC:   ReeYee Brown w St Rifle
# BBOX: -0.046863 -0.025046 -0.131246 0.045997 0.027318 0.073257
reeyeesgun       _humanactor       thingflags=0x20000400 model3d=rystr.3do size=0.072900 movesize=0.072900 puppet=rystr.pup soundclass=rystr.snd cog=actor_rb.cog weapon=+elaser health=50.00 maxhealth=50.00 maxthrust=0.90 maxrotthrust=70.00 jumpspeed=1.20 typeflags=0x1 fireoffset=(0.014700/0.080000/0.034000) aiclass=rydefault.ai
# DESC:   Door 2x3 Single Drk Grey w Round Bump
# BBOX: -0.1 -0.0125 -0.15 0.1 0.0125 0.15
2x3door          _structure        model3d=d2x3.3do size=0.180710 movesize=0.180710 soundclass=sm_door.snd
# DESC:   
# BBOX: -0.15 -0.5 -0.012306 0.15 0.5 0.012306
3x10catwalk      _walkstruct       model3d=c3x10.3do size=0.522160 movesize=0.522160
# DESC:   
# BBOX: -0.1 -0.1 -0.099985 0.1 0.1 0.099985
crate4_3         _walkstruct       model3d=crt4-3.3do size=0.173196 movesize=0.173196
# DESC:   
# BBOX: -0.050887 -0.075508 -0.16702 0.050887 0.25464 0.131736
crane            _decor            thingflags=0x400 model3d=cran.3do size=0.075000 movesize=0.075000 cog=xcrane.cog
# DESC:   
# BBOX: -0.148021 -0.200008 -0.199979 0.148021 0.200008 0.199979
funicular        _walkstruct       model3d=01func.3do size=0.369225 movesize=0.369225 soundclass=funicular.snd
# DESC:   
# BBOX: -0.053401 -0.061619 -0.061702 0.053401 0.061619 0.061702
crate6_2         _walkstruct       thingflags=0x400448 model3d=crt6-2.3do size=0.087201 movesize=0.087201 cog=actor_cr.cog
# DESC:   
# BBOX: -0.038064 -0.024511 -0.05829 0.038064 0.024511 0.05829
console2         _walkstruct       model3d=con2.3do size=0.073806 movesize=0.073806
# DESC:   
# BBOX: 0 0 0 0 0 0
_ghoststructure  _structure        collide=0
# DESC:   
# BBOX: -0.192079 -0.012546 -0.187495 0.192079 0.012546 0.187494
2x3jam           _ghoststructure   model3d=j2x3.3do size=0.268712 movesize=0.268712
# DESC:   
# BBOX: -0.03 -0.03 -0.030002 0.03 0.03 0.030002
crate4_1         _walkstruct       model3d=crt4-1.3do size=0.051962 movesize=0.051962
# DESC:   
# BBOX: -0.05 -0.05 -0.049999 0.05 0.05 0.049999
crate4_2         _walkstruct       model3d=crt4-2.3do size=0.086602 movesize=0.086602
# DESC:   
# BBOX: 0 0 0 0 0 0
+whitecloud      none              orient=(0.000000/0.000000/0.000000) type=particle timer=0.200000 typeflags=0x3f material=00gsmoke.mat range=0.020000 rate=128.000000 maxthrust=30.000000 elementsize=0.007000 count=128
# DESC:   
# BBOX: 0 0 0 0 0 0
+dustcloud       +whitecloud       timer=0.120000 material=dusty.mat range=0.015000 rate=256.000000 maxthrust=80.000000 elementsize=0.010000
# DESC:   
# BBOX: 0 0 0 0 0 0
+punchcloud      +dustcloud        timer=0.150000 material=00gsmoke.mat range=0.006000 rate=64.000000 maxthrust=4.000000 elementsize=0.004000 count=16
# DESC:   
# BBOX: 0 0 0 0 0 0
+punch_exp       _explosion        timer=0.001000 soundclass=exp_punch.snd creatething=+punchcloud typeflags=0x0
# DESC:   
# BBOX: 0 0 0 0 0 0
+punch           _weapon           size=0.001000 movesize=0.001000 mass=50.000000 explode=+punch_exp fleshhit=+punch_exp damage=20.000000 damageclass=0x1 typeflags=0x200d range=0.150000 force=50.000000
# DESC:   
# BBOX: 0 0 0 0 0 0
+ry_punch        +punch            damage=10.000000 force=25.000000
# DESC:   ReeYee Brown w Fists Only
# BBOX: -0.046863 -0.025046 -0.131246 0.045997 0.027318 0.073257
reeyeesfist      _humanactor       thingflags=0x20000400 model3d=ry.3do size=0.072900 movesize=0.072900 puppet=ryfist.pup soundclass=ryfist.snd cog=actor_rf.cog weapon=+ry_punch health=50.00 maxhealth=50.00 maxthrust=1.00 maxrotthrust=70.00 jumpspeed=1.20 typeflags=0x20001 aiclass=rypundefault.ai
# DESC:   
# BBOX: 0 0 0 0 0 0
_droidactor      _humanactor       typeflags=0x80140
# DESC:   Gonk Droid 
# BBOX: -0.034 -0.05906 -0.093167 0.034804 0.05936 0.052273
powerdroid       _droidactor       thingflags=0x20000400 model3d=pd.3do size=0.080000 movesize=0.080000 puppet=pd.pup soundclass=gonk.snd cog=actor_go.cog mass=180.000000 health=50.00 maxhealth=50.00 maxthrust=0.20 maxrotthrust=50.00 typeflags=0x180140 aiclass=gonk.ai
# DESC:   
# BBOX: 0 0 0 0 0 0
_powerup         none              orient=(0.000000/0.000000/0.000000) type=item collide=1 move=physics size=0.100000 movesize=0.010000 surfdrag=3.000000 airdrag=1.000000 mass=10.000000 height=0.050000 physflags=0x400000 angvel=(0.000000/90.000000/0.000000) typeflags=0x1 respawn=30.000000
# DESC:   Shield Pickup
# BBOX: -0.040113 -0.035949 -0.021618 0.040113 0.035949 0.021618
shieldrecharge   _powerup          thingflags=0x400 model3d=shld.3do cog=pow_shields.cog
# DESC:   Door 3x3 Single Drk Grey Warn Sign on Bottom
# BBOX: -0.15 -0.011875 -0.15 0.15 0.011875 0.15
3x3door_1        _walkstruct       model3d=d3x3_1.3do size=0.262464 movesize=0.262464 soundclass=sm_door.snd
# DESC:   
# BBOX: 0 0 0 0 0 0
_ghostdecor      _decor            collide=0
# DESC:   Dynamic Light 1.0
# BBOX: 0 0 0 0 0 0
light1.0         _ghostdecor       thingflags=0x1 light=1.000000
# DESC:   
# BBOX: -0.817281 -1.132091 -0.454926 0.815909 1.276639 1.852229
ghostshuttle     _ghostdecor       model3d=shut.3do size=1.000000 movesize=1.000000
# DESC:   
# BBOX: -0.187173 -0.171934 -0.041875 0.196141 0.200354 0.037814
fan1             none              orient=(0.000000/0.000000/0.000000) type=cog collide=1 move=physics model3d=fan1.3do size=0.005000 movesize=0.005000 physflags=0x200 maxrotvel=90.000000 angvel=(0.000000/90.000000/0.000000)
# DESC:   
# BBOX: -0.181678 -0.060004 -0.073819 0.181678 0.060004 0.073819
roofvent3        _walkstruct       model3d=01rfd3.3do size=0.249375 movesize=0.249375
# DESC:   
# BBOX: -0.13015 -0.091058 -0.100298 0.13015 0.091058 0.100298
roofvent2        _walkstruct       model3d=01rfd2.3do size=0.226595 movesize=0.226595
# DESC:   
# BBOX: -0.01459 -0.075274 -0.015287 0.01459 0.075274 0.015287
roofpipe         _ghostdecor       model3d=01rfd0.3do size=0.077027 movesize=0.077027
# DESC:   Door 4x3 Single Drk Grey with Diagonal Slant 
# BBOX: -0.2 -0.0125 -0.15 0.2 0.0125 0.15
4x3door_1        _structure        model3d=d4x3_1.3do size=0.250312 movesize=0.250312 soundclass=med_door.snd
# DESC:   ReeYee Black w Fists Only
# BBOX: -0.046863 -0.025046 -0.131246 0.045997 0.027318 0.073257
reeyeesbhfist    reeyeesfist       model3d=rh.3do health=60.00 maxhealth=60.00 maxrotthrust=85.00
# DESC:   ReeYee Black w St Rifle
# BBOX: -0.046863 -0.025046 -0.131246 0.045997 0.027318 0.073257
reeyeesbhgun     reeyeesgun        model3d=rhstr.3do health=60.00 maxhealth=60.00 maxrotthrust=80.00 aiclass=rybdefault.ai
# DESC:   
# BBOX: -0.027845 -0.025398 -0.02252 0.027845 0.025398 0.024913
healthpack       _powerup          thingflags=0x408 model3d=hepk.3do cog=pow_health.cog
# DESC:   StormTroop Rifle Pickup
# BBOX: -0.016822 -0.05557 -0.026043 0.007966 0.058 0.014355
strifle          _powerup          thingflags=0x400 model3d=strp.3do cog=pow_strifle.cog
# DESC:   Elevator 2x2 w three legs underneath 
# BBOX: -0.098284 -0.097923 -0.031081 0.097733 0.098093 0.031081
2x2elev          _walkstruct       model3d=e2x2_0.3do size=0.192295 movesize=0.192295 soundclass=sm_elev.snd
# DESC:   
# BBOX: -0.15 -0.4 -0.012306 0.15 0.4 0.012306
3x8catwalk       _walkstruct       model3d=c3x8.3do size=0.427378 movesize=0.427378
# DESC:   EnergyCell Pickup  (pistol,rifle)
# BBOX: -0.008536 -0.014067 -0.022418 0.008536 0.014068 0.022418
energycell       _powerup          thingflags=0x400 model3d=ecel.3do cog=pow_energy.cog
# DESC:   Greedo Pistol Pickup
# BBOX: -0.005081 -0.030803 -0.01143 0.005081 0.030803 0.01143
greedopistol     _powerup          thingflags=0x400 model3d=ggun.3do cog=pow_bryar.cog
# DESC:   Enemy Bryar Pistol
# BBOX: -0.007791 -0.090587 -0.006258 0.007791 0.017998 0.006258
+ebolt           +bryarbolt        model3d=bry1.3do vel=(0.000000/3.500000/0.000000) damage=10.000000 mindamage=5.000000
# DESC:   Greedo
# BBOX: -0.026466 -0.012833 -0.099682 0.026261 0.023106 0.06583
greedo           _humanactor       thingflags=0x20000400 model3d=gr.3do size=0.061500 movesize=0.061500 puppet=gr.pup soundclass=gr.snd cog=actor_gr.cog mass=70.000000 maxvel=0.300000 weapon=+ebolt maxrotthrust=80.00 typeflags=0x2080001 fireoffset=(0.016000/0.055000/0.004000) aiclass=grdefault.ai
# DESC:   Bacta Tank Pickup 
# BBOX: -0.015415 -0.017797 -0.028888 0.015415 0.017797 0.028888
bactatank        _powerup          thingflags=0x400 model3d=bact.3do cog=pow_bacta.cog respawn=60.000000
# DESC:   
# BBOX: 0 0 0 0 0 0
_civilian        _humanactor       surfdrag=2.000000 mass=100.000000 maxvel=0.300000 maxthrust=2.00 maxrotthrust=90.00 typeflags=0x180000
# DESC:   Man 
# BBOX: -0.03954 -0.01907 -0.133437 0.039738 0.026146 0.061238
man2             _civilian         thingflags=0x20000400 model3d=m2.3do size=0.060000 movesize=0.060000 puppet=m2.pup soundclass=mn1.snd cog=actor_pm.cog maxvel=0.200000 health=50.00 maxhealth=50.00 maxthrust=0.50 maxrotthrust=60.00 aiclass=peddefault.ai
# DESC:   Man Narshadda
# BBOX: -0.03954 -0.01907 -0.133437 0.039738 0.026146 0.061238
nman2            man2              cog=actor_mn.cog aiclass=pednarsh.ai
# DESC:   
# BBOX: -0.15 -0.15 -0.150003 0.15 0.15 0.150003
crate4_4         _walkstruct       model3d=crt4-4.3do size=0.259810 movesize=0.259810
# DESC:   
# BBOX: -0.027923 -0.032221 -0.032264 0.027923 0.032221 0.032264
crate6_1         _walkstruct       thingflags=0x400448 model3d=crt6-1.3do size=0.045598 movesize=0.045598 cog=actor_cs.cog
# DESC:   Man 
# BBOX: -0.035537 -0.014648 -0.128737 0.035536 0.025457 0.065502
man1             _civilian         thingflags=0x20000400 model3d=mn.3do size=0.060000 movesize=0.060000 puppet=mn.pup soundclass=mn1.snd cog=actor_pm.cog maxvel=0.200000 health=50.00 maxhealth=50.00 maxthrust=0.50 maxrotthrust=60.00 aiclass=peddefault.ai
# DESC:   Man Narshadda
# BBOX: -0.035537 -0.014648 -0.128737 0.035536 0.025457 0.065502
nman1            man1              cog=actor_mn.cog aiclass=pednarsh.ai
# DESC:   Woman 
# BBOX: -0.025123 -0.01744 -0.127094 0.025123 0.015562 0.056718
woman1           _civilian         thingflags=0x20000400 model3d=wm.3do size=0.056000 movesize=0.056000 puppet=wm.pup soundclass=wm1.snd cog=actor_pf.cog maxvel=0.200000 health=50.00 maxhealth=50.00 maxthrust=0.50 maxrotthrust=60.00 aiclass=peddefault.ai
# DESC:   Woman Narshadda 
# BBOX: -0.025123 -0.01744 -0.127094 0.025123 0.015562 0.056718
nwoman1          woman1            cog=actor_fn.cog aiclass=pednarsh.ai
# DESC:   IR Goggles Pickup 
# BBOX: -0.036535 -0.022706 -0.017295 0.036535 0.022706 0.017295
goggles          _powerup          thingflags=0x400 model3d=gogl.3do cog=pow_goggles.cog respawn=60.000000
# DESC:   
# BBOX: -5.540396 -4.990839 -1.191934 5.533077 4.990839 3.900447
freighter        _ghoststructure   model3d=01frt.3do size=7.945067 movesize=7.945067
# DESC:   R5 Droid
# BBOX: -0.033545 -0.020533 -0.064529 0.03321 0.020809 0.02539
r5               _droidactor       thingflags=0x20000400 model3d=r5.3do size=0.036000 movesize=0.036000 puppet=r5.pup soundclass=r2.snd cog=actor_r5.cog maxvel=0.400000 health=50.00 maxhealth=50.00 maxthrust=0.40 maxrotthrust=50.00 typeflags=0x180140 aiclass=drdefault.ai
# DESC:   Smuggler Backpack Pickup
# BBOX: -0.021089 -0.023005 -0.035347 0.016307 0.023005 0.035347
smbp             _powerup          thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk.cog
# DESC:   
# BBOX: 0 0 0 0 0 0
_zwalkstruct     _structure        thingflags=0x400040
# DESC:   
# BBOX: -0.549776 -0.490931 -0.315187 0.549776 0.4752 0.320955
tiebomber        _zwalkstruct      model3d=tieb.3do size=0.500000 movesize=0.500000 puppet=cr.pup
# DESC:   Elevator 3x3 one end not textured 
# BBOX: -0.148 -0.148 -0.013055 0.148 0.148 0.013695
3x3elev          _walkstruct       model3d=e3x3.3do size=0.209751 movesize=0.209751 soundclass=sm_elev.snd
# DESC:   8t88 Droid
# BBOX: -0.032005 -0.012042 -0.115282 0.032144 0.017313 0.07605
8t88             _humanactor       model3d=8t.3do size=0.060000 movesize=0.060000 puppet=8t.pup soundclass=8t.snd mass=300.000000 health=10000.00 maxhealth=10000.00 maxrotthrust=70.00 typeflags=0x100 aiclass=default.ai
# DESC:   
# BBOX: -0.063294 -0.054836 -0.040528 0.063294 0.054836 0.040528
00table          _walkstruct       thingflags=0x48 model3d=00tabl.3do size=0.070420 movesize=0.070420
# DESC:   
# BBOX: -0.039015 -0.039769 -0.200258 0.039015 0.039769 0.200258
column01         _structure        model3d=01colm.3do size=0.204987 movesize=0.204987
# DESC:   Man Bartender
# BBOX: -0.03954 -0.01907 -0.133437 0.039738 0.026146 0.061238
nbartender       nman2             cog=actor_bt.cog
# DESC:   
# BBOX: -0.07604212 -0.07113472 -0.05297579 0.06296778 0.08321835 0.05456368
tuskensoused     _ghostdecor       move=none model3d=thout.3do size=0.227490 movesize=0.227490 puppet=cr.pup
# DESC:   Ghost Object
# BBOX: 0 0 0 0 0 0
ghost            none              orient=(0.000000/0.000000/0.000000) type=ghost move=path
# DESC:   Armor Vest Pickup
# BBOX: -0.021293 -0.02235 -0.028598 0.01838 0.02235 0.028374
fullshield       _powerup          thingflags=0x400 model3d=vest.3do cog=pow_vest.cog respawn=60.000000
# DESC:   Door 10x10 Single Thinner end ,used in narshadda above crates 
# BBOX: -0.498 -0.998 -0.1 0.498 -0.002 0
10x10door        _walkstruct       model3d=d10x10.3do size=1.169825 movesize=1.169825 soundclass=lg_elev.snd
# DESC:   Thermal Detonators Pickup
# BBOX: -0.020824 -0.043225 0.000892 0.011798 0.043225 0.024826
detonator        _powerup          thingflags=0x400 model3d=detp.3do cog=pow_thermal.cog
# DESC:   
# BBOX: -0.016298 -0.015506 -0.024816 0.016298 0.015506 0.024816
bottle04         _structure        model3d=bottle4.3do size=0.080942 movesize=0.080942
# DESC:   
# BBOX: 0 0 0 0 0 0
+firecloud       +dustcloud        material=00explosion.mat range=0.050000 rate=64.000000 maxthrust=40.000000 count=64
# DESC:   
# BBOX: 0 0 0 0 0 0
+grenade_exp     _explosion        thingflags=0x1 light=0.200000 timer=0.800000 sprite=detx.spr soundclass=exp_det.snd creatething=+firecloud typeflags=0x17 damage=75.000000 blasttime=0.700000 force=100.000000 maxlight=0.800000 range=0.450000
# DESC:   
# BBOX: 0 0 0 0 0 0
+remote_exp      +grenade_exp      sprite=debrisx.spr soundclass=exp_remote.snd damage=0.000000 force=25.000000
# DESC:   
# BBOX: 0 0 0 0 0 0
+mouse_exp       +remote_exp       force=10.000000
# DESC:   Mousebot Droid
# BBOX: -0.018576 -0.029933 -0.04006 0.017936 0.031186 -0.009989
mousebot         _droidactor       thingflags=0x20000400 model3d=mb.3do size=0.039000 movesize=0.039000 soundclass=mousebot.snd cog=actor_mb.cog mass=20.000000 height=0.040000 health=10.00 maxthrust=1.00 maxrotthrust=80.00 typeflags=0x20 explode=+mouse_exp aiclass=mouse.ai
# DESC:   
# BBOX: -0.012516 -0.012516 -0.012524 0.012516 0.012516 0.014655
+grenade2        _weapon           timer=3.000000 model3d=det0.3do size=0.014895 movesize=0.014895 soundclass=det.snd surfdrag=3.000000 airdrag=0.800000 mass=1.000000 physflags=0x225 vel=(0.000000/2.000000/1.500000) angvel=(90.000000/45.000000/90.000000) buoyancy=0.250000 explode=+grenade_exp fleshhit=+grenade_exp damageclass=0x4 typeflags=0x40309
# DESC:   
# BBOX: -0.012516 -0.012516 -0.012524 0.012516 0.012516 0.014655
+grenade1        +grenade2         timer=10.000000 typeflags=0x4030d
# DESC:   
# BBOX: 0 0 0 0 0 0
+crossbowhit     +laserhit         sprite=bowx2.spr soundclass=exp_bowhit.snd
# DESC:   
# BBOX: 0 0 0 0 0 0
+lgflash         _explosion        thingflags=0x1 light=0.300000 timer=0.400000 typeflags=0x12 blasttime=0.200000 maxlight=1.000000
# DESC:   Crossbow
# BBOX: -0.020392 -0.135722 -0.016856 0.021073 0.013215 0.015722
+crossbowbolt    _weapon           thingflags=0x20000001 light=0.500000 model3d=bow0.3do size=0.005000 movesize=0.005000 soundclass=bow.snd creatething=+lgflash vel=(0.000000/5.000000/0.000000) angvel=(0.000000/0.000000/120.000000) explode=+crossbowhit fleshhit=+crossbowhit damage=60.000000 mindamage=20.000000 damageclass=0x4 typeflags=0x20440d rate=10.000000
# DESC:   
# BBOX: 0 0 0 0 0 0
+crossbowhit2    _explosion        thingflags=0x1 light=0.200000 timer=0.500000 sprite=bowx.spr typeflags=0x33 blasttime=0.300000 maxlight=0.400000
# DESC:   Crossbow Spread Shots
# BBOX: -0.014666 -0.12215 -0.016699 0.015346 0.011894 0.015879
+crossbowbolt2   +crossbowbolt     model3d=bow1.3do soundclass=none creatething=none explode=+crossbowhit2 fleshhit=+crossbowhit2 damage=40.000000 typeflags=0x440d
# DESC:   Crossbow Bouncing Shots
# BBOX: -0.020392 -0.135722 -0.016856 0.021073 0.013215 0.015722
+crossbowbolt3   +crossbowbolt     typeflags=0x28440d
# DESC:   
# BBOX: 0 0 0 0 0 0
+repeaterhit     +laserhit         sprite=rptx.spr soundclass=exp_rpthit.snd
# DESC:   
# BBOX: 0 0 0 0 0 0
+rpt_sparks      none              orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=0.700000 mass=0.100000 physflags=0x400201 vel=(0.000000/0.000000/0.200000) typeflags=0x27 material=sparky.mat range=0.040000 rate=24.000000 maxthrust=7.000000 elementsize=0.005000 count=24
# DESC:   
# BBOX: 0 0 0 0 0 0
+rptfleshhit     _explosion        thingflags=0x1 light=0.100000 timer=0.800000 soundclass=exp_fleshy.snd creatething=+rpt_sparks typeflags=0x12 blasttime=0.700000 maxlight=0.300000
# DESC:   Repeater Gun
# BBOX: -0.005771 -0.015021 -0.00466 0.005771 0.003935 0.00466
+repeaterball    _weapon           thingflags=0x20000001 light=0.300000 model3d=rpt0.3do size=0.005000 movesize=0.005000 soundclass=rep.snd creatething=+smflash vel=(0.000000/6.000000/0.000000) explode=+repeaterhit fleshhit=+rptfleshhit damage=20.000000 mindamage=5.000000 typeflags=0x440d rate=20.000000
# DESC:   
# BBOX: 0 0 0 0 0 0
+firecloud2      +firecloud        elementsize=0.012500
# DESC:   
# BBOX: 0 0 0 0 0 0
+firecloud3      +firecloud        maxthrust=20.000000 count=32
# DESC:   
# BBOX: 0 0 0 0 0 0
+debris_exp      _explosion        timer=1.000000 sprite=debrisx.spr soundclass=exp_small.snd creatething=+firecloud3 typeflags=0x7 blasttime=0.500000
# DESC:   
# BBOX: 0 0 0 0 0 0
_debris          none              orient=(0.000000/0.000000/0.000000) type=weapon collide=1 move=physics timer=1.100000 airdrag=3.000000 physflags=0x201 vel=(0.000000/4.000000/0.000000) angvel=(5.000000/10.000000/0.000000) explode=+debris_exp damage=5.000000 damageclass=0x1 typeflags=0xc
# DESC:   
# BBOX: 0 0 0 0 0 0
_debris2         _debris           explode=+smflash
# DESC:   
# BBOX: -0.026724 -0.026724 -0.044654 0.026724 0.026724 0.044654
shrapnel2_1      _debris2          model3d=shrp_1.3do size=0.045000 movesize=0.045000
# DESC:   
# BBOX: -0.011171 -0.010389 -0.042003 0.011171 0.010389 0.042003
shrapnel2_2      _debris2          model3d=shrp_2.3do size=0.040000 movesize=0.040000
# DESC:   
# BBOX: -0.015988 -0.015443 -0.026188 0.015988 0.015443 0.026188
shrapnel2_3      _debris2          model3d=shrp_3.3do size=0.028000 movesize=0.028000
# DESC:   
# BBOX: -0.016213 -0.014647 -0.020855 0.016213 0.014647 0.020855
shrapnel2_4      _debris2          model3d=shrp_4.3do size=0.026000 movesize=0.026000
# DESC:   
# BBOX: 0 0 0 0 0 0
+sequencer_exp   _explosion        thingflags=0x1 light=0.200000 timer=0.800000 sprite=seqx2.spr soundclass=exp_med.snd creatething=+firecloud2 typeflags=0x17 damage=100.000000 blasttime=0.700000 force=300.000000 maxlight=0.800000 range=0.600000 debris=shrapnel2_1 debris=shrapnel2_2 debris=shrapnel2_3 debris=shrapnel2_4
# DESC:   Sequencer Mine
# BBOX: -0.022216 -0.023897 -0.005309 0.022216 0.023897 0.005309
+seqchrg         +grenade2         timer=1.000000 model3d=seq0.3do size=0.010000 movesize=0.010000 soundclass=seq.snd surfdrag=5.000000 airdrag=1.000000 physflags=0x29d vel=(0.000000/0.100000/0.000000) angvel=(0.000000/0.000000/0.000000) buoyancy=0.150000 explode=+sequencer_exp fleshhit=+sequencer_exp typeflags=0x40380
# DESC:   
# BBOX: -0.022216 -0.023897 -0.005309 0.022216 0.023897 0.005309
+seqchrg2        +seqchrg          thingflags=0x20000400 timer=60.000003 cog=class_sequencer.cog
# DESC:   
# BBOX: -0.026724 -0.026724 -0.044654 0.026724 0.026724 0.044654
shrapnel_1       _debris           model3d=shrp_1.3do size=0.045000 movesize=0.045000
# DESC:   
# BBOX: -0.011171 -0.010389 -0.042003 0.011171 0.010389 0.042003
shrapnel_2       _debris           model3d=shrp_2.3do size=0.040000 movesize=0.040000
# DESC:   
# BBOX: -0.015988 -0.015443 -0.026188 0.015988 0.015443 0.026188
shrapnel_3       _debris           model3d=shrp_3.3do size=0.028000 movesize=0.028000
# DESC:   
# BBOX: -0.016213 -0.014647 -0.020855 0.016213 0.014647 0.020855
shrapnel_4       _debris           model3d=shrp_4.3do size=0.026000 movesize=0.026000
# DESC:   
# BBOX: 0 0 0 0 0 0
+raildet_exp     _explosion        thingflags=0x1 light=0.200000 timer=0.500000 sprite=rldx.spr soundclass=exp_raildet.snd typeflags=0x17 damage=110.000000 blasttime=0.700000 force=300.000000 maxlight=0.800000 range=0.500000 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_4
# DESC:   Rail Detonator
# BBOX: -0.01058 -0.013704 -0.006548 0.010593 0.009939 0.012329
+raildet2        +grenade1         thingflags=0x20000400 timer=2.500000 model3d=rld0.3do size=0.003000 movesize=0.003000 puppet=ra.pup soundclass=rail.snd creatething=+lgflash cog=00_smoketrail.cog airdrag=0.000000 height=0.003000 physflags=0x200 vel=(0.000000/2.500000/0.000000) angvel=(0.000000/0.000000/90.000000) explode=+raildet_exp fleshhit=+raildet_exp damage=5.000000 typeflags=0x240b81
# DESC:   
# BBOX: 0 0 0 0 0 0
+raildet_exp2    +raildet_exp      debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_4
# DESC:   
# BBOX: -0.01058 -0.013704 -0.006548 0.010593 0.009939 0.012329
+raildet         +raildet2         timer=10.000000 explode=+raildet_exp2 fleshhit=+raildet_exp2 damage=20.000000 typeflags=0x24020d
# DESC:   
# BBOX: 0 0 0 0 0 0
+conccloud       +dustcloud        rate=512.000000 maxthrust=100.000000 count=256 minsize=0.012000 pitchrange=5.000000
# DESC:   
# BBOX: 0 0 0 0 0 0
+conc_exp        _explosion        thingflags=0x1 light=0.300000 timer=1.000000 sprite=conx.spr soundclass=exp_conc.snd creatething=+conccloud typeflags=0x17 damage=80.000000 blasttime=1.000000 force=200.000000 maxlight=0.800000 range=0.800000
# DESC:   Concussion Rifle
# BBOX: -0.01341 -0.149802 -0.01341 0.01341 0.015603 0.01341
+concbullet      _weapon           thingflags=0x20000001 model3d=con0.3do size=0.005000 movesize=0.005000 soundclass=conc.snd creatething=+lgflash vel=(0.000000/7.000000/0.000000) explode=+conc_exp fleshhit=+conc_exp damage=20.000000 typeflags=0x20000d
# DESC:   
# BBOX: 0 0 0 0 0 0
+concblast2p     +dustcloud        orient=(90.000000/0.000000/0.000000) range=0.001500 rate=128.000000 elementsize=0.005000 count=64 minsize=0.001200 pitchrange=5.000000
# DESC:   
# BBOX: 0 0 0 0 0 0
+concblast3p     +concblast2p      typeflags=0x2b material=00ramp4.mat maxthrust=100.000000
# DESC:   
# BBOX: 0 0 0 0 0 0
+concblast2      _weapon           size=0.005000 movesize=0.005000 fleshhit=+concblast3p trailthing=+concblast2p elementsize=0.300000 damage=80.000000 mindamage=20.000000 typeflags=0xa00d range=5.000000 rate=1.000000
# DESC:   
# BBOX: 0 0 0 0 0 0
+axe_exp         _explosion        timer=0.001000 soundclass=exp_axe.snd typeflags=0x0
# DESC:   
# BBOX: 0 0 0 0 0 0
+gamaxe          _weapon           size=0.001000 movesize=0.001000 mass=100.000000 explode=+axe_exp fleshhit=+axe_exp damage=40.000000 damageclass=0x1 typeflags=0x200d range=0.250000 force=50.000000
# DESC:   
# BBOX: 0 0 0 0 0 0
+force_repel     _explosion        thingflags=0x1 light=0.500000 timer=0.400000 soundclass=exp_dest.snd typeflags=0x52 blasttime=0.300000 force=200.000000 maxlight=1.000000 range=0.500000
# DESC:   
# BBOX: -0.007659 -7E-6 -0.008551 0.008867 0.076357 0.007405
+force_ltpeice   none              orient=(0.000000/0.000000/0.000000) type=weapon move=physics timer=0.250000 model3d=ligt2.3do size=0.005000 movesize=0.005000 physflags=0x200 maxrotvel=360.000000 angvel=(0.000000/0.000000/360.000000)
# DESC:   
# BBOX: 0 0 0 0 0 0
+lightninghit    none              orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=0.200000 typeflags=0x2b material=00ramp4.mat range=0.020000 rate=16.000000 maxthrust=8.000000 elementsize=0.003000 count=16
# DESC:   Force Lightning
# BBOX: 0 0 0 0 0 0
+force_lightning _weapon           thingflags=0x1 light=0.100000 timer=0.100000 size=0.005000 movesize=0.005000 vel=(0.000000/1.000000/0.000000) angvel=(0.000000/0.000000/360.000000) explode=+lightninghit fleshhit=+lightninghit trailthing=+force_ltpeice elementsize=0.075000 trailrandangle=30.000000 damage=13.000000 damageclass=0x8 typeflags=0x1840d range=1.500000
# DESC:   
# BBOX: 0 0 0 0 0 0
+force_lightning2+force_lightningdamage=15.000000                 
# DESC:   
# BBOX: 0 0 0 0 0 0
+force_lightning3+force_lightningdamage=20.000000                 
# DESC:   
# BBOX: 0 0 0 0 0 0
+force_lightning4+force_lightningdamage=25.000000                 
# DESC:   LightSaber Pickup
# BBOX: -0.008988 -0.030139 -0.030139 0.008988 0.030139 0.030139
lightsaber       _powerup          thingflags=0x400 model3d=sabp.3do cog=pow_saber.cog
# DESC:   Force Saber
# BBOX: -0.002761 -0.017436 -0.002868 0.002754 0.16643 0.003496
+force_saber     _weapon           thingflags=0x20000001 light=0.100000 timer=1.000000 model3d=sab0.3do size=0.005000 movesize=0.005000 vel=(0.000000/1.000000/0.000000) angvel=(0.000000/150.000000/0.000000) explode=lightsaber fleshhit=lightsaber damage=50.000000 damageclass=0x10 typeflags=0x40d
# DESC:   Force Shield
# BBOX: 0 0 0 0 0 0
+force_shield    none              orient=(0.000000/0.000000/0.000000) type=cog move=physics timer=61.000003 particle=sphere.par angvel=(60.000000/60.000000/60.000000)
# DESC:   Force Blinding
# BBOX: 0 0 0 0 0 0
+force_blind     +dustcloud        material=00teleport.mat range=0.050000 rate=32.000000 elementsize=0.003000 minsize=0.020000 pitchrange=1.000000 yawrange=1.000000
# DESC:   
# BBOX: 0 0 0 0 0 0
+smoke           none              orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=0.800000 physflags=0x20000 vel=(0.000000/0.000000/0.120000) angvel=(0.000000/90.000000/0.000000) typeflags=0x3e material=00gsmoke.mat range=0.030000 elementsize=0.005000 count=8
# DESC:   
# BBOX: 0 0 0 0 0 0
+dest_trail      +smoke            vel=(0.000000/0.000000/0.000000) material=dstructparts.mat
# DESC:   
# BBOX: 0 0 0 0 0 0
+dest_cloud      +firecloud        material=dstructparts.mat
# DESC:   
# BBOX: 0 0 0 0 0 0
+force_dest1     _explosion        thingflags=0x1 light=0.100000 timer=0.400000 sprite=destruct8.spr soundclass=exp_dest.snd creatething=+dest_cloud typeflags=0x53 damage=15.000000 damageclass=0x8 blasttime=0.300000 force=100.000000 maxlight=0.500000 range=0.500000
# DESC:   
# BBOX: 0 0 0 0 0 0
+force_dest2     _explosion        thingflags=0x1 light=0.200000 timer=0.600000 sprite=destruct8.spr soundclass=exp_dest.snd creatething=+dest_cloud typeflags=0x53 damage=30.000000 damageclass=0x8 blasttime=0.400000 force=200.000000 maxlight=0.600000 range=1.000000
# DESC:   
# BBOX: 0 0 0 0 0 0
+force_dest3     _explosion        thingflags=0x1 light=0.300000 timer=0.800000 sprite=destruct8.spr soundclass=exp_dest.snd creatething=+dest_cloud typeflags=0x53 damage=45.000000 damageclass=0x8 blasttime=0.500000 force=300.000000 maxlight=0.700000 range=1.500000
# DESC:   
# BBOX: 0 0 0 0 0 0
+force_dest4     _explosion        thingflags=0x1 light=0.400000 timer=1.000000 sprite=destruct8.spr soundclass=exp_dest.snd creatething=+dest_cloud typeflags=0x53 damage=60.000000 damageclass=0x8 blasttime=0.600000 force=400.000000 maxlight=0.800000 range=2.000000
# DESC:   Force Destruction
# BBOX: 0 0 0 0 0 0
+force_dest_p1   _weapon           thingflags=0x20000401 light=0.400000 sprite=forcedstruct_150.spr cog=00_desttrail.cog vel=(0.000000/7.000000/0.000000) explode=+force_dest1 fleshhit=+force_dest1 damage=20.000000 typeflags=0xd
# DESC:   
# BBOX: 0 0 0 0 0 0
+force_dest_p2   +force_dest_p1    explode=+force_dest2 fleshhit=+force_dest2
# DESC:   
# BBOX: 0 0 0 0 0 0
+force_dest_p3   +force_dest_p1    explode=+force_dest3 fleshhit=+force_dest3
# DESC:   
# BBOX: 0 0 0 0 0 0
+force_dest_p4   +force_dest_p1    explode=+force_dest4 fleshhit=+force_dest4
# DESC:   
# BBOX: 0 0 0 0 0 0
+force_heal      none              orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=2.000000 angvel=(90.000000/90.000000/90.000000) typeflags=0x3f material=00teleport.mat range=0.300000 rate=128.000000 maxthrust=0.010000 elementsize=0.003000 count=128
# DESC:   
# BBOX: 0 0 0 0 0 0
+heavysmoke      +smoke            timer=1.600000 range=0.100000 rate=32.000000 count=64
# DESC:   
# BBOX: 0 0 0 0 0 0
+twinkle         none              orient=(0.000000/0.000000/0.000000) type=explosion thingflags=0x1 timer=0.500000 sprite=twinkle.spr typeflags=0x11
# DESC:   
# BBOX: 0 0 0 0 0 0
+sspks_wall      none              orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=0.100000 mass=0.050000 physflags=0x400201 vel=(0.000000/0.000000/0.150000) typeflags=0x2b material=00ramp1.mat range=0.030000 rate=4.000000 maxthrust=8.000000 elementsize=0.003000 count=48
# DESC:   
# BBOX: 0 0 0 0 0 0
+ssparks_wall    _explosion        thingflags=0x1 light=0.300000 timer=0.800000 soundclass=exp_saber_wall.snd creatething=+sspks_wall typeflags=0x112 blasttime=0.700000 maxlight=0.500000 flashrgb=(80/90/80)
# DESC:   
# BBOX: 0 0 0 0 0 0
+sspks_blood     none              orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=0.200000 mass=0.100000 physflags=0x400201 vel=(0.000000/0.000000/0.200000) typeflags=0x2b material=00ramp2.mat range=0.040000 rate=4.000000 maxthrust=9.000000 elementsize=0.005000 count=24
# DESC:   
# BBOX: 0 0 0 0 0 0
+ssparks_blood   _explosion        thingflags=0x1 light=0.100000 timer=0.800000 soundclass=exp_saber_blood.snd creatething=+sspks_blood typeflags=0x12 blasttime=0.700000 maxlight=0.300000
# DESC:   
# BBOX: 0 0 0 0 0 0
+sspks_saber     none              orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=0.300000 mass=0.050000 physflags=0x400200 vel=(0.000000/0.000000/0.100000) typeflags=0x27 material=00teleport.mat range=0.020000 rate=32.000000 maxthrust=8.000000 elementsize=0.003000 count=32
# DESC:   
# BBOX: 0 0 0 0 0 0
+ssparks_saber   _explosion        thingflags=0x1 light=0.800000 timer=0.800000 soundclass=exp_saber_saber.snd creatething=+sspks_saber typeflags=0x112 blasttime=0.700000 maxlight=1.000000 flashrgb=(150/160/150)
# DESC:   
# BBOX: 0 0 0 0 0 0
_droppowerup     _powerup          timer=30.000001 height=0.011000 physflags=0x41 typeflags=0x0
# DESC:   BackPack Pickup
# BBOX: -0.021089 -0.023005 -0.035347 0.016307 0.023005 0.035347
+backpack        _droppowerup      thingflags=0x400 model3d=bpck.3do cog=pow_backpack.cog height=0.036337 typeflags=0x4
# DESC:   Bryar Pistol Pickup
# BBOX: -0.008248 -0.030839 -0.011813 0.008248 0.030839 0.011813
bryarpistol      _powerup          thingflags=0x400 model3d=bryp.3do cog=pow_bryar.cog
# DESC:   1 Thermal Detonator Pickup
# BBOX: -0.017868 -0.016799 -0.017934 0.017868 0.016799 0.017934
singledetonator  _powerup          thingflags=0x400 model3d=detp_1.3do cog=pow_single_thermal.cog
# DESC:   CrossBow Pickup
# BBOX: -0.034888 -0.055377 0.000124 0.034343 0.059997 0.031925
crossbow         _powerup          thingflags=0x400 model3d=bowp.3do cog=pow_crossbow.cog
# DESC:   RepeaterGun Pickup
# BBOX: -0.00548 -0.051379 -0.021767 0.00548 0.039969 0.012035
repeatergun      _powerup          thingflags=0x400 model3d=rptp.3do cog=pow_repeater.cog
# DESC:   RailGun Pickup
# BBOX: -0.018484 -0.070714 -0.030025 0.019883 0.081826 0.013227
railgun          _powerup          thingflags=0x400 model3d=rldp.3do cog=pow_railgun.cog respawn=60.000000
# DESC:   1 Sequencer Mine Pickup
# BBOX: -0.022216 -0.005309 -0.024793 0.022216 0.005309 0.024793
singleseqcharge  _powerup          thingflags=0x400 model3d=seqp_1.3do cog=pow_single_sequencer.cog
# DESC:   Concussion Rifle Pickup
# BBOX: -0.030347 -0.072704 -0.024787 0.009211 0.072704 0.024341
concrifle        _powerup          thingflags=0x400 model3d=conp.3do cog=pow_concrifle.cog respawn=60.000000
# DESC:   Force Pulled Bryar Pistol Pickup
# BBOX: -0.008248 -0.030839 -0.011813 0.008248 0.030839 0.011813
+fpbryarpistol   bryarpistol       collide=0 timer=4.000000 typeflags=0x0 respawn=0.000000
# DESC:   Force Pulled St Rifle Pickup
# BBOX: -0.016822 -0.05557 -0.026043 0.007966 0.058 0.014355
+fpstrifle       strifle           collide=0 timer=4.000000 typeflags=0x0 respawn=0.000000
# DESC:   Force Pulled Thermal Detonator Pickup
# BBOX: -0.017868 -0.016799 -0.017934 0.017868 0.016799 0.017934
+fpdetonator     singledetonator   collide=0 timer=4.000000 typeflags=0x0 respawn=0.000000
# DESC:   Force Pulled CrossBow Pickup
# BBOX: -0.034888 -0.055377 0.000124 0.034343 0.059997 0.031925
+fpcrossbow      crossbow          collide=0 timer=4.000000 typeflags=0x0 respawn=0.000000
# DESC:   Force Pulled Repeater Gun Pickup
# BBOX: -0.00548 -0.051379 -0.021767 0.00548 0.039969 0.012035
+fprepeatergun   repeatergun       collide=0 timer=4.000000 typeflags=0x0 respawn=0.000000
# DESC:   Force Pulled Rail Detonator Gun Pickup
# BBOX: -0.018484 -0.070714 -0.030025 0.019883 0.081826 0.013227
+fprailgun       railgun           collide=0 timer=4.000000 typeflags=0x0 respawn=0.000000
# DESC:   Force Pulled Single Sequencer Mine Pickup
# BBOX: -0.022216 -0.005309 -0.024793 0.022216 0.005309 0.024793
+fpseqcharge     singleseqcharge   collide=0 timer=4.000000 typeflags=0x0 respawn=0.000000
# DESC:   Force Pulled Concussion Rifle Pickup
# BBOX: -0.030347 -0.072704 -0.024787 0.009211 0.072704 0.024341
+fpconcrifle     concrifle         collide=0 timer=4.000000 typeflags=0x0 respawn=0.000000
# DESC:   
# BBOX: 0 0 0 0 0 0
+telesparks      none              orient=(0.000000/0.000000/0.000000) type=particle timer=0.120000 typeflags=0x3f material=00teleport.mat range=0.030000 rate=256.000000 maxthrust=80.000000 elementsize=0.002000 count=256
# DESC:   
# BBOX: 0 0 0 0 0 0
bubble           none              orient=(0.000000/0.000000/0.000000) type=cog move=physics thingflags=0x10000000 timer=3.000000 sprite=bubble.spr mass=0.050000 physflags=0x200 vel=(0.000000/0.000000/0.300000)
# DESC:   
# BBOX: 0 0 0 0 0 0
bubble2          bubble            sprite=bubble2.spr vel=(0.000000/0.000000/0.450000)
# DESC:   
# BBOX: 0 0 0 0 0 0
bubble3          bubble            sprite=bubble3.spr vel=(0.000000/0.000000/0.600000)
# DESC:   
# BBOX: 0 0 0 0 0 0
+watersplash     none              orient=(0.000000/0.000000/0.000000) type=explosion timer=0.500000 sprite=splash.spr typeflags=0x1 damageclass=0x4
# DESC:   
# BBOX: 0 0 0 0 0 0
+watersplash2    +watersplash      sprite=splooshx.spr
# DESC:   Stormtroop Rifle Dropped
# BBOX: -0.016822 -0.05557 -0.026043 0.007966 0.058 0.014355
+dstrifle        _droppowerup      thingflags=0x400 model3d=strp.3do cog=pow_dstrifle.cog height=0.035177
# DESC:   
# BBOX: -0.012585 -0.050304 -0.010582 0.022579 0.027638 0.007152
_limb            none              orient=(0.000000/0.000000/0.000000) type=corpse collide=1 move=physics thingflags=0x400 timer=30.000001 model3d=stlimb.3do size=0.102337 movesize=0.000001 cog=00_bloodtrail.cog surfdrag=1.000000 airdrag=4.000000 mass=15.000000 physflags=0x44261 buoyancy=0.500000
# DESC:   
# BBOX: -0.024586 -0.02755 -0.015015 0.028804 0.052615 0.015553
+rylimb          _limb             model3d=rylimb.3do size=0.050000 movesize=0.010000
# DESC:   
# BBOX: -0.024586 -0.02755 -0.015015 0.028804 0.052615 0.015553
+rhlimb          _limb             model3d=rhlimb.3do size=0.050000 movesize=0.010000
# DESC:   
# BBOX: -0.026804 -0.036274 -0.074092 0.026804 0.036274 0.074092
shrapnel_5       _debris           model3d=shrp_5.3do size=0.084000 movesize=0.084000
# DESC:   
# BBOX: 0 0 0 0 0 0
+xtank1_exp      _explosion        thingflags=0x1 light=0.200000 timer=0.800000 sprite=detx.spr soundclass=exp_probe.snd typeflags=0x17 damage=90.000000 blasttime=0.700000 force=100.000000 maxlight=0.800000 range=0.450000 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_5
# DESC:   
# BBOX: 0 0 0 0 0 0
+xtank4_exp      +xtank1_exp       sprite=tiex.spr soundclass=exp_tie.snd damage=250.000000 force=300.000000 range=0.700000 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_5
# DESC:   
# BBOX: 0 0 0 0 0 0
+crane_exp       +xtank4_exp       damage=50.000000 force=75.000000 range=0.500000 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_5
# DESC:   
# BBOX: 0 0 0 0 0 0
+sentry_exp      +grenade_exp      damage=5.000000 force=50.000000
# DESC:   
# BBOX: 0 0 0 0 0 0
+crate_exp       +sentry_exp       damage=30.000000 force=75.000000 range=0.200000
# DESC:   
# BBOX: -0.027845 -0.025398 -0.02252 0.027845 0.025398 0.024913
+dhealthpack     _droppowerup      thingflags=0x408 model3d=hepk.3do cog=pow_health.cog height=0.023701
# DESC:   Bacta Tank Dropped 
# BBOX: -0.015415 -0.017797 -0.028888 0.015415 0.017797 0.028888
+dbactatank      _droppowerup      thingflags=0x400 model3d=bact.3do cog=pow_bacta.cog height=0.028885
# DESC:   Shield Dropped
# BBOX: -0.040113 -0.035949 -0.021618 0.040113 0.035949 0.021618
+dshield         _droppowerup      thingflags=0x400 model3d=shld.3do cog=pow_shields.cog height=0.021618
# DESC:   Battery Dropped 
# BBOX: -0.006993 -0.00625 -0.021875 0.006993 0.00625 0.021875
+dbattery        _droppowerup      thingflags=0x408 model3d=battery.3do cog=pow_battery.cog height=0.022909
# DESC:   EnergyCell Dropped  (pistol,rifle)
# BBOX: -0.008536 -0.014067 -0.022418 0.008536 0.014068 0.022418
+denergycell     _droppowerup      thingflags=0x400 model3d=ecel.3do cog=pow_energy.cog height=0.022435
# DESC:   PowerCell Dropped  (repeater,concusion,bowcaster)
# BBOX: -0.009653 -0.02399 -0.017867 0.009653 0.02399 0.017867
+dpowercell      _droppowerup      thingflags=0x400 model3d=pcel.3do cog=pow_power.cog height=0.017868
# DESC:   Smuggler BackPack Dropped
# BBOX: -0.021089 -0.023005 -0.035347 0.016307 0.023005 0.035347
+dsmbp           _droppowerup      thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk.cog height=0.036337
# DESC:   
# BBOX: 0 0 0 0 0 0
+rycw_punch      +ry_punch         damage=5.000000
# DESC:   
# BBOX: -0.046863 -0.025046 -0.131246 0.045997 0.027318 0.073257
reeyeescw        reeyeesfist       weapon=+rycw_punch typeflags=0x1020001
# DESC:   
# BBOX: 0 0 0 0 0 0
+gonk_exp        +grenade_exp      damage=20.000000
# DESC:   Greedo Pistol Dropped
# BBOX: -0.005081 -0.030803 -0.01143 0.005081 0.030803 0.01143
+dgreedopistol   _droppowerup      thingflags=0x400 model3d=ggun.3do cog=pow_bryar.cog height=0.011450
# DESC:   
# BBOX: 0 0 0 0 0 0
+scrate_exp      +crate_exp        damage=10.000000 force=25.000000 range=0.100000
# DESC:   
# BBOX: 0 0 0 0 0 0
+r2r5_exp        +grenade_exp      damage=10.000000 force=50.000000
# DESC:   
# BBOX: 0 0 0 0 0 0
+sparks          none              orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=1.000000 mass=1.000000 physflags=0x201 typeflags=0x3d material=embers.mat range=0.030000 rate=32.000000 maxthrust=20.000000 elementsize=0.010000 count=32
# DESC:   
# BBOX: 0 0 0 0 0 0
+limb_blood      none              orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=0.250000 physflags=0x201 vel=(0.000000/0.000000/0.200000) typeflags=0x27 material=00gsmoke.mat range=0.020000 rate=48.000000 maxthrust=3.000000 elementsize=0.005000 count=16
# DESC:   Enemy Thermal Detonator
# BBOX: 0 0 0 0 0 0
+egrenade_exp    +grenade_exp      damage=50.000000
# DESC:   
# BBOX: -0.012516 -0.012516 -0.012524 0.012516 0.012516 0.014655
+actor_grenade   +grenade1         surfdrag=0.000000 airdrag=0.000000 vel=(0.000000/2.000000/0.000000) explode=+egrenade_exp fleshhit=+egrenade_exp
# DESC:   ReeYee Brown w Thermal Detonators
# BBOX: -0.046863 -0.025046 -0.131246 0.045997 0.027318 0.073257
reeyees          _humanactor       thingflags=0x20000400 model3d=ry.3do size=0.072900 movesize=0.072900 puppet=ry.pup soundclass=ry.snd cog=actor_ry.cog weapon=+actor_grenade weapon2=+ry_punch health=50.00 maxhealth=50.00 maxthrust=0.90 maxrotthrust=70.00 jumpspeed=1.20 typeflags=0x20001 fireoffset=(0.046500/0.070000/0.032700) aiclass=rytherdefault.ai
# DESC:   
# BBOX: -0.015083 -0.063181 -0.010398 0.052176 0.070684 0.0122
disc             _powerup          thingflags=0x400 model3d=disc.3do cog=pow_datadisk.cog
# DESC:   
# BBOX: -0.544736 -0.786304 -0.229997 0.544736 0.786304 0.229997
flyingcrow       _zwalkstruct      model3d=crow.3do size=0.853084 movesize=0.853084 puppet=cr.pup
# DESC:   ReeYee Black w Thermal Detonators
# BBOX: -0.046863 -0.025046 -0.131246 0.045997 0.027318 0.073257
reeyeesbh        reeyees           model3d=rh.3do health=60.00 maxhealth=60.00 maxrotthrust=80.00
# DESC:   
# BBOX: -0.012516 -0.012516 -0.012524 0.012516 0.012516 0.014655
+actor_grenade2  +grenade2         vel=(0.000000/2.000000/0.000000) explode=+egrenade_exp fleshhit=+egrenade_exp
# DESC:   ReeYee Black w Bouncing Thermal Detonators
# BBOX: -0.046863 -0.025046 -0.131246 0.045997 0.027318 0.073257
reeyeesbhs       reeyeesbh         weapon=+actor_grenade2
# DESC:   ReeYee Brown w Bouncing Thermal Detonators
# BBOX: -0.046863 -0.025046 -0.131246 0.045997 0.027318 0.073257
reeyeess         reeyees           weapon=+actor_grenade2
# DESC:   Door 6x6 Single Drk Grey w Ribs 
# BBOX: -0.3 -0.0125 -0.3 0.3 0.0125 0.3
6x6door          _structure        model3d=d6x6.3do size=0.474448 movesize=0.474448 soundclass=lg_door.snd
# DESC:   
# BBOX: -0.137499 -0.154503 -0.1497 0.137499 0.154503 0.150618
roofvent1        _walkstruct       model3d=01rfd1.3do size=0.305318 movesize=0.305318
# DESC:   Gammorean Guard
# BBOX: -0.064855 -0.030184 -0.137291 0.064857 0.181468 0.079798
gammorean        _humanactor       thingflags=0x20000400 model3d=gm.3do size=0.078780 movesize=0.078780 puppet=gm.pup soundclass=gm.snd cog=actor_gg.cog mass=200.000000 weapon=+gamaxe weapon2=+gamaxe health=120.00 maxhealth=120.00 maxrotthrust=80.00 typeflags=0x20001 aiclass=ggdefault.ai
# DESC:   
# BBOX: -0.1 -0.5 -0.012306 0.1 0.5 0.012306
2x10catwalk      _walkstruct       model3d=c2x10.3do size=0.510050 movesize=0.510050
# DESC:   
# BBOX: -0.200159 -0.024886 -0.057805 0.200159 0.024886 0.057805
console5         _walkstruct       model3d=con5.3do size=0.209820 movesize=0.209820
# DESC:   Red Key Pickup
# BBOX: -0.010023 -0.016964 -0.033841 0.010023 0.016964 0.033841
keyred           _powerup          thingflags=0x400 model3d=r-key.3do cog=pow_keyred.cog
# DESC:   
# BBOX: -0.050201 -0.999639 -0.073612 0.050201 0.999639 0.073612
boom             _walkstruct       model3d=boom.3do size=1.003602 movesize=1.003602 soundclass=02boom.snd
# DESC:   
# BBOX: -0.150124 -0.150615 -0.150124 0.149876 0.149385 0.149872
crate4_5         _walkstruct       model3d=crt4-5.3do size=0.260306 movesize=0.260306
# DESC:   
# BBOX: -0.2 -0.198942 -0.2 0.2 0.201059 0.2
crate4_6         _walkstruct       model3d=crt4-6.3do size=0.347022 movesize=0.347022
# DESC:   Door 8x8 Single Drk Grey w 2 Large Indents
# BBOX: -0.4 -0.050015 -0.398 0.4 0.050015 0.4
8x8door          _structure        model3d=d8x8.3do size=0.617892 movesize=0.617892 soundclass=lg_door.snd
# DESC:   C2PO Droid
# BBOX: -0.030754 -0.015261 -0.124016 0.030821 0.02207 0.067788
seetoopio        _droidactor       thingflags=0x20000400 model3d=c2.3do size=0.067800 movesize=0.067800 puppet=c3.pup soundclass=c3po.snd cog=actor_c3po.cog mass=120.000000 maxvel=0.400000 health=50.00 maxhealth=50.00 maxthrust=0.30 maxrotthrust=50.00 typeflags=0x180140 aiclass=drdefault.ai
# DESC:   
# BBOX: -1.850091 -0.300148 -0.107601 1.850091 0.300148 0.107601
lift             _walkstruct       model3d=lift.3do size=1.877332 movesize=1.877332 soundclass=funicular.snd
# DESC:   
# BBOX: -0.063294 -0.054836 -0.040528 0.063294 0.054836 0.040528
table            _decor            model3d=01tabl.3do size=0.065000 movesize=0.065000
# DESC:   Invinsible Pickup
# BBOX: -0.021293 -0.02235 -0.028598 0.01838 0.02235 0.028374
supershield      _powerup          thingflags=0x400 model3d=vestl.3do cog=pow_supershield.cog respawn=60.000000
# DESC:   Battery Pickup 
# BBOX: -0.006993 -0.00625 -0.021875 0.006993 0.00625 0.021875
battery          _powerup          thingflags=0x408 model3d=battery.3do cog=pow_battery.cog
# DESC:   BackPack Pickup
# BBOX: -0.021089 -0.023005 -0.035347 0.016307 0.023005 0.035347
rbackpack        _powerup          model3d=bpck.3do
# DESC:   Revive Pickup
# BBOX: -0.021089 -0.023005 -0.035347 0.016307 0.023005 0.035347
revivepack       _powerup          thingflags=0x400 model3d=packrev.3do cog=pow_revive.cog respawn=60.000000
# DESC:   Woman
# BBOX: -0.021527 -0.011397 -0.102055 0.021527 0.014719 0.057565
woman2           _civilian         thingflags=0x20000400 model3d=w2.3do size=0.056000 movesize=0.056000 puppet=w2.pup soundclass=wm1.snd cog=actor_pf.cog maxvel=0.200000 health=50.00 maxhealth=50.00 maxthrust=0.50 maxrotthrust=60.00 aiclass=peddefault.ai
# DESC:   Woman Narshadda
# BBOX: -0.021527 -0.011397 -0.102055 0.021527 0.014719 0.057565
nwoman2          woman2            cog=actor_fn.cog aiclass=pednarsh.ai
# DESC:   Woman greedo
# BBOX: -0.021527 -0.011397 -0.102055 0.021527 0.014719 0.057565
gwoman2          nwoman2           cog=actor_gd.cog aiclass=peddefault.ai
# DESC:   
# BBOX: -0.2 -0.2 -0.2 0.2 0.2 0.2
02crate1         _walkstruct       model3d=02crate1.3do size=0.346410 movesize=0.346410
# DESC:   
# BBOX: -0.2 -0.2 -0.2 0.2 0.2 0.2
02crate2         _walkstruct       model3d=02crate2.3do size=0.346410 movesize=0.346410
# DESC:   
# BBOX: -0.2 -0.198942 -0.2 0.2 0.201059 0.2
02crate3         _walkstruct       model3d=02crate3.3do size=0.397022 movesize=0.397022
# DESC:   
# BBOX: -0.597832 -0.597465 -0.299701 0.597179 0.59758 0.299701
00zcrate         _zwalkstruct      model3d=00crte.3do size=0.830224 movesize=0.830224 soundclass=funicular.snd
# DESC:   Thermal Detonators Dropped
# BBOX: -0.020824 -0.043225 0.000892 0.011798 0.043225 0.024826
+ddetonator      _droppowerup      thingflags=0x400 model3d=detp.3do cog=pow_thermal.cog
# DESC:   1 Thermal Detonator Dropped
# BBOX: -0.017868 -0.016799 -0.017934 0.017868 0.016799 0.017934
+dsingle         _droppowerup      thingflags=0x400 model3d=detp_1.3do cog=pow_single_thermal.cog height=0.021909
# DESC:   
# BBOX: -0.012516 -0.012516 -0.012524 0.012516 0.012516 0.014655
+spec_grenade    +grenade2         vel=(0.000000/0.000000/0.000000)
# DESC:   
# BBOX: -0.016813 -0.025219 -0.025219 0.016813 0.025219 0.025219
bottle02         _structure        model3d=bottle2.3do size=0.089429 movesize=0.089429
# DESC:   
# BBOX: -0.512471 -0.025172 -0.165 0.512502 0.025172 0.163
window3a         _zwalkstruct      thingflags=0x40 model3d=wn3a.3do size=0.588384 movesize=0.588384
# DESC:   
# BBOX: -0.037244 -0.032884 -0.063574 0.037244 0.032884 0.063574
chair04          _walkstruct       thingflags=0x48 model3d=chr4.3do size=0.124181 movesize=0.124181
# DESC:   
# BBOX: -0.1 -0.0125 -0.15 0.1 0.0125 0.15
03_2x3door       _structure        thingflags=0x808 model3d=03d2x3.3do size=0.180710 movesize=0.180710 soundclass=03wood_door.snd
# DESC:   
# BBOX: 0 -0.0125 -0.15 0.2 0.0125 0.15
4x3door_b        _structure        thingflags=0x808 model3d=03d4x3_b.3do size=0.250312 movesize=0.250312 soundclass=03wood_door.snd
# DESC:   
# BBOX: -0.2 -0.0125 -0.15 0 0.0125 0.15
4x3door_a        _structure        thingflags=0x808 model3d=03d4x3_a.3do size=0.250312 movesize=0.250312 soundclass=03wood_door.snd
# DESC:   
# BBOX: -0.15 -0.0125 -0.15 0.15 0.0125 0.15
03_3x3door       _structure        thingflags=0x808 model3d=03d3x3.3do size=0.212500 movesize=0.212500 soundclass=03wood_door.snd
# DESC:   Enemy Crossbow 
# BBOX: -0.020392 -0.135722 -0.016856 0.021073 0.013215 0.015722
+ebow            +crossbowbolt     vel=(0.000000/4.000000/0.000000) damage=20.000000 mindamage=10.000000
# DESC:   
# BBOX: -0.020392 -0.135722 -0.016856 0.021073 0.013215 0.015722
+ebowplus5       +ebow             soundclass=none vel=(-1.000000/4.000000/0.000000)
# DESC:   
# BBOX: -0.020392 -0.135722 -0.016856 0.021073 0.013215 0.015722
+ebowplus4       +ebow             soundclass=none creatething=+ebowplus5 vel=(1.000000/4.000000/0.000000)
# DESC:   
# BBOX: -0.020392 -0.135722 -0.016856 0.021073 0.013215 0.015722
+ebowplus3       +ebow             soundclass=none creatething=+ebowplus4 vel=(-0.500000/4.000000/0.000000)
# DESC:   
# BBOX: -0.020392 -0.135722 -0.016856 0.021073 0.013215 0.015722
+ebowplus2       +ebow             soundclass=none creatething=+ebowplus3 vel=(0.500000/4.000000/0.000000)
# DESC:   
# BBOX: -0.020392 -0.135722 -0.016856 0.021073 0.013215 0.015722
+ebowplus        +ebow             creatething=+ebowplus2
# DESC:   
# BBOX: -0.020392 -0.135722 -0.016856 0.021073 0.013215 0.015722
+ebowplus3m      +ebow             soundclass=none vel=(-0.500000/4.000000/0.000000)
# DESC:   
# BBOX: -0.020392 -0.135722 -0.016856 0.021073 0.013215 0.015722
+ebowplus2m      +ebowplus2        creatething=+ebowplus3m
# DESC:   
# BBOX: -0.020392 -0.135722 -0.016856 0.021073 0.013215 0.015722
+ebowplusm       +ebowplus         creatething=+ebowplus2m
# DESC:   Tuskan Raider Brown w Bowcaster
# BBOX: -0.037806 -0.014831 -0.120834 0.054054 0.062457 0.065442
tuskan           _humanactor       thingflags=0x20000400 model3d=tk.3do size=0.064900 movesize=0.064900 puppet=tk.pup soundclass=tk.snd cog=actor_tk.cog mass=120.000000 maxvel=0.200000 weapon=+ebow weapon2=+ebowplusm health=50.00 maxhealth=50.00 maxthrust=0.90 maxrotthrust=70.00 jumpspeed=1.20 typeflags=0x20000 fireoffset=(0.006000/0.061000/0.027000) aiclass=tuproddefault.ai
# DESC:   
# BBOX: -0.1 -0.0125 -0.15 0.1 0.0125 0.15
03_2x3door2      _structure        thingflags=0x808 model3d=03d2x3_2.3do size=0.180710 movesize=0.180710 soundclass=03wood_door.snd
# DESC:   
# BBOX: -0.044472 -0.026134 -0.026481 0.044472 5.6E-5 0.041013
lightfix00       _ghoststructure   thingflags=0x9 model3d=lfix.3do size=0.100531 movesize=0.100531
# DESC:   Tuskan Raider Black w Bowcaster
# BBOX: -0.037806 -0.014831 -0.120834 0.054054 0.062457 0.065442
tuskanbh         tuskan            model3d=th.3do weapon2=+ebowplus health=60.00 maxhealth=60.00 maxrotthrust=80.00 aiclass=tubproddefault.ai
# DESC:   Tuskan Raider Brown w St Rifle
# BBOX: -0.037806 -0.014831 -0.120834 0.037548 0.016412 0.065442
tuskangun        _humanactor       thingflags=0x20000400 model3d=tkstr.3do size=0.064900 movesize=0.064900 puppet=tkstr.pup soundclass=tkstr.snd cog=actor_tb.cog mass=120.000000 maxvel=0.200000 weapon=+elaser health=50.00 maxhealth=50.00 maxthrust=0.90 maxrotthrust=70.00 jumpspeed=1.20 fireoffset=(-0.041600/0.068200/0.022500) aiclass=tudefault.ai
# DESC:   Tuskan Raider Black w St Rifle
# BBOX: -0.037806 -0.014831 -0.120834 0.037548 0.016412 0.065442
tuskanbhgun      tuskangun         model3d=thstr.3do health=60.00 maxhealth=60.00 maxrotthrust=80.00 aiclass=tubdefault.ai
# DESC:   
# BBOX: 0 0 0 0 0 0
+mailocsting     +gamaxe           damage=15.000000 force=25.000000
# DESC:   Mynok
# BBOX: -0.080669 -0.095318 -0.091964 0.082473 0.08505 0.212669
mynok            _humanactor       thingflags=0x20000400 model3d=my.3do size=0.100000 movesize=0.100000 puppet=my.pup soundclass=mynoc.snd cog=actor_mailoc.cog mass=40.000000 physflags=0x6a06 weapon=+mailocsting maxthrust=0.90 maxrotthrust=70.00 typeflags=0x2020000 aiclass=maidefault.ai
# DESC:   
# BBOX: -0.163442 -0.025172 -0.165 0.164415 0.025172 0.163
window3b         _zwalkstruct      thingflags=0x40 model3d=wn3b.3do size=0.282198 movesize=0.282198
# DESC:   
# BBOX: -0.044472 -0.026134 -0.026481 0.044472 5.6E-5 0.041013
lightfixture     _ghoststructure   thingflags=0x9 light=0.400000 model3d=lfix.3do size=0.100531 movesize=0.100531
# DESC:   PowerCell Pickup  (repeater,concusion,bowcaster)
# BBOX: -0.009653 -0.02399 -0.017867 0.009653 0.02399 0.017867
powercell        _powerup          thingflags=0x400 model3d=pcel.3do cog=pow_power.cog
# DESC:   
# BBOX: -0.016298 -0.015506 -0.024816 0.016298 0.015506 0.024816
bottle03         _structure        model3d=bottle3.3do size=0.080942 movesize=0.080942
# DESC:   Dynamic Light 0.7
# BBOX: 0 0 0 0 0 0
light0.7         _ghostdecor       thingflags=0x1 light=0.700000
# DESC:   
# BBOX: -0.032036 -0.054681 -0.024816 0.032036 0.054682 0.024816
bottles03        _structure        collide=1 model3d=bottles3.3do size=0.050000 movesize=0.050000
# DESC:   
# BBOX: -0.025015 -0.324814 -0.025014 0.025015 0.324814 0.025014
beam02           _zwalkstruct      thingflags=0x40 model3d=beam02.3do size=0.376735 movesize=0.376735
# DESC:   
# BBOX: -0.075889 -0.047877 -0.051709 0.075889 0.047877 0.051709
table4           _walkstruct       thingflags=0x48 model3d=tbl4.3do size=0.153561 movesize=0.153561
# DESC:   
# BBOX: -0.050387 -0.043746 -0.161575 0.050387 0.043746 0.161575
vaporator        _structure        model3d=vap1.3do size=0.219250 movesize=0.219250
# DESC:   
# BBOX: -0.899877 -0.063413 -0.499966 0.899877 0.063413 0.499965
trelus           _walkstruct       thingflags=0x48 model3d=trel.3do size=1.031390 movesize=1.031390
# DESC:   
# BBOX: -0.125 -0.200163 -0.049948 0.125 0.200163 0.072163
bed              _walkstruct       thingflags=0x800048 model3d=bed.3do size=0.291216 movesize=0.291216
# DESC:   Door 1/2 4x3  Gold Color Vertical Split 
# BBOX: -0.1 -0.0125 -0.15 0.1 0.0125 0.15
4x3door_2a       _structure        model3d=d4x3_2a.3do size=0.230710 movesize=0.230710 soundclass=med_door.snd
# DESC:   Door 1/2 4x3  Gold Color Vertical Split 
# BBOX: -0.1 -0.0125 -0.15 0.1 0.0125 0.15
4x3door_2b       _structure        model3d=d4x3_2b.3do size=0.230710 movesize=0.230710
# DESC:   
# BBOX: -0.2 -0.1 -0.050028 0.2 0.1 0.050359
table2           _walkstruct       thingflags=0x48 model3d=tbl2.3do size=0.279207 movesize=0.279207
# DESC:   
# BBOX: -0.049067 -0.02563 -0.000348 0.049067 0.02563 0.153568
cabinet1         _walkstruct       thingflags=0x48 model3d=cab_1.3do size=0.213241 movesize=0.213241
# DESC:   
# BBOX: -0.035377 -0.019473 0.000188 0.035377 0.019473 0.12627
cabinet2         _walkstruct       thingflags=0x48 model3d=cab_2.3do size=0.182570 movesize=0.182570
# DESC:   
# BBOX: -0.098661 -0.114859 -0.001485 0.095745 0.114859 0.209053
chair2           _walkstruct       thingflags=0x800048 model3d=chair2.3do size=0.308127 movesize=0.308127
# DESC:   
# BBOX: -0.042032 -0.049632 0.000337 0.042032 0.049632 0.195296
chair3           _walkstruct       thingflags=0x48 model3d=chair3.3do size=0.254420 movesize=0.254420
# DESC:   
# BBOX: -0.074978 -0.074978 -0.069146 0.074978 0.074978 0.069146
woodcrate        _walkstruct       thingflags=0x48 model3d=wcrt.3do size=0.176588 movesize=0.176588
# DESC:   
# BBOX: -0.8 -0.049136 -0.183134 0.8 0.05599 0.183134
waterpumpblock   _zwalkstruct      model3d=wpmpbloc.3do size=0.822601 movesize=0.822601
# DESC:   Blue Wrench Key Pickup
# BBOX: -0.032373 -0.017479 9.7E-5 0.031819 0.017479 0.079704
bluewrenchkey    _powerup          thingflags=0x400 model3d=keyblu.3do cog=pow_wrenchblue.cog
# DESC:   Yellow Wrench Key Pickup
# BBOX: -0.032373 -0.017479 9.7E-5 0.031819 0.017479 0.079704
yellowwrenchkey  _powerup          thingflags=0x400 model3d=keyyel.3do cog=pow_wrenchyellow.cog
# DESC:   Door 16x6.5 Single Drk Grey  
# BBOX: -0.8 -0.016042 -0.325 0.8 0.016042 0.325
garagedoor       _structure        model3d=grgd.3do size=0.913645 movesize=0.913645 soundclass=lg_door.snd
# DESC:   
# BBOX: -0.04272 -0.049295 -0.065661 0.04272 0.049295 0.065661
woodcrate2       _walkstruct       thingflags=0x448 model3d=wcrt6-0.3do size=0.132106 movesize=0.132106 cog=actor_wc.cog
# DESC:   
# BBOX: -0.016813 -0.025219 -0.025219 0.016813 0.025219 0.025219
bottle01         _structure        model3d=bottle1.3do size=0.089429 movesize=0.089429
# DESC:   
# BBOX: -0.086593 -0.082033 -0.02768 0.086593 0.082033 0.02768
wholerock_2      _walkstruct       thingflags=0x48 model3d=rok_2.3do size=0.151886 movesize=0.151886
# DESC:   
# BBOX: -0.05 -0.05 -0.075 0.05 0.05 0.075
03blockbottom    _zwalkstruct      model3d=03blockb.3do size=0.153078 movesize=0.153078
# DESC:   
# BBOX: -0.15 -0.0125 -0.15 0.15 0.0125 0.15
03_3x3door_2     _zwalkstruct      thingflags=0x800 model3d=03d3x3_2.3do size=0.262500 movesize=0.262500 soundclass=03wood_door.snd
# DESC:   
# BBOX: -0.3 -0.075 -0.05 0.3 0.075 0.05
reclight         _structure        model3d=g_lite.3do size=0.363249 movesize=0.363249
# DESC:   Stormtoop BackPack Pickup
# BBOX: -0.018131 -0.009419 -0.000113 0.037525 0.027081 0.049637
stbp             _powerup          thingflags=0x400 model3d=stpack.3do cog=pow_stbkpk.cog
# DESC:   
# BBOX: -0.070489 -0.081337 -0.065817 0.070489 0.081337 0.065817
woodcrate3       _walkstruct       thingflags=0x448 model3d=wcrt6-1.3do size=0.154631 movesize=0.154631 cog=actor_wc.cog
# DESC:   
# BBOX: -0.15 -0.15 -0.15 0.15 0.15 0.15
woodcratebig     _walkstruct       thingflags=0x48 model3d=wcrt2.3do size=0.309808 movesize=0.309808
# DESC:   
# BBOX: -0.199721 -0.174982 -0.070859 0.199966 0.174982 0.029196
canopy           _zwalkstruct      thingflags=0x40 model3d=cany.3do size=0.325002 movesize=0.325002
# DESC:   
# BBOX: -0.016342 -0.015548 -0.018445 0.016342 0.015548 0.018445
bottle06         _structure        model3d=bottle6.3do size=0.076147 movesize=0.076147
# DESC:   
# BBOX: -0.032771 -0.046457 -0.018445 0.032771 0.046457 0.018445
bottles06        _structure        collide=1 model3d=bottles6.3do size=0.050000 movesize=0.050000
# DESC:   
# BBOX: -0.038893 -0.037552 -0.024816 0.039144 0.036124 0.024816
bottles04        _structure        collide=1 model3d=bottles4.3do size=0.050000 movesize=0.050000
# DESC:   
# BBOX: -0.061611 -0.054554 -0.025219 0.061611 0.054554 0.025219
bottles01        _structure        collide=1 model3d=bottles1.3do size=0.050000 movesize=0.050000
# DESC:   
# BBOX: -0.15 -0.15 -0.15 0.15 0.15 0.15
03woodcrate2     _zwalkstruct      thingflags=0x40 model3d=wcrt2.3do size=0.309808 movesize=0.309808
# DESC:   
# BBOX: -0.066759 -0.059659 -0.025219 0.066759 0.059659 0.025219
bottles02        _structure        collide=1 model3d=bottles2.3do size=0.050000 movesize=0.050000
# DESC:   
# BBOX: -0.05 -0.05 -0.000679 0.05 0.05 0.299321
03block          _zwalkstruct      model3d=03block.3do size=0.357560 movesize=0.357560
# DESC:   WeeGee Droid 
# BBOX: -0.072521 -0.040875 -0.055302 0.074842 0.129417 0.069428
weegee           _droidactor       thingflags=0x404 model3d=wg.3do size=0.100000 movesize=0.100000 puppet=wg.pup soundclass=wg.snd cog=actor_wg.cog mass=200.000000 maxvel=0.250000 health=5000.00 maxhealth=5000.00 maxthrust=1.00 maxrotthrust=60.00 typeflags=0x180140 aiclass=gonk.ai
# DESC:   
# BBOX: -0.05 -0.05 -0.075 0.05 0.05 0.075
03blocktop       _zwalkstruct      model3d=03blockt.3do size=0.153078 movesize=0.153078
# DESC:   CrossBow Dropped
# BBOX: -0.034888 -0.055377 0.000124 0.034343 0.059997 0.031925
+dcrossbow       _droppowerup      thingflags=0x400 model3d=bowp.3do cog=pow_dcrossbow.cog height=0.003435
# DESC:   
# BBOX: -0.042717 -0.015112 -0.011798 0.042717 0.015112 0.011798
+tklimb          _limb             model3d=tklimb.3do size=0.050000 movesize=0.010000
# DESC:   
# BBOX: -0.042717 -0.015112 -0.011798 0.042717 0.015112 0.011798
+thlimb          _limb             model3d=thlimb.3do size=0.050000 movesize=0.010000
# DESC:   
# BBOX: -0.037418 -0.063387 -0.017067 0.025236 0.03643 0.012634
+mylimb          _limb             model3d=mylimb.3do size=0.070000 movesize=0.010000
# DESC:   
# BBOX: 0 0 0 0 0 0
+wcrate_exp      +crate_exp        damage=20.000000 force=50.000000 range=0.150000
# DESC:   
# BBOX: 0 0 0 0 0 0
+xtank3_exp      +xtank1_exp       sprite=tiex.spr soundclass=exp_tie.snd damage=200.000000 force=200.000000 range=0.600000 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_5
# DESC:   
# BBOX: -0.034039 -0.029553 -0.032378 0.034039 0.029553 0.032378
vapdebris_1      _debris           model3d=v_deb1.3do size=0.040000 movesize=0.040000
# DESC:   
# BBOX: -0.00911 -0.009437 -0.061645 0.00911 0.009437 0.061645
vapdebris_2      _debris           model3d=v_deb2.3do size=0.060000 movesize=0.060000
# DESC:   
# BBOX: -0.031695 -0.036598 -0.014875 0.031695 0.036598 0.014875
vapdebris_3      _debris           model3d=v_deb3.3do size=0.030000 movesize=0.030000
# DESC:   
# BBOX: -0.016565 -0.043636 -0.052678 0.016565 0.043636 0.052678
vapdebris_4      _debris           model3d=v_deb4.3do size=0.050000 movesize=0.050000
# DESC:   
# BBOX: 0 0 0 0 0 0
+vaporator_exp   _explosion        thingflags=0x1 light=0.200000 timer=0.800000 sprite=seqx2.spr soundclass=exp_probe.snd typeflags=0x17 damage=40.000000 blasttime=0.700000 force=120.000000 maxlight=0.800000 range=0.450000 debris=vapdebris_1 debris=vapdebris_2 debris=vapdebris_3 debris=vapdebris_4
# DESC:   
# BBOX: 0 0 0 0 0 0
+03_exp          +raildet_exp      damage=25.000000 force=60.000000 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_4
# DESC:   
# BBOX: -0.314105 -0.627595 -0.074421 0.314105 0.627595 0.074421
trooptran        _zwalkstruct      model3d=ttrn.3do size=0.718795 movesize=0.718795
# DESC:   
# BBOX: -0.1 -0.748063 -0.424992 0.1 0.748063 0.424992
04beam2          _walkstruct       model3d=04beam2.3do size=0.916150 movesize=0.916150
# DESC:   
# BBOX: -0.1 -0.75 -0.1 0.1 0.75 0.1
04beam1          _walkstruct       model3d=04beam1.3do size=0.763217 movesize=0.763217
# DESC:   Door 4x4 Single Drk Grey
# BBOX: -0.2 -0.0125 -0.15 0.2 0.0125 0.25
4x4door          _walkstruct       model3d=d4x4.3do size=0.320400 movesize=0.320400 soundclass=med_door.snd
# DESC:   
# BBOX: -0.242583 -0.242583 0 0.242583 0.242583 0
04water          _ghostdecor       model3d=04water.3do size=0.301141 movesize=0.301141
# DESC:   
# BBOX: -0.1 -0.15 -0.0125 0.1 0.15 0.0125
04platform       _walkstruct       model3d=04p2x3.3do size=0.230710 movesize=0.230710 soundclass=sm_door.snd
# DESC:   
# BBOX: 0 0 0 0 0 0
+esequencer_exp  _explosion        thingflags=0x1 light=0.200000 timer=0.800000 sprite=seqx2.spr soundclass=exp_med.snd creatething=+firecloud2 typeflags=0x17 damage=75.000000 blasttime=0.700000 force=300.000000 maxlight=0.800000 range=0.600000 debris=shrapnel2_1 debris=shrapnel2_2 debris=shrapnel2_3 debris=shrapnel2_4
# DESC:   Sequencer Mine Live
# BBOX: -0.022216 -0.023897 -0.005309 0.022216 0.023897 0.005309
sequencermine    +grenade2         timer=0.000000 model3d=seq0.3do size=0.300000 movesize=0.010000 soundclass=seq.snd surfdrag=5.000000 airdrag=1.000000 physflags=0x29c vel=(0.000000/0.000000/0.000000) angvel=(0.000000/0.000000/0.000000) buoyancy=0.500000 explode=+esequencer_exp typeflags=0x1380
# DESC:   Expolding Tank 1 Meter Tall
# BBOX: -0.040024 -0.034696 -0.049915 0.040024 0.034696 0.049915
xtank1a          _walkstruct       thingflags=0x400448 model3d=xtank1a.3do size=0.063985 movesize=0.063985 cog=xtank1.cog
# DESC:   
# BBOX: 0 0 0 0 0 0
_swimactor       _humanactor       physflags=0x4a0f typeflags=0x20040
# DESC:   
# BBOX: 0 0 0 0 0 0
+cycbite         +gamaxe           damage=25.000000 range=0.500000
# DESC:   Octotpus
# BBOX: -0.112594 -0.066536 -0.714146 0.146652 0.121299 0.144462
octopus          _swimactor        model3d=oc.3do size=0.178000 movesize=0.178000 puppet=oc.pup mass=300.000000 physflags=0x404a0f maxvel=0.600000 weapon=+cycbite health=100.00 maxhealth=100.00 maxthrust=0.40 maxrotthrust=60.00 typeflags=0x20140 aiclass=wcdefault.ai
# DESC:   
# BBOX: 0 0 0 0 0 0
_flyactor        _actor            airdrag=2.000000 physflags=0x7606 maxvel=0.800000 maxthrust=0.50
# DESC:   
# BBOX: -0.005771 -0.015021 -0.00466 0.005771 0.003935 0.00466
+remotelaser     +repeaterball     damage=2.000000 mindamage=1.000000
# DESC:   Remote Droid
# BBOX: -0.01477 -0.014666 -0.01477 0.01477 0.014666 0.01477
remote           _flyactor         thingflags=0x20000400 model3d=remo.3do size=0.025000 movesize=0.025000 soundclass=re.snd cog=actor_re.cog mass=15.000000 physflags=0x6a06 maxvel=0.600000 weapon=+remotelaser health=30.00 maxthrust=0.90 maxrotthrust=150.00 typeflags=0x2000120 fireoffset=(0.015000/0.020000/0.000000) explode=+remote_exp aiclass=redefault.ai
# DESC:   
# BBOX: -0.490393 -0.196424 -0.05 0.490393 0.196424 0.05
plathalf         _walkstruct       thingflags=0x48 model3d=plathalf.3do size=0.580629 movesize=0.580629
# DESC:   
# BBOX: -0.15 -0.0125 -0.1 0.15 0.0125 0.1
06door3x2        _walkstruct       thingflags=0x848 model3d=06d3x2.3do size=0.230710 movesize=0.230710 soundclass=03wood_door.snd
# DESC:   
# BBOX: -0.024829 -1.295061 -1.126603 0.307452 1.295061 0.996689
wall_walltrans.  _walkstruct       model3d=walt_a.3do size=1.766691 movesize=1.766691
# DESC:   
# BBOX: -0.2875 -0.0375 -0.05 0.2875 0.0375 0.05
06counter2       _walkstruct       thingflags=0x48 model3d=06count2.3do size=0.344215 movesize=0.344215
# DESC:   
# BBOX: -0.2 -0.0375 -0.05 0.2 0.0375 0.05
06counter        _walkstruct       thingflags=0x48 model3d=06countr.3do size=0.259538 movesize=0.259538
# DESC:   
# BBOX: -0.027321 -0.032261 0.000219 0.027321 0.032261 0.126942
minichair3       _walkstruct       thingflags=0x48 model3d=chair3m.3do size=0.182873 movesize=0.182873
# DESC:   Man No Voice
# BBOX: -0.035537 -0.014648 -0.128737 0.035536 0.025457 0.065502
mman1            man1              cog=actor_mu.cog
# DESC:   
# BBOX: -0.132528 -0.06827 -0.073365 0.132528 0.06827 0.073365
sofa             _zwalkstruct      thingflags=0x800048 model3d=sofa.3do size=0.216153 movesize=0.216153
# DESC:   
# BBOX: -0.165589 -0.046318 -0.208452 0.054283 0.051379 0.208452
06streetlight    _zwalkstruct      model3d=stlt.3do size=0.316625 movesize=0.316625
# DESC:   Flying Guard Droid
# BBOX: -0.037102 -0.076502 -0.041518 0.037102 0.046066 0.03417
robotsentry      _flyactor         thingflags=0x20000400 model3d=rs.3do size=0.055000 movesize=0.055000 soundclass=rs.snd cog=actor_sd.cog mass=30.000000 physflags=0x6a06 maxvel=0.600000 weapon=+bryarbolt health=60.00 maxhealth=60.00 maxthrust=0.80 maxrotthrust=80.00 typeflags=0x2000120 fireoffset=(-0.007300/0.039300/0.000000) explode=+sentry_exp aiclass=sddefault.ai
# DESC:   
# BBOX: -0.028121 -0.020533 -0.054867 0.028121 0.020533 0.055636
chair07          _decor            thingflags=0x48 model3d=chair07.3do size=0.045000 movesize=0.045000
# DESC:   
# BBOX: -0.065678 -0.075809 -0.044765 0.065678 0.075809 0.044765
gametable        _walkstruct       model3d=game.3do size=0.138039 movesize=0.138039
# DESC:   Woman No Voice
# BBOX: -0.025123 -0.01744 -0.127094 0.025123 0.015562 0.056718
mwoman1          woman1            cog=actor_mu.cog
# DESC:   
# BBOX: -0.021527 -0.011397 -0.102055 0.021527 0.014719 0.057565
mwoman2          woman2            cog=actor_mu.cog
# DESC:   
# BBOX: -0.031058 -0.027406 -0.023339 0.031058 0.027406 0.023339
bottles05        _structure        collide=1 model3d=bottles5.3do size=0.050000 movesize=0.050000
# DESC:   
# BBOX: -0.122086 -0.117592 -0.00067 0.122086 0.117592 0.295542
tarp             _walkstruct       thingflags=0x800048 model3d=tarp.3do size=0.390702 movesize=0.390702
# DESC:   
# BBOX: -0.100136 -0.200678 -0.052048 0.100126 0.200678 0.062505
vegtablestand    _walkstruct       thingflags=0x800048 model3d=vegstand.3do size=0.280234 movesize=0.280234
# DESC:   
# BBOX: -0.012767 -0.012147 -0.023339 0.012767 0.012147 0.023339
bottle05         _structure        model3d=bottle5.3do size=0.077465 movesize=0.077465
# DESC:   
# BBOX: -0.013049 -0.01372 -0.021966 0.013049 0.01372 0.021966
streetlightlamp  _structure        model3d=stlamp.3do size=0.076920 movesize=0.076920
# DESC:   
# BBOX: 0 0 0 0 0 0
+drugonbite      +gamaxe           damage=50.000000 range=0.400000
# DESC:   Sucker
# BBOX: -0.122593 -0.438462 -0.119063 0.144735 0.269351 0.112767
sucker           _swimactor        model3d=su.3do size=0.330000 movesize=0.330000 puppet=su.pup soundclass=sucker.snd mass=500.000000 physflags=0x404a0f maxvel=0.600000 weapon=+drugonbite health=160.00 maxhealth=160.00 maxthrust=0.70 maxrotthrust=90.00 typeflags=0x20140 aiclass=dudefault.ai
# DESC:   
# BBOX: -0.04933 -0.05743 -0.000743 0.047873 0.05743 0.104526
minichair2       _walkstruct       thingflags=0x800048 model3d=chair2m.3do size=0.179064 movesize=0.179064
# DESC:   
# BBOX: -0.2 -0.2 0 0.2 0.2 0
carpet           _walkstruct       thingflags=0x800048 model3d=carpet.3do size=0.332843 movesize=0.332843
# DESC:   
# BBOX: -0.9592 -0.9592 -0.219578 0.9592 0.9592 0.219578
waterpump        _zwalkstruct      move=physics model3d=wpmp.3do size=1.036299 movesize=1.036299 physflags=0x200 maxrotvel=25.000000
# DESC:   Dynamic Light 0.0
# BBOX: 0 0 0 0 0 0
light0.0         _ghostdecor       thingflags=0x1
# DESC:   Max The Rabbit Bryar Pistol
# BBOX: -0.007791 -0.090587 -0.006258 0.007791 0.017998 0.006258
+mbolt           +bryarbolt        model3d=bry1.3do damage=20.000000
# DESC:   Max The Rabbit 
# BBOX: -0.04238466 -0.02100349 -0.058578 0.03291002 0.03797358 0.0730266
max              _humanactor       model3d=mx.3do size=0.060000 movesize=0.060000 puppet=mx.pup soundclass=mx.snd mass=60.000000 maxvel=0.400000 weapon=+mbolt health=5000.00 maxhealth=5000.00 typeflags=0x1080000 fireoffset=(0.007000/0.048000/0.008000) aiclass=max.ai
# DESC:   
# BBOX: -0.013049 -0.01372 0.002105 0.013049 0.01372 0.021966
streetlightlamp2 _ghoststructure   model3d=stlamp1.3do size=0.076920 movesize=0.076920
# DESC:   
# BBOX: -0.964666 -0.781463 -0.554109 0.627416 0.781463 1.699674
tran_walltrans.  _ghostdecor       model3d=walt_b.3do size=1.906994 movesize=1.906994
# DESC:   
# BBOX: -0.063767 -0.055224 -0.021878 0.063767 0.055224 0.021878
probedebris_1    _debris           model3d=pb_1.3do size=0.053500 movesize=0.053500
# DESC:   
# BBOX: -0.071973 -0.062331 -0.029124 0.071973 0.062331 0.029124
probedebris_2    _debris           model3d=pb_2.3do size=0.057000 movesize=0.057000
# DESC:   
# BBOX: -0.009722 -0.012251 -0.047823 0.009722 0.012251 0.047823
probedebris_3    _debris           model3d=pb_3.3do size=0.041500 movesize=0.041500
# DESC:   
# BBOX: -0.048787 -0.012879 -0.031716 0.048787 0.012879 0.031716
probedebris_4    _debris           model3d=pb_4.3do size=0.040000 movesize=0.040000
# DESC:   
# BBOX: 0 0 0 0 0 0
+probedroid_exp  _explosion        thingflags=0x1 light=0.200000 timer=0.800000 sprite=seqx.spr soundclass=exp_probe.snd typeflags=0x17 damage=40.000000 blasttime=0.700000 force=200.000000 maxlight=0.800000 range=0.450000 debris=probedebris_1 debris=probedebris_2 debris=probedebris_3 debris=probedebris_4
# DESC:   
# BBOX: -0.01341 -0.149802 -0.01341 0.01341 0.015603 0.01341
+turretlaser     +stlaser          light=0.500000 model3d=con0.3do size=0.005000 movesize=0.005000 vel=(0.000000/5.000000/0.000000) fleshhit=+laserhit damage=15.000000 mindamage=5.000000
# DESC:   
# BBOX: -0.007791 -0.090587 -0.006258 0.007791 0.017998 0.006258
+probedroidbolt  +turretlaser      model3d=bry0.3do vel=(0.000000/4.000000/0.000000) damage=30.000000 mindamage=10.000000
# DESC:   ProbeDroid
# BBOX: -0.07470135 -0.09338056 -0.2356844 0.071943 0.06479 0.140417
probedroid       _flyactor         thingflags=0x20000400 model3d=pb.3do size=0.078000 movesize=0.078000 puppet=pb.pup soundclass=pb.snd cog=actor_pd.cog mass=300.000000 physflags=0x6a06 maxvel=0.500000 weapon=+probedroidbolt health=90.00 maxhealth=90.00 maxthrust=0.60 maxrotthrust=150.00 typeflags=0x2000120 fireoffset=(0.037500/0.065000/0.000000) explode=+probedroid_exp aiclass=pddefault.ai
# DESC:   
# BBOX: -0.2 -0.0125 -0.15 0.2 0.0125 0.25
07_4x4door       _structure        model3d=07d4x4.3do size=0.370400 movesize=0.370400 soundclass=med_door.snd
# DESC:   
# BBOX: -0.100779 -0.107544 0.000686 0.100779 0.107544 0.023173
ceilinglight     _structure        thingflags=0x9 light=0.500000 model3d=clight.3do size=0.116487 movesize=0.116487
# DESC:   
# BBOX: -0.1 -0.0125 -0.15 0.1 0.0125 0.15
2x3door_4        _structure        model3d=d2x3_4.3do size=0.180710 movesize=0.180710 soundclass=sm_door.snd
# DESC:   
# BBOX: -0.75 -0.2 -0.75 0.75 0.2 0.75
06solidlockdoor  _walkstruct       thingflags=0x48 model3d=06lock_1.3do size=1.129352 movesize=1.129352 soundclass=06stone_door.snd
# DESC:   
# BBOX: -0.75 -0.2 -0.85 0.75 0.2 0.85
06holeylockdoor  _walkstruct       thingflags=0x48 model3d=06lock_2.3do size=1.201086 movesize=1.201086 soundclass=06stone_door.snd
# DESC:   
# BBOX: 0 0 0 0 0 0
+turret_exp      _explosion        thingflags=0x1 light=0.200000 timer=0.800000 sprite=detx.spr soundclass=exp_probe.snd typeflags=0x17 damage=10.000000 blasttime=0.700000 force=100.000000 maxlight=0.800000 range=0.450000 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_5
# DESC:   
# BBOX: -0.01341 -0.149802 -0.01341 0.01341 0.015603 0.01341
+turretlaser2    +turretlaser      vel=(0.000000/4.000000/0.000000) damage=25.000000 mindamage=10.000000
# DESC:   Turret Gun
# BBOX: -0.059999 -0.051358 -0.040791 0.060835 0.061748 0.050725
boxturret        none              orient=(0.000000/0.000000/0.000000) type=actor collide=1 move=physics thingflags=0x400 model3d=tur1.3do size=0.075000 movesize=0.075000 puppet=tur1.pup soundclass=turret.snd cog=class_boxturret.cog physflags=0x400000 weapon=+turretlaser2 health=80.00 maxhealth=80.00 typeflags=0x2080160 fireoffset=(0.000000/-0.003000/0.036000) explode=+turret_exp aiclass=turbdefault.ai
# DESC:   
# BBOX: -0.35 -0.15 -0.55 0.35 0.15 0.55
06thirddoor      _walkstruct       thingflags=0x48 model3d=06door3.3do size=0.718954 movesize=0.718954 soundclass=06stone_door.snd
# DESC:   
# BBOX: -0.13965 -1.25 -0.069825 0.13965 0.05 0.069825
06bolt           _walkstruct       collide=1 model3d=06bolt.3do size=0.100000 movesize=0.100000 soundclass=06bolt.snd
# DESC:   Imperial Officer
# BBOX: -0.037458 -0.014968 -0.120277 0.037328 0.026217 0.066384
iofficer         _humanactor       thingflags=0x20000400 model3d=io.3do size=0.066700 movesize=0.066700 puppet=io.pup soundclass=io.snd cog=actor_io.cog maxvel=0.300000 weapon=+ebolt health=30.00 maxthrust=0.90 maxrotthrust=80.00 typeflags=0x1 fireoffset=(0.050000/0.065000/0.014000) aiclass=iodefault.ai
# DESC:   Stormtrooper
# BBOX: -0.02978572 -0.01917877 -0.1247011 0.02409064 0.06467322 0.0434061
stormtroop       _humanactor       thingflags=0x20000400 model3d=st.3do puppet=st.pup soundclass=st.snd cog=actor_st.cog maxvel=0.250000 weapon=+elaser health=60.00 maxhealth=60.00 maxthrust=1.00 maxrotthrust=90.00 typeflags=0x1 fireoffset=(0.004800/0.050000/0.012000) aiclass=stdefault.ai
# DESC:   Imperial Officer w diff Voice
# BBOX: -0.037458 -0.014968 -0.120277 0.037328 0.026217 0.066384
iofficer2        iofficer          soundclass=io2.snd
# DESC:   
# BBOX: -0.095 -0.2 -0.095 0.095 0.2 0.095
06lock_x         _structure        model3d=06lock_x.3do size=0.290936 movesize=0.290936
# DESC:   
# BBOX: -0.2375 -0.2 -0.095 0.2375 0.2 0.095
06lock_y         _structure        model3d=06lock_y.3do size=0.374702 movesize=0.374702
# DESC:   Door 2x3m Single w Orange warning on right side
# BBOX: -0.1 -0.0125 -0.15 0.1 0.0125 0.15
2x3door_1        _structure        model3d=d2x3_1.3do size=0.180710 movesize=0.180710 soundclass=sm_door.snd
# DESC:   
# BBOX: -0.005653 -0.113701 -0.004853 0.005653 0.02259 0.004853
+lasercannon     +stlaser          light=0.500000 size=0.015000 movesize=0.015000 vel=(0.000000/4.000000/0.000000) fleshhit=+laserhit damage=40.000000
# DESC:   AT-ST with Commando
# BBOX: -0.1616239 -0.2391249 -0.45412 0.1605707 0.2508638 0.2235963
atst             _humanactor       thingflags=0x4 model3d=at.3do size=0.300000 movesize=0.300000 puppet=atst.pup soundclass=atst.snd cog=actor_atst.cog mass=0.000000 maxvel=0.400000 weapon=+lasercannon health=400.00 maxhealth=400.00 maxthrust=0.30 maxrotthrust=10.00 typeflags=0x100 fireoffset=(0.000000/0.200000/0.000000) aiclass=atstdefault.ai
# DESC:   AT-ST with Officer
# BBOX: -0.1616239 -0.2391249 -0.45412 0.1605707 0.2508638 0.2235963
iatst            atst              thingflags=0x404 cog=actor_iatst.cog
# DESC:   
# BBOX: -0.6 -2.1 -0.048 0.6 0.2 0.048
06archdrawbridge _walkstruct       model3d=08dbrg.3do size=2.234560 movesize=2.234560 soundclass=06drawbridge.snd
# DESC:   
# BBOX: -0.6 -2.1 -0.048 0.6 0.2 0.048
06drawbridge     _zwalkstruct      model3d=08dbrg.3do size=2.234560 movesize=2.234560 soundclass=06drawbridge.snd
# DESC:   Sequencer Mines Pickup
# BBOX: -0.019886 -0.022184 0 0.019886 0.022184 0.042868
seqcharge        _powerup          thingflags=0x400 model3d=seqp.3do cog=pow_sequencer.cog
# DESC:   Expolding Tank 4 Meters Tall
# BBOX: -0.160097 -0.138783 -0.199662 0.160096 0.138783 0.199662
xtank4a          _walkstruct       thingflags=0x400448 model3d=xtank4a.3do size=0.255942 movesize=0.255942 cog=xtank4.cog
# DESC:   Weapon SuperCharge Pickup 
# BBOX: -0.016734 -0.016734 -0.026596 0.016734 0.016734 0.026596
powerboost       _powerup          thingflags=0x400 model3d=boost.3do cog=pow_powerboost.cog respawn=60.000000
# DESC:   21B Medical Droid 
# BBOX: -0.03448359 -0.022835 -0.1202637 0.04545573 0.021911 0.07040831
twoonebee        none              orient=(0.000000/0.000000/0.000000) type=cog collide=1 thingflags=0x400 model3d=med.3do size=0.060000 movesize=0.060000 puppet=cr.pup cog=00_twoonebee.cog
# DESC:   Expolding Fuel Tank 4 Meters Tall
# BBOX: -0.160097 -0.138783 -0.199662 0.160096 0.138783 0.199662
xtank4rd         xtank4a           cog=xtank4rd.cog
# DESC:   Stormtrooper Special Seeing Ai
# BBOX: -0.02978572 -0.01917877 -0.1247011 0.02409064 0.06467322 0.0434061
20storm          stormtroop        aiclass=st2020.ai
# DESC:   Stormtrooper Roaming
# BBOX: -0.02978572 -0.01917877 -0.1247011 0.02409064 0.06467322 0.0434061
rstorm           stormtroop        aiclass=stroam.ai
# DESC:   
# BBOX: -0.012585 -0.050304 -0.010582 0.022579 0.027638 0.007152
+stormlimb       _limb             size=0.050000 movesize=0.010000
# DESC:   Imperial Commando
# BBOX: -0.037893 -0.021815 -0.12043 0.03746 0.022321 0.066407
icommando        _humanactor       thingflags=0x20000400 model3d=ic.3do size=0.065950 movesize=0.065950 puppet=ic.pup soundclass=ic.snd cog=actor_ic.cog maxvel=0.300000 weapon=+elaser health=90.00 maxhealth=90.00 maxthrust=1.10 maxrotthrust=90.00 typeflags=0x1 fireoffset=(0.014000/0.059000/0.031000) aiclass=icdefault.ai
# DESC:   Enemy Repeater Gun
# BBOX: -0.005771 -0.015021 -0.00466 0.005771 0.003935 0.00466
+eball           +repeaterball     damage=8.000000 mindamage=4.000000
# DESC:   Imperial Commando w Repeatergun
# BBOX: -0.037893 -0.02023 -0.12043 0.03746 0.022321 0.065552
icommandorpt     icommando         model3d=icrpt.3do soundclass=icrpt.snd cog=actor_i2.cog weapon=+eball aiclass=icreapdefault.ai
# DESC:   
# BBOX: 0 0 0 0 0 0
+atst_exp        +grenade_exp      damage=10.000000
# DESC:   
# BBOX: 0 0 0 0 0 0
+xtank4rd_exp    +xtank4_exp       damage=1000.000000 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_5
# DESC:   Repeater Gun Dropped
# BBOX: -0.00548 -0.051379 -0.021767 0.00548 0.039969 0.012035
+drepeatergun    _droppowerup      thingflags=0x400 model3d=rptp.3do cog=pow_drepeater.cog height=0.042069
# DESC:   
# BBOX: 0 0 0 0 0 0
_darkjedi        _humanactor       typeflags=0x2000201
# DESC:   DarkJedi Yun
# BBOX: -0.031992 -0.017003 -0.134424 0.032032 0.024487 0.073659
yun              _darkjedi         model3d=yu.3do size=0.060400 movesize=0.060400 puppet=yu.pup soundclass=yu.snd mass=160.000000 maxvel=0.300000 weapon=+gamaxe health=500.00 maxhealth=500.00 maxrotthrust=200.00 jumpspeed=1.60 error=0.60 fov=0.50 chance=1.00 aiclass=yun.ai
# DESC:   
# BBOX: -0.25 -0.025 -0.2 0.25 0.025 0.2
07door5x4        _walkstruct       model3d=07d5x4.3do size=0.371131 movesize=0.371131 soundclass=med_door.snd
# DESC:   
# BBOX: -0.198658 -0.198003 -0.001357 0.197787 0.198497 0.598643
07lift           _walkstruct       thingflags=0x48 model3d=07lift.3do size=0.711240 movesize=0.711240 soundclass=med_elev.snd
# DESC:   
# BBOX: -0.165685 -0.006493 -0.2 0.165685 0.006493 0.2
tridoor          _walkstruct       thingflags=0x48 model3d=trid.3do size=0.309796 movesize=0.309796 soundclass=med_door.snd
# DESC:   
# BBOX: -0.092328 -0.184027 -0.04612 0.092328 0.184027 0.04612
table07          _walkstruct       thingflags=0x48 model3d=table07.3do size=0.260992 movesize=0.260992
# DESC:   Camera Droid
# BBOX: -0.037102 -0.076502 -0.041518 0.037102 0.046066 0.03417
droidcam         _decor            collide=0 model3d=rs.3do size=0.127630 movesize=0.127630 puppet=cr.pup
# DESC:   
# BBOX: -0.092357 -0.112123 -0.045808 0.092357 0.112123 0.045808
table07a         _walkstruct       thingflags=0x48 model3d=table07b.3do size=0.202315 movesize=0.202315
# DESC:   
# BBOX: -0.092357 -0.109937 -0.045808 0.092357 0.109937 0.045808
table07b         _walkstruct       thingflags=0x48 model3d=table07a.3do size=0.200712 movesize=0.200712
# DESC:   
# BBOX: 0 0 0 0 0 0
+furniture_exp   _explosion        thingflags=0x1 light=0.200000 timer=0.800000 sprite=detx.spr soundclass=exp_small.snd creatething=+firecloud typeflags=0x17 blasttime=0.700000 force=100.000000 maxlight=0.800000 range=0.450000
# DESC:   Turret Gun Round                                              
# BBOX: -0.058293 -0.078937 -0.033443 0.058195 0.084676 0.035083
roundturret      none              orient=(0.000000/0.000000/0.000000) type=actor collide=1 move=physics thingflags=0x400 model3d=tur2.3do size=0.080000 movesize=0.080000 puppet=tur2.pup soundclass=turret.snd cog=class_roundturret.cog physflags=0x400000 weapon=+turretlaser health=100.00 maxhealth=100.00 typeflags=0x2080160 fireoffset=(0.000000/-0.028000/0.022000) explode=+turret_exp aiclass=turdefault.ai
# DESC:   
# BBOX: -0.15 -0.3 -0.012306 0.15 0.3 0.012306
3x6catwalk       _walkstruct       model3d=c3x6.3do size=0.335636 movesize=0.335636
# DESC:   FieldTrooper
# BBOX: -0.03618964 -0.01917877 -0.1247011 0.02409064 0.06467322 0.0434061
stormtroop2      _humanactor       thingflags=0x20000400 model3d=s2.3do puppet=st.pup soundclass=s2.snd cog=actor_st.cog maxvel=0.250000 weapon=+elaser health=70.00 maxhealth=70.00 maxthrust=0.90 maxrotthrust=90.00 typeflags=0x1 fireoffset=(0.004800/0.050000/0.012000) aiclass=stdefault.ai
# DESC:   StormTrooper W Repeater
# BBOX: -0.03618964 -0.01917877 -0.1247011 0.02409064 0.06395282 0.0434061
stormtroop3      _humanactor       thingflags=0x20000400 model3d=s3.3do puppet=s3.pup soundclass=s3.snd cog=actor_i2.cog maxvel=0.250000 weapon=+eball health=70.00 maxhealth=70.00 maxthrust=0.90 maxrotthrust=90.00 typeflags=0x1 fireoffset=(0.009800/0.057000/0.010000) aiclass=streapdefault.ai
# DESC:   
# BBOX: 0 0 0 0 0 0
+eraildet_exp    +raildet_exp      damage=50.000000 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_4
# DESC:   Enemy Rail Detonator
# BBOX: -0.01058 -0.013704 -0.006548 0.010593 0.009939 0.012329
+eraildet        +raildet2         vel=(0.000000/2.000000/0.000000) explode=+eraildet_exp fleshhit=+eraildet_exp damage=20.000000 typeflags=0xb81
# DESC:   
# BBOX: 0 0 0 0 0 0
+smack           +gamaxe           damage=15.000000 range=0.150000
# DESC:   StormTrooper W Railgun
# BBOX: -0.03618964 -0.01917877 -0.1247011 0.02409064 0.08688994 0.0434061
stormtroop4      _humanactor       thingflags=0x20000400 model3d=s4.3do puppet=s2.pup soundclass=s4.snd cog=actor_s3.cog maxvel=0.250000 weapon=+eraildet weapon2=+smack health=70.00 maxhealth=70.00 maxthrust=0.90 maxrotthrust=90.00 typeflags=0x20001 fireoffset=(0.014000/0.053200/0.013000) aiclass=straildefault.ai
# DESC:   
# BBOX: 0 0 0 0 0 0
+kellbite        +gamaxe           damage=50.000000 range=0.300000 force=100.000000
# DESC:   Kell Dragon
# BBOX: -0.2104616 -0.4739575 -0.1266591 0.2112462 0.1834475 0.1065705
kelldragon       _humanactor       thingflags=0x404 model3d=kd.3do size=0.125000 movesize=0.125000 puppet=kd.pup soundclass=kd.snd cog=actor_kd.cog mass=3000.000000 maxvel=0.600000 weapon=+kellbite health=500.00 maxhealth=500.00 maxthrust=1.00 maxrotthrust=120.00 typeflags=0x2028100 aiclass=kddefault.ai
# DESC:   
# BBOX: -0.024596 -0.024511 -0.064105 0.024596 0.024511 0.064105
console1         _walkstruct       model3d=con1.3do size=0.072905 movesize=0.072905
# DESC:   
# BBOX: -0.817281 -1.132091 -0.454926 0.815909 1.276639 1.852229
shuttle          _decor            model3d=shut.3do size=1.000000 movesize=1.000000 puppet=cr.pup
# DESC:   
# BBOX: 0 0 0 0 0 0
+tiebomb_exp     _explosion        thingflags=0x1 light=0.200000 timer=0.800000 sprite=tiex.spr soundclass=exp_tie.snd typeflags=0x17 damage=100.000000 blasttime=0.700000 force=300.000000 maxlight=0.800000 range=1.000000
# DESC:   
# BBOX: 0 0 0 0 0 0
08tiebomb        _weapon           sprite=tiebomb.spr soundclass=det.snd surfdrag=3.000000 airdrag=0.800000 mass=1.000000 physflags=0x225 vel=(0.000000/0.000000/-1.000000) angvel=(90.000000/45.000000/90.000000) explode=+tiebomb_exp damageclass=0x1 typeflags=0x30d
# DESC:   
# BBOX: -0.549776 -0.490931 -0.315187 0.549776 0.4752 0.320955
08tiebomber      _decor            model3d=tieb.3do size=0.300000 movesize=0.300000 puppet=cr.pup
# DESC:   
# BBOX: 0 0 0 0 0 0
+extiebomb_exp   _explosion        thingflags=0x1 light=0.200000 timer=0.800000 sprite=tiex.spr soundclass=exp_tie.snd typeflags=0x17 blasttime=0.700000 maxlight=0.800000
# DESC:   
# BBOX: 0 0 0 0 0 0
08extiebomb      _weapon           sprite=tiebomb.spr soundclass=det.snd surfdrag=3.000000 airdrag=0.800000 mass=1.000000 physflags=0x225 vel=(0.000000/0.000000/-1.000000) angvel=(90.000000/45.000000/90.000000) explode=+extiebomb_exp damageclass=0x1 typeflags=0x30d
# DESC:   RailGun Dropped
# BBOX: -0.018484 -0.070714 -0.030025 0.019883 0.081826 0.013227
+drailgun        _droppowerup      thingflags=0x400 model3d=rldp.3do cog=pow_drailgun.cog height=0.044420
# DESC:   
# BBOX: 0 0 0 0 0 0
+tie_exp         _explosion        thingflags=0x1 light=0.200000 timer=0.800000 sprite=tiex.spr soundclass=exp_tie.snd typeflags=0x17 damage=20.000000 blasttime=0.700000 force=300.000000 maxlight=0.800000 range=1.000000
# DESC:   
# BBOX: -0.255833 -0.483066 -0.318071 0.255833 0.483066 0.318071
tieb_debris_1    _debris           model3d=tieb_1.3do size=0.553984 movesize=0.553984
# DESC:   
# BBOX: -0.123836 -0.490931 -0.315187 0.123836 0.4752 0.320955
tieb_debris_2    _debris           model3d=tieb_2.3do size=0.514754 movesize=0.514754
# DESC:   
# BBOX: -0.204306 -0.300444 -0.149529 0.204306 0.284713 0.155297
tieb_debris_3    _debris           model3d=tieb_3.3do size=0.371837 movesize=0.371837
# DESC:   
# BBOX: -0.048269 -0.051853 -0.059699 0.048269 0.036122 0.065467
tieb_debris_4    _debris           model3d=tieb_4.3do size=0.083231 movesize=0.083231
# DESC:   
# BBOX: 0 0 0 0 0 0
+tieb_exp        +tie_exp          debris=tieb_debris_1 debris=tieb_debris_2 debris=tieb_debris_3 debris=tieb_debris_4
# DESC:   
# BBOX: -0.15 -0.5 -0.012306 0.15 0.5 0.012306
c3x10e           _walkstruct       model3d=c3x10e.3do size=0.522160 movesize=0.522160
# DESC:   
# BBOX: -0.175 -0.025 -0.175 0.175 0.025 0.175
09_octdoor       _structure        thingflags=0xc model3d=09doct1.3do size=0.192040 movesize=0.192040 soundclass=med_door.snd
# DESC:   
# BBOX: -0.175 -0.8 -0.063278 0.175 0.8 0.063278
pontoon          _zwalkstruct      model3d=pontoon.3do size=0.871358 movesize=0.871358
# DESC:   
# BBOX: -0.1 -0.0125 -0.1 0.1 0.0125 0.1
09_2x2door       _structure        model3d=09d2x2_1.3do size=0.141973 movesize=0.141973 soundclass=sm_door.snd
# DESC:   
# BBOX: -0.1 -0.0125 -0.15 0.1 0.0125 0.15
09_2x3door1      _structure        model3d=09d2x3_1.3do size=0.180710 movesize=0.180710 soundclass=sm_door.snd
# DESC:   
# BBOX: -2 -2.000001 -4.00002 2 2.000001 4.00002
09tank_r20       _structure        thingflags=0xc model3d=09tnk.3do size=4.472154 movesize=4.472154
# DESC:   
# BBOX: -0.078121 -0.099868 -0.090436 0.078121 0.099868 0.090436
09pipebrace      _structure        model3d=09pipb.3do size=0.155740 movesize=0.155740
# DESC:   
# BBOX: -1.000702 -0.2 -0.2 1.000702 0.2 0.2
09pipe_18        _structure        thingflags=0xc model3d=09pipe18.3do size=1.020492 movesize=1.020492
# DESC:   
# BBOX: -0.4 -0.025 -0.4 0.4 0.025 0.4
rounddoor4       _walkstruct       thingflags=0x400848 model3d=dr4.3do size=0.400781 movesize=0.400781
# DESC:   
# BBOX: -1.440012 -0.1 -1.440018 1.439986 0.1 -1.7E-5
sewerdoor        _walkstruct       model3d=dr13half.3do size=1.471634 movesize=1.471634 soundclass=lg_door.snd
# DESC:   
# BBOX: -0.6 -0.075 -0.3 0.6 0.075 0.3
halfcircledoor   _walkstruct       model3d=dr6half.3do size=0.675000 movesize=0.675000 soundclass=med_door.snd
# DESC:   
# BBOX: -0.5 -0.2375 -0.1875 0.5 0.2375 0.1875
door10x3         _walkstruct       model3d=d10x375.3do size=0.634434 movesize=0.634434 soundclass=09accessdoor.snd
# DESC:   
# BBOX: -0.45 -0.0375 -0.15 0.45 0.0375 0.15
door9x3          _walkstruct       model3d=d9x3.3do size=0.525822 movesize=0.525822 soundclass=09accessdoor.snd
# DESC:   Expolding Tank 3 Meters Tall
# BBOX: -0.120072 -0.104087 -0.149746 0.120072 0.104087 0.149746
xtank3a          _walkstruct       thingflags=0x400448 model3d=xtank3a.3do size=0.191956 movesize=0.191956 cog=xtank3.cog
# DESC:   
# BBOX: -35.42969 -4.751811 -9.361874 35.42969 4.75181 9.335024
09cargoship      _ghoststructure   model3d=cargo2.3do size=35.991570 movesize=35.991570
# DESC:   
# BBOX: -6.669081 -0.681354 -1.103577 6.66908 0.681354 1.103577
09cargocatwalk   _walkstruct       model3d=cargowlk.3do size=6.844024 movesize=6.844024 soundclass=09cargocatwalk.snd
# DESC:   Yellow Key Pickup
# BBOX: -0.010023 -0.016964 -0.033841 0.010023 0.016964 0.033841
keyyellow        _powerup          thingflags=0x400 model3d=y-key.3do cog=pow_keyyellow.cog
# DESC:   Ugnaut 
# BBOX: -0.035186 -0.01745 -0.100835 0.035171 0.022972 0.063971
ug               _civilian         thingflags=0x20000400 model3d=ugalt.3do size=0.064000 movesize=0.064000 puppet=ug.pup soundclass=ug.snd cog=actor_ug.cog mass=60.000000 maxvel=0.400000 health=30.00 maxthrust=0.50 maxrotthrust=60.00 aiclass=peddefault.ai
# DESC:   Ugnaut Sitting
# BBOX: -0.035186 -0.01745 -0.100835 0.035171 0.022972 0.063971
ugsit            ug                puppet=ugsit.pup mass=200.000000 aiclass=pedhome.ai
# DESC:   Enemy Concussion Rifle
# BBOX: 0 0 0 0 0 0
+econc_exp       +conc_exp         damage=60.000000
# DESC:   
# BBOX: -0.01341 -0.149802 -0.01341 0.01341 0.015603 0.01341
+econcbullet     +concbullet       explode=+econc_exp fleshhit=+econc_exp damage=10.000000
# DESC:   
# BBOX: 0 0 0 0 0 0
+bosskswipe      +gamaxe           damage=20.000000 range=0.200000
# DESC:   Bossk w Concussion Rifle
# BBOX: -0.040341 -0.017317 -0.123441 0.040266 0.112852 0.068602
bossk            _humanactor       thingflags=0x20000400 model3d=bs.3do size=0.068100 movesize=0.068100 puppet=bs.pup soundclass=bs.snd cog=actor_bs.cog mass=120.000000 maxvel=0.400000 weapon=+econcbullet weapon2=+bosskswipe health=120.00 maxhealth=120.00 maxthrust=0.90 maxrotthrust=80.00 typeflags=0x20a0001 fireoffset=(-0.039000/0.070000/0.027000) aiclass=bodefault.ai
# DESC:   Ugnaut w wrench
# BBOX: -0.033249 -0.025869 -0.100835 0.033234 0.06466 0.062592
u2               ug                model3d=u2alt.3do puppet=u2.pup aiclass=pedhome.ai
# DESC:   Bossk w RailDetonator
# BBOX: -0.040341 -0.017317 -0.123441 0.041996 0.078346 0.068602
bosskrld         bossk             model3d=bsrld.3do soundclass=bsrld.snd cog=actor_s3.cog weapon=+eraildet fireoffset=(-0.035000/0.070000/0.027000) aiclass=bldefault.ai
# DESC:   Wrench Pickup 
# BBOX: -0.032096 -0.017684 -0.038933 0.032096 0.017684 0.038933
wrench           _powerup          thingflags=0x400 model3d=t-key.3do cog=pow_wrench.cog
# DESC:   
# BBOX: -0.3 -0.6 -0.1 0.3 1.8 0.1
09tribolt        _walkstruct       thingflags=0x4c model3d=09bogus.3do size=1.875299 movesize=1.875299 soundclass=09pipes.snd
# DESC:   
# BBOX: -0.0875 -3.1 0 0.0875 0 0
09slice          _ghoststructure   model3d=09slice.3do size=3.151235 movesize=3.151235
# DESC:   
# BBOX: -0.55 -0.092959 -0.28125 0.55 0.092959 0.28125
door11x5         _walkstruct       model3d=d11x5625.3do size=0.674694 movesize=0.674694 soundclass=09accessdoor.snd
# DESC:   RailGun Ammo Pickup
# BBOX: -0.01636 -0.014643 -0.011603 0.017727 0.017747 0.016258
railcharges      _powerup          thingflags=0x400 model3d=rcrg.3do cog=pow_railcharges.cog
# DESC:   
# BBOX: -0.4 -0.025 -0.4 0.4 0.025 0.4
rounddoor4hole   _walkstruct       thingflags=0x400848 model3d=dr4hole.3do size=0.450781 movesize=0.450781
# DESC:   Concussion Rifle Dropped
# BBOX: -0.030347 -0.072704 -0.024787 0.009211 0.072704 0.024341
+dconcrifle      _droppowerup      thingflags=0x400 model3d=conp.3do cog=pow_dconcrifle.cog height=0.024753
# DESC:   
# BBOX: -0.043832 -0.019016 -0.007017 0.043832 0.019016 0.007017
+bslimb          _limb             model3d=bslimb.3do size=0.050000 movesize=0.010000
# DESC:   
# BBOX: -0.3 -0.025 -0.8 0.3 0.025 0.8
mndr             _walkstruct       model3d=mndr.3do size=0.904766 movesize=0.904766
# DESC:   Door Right Side of 3x3 Drk Grey with Lt Grey
# BBOX: -0.05 -0.0125 -0.15 0.1 0.0125 0.15
3x3door_2a       _structure        model3d=d3x3_2a.3do size=0.230710 movesize=0.230710 soundclass=med_door.snd
# DESC:   Door Left Side of 3x3 Drk Grey with Lt Grey
# BBOX: -0.1 -0.0125 -0.15 0.05 0.0125 0.15
3x3door_2b       _structure        model3d=d3x3_2b.3do size=0.230710 movesize=0.230710
# DESC:   
# BBOX: -0.15 -0.0125 -0.15 0.15 0.0125 0.15
10_3x3door       _structure        model3d=10d3x3.3do size=0.262500 movesize=0.262500 soundclass=med_door.snd
# DESC:   Door 1/2 3x3  same as 3x3door_2a w angled edges
# BBOX: -0.075 -0.1025 -0.15 0.075 0.1025 0.15
odd_door_a       _structure        model3d=d3x3odda.3do size=0.246548 movesize=0.246548 soundclass=med_door.snd
# DESC:   Door 1/2 3x3  same as 3x3door_2b w angled edges
# BBOX: -0.075 -0.1025 -0.15 0.075 0.1025 0.15
odd_door_b       _structure        model3d=d3x3oddb.3do size=0.246548 movesize=0.246548
# DESC:   
# BBOX: -0.016314 -0.219705 -0.118596 0.016314 0.223171 0.249539
10fanfast        none              orient=(0.000000/0.000000/0.000000) type=cog collide=3 move=physics model3d=fan0.3do size=0.299843 movesize=0.299843 physflags=0x200 maxrotvel=400.000000 angvel=(300.000000/0.000000/0.000000)
# DESC:   
# BBOX: -0.15 -1.875 -0.012306 0.15 1.875 0.012306
3x37catwalk      _walkstruct       model3d=c3x37.3do size=1.881031 movesize=1.881031
# DESC:   
# BBOX: -0.125 -0.1625 -0.25 0.125 0.1625 0.25
10_door1a        _walkstruct       model3d=10door1a.3do size=0.323313 movesize=0.323313 soundclass=med_door.snd
# DESC:   
# BBOX: -0.125 -0.1625 -0.25 0.125 0.1625 0.25
10_door1b        _walkstruct       model3d=10door1b.3do size=0.323313 movesize=0.323313 soundclass=med_door.snd
# DESC:   
# BBOX: -0.25 -0.1125 -0.25 0.25 0.1125 0.25
10_door2         _walkstruct       model3d=10door2.3do size=0.371021 movesize=0.371021 soundclass=med_door.snd
# DESC:   
# BBOX: -0.1 -0.0925 -0.2 0.1 0.0925 0.2
10_door3         _walkstruct       model3d=10door3.3do size=0.241984 movesize=0.241984 soundclass=med_door.snd
# DESC:   
# BBOX: -0.75 -0.515 -0.475 0.75 0.515 0.475
12_door1         _structure        model3d=12dr01.3do size=1.076329 movesize=1.076329 soundclass=lg_door.snd
# DESC:   
# BBOX: -0.325 -0.0125 -0.25 0.325 0.0125 0.25
10_door4         _walkstruct       model3d=10door4.3do size=0.460221 movesize=0.460221 soundclass=lg_door.snd
# DESC:  
# BBOX: -0.15 -0.025 -0.1 0.15 0.025 0.1
3x2door_2        _zwalkstruct      model3d=d3x2_2.3do size=0.232003 movesize=0.232003 soundclass=sm_door.snd
# DESC:   Dynamic Light 0.1
# BBOX: 0 0 0 0 0 0
light0.1         _ghostdecor       thingflags=0x1 light=0.100000
# DESC:   Blue Key Pickup
# BBOX: -0.010023 -0.016964 -0.033841 0.010023 0.016964 0.033841
keyblue          _powerup          thingflags=0x400 model3d=b-key.3do cog=pow_keyblue.cog
# DESC:   
# BBOX: -0.025 -0.5 -0.025 0.025 0.5 0.025
10_beam          _zwalkstruct      model3d=10beam10.3do size=0.551248 movesize=0.551248 soundclass=lg_elev.snd
# DESC:   
# BBOX: -0.025 -0.375 -0.025 0.025 0.375 0.025
10_beam9         _zwalkstruct      model3d=10beam09.3do size=0.426663 movesize=0.426663 soundclass=lg_elev.snd
# DESC:   R2 Droid
# BBOX: -0.035317 -0.025605 -0.047024 0.035366 0.022343 0.048893
r2               _droidactor       thingflags=0x20000400 model3d=r2.3do size=0.058000 movesize=0.058000 puppet=r2.pup soundclass=r2.snd cog=actor_r2.cog maxvel=0.400000 health=50.00 maxhealth=50.00 maxthrust=0.40 maxrotthrust=50.00 typeflags=0x180140 aiclass=drdefault.ai
# DESC:   
# BBOX: -0.016314 -0.219705 -0.118596 0.016314 0.223171 0.249539
10fanstop        none              orient=(0.000000/0.000000/0.000000) type=cog collide=3 move=physics model3d=fan0.3do size=0.299843 movesize=0.299843 physflags=0x200 maxrotvel=400.000000
# DESC:   
# BBOX: 0 0 0 0 0 0
+superenerg      none              orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=0.600000 vel=(0.000000/0.000000/2.000000) angvel=(0.000000/360.000000/0.000000) typeflags=0x3e material=00red.mat range=1.200000 elementsize=0.020000 count=30
# DESC:   
# BBOX: -1.2 -1.8 -0.3 1.2 1.8 0.3
tetriscrate      _walkstruct       model3d=00tetris.3do size=2.234033 movesize=2.234033
# DESC:   DarkJedi Gorc
# BBOX: -0.085744 -0.044203 -0.133945 0.084923 0.128448 0.082581
gorc             _darkjedi         model3d=go.3do size=0.082800 movesize=0.082800 puppet=go.pup soundclass=go.snd airdrag=0.400000 mass=280.000000 maxvel=0.300000 weapon=+gamaxe health=800.00 maxhealth=800.00 maxrotthrust=200.00 jumpspeed=1.60 error=0.40 fov=0.50 chance=1.00 aiclass=gorc.ai
# DESC:   DarkJedi Pic
# BBOX: -0.028706 -0.013615 -0.072261 0.024789 0.02181 0.039997
pic              _darkjedi         model3d=pi.3do size=0.040050 movesize=0.040050 puppet=pi.pup soundclass=pi.snd mass=75.000000 maxvel=0.200000 weapon=+gamaxe health=600.00 maxhealth=600.00 maxthrust=1.50 jumpspeed=0.75 error=0.50 fov=0.40 chance=0.80 aiclass=pic.ai
# DESC:   8t88 Sitting
# BBOX: -0.018951 -0.01169 -0.121405 0.032317 0.017665 0.054771
8t88_sittingbody _decor            model3d=8tsit.3do size=0.060000 movesize=0.060000
# DESC:   8t88 Head
# BBOX: -0.005748 -0.007884 -0.009493 0.005748 0.007884 0.009493
8t88_head        none              orient=(0.000000/0.000000/0.000000) type=cog collide=1 model3d=8thead.3do size=0.061601 movesize=0.061601
# DESC:   
# BBOX: -0.098 -0.0125 -0.15 0.098 0.0125 0.15
12door2x3        _structure        model3d=12d2x3_1.3do size=0.229611 movesize=0.229611 soundclass=sm_door.snd
# DESC:   
# BBOX: -0.597832 -0.597465 -0.299701 0.597179 0.59758 0.299701
00archcrate      _walkstruct       model3d=00crte.3do size=0.830224 movesize=0.830224
# DESC:   
# BBOX: -0.597832 -0.597465 -0.299701 0.597179 0.59758 0.299701
00crate          _walkstruct       model3d=00crte.3do size=0.830224 movesize=0.830224
# DESC:   
# BBOX: -0.4875 -0.025 -0.4 0.4875 0.025 0.4
12_door2a        _structure        model3d=12dr02_a.3do size=0.681095 movesize=0.681095 soundclass=lg_door.snd
# DESC:   
# BBOX: -0.4875 -0.025 -0.4 0.4875 0.025 0.4
12_door2b        _structure        model3d=12dr02_b.3do size=0.681095 movesize=0.681095
# DESC:   
# BBOX: -0.170777 -0.170777 -0.012502 0.170777 0.170777 0.012502
12_epad1         _walkstruct       model3d=12epad.3do size=0.224571 movesize=0.224571 soundclass=med_elev.snd
# DESC:   
# BBOX: -0.119 -0.0975 -0.0125 0.119 0.0975 0.0125
12_epad2         _walkstruct       model3d=12pad2.3do size=0.154349 movesize=0.154349 soundclass=sm_elev.snd
# DESC:   
# BBOX: -0.95 -0.04 -1.005 0.95 0.04 1.005
12_door3         _structure        model3d=12dr03.3do size=1.433519 movesize=1.433519 soundclass=lg_door.snd
# DESC:   Green key Pickup
# BBOX: -0.010023 -0.016964 -0.033841 0.010023 0.016964 0.033841
keygreen         _powerup          thingflags=0x400 model3d=g-key.3do cog=pow_keygreen.cog
# DESC:   
# BBOX: -0.15 -0.4 -0.0125 0.15 0.4 0.0125
12ucatwalk_a     _walkstruct       model3d=12uwalka.3do size=0.477383 movesize=0.477383
# DESC:   
# BBOX: -0.505 -0.15 -0.0125 0.505 0.15 0.0125
12ucatwalk_b     _walkstruct       model3d=12uwalkb.3do size=0.576955 movesize=0.576955
# DESC:   
# BBOX: -0.15 -0.4 -0.0125 0.15 0.4 0.0125
12ucatwalk_c     _walkstruct       model3d=12uwalkc.3do size=0.477383 movesize=0.477383
# DESC:   
# BBOX: 0 0 0 0 0 0
+superenerg2     +superenerg       range=0.600000
# DESC:   
# BBOX: -0.650003 -0.037533 -0.499995 0.650003 0.037533 0.499995
14d13x10         _structure        model3d=14d13x10.3do size=0.820919 movesize=0.820919 soundclass=lg_door.snd
# DESC:   
# BBOX: -0.400003 -0.037533 -0.499995 0.400003 0.037533 0.299995
14d8x8           _structure        model3d=14d8x8.3do size=0.641410 movesize=0.641410 soundclass=lg_door.snd
# DESC:   
# BBOX: -0.500004 -1.499938 -0.299921 0.500004 1.499938 0.299921
14bridge         _walkstruct       model3d=14brdg.3do size=1.659276 movesize=1.659276
# DESC:   
# BBOX: -0.100241 -0.301697 -0.045628 0.099795 0.297631 0.038858
c2x6e            _walkstruct       model3d=c2x6e.3do size=0.320280 movesize=0.320280
# DESC:   
# BBOX: -5.473804 -4.770837 -5.781102 5.473804 4.770837 5.781102
14tower_whole    _ghoststructure   model3d=towr.3do size=7.961624 movesize=7.961624
# DESC:   
# BBOX: -0.334721 -1.341961 -0.35292 0.319207 1.368196 0.319539
14bigpipe        _walkstruct       model3d=14bpip.3do size=1.465595 movesize=1.465595
# DESC:   
# BBOX: -0.158991 -0.163886 -0.224502 0.158991 0.163886 0.224502
rock2mini        _walkstruct       thingflags=0x48 model3d=rck2mini.3do size=0.369958 movesize=0.369958
# DESC:   
# BBOX: -0.01341 -0.149802 -0.01341 0.01341 0.015603 0.01341
+turretlaser3    +turretlaser      vel=(0.000000/4.000000/0.000000) damage=40.000000 mindamage=20.000000
# DESC:   Turret Gun Larger
# BBOX: -0.124322 -0.166491 -0.102246 0.124322 0.167917 0.079419
boxturret4       boxturret         model3d=tur4.3do size=0.200000 movesize=0.200000 cog=class_boxturret4.cog weapon=+turretlaser3 health=150.00 maxhealth=150.00 fireoffset=(0.000000/-0.043000/0.036000) aiclass=turbbdefault.ai
# DESC:   
# BBOX: -0.317982 -0.327771 -0.449003 0.317982 0.327771 0.449003
rock2            _walkstruct       thingflags=0x48 model3d=rck2.3do size=0.689916 movesize=0.689916
# DESC:   
# BBOX: 0 0 0 0 0 0
_throwable       none              orient=(0.000000/0.000000/0.000000) type=debris collide=1 move=physics movesize=0.010000 surfdrag=3.000000 airdrag=1.000000 mass=25.000000 height=0.011000 physflags=0x404041 buoyancy=0.500000
# DESC:   
# BBOX: -0.063042 -0.062892 -0.063041 0.063042 0.062892 0.063041
throwrock3       _throwable        model3d=rcktoss3.3do size=0.062900 movesize=0.062900 height=0.063000
# DESC:   
# BBOX: -25.1632 -2.863825 -8.585076 22.07636 3.652758 3.897422
cargoship        _ghoststructure   model3d=cargo.3do size=25.332577 movesize=25.332577
# DESC:   
# BBOX: -0.15 -1 -0.012306 0.15 1 0.012306
3x20catwalk      _walkstruct       model3d=c3x20.3do size=1.061262 movesize=1.061262
# DESC:   
# BBOX: -0.199801 -2.500005 -0.049881 0.199801 2.499985 0.049881
15c4x50          _walkstruct       model3d=15c50x4.3do size=2.558472 movesize=2.558472
# DESC:   
# BBOX: -1.462497 -1.455023 -0.149063 1.462497 1.455023 0.149063
landpad          _walkstruct       model3d=landpad.3do size=1.650857 movesize=1.650857
# DESC:   
# BBOX: -0.199801 -2.369914 -0.049881 0.199801 2.365105 0.049881
15c4x47          _walkstruct       model3d=15c47x4.3do size=2.428844 movesize=2.428844
# DESC:   
# BBOX: -0.036543 -0.036456 -0.036542 0.036543 0.036456 0.036542
throwcrate1      _throwable        model3d=crttoss1.3do size=0.036400 movesize=0.036400 height=0.036500
# DESC:   
# BBOX: -0.00375 -0.1 -0.052165 0.00375 0.1 0.052165
15_2x1rails      _walkstruct       model3d=15r2x1.3do size=0.162851 movesize=0.162851
# DESC:   Force Mana Boost Pickup
# BBOX: -0.020759 -0.01902 -0.030703 0.020759 0.01902 0.030703
manaboost        _powerup          thingflags=0x400 model3d=dark.3do cog=pow_mana.cog
# DESC:   DarkJedi Maw
# BBOX: -0.04680417 -0.07297424 -0.04086271 0.04567215 0.0882907 0.06189587
maw              _flyactor         model3d=ma.3do size=0.063100 movesize=0.063100 puppet=ma.pup soundclass=ma.snd maxvel=0.200000 weapon=+gamaxe health=900.00 maxhealth=900.00 maxthrust=1.50 maxrotthrust=200.00 typeflags=0x2000201 error=0.35 fov=0.40 chance=1.00 aiclass=maw.ai
# DESC:   DarkSide Force Boost Pickup
# BBOX: -0.03092 -0.030162 -0.030457 0.03092 0.030162 0.030457
darkside         _powerup          thingflags=0x400 model3d=mana1.3do cog=pow_darkside.cog respawn=60.000000
# DESC:   LightSide Force Boost Pickup
# BBOX: -0.024476 -0.030289 7.4E-5 0.02429 0.030289 0.060652
lightside        _powerup          thingflags=0x400 model3d=lite.3do cog=pow_lightside.cog respawn=60.000000
# DESC:   
# BBOX: -0.026724 -0.026724 -0.044654 0.026724 0.026724 0.044654
throwshrapnel_1  _throwable        model3d=shrp_1.3do size=0.045000 movesize=0.045000 height=0.045100
# DESC:   
# BBOX: -0.011171 -0.010389 -0.042003 0.011171 0.010389 0.042003
throwshrapnel_2  _throwable        model3d=shrp_2.3do size=0.040000 movesize=0.040000 height=0.040100
# DESC:   
# BBOX: -0.015988 -0.015443 -0.026188 0.015988 0.015443 0.026188
throwshrapnel_3  _throwable        model3d=shrp_3.3do size=0.028000 movesize=0.028000 height=0.028100
# DESC:   
# BBOX: -0.016213 -0.014647 -0.020855 0.016213 0.014647 0.020855
throwshrapnel_4  _throwable        model3d=shrp_4.3do size=0.026000 movesize=0.026000 height=0.026100
# DESC:   
# BBOX: -0.026804 -0.036274 -0.074092 0.026804 0.036274 0.074092
throwshrapnel_5  _throwable        model3d=shrp_5.3do size=0.084000 movesize=0.084000 height=0.084100
# DESC:   
# BBOX: -0.4 -0.050015 -0.398 0.4 0.050015 0.4
16_8x8door       _walkstruct       model3d=d8x8.3do size=0.617892 movesize=0.617892 soundclass=lg_door.snd
# DESC:   
# BBOX: -45.0029 -15.63616 -38.87886 45.0029 -1.02123 38.87887
16ground         _walkstruct       model3d=16grnd.3do size=59.529995 movesize=59.529995
# DESC:   
# BBOX: -0.544736 -0.786304 -0.229997 0.544736 0.786304 0.229997
16moldycrow      _decor            model3d=crow.3do size=0.800000 movesize=0.800000
# DESC:   
# BBOX: -0.273491 -0.232794 -0.725108 0.279523 0.232794 2.487975
dockingclamp     _ghostdecor       model3d=clmp.3do size=0.010000 movesize=0.010000
# DESC:   C3PO Droid 
# BBOX: -0.030754 -0.015261 -0.124016 0.030821 0.02207 0.067788
seethreepio      _droidactor       thingflags=0x20000400 model3d=c3.3do size=0.067800 movesize=0.067800 puppet=c3.pup soundclass=c3po.snd cog=actor_c3po.cog mass=120.000000 maxvel=0.400000 health=50.00 maxhealth=50.00 maxthrust=0.30 maxrotthrust=50.00 typeflags=0x180140 aiclass=drdefault.ai
# DESC:   
# BBOX: -0.05920881 -0.0562865 -0.09178185 0.08356237 0.0621335 0.04680145
powerdroid45     _droidactor       thingflags=0x20000400 model3d=pd45.3do size=0.080000 movesize=0.080000 puppet=pd45.pup soundclass=gonk.snd cog=actor_go.cog mass=180.000000 health=50.00 maxhealth=50.00 maxthrust=0.40 maxrotthrust=50.00 typeflags=0x180140 aiclass=drdefault.ai
# DESC:   
# BBOX: -0.050887 -0.075508 -0.16702 0.152168 0.25464 0.131736
16crane_1        _decor            thingflags=0x400 model3d=cran45_1.3do size=0.075000 movesize=0.075000 cog=xcrane.cog
# DESC:   
# BBOX: -0.298916 -0.298733 -0.14985 0.29859 0.29879 0.14985
00crate6x6       _walkstruct       model3d=00crte6x6.3do size=0.440112 movesize=0.440112
# DESC:   
# BBOX: -0.016314 -0.219705 -0.118596 0.016314 0.223171 0.249539
16fan0           none              orient=(0.000000/0.000000/0.000000) type=cog collide=3 move=physics model3d=fan0.3do size=0.299843 movesize=0.299843 physflags=0x200 maxrotvel=480.000000 angvel=(400.000000/0.000000/0.000000)
# DESC:   
# BBOX: 0 0 0 0 0 0
16_exp           +grenade_exp      damage=0.000000
# DESC:   DarkJedi Sariss
# BBOX: -0.027434 -0.017686 -0.118967 0.027531 0.017798 0.065257
sariss           _darkjedi         model3d=sa.3do size=0.064450 movesize=0.064450 puppet=sa.pup soundclass=sa.snd mass=140.000000 maxvel=0.200000 weapon=+gamaxe health=1900.00 maxhealth=1900.00 maxrotthrust=220.00 jumpspeed=1.60 typeflags=0x2008201 fov=0.40 chance=1.00 aiclass=sariss.ai
# DESC:   DarkJedi Yun Dead
# BBOX: -0.089696 -0.129795 -0.016968 0.089696 0.129795 0.016968
yundead          _decor            model3d=yundead.3do size=0.020000 movesize=0.020000
# DESC:   DarkJedi Yun Stronger Health
# BBOX: -0.031992 -0.017003 -0.134424 0.032032 0.024487 0.073659
yun2             yun               soundclass=yu2.snd health=1000.00 maxhealth=1000.00 maxrotthrust=220.00 error=0.30 fov=0.40 aiclass=yunds.ai
# DESC:   
# BBOX: -0.4 -0.4 -0.05 0.4 0.4 0.05
18platform       _walkstruct       model3d=8x8ev.3do size=0.617891 movesize=0.617891 soundclass=lg_elev.snd
# DESC:   
# BBOX: -0.248528 -0.3 -0.025 0.248528 0.3 0.025
18wedgedoor      _walkstruct       model3d=18wedge.3do size=0.440373 movesize=0.440373
# DESC:   Dynamic Light 3.0
# BBOX: 0 0 0 0 0 0
light3.0         _ghostdecor       thingflags=0x1 light=3.000000
# DESC:   
# BBOX: -0.1 -0.5 -0.012306 0.1 0.5 0.012306
c2x10e           _walkstruct       model3d=c2x10e.3do size=0.510050 movesize=0.510050 soundclass=med_elev.snd
# DESC:   
# BBOX: -0.565528 -0.327771 -0.333229 0.565528 0.327771 0.333229
rock1            _walkstruct       thingflags=0x48 model3d=rck1.3do size=0.783239 movesize=0.783239
# DESC:   
# BBOX: -0.034673 -0.03459 -0.034673 0.034673 0.03459 0.034673
throwrock2       _throwable        model3d=rcktoss2.3do size=0.034600 movesize=0.034600 height=0.034700
# DESC:   
# BBOX: -0.0547 -0.05457 -0.0547 0.0547 0.05457 0.0547
throwrock4       _throwable        model3d=rcktoss4.3do size=0.054500 movesize=0.054500 height=0.054600
# DESC:   AT-AT Legs
# BBOX: -0.605016 -0.480794 -0.833671 0.605016 0.480794 0.833671
atatlegs         _zwalkstruct      model3d=aalegs.3do size=1.099075 movesize=1.099075
# DESC:   AT-AT Body
# BBOX: -0.380577 -1.359003 -0.458991 0.380577 1.359003 0.458991
atatbody         _zwalkstruct      model3d=aabody.3do size=1.451033 movesize=1.451033
# DESC:   
# BBOX: -0.15 -0.15 -0.150003 0.15 0.15 0.150003
18crate3x3_1     _walkstruct       timer=25.000001 model3d=crt4-4.3do size=0.259810 movesize=0.259810
# DESC:   
# BBOX: -0.150124 -0.150615 -0.150124 0.149876 0.149385 0.149872
18crate3x3_2     _walkstruct       timer=25.000001 model3d=crt4-5.3do size=0.260306 movesize=0.260306
# DESC:   
# BBOX: -0.1 -0.1 -0.099985 0.1 0.1 0.099985
18crate2x2       _walkstruct       timer=25.000001 model3d=crt4-3.3do size=0.173196 movesize=0.173196
# DESC:   
# BBOX: 0 0 0 0 0 0
+upenergyhi      none              orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=0.600000 vel=(0.000000/0.000000/2.000000) angvel=(0.000000/360.000000/0.000000) typeflags=0x3e material=embers.mat range=0.400000 elementsize=0.007500 count=30
# DESC:   Elevator 2x2 w three legs
# BBOX: -0.098284 -0.097923 -0.031081 0.097733 0.098093 0.031081
2x2elev_2        _walkstruct       model3d=e2x2_0.3do size=0.192295 movesize=0.192295 soundclass=sm_elev.snd
# DESC:   Door fake not meant to be moved
# BBOX: -0.4 -0.25 -0.050031 0 0.25 0
d8x5_a           _walkstruct       model3d=d8x5_a.3do size=0.474345 movesize=0.474345 soundclass=lg_door.snd
# DESC:   Door fake not meant to be moved
# BBOX: 0 -0.25 -0.050031 0.4 0.25 0
d8x5_b           _walkstruct       model3d=d8x5_b.3do size=0.474345 movesize=0.474345 soundclass=lg_door.snd
# DESC:   
# BBOX: -0.15 -0.4 -0.012306 0.15 0.4 0.012306
19c3x8e          _zwalkstruct      model3d=c3x8e.3do size=0.477378 movesize=0.477378 soundclass=med_elev.snd
# DESC:   
# BBOX: -0.00929 -0.00375 -0.04177 0.00929 0.00375 0.04177
controlroomkey   _powerup          thingflags=0x408 model3d=dkey.3do cog=pow_ckey.cog
# DESC:   
# BBOX: -0.2 -0.349883 -0.275 0.2 0.349883 0.275
payload_a        _walkstruct       model3d=payloada.3do size=0.487897 movesize=0.010000 soundclass=payload.snd
# DESC:   
# BBOX: -0.2 -0.349883 -0.275 0.2 0.349883 0.275
payload_b        _walkstruct       model3d=payloadb.3do size=0.487897 movesize=0.010000
# DESC:   
# BBOX: -0.2 -0.349883 -0.275 0.2 0.349883 0.275
payload_c        _walkstruct       model3d=payloadc.3do size=0.487897 movesize=0.010000
# DESC:   
# BBOX: -0.2 -0.349883 -0.275 0.2 0.349883 0.275
payload_d        _walkstruct       model3d=payloadd.3do size=0.487897 movesize=0.010000
# DESC:   Door fake not meant to be moved
# BBOX: -0.4 -0.25 0 0.4 0.25 0
d8x5fake         _walkstruct       model3d=d8x5fake.3do size=0.471699 movesize=0.471699
# DESC:   
# BBOX: -0.100779 -0.107544 0.000686 0.100779 0.107544 0.023173
ceilinglight2    _ghoststructure   model3d=clight0.3do size=0.166487 movesize=0.166487
# DESC:   
# BBOX: -0.801732 -0.799971 -0.500413 0.801732 0.79997 0.500413
drill            _structure        model3d=drill.3do size=0.010000 movesize=0.010000
# DESC:   
# BBOX: -0.3 -0.57 -0.3 0.3 0.57 0.3
weight_1         _walkstruct       thingflags=0x48 model3d=wght01.3do size=0.760563 movesize=0.760563 soundclass=19counterweight.snd
# DESC:   
# BBOX: -0.01264 -0.999708 -0.01129 0.01264 0.999708 0.01129
rope03           _structure        thingflags=0xc model3d=rope03.3do size=1.049851 movesize=1.049851
# DESC:   
# BBOX: -0.2125 -0.475 -0.25 0.2125 0.475 0.25
19stonedoor      _walkstruct       thingflags=0x48 model3d=19stndoor.3do size=0.627305 movesize=0.627305 soundclass=19stone_door.snd
# DESC:   
# BBOX: -0.1 0 -0.1 0.1 0 0.1
19handswitch     _structure        model3d=19hands.3do size=0.191421 movesize=0.191421
# DESC:   
# BBOX: -0.1616239 -0.2391249 -0.45412 0.1605707 0.2508638 0.2235963
jatst            atst              thingflags=0x404 cog=actor_jatst.cog
# DESC:   
# BBOX: -0.921603 -1.937083 -1.696014 2.015026 1.014213 1.696014
bust5_2x         _ghoststructure   model3d=bust5_2x.3do size=3.170455 movesize=3.170455
# DESC:   
# BBOX: -0.051662 -0.098584 -0.019834 0.082951 0.078536 0.031738
s2carcass        _ghostdecor       model3d=s2eaten.3do size=0.163383 movesize=0.163383
# DESC:   
# BBOX: -0.054992 -0.082775 -0.019738 0.085887 0.072311 0.018145
stcarcass        _ghostdecor       model3d=steaten.3do size=0.162293 movesize=0.162293
# DESC:   
# BBOX: -0.017157 -0.016142 -0.014361 0.017157 0.016142 0.014361
throwhelmet      _throwable        model3d=stgib1.3do size=0.014700 movesize=0.014700 height=0.014800
# DESC:   
# BBOX: -0.034756 -0.02133 -0.007677 0.034756 0.02133 0.007677
stgib3           _ghostdecor       model3d=stgib3.3do size=0.088910 movesize=0.088910
# DESC:   
# BBOX: -0.049407 -0.030527 -0.009205 0.049407 0.030527 0.009205
stgib2           _ghostdecor       model3d=stgib2.3do size=0.103988 movesize=0.103988
# DESC:   
# BBOX: -0.037893 -0.021815 -0.12043 0.03746 0.022321 0.066407
jake             icommando         cog=actor_jake.cog
# DESC:   Red key Dropped
# BBOX: -0.010023 -0.016964 -0.033841 0.010023 0.016964 0.033841
+dkeyred         _powerup          thingflags=0x400 model3d=r-key.3do cog=pow_keyred.cog height=0.033840 physflags=0x41 typeflags=0x0
# DESC:   
# BBOX: -1.019287 -0.993732 -0.269209 1.019287 0.993732 0.269209
jedimound        _walkstruct       thingflags=0x48 model3d=jmnd.3do size=1.104239 movesize=1.104239
# DESC:   DarkJedi Boc
# BBOX: -0.06032984 -0.06567548 -0.1185037 0.041566 0.08251777 0.02764389
boc              _darkjedi         model3d=bo.3do size=0.060700 movesize=0.060700 puppet=bo.pup soundclass=bo.snd mass=160.000000 maxvel=0.200000 weapon=+gamaxe health=1500.00 maxhealth=1500.00 maxrotthrust=200.00 jumpspeed=1.60 error=0.25 fov=0.40 chance=1.00 aiclass=boc.ai
# DESC:   
# BBOX: -1.485445 -1.431855 -1.505791 1.396193 1.128748 1.505791
bust3_2x         _ghoststructure   model3d=bust3_2x.3do size=2.461241 movesize=2.461241
# DESC:   
# BBOX: -0.742723 -0.735947 -0.800791 0.698097 0.698463 0.800792
bust4            _ghoststructure   model3d=bust4.3do size=1.301096 movesize=1.301096
# DESC:   
# BBOX: -0.17849 -0.059063 -0.119858 0.17849 0.059063 0.119858
slab1            _walkstruct       thingflags=0x48 model3d=slab1.3do size=0.222965 movesize=0.222965
# DESC:   
# BBOX: -0.072557 -0.059063 -0.164788 0.072557 0.059063 0.164788
slab2            _walkstruct       thingflags=0x48 model3d=slab2.3do size=0.189494 movesize=0.189494
# DESC:   
# BBOX: -0.458867 -0.036619 -0.0973 0.458867 0.036619 0.0973
slab3            _walkstruct       thingflags=0x48 model3d=slab3.3do size=0.470497 movesize=0.470497
# DESC:   
# BBOX: -0.075265 -0.064442 -0.151527 0.075265 0.064442 0.151527
statue           _walkstruct       thingflags=0x48 model3d=stat.3do size=0.231047 movesize=0.231047
# DESC:   
# BBOX: -0.807034 -1.054058 -1.386616 0.755901 1.09021 1.386616
bust2_2x         _ghoststructure   model3d=bust2_2x.3do size=1.850532 movesize=1.850532
# DESC:   
# BBOX: -1.184637 -0.813483 -0.761018 0.715234 0.683786 0.761018
bust2a           _ghoststructure   model3d=bust2a.3do size=1.525402 movesize=1.525402
# DESC:   
# BBOX: -0.742723 -0.715928 -0.752895 0.698097 0.564374 0.752895
bust3            _ghoststructure   model3d=bust3.3do size=1.255621 movesize=1.255621
# DESC:   Jan Orrs
# BBOX: -0.035332 -0.013185 -0.120103 0.035579 0.016413 0.064509
jan              _humanactor       model3d=ja.3do puppet=ja.pup soundclass=wm1.snd maxvel=0.400000 health=20000.00 maxhealth=20000.00 maxthrust=1.00 maxrotthrust=60.00 aiclass=jan.ai
# DESC:   DarkJedi Jerec
# BBOX: -0.031436 -0.018399 -0.121814 0.032728 0.01913 0.065791
jerec            _darkjedi         model3d=je.3do size=0.063000 movesize=0.063000 puppet=je.pup soundclass=je.snd mass=170.000000 maxvel=0.200000 weapon=+gamaxe health=2000.00 maxhealth=2000.00 maxthrust=1.00 maxrotthrust=200.00 jumpspeed=4.00 typeflags=0x2008201 error=0.10 fov=-0.40 chance=1.00 aiclass=jerec.ai
# DESC:   
# BBOX: -0.198498 -0.173539 0.001103 0.198498 0.181053 1.528715
statpole         _structure        model3d=statpole.3do size=1.581753 movesize=1.581753 soundclass=19stone_door.snd
# DESC:   
# BBOX: -0.1 0 -0.1 0.1 0 0.1
21handswitch     _structure        model3d=21hands.3do size=0.191421 movesize=0.191421 soundclass=19stone_door.snd
# DESC:   HealthPack Respawns Single Player
# BBOX: -0.027845 -0.025398 -0.02252 0.027845 0.025398 0.024913
rhealth          healthpack        typeflags=0x2
# DESC:   Bacta Tank Respawns Single Player
# BBOX: -0.015415 -0.017797 -0.028888 0.015415 0.017797 0.028888
rbacta           bactatank         typeflags=0x2 respawn=30.000000
# DESC:   Mana Boost Respawns Single Player
# BBOX: -0.03092 -0.030162 -0.030457 0.03092 0.030162 0.030457
rmanaboost       manaboost         model3d=mana1.3do typeflags=0x2
# DESC:   Light Side Boost Respawns Single Player
# BBOX: -0.024476 -0.030289 7.4E-5 0.02429 0.030289 0.060652
rlsurge          lightside         typeflags=0x2 respawn=30.000000
# DESC:   
# BBOX: 0 0 0 0 0 0
+coreflow        none              orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=3.000000 vel=(0.000000/0.000000/1.000000) angvel=(0.000000/540.000000/0.000000) typeflags=0x3e material=embers.mat range=0.200000 elementsize=0.005000 count=10 yawrange=90.000000
# DESC:   
# BBOX: 0 0 0 0 0 0
+smallcoreflow   none              orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=3.000000 vel=(0.000000/0.000000/1.000000) angvel=(0.000000/540.000000/0.000000) typeflags=0x3e material=embers.mat range=0.100000 elementsize=0.005000 count=5 yawrange=90.000000
# DESC:   Revive Respawns Single Player
# BBOX: -0.021089 -0.023005 -0.035347 0.016307 0.023005 0.035347
rrevivepack      _powerup          thingflags=0x400 model3d=packrev.3do cog=pow_revive.cog typeflags=0x2 respawn=60.000000
# DESC:   
# BBOX: -0.1 -0.1 -0.45 0.1 0.1 0.45
m10column        _structure        model3d=m10colmn.3do size=0.521699 movesize=0.521699
# DESC:   
# BBOX: -0.25 -0.05 -0.2 0.25 0.05 0.2
m2heater         _walkstruct       thingflags=0x4c model3d=m2heat.3do size=0.324037 movesize=0.324037
# DESC:   
# BBOX: -0.059999 -0.051358 -0.040791 0.060835 0.061748 0.050725
m2boxturret      _zwalkstruct      model3d=tur1.3do size=0.139457 movesize=0.139457 soundclass=med_elev.snd
# DESC:   
# BBOX: -0.036543 -0.036456 -0.036542 0.036543 0.036456 0.036542
throwcrate2      _throwable        model3d=crttoss2.3do size=0.036400 movesize=0.036400 height=0.036500
# DESC:   
# BBOX: -0.030211 -0.406862 -0.219622 0.030211 0.41328 0.46211
m5_fan0_moving   none              orient=(0.000000/0.000000/0.000000) type=cog collide=3 move=physics model3d=fan0big.3do size=0.512672 movesize=0.512672 physflags=0x200 maxrotvel=480.000000 angvel=(400.000000/0.000000/0.000000)
# DESC:   
# BBOX: -0.018484 -0.070714 -0.030025 0.019883 0.081826 0.013227
m5railgun        railgun           typeflags=0x9
# DESC:   
# BBOX: -0.030347 -0.072704 -0.024787 0.009211 0.072704 0.024341
m5concrifle      concrifle         typeflags=0x9
# DESC:   
# BBOX: -0.099008 -0.068267 -0.014304 0.099008 0.129749 0.009657
2x2elev_0        _walkstruct       model3d=19e2x2.3do size=0.213495 movesize=0.213495 soundclass=sm_elev.snd
# DESC:   
# BBOX: -0.198 -0.198 -0.198 0.198 0.198 0.198
c1_4x4llift      _walkstruct       thingflags=0x400049 light=0.600000 model3d=l4x4_2.3do size=0.392946 movesize=0.392946 soundclass=med_elev.snd
# DESC:   
# BBOX: -0.017376 -0.026884 -0.017659 0.017376 0.026884 0.017659
ctf_redflag      _powerup          move=none model3d=flgr.3do size=0.050000 movesize=0.050000 puppet=cr.pup typeflags=0x8
# DESC:   
# BBOX: -0.017376 -0.027858 -0.017659 0.017376 0.027858 0.017659
ctf_goldflag     _powerup          move=none model3d=flgy.3do size=0.050000 movesize=0.050000 puppet=cr.pup typeflags=0x8
# DESC:   
# BBOX: -0.198 -0.198 -0.198 0.198 0.198 0.198
c3_4x4llift      _walkstruct       thingflags=0x400049 light=0.600000 model3d=l4x4.3do size=0.392946 movesize=0.392946 soundclass=med_elev.snd
# DESC:   
# BBOX: -0.15 -0.4 -0.012306 0.15 0.4 0.012306
c2_catwalk       _walkstruct       model3d=01c3x8.3do size=0.427378 movesize=0.427378
# DESC:   
# BBOX: 0 0 0 0 0 0
_shard           none              orient=(0.000000/0.000000/0.000000) type=cog move=physics mass=0.050000 physflags=0x221
# DESC:   
# BBOX: -0.03133 -1E-6 -0.038594 0.031989 1E-6 0.027215
shard00          _shard            timer=1.000000 model3d=shard0.3do size=0.042000 movesize=0.042000
# DESC:   
# BBOX: -0.187173 -0.171934 -0.041875 0.196141 0.200354 0.037814
c3_fan           _decor            move=physics model3d=fan1.3do size=0.250944 movesize=0.250944 angvel=(0.000000/360.000000/0.000000)
# DESC:   
# BBOX: -0.15 -0.011875 -0.15 0.15 0.011875 0.15
c3_3x3door_1     _structure        model3d=d3x3_1.3do size=0.262464 movesize=0.262464 soundclass=sm_door.snd
# DESC:   
# BBOX: -0.016314 -0.219705 -0.118596 0.016314 0.223171 0.249539
c3_fan0_moving   none              orient=(0.000000/0.000000/0.000000) type=cog move=physics model3d=fan0.3do size=0.299843 movesize=0.299843 physflags=0x200 maxrotvel=480.000000 angvel=(400.000000/0.000000/0.000000)
# DESC:   
# BBOX: -0.059999 -0.051358 -0.040791 0.060835 0.061748 0.050725
c3_boxturret     _zwalkstruct      model3d=tur1.3do size=0.100000 movesize=0.100000
# DESC:   
# BBOX: -0.010023 -0.016964 -0.033841 0.010023 0.016964 0.033841
ctf_redkey       none              orient=(0.000000/0.000000/0.000000) type=item collide=1 thingflags=0x401 light=0.750000 model3d=r-key.3do size=0.088183 movesize=0.088183 cog=ctf_keyred.cog typeflags=0x8
# DESC:   
# BBOX: -0.010023 -0.016964 -0.033841 0.010023 0.016964 0.033841
ctf_goldkey      none              orient=(0.000000/0.000000/0.000000) type=item collide=1 thingflags=0x401 light=0.750000 model3d=y-key.3do size=0.100000 movesize=0.100000 cog=ctf_keygold.cog typeflags=0x8
# DESC:   
# BBOX: -0.15 -0.4 -0.012306 0.15 0.4 0.012306
c3_hdoor         _walkstruct       model3d=01c3x8.3do size=0.427378 movesize=0.427378
# DESC:   
# BBOX: -0.148 -0.148 -0.013055 0.148 0.148 0.013695
ctf_3x3elev      _walkstruct       model3d=ctf_e3x3.3do size=0.259751 movesize=0.259751 soundclass=sm_elev.snd
# DESC:   
# BBOX: -0.15 -0.4 -0.012306 0.15 0.4 0.012306
01cat3x8_2       _walkstruct       model3d=01c3x8.3do size=0.427378 movesize=0.427378
# DESC:   Catwalk 2x6 triangular shaped bottom
# BBOX: -0.100241 -0.301697 -0.045628 0.099795 0.297631 0.038858
2x6catwalk       _walkstruct       model3d=c2x6.3do size=0.320280 movesize=0.320280
# DESC:   catwalk 2x8
# BBOX: -0.1 -0.399762 -0.024738 0.1 0.399762 0.024738
2x8catwalk       _walkstruct       model3d=c2x8.3do size=0.412822 movesize=0.412822
# DESC:   Elevator 2x4 w 2 legs
# BBOX: -0.09999 -0.199962 -0.031081 0.09999 0.199962 0.031081
2x4elev          _walkstruct       model3d=e2x4.3do size=0.225719 movesize=0.225719 soundclass=med_elev.snd
# DESC:   Elevator 4x4 w one end not textured and 3 legs
# BBOX: -0.198 -0.198002 -0.05971 0.198 0.198002 0.05971
4x4elev          _walkstruct       model3d=e4x4.3do size=0.336311 movesize=0.336311 soundclass=med_elev.snd
# DESC:   Elevator 4x4 same as 4x4elev
# BBOX: -0.198 -0.198002 -0.05971 0.198 0.198002 0.05971
4x4elev_1        _walkstruct       model3d=e4x4_1.3do size=0.336311 movesize=0.336311 soundclass=med_elev.snd
# DESC:   Elevator 4x4 w two legs
# BBOX: -0.200716 -0.195991 -0.079681 0.195304 0.200102 0.079681
4x4elev_2        _walkstruct       model3d=e4x4_2.3do size=0.336311 movesize=0.336311 soundclass=med_elev.snd
# DESC:   Elevator same as 4x4elev_2 but w no legs 
# BBOX: -0.200716 -0.195991 0.038027 0.195304 0.200102 0.079681
4x4ingarvator    _zwalkstruct      model3d=e4x4_2i.3do size=0.344409 movesize=0.344409 soundclass=med_elev.snd
# DESC:   Elevator Complete w sides and a light in ceiling
# BBOX: -0.198 -0.198 -0.198 0.198 0.198 0.198
4x4lift          _walkstruct       model3d=l4x4.3do size=0.392946 movesize=0.392946 soundclass=med_elev.snd
# DESC:   Elevator 4x4 complete with sides. 
# BBOX: -0.198 -0.198 -0.198 0.198 0.198 0.198
4x4lift_2        _walkstruct       model3d=l4x4_2.3do size=0.392946 movesize=0.392946 soundclass=med_elev.snd
# DESC:   Door for 4x4Lift Elevator  complete with sides.  
# BBOX: -0.199997 -0.013245 -0.200025 0.199997 0.013245 0.200025
4x4liftdoor      _walkstruct       model3d=ld4x4.3do size=0.283168 movesize=0.283168 soundclass=lg_elev.snd
# DESC:   Elevator 6x6 with bottom sides ,use in a hole 
# BBOX: -0.297299 -0.296497 -1.049955 0.297299 0.297249 1.049955
6x6elev_1        _walkstruct       model3d=e6x6_1.3do size=1.180995 movesize=1.180995 soundclass=lg_elev.snd
# DESC:   Elevator 8x4 w 3 legs
# BBOX: -0.40002 -0.200046 -0.079681 0.40002 0.200046 0.079681
8x4elev          _walkstruct       model3d=e8x4.3do size=0.454295 movesize=0.454295 soundclass=lg_elev.snd
# DESC:   Elevator 8x8 no legs
# BBOX: -0.40002 -0.400093 -0.024993 0.40002 0.400093 0.024993
8x8elev          _walkstruct       model3d=e8x8.3do size=0.616317 movesize=0.616317 soundclass=lg_elev.snd
# DESC:   Door 30x10 Single  Drk Grey w yellow warn on Ends
# BBOX: 0 -0.499 -0.069717 3 0.499 0
baydoor          _walkstruct       model3d=bayd.3do size=3.092016 movesize=3.092016 soundclass=lg_door.snd
# DESC:   Door 15x10 1/2 of BayDoor type
# BBOX: -0.75 -0.499 -0.034859 0.75 0.499 0.034859
baydoor_a        _walkstruct       model3d=bayd_a.3do size=0.951508 movesize=0.951508
# DESC:   Door 15x10 1/2 of BayDoor type door
# BBOX: -0.75 -0.499 -0.034859 0.75 0.499 0.034859
baydoor_b        _walkstruct       model3d=bayd_b.3do size=0.951508 movesize=0.951508
