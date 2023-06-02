# DESC:    Class Actor
# BBOX: 0 0 0 0 0 0
_actor            none               orient=(0.000000/0.000000/0.000000) type=actor collide=1 move=physics thingflags=0x20000000 mass=150.000000 physflags=0x4a4f maxrotvel=200.000000 maxvel=1.000000 health=40.00 maxhealth=40.00 maxrotthrust=180.00 jumpspeed=1.50 eyeoffset=(0.000000/0.000000/0.037000) minheadpitch=-80.00 maxheadpitch=80.00 lightoffset=(0.000000/0.070000/0.040000) lightintensity=0.80
# DESC:    Player
# BBOX: -.037301 -.013874 -.118461 .038114 .03984 .064544
walkplayer        _actor             type=player thingflags=0x20000401 light=0.200000 model3d=ky.3do size=0.065000 movesize=0.065000 puppet=ky.pup soundclass=ky.snd cog=kyle.cog surfdrag=3.000000 airdrag=0.500000 staticdrag=0.300000 health=100.00 maxhealth=100.00 maxthrust=2.00 typeflags=0x1 error=0.50 fov=0.71 chance=1.00
# DESC:    Class Decor
# BBOX: 0 0 0 0 0 0
_decor            none               orient=(0.000000/0.000000/0.000000) type=cog collide=1 move=path
# DESC:    
# BBOX: 0 0 0 0 0 0
_structure        _decor             collide=3 thingflags=0x8
# DESC:    
# BBOX: 0 0 0 0 0 0
_walkstruct       _structure         thingflags=0x400048
# DESC:    
# BBOX: 0 0 0 0 0 0
_humanactor       _actor             size=0.065000 movesize=0.065000 surfdrag=3.000000 airdrag=0.500000 maxvel=0.500000 maxthrust=0.80 typeflags=0x80000
# DESC:    
# BBOX: 0 0 0 0 0 0
_weapon           none               orient=(0.000000/0.000000/0.000000) type=weapon collide=1 move=physics thingflags=0x20000000 timer=10.000000 mass=5.000000 physflags=0x200 maxrotvel=90.000000 damageclass=0x2 typeflags=0x1
# DESC:    
# BBOX: 0 0 0 0 0 0
_explosion        none               orient=(0.000000/0.000000/0.000000) type=explosion typeflags=0x1 damageclass=0x4
# DESC:    
# BBOX: 0 0 0 0 0 0
+laserhit         _explosion         thingflags=0x1 light=0.200000 timer=0.500000 sprite=bryx.spr soundclass=exp_laserhit.snd typeflags=0x33 blasttime=0.300000 maxlight=0.400000
# DESC:    
# BBOX: 0 0 0 0 0 0
+smflash          _explosion         thingflags=0x1 light=0.400000 timer=0.100000 typeflags=0x0
# DESC:    
# BBOX: 0 0 0 0 0 0
+laserfleshhit    +laserhit          soundclass=exp_fleshy.snd
# DESC:    Bryar Pistol
# BBOX: -.007791 -.090587 -.006258 .007791 .017998 .006258
+bryarbolt        _weapon            thingflags=0x20000001 light=0.400000 model3d=bry0.3do size=0.001000 movesize=0.001000 soundclass=bry.snd creatething=+smflash maxrotvel=0.000000 vel=(0.000000/4.000000/0.000000) explode=+laserhit fleshhit=+laserfleshhit damage=30.000000 mindamage=10.000000 typeflags=0x20440d rate=15.000000
# DESC:    Storm Trooper Rifle
# BBOX: -.005653 -.113701 -.004853 .005653 .02259 .004853
+stlaser          +bryarbolt         model3d=str0.3do soundclass=stlaser.snd vel=(0.000000/6.000000/0.000000) rate=10.000000
# DESC:    Enemy Rifle
# BBOX: -.005653 -.113701 -.004853 .005653 .02259 .004853
+elaser           +stlaser           vel=(0.000000/4.000000/0.000000) damage=12.000000 mindamage=5.000000
# DESC:    ReeYee Brown w St Rifle
# BBOX: -.046863 -.025046 -.131246 .045997 .027318 .073257
reeyeesgun        _humanactor        thingflags=0x20000400 model3d=rystr.3do size=0.072900 movesize=0.072900 puppet=rystr.pup soundclass=rystr.snd cog=actor_rb.cog weapon=+elaser health=50.00 maxhealth=50.00 maxthrust=0.90 maxrotthrust=70.00 jumpspeed=1.20 typeflags=0x1 fireoffset=(0.014700/0.080000/0.034000) aiclass=rydefault.ai
# DESC:    Door 2x3 Single Drk Grey w Round Bump
# BBOX: -.1 -.0125 -.15 .1 .0125 .15
2x3door           _structure         model3d=d2x3.3do size=0.180710 movesize=0.180710 soundclass=sm_door.snd
# DESC:    
# BBOX: -.15 -.5 -.012306 .15 .5 .012306
3x10catwalk       _walkstruct        model3d=c3x10.3do size=0.522160 movesize=0.522160
# DESC:    
# BBOX: -.1 -.1 -.099985 .1 .1 .099985
crate4_3          _walkstruct        model3d=crt4-3.3do size=0.173196 movesize=0.173196
# DESC:    
# BBOX: -.050887 -.075508 -.16702 .050887 .25464 .131736
crane             _decor             thingflags=0x400 model3d=cran.3do size=0.075000 movesize=0.075000 cog=xcrane.cog
# DESC:    
# BBOX: -.053401 -.061619 -.061702 .053401 .061619 .061702
crate6_2          _walkstruct        thingflags=0x400448 model3d=crt6-2.3do size=0.087201 movesize=0.087201 cog=actor_cr.cog
# DESC:    
# BBOX: -.038064 -.024511 -.05829 .038064 .024511 .05829
console2          _walkstruct        model3d=con2.3do size=0.073806 movesize=0.073806
# DESC:    
# BBOX: 0 0 0 0 0 0
_ghoststructure   _structure         collide=0
# DESC:    
# BBOX: -.192079 -.012546 -.187495 .192079 .012546 .187494
2x3jam            _ghoststructure    model3d=j2x3.3do size=0.268712 movesize=0.268712
# DESC:    
# BBOX: -.03 -.03 -.030002 .03 .03 .030002
crate4_1          _walkstruct        model3d=crt4-1.3do size=0.051962 movesize=0.051962
# DESC:    
# BBOX: -.05 -.05 -.049999 .05 .05 .049999
crate4_2          _walkstruct        model3d=crt4-2.3do size=0.086602 movesize=0.086602
# DESC:    
# BBOX: 0 0 0 0 0 0
+whitecloud       none               orient=(0.000000/0.000000/0.000000) type=particle timer=0.200000 typeflags=0x3f material=00gsmoke.mat range=0.020000 rate=128.000000 maxthrust=30.000000 elementsize=0.007000 count=128
# DESC:    
# BBOX: 0 0 0 0 0 0
+dustcloud        +whitecloud        timer=0.120000 material=dusty.mat range=0.015000 rate=256.000000 maxthrust=80.000000 elementsize=0.010000
# DESC:    
# BBOX: 0 0 0 0 0 0
+punchcloud       +dustcloud         timer=0.150000 material=00gsmoke.mat range=0.006000 rate=64.000000 maxthrust=4.000000 elementsize=0.004000 count=16
# DESC:    
# BBOX: 0 0 0 0 0 0
+punch_exp        _explosion         timer=0.001000 soundclass=exp_punch.snd creatething=+punchcloud typeflags=0x0
# DESC:    
# BBOX: 0 0 0 0 0 0
+punch            _weapon            size=0.001000 movesize=0.001000 mass=50.000000 explode=+punch_exp fleshhit=+punch_exp damage=20.000000 damageclass=0x1 typeflags=0x200d range=0.150000 force=50.000000
# DESC:    
# BBOX: 0 0 0 0 0 0
+ry_punch         +punch             damage=10.000000 force=25.000000
# DESC:    ReeYee Brown w Fists Only
# BBOX: -.046863 -.025046 -.131246 .045997 .027318 .073257
reeyeesfist       _humanactor        thingflags=0x20000400 model3d=ry.3do size=0.072900 movesize=0.072900 puppet=ryfist.pup soundclass=ryfist.snd cog=actor_rf.cog weapon=+ry_punch health=50.00 maxhealth=50.00 maxthrust=1.00 maxrotthrust=70.00 jumpspeed=1.20 typeflags=0x20001 aiclass=rypundefault.ai
# DESC:    
# BBOX: 0 0 0 0 0 0
_droidactor       _humanactor        typeflags=0x80140
# DESC:    Gonk Droid 
# BBOX: -.034 -.05906 -.093167 .034804 .05936 .052273
powerdroid        _droidactor        thingflags=0x20000400 model3d=pd.3do size=0.080000 movesize=0.080000 puppet=pd.pup soundclass=gonk.snd cog=actor_go.cog mass=180.000000 health=50.00 maxhealth=50.00 maxthrust=0.20 maxrotthrust=50.00 typeflags=0x180140 aiclass=gonk.ai
# DESC:    
# BBOX: 0 0 0 0 0 0
_powerup          none               orient=(0.000000/0.000000/0.000000) type=item collide=1 move=physics size=0.100000 movesize=0.010000 surfdrag=3.000000 airdrag=1.000000 mass=10.000000 height=0.050000 physflags=0x400000 angvel=(0.000000/90.000000/0.000000) typeflags=0x1 respawn=30.000000
# DESC:    Shield Pickup
# BBOX: -.040113 -.035949 -.021618 .040113 .035949 .021618
shieldrecharge    _powerup           thingflags=0x400 model3d=shld.3do cog=pow_shields.cog
# DESC:    Door 3x3 Single Drk Grey Warn Sign on Bottom
# BBOX: -.15 -.011875 -.15 .15 .011875 .15
3x3door_1         _walkstruct        model3d=d3x3_1.3do size=0.262464 movesize=0.262464 soundclass=sm_door.snd
# DESC:    
# BBOX: 0 0 0 0 0 0
_ghostdecor       _decor             collide=0
# DESC:    Dynamic Light 1.0
# BBOX: 0 0 0 0 0 0
light1.0          _ghostdecor        thingflags=0x1 light=1.000000
# DESC:    
# BBOX: -.817281 -1.132091 -.454926 .815909 1.276639 1.852229
ghostshuttle      _ghostdecor        model3d=shut.3do size=1.000000 movesize=1.000000
# DESC:    
# BBOX: -.187173 -.171934 -.041875 .196141 .200354 .037814
fan1              none               orient=(0.000000/0.000000/0.000000) type=cog collide=1 move=physics model3d=fan1.3do size=0.005000 movesize=0.005000 physflags=0x200 maxrotvel=90.000000 angvel=(0.000000/90.000000/0.000000)
# DESC:    
# BBOX: -.181678 -.060004 -.073819 .181678 .060004 .073819
roofvent3         _walkstruct        model3d=01rfd3.3do size=0.249375 movesize=0.249375
# DESC:    
# BBOX: -.13015 -.091058 -.100298 .13015 .091058 .100298
roofvent2         _walkstruct        model3d=01rfd2.3do size=0.226595 movesize=0.226595
# DESC:    
# BBOX: -.01459 -.075274 -.015287 .01459 .075274 .015287
roofpipe          _ghostdecor        model3d=01rfd0.3do size=0.077027 movesize=0.077027
# DESC:    Door 4x3 Single Drk Grey with Diagonal Slant 
# BBOX: -.2 -.0125 -.15 .2 .0125 .15
4x3door_1         _structure         model3d=d4x3_1.3do size=0.250312 movesize=0.250312 soundclass=med_door.snd
# DESC:    ReeYee Black w Fists Only
# BBOX: -.046863 -.025046 -.131246 .045997 .027318 .073257
reeyeesbhfist     reeyeesfist        model3d=rh.3do health=60.00 maxhealth=60.00 maxrotthrust=85.00
# DESC:    ReeYee Black w St Rifle
# BBOX: -.046863 -.025046 -.131246 .045997 .027318 .073257
reeyeesbhgun      reeyeesgun         model3d=rhstr.3do health=60.00 maxhealth=60.00 maxrotthrust=80.00 aiclass=rybdefault.ai
# DESC:    
# BBOX: -.027845 -.025398 -.02252 .027845 .025398 .024913
healthpack        _powerup           thingflags=0x408 model3d=hepk.3do cog=pow_health.cog
# DESC:    StormTroop Rifle Pickup
# BBOX: -.016822 -.05557 -.026043 .007966 .058 .014355
strifle           _powerup           thingflags=0x400 model3d=strp.3do cog=pow_strifle.cog
# DESC:    Elevator 2x2 w three legs underneath 
# BBOX: -.098284 -.097923 -.031081 .097733 .098093 .031081
2x2elev           _walkstruct        model3d=e2x2_0.3do size=0.192295 movesize=0.192295 soundclass=sm_elev.snd
# DESC:    
# BBOX: -.15 -.4 -.012306 .15 .4 .012306
3x8catwalk        _walkstruct        model3d=c3x8.3do size=0.427378 movesize=0.427378
# DESC:    EnergyCell Pickup  (pistol,rifle)
# BBOX: -.008536 -.014067 -.022418 .008536 .014068 .022418
energycell        _powerup           thingflags=0x400 model3d=ecel.3do cog=pow_energy.cog
# DESC:    Greedo Pistol Pickup
# BBOX: -.005081 -.030803 -.01143 .005081 .030803 .01143
greedopistol      _powerup           thingflags=0x400 model3d=ggun.3do cog=pow_bryar.cog
# DESC:    Enemy Bryar Pistol
# BBOX: -.007791 -.090587 -.006258 .007791 .017998 .006258
+ebolt            +bryarbolt         model3d=bry1.3do vel=(0.000000/3.500000/0.000000) damage=10.000000 mindamage=5.000000
# DESC:    Greedo
# BBOX: -.026466 -.012833 -.099682 .026261 .023106 .06583
greedo            _humanactor        thingflags=0x20000400 model3d=gr.3do size=0.061500 movesize=0.061500 puppet=gr.pup soundclass=gr.snd cog=actor_gr.cog mass=70.000000 maxvel=0.300000 weapon=+ebolt maxrotthrust=80.00 typeflags=0x2080001 fireoffset=(0.016000/0.055000/0.004000) aiclass=grdefault.ai
# DESC:    Bacta Tank Pickup 
# BBOX: -.015415 -.017797 -.028888 .015415 .017797 .028888
bactatank         _powerup           thingflags=0x400 model3d=bact.3do cog=pow_bacta.cog respawn=60.000000
# DESC:    
# BBOX: 0 0 0 0 0 0
_civilian         _humanactor        surfdrag=2.000000 mass=100.000000 maxvel=0.300000 maxthrust=2.00 maxrotthrust=90.00 typeflags=0x180000
# DESC:    Man 
# BBOX: -.03954 -.01907 -.133437 .039738 .026146 .061238
man2              _civilian          thingflags=0x20000400 model3d=m2.3do size=0.060000 movesize=0.060000 puppet=m2.pup soundclass=mn1.snd cog=actor_pm.cog maxvel=0.200000 health=50.00 maxhealth=50.00 maxthrust=0.50 maxrotthrust=60.00 aiclass=peddefault.ai
# DESC:    Man Narshadda
# BBOX: -.03954 -.01907 -.133437 .039738 .026146 .061238
nman2             man2               cog=actor_mn.cog aiclass=pednarsh.ai
# DESC:    
# BBOX: -.15 -.15 -.150003 .15 .15 .150003
crate4_4          _walkstruct        model3d=crt4-4.3do size=0.259810 movesize=0.259810
# DESC:    
# BBOX: -.027923 -.032221 -.032264 .027923 .032221 .032264
crate6_1          _walkstruct        thingflags=0x400448 model3d=crt6-1.3do size=0.045598 movesize=0.045598 cog=actor_cs.cog
# DESC:    Man 
# BBOX: -.035537 -.014648 -.128737 .035536 .025457 .065502
man1              _civilian          thingflags=0x20000400 model3d=mn.3do size=0.060000 movesize=0.060000 puppet=mn.pup soundclass=mn1.snd cog=actor_pm.cog maxvel=0.200000 health=50.00 maxhealth=50.00 maxthrust=0.50 maxrotthrust=60.00 aiclass=peddefault.ai
# DESC:    Man Narshadda
# BBOX: -.035537 -.014648 -.128737 .035536 .025457 .065502
nman1             man1               cog=actor_mn.cog aiclass=pednarsh.ai
# DESC:    Woman 
# BBOX: -.025123 -.01744 -.127094 .025123 .015562 .056718
woman1            _civilian          thingflags=0x20000400 model3d=wm.3do size=0.056000 movesize=0.056000 puppet=wm.pup soundclass=wm1.snd cog=actor_pf.cog maxvel=0.200000 health=50.00 maxhealth=50.00 maxthrust=0.50 maxrotthrust=60.00 aiclass=peddefault.ai
# DESC:    Woman Narshadda 
# BBOX: -.025123 -.01744 -.127094 .025123 .015562 .056718
nwoman1           woman1             cog=actor_fn.cog aiclass=pednarsh.ai
# DESC:    IR Goggles Pickup 
# BBOX: -.036535 -.022706 -.017295 .036535 .022706 .017295
goggles           _powerup           thingflags=0x400 model3d=gogl.3do cog=pow_goggles.cog respawn=60.000000
# DESC:    
# BBOX: -5.540396 -4.990839 -1.191934 5.533077 4.990839 3.900447
freighter         _ghoststructure    model3d=01frt.3do size=7.945067 movesize=7.945067
# DESC:    R5 Droid
# BBOX: -.033545 -.020533 -.064529 .03321 .020809 .02539
r5                _droidactor        thingflags=0x20000400 model3d=r5.3do size=0.036000 movesize=0.036000 puppet=r5.pup soundclass=r2.snd cog=actor_r5.cog maxvel=0.400000 health=50.00 maxhealth=50.00 maxthrust=0.40 maxrotthrust=50.00 typeflags=0x180140 aiclass=drdefault.ai
# DESC:    Smuggler Backpack Pickup
# BBOX: -.021089 -.023005 -.035347 .016307 .023005 .035347
smbp              _powerup           thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk.cog
# DESC:    
# BBOX: 0 0 0 0 0 0
_zwalkstruct      _structure         thingflags=0x400040
# DESC:    
# BBOX: -.549776 -.490931 -.315187 .549776 .4752 .320955
tiebomber         _zwalkstruct       model3d=tieb.3do size=0.500000 movesize=0.500000 puppet=cr.pup
# DESC:    Elevator 3x3 one end not textured 
# BBOX: -.148 -.148 -.013055 .148 .148 .013695
3x3elev           _walkstruct        model3d=e3x3.3do size=0.209751 movesize=0.209751 soundclass=sm_elev.snd
# DESC:    8t88 Droid
# BBOX: -.032005 -.012042 -.115282 .032144 .017313 .07605
8t88              _humanactor        model3d=8t.3do size=0.060000 movesize=0.060000 puppet=8t.pup soundclass=8t.snd mass=300.000000 health=10000.00 maxhealth=10000.00 maxrotthrust=70.00 typeflags=0x100 aiclass=default.ai
# DESC:    
# BBOX: -.063294 -.054836 -.040528 .063294 .054836 .040528
00table           _walkstruct        thingflags=0x48 model3d=00tabl.3do size=0.070420 movesize=0.070420
# DESC:    
# BBOX: -.039015 -.039769 -.200258 .039015 .039769 .200258
column01          _structure         model3d=01colm.3do size=0.204987 movesize=0.204987
# DESC:    Man Bartender
# BBOX: -.03954 -.01907 -.133437 .039738 .026146 .061238
nbartender        nman2              cog=actor_bt.cog
# DESC:    
# BBOX: -.076042 -.071135 -.052976 .062968 .083218 .054564
tuskensoused      _ghostdecor        move=none model3d=thout.3do size=0.227490 movesize=0.227490 puppet=cr.pup
# DESC:    Ghost Object
# BBOX: 0 0 0 0 0 0
ghost             none               orient=(0.000000/0.000000/0.000000) type=ghost move=path
# DESC:    Armor Vest Pickup
# BBOX: -.021293 -.02235 -.028598 .01838 .02235 .028374
fullshield        _powerup           thingflags=0x400 model3d=vest.3do cog=pow_vest.cog respawn=60.000000
# DESC:    Door 10x10 Single Thinner end ,used in narshadda above crates 
# BBOX: -.498 -.998 -.1 .498 -.002 0
10x10door         _walkstruct        model3d=d10x10.3do size=1.169825 movesize=1.169825 soundclass=lg_elev.snd
# DESC:    Thermal Detonators Pickup
# BBOX: -.020824 -.043225 .000892 .011798 .043225 .024826
detonator         _powerup           thingflags=0x400 model3d=detp.3do cog=pow_thermal.cog
# DESC:    
# BBOX: -.016298 -.015506 -.024816 .016298 .015506 .024816
bottle04          _structure         model3d=bottle4.3do size=0.080942 movesize=0.080942
# DESC:    
# BBOX: 0 0 0 0 0 0
+firecloud        +dustcloud         material=00explosion.mat range=0.050000 rate=64.000000 maxthrust=40.000000 count=64
# DESC:    
# BBOX: 0 0 0 0 0 0
+grenade_exp      _explosion         thingflags=0x1 light=0.200000 timer=0.800000 sprite=detx.spr soundclass=exp_det.snd creatething=+firecloud typeflags=0x17 damage=75.000000 blasttime=0.700000 force=100.000000 maxlight=0.800000 range=0.450000
# DESC:    
# BBOX: 0 0 0 0 0 0
+remote_exp       +grenade_exp       sprite=debrisx.spr soundclass=exp_remote.snd damage=0.000000 force=25.000000
# DESC:    
# BBOX: 0 0 0 0 0 0
+mouse_exp        +remote_exp        force=10.000000
# DESC:    Mousebot Droid
# BBOX: -.018576 -.029933 -.04006 .017936 .031186 -.009989
mousebot          _droidactor        thingflags=0x20000400 model3d=mb.3do size=0.039000 movesize=0.039000 soundclass=mousebot.snd cog=actor_mb.cog mass=20.000000 height=0.040000 health=10.00 maxthrust=1.00 maxrotthrust=80.00 typeflags=0x20 explode=+mouse_exp aiclass=mouse.ai
# DESC:    
# BBOX: -.012516 -.012516 -.012524 .012516 .012516 .014655
+grenade2         _weapon            timer=3.000000 model3d=det0.3do size=0.014895 movesize=0.014895 soundclass=det.snd surfdrag=3.000000 airdrag=0.800000 mass=1.000000 physflags=0x225 vel=(0.000000/2.000000/1.500000) angvel=(90.000000/45.000000/90.000000) buoyancy=0.250000 explode=+grenade_exp fleshhit=+grenade_exp damageclass=0x4 typeflags=0x40309
# DESC:    
# BBOX: -.012516 -.012516 -.012524 .012516 .012516 .014655
+grenade1         +grenade2          timer=10.000000 typeflags=0x4030d
# DESC:    
# BBOX: 0 0 0 0 0 0
+crossbowhit      +laserhit          sprite=bowx2.spr soundclass=exp_bowhit.snd
# DESC:    
# BBOX: 0 0 0 0 0 0
+lgflash          _explosion         thingflags=0x1 light=0.300000 timer=0.400000 typeflags=0x12 blasttime=0.200000 maxlight=1.000000
# DESC:    Crossbow
# BBOX: -.020392 -.135722 -.016856 .021073 .013215 .015722
+crossbowbolt     _weapon            thingflags=0x20000001 light=0.500000 model3d=bow0.3do size=0.005000 movesize=0.005000 soundclass=bow.snd creatething=+lgflash vel=(0.000000/5.000000/0.000000) angvel=(0.000000/0.000000/120.000000) explode=+crossbowhit fleshhit=+crossbowhit damage=60.000000 mindamage=20.000000 damageclass=0x4 typeflags=0x20440d rate=10.000000
# DESC:    
# BBOX: 0 0 0 0 0 0
+crossbowhit2     _explosion         thingflags=0x1 light=0.200000 timer=0.500000 sprite=bowx.spr typeflags=0x33 blasttime=0.300000 maxlight=0.400000
# DESC:    Crossbow Spread Shots
# BBOX: -.014666 -.12215 -.016699 .015346 .011894 .015879
+crossbowbolt2    +crossbowbolt      model3d=bow1.3do soundclass=none creatething=none explode=+crossbowhit2 fleshhit=+crossbowhit2 damage=40.000000 typeflags=0x440d
# DESC:    Crossbow Bouncing Shots
# BBOX: -.020392 -.135722 -.016856 .021073 .013215 .015722
+crossbowbolt3    +crossbowbolt      typeflags=0x28440d
# DESC:    
# BBOX: 0 0 0 0 0 0
+repeaterhit      +laserhit          sprite=rptx.spr soundclass=exp_rpthit.snd
# DESC:    
# BBOX: 0 0 0 0 0 0
+rpt_sparks       none               orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=0.700000 mass=0.100000 physflags=0x400201 vel=(0.000000/0.000000/0.200000) typeflags=0x27 material=sparky.mat range=0.040000 rate=24.000000 maxthrust=7.000000 elementsize=0.005000 count=24
# DESC:    
# BBOX: 0 0 0 0 0 0
+rptfleshhit      _explosion         thingflags=0x1 light=0.100000 timer=0.800000 soundclass=exp_fleshy.snd creatething=+rpt_sparks typeflags=0x12 blasttime=0.700000 maxlight=0.300000
# DESC:    Repeater Gun
# BBOX: -.005771 -.015021 -.00466 .005771 .003935 .00466
+repeaterball     _weapon            thingflags=0x20000001 light=0.300000 model3d=rpt0.3do size=0.005000 movesize=0.005000 soundclass=rep.snd creatething=+smflash vel=(0.000000/6.000000/0.000000) explode=+repeaterhit fleshhit=+rptfleshhit damage=20.000000 mindamage=5.000000 typeflags=0x440d rate=20.000000
# DESC:    
# BBOX: 0 0 0 0 0 0
+firecloud2       +firecloud         elementsize=0.012500
# DESC:    
# BBOX: 0 0 0 0 0 0
+firecloud3       +firecloud         maxthrust=20.000000 count=32
# DESC:    
# BBOX: 0 0 0 0 0 0
+debris_exp       _explosion         timer=1.000000 sprite=debrisx.spr soundclass=exp_small.snd creatething=+firecloud3 typeflags=0x7 blasttime=0.500000
# DESC:    
# BBOX: 0 0 0 0 0 0
_debris           none               orient=(0.000000/0.000000/0.000000) type=weapon collide=1 move=physics timer=1.100000 airdrag=3.000000 physflags=0x201 vel=(0.000000/4.000000/0.000000) angvel=(5.000000/10.000000/0.000000) explode=+debris_exp damage=5.000000 damageclass=0x1 typeflags=0xc
# DESC:    
# BBOX: 0 0 0 0 0 0
_debris2          _debris            explode=+smflash
# DESC:    
# BBOX: -.026724 -.026724 -.044654 .026724 .026724 .044654
shrapnel2_1       _debris2           model3d=shrp_1.3do size=0.045000 movesize=0.045000
# DESC:    
# BBOX: -.011171 -.010389 -.042003 .011171 .010389 .042003
shrapnel2_2       _debris2           model3d=shrp_2.3do size=0.040000 movesize=0.040000
# DESC:    
# BBOX: -.015988 -.015443 -.026188 .015988 .015443 .026188
shrapnel2_3       _debris2           model3d=shrp_3.3do size=0.028000 movesize=0.028000
# DESC:    
# BBOX: -.016213 -.014647 -.020855 .016213 .014647 .020855
shrapnel2_4       _debris2           model3d=shrp_4.3do size=0.026000 movesize=0.026000
# DESC:    
# BBOX: 0 0 0 0 0 0
+sequencer_exp    _explosion         thingflags=0x1 light=0.200000 timer=0.800000 sprite=seqx2.spr soundclass=exp_med.snd creatething=+firecloud2 typeflags=0x17 damage=100.000000 blasttime=0.700000 force=300.000000 maxlight=0.800000 range=0.600000 debris=shrapnel2_1 debris=shrapnel2_2 debris=shrapnel2_3 debris=shrapnel2_4
# DESC:    Sequencer Mine
# BBOX: -.022216 -.023897 -.005309 .022216 .023897 .005309
+seqchrg          +grenade2          timer=1.000000 model3d=seq0.3do size=0.010000 movesize=0.010000 soundclass=seq.snd surfdrag=5.000000 airdrag=1.000000 physflags=0x29d vel=(0.000000/0.100000/0.000000) angvel=(0.000000/0.000000/0.000000) buoyancy=0.150000 explode=+sequencer_exp fleshhit=+sequencer_exp typeflags=0x40380
# DESC:    
# BBOX: -.022216 -.023897 -.005309 .022216 .023897 .005309
+seqchrg2         +seqchrg           thingflags=0x20000400 timer=60.000003 cog=class_sequencer.cog
# DESC:    
# BBOX: -.026724 -.026724 -.044654 .026724 .026724 .044654
shrapnel_1        _debris            model3d=shrp_1.3do size=0.045000 movesize=0.045000
# DESC:    
# BBOX: -.011171 -.010389 -.042003 .011171 .010389 .042003
shrapnel_2        _debris            model3d=shrp_2.3do size=0.040000 movesize=0.040000
# DESC:    
# BBOX: -.015988 -.015443 -.026188 .015988 .015443 .026188
shrapnel_3        _debris            model3d=shrp_3.3do size=0.028000 movesize=0.028000
# DESC:    
# BBOX: -.016213 -.014647 -.020855 .016213 .014647 .020855
shrapnel_4        _debris            model3d=shrp_4.3do size=0.026000 movesize=0.026000
# DESC:    
# BBOX: 0 0 0 0 0 0
+raildet_exp      _explosion         thingflags=0x1 light=0.200000 timer=0.500000 sprite=rldx.spr soundclass=exp_raildet.snd typeflags=0x17 damage=110.000000 blasttime=0.700000 force=300.000000 maxlight=0.800000 range=0.500000 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_4
# DESC:    Rail Detonator
# BBOX: -.01058 -.013704 -.006548 .010593 .009939 .012329
+raildet2         +grenade1          thingflags=0x20000400 timer=2.500000 model3d=rld0.3do size=0.003000 movesize=0.003000 puppet=ra.pup soundclass=rail.snd creatething=+lgflash cog=00_smoketrail.cog airdrag=0.000000 height=0.003000 physflags=0x200 vel=(0.000000/2.500000/0.000000) angvel=(0.000000/0.000000/90.000000) explode=+raildet_exp fleshhit=+raildet_exp damage=5.000000 typeflags=0x240b81
# DESC:    
# BBOX: 0 0 0 0 0 0
+raildet_exp2     +raildet_exp       debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_4
# DESC:    
# BBOX: -.01058 -.013704 -.006548 .010593 .009939 .012329
+raildet          +raildet2          timer=10.000000 explode=+raildet_exp2 fleshhit=+raildet_exp2 damage=20.000000 typeflags=0x24020d
# DESC:    
# BBOX: 0 0 0 0 0 0
+conccloud        +dustcloud         rate=512.000000 maxthrust=100.000000 count=256 minsize=0.012000 pitchrange=5.000000
# DESC:    
# BBOX: 0 0 0 0 0 0
+conc_exp         _explosion         thingflags=0x1 light=0.300000 timer=1.000000 sprite=conx.spr soundclass=exp_conc.snd creatething=+conccloud typeflags=0x17 damage=80.000000 blasttime=1.000000 force=200.000000 maxlight=0.800000 range=0.800000
# DESC:    Concussion Rifle
# BBOX: -.01341 -.149802 -.01341 .01341 .015603 .01341
+concbullet       _weapon            thingflags=0x20000001 model3d=con0.3do size=0.005000 movesize=0.005000 soundclass=conc.snd creatething=+lgflash vel=(0.000000/7.000000/0.000000) explode=+conc_exp fleshhit=+conc_exp damage=20.000000 typeflags=0x20000d
# DESC:    
# BBOX: 0 0 0 0 0 0
+concblast2p      +dustcloud         orient=(90.000000/0.000000/0.000000) range=0.001500 rate=128.000000 elementsize=0.005000 count=64 minsize=0.001200 pitchrange=5.000000
# DESC:    
# BBOX: 0 0 0 0 0 0
+concblast3p      +concblast2p       typeflags=0x2b material=00ramp4.mat maxthrust=100.000000
# DESC:    
# BBOX: 0 0 0 0 0 0
+concblast2       _weapon            size=0.005000 movesize=0.005000 fleshhit=+concblast3p trailthing=+concblast2p elementsize=0.300000 damage=80.000000 mindamage=20.000000 typeflags=0xa00d range=5.000000 rate=1.000000
# DESC:    
# BBOX: 0 0 0 0 0 0
+axe_exp          _explosion         timer=0.001000 soundclass=exp_axe.snd typeflags=0x0
# DESC:    
# BBOX: 0 0 0 0 0 0
+gamaxe           _weapon            size=0.001000 movesize=0.001000 mass=100.000000 explode=+axe_exp fleshhit=+axe_exp damage=40.000000 damageclass=0x1 typeflags=0x200d range=0.250000 force=50.000000
# DESC:    
# BBOX: 0 0 0 0 0 0
+force_repel      _explosion         thingflags=0x1 light=0.500000 timer=0.400000 soundclass=exp_dest.snd typeflags=0x52 blasttime=0.300000 force=200.000000 maxlight=1.000000 range=0.500000
# DESC:    
# BBOX: -.007659 -.000007 -.008551 .008867 .076357 .007405
+force_ltpeice    none               orient=(0.000000/0.000000/0.000000) type=weapon move=physics timer=0.250000 model3d=ligt2.3do size=0.005000 movesize=0.005000 physflags=0x200 maxrotvel=360.000000 angvel=(0.000000/0.000000/360.000000)
# DESC:    
# BBOX: 0 0 0 0 0 0
+lightninghit     none               orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=0.200000 typeflags=0x2b material=00ramp4.mat range=0.020000 rate=16.000000 maxthrust=8.000000 elementsize=0.003000 count=16
# DESC:    Force Lightning
# BBOX: 0 0 0 0 0 0
+force_lightning  _weapon            thingflags=0x1 light=0.100000 timer=0.100000 size=0.005000 movesize=0.005000 vel=(0.000000/1.000000/0.000000) angvel=(0.000000/0.000000/360.000000) explode=+lightninghit fleshhit=+lightninghit trailthing=+force_ltpeice elementsize=0.075000 trailrandangle=30.000000 damage=13.000000 damageclass=0x8 typeflags=0x1840d range=1.500000
# DESC:    
# BBOX: 0 0 0 0 0 0
+force_lightning2+force_lightningdamage=15.000000                   
# DESC:    
# BBOX: 0 0 0 0 0 0
+force_lightning3+force_lightningdamage=20.000000                   
# DESC:    
# BBOX: 0 0 0 0 0 0
+force_lightning4+force_lightningdamage=25.000000                   
# DESC:    LightSaber Pickup
# BBOX: -.008988 -.030139 -.030139 .008988 .030139 .030139
lightsaber        _powerup           thingflags=0x400 model3d=sabp.3do cog=pow_saber.cog
# DESC:    Force Saber
# BBOX: -.002761 -.017436 -.002868 .002754 .16643 .003496
+force_saber      _weapon            thingflags=0x20000001 light=0.100000 timer=1.000000 model3d=sab0.3do size=0.005000 movesize=0.005000 vel=(0.000000/1.000000/0.000000) angvel=(0.000000/150.000000/0.000000) explode=lightsaber fleshhit=lightsaber damage=50.000000 damageclass=0x10 typeflags=0x40d
# DESC:    Force Shield
# BBOX: 0 0 0 0 0 0
+force_shield     none               orient=(0.000000/0.000000/0.000000) type=cog move=physics timer=61.000003 particle=sphere.par angvel=(60.000000/60.000000/60.000000)
# DESC:    Force Blinding
# BBOX: 0 0 0 0 0 0
+force_blind      +dustcloud         material=00teleport.mat range=0.050000 rate=32.000000 elementsize=0.003000 minsize=0.020000 pitchrange=1.000000 yawrange=1.000000
# DESC:    
# BBOX: 0 0 0 0 0 0
+smoke            none               orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=0.800000 physflags=0x20000 vel=(0.000000/0.000000/0.120000) angvel=(0.000000/90.000000/0.000000) typeflags=0x3e material=00gsmoke.mat range=0.030000 elementsize=0.005000 count=8
# DESC:    
# BBOX: 0 0 0 0 0 0
+dest_trail       +smoke             vel=(0.000000/0.000000/0.000000) material=dstructparts.mat
# DESC:    
# BBOX: 0 0 0 0 0 0
+dest_cloud       +firecloud         material=dstructparts.mat
# DESC:    
# BBOX: 0 0 0 0 0 0
+force_dest1      _explosion         thingflags=0x1 light=0.100000 timer=0.400000 sprite=destruct8.spr soundclass=exp_dest.snd creatething=+dest_cloud typeflags=0x53 damage=15.000000 damageclass=0x8 blasttime=0.300000 force=100.000000 maxlight=0.500000 range=0.500000
# DESC:    
# BBOX: 0 0 0 0 0 0
+force_dest2      _explosion         thingflags=0x1 light=0.200000 timer=0.600000 sprite=destruct8.spr soundclass=exp_dest.snd creatething=+dest_cloud typeflags=0x53 damage=30.000000 damageclass=0x8 blasttime=0.400000 force=200.000000 maxlight=0.600000 range=1.000000
# DESC:    
# BBOX: 0 0 0 0 0 0
+force_dest3      _explosion         thingflags=0x1 light=0.300000 timer=0.800000 sprite=destruct8.spr soundclass=exp_dest.snd creatething=+dest_cloud typeflags=0x53 damage=45.000000 damageclass=0x8 blasttime=0.500000 force=300.000000 maxlight=0.700000 range=1.500000
# DESC:    
# BBOX: 0 0 0 0 0 0
+force_dest4      _explosion         thingflags=0x1 light=0.400000 timer=1.000000 sprite=destruct8.spr soundclass=exp_dest.snd creatething=+dest_cloud typeflags=0x53 damage=60.000000 damageclass=0x8 blasttime=0.600000 force=400.000000 maxlight=0.800000 range=2.000000
# DESC:    Force Destruction
# BBOX: 0 0 0 0 0 0
+force_dest_p1    _weapon            thingflags=0x20000401 light=0.400000 sprite=forcedstruct_150.spr cog=00_desttrail.cog vel=(0.000000/7.000000/0.000000) explode=+force_dest1 fleshhit=+force_dest1 damage=20.000000 typeflags=0xd
# DESC:    
# BBOX: 0 0 0 0 0 0
+force_dest_p2    +force_dest_p1     explode=+force_dest2 fleshhit=+force_dest2
# DESC:    
# BBOX: 0 0 0 0 0 0
+force_dest_p3    +force_dest_p1     explode=+force_dest3 fleshhit=+force_dest3
# DESC:    
# BBOX: 0 0 0 0 0 0
+force_dest_p4    +force_dest_p1     explode=+force_dest4 fleshhit=+force_dest4
# DESC:    
# BBOX: 0 0 0 0 0 0
+force_heal       none               orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=2.000000 angvel=(90.000000/90.000000/90.000000) typeflags=0x3f material=00teleport.mat range=0.300000 rate=128.000000 maxthrust=0.010000 elementsize=0.003000 count=128
# DESC:    
# BBOX: 0 0 0 0 0 0
+heavysmoke       +smoke             timer=1.600000 range=0.100000 rate=32.000000 count=64
# DESC:    
# BBOX: 0 0 0 0 0 0
+twinkle          none               orient=(0.000000/0.000000/0.000000) type=explosion thingflags=0x1 timer=0.500000 sprite=twinkle.spr typeflags=0x11
# DESC:    
# BBOX: 0 0 0 0 0 0
+sspks_wall       none               orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=0.100000 mass=0.050000 physflags=0x400201 vel=(0.000000/0.000000/0.150000) typeflags=0x2b material=00ramp1.mat range=0.030000 rate=4.000000 maxthrust=8.000000 elementsize=0.003000 count=48
# DESC:    
# BBOX: 0 0 0 0 0 0
+ssparks_wall     _explosion         thingflags=0x1 light=0.300000 timer=0.800000 soundclass=exp_saber_wall.snd creatething=+sspks_wall typeflags=0x112 blasttime=0.700000 maxlight=0.500000 flashrgb=(80/90/80)
# DESC:    
# BBOX: 0 0 0 0 0 0
+sspks_blood      none               orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=0.200000 mass=0.100000 physflags=0x400201 vel=(0.000000/0.000000/0.200000) typeflags=0x2b material=00ramp2.mat range=0.040000 rate=4.000000 maxthrust=9.000000 elementsize=0.005000 count=24
# DESC:    
# BBOX: 0 0 0 0 0 0
+ssparks_blood    _explosion         thingflags=0x1 light=0.100000 timer=0.800000 soundclass=exp_saber_blood.snd creatething=+sspks_blood typeflags=0x12 blasttime=0.700000 maxlight=0.300000
# DESC:    
# BBOX: 0 0 0 0 0 0
+sspks_saber      none               orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=0.300000 mass=0.050000 physflags=0x400200 vel=(0.000000/0.000000/0.100000) typeflags=0x27 material=00teleport.mat range=0.020000 rate=32.000000 maxthrust=8.000000 elementsize=0.003000 count=32
# DESC:    
# BBOX: 0 0 0 0 0 0
+ssparks_saber    _explosion         thingflags=0x1 light=0.800000 timer=0.800000 soundclass=exp_saber_saber.snd creatething=+sspks_saber typeflags=0x112 blasttime=0.700000 maxlight=1.000000 flashrgb=(150/160/150)
# DESC:    
# BBOX: 0 0 0 0 0 0
_droppowerup      _powerup           timer=30.000001 height=0.011000 physflags=0x41 typeflags=0x0
# DESC:    BackPack Pickup
# BBOX: -.021089 -.023005 -.035347 .016307 .023005 .035347
+backpack         _droppowerup       thingflags=0x400 model3d=bpck.3do cog=pow_backpack.cog height=0.036337 typeflags=0x4
# DESC:    Bryar Pistol Pickup
# BBOX: -.008248 -.030839 -.011813 .008248 .030839 .011813
bryarpistol       _powerup           thingflags=0x400 model3d=bryp.3do cog=pow_bryar.cog
# DESC:    1 Thermal Detonator Pickup
# BBOX: -.017868 -.016799 -.017934 .017868 .016799 .017934
singledetonator   _powerup           thingflags=0x400 model3d=detp_1.3do cog=pow_single_thermal.cog
# DESC:    CrossBow Pickup
# BBOX: -.034888 -.055377 .000124 .034343 .059997 .031925
crossbow          _powerup           thingflags=0x400 model3d=bowp.3do cog=pow_crossbow.cog
# DESC:    RepeaterGun Pickup
# BBOX: -.00548 -.051379 -.021767 .00548 .039969 .012035
repeatergun       _powerup           thingflags=0x400 model3d=rptp.3do cog=pow_repeater.cog
# DESC:    RailGun Pickup
# BBOX: -.018484 -.070714 -.030025 .019883 .081826 .013227
railgun           _powerup           thingflags=0x400 model3d=rldp.3do cog=pow_railgun.cog respawn=60.000000
# DESC:    1 Sequencer Mine Pickup
# BBOX: -.022216 -.005309 -.024793 .022216 .005309 .024793
singleseqcharge   _powerup           thingflags=0x400 model3d=seqp_1.3do cog=pow_single_sequencer.cog
# DESC:    Concussion Rifle Pickup
# BBOX: -.030347 -.072704 -.024787 .009211 .072704 .024341
concrifle         _powerup           thingflags=0x400 model3d=conp.3do cog=pow_concrifle.cog respawn=60.000000
# DESC:    Force Pulled Bryar Pistol Pickup
# BBOX: -.008248 -.030839 -.011813 .008248 .030839 .011813
+fpbryarpistol    bryarpistol        collide=0 timer=4.000000 typeflags=0x0 respawn=0.000000
# DESC:    Force Pulled St Rifle Pickup
# BBOX: -.016822 -.05557 -.026043 .007966 .058 .014355
+fpstrifle        strifle            collide=0 timer=4.000000 typeflags=0x0 respawn=0.000000
# DESC:    Force Pulled Thermal Detonator Pickup
# BBOX: -.017868 -.016799 -.017934 .017868 .016799 .017934
+fpdetonator      singledetonator    collide=0 timer=4.000000 typeflags=0x0 respawn=0.000000
# DESC:    Force Pulled CrossBow Pickup
# BBOX: -.034888 -.055377 .000124 .034343 .059997 .031925
+fpcrossbow       crossbow           collide=0 timer=4.000000 typeflags=0x0 respawn=0.000000
# DESC:    Force Pulled Repeater Gun Pickup
# BBOX: -.00548 -.051379 -.021767 .00548 .039969 .012035
+fprepeatergun    repeatergun        collide=0 timer=4.000000 typeflags=0x0 respawn=0.000000
# DESC:    Force Pulled Rail Detonator Gun Pickup
# BBOX: -.018484 -.070714 -.030025 .019883 .081826 .013227
+fprailgun        railgun            collide=0 timer=4.000000 typeflags=0x0 respawn=0.000000
# DESC:    Force Pulled Single Sequencer Mine Pickup
# BBOX: -.022216 -.005309 -.024793 .022216 .005309 .024793
+fpseqcharge      singleseqcharge    collide=0 timer=4.000000 typeflags=0x0 respawn=0.000000
# DESC:    Force Pulled Concussion Rifle Pickup
# BBOX: -.030347 -.072704 -.024787 .009211 .072704 .024341
+fpconcrifle      concrifle          collide=0 timer=4.000000 typeflags=0x0 respawn=0.000000
# DESC:    
# BBOX: 0 0 0 0 0 0
+telesparks       none               orient=(0.000000/0.000000/0.000000) type=particle timer=0.120000 typeflags=0x3f material=00teleport.mat range=0.030000 rate=256.000000 maxthrust=80.000000 elementsize=0.002000 count=256
# DESC:    
# BBOX: 0 0 0 0 0 0
bubble            none               orient=(0.000000/0.000000/0.000000) type=cog move=physics thingflags=0x10000000 timer=3.000000 sprite=bubble.spr mass=0.050000 physflags=0x200 vel=(0.000000/0.000000/0.300000)
# DESC:    
# BBOX: 0 0 0 0 0 0
bubble2           bubble             sprite=bubble2.spr vel=(0.000000/0.000000/0.450000)
# DESC:    
# BBOX: 0 0 0 0 0 0
bubble3           bubble             sprite=bubble3.spr vel=(0.000000/0.000000/0.600000)
# DESC:    
# BBOX: 0 0 0 0 0 0
+watersplash      none               orient=(0.000000/0.000000/0.000000) type=explosion timer=0.500000 sprite=splash.spr typeflags=0x1 damageclass=0x4
# DESC:    
# BBOX: 0 0 0 0 0 0
+watersplash2     +watersplash       sprite=splooshx.spr
# DESC:    Stormtroop Rifle Dropped
# BBOX: -.016822 -.05557 -.026043 .007966 .058 .014355
+dstrifle         _droppowerup       thingflags=0x400 model3d=strp.3do cog=pow_dstrifle.cog height=0.035177
# DESC:    
# BBOX: -.012585 -.050304 -.010582 .022579 .027638 .007152
_limb             none               orient=(0.000000/0.000000/0.000000) type=corpse collide=1 move=physics thingflags=0x400 timer=30.000001 model3d=stlimb.3do size=0.102337 movesize=0.000001 cog=00_bloodtrail.cog surfdrag=1.000000 airdrag=4.000000 mass=15.000000 physflags=0x44261 buoyancy=0.500000
# DESC:    
# BBOX: -.024586 -.02755 -.015015 .028804 .052615 .015553
+rylimb           _limb              model3d=rylimb.3do size=0.050000 movesize=0.010000
# DESC:    
# BBOX: -.024586 -.02755 -.015015 .028804 .052615 .015553
+rhlimb           _limb              model3d=rhlimb.3do size=0.050000 movesize=0.010000
# DESC:    
# BBOX: -.026804 -.036274 -.074092 .026804 .036274 .074092
shrapnel_5        _debris            model3d=shrp_5.3do size=0.084000 movesize=0.084000
# DESC:    
# BBOX: 0 0 0 0 0 0
+xtank1_exp       _explosion         thingflags=0x1 light=0.200000 timer=0.800000 sprite=detx.spr soundclass=exp_probe.snd typeflags=0x17 damage=90.000000 blasttime=0.700000 force=100.000000 maxlight=0.800000 range=0.450000 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_5
# DESC:    
# BBOX: 0 0 0 0 0 0
+xtank4_exp       +xtank1_exp        sprite=tiex.spr soundclass=exp_tie.snd damage=250.000000 force=300.000000 range=0.700000 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_5
# DESC:    
# BBOX: 0 0 0 0 0 0
+crane_exp        +xtank4_exp        damage=50.000000 force=75.000000 range=0.500000 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_5
# DESC:    
# BBOX: 0 0 0 0 0 0
+sentry_exp       +grenade_exp       damage=5.000000 force=50.000000
# DESC:    
# BBOX: 0 0 0 0 0 0
+crate_exp        +sentry_exp        damage=30.000000 force=75.000000 range=0.200000
# DESC:    
# BBOX: -.027845 -.025398 -.02252 .027845 .025398 .024913
+dhealthpack      _droppowerup       thingflags=0x408 model3d=hepk.3do cog=pow_health.cog height=0.023701
# DESC:    Bacta Tank Dropped 
# BBOX: -.015415 -.017797 -.028888 .015415 .017797 .028888
+dbactatank       _droppowerup       thingflags=0x400 model3d=bact.3do cog=pow_bacta.cog height=0.028885
# DESC:    Shield Dropped
# BBOX: -.040113 -.035949 -.021618 .040113 .035949 .021618
+dshield          _droppowerup       thingflags=0x400 model3d=shld.3do cog=pow_shields.cog height=0.021618
# DESC:    Battery Dropped 
# BBOX: -.006993 -.00625 -.021875 .006993 .00625 .021875
+dbattery         _droppowerup       thingflags=0x408 model3d=battery.3do cog=pow_battery.cog height=0.022909
# DESC:    EnergyCell Dropped  (pistol,rifle)
# BBOX: -.008536 -.014067 -.022418 .008536 .014068 .022418
+denergycell      _droppowerup       thingflags=0x400 model3d=ecel.3do cog=pow_energy.cog height=0.022435
# DESC:    PowerCell Dropped  (repeater,concusion,bowcaster)
# BBOX: -.009653 -.02399 -.017867 .009653 .02399 .017867
+dpowercell       _droppowerup       thingflags=0x400 model3d=pcel.3do cog=pow_power.cog height=0.017868
# DESC:    Smuggler BackPack Dropped
# BBOX: -.021089 -.023005 -.035347 .016307 .023005 .035347
+dsmbp            _droppowerup       thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk.cog height=0.036337
# DESC:    
# BBOX: 0 0 0 0 0 0
+rycw_punch       +ry_punch          damage=5.000000
# DESC:    
# BBOX: -.046863 -.025046 -.131246 .045997 .027318 .073257
reeyeescw         reeyeesfist        weapon=+rycw_punch typeflags=0x1020001
# DESC:    
# BBOX: 0 0 0 0 0 0
+gonk_exp         +grenade_exp       damage=20.000000
# DESC:    Greedo Pistol Dropped
# BBOX: -.005081 -.030803 -.01143 .005081 .030803 .01143
+dgreedopistol    _droppowerup       thingflags=0x400 model3d=ggun.3do cog=pow_bryar.cog height=0.011450
# DESC:    
# BBOX: 0 0 0 0 0 0
+scrate_exp       +crate_exp         damage=10.000000 force=25.000000 range=0.100000
# DESC:    
# BBOX: 0 0 0 0 0 0
+r2r5_exp         +grenade_exp       damage=10.000000 force=50.000000
# DESC:    
# BBOX: 0 0 0 0 0 0
+sparks           none               orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=1.000000 mass=1.000000 physflags=0x201 typeflags=0x3d material=embers.mat range=0.030000 rate=32.000000 maxthrust=20.000000 elementsize=0.010000 count=32
# DESC:    
# BBOX: 0 0 0 0 0 0
+limb_blood       none               orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=0.250000 physflags=0x201 vel=(0.000000/0.000000/0.200000) typeflags=0x27 material=00gsmoke.mat range=0.020000 rate=48.000000 maxthrust=3.000000 elementsize=0.005000 count=16
# DESC:    Enemy Thermal Detonator
# BBOX: 0 0 0 0 0 0
+egrenade_exp     +grenade_exp       damage=50.000000
# DESC:    
# BBOX: -.012516 -.012516 -.012524 .012516 .012516 .014655
+actor_grenade    +grenade1          surfdrag=0.000000 airdrag=0.000000 vel=(0.000000/2.000000/0.000000) explode=+egrenade_exp fleshhit=+egrenade_exp
# DESC:    ReeYee Brown w Thermal Detonators
# BBOX: -.046863 -.025046 -.131246 .045997 .027318 .073257
reeyees           _humanactor        thingflags=0x20000400 model3d=ry.3do size=0.072900 movesize=0.072900 puppet=ry.pup soundclass=ry.snd cog=actor_ry.cog weapon=+actor_grenade weapon2=+ry_punch health=50.00 maxhealth=50.00 maxthrust=0.90 maxrotthrust=70.00 jumpspeed=1.20 typeflags=0x20001 fireoffset=(0.046500/0.070000/0.032700) aiclass=rytherdefault.ai
# DESC:    
# BBOX: -.015083 -.063181 -.010398 .052176 .070684 .0122
disc              _powerup           thingflags=0x400 model3d=disc.3do cog=pow_datadisk.cog
# DESC:    
# BBOX: -.544736 -.786304 -.229997 .544736 .786304 .229997
flyingcrow        _zwalkstruct       model3d=crow.3do size=0.853084 movesize=0.853084 puppet=cr.pup
# DESC:    ReeYee Black w Thermal Detonators
# BBOX: -.046863 -.025046 -.131246 .045997 .027318 .073257
reeyeesbh         reeyees            model3d=rh.3do health=60.00 maxhealth=60.00 maxrotthrust=80.00
# DESC:    
# BBOX: -.012516 -.012516 -.012524 .012516 .012516 .014655
+actor_grenade2   +grenade2          vel=(0.000000/2.000000/0.000000) explode=+egrenade_exp fleshhit=+egrenade_exp
# DESC:    ReeYee Black w Bouncing Thermal Detonators
# BBOX: -.046863 -.025046 -.131246 .045997 .027318 .073257
reeyeesbhs        reeyeesbh          weapon=+actor_grenade2
# DESC:    ReeYee Brown w Bouncing Thermal Detonators
# BBOX: -.046863 -.025046 -.131246 .045997 .027318 .073257
reeyeess          reeyees            weapon=+actor_grenade2
# DESC:    Door 6x6 Single Drk Grey w Ribs 
# BBOX: -.3 -.0125 -.3 .3 .0125 .3
6x6door           _structure         model3d=d6x6.3do size=0.474448 movesize=0.474448 soundclass=lg_door.snd
# DESC:    
# BBOX: -.137499 -.154503 -.1497 .137499 .154503 .150618
roofvent1         _walkstruct        model3d=01rfd1.3do size=0.305318 movesize=0.305318
# DESC:    Gammorean Guard
# BBOX: -.064855 -.030184 -.137291 .064857 .181468 .079798
gammorean         _humanactor        thingflags=0x20000400 model3d=gm.3do size=0.078780 movesize=0.078780 puppet=gm.pup soundclass=gm.snd cog=actor_gg.cog mass=200.000000 weapon=+gamaxe weapon2=+gamaxe health=120.00 maxhealth=120.00 maxrotthrust=80.00 typeflags=0x20001 aiclass=ggdefault.ai
# DESC:    
# BBOX: -.1 -.5 -.012306 .1 .5 .012306
2x10catwalk       _walkstruct        model3d=c2x10.3do size=0.510050 movesize=0.510050
# DESC:    
# BBOX: -.200159 -.024886 -.057805 .200159 .024886 .057805
console5          _walkstruct        model3d=con5.3do size=0.209820 movesize=0.209820
# DESC:    Red Key Pickup
# BBOX: -.010023 -.016964 -.033841 .010023 .016964 .033841
keyred            _powerup           thingflags=0x400 model3d=r-key.3do cog=pow_keyred.cog
# DESC:    
# BBOX: -.050201 -.999639 -.073612 .050201 .999639 .073612
boom              _walkstruct        model3d=boom.3do size=1.003602 movesize=1.003602 soundclass=02boom.snd
# DESC:    
# BBOX: -.150124 -.150615 -.150124 .149876 .149385 .149872
crate4_5          _walkstruct        model3d=crt4-5.3do size=0.260306 movesize=0.260306
# DESC:    
# BBOX: -.2 -.198942 -.2 .2 .201059 .2
crate4_6          _walkstruct        model3d=crt4-6.3do size=0.347022 movesize=0.347022
# DESC:    Door 8x8 Single Drk Grey w 2 Large Indents
# BBOX: -.4 -.050015 -.398 .4 .050015 .4
8x8door           _structure         model3d=d8x8.3do size=0.617892 movesize=0.617892 soundclass=lg_door.snd
# DESC:    C2PO Droid
# BBOX: -.030754 -.015261 -.124016 .030821 .02207 .067788
seetoopio         _droidactor        thingflags=0x20000400 model3d=c2.3do size=0.067800 movesize=0.067800 puppet=c3.pup soundclass=c3po.snd cog=actor_c3po.cog mass=120.000000 maxvel=0.400000 health=50.00 maxhealth=50.00 maxthrust=0.30 maxrotthrust=50.00 typeflags=0x180140 aiclass=drdefault.ai
# DESC:    
# BBOX: -1.850091 -.300148 -.107601 1.850091 .300148 .107601
lift              _walkstruct        model3d=lift.3do size=1.877332 movesize=1.877332 soundclass=funicular.snd
# DESC:    
# BBOX: -.063294 -.054836 -.040528 .063294 .054836 .040528
table             _decor             model3d=01tabl.3do size=0.065000 movesize=0.065000
# DESC:    Invinsible Pickup
# BBOX: -.021293 -.02235 -.028598 .01838 .02235 .028374
supershield       _powerup           thingflags=0x400 model3d=vestl.3do cog=pow_supershield.cog respawn=60.000000
# DESC:    Battery Pickup 
# BBOX: -.006993 -.00625 -.021875 .006993 .00625 .021875
battery           _powerup           thingflags=0x408 model3d=battery.3do cog=pow_battery.cog
# DESC:    BackPack Pickup
# BBOX: -.021089 -.023005 -.035347 .016307 .023005 .035347
rbackpack         _powerup           model3d=bpck.3do
# DESC:    Revive Pickup
# BBOX: -.021089 -.023005 -.035347 .016307 .023005 .035347
revivepack        _powerup           thingflags=0x400 model3d=packrev.3do cog=pow_revive.cog respawn=60.000000
# DESC:    Woman
# BBOX: -.021527 -.011397 -.102055 .021527 .014719 .057565
woman2            _civilian          thingflags=0x20000400 model3d=w2.3do size=0.056000 movesize=0.056000 puppet=w2.pup soundclass=wm1.snd cog=actor_pf.cog maxvel=0.200000 health=50.00 maxhealth=50.00 maxthrust=0.50 maxrotthrust=60.00 aiclass=peddefault.ai
# DESC:    Woman Narshadda
# BBOX: -.021527 -.011397 -.102055 .021527 .014719 .057565
nwoman2           woman2             cog=actor_fn.cog aiclass=pednarsh.ai
# DESC:    Woman greedo
# BBOX: -.021527 -.011397 -.102055 .021527 .014719 .057565
gwoman2           nwoman2            cog=actor_gd.cog aiclass=peddefault.ai
# DESC:    
# BBOX: -.2 -.2 -.2 .2 .2 .2
02crate1          _walkstruct        model3d=02crate1.3do size=0.346410 movesize=0.346410
# DESC:    
# BBOX: -.2 -.2 -.2 .2 .2 .2
02crate2          _walkstruct        model3d=02crate2.3do size=0.346410 movesize=0.346410
# DESC:    
# BBOX: -.2 -.198942 -.2 .2 .201059 .2
02crate3          _walkstruct        model3d=02crate3.3do size=0.397022 movesize=0.397022
# DESC:    
# BBOX: -.597832 -.597465 -.299701 .597179 .59758 .299701
00zcrate          _zwalkstruct       model3d=00crte.3do size=0.830224 movesize=0.830224 soundclass=funicular.snd
# DESC:    Thermal Detonators Dropped
# BBOX: -.020824 -.043225 .000892 .011798 .043225 .024826
+ddetonator       _droppowerup       thingflags=0x400 model3d=detp.3do cog=pow_thermal.cog
# DESC:    1 Thermal Detonator Dropped
# BBOX: -.017868 -.016799 -.017934 .017868 .016799 .017934
+dsingle          _droppowerup       thingflags=0x400 model3d=detp_1.3do cog=pow_single_thermal.cog height=0.021909
# DESC:    
# BBOX: -.012516 -.012516 -.012524 .012516 .012516 .014655
+spec_grenade     +grenade2          vel=(0.000000/0.000000/0.000000)
# DESC:    
# BBOX: -.016813 -.025219 -.025219 .016813 .025219 .025219
bottle02          _structure         model3d=bottle2.3do size=0.089429 movesize=0.089429
# DESC:    
# BBOX: -.512471 -.025172 -.165 .512502 .025172 .163
window3a          _zwalkstruct       thingflags=0x40 model3d=wn3a.3do size=0.588384 movesize=0.588384
# DESC:    
# BBOX: -.037244 -.032884 -.063574 .037244 .032884 .063574
chair04           _walkstruct        thingflags=0x48 model3d=chr4.3do size=0.124181 movesize=0.124181
# DESC:    
# BBOX: -.1 -.0125 -.15 .1 .0125 .15
03_2x3door        _structure         thingflags=0x808 model3d=03d2x3.3do size=0.180710 movesize=0.180710 soundclass=03wood_door.snd
# DESC:    
# BBOX: 0 -.0125 -.15 .2 .0125 .15
4x3door_b         _structure         thingflags=0x808 model3d=03d4x3_b.3do size=0.250312 movesize=0.250312 soundclass=03wood_door.snd
# DESC:    
# BBOX: -.2 -.0125 -.15 0 .0125 .15
4x3door_a         _structure         thingflags=0x808 model3d=03d4x3_a.3do size=0.250312 movesize=0.250312 soundclass=03wood_door.snd
# DESC:    
# BBOX: -.15 -.0125 -.15 .15 .0125 .15
03_3x3door        _structure         thingflags=0x808 model3d=03d3x3.3do size=0.212500 movesize=0.212500 soundclass=03wood_door.snd
# DESC:    Enemy Crossbow 
# BBOX: -.020392 -.135722 -.016856 .021073 .013215 .015722
+ebow             +crossbowbolt      vel=(0.000000/4.000000/0.000000) damage=20.000000 mindamage=10.000000
# DESC:    
# BBOX: -.020392 -.135722 -.016856 .021073 .013215 .015722
+ebowplus5        +ebow              soundclass=none vel=(-1.000000/4.000000/0.000000)
# DESC:    
# BBOX: -.020392 -.135722 -.016856 .021073 .013215 .015722
+ebowplus4        +ebow              soundclass=none creatething=+ebowplus5 vel=(1.000000/4.000000/0.000000)
# DESC:    
# BBOX: -.020392 -.135722 -.016856 .021073 .013215 .015722
+ebowplus3        +ebow              soundclass=none creatething=+ebowplus4 vel=(-0.500000/4.000000/0.000000)
# DESC:    
# BBOX: -.020392 -.135722 -.016856 .021073 .013215 .015722
+ebowplus2        +ebow              soundclass=none creatething=+ebowplus3 vel=(0.500000/4.000000/0.000000)
# DESC:    
# BBOX: -.020392 -.135722 -.016856 .021073 .013215 .015722
+ebowplus         +ebow              creatething=+ebowplus2
# DESC:    
# BBOX: -.020392 -.135722 -.016856 .021073 .013215 .015722
+ebowplus3m       +ebow              soundclass=none vel=(-0.500000/4.000000/0.000000)
# DESC:    
# BBOX: -.020392 -.135722 -.016856 .021073 .013215 .015722
+ebowplus2m       +ebowplus2         creatething=+ebowplus3m
# DESC:    
# BBOX: -.020392 -.135722 -.016856 .021073 .013215 .015722
+ebowplusm        +ebowplus          creatething=+ebowplus2m
# DESC:    Tuskan Raider Brown w Bowcaster
# BBOX: -.037806 -.014831 -.120834 .054054 .062457 .065442
tuskan            _humanactor        thingflags=0x20000400 model3d=tk.3do size=0.064900 movesize=0.064900 puppet=tk.pup soundclass=tk.snd cog=actor_tk.cog mass=120.000000 maxvel=0.200000 weapon=+ebow weapon2=+ebowplusm health=50.00 maxhealth=50.00 maxthrust=0.90 maxrotthrust=70.00 jumpspeed=1.20 typeflags=0x20000 fireoffset=(0.006000/0.061000/0.027000) aiclass=tuproddefault.ai
# DESC:    
# BBOX: -.1 -.0125 -.15 .1 .0125 .15
03_2x3door2       _structure         thingflags=0x808 model3d=03d2x3_2.3do size=0.180710 movesize=0.180710 soundclass=03wood_door.snd
# DESC:    
# BBOX: -.044472 -.026134 -.026481 .044472 .000056 .041013
lightfix00        _ghoststructure    thingflags=0x9 model3d=lfix.3do size=0.100531 movesize=0.100531
# DESC:    Tuskan Raider Black w Bowcaster
# BBOX: -.037806 -.014831 -.120834 .054054 .062457 .065442
tuskanbh          tuskan             model3d=th.3do weapon2=+ebowplus health=60.00 maxhealth=60.00 maxrotthrust=80.00 aiclass=tubproddefault.ai
# DESC:    Tuskan Raider Brown w St Rifle
# BBOX: -.037806 -.014831 -.120834 .037548 .016412 .065442
tuskangun         _humanactor        thingflags=0x20000400 model3d=tkstr.3do size=0.064900 movesize=0.064900 puppet=tkstr.pup soundclass=tkstr.snd cog=actor_tb.cog mass=120.000000 maxvel=0.200000 weapon=+elaser health=50.00 maxhealth=50.00 maxthrust=0.90 maxrotthrust=70.00 jumpspeed=1.20 fireoffset=(-0.041600/0.068200/0.022500) aiclass=tudefault.ai
# DESC:    Tuskan Raider Black w St Rifle
# BBOX: -.037806 -.014831 -.120834 .037548 .016412 .065442
tuskanbhgun       tuskangun          model3d=thstr.3do health=60.00 maxhealth=60.00 maxrotthrust=80.00 aiclass=tubdefault.ai
# DESC:    
# BBOX: 0 0 0 0 0 0
+mailocsting      +gamaxe            damage=15.000000 force=25.000000
# DESC:    Mynok
# BBOX: -.080669 -.095318 -.091964 .082473 .08505 .212669
mynok             _humanactor        thingflags=0x20000400 model3d=my.3do size=0.100000 movesize=0.100000 puppet=my.pup soundclass=mynoc.snd cog=actor_mailoc.cog mass=40.000000 physflags=0x6a06 weapon=+mailocsting maxthrust=0.90 maxrotthrust=70.00 typeflags=0x2020000 aiclass=maidefault.ai
# DESC:    
# BBOX: -.163442 -.025172 -.165 .164415 .025172 .163
window3b          _zwalkstruct       thingflags=0x40 model3d=wn3b.3do size=0.282198 movesize=0.282198
# DESC:    
# BBOX: -.044472 -.026134 -.026481 .044472 .000056 .041013
lightfixture      _ghoststructure    thingflags=0x9 light=0.400000 model3d=lfix.3do size=0.100531 movesize=0.100531
# DESC:    PowerCell Pickup  (repeater,concusion,bowcaster)
# BBOX: -.009653 -.02399 -.017867 .009653 .02399 .017867
powercell         _powerup           thingflags=0x400 model3d=pcel.3do cog=pow_power.cog
# DESC:    
# BBOX: -.016298 -.015506 -.024816 .016298 .015506 .024816
bottle03          _structure         model3d=bottle3.3do size=0.080942 movesize=0.080942
# DESC:    Dynamic Light 0.7
# BBOX: 0 0 0 0 0 0
light0.7          _ghostdecor        thingflags=0x1 light=0.700000
# DESC:    
# BBOX: -.032036 -.054681 -.024816 .032036 .054682 .024816
bottles03         _structure         collide=1 model3d=bottles3.3do size=0.050000 movesize=0.050000
# DESC:    
# BBOX: -.025015 -.324814 -.025014 .025015 .324814 .025014
beam02            _zwalkstruct       thingflags=0x40 model3d=beam02.3do size=0.376735 movesize=0.376735
# DESC:    
# BBOX: -.075889 -.047877 -.051709 .075889 .047877 .051709
table4            _walkstruct        thingflags=0x48 model3d=tbl4.3do size=0.153561 movesize=0.153561
# DESC:    
# BBOX: -.050387 -.043746 -.161575 .050387 .043746 .161575
vaporator         _structure         model3d=vap1.3do size=0.219250 movesize=0.219250
# DESC:    
# BBOX: -.899877 -.063413 -.499966 .899877 .063413 .499965
trelus            _walkstruct        thingflags=0x48 model3d=trel.3do size=1.031390 movesize=1.031390
# DESC:    
# BBOX: -.125 -.200163 -.049948 .125 .200163 .072163
bed               _walkstruct        thingflags=0x800048 model3d=bed.3do size=0.291216 movesize=0.291216
# DESC:    Door 1/2 4x3  Gold Color Vertical Split 
# BBOX: -.1 -.0125 -.15 .1 .0125 .15
4x3door_2a        _structure         model3d=d4x3_2a.3do size=0.230710 movesize=0.230710 soundclass=med_door.snd
# DESC:    Door 1/2 4x3  Gold Color Vertical Split 
# BBOX: -.1 -.0125 -.15 .1 .0125 .15
4x3door_2b        _structure         model3d=d4x3_2b.3do size=0.230710 movesize=0.230710
# DESC:    
# BBOX: -.2 -.1 -.050028 .2 .1 .050359
table2            _walkstruct        thingflags=0x48 model3d=tbl2.3do size=0.279207 movesize=0.279207
# DESC:    
# BBOX: -.049067 -.02563 -.000348 .049067 .02563 .153568
cabinet1          _walkstruct        thingflags=0x48 model3d=cab_1.3do size=0.213241 movesize=0.213241
# DESC:    
# BBOX: -.035377 -.019473 .000188 .035377 .019473 .12627
cabinet2          _walkstruct        thingflags=0x48 model3d=cab_2.3do size=0.182570 movesize=0.182570
# DESC:    
# BBOX: -.098661 -.114859 -.001485 .095745 .114859 .209053
chair2            _walkstruct        thingflags=0x800048 model3d=chair2.3do size=0.308127 movesize=0.308127
# DESC:    
# BBOX: -.042032 -.049632 .000337 .042032 .049632 .195296
chair3            _walkstruct        thingflags=0x48 model3d=chair3.3do size=0.254420 movesize=0.254420
# DESC:    
# BBOX: -.074978 -.074978 -.069146 .074978 .074978 .069146
woodcrate         _walkstruct        thingflags=0x48 model3d=wcrt.3do size=0.176588 movesize=0.176588
# DESC:    
# BBOX: -.8 -.049136 -.183134 .8 .05599 .183134
waterpumpblock    _zwalkstruct       model3d=wpmpbloc.3do size=0.822601 movesize=0.822601
# DESC:    Blue Wrench Key Pickup
# BBOX: -.032373 -.017479 .000097 .031819 .017479 .079704
bluewrenchkey     _powerup           thingflags=0x400 model3d=keyblu.3do cog=pow_wrenchblue.cog
# DESC:    Yellow Wrench Key Pickup
# BBOX: -.032373 -.017479 .000097 .031819 .017479 .079704
yellowwrenchkey   _powerup           thingflags=0x400 model3d=keyyel.3do cog=pow_wrenchyellow.cog
# DESC:    Door 16x6.5 Single Drk Grey  
# BBOX: -.8 -.016042 -.325 .8 .016042 .325
garagedoor        _structure         model3d=grgd.3do size=0.913645 movesize=0.913645 soundclass=lg_door.snd
# DESC:    
# BBOX: -.04272 -.049295 -.065661 .04272 .049295 .065661
woodcrate2        _walkstruct        thingflags=0x448 model3d=wcrt6-0.3do size=0.132106 movesize=0.132106 cog=actor_wc.cog
# DESC:    
# BBOX: -.016813 -.025219 -.025219 .016813 .025219 .025219
bottle01          _structure         model3d=bottle1.3do size=0.089429 movesize=0.089429
# DESC:    
# BBOX: -.086593 -.082033 -.02768 .086593 .082033 .02768
wholerock_2       _walkstruct        thingflags=0x48 model3d=rok_2.3do size=0.151886 movesize=0.151886
# DESC:    
# BBOX: -.05 -.05 -.075 .05 .05 .075
03blockbottom     _zwalkstruct       model3d=03blockb.3do size=0.153078 movesize=0.153078
# DESC:    
# BBOX: -.15 -.0125 -.15 .15 .0125 .15
03_3x3door_2      _zwalkstruct       thingflags=0x800 model3d=03d3x3_2.3do size=0.262500 movesize=0.262500 soundclass=03wood_door.snd
# DESC:    
# BBOX: -.3 -.075 -.05 .3 .075 .05
reclight          _structure         model3d=g_lite.3do size=0.363249 movesize=0.363249
# DESC:    Stormtoop BackPack Pickup
# BBOX: -.018131 -.009419 -.000113 .037525 .027081 .049637
stbp              _powerup           thingflags=0x400 model3d=stpack.3do cog=pow_stbkpk.cog
# DESC:    
# BBOX: -.070489 -.081337 -.065817 .070489 .081337 .065817
woodcrate3        _walkstruct        thingflags=0x448 model3d=wcrt6-1.3do size=0.154631 movesize=0.154631 cog=actor_wc.cog
# DESC:    
# BBOX: -.15 -.15 -.15 .15 .15 .15
woodcratebig      _walkstruct        thingflags=0x48 model3d=wcrt2.3do size=0.309808 movesize=0.309808
# DESC:    
# BBOX: -.199721 -.174982 -.070859 .199966 .174982 .029196
canopy            _zwalkstruct       thingflags=0x40 model3d=cany.3do size=0.325002 movesize=0.325002
# DESC:    
# BBOX: -.016342 -.015548 -.018445 .016342 .015548 .018445
bottle06          _structure         model3d=bottle6.3do size=0.076147 movesize=0.076147
# DESC:    
# BBOX: -.032771 -.046457 -.018445 .032771 .046457 .018445
bottles06         _structure         collide=1 model3d=bottles6.3do size=0.050000 movesize=0.050000
# DESC:    
# BBOX: -.038893 -.037552 -.024816 .039144 .036124 .024816
bottles04         _structure         collide=1 model3d=bottles4.3do size=0.050000 movesize=0.050000
# DESC:    
# BBOX: -.061611 -.054554 -.025219 .061611 .054554 .025219
bottles01         _structure         collide=1 model3d=bottles1.3do size=0.050000 movesize=0.050000
# DESC:    
# BBOX: -.15 -.15 -.15 .15 .15 .15
03woodcrate2      _zwalkstruct       thingflags=0x40 model3d=wcrt2.3do size=0.309808 movesize=0.309808
# DESC:    
# BBOX: -.066759 -.059659 -.025219 .066759 .059659 .025219
bottles02         _structure         collide=1 model3d=bottles2.3do size=0.050000 movesize=0.050000
# DESC:    
# BBOX: -.05 -.05 -.000679 .05 .05 .299321
03block           _zwalkstruct       model3d=03block.3do size=0.357560 movesize=0.357560
# DESC:    WeeGee Droid 
# BBOX: -.072521 -.040875 -.055302 .074842 .129417 .069428
weegee            _droidactor        thingflags=0x404 model3d=wg.3do size=0.100000 movesize=0.100000 puppet=wg.pup soundclass=wg.snd cog=actor_wg.cog mass=200.000000 maxvel=0.250000 health=5000.00 maxhealth=5000.00 maxthrust=1.00 maxrotthrust=60.00 typeflags=0x180140 aiclass=gonk.ai
# DESC:    
# BBOX: -.05 -.05 -.075 .05 .05 .075
03blocktop        _zwalkstruct       model3d=03blockt.3do size=0.153078 movesize=0.153078
# DESC:    CrossBow Dropped
# BBOX: -.034888 -.055377 .000124 .034343 .059997 .031925
+dcrossbow        _droppowerup       thingflags=0x400 model3d=bowp.3do cog=pow_dcrossbow.cog height=0.003435
# DESC:    
# BBOX: -.042717 -.015112 -.011798 .042717 .015112 .011798
+tklimb           _limb              model3d=tklimb.3do size=0.050000 movesize=0.010000
# DESC:    
# BBOX: -.042717 -.015112 -.011798 .042717 .015112 .011798
+thlimb           _limb              model3d=thlimb.3do size=0.050000 movesize=0.010000
# DESC:    
# BBOX: -.037418 -.063387 -.017067 .025236 .03643 .012634
+mylimb           _limb              model3d=mylimb.3do size=0.070000 movesize=0.010000
# DESC:    
# BBOX: 0 0 0 0 0 0
+wcrate_exp       +crate_exp         damage=20.000000 force=50.000000 range=0.150000
# DESC:    
# BBOX: 0 0 0 0 0 0
+xtank3_exp       +xtank1_exp        sprite=tiex.spr soundclass=exp_tie.snd damage=200.000000 force=200.000000 range=0.600000 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_5
# DESC:    
# BBOX: -.034039 -.029553 -.032378 .034039 .029553 .032378
vapdebris_1       _debris            model3d=v_deb1.3do size=0.040000 movesize=0.040000
# DESC:    
# BBOX: -.00911 -.009437 -.061645 .00911 .009437 .061645
vapdebris_2       _debris            model3d=v_deb2.3do size=0.060000 movesize=0.060000
# DESC:    
# BBOX: -.031695 -.036598 -.014875 .031695 .036598 .014875
vapdebris_3       _debris            model3d=v_deb3.3do size=0.030000 movesize=0.030000
# DESC:    
# BBOX: -.016565 -.043636 -.052678 .016565 .043636 .052678
vapdebris_4       _debris            model3d=v_deb4.3do size=0.050000 movesize=0.050000
# DESC:    
# BBOX: 0 0 0 0 0 0
+vaporator_exp    _explosion         thingflags=0x1 light=0.200000 timer=0.800000 sprite=seqx2.spr soundclass=exp_probe.snd typeflags=0x17 damage=40.000000 blasttime=0.700000 force=120.000000 maxlight=0.800000 range=0.450000 debris=vapdebris_1 debris=vapdebris_2 debris=vapdebris_3 debris=vapdebris_4
# DESC:    
# BBOX: 0 0 0 0 0 0
+03_exp           +raildet_exp       damage=25.000000 force=60.000000 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_4
# DESC:    
# BBOX: -.314105 -.627595 -.074421 .314105 .627595 .074421
trooptran         _zwalkstruct       model3d=ttrn.3do size=0.718795 movesize=0.718795
# DESC:    
# BBOX: -.1 -.748063 -.424992 .1 .748063 .424992
04beam2           _walkstruct        model3d=04beam2.3do size=0.916150 movesize=0.916150
# DESC:    
# BBOX: -.1 -.75 -.1 .1 .75 .1
04beam1           _walkstruct        model3d=04beam1.3do size=0.763217 movesize=0.763217
# DESC:    Door 4x4 Single Drk Grey
# BBOX: -.2 -.0125 -.15 .2 .0125 .25
4x4door           _walkstruct        model3d=d4x4.3do size=0.320400 movesize=0.320400 soundclass=med_door.snd
# DESC:    
# BBOX: -.242583 -.242583 0 .242583 .242583 0
04water           _ghostdecor        model3d=04water.3do size=0.301141 movesize=0.301141
# DESC:    
# BBOX: -.1 -.15 -.0125 .1 .15 .0125
04platform        _walkstruct        model3d=04p2x3.3do size=0.230710 movesize=0.230710 soundclass=sm_door.snd
# DESC:    
# BBOX: 0 0 0 0 0 0
+esequencer_exp   _explosion         thingflags=0x1 light=0.200000 timer=0.800000 sprite=seqx2.spr soundclass=exp_med.snd creatething=+firecloud2 typeflags=0x17 damage=75.000000 blasttime=0.700000 force=300.000000 maxlight=0.800000 range=0.600000 debris=shrapnel2_1 debris=shrapnel2_2 debris=shrapnel2_3 debris=shrapnel2_4
# DESC:    Sequencer Mine Live
# BBOX: -.022216 -.023897 -.005309 .022216 .023897 .005309
sequencermine     +grenade2          timer=0.000000 model3d=seq0.3do size=0.300000 movesize=0.010000 soundclass=seq.snd surfdrag=5.000000 airdrag=1.000000 physflags=0x29c vel=(0.000000/0.000000/0.000000) angvel=(0.000000/0.000000/0.000000) buoyancy=0.500000 explode=+esequencer_exp typeflags=0x1380
# DESC:    Expolding Tank 1 Meter Tall
# BBOX: -.040024 -.034696 -.049915 .040024 .034696 .049915
xtank1a           _walkstruct        thingflags=0x400448 model3d=xtank1a.3do size=0.063985 movesize=0.063985 cog=xtank1.cog
# DESC:    
# BBOX: 0 0 0 0 0 0
_swimactor        _humanactor        physflags=0x4a0f typeflags=0x20040
# DESC:    
# BBOX: 0 0 0 0 0 0
+cycbite          +gamaxe            damage=25.000000 range=0.500000
# DESC:    Octotpus
# BBOX: -.112594 -.066536 -.714146 .146652 .121299 .144462
octopus           _swimactor         model3d=oc.3do size=0.178000 movesize=0.178000 puppet=oc.pup mass=300.000000 physflags=0x404a0f maxvel=0.600000 weapon=+cycbite health=100.00 maxhealth=100.00 maxthrust=0.40 maxrotthrust=60.00 typeflags=0x20140 aiclass=wcdefault.ai
# DESC:    
# BBOX: 0 0 0 0 0 0
_flyactor         _actor             airdrag=2.000000 physflags=0x7606 maxvel=0.800000 maxthrust=0.50
# DESC:    
# BBOX: -.005771 -.015021 -.00466 .005771 .003935 .00466
+remotelaser      +repeaterball      damage=2.000000 mindamage=1.000000
# DESC:    Remote Droid
# BBOX: -.01477 -.014666 -.01477 .01477 .014666 .01477
remote            _flyactor          thingflags=0x20000400 model3d=remo.3do size=0.025000 movesize=0.025000 soundclass=re.snd cog=actor_re.cog mass=15.000000 physflags=0x6a06 maxvel=0.600000 weapon=+remotelaser health=30.00 maxthrust=0.90 maxrotthrust=150.00 typeflags=0x2000120 fireoffset=(0.015000/0.020000/0.000000) explode=+remote_exp aiclass=redefault.ai
# DESC:    
# BBOX: -.490393 -.196424 -.05 .490393 .196424 .05
plathalf          _walkstruct        thingflags=0x48 model3d=plathalf.3do size=0.580629 movesize=0.580629
# DESC:    
# BBOX: -.15 -.0125 -.1 .15 .0125 .1
06door3x2         _walkstruct        thingflags=0x848 model3d=06d3x2.3do size=0.230710 movesize=0.230710 soundclass=03wood_door.snd
# DESC:    
# BBOX: -.024829 -1.295061 -1.126603 .307452 1.295061 .996689
wall_walltrans.   _walkstruct        model3d=walt_a.3do size=1.766691 movesize=1.766691
# DESC:    
# BBOX: -.2875 -.0375 -.05 .2875 .0375 .05
06counter2        _walkstruct        thingflags=0x48 model3d=06count2.3do size=0.344215 movesize=0.344215
# DESC:    
# BBOX: -.2 -.0375 -.05 .2 .0375 .05
06counter         _walkstruct        thingflags=0x48 model3d=06countr.3do size=0.259538 movesize=0.259538
# DESC:    
# BBOX: -.027321 -.032261 .000219 .027321 .032261 .126942
minichair3        _walkstruct        thingflags=0x48 model3d=chair3m.3do size=0.182873 movesize=0.182873
# DESC:    Man No Voice
# BBOX: -.035537 -.014648 -.128737 .035536 .025457 .065502
mman1             man1               cog=actor_mu.cog
# DESC:    
# BBOX: -.132528 -.06827 -.073365 .132528 .06827 .073365
sofa              _zwalkstruct       thingflags=0x800048 model3d=sofa.3do size=0.216153 movesize=0.216153
# DESC:    
# BBOX: -.165589 -.046318 -.208452 .054283 .051379 .208452
06streetlight     _zwalkstruct       model3d=stlt.3do size=0.316625 movesize=0.316625
# DESC:    Flying Guard Droid
# BBOX: -.037102 -.076502 -.041518 .037102 .046066 .03417
robotsentry       _flyactor          thingflags=0x20000400 model3d=rs.3do size=0.055000 movesize=0.055000 soundclass=rs.snd cog=actor_sd.cog mass=30.000000 physflags=0x6a06 maxvel=0.600000 weapon=+bryarbolt health=60.00 maxhealth=60.00 maxthrust=0.80 maxrotthrust=80.00 typeflags=0x2000120 fireoffset=(-0.007300/0.039300/0.000000) explode=+sentry_exp aiclass=sddefault.ai
# DESC:    
# BBOX: -.028121 -.020533 -.054867 .028121 .020533 .055636
chair07           _decor             thingflags=0x48 model3d=chair07.3do size=0.045000 movesize=0.045000
# DESC:    
# BBOX: -.065678 -.075809 -.044765 .065678 .075809 .044765
gametable         _walkstruct        model3d=game.3do size=0.138039 movesize=0.138039
# DESC:    Woman No Voice
# BBOX: -.025123 -.01744 -.127094 .025123 .015562 .056718
mwoman1           woman1             cog=actor_mu.cog
# DESC:    
# BBOX: -.021527 -.011397 -.102055 .021527 .014719 .057565
mwoman2           woman2             cog=actor_mu.cog
# DESC:    
# BBOX: -.031058 -.027406 -.023339 .031058 .027406 .023339
bottles05         _structure         collide=1 model3d=bottles5.3do size=0.050000 movesize=0.050000
# DESC:    
# BBOX: -.122086 -.117592 -.00067 .122086 .117592 .295542
tarp              _walkstruct        thingflags=0x800048 model3d=tarp.3do size=0.390702 movesize=0.390702
# DESC:    
# BBOX: -.100136 -.200678 -.052048 .100126 .200678 .062505
vegtablestand     _walkstruct        thingflags=0x800048 model3d=vegstand.3do size=0.280234 movesize=0.280234
# DESC:    
# BBOX: -.012767 -.012147 -.023339 .012767 .012147 .023339
bottle05          _structure         model3d=bottle5.3do size=0.077465 movesize=0.077465
# DESC:    
# BBOX: -.013049 -.01372 -.021966 .013049 .01372 .021966
streetlightlamp   _structure         model3d=stlamp.3do size=0.076920 movesize=0.076920
# DESC:    
# BBOX: 0 0 0 0 0 0
+drugonbite       +gamaxe            damage=50.000000 range=0.400000
# DESC:    Sucker
# BBOX: -.122593 -.438462 -.119063 .144735 .269351 .112767
sucker            _swimactor         model3d=su.3do size=0.330000 movesize=0.330000 puppet=su.pup soundclass=sucker.snd mass=500.000000 physflags=0x404a0f maxvel=0.600000 weapon=+drugonbite health=160.00 maxhealth=160.00 maxthrust=0.70 maxrotthrust=90.00 typeflags=0x20140 aiclass=dudefault.ai
# DESC:    
# BBOX: -.04933 -.05743 -.000743 .047873 .05743 .104526
minichair2        _walkstruct        thingflags=0x800048 model3d=chair2m.3do size=0.179064 movesize=0.179064
# DESC:    
# BBOX: -.2 -.2 0 .2 .2 0
carpet            _walkstruct        thingflags=0x800048 model3d=carpet.3do size=0.332843 movesize=0.332843
# DESC:    
# BBOX: -.9592 -.9592 -.219578 .9592 .9592 .219578
waterpump         _zwalkstruct       move=physics model3d=wpmp.3do size=1.036299 movesize=1.036299 physflags=0x200 maxrotvel=25.000000
# DESC:    Dynamic Light 0.0
# BBOX: 0 0 0 0 0 0
light0.0          _ghostdecor        thingflags=0x1
# DESC:    Max The Rabbit Bryar Pistol
# BBOX: -.007791 -.090587 -.006258 .007791 .017998 .006258
+mbolt            +bryarbolt         model3d=bry1.3do damage=20.000000
# DESC:    Max The Rabbit 
# BBOX: -.042385 -.021003 -.058578 .03291 .037974 .073027
max               _humanactor        model3d=mx.3do size=0.060000 movesize=0.060000 puppet=mx.pup soundclass=mx.snd mass=60.000000 maxvel=0.400000 weapon=+mbolt health=5000.00 maxhealth=5000.00 typeflags=0x1080000 fireoffset=(0.007000/0.048000/0.008000) aiclass=max.ai
# DESC:    
# BBOX: -.013049 -.01372 .002105 .013049 .01372 .021966
streetlightlamp2  _ghoststructure    model3d=stlamp1.3do size=0.076920 movesize=0.076920
# DESC:    
# BBOX: -.964666 -.781463 -.554109 .627416 .781463 1.699674
tran_walltrans.   _ghostdecor        model3d=walt_b.3do size=1.906994 movesize=1.906994
# DESC:    
# BBOX: -.063767 -.055224 -.021878 .063767 .055224 .021878
probedebris_1     _debris            model3d=pb_1.3do size=0.053500 movesize=0.053500
# DESC:    
# BBOX: -.071973 -.062331 -.029124 .071973 .062331 .029124
probedebris_2     _debris            model3d=pb_2.3do size=0.057000 movesize=0.057000
# DESC:    
# BBOX: -.009722 -.012251 -.047823 .009722 .012251 .047823
probedebris_3     _debris            model3d=pb_3.3do size=0.041500 movesize=0.041500
# DESC:    
# BBOX: -.048787 -.012879 -.031716 .048787 .012879 .031716
probedebris_4     _debris            model3d=pb_4.3do size=0.040000 movesize=0.040000
# DESC:    
# BBOX: 0 0 0 0 0 0
+probedroid_exp   _explosion         thingflags=0x1 light=0.200000 timer=0.800000 sprite=seqx.spr soundclass=exp_probe.snd typeflags=0x17 damage=40.000000 blasttime=0.700000 force=200.000000 maxlight=0.800000 range=0.450000 debris=probedebris_1 debris=probedebris_2 debris=probedebris_3 debris=probedebris_4
# DESC:    
# BBOX: -.01341 -.149802 -.01341 .01341 .015603 .01341
+turretlaser      +stlaser           light=0.500000 model3d=con0.3do size=0.005000 movesize=0.005000 vel=(0.000000/5.000000/0.000000) fleshhit=+laserhit damage=15.000000 mindamage=5.000000
# DESC:    
# BBOX: -.007791 -.090587 -.006258 .007791 .017998 .006258
+probedroidbolt   +turretlaser       model3d=bry0.3do vel=(0.000000/4.000000/0.000000) damage=30.000000 mindamage=10.000000
# DESC:    ProbeDroid
# BBOX: -.074701 -.093381 -.235684 .071943 .06479 .140417
probedroid        _flyactor          thingflags=0x20000400 model3d=pb.3do size=0.078000 movesize=0.078000 puppet=pb.pup soundclass=pb.snd cog=actor_pd.cog mass=300.000000 physflags=0x6a06 maxvel=0.500000 weapon=+probedroidbolt health=90.00 maxhealth=90.00 maxthrust=0.60 maxrotthrust=150.00 typeflags=0x2000120 fireoffset=(0.037500/0.065000/0.000000) explode=+probedroid_exp aiclass=pddefault.ai
# DESC:    
# BBOX: -.2 -.0125 -.15 .2 .0125 .25
07_4x4door        _structure         model3d=07d4x4.3do size=0.370400 movesize=0.370400 soundclass=med_door.snd
# DESC:    
# BBOX: -.100779 -.107544 .000686 .100779 .107544 .023173
ceilinglight      _structure         thingflags=0x9 light=0.500000 model3d=clight.3do size=0.116487 movesize=0.116487
# DESC:    
# BBOX: -.1 -.0125 -.15 .1 .0125 .15
2x3door_4         _structure         model3d=d2x3_4.3do size=0.180710 movesize=0.180710 soundclass=sm_door.snd
# DESC:    
# BBOX: -.75 -.2 -.75 .75 .2 .75
06solidlockdoor   _walkstruct        thingflags=0x48 model3d=06lock_1.3do size=1.129352 movesize=1.129352 soundclass=06stone_door.snd
# DESC:    
# BBOX: -.75 -.2 -.85 .75 .2 .85
06holeylockdoor   _walkstruct        thingflags=0x48 model3d=06lock_2.3do size=1.201086 movesize=1.201086 soundclass=06stone_door.snd
# DESC:    
# BBOX: 0 0 0 0 0 0
+turret_exp       _explosion         thingflags=0x1 light=0.200000 timer=0.800000 sprite=detx.spr soundclass=exp_probe.snd typeflags=0x17 damage=10.000000 blasttime=0.700000 force=100.000000 maxlight=0.800000 range=0.450000 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_5
# DESC:    
# BBOX: -.01341 -.149802 -.01341 .01341 .015603 .01341
+turretlaser2     +turretlaser       vel=(0.000000/4.000000/0.000000) damage=25.000000 mindamage=10.000000
# DESC:    Turret Gun
# BBOX: -.059999 -.051358 -.040791 .060835 .061748 .050725
boxturret         none               orient=(0.000000/0.000000/0.000000) type=actor collide=1 move=physics thingflags=0x400 model3d=tur1.3do size=0.075000 movesize=0.075000 puppet=tur1.pup soundclass=turret.snd cog=class_boxturret.cog physflags=0x400000 weapon=+turretlaser2 health=80.00 maxhealth=80.00 typeflags=0x2080160 fireoffset=(0.000000/-0.003000/0.036000) explode=+turret_exp aiclass=turbdefault.ai
# DESC:    
# BBOX: -.35 -.15 -.55 .35 .15 .55
06thirddoor       _walkstruct        thingflags=0x48 model3d=06door3.3do size=0.718954 movesize=0.718954 soundclass=06stone_door.snd
# DESC:    
# BBOX: -.13965 -1.25 -.069825 .13965 .05 .069825
06bolt            _walkstruct        collide=1 model3d=06bolt.3do size=0.100000 movesize=0.100000 soundclass=06bolt.snd
# DESC:    Imperial Officer
# BBOX: -.037458 -.014968 -.120277 .037328 .026217 .066384
iofficer          _humanactor        thingflags=0x20000400 model3d=io.3do size=0.066700 movesize=0.066700 puppet=io.pup soundclass=io.snd cog=actor_io.cog maxvel=0.300000 weapon=+ebolt health=30.00 maxthrust=0.90 maxrotthrust=80.00 typeflags=0x1 fireoffset=(0.050000/0.065000/0.014000) aiclass=iodefault.ai
# DESC:    Stormtrooper
# BBOX: -.029786 -.019179 -.124701 .024091 .064673 .043406
stormtroop        _humanactor        thingflags=0x20000400 model3d=st.3do puppet=st.pup soundclass=st.snd cog=actor_st.cog maxvel=0.250000 weapon=+elaser health=60.00 maxhealth=60.00 maxthrust=1.00 maxrotthrust=90.00 typeflags=0x1 fireoffset=(0.004800/0.050000/0.012000) aiclass=stdefault.ai
# DESC:    Imperial Officer w diff Voice
# BBOX: -.037458 -.014968 -.120277 .037328 .026217 .066384
iofficer2         iofficer           soundclass=io2.snd
# DESC:    
# BBOX: -.095 -.2 -.095 .095 .2 .095
06lock_x          _structure         model3d=06lock_x.3do size=0.290936 movesize=0.290936
# DESC:    
# BBOX: -.2375 -.2 -.095 .2375 .2 .095
06lock_y          _structure         model3d=06lock_y.3do size=0.374702 movesize=0.374702
# DESC:    Door 2x3m Single w Orange warning on right side
# BBOX: -.1 -.0125 -.15 .1 .0125 .15
2x3door_1         _structure         model3d=d2x3_1.3do size=0.180710 movesize=0.180710 soundclass=sm_door.snd
# DESC:    
# BBOX: -.005653 -.113701 -.004853 .005653 .02259 .004853
+lasercannon      +stlaser           light=0.500000 size=0.015000 movesize=0.015000 vel=(0.000000/4.000000/0.000000) fleshhit=+laserhit damage=40.000000
# DESC:    AT-ST with Commando
# BBOX: -.161624 -.239125 -.45412 .160571 .250864 .223596
atst              _humanactor        thingflags=0x4 model3d=at.3do size=0.300000 movesize=0.300000 puppet=atst.pup soundclass=atst.snd cog=actor_atst.cog mass=0.000000 maxvel=0.400000 weapon=+lasercannon health=400.00 maxhealth=400.00 maxthrust=0.30 maxrotthrust=10.00 typeflags=0x100 fireoffset=(0.000000/0.200000/0.000000) aiclass=atstdefault.ai
# DESC:    AT-ST with Officer
# BBOX: -.161624 -.239125 -.45412 .160571 .250864 .223596
iatst             atst               thingflags=0x404 cog=actor_iatst.cog
# DESC:    
# BBOX: -.6 -2.1 -.048 .6 .2 .048
06archdrawbridge  _walkstruct        model3d=08dbrg.3do size=2.234560 movesize=2.234560 soundclass=06drawbridge.snd
# DESC:    
# BBOX: -.6 -2.1 -.048 .6 .2 .048
06drawbridge      _zwalkstruct       model3d=08dbrg.3do size=2.234560 movesize=2.234560 soundclass=06drawbridge.snd
# DESC:    Sequencer Mines Pickup
# BBOX: -.019886 -.022184 0 .019886 .022184 .042868
seqcharge         _powerup           thingflags=0x400 model3d=seqp.3do cog=pow_sequencer.cog
# DESC:    Expolding Tank 4 Meters Tall
# BBOX: -.160097 -.138783 -.199662 .160096 .138783 .199662
xtank4a           _walkstruct        thingflags=0x400448 model3d=xtank4a.3do size=0.255942 movesize=0.255942 cog=xtank4.cog
# DESC:    Weapon SuperCharge Pickup 
# BBOX: -.016734 -.016734 -.026596 .016734 .016734 .026596
powerboost        _powerup           thingflags=0x400 model3d=boost.3do cog=pow_powerboost.cog respawn=60.000000
# DESC:    21B Medical Droid 
# BBOX: -.034484 -.022835 -.120264 .045456 .021911 .070408
twoonebee         none               orient=(0.000000/0.000000/0.000000) type=cog collide=1 thingflags=0x400 model3d=med.3do size=0.060000 movesize=0.060000 puppet=cr.pup cog=00_twoonebee.cog
# DESC:    Expolding Fuel Tank 4 Meters Tall
# BBOX: -.160097 -.138783 -.199662 .160096 .138783 .199662
xtank4rd          xtank4a            cog=xtank4rd.cog
# DESC:    Stormtrooper Special Seeing Ai
# BBOX: -.029786 -.019179 -.124701 .024091 .064673 .043406
20storm           stormtroop         aiclass=st2020.ai
# DESC:    Stormtrooper Roaming
# BBOX: -.029786 -.019179 -.124701 .024091 .064673 .043406
rstorm            stormtroop         aiclass=stroam.ai
# DESC:    
# BBOX: -.012585 -.050304 -.010582 .022579 .027638 .007152
+stormlimb        _limb              size=0.050000 movesize=0.010000
# DESC:    Imperial Commando
# BBOX: -.037893 -.021815 -.12043 .03746 .022321 .066407
icommando         _humanactor        thingflags=0x20000400 model3d=ic.3do size=0.065950 movesize=0.065950 puppet=ic.pup soundclass=ic.snd cog=actor_ic.cog maxvel=0.300000 weapon=+elaser health=90.00 maxhealth=90.00 maxthrust=1.10 maxrotthrust=90.00 typeflags=0x1 fireoffset=(0.014000/0.059000/0.031000) aiclass=icdefault.ai
# DESC:    Enemy Repeater Gun
# BBOX: -.005771 -.015021 -.00466 .005771 .003935 .00466
+eball            +repeaterball      damage=8.000000 mindamage=4.000000
# DESC:    Imperial Commando w Repeatergun
# BBOX: -.037893 -.02023 -.12043 .03746 .022321 .065552
icommandorpt      icommando          model3d=icrpt.3do soundclass=icrpt.snd cog=actor_i2.cog weapon=+eball aiclass=icreapdefault.ai
# DESC:    
# BBOX: 0 0 0 0 0 0
+atst_exp         +grenade_exp       damage=10.000000
# DESC:    
# BBOX: 0 0 0 0 0 0
+xtank4rd_exp     +xtank4_exp        damage=1000.000000 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_5
# DESC:    Repeater Gun Dropped
# BBOX: -.00548 -.051379 -.021767 .00548 .039969 .012035
+drepeatergun     _droppowerup       thingflags=0x400 model3d=rptp.3do cog=pow_drepeater.cog height=0.042069
# DESC:    
# BBOX: 0 0 0 0 0 0
_darkjedi         _humanactor        typeflags=0x2000201
# DESC:    DarkJedi Yun
# BBOX: -.031992 -.017003 -.134424 .032032 .024487 .073659
yun               _darkjedi          model3d=yu.3do size=0.060400 movesize=0.060400 puppet=yu.pup soundclass=yu.snd mass=160.000000 maxvel=0.300000 weapon=+gamaxe health=500.00 maxhealth=500.00 maxrotthrust=200.00 jumpspeed=1.60 error=0.60 fov=0.50 chance=1.00 aiclass=yun.ai
# DESC:    
# BBOX: -.25 -.025 -.2 .25 .025 .2
07door5x4         _walkstruct        model3d=07d5x4.3do size=0.371131 movesize=0.371131 soundclass=med_door.snd
# DESC:    
# BBOX: -.198658 -.198003 -.001357 .197787 .198497 .598643
07lift            _walkstruct        thingflags=0x48 model3d=07lift.3do size=0.711240 movesize=0.711240 soundclass=med_elev.snd
# DESC:    
# BBOX: -.165685 -.006493 -.2 .165685 .006493 .2
tridoor           _walkstruct        thingflags=0x48 model3d=trid.3do size=0.309796 movesize=0.309796 soundclass=med_door.snd
# DESC:    
# BBOX: -.092328 -.184027 -.04612 .092328 .184027 .04612
table07           _walkstruct        thingflags=0x48 model3d=table07.3do size=0.260992 movesize=0.260992
# DESC:    Camera Droid
# BBOX: -.037102 -.076502 -.041518 .037102 .046066 .03417
droidcam          _decor             collide=0 model3d=rs.3do size=0.127630 movesize=0.127630 puppet=cr.pup
# DESC:    
# BBOX: -.092357 -.112123 -.045808 .092357 .112123 .045808
table07a          _walkstruct        thingflags=0x48 model3d=table07b.3do size=0.202315 movesize=0.202315
# DESC:    
# BBOX: -.092357 -.109937 -.045808 .092357 .109937 .045808
table07b          _walkstruct        thingflags=0x48 model3d=table07a.3do size=0.200712 movesize=0.200712
# DESC:    
# BBOX: 0 0 0 0 0 0
+furniture_exp    _explosion         thingflags=0x1 light=0.200000 timer=0.800000 sprite=detx.spr soundclass=exp_small.snd creatething=+firecloud typeflags=0x17 blasttime=0.700000 force=100.000000 maxlight=0.800000 range=0.450000
# DESC:    Turret Gun Round                                              
# BBOX: -.058293 -.078937 -.033443 .058195 .084676 .035083
roundturret       none               orient=(0.000000/0.000000/0.000000) type=actor collide=1 move=physics thingflags=0x400 model3d=tur2.3do size=0.080000 movesize=0.080000 puppet=tur2.pup soundclass=turret.snd cog=class_roundturret.cog physflags=0x400000 weapon=+turretlaser health=100.00 maxhealth=100.00 typeflags=0x2080160 fireoffset=(0.000000/-0.028000/0.022000) explode=+turret_exp aiclass=turdefault.ai
# DESC:    
# BBOX: -.15 -.3 -.012306 .15 .3 .012306
3x6catwalk        _walkstruct        model3d=c3x6.3do size=0.335636 movesize=0.335636
# DESC:    FieldTrooper
# BBOX: -.03619 -.019179 -.124701 .024091 .064673 .043406
stormtroop2       _humanactor        thingflags=0x20000400 model3d=s2.3do puppet=st.pup soundclass=s2.snd cog=actor_st.cog maxvel=0.250000 weapon=+elaser health=70.00 maxhealth=70.00 maxthrust=0.90 maxrotthrust=90.00 typeflags=0x1 fireoffset=(0.004800/0.050000/0.012000) aiclass=stdefault.ai
# DESC:    StormTrooper W Repeater
# BBOX: -.03619 -.019179 -.124701 .024091 .063953 .043406
stormtroop3       _humanactor        thingflags=0x20000400 model3d=s3.3do puppet=s3.pup soundclass=s3.snd cog=actor_i2.cog maxvel=0.250000 weapon=+eball health=70.00 maxhealth=70.00 maxthrust=0.90 maxrotthrust=90.00 typeflags=0x1 fireoffset=(0.009800/0.057000/0.010000) aiclass=streapdefault.ai
# DESC:    
# BBOX: 0 0 0 0 0 0
+eraildet_exp     +raildet_exp       damage=50.000000 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_4
# DESC:    Enemy Rail Detonator
# BBOX: -.01058 -.013704 -.006548 .010593 .009939 .012329
+eraildet         +raildet2          vel=(0.000000/2.000000/0.000000) explode=+eraildet_exp fleshhit=+eraildet_exp damage=20.000000 typeflags=0xb81
# DESC:    
# BBOX: 0 0 0 0 0 0
+smack            +gamaxe            damage=15.000000 range=0.150000
# DESC:    StormTrooper W Railgun
# BBOX: -.03619 -.019179 -.124701 .024091 .08689 .043406
stormtroop4       _humanactor        thingflags=0x20000400 model3d=s4.3do puppet=s2.pup soundclass=s4.snd cog=actor_s3.cog maxvel=0.250000 weapon=+eraildet weapon2=+smack health=70.00 maxhealth=70.00 maxthrust=0.90 maxrotthrust=90.00 typeflags=0x20001 fireoffset=(0.014000/0.053200/0.013000) aiclass=straildefault.ai
# DESC:    
# BBOX: 0 0 0 0 0 0
+kellbite         +gamaxe            damage=50.000000 range=0.300000 force=100.000000
# DESC:    Kell Dragon
# BBOX: -.210462 -.473958 -.126659 .211246 .183447 .10657
kelldragon        _humanactor        thingflags=0x404 model3d=kd.3do size=0.125000 movesize=0.125000 puppet=kd.pup soundclass=kd.snd cog=actor_kd.cog mass=3000.000000 maxvel=0.600000 weapon=+kellbite health=500.00 maxhealth=500.00 maxthrust=1.00 maxrotthrust=120.00 typeflags=0x2028100 aiclass=kddefault.ai
# DESC:    
# BBOX: -.024596 -.024511 -.064105 .024596 .024511 .064105
console1          _walkstruct        model3d=con1.3do size=0.072905 movesize=0.072905
# DESC:    
# BBOX: -.817281 -1.132091 -.454926 .815909 1.276639 1.852229
shuttle           _decor             model3d=shut.3do size=1.000000 movesize=1.000000 puppet=cr.pup
# DESC:    
# BBOX: 0 0 0 0 0 0
+tiebomb_exp      _explosion         thingflags=0x1 light=0.200000 timer=0.800000 sprite=tiex.spr soundclass=exp_tie.snd typeflags=0x17 damage=100.000000 blasttime=0.700000 force=300.000000 maxlight=0.800000 range=1.000000
# DESC:    
# BBOX: 0 0 0 0 0 0
08tiebomb         _weapon            sprite=tiebomb.spr soundclass=det.snd surfdrag=3.000000 airdrag=0.800000 mass=1.000000 physflags=0x225 vel=(0.000000/0.000000/-1.000000) angvel=(90.000000/45.000000/90.000000) explode=+tiebomb_exp damageclass=0x1 typeflags=0x30d
# DESC:    
# BBOX: -.549776 -.490931 -.315187 .549776 .4752 .320955
08tiebomber       _decor             model3d=tieb.3do size=0.300000 movesize=0.300000 puppet=cr.pup
# DESC:    
# BBOX: 0 0 0 0 0 0
+extiebomb_exp    _explosion         thingflags=0x1 light=0.200000 timer=0.800000 sprite=tiex.spr soundclass=exp_tie.snd typeflags=0x17 blasttime=0.700000 maxlight=0.800000
# DESC:    
# BBOX: 0 0 0 0 0 0
08extiebomb       _weapon            sprite=tiebomb.spr soundclass=det.snd surfdrag=3.000000 airdrag=0.800000 mass=1.000000 physflags=0x225 vel=(0.000000/0.000000/-1.000000) angvel=(90.000000/45.000000/90.000000) explode=+extiebomb_exp damageclass=0x1 typeflags=0x30d
# DESC:    RailGun Dropped
# BBOX: -.018484 -.070714 -.030025 .019883 .081826 .013227
+drailgun         _droppowerup       thingflags=0x400 model3d=rldp.3do cog=pow_drailgun.cog height=0.044420
# DESC:    
# BBOX: 0 0 0 0 0 0
+tie_exp          _explosion         thingflags=0x1 light=0.200000 timer=0.800000 sprite=tiex.spr soundclass=exp_tie.snd typeflags=0x17 damage=20.000000 blasttime=0.700000 force=300.000000 maxlight=0.800000 range=1.000000
# DESC:    
# BBOX: -.255833 -.483066 -.318071 .255833 .483066 .318071
tieb_debris_1     _debris            model3d=tieb_1.3do size=0.553984 movesize=0.553984
# DESC:    
# BBOX: -.123836 -.490931 -.315187 .123836 .4752 .320955
tieb_debris_2     _debris            model3d=tieb_2.3do size=0.514754 movesize=0.514754
# DESC:    
# BBOX: -.204306 -.300444 -.149529 .204306 .284713 .155297
tieb_debris_3     _debris            model3d=tieb_3.3do size=0.371837 movesize=0.371837
# DESC:    
# BBOX: -.048269 -.051853 -.059699 .048269 .036122 .065467
tieb_debris_4     _debris            model3d=tieb_4.3do size=0.083231 movesize=0.083231
# DESC:    
# BBOX: 0 0 0 0 0 0
+tieb_exp         +tie_exp           debris=tieb_debris_1 debris=tieb_debris_2 debris=tieb_debris_3 debris=tieb_debris_4
# DESC:    
# BBOX: -.15 -.5 -.012306 .15 .5 .012306
c3x10e            _walkstruct        model3d=c3x10e.3do size=0.522160 movesize=0.522160
# DESC:    
# BBOX: -.175 -.025 -.175 .175 .025 .175
09_octdoor        _structure         thingflags=0xc model3d=09doct1.3do size=0.192040 movesize=0.192040 soundclass=med_door.snd
# DESC:    
# BBOX: -.175 -.8 -.063278 .175 .8 .063278
pontoon           _zwalkstruct       model3d=pontoon.3do size=0.871358 movesize=0.871358
# DESC:    
# BBOX: -.1 -.0125 -.1 .1 .0125 .1
09_2x2door        _structure         model3d=09d2x2_1.3do size=0.141973 movesize=0.141973 soundclass=sm_door.snd
# DESC:    
# BBOX: -.1 -.0125 -.15 .1 .0125 .15
09_2x3door1       _structure         model3d=09d2x3_1.3do size=0.180710 movesize=0.180710 soundclass=sm_door.snd
# DESC:    
# BBOX: -2 -2.000001 -4.00002 2 2.000001 4.00002
09tank_r20        _structure         thingflags=0xc model3d=09tnk.3do size=4.472154 movesize=4.472154
# DESC:    
# BBOX: -.078121 -.099868 -.090436 .078121 .099868 .090436
09pipebrace       _structure         model3d=09pipb.3do size=0.155740 movesize=0.155740
# DESC:    
# BBOX: -1.000702 -.2 -.2 1.000702 .2 .2
09pipe_18         _structure         thingflags=0xc model3d=09pipe18.3do size=1.020492 movesize=1.020492
# DESC:    
# BBOX: -.4 -.025 -.4 .4 .025 .4
rounddoor4        _walkstruct        thingflags=0x400848 model3d=dr4.3do size=0.400781 movesize=0.400781
# DESC:    
# BBOX: -1.440012 -.1 -1.440018 1.439986 .1 -.000017
sewerdoor         _walkstruct        model3d=dr13half.3do size=1.471634 movesize=1.471634 soundclass=lg_door.snd
# DESC:    
# BBOX: -.6 -.075 -.3 .6 .075 .3
halfcircledoor    _walkstruct        model3d=dr6half.3do size=0.675000 movesize=0.675000 soundclass=med_door.snd
# DESC:    
# BBOX: -.5 -.2375 -.1875 .5 .2375 .1875
door10x3          _walkstruct        model3d=d10x375.3do size=0.634434 movesize=0.634434 soundclass=09accessdoor.snd
# DESC:    
# BBOX: -.45 -.0375 -.15 .45 .0375 .15
door9x3           _walkstruct        model3d=d9x3.3do size=0.525822 movesize=0.525822 soundclass=09accessdoor.snd
# DESC:    Expolding Tank 3 Meters Tall
# BBOX: -.120072 -.104087 -.149746 .120072 .104087 .149746
xtank3a           _walkstruct        thingflags=0x400448 model3d=xtank3a.3do size=0.191956 movesize=0.191956 cog=xtank3.cog
# DESC:    
# BBOX: -35.429691 -4.751811 -9.361874 35.429691 4.75181 9.335024
09cargoship       _ghoststructure    model3d=cargo2.3do size=35.991570 movesize=35.991570
# DESC:    
# BBOX: -6.669081 -.681354 -1.103577 6.66908 .681354 1.103577
09cargocatwalk    _walkstruct        model3d=cargowlk.3do size=6.844024 movesize=6.844024 soundclass=09cargocatwalk.snd
# DESC:    Yellow Key Pickup
# BBOX: -.010023 -.016964 -.033841 .010023 .016964 .033841
keyyellow         _powerup           thingflags=0x400 model3d=y-key.3do cog=pow_keyyellow.cog
# DESC:    Ugnaut 
# BBOX: -.035186 -.01745 -.100835 .035171 .022972 .063971
ug                _civilian          thingflags=0x20000400 model3d=ugalt.3do size=0.064000 movesize=0.064000 puppet=ug.pup soundclass=ug.snd cog=actor_ug.cog mass=60.000000 maxvel=0.400000 health=30.00 maxthrust=0.50 maxrotthrust=60.00 aiclass=peddefault.ai
# DESC:    Ugnaut Sitting
# BBOX: -.035186 -.01745 -.100835 .035171 .022972 .063971
ugsit             ug                 puppet=ugsit.pup mass=200.000000 aiclass=pedhome.ai
# DESC:    Enemy Concussion Rifle
# BBOX: 0 0 0 0 0 0
+econc_exp        +conc_exp          damage=60.000000
# DESC:    
# BBOX: -.01341 -.149802 -.01341 .01341 .015603 .01341
+econcbullet      +concbullet        explode=+econc_exp fleshhit=+econc_exp damage=10.000000
# DESC:    
# BBOX: 0 0 0 0 0 0
+bosskswipe       +gamaxe            damage=20.000000 range=0.200000
# DESC:    Bossk w Concussion Rifle
# BBOX: -.040341 -.017317 -.123441 .040266 .112852 .068602
bossk             _humanactor        thingflags=0x20000400 model3d=bs.3do size=0.068100 movesize=0.068100 puppet=bs.pup soundclass=bs.snd cog=actor_bs.cog mass=120.000000 maxvel=0.400000 weapon=+econcbullet weapon2=+bosskswipe health=120.00 maxhealth=120.00 maxthrust=0.90 maxrotthrust=80.00 typeflags=0x20a0001 fireoffset=(-0.039000/0.070000/0.027000) aiclass=bodefault.ai
# DESC:    Ugnaut w wrench
# BBOX: -.033249 -.025869 -.100835 .033234 .06466 .062592
u2                ug                 model3d=u2alt.3do puppet=u2.pup aiclass=pedhome.ai
# DESC:    Bossk w RailDetonator
# BBOX: -.040341 -.017317 -.123441 .041996 .078346 .068602
bosskrld          bossk              model3d=bsrld.3do soundclass=bsrld.snd cog=actor_s3.cog weapon=+eraildet fireoffset=(-0.035000/0.070000/0.027000) aiclass=bldefault.ai
# DESC:    Wrench Pickup 
# BBOX: -.032096 -.017684 -.038933 .032096 .017684 .038933
wrench            _powerup           thingflags=0x400 model3d=t-key.3do cog=pow_wrench.cog
# DESC:    
# BBOX: -.3 -.6 -.1 .3 1.8 .1
09tribolt         _walkstruct        thingflags=0x4c model3d=09bogus.3do size=1.875299 movesize=1.875299 soundclass=09pipes.snd
# DESC:    
# BBOX: -.0875 -3.1 0 .0875 0 0
09slice           _ghoststructure    model3d=09slice.3do size=3.151235 movesize=3.151235
# DESC:    
# BBOX: -.55 -.092959 -.28125 .55 .092959 .28125
door11x5          _walkstruct        model3d=d11x5625.3do size=0.674694 movesize=0.674694 soundclass=09accessdoor.snd
# DESC:    RailGun Ammo Pickup
# BBOX: -.01636 -.014643 -.011603 .017727 .017747 .016258
railcharges       _powerup           thingflags=0x400 model3d=rcrg.3do cog=pow_railcharges.cog
# DESC:    
# BBOX: -.4 -.025 -.4 .4 .025 .4
rounddoor4hole    _walkstruct        thingflags=0x400848 model3d=dr4hole.3do size=0.450781 movesize=0.450781
# DESC:    Concussion Rifle Dropped
# BBOX: -.030347 -.072704 -.024787 .009211 .072704 .024341
+dconcrifle       _droppowerup       thingflags=0x400 model3d=conp.3do cog=pow_dconcrifle.cog height=0.024753
# DESC:    
# BBOX: -.043832 -.019016 -.007017 .043832 .019016 .007017
+bslimb           _limb              model3d=bslimb.3do size=0.050000 movesize=0.010000
# DESC:    
# BBOX: -.3 -.025 -.8 .3 .025 .8
mndr              _walkstruct        model3d=mndr.3do size=0.904766 movesize=0.904766
# DESC:    Door Right Side of 3x3 Drk Grey with Lt Grey
# BBOX: -.05 -.0125 -.15 .1 .0125 .15
3x3door_2a        _structure         model3d=d3x3_2a.3do size=0.230710 movesize=0.230710 soundclass=med_door.snd
# DESC:    Door Left Side of 3x3 Drk Grey with Lt Grey
# BBOX: -.1 -.0125 -.15 .05 .0125 .15
3x3door_2b        _structure         model3d=d3x3_2b.3do size=0.230710 movesize=0.230710
# DESC:    
# BBOX: -.15 -.0125 -.15 .15 .0125 .15
10_3x3door        _structure         model3d=10d3x3.3do size=0.262500 movesize=0.262500 soundclass=med_door.snd
# DESC:    Door 1/2 3x3  same as 3x3door_2a w angled edges
# BBOX: -.075 -.1025 -.15 .075 .1025 .15
odd_door_a        _structure         model3d=d3x3odda.3do size=0.246548 movesize=0.246548 soundclass=med_door.snd
# DESC:    Door 1/2 3x3  same as 3x3door_2b w angled edges
# BBOX: -.075 -.1025 -.15 .075 .1025 .15
odd_door_b        _structure         model3d=d3x3oddb.3do size=0.246548 movesize=0.246548
# DESC:    
# BBOX: -.016314 -.219705 -.118596 .016314 .223171 .249539
10fanfast         none               orient=(0.000000/0.000000/0.000000) type=cog collide=3 move=physics model3d=fan0.3do size=0.299843 movesize=0.299843 physflags=0x200 maxrotvel=400.000000 angvel=(300.000000/0.000000/0.000000)
# DESC:    
# BBOX: -.15 -1.875 -.012306 .15 1.875 .012306
3x37catwalk       _walkstruct        model3d=c3x37.3do size=1.881031 movesize=1.881031
# DESC:    
# BBOX: -.125 -.1625 -.25 .125 .1625 .25
10_door1a         _walkstruct        model3d=10door1a.3do size=0.323313 movesize=0.323313 soundclass=med_door.snd
# DESC:    
# BBOX: -.125 -.1625 -.25 .125 .1625 .25
10_door1b         _walkstruct        model3d=10door1b.3do size=0.323313 movesize=0.323313 soundclass=med_door.snd
# DESC:    
# BBOX: -.25 -.1125 -.25 .25 .1125 .25
10_door2          _walkstruct        model3d=10door2.3do size=0.371021 movesize=0.371021 soundclass=med_door.snd
# DESC:    
# BBOX: -.1 -.0925 -.2 .1 .0925 .2
10_door3          _walkstruct        model3d=10door3.3do size=0.241984 movesize=0.241984 soundclass=med_door.snd
# DESC:    
# BBOX: -.75 -.515 -.475 .75 .515 .475
12_door1          _structure         model3d=12dr01.3do size=1.076329 movesize=1.076329 soundclass=lg_door.snd
# DESC:    
# BBOX: -.325 -.0125 -.25 .325 .0125 .25
10_door4          _walkstruct        model3d=10door4.3do size=0.460221 movesize=0.460221 soundclass=lg_door.snd
# DESC:   
# BBOX: -.15 -.025 -.1 .15 .025 .1
3x2door_2         _zwalkstruct       model3d=d3x2_2.3do size=0.232003 movesize=0.232003 soundclass=sm_door.snd
# DESC:    Dynamic Light 0.1
# BBOX: 0 0 0 0 0 0
light0.1          _ghostdecor        thingflags=0x1 light=0.100000
# DESC:    Blue Key Pickup
# BBOX: -.010023 -.016964 -.033841 .010023 .016964 .033841
keyblue           _powerup           thingflags=0x400 model3d=b-key.3do cog=pow_keyblue.cog
# DESC:    
# BBOX: -.025 -.5 -.025 .025 .5 .025
10_beam           _zwalkstruct       model3d=10beam10.3do size=0.551248 movesize=0.551248 soundclass=lg_elev.snd
# DESC:    
# BBOX: -.025 -.375 -.025 .025 .375 .025
10_beam9          _zwalkstruct       model3d=10beam09.3do size=0.426663 movesize=0.426663 soundclass=lg_elev.snd
# DESC:    R2 Droid
# BBOX: -.035317 -.025605 -.047024 .035366 .022343 .048893
r2                _droidactor        thingflags=0x20000400 model3d=r2.3do size=0.058000 movesize=0.058000 puppet=r2.pup soundclass=r2.snd cog=actor_r2.cog maxvel=0.400000 health=50.00 maxhealth=50.00 maxthrust=0.40 maxrotthrust=50.00 typeflags=0x180140 aiclass=drdefault.ai
# DESC:    
# BBOX: -.016314 -.219705 -.118596 .016314 .223171 .249539
10fanstop         none               orient=(0.000000/0.000000/0.000000) type=cog collide=3 move=physics model3d=fan0.3do size=0.299843 movesize=0.299843 physflags=0x200 maxrotvel=400.000000
# DESC:    
# BBOX: 0 0 0 0 0 0
+superenerg       none               orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=0.600000 vel=(0.000000/0.000000/2.000000) angvel=(0.000000/360.000000/0.000000) typeflags=0x3e material=00red.mat range=1.200000 elementsize=0.020000 count=30
# DESC:    
# BBOX: -1.2 -1.8 -.3 1.2 1.8 .3
tetriscrate       _walkstruct        model3d=00tetris.3do size=2.234033 movesize=2.234033
# DESC:    DarkJedi Gorc
# BBOX: -.085744 -.044203 -.133945 .084923 .128448 .082581
gorc              _darkjedi          model3d=go.3do size=0.082800 movesize=0.082800 puppet=go.pup soundclass=go.snd airdrag=0.400000 mass=280.000000 maxvel=0.300000 weapon=+gamaxe health=800.00 maxhealth=800.00 maxrotthrust=200.00 jumpspeed=1.60 error=0.40 fov=0.50 chance=1.00 aiclass=gorc.ai
# DESC:    DarkJedi Pic
# BBOX: -.028706 -.013615 -.072261 .024789 .02181 .039997
pic               _darkjedi          model3d=pi.3do size=0.040050 movesize=0.040050 puppet=pi.pup soundclass=pi.snd mass=75.000000 maxvel=0.200000 weapon=+gamaxe health=600.00 maxhealth=600.00 maxthrust=1.50 jumpspeed=0.75 error=0.50 fov=0.40 chance=0.80 aiclass=pic.ai
# DESC:    8t88 Sitting
# BBOX: -.018951 -.01169 -.121405 .032317 .017665 .054771
8t88_sittingbody  _decor             model3d=8tsit.3do size=0.060000 movesize=0.060000
# DESC:    8t88 Head
# BBOX: -.005748 -.007884 -.009493 .005748 .007884 .009493
8t88_head         none               orient=(0.000000/0.000000/0.000000) type=cog collide=1 model3d=8thead.3do size=0.061601 movesize=0.061601
# DESC:    
# BBOX: -.098 -.0125 -.15 .098 .0125 .15
12door2x3         _structure         model3d=12d2x3_1.3do size=0.229611 movesize=0.229611 soundclass=sm_door.snd
# DESC:    
# BBOX: -.597832 -.597465 -.299701 .597179 .59758 .299701
00archcrate       _walkstruct        model3d=00crte.3do size=0.830224 movesize=0.830224
# DESC:    
# BBOX: -.597832 -.597465 -.299701 .597179 .59758 .299701
00crate           _walkstruct        model3d=00crte.3do size=0.830224 movesize=0.830224
# DESC:    
# BBOX: -.4875 -.025 -.4 .4875 .025 .4
12_door2a         _structure         model3d=12dr02_a.3do size=0.681095 movesize=0.681095 soundclass=lg_door.snd
# DESC:    
# BBOX: -.4875 -.025 -.4 .4875 .025 .4
12_door2b         _structure         model3d=12dr02_b.3do size=0.681095 movesize=0.681095
# DESC:    
# BBOX: -.170777 -.170777 -.012502 .170777 .170777 .012502
12_epad1          _walkstruct        model3d=12epad.3do size=0.224571 movesize=0.224571 soundclass=med_elev.snd
# DESC:    
# BBOX: -.119 -.0975 -.0125 .119 .0975 .0125
12_epad2          _walkstruct        model3d=12pad2.3do size=0.154349 movesize=0.154349 soundclass=sm_elev.snd
# DESC:    
# BBOX: -.95 -.04 -1.005 .95 .04 1.005
12_door3          _structure         model3d=12dr03.3do size=1.433519 movesize=1.433519 soundclass=lg_door.snd
# DESC:    Green key Pickup
# BBOX: -.010023 -.016964 -.033841 .010023 .016964 .033841
keygreen          _powerup           thingflags=0x400 model3d=g-key.3do cog=pow_keygreen.cog
# DESC:    
# BBOX: -.15 -.4 -.0125 .15 .4 .0125
12ucatwalk_a      _walkstruct        model3d=12uwalka.3do size=0.477383 movesize=0.477383
# DESC:    
# BBOX: -.505 -.15 -.0125 .505 .15 .0125
12ucatwalk_b      _walkstruct        model3d=12uwalkb.3do size=0.576955 movesize=0.576955
# DESC:    
# BBOX: -.15 -.4 -.0125 .15 .4 .0125
12ucatwalk_c      _walkstruct        model3d=12uwalkc.3do size=0.477383 movesize=0.477383
# DESC:    
# BBOX: 0 0 0 0 0 0
+superenerg2      +superenerg        range=0.600000
# DESC:    
# BBOX: -.650003 -.037533 -.499995 .650003 .037533 .499995
14d13x10          _structure         model3d=14d13x10.3do size=0.820919 movesize=0.820919 soundclass=lg_door.snd
# DESC:    
# BBOX: -.400003 -.037533 -.499995 .400003 .037533 .299995
14d8x8            _structure         model3d=14d8x8.3do size=0.641410 movesize=0.641410 soundclass=lg_door.snd
# DESC:    
# BBOX: -.500004 -1.499938 -.299921 .500004 1.499938 .299921
14bridge          _walkstruct        model3d=14brdg.3do size=1.659276 movesize=1.659276
# DESC:    
# BBOX: -.100241 -.301697 -.045628 .099795 .297631 .038858
c2x6e             _walkstruct        model3d=c2x6e.3do size=0.320280 movesize=0.320280
# DESC:    
# BBOX: -5.473804 -4.770837 -5.781102 5.473804 4.770837 5.781102
14tower_whole     _ghoststructure    model3d=towr.3do size=7.961624 movesize=7.961624
# DESC:    
# BBOX: -.334721 -1.341961 -.35292 .319207 1.368196 .319539
14bigpipe         _walkstruct        model3d=14bpip.3do size=1.465595 movesize=1.465595
# DESC:    
# BBOX: -.158991 -.163886 -.224502 .158991 .163886 .224502
rock2mini         _walkstruct        thingflags=0x48 model3d=rck2mini.3do size=0.369958 movesize=0.369958
# DESC:    
# BBOX: -.01341 -.149802 -.01341 .01341 .015603 .01341
+turretlaser3     +turretlaser       vel=(0.000000/4.000000/0.000000) damage=40.000000 mindamage=20.000000
# DESC:    Turret Gun Larger
# BBOX: -.124322 -.166491 -.102246 .124322 .167917 .079419
boxturret4        boxturret          model3d=tur4.3do size=0.200000 movesize=0.200000 cog=class_boxturret4.cog weapon=+turretlaser3 health=150.00 maxhealth=150.00 fireoffset=(0.000000/-0.043000/0.036000) aiclass=turbbdefault.ai
# DESC:    
# BBOX: -.317982 -.327771 -.449003 .317982 .327771 .449003
rock2             _walkstruct        thingflags=0x48 model3d=rck2.3do size=0.689916 movesize=0.689916
# DESC:    
# BBOX: 0 0 0 0 0 0
_throwable        none               orient=(0.000000/0.000000/0.000000) type=debris collide=1 move=physics movesize=0.010000 surfdrag=3.000000 airdrag=1.000000 mass=25.000000 height=0.011000 physflags=0x404041 buoyancy=0.500000
# DESC:    
# BBOX: -.063042 -.062892 -.063041 .063042 .062892 .063041
throwrock3        _throwable         model3d=rcktoss3.3do size=0.062900 movesize=0.062900 height=0.063000
# DESC:    
# BBOX: -25.1632 -2.863825 -8.585076 22.076361 3.652758 3.897422
cargoship         _ghoststructure    model3d=cargo.3do size=25.332577 movesize=25.332577
# DESC:    
# BBOX: -.15 -1 -.012306 .15 1 .012306
3x20catwalk       _walkstruct        model3d=c3x20.3do size=1.061262 movesize=1.061262
# DESC:    
# BBOX: -.199801 -2.500005 -.049881 .199801 2.499985 .049881
15c4x50           _walkstruct        model3d=15c50x4.3do size=2.558472 movesize=2.558472
# DESC:    
# BBOX: -1.462497 -1.455023 -.149063 1.462497 1.455023 .149063
landpad           _walkstruct        model3d=landpad.3do size=1.650857 movesize=1.650857
# DESC:    
# BBOX: -.199801 -2.369914 -.049881 .199801 2.365105 .049881
15c4x47           _walkstruct        model3d=15c47x4.3do size=2.428844 movesize=2.428844
# DESC:    
# BBOX: -.036543 -.036456 -.036542 .036543 .036456 .036542
throwcrate1       _throwable         model3d=crttoss1.3do size=0.036400 movesize=0.036400 height=0.036500
# DESC:    
# BBOX: -.00375 -.1 -.052165 .00375 .1 .052165
15_2x1rails       _walkstruct        model3d=15r2x1.3do size=0.162851 movesize=0.162851
# DESC:    Force Mana Boost Pickup
# BBOX: -.020759 -.01902 -.030703 .020759 .01902 .030703
manaboost         _powerup           thingflags=0x400 model3d=dark.3do cog=pow_mana.cog
# DESC:    DarkJedi Maw
# BBOX: -.046804 -.072974 -.040863 .045672 .088291 .061896
maw               _flyactor          model3d=ma.3do size=0.063100 movesize=0.063100 puppet=ma.pup soundclass=ma.snd maxvel=0.200000 weapon=+gamaxe health=900.00 maxhealth=900.00 maxthrust=1.50 maxrotthrust=200.00 typeflags=0x2000201 error=0.35 fov=0.40 chance=1.00 aiclass=maw.ai
# DESC:    DarkSide Force Boost Pickup
# BBOX: -.03092 -.030162 -.030457 .03092 .030162 .030457
darkside          _powerup           thingflags=0x400 model3d=mana1.3do cog=pow_darkside.cog respawn=60.000000
# DESC:    LightSide Force Boost Pickup
# BBOX: -.024476 -.030289 .000074 .02429 .030289 .060652
lightside         _powerup           thingflags=0x400 model3d=lite.3do cog=pow_lightside.cog respawn=60.000000
# DESC:    
# BBOX: -.026724 -.026724 -.044654 .026724 .026724 .044654
throwshrapnel_1   _throwable         model3d=shrp_1.3do size=0.045000 movesize=0.045000 height=0.045100
# DESC:    
# BBOX: -.011171 -.010389 -.042003 .011171 .010389 .042003
throwshrapnel_2   _throwable         model3d=shrp_2.3do size=0.040000 movesize=0.040000 height=0.040100
# DESC:    
# BBOX: -.015988 -.015443 -.026188 .015988 .015443 .026188
throwshrapnel_3   _throwable         model3d=shrp_3.3do size=0.028000 movesize=0.028000 height=0.028100
# DESC:    
# BBOX: -.016213 -.014647 -.020855 .016213 .014647 .020855
throwshrapnel_4   _throwable         model3d=shrp_4.3do size=0.026000 movesize=0.026000 height=0.026100
# DESC:    
# BBOX: -.026804 -.036274 -.074092 .026804 .036274 .074092
throwshrapnel_5   _throwable         model3d=shrp_5.3do size=0.084000 movesize=0.084000 height=0.084100
# DESC:    
# BBOX: -.4 -.050015 -.398 .4 .050015 .4
16_8x8door        _walkstruct        model3d=d8x8.3do size=0.617892 movesize=0.617892 soundclass=lg_door.snd
# DESC:    
# BBOX: -45.002899 -15.63616 -38.87886 45.002899 -1.02123 38.878868
16ground          _walkstruct        model3d=16grnd.3do size=59.529995 movesize=59.529995
# DESC:    
# BBOX: -.544736 -.786304 -.229997 .544736 .786304 .229997
16moldycrow       _decor             model3d=crow.3do size=0.800000 movesize=0.800000
# DESC:    
# BBOX: -.273491 -.232794 -.725108 .279523 .232794 2.487975
dockingclamp      _ghostdecor        model3d=clmp.3do size=0.010000 movesize=0.010000
# DESC:    C3PO Droid 
# BBOX: -.030754 -.015261 -.124016 .030821 .02207 .067788
seethreepio       _droidactor        thingflags=0x20000400 model3d=c3.3do size=0.067800 movesize=0.067800 puppet=c3.pup soundclass=c3po.snd cog=actor_c3po.cog mass=120.000000 maxvel=0.400000 health=50.00 maxhealth=50.00 maxthrust=0.30 maxrotthrust=50.00 typeflags=0x180140 aiclass=drdefault.ai
# DESC:    
# BBOX: -.059209 -.056286 -.091782 .083562 .062133 .046801
powerdroid45      _droidactor        thingflags=0x20000400 model3d=pd45.3do size=0.080000 movesize=0.080000 puppet=pd45.pup soundclass=gonk.snd cog=actor_go.cog mass=180.000000 health=50.00 maxhealth=50.00 maxthrust=0.40 maxrotthrust=50.00 typeflags=0x180140 aiclass=drdefault.ai
# DESC:    
# BBOX: -.050887 -.075508 -.16702 .152168 .25464 .131736
16crane_1         _decor             thingflags=0x400 model3d=cran45_1.3do size=0.075000 movesize=0.075000 cog=xcrane.cog
# DESC:    
# BBOX: -.298916 -.298733 -.14985 .29859 .29879 .14985
00crate6x6        _walkstruct        model3d=00crte6x6.3do size=0.440112 movesize=0.440112
# DESC:    
# BBOX: -.016314 -.219705 -.118596 .016314 .223171 .249539
16fan0            none               orient=(0.000000/0.000000/0.000000) type=cog collide=3 move=physics model3d=fan0.3do size=0.299843 movesize=0.299843 physflags=0x200 maxrotvel=480.000000 angvel=(400.000000/0.000000/0.000000)
# DESC:    
# BBOX: 0 0 0 0 0 0
16_exp            +grenade_exp       damage=0.000000
# DESC:    DarkJedi Sariss
# BBOX: -.027434 -.017686 -.118967 .027531 .017798 .065257
sariss            _darkjedi          model3d=sa.3do size=0.064450 movesize=0.064450 puppet=sa.pup soundclass=sa.snd mass=140.000000 maxvel=0.200000 weapon=+gamaxe health=1900.00 maxhealth=1900.00 maxrotthrust=220.00 jumpspeed=1.60 typeflags=0x2008201 fov=0.40 chance=1.00 aiclass=sariss.ai
# DESC:    DarkJedi Yun Dead
# BBOX: -.089696 -.129795 -.016968 .089696 .129795 .016968
yundead           _decor             model3d=yundead.3do size=0.020000 movesize=0.020000
# DESC:    DarkJedi Yun Stronger Health
# BBOX: -.031992 -.017003 -.134424 .032032 .024487 .073659
yun2              yun                soundclass=yu2.snd health=1000.00 maxhealth=1000.00 maxrotthrust=220.00 error=0.30 fov=0.40 aiclass=yunds.ai
# DESC:    
# BBOX: -.4 -.4 -.05 .4 .4 .05
18platform        _walkstruct        model3d=8x8ev.3do size=0.617891 movesize=0.617891 soundclass=lg_elev.snd
# DESC:    
# BBOX: -.248528 -.3 -.025 .248528 .3 .025
18wedgedoor       _walkstruct        model3d=18wedge.3do size=0.440373 movesize=0.440373
# DESC:    Dynamic Light 3.0
# BBOX: 0 0 0 0 0 0
light3.0          _ghostdecor        thingflags=0x1 light=3.000000
# DESC:    
# BBOX: -.1 -.5 -.012306 .1 .5 .012306
c2x10e            _walkstruct        model3d=c2x10e.3do size=0.510050 movesize=0.510050 soundclass=med_elev.snd
# DESC:    
# BBOX: -.565528 -.327771 -.333229 .565528 .327771 .333229
rock1             _walkstruct        thingflags=0x48 model3d=rck1.3do size=0.783239 movesize=0.783239
# DESC:    
# BBOX: -.034673 -.03459 -.034673 .034673 .03459 .034673
throwrock2        _throwable         model3d=rcktoss2.3do size=0.034600 movesize=0.034600 height=0.034700
# DESC:    
# BBOX: -.0547 -.05457 -.0547 .0547 .05457 .0547
throwrock4        _throwable         model3d=rcktoss4.3do size=0.054500 movesize=0.054500 height=0.054600
# DESC:    AT-AT Legs
# BBOX: -.605016 -.480794 -.833671 .605016 .480794 .833671
atatlegs          _zwalkstruct       model3d=aalegs.3do size=1.099075 movesize=1.099075
# DESC:    AT-AT Body
# BBOX: -.380577 -1.359003 -.458991 .380577 1.359003 .458991
atatbody          _zwalkstruct       model3d=aabody.3do size=1.451033 movesize=1.451033
# DESC:    
# BBOX: -.15 -.15 -.150003 .15 .15 .150003
18crate3x3_1      _walkstruct        timer=25.000001 model3d=crt4-4.3do size=0.259810 movesize=0.259810
# DESC:    
# BBOX: -.150124 -.150615 -.150124 .149876 .149385 .149872
18crate3x3_2      _walkstruct        timer=25.000001 model3d=crt4-5.3do size=0.260306 movesize=0.260306
# DESC:    
# BBOX: -.1 -.1 -.099985 .1 .1 .099985
18crate2x2        _walkstruct        timer=25.000001 model3d=crt4-3.3do size=0.173196 movesize=0.173196
# DESC:    
# BBOX: 0 0 0 0 0 0
+upenergyhi       none               orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=0.600000 vel=(0.000000/0.000000/2.000000) angvel=(0.000000/360.000000/0.000000) typeflags=0x3e material=embers.mat range=0.400000 elementsize=0.007500 count=30
# DESC:    Elevator 2x2 w three legs
# BBOX: -.098284 -.097923 -.031081 .097733 .098093 .031081
2x2elev_2         _walkstruct        model3d=e2x2_0.3do size=0.192295 movesize=0.192295 soundclass=sm_elev.snd
# DESC:    Door fake not meant to be moved
# BBOX: -.4 -.25 -.050031 0 .25 0
d8x5_a            _walkstruct        model3d=d8x5_a.3do size=0.474345 movesize=0.474345 soundclass=lg_door.snd
# DESC:    Door fake not meant to be moved
# BBOX: 0 -.25 -.050031 .4 .25 0
d8x5_b            _walkstruct        model3d=d8x5_b.3do size=0.474345 movesize=0.474345 soundclass=lg_door.snd
# DESC:    
# BBOX: -.15 -.4 -.012306 .15 .4 .012306
19c3x8e           _zwalkstruct       model3d=c3x8e.3do size=0.477378 movesize=0.477378 soundclass=med_elev.snd
# DESC:    
# BBOX: -.00929 -.00375 -.04177 .00929 .00375 .04177
controlroomkey    _powerup           thingflags=0x408 model3d=dkey.3do cog=pow_ckey.cog
# DESC:    
# BBOX: -.2 -.349883 -.275 .2 .349883 .275
payload_a         _walkstruct        model3d=payloada.3do size=0.487897 movesize=0.010000 soundclass=payload.snd
# DESC:    
# BBOX: -.2 -.349883 -.275 .2 .349883 .275
payload_b         _walkstruct        model3d=payloadb.3do size=0.487897 movesize=0.010000
# DESC:    
# BBOX: -.2 -.349883 -.275 .2 .349883 .275
payload_c         _walkstruct        model3d=payloadc.3do size=0.487897 movesize=0.010000
# DESC:    
# BBOX: -.2 -.349883 -.275 .2 .349883 .275
payload_d         _walkstruct        model3d=payloadd.3do size=0.487897 movesize=0.010000
# DESC:    Door fake not meant to be moved
# BBOX: -.4 -.25 0 .4 .25 0
d8x5fake          _walkstruct        model3d=d8x5fake.3do size=0.471699 movesize=0.471699
# DESC:    
# BBOX: -.100779 -.107544 .000686 .100779 .107544 .023173
ceilinglight2     _ghoststructure    model3d=clight0.3do size=0.166487 movesize=0.166487
# DESC:    
# BBOX: -.801732 -.799971 -.500413 .801732 .79997 .500413
drill             _structure         model3d=drill.3do size=0.010000 movesize=0.010000
# DESC:    
# BBOX: -.3 -.57 -.3 .3 .57 .3
weight_1          _walkstruct        thingflags=0x48 model3d=wght01.3do size=0.760563 movesize=0.760563 soundclass=19counterweight.snd
# DESC:    
# BBOX: -.01264 -.999708 -.01129 .01264 .999708 .01129
rope03            _structure         thingflags=0xc model3d=rope03.3do size=1.049851 movesize=1.049851
# DESC:    
# BBOX: -.2125 -.475 -.25 .2125 .475 .25
19stonedoor       _walkstruct        thingflags=0x48 model3d=19stndoor.3do size=0.627305 movesize=0.627305 soundclass=19stone_door.snd
# DESC:    
# BBOX: -.1 0 -.1 .1 0 .1
19handswitch      _structure         model3d=19hands.3do size=0.191421 movesize=0.191421
# DESC:    
# BBOX: -.161624 -.239125 -.45412 .160571 .250864 .223596
jatst             atst               thingflags=0x404 cog=actor_jatst.cog
# DESC:    
# BBOX: -.921603 -1.937083 -1.696014 2.015026 1.014213 1.696014
bust5_2x          _ghoststructure    model3d=bust5_2x.3do size=3.170455 movesize=3.170455
# DESC:    
# BBOX: -.051662 -.098584 -.019834 .082951 .078536 .031738
s2carcass         _ghostdecor        model3d=s2eaten.3do size=0.163383 movesize=0.163383
# DESC:    
# BBOX: -.054992 -.082775 -.019738 .085887 .072311 .018145
stcarcass         _ghostdecor        model3d=steaten.3do size=0.162293 movesize=0.162293
# DESC:    
# BBOX: -.017157 -.016142 -.014361 .017157 .016142 .014361
throwhelmet       _throwable         model3d=stgib1.3do size=0.014700 movesize=0.014700 height=0.014800
# DESC:    
# BBOX: -.034756 -.02133 -.007677 .034756 .02133 .007677
stgib3            _ghostdecor        model3d=stgib3.3do size=0.088910 movesize=0.088910
# DESC:    
# BBOX: -.049407 -.030527 -.009205 .049407 .030527 .009205
stgib2            _ghostdecor        model3d=stgib2.3do size=0.103988 movesize=0.103988
# DESC:    
# BBOX: -.037893 -.021815 -.12043 .03746 .022321 .066407
jake              icommando          cog=actor_jake.cog
# DESC:    Red key Dropped
# BBOX: -.010023 -.016964 -.033841 .010023 .016964 .033841
+dkeyred          _powerup           thingflags=0x400 model3d=r-key.3do cog=pow_keyred.cog height=0.033840 physflags=0x41 typeflags=0x0
# DESC:    
# BBOX: -1.019287 -.993732 -.269209 1.019287 .993732 .269209
jedimound         _walkstruct        thingflags=0x48 model3d=jmnd.3do size=1.104239 movesize=1.104239
# DESC:    DarkJedi Boc
# BBOX: -.06033 -.065675 -.118504 .041566 .082518 .027644
boc               _darkjedi          model3d=bo.3do size=0.060700 movesize=0.060700 puppet=bo.pup soundclass=bo.snd mass=160.000000 maxvel=0.200000 weapon=+gamaxe health=1500.00 maxhealth=1500.00 maxrotthrust=200.00 jumpspeed=1.60 error=0.25 fov=0.40 chance=1.00 aiclass=boc.ai
# DESC:    
# BBOX: -1.485445 -1.431855 -1.505791 1.396193 1.128748 1.505791
bust3_2x          _ghoststructure    model3d=bust3_2x.3do size=2.461241 movesize=2.461241
# DESC:    
# BBOX: -.742723 -.735947 -.800791 .698097 .698463 .800792
bust4             _ghoststructure    model3d=bust4.3do size=1.301096 movesize=1.301096
# DESC:    
# BBOX: -.17849 -.059063 -.119858 .17849 .059063 .119858
slab1             _walkstruct        thingflags=0x48 model3d=slab1.3do size=0.222965 movesize=0.222965
# DESC:    
# BBOX: -.072557 -.059063 -.164788 .072557 .059063 .164788
slab2             _walkstruct        thingflags=0x48 model3d=slab2.3do size=0.189494 movesize=0.189494
# DESC:    
# BBOX: -.458867 -.036619 -.0973 .458867 .036619 .0973
slab3             _walkstruct        thingflags=0x48 model3d=slab3.3do size=0.470497 movesize=0.470497
# DESC:    
# BBOX: -.075265 -.064442 -.151527 .075265 .064442 .151527
statue            _walkstruct        thingflags=0x48 model3d=stat.3do size=0.231047 movesize=0.231047
# DESC:    
# BBOX: -.807034 -1.054058 -1.386616 .755901 1.09021 1.386616
bust2_2x          _ghoststructure    model3d=bust2_2x.3do size=1.850532 movesize=1.850532
# DESC:    
# BBOX: -1.184637 -.813483 -.761018 .715234 .683786 .761018
bust2a            _ghoststructure    model3d=bust2a.3do size=1.525402 movesize=1.525402
# DESC:    
# BBOX: -.742723 -.715928 -.752895 .698097 .564374 .752895
bust3             _ghoststructure    model3d=bust3.3do size=1.255621 movesize=1.255621
# DESC:    Jan Orrs
# BBOX: -.035332 -.013185 -.120103 .035579 .016413 .064509
jan               _humanactor        model3d=ja.3do puppet=ja.pup soundclass=wm1.snd maxvel=0.400000 health=20000.00 maxhealth=20000.00 maxthrust=1.00 maxrotthrust=60.00 aiclass=jan.ai
# DESC:    DarkJedi Jerec
# BBOX: -.031436 -.018399 -.121814 .032728 .01913 .065791
jerec             _darkjedi          model3d=je.3do size=0.063000 movesize=0.063000 puppet=je.pup soundclass=je.snd mass=170.000000 maxvel=0.200000 weapon=+gamaxe health=2000.00 maxhealth=2000.00 maxthrust=1.00 maxrotthrust=200.00 jumpspeed=4.00 typeflags=0x2008201 error=0.10 fov=-0.40 chance=1.00 aiclass=jerec.ai
# DESC:    
# BBOX: -.198498 -.173539 .001103 .198498 .181053 1.528715
statpole          _structure         model3d=statpole.3do size=1.581753 movesize=1.581753 soundclass=19stone_door.snd
# DESC:    
# BBOX: -.1 0 -.1 .1 0 .1
21handswitch      _structure         model3d=21hands.3do size=0.191421 movesize=0.191421 soundclass=19stone_door.snd
# DESC:    HealthPack Respawns Single Player
# BBOX: -.027845 -.025398 -.02252 .027845 .025398 .024913
rhealth           healthpack         typeflags=0x2
# DESC:    Bacta Tank Respawns Single Player
# BBOX: -.015415 -.017797 -.028888 .015415 .017797 .028888
rbacta            bactatank          typeflags=0x2 respawn=30.000000
# DESC:    Mana Boost Respawns Single Player
# BBOX: -.03092 -.030162 -.030457 .03092 .030162 .030457
rmanaboost        manaboost          model3d=mana1.3do typeflags=0x2
# DESC:    Light Side Boost Respawns Single Player
# BBOX: -.024476 -.030289 .000074 .02429 .030289 .060652
rlsurge           lightside          typeflags=0x2 respawn=30.000000
# DESC:    
# BBOX: 0 0 0 0 0 0
+coreflow         none               orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=3.000000 vel=(0.000000/0.000000/1.000000) angvel=(0.000000/540.000000/0.000000) typeflags=0x3e material=embers.mat range=0.200000 elementsize=0.005000 count=10 yawrange=90.000000
# DESC:    
# BBOX: 0 0 0 0 0 0
+smallcoreflow    none               orient=(0.000000/0.000000/0.000000) type=particle move=physics timer=3.000000 vel=(0.000000/0.000000/1.000000) angvel=(0.000000/540.000000/0.000000) typeflags=0x3e material=embers.mat range=0.100000 elementsize=0.005000 count=5 yawrange=90.000000
# DESC:    Revive Respawns Single Player
# BBOX: -.021089 -.023005 -.035347 .016307 .023005 .035347
rrevivepack       _powerup           thingflags=0x400 model3d=packrev.3do cog=pow_revive.cog typeflags=0x2 respawn=60.000000
# DESC:    
# BBOX: -.1 -.1 -.45 .1 .1 .45
m10column         _structure         model3d=m10colmn.3do size=0.521699 movesize=0.521699
# DESC:    
# BBOX: -.25 -.05 -.2 .25 .05 .2
m2heater          _walkstruct        thingflags=0x4c model3d=m2heat.3do size=0.324037 movesize=0.324037
# DESC:    
# BBOX: -.059999 -.051358 -.040791 .060835 .061748 .050725
m2boxturret       _zwalkstruct       model3d=tur1.3do size=0.139457 movesize=0.139457 soundclass=med_elev.snd
# DESC:    
# BBOX: -.036543 -.036456 -.036542 .036543 .036456 .036542
throwcrate2       _throwable         model3d=crttoss2.3do size=0.036400 movesize=0.036400 height=0.036500
# DESC:    
# BBOX: -.030211 -.406862 -.219622 .030211 .41328 .46211
m5_fan0_moving    none               orient=(0.000000/0.000000/0.000000) type=cog collide=3 move=physics model3d=fan0big.3do size=0.512672 movesize=0.512672 physflags=0x200 maxrotvel=480.000000 angvel=(400.000000/0.000000/0.000000)
# DESC:    
# BBOX: -.018484 -.070714 -.030025 .019883 .081826 .013227
m5railgun         railgun            typeflags=0x9
# DESC:    
# BBOX: -.030347 -.072704 -.024787 .009211 .072704 .024341
m5concrifle       concrifle          typeflags=0x9
# DESC:    
# BBOX: -.099008 -.068267 -.014304 .099008 .129749 .009657
2x2elev_0         _walkstruct        model3d=19e2x2.3do size=0.213495 movesize=0.213495 soundclass=sm_elev.snd
# DESC:    
# BBOX: -.198 -.198 -.198 .198 .198 .198
c1_4x4llift       _walkstruct        thingflags=0x400049 light=0.600000 model3d=l4x4_2.3do size=0.392946 movesize=0.392946 soundclass=med_elev.snd
# DESC:    
# BBOX: -.017376 -.026884 -.017659 .017376 .026884 .017659
ctf_redflag       _powerup           move=none model3d=flgr.3do size=0.050000 movesize=0.050000 puppet=cr.pup typeflags=0x8
# DESC:    
# BBOX: -.017376 -.027858 -.017659 .017376 .027858 .017659
ctf_goldflag      _powerup           move=none model3d=flgy.3do size=0.050000 movesize=0.050000 puppet=cr.pup typeflags=0x8
# DESC:    
# BBOX: -.198 -.198 -.198 .198 .198 .198
c3_4x4llift       _walkstruct        thingflags=0x400049 light=0.600000 model3d=l4x4.3do size=0.392946 movesize=0.392946 soundclass=med_elev.snd
# DESC:    
# BBOX: -.15 -.4 -.012306 .15 .4 .012306
c2_catwalk        _walkstruct        model3d=01c3x8.3do size=0.427378 movesize=0.427378
# DESC:    
# BBOX: 0 0 0 0 0 0
_shard            none               orient=(0.000000/0.000000/0.000000) type=cog move=physics mass=0.050000 physflags=0x221
# DESC:    
# BBOX: -.03133 -.000001 -.038594 .031989 .000001 .027215
shard00           _shard             timer=1.000000 model3d=shard0.3do size=0.042000 movesize=0.042000
# DESC:    
# BBOX: -.187173 -.171934 -.041875 .196141 .200354 .037814
c3_fan            _decor             move=physics model3d=fan1.3do size=0.250944 movesize=0.250944 angvel=(0.000000/360.000000/0.000000)
# DESC:    
# BBOX: -.15 -.011875 -.15 .15 .011875 .15
c3_3x3door_1      _structure         model3d=d3x3_1.3do size=0.262464 movesize=0.262464 soundclass=sm_door.snd
# DESC:    
# BBOX: -.016314 -.219705 -.118596 .016314 .223171 .249539
c3_fan0_moving    none               orient=(0.000000/0.000000/0.000000) type=cog move=physics model3d=fan0.3do size=0.299843 movesize=0.299843 physflags=0x200 maxrotvel=480.000000 angvel=(400.000000/0.000000/0.000000)
# DESC:    
# BBOX: -.059999 -.051358 -.040791 .060835 .061748 .050725
c3_boxturret      _zwalkstruct       model3d=tur1.3do size=0.100000 movesize=0.100000
# DESC:    
# BBOX: -.010023 -.016964 -.033841 .010023 .016964 .033841
ctf_redkey        none               orient=(0.000000/0.000000/0.000000) type=item collide=1 thingflags=0x401 light=0.750000 model3d=r-key.3do size=0.088183 movesize=0.088183 cog=ctf_keyred.cog typeflags=0x8
# DESC:    
# BBOX: -.010023 -.016964 -.033841 .010023 .016964 .033841
ctf_goldkey       none               orient=(0.000000/0.000000/0.000000) type=item collide=1 thingflags=0x401 light=0.750000 model3d=y-key.3do size=0.100000 movesize=0.100000 cog=ctf_keygold.cog typeflags=0x8
# DESC:    
# BBOX: -.15 -.4 -.012306 .15 .4 .012306
c3_hdoor          _walkstruct        model3d=01c3x8.3do size=0.427378 movesize=0.427378
# DESC:    
# BBOX: -.148 -.148 -.013055 .148 .148 .013695
ctf_3x3elev       _walkstruct        model3d=ctf_e3x3.3do size=0.259751 movesize=0.259751 soundclass=sm_elev.snd
# DESC:    
# BBOX: -.15 -.4 -.012306 .15 .4 .012306
01cat3x8_2        _walkstruct        model3d=01c3x8.3do size=0.427378 movesize=0.427378
# DESC:    Catwalk 2x6 triangular shaped bottom
# BBOX: -.100241 -.301697 -.045628 .099795 .297631 .038858
2x6catwalk        _walkstruct        model3d=c2x6.3do size=0.320280 movesize=0.320280
# DESC:    catwalk 2x8
# BBOX: -.1 -.399762 -.024738 .1 .399762 .024738
2x8catwalk        _walkstruct        model3d=c2x8.3do size=0.412822 movesize=0.412822
# DESC:    Elevator 2x4 w 2 legs
# BBOX: -.09999 -.199962 -.031081 .09999 .199962 .031081
2x4elev           _walkstruct        model3d=e2x4.3do size=0.225719 movesize=0.225719 soundclass=med_elev.snd
# DESC:    Elevator 4x4 w one end not textured and 3 legs
# BBOX: -.198 -.198002 -.05971 .198 .198002 .05971
4x4elev           _walkstruct        model3d=e4x4.3do size=0.336311 movesize=0.336311 soundclass=med_elev.snd
# DESC:    Elevator 4x4 same as 4x4elev
# BBOX: -.198 -.198002 -.05971 .198 .198002 .05971
4x4elev_1         _walkstruct        model3d=e4x4_1.3do size=0.336311 movesize=0.336311 soundclass=med_elev.snd
# DESC:    Elevator 4x4 w two legs
# BBOX: -.200716 -.195991 -.079681 .195304 .200102 .079681
4x4elev_2         _walkstruct        model3d=e4x4_2.3do size=0.336311 movesize=0.336311 soundclass=med_elev.snd
# DESC:    Elevator same as 4x4elev_2 but w no legs 
# BBOX: -.200716 -.195991 .038027 .195304 .200102 .079681
4x4ingarvator     _zwalkstruct       model3d=e4x4_2i.3do size=0.344409 movesize=0.344409 soundclass=med_elev.snd
# DESC:    Elevator Complete w sides and a light in ceiling
# BBOX: -.198 -.198 -.198 .198 .198 .198
4x4lift           _walkstruct        model3d=l4x4.3do size=0.392946 movesize=0.392946 soundclass=med_elev.snd
# DESC:    Elevator 4x4 complete with sides. 
# BBOX: -.198 -.198 -.198 .198 .198 .198
4x4lift_2         _walkstruct        model3d=l4x4_2.3do size=0.392946 movesize=0.392946 soundclass=med_elev.snd
# DESC:    Door for 4x4Lift Elevator  complete with sides.  
# BBOX: -.199997 -.013245 -.200025 .199997 .013245 .200025
4x4liftdoor       _walkstruct        model3d=ld4x4.3do size=0.283168 movesize=0.283168 soundclass=lg_elev.snd
# DESC:    Elevator 6x6 with bottom sides ,use in a hole 
# BBOX: -.297299 -.296497 -1.049955 .297299 .297249 1.049955
6x6elev_1         _walkstruct        model3d=e6x6_1.3do size=1.180995 movesize=1.180995 soundclass=lg_elev.snd
# DESC:    Elevator 8x4 w 3 legs
# BBOX: -.40002 -.200046 -.079681 .40002 .200046 .079681
8x4elev           _walkstruct        model3d=e8x4.3do size=0.454295 movesize=0.454295 soundclass=lg_elev.snd
# DESC:    Elevator 8x8 no legs
# BBOX: -.40002 -.400093 -.024993 .40002 .400093 .024993
8x8elev           _walkstruct        model3d=e8x8.3do size=0.616317 movesize=0.616317 soundclass=lg_elev.snd
# DESC:    Door 30x10 Single  Drk Grey w yellow warn on Ends
# BBOX: 0 -.499 -.069717 3 .499 0
baydoor           _walkstruct        model3d=bayd.3do size=3.092016 movesize=3.092016 soundclass=lg_door.snd
# DESC:    Door 15x10 1/2 of BayDoor type
# BBOX: -.75 -.499 -.034859 .75 .499 .034859
baydoor_a         _walkstruct        model3d=bayd_a.3do size=0.951508 movesize=0.951508
# DESC:    Door 15x10 1/2 of BayDoor type door
# BBOX: -.75 -.499 -.034859 .75 .499 .034859
baydoor_b         _walkstruct        model3d=bayd_b.3do size=0.951508 movesize=0.951508
# DESC:    
# BBOX: -.2 -.2 -.2 .2 .2 .2
funicular         _walkstruct        model3d=02crate1.3do size=.34641 movesize=.34641 soundclass=funicular.snd
