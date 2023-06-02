# DESC:  
# BBOX: 0 0 0 0 0 0
_decor            none               orient=(0/0/0) type=cog collide=1 move=path
# DESC:  
# BBOX: 0 0 0 0 0 0
_structure        _decor             collide=3 thingflags=0x8
# DESC:  
# BBOX: 0 0 0 0 0 0
_zwalkstruct      _structure         thingflags=0x400040
# DESC:  
# BBOX: -.228837 -.19266 -.55048 .228837 .19266 .55048
freezingclamp     _zwalkstruct       model3d=mdm02clmp.3do size=.606291 movesize=.606291
# DESC:  
# BBOX: -.30607 -.49657 -.15875 .30607 .49657 .15875
awing             _zwalkstruct       model3d=awing1.3do size=.607968 movesize=.607968
# DESC:  
# BBOX: 0 0 0 0 0 0
_walkstruct       _structure         thingflags=0x400048
# DESC:  
# BBOX: -.098284 -.097923 -.031081 .097733 .098093 .031081
2x2elev           _walkstruct        model3d=e2x2_0.3do size=.192295 movesize=.192295 soundclass=sm_elev.snd
# DESC:  
# BBOX: -.650003 -.037533 -.499995 .650003 .037533 .499995
m9_14d13x10       _structure         thingflags=0xc model3d=14d13x10.3do size=.820919 movesize=.820919 soundclass=lg_door.snd
# DESC:  
# BBOX: 0 0 0 0 0 0
ghost             none               orient=(0/0/0) type=ghost move=path
# DESC:  
# BBOX: -.549776 -.490931 -.315187 .549776 .4752 .320955
tiebomber         _zwalkstruct       thingflags=0x400440 model3d=tieb.3do size=.5 movesize=.5 puppet=cr.pup cog=ship_tiebomber.cog
# DESC:  
# BBOX: -.15 -.011875 -.15 .15 .011875 .15
3x3door_1         _walkstruct        model3d=d3x3_1.3do size=.262464 movesize=.262464 soundclass=sm_door.snd
# DESC:  
# BBOX: -.2 -.0125 -.15 .2 .0125 .15
4x3door_1         _structure         model3d=d4x3_1.3do size=.250312 movesize=.250312 soundclass=med_door.snd
# DESC:  
# BBOX: -.099008 -.068267 -.014304 .099008 .129749 .009657
2x2elev_0         _walkstruct        model3d=19e2x2.3do size=.213495 movesize=.213495 soundclass=sm_elev.snd
# DESC:  
# BBOX: 0 0 0 0 0 0
_actor            none               orient=(0/0/0) type=actor collide=1 move=physics thingflags=0x20000000 mass=150 physflags=0x4a4f maxrotvel=200 maxvel=1 health=40 maxhealth=40 maxrotthrust=180 jumpspeed=1.5 eyeoffset=(0/0/.037) minheadpitch=-80 maxheadpitch=80 lightoffset=(0/.07/.04) lightintensity=.8
# DESC:  
# BBOX: -.037301 -.013874 -.118461 .038114 .03984 .064544
walkplayer        _actor             type=player thingflags=0x20000401 light=.2 model3d=kk.3do size=.065 movesize=.065 puppet=ky.pup soundclass=ky.snd cog=kyle_m.cog surfdrag=3 airdrag=.5 staticdrag=.3 health=100 maxhealth=100 maxthrust=2 typeflags=0x1 error=.5 fov=.71 chance=1
# DESC:  
# BBOX: 0 0 0 0 0 0
_humanactor       _actor             size=.065 movesize=.065 surfdrag=3 airdrag=.5 maxvel=.5 maxthrust=.8 typeflags=0x80000
# DESC:  
# BBOX: 0 0 0 0 0 0
_weapon           none               orient=(0/0/0) type=weapon collide=1 move=physics thingflags=0x20000000 timer=10 mass=5 physflags=0x200 maxrotvel=90 damageclass=0x2 typeflags=0x1
# DESC:  
# BBOX: 0 0 0 0 0 0
_explosion        none               orient=(0/0/0) type=explosion typeflags=0x1 damageclass=0x4
# DESC:  
# BBOX: 0 0 0 0 0 0
+laserhit         _explosion         thingflags=0x1 light=.2 timer=.5 sprite=bryx.spr soundclass=exp_laserhit.snd typeflags=0x33 blasttime=.3 maxlight=.4
# DESC:  
# BBOX: 0 0 0 0 0 0
+smflash          _explosion         thingflags=0x1 light=.4 timer=.1 typeflags=0x0
# DESC:  
# BBOX: 0 0 0 0 0 0
+laserfleshhit    +laserhit          soundclass=exp_fleshy.snd
# DESC:  
# BBOX: -.007791 -.090587 -.006258 .007791 .017998 .006258
+bryarbolt        _weapon            thingflags=0x20000001 light=.4 model3d=bry0.3do size=.001 movesize=.001 soundclass=bry.snd creatething=+smflash maxrotvel=0 vel=(0/4/0) explode=+laserhit fleshhit=+laserfleshhit damage=30 mindamage=10 typeflags=0x20440d rate=15
# DESC:  
# BBOX: -.005653 -.113701 -.004853 .005653 .02259 .004853
+stlaser          +bryarbolt         model3d=str0.3do soundclass=stlaser.snd vel=(0/6/0) rate=10
# DESC:  
# BBOX: -.005653 -.113701 -.004853 .005653 .02259 .004853
+elaser           +stlaser           vel=(0/4/0) damage=12 mindamage=5
# DESC:  
# BBOX: 0 0 0 0 0 0
+whitecloud       none               orient=(0/0/0) type=particle timer=.2 typeflags=0x3f material=00gsmoke.mat range=.02 rate=128 maxthrust=30 elementsize=.007 count=128
# DESC:  
# BBOX: 0 0 0 0 0 0
+dustcloud        +whitecloud        timer=.119 material=dusty.mat range=.015 rate=256 maxthrust=80 elementsize=.01
# DESC:  
# BBOX: 0 0 0 0 0 0
+punchcloud       +dustcloud         timer=.15 material=00gsmoke.mat range=.006 rate=64 maxthrust=4 elementsize=.004 count=16
# DESC:  
# BBOX: 0 0 0 0 0 0
+punch_exp        _explosion         timer=.001 soundclass=exp_punch.snd creatething=+punchcloud typeflags=0x0
# DESC:  
# BBOX: 0 0 0 0 0 0
+punch            _weapon            size=.001 movesize=.001 mass=50 explode=+punch_exp fleshhit=+punch_exp damage=10 damageclass=0x1 typeflags=0x200d range=.15 force=50
# DESC:  
# BBOX: 0 0 0 0 0 0
+st_punch         +punch             force=25
# DESC:  
# BBOX: -.03619 -.019179 -.124701 .024091 .064673 .043406
stormtroop2_m     _humanactor        thingflags=0x20000400 model3d=s2.3do puppet=st.pup soundclass=st2_s1.snd cog=actor_st_pnch.cog maxvel=.25 weapon=+elaser weapon2=+st_punch health=70 maxhealth=70 maxthrust=.9 maxrotthrust=90 typeflags=0x20001 fireoffset=(.0048/.05/.012) aiclass=st_def.ai
# DESC:  
# BBOX: -.00375 -.1 -.052165 .00375 .1 .052165
15_2x1rails       _walkstruct        model3d=15r2x1.3do size=.162851 movesize=.162851
# DESC:  
# BBOX: 0 0 0 0 0 0
+firecloud        +dustcloud         material=00explosion.mat range=.05 rate=64 maxthrust=40 count=64
# DESC:  
# BBOX: 0 0 0 0 0 0
+grenade_exp      _explosion         thingflags=0x1 light=.2 timer=.8 sprite=detx.spr soundclass=exp_det.snd creatething=+firecloud typeflags=0x17 damage=75 blasttime=.699 force=100 maxlight=.8 range=.45
# DESC:  
# BBOX: -.012516 -.012516 -.012524 .012516 .012516 .014655
+grenade2         _weapon            timer=3 model3d=det0.3do size=.014895 movesize=.014895 soundclass=det.snd surfdrag=3 airdrag=.8 mass=1 physflags=0x225 vel=(0/2/1.5) angvel=(90/45/90) buoyancy=.25 explode=+grenade_exp fleshhit=+grenade_exp damageclass=0x4 typeflags=0x40309
# DESC:  
# BBOX: -.012516 -.012516 -.012524 .012516 .012516 .014655
+grenade1         +grenade2          timer=10 typeflags=0x4030d
# DESC:  
# BBOX: 0 0 0 0 0 0
+firecloud3       +firecloud         maxthrust=20 count=32
# DESC:  
# BBOX: 0 0 0 0 0 0
+debris_exp       _explosion         timer=1 sprite=debrisx.spr soundclass=exp_small.snd creatething=+firecloud3 typeflags=0x7 blasttime=.5
# DESC:  
# BBOX: 0 0 0 0 0 0
_debris           none               orient=(0/0/0) type=weapon collide=1 move=physics timer=1.1 airdrag=3 physflags=0x201 vel=(0/4/0) angvel=(5/10/0) explode=+debris_exp damage=5 damageclass=0x1 typeflags=0xc
# DESC:  
# BBOX: -.026724 -.026724 -.044654 .026724 .026724 .044654
shrapnel_1        _debris            model3d=shrp_1.3do size=.045 movesize=.045 damage=0
# DESC:  
# BBOX: -.011171 -.010389 -.042003 .011171 .010389 .042003
shrapnel_2        _debris            model3d=shrp_2.3do size=.04 movesize=.04 damage=0
# DESC:  
# BBOX: -.015988 -.015443 -.026188 .015988 .015443 .026188
shrapnel_3        _debris            model3d=shrp_3.3do size=.028 movesize=.028 damage=0
# DESC:  
# BBOX: -.016213 -.014647 -.020855 .016213 .014647 .020855
shrapnel_4        _debris            model3d=shrp_4.3do size=.026 movesize=.026 damage=0
# DESC:  
# BBOX: 0 0 0 0 0 0
+raildet_exp      _explosion         thingflags=0x1 light=.2 timer=.5 sprite=rldx.spr soundclass=exp_raildet.snd typeflags=0x17 damage=110 blasttime=.699 force=300 maxlight=.8 range=.5 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_4
# DESC:  
# BBOX: 0 0 0 0 0 0
+lgflash          _explosion         thingflags=0x1 light=.3 timer=.4 typeflags=0x12 blasttime=.2 maxlight=1
# DESC:  
# BBOX: -.01058 -.013704 -.006548 .010593 .009939 .012329
+raildet2         +grenade1          thingflags=0x20000400 timer=2.5 model3d=rld0.3do size=.003 movesize=.003 puppet=ra.pup soundclass=rail.snd creatething=+lgflash cog=00_smoketrail.cog airdrag=0 height=.003 physflags=0x200 maxrotvel=20 vel=(0/2.5/0) angvel=(0/0/90) explode=+raildet_exp fleshhit=+raildet_exp damage=5 typeflags=0x240b81
# DESC:  
# BBOX: 0 0 0 0 0 0
+eraildet_exp     +raildet_exp       damage=50 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_4
# DESC:  
# BBOX: -.01058 -.013704 -.006548 .010593 .009939 .012329
+eraildet         +raildet2          vel=(0/2/0) explode=+eraildet_exp fleshhit=+eraildet_exp damage=20 typeflags=0xb81
# DESC:  
# BBOX: 0 0 0 0 0 0
+axe_exp          _explosion         timer=.001 soundclass=exp_axe.snd typeflags=0x0
# DESC:  
# BBOX: 0 0 0 0 0 0
+gamaxe           _weapon            size=.001 movesize=.001 mass=100 explode=+axe_exp fleshhit=+axe_exp damage=40 damageclass=0x1 typeflags=0x200d range=.25 force=50
# DESC:  
# BBOX: 0 0 0 0 0 0
+smack            +gamaxe            damage=15 range=.15
# DESC:  
# BBOX: -.03619 -.019179 -.124701 .024091 .08689 .043406
stormtroop4_m     _humanactor        thingflags=0x20000400 model3d=s4.3do puppet=s2.pup soundclass=st4_s1.snd cog=actor_s3.cog maxvel=.25 weapon=+eraildet weapon2=+smack health=70 maxhealth=70 maxthrust=.9 maxrotthrust=90 typeflags=0x20001 fireoffset=(.014/.0532/.013) aiclass=straildefault.ai
# DESC:  
# BBOX: -.024596 -.024511 -.064105 .024596 .024511 .064105
console1          _walkstruct        model3d=con1.3do size=.072905 movesize=.072905
# DESC:  
# BBOX: -.200159 -.024886 -.057805 .200159 .024886 .057805
console5          _walkstruct        model3d=con5.3do size=.20982 movesize=.20982
# DESC:  
# BBOX: -.03619 -.019179 -.124701 .024091 .064673 .043406
st_11_m           _humanactor        thingflags=0x20000400 model3d=s2.3do puppet=st.pup soundclass=st_s1.snd cog=actor_st_m.cog maxvel=.25 weapon=+elaser weapon2=+st_punch health=60 maxhealth=60 maxthrust=1 maxrotthrust=90 typeflags=0x20001 fireoffset=(.0048/.05/.012) aiclass=st_def_11.ai
# DESC:  
# BBOX: -.39751 -.94488 -.28956 .39751 .94488 .28956
transport         _zwalkstruct       model3d=trnsprt1.3do size=1.098054 movesize=1.098054
# DESC:  
# BBOX: -.2 -.0125 -.15 .2 .0125 .25
4x4door           _walkstruct        model3d=d4x4.3do size=.3204 movesize=.3204 soundclass=med_door.snd
# DESC:  
# BBOX: 0 0 0 0 0 0
_powerup          none               orient=(0/0/0) type=item collide=1 move=physics size=.1 movesize=.01 surfdrag=3 airdrag=1 mass=10 height=.05 physflags=0x400000 typeflags=0x1 respawn=30
# DESC:  
# BBOX: -.010023 -.016964 -.033841 .010023 .016964 .033841
keyblue           _powerup           thingflags=0x400 model3d=b-key.3do cog=pow_keyblue.cog
# DESC:  
# BBOX: -.148 -.148 -.013055 .148 .148 .013695
3x3elev           _walkstruct        model3d=e3x3.3do size=.209751 movesize=.209751 soundclass=sm_elev.snd
# DESC:  
# BBOX: -.016314 -.219705 -.118596 .016314 .223171 .249539
fan0              none               orient=(0/0/0) type=cog collide=3 move=physics model3d=fan0.3do size=.299843 movesize=.299843 physflags=0x200 maxrotvel=400 angvel=(180/0/0)
# DESC:  
# BBOX: -.817281 -1.132091 -.454926 .815909 1.276639 1.852229
shuttle           _decor             model3d=shut.3do size=1 movesize=1 puppet=cr.pup
# DESC:  
# BBOX: 0 0 0 0 0 0
_ghostdecor       _decor             collide=0
# DESC:  
# BBOX: 0 0 0 0 0 0
light1.0          _ghostdecor        thingflags=0x1 light=1
# DESC:  
# BBOX: 0 0 0 0 0 0
light0.0          _ghostdecor        thingflags=0x1
# DESC:  
# BBOX: -.13015 -.091058 -.100298 .13015 .091058 .100298
roofvent2         _walkstruct        model3d=01rfd2.3do size=.226595 movesize=.226595
# DESC:  
# BBOX: -.040113 -.035949 -.021618 .040113 .035949 .021618
shieldrecharge    _powerup           thingflags=0x400 model3d=shld.3do cog=pow_shields.cog
# DESC:  
# BBOX: -.027845 -.025398 -.02252 .027845 .025398 .024913
healthpack        _powerup           thingflags=0x408 model3d=hepk.3do cog=pow_health.cog
# DESC:  
# BBOX: 0 0 0 0 0 0
_cpowerup         _powerup           typeflags=0x11
# DESC:  
# BBOX: -.022216 -.005309 -.024793 .022216 .005309 .024793
singleseqcharge   _cpowerup          thingflags=0x400 model3d=seqp_1.3do cog=pow_single_sequencer_m.cog
# DESC:  
# BBOX: -.01636 -.014643 -.011603 .017727 .017747 .016258
railcharges       _cpowerup          thingflags=0x400 model3d=rcrg.3do cog=pow_railcharges_m.cog
# DESC:  
# BBOX: -.015415 -.017797 -.028888 .015415 .017797 .028888
bactatank         _powerup           thingflags=0x400 model3d=bact.3do cog=pow_bacta.cog respawn=60
# DESC:  
# BBOX: -.007791 -.090587 -.006258 .007791 .017998 .006258
+ebolt            +bryarbolt         model3d=bry1.3do vel=(0/3.5/0) damage=10 mindamage=5
# DESC:  
# BBOX: -.034444 -.015005 -.120724 .034894 .020299 .062428
scouttrooper_m    _humanactor        thingflags=0x20000400 model3d=sc.3do puppet=st.pup soundclass=st_s1.snd cog=actor_sc_pnch.cog maxvel=.25 weapon=+ebolt weapon2=+st_punch health=80 maxhealth=80 maxthrust=1 maxrotthrust=90 typeflags=0x20001 fireoffset=(.0048/.05/.012) aiclass=sc_def.ai
# DESC:  
# BBOX: 0 0 0 0 0 0
+repeaterhit      +laserhit          sprite=rptx.spr soundclass=exp_rpthit.snd
# DESC:  
# BBOX: 0 0 0 0 0 0
+rpt_sparks       none               orient=(0/0/0) type=particle move=physics timer=.699 mass=.1 physflags=0x400201 vel=(0/0/.2) typeflags=0x27 material=sparky.mat range=.04 rate=24 maxthrust=7 elementsize=.005 count=24
# DESC:  
# BBOX: 0 0 0 0 0 0
+rptfleshhit      _explosion         thingflags=0x1 light=.1 timer=.8 soundclass=exp_fleshy.snd creatething=+rpt_sparks typeflags=0x12 blasttime=.699 maxlight=.3
# DESC:  
# BBOX: -.005771 -.015021 -.00466 .005771 .003935 .00466
+repeaterball     _weapon            thingflags=0x20000001 light=.3 model3d=rpt0.3do size=.005 movesize=.005 soundclass=rep.snd creatething=+smflash vel=(0/6/0) explode=+repeaterhit fleshhit=+rptfleshhit damage=20 mindamage=5 typeflags=0x440d rate=20
# DESC:  
# BBOX: -.005771 -.015021 -.00466 .005771 .003935 .00466
+eball            +repeaterball      damage=8 mindamage=4
# DESC:  
# BBOX: -.03619 -.019179 -.124701 .024091 .063953 .043406
stormtroop3_m     _humanactor        thingflags=0x20000400 model3d=s3.3do puppet=s3.pup soundclass=st3_s1.snd cog=actor_i2_pnch.cog maxvel=.25 weapon=+eball weapon2=+st_punch health=70 maxhealth=70 maxthrust=.9 maxrotthrust=90 typeflags=0x20001 fireoffset=(.0098/.057/.01) aiclass=streapdefault.ai
# DESC:  
# BBOX: -.034444 -.015005 -.120724 .034894 .020299 .062428
scouttrooper      _humanactor        thingflags=0x20000400 model3d=sc.3do puppet=st.pup soundclass=st.snd cog=actor_sc_pnch.cog maxvel=.25 weapon=+ebolt weapon2=+st_punch health=80 maxhealth=80 maxthrust=1 maxrotthrust=90 typeflags=0x20001 fireoffset=(.0048/.05/.012) aiclass=sc_def.ai
# DESC:  
# BBOX: -.020824 -.043225 .000892 .011798 .043225 .024826
detonator         _cpowerup          thingflags=0x400 model3d=detp.3do cog=pow_thermal_m.cog
# DESC:  
# BBOX: -.009653 -.02399 -.017867 .009653 .02399 .017867
powercell         _powerup           thingflags=0x400 model3d=pcel.3do cog=pow_power_m.cog
# DESC:  
# BBOX: -.021089 -.023005 -.035347 .016307 .023005 .035347
revivepack        _powerup           thingflags=0x400 model3d=packrev.3do cog=pow_revive.cog respawn=60
# DESC:  
# BBOX: -.005653 -.113701 -.004853 .005653 .02259 .004853
+lasercannon      +stlaser           light=.5 size=.015 movesize=.015 vel=(0/4/0) fleshhit=+laserhit damage=40
# DESC:  
# BBOX: -.161624 -.239125 -.45412 .160571 .250864 .223596
atst_newdrvr      _humanactor        thingflags=0x4 model3d=at.3do size=.3 movesize=.3 puppet=atst.pup soundclass=atst.snd cog=actor_atstdriver.cog mass=0 maxvel=.4 weapon=+lasercannon health=400 maxhealth=400 maxthrust=.3 maxrotthrust=10 typeflags=0x100 fireoffset=(0/.2/0) aiclass=atstdefault.ai
# DESC:  
# BBOX: 0 0 0 0 0 0
_droidactor       _humanactor        physflags=0x2004a4f typeflags=0x88140
# DESC:  
# BBOX: -.033545 -.020533 -.064529 .03321 .020809 .02539
r5                _droidactor        thingflags=0x20000400 model3d=r5.3do size=.036 movesize=.036 puppet=r5.pup soundclass=r2.snd cog=actor_r5.cog maxvel=.4 health=50 maxhealth=50 maxthrust=.4 maxrotthrust=50 typeflags=0x180140 aiclass=drdefault.ai
# DESC:  
# BBOX: -.1 -.1 -.099985 .1 .1 .099985
crate4_3          _walkstruct        model3d=crt4-3.3do size=.173196 movesize=.173196
# DESC:  
# BBOX: -.027923 -.032221 -.032264 .027923 .032221 .032264
crate6_1          _walkstruct        thingflags=0x400448 model3d=crt6-1.3do size=.045598 movesize=.045598 cog=actor_cs.cog
# DESC:  
# BBOX: -.037893 -.021815 -.12043 .03746 .022321 .066407
rcommando         _humanactor        thingflags=0x20000400 model3d=rc.3do size=.06595 movesize=.06595 puppet=ic.pup soundclass=rc.snd cog=actor_rc.cog maxvel=.3 weapon=+elaser weapon2=+st_punch health=90 maxhealth=90 maxthrust=1.1 maxrotthrust=90 typeflags=0x120001 fireoffset=(.014/.059/.031) aiclass=rc_def.ai
# DESC:  
# BBOX: 0 0 0 0 0 0
_flyactor         _actor             airdrag=2 physflags=0x2007606 maxvel=.8 maxthrust=.5
# DESC:  
# BBOX: 0 0 0 0 0 0
+sentry_exp       +grenade_exp       damage=5 force=50
# DESC:  
# BBOX: -.037102 -.076502 -.041518 .037102 .046066 .03417
robotsentry       _flyactor          thingflags=0x20000400 model3d=rs.3do size=.055 movesize=.055 soundclass=rs.snd cog=actor_sd.cog mass=30 physflags=0x2006a06 maxvel=.6 weapon=+bryarbolt health=60 maxhealth=60 maxthrust=.8 maxrotthrust=80 typeflags=0x2008120 fireoffset=(-.0073/.0393/0) explode=+sentry_exp aiclass=sddefault.ai
# DESC:  
# BBOX: -.03619 -.019179 -.124701 .024091 .064673 .043406
stormtroop2       _humanactor        thingflags=0x20000400 model3d=s2.3do puppet=st.pup soundclass=st2.snd cog=actor_st_pnch.cog maxvel=.25 weapon=+elaser weapon2=+st_punch health=70 maxhealth=70 maxthrust=.9 maxrotthrust=90 typeflags=0x20001 fireoffset=(.0048/.05/.012) aiclass=st_def.ai
# DESC:  
# BBOX: -.2 -.0125 -.2 .2 .0125 .2
06expwall1a       _walkstruct        thingflags=0x48 model3d=06wexp1a.3do size=.333119 movesize=.333119
# DESC:  
# BBOX: -.016822 -.05557 -.026043 .007966 .058 .014355
strifle           _powerup           thingflags=0x400 model3d=strp.3do cog=pow_strifle_m.cog
# DESC:  
# BBOX: -.03092 -.030162 -.030457 .03092 .030162 .030457
darkside          _powerup           thingflags=0x400 model3d=mana1.3do cog=pow_darkside.cog respawn=60
# DESC:  
# BBOX: -.016734 -.016734 -.026596 .016734 .016734 .026596
powerboost        _powerup           thingflags=0x400 model3d=boost.3do cog=pow_powerboost.cog respawn=60
# DESC:  
# BBOX: -.586164 -1.234832 -.489865 .586164 1.305274 1.427284
liftershuttle     _zwalkstruct       model3d=ltshut.3do size=1.5 movesize=1.5 puppet=cr.pup
# DESC:  
# BBOX: -.018131 -.009419 -.000113 .037525 .027081 .049637
stbp              _powerup           thingflags=0x400 model3d=stpack.3do cog=pow_stbkpk.cog
# DESC:  
# BBOX: -.037458 -.014968 -.120277 .037328 .026217 .066384
rofficer          _humanactor        thingflags=0x20000400 model3d=ro.3do size=.0667 movesize=.0667 puppet=io.pup soundclass=rc.snd cog=actor_ro.cog maxvel=.3 weapon=+ebolt weapon2=+st_punch health=30 maxthrust=.9 maxrotthrust=80 typeflags=0x120001 fireoffset=(.05/.065/.014) aiclass=ro_def.ai
# DESC:  
# BBOX: -.084092 -.086378 -.131298 .049191 .102691 -.08341
deadrebelsoldier1 _ghostdecor        model3d=rc-dead1.3do size=.173969 movesize=.173969
# DESC:  
# BBOX: -.034704 -.088802 -.168907 .08018 .097818 -.080358
deadrebelsoldier2 _ghostdecor        model3d=rc-dead2.3do size=.173969 movesize=.173969
# DESC:  
# BBOX: -.035299 -.024241 -.207447 .074687 .046537 -.07682
deadrebelofficer1 _ghostdecor        model3d=ro-dead1.3do size=.174602 movesize=.174602
# DESC:  
# BBOX: -.2 -.0125 -.2 .2 .0125 .2
06expwall1b       _walkstruct        thingflags=0x48 model3d=06wexp1b.3do size=.333119 movesize=.333119
# DESC:  
# BBOX: -.349725 -.412297 -.47738 .348866 .379966 .482264
tiefighter        _zwalkstruct       model3d=tief.3do size=.659152 movesize=.659152
# DESC:  
# BBOX: 0 0 0 0 0 0
+tiebomb_exp      _explosion         thingflags=0x1 light=.2 timer=.8 sprite=tiex.spr soundclass=exp_tie.snd typeflags=0x17 damage=100 blasttime=.699 force=300 maxlight=.8 range=1
# DESC:  
# BBOX: 0 0 0 0 0 0
08tiebomb         _weapon            sprite=tiebomb.spr soundclass=det.snd surfdrag=3 airdrag=.8 mass=1 physflags=0x225 vel=(0/0/-1) angvel=(90/45/90) explode=+tiebomb_exp damageclass=0x1 typeflags=0x30d
# DESC:  
# BBOX: 0 0 0 0 0 0
+bigpunch         +punch             damage=20
# DESC:  
# BBOX: -.020392 -.135722 -.016856 .021073 .013215 .015722
+ewebround        +bryarbolt         model3d=eweb0.3do soundclass=stlaser.snd vel=(0/10/0) damage=100 rate=10
# DESC:  
# BBOX: -.005653 -.113701 -.004853 .005653 .02259 .004853
+sclaser          +stlaser           vel=(0/36/0) damage=100
# DESC:  
# BBOX: 0 0 0 0 0 0
+flashbomb_exp    _explosion         thingflags=0x1 light=1 timer=.8 sprite=detx.spr soundclass=exp_det.snd creatething=+firecloud typeflags=0x17 damage=5 blasttime=.699 force=100 maxlight=1 range=.45
# DESC:  
# BBOX: 0 0 0 0 0 0
+flashfire_exp    _explosion         timer=.8 creatething=+flashbomb_exp typeflags=0x17 damage=75 blasttime=.699 range=.05
# DESC:  
# BBOX: 0 0 0 0 0 0
+flash_exp        none               orient=(0/0/0) type=cog thingflags=0x400 creatething=+flashfire_exp cog=exp_flash.cog
# DESC:  
# BBOX: -.012516 -.012516 -.012524 .012516 .012516 .014655
+flashbomb2       _weapon            timer=3 model3d=fls0.3do size=.014895 movesize=.014895 soundclass=det.snd surfdrag=3 airdrag=.8 mass=1 physflags=0x225 vel=(0/2/1.5) angvel=(90/45/90) buoyancy=.25 explode=+flash_exp fleshhit=+flash_exp damageclass=0x4 typeflags=0x40309
# DESC:  
# BBOX: -.012516 -.012516 -.012524 .012516 .012516 .014655
+flashbomb1       +flashbomb2        timer=10 typeflags=0x4030d
# DESC:  
# BBOX: 0 0 0 0 0 0
+crossbowhit      +laserhit          sprite=bowx2.spr soundclass=exp_bowhit.snd
# DESC:  
# BBOX: -.020392 -.135722 -.016856 .021073 .013215 .015722
+crossbowbolt     _weapon            thingflags=0x20000001 light=.5 model3d=bow0.3do size=.005 movesize=.005 soundclass=bow.snd creatething=+lgflash vel=(0/5/0) angvel=(0/0/120) explode=+crossbowhit fleshhit=+crossbowhit damage=60 mindamage=20 damageclass=0x4 typeflags=0x20440d rate=10
# DESC:  
# BBOX: 0 0 0 0 0 0
+crossbowhit2     _explosion         thingflags=0x1 light=.2 timer=.5 sprite=bowx.spr typeflags=0x33 blasttime=.3 maxlight=.4
# DESC:  
# BBOX: -.014666 -.12215 -.016699 .015346 .011894 .015879
+crossbowbolt2    +crossbowbolt      model3d=bow1.3do soundclass=none creatething=none explode=+crossbowhit2 fleshhit=+crossbowhit2 damage=40 typeflags=0x440d
# DESC:  
# BBOX: -.020392 -.135722 -.016856 .021073 .013215 .015722
+crossbowbolt3    +crossbowbolt      typeflags=0x28440d
# DESC:  
# BBOX: 0 0 0 0 0 0
+firecloud2       +firecloud         elementsize=.0125
# DESC:  
# BBOX: 0 0 0 0 0 0
_debris2          _debris            explode=+smflash
# DESC:  
# BBOX: -.026724 -.026724 -.044654 .026724 .026724 .044654
shrapnel2_1       _debris2           model3d=shrp_1.3do size=.045 movesize=.045
# DESC:  
# BBOX: -.011171 -.010389 -.042003 .011171 .010389 .042003
shrapnel2_2       _debris2           model3d=shrp_2.3do size=.04 movesize=.04
# DESC:  
# BBOX: -.015988 -.015443 -.026188 .015988 .015443 .026188
shrapnel2_3       _debris2           model3d=shrp_3.3do size=.028 movesize=.028
# DESC:  
# BBOX: -.016213 -.014647 -.020855 .016213 .014647 .020855
shrapnel2_4       _debris2           model3d=shrp_4.3do size=.026 movesize=.026
# DESC:  
# BBOX: 0 0 0 0 0 0
+sequencer_exp    _explosion         thingflags=0x1 light=.2 timer=.8 sprite=seqx2.spr soundclass=exp_med.snd creatething=+firecloud2 typeflags=0x17 damage=100 blasttime=.699 force=300 maxlight=.8 range=.6 debris=shrapnel2_1 debris=shrapnel2_2 debris=shrapnel2_3 debris=shrapnel2_4
# DESC:  
# BBOX: -.022216 -.023897 -.005309 .022216 .023897 .005309
+seqchrg          +grenade2          timer=1 model3d=seq0.3do size=.01 movesize=.01 soundclass=seq.snd surfdrag=5 airdrag=1 physflags=0x29d vel=(0/.1/0) angvel=(0/0/0) buoyancy=.15 explode=+sequencer_exp fleshhit=+sequencer_exp typeflags=0x40380
# DESC:  
# BBOX: -.022216 -.023897 -.005309 .022216 .023897 .005309
+seqchrg2         +seqchrg           thingflags=0x20000400 timer=60 cog=class_sequencer.cog
# DESC:  
# BBOX: -.022216 -.023897 -.005309 .022216 .023897 .005309
+seqchrg3         +seqchrg           timer=300
# DESC:  
# BBOX: -.022216 -.023897 -.005309 .022216 .023897 .005309
+seqchrg4         +seqchrg           thingflags=0x20000400 timer=300 cog=class_sequencer.cog
# DESC:  
# BBOX: 0 0 0 0 0 0
+raildet_exp2     +raildet_exp       debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_4
# DESC:  
# BBOX: -.01058 -.013704 -.006548 .010593 .009939 .012329
+raildet          +raildet2          timer=10 explode=+raildet_exp2 fleshhit=+raildet_exp2 damage=20 typeflags=0x24020d
# DESC:  
# BBOX: -.01058 -.013704 -.006548 .010593 .009939 .012329
+lhraildet        +raildet           timer=3 model3d=skr0.3do cog=00_hraildet.cog physflags=0x2200 angvel=(0/0/0) explode=+raildet_exp fleshhit=+raildet_exp typeflags=0x240b81
# DESC:  
# BBOX: 0 0 0 0 0 0
+hraildet_exp     none               orient=(0/0/0) type=cog thingflags=0x400 cog=exp_hrail.cog
# DESC:  
# BBOX: -.01058 -.013704 -.006548 .010593 .009939 .012329
+hraildet         +raildet           model3d=skr0.3do physflags=0x2200 angvel=(0/0/0) explode=+hraildet_exp fleshhit=+hraildet_exp
# DESC:  
# BBOX: -.01058 -.013704 -.006548 .010593 .009939 .012329
+hraildet2        +raildet2          model3d=skr0.3do physflags=0x2200 angvel=(0/0/0) explode=+hraildet_exp fleshhit=+hraildet_exp
# DESC:  
# BBOX: -.01058 -.013704 -.006548 .010593 .009939 .012329
+nchraildet       +hraildet          collide=0
# DESC:  
# BBOX: -.01058 -.013704 -.006548 .010593 .009939 .012329
+nchraildet2      +hraildet2         collide=0
# DESC:  
# BBOX: -.01058 -.013704 -.006548 .010593 .009939 .012329
+sraildet         +raildet           vel=(0/1/0) angvel=(0/90/90)
# DESC:  
# BBOX: 0 0 0 0 0 0
+conccloud        +dustcloud         rate=512 maxthrust=100 count=256 minsize=.012 pitchrange=5
# DESC:  
# BBOX: 0 0 0 0 0 0
+conc_exp         _explosion         thingflags=0x1 light=.3 timer=1 sprite=conx.spr soundclass=exp_conc.snd creatething=+conccloud typeflags=0x17 damage=80 blasttime=1 force=200 maxlight=.8 range=.8
# DESC:  
# BBOX: -.01341 -.149802 -.01341 .01341 .015603 .01341
+concbullet       _weapon            thingflags=0x20000001 model3d=con0.3do size=.005 movesize=.005 soundclass=conc.snd creatething=+lgflash vel=(0/7/0) explode=+conc_exp fleshhit=+conc_exp damage=20 typeflags=0x20000d
# DESC:  
# BBOX: 0 0 0 0 0 0
+concblast2p      +dustcloud         orient=(90/0/0) range=.0015 rate=128 elementsize=.005 count=64 minsize=.0012 pitchrange=5
# DESC:  
# BBOX: 0 0 0 0 0 0
+concblast3p      +concblast2p       typeflags=0x2b material=00ramp4.mat maxthrust=100
# DESC:  
# BBOX: 0 0 0 0 0 0
+concblast2       _weapon            size=.005 movesize=.005 fleshhit=+concblast3p trailthing=+concblast2p elementsize=.3 damage=80 mindamage=20 typeflags=0xa00d range=5 rate=1
# DESC:  
# BBOX: 0 0 0 0 0 0
+force_repel      _explosion         thingflags=0x1 light=.5 timer=.4 soundclass=exp_dest.snd typeflags=0x52 blasttime=.3 force=200 maxlight=1 range=.5
# DESC:  
# BBOX: -.007659 -.000007 -.008551 .008867 .076357 .007405
+force_ltpeice    none               orient=(0/0/0) type=weapon move=physics timer=.25 model3d=ligt2.3do size=.005 movesize=.005 physflags=0x200 maxrotvel=360 angvel=(0/0/360)
# DESC:  
# BBOX: 0 0 0 0 0 0
+lightninghit     none               orient=(0/0/0) type=particle move=physics timer=.2 typeflags=0x2b material=00ramp4.mat range=.02 rate=16 maxthrust=8 elementsize=.003 count=16
# DESC:  
# BBOX: 0 0 0 0 0 0
+force_lightning  _weapon            thingflags=0x1 light=.1 timer=.05 size=.005 movesize=.005 angvel=(0/0/360) explode=+lightninghit fleshhit=+lightninghit trailthing=+force_ltpeice elementsize=.075 trailcylradius=.04 trailrandangle=25 damage=25 damageclass=0x8 typeflags=0x1800d range=1.5
# DESC:  
# BBOX: 0 0 0 0 0 0
+force_lightning2 +force_lightning   damage=15
# DESC:  
# BBOX: 0 0 0 0 0 0
+force_lightning3 +force_lightning   damage=20
# DESC:  
# BBOX: 0 0 0 0 0 0
+force_lightning4 +force_lightning  
# DESC:  
# BBOX: 0 0 0 0 0 0
+fsaberexplode    none               orient=(0/0/0) type=explosion thingflags=0x400 timer=.2 cog=00_thrownsaber.cog typeflags=0x46 damage=60 damageclass=0x10 blasttime=.1 force=20 range=.3
# DESC:  
# BBOX: 0 0 0 0 0 0
+fsaberflesh      none               orient=(0/0/0) type=explosion thingflags=0x400 timer=.2 cog=00_thrownsaber.cog typeflags=0x46 damage=60 damageclass=0x10 blasttime=.1 force=20 range=.2
# DESC:  
# BBOX: -.002761 -.017436 -.002868 .002754 .16643 .003496
+force_saber      _weapon            thingflags=0x20000001 light=.5 timer=2 model3d=sab0.3do size=.05 movesize=.05 vel=(0/1.1/0) angvel=(0/-400/0) explode=+fsaberexplode fleshhit=+fsaberflesh damage=80 damageclass=0x10 typeflags=0x10d
# DESC:  
# BBOX: -.036175 -.01856 -.009087 .028413 .153538 .003912
+fsab_green       +force_saber       model3d=sab0gree.3do
# DESC:  
# BBOX: -.036175 -.01856 -.009087 .028413 .153538 .003912
+fsab_orange      +force_saber       model3d=sab0oran.3do
# DESC:  
# BBOX: -.036175 -.01856 -.009087 .028413 .153538 .003912
+fsab_red         +force_saber       model3d=sab0red.3do
# DESC:  
# BBOX: -.036175 -.01856 -.009087 .028413 .153538 .003912
+fsab_dred        +force_saber       model3d=sab0dred.3do
# DESC:  
# BBOX: -.036175 -.01856 -.009087 .028413 .153538 .003912
+fsab_blue        +force_saber       model3d=sab0blue.3do
# DESC:  
# BBOX: -.036175 -.01856 -.009087 .028413 .153538 .003912
+fsab_purple      +force_saber       model3d=sab0purp.3do
# DESC:  
# BBOX: -.036175 -.01856 -.009087 .028413 .153538 .003912
+fsab_yellow      +force_saber       model3d=sab0yell.3do
# DESC:  
# BBOX: 0 0 0 0 0 0
+force_shield     none               orient=(0/0/0) type=cog move=physics timer=41 particle=sphere.par angvel=(60/60/60)
# DESC:  
# BBOX: 0 0 0 0 0 0
+force_blind      +dustcloud         material=00teleport.mat range=.05 rate=32 elementsize=.003 minsize=.02 pitchrange=1 yawrange=1
# DESC:  
# BBOX: 0 0 0 0 0 0
+smoke            none               orient=(0/0/0) type=particle move=physics timer=.8 physflags=0x20000 vel=(0/0/.12) angvel=(0/90/0) typeflags=0x3e material=00gsmoke.mat range=.03 elementsize=.005 count=8
# DESC:  
# BBOX: 0 0 0 0 0 0
+dest_trail       +smoke             vel=(0/0/0) material=dstructparts.mat
# DESC:  
# BBOX: 0 0 0 0 0 0
+dest_cloud       +firecloud         material=dstructparts.mat
# DESC:  
# BBOX: 0 0 0 0 0 0
+force_dest1      _explosion         thingflags=0x1 light=.1 timer=.4 sprite=destruct8.spr soundclass=exp_dest.snd creatething=+dest_cloud typeflags=0x53 damage=15 damageclass=0x8 blasttime=.3 force=100 maxlight=.5 range=.5
# DESC:  
# BBOX: 0 0 0 0 0 0
+force_dest2      _explosion         thingflags=0x1 light=.2 timer=.6 sprite=destruct8.spr soundclass=exp_dest.snd creatething=+dest_cloud typeflags=0x53 damage=30 damageclass=0x8 blasttime=.4 force=200 maxlight=.6 range=1
# DESC:  
# BBOX: 0 0 0 0 0 0
+force_dest3      _explosion         thingflags=0x1 light=.3 timer=.8 sprite=destruct8.spr soundclass=exp_dest.snd creatething=+dest_cloud typeflags=0x53 damage=45 damageclass=0x8 blasttime=.5 force=300 maxlight=.7 range=1.5
# DESC:  
# BBOX: 0 0 0 0 0 0
+force_dest4      _explosion         thingflags=0x1 light=.4 timer=1 sprite=destruct8.spr soundclass=exp_dest.snd creatething=+dest_cloud typeflags=0x53 damage=60 damageclass=0x8 blasttime=.6 force=400 maxlight=.8 range=2
# DESC:  
# BBOX: 0 0 0 0 0 0
+force_dest_p1    _weapon            thingflags=0x20000401 light=.4 sprite=forcedstruct_150.spr cog=00_desttrail.cog vel=(0/7/0) explode=+force_dest1 fleshhit=+force_dest1 damage=20 typeflags=0xd
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
+force_heal       none               orient=(0/0/0) type=particle move=physics timer=2 angvel=(90/90/90) typeflags=0x3f material=00teleport.mat range=.3 rate=128 maxthrust=.01 elementsize=.003 count=128
# DESC:  
# BBOX: 0 0 0 0 0 0
+carbosmoke       +smoke            
# DESC:  
# BBOX: 0 0 0 0 0 0
+heavysmoke       +smoke             timer=1.6 range=.1 rate=32 count=64
# DESC:  
# BBOX: 0 0 0 0 0 0
+twinkle          none               orient=(0/0/0) type=explosion thingflags=0x1 timer=.5 sprite=twinkle.spr typeflags=0x11
# DESC:  
# BBOX: 0 0 0 0 0 0
+sspks_wall       none               orient=(0/0/0) type=particle move=physics timer=.1 mass=.05 physflags=0x400201 vel=(0/0/.15) typeflags=0x2b material=00ramp1.mat range=.03 rate=4 maxthrust=8 elementsize=.003 count=48
# DESC:  
# BBOX: 0 0 0 0 0 0
+ssparks_wall     _explosion         thingflags=0x1 light=.3 timer=.8 soundclass=exp_saber_wall.snd creatething=+sspks_wall typeflags=0x112 blasttime=.699 maxlight=.5 flashrgb=(80/90/80)
# DESC:  
# BBOX: 0 0 0 0 0 0
+sspks_blood      none               orient=(0/0/0) type=particle move=physics timer=.2 mass=.1 physflags=0x400201 vel=(0/0/.2) typeflags=0x2b material=00ramp2.mat range=.04 rate=4 maxthrust=9 elementsize=.005 count=24
# DESC:  
# BBOX: 0 0 0 0 0 0
+ssparks_blood    _explosion         thingflags=0x1 light=.1 timer=.8 soundclass=exp_saber_blood.snd creatething=+sspks_blood typeflags=0x12 blasttime=.699 maxlight=.3
# DESC:  
# BBOX: 0 0 0 0 0 0
+sspks_saber      none               orient=(0/0/0) type=particle move=physics timer=.3 mass=.05 physflags=0x400200 vel=(0/0/.1) typeflags=0x27 material=00teleport.mat range=.02 rate=32 maxthrust=8 elementsize=.003 count=32
# DESC:  
# BBOX: 0 0 0 0 0 0
+ssparks_saber    _explosion         thingflags=0x1 light=.8 timer=.8 soundclass=exp_saber_saber.snd creatething=+sspks_saber typeflags=0x112 blasttime=.699 maxlight=1 flashrgb=(150/160/150)
# DESC:  
# BBOX: 0 0 0 0 0 0
+sparks           none               orient=(0/0/0) type=particle move=physics timer=1 mass=1 physflags=0x201 typeflags=0x3d material=embers.mat range=.03 rate=32 maxthrust=20 elementsize=.01 count=32
# DESC:  
# BBOX: 0 0 0 0 0 0
+swsparks         +sparks            vel=(0/3/0)
# DESC:  
# BBOX: 0 0 0 0 0 0
_droppowerup      _powerup           timer=30 height=.011 physflags=0x41 typeflags=0x0
# DESC:  
# BBOX: -.021089 -.023005 -.035347 .016307 .023005 .035347
+backpack         _droppowerup       thingflags=0x400 model3d=bpck.3do cog=pow_backpack_m.cog height=.036337 typeflags=0x4
# DESC:  
# BBOX: -.003197 -.019383 -.01078 .003197 .019383 .01078
blastechpistol    _powerup           thingflags=0x400 model3d=blsp.3do cog=pow_blastech_m.cog
# DESC:  BryarPistol
# BBOX: -.003197 -.019383 -.01078 .003197 .019383 .01078
bryarpistol       _powerup           thingflags=0x400 model3d=bryp.3do cog=pow_bryar_m.cog
# DESC:  
# BBOX: -.017868 -.016799 -.017934 .017868 .016799 .017934
singledetonator   _cpowerup          thingflags=0x400 model3d=detp_1.3do cog=pow_single_thermal_m.cog
# DESC:  
# BBOX: -.017868 -.016799 -.017934 .017868 .016799 .017934
singleflashbomb   _cpowerup          thingflags=0x400 model3d=flsp_1.3do cog=pow_single_flashbomb_m.cog
# DESC:  
# BBOX: -.034888 -.055377 .000124 .034343 .059997 .031925
crossbow          _powerup           thingflags=0x400 model3d=bowp.3do cog=pow_crossbow_m.cog
# DESC:  
# BBOX: -.00548 -.051379 -.021767 .00548 .039969 .012035
repeatergun       _powerup           thingflags=0x400 model3d=rptp.3do cog=pow_repeater_m.cog
# DESC:  
# BBOX: -.018484 -.070714 -.030025 .019883 .081826 .013227
railgun           _powerup           thingflags=0x400 model3d=rldp.3do cog=pow_railgun_m.cog respawn=60
# DESC:  
# BBOX: -.030347 -.072704 -.024787 .009211 .072704 .024341
concrifle         _powerup           thingflags=0x400 model3d=conp.3do cog=pow_concrifle_m.cog respawn=60
# DESC:  
# BBOX: 0 0 0 0 0 0
+cpel_exp         none               orient=(0/0/0) type=cog thingflags=0x400 cog=exp_cpel.cog
# DESC:  
# BBOX: -.003123 -.004817 -.002719 .003123 .004817 .002719
+carbpellet       _weapon            thingflags=0x20000400 timer=3 model3d=cbn0.3do size=.005939 movesize=.005939 soundclass=cpel.snd cog=00_smoketrail.cog surfdrag=3 airdrag=.8 mass=1 physflags=0x225 vel=(0/10/0) angvel=(90/45/90) buoyancy=.25 explode=+cpel_exp fleshhit=+cpel_exp damageclass=0x4 typeflags=0x440309
# DESC:  
# BBOX: 0 0 0 0 0 0
+carbchunks       none               orient=(0/0/0) type=particle move=physics timer=.2 typeflags=0x27 material=00explosion.mat range=.2 rate=48 maxthrust=16 elementsize=.015 count=96
# DESC:  
# BBOX: -.058497 -.057454 -.12002 .056291 .06851 .063952
+carbonitechar    _decor             type=item collide=0 thingflags=0x400 timer=15 model3d=crbn.3do size=.065 movesize=.065 cog=00_carbstatue.cog
# DESC:  
# BBOX: 0 0 0 0 0 0
+nothing_exp      _explosion         thingflags=0x1 light=.2 timer=.8 sprite=detx.spr soundclass=exp_det.snd creatething=+firecloud typeflags=0x17 blasttime=.699 maxlight=.8
# DESC:  
# BBOX: 0 0 0 0 0 0
+small_exp        _explosion         thingflags=0x1 light=.2 timer=.8 sprite=detx.spr soundclass=exp_det.snd creatething=+firecloud typeflags=0x17 damage=20 blasttime=.699 force=60 maxlight=.8 range=.1
# DESC:  
# BBOX: 0 0 0 0 0 0
+medium_exp       _explosion         thingflags=0x1 light=.2 timer=.8 sprite=detx.spr soundclass=exp_det.snd creatething=+firecloud typeflags=0x17 damage=30 blasttime=.699 force=100 maxlight=.8 range=.25
# DESC:  
# BBOX: 0 0 0 0 0 0
+large_exp        _explosion         thingflags=0x1 light=.2 timer=.8 sprite=detx.spr soundclass=exp_det.snd creatething=+firecloud typeflags=0x17 damage=40 blasttime=.699 force=100 maxlight=.8 range=.45
# DESC:  
# BBOX: -.035332 -.013185 -.120103 .035579 .016413 .064197
+projection       none               orient=(0/0/0) type=cog thingflags=0x400 timer=35 model3d=mj.3do size=.06 movesize=.06 puppet=ky.pup cog=thing_proj_image.cog
# DESC:  
# BBOX: -.005771 -.015021 -.00466 .005771 .003935 .00466
+farsight         none               orient=(0/0/0) type=cog collide=1 move=physics timer=25 model3d=farsight.3do size=.05 movesize=.1 airdrag=.5 mass=.05 physflags=0xa00 maxvel=.5
# DESC:  
# BBOX: -.086772 -.114302 -.025049 .086772 .142704 .149408
+eweb             _actor             collide=0 thingflags=0x20000400 model3d=eweb.3do size=.197239 movesize=.197239 puppet=eweb.pup surfdrag=1000 airdrag=1000 mass=0 maxvel=0 health=10 maxrotthrust=0 typeflags=0x20
# DESC:  
# BBOX: -.035332 -.013185 -.120103 .035579 .016413 .064197
+ewebrider        _ghostdecor        model3d=mj.3do size=.17358 movesize=.17358
# DESC:  
# BBOX: 0 0 0 0 0 0
+carbsparks       +sparks            timer=2 vel=(0/1/0) material=00carbonite.mat range=.07 count=96 pitchrange=90 yawrange=90
# DESC:  
# BBOX: -.003197 -.019383 -.01078 .003197 .019383 .01078
+dblastechpistol  _powerup           model3d=blsp.3do
# DESC:  
# BBOX: -.012516 -.012516 -.012524 .012516 .012516 .014655
+dudgrenade       +grenade2          explode=singledetonator typeflags=0x80120
# DESC:  
# BBOX: -.012516 -.012516 -.012524 .012516 .012516 .014655
+dudflash         +flashbomb2        explode=singleflashbomb typeflags=0x80120
# DESC:  
# BBOX: -.01058 -.013704 -.006548 .010593 .009939 .012329
+dudrail          +grenade1          timer=3 model3d=rld0.3do airdrag=1.8 angvel=(0/0/0) explode=+sparks typeflags=0x80120
# DESC:  
# BBOX: -.022216 -.023897 -.005309 .022216 .023897 .005309
+nospinseqcharge  singleseqcharge    model3d=seq0.3do
# DESC:  
# BBOX: -.022216 -.023897 -.005309 .022216 .023897 .005309
+dudseqchrg       +seqchrg           explode=+nospinseqcharge
# DESC:  
# BBOX: -.01058 -.013704 -.006548 .010593 .009939 .012329
+dudseek          +dudrail           model3d=skr0.3do
# DESC:  
# BBOX: 0 0 0 0 0 0
+force_clightn    +force_lightning   vel=(0/5/0) damage=0
# DESC:  
# BBOX: -.058497 -.057454 -.12002 .056291 .06851 -.064426
+carbonitestem    _zwalkstruct       model3d=crbnstem.3do size=.19317 movesize=.19317
# DESC:  
# BBOX: 0 0 0 0 0 0
+carbodeb_exp     none               orient=(0/0/0) type=particle move=physics timer=.3 mass=.5 physflags=0x60000 typeflags=0x3f material=00gsmoke.mat range=.03 rate=128 maxthrust=5 elementsize=.005 count=32
# DESC:  
# BBOX: 0 0 0 0 0 0
_carbodebris      none               orient=(0/0/0) type=weapon collide=1 move=physics timer=3 airdrag=3 mass=30 physflags=0x201 angvel=(5/10/0) explode=+carbodeb_exp damage=5 damageclass=0x1 typeflags=0x10c
# DESC:  
# BBOX: -.01358 -.014556 -.038687 .01358 .014556 .038687
+carbodebris_0    _carbodebris       model3d=crbn_0.3do size=.04 movesize=.04
# DESC:  
# BBOX: -.010249 -.011301 -.016265 .010249 .011301 .016265
+carbodebris_1    _carbodebris       model3d=crbn_1.3do size=.017 movesize=.017
# DESC:  
# BBOX: -.02119 -.014471 -.024481 .02119 .014471 .024481
+carbodebris_2    _carbodebris       model3d=crbn_2.3do size=.031 movesize=.031
# DESC:  
# BBOX: -.00871 -.015466 -.02829 .00871 .015466 .02829
+carbodebris_3    _carbodebris       model3d=crbn_3.3do size=.032 movesize=.032
# DESC:  
# BBOX: -.003675 -.006301 -.001462 .003675 .006301 .001462
+carbodebris_4    _carbodebris       model3d=crbn_4.3do size=.007 movesize=.007
# DESC:  
# BBOX: -.549776 -.490931 -.315187 .549776 .4752 .320955
tiebomber_crsh    _zwalkstruct       collide=1 move=physics model3d=tieb.3do size=.5 movesize=.5 puppet=cr.pup mass=3000 physflags=0x201
# DESC:  
# BBOX: -.003197 -.019383 -.01078 .003197 .019383 .01078
+fpbryarpistol    bryarpistol        collide=0 timer=4 typeflags=0x0 respawn=0
# DESC:  
# BBOX: -.016822 -.05557 -.026043 .007966 .058 .014355
+fpstrifle        strifle            collide=0 timer=4 typeflags=0x0 respawn=0
# DESC:  
# BBOX: -.017868 -.016799 -.017934 .017868 .016799 .017934
+fpdetolive       _powerup           collide=0 thingflags=0x400 timer=4 model3d=detp_1.3do cog=pow_livethermal.cog typeflags=0x0 respawn=0
# DESC:  
# BBOX: -.00548 -.051379 -.021767 .00548 .039969 .012035
+fprepeatergun    repeatergun        collide=0 timer=4 typeflags=0x0 respawn=0
# DESC:  
# BBOX: -.018484 -.070714 -.030025 .019883 .081826 .013227
+fprailgun        railgun            collide=0 timer=4 typeflags=0x0 respawn=0
# DESC:  
# BBOX: -.030347 -.072704 -.024787 .009211 .072704 .024341
+fpconcrifle      concrifle          collide=0 timer=4 typeflags=0x0 respawn=0
# DESC:  
# BBOX: -.008988 -.030139 -.030139 .008988 .030139 .030139
lightsaber        _powerup           thingflags=0x400 model3d=sabp.3do cog=pow_saber_m.cog
# DESC:  
# BBOX: -.008988 -.030139 -.030139 .008988 .030139 .030139
+fplightsaber     lightsaber         collide=0 timer=4 typeflags=0x0 respawn=0
# DESC:  
# BBOX: -.003197 -.019383 -.01078 .003197 .019383 .01078
+fpblastech       blastechpistol     collide=0 timer=4 typeflags=0x0 respawn=0
# DESC:  
# BBOX: -.009575 -.037873 -.008367 .009575 .037873 .008367
scope             _powerup           thingflags=0x400 model3d=scpe.3do cog=pow_scope_m.cog
# DESC:  
# BBOX: -.009575 -.037873 -.008367 .009575 .037873 .008367
+fpscope          scope              collide=0 timer=4 typeflags=0x0 respawn=0
# DESC:  
# BBOX: -.034888 -.055377 .000124 .034343 .059997 .031925
+fpcrossbow       crossbow           collide=0 timer=4 typeflags=0x0 respawn=0
# DESC:  
# BBOX: -.018484 -.070714 -.030025 .019883 .081826 .013227
railseek          _powerup           thingflags=0x400 model3d=skrp.3do cog=pow_railseek_m.cog respawn=60
# DESC:  
# BBOX: -.018484 -.070714 -.030025 .019883 .081826 .013227
+fprailseek       railseek           collide=0 timer=4 typeflags=0x0 respawn=0
# DESC:  
# BBOX: -.00415 -.047867 -.01666 .00415 .035584 .00712
carbgun           _powerup           thingflags=0x400 model3d=cbnp.3do cog=pow_carbgun_m.cog
# DESC:  
# BBOX: -.00415 -.047867 -.01666 .00415 .035584 .00712
+fpcarbgun        carbgun            collide=0 timer=4 typeflags=0x0 respawn=0
# DESC:  
# BBOX: -.006993 -.00625 -.021875 .006993 .00625 .021875
battery           _powerup           thingflags=0x408 model3d=battery.3do cog=pow_battery_m.cog
# DESC:  
# BBOX: -.006993 -.00625 -.021875 .006993 .00625 .021875
+fpbattery        battery            collide=0 timer=4 typeflags=0x0 respawn=0
# DESC:  
# BBOX: 0 0 0 0 0 0
+telesparks       none               orient=(0/0/0) type=particle timer=.119 typeflags=0x3f material=00teleport.mat range=.03 rate=256 maxthrust=80 elementsize=.002 count=256
# DESC:  
# BBOX: 0 0 0 0 0 0
bubble            none               orient=(0/0/0) type=cog move=physics thingflags=0x10000000 timer=3 sprite=bubble.spr mass=.05 physflags=0x200 vel=(0/0/.3)
# DESC:  
# BBOX: 0 0 0 0 0 0
bubble2           bubble             sprite=bubble2.spr vel=(0/0/.45)
# DESC:  
# BBOX: 0 0 0 0 0 0
bubble3           bubble             sprite=bubble3.spr vel=(0/0/.6)
# DESC:  
# BBOX: 0 0 0 0 0 0
+watersplash      none               orient=(0/0/0) type=explosion timer=.5 sprite=splash.spr typeflags=0x1 damageclass=0x4
# DESC:  
# BBOX: 0 0 0 0 0 0
+watersplash2     +watersplash       sprite=splooshx.spr
# DESC:  
# BBOX: -.016822 -.05557 -.026043 .007966 .058 .014355
+dstrifle         _droppowerup       thingflags=0x400 model3d=strp.3do cog=pow_dstrifle_m.cog height=.035177
# DESC:  
# BBOX: -.012585 -.050304 -.010582 .022579 .027638 .007152
_limb             none               orient=(0/0/0) type=corpse collide=1 move=physics thingflags=0x400 timer=30 model3d=stlimb.3do size=.102337 movesize=.000001 cog=00_bloodtrail.cog surfdrag=1 airdrag=4 mass=15 physflags=0x44261 buoyancy=.5
# DESC:  
# BBOX: -.012585 -.050304 -.010582 .022579 .027638 .007152
+stormlimb        _limb              size=.05 movesize=.01
# DESC:  
# BBOX: -.018484 -.070714 -.030025 .019883 .081826 .013227
+drailgun         _droppowerup       thingflags=0x400 model3d=rldp.3do cog=pow_drailgun_m.cog height=.04442
# DESC:  
# BBOX: -.00548 -.051379 -.021767 .00548 .039969 .012035
+drepeatergun     _droppowerup       thingflags=0x400 model3d=rptp.3do cog=pow_drepeater_m.cog height=.042069
# DESC:  
# BBOX: -.037893 -.014351 -.12043 .03746 .022321 .063011
atatpilot         _humanactor        thingflags=0x20000400 model3d=ap.3do size=.06595 movesize=.06595 puppet=ic.pup soundclass=st.snd cog=actor_tp_pnch.cog maxvel=.3 weapon=+ebolt weapon2=+st_punch health=130 maxhealth=130 maxthrust=1.1 maxrotthrust=90 typeflags=0x20001 fireoffset=(.014/.059/.031) aiclass=tp_def.ai
# DESC:  
# BBOX: 0 0 0 0 0 0
+atst_exp         +grenade_exp       damage=10
# DESC:  
# BBOX: -.009653 -.02399 -.017867 .009653 .02399 .017867
+dpowercell       _droppowerup       thingflags=0x400 model3d=pcel.3do cog=pow_power_m.cog height=.017868
# DESC:  
# BBOX: -.040113 -.035949 -.021618 .040113 .035949 .021618
+dshield          _droppowerup       thingflags=0x400 model3d=shld.3do cog=pow_shields.cog height=.021618
# DESC:  
# BBOX: 0 0 0 0 0 0
+r2r5_exp         +grenade_exp       damage=10 force=50
# DESC:  
# BBOX: 0 0 0 0 0 0
+crate_exp        +sentry_exp        damage=30 force=75 range=.2
# DESC:  
# BBOX: 0 0 0 0 0 0
+scrate_exp       +crate_exp         damage=10 force=25 range=.1
# DESC:  
# BBOX: -.015415 -.017797 -.028888 .015415 .017797 .028888
+dbactatank       _droppowerup       thingflags=0x400 model3d=bact.3do cog=pow_bacta.cog height=.028885
# DESC:  
# BBOX: -.006993 -.00625 -.021875 .006993 .00625 .021875
+dbattery         _droppowerup       thingflags=0x408 model3d=battery.3do cog=pow_battery_m.cog height=.022909
# DESC:  
# BBOX: -.008536 -.014067 -.022418 .008536 .014068 .022418
+denergycell      _droppowerup       thingflags=0x400 model3d=ecel.3do cog=pow_energy_m.cog height=.022435
# DESC:  
# BBOX: -.008988 -.030139 -.030139 .008988 .030139 .030139
+fsaberreturns    lightsaber         timer=5 cog=00_returnsaber.cog mass=0 physflags=0xc00000 typeflags=0x8 respawn=0
# DESC:  
# BBOX: 0 0 0 0 0 0
_gravdebris       none               orient=(0/0/0) type=weapon collide=1 move=physics timer=4.5 mass=.01 physflags=0x201 damage=20 damageclass=0x1 typeflags=0xc
# DESC:  
# BBOX: -.026724 -.026724 -.044654 .026724 .026724 .044654
grshrapnel_1      _gravdebris        model3d=shrp_1.3do size=.045 movesize=.045
# DESC:  
# BBOX: -.011171 -.010389 -.042003 .011171 .010389 .042003
grshrapnel_2      _gravdebris        model3d=shrp_2.3do size=.04 movesize=.04
# DESC:  
# BBOX: -.015988 -.015443 -.026188 .015988 .015443 .026188
grshrapnel_3      _gravdebris        model3d=shrp_3.3do size=.028 movesize=.028
# DESC:  
# BBOX: -.016213 -.014647 -.020855 .016213 .014647 .020855
grshrapnel_4      _gravdebris        model3d=shrp_4.3do size=.026 movesize=.026
# DESC:  
# BBOX: -.026804 -.036274 -.074092 .026804 .036274 .074092
grshrapnel_5      _gravdebris        model3d=shrp_5.3do size=.084 movesize=.084
# DESC:  
# BBOX: 0 0 0 0 0 0
+limb_blood       none               orient=(0/0/0) type=particle move=physics timer=.25 physflags=0x201 vel=(0/0/.2) typeflags=0x27 material=00gsmoke.mat range=.02 rate=48 maxthrust=3 elementsize=.005 count=16
# DESC:  
# BBOX: -.3 -.0125 -.3 .3 .0125 .3
6x6door           _structure         model3d=d6x6.3do size=.474448 movesize=.474448 soundclass=lg_door.snd
# DESC:  
# BBOX: -.15 -.4 -.012306 .15 .4 .012306
3x8catwalk        _walkstruct        model3d=c3x8.3do size=.427378 movesize=.427378
# DESC:  
# BBOX: -.15 -.3 -.012306 .15 .3 .012306
3x6catwalk        _walkstruct        model3d=c3x6.3do size=.335636 movesize=.335636
# DESC:  
# BBOX: -.1 -.0125 -.15 .1 .0125 .15
2x3door_4         _structure         model3d=d2x3_4.3do size=.18071 movesize=.18071 soundclass=sm_door.snd
# DESC:  
# BBOX: -.25 -.05 -.2 .25 .05 .2
m2heater          _walkstruct        thingflags=0x4c model3d=m2heat.3do size=.324037 movesize=.324037
# DESC:  
# BBOX: -.063767 -.055224 -.021878 .063767 .055224 .021878
probedebris_1     _debris            model3d=pb_1.3do size=.0535 movesize=.0535
# DESC:  
# BBOX: -.071973 -.062331 -.029124 .071973 .062331 .029124
probedebris_2     _debris            model3d=pb_2.3do size=.057 movesize=.057
# DESC:  
# BBOX: -.009722 -.012251 -.047823 .009722 .012251 .047823
probedebris_3     _debris            model3d=pb_3.3do size=.0415 movesize=.0415
# DESC:  
# BBOX: -.048787 -.012879 -.031716 .048787 .012879 .031716
probedebris_4     _debris            model3d=pb_4.3do size=.04 movesize=.04
# DESC:  
# BBOX: 0 0 0 0 0 0
+probedroid_exp   _explosion         thingflags=0x1 light=.2 timer=.8 sprite=seqx.spr soundclass=exp_probe.snd typeflags=0x17 damage=40 blasttime=.699 force=200 maxlight=.8 range=.45 debris=probedebris_1 debris=probedebris_2 debris=probedebris_3 debris=probedebris_4
# DESC:  
# BBOX: -.01341 -.149802 -.01341 .01341 .015603 .01341
+turretlaser      +stlaser           light=.5 model3d=con0.3do size=.005 movesize=.005 vel=(0/5/0) fleshhit=+laserhit damage=15 mindamage=5
# DESC:  
# BBOX: -.007791 -.090587 -.006258 .007791 .017998 .006258
+probedroidbolt   +turretlaser       model3d=bry0.3do vel=(0/4/0) damage=30 mindamage=10
# DESC:  
# BBOX: -.074435 -.062273 -.215886 .071944 .062389 .198272
probedroid        _flyactor          thingflags=0x20000400 model3d=pb.3do size=.078 movesize=.078 puppet=pb.pup soundclass=pb.snd cog=actor_pd.cog mass=300 physflags=0x2006a06 maxvel=.5 weapon=+probedroidbolt health=90 maxhealth=90 maxthrust=.6 maxrotthrust=150 typeflags=0x2008120 fireoffset=(.0375/.065/0) explode=+probedroid_exp aiclass=pddefault.ai
# DESC:  
# BBOX: -.037458 -.014968 -.120277 .037328 .026217 .066384
iofficer_m        _humanactor        thingflags=0x20000400 model3d=io.3do size=.0667 movesize=.0667 puppet=io.pup soundclass=io_s1.snd cog=actor_io.cog maxvel=.3 weapon=+ebolt weapon2=+st_punch health=30 maxthrust=.9 maxrotthrust=80 typeflags=0x20001 fireoffset=(.05/.065/.014) aiclass=iodefault.ai
# DESC:  
# BBOX: -.150124 -.150615 -.150124 .149876 .149385 .149872
crate4_5          _walkstruct        model3d=crt4-5.3do size=.260306 movesize=.260306
# DESC:  
# BBOX: -.120072 -.104087 -.149746 .120072 .104087 .149746
xtank3a           _walkstruct        thingflags=0x400448 model3d=xtank3a.3do size=.191956 movesize=.191956 cog=xtank3.cog
# DESC:  
# BBOX: -.314105 -.627595 -.074421 .314105 .627595 .074421
trooptran         _zwalkstruct       model3d=ttrn.3do size=.718795 movesize=.718795
# DESC:  
# BBOX: -.383546 -.285641 -.334209 .372394 .719699 .318835
tieinterceptor    _zwalkstruct       model3d=tiei.3do size=.817667 movesize=.817667
# DESC:  
# BBOX: -.605016 -1.377949 -1.085803 .605016 1.377949 1.085803
atatactor         _zwalkstruct       model3d=aa.3do size=.7 movesize=.7
# DESC:  
# BBOX: -.1 -.0125 -.15 .1 .0125 .15
2x3door_1         _structure         model3d=d2x3_1.3do size=.18071 movesize=.18071 soundclass=sm_door.snd
# DESC:  
# BBOX: -.198 -.198002 -.05971 .198 .198002 .05971
4x4elev           _walkstruct        model3d=e4x4.3do size=.336311 movesize=.336311 soundclass=med_elev.snd
# DESC:  
# BBOX: -.010023 -.016964 -.033841 .010023 .016964 .033841
keyyellow         _powerup           thingflags=0x400 model3d=y-key.3do cog=pow_keyyellow.cog
# DESC:  
# BBOX: -.037893 -.014351 -.12043 .03746 .022321 .063011
atatpilot_m       _humanactor        thingflags=0x20000400 model3d=ap.3do size=.06595 movesize=.06595 puppet=ic.pup soundclass=st_s1.snd cog=actor_tp_pnch.cog maxvel=.3 weapon=+ebolt weapon2=+st_punch health=130 maxhealth=130 maxthrust=1.1 maxrotthrust=90 typeflags=0x20001 fireoffset=(.014/.059/.031) aiclass=tp_def.ai
# DESC:  
# BBOX: -.037893 -.014351 -.12043 .03746 .022321 .063011
tiepilot_m        _humanactor        thingflags=0x20000400 model3d=tp.3do size=.06595 movesize=.06595 puppet=ic.pup soundclass=st_s1.snd cog=actor_tp_pnch.cog maxvel=.3 weapon=+ebolt weapon2=+st_punch health=110 maxhealth=110 maxthrust=1.1 maxrotthrust=90 typeflags=0x20001 fireoffset=(.014/.059/.031) aiclass=tp_def.ai
# DESC:  
# BBOX: -.019886 -.022184 0 .019886 .022184 .042868
seqcharge         _cpowerup          thingflags=0x400 model3d=seqp.3do cog=pow_sequencer_m.cog
# DESC:  
# BBOX: -.008536 -.014067 -.022418 .008536 .014068 .022418
energycell        _powerup           thingflags=0x400 model3d=ecel.3do cog=pow_energy_m.cog
# DESC:  
# BBOX: -.037893 -.021815 -.12043 .03746 .022321 .066407
icommando         _humanactor        thingflags=0x20000400 model3d=ic.3do size=.06595 movesize=.06595 puppet=ic.pup soundclass=ic.snd cog=actor_ic.cog maxvel=.3 weapon=+elaser weapon2=+st_punch health=90 maxhealth=90 maxthrust=1.1 maxrotthrust=90 typeflags=0x20001 fireoffset=(.014/.059/.031) aiclass=icdefault.ai
# DESC:  
# BBOX: -.037893 -.02023 -.12043 .03746 .022321 .065552
icommandorpt_m    icommando          model3d=icrpt.3do soundclass=icrpt_s1.snd cog=actor_i2.cog weapon=+eball aiclass=icreapdefault.ai
# DESC:  
# BBOX: -.021293 -.02235 -.028598 .01838 .02235 .028374
fullshield        _powerup           thingflags=0x400 model3d=vest.3do cog=pow_vest.cog respawn=60
# DESC:  
# BBOX: -.026804 -.036274 -.074092 .026804 .036274 .074092
shrapnel_5        _debris            model3d=shrp_5.3do size=.084 movesize=.084 damage=0
# DESC:  
# BBOX: 0 0 0 0 0 0
+turret_exp       _explosion         thingflags=0x1 light=.2 timer=.8 sprite=detx.spr soundclass=exp_probe.snd typeflags=0x17 damage=10 blasttime=.699 force=100 maxlight=.8 range=.45 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_5
# DESC:  
# BBOX: -.058293 -.078937 -.033443 .058195 .084676 .035083
roundturret       none               orient=(0/0/0) type=actor collide=1 move=physics thingflags=0x400 model3d=tur2.3do size=.08 movesize=.08 puppet=tur2.pup soundclass=turret.snd cog=class_roundturret.cog physflags=0x2400000 weapon=+turretlaser health=100 maxhealth=100 typeflags=0x2080160 fireoffset=(0/-.028/.022) explode=+turret_exp aiclass=turdefault.ai
# DESC:  
# BBOX: -.01341 -.149802 -.01341 .01341 .015603 .01341
+turretlaser2     +turretlaser       vel=(0/4/0) damage=25 mindamage=10
# DESC:  
# BBOX: -.059999 -.051358 -.040791 .060835 .061748 .050725
boxturret         none               orient=(0/0/0) type=actor collide=1 move=physics thingflags=0x400 model3d=tur1.3do size=.075 movesize=.075 puppet=tur1.pup soundclass=turret.snd cog=class_boxturret.cog physflags=0x2400000 weapon=+turretlaser2 health=80 maxhealth=80 typeflags=0x2080160 fireoffset=(0/-.003/.036) explode=+turret_exp aiclass=turbdefault.ai
# DESC:  
# BBOX: -.01341 -.149802 -.01341 .01341 .015603 .01341
+turretlaser3     +turretlaser       vel=(0/4/0) damage=40 mindamage=20
# DESC:  
# BBOX: -.124322 -.166491 -.102246 .124322 .167917 .079419
boxturret4        boxturret          model3d=tur4.3do size=.2 movesize=.2 cog=class_boxturret4.cog weapon=+turretlaser3 health=150 maxhealth=150 fireoffset=(0/-.043/.036) aiclass=turbbdefault.ai
# DESC:  
# BBOX: -.009575 -.037873 -.008367 .009575 .037873 .008367
npscope           scope              typeflags=0x9
# DESC:  
# BBOX: -.2 -.400002 -.05971 .2 .400002 .05971
4x8elev           _walkstruct        model3d=e4x8.3do size=.451184 movesize=.451184 soundclass=lg_elev.snd
# DESC:  
# BBOX: -.007408 -.00644 -.031415 .007408 .00644 .031415
securitycambase   _structure         model3d=scambase.3do size=.082913 movesize=.082913
# DESC:  
# BBOX: 0 0 0 0 0 0
+remote_exp       +grenade_exp       sprite=debrisx.spr soundclass=exp_remote.snd damage=0 force=25
# DESC:  
# BBOX: -.031793 -.031444 -.039413 .031793 .031444 .039413
securitycamera    none               orient=(0/0/0) type=actor collide=1 move=physics thingflags=0x400 model3d=scam.3do size=.035 movesize=.035 soundclass=turret.snd cog=00_securitycam.cog physflags=0x2400000 health=50 maxhealth=50 typeflags=0x2180160 explode=+remote_exp aiclass=turdefault.ai
# DESC:  
# BBOX: 0 0 0 0 0 0
ghostcam          none               orient=(0/0/0) type=ghost move=path puppet=cr.pup
# DESC:  
# BBOX: 0 0 0 0 0 0
light0.25         _ghostdecor        thingflags=0x1 light=.25
# DESC:  
# BBOX: -.040024 -.034696 -.049915 .040024 .034696 .049915
xtank1a           _walkstruct        thingflags=0x400448 model3d=xtank1a.3do size=.063985 movesize=.063985 cog=xtank1.cog
# DESC:  
# BBOX: -.01636 -.014643 -.011603 .017727 .017747 .016258
srailcharges      _cpowerup          thingflags=0x400 model3d=scrg.3do cog=pow_srailcharges_m.cog
# DESC:  
# BBOX: -.037458 -.014968 -.120277 .037328 .026217 .066384
iofficer          _humanactor        thingflags=0x20000400 model3d=io.3do size=.0667 movesize=.0667 puppet=io.pup soundclass=io.snd cog=actor_io.cog maxvel=.3 weapon=+ebolt weapon2=+st_punch health=30 maxthrust=.9 maxrotthrust=80 typeflags=0x20001 fireoffset=(.05/.065/.014) aiclass=iodefault.ai
# DESC:  
# BBOX: -.037458 -.014968 -.120277 .037328 .026217 .066384
iofficer2_m       iofficer           soundclass=io2_s1.snd
# DESC:  
# BBOX: -.005771 -.015021 -.00466 .005771 .003935 .00466
+remotelaser      +repeaterball      damage=2 mindamage=1
# DESC:  
# BBOX: -.053638 -.040044 -.039175 .046239 .040044 .06128
torturedroid      _flyactor          thingflags=0x20000400 model3d=id.3do size=.05 movesize=.05 soundclass=td.snd cog=actor_re.cog mass=45 physflags=0x2006a06 maxvel=.6 weapon=+remotelaser health=30 maxthrust=.9 maxrotthrust=150 typeflags=0x2008120 fireoffset=(-.03/.04/0) explode=+remote_exp aiclass=td_def.ai
# DESC:  
# BBOX: 0 0 0 0 0 0
+mouse_exp        +remote_exp        force=10
# DESC:  
# BBOX: -.018576 -.029933 -.04006 .017936 .031186 -.009989
mousebot          _droidactor        thingflags=0x20000400 model3d=mb.3do size=.039 movesize=.039 soundclass=mousebot.snd cog=actor_mb.cog mass=20 height=.04 health=10 maxthrust=1 maxrotthrust=80 typeflags=0x20 explode=+mouse_exp aiclass=mouse.ai
# DESC:  
# BBOX: -.036535 -.022706 -.017295 .036535 .022706 .017295
goggles           _powerup           thingflags=0x400 model3d=gogl.3do cog=pow_goggles.cog respawn=60
# DESC:  
# BBOX: -.022125 -.04425 -.0001 .022125 .04425 .04415
ewebammo          _powerup           thingflags=0x400 model3d=epack.3do cog=pow_ewebammo_m.cog
# DESC:  
# BBOX: 0 0 0 0 0 0
eweb              _powerup           type=cog move=path thingflags=0x400 model3d=eweb.3do cog=pow_eweb_m.cog
# DESC:  
# BBOX: 0 0 0 0 0 0
+esequencer_exp   _explosion         thingflags=0x1 light=.2 timer=.8 sprite=seqx2.spr soundclass=exp_med.snd creatething=+firecloud2 typeflags=0x17 damage=75 blasttime=.699 force=300 maxlight=.8 range=.6 debris=shrapnel2_1 debris=shrapnel2_2 debris=shrapnel2_3 debris=shrapnel2_4
# DESC:  
# BBOX: -.022216 -.023897 -.005309 .022216 .023897 .005309
sequencermine     +grenade2          timer=0 model3d=seq0.3do size=.3 movesize=.01 soundclass=seq.snd surfdrag=5 airdrag=1 physflags=0x29c vel=(0/0/0) angvel=(0/0/0) buoyancy=.5 explode=+esequencer_exp typeflags=0x1380
# DESC:  
# BBOX: -.005081 -.030803 -.01143 .005081 .030803 .01143
+dgreedopistol    _droppowerup       thingflags=0x400 model3d=ggun.3do cog=pow_blastech_m.cog height=.01145
# DESC:  
# BBOX: 0 0 0 0 0 0
+xtank1_exp       _explosion         thingflags=0x1 light=.2 timer=.8 sprite=detx.spr soundclass=exp_probe.snd typeflags=0x17 damage=90 blasttime=.699 force=100 maxlight=.8 range=.45 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_5
# DESC:  
# BBOX: 0 0 0 0 0 0
+xtank3_exp       +xtank1_exp        sprite=tiex.spr soundclass=exp_tie.snd damage=200 force=200 range=.6 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_5
# DESC:  
# BBOX: 0 0 0 0 0 0
+tie_exp          _explosion         thingflags=0x1 light=.2 timer=.8 sprite=tiex.spr soundclass=exp_tie.snd typeflags=0x17 damage=20 blasttime=.699 force=300 maxlight=.8 range=1
# DESC:  
# BBOX: -.011462 -.396132 -.479822 .011462 .396132 .479822
tief_debris_1     _debris            model3d=tief_1.3do size=.549332 movesize=.549332
# DESC:  
# BBOX: -.123623 -.077635 -.077635 .123623 .077635 .077635
tief_debris_2     _debris            model3d=tief_2.3do size=.145978 movesize=.145978
# DESC:  
# BBOX: -.132555 -.152638 -.129684 .131696 .120306 .134567
tief_debris_3     _debris            model3d=tief_3.3do size=.163079 movesize=.163079
# DESC:  
# BBOX: 0 0 0 0 0 0
+tief_exp         +tie_exp           debris=tief_debris_1 debris=tief_debris_2 debris=tief_debris_3 debris=tief_debris_1
# DESC:  
# BBOX: -.140444 -.50267 -.326522 .140444 .50267 .326522
tiei_debris_1     _debris            model3d=tiei_1.3do size=.403432 movesize=.403432
# DESC:  
# BBOX: -.083496 -.285641 -.222454 .072344 .719698 .207079
tiei_debris_2     _debris            model3d=tiei_2.3do size=.403432 movesize=.403432
# DESC:  
# BBOX: -.261944 .069298 -.150714 .250793 .36476 .135339
tiei_debris_3     _debris            model3d=tiei_3.3do size=.403432 movesize=.403432
# DESC:  
# BBOX: -.090911 -.285641 -.119597 .07976 .719698 .104222
tiei_debris_4     _debris            model3d=tiei_4.3do size=.403432 movesize=.403432
# DESC:  
# BBOX: 0 0 0 0 0 0
+tiei_exp         +tie_exp           debris=tiei_debris_1 debris=tiei_debris_2 debris=tiei_debris_3 debris=tiei_debris_4
# DESC:  
# BBOX: -.40002 -.400093 -.024993 .40002 .400093 .024993
8x8elev           _walkstruct        model3d=e8x8.3do size=.616317 movesize=.616317 soundclass=lg_elev.snd
# DESC:  
# BBOX: -.029714 -.02826 -.025 .024039 .02826 .02818
landinglight      _structure         model3d=l_lite.3do size=.088832 movesize=.088832
# DESC:  
# BBOX: -.034 -.05906 -.093167 .034804 .05936 .052273
powerdroid        _droidactor        thingflags=0x20000400 model3d=pd.3do size=.08 movesize=.08 puppet=pd.pup soundclass=gonk.snd cog=actor_go.cog mass=180 health=50 maxhealth=50 maxthrust=.2 maxrotthrust=50 typeflags=0x180140 aiclass=gonk.ai
# DESC:  
# BBOX: -.100779 -.107544 .000686 .100779 .107544 .023173
ceilinglight      _structure         thingflags=0x9 light=.5 model3d=clight.3do size=.116487 movesize=.116487
# DESC:  
# BBOX: -.4 -.050015 -.398 .4 .050015 .4
8x8door           _structure         model3d=d8x8.3do size=.617892 movesize=.617892 soundclass=lg_door.snd
# DESC:  
# BBOX: -.02013 -.030328 -.022079 .018742 .092392 .056747
securitygun       none               orient=(0/0/0) type=actor collide=1 move=physics thingflags=0x400 model3d=sgun.3do size=.06 movesize=.06 puppet=sgun.pup soundclass=turret.snd cog=class_roundturret.cog physflags=0x2400000 weapon=+turretlaser health=100 maxhealth=100 typeflags=0x2080160 fireoffset=(0/0/.022) explode=+turret_exp aiclass=turdefault.ai
# DESC:  
# BBOX: -.02509 -.216005 -.050902 .02509 .216669 .117932
8x8elevstrut      _walkstruct        model3d=e8x8stru.3do size=.272753 movesize=.272753
# DESC:  
# BBOX: -.010023 -.016964 -.033841 .010023 .016964 .033841
keyred            _powerup           thingflags=0x400 model3d=r-key.3do cog=pow_keyred.cog
# DESC:  
# BBOX: -.014212 -.013093 -.011834 .014212 .013093 .011834
assassindebris_1  _debris            model3d=adpart0.3do size=.0072 movesize=.0072
# DESC:  
# BBOX: -.020943 -.017223 -.022127 .020943 .017223 .022127
assassindebris_2  _debris            model3d=adpart1.3do size=.0264 movesize=.0264
# DESC:  
# BBOX: -.016411 -.007309 -.014852 .016411 .007309 .014852
assassindebris_3  _debris            model3d=adpart2.3do size=.02 movesize=.02
# DESC:  
# BBOX: -.01233 -.01609 -.007689 .01233 .01609 .007689
assassindebris_4  _debris            model3d=adpart3.3do size=.0208 movesize=.0208
# DESC:  
# BBOX: 0 0 0 0 0 0
+assassindroid_exp _explosion         thingflags=0x1 light=.2 timer=.8 sprite=seqx.spr soundclass=exp_probe.snd typeflags=0x17 damage=20 blasttime=.699 force=120 maxlight=.8 range=.3 debris=assassindebris_1 debris=assassindebris_2 debris=assassindebris_3 debris=assassindebris_4
# DESC:  
# BBOX: -.030923 -.014545 -.099713 .030923 .02174 .060004
assassindroid     _droidactor        thingflags=0x20000400 model3d=ad.3do size=.058 movesize=.058 puppet=ad.pup soundclass=ad.snd cog=actor_ad.cog mass=160 weapon=+probedroidbolt health=100 maxhealth=100 maxrotthrust=80 typeflags=0x1a8161 explode=+assassindroid_exp aiclass=addefault.ai
# DESC:  
# BBOX: 0 0 0 0 0 0
+upenergyhi       none               orient=(0/0/0) type=particle move=physics timer=.6 vel=(0/0/2) angvel=(0/360/0) typeflags=0x3e material=embers.mat range=.4 elementsize=.0075 count=30
# DESC:  
# BBOX: -.01477 -.014666 -.01477 .01477 .014666 .01477
remote            _flyactor          thingflags=0x20000400 model3d=remo.3do size=.025 movesize=.025 soundclass=re.snd cog=actor_re.cog mass=15 physflags=0x2006a06 maxvel=.6 weapon=+remotelaser health=30 maxthrust=.9 maxrotthrust=150 typeflags=0x2008120 fireoffset=(.015/.02/0) explode=+remote_exp aiclass=redefault.ai
# DESC:  
# BBOX: 0 0 0 0 0 0
+gonk_exp         +grenade_exp       damage=20
# DESC:  
# BBOX: -.200716 -.195991 -.079681 .195304 .200102 .079681
4x4elev_2         _walkstruct        model3d=e4x4_2.3do size=.344409 movesize=.344409 soundclass=med_elev.snd
# DESC:  
# BBOX: -.498 -.998 -.1 .498 -.002 0
10x10door         _walkstruct        model3d=d10x10.3do size=1.169825 movesize=1.169825 soundclass=lg_elev.snd
# DESC:  
# BBOX: -.021293 -.02235 -.028598 .01838 .02235 .028374
supershield       _powerup           thingflags=0x400 model3d=vestl.3do cog=pow_supershield.cog respawn=60
# DESC:  
# BBOX: -.029786 -.019179 -.124701 .024091 .064673 .043406
stormtroop_m      _humanactor        thingflags=0x20000400 model3d=st.3do puppet=st.pup soundclass=st_s1.snd cog=actor_st_pnch.cog maxvel=.25 weapon=+elaser weapon2=+st_punch health=60 maxhealth=60 maxthrust=1 maxrotthrust=90 typeflags=0x20001 fireoffset=(.0048/.05/.012) aiclass=st_def.ai
# DESC:  
# BBOX: -.035317 -.025605 -.047024 .035366 .022343 .048893
r2                _droidactor        thingflags=0x20000400 model3d=r2.3do size=.058 movesize=.058 puppet=r2.pup soundclass=r2.snd cog=actor_r2.cog maxvel=.4 health=50 maxhealth=50 maxthrust=.4 maxrotthrust=50 typeflags=0x180140 aiclass=drdefault.ai
# DESC:  
# BBOX: -.037893 -.021815 -.12043 .03746 .022321 .066407
icommando_m       _humanactor        thingflags=0x20000400 model3d=ic.3do size=.06595 movesize=.06595 puppet=ic.pup soundclass=ic_s1.snd cog=actor_ic.cog maxvel=.3 weapon=+elaser weapon2=+st_punch health=90 maxhealth=90 maxthrust=1.1 maxrotthrust=90 typeflags=0x20001 fireoffset=(.014/.059/.031) aiclass=icdefault.ai
# DESC:  
# BBOX: 0 0 0 0 0 0
16_exp            +grenade_exp       damage=0
# DESC:  
# BBOX: -.035332 -.013185 -.120103 .035579 .016413 .064197
marawalkplayer    _actor             type=player thingflags=0x20000401 light=.2 model3d=mj.3do size=.065 movesize=.065 puppet=mj.pup soundclass=mj.snd cog=mara_m.cog surfdrag=3 airdrag=.5 staticdrag=.3 health=100 maxhealth=100 maxthrust=2 typeflags=0x1 error=.5 fov=.71 chance=1
# DESC:  
# BBOX: -.005081 -.030803 -.01143 .005081 .030803 .01143
greedopistol      _powerup           thingflags=0x400 model3d=ggun.3do cog=pow_blastech_m.cog
# DESC:  
# BBOX: -.012329 -.036225 -.012284 .012329 .036225 .012284
droideye          _structure         model3d=deye.3do size=.040592 movesize=.040592 puppet=cr.pup
# DESC:  
# BBOX: -.25 -.475 -.25 .25 .475 .25
s2l1weight_1      _walkstruct        thingflags=0x48 model3d=21wght01.3do size=.642136 movesize=.642136 soundclass=19counterweight.snd
# DESC:  
# BBOX: -.01264 -.999708 -.01129 .01264 .999708 .01129
s2l1rope03        _structure         thingflags=0xc model3d=21rope03.3do size=1.049851 movesize=1.049851
# DESC:  
# BBOX: -.7 -.025 .00207 .7 .025 .80207
shielddoor        _structure         model3d=bigdoor.3do size=1.114867 movesize=1.114867
# DESC:  
# BBOX: -.2 -.0125 -.15 .2 .0125 .15
rustydoor         _structure         model3d=d4x3_2.3do size=.300312 movesize=.300312 soundclass=med_door.snd
# DESC:  
# BBOX: -.2 -.015 -.175 .2 .015 .175
securitydoor      _structure         model3d=secdoor.3do size=.316177 movesize=.316177 soundclass=med_door.snd
# DESC:  
# BBOX: -.3 -.026218 -.00201 .3 .023782 .34799
securitydoor2     _structure         model3d=secdoor2.3do size=.5102 movesize=.5102 soundclass=med_door.snd
# DESC:  
# BBOX: -.224901 -.01243 -.12491 .224901 .01243 .12491
barredgate        _structure         model3d=s2l4gate.3do size=.307561 movesize=.307561
# DESC:  
# BBOX: -.1 -.005844 -.1 .1 .005844 .1
woodwindowcover   _structure         model3d=shutr.3do size=.191542 movesize=.191542 soundclass=03wood_door.snd
# DESC:  
# BBOX: -.064855 -.030184 -.137291 .064857 .181468 .079798
gammorean         _humanactor        thingflags=0x20000400 model3d=gm.3do size=.07878 movesize=.07878 puppet=gm.pup soundclass=gm.snd cog=actor_gg.cog mass=200 weapon=+gamaxe weapon2=+gamaxe health=120 maxhealth=120 maxrotthrust=80 typeflags=0x20001 aiclass=ggdefault.ai
# DESC:  
# BBOX: 0 0 0 0 0 0
+ry_punch         +punch             force=25
# DESC:  
# BBOX: -.046863 -.025046 -.131246 .045997 .027318 .073257
reeyeesgun        _humanactor        thingflags=0x20000400 model3d=rystr.3do size=.0729 movesize=.0729 puppet=rystr.pup soundclass=rystr.snd cog=actor_rb_pnch.cog weapon=+elaser weapon2=+ry_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(.0147/.08/.034) aiclass=rydefault.ai
# DESC:  
# BBOX: 0 0 0 0 0 0
+egrenade_exp     +grenade_exp       damage=50
# DESC:  
# BBOX: -.012516 -.012516 -.012524 .012516 .012516 .014655
+actor_grenade    +grenade1          surfdrag=0 airdrag=0 vel=(0/2/0) explode=+egrenade_exp fleshhit=+egrenade_exp
# DESC:  
# BBOX: -.046863 -.025046 -.131246 .045997 .027318 .073257
reeyees           _humanactor        thingflags=0x20000400 model3d=ry.3do size=.0729 movesize=.0729 puppet=ry.pup soundclass=ry.snd cog=actor_ry_pnch.cog weapon=+actor_grenade weapon2=+ry_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(.0465/.07/.0327) aiclass=rytherdefault.ai
# DESC:  
# BBOX: -.012516 -.012516 -.012524 .012516 .012516 .014655
+actor_grenade2   +grenade2          vel=(0/2/0) explode=+egrenade_exp fleshhit=+egrenade_exp
# DESC:  
# BBOX: -.046863 -.025046 -.131246 .045997 .027318 .073257
reeyeess          reeyees            weapon=+actor_grenade2
# DESC:  
# BBOX: -.020392 -.135722 -.016856 .021073 .013215 .015722
+ebow             +crossbowbolt      vel=(0/4/0) damage=20 mindamage=10
# DESC:  
# BBOX: -.037806 -.014831 -.120834 .054054 .062457 .065442
tuskan            _humanactor        thingflags=0x20000400 model3d=tk.3do size=.0649 movesize=.0649 puppet=tk.pup soundclass=tk.snd cog=actor_tk_pnch.cog mass=120 maxvel=.2 weapon=+ebow weapon2=+st_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(.006/.061/.027) aiclass=tuproddefault.ai
# DESC:  
# BBOX: -.15 -.025 -.025 .15 .025 .025
windowledge       _walkstruct        thingflags=0x48 model3d=wledge.3do size=.20411 movesize=.20411
# DESC:  
# BBOX: -.021089 -.023005 -.035347 .016307 .023005 .035347
clothespack       _powerup           move=path thingflags=0x400 model3d=clths.3do cog=pow_tusken_clothes.cog
# DESC:  
# BBOX: -.063294 -.054836 -.040528 .063294 .054836 .040528
00table           _walkstruct        thingflags=0x48 model3d=00tabl.3do size=.07042 movesize=.07042
# DESC:  
# BBOX: -.046863 -.025046 -.131246 .045997 .027318 .073257
preeyeesgun       _humanactor        thingflags=0x20000400 model3d=rystr.3do size=.0729 movesize=.0729 puppet=rystr.pup soundclass=rystr.snd cog=actor_rb_ped3.cog weapon=+elaser weapon2=+ry_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(.0147/.08/.034) aiclass=rydefault.ai
# DESC:  
# BBOX: 0 0 0 0 0 0
_civilian         _humanactor        surfdrag=2 mass=100 maxvel=.3 maxthrust=2 maxrotthrust=90 typeflags=0x180000
# DESC:  
# BBOX: -.035186 -.01745 -.100835 .035171 .022972 .063971
ug                _civilian          thingflags=0x20000400 model3d=ugalt.3do size=.064 movesize=.064 puppet=ug.pup soundclass=ug.snd cog=actor_ug.cog mass=60 maxvel=.4 health=30 maxthrust=.5 maxrotthrust=60 aiclass=peddefault.ai
# DESC:  
# BBOX: -.035186 -.01745 -.100835 .035171 .022972 .063971
ugsit             ug                 puppet=ugsit.pup mass=200 aiclass=pedhome.ai
# DESC:  
# BBOX: -.026466 -.012833 -.099682 .026261 .023106 .06583
pgreedo           _humanactor        thingflags=0x20000400 model3d=gr.3do size=.0615 movesize=.0615 puppet=gr.pup soundclass=gr.snd cog=actor_gr_ped.cog mass=70 maxvel=.3 weapon=+ebolt maxrotthrust=80 typeflags=0x2080001 fireoffset=(.016/.055/.004) aiclass=grdefault.ai
# DESC:  
# BBOX: -.046863 -.025046 -.131246 .045997 .027318 .073257
preeyeesfist      _humanactor        thingflags=0x20000400 model3d=ry.3do size=.0729 movesize=.0729 puppet=ryfist.pup soundclass=ryfist.snd cog=actor_rf_ped2.cog weapon=+ry_punch health=50 maxhealth=50 maxthrust=1 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 aiclass=rypundefault.ai
# DESC:  
# BBOX: -.046863 -.025046 -.131246 .045997 .027318 .073257
reeyeesfist       _humanactor        thingflags=0x20000400 model3d=ry.3do size=.0729 movesize=.0729 puppet=ryfist.pup soundclass=ryfist.snd cog=actor_rf.cog weapon=+ry_punch health=50 maxhealth=50 maxthrust=1 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 aiclass=rypundefault.ai
# DESC:  
# BBOX: -.046863 -.025046 -.131246 .045997 .027318 .073257
preeyees          _humanactor        thingflags=0x20000400 model3d=ry.3do size=.0729 movesize=.0729 puppet=ry.pup soundclass=ry.snd cog=actor_ry_ped1.cog weapon=+actor_grenade weapon2=+ry_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(.0465/.07/.0327) aiclass=rytherdefault.ai
# DESC:  
# BBOX: -.026466 -.012833 -.099682 .026261 .023106 .06583
greedo            _humanactor        thingflags=0x20000400 model3d=gr.3do size=.0615 movesize=.0615 puppet=gr.pup soundclass=gr.snd cog=actor_gr.cog mass=70 maxvel=.3 weapon=+ebolt maxrotthrust=80 typeflags=0x2080001 fireoffset=(.016/.055/.004) aiclass=grdefault.ai
# DESC:  
# BBOX: -.046863 -.025046 -.131227 .045997 .01931 .074096
pithorian         _humanactor        thingflags=0x20000400 model3d=it.3do size=.0729 movesize=.0729 puppet=it.pup soundclass=itstr.snd cog=actor_it_ped.cog weapon=+elaser weapon2=+ry_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(.0147/.08/.034) aiclass=it_def.ai
# DESC:  
# BBOX: 0 0 0 0 0 0
light0.7          _ghostdecor        thingflags=0x1 light=.7
# DESC:  
# BBOX: -.027321 -.032261 .000219 .027321 .032261 .126942
minichair3        _walkstruct        thingflags=0x48 model3d=chair3m.3do size=.182873 movesize=.182873
# DESC:  
# BBOX: -.061611 -.054554 -.025219 .061611 .054554 .025219
bottles01         _structure         collide=1 model3d=bottles1.3do size=.05 movesize=.05
# DESC:  
# BBOX: -.031058 -.027406 -.023339 .031058 .027406 .023339
bottles05         _structure         collide=1 model3d=bottles5.3do size=.05 movesize=.05
# DESC:  
# BBOX: -.016342 -.015548 -.018445 .016342 .015548 .018445
bottle06          _structure         model3d=bottle6.3do size=.076147 movesize=.076147
# DESC:  
# BBOX: -.8544 -.8544 -.200784 .8544 .8544 .200784
bigdome           _zwalkstruct       model3d=dome.3do size=.965443 movesize=.965443
# DESC:  
# BBOX: -.05 -.1 -.025 .05 .1 .025
s2l1step          _walkstruct        thingflags=0x48 model3d=s2l1step.3do size=.164564 movesize=.164564
# DESC:  
# BBOX: -.290689 -.400058 -.45 .290689 .400058 .45
pump_b            _walkstruct        thingflags=0x48 model3d=pump_b.3do size=.718615 movesize=.718615
# DESC:  
# BBOX: -.396898 -.09903 -.348531 .112036 .098922 .112506
pump_c            _walkstruct        model3d=pump_c.3do size=.464086 movesize=.464086
# DESC:  
# BBOX: -.38877 -.071487 -.06191 .38877 .071487 .06191
pump_a            _walkstruct        model3d=pump_a.3do size=.445288 movesize=.445288
# DESC:  
# BBOX: -.160158 -.036477 -.140057 .144928 .036013 .196945
dualpiston        _decor             model3d=pist.3do size=.22 movesize=.22 puppet=cr.pup
# DESC:  
# BBOX: -.05 -.009869 -.05 .05 .009869 .05
droideyedoor      _structure         model3d=deyedoor.3do size=.121396 movesize=.121396
# DESC:  
# BBOX: -.024356 -.028089 -.032167 .024356 .028089 .032167
explosivedevice   _powerup           thingflags=0x408 model3d=ec.3do cog=pow_heavy_explosive.cog
# DESC:  
# BBOX: -.020759 -.01902 -.030703 .020759 .01902 .030703
manaboost         _powerup           thingflags=0x400 model3d=dark.3do cog=pow_mana.cog
# DESC:  
# BBOX: -.046863 -.025046 -.131227 .045997 .01931 .074096
ithorian          _humanactor        thingflags=0x20000400 model3d=it.3do size=.0729 movesize=.0729 puppet=it.pup soundclass=itstr.snd cog=actor_it_pnch.cog weapon=+elaser weapon2=+ry_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(.0147/.08/.034) aiclass=it_def.ai
# DESC:  
# BBOX: -.020824 -.043225 .000892 .011798 .043225 .024826
flash_bombs       _cpowerup          thingflags=0x400 model3d=flsp.3do cog=pow_flashbombs_m.cog
# DESC:  
# BBOX: -.027845 -.025398 -.02252 .027845 .025398 .024913
+dhealthpack      _droppowerup       thingflags=0x408 model3d=hepk.3do cog=pow_health.cog height=.023701
# DESC:  
# BBOX: -.024586 -.02755 -.015015 .028804 .052615 .015553
+rylimb           _limb              model3d=rylimb.3do size=.05 movesize=.01
# DESC:  
# BBOX: -.024586 -.02755 -.015015 .028804 .052615 .015553
+rhlimb           _limb              model3d=rhlimb.3do size=.05 movesize=.01
# DESC:  
# BBOX: -.020824 -.043225 .000892 .011798 .043225 .024826
+ddetonator       _droppowerup       thingflags=0x400 model3d=detp.3do cog=pow_thermal_m.cog
# DESC:  
# BBOX: -.017868 -.016799 -.017934 .017868 .016799 .017934
+dsingle          _droppowerup       thingflags=0x400 model3d=detp_1.3do cog=pow_single_thermal_m.cog height=.021909
# DESC:  
# BBOX: -.012516 -.012516 -.012524 .012516 .012516 .014655
+spec_grenade     +grenade2          vel=(0/0/0)
# DESC:  
# BBOX: -.034888 -.055377 .000124 .034343 .059997 .031925
+dcrossbow        _droppowerup       thingflags=0x400 model3d=bowp.3do cog=pow_dcrossbow_m.cog height=.003435
# DESC:  
# BBOX: -.042717 -.015112 -.011798 .042717 .015112 .011798
+tklimb           _limb              model3d=tklimb.3do size=.05 movesize=.01
# DESC:  
# BBOX: -.042717 -.015112 -.011798 .042717 .015112 .011798
+thlimb           _limb              model3d=thlimb.3do size=.05 movesize=.01
# DESC:  
# BBOX: 0 0 0 0 0 0
+rycw_punch       +ry_punch          damage=5
# DESC:  
# BBOX: -.046863 -.025046 -.131246 .045997 .027318 .073257
reeyeescw         reeyeesfist        weapon=+rycw_punch typeflags=0x1020001
# DESC:  
# BBOX: 0 0 0 0 0 0
_shard            none               orient=(0/0/0) type=cog move=physics mass=.05 physflags=0x221
# DESC:  
# BBOX: -.03133 -.000001 -.038594 .031989 .000001 .027215
shard00           _shard             timer=1 model3d=shard0.3do size=.042 movesize=.042
# DESC:  
# BBOX: -.125 -.05 -.3 .125 .05 .3
strut06           _structure         model3d=strut06.3do size=.328824 movesize=.328824
# DESC:  
# BBOX: -.021527 -.011397 -.102055 .021527 .014719 .057565
woman2            _civilian          thingflags=0x20000400 model3d=w2.3do size=.056 movesize=.056 puppet=w2.pup soundclass=wm1.snd cog=actor_pf.cog maxvel=.2 health=50 maxhealth=50 maxthrust=.5 maxrotthrust=60 aiclass=peddefault.ai
# DESC:  
# BBOX: -.037806 -.014831 -.120834 .037548 .016412 .065442
weequay           _humanactor        thingflags=0x20000400 model3d=wq.3do size=.0649 movesize=.0649 puppet=tkstr.pup soundclass=wqstr.snd cog=actor_wq_pnch.cog mass=120 maxvel=.2 weapon=+elaser weapon2=+st_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(.0416/.0682/.0225) aiclass=wq_def.ai
# DESC:  
# BBOX: -.033249 -.025869 -.100835 .033234 .06466 .062592
u2                ug                 model3d=u2alt.3do puppet=u2.pup aiclass=pedhome.ai
# DESC:  
# BBOX: -.076042 -.071135 -.052976 .062968 .083218 .054564
tuskensoused      _ghostdecor        move=none model3d=thout.3do size=.22749 movesize=.22749 puppet=cr.pup
# DESC:  
# BBOX: -.187173 -.171934 -.041875 .196141 .200354 .037814
fan1              none               orient=(0/0/0) type=cog collide=1 move=physics model3d=fan1.3do size=.005 movesize=.005 physflags=0x200 maxrotvel=90 angvel=(0/90/0)
# DESC:  
# BBOX: -.037244 -.032884 -.063574 .037244 .032884 .063574
chair04           _walkstruct        thingflags=0x48 model3d=chr4.3do size=.124181 movesize=.124181
# DESC:  
# BBOX: -.046863 -.025046 -.131246 .045997 .027318 .073257
reeyeesbhfist     reeyeesfist        model3d=rh.3do health=60 maxhealth=60 maxrotthrust=85
# DESC:  
# BBOX: -.035537 -.014648 -.128737 .035536 .025457 .065502
man1              _civilian          thingflags=0x20000400 model3d=mn.3do size=.06 movesize=.06 puppet=mn.pup soundclass=mn1.snd cog=actor_pm.cog maxvel=.2 health=50 maxhealth=50 maxthrust=.5 maxrotthrust=60 aiclass=peddefault.ai
# DESC:  
# BBOX: -.030754 -.015261 -.124016 .030821 .02207 .067788
seethreepio       _droidactor        thingflags=0x20000400 model3d=c3.3do size=.0678 movesize=.0678 puppet=c3.pup soundclass=c3po.snd cog=actor_c3po.cog mass=120 maxvel=.4 health=50 maxhealth=50 maxthrust=.3 maxrotthrust=50 typeflags=0x180140 aiclass=drdefault.ai
# DESC:  
# BBOX: -.046863 -.025046 -.131246 .045997 .027318 .073257
reeyeesbh         reeyees            model3d=rh.3do health=60 maxhealth=60 maxrotthrust=80
# DESC:  
# BBOX: -.046863 -.025046 -.131246 .045997 .027318 .073257
reeyeesbhs        reeyeesbh          weapon=+actor_grenade2
# DESC:  
# BBOX: -.8 -.016042 -.325 .8 .016042 .325
garagedoor        _structure         model3d=grgd.3do size=.913645 movesize=.913645 soundclass=lg_door.snd
# DESC:  
# BBOX: -.040769 -.082095 -.087667 .040769 .082487 .086583
powercoupling     _walkstruct        model3d=pcoup.3do size=.170373 movesize=.170373
# DESC:  
# BBOX: -.125 -.05 -.15 .125 .05 .15
strut03           _structure         model3d=strut03.3do size=.201556 movesize=.201556
# DESC:  
# BBOX: -.046863 -.025046 -.131246 .045997 .027318 .073257
reeyeesbhgun      reeyeesgun         model3d=rhstr.3do health=60 maxhealth=60 maxrotthrust=80 aiclass=rybdefault.ai
# DESC:  
# BBOX: -.021089 -.023005 -.035347 .016307 .023005 .035347
smbp              _powerup           thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk.cog
# DESC:  
# BBOX: -.025123 -.01744 -.127094 .025123 .015562 .056718
woman1            _civilian          thingflags=0x20000400 model3d=wm.3do size=.056 movesize=.056 puppet=wm.pup soundclass=wm1.snd cog=actor_pf.cog maxvel=.2 health=50 maxhealth=50 maxthrust=.5 maxrotthrust=60 aiclass=peddefault.ai
# DESC:  
# BBOX: -.025123 -.01744 -.127094 .025123 .015562 .056718
nwoman1           woman1             cog=actor_fn.cog aiclass=pednarsh.ai
# DESC:  
# BBOX: -.035537 -.014648 -.128737 .035536 .025457 .065502
nman1             man1               cog=actor_mn.cog aiclass=pednarsh.ai
# DESC:  
# BBOX: -.032373 -.017479 .000097 .031819 .017479 .079704
bluewrenchkey     _powerup           thingflags=0x400 model3d=keyblu.3do cog=pow_wrenchblue.cog
# DESC:  
# BBOX: -.72683 -.540973 -.382056 .727153 .325372 .815553
jade'sshrike_park _zwalkstruct       model3d=jphnxprk.3do size=1 movesize=1 puppet=cr.pup
# DESC:  
# BBOX: -.369375 -.418438 -.086562 .369375 .418437 .086562
r41               _zwalkstruct       model3d=r41_1.3do size=.514784 movesize=.514784
# DESC:  
# BBOX: -.036499 -.015206 -.119568 .03663 .02734 .066416
abronmar          _humanactor        thingflags=0x20000400 model3d=ab.3do size=.0649 movesize=.0649 puppet=ab.pup soundclass=tkstr.snd cog=actor_tb.cog mass=120 maxvel=.2 weapon=+elaser health=100 maxhealth=100 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x1 fireoffset=(.0416/.0682/.0225) aiclass=tudefault.ai
# DESC:  
# BBOX: -.61214 -.74676 -.13843 .61214 .74676 .13843
z95               _zwalkstruct       model3d=z95_1.3do size=.956309 movesize=.956309
# DESC:  
# BBOX: -.021089 -.023005 -.035347 .016307 .023005 .035347
tim's_bkpk10      _powerup           thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk_10m.cog
# DESC:  
# BBOX: -.8 -.016042 -.325 .8 .016042 .325
garagedoor1       _structure         model3d=grgd.3do size=.913645 movesize=.913645 soundclass=lg_door.snd
# DESC:  
# BBOX: -.021527 -.011397 -.102055 .021527 .014719 .057565
nwoman2           woman2             cog=actor_fn.cog aiclass=pednarsh.ai
# DESC:  
# BBOX: -.03619 -.019179 -.124701 .024091 .063953 .043406
stormtroop3       _humanactor        thingflags=0x20000400 model3d=s3.3do puppet=s3.pup soundclass=st3.snd cog=actor_i2_pnch.cog maxvel=.25 weapon=+eball weapon2=+st_punch health=70 maxhealth=70 maxthrust=.9 maxrotthrust=90 typeflags=0x20001 fireoffset=(.0098/.057/.01) aiclass=streapdefault.ai
# DESC:  
# BBOX: -.037806 -.014831 -.120834 .037548 .016412 .065442
tuskangun         _humanactor        thingflags=0x20000400 model3d=tkstr.3do size=.0649 movesize=.0649 puppet=tkstr.pup soundclass=tkstr.snd cog=actor_tb_pnch.cog mass=120 maxvel=.2 weapon=+elaser weapon2=+st_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(-.0416/.0682/.0225) aiclass=tudefault.ai
# DESC:  
# BBOX: -.03954 -.01907 -.133437 .039738 .026146 .061238
man2              _civilian          thingflags=0x20000400 model3d=m2.3do size=.06 movesize=.06 puppet=m2.pup soundclass=mn1.snd cog=actor_pm.cog maxvel=.2 health=50 maxhealth=50 maxthrust=.5 maxrotthrust=60 aiclass=peddefault.ai
# DESC:  
# BBOX: -.212425 -.229246 -.123998 .212425 .229246 .123998
duiskiff          _zwalkstruct       model3d=dui.3do size=.374132 movesize=.374132
# DESC:  
# BBOX: -.122086 -.117592 -.00067 .122086 .117592 .295542
tarp              _walkstruct        thingflags=0x800048 model3d=tarp.3do size=.390702 movesize=.390702
# DESC:  
# BBOX: -.122086 -.117592 -.148106 .122086 .117592 .148106
tarp2             _walkstruct        thingflags=0x800048 model3d=tarp2.3do size=.247606 movesize=.247606
# DESC:  
# BBOX: -.063294 -.054836 -.040528 .063294 .054836 .040528
table             _decor             model3d=01tabl.3do size=.065 movesize=.065
# DESC:  
# BBOX: -.165685 -.006493 -.2 .165685 .006493 .2
tridoor           _walkstruct        thingflags=0x48 model3d=trid.3do size=.309796 movesize=.309796 soundclass=med_door.snd
# DESC:  
# BBOX: 0 0 0 0 0 0
+econc_exp        +conc_exp          damage=60
# DESC:  
# BBOX: -.01341 -.149802 -.01341 .01341 .015603 .01341
+econcbullet      +concbullet        explode=+econc_exp fleshhit=+econc_exp damage=10
# DESC:  
# BBOX: 0 0 0 0 0 0
+bosskswipe       +gamaxe            damage=20 range=.2
# DESC:  
# BBOX: -.040341 -.017317 -.123441 .040266 .112852 .068602
bossk             _humanactor        thingflags=0x20000400 model3d=bs.3do size=.0681 movesize=.0681 puppet=bs.pup soundclass=bs.snd cog=actor_bs_pnch.cog mass=120 maxvel=.4 weapon=+econcbullet weapon2=+bosskswipe health=120 maxhealth=120 maxthrust=.9 maxrotthrust=80 typeflags=0x20a0001 fireoffset=(-.039/.07/.027) aiclass=bodefault.ai
# DESC:  
# BBOX: -.125 -.200163 -.049948 .125 .200163 .072163
bed               _walkstruct        thingflags=0x800048 model3d=bed.3do size=.291216 movesize=.291216
# DESC:  
# BBOX: -.040341 -.017317 -.123441 .041996 .078346 .068602
bosskrld          bossk              model3d=bsrld.3do soundclass=bsrld.snd cog=actor_bsrd_pnch.cog weapon=+eraildet fireoffset=(-.035/.07/.027) aiclass=bldefault.ai
# DESC:  
# BBOX: -.025123 -.01744 -.127094 .025123 .015562 .056718
mwoman1           woman1             cog=none
# DESC:  
# BBOX: -1.362535 -1.904275 -.304891 1.362534 1.904275 .304891
falcon            _zwalkstruct       model3d=falcon1.3do size=2.13595 movesize=2.13595
# DESC:  
# BBOX: -.05 -.05 -.049999 .05 .05 .049999
crate4_2          _walkstruct        model3d=crt4-2.3do size=.086602 movesize=.086602
# DESC:  
# BBOX: -.8 -.025 -.25 .8 .025 .25
garagedoor2       _walkstruct        model3d=gdoor-2.3do size=.922378 movesize=.922378
# DESC:  
# BBOX: -.213014 -.198738 -.152333 .213014 .198738 .152333
duiskiff2         _zwalkstruct       model3d=dui2.3do size=.325555 movesize=.325555
# DESC:  
# BBOX: -.03954 -.01907 -.133437 .039738 .026146 .061238
nman2             man2               cog=actor_mn.cog aiclass=pednarsh.ai
# DESC:  
# BBOX: -.030347 -.072704 -.024787 .009211 .072704 .024341
+dconcrifle       _droppowerup       thingflags=0x400 model3d=conp.3do cog=pow_dconcrifle_m.cog height=.024753
# DESC:  
# BBOX: -.043832 -.019016 -.007017 .043832 .019016 .007017
+bslimb           _limb              model3d=bslimb.3do size=.05 movesize=.01
# DESC:  
# BBOX: -.00929 -.00375 -.04177 .00929 .00375 .04177
strongholdkey     _powerup           thingflags=0x408 model3d=dkey.3do cog=pow_prisonkey.cog
# DESC:  
# BBOX: 0 0 0 0 0 0
+ra_claw1         _weapon            size=.001 movesize=.001 mass=100 explode=+axe_exp fleshhit=+axe_exp damage=80 damageclass=0x80 typeflags=0x200d range=.5 force=120
# DESC:  
# BBOX: 0 0 0 0 0 0
+ra_bite          _weapon            size=.001 movesize=.001 mass=100 explode=+axe_exp fleshhit=+axe_exp damage=40 damageclass=0x80 typeflags=0x200d range=.4 force=100
# DESC:  
# BBOX: -.195539 -.184358 -.131541 .206386 .368168 .281414
rancor            _humanactor        collide=2 thingflags=0x4 model3d=ra.3do size=.13 movesize=.13 treesize=1 puppet=rancor.pup soundclass=ra.snd cog=actor_ra.cog mass=3000 maxvel=.6 weapon=+ra_claw1 weapon2=+ra_bite health=500 maxhealth=500 maxthrust=1 maxrotthrust=25 typeflags=0x20101 aiclass=ra_def.ai
# DESC:  
# BBOX: -.2 -.0125 -.15 .2 .0125 .15
prisonservicedoor _structure         model3d=24pdoor.3do size=.300312 movesize=.300312 soundclass=med_door.snd
# DESC:  
# BBOX: -.15 -.011875 -.15 .15 .011875 .15
droiddoor         _structure         model3d=24ddoor.3do size=.262464 movesize=.262464 soundclass=sm_door.snd
# DESC:  
# BBOX: -.038064 -.024511 -.05829 .038064 .024511 .05829
console2          _walkstruct        model3d=con2.3do size=.073806 movesize=.073806
# DESC:  
# BBOX: -.15 -1 -.012306 .15 1 .012306
3x20catwalk       _walkstruct        model3d=c3x20.3do size=1.061262 movesize=1.061262
# DESC:  
# BBOX: -.45 -.0375 -.15 .45 .0375 .15
door9x3           _walkstruct        model3d=d9x3.3do size=.525822 movesize=.525822 soundclass=09accessdoor.snd
# DESC:  
# BBOX: -.060504 -.0375 -.025 .060504 .0375 .025
radiolisdev       _powerup           thingflags=0x400 model3d=radiold.3do cog=pow_radio.cog
# DESC:  
# BBOX: -.2 -.198942 -.2 .2 .201059 .2
crate4_6          _walkstruct        model3d=crt4-6.3do size=.347022 movesize=.347022
# DESC:  
# BBOX: -.029373 -.010613 -.030526 .029373 .010613 .030526
prybarkey         _powerup           thingflags=0x400 model3d=pb_5.3do cog=pow_prybar.cog
# DESC:  
# BBOX: -.046863 -.025046 -.131246 .045997 .027318 .073257
reeyeespris       reeyeesfist        health=10
# DESC:  
# BBOX: -.018462 -.030983 -.033409 .018462 .030983 .033409
droidarmkey       _powerup           thingflags=0x400 model3d=pbkey.3do cog=pow_droidarm.cog
# DESC:  
# BBOX: -.036362 -.017961 -.120075 .036864 .02734 .067086
maraactor         _humanactor        model3d=mj-0.3do puppet=mj_cuts.pup soundclass=kyfemale.snd maxvel=.4 health=20000 maxhealth=20000 maxthrust=1 maxrotthrust=60 aiclass=jan.ai
# DESC:  
# BBOX: -.016813 -.025219 -.025219 .016813 .025219 .025219
bottle02          _structure         model3d=bottle2.3do size=.089429 movesize=.089429
# DESC:  
# BBOX: -.016298 -.015506 -.024816 .016298 .015506 .024816
bottle04          _structure         model3d=bottle4.3do size=.080942 movesize=.080942
# DESC:  
# BBOX: -.021836 -.04524 -.000124 .021836 .037848 .034517
bones             _decor             collide=0 model3d=bones.3do size=.05954 movesize=.05954
# DESC:  
# BBOX: -.021878 -.019871 -.010012 .021878 .019871 .010012
droidpart1        _structure         model3d=r5part.3do size=.074778 movesize=.074778
# DESC:  
# BBOX: -.030034 -.034716 -.007363 .030034 .034716 .007363
droidpart2        _structure         model3d=c3limb.3do size=.095514 movesize=.095514
# DESC:  
# BBOX: -.018064 -.014288 -.007564 .018064 .014288 .007564
droidpart3        _structure         model3d=pdpart.3do size=.073882 movesize=.073882
# DESC:  
# BBOX: -.400093 -.798777 .059504 .400093 .801303 .10949
8x16elev          _walkstruct        model3d=e8x16.3do size=.952302 movesize=.952302 soundclass=lg_elev.snd
# DESC:  
# BBOX: -.496075 -.67564 -.137442 .495972 .67564 .1016
xwing_land        _zwalkstruct       model3d=xwnglnd.3do size=.813117 movesize=.813117
# DESC:  
# BBOX: -.597832 -.597465 -.299701 .597179 .59758 .299701
00crate           _walkstruct        model3d=00crte.3do size=.830224 movesize=.830224
# DESC:  
# BBOX: -.21867 -.194901 -.204409 .21867 .194901 .204409
puzzlecrate       _walkstruct        model3d=crtz.3do size=.392911 movesize=.392911
# DESC:  
# BBOX: -.222831 -.019392 -.250005 .222831 .019392 .250005
puzzlelid2        _walkstruct        model3d=crtzlid2.3do size=.385459 movesize=.385459
# DESC:  
# BBOX: -1.2 -1.8 -.3 1.2 1.8 .3
tetriscrate       _walkstruct        model3d=00tetris.3do size=2.234033 movesize=2.234033
# DESC:  
# BBOX: -3.848692 -19.22806 -7.892384 3.848692 19.22806 7.892383
nebblo            _zwalkstruct       model3d=neb_lo1.3do size=21.991705 movesize=21.991705
# DESC:  
# BBOX: 0 0 0 0 0 0
_ghoststructure   _structure         collide=0
# DESC:  
# BBOX: -.356182 -1.481516 -.301418 .356182 1.481516 .301418
dockingtube0      _ghoststructure    model3d=dtube0.3do size=1.573731 movesize=1.573731
# DESC:  
# BBOX: -.471765 -1.495065 -.399229 .471765 1.495065 .399229
dockingtube1      _ghoststructure    model3d=dtube1.3do size=1.617731 movesize=1.617731
# DESC:  
# BBOX: -.95 -.04 -1.005 .95 .04 1.005
12_door3          _structure         model3d=12dr03.3do size=1.433519 movesize=1.433519 soundclass=lg_door.snd
# DESC:  
# BBOX: -.051037 -.040052 -.175591 .051037 .040052 .175591
s4l1coolingrod    _structure         model3d=coolrod.3do size=.232858 movesize=.232858
# DESC:  
# BBOX: 0 0 0 0 0 0
light0.1          _ghostdecor        thingflags=0x1 light=.1
# DESC:  
# BBOX: -.065678 -.075809 -.044765 .065678 .075809 .044765
gametable         _walkstruct        model3d=game.3do size=.138039 movesize=.138039
# DESC:  
# BBOX: -.054707 -.058748 -.085259 .054707 .058748 .085259
floatingdeadsoldier _decor             move=physics model3d=rc-float.3do size=.09 movesize=.09 surfdrag=.7 airdrag=1 mass=170 physflags=0x422c angvel=(22.5/11.25/22.5)
# DESC:  
# BBOX: -.026466 -.012833 -.099682 .026261 .023106 .061024
pirateraider4     _humanactor        thingflags=0x20000400 model3d=p4.3do size=.0615 movesize=.0615 puppet=p2.pup soundclass=pr.snd cog=actor_pr.cog mass=70 maxvel=.3 weapon=+elaser maxrotthrust=80 typeflags=0x2080001 fireoffset=(.016/.065/.004) aiclass=pr_def.ai
# DESC:  
# BBOX: -.037806 -.014831 -.120834 .041996 .084407 .065442
pirateraider3     _humanactor        thingflags=0x20000400 model3d=p3.3do size=.0649 movesize=.0649 puppet=tk.pup soundclass=prrld.snd cog=actor_pr3_pnch.cog mass=120 maxvel=.2 weapon=+eraildet weapon2=+st_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(.0416/.0702/.0225) aiclass=pr3_def.ai
# DESC:  
# BBOX: -.068709 -.099565 -.180299 .068019 .082956 -.04085
deadrebelofficer2 _ghostdecor        model3d=ro-dead2.3do size=.174602 movesize=.174602
# DESC:  
# BBOX: -.037806 -.014831 -.120834 .041996 .084407 .065442
piratebattler3    pirateraider3      aiclass=pb3default.ai
# DESC:  
# BBOX: -.026466 -.012833 -.099682 .026261 .023106 .061024
piratebattler4    pirateraider4      aiclass=pb2default.ai
# DESC:  
# BBOX: -.125 -.125 -.150003 .125 .125 .100004
crate_push        _walkstruct        move=physics model3d=crt-push.3do size=.246224 movesize=.15 surfdrag=2 mass=150 height=.15 physflags=0x425d
# DESC:  
# BBOX: -.148021 -.200008 -.199979 .148021 .200008 .199979
funicular         _walkstruct        model3d=01func.3do size=.369225 movesize=.369225 soundclass=funicular.snd
# DESC:  
# BBOX: -.038893 -.037552 -.024816 .039144 .036124 .024816
bottles04         _structure         collide=1 model3d=bottles4.3do size=.05 movesize=.05
# DESC:  
# BBOX: -.15875 -.30226 -.05207 .15875 .30226 .05207
xwing_debris_0    _debris            model3d=xwng_prt.3do size=.325479 movesize=.325479
# DESC:  
# BBOX: -.012965 -.007189 -.036159 .012965 .007189 .036159
s4l1switch0       _powerup           thingflags=0x10 model3d=levr-1.3do cog=s4l1_forceswitch0.cog
# DESC:  
# BBOX: -.012965 -.007189 -.036159 .012965 .007189 .036159
platformcontrolswitch _ghostdecor        model3d=levr-1.3do size=.042988 movesize=.042988
# DESC:  
# BBOX: -.1 -.0125 -.1 .1 .0125 .1
09_2x2door        _structure         model3d=09d2x2_1.3do size=.141973 movesize=.141973 soundclass=sm_door.snd
# DESC:  
# BBOX: -.238012 -.5 -.077109 .238012 .5 .077109
s4l1coridoor      _walkstruct        model3d=41cord.3do size=.609102 movesize=.609102 soundclass=catwalkfall.snd
# DESC:  
# BBOX: -.01264 -.049041 -.01129 .01264 .049041 .01129
shortrod          _structure         collide=1 model3d=41rod.3do size=.05 movesize=.05
# DESC:  
# BBOX: -.01264 -.299708 -.01129 .01264 .299708 .01129
rod02             _structure         model3d=rod02.3do size=.350187 movesize=.350187
# DESC:  
# BBOX: -.199901 -.025655 -.200152 .199901 .025655 .200152
quadwindow        _ghoststructure    model3d=gwndw_2.3do size=.268611 movesize=.268611
# DESC:  
# BBOX: -.15 -.15 -.150003 .15 .15 .150003
crate4_4          _walkstruct        model3d=crt4-4.3do size=.25981 movesize=.25981
# DESC:  
# BBOX: -.020439 -.013603 -.012042 .020439 .013603 .012042
fivepellets       _cpowerup          thingflags=0x400 model3d=cbnpk5.3do cog=pow_carbpellets_m.cog
# DESC:  
# BBOX: -.026466 -.012833 -.099682 .026261 .023106 .061024
pirateraider2     _humanactor        thingflags=0x20000400 model3d=p2.3do size=.0615 movesize=.0615 puppet=p2.pup soundclass=pr.snd cog=actor_pr.cog mass=70 maxvel=.3 weapon=+ebolt maxrotthrust=80 typeflags=0x2080001 fireoffset=(.016/.055/.004) aiclass=pr_def.ai
# DESC:  
# BBOX: -.09999 -.199962 -.031081 .09999 .199962 .031081
2x4elev           _walkstruct        model3d=e2x4.3do size=.225719 movesize=.225719 soundclass=med_elev.snd
# DESC:  
# BBOX: -.034484 -.022835 -.120264 .045456 .021911 .070408
twoonebee         none               orient=(0/0/0) type=cog collide=1 thingflags=0x400 model3d=med.3do size=.06 movesize=.06 puppet=cr.pup cog=00_twoonebee.cog
# DESC:  
# BBOX: -.037806 -.014831 -.120834 .037548 .016412 .065442
pirateraider1     _humanactor        thingflags=0x20000400 model3d=p1.3do size=.0649 movesize=.0649 puppet=tkstr.pup soundclass=prstr.snd cog=actor_pr1_pnch.cog mass=120 maxvel=.2 weapon=+elaser weapon2=+st_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(.0416/.0682/.0225) aiclass=pr1_def.ai
# DESC:  
# BBOX: -.123035 -.075245 -.189945 .123034 .145052 .179595
loaderdroid2      _droidactor        thingflags=0x4 model3d=lb2.3do size=.18 movesize=.18 puppet=lb2.pup soundclass=lb.snd cog=actor_gg.cog mass=750 weapon=+gamaxe health=100 maxhealth=100 maxrotthrust=80 typeflags=0x1a8141 aiclass=ld2default.ai
# DESC:  
# BBOX: -.026466 -.012833 -.099682 .026261 .023106 .061024
piratebattler2    pirateraider2      aiclass=pb2default.ai
# DESC:  
# BBOX: -.035317 -.025605 -.047024 .035366 .022343 .048893
r2_good           r2                 aiclass=drgdefault.ai
# DESC:  
# BBOX: -.03677 -.021965 -.007763 .03677 .027597 .007763
+p1limb           _limb              model3d=p1limb.3do size=.07 movesize=.01
# DESC:  
# BBOX: 0 0 0 0 0 0
+xwing_exp        +tie_exp           debris=xwing_debris_0
# DESC:  
# BBOX: -.597832 -.597465 -.299701 .597179 .59758 .299701
00archcrate       _walkstruct        model3d=00crte.3do size=.830224 movesize=.830224
# DESC:  
# BBOX: -1.58115 -2.28346 -1.066165 1.58115 2.28346 1.066165
transb            _zwalkstruct       model3d=transb1.3do size=2.719194 movesize=2.719194
# DESC:  
# BBOX: -.597832 -.597465 -.299701 .597179 .59758 .299701
00zpicrate        _zwalkstruct       model3d=00picrte.3do size=.858247 movesize=.858247 soundclass=funicular.snd
# DESC:  
# BBOX: -.057012 -.079857 -.050443 .057012 .079857 .050443
constructiondroid _flyactor          thingflags=0x20000400 model3d=cdrd.3do size=.055 movesize=.055 soundclass=rs.snd cog=actor_sd.cog mass=30 physflags=0x2006a06 maxvel=.6 weapon=+ebolt health=60 maxhealth=60 maxthrust=.8 maxrotthrust=80 typeflags=0x2108120 fireoffset=(-.0073/.0393/0) explode=+sentry_exp aiclass=sddefault.ai
# DESC:  
# BBOX: -.053401 -.061619 -.061702 .053401 .061619 .061702
crate6_2          _walkstruct        thingflags=0x400448 model3d=crt6-2.3do size=.087201 movesize=.087201 cog=actor_cr.cog
# DESC:  
# BBOX: -.181678 -.060004 -.073819 .181678 .060004 .073819
roofvent3         _walkstruct        model3d=01rfd3.3do size=.249375 movesize=.249375
# DESC:  
# BBOX: -.228837 -.19266 -.30048 .228837 .19266 .30048
s4l3clamp         _decor             model3d=43clamp.3do size=.26 movesize=.26
# DESC:  
# BBOX: -.15 -.025 -.1 .15 .025 .1
3x2door_2         _zwalkstruct       model3d=d3x2_2.3do size=.232003 movesize=.232003 soundclass=sm_door.snd
# DESC:  
# BBOX: -.2 -.5 -.025 .2 .5 .025
forklift          _zwalkstruct       model3d=fklift.3do size=.589096 movesize=.589096 soundclass=lg_elev.snd
# DESC:  
# BBOX: -.061517 -.037623 -.094972 .061517 .072526 .089798
loaderdroid       _droidactor        thingflags=0x20000400 model3d=lb.3do size=.09 movesize=.09 puppet=lb.pup soundclass=lb.snd cog=actor_gg.cog mass=250 weapon=+gamaxe health=100 maxhealth=100 maxrotthrust=80 typeflags=0x1a8141 aiclass=lddefault.ai
# DESC:  
# BBOX: -.061517 -.037623 -.094972 .061517 .072526 .089798
loaderdroids4l2   loaderdroid        cog=actor_ggs4l2.cog
# DESC:  
# BBOX: -.2 -.0125 -.2 .2 .0125 .2
00expwall1a       _walkstruct        model3d=00wexp1a.3do size=.333119 movesize=.333119
# DESC:  
# BBOX: -.242535 -.449297 -.033361 .510302 .456385 .033361
s4l2fan           2x3door_1          model3d=s4l2fan.3do size=.475 movesize=.475 puppet=cr.pup
# DESC:  
# BBOX: -.40002 -.400093 -.024993 .40002 .400093 .024993
8x8elevs4l2       _walkstruct        thingflags=0x400008 model3d=e8x8.3do size=.616317 movesize=.616317 soundclass=lg_elev.snd
# DESC:  
# BBOX: -.032373 -.017479 .000097 .031819 .017479 .079704
yellowwrenchkey   _powerup           thingflags=0x400 model3d=keyyel.3do cog=pow_wrenchyellow.cog
# DESC:  
# BBOX: -.100779 -.107544 .000686 .100779 .107544 .023173
ceilinglight2     _ghoststructure    model3d=clight0.3do size=.166487 movesize=.166487
# DESC:  
# BBOX: -.021089 -.023005 -.035347 .016307 .023005 .035347
+dsmbp            _droppowerup       thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk.cog height=.036337
# DESC:  
# BBOX: 0 0 0 0 0 0
+superenerg       none               orient=(0/0/0) type=particle move=physics timer=.6 vel=(0/0/2) angvel=(0/360/0) typeflags=0x3e material=00red.mat range=1.2 elementsize=.02 count=30
# DESC:  
# BBOX: 0 0 0 0 0 0
+superenerg2      +superenerg        range=.6
# DESC:  
# BBOX: -.0254 -.32385 -.03048 .0254 .32385 .03048
z95_debris_0      _debris            model3d=z95_1d0.3do size=.325479 movesize=.325479
# DESC:  
# BBOX: -.4064 -.42672 -.13843 .4064 .42672 .13843
z95_debris_1      _debris            model3d=z95_1d1.3do size=.584212 movesize=.584212
# DESC:  
# BBOX: -.20193 -.34925 -.03048 .20193 .34925 .03048
z95_debris_2      _debris            model3d=z95_1d2.3do size=.403432 movesize=.403432
# DESC:  
# BBOX: -.25146 -.60071 -.08636 .25146 .60071 .08636
z95_debris_3      _debris            model3d=z95_1d3.3do size=.623332 movesize=.623332
# DESC:  
# BBOX: 0 0 0 0 0 0
+z95_exp          +tie_exp           debris=z95_debris_0 debris=z95_debris_1 debris=z95_debris_2 debris=z95_debris_3
# DESC:  
# BBOX: 0 0 0 0 0 0
+03_exp           +raildet_exp       damage=25 force=60 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_4
# DESC:  
# BBOX: -.2 -.0125 -.2 .2 .0125 .2
00expwall1b       _walkstruct        model3d=00wexp1b.3do size=.333119 movesize=.333119
# DESC:  
# BBOX: -.026466 -.012833 -.099682 .026261 .023106 .061024
pirateped4        _humanactor        thingflags=0x20000400 model3d=p4.3do size=.0615 movesize=.0615 puppet=p2.pup soundclass=pr.snd cog=actor_prped4.cog mass=70 maxvel=.3 weapon=+elaser maxrotthrust=80 typeflags=0x2080001 fireoffset=(.016/.065/.004) aiclass=pr_def.ai
# DESC:  
# BBOX: -.037806 -.014831 -.120834 .037548 .016412 .065442
pirateped1        _humanactor        thingflags=0x20000400 model3d=p1.3do size=.0649 movesize=.0649 puppet=tkstr.pup soundclass=prstr.snd cog=actor_prped1.cog mass=120 maxvel=.2 weapon=+elaser weapon2=+st_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(.0416/.0682/.0225) aiclass=pr1_def.ai
# DESC:  
# BBOX: -.163907 -.240264 -.151864 .163907 .240264 .151864
tent              _zwalkstruct       thingflags=0x800040 model3d=tent.3do size=.355021 movesize=.355021
# DESC:  
# BBOX: 0 0 0 0 0 0
+ng_sword1        _weapon            size=.001 movesize=.001 mass=100 explode=+axe_exp fleshhit=+axe_exp damage=45 damageclass=0x1 typeflags=0x200d range=.26 force=50
# DESC:  
# BBOX: 0 0 0 0 0 0
+ng_sword2        _weapon            size=.001 movesize=.001 mass=100 explode=+axe_exp fleshhit=+axe_exp damage=60 damageclass=0x1 typeflags=0x200d range=.27 force=50
# DESC:  
# BBOX: -.085128 -.050675 -.129376 .16834 .088949 .086323
noghri            _humanactor        collide=2 thingflags=0x20000400 model3d=ng.3do size=.0775 movesize=.0775 treesize=.5 puppet=ng.pup soundclass=ng.snd cog=actor_ng.cog mass=300 weapon=+ng_sword1 weapon2=+ng_sword2 health=200 maxhealth=200 maxthrust=1 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 aiclass=ng_def.ai
# DESC:  
# BBOX: -.149972 -.150118 -.099987 .149972 .150118 .099987
puzzlecolumn      _walkstruct        model3d=clmz.3do size=.284573 movesize=.284573
# DESC:  
# BBOX: 0 0 0 0 0 0
_swimactor        _humanactor        physflags=0x2004a0f typeflags=0x20040
# DESC:  
# BBOX: 0 0 0 0 0 0
+drugonbite       +gamaxe            damage=50 range=.4
# DESC:  
# BBOX: -.122593 -.438462 -.119063 .144735 .269351 .112767
sucker            _swimactor         model3d=su.3do size=.33 movesize=.33 puppet=su.pup soundclass=sucker.snd mass=500 physflags=0x2404a0f maxvel=.6 weapon=+drugonbite health=160 maxhealth=160 maxthrust=.7 maxrotthrust=90 typeflags=0x20140 aiclass=dudefault.ai
# DESC:  
# BBOX: -.0375 -.0375 -.00017 .0375 .0375 .07483
holocroncube      _powerup           thingflags=0x408 model3d=holocube.3do cog=pow_holocron.cog mass=0 angvel=(0/90/0)
# DESC:  
# BBOX: -.174123 -.174123 -.012502 .174123 .174123 .012502
s5l4_disc         _walkstruct        thingflags=0x48 model3d=54epad.3do size=.192028 movesize=.192028
# DESC:  
# BBOX: -.011495 -.015768 -.018985 .011495 .015768 .018985
8t88_head2        none               orient=(0/0/0) type=cog collide=1 model3d=8tbighed.3do size=.073202 movesize=.073202
# DESC:  
# BBOX: -.4 -.050015 -.398 .4 .050015 .4
s4l3_8x8door      _walkstruct        model3d=d8x8.3do size=.617892 movesize=.617892 soundclass=lg_door.snd
# DESC:  
# BBOX: -.012965 -.007189 -.036159 .012965 .007189 .036159
s4l3switch0       _powerup           thingflags=0x10 model3d=levr-1.3do cog=s4l3_forceswitch0.cog
# DESC:  
# BBOX: -.012965 -.007189 -.036159 .012965 .007189 .036159
s4l3switch1       _powerup           thingflags=0x10 model3d=levr-1.3do cog=s4l3_forceswitch1.cog
# DESC:  
# BBOX: -.012965 -.007189 -.036159 .012965 .007189 .036159
s4l3switch2       _powerup           thingflags=0x10 model3d=levr-1.3do cog=s4l3_forceswitch2.cog
# DESC:  
# BBOX: -.75 -.499 -.034859 .75 .499 .034859
baydoor_a         _walkstruct        model3d=bayd_a.3do size=.951508 movesize=.951508
# DESC:  
# BBOX: -.032914 -.035695 -.000032 .032825 .029144 .056184
dtroophead        _decor             model3d=darktroophead.3do size=.063718 movesize=.063718
# DESC:  
# BBOX: -.36642 -.41509 -.08587 .36642 .41509 .08587
r41_flt           _zwalkstruct       model3d=r41_1_flt.3do size=.510666 movesize=.510666
# DESC:  
# BBOX: -.39751 -.94488 -.28956 .39751 .94488 .28956
transport_flt     _zwalkstruct       model3d=trnsprt1_flt.3do size=1.098054 movesize=1.098054
# DESC:  
# BBOX: 0 0 0 0 0 0
+upenergylo       none               orient=(0/0/0) type=particle move=physics timer=.8 vel=(0/0/5) typeflags=0x3e material=00red.mat range=.05 elementsize=.005 count=2
# DESC:  
# BBOX: 0 0 0 0 0 0
+mailocsting      +gamaxe            damage=15 force=25
# DESC:  
# BBOX: -.080669 -.095318 -.091964 .082473 .08505 .212669
mynok             _humanactor        thingflags=0x20000400 model3d=my.3do size=.1 movesize=.1 puppet=my.pup soundclass=mynoc.snd cog=actor_mailoc.cog mass=40 physflags=0x6a06 weapon=+mailocsting maxthrust=.9 maxrotthrust=70 typeflags=0x2020000 aiclass=maidefault.ai
# DESC:  
# BBOX: -.075265 -.064442 -.151527 .075265 .064442 .151527
zstatue           _zwalkstruct       thingflags=0x40 model3d=stat.3do size=.231047 movesize=.231047
# DESC:  
# BBOX: 0 0 0 0 0 0
+ws_claw1         _weapon            size=.001 movesize=.001 mass=100 explode=+axe_exp fleshhit=+axe_exp damage=40 damageclass=0x1 typeflags=0x200d range=.21 force=50
# DESC:  
# BBOX: 0 0 0 0 0 0
+ws_claw2         _weapon            size=.001 movesize=.001 mass=100 explode=+axe_exp fleshhit=+axe_exp damage=50 damageclass=0x1 typeflags=0x200d range=.22 force=50
# DESC:  
# BBOX: -.088304 -.080169 -.161827 .088612 .02719 .102677
wampaswamp        _humanactor        thingflags=0x20000400 model3d=ws.3do size=.1027 movesize=.1027 puppet=ws.pup soundclass=ws.snd cog=actor_ws.cog mass=250 weapon=+ws_claw1 weapon2=+ws_claw2 health=150 maxhealth=150 maxthrust=1 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 aiclass=ws_def.ai
# DESC:  
# BBOX: -.017157 -.016142 -.014361 .017157 .016142 .014361
stgib1            _ghostdecor        model3d=stgib1.3do size=.069187 movesize=.069187
# DESC:  
# BBOX: -.021089 -.023005 -.035347 .016307 .023005 .035347
tim's_bkpk11      _powerup           thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk_11m.cog
# DESC:  
# BBOX: -.2 -.2 0 .2 .2 0
carpet            _walkstruct        thingflags=0x800048 model3d=carpet.3do size=.332843 movesize=.332843
# DESC:  
# BBOX: -.161554 -.23891 -.384493 .161554 .23891 .384493
treestump1-a      _zwalkstruct       thingflags=0x40 model3d=btr1-a.3do size=.504194 movesize=.504194
# DESC:  
# BBOX: -.376358 -.419191 -.848055 .376357 .41919 .848055
treestump2-a      _walkstruct        thingflags=0x48 model3d=btr2-a.3do size=1.012674 movesize=1.012674
# DESC:  
# BBOX: -.021089 -.023005 -.035347 .016307 .023005 .035347
tim's_bkpk2       _powerup           thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk_2m.cog
# DESC:  
# BBOX: -.021089 -.023005 -.035347 .016307 .023005 .035347
tim's_bkpk3       _powerup           thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk_3m.cog
# DESC:  
# BBOX: -.016776 -.034744 -.010952 .018316 .034538 .00282
ysalamari         _civilian          thingflags=0x0 model3d=ys.3do size=.0055 movesize=.0055 puppet=ys.pup soundclass=ug.snd cog=actor_ys.cog mass=60 maxvel=.4 health=30 maxthrust=.5 maxrotthrust=60 aiclass=peddefault.ai
# DESC:  
# BBOX: -.040553 -.199597 -.201059 .040553 .199597 .201059
s5l2vines         _decor             model3d=bvine.3do size=.15 movesize=.15
# DESC:  
# BBOX: -.034756 -.02133 -.007677 .034756 .02133 .007677
stgib3            _ghostdecor        model3d=stgib3.3do size=.08891 movesize=.08891
# DESC:  
# BBOX: -.174338 -.220588 -.001853 .174338 .284947 .817133
treestump1        _zwalkstruct       thingflags=0x40 model3d=btr1.3do size=.896716 movesize=.896716
# DESC:  
# BBOX: -.03092 -.030162 -.030457 .03092 .030162 .030457
lightside         _powerup           thingflags=0x400 model3d=mana1.3do cog=pow_darkside.cog respawn=60
# DESC:  
# BBOX: -.180762 -.209203 .023289 .180762 .164571 .135454
swhead            _zwalkstruct       collide=1 thingflags=0x800440 model3d=swhead.3do size=.22953 movesize=.26953 cog=actor_sl.cog
# DESC:  
# BBOX: -.405101 -.598233 -.873055 .405101 .296312 .873055
treestump2        _zwalkstruct       thingflags=0x40 model3d=btr2.3do size=1.124021 movesize=1.124021
# DESC:  
# BBOX: -.164828 -.141095 -.188794 .15033 .099739 .190329
dianoga           _swimactor         thingflags=0x400 model3d=di.3do size=.165 movesize=.165 puppet=di.pup soundclass=sucker.snd cog=actor_dnga.cog mass=380 physflags=0x2404a0f maxvel=.6 weapon=+drugonbite health=160 maxhealth=160 maxthrust=.7 maxrotthrust=90 typeflags=0x20140 aiclass=di_def.ai
# DESC:  
# BBOX: -.004485 -.017964 -.004485 .004485 .017964 .004485
+plantspittle     _weapon            model3d=vfspittl.3do size=.001 movesize=.001 soundclass=bry.snd maxrotvel=0 vel=(0/4/0) explode=+axe_exp fleshhit=+axe_exp damage=30 mindamage=10 damageclass=0x1 typeflags=0x20440d rate=15
# DESC:  
# BBOX: -.058684 -.017148 -.124887 .007903 .018428 .117391
dartflower        _humanactor        collide=2 model3d=vf.3do size=.075 movesize=.075 treesize=.2 puppet=vf.pup soundclass=vf.snd mass=0 weapon=+plantspittle health=50 maxhealth=50 typeflags=0x20001 aiclass=dfdefault.ai
# DESC:  
# BBOX: -.174338 -.180157 -.025 .174338 .180157 .025
treestump1-b      _ghostdecor        model3d=btr1-b.3do size=.254496 movesize=.254496
# DESC:  
# BBOX: -.405101 -.432609 -.025 .405101 .432609 .025
treestump2-b      _structure         model3d=btr2-b.3do size=.520307 movesize=.520307
# DESC:  
# BBOX: -.300125 -.025 -.001357 .300125 .025 .598643
52stonedoor       _walkstruct        model3d=52utdr.3do size=.720129 movesize=.720129 soundclass=19stone_door.snd
# DESC:  
# BBOX: 0 0 0 0 0 0
_darkjedi         _humanactor        typeflags=0x2000201
# DESC:  
# BBOX: -.035332 -.013185 -.120103 .035579 .016413 .064509
darkmara          _darkjedi          model3d=kydm.3do puppet=mjdarkjedi.pup soundclass=mj.snd mass=170 maxvel=.2 weapon=+gamaxe health=2000 maxhealth=2000 maxthrust=1 maxrotthrust=200 jumpspeed=4 typeflags=0x2008201 error=.1 fov=.4 chance=1 aiclass=darkmara.ai
# DESC:  
# BBOX: -.035332 -.01877 -.120103 .036644 .016413 .064197
darkmaraboss      darkmara           model3d=mjboss.3do
# DESC:  
# BBOX: -.005058 0 -.002337 .005058 0 .002337
s5l2eyes          _ghostdecor        model3d=52eye.3do size=.055058 movesize=.055058
# DESC:  
# BBOX: -.037418 -.063387 -.017067 .025236 .03643 .012634
+mylimb           _limb              model3d=mylimb.3do size=.07 movesize=.01
# DESC:  
# BBOX: -.078691 -.073436 -.037355 .048371 .113983 .029054
+wslimb           _limb              model3d=wslimb.3do size=.125978 movesize=.01
# DESC:  
# BBOX: 0 0 0 0 0 0
+cycbite          +gamaxe            damage=25 range=.5
# DESC:  
# BBOX: -.06753 -.017802 -.714146 .097985 .078244 .099411
+lurktentacles    _actor             thingflags=0x20000400 model3d=sw.3do size=.178 movesize=.178 puppet=sw.pup cog=actor_slt.cog mass=0 physflags=0x2404a0f maxvel=.6 weapon=+cycbite health=100 maxhealth=100 maxthrust=.4 maxrotthrust=60 typeflags=0x20140
# DESC:  
# BBOX: -.300125 -.025 -.001357 .300125 .025 .598643
undergroundtempledoor _structure         model3d=utdr.3do size=.720129 movesize=.720129 soundclass=54stonedoors.snd
# DESC:  
# BBOX: -.035671 -.036443 -.123155 .055853 .026546 .063148
statue1           _humanactor        thingflags=0x404 model3d=s1.3do size=.063 movesize=.063 puppet=statue1.pup soundclass=s0.snd cog=actor_ss1.cog mass=170 maxrotvel=15 maxvel=.2 buoyancy=1 weapon=+gamaxe health=300 maxhealth=300 maxthrust=1 maxrotthrust=40 jumpspeed=4 typeflags=0x28201 error=.1 fov=-.4 chance=1 aiclass=s0i_def.ai
# DESC:  
# BBOX: -.078392 -.040153 -.120632 .032111 .079957 .038706
statue0           _humanactor        thingflags=0x404 model3d=s0.3do size=.0604 movesize=.0604 puppet=statue0.pup soundclass=s0.snd cog=actor_ss0.cog mass=160 maxvel=.3 buoyancy=1 weapon=+gamaxe health=300 maxhealth=300 maxrotthrust=40 jumpspeed=1.6 typeflags=0x20201 error=.6 fov=.5 chance=1 aiclass=s1i_def.ai
# DESC:  
# BBOX: -.4 -.05 -.4 .4 .05 .4
templedoorbasic   _structure         model3d=54d8x8.3do size=.617891 movesize=.617891 soundclass=54stonedoors.snd
# DESC:  
# BBOX: -.027321 -.032261 .000219 .027321 .032261 .126942
floatingchair     _decor             move=physics model3d=chair3m.3do size=.09 movesize=.09 surfdrag=.7 airdrag=1 mass=35 physflags=0x422c angvel=(22.5/11.25/22.5)
# DESC:  
# BBOX: -.158991 -.163886 -.224502 .158991 .163886 .224502
rock2mini         _walkstruct        thingflags=0x48 model3d=rck2mini.3do size=.369958 movesize=.369958
# DESC:  
# BBOX: 0 0 0 0 0 0
+vornbite         +gamaxe            damage=25 range=.15 force=100
# DESC:  
# BBOX: -.063829 -.124708 -.075524 .067364 .10618 .075852
vornskr           _humanactor        model3d=vr.3do size=.0685 movesize=.0685 puppet=vr.pup soundclass=vr.snd mass=300 maxvel=.6 weapon=+vornbite weapon2=+vornbite health=200 maxhealth=200 maxthrust=1 maxrotthrust=120 typeflags=0x2028100 aiclass=vrdefault.ai
# DESC:  
# BBOX: -.1 -.0125 -.15 .1 .0125 .15
s5l3door1         _structure         model3d=53d2x3.3do size=.23071 movesize=.23071 soundclass=03wood_door.snd
# DESC:  
# BBOX: -.098284 -.097923 -.031081 .097733 .098093 .031081
s5l3elev          _walkstruct        thingflags=0x48 model3d=53e2x2.3do size=.192295 movesize=.192295 soundclass=53stoneelev.snd
# DESC:  
# BBOX: -.036449 -.016107 -.120075 .037661 .02734 .064161
kyleactor         _humanactor        model3d=ky-0.3do puppet=mj_cuts.pup soundclass=ky.snd maxvel=.4 health=20000 maxhealth=20000 maxthrust=1 maxrotthrust=60 aiclass=jan.ai
# DESC:  
# BBOX: -.2 -.0125 -.15 .2 .0125 .15
s5l3door2         _structure         model3d=53d4x3.3do size=.300312 movesize=.300312 soundclass=03wood_door.snd
# DESC:  
# BBOX: -.35 -.625 -.05 .35 .625 .05
s5l3_beam0        _walkstruct        thingflags=0x48 model3d=53beam0.3do size=.76807 movesize=.76807
# DESC:  
# BBOX: -.375 -.25 -.05 .375 .25 .05
s5l3_beam1        _walkstruct        thingflags=0x48 model3d=53beam1.3do size=.503459 movesize=.503459
# DESC:  
# BBOX: -.55 -1.025 -.05 .55 1.025 .05
s5l3_beam2        _walkstruct        thingflags=0x48 model3d=53beam2.3do size=1.214313 movesize=1.214313
# DESC:  
# BBOX: -.021089 -.023005 -.035347 .016307 .023005 .035347
tim's_bkpk7       _powerup           thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk_7m.cog
# DESC:  
# BBOX: -.021089 -.023005 -.035347 .016307 .023005 .035347
tim's_bkpk8       _powerup           thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk_8m.cog
# DESC:  
# BBOX: -.021089 -.023005 -.035347 .016307 .023005 .035347
tim's_bkpk1       _powerup           thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk_1m.cog
# DESC:  
# BBOX: 0 0 0 0 0 0
_statuedebris     none               orient=(0/0/0) type=weapon collide=1 move=physics timer=15 airdrag=3 mass=30 physflags=0x201 typeflags=0x100
# DESC:  
# BBOX: -.014366 -.00976 -.00814 .014538 .009633 .007972
+statue1debris_1  _statuedebris      model3d=s1-d1.3do size=.008301 movesize=.008301
# DESC:  
# BBOX: -.013278 -.041702 -.013703 .013278 .041702 .013703
+statue1debris_2  _statuedebris      timer=20 model3d=s1-d2.3do size=.010207 movesize=.010207
# DESC:  
# BBOX: -.015753 -.015778 -.011138 .015753 .015778 .011138
+statue1debris_3  _statuedebris      timer=19 model3d=s1-d3.3do size=.012147 movesize=.012147
# DESC:  
# BBOX: -.052358 -.035478 -.01031 .055594 .022557 .01031
+statue1debris_4  _statuedebris      timer=18 model3d=s1-d6.3do size=.011599 movesize=.011599
# DESC:  
# BBOX: -.018981 -.026507 -.014317 .018981 .026507 .014317
+statue1debris_5  _statuedebris      timer=25 model3d=s1-d5.3do size=.015215 movesize=.015215
# DESC:  
# BBOX: -.052358 -.035478 -.01031 .055594 .022557 .01031
+statue1debris_6  _statuedebris      timer=10 model3d=s1-d6.3do size=.011599 movesize=.011599
# DESC:  
# BBOX: -.001953 .010832 -.10415 .003749 .014182 -.005192
s1blade           _ghostdecor        model3d=s1blade.3do size=.15497 movesize=.15497
# DESC:  
# BBOX: -.015774 -.012607 -.008304 .016083 .009876 .012121
+statue0debris_1  _statuedebris      model3d=s0-d1.3do size=.008301 movesize=.008301
# DESC:  
# BBOX: -.013278 -.041702 -.013703 .013278 .041702 .013703
+statue0debris_2  _statuedebris      timer=20 model3d=s0-d2.3do size=.010207 movesize=.010207
# DESC:  
# BBOX: -.015753 -.015778 -.011138 .015753 .015778 .011138
+statue0debris_3  _statuedebris      timer=19 model3d=s0-d3.3do size=.012147 movesize=.012147
# DESC:  
# BBOX: -.052358 -.035478 -.01031 .055594 .022557 .01031
+statue0debris_4  _statuedebris      timer=18 model3d=s0-d6.3do size=.011599 movesize=.011599
# DESC:  
# BBOX: -.018981 -.026507 -.014317 .018981 .026507 .014317
+statue0debris_5  _statuedebris      timer=25 model3d=s0-d5.3do size=.015215 movesize=.015215
# DESC:  
# BBOX: -.052358 -.035478 -.01031 .055594 .022557 .01031
+statue0debris_6  _statuedebris      timer=10 model3d=s0-d6.3do size=.011599 movesize=.011599
# DESC:  
# BBOX: -.000719 .01684 -.117316 .007319 .027056 .001032
s0blade           _ghostdecor        model3d=s0blade.3do size=.170383 movesize=.170383
# DESC:  
# BBOX: -.4 -.05 -.4 .4 .05 .4
templedoor1b      _structure         model3d=54d8x81b.3do size=.617891 movesize=.617891 soundclass=54stonedoors.snd
# DESC:  
# BBOX: -.4 -.05 -.4 .4 .05 .4
templedoor1a      _structure         model3d=54d8x81a.3do size=.617891 movesize=.617891 soundclass=54stonedoors.snd
# DESC:  
# BBOX: -.4 -.05 -.4 .4 .05 .4
templedoor3b      _structure         model3d=54d8x83b.3do size=.617891 movesize=.617891 soundclass=54stonedoors.snd
# DESC:  
# BBOX: -.4 -.05 -.4 .4 .05 .4
templedoor3a      _structure         model3d=54d8x83a.3do size=.617891 movesize=.617891 soundclass=54stonedoors.snd
# DESC:  
# BBOX: -.4 -.05 -.4 .4 .05 .4
templedoor2b      _structure         model3d=54d8x82b.3do size=.617891 movesize=.617891 soundclass=54stonedoors.snd
# DESC:  
# BBOX: -.4 -.05 -.4 .4 .05 .4
templedoor2a      _structure         model3d=54d8x82a.3do size=.617891 movesize=.617891 soundclass=54stonedoors.snd
# DESC:  
# BBOX: -.4 -.05 -.4 .4 .05 .4
templedoor4       _structure         model3d=54d8x84.3do size=.617891 movesize=.617891 soundclass=54stonedoors.snd
# DESC:  
# BBOX: -.09999 -.199962 -.031081 .09999 .199962 .031081
s5l4elev2x4       _walkstruct        thingflags=0x48 model3d=54e2x4.3do size=.275719 movesize=.275719 soundclass=54stoneelev.snd
# DESC:  
# BBOX: -.148021 -.200008 -.199979 .148021 .200008 .199979
s5l4funicular     _walkstruct        thingflags=0x48 model3d=sfun.3do size=.369225 movesize=.369225 soundclass=54stoneelev.snd
# DESC:  
# BBOX: -.198 -.197998 .03296 .198 .198002 .05971
s5l4elev4x4       _walkstruct        thingflags=0x48 model3d=54e4x4.3do size=.336311 movesize=.336311 soundclass=54stoneelev.snd
# DESC:  
# BBOX: -.15 -1.875 -.012306 .15 1.875 .012306
sithcatwalk       _zwalkstruct       thingflags=0x40 model3d=sthcwlk.3do size=2.069134 movesize=2.069134
# DESC:  
# BBOX: -.174338 -.220588 -.001853 .174338 .284947 .817133
s5l4treestump1    _walkstruct        thingflags=0x800048 model3d=54btr1.3do size=.896716 movesize=.896716
# DESC:  
# BBOX: -.405101 -.598233 -.873055 .405101 .296312 .873055
s5l4treestump2    _walkstruct        thingflags=0x800048 model3d=54btr2.3do size=1.124021 movesize=1.124021
# DESC:  
# BBOX: -.075265 -.064442 -.151527 .075265 .064442 .151527
54statue          _walkstruct        thingflags=0x48 model3d=54stat.3do size=.231047 movesize=.231047 soundclass=19stone_door.snd
# DESC:  
# BBOX: -.068375 -.11004 -.03939 .068375 .11004 .038931
coffin1           _walkstruct        thingflags=0x48 model3d=coff-1.3do size=.134448 movesize=.134448
# DESC:  
# BBOX: -.2 -.2 -.8 .2 .2 .8
s5l4_column_1     _structure         model3d=54clm1.3do size=.933381 movesize=.933381
# DESC:  
# BBOX: -.2 -.2 -.8 .2 .2 .8
s5l4_column_2     _structure         model3d=54clm2.3do size=.933381 movesize=.933381
# DESC:  
# BBOX: -.021089 -.023005 -.035347 .016307 .023005 .035347
tim's_bkpk4       _powerup           thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk_4m.cog
# DESC:  
# BBOX: -.021089 -.023005 -.035347 .016307 .023005 .035347
tim's_bkpk5       _powerup           thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk_5m.cog
# DESC:  
# BBOX: -.037301 -.013874 -.118461 .038114 .03984 .064544
darkkyle          _darkjedi          model3d=kk.3do puppet=mjdarkjedi.pup soundclass=ky.snd mass=170 maxvel=.2 weapon=+gamaxe health=20000 maxhealth=20000 maxthrust=1 maxrotthrust=200 jumpspeed=4 typeflags=0x2008201 error=.1 fov=-.4 chance=1 aiclass=darkkyle.ai
# DESC:  
# BBOX: -.037834 -.01877 -.121446 .03752 .016413 .064074
darkkyleboss      darkkyle           model3d=kkboss.3do
# DESC:  
# BBOX: -.021089 -.023005 -.035347 .016307 .023005 .035347
tim's_bkpk6       _powerup           thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk_6m.cog
# DESC:  
# BBOX: -.058684 -.017148 -.124887 .007903 .018428 .117391
dartflower_g      _humanactor        collide=2 model3d=vf.3do size=.075 movesize=.075 treesize=.2 puppet=vf.pup soundclass=vf2.snd mass=0 weapon=+plantspittle health=50 maxhealth=50 typeflags=0x20001 aiclass=dfdefault.ai
# DESC:  
# BBOX: -.2 -.0125 -.2 .2 .0125 .2
54expwall1a       _walkstruct        thingflags=0x48 model3d=54wexp1a.3do size=.333119 movesize=.333119
# DESC:  
# BBOX: -.119692 -.234862 -.063257 .119692 .234862 .063257
coffin2           _walkstruct        thingflags=0x48 model3d=coff-2.3do size=.321087 movesize=.321087
# DESC:  
# BBOX: -.040553 -.199597 -.201059 .040553 .199597 .201059
s5l4vines         _decor             model3d=54bvine.3do size=.15 movesize=.15
# DESC:  
# BBOX: -.124746 -.234862 -.027431 .124746 .234862 .027431
coffin2lid        _walkstruct        thingflags=0x48 model3d=coff-2l.3do size=.317347 movesize=.317347
# DESC:  
# BBOX: -.01264 -.299708 -.01129 .01264 .299708 .01129
s5l4vine02        _structure         model3d=54rope02.3do size=.350187 movesize=.350187
# DESC:  
# BBOX: -.035125 -.054514 -.118648 .040701 .051785 .046116
darkundead2       _humanactor        thingflags=0x20000400 model3d=du2.3do size=.0738 movesize=.0738 puppet=du.pup soundclass=du.snd cog=actor_du.cog mass=70 maxvel=.3 weapon=+force_lightning health=200 maxhealth=200 maxrotthrust=80 typeflags=0x2080001 fireoffset=(0/.066/.005) aiclass=du_def.ai
# DESC:  
# BBOX: -.035125 -.054514 -.11841 .040704 .051785 .045109
darkundead        _humanactor        thingflags=0x20000400 model3d=du.3do size=.0738 movesize=.0738 puppet=du.pup soundclass=du.snd cog=actor_du.cog mass=70 maxvel=.3 weapon=+force_lightning health=200 maxhealth=200 maxrotthrust=80 typeflags=0x2080001 fireoffset=(0/.066/.005) aiclass=du_def.ai
# DESC:  
# BBOX: -.2 -.0125 -.2 .2 .0125 .2
54expwall1b       _walkstruct        thingflags=0x48 model3d=54wexp1b.3do size=.333119 movesize=.333119
# DESC:  
# BBOX: 0 0 0 0 0 0
+force_lightning5 +force_lightning   damage=50
# DESC:  
# BBOX: 0 0 0 0 0 0
+s5l4_lightning   _weapon            thingflags=0x1 light=.1 timer=.25 size=.005 movesize=.005 angvel=(0/0/360) explode=+lightninghit fleshhit=+lightninghit trailthing=+force_ltpeice elementsize=.075 trailcylradius=.04 trailrandangle=25 damage=.1 damageclass=0x8 typeflags=0x1800d range=4
# DESC:  
# BBOX: -.170777 -.170777 -.012502 .170777 .170777 .012502
12_epad1          _walkstruct        model3d=12epad.3do size=.224571 movesize=.224571 soundclass=med_elev.snd
# DESC:  
# BBOX: -.490393 -.196424 -.05 .490393 .196424 .05
plathalf          _walkstruct        thingflags=0x48 model3d=plathalf.3do size=.580629 movesize=.580629
# DESC:  
# BBOX: -.1 -.0125 -.15 .1 .0125 .15
09_2x3door1       _structure         model3d=09d2x3_1.3do size=.18071 movesize=.18071 soundclass=sm_door.snd
# DESC:  
# BBOX: -.148 -.148 -.013055 .148 .148 .013695
ctf_3x3elev       _walkstruct        model3d=ctf_e3x3.3do size=.259751 movesize=.259751 soundclass=sm_elev.snd
# DESC:  
# BBOX: -.1 -.0125 -.15 .1 .0125 .15
09_2x3door2       _structure         model3d=09d2x3_2.3do size=.18071 movesize=.18071 soundclass=sm_door.snd
# DESC:  
# BBOX: -.4 -.025 -.4 .4 .025 .4
rounddoor4        _walkstruct        thingflags=0x400848 model3d=dr4.3do size=.400781 movesize=.400781
# DESC:  
# BBOX: -.030347 -.072704 -.024787 .009211 .072704 .024341
npconcrifle       concrifle          typeflags=0x9
# DESC:  
# BBOX: -.021293 -.02235 -.028598 .01838 .02235 .028374
npsupershield     supershield        typeflags=0x9
# DESC:  
# BBOX: -.03092 -.030162 -.030457 .03092 .030162 .030457
npdarkside        darkside           typeflags=0x9
# DESC:  
# BBOX: -.015415 -.017797 -.028888 .015415 .017797 .028888
rbacta            bactatank          typeflags=0x2 respawn=30
# DESC:  
# BBOX: -.012965 -.007189 -.036159 .012965 .007189 .036159
mdm02switch0      _powerup           thingflags=0x10 model3d=levr-1.3do cog=mdm02_forceswitch0.cog
# DESC:  
# BBOX: -.074978 -.074978 -.069146 .074978 .074978 .069146
woodcrate         _walkstruct        thingflags=0x48 model3d=wcrt.3do size=.176588 movesize=.176588
# DESC:  
# BBOX: -.1 -.005844 -.1 .1 .005844 .1
woodcatwalk       _walkstruct        model3d=shutr.3do size=.191542 movesize=.191542
# DESC:  
# BBOX: -.15 -.15 -.00625 .15 .15 .00625
03_3x3door_a      _walkstruct        model3d=03d3x3_3a.3do size=.262224 movesize=.262224
# DESC:  
# BBOX: -.3 -.0125 -.3 .3 .0125 .3
6x6door2          _walkstruct        model3d=d6x6.3do size=.474448 movesize=.474448 soundclass=lg_door.snd
# DESC:  
# BBOX: -.7 -.025 .00207 .7 .025 .80207
shielddoorsnd     _structure         model3d=bigdoor.3do size=1.114867 movesize=1.114867 soundclass=lg_door.snd
# DESC:  
# BBOX: -.2 -.0125 -.15 .2 .0125 .15
s2l1_door1        _structure         model3d=d4x3_s2l1.3do size=.300312 movesize=.300312 soundclass=med_door.snd
# DESC:  
# BBOX: -.174123 -.174123 -.012502 .174123 .174123 .012502
holocronpad       _walkstruct        model3d=hpad.3do size=.224571 movesize=.224571
# DESC:  
# BBOX: -.333269 -.125 -.001666 .333269 .125 .421753
throne            _walkstruct        thingflags=0x48 model3d=throne.3do size=.586502 movesize=.586502
# DESC:  
# BBOX: -.1 -.05 .000731 .1 .05 .600731
palacecolumn      _structure         model3d=06colm.3do size=.672151 movesize=.672151
# DESC:  
# BBOX: -.030347 -.072704 -.024787 .009211 .072704 .024341
+mdm04concrifle   _powerup           thingflags=0x400 model3d=conp.3do cog=pow_concrifle_m.cog typeflags=0x8 respawn=0
# DESC:  
# BBOX: -.030347 -.072704 -.024787 .009211 .072704 .024341
+mdm04concrifledecoy _powerup           thingflags=0x400 model3d=conp.3do cog=pow_concrifle_decoy.cog typeflags=0x8 respawn=0
# DESC:  
# BBOX: 0 0 0 0 0 0
+dustcloud2       +whitecloud        timer=.119 material=dusty.mat range=.01 rate=256 maxthrust=80 elementsize=.01
# DESC:  
# BBOX: -.15 -.011875 -.15 .15 .011875 .15
3x3reac           _walkstruct        model3d=e3x3reac.3do size=.233711 movesize=.233711 soundclass=sm_elev.snd
# DESC:  
# BBOX: -.200716 -.195991 .038027 .195304 .200102 .079681
4x4ingarvator     _zwalkstruct       model3d=e4x4_2i.3do size=.344409 movesize=.344409 soundclass=med_elev.snd
# DESC:  
# BBOX: -.1 -.5 -.012306 .1 .5 .012306
2x10catwalk       _walkstruct        model3d=c2x10.3do size=.51005 movesize=.51005
# DESC:  
# BBOX: -.052028 -.060925 .000731 .05132 .060823 .60066
moisturevaporator _structure         thingflags=0x400 model3d=vapr.3do size=.66097 movesize=.66097
# DESC:  
# BBOX: -.175 -.025 -.175 .175 .025 .175
09_octdoor        _structure         thingflags=0xc model3d=09doct1.3do size=.19204 movesize=.19204 soundclass=med_door.snd
# DESC:  
# BBOX: -.070489 -.081337 -.065817 .070489 .081337 .065817
woodcrate3        _walkstruct        thingflags=0x448 model3d=wcrt6-1.3do size=.154631 movesize=.154631 cog=actor_wc.cog
# DESC:  
# BBOX: -.25 -1.05 -.012306 .25 1.025 .012306
mdm07catwalk5     _walkstruct        model3d=c5x21_mdm07.3do size=1.187364 movesize=1.187364
# DESC:  
# BBOX: -.15 -1.2 -.012306 .15 1.2 .012306
mdm07catwalk3     _walkstruct        model3d=c3x24_mdm07.3do size=1.330341 movesize=1.330341
# DESC:  
# BBOX: -.1 -.9 -.012306 .1 .9 .012306
mdm07catwalk18    _walkstruct        model3d=c2x18_mdm07.3do size=.996184 movesize=.996184 soundclass=catwalkfall.snd
# DESC:  
# BBOX: -.024736 -.01943 -.097598 .024736 .01943 .102402
pipe1             _structure         model3d=p2xhf_1.3do size=.105347 movesize=.105347
# DESC:  
# BBOX: -.623026 -.283773 -.601571 .623026 .283773 .601571
t-16              _zwalkstruct       model3d=skyhop.3do size=.988847 movesize=.988847
# DESC:  
# BBOX: 0 0 0 0 0 0
+wcrate_exp       +crate_exp         damage=20 force=50 range=.15
# DESC:  
# BBOX: -12.90447 -31.23819 -7.19836 12.90447 31.23819 7.19836
dread             _zwalkstruct       model3d=dread.3do size=37.47168 movesize=37.47168
# DESC:  
# BBOX: -.098284 -.097923 -.031081 .097733 .098093 .031081
2x2elev_2         _walkstruct        model3d=e2x2_0.3do size=.192295 movesize=.192295 soundclass=sm_elev.snd
# DESC:  
# BBOX: -.151582 -.970063 -.066789 .151582 .970063 .066789
gantry1           _walkstruct        model3d=gan1.3do size=1.080419 movesize=1.080419
# DESC:  
# BBOX: -.409606 -.453233 -.120333 .409606 .453233 .120333
gantry2           _walkstruct        model3d=gan2.3do size=.553288 movesize=.553288
# DESC:  
# BBOX: -.059157 -.512454 -1.207433 .458773 .574397 .829692
gantry3           _zwalkstruct       model3d=gan3.3do size=1.305442 movesize=1.305442
# DESC:  
# BBOX: -.249628 -.011529 -.249901 .249628 .011529 .249901
gantrywindow      _walkstruct        model3d=gwndw.3do size=.321489 movesize=.321489
# DESC:  
# BBOX: -.650468 -1.749941 -.049964 .250468 1.749941 .049964
mdm10catwalk      _zwalkstruct       model3d=mdm10wlk.3do size=1.917435 movesize=1.917435
# DESC:  
# BBOX: -.038049 -.029403 -.124426 .038049 .029403 .124426
gantrypipe        _walkstruct        model3d=mdm10p.3do size=.176557 movesize=.176557
# DESC:  
# BBOX: -.003958 -.003824 -.004924 .003958 .003824 .004924
raillight         _ghostdecor        model3d=raillite.3do size=.055504 movesize=.055504
# DESC:  
# BBOX: -.038273 -.031091 -.073557 .038273 .031091 .073557
gantrypipe2       _walkstruct        model3d=mdm10p2.3do size=.13323 movesize=.13323
# DESC:  
# BBOX: -.249628 -.05511 -.249901 .249628 .05511 .249901
gantrydamwindow   _ghoststructure    model3d=gwndwdam.3do size=.323666 movesize=.323666
# DESC:  
# BBOX: -.544736 -.786304 -.229997 .544736 .786304 .229997
flyingcrow        _zwalkstruct       model3d=crow.3do size=.853084 movesize=.853084 puppet=cr.pup
# DESC:  
# BBOX: -.08322 -.046436 -.09211 .08322 .046436 .09211
emp_chair         _zwalkstruct       model3d=chair.3do size=.114176 movesize=.114176
# DESC:  
# BBOX: -.057535 -.059301 .000733 .057535 .059301 .156623
thrnrpan          _zwalkstruct       model3d=trpnl.3do size=.185498 movesize=.185498
# DESC:  
# BBOX: -.01264 -.299708 -.01129 .01264 .299708 .01129
visiblerod02      _structure         collide=1 model3d=rod02.3do size=.025 movesize=.025
# DESC:  
# BBOX: -.01264 -.049041 -.01129 .01264 .049041 .01129
inviscollide      _structure         collide=1 thingflags=0x10 model3d=41rod.3do size=.025 movesize=.025
# DESC:  
# BBOX: -.025263 -.019826 -.199259 .025263 .019826 .199259
thronecolumn      _structure         model3d=thrncolm.3do size=.22094 movesize=.22094
# DESC:  
# BBOX: -.143317 -.201706 -.078876 .143317 .201706 .078876
waterwell         _zwalkstruct       model3d=wtrw.3do size=.274523 movesize=.274523
# DESC:  
# BBOX: -.050887 -.075508 -.16702 .050887 .25464 .131736
crane             _decor             model3d=cran.3do size=.075 movesize=.075
# DESC:  
# BBOX: -.024736 -.01943 -.097343 .024736 .01943 .102387
pipe5             _structure         model3d=p2xhf_5.3do size=.105333 movesize=.105333
# DESC:  
# BBOX: -.042032 -.049632 .000337 .042032 .049632 .195296
chair3            _walkstruct        thingflags=0x48 model3d=chair3.3do size=.25442 movesize=.25442
# DESC:  
# BBOX: -.2 -.1 -.050028 .2 .1 .050359
table2            _walkstruct        thingflags=0x48 model3d=tbl2.3do size=.279207 movesize=.279207
# DESC:  
# BBOX: -.15 -.15 -.15 .15 .15 .15
woodcratebig      _walkstruct        thingflags=0x48 model3d=wcrt2.3do size=.309808 movesize=.309808
# DESC:  
# BBOX: -.016813 -.025219 -.025219 .016813 .025219 .025219
bottle01          _structure         model3d=bottle1.3do size=.089429 movesize=.089429
# DESC:  
# BBOX: -.012767 -.012147 -.023339 .012767 .012147 .023339
bottle05          _structure         model3d=bottle5.3do size=.077465 movesize=.077465
# DESC:  
# BBOX: -.024736 -.01943 -.097342 .024736 .01943 .102406
pipe3             _structure         model3d=p2xhf_3.3do size=.105352 movesize=.105352
# DESC:  
# BBOX: -.034039 -.029553 -.032378 .034039 .029553 .032378
vaporatordead     _walkstruct        model3d=v_deb1.3do size=.096979 movesize=.096979
# DESC:  
# BBOX: -.050387 -.043746 -.161575 .050387 .043746 .161575
vaporator         _structure         model3d=vap1.3do size=.21925 movesize=.21925
# DESC:  
# BBOX: -.01459 -.075274 -.015287 .01459 .075274 .015287
roofpipe          _ghostdecor        model3d=01rfd0.3do size=.077027 movesize=.077027
# DESC:  
# BBOX: -.031151 -.014189 -.030079 .031151 .014189 .030079
t-16model         _decor             model3d=skyhopmodel.3do size=.049442 movesize=.049442
# DESC:  
# BBOX: -.016776 -.034744 -.010952 .018316 .034538 .00282
ctf_redflag       _powerup           move=none model3d=ys.3do size=.05 movesize=.05 puppet=cr.pup typeflags=0x8
# DESC:  
# BBOX: -.102494 -.034109 -.028188 .102494 .034109 .028188
ysalamiribranch   _walkstruct        thingflags=0x800048 model3d=ysbranch.3do size=.159664 movesize=.159664
# DESC:  
# BBOX: -.094105 -.094105 -.094105 .094105 .094105 .094105
+redsphere        none               orient=(0/0/0) type=cog move=physics thingflags=0x80000000 model3d=redsphr.3do size=.144105 movesize=.144105 angvel=(60/60/60)
# DESC:  
# BBOX: 0 0 0 0 0 0
kyleduelingboss   _humanactor        move=path model3d=kkduel.3do puppet=mjdueling.pup typeflags=0x80200
# DESC:  
# BBOX: 0 0 0 0 0 0
kylebossactor     kyleactor          model3d=kkduel.3do puppet=mjdueling.pup typeflags=0x80200
# DESC:  
# BBOX: 0 0 0 0 0 0
kylebryaractor    kyleactor          model3d=kkbryar.3do
# DESC:  
# BBOX: 0 0 0 0 0 0
maraduelingboss   _humanactor        move=path model3d=mjduel.3do puppet=mjdueling.pup typeflags=0x80200
# DESC:  
# BBOX: 0 0 0 0 0 0
marabossactor     maraactor          model3d=mj.3do typeflags=0x80200
# DESC:  
# BBOX: 0 0 0 0 0 0
marablastekactor  maraactor          model3d=mjblastk.3do
# DESC:  
# BBOX: 0 0 0 0 0 0
rebcommander      _humanactor        move=path model3d=rm.3do puppet=io.pup
# DESC:  
# BBOX: 0 0 0 0 0 0
nbartender        nman2              cog=actor_bt.cog
# DESC:  
# BBOX: 0 0 0 0 0 0
_throwable        none               orient=(0/0/0) type=debris collide=1 move=physics movesize=.01 surfdrag=3 airdrag=1 mass=25 height=.011 physflags=0x404041 buoyancy=.5
# DESC:  
# BBOX: 0 0 0 0 0 0
throwcrate1       _throwable         model3d=crttoss1.3do size=.0364 movesize=.0364 height=.0365
# DESC:  
# BBOX: 0 0 0 0 0 0
throwshrapnel_1   _throwable         model3d=shrp_1.3do size=.045 movesize=.045 height=.0451
# DESC:  
# BBOX: 0 0 0 0 0 0
a                 none               model3d=a.3do
# DESC:  
# BBOX: -.1 -.01 -.15 .1 .01 .15
EBTPL             none               model3d=04d2x3.3do size=.04 movesize=.04 damage=0 aiclass=icreapdefault.ai cog=00_simplesparks.co
