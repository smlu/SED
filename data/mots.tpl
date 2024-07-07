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
# BBOX: -0.228837 -0.19266 -0.55048 0.228837 0.19266 0.55048
freezingclamp     _zwalkstruct       model3d=mdm02clmp.3do size=.606291 movesize=.606291
# DESC: 
# BBOX: -0.30607 -0.49657 -0.15875 0.30607 0.49657 0.15875
awing             _zwalkstruct       model3d=awing1.3do size=.607968 movesize=.607968
# DESC: 
# BBOX: 0 0 0 0 0 0
_walkstruct       _structure         thingflags=0x400048
# DESC: 
# BBOX: -0.098284 -0.097923 -0.031081 0.097733 0.098093 0.031081
2x2elev           _walkstruct        model3d=e2x2_0.3do size=.192295 movesize=.192295 soundclass=sm_elev.snd
# DESC: 
# BBOX: -0.650003 -0.037533 -0.499995 0.650003 0.037533 0.499995
m9_14d13x10       _structure         thingflags=0xc model3d=14d13x10.3do size=.820919 movesize=.820919 soundclass=lg_door.snd
# DESC: 
# BBOX: 0 0 0 0 0 0
ghost             none               orient=(0/0/0) type=ghost move=path
# DESC: 
# BBOX: -0.549776 -0.490931 -0.315187 0.549776 0.4752 0.320955
tiebomber         _zwalkstruct       thingflags=0x400440 model3d=tieb.3do size=.5 movesize=.5 puppet=cr.pup cog=ship_tiebomber.cog
# DESC: 
# BBOX: -0.15 -0.011875 -0.15 0.15 0.011875 0.15
3x3door_1         _walkstruct        model3d=d3x3_1.3do size=.262464 movesize=.262464 soundclass=sm_door.snd
# DESC: 
# BBOX: -0.2 -0.0125 -0.15 0.2 0.0125 0.15
4x3door_1         _structure         model3d=d4x3_1.3do size=.250312 movesize=.250312 soundclass=med_door.snd
# DESC: 
# BBOX: -0.099008 -0.068267 -0.014304 0.099008 0.129749 0.009657
2x2elev_0         _walkstruct        model3d=19e2x2.3do size=.213495 movesize=.213495 soundclass=sm_elev.snd
# DESC: 
# BBOX: 0 0 0 0 0 0
_actor            none               orient=(0/0/0) type=actor collide=1 move=physics thingflags=0x20000000 mass=150 physflags=0x4a4f maxrotvel=200 maxvel=1 health=40 maxhealth=40 maxrotthrust=180 jumpspeed=1.5 eyeoffset=(0/0/.037) minheadpitch=-80 maxheadpitch=80 lightoffset=(0/.07/.04) lightintensity=.8
# DESC: 
# BBOX: -0.03730128 -0.01387423 -0.1184614 0.03811364 0.03983956 0.06454442
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
# BBOX: -0.007791 -0.090587 -0.006258 0.007791 0.017998 0.006258
+bryarbolt        _weapon            thingflags=0x20000001 light=.4 model3d=bry0.3do size=.001 movesize=.001 soundclass=bry.snd creatething=+smflash maxrotvel=0 vel=(0/4/0) explode=+laserhit fleshhit=+laserfleshhit damage=30 mindamage=10 typeflags=0x20440d rate=15
# DESC: 
# BBOX: -0.005653 -0.113701 -0.004853 0.005653 0.02259 0.004853
+stlaser          +bryarbolt         model3d=str0.3do soundclass=stlaser.snd vel=(0/6/0) rate=10
# DESC: 
# BBOX: -0.005653 -0.113701 -0.004853 0.005653 0.02259 0.004853
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
# BBOX: -0.03618964 -0.01917877 -0.1247011 0.02409064 0.06467322 0.0434061
stormtroop2_m     _humanactor        thingflags=0x20000400 model3d=s2.3do puppet=st.pup soundclass=st2_s1.snd cog=actor_st_pnch.cog maxvel=.25 weapon=+elaser weapon2=+st_punch health=70 maxhealth=70 maxthrust=.9 maxrotthrust=90 typeflags=0x20001 fireoffset=(.0048/.05/.012) aiclass=st_def.ai
# DESC: 
# BBOX: -0.00375 -0.1 -0.052165 0.00375 0.1 0.052165
15_2x1rails       _walkstruct        model3d=15r2x1.3do size=.162851 movesize=.162851
# DESC: 
# BBOX: 0 0 0 0 0 0
+firecloud        +dustcloud         material=00explosion.mat range=.05 rate=64 maxthrust=40 count=64
# DESC: 
# BBOX: 0 0 0 0 0 0
+grenade_exp      _explosion         thingflags=0x1 light=.2 timer=.8 sprite=detx.spr soundclass=exp_det.snd creatething=+firecloud typeflags=0x17 damage=75 blasttime=.699 force=100 maxlight=.8 range=.45
# DESC: 
# BBOX: -0.012516 -0.012516 -0.012524 0.012516 0.012516 0.014655
+grenade2         _weapon            timer=3 model3d=det0.3do size=.014895 movesize=.014895 soundclass=det.snd surfdrag=3 airdrag=.8 mass=1 physflags=0x225 vel=(0/2/1.5) angvel=(90/45/90) buoyancy=.25 explode=+grenade_exp fleshhit=+grenade_exp damageclass=0x4 typeflags=0x40309
# DESC: 
# BBOX: -0.012516 -0.012516 -0.012524 0.012516 0.012516 0.014655
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
# BBOX: -0.026724 -0.026724 -0.044654 0.026724 0.026724 0.044654
shrapnel_1        _debris            model3d=shrp_1.3do size=.045 movesize=.045 damage=0
# DESC: 
# BBOX: -0.011171 -0.010389 -0.042003 0.011171 0.010389 0.042003
shrapnel_2        _debris            model3d=shrp_2.3do size=.04 movesize=.04 damage=0
# DESC: 
# BBOX: -0.015988 -0.015443 -0.026188 0.015988 0.015443 0.026188
shrapnel_3        _debris            model3d=shrp_3.3do size=.028 movesize=.028 damage=0
# DESC: 
# BBOX: -0.016213 -0.014647 -0.020855 0.016213 0.014647 0.020855
shrapnel_4        _debris            model3d=shrp_4.3do size=.026 movesize=.026 damage=0
# DESC: 
# BBOX: 0 0 0 0 0 0
+raildet_exp      _explosion         thingflags=0x1 light=.2 timer=.5 sprite=rldx.spr soundclass=exp_raildet.snd typeflags=0x17 damage=110 blasttime=.699 force=300 maxlight=.8 range=.5 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_4
# DESC: 
# BBOX: 0 0 0 0 0 0
+lgflash          _explosion         thingflags=0x1 light=.3 timer=.4 typeflags=0x12 blasttime=.2 maxlight=1
# DESC: 
# BBOX: -0.01058 -0.013704 -0.006548 0.010593 0.009939 0.012329
+raildet2         +grenade1          thingflags=0x20000400 timer=2.5 model3d=rld0.3do size=.003 movesize=.003 puppet=ra.pup soundclass=rail.snd creatething=+lgflash cog=00_smoketrail.cog airdrag=0 height=.003 physflags=0x200 maxrotvel=20 vel=(0/2.5/0) angvel=(0/0/90) explode=+raildet_exp fleshhit=+raildet_exp damage=5 typeflags=0x240b81
# DESC: 
# BBOX: 0 0 0 0 0 0
+eraildet_exp     +raildet_exp       damage=50 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_4
# DESC: 
# BBOX: -0.01058 -0.013704 -0.006548 0.010593 0.009939 0.012329
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
# BBOX: -0.03618964 -0.01917877 -0.1247011 0.02409064 0.08688994 0.0434061
stormtroop4_m     _humanactor        thingflags=0x20000400 model3d=s4.3do puppet=s2.pup soundclass=st4_s1.snd cog=actor_s3.cog maxvel=.25 weapon=+eraildet weapon2=+smack health=70 maxhealth=70 maxthrust=.9 maxrotthrust=90 typeflags=0x20001 fireoffset=(.014/.0532/.013) aiclass=straildefault.ai
# DESC: 
# BBOX: -0.024596 -0.024511 -0.064105 0.024596 0.024511 0.064105
console1          _walkstruct        model3d=con1.3do size=.072905 movesize=.072905
# DESC: 
# BBOX: -0.200159 -0.024886 -0.057805 0.200159 0.024886 0.057805
console5          _walkstruct        model3d=con5.3do size=.20982 movesize=.20982
# DESC: 
# BBOX: -0.03618964 -0.01917877 -0.1247011 0.02409064 0.06467322 0.0434061
st_11_m           _humanactor        thingflags=0x20000400 model3d=s2.3do puppet=st.pup soundclass=st_s1.snd cog=actor_st_m.cog maxvel=.25 weapon=+elaser weapon2=+st_punch health=60 maxhealth=60 maxthrust=1 maxrotthrust=90 typeflags=0x20001 fireoffset=(.0048/.05/.012) aiclass=st_def_11.ai
# DESC: 
# BBOX: -0.39751 -0.94488 -0.28956 0.39751 0.94488 0.28956
transport         _zwalkstruct       model3d=trnsprt1.3do size=1.098054 movesize=1.098054
# DESC: 
# BBOX: -0.2 -0.0125 -0.15 0.2 0.0125 0.25
4x4door           _walkstruct        model3d=d4x4.3do size=.3204 movesize=.3204 soundclass=med_door.snd
# DESC: 
# BBOX: 0 0 0 0 0 0
_powerup          none               orient=(0/0/0) type=item collide=1 move=physics size=.1 movesize=.01 surfdrag=3 airdrag=1 mass=10 height=.05 physflags=0x400000 typeflags=0x1 respawn=30
# DESC: 
# BBOX: -0.010023 -0.016964 -0.033841 0.010023 0.016964 0.033841
keyblue           _powerup           thingflags=0x400 model3d=b-key.3do cog=pow_keyblue.cog
# DESC: 
# BBOX: -0.148 -0.148 -0.013055 0.148 0.148 0.013695
3x3elev           _walkstruct        model3d=e3x3.3do size=.209751 movesize=.209751 soundclass=sm_elev.snd
# DESC: 
# BBOX: -0.016314 -0.219705 -0.118596 0.016314 0.223171 0.249539
fan0              none               orient=(0/0/0) type=cog collide=3 move=physics model3d=fan0.3do size=.299843 movesize=.299843 physflags=0x200 maxrotvel=400 angvel=(180/0/0)
# DESC: 
# BBOX: -0.817281 -1.132091 -0.454926 0.815909 1.276639 1.852229
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
# BBOX: -0.13015 -0.091058 -0.100298 0.13015 0.091058 0.100298
roofvent2         _walkstruct        model3d=01rfd2.3do size=.226595 movesize=.226595
# DESC: 
# BBOX: -0.040113 -0.035949 -0.021618 0.040113 0.035949 0.021618
shieldrecharge    _powerup           thingflags=0x400 model3d=shld.3do cog=pow_shields.cog
# DESC: 
# BBOX: -0.027845 -0.025398 -0.02252 0.027845 0.025398 0.024913
healthpack        _powerup           thingflags=0x408 model3d=hepk.3do cog=pow_health.cog
# DESC: 
# BBOX: 0 0 0 0 0 0
_cpowerup         _powerup           typeflags=0x11
# DESC: 
# BBOX: -0.022216 -0.005309 -0.024793 0.022216 0.005309 0.024793
singleseqcharge   _cpowerup          thingflags=0x400 model3d=seqp_1.3do cog=pow_single_sequencer_m.cog
# DESC: 
# BBOX: -0.01636 -0.014643 -0.011603 0.017727 0.017747 0.016258
railcharges       _cpowerup          thingflags=0x400 model3d=rcrg.3do cog=pow_railcharges_m.cog
# DESC: 
# BBOX: -0.015415 -0.017797 -0.028888 0.015415 0.017797 0.028888
bactatank         _powerup           thingflags=0x400 model3d=bact.3do cog=pow_bacta.cog respawn=60
# DESC: 
# BBOX: -0.007791 -0.090587 -0.006258 0.007791 0.017998 0.006258
+ebolt            +bryarbolt         model3d=bry1.3do vel=(0/3.5/0) damage=10 mindamage=5
# DESC: 
# BBOX: -0.034444 -0.015005 -0.120724 0.034894 0.020299 0.062428
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
# BBOX: -0.005771 -0.015021 -0.00466 0.005771 0.003935 0.00466
+repeaterball     _weapon            thingflags=0x20000001 light=.3 model3d=rpt0.3do size=.005 movesize=.005 soundclass=rep.snd creatething=+smflash vel=(0/6/0) explode=+repeaterhit fleshhit=+rptfleshhit damage=20 mindamage=5 typeflags=0x440d rate=20
# DESC: 
# BBOX: -0.005771 -0.015021 -0.00466 0.005771 0.003935 0.00466
+eball            +repeaterball      damage=8 mindamage=4
# DESC: 
# BBOX: -0.03618964 -0.01917877 -0.1247011 0.02409064 0.06395282 0.0434061
stormtroop3_m     _humanactor        thingflags=0x20000400 model3d=s3.3do puppet=s3.pup soundclass=st3_s1.snd cog=actor_i2_pnch.cog maxvel=.25 weapon=+eball weapon2=+st_punch health=70 maxhealth=70 maxthrust=.9 maxrotthrust=90 typeflags=0x20001 fireoffset=(.0098/.057/.01) aiclass=streapdefault.ai
# DESC: 
# BBOX: -0.034444 -0.015005 -0.120724 0.034894 0.020299 0.062428
scouttrooper      _humanactor        thingflags=0x20000400 model3d=sc.3do puppet=st.pup soundclass=st.snd cog=actor_sc_pnch.cog maxvel=.25 weapon=+ebolt weapon2=+st_punch health=80 maxhealth=80 maxthrust=1 maxrotthrust=90 typeflags=0x20001 fireoffset=(.0048/.05/.012) aiclass=sc_def.ai
# DESC: 
# BBOX: -0.020824 -0.043225 0.000892 0.011798 0.043225 0.024826
detonator         _cpowerup          thingflags=0x400 model3d=detp.3do cog=pow_thermal_m.cog
# DESC: 
# BBOX: -0.009653 -0.02399 -0.017867 0.009653 0.02399 0.017867
powercell         _powerup           thingflags=0x400 model3d=pcel.3do cog=pow_power_m.cog
# DESC: 
# BBOX: -0.021089 -0.023005 -0.035347 0.016307 0.023005 0.035347
revivepack        _powerup           thingflags=0x400 model3d=packrev.3do cog=pow_revive.cog respawn=60
# DESC: 
# BBOX: -0.005653 -0.113701 -0.004853 0.005653 0.02259 0.004853
+lasercannon      +stlaser           light=.5 size=.015 movesize=.015 vel=(0/4/0) fleshhit=+laserhit damage=40
# DESC: 
# BBOX: -0.1616239 -0.2391249 -0.45412 0.1605707 0.2508638 0.2235963
atst_newdrvr      _humanactor        thingflags=0x4 model3d=at.3do size=.3 movesize=.3 puppet=atst.pup soundclass=atst.snd cog=actor_atstdriver.cog mass=0 maxvel=.4 weapon=+lasercannon health=400 maxhealth=400 maxthrust=.3 maxrotthrust=10 typeflags=0x100 fireoffset=(0/.2/0) aiclass=atstdefault.ai
# DESC: 
# BBOX: 0 0 0 0 0 0
_droidactor       _humanactor        physflags=0x2004a4f typeflags=0x88140
# DESC: 
# BBOX: -0.033545 -0.020533 -0.064529 0.03321 0.020809 0.02539
r5                _droidactor        thingflags=0x20000400 model3d=r5.3do size=.036 movesize=.036 puppet=r5.pup soundclass=r2.snd cog=actor_r5.cog maxvel=.4 health=50 maxhealth=50 maxthrust=.4 maxrotthrust=50 typeflags=0x180140 aiclass=drdefault.ai
# DESC: 
# BBOX: -0.1 -0.1 -0.099985 0.1 0.1 0.099985
crate4_3          _walkstruct        model3d=crt4-3.3do size=.173196 movesize=.173196
# DESC: 
# BBOX: -0.027923 -0.032221 -0.032264 0.027923 0.032221 0.032264
crate6_1          _walkstruct        thingflags=0x400448 model3d=crt6-1.3do size=.045598 movesize=.045598 cog=actor_cs.cog
# DESC: 
# BBOX: -0.037893 -0.021815 -0.12043 0.03746 0.022321 0.066407
rcommando         _humanactor        thingflags=0x20000400 model3d=rc.3do size=.06595 movesize=.06595 puppet=ic.pup soundclass=rc.snd cog=actor_rc.cog maxvel=.3 weapon=+elaser weapon2=+st_punch health=90 maxhealth=90 maxthrust=1.1 maxrotthrust=90 typeflags=0x120001 fireoffset=(.014/.059/.031) aiclass=rc_def.ai
# DESC: 
# BBOX: 0 0 0 0 0 0
_flyactor         _actor             airdrag=2 physflags=0x2007606 maxvel=.8 maxthrust=.5
# DESC: 
# BBOX: 0 0 0 0 0 0
+sentry_exp       +grenade_exp       damage=5 force=50
# DESC: 
# BBOX: -0.037102 -0.076502 -0.041518 0.037102 0.046066 0.03417
robotsentry       _flyactor          thingflags=0x20000400 model3d=rs.3do size=.055 movesize=.055 soundclass=rs.snd cog=actor_sd.cog mass=30 physflags=0x2006a06 maxvel=.6 weapon=+bryarbolt health=60 maxhealth=60 maxthrust=.8 maxrotthrust=80 typeflags=0x2008120 fireoffset=(-.0073/.0393/0) explode=+sentry_exp aiclass=sddefault.ai
# DESC: 
# BBOX: -0.03618964 -0.01917877 -0.1247011 0.02409064 0.06467322 0.0434061
stormtroop2       _humanactor        thingflags=0x20000400 model3d=s2.3do puppet=st.pup soundclass=st2.snd cog=actor_st_pnch.cog maxvel=.25 weapon=+elaser weapon2=+st_punch health=70 maxhealth=70 maxthrust=.9 maxrotthrust=90 typeflags=0x20001 fireoffset=(.0048/.05/.012) aiclass=st_def.ai
# DESC: 
# BBOX: -0.2 -0.0125 -0.2 0.2 0.0125 0.2
06expwall1a       _walkstruct        thingflags=0x48 model3d=06wexp1a.3do size=.333119 movesize=.333119
# DESC: 
# BBOX: -0.016822 -0.05557 -0.026043 0.007966 0.058 0.014355
strifle           _powerup           thingflags=0x400 model3d=strp.3do cog=pow_strifle_m.cog
# DESC: 
# BBOX: -0.03092 -0.030162 -0.030457 0.03092 0.030162 0.030457
darkside          _powerup           thingflags=0x400 model3d=mana1.3do cog=pow_darkside.cog respawn=60
# DESC: 
# BBOX: -0.016734 -0.016734 -0.026596 0.016734 0.016734 0.026596
powerboost        _powerup           thingflags=0x400 model3d=boost.3do cog=pow_powerboost.cog respawn=60
# DESC: 
# BBOX: -0.586164 -1.234832 -0.489865 0.586164 1.305274 1.427284
liftershuttle     _zwalkstruct       model3d=ltshut.3do size=1.5 movesize=1.5 puppet=cr.pup
# DESC: 
# BBOX: -0.018131 -0.009419 -0.000113 0.037525 0.027081 0.049637
stbp              _powerup           thingflags=0x400 model3d=stpack.3do cog=pow_stbkpk.cog
# DESC: 
# BBOX: -0.037458 -0.014968 -0.120277 0.037328 0.026217 0.066384
rofficer          _humanactor        thingflags=0x20000400 model3d=ro.3do size=.0667 movesize=.0667 puppet=io.pup soundclass=rc.snd cog=actor_ro.cog maxvel=.3 weapon=+ebolt weapon2=+st_punch health=30 maxthrust=.9 maxrotthrust=80 typeflags=0x120001 fireoffset=(.05/.065/.014) aiclass=ro_def.ai
# DESC: 
# BBOX: -0.08409196 -0.08637844 -0.1312981 0.04919052 0.102691 -0.08340979
deadrebelsoldier1 _ghostdecor        model3d=rc-dead1.3do size=.173969 movesize=.173969
# DESC: 
# BBOX: -0.03470412 -0.08880202 -0.1689068 0.08018033 0.0978179 -0.08035785
deadrebelsoldier2 _ghostdecor        model3d=rc-dead2.3do size=.173969 movesize=.173969
# DESC: 
# BBOX: -0.03529926 -0.02424149 -0.207447 0.07468747 0.04653724 -0.07682037
deadrebelofficer1 _ghostdecor        model3d=ro-dead1.3do size=.174602 movesize=.174602
# DESC: 
# BBOX: -0.2 -0.0125 -0.2 0.2 0.0125 0.2
06expwall1b       _walkstruct        thingflags=0x48 model3d=06wexp1b.3do size=.333119 movesize=.333119
# DESC: 
# BBOX: -0.349725 -0.412297 -0.47738 0.348866 0.379966 0.482264
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
# BBOX: -0.020392 -0.135722 -0.016856 0.021073 0.013215 0.015722
+ewebround        +bryarbolt         model3d=eweb0.3do soundclass=stlaser.snd vel=(0/10/0) damage=100 rate=10
# DESC: 
# BBOX: -0.005653 -0.113701 -0.004853 0.005653 0.02259 0.004853
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
# BBOX: -0.012516 -0.012516 -0.012524 0.012516 0.012516 0.014655
+flashbomb2       _weapon            timer=3 model3d=fls0.3do size=.014895 movesize=.014895 soundclass=det.snd surfdrag=3 airdrag=.8 mass=1 physflags=0x225 vel=(0/2/1.5) angvel=(90/45/90) buoyancy=.25 explode=+flash_exp fleshhit=+flash_exp damageclass=0x4 typeflags=0x40309
# DESC: 
# BBOX: -0.012516 -0.012516 -0.012524 0.012516 0.012516 0.014655
+flashbomb1       +flashbomb2        timer=10 typeflags=0x4030d
# DESC: 
# BBOX: 0 0 0 0 0 0
+crossbowhit      +laserhit          sprite=bowx2.spr soundclass=exp_bowhit.snd
# DESC: 
# BBOX: -0.020392 -0.135722 -0.016856 0.021073 0.013215 0.015722
+crossbowbolt     _weapon            thingflags=0x20000001 light=.5 model3d=bow0.3do size=.005 movesize=.005 soundclass=bow.snd creatething=+lgflash vel=(0/5/0) angvel=(0/0/120) explode=+crossbowhit fleshhit=+crossbowhit damage=60 mindamage=20 damageclass=0x4 typeflags=0x20440d rate=10
# DESC: 
# BBOX: 0 0 0 0 0 0
+crossbowhit2     _explosion         thingflags=0x1 light=.2 timer=.5 sprite=bowx.spr typeflags=0x33 blasttime=.3 maxlight=.4
# DESC: 
# BBOX: -0.014666 -0.12215 -0.016699 0.015346 0.011894 0.015879
+crossbowbolt2    +crossbowbolt      model3d=bow1.3do soundclass=none creatething=none explode=+crossbowhit2 fleshhit=+crossbowhit2 damage=40 typeflags=0x440d
# DESC: 
# BBOX: -0.020392 -0.135722 -0.016856 0.021073 0.013215 0.015722
+crossbowbolt3    +crossbowbolt      typeflags=0x28440d
# DESC: 
# BBOX: 0 0 0 0 0 0
+firecloud2       +firecloud         elementsize=.0125
# DESC: 
# BBOX: 0 0 0 0 0 0
_debris2          _debris            explode=+smflash
# DESC: 
# BBOX: -0.026724 -0.026724 -0.044654 0.026724 0.026724 0.044654
shrapnel2_1       _debris2           model3d=shrp_1.3do size=.045 movesize=.045
# DESC: 
# BBOX: -0.011171 -0.010389 -0.042003 0.011171 0.010389 0.042003
shrapnel2_2       _debris2           model3d=shrp_2.3do size=.04 movesize=.04
# DESC: 
# BBOX: -0.015988 -0.015443 -0.026188 0.015988 0.015443 0.026188
shrapnel2_3       _debris2           model3d=shrp_3.3do size=.028 movesize=.028
# DESC: 
# BBOX: -0.016213 -0.014647 -0.020855 0.016213 0.014647 0.020855
shrapnel2_4       _debris2           model3d=shrp_4.3do size=.026 movesize=.026
# DESC: 
# BBOX: 0 0 0 0 0 0
+sequencer_exp    _explosion         thingflags=0x1 light=.2 timer=.8 sprite=seqx2.spr soundclass=exp_med.snd creatething=+firecloud2 typeflags=0x17 damage=100 blasttime=.699 force=300 maxlight=.8 range=.6 debris=shrapnel2_1 debris=shrapnel2_2 debris=shrapnel2_3 debris=shrapnel2_4
# DESC: 
# BBOX: -0.022216 -0.023897 -0.005309 0.022216 0.023897 0.005309
+seqchrg          +grenade2          timer=1 model3d=seq0.3do size=.01 movesize=.01 soundclass=seq.snd surfdrag=5 airdrag=1 physflags=0x29d vel=(0/.1/0) angvel=(0/0/0) buoyancy=.15 explode=+sequencer_exp fleshhit=+sequencer_exp typeflags=0x40380
# DESC: 
# BBOX: -0.022216 -0.023897 -0.005309 0.022216 0.023897 0.005309
+seqchrg2         +seqchrg           thingflags=0x20000400 timer=60 cog=class_sequencer.cog
# DESC: 
# BBOX: -0.022216 -0.023897 -0.005309 0.022216 0.023897 0.005309
+seqchrg3         +seqchrg           timer=300
# DESC: 
# BBOX: -0.022216 -0.023897 -0.005309 0.022216 0.023897 0.005309
+seqchrg4         +seqchrg           thingflags=0x20000400 timer=300 cog=class_sequencer.cog
# DESC: 
# BBOX: 0 0 0 0 0 0
+raildet_exp2     +raildet_exp       debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_4
# DESC: 
# BBOX: -0.01058 -0.013704 -0.006548 0.010593 0.009939 0.012329
+raildet          +raildet2          timer=10 explode=+raildet_exp2 fleshhit=+raildet_exp2 damage=20 typeflags=0x24020d
# DESC: 
# BBOX: -0.01058 -0.013704 -0.006548 0.010593 0.009939 0.012329
+lhraildet        +raildet           timer=3 model3d=skr0.3do cog=00_hraildet.cog physflags=0x2200 angvel=(0/0/0) explode=+raildet_exp fleshhit=+raildet_exp typeflags=0x240b81
# DESC: 
# BBOX: 0 0 0 0 0 0
+hraildet_exp     none               orient=(0/0/0) type=cog thingflags=0x400 cog=exp_hrail.cog
# DESC: 
# BBOX: -0.01058 -0.013704 -0.006548 0.010593 0.009939 0.012329
+hraildet         +raildet           model3d=skr0.3do physflags=0x2200 angvel=(0/0/0) explode=+hraildet_exp fleshhit=+hraildet_exp
# DESC: 
# BBOX: -0.01058 -0.013704 -0.006548 0.010593 0.009939 0.012329
+hraildet2        +raildet2          model3d=skr0.3do physflags=0x2200 angvel=(0/0/0) explode=+hraildet_exp fleshhit=+hraildet_exp
# DESC: 
# BBOX: -0.01058 -0.013704 -0.006548 0.010593 0.009939 0.012329
+nchraildet       +hraildet          collide=0
# DESC: 
# BBOX: -0.01058 -0.013704 -0.006548 0.010593 0.009939 0.012329
+nchraildet2      +hraildet2         collide=0
# DESC: 
# BBOX: -0.01058 -0.013704 -0.006548 0.010593 0.009939 0.012329
+sraildet         +raildet           vel=(0/1/0) angvel=(0/90/90)
# DESC: 
# BBOX: 0 0 0 0 0 0
+conccloud        +dustcloud         rate=512 maxthrust=100 count=256 minsize=.012 pitchrange=5
# DESC: 
# BBOX: 0 0 0 0 0 0
+conc_exp         _explosion         thingflags=0x1 light=.3 timer=1 sprite=conx.spr soundclass=exp_conc.snd creatething=+conccloud typeflags=0x17 damage=80 blasttime=1 force=200 maxlight=.8 range=.8
# DESC: 
# BBOX: -0.01341 -0.149802 -0.01341 0.01341 0.015603 0.01341
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
# BBOX: -0.007659 -7E-6 -0.008551 0.008867 0.076357 0.007405
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
# BBOX: -0.002761 -0.017436 -0.002868 0.002754 0.16643 0.003496
+force_saber      _weapon            thingflags=0x20000001 light=.5 timer=2 model3d=sab0.3do size=.05 movesize=.05 vel=(0/1.1/0) angvel=(0/-400/0) explode=+fsaberexplode fleshhit=+fsaberflesh damage=80 damageclass=0x10 typeflags=0x10d
# DESC: 
# BBOX: -0.036175 -0.01856 -0.009087 0.028413 0.153538 0.003912
+fsab_green       +force_saber       model3d=sab0gree.3do
# DESC: 
# BBOX: -0.036175 -0.01856 -0.009087 0.028413 0.153538 0.003912
+fsab_orange      +force_saber       model3d=sab0oran.3do
# DESC: 
# BBOX: -0.036175 -0.01856 -0.009087 0.028413 0.153538 0.003912
+fsab_red         +force_saber       model3d=sab0red.3do
# DESC: 
# BBOX: -0.036175 -0.01856 -0.009087 0.028413 0.153538 0.003912
+fsab_dred        +force_saber       model3d=sab0dred.3do
# DESC: 
# BBOX: -0.036175 -0.01856 -0.009087 0.028413 0.153538 0.003912
+fsab_blue        +force_saber       model3d=sab0blue.3do
# DESC: 
# BBOX: -0.036175 -0.01856 -0.009087 0.028413 0.153538 0.003912
+fsab_purple      +force_saber       model3d=sab0purp.3do
# DESC: 
# BBOX: -0.036175 -0.01856 -0.009087 0.028413 0.153538 0.003912
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
# BBOX: -0.021089 -0.023005 -0.035347 0.016307 0.023005 0.035347
+backpack         _droppowerup       thingflags=0x400 model3d=bpck.3do cog=pow_backpack_m.cog height=.036337 typeflags=0x4
# DESC: 
# BBOX: -0.003197 -0.019383 -0.01078 0.003197 0.019383 0.01078
blastechpistol    _powerup           thingflags=0x400 model3d=blsp.3do cog=pow_blastech_m.cog
# DESC: BryarPistol
# BBOX: -0.003197 -0.019383 -0.01078 0.003197 0.019383 0.01078
bryarpistol _powerup thingflags=0x400 model3d=bryp.3do cog=pow_bryar_m.cog
# DESC: 
# BBOX: -0.017868 -0.016799 -0.017934 0.017868 0.016799 0.017934
singledetonator   _cpowerup          thingflags=0x400 model3d=detp_1.3do cog=pow_single_thermal_m.cog
# DESC: 
# BBOX: -0.017868 -0.016799 -0.017934 0.017868 0.016799 0.017934
singleflashbomb   _cpowerup          thingflags=0x400 model3d=flsp_1.3do cog=pow_single_flashbomb_m.cog
# DESC: 
# BBOX: -0.034888 -0.055377 0.000124 0.034343 0.059997 0.031925
crossbow          _powerup           thingflags=0x400 model3d=bowp.3do cog=pow_crossbow_m.cog
# DESC: 
# BBOX: -0.00548 -0.051379 -0.021767 0.00548 0.039969 0.012035
repeatergun       _powerup           thingflags=0x400 model3d=rptp.3do cog=pow_repeater_m.cog
# DESC: 
# BBOX: -0.018484 -0.070714 -0.030025 0.019883 0.081826 0.013227
railgun           _powerup           thingflags=0x400 model3d=rldp.3do cog=pow_railgun_m.cog respawn=60
# DESC: 
# BBOX: -0.030347 -0.072704 -0.024787 0.009211 0.072704 0.024341
concrifle         _powerup           thingflags=0x400 model3d=conp.3do cog=pow_concrifle_m.cog respawn=60
# DESC: 
# BBOX: 0 0 0 0 0 0
+cpel_exp         none               orient=(0/0/0) type=cog thingflags=0x400 cog=exp_cpel.cog
# DESC: 
# BBOX: -0.003123 -0.004817 -0.002719 0.003123 0.004817 0.002719
+carbpellet       _weapon            thingflags=0x20000400 timer=3 model3d=cbn0.3do size=.005939 movesize=.005939 soundclass=cpel.snd cog=00_smoketrail.cog surfdrag=3 airdrag=.8 mass=1 physflags=0x225 vel=(0/10/0) angvel=(90/45/90) buoyancy=.25 explode=+cpel_exp fleshhit=+cpel_exp damageclass=0x4 typeflags=0x440309
# DESC: 
# BBOX: 0 0 0 0 0 0
+carbchunks       none               orient=(0/0/0) type=particle move=physics timer=.2 typeflags=0x27 material=00explosion.mat range=.2 rate=48 maxthrust=16 elementsize=.015 count=96
# DESC: 
# BBOX: -0.058497 -0.057454 -0.12002 0.056291 0.06851 0.06395201
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
# BBOX: -0.035332 -0.013185 -0.120103 0.035579 0.016413 0.064197
+projection       none               orient=(0/0/0) type=cog thingflags=0x400 timer=35 model3d=mj.3do size=.06 movesize=.06 puppet=ky.pup cog=thing_proj_image.cog
# DESC: 
# BBOX: -0.005771 -0.015021 -0.00466 0.005771 0.003935 0.00466
+farsight         none               orient=(0/0/0) type=cog collide=1 move=physics timer=25 model3d=farsight.3do size=.05 movesize=.1 airdrag=.5 mass=.05 physflags=0xa00 maxvel=.5
# DESC: 
# BBOX: -0.086772 -0.114302 -0.025049 0.086772 0.142704 0.149408
+eweb             _actor             collide=0 thingflags=0x20000400 model3d=eweb.3do size=.197239 movesize=.197239 puppet=eweb.pup surfdrag=1000 airdrag=1000 mass=0 maxvel=0 health=10 maxrotthrust=0 typeflags=0x20
# DESC: 
# BBOX: -0.035332 -0.013185 -0.120103 0.035579 0.016413 0.064197
+ewebrider        _ghostdecor        model3d=mj.3do size=.17358 movesize=.17358
# DESC: 
# BBOX: 0 0 0 0 0 0
+carbsparks       +sparks            timer=2 vel=(0/1/0) material=00carbonite.mat range=.07 count=96 pitchrange=90 yawrange=90
# DESC: 
# BBOX: -0.003197 -0.019383 -0.01078 0.003197 0.019383 0.01078
+dblastechpistol  _powerup           model3d=blsp.3do
# DESC: 
# BBOX: -0.012516 -0.012516 -0.012524 0.012516 0.012516 0.014655
+dudgrenade       +grenade2          explode=singledetonator typeflags=0x80120
# DESC: 
# BBOX: -0.012516 -0.012516 -0.012524 0.012516 0.012516 0.014655
+dudflash         +flashbomb2        explode=singleflashbomb typeflags=0x80120
# DESC: 
# BBOX: -0.01058 -0.013704 -0.006548 0.010593 0.009939 0.012329
+dudrail          +grenade1          timer=3 model3d=rld0.3do airdrag=1.8 angvel=(0/0/0) explode=+sparks typeflags=0x80120
# DESC: 
# BBOX: -0.022216 -0.023897 -0.005309 0.022216 0.023897 0.005309
+nospinseqcharge  singleseqcharge    model3d=seq0.3do
# DESC: 
# BBOX: -0.022216 -0.023897 -0.005309 0.022216 0.023897 0.005309
+dudseqchrg       +seqchrg           explode=+nospinseqcharge
# DESC: 
# BBOX: -0.01058 -0.013704 -0.006548 0.010593 0.009939 0.012329
+dudseek          +dudrail           model3d=skr0.3do
# DESC: 
# BBOX: 0 0 0 0 0 0
+force_clightn    +force_lightning   vel=(0/5/0) damage=0
# DESC: 
# BBOX: -0.058497 -0.057454 -0.12002 0.056291 0.06851 -0.064426
+carbonitestem    _zwalkstruct       model3d=crbnstem.3do size=.19317 movesize=.19317
# DESC: 
# BBOX: 0 0 0 0 0 0
+carbodeb_exp     none               orient=(0/0/0) type=particle move=physics timer=.3 mass=.5 physflags=0x60000 typeflags=0x3f material=00gsmoke.mat range=.03 rate=128 maxthrust=5 elementsize=.005 count=32
# DESC: 
# BBOX: 0 0 0 0 0 0
_carbodebris      none               orient=(0/0/0) type=weapon collide=1 move=physics timer=3 airdrag=3 mass=30 physflags=0x201 angvel=(5/10/0) explode=+carbodeb_exp damage=5 damageclass=0x1 typeflags=0x10c
# DESC: 
# BBOX: -0.01358 -0.014556 -0.038687 0.01358 0.014556 0.038687
+carbodebris_0    _carbodebris       model3d=crbn_0.3do size=.04 movesize=.04
# DESC: 
# BBOX: -0.010249 -0.011301 -0.016265 0.010249 0.011301 0.016265
+carbodebris_1    _carbodebris       model3d=crbn_1.3do size=.017 movesize=.017
# DESC: 
# BBOX: -0.02119 -0.014471 -0.024481 0.02119 0.014471 0.024481
+carbodebris_2    _carbodebris       model3d=crbn_2.3do size=.031 movesize=.031
# DESC: 
# BBOX: -0.00871 -0.015466 -0.02829 0.00871 0.015466 0.02829
+carbodebris_3    _carbodebris       model3d=crbn_3.3do size=.032 movesize=.032
# DESC: 
# BBOX: -0.003675 -0.006301 -0.001462 0.003675 0.006301 0.001462
+carbodebris_4    _carbodebris       model3d=crbn_4.3do size=.007 movesize=.007
# DESC: 
# BBOX: -0.549776 -0.490931 -0.315187 0.549776 0.4752 0.320955
tiebomber_crsh    _zwalkstruct       collide=1 move=physics model3d=tieb.3do size=.5 movesize=.5 puppet=cr.pup mass=3000 physflags=0x201
# DESC: 
# BBOX: -0.003197 -0.019383 -0.01078 0.003197 0.019383 0.01078
+fpbryarpistol    bryarpistol        collide=0 timer=4 typeflags=0x0 respawn=0
# DESC: 
# BBOX: -0.016822 -0.05557 -0.026043 0.007966 0.058 0.014355
+fpstrifle        strifle            collide=0 timer=4 typeflags=0x0 respawn=0
# DESC: 
# BBOX: -0.017868 -0.016799 -0.017934 0.017868 0.016799 0.017934
+fpdetolive       _powerup           collide=0 thingflags=0x400 timer=4 model3d=detp_1.3do cog=pow_livethermal.cog typeflags=0x0 respawn=0
# DESC: 
# BBOX: -0.00548 -0.051379 -0.021767 0.00548 0.039969 0.012035
+fprepeatergun    repeatergun        collide=0 timer=4 typeflags=0x0 respawn=0
# DESC: 
# BBOX: -0.018484 -0.070714 -0.030025 0.019883 0.081826 0.013227
+fprailgun        railgun            collide=0 timer=4 typeflags=0x0 respawn=0
# DESC: 
# BBOX: -0.030347 -0.072704 -0.024787 0.009211 0.072704 0.024341
+fpconcrifle      concrifle          collide=0 timer=4 typeflags=0x0 respawn=0
# DESC: 
# BBOX: -0.008988 -0.030139 -0.030139 0.008988 0.030139 0.030139
lightsaber        _powerup           thingflags=0x400 model3d=sabp.3do cog=pow_saber_m.cog
# DESC: 
# BBOX: -0.008988 -0.030139 -0.030139 0.008988 0.030139 0.030139
+fplightsaber     lightsaber         collide=0 timer=4 typeflags=0x0 respawn=0
# DESC: 
# BBOX: -0.003197 -0.019383 -0.01078 0.003197 0.019383 0.01078
+fpblastech       blastechpistol     collide=0 timer=4 typeflags=0x0 respawn=0
# DESC: 
# BBOX: -0.009575 -0.037873 -0.008367 0.009575 0.037873 0.008367
scope             _powerup           thingflags=0x400 model3d=scpe.3do cog=pow_scope_m.cog
# DESC: 
# BBOX: -0.009575 -0.037873 -0.008367 0.009575 0.037873 0.008367
+fpscope          scope              collide=0 timer=4 typeflags=0x0 respawn=0
# DESC: 
# BBOX: -0.034888 -0.055377 0.000124 0.034343 0.059997 0.031925
+fpcrossbow       crossbow           collide=0 timer=4 typeflags=0x0 respawn=0
# DESC: 
# BBOX: -0.018484 -0.070714 -0.030025 0.019883 0.081826 0.013227
railseek          _powerup           thingflags=0x400 model3d=skrp.3do cog=pow_railseek_m.cog respawn=60
# DESC: 
# BBOX: -0.018484 -0.070714 -0.030025 0.019883 0.081826 0.013227
+fprailseek       railseek           collide=0 timer=4 typeflags=0x0 respawn=0
# DESC: 
# BBOX: -0.00415 -0.047867 -0.01666 0.00415 0.035584 0.00712
carbgun           _powerup           thingflags=0x400 model3d=cbnp.3do cog=pow_carbgun_m.cog
# DESC: 
# BBOX: -0.00415 -0.047867 -0.01666 0.00415 0.035584 0.00712
+fpcarbgun        carbgun            collide=0 timer=4 typeflags=0x0 respawn=0
# DESC: 
# BBOX: -0.006993 -0.00625 -0.021875 0.006993 0.00625 0.021875
battery           _powerup           thingflags=0x408 model3d=battery.3do cog=pow_battery_m.cog
# DESC: 
# BBOX: -0.006993 -0.00625 -0.021875 0.006993 0.00625 0.021875
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
# BBOX: -0.016822 -0.05557 -0.026043 0.007966 0.058 0.014355
+dstrifle         _droppowerup       thingflags=0x400 model3d=strp.3do cog=pow_dstrifle_m.cog height=.035177
# DESC: 
# BBOX: -0.012585 -0.050304 -0.010582 0.022579 0.027638 0.007152
_limb             none               orient=(0/0/0) type=corpse collide=1 move=physics thingflags=0x400 timer=30 model3d=stlimb.3do size=.102337 movesize=.000001 cog=00_bloodtrail.cog surfdrag=1 airdrag=4 mass=15 physflags=0x44261 buoyancy=.5
# DESC: 
# BBOX: -0.012585 -0.050304 -0.010582 0.022579 0.027638 0.007152
+stormlimb        _limb              size=.05 movesize=.01
# DESC: 
# BBOX: -0.018484 -0.070714 -0.030025 0.019883 0.081826 0.013227
+drailgun         _droppowerup       thingflags=0x400 model3d=rldp.3do cog=pow_drailgun_m.cog height=.04442
# DESC: 
# BBOX: -0.00548 -0.051379 -0.021767 0.00548 0.039969 0.012035
+drepeatergun     _droppowerup       thingflags=0x400 model3d=rptp.3do cog=pow_drepeater_m.cog height=.042069
# DESC: 
# BBOX: -0.037893 -0.014351 -0.12043 0.03746 0.022321 0.063011
atatpilot         _humanactor        thingflags=0x20000400 model3d=ap.3do size=.06595 movesize=.06595 puppet=ic.pup soundclass=st.snd cog=actor_tp_pnch.cog maxvel=.3 weapon=+ebolt weapon2=+st_punch health=130 maxhealth=130 maxthrust=1.1 maxrotthrust=90 typeflags=0x20001 fireoffset=(.014/.059/.031) aiclass=tp_def.ai
# DESC: 
# BBOX: 0 0 0 0 0 0
+atst_exp         +grenade_exp       damage=10
# DESC: 
# BBOX: -0.009653 -0.02399 -0.017867 0.009653 0.02399 0.017867
+dpowercell       _droppowerup       thingflags=0x400 model3d=pcel.3do cog=pow_power_m.cog height=.017868
# DESC: 
# BBOX: -0.040113 -0.035949 -0.021618 0.040113 0.035949 0.021618
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
# BBOX: -0.015415 -0.017797 -0.028888 0.015415 0.017797 0.028888
+dbactatank       _droppowerup       thingflags=0x400 model3d=bact.3do cog=pow_bacta.cog height=.028885
# DESC: 
# BBOX: -0.006993 -0.00625 -0.021875 0.006993 0.00625 0.021875
+dbattery         _droppowerup       thingflags=0x408 model3d=battery.3do cog=pow_battery_m.cog height=.022909
# DESC: 
# BBOX: -0.008536 -0.014067 -0.022418 0.008536 0.014068 0.022418
+denergycell      _droppowerup       thingflags=0x400 model3d=ecel.3do cog=pow_energy_m.cog height=.022435
# DESC: 
# BBOX: -0.008988 -0.030139 -0.030139 0.008988 0.030139 0.030139
+fsaberreturns    lightsaber         timer=5 cog=00_returnsaber.cog mass=0 physflags=0xc00000 typeflags=0x8 respawn=0
# DESC: 
# BBOX: 0 0 0 0 0 0
_gravdebris       none               orient=(0/0/0) type=weapon collide=1 move=physics timer=4.5 mass=.01 physflags=0x201 damage=20 damageclass=0x1 typeflags=0xc
# DESC: 
# BBOX: -0.026724 -0.026724 -0.044654 0.026724 0.026724 0.044654
grshrapnel_1      _gravdebris        model3d=shrp_1.3do size=.045 movesize=.045
# DESC: 
# BBOX: -0.011171 -0.010389 -0.042003 0.011171 0.010389 0.042003
grshrapnel_2      _gravdebris        model3d=shrp_2.3do size=.04 movesize=.04
# DESC: 
# BBOX: -0.015988 -0.015443 -0.026188 0.015988 0.015443 0.026188
grshrapnel_3      _gravdebris        model3d=shrp_3.3do size=.028 movesize=.028
# DESC: 
# BBOX: -0.016213 -0.014647 -0.020855 0.016213 0.014647 0.020855
grshrapnel_4      _gravdebris        model3d=shrp_4.3do size=.026 movesize=.026
# DESC: 
# BBOX: -0.026804 -0.036274 -0.074092 0.026804 0.036274 0.074092
grshrapnel_5      _gravdebris        model3d=shrp_5.3do size=.084 movesize=.084
# DESC: 
# BBOX: 0 0 0 0 0 0
+limb_blood       none               orient=(0/0/0) type=particle move=physics timer=.25 physflags=0x201 vel=(0/0/.2) typeflags=0x27 material=00gsmoke.mat range=.02 rate=48 maxthrust=3 elementsize=.005 count=16
# DESC: 
# BBOX: -0.3 -0.0125 -0.3 0.3 0.0125 0.3
6x6door           _structure         model3d=d6x6.3do size=.474448 movesize=.474448 soundclass=lg_door.snd
# DESC: 
# BBOX: -0.15 -0.4 -0.012306 0.15 0.4 0.012306
3x8catwalk        _walkstruct        model3d=c3x8.3do size=.427378 movesize=.427378
# DESC: 
# BBOX: -0.15 -0.3 -0.012306 0.15 0.3 0.012306
3x6catwalk        _walkstruct        model3d=c3x6.3do size=.335636 movesize=.335636
# DESC: 
# BBOX: -0.1 -0.0125 -0.15 0.1 0.0125 0.15
2x3door_4         _structure         model3d=d2x3_4.3do size=.18071 movesize=.18071 soundclass=sm_door.snd
# DESC: 
# BBOX: -0.25 -0.05 -0.2 0.25 0.05 0.2
m2heater          _walkstruct        thingflags=0x4c model3d=m2heat.3do size=.324037 movesize=.324037
# DESC: 
# BBOX: -0.063767 -0.055224 -0.021878 0.063767 0.055224 0.021878
probedebris_1     _debris            model3d=pb_1.3do size=.0535 movesize=.0535
# DESC: 
# BBOX: -0.071973 -0.062331 -0.029124 0.071973 0.062331 0.029124
probedebris_2     _debris            model3d=pb_2.3do size=.057 movesize=.057
# DESC: 
# BBOX: -0.009722 -0.012251 -0.047823 0.009722 0.012251 0.047823
probedebris_3     _debris            model3d=pb_3.3do size=.0415 movesize=.0415
# DESC: 
# BBOX: -0.048787 -0.012879 -0.031716 0.048787 0.012879 0.031716
probedebris_4     _debris            model3d=pb_4.3do size=.04 movesize=.04
# DESC: 
# BBOX: 0 0 0 0 0 0
+probedroid_exp   _explosion         thingflags=0x1 light=.2 timer=.8 sprite=seqx.spr soundclass=exp_probe.snd typeflags=0x17 damage=40 blasttime=.699 force=200 maxlight=.8 range=.45 debris=probedebris_1 debris=probedebris_2 debris=probedebris_3 debris=probedebris_4
# DESC: 
# BBOX: -0.01341 -0.149802 -0.01341 0.01341 0.015603 0.01341
+turretlaser      +stlaser           light=.5 model3d=con0.3do size=.005 movesize=.005 vel=(0/5/0) fleshhit=+laserhit damage=15 mindamage=5
# DESC: 
# BBOX: -0.007791 -0.090587 -0.006258 0.007791 0.017998 0.006258
+probedroidbolt   +turretlaser       model3d=bry0.3do vel=(0/4/0) damage=30 mindamage=10
# DESC: 
# BBOX: -0.074435 -0.062273 -0.215886 0.071944 0.062389 0.198272
probedroid        _flyactor          thingflags=0x20000400 model3d=pb.3do size=.078 movesize=.078 puppet=pb.pup soundclass=pb.snd cog=actor_pd.cog mass=300 physflags=0x2006a06 maxvel=.5 weapon=+probedroidbolt health=90 maxhealth=90 maxthrust=.6 maxrotthrust=150 typeflags=0x2008120 fireoffset=(.0375/.065/0) explode=+probedroid_exp aiclass=pddefault.ai
# DESC: 
# BBOX: -0.037458 -0.014968 -0.120277 0.037328 0.026217 0.066384
iofficer_m        _humanactor        thingflags=0x20000400 model3d=io.3do size=.0667 movesize=.0667 puppet=io.pup soundclass=io_s1.snd cog=actor_io.cog maxvel=.3 weapon=+ebolt weapon2=+st_punch health=30 maxthrust=.9 maxrotthrust=80 typeflags=0x20001 fireoffset=(.05/.065/.014) aiclass=iodefault.ai
# DESC: 
# BBOX: -0.150124 -0.150615 -0.150124 0.149876 0.149385 0.149872
crate4_5          _walkstruct        model3d=crt4-5.3do size=.260306 movesize=.260306
# DESC: 
# BBOX: -0.120072 -0.104087 -0.149746 0.120072 0.104087 0.149746
xtank3a           _walkstruct        thingflags=0x400448 model3d=xtank3a.3do size=.191956 movesize=.191956 cog=xtank3.cog
# DESC: 
# BBOX: -0.314105 -0.627595 -0.074421 0.314105 0.627595 0.074421
trooptran         _zwalkstruct       model3d=ttrn.3do size=.718795 movesize=.718795
# DESC: 
# BBOX: -0.383546 -0.285641 -0.334209 0.372394 0.719699 0.318835
tieinterceptor    _zwalkstruct       model3d=tiei.3do size=.817667 movesize=.817667
# DESC: 
# BBOX: -0.605016 -1.377949 -1.085803 0.605016 1.377949 1.085803
atatactor         _zwalkstruct       model3d=aa.3do size=.7 movesize=.7
# DESC: 
# BBOX: -0.1 -0.0125 -0.15 0.1 0.0125 0.15
2x3door_1         _structure         model3d=d2x3_1.3do size=.18071 movesize=.18071 soundclass=sm_door.snd
# DESC: 
# BBOX: -0.198 -0.198002 -0.05971 0.198 0.198002 0.05971
4x4elev           _walkstruct        model3d=e4x4.3do size=.336311 movesize=.336311 soundclass=med_elev.snd
# DESC: 
# BBOX: -0.010023 -0.016964 -0.033841 0.010023 0.016964 0.033841
keyyellow         _powerup           thingflags=0x400 model3d=y-key.3do cog=pow_keyyellow.cog
# DESC: 
# BBOX: -0.037893 -0.014351 -0.12043 0.03746 0.022321 0.063011
atatpilot_m       _humanactor        thingflags=0x20000400 model3d=ap.3do size=.06595 movesize=.06595 puppet=ic.pup soundclass=st_s1.snd cog=actor_tp_pnch.cog maxvel=.3 weapon=+ebolt weapon2=+st_punch health=130 maxhealth=130 maxthrust=1.1 maxrotthrust=90 typeflags=0x20001 fireoffset=(.014/.059/.031) aiclass=tp_def.ai
# DESC: 
# BBOX: -0.037893 -0.014351 -0.12043 0.03746 0.022321 0.063011
tiepilot_m        _humanactor        thingflags=0x20000400 model3d=tp.3do size=.06595 movesize=.06595 puppet=ic.pup soundclass=st_s1.snd cog=actor_tp_pnch.cog maxvel=.3 weapon=+ebolt weapon2=+st_punch health=110 maxhealth=110 maxthrust=1.1 maxrotthrust=90 typeflags=0x20001 fireoffset=(.014/.059/.031) aiclass=tp_def.ai
# DESC: 
# BBOX: -0.019886 -0.022184 0 0.019886 0.022184 0.042868
seqcharge         _cpowerup          thingflags=0x400 model3d=seqp.3do cog=pow_sequencer_m.cog
# DESC: 
# BBOX: -0.008536 -0.014067 -0.022418 0.008536 0.014068 0.022418
energycell        _powerup           thingflags=0x400 model3d=ecel.3do cog=pow_energy_m.cog
# DESC: 
# BBOX: -0.037893 -0.021815 -0.12043 0.03746 0.022321 0.066407
icommando         _humanactor        thingflags=0x20000400 model3d=ic.3do size=.06595 movesize=.06595 puppet=ic.pup soundclass=ic.snd cog=actor_ic.cog maxvel=.3 weapon=+elaser weapon2=+st_punch health=90 maxhealth=90 maxthrust=1.1 maxrotthrust=90 typeflags=0x20001 fireoffset=(.014/.059/.031) aiclass=icdefault.ai
# DESC: 
# BBOX: -0.037893 -0.02023 -0.12043 0.03746 0.022321 0.065552
icommandorpt_m    icommando          model3d=icrpt.3do soundclass=icrpt_s1.snd cog=actor_i2.cog weapon=+eball aiclass=icreapdefault.ai
# DESC: 
# BBOX: -0.021293 -0.02235 -0.028598 0.01838 0.02235 0.028374
fullshield        _powerup           thingflags=0x400 model3d=vest.3do cog=pow_vest.cog respawn=60
# DESC: 
# BBOX: -0.026804 -0.036274 -0.074092 0.026804 0.036274 0.074092
shrapnel_5        _debris            model3d=shrp_5.3do size=.084 movesize=.084 damage=0
# DESC: 
# BBOX: 0 0 0 0 0 0
+turret_exp       _explosion         thingflags=0x1 light=.2 timer=.8 sprite=detx.spr soundclass=exp_probe.snd typeflags=0x17 damage=10 blasttime=.699 force=100 maxlight=.8 range=.45 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_5
# DESC: 
# BBOX: -0.058293 -0.078937 -0.033443 0.058195 0.084676 0.035083
roundturret       none               orient=(0/0/0) type=actor collide=1 move=physics thingflags=0x400 model3d=tur2.3do size=.08 movesize=.08 puppet=tur2.pup soundclass=turret.snd cog=class_roundturret.cog physflags=0x2400000 weapon=+turretlaser health=100 maxhealth=100 typeflags=0x2080160 fireoffset=(0/-.028/.022) explode=+turret_exp aiclass=turdefault.ai
# DESC: 
# BBOX: -0.01341 -0.149802 -0.01341 0.01341 0.015603 0.01341
+turretlaser2     +turretlaser       vel=(0/4/0) damage=25 mindamage=10
# DESC: 
# BBOX: -0.059999 -0.051358 -0.040791 0.060835 0.061748 0.050725
boxturret         none               orient=(0/0/0) type=actor collide=1 move=physics thingflags=0x400 model3d=tur1.3do size=.075 movesize=.075 puppet=tur1.pup soundclass=turret.snd cog=class_boxturret.cog physflags=0x2400000 weapon=+turretlaser2 health=80 maxhealth=80 typeflags=0x2080160 fireoffset=(0/-.003/.036) explode=+turret_exp aiclass=turbdefault.ai
# DESC: 
# BBOX: -0.01341 -0.149802 -0.01341 0.01341 0.015603 0.01341
+turretlaser3     +turretlaser       vel=(0/4/0) damage=40 mindamage=20
# DESC: 
# BBOX: -0.124322 -0.166491 -0.102246 0.124322 0.167917 0.079419
boxturret4        boxturret          model3d=tur4.3do size=.2 movesize=.2 cog=class_boxturret4.cog weapon=+turretlaser3 health=150 maxhealth=150 fireoffset=(0/-.043/.036) aiclass=turbbdefault.ai
# DESC: 
# BBOX: -0.009575 -0.037873 -0.008367 0.009575 0.037873 0.008367
npscope           scope              typeflags=0x9
# DESC: 
# BBOX: -0.2 -0.400002 -0.05971 0.2 0.400002 0.05971
4x8elev           _walkstruct        model3d=e4x8.3do size=.451184 movesize=.451184 soundclass=lg_elev.snd
# DESC: 
# BBOX: -0.007408 -0.00644 -0.031415 0.007408 0.00644 0.031415
securitycambase   _structure         model3d=scambase.3do size=.082913 movesize=.082913
# DESC: 
# BBOX: 0 0 0 0 0 0
+remote_exp       +grenade_exp       sprite=debrisx.spr soundclass=exp_remote.snd damage=0 force=25
# DESC: 
# BBOX: -0.031793 -0.031444 -0.039413 0.031793 0.031444 0.039413
securitycamera    none               orient=(0/0/0) type=actor collide=1 move=physics thingflags=0x400 model3d=scam.3do size=.035 movesize=.035 soundclass=turret.snd cog=00_securitycam.cog physflags=0x2400000 health=50 maxhealth=50 typeflags=0x2180160 explode=+remote_exp aiclass=turdefault.ai
# DESC: 
# BBOX: 0 0 0 0 0 0
ghostcam          none               orient=(0/0/0) type=ghost move=path puppet=cr.pup
# DESC: 
# BBOX: 0 0 0 0 0 0
light0.25         _ghostdecor        thingflags=0x1 light=.25
# DESC: 
# BBOX: -0.040024 -0.034696 -0.049915 0.040024 0.034696 0.049915
xtank1a           _walkstruct        thingflags=0x400448 model3d=xtank1a.3do size=.063985 movesize=.063985 cog=xtank1.cog
# DESC: 
# BBOX: -0.01636 -0.014643 -0.011603 0.017727 0.017747 0.016258
srailcharges      _cpowerup          thingflags=0x400 model3d=scrg.3do cog=pow_srailcharges_m.cog
# DESC: 
# BBOX: -0.037458 -0.014968 -0.120277 0.037328 0.026217 0.066384
iofficer          _humanactor        thingflags=0x20000400 model3d=io.3do size=.0667 movesize=.0667 puppet=io.pup soundclass=io.snd cog=actor_io.cog maxvel=.3 weapon=+ebolt weapon2=+st_punch health=30 maxthrust=.9 maxrotthrust=80 typeflags=0x20001 fireoffset=(.05/.065/.014) aiclass=iodefault.ai
# DESC: 
# BBOX: -0.037458 -0.014968 -0.120277 0.037328 0.026217 0.066384
iofficer2_m       iofficer           soundclass=io2_s1.snd
# DESC: 
# BBOX: -0.005771 -0.015021 -0.00466 0.005771 0.003935 0.00466
+remotelaser      +repeaterball      damage=2 mindamage=1
# DESC: 
# BBOX: -0.053638 -0.040044 -0.039175 0.046239 0.040044 0.06128
torturedroid      _flyactor          thingflags=0x20000400 model3d=id.3do size=.05 movesize=.05 soundclass=td.snd cog=actor_re.cog mass=45 physflags=0x2006a06 maxvel=.6 weapon=+remotelaser health=30 maxthrust=.9 maxrotthrust=150 typeflags=0x2008120 fireoffset=(-.03/.04/0) explode=+remote_exp aiclass=td_def.ai
# DESC: 
# BBOX: 0 0 0 0 0 0
+mouse_exp        +remote_exp        force=10
# DESC: 
# BBOX: -0.018576 -0.029933 -0.04006 0.017936 0.031186 -0.009989
mousebot          _droidactor        thingflags=0x20000400 model3d=mb.3do size=.039 movesize=.039 soundclass=mousebot.snd cog=actor_mb.cog mass=20 height=.04 health=10 maxthrust=1 maxrotthrust=80 typeflags=0x20 explode=+mouse_exp aiclass=mouse.ai
# DESC: 
# BBOX: -0.036535 -0.022706 -0.017295 0.036535 0.022706 0.017295
goggles           _powerup           thingflags=0x400 model3d=gogl.3do cog=pow_goggles.cog respawn=60
# DESC: 
# BBOX: -0.022125 -0.04425 -0.0001 0.022125 0.04425 0.04415
ewebammo          _powerup           thingflags=0x400 model3d=epack.3do cog=pow_ewebammo_m.cog
# DESC: 
eweb              _powerup           type=cog move=path thingflags=0x400 model3d=eweb.3do cog=pow_eweb_m.cog
# DESC: 
# BBOX: 0 0 0 0 0 0
+esequencer_exp   _explosion         thingflags=0x1 light=.2 timer=.8 sprite=seqx2.spr soundclass=exp_med.snd creatething=+firecloud2 typeflags=0x17 damage=75 blasttime=.699 force=300 maxlight=.8 range=.6 debris=shrapnel2_1 debris=shrapnel2_2 debris=shrapnel2_3 debris=shrapnel2_4
# DESC: 
# BBOX: -0.022216 -0.023897 -0.005309 0.022216 0.023897 0.005309
sequencermine     +grenade2          timer=0 model3d=seq0.3do size=.3 movesize=.01 soundclass=seq.snd surfdrag=5 airdrag=1 physflags=0x29c vel=(0/0/0) angvel=(0/0/0) buoyancy=.5 explode=+esequencer_exp typeflags=0x1380
# DESC: 
# BBOX: -0.005081 -0.030803 -0.01143 0.005081 0.030803 0.01143
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
# BBOX: -0.011462 -0.396132 -0.479822 0.011462 0.396132 0.479822
tief_debris_1     _debris            model3d=tief_1.3do size=.549332 movesize=.549332
# DESC: 
# BBOX: -0.123623 -0.077635 -0.077635 0.123623 0.077635 0.077635
tief_debris_2     _debris            model3d=tief_2.3do size=.145978 movesize=.145978
# DESC: 
# BBOX: -0.132555 -0.152638 -0.129684 0.131696 0.120306 0.134567
tief_debris_3     _debris            model3d=tief_3.3do size=.163079 movesize=.163079
# DESC: 
# BBOX: 0 0 0 0 0 0
+tief_exp         +tie_exp           debris=tief_debris_1 debris=tief_debris_2 debris=tief_debris_3 debris=tief_debris_1
# DESC: 
# BBOX: -0.140444 -0.50267 -0.326522 0.140444 0.50267 0.326522
tiei_debris_1     _debris            model3d=tiei_1.3do size=.403432 movesize=.403432
# DESC: 
# BBOX: -0.083496 -0.285641 -0.222454 0.072344 0.719698 0.207079
tiei_debris_2     _debris            model3d=tiei_2.3do size=.403432 movesize=.403432
# DESC: 
# BBOX: -0.261944 0.069298 -0.150714 0.250793 0.36476 0.135339
tiei_debris_3     _debris            model3d=tiei_3.3do size=.403432 movesize=.403432
# DESC: 
# BBOX: -0.090911 -0.285641 -0.119597 0.07976 0.719698 0.104222
tiei_debris_4     _debris            model3d=tiei_4.3do size=.403432 movesize=.403432
# DESC: 
# BBOX: 0 0 0 0 0 0
+tiei_exp         +tie_exp           debris=tiei_debris_1 debris=tiei_debris_2 debris=tiei_debris_3 debris=tiei_debris_4
# DESC: 
# BBOX: -0.40002 -0.400093 -0.024993 0.40002 0.400093 0.024993
8x8elev           _walkstruct        model3d=e8x8.3do size=.616317 movesize=.616317 soundclass=lg_elev.snd
# DESC: 
# BBOX: -0.029714 -0.02826 -0.025 0.024039 0.02826 0.02818
landinglight      _structure         model3d=l_lite.3do size=.088832 movesize=.088832
# DESC: 
# BBOX: -0.034 -0.05906 -0.093167 0.034804 0.05936 0.052273
powerdroid        _droidactor        thingflags=0x20000400 model3d=pd.3do size=.08 movesize=.08 puppet=pd.pup soundclass=gonk.snd cog=actor_go.cog mass=180 health=50 maxhealth=50 maxthrust=.2 maxrotthrust=50 typeflags=0x180140 aiclass=gonk.ai
# DESC: 
# BBOX: -0.100779 -0.107544 0.000686 0.100779 0.107544 0.023173
ceilinglight      _structure         thingflags=0x9 light=.5 model3d=clight.3do size=.116487 movesize=.116487
# DESC: 
# BBOX: -0.4 -0.050015 -0.398 0.4 0.050015 0.4
8x8door           _structure         model3d=d8x8.3do size=.617892 movesize=.617892 soundclass=lg_door.snd
# DESC: 
# BBOX: -0.02013 -0.030328 -0.022079 0.018742 0.092392 0.056747
securitygun       none               orient=(0/0/0) type=actor collide=1 move=physics thingflags=0x400 model3d=sgun.3do size=.06 movesize=.06 puppet=sgun.pup soundclass=turret.snd cog=class_roundturret.cog physflags=0x2400000 weapon=+turretlaser health=100 maxhealth=100 typeflags=0x2080160 fireoffset=(0/0/.022) explode=+turret_exp aiclass=turdefault.ai
# DESC: 
# BBOX: -0.02509 -0.216005 -0.050902 0.02509 0.216669 0.117932
8x8elevstrut      _walkstruct        model3d=e8x8stru.3do size=.272753 movesize=.272753
# DESC: 
# BBOX: -0.010023 -0.016964 -0.033841 0.010023 0.016964 0.033841
keyred            _powerup           thingflags=0x400 model3d=r-key.3do cog=pow_keyred.cog
# DESC: 
# BBOX: -0.014212 -0.013093 -0.011834 0.014212 0.013093 0.011834
assassindebris_1  _debris            model3d=adpart0.3do size=.0072 movesize=.0072
# DESC: 
# BBOX: -0.020943 -0.017223 -0.022127 0.020943 0.017223 0.022127
assassindebris_2  _debris            model3d=adpart1.3do size=.0264 movesize=.0264
# DESC: 
# BBOX: -0.016411 -0.007309 -0.014852 0.016411 0.007309 0.014852
assassindebris_3  _debris            model3d=adpart2.3do size=.02 movesize=.02
# DESC: 
# BBOX: -0.01233 -0.01609 -0.007689 0.01233 0.01609 0.007689
assassindebris_4  _debris            model3d=adpart3.3do size=.0208 movesize=.0208
# DESC: 
# BBOX: 0 0 0 0 0 0
+assassindroid_exp _explosion         thingflags=0x1 light=.2 timer=.8 sprite=seqx.spr soundclass=exp_probe.snd typeflags=0x17 damage=20 blasttime=.699 force=120 maxlight=.8 range=.3 debris=assassindebris_1 debris=assassindebris_2 debris=assassindebris_3 debris=assassindebris_4
# DESC: 
# BBOX: -0.030923 -0.014545 -0.099713 0.030923 0.02174 0.060004
assassindroid     _droidactor        thingflags=0x20000400 model3d=ad.3do size=.058 movesize=.058 puppet=ad.pup soundclass=ad.snd cog=actor_ad.cog mass=160 weapon=+probedroidbolt health=100 maxhealth=100 maxrotthrust=80 typeflags=0x1a8161 explode=+assassindroid_exp aiclass=addefault.ai
# DESC: 
# BBOX: 0 0 0 0 0 0
+upenergyhi       none               orient=(0/0/0) type=particle move=physics timer=.6 vel=(0/0/2) angvel=(0/360/0) typeflags=0x3e material=embers.mat range=.4 elementsize=.0075 count=30
# DESC: 
# BBOX: -0.01477 -0.014666 -0.01477 0.01477 0.014666 0.01477
remote            _flyactor          thingflags=0x20000400 model3d=remo.3do size=.025 movesize=.025 soundclass=re.snd cog=actor_re.cog mass=15 physflags=0x2006a06 maxvel=.6 weapon=+remotelaser health=30 maxthrust=.9 maxrotthrust=150 typeflags=0x2008120 fireoffset=(.015/.02/0) explode=+remote_exp aiclass=redefault.ai
# DESC: 
# BBOX: 0 0 0 0 0 0
+gonk_exp         +grenade_exp       damage=20
# DESC: 
# BBOX: -0.200716 -0.195991 -0.079681 0.195304 0.200102 0.079681
4x4elev_2         _walkstruct        model3d=e4x4_2.3do size=.344409 movesize=.344409 soundclass=med_elev.snd
# DESC: 
# BBOX: -0.498 -0.998 -0.1 0.498 -0.002 0
10x10door         _walkstruct        model3d=d10x10.3do size=1.169825 movesize=1.169825 soundclass=lg_elev.snd
# DESC: 
# BBOX: -0.021293 -0.02235 -0.028598 0.01838 0.02235 0.028374
supershield       _powerup           thingflags=0x400 model3d=vestl.3do cog=pow_supershield.cog respawn=60
# DESC: 
# BBOX: -0.02978572 -0.01917877 -0.1247011 0.02409064 0.06467322 0.0434061
stormtroop_m      _humanactor        thingflags=0x20000400 model3d=st.3do puppet=st.pup soundclass=st_s1.snd cog=actor_st_pnch.cog maxvel=.25 weapon=+elaser weapon2=+st_punch health=60 maxhealth=60 maxthrust=1 maxrotthrust=90 typeflags=0x20001 fireoffset=(.0048/.05/.012) aiclass=st_def.ai
# DESC: 
# BBOX: -0.035317 -0.025605 -0.047024 0.035366 0.022343 0.048893
r2                _droidactor        thingflags=0x20000400 model3d=r2.3do size=.058 movesize=.058 puppet=r2.pup soundclass=r2.snd cog=actor_r2.cog maxvel=.4 health=50 maxhealth=50 maxthrust=.4 maxrotthrust=50 typeflags=0x180140 aiclass=drdefault.ai
# DESC: 
# BBOX: -0.037893 -0.021815 -0.12043 0.03746 0.022321 0.066407
icommando_m       _humanactor        thingflags=0x20000400 model3d=ic.3do size=.06595 movesize=.06595 puppet=ic.pup soundclass=ic_s1.snd cog=actor_ic.cog maxvel=.3 weapon=+elaser weapon2=+st_punch health=90 maxhealth=90 maxthrust=1.1 maxrotthrust=90 typeflags=0x20001 fireoffset=(.014/.059/.031) aiclass=icdefault.ai
# DESC: 
# BBOX: 0 0 0 0 0 0
16_exp            +grenade_exp       damage=0
# DESC: 
# BBOX: -0.035332 -0.013185 -0.120103 0.035579 0.016413 0.064197
marawalkplayer    _actor             type=player thingflags=0x20000401 light=.2 model3d=mj.3do size=.065 movesize=.065 puppet=mj.pup soundclass=mj.snd cog=mara_m.cog surfdrag=3 airdrag=.5 staticdrag=.3 health=100 maxhealth=100 maxthrust=2 typeflags=0x1 error=.5 fov=.71 chance=1
# DESC: 
# BBOX: -0.005081 -0.030803 -0.01143 0.005081 0.030803 0.01143
greedopistol      _powerup           thingflags=0x400 model3d=ggun.3do cog=pow_blastech_m.cog
# DESC: 
# BBOX: -0.012329 -0.036225 -0.012284 0.012329 0.036225 0.012284
droideye          _structure         model3d=deye.3do size=.040592 movesize=.040592 puppet=cr.pup
# DESC: 
# BBOX: -0.25 -0.475 -0.25 0.25 0.475 0.25
s2l1weight_1      _walkstruct        thingflags=0x48 model3d=21wght01.3do size=.642136 movesize=.642136 soundclass=19counterweight.snd
# DESC: 
# BBOX: -0.01264 -0.999708 -0.01129 0.01264 0.999708 0.01129
s2l1rope03        _structure         thingflags=0xc model3d=21rope03.3do size=1.049851 movesize=1.049851
# DESC: 
# BBOX: -0.7 -0.025 0.00207 0.7 0.025 0.80207
shielddoor        _structure         model3d=bigdoor.3do size=1.114867 movesize=1.114867
# DESC: 
# BBOX: -0.2 -0.0125 -0.15 0.2 0.0125 0.15
rustydoor         _structure         model3d=d4x3_2.3do size=.300312 movesize=.300312 soundclass=med_door.snd
# DESC: 
# BBOX: -0.2 -0.015 -0.175 0.2 0.015 0.175
securitydoor      _structure         model3d=secdoor.3do size=.316177 movesize=.316177 soundclass=med_door.snd
# DESC: 
# BBOX: -0.3 -0.026218 -0.00201 0.3 0.023782 0.34799
securitydoor2     _structure         model3d=secdoor2.3do size=.5102 movesize=.5102 soundclass=med_door.snd
# DESC: 
# BBOX: -0.224901 -0.01243 -0.12491 0.224901 0.01243 0.12491
barredgate        _structure         model3d=s2l4gate.3do size=.307561 movesize=.307561
# DESC: 
# BBOX: -0.1 -0.005844 -0.1 0.1 0.005844 0.1
woodwindowcover   _structure         model3d=shutr.3do size=.191542 movesize=.191542 soundclass=03wood_door.snd
# DESC: 
# BBOX: -0.064855 -0.030184 -0.137291 0.064857 0.181468 0.079798
gammorean         _humanactor        thingflags=0x20000400 model3d=gm.3do size=.07878 movesize=.07878 puppet=gm.pup soundclass=gm.snd cog=actor_gg.cog mass=200 weapon=+gamaxe weapon2=+gamaxe health=120 maxhealth=120 maxrotthrust=80 typeflags=0x20001 aiclass=ggdefault.ai
# DESC: 
# BBOX: 0 0 0 0 0 0
+ry_punch         +punch             force=25
# DESC: 
# BBOX: -0.046863 -0.025046 -0.131246 0.045997 0.027318 0.073257
reeyeesgun        _humanactor        thingflags=0x20000400 model3d=rystr.3do size=.0729 movesize=.0729 puppet=rystr.pup soundclass=rystr.snd cog=actor_rb_pnch.cog weapon=+elaser weapon2=+ry_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(.0147/.08/.034) aiclass=rydefault.ai
# DESC: 
# BBOX: 0 0 0 0 0 0
+egrenade_exp     +grenade_exp       damage=50
# DESC: 
# BBOX: -0.012516 -0.012516 -0.012524 0.012516 0.012516 0.014655
+actor_grenade    +grenade1          surfdrag=0 airdrag=0 vel=(0/2/0) explode=+egrenade_exp fleshhit=+egrenade_exp
# DESC: 
# BBOX: -0.046863 -0.025046 -0.131246 0.045997 0.027318 0.073257
reeyees           _humanactor        thingflags=0x20000400 model3d=ry.3do size=.0729 movesize=.0729 puppet=ry.pup soundclass=ry.snd cog=actor_ry_pnch.cog weapon=+actor_grenade weapon2=+ry_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(.0465/.07/.0327) aiclass=rytherdefault.ai
# DESC: 
# BBOX: -0.012516 -0.012516 -0.012524 0.012516 0.012516 0.014655
+actor_grenade2   +grenade2          vel=(0/2/0) explode=+egrenade_exp fleshhit=+egrenade_exp
# DESC: 
# BBOX: -0.046863 -0.025046 -0.131246 0.045997 0.027318 0.073257
reeyeess          reeyees            weapon=+actor_grenade2
# DESC: 
# BBOX: -0.020392 -0.135722 -0.016856 0.021073 0.013215 0.015722
+ebow             +crossbowbolt      vel=(0/4/0) damage=20 mindamage=10
# DESC: 
# BBOX: -0.037806 -0.014831 -0.120834 0.054054 0.062457 0.065442
tuskan            _humanactor        thingflags=0x20000400 model3d=tk.3do size=.0649 movesize=.0649 puppet=tk.pup soundclass=tk.snd cog=actor_tk_pnch.cog mass=120 maxvel=.2 weapon=+ebow weapon2=+st_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(.006/.061/.027) aiclass=tuproddefault.ai
# DESC: 
# BBOX: -0.15 -0.025 -0.025 0.15 0.025 0.025
windowledge       _walkstruct        thingflags=0x48 model3d=wledge.3do size=.20411 movesize=.20411
# DESC: 
# BBOX: -0.021089 -0.023005 -0.035347 0.016307 0.023005 0.035347
clothespack       _powerup           move=path thingflags=0x400 model3d=clths.3do cog=pow_tusken_clothes.cog
# DESC: 
# BBOX: -0.063294 -0.054836 -0.040528 0.063294 0.054836 0.040528
00table           _walkstruct        thingflags=0x48 model3d=00tabl.3do size=.07042 movesize=.07042
# DESC: 
# BBOX: -0.046863 -0.025046 -0.131246 0.045997 0.027318 0.073257
preeyeesgun       _humanactor        thingflags=0x20000400 model3d=rystr.3do size=.0729 movesize=.0729 puppet=rystr.pup soundclass=rystr.snd cog=actor_rb_ped3.cog weapon=+elaser weapon2=+ry_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(.0147/.08/.034) aiclass=rydefault.ai
# DESC: 
# BBOX: 0 0 0 0 0 0
_civilian         _humanactor        surfdrag=2 mass=100 maxvel=.3 maxthrust=2 maxrotthrust=90 typeflags=0x180000
# DESC: 
# BBOX: -0.035186 -0.01745 -0.100835 0.035171 0.022972 0.063971
ug                _civilian          thingflags=0x20000400 model3d=ugalt.3do size=.064 movesize=.064 puppet=ug.pup soundclass=ug.snd cog=actor_ug.cog mass=60 maxvel=.4 health=30 maxthrust=.5 maxrotthrust=60 aiclass=peddefault.ai
# DESC: 
# BBOX: -0.035186 -0.01745 -0.100835 0.035171 0.022972 0.063971
ugsit             ug                 puppet=ugsit.pup mass=200 aiclass=pedhome.ai
# DESC: 
# BBOX: -0.026466 -0.012833 -0.099682 0.026261 0.023106 0.06583
pgreedo           _humanactor        thingflags=0x20000400 model3d=gr.3do size=.0615 movesize=.0615 puppet=gr.pup soundclass=gr.snd cog=actor_gr_ped.cog mass=70 maxvel=.3 weapon=+ebolt maxrotthrust=80 typeflags=0x2080001 fireoffset=(.016/.055/.004) aiclass=grdefault.ai
# DESC: 
# BBOX: -0.046863 -0.025046 -0.131246 0.045997 0.027318 0.073257
preeyeesfist      _humanactor        thingflags=0x20000400 model3d=ry.3do size=.0729 movesize=.0729 puppet=ryfist.pup soundclass=ryfist.snd cog=actor_rf_ped2.cog weapon=+ry_punch health=50 maxhealth=50 maxthrust=1 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 aiclass=rypundefault.ai
# DESC: 
# BBOX: -0.046863 -0.025046 -0.131246 0.045997 0.027318 0.073257
reeyeesfist       _humanactor        thingflags=0x20000400 model3d=ry.3do size=.0729 movesize=.0729 puppet=ryfist.pup soundclass=ryfist.snd cog=actor_rf.cog weapon=+ry_punch health=50 maxhealth=50 maxthrust=1 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 aiclass=rypundefault.ai
# DESC: 
# BBOX: -0.046863 -0.025046 -0.131246 0.045997 0.027318 0.073257
preeyees          _humanactor        thingflags=0x20000400 model3d=ry.3do size=.0729 movesize=.0729 puppet=ry.pup soundclass=ry.snd cog=actor_ry_ped1.cog weapon=+actor_grenade weapon2=+ry_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(.0465/.07/.0327) aiclass=rytherdefault.ai
# DESC: 
# BBOX: -0.026466 -0.012833 -0.099682 0.026261 0.023106 0.06583
greedo            _humanactor        thingflags=0x20000400 model3d=gr.3do size=.0615 movesize=.0615 puppet=gr.pup soundclass=gr.snd cog=actor_gr.cog mass=70 maxvel=.3 weapon=+ebolt maxrotthrust=80 typeflags=0x2080001 fireoffset=(.016/.055/.004) aiclass=grdefault.ai
# DESC: 
# BBOX: -0.046863 -0.025046 -0.131227 0.045997 0.01931 0.074096
pithorian         _humanactor        thingflags=0x20000400 model3d=it.3do size=.0729 movesize=.0729 puppet=it.pup soundclass=itstr.snd cog=actor_it_ped.cog weapon=+elaser weapon2=+ry_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(.0147/.08/.034) aiclass=it_def.ai
# DESC: 
# BBOX: 0 0 0 0 0 0
light0.7          _ghostdecor        thingflags=0x1 light=.7
# DESC: 
# BBOX: -0.027321 -0.032261 0.000219 0.027321 0.032261 0.126942
minichair3        _walkstruct        thingflags=0x48 model3d=chair3m.3do size=.182873 movesize=.182873
# DESC: 
# BBOX: -0.061611 -0.054554 -0.025219 0.061611 0.054554 0.025219
bottles01         _structure         collide=1 model3d=bottles1.3do size=.05 movesize=.05
# DESC: 
# BBOX: -0.031058 -0.027406 -0.023339 0.031058 0.027406 0.023339
bottles05         _structure         collide=1 model3d=bottles5.3do size=.05 movesize=.05
# DESC: 
# BBOX: -0.016342 -0.015548 -0.018445 0.016342 0.015548 0.018445
bottle06          _structure         model3d=bottle6.3do size=.076147 movesize=.076147
# DESC: 
# BBOX: -0.8544 -0.8544 -0.200784 0.8544 0.8544 0.200784
bigdome           _zwalkstruct       model3d=dome.3do size=.965443 movesize=.965443
# DESC: 
# BBOX: -0.05 -0.1 -0.025 0.05 0.1 0.025
s2l1step          _walkstruct        thingflags=0x48 model3d=s2l1step.3do size=.164564 movesize=.164564
# DESC: 
# BBOX: -0.290689 -0.400058 -0.45 0.290689 0.400058 0.45
pump_b            _walkstruct        thingflags=0x48 model3d=pump_b.3do size=.718615 movesize=.718615
# DESC: 
# BBOX: -0.396898 -0.09903 -0.348531 0.112036 0.098922 0.112506
pump_c            _walkstruct        model3d=pump_c.3do size=.464086 movesize=.464086
# DESC: 
# BBOX: -0.38877 -0.071487 -0.06191 0.38877 0.071487 0.06191
pump_a            _walkstruct        model3d=pump_a.3do size=.445288 movesize=.445288
# DESC: 
# BBOX: -0.160158 -0.036477 -0.140057 0.144928 0.036013 0.196945
dualpiston        _decor             model3d=pist.3do size=.22 movesize=.22 puppet=cr.pup
# DESC: 
# BBOX: -0.05 -0.009869 -0.05 0.05 0.009869 0.05
droideyedoor      _structure         model3d=deyedoor.3do size=.121396 movesize=.121396
# DESC: 
# BBOX: -0.024356 -0.028089 -0.032167 0.024356 0.028089 0.032167
explosivedevice   _powerup           thingflags=0x408 model3d=ec.3do cog=pow_heavy_explosive.cog
# DESC: 
# BBOX: -0.020759 -0.01902 -0.030703 0.020759 0.01902 0.030703
manaboost         _powerup           thingflags=0x400 model3d=dark.3do cog=pow_mana.cog
# DESC: 
# BBOX: -0.046863 -0.025046 -0.131227 0.045997 0.01931 0.074096
ithorian          _humanactor        thingflags=0x20000400 model3d=it.3do size=.0729 movesize=.0729 puppet=it.pup soundclass=itstr.snd cog=actor_it_pnch.cog weapon=+elaser weapon2=+ry_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(.0147/.08/.034) aiclass=it_def.ai
# DESC: 
# BBOX: -0.020824 -0.043225 0.000892 0.011798 0.043225 0.024826
flash_bombs       _cpowerup          thingflags=0x400 model3d=flsp.3do cog=pow_flashbombs_m.cog
# DESC: 
# BBOX: -0.027845 -0.025398 -0.02252 0.027845 0.025398 0.024913
+dhealthpack      _droppowerup       thingflags=0x408 model3d=hepk.3do cog=pow_health.cog height=.023701
# DESC: 
# BBOX: -0.024586 -0.02755 -0.015015 0.028804 0.052615 0.015553
+rylimb           _limb              model3d=rylimb.3do size=.05 movesize=.01
# DESC: 
# BBOX: -0.024586 -0.02755 -0.015015 0.028804 0.052615 0.015553
+rhlimb           _limb              model3d=rhlimb.3do size=.05 movesize=.01
# DESC: 
# BBOX: -0.020824 -0.043225 0.000892 0.011798 0.043225 0.024826
+ddetonator       _droppowerup       thingflags=0x400 model3d=detp.3do cog=pow_thermal_m.cog
# DESC: 
# BBOX: -0.017868 -0.016799 -0.017934 0.017868 0.016799 0.017934
+dsingle          _droppowerup       thingflags=0x400 model3d=detp_1.3do cog=pow_single_thermal_m.cog height=.021909
# DESC: 
# BBOX: -0.012516 -0.012516 -0.012524 0.012516 0.012516 0.014655
+spec_grenade     +grenade2          vel=(0/0/0)
# DESC: 
# BBOX: -0.034888 -0.055377 0.000124 0.034343 0.059997 0.031925
+dcrossbow        _droppowerup       thingflags=0x400 model3d=bowp.3do cog=pow_dcrossbow_m.cog height=.003435
# DESC: 
# BBOX: -0.042717 -0.015112 -0.011798 0.042717 0.015112 0.011798
+tklimb           _limb              model3d=tklimb.3do size=.05 movesize=.01
# DESC: 
# BBOX: -0.042717 -0.015112 -0.011798 0.042717 0.015112 0.011798
+thlimb           _limb              model3d=thlimb.3do size=.05 movesize=.01
# DESC: 
# BBOX: 0 0 0 0 0 0
+rycw_punch       +ry_punch          damage=5
# DESC: 
# BBOX: -0.046863 -0.025046 -0.131246 0.045997 0.027318 0.073257
reeyeescw         reeyeesfist        weapon=+rycw_punch typeflags=0x1020001
# DESC: 
# BBOX: 0 0 0 0 0 0
_shard            none               orient=(0/0/0) type=cog move=physics mass=.05 physflags=0x221
# DESC: 
# BBOX: -0.03133 -1E-6 -0.038594 0.031989 1E-6 0.027215
shard00           _shard             timer=1 model3d=shard0.3do size=.042 movesize=.042
# DESC: 
# BBOX: -0.125 -0.05 -0.3 0.125 0.05 0.3
strut06           _structure         model3d=strut06.3do size=.328824 movesize=.328824
# DESC: 
# BBOX: -0.021527 -0.011397 -0.102055 0.021527 0.014719 0.057565
woman2            _civilian          thingflags=0x20000400 model3d=w2.3do size=.056 movesize=.056 puppet=w2.pup soundclass=wm1.snd cog=actor_pf.cog maxvel=.2 health=50 maxhealth=50 maxthrust=.5 maxrotthrust=60 aiclass=peddefault.ai
# DESC: 
# BBOX: -0.037806 -0.014831 -0.120834 0.037548 0.016412 0.065442
weequay           _humanactor        thingflags=0x20000400 model3d=wq.3do size=.0649 movesize=.0649 puppet=tkstr.pup soundclass=wqstr.snd cog=actor_wq_pnch.cog mass=120 maxvel=.2 weapon=+elaser weapon2=+st_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(.0416/.0682/.0225) aiclass=wq_def.ai
# DESC: 
# BBOX: -0.033249 -0.025869 -0.100835 0.033234 0.06466 0.062592
u2                ug                 model3d=u2alt.3do puppet=u2.pup aiclass=pedhome.ai
# DESC: 
# BBOX: -0.07604212 -0.07113472 -0.05297579 0.06296778 0.08321835 0.05456368
tuskensoused      _ghostdecor        move=none model3d=thout.3do size=.22749 movesize=.22749 puppet=cr.pup
# DESC: 
# BBOX: -0.187173 -0.171934 -0.041875 0.196141 0.200354 0.037814
fan1              none               orient=(0/0/0) type=cog collide=1 move=physics model3d=fan1.3do size=.005 movesize=.005 physflags=0x200 maxrotvel=90 angvel=(0/90/0)
# DESC: 
# BBOX: -0.037244 -0.032884 -0.063574 0.037244 0.032884 0.063574
chair04           _walkstruct        thingflags=0x48 model3d=chr4.3do size=.124181 movesize=.124181
# DESC: 
# BBOX: -0.046863 -0.025046 -0.131246 0.045997 0.027318 0.073257
reeyeesbhfist     reeyeesfist        model3d=rh.3do health=60 maxhealth=60 maxrotthrust=85
# DESC: 
# BBOX: -0.035537 -0.014648 -0.128737 0.035536 0.025457 0.065502
man1              _civilian          thingflags=0x20000400 model3d=mn.3do size=.06 movesize=.06 puppet=mn.pup soundclass=mn1.snd cog=actor_pm.cog maxvel=.2 health=50 maxhealth=50 maxthrust=.5 maxrotthrust=60 aiclass=peddefault.ai
# DESC: 
# BBOX: -0.030754 -0.015261 -0.124016 0.030821 0.02207 0.067788
seethreepio       _droidactor        thingflags=0x20000400 model3d=c3.3do size=.0678 movesize=.0678 puppet=c3.pup soundclass=c3po.snd cog=actor_c3po.cog mass=120 maxvel=.4 health=50 maxhealth=50 maxthrust=.3 maxrotthrust=50 typeflags=0x180140 aiclass=drdefault.ai
# DESC: 
# BBOX: -0.046863 -0.025046 -0.131246 0.045997 0.027318 0.073257
reeyeesbh         reeyees            model3d=rh.3do health=60 maxhealth=60 maxrotthrust=80
# DESC: 
# BBOX: -0.046863 -0.025046 -0.131246 0.045997 0.027318 0.073257
reeyeesbhs        reeyeesbh          weapon=+actor_grenade2
# DESC: 
# BBOX: -0.8 -0.016042 -0.325 0.8 0.016042 0.325
garagedoor        _structure         model3d=grgd.3do size=.913645 movesize=.913645 soundclass=lg_door.snd
# DESC: 
# BBOX: -0.040769 -0.082095 -0.087667 0.040769 0.082487 0.086583
powercoupling     _walkstruct        model3d=pcoup.3do size=.170373 movesize=.170373
# DESC: 
# BBOX: -0.125 -0.05 -0.15 0.125 0.05 0.15
strut03           _structure         model3d=strut03.3do size=.201556 movesize=.201556
# DESC: 
# BBOX: -0.046863 -0.025046 -0.131246 0.045997 0.027318 0.073257
reeyeesbhgun      reeyeesgun         model3d=rhstr.3do health=60 maxhealth=60 maxrotthrust=80 aiclass=rybdefault.ai
# DESC: 
# BBOX: -0.021089 -0.023005 -0.035347 0.016307 0.023005 0.035347
smbp              _powerup           thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk.cog
# DESC: 
# BBOX: -0.025123 -0.01744 -0.127094 0.025123 0.015562 0.056718
woman1            _civilian          thingflags=0x20000400 model3d=wm.3do size=.056 movesize=.056 puppet=wm.pup soundclass=wm1.snd cog=actor_pf.cog maxvel=.2 health=50 maxhealth=50 maxthrust=.5 maxrotthrust=60 aiclass=peddefault.ai
# DESC: 
# BBOX: -0.025123 -0.01744 -0.127094 0.025123 0.015562 0.056718
nwoman1           woman1             cog=actor_fn.cog aiclass=pednarsh.ai
# DESC: 
# BBOX: -0.035537 -0.014648 -0.128737 0.035536 0.025457 0.065502
nman1             man1               cog=actor_mn.cog aiclass=pednarsh.ai
# DESC: 
# BBOX: -0.032373 -0.017479 9.7E-5 0.031819 0.017479 0.079704
bluewrenchkey     _powerup           thingflags=0x400 model3d=keyblu.3do cog=pow_wrenchblue.cog
# DESC: 
# BBOX: -0.72683 -0.540973 -0.382056 0.727153 0.325372 0.815553
jade'sshrike_park _zwalkstruct       model3d=jphnxprk.3do size=1 movesize=1 puppet=cr.pup
# DESC: 
# BBOX: -0.369375 -0.418438 -0.086562 0.369375 0.418437 0.086562
r41               _zwalkstruct       model3d=r41_1.3do size=.514784 movesize=.514784
# DESC: 
# BBOX: -0.036499 -0.015206 -0.119568 0.03663 0.02734 0.066416
abronmar          _humanactor        thingflags=0x20000400 model3d=ab.3do size=.0649 movesize=.0649 puppet=ab.pup soundclass=tkstr.snd cog=actor_tb.cog mass=120 maxvel=.2 weapon=+elaser health=100 maxhealth=100 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x1 fireoffset=(.0416/.0682/.0225) aiclass=tudefault.ai
# DESC: 
# BBOX: -0.61214 -0.74676 -0.13843 0.61214 0.74676 0.13843
z95               _zwalkstruct       model3d=z95_1.3do size=.956309 movesize=.956309
# DESC: 
# BBOX: -0.021089 -0.023005 -0.035347 0.016307 0.023005 0.035347
tim's_bkpk10      _powerup           thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk_10m.cog
# DESC: 
# BBOX: -0.8 -0.016042 -0.325 0.8 0.016042 0.325
garagedoor1       _structure         model3d=grgd.3do size=.913645 movesize=.913645 soundclass=lg_door.snd
# DESC: 
# BBOX: -0.021527 -0.011397 -0.102055 0.021527 0.014719 0.057565
nwoman2           woman2             cog=actor_fn.cog aiclass=pednarsh.ai
# DESC: 
# BBOX: -0.03618964 -0.01917877 -0.1247011 0.02409064 0.06395282 0.0434061
stormtroop3       _humanactor        thingflags=0x20000400 model3d=s3.3do puppet=s3.pup soundclass=st3.snd cog=actor_i2_pnch.cog maxvel=.25 weapon=+eball weapon2=+st_punch health=70 maxhealth=70 maxthrust=.9 maxrotthrust=90 typeflags=0x20001 fireoffset=(.0098/.057/.01) aiclass=streapdefault.ai
# DESC: 
# BBOX: -0.037806 -0.014831 -0.120834 0.037548 0.016412 0.065442
tuskangun         _humanactor        thingflags=0x20000400 model3d=tkstr.3do size=.0649 movesize=.0649 puppet=tkstr.pup soundclass=tkstr.snd cog=actor_tb_pnch.cog mass=120 maxvel=.2 weapon=+elaser weapon2=+st_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(-.0416/.0682/.0225) aiclass=tudefault.ai
# DESC: 
# BBOX: -0.03954 -0.01907 -0.133437 0.039738 0.026146 0.061238
man2              _civilian          thingflags=0x20000400 model3d=m2.3do size=.06 movesize=.06 puppet=m2.pup soundclass=mn1.snd cog=actor_pm.cog maxvel=.2 health=50 maxhealth=50 maxthrust=.5 maxrotthrust=60 aiclass=peddefault.ai
# DESC: 
# BBOX: -0.212425 -0.229246 -0.123998 0.212425 0.229246 0.123998
duiskiff          _zwalkstruct       model3d=dui.3do size=.374132 movesize=.374132
# DESC: 
# BBOX: -0.122086 -0.117592 -0.00067 0.122086 0.117592 0.295542
tarp              _walkstruct        thingflags=0x800048 model3d=tarp.3do size=.390702 movesize=.390702
# DESC: 
# BBOX: -0.122086 -0.117592 -0.148106 0.122086 0.117592 0.148106
tarp2             _walkstruct        thingflags=0x800048 model3d=tarp2.3do size=.247606 movesize=.247606
# DESC: 
# BBOX: -0.063294 -0.054836 -0.040528 0.063294 0.054836 0.040528
table             _decor             model3d=01tabl.3do size=.065 movesize=.065
# DESC: 
# BBOX: -0.165685 -0.006493 -0.2 0.165685 0.006493 0.2
tridoor           _walkstruct        thingflags=0x48 model3d=trid.3do size=.309796 movesize=.309796 soundclass=med_door.snd
# DESC: 
# BBOX: 0 0 0 0 0 0
+econc_exp        +conc_exp          damage=60
# DESC: 
# BBOX: -0.01341 -0.149802 -0.01341 0.01341 0.015603 0.01341
+econcbullet      +concbullet        explode=+econc_exp fleshhit=+econc_exp damage=10
# DESC: 
# BBOX: 0 0 0 0 0 0
+bosskswipe       +gamaxe            damage=20 range=.2
# DESC: 
# BBOX: -0.040341 -0.017317 -0.123441 0.040266 0.112852 0.068602
bossk             _humanactor        thingflags=0x20000400 model3d=bs.3do size=.0681 movesize=.0681 puppet=bs.pup soundclass=bs.snd cog=actor_bs_pnch.cog mass=120 maxvel=.4 weapon=+econcbullet weapon2=+bosskswipe health=120 maxhealth=120 maxthrust=.9 maxrotthrust=80 typeflags=0x20a0001 fireoffset=(-.039/.07/.027) aiclass=bodefault.ai
# DESC: 
# BBOX: -0.125 -0.200163 -0.049948 0.125 0.200163 0.072163
bed               _walkstruct        thingflags=0x800048 model3d=bed.3do size=.291216 movesize=.291216
# DESC: 
# BBOX: -0.040341 -0.017317 -0.123441 0.041996 0.078346 0.068602
bosskrld          bossk              model3d=bsrld.3do soundclass=bsrld.snd cog=actor_bsrd_pnch.cog weapon=+eraildet fireoffset=(-.035/.07/.027) aiclass=bldefault.ai
# DESC: 
# BBOX: -0.025123 -0.01744 -0.127094 0.025123 0.015562 0.056718
mwoman1           woman1             cog=none
# DESC: 
# BBOX: -1.362535 -1.904275 -0.304891 1.362534 1.904275 0.304891
falcon            _zwalkstruct       model3d=falcon1.3do size=2.13595 movesize=2.13595
# DESC: 
# BBOX: -0.05 -0.05 -0.049999 0.05 0.05 0.049999
crate4_2          _walkstruct        model3d=crt4-2.3do size=.086602 movesize=.086602
# DESC: 
# BBOX: -0.8 -0.025 -0.25 0.8 0.025 0.25
garagedoor2       _walkstruct        model3d=gdoor-2.3do size=.922378 movesize=.922378
# DESC: 
# BBOX: -0.213014 -0.198738 -0.152333 0.213014 0.198738 0.152333
duiskiff2         _zwalkstruct       model3d=dui2.3do size=.325555 movesize=.325555
# DESC: 
# BBOX: -0.03954 -0.01907 -0.133437 0.039738 0.026146 0.061238
nman2             man2               cog=actor_mn.cog aiclass=pednarsh.ai
# DESC: 
# BBOX: -0.030347 -0.072704 -0.024787 0.009211 0.072704 0.024341
+dconcrifle       _droppowerup       thingflags=0x400 model3d=conp.3do cog=pow_dconcrifle_m.cog height=.024753
# DESC: 
# BBOX: -0.043832 -0.019016 -0.007017 0.043832 0.019016 0.007017
+bslimb           _limb              model3d=bslimb.3do size=.05 movesize=.01
# DESC: 
# BBOX: -0.00929 -0.00375 -0.04177 0.00929 0.00375 0.04177
strongholdkey     _powerup           thingflags=0x408 model3d=dkey.3do cog=pow_prisonkey.cog
# DESC: 
# BBOX: 0 0 0 0 0 0
+ra_claw1         _weapon            size=.001 movesize=.001 mass=100 explode=+axe_exp fleshhit=+axe_exp damage=80 damageclass=0x80 typeflags=0x200d range=.5 force=120
# DESC: 
# BBOX: 0 0 0 0 0 0
+ra_bite          _weapon            size=.001 movesize=.001 mass=100 explode=+axe_exp fleshhit=+axe_exp damage=40 damageclass=0x80 typeflags=0x200d range=.4 force=100
# DESC: 
# BBOX: -0.1955387 -0.1843584 -0.1315414 0.2063856 0.3681681 0.2814138
rancor            _humanactor        collide=2 thingflags=0x4 model3d=ra.3do size=.13 movesize=.13 treesize=1 puppet=rancor.pup soundclass=ra.snd cog=actor_ra.cog mass=3000 maxvel=.6 weapon=+ra_claw1 weapon2=+ra_bite health=500 maxhealth=500 maxthrust=1 maxrotthrust=25 typeflags=0x20101 aiclass=ra_def.ai
# DESC: 
# BBOX: -0.2 -0.0125 -0.15 0.2 0.0125 0.15
prisonservicedoor _structure         model3d=24pdoor.3do size=.300312 movesize=.300312 soundclass=med_door.snd
# DESC: 
# BBOX: -0.15 -0.011875 -0.15 0.15 0.011875 0.15
droiddoor         _structure         model3d=24ddoor.3do size=.262464 movesize=.262464 soundclass=sm_door.snd
# DESC: 
# BBOX: -0.038064 -0.024511 -0.05829 0.038064 0.024511 0.05829
console2          _walkstruct        model3d=con2.3do size=.073806 movesize=.073806
# DESC: 
# BBOX: -0.15 -1 -0.012306 0.15 1 0.012306
3x20catwalk       _walkstruct        model3d=c3x20.3do size=1.061262 movesize=1.061262
# DESC: 
# BBOX: -0.45 -0.0375 -0.15 0.45 0.0375 0.15
door9x3           _walkstruct        model3d=d9x3.3do size=.525822 movesize=.525822 soundclass=09accessdoor.snd
# DESC: 
# BBOX: -0.060504 -0.0375 -0.025 0.060504 0.0375 0.025
radiolisdev       _powerup           thingflags=0x400 model3d=radiold.3do cog=pow_radio.cog
# DESC: 
# BBOX: -0.2 -0.198942 -0.2 0.2 0.201059 0.2
crate4_6          _walkstruct        model3d=crt4-6.3do size=.347022 movesize=.347022
# DESC: 
# BBOX: -0.029373 -0.010613 -0.030526 0.029373 0.010613 0.030526
prybarkey         _powerup           thingflags=0x400 model3d=pb_5.3do cog=pow_prybar.cog
# DESC: 
# BBOX: -0.046863 -0.025046 -0.131246 0.045997 0.027318 0.073257
reeyeespris       reeyeesfist        health=10
# DESC: 
# BBOX: -0.018462 -0.030983 -0.033409 0.018462 0.030983 0.033409
droidarmkey       _powerup           thingflags=0x400 model3d=pbkey.3do cog=pow_droidarm.cog
# DESC: 
# BBOX: -0.036362 -0.017961 -0.120075 0.036864 0.02734 0.067086
maraactor         _humanactor        model3d=mj-0.3do puppet=mj_cuts.pup soundclass=kyfemale.snd maxvel=.4 health=20000 maxhealth=20000 maxthrust=1 maxrotthrust=60 aiclass=jan.ai
# DESC: 
# BBOX: -0.016813 -0.025219 -0.025219 0.016813 0.025219 0.025219
bottle02          _structure         model3d=bottle2.3do size=.089429 movesize=.089429
# DESC: 
# BBOX: -0.016298 -0.015506 -0.024816 0.016298 0.015506 0.024816
bottle04          _structure         model3d=bottle4.3do size=.080942 movesize=.080942
# DESC: 
# BBOX: -0.021836 -0.04524 -0.000124 0.021836 0.037848 0.034517
bones             _decor             collide=0 model3d=bones.3do size=.05954 movesize=.05954
# DESC: 
# BBOX: -0.021878 -0.019871 -0.010012 0.021878 0.019871 0.010012
droidpart1        _structure         model3d=r5part.3do size=.074778 movesize=.074778
# DESC: 
# BBOX: -0.030034 -0.034716 -0.007363 0.030034 0.034716 0.007363
droidpart2        _structure         model3d=c3limb.3do size=.095514 movesize=.095514
# DESC: 
# BBOX: -0.018064 -0.014288 -0.007564 0.018064 0.014288 0.007564
droidpart3        _structure         model3d=pdpart.3do size=.073882 movesize=.073882
# DESC: 
# BBOX: -0.400093 -0.798777 0.059504 0.400093 0.801303 0.10949
8x16elev          _walkstruct        model3d=e8x16.3do size=.952302 movesize=.952302 soundclass=lg_elev.snd
# DESC: 
# BBOX: -0.496075 -0.67564 -0.137442 0.495972 0.67564 0.1016
xwing_land        _zwalkstruct       model3d=xwnglnd.3do size=.813117 movesize=.813117
# DESC: 
# BBOX: -0.597832 -0.597465 -0.299701 0.597179 0.59758 0.299701
00crate           _walkstruct        model3d=00crte.3do size=.830224 movesize=.830224
# DESC: 
# BBOX: -0.21867 -0.194901 -0.204409 0.21867 0.194901 0.204409
puzzlecrate       _walkstruct        model3d=crtz.3do size=.392911 movesize=.392911
# DESC: 
# BBOX: -0.222831 -0.019392 -0.250005 0.222831 0.019392 0.250005
puzzlelid2        _walkstruct        model3d=crtzlid2.3do size=.385459 movesize=.385459
# DESC: 
# BBOX: -1.2 -1.8 -0.3 1.2 1.8 0.3
tetriscrate       _walkstruct        model3d=00tetris.3do size=2.234033 movesize=2.234033
# DESC: 
# BBOX: -3.848692 -19.22806 -7.892384 3.848692 19.22806 7.892383
nebblo            _zwalkstruct       model3d=neb_lo1.3do size=21.991705 movesize=21.991705
# DESC: 
# BBOX: 0 0 0 0 0 0
_ghoststructure   _structure         collide=0
# DESC: 
# BBOX: -0.356182 -1.481516 -0.301418 0.356182 1.481516 0.301418
dockingtube0      _ghoststructure    model3d=dtube0.3do size=1.573731 movesize=1.573731
# DESC: 
# BBOX: -0.471765 -1.495065 -0.399229 0.471765 1.495065 0.399229
dockingtube1      _ghoststructure    model3d=dtube1.3do size=1.617731 movesize=1.617731
# DESC: 
# BBOX: -0.95 -0.04 -1.005 0.95 0.04 1.005
12_door3          _structure         model3d=12dr03.3do size=1.433519 movesize=1.433519 soundclass=lg_door.snd
# DESC: 
# BBOX: -0.051037 -0.040052 -0.175591 0.051037 0.040052 0.175591
s4l1coolingrod    _structure         model3d=coolrod.3do size=.232858 movesize=.232858
# DESC: 
# BBOX: 0 0 0 0 0 0
light0.1          _ghostdecor        thingflags=0x1 light=.1
# DESC: 
# BBOX: -0.065678 -0.075809 -0.044765 0.065678 0.075809 0.044765
gametable         _walkstruct        model3d=game.3do size=.138039 movesize=.138039
# DESC: 
# BBOX: -0.054707 -0.058748 -0.085259 0.054707 0.058748 0.085259
floatingdeadsoldier _decor             move=physics model3d=rc-float.3do size=.09 movesize=.09 surfdrag=.7 airdrag=1 mass=170 physflags=0x422c angvel=(22.5/11.25/22.5)
# DESC: 
# BBOX: -0.026466 -0.012833 -0.099682 0.026261 0.023106 0.061024
pirateraider4     _humanactor        thingflags=0x20000400 model3d=p4.3do size=.0615 movesize=.0615 puppet=p2.pup soundclass=pr.snd cog=actor_pr.cog mass=70 maxvel=.3 weapon=+elaser maxrotthrust=80 typeflags=0x2080001 fireoffset=(.016/.065/.004) aiclass=pr_def.ai
# DESC: 
# BBOX: -0.037806 -0.014831 -0.120834 0.041996 0.084407 0.065442
pirateraider3     _humanactor        thingflags=0x20000400 model3d=p3.3do size=.0649 movesize=.0649 puppet=tk.pup soundclass=prrld.snd cog=actor_pr3_pnch.cog mass=120 maxvel=.2 weapon=+eraildet weapon2=+st_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(.0416/.0702/.0225) aiclass=pr3_def.ai
# DESC: 
# BBOX: -0.06870934 -0.09956504 -0.1802985 0.06801872 0.08295557 -0.04085019
deadrebelofficer2 _ghostdecor        model3d=ro-dead2.3do size=.174602 movesize=.174602
# DESC: 
# BBOX: -0.037806 -0.014831 -0.120834 0.041996 0.084407 0.065442
piratebattler3    pirateraider3      aiclass=pb3default.ai
# DESC: 
# BBOX: -0.026466 -0.012833 -0.099682 0.026261 0.023106 0.061024
piratebattler4    pirateraider4      aiclass=pb2default.ai
# DESC: 
# BBOX: -0.125 -0.125 -0.150003 0.125 0.125 0.100004
crate_push        _walkstruct        move=physics model3d=crt-push.3do size=.246224 movesize=.15 surfdrag=2 mass=150 height=.15 physflags=0x425d
# DESC: 
# BBOX: -0.148021 -0.200008 -0.199979 0.148021 0.200008 0.199979
funicular         _walkstruct        model3d=01func.3do size=.369225 movesize=.369225 soundclass=funicular.snd
# DESC: 
# BBOX: -0.038893 -0.037552 -0.024816 0.039144 0.036124 0.024816
bottles04         _structure         collide=1 model3d=bottles4.3do size=.05 movesize=.05
# DESC: 
# BBOX: -0.15875 -0.30226 -0.05207 0.15875 0.30226 0.05207
xwing_debris_0    _debris            model3d=xwng_prt.3do size=.325479 movesize=.325479
# DESC: 
# BBOX: -0.012965 -0.007189 -0.036159 0.012965 0.007189 0.036159
s4l1switch0       _powerup           thingflags=0x10 model3d=levr-1.3do cog=s4l1_forceswitch0.cog
# DESC: 
# BBOX: -0.012965 -0.007189 -0.036159 0.012965 0.007189 0.036159
platformcontrolswitch _ghostdecor        model3d=levr-1.3do size=.042988 movesize=.042988
# DESC: 
# BBOX: -0.1 -0.0125 -0.1 0.1 0.0125 0.1
09_2x2door        _structure         model3d=09d2x2_1.3do size=.141973 movesize=.141973 soundclass=sm_door.snd
# DESC: 
# BBOX: -0.238012 -0.5 -0.077109 0.238012 0.5 0.077109
s4l1coridoor      _walkstruct        model3d=41cord.3do size=.609102 movesize=.609102 soundclass=catwalkfall.snd
# DESC: 
# BBOX: -0.01264 -0.049041 -0.01129 0.01264 0.049041 0.01129
shortrod          _structure         collide=1 model3d=41rod.3do size=.05 movesize=.05
# DESC: 
# BBOX: -0.01264 -0.299708 -0.01129 0.01264 0.299708 0.01129
rod02             _structure         model3d=rod02.3do size=.350187 movesize=.350187
# DESC: 
# BBOX: -0.199901 -0.025655 -0.200152 0.199901 0.025655 0.200152
quadwindow        _ghoststructure    model3d=gwndw_2.3do size=.268611 movesize=.268611
# DESC: 
# BBOX: -0.15 -0.15 -0.150003 0.15 0.15 0.150003
crate4_4          _walkstruct        model3d=crt4-4.3do size=.25981 movesize=.25981
# DESC: 
# BBOX: -0.020439 -0.013603 -0.012042 0.020439 0.013603 0.012042
fivepellets       _cpowerup          thingflags=0x400 model3d=cbnpk5.3do cog=pow_carbpellets_m.cog
# DESC: 
# BBOX: -0.026466 -0.012833 -0.099682 0.026261 0.023106 0.061024
pirateraider2     _humanactor        thingflags=0x20000400 model3d=p2.3do size=.0615 movesize=.0615 puppet=p2.pup soundclass=pr.snd cog=actor_pr.cog mass=70 maxvel=.3 weapon=+ebolt maxrotthrust=80 typeflags=0x2080001 fireoffset=(.016/.055/.004) aiclass=pr_def.ai
# DESC: 
# BBOX: -0.09999 -0.199962 -0.031081 0.09999 0.199962 0.031081
2x4elev           _walkstruct        model3d=e2x4.3do size=.225719 movesize=.225719 soundclass=med_elev.snd
# DESC: 
# BBOX: -0.03448359 -0.022835 -0.1202637 0.04545573 0.021911 0.07040831
twoonebee         none               orient=(0/0/0) type=cog collide=1 thingflags=0x400 model3d=med.3do size=.06 movesize=.06 puppet=cr.pup cog=00_twoonebee.cog
# DESC: 
# BBOX: -0.037806 -0.014831 -0.120834 0.037548 0.016412 0.065442
pirateraider1     _humanactor        thingflags=0x20000400 model3d=p1.3do size=.0649 movesize=.0649 puppet=tkstr.pup soundclass=prstr.snd cog=actor_pr1_pnch.cog mass=120 maxvel=.2 weapon=+elaser weapon2=+st_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(.0416/.0682/.0225) aiclass=pr1_def.ai
# DESC: 
# BBOX: -0.123035 -0.075245 -0.189945 0.123034 0.145052 0.179595
loaderdroid2      _droidactor        thingflags=0x4 model3d=lb2.3do size=.18 movesize=.18 puppet=lb2.pup soundclass=lb.snd cog=actor_gg.cog mass=750 weapon=+gamaxe health=100 maxhealth=100 maxrotthrust=80 typeflags=0x1a8141 aiclass=ld2default.ai
# DESC: 
# BBOX: -0.026466 -0.012833 -0.099682 0.026261 0.023106 0.061024
piratebattler2    pirateraider2      aiclass=pb2default.ai
# DESC: 
# BBOX: -0.035317 -0.025605 -0.047024 0.035366 0.022343 0.048893
r2_good           r2                 aiclass=drgdefault.ai
# DESC: 
# BBOX: -0.03677 -0.021965 -0.007763 0.03677 0.027597 0.007763
+p1limb           _limb              model3d=p1limb.3do size=.07 movesize=.01
# DESC: 
# BBOX: 0 0 0 0 0 0
+xwing_exp        +tie_exp           debris=xwing_debris_0
# DESC: 
# BBOX: -0.597832 -0.597465 -0.299701 0.597179 0.59758 0.299701
00archcrate       _walkstruct        model3d=00crte.3do size=.830224 movesize=.830224
# DESC: 
# BBOX: -1.58115 -2.28346 -1.066165 1.58115 2.28346 1.066165
transb            _zwalkstruct       model3d=transb1.3do size=2.719194 movesize=2.719194
# DESC: 
# BBOX: -0.597832 -0.597465 -0.299701 0.597179 0.59758 0.299701
00zpicrate        _zwalkstruct       model3d=00picrte.3do size=.858247 movesize=.858247 soundclass=funicular.snd
# DESC: 
# BBOX: -0.057012 -0.079857 -0.050443 0.057012 0.079857 0.050443
constructiondroid _flyactor          thingflags=0x20000400 model3d=cdrd.3do size=.055 movesize=.055 soundclass=rs.snd cog=actor_sd.cog mass=30 physflags=0x2006a06 maxvel=.6 weapon=+ebolt health=60 maxhealth=60 maxthrust=.8 maxrotthrust=80 typeflags=0x2108120 fireoffset=(-.0073/.0393/0) explode=+sentry_exp aiclass=sddefault.ai
# DESC: 
# BBOX: -0.053401 -0.061619 -0.061702 0.053401 0.061619 0.061702
crate6_2          _walkstruct        thingflags=0x400448 model3d=crt6-2.3do size=.087201 movesize=.087201 cog=actor_cr.cog
# DESC: 
# BBOX: -0.181678 -0.060004 -0.073819 0.181678 0.060004 0.073819
roofvent3         _walkstruct        model3d=01rfd3.3do size=.249375 movesize=.249375
# DESC: 
# BBOX: -0.228837 -0.19266 -0.30048 0.228837 0.19266 0.30048
s4l3clamp         _decor             model3d=43clamp.3do size=.26 movesize=.26
# DESC: 
# BBOX: -0.15 -0.025 -0.1 0.15 0.025 0.1
3x2door_2         _zwalkstruct       model3d=d3x2_2.3do size=.232003 movesize=.232003 soundclass=sm_door.snd
# DESC: 
# BBOX: -0.2 -0.5 -0.025 0.2 0.5 0.025
forklift          _zwalkstruct       model3d=fklift.3do size=.589096 movesize=.589096 soundclass=lg_elev.snd
# DESC: 
# BBOX: -0.061517 -0.037623 -0.094972 0.061517 0.072526 0.089798
loaderdroid       _droidactor        thingflags=0x20000400 model3d=lb.3do size=.09 movesize=.09 puppet=lb.pup soundclass=lb.snd cog=actor_gg.cog mass=250 weapon=+gamaxe health=100 maxhealth=100 maxrotthrust=80 typeflags=0x1a8141 aiclass=lddefault.ai
# DESC: 
# BBOX: -0.061517 -0.037623 -0.094972 0.061517 0.072526 0.089798
loaderdroids4l2   loaderdroid        cog=actor_ggs4l2.cog
# DESC: 
# BBOX: -0.2 -0.0125 -0.2 0.2 0.0125 0.2
00expwall1a       _walkstruct        model3d=00wexp1a.3do size=.333119 movesize=.333119
# DESC: 
# BBOX: -0.242535 -0.449297 -0.033361 0.510302 0.456385 0.033361
s4l2fan           2x3door_1          model3d=s4l2fan.3do size=.475 movesize=.475 puppet=cr.pup
# DESC: 
# BBOX: -0.40002 -0.400093 -0.024993 0.40002 0.400093 0.024993
8x8elevs4l2       _walkstruct        thingflags=0x400008 model3d=e8x8.3do size=.616317 movesize=.616317 soundclass=lg_elev.snd
# DESC: 
# BBOX: -0.032373 -0.017479 9.7E-5 0.031819 0.017479 0.079704
yellowwrenchkey   _powerup           thingflags=0x400 model3d=keyyel.3do cog=pow_wrenchyellow.cog
# DESC: 
# BBOX: -0.100779 -0.107544 0.000686 0.100779 0.107544 0.023173
ceilinglight2     _ghoststructure    model3d=clight0.3do size=.166487 movesize=.166487
# DESC: 
# BBOX: -0.021089 -0.023005 -0.035347 0.016307 0.023005 0.035347
+dsmbp            _droppowerup       thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk.cog height=.036337
# DESC: 
# BBOX: 0 0 0 0 0 0
+superenerg       none               orient=(0/0/0) type=particle move=physics timer=.6 vel=(0/0/2) angvel=(0/360/0) typeflags=0x3e material=00red.mat range=1.2 elementsize=.02 count=30
# DESC: 
# BBOX: 0 0 0 0 0 0
+superenerg2      +superenerg        range=.6
# DESC: 
# BBOX: -0.0254 -0.32385 -0.03048 0.0254 0.32385 0.03048
z95_debris_0      _debris            model3d=z95_1d0.3do size=.325479 movesize=.325479
# DESC: 
# BBOX: -0.4064 -0.42672 -0.13843 0.4064 0.42672 0.13843
z95_debris_1      _debris            model3d=z95_1d1.3do size=.584212 movesize=.584212
# DESC: 
# BBOX: -0.20193 -0.34925 -0.03048 0.20193 0.34925 0.03048
z95_debris_2      _debris            model3d=z95_1d2.3do size=.403432 movesize=.403432
# DESC: 
# BBOX: -0.25146 -0.60071 -0.08636 0.25146 0.60071 0.08636
z95_debris_3      _debris            model3d=z95_1d3.3do size=.623332 movesize=.623332
# DESC: 
# BBOX: 0 0 0 0 0 0
+z95_exp          +tie_exp           debris=z95_debris_0 debris=z95_debris_1 debris=z95_debris_2 debris=z95_debris_3
# DESC: 
# BBOX: 0 0 0 0 0 0
+03_exp           +raildet_exp       damage=25 force=60 debris=shrapnel_1 debris=shrapnel_2 debris=shrapnel_3 debris=shrapnel_4
# DESC: 
# BBOX: -0.2 -0.0125 -0.2 0.2 0.0125 0.2
00expwall1b       _walkstruct        model3d=00wexp1b.3do size=.333119 movesize=.333119
# DESC: 
# BBOX: -0.026466 -0.012833 -0.099682 0.026261 0.023106 0.061024
pirateped4        _humanactor        thingflags=0x20000400 model3d=p4.3do size=.0615 movesize=.0615 puppet=p2.pup soundclass=pr.snd cog=actor_prped4.cog mass=70 maxvel=.3 weapon=+elaser maxrotthrust=80 typeflags=0x2080001 fireoffset=(.016/.065/.004) aiclass=pr_def.ai
# DESC: 
# BBOX: -0.037806 -0.014831 -0.120834 0.037548 0.016412 0.065442
pirateped1        _humanactor        thingflags=0x20000400 model3d=p1.3do size=.0649 movesize=.0649 puppet=tkstr.pup soundclass=prstr.snd cog=actor_prped1.cog mass=120 maxvel=.2 weapon=+elaser weapon2=+st_punch health=50 maxhealth=50 maxthrust=.9 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 fireoffset=(.0416/.0682/.0225) aiclass=pr1_def.ai
# DESC: 
# BBOX: -0.163907 -0.240264 -0.151864 0.163907 0.240264 0.151864
tent              _zwalkstruct       thingflags=0x800040 model3d=tent.3do size=.355021 movesize=.355021
# DESC: 
# BBOX: 0 0 0 0 0 0
+ng_sword1        _weapon            size=.001 movesize=.001 mass=100 explode=+axe_exp fleshhit=+axe_exp damage=45 damageclass=0x1 typeflags=0x200d range=.26 force=50
# DESC: 
# BBOX: 0 0 0 0 0 0
+ng_sword2        _weapon            size=.001 movesize=.001 mass=100 explode=+axe_exp fleshhit=+axe_exp damage=60 damageclass=0x1 typeflags=0x200d range=.27 force=50
# DESC: 
# BBOX: -0.085128 -0.050675 -0.129376 0.16834 0.088949 0.086323
noghri            _humanactor        collide=2 thingflags=0x20000400 model3d=ng.3do size=.0775 movesize=.0775 treesize=.5 puppet=ng.pup soundclass=ng.snd cog=actor_ng.cog mass=300 weapon=+ng_sword1 weapon2=+ng_sword2 health=200 maxhealth=200 maxthrust=1 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 aiclass=ng_def.ai
# DESC: 
# BBOX: -0.149972 -0.150118 -0.099987 0.149972 0.150118 0.099987
puzzlecolumn      _walkstruct        model3d=clmz.3do size=.284573 movesize=.284573
# DESC: 
# BBOX: 0 0 0 0 0 0
_swimactor        _humanactor        physflags=0x2004a0f typeflags=0x20040
# DESC: 
# BBOX: 0 0 0 0 0 0
+drugonbite       +gamaxe            damage=50 range=.4
# DESC: 
# BBOX: -0.122593 -0.438462 -0.119063 0.144735 0.269351 0.112767
sucker            _swimactor         model3d=su.3do size=.33 movesize=.33 puppet=su.pup soundclass=sucker.snd mass=500 physflags=0x2404a0f maxvel=.6 weapon=+drugonbite health=160 maxhealth=160 maxthrust=.7 maxrotthrust=90 typeflags=0x20140 aiclass=dudefault.ai
# DESC: 
# BBOX: -0.0375 -0.0375 -0.00017 0.0375 0.0375 0.07483
holocroncube      _powerup           thingflags=0x408 model3d=holocube.3do cog=pow_holocron.cog mass=0 angvel=(0/90/0)
# DESC: 
# BBOX: -0.174123 -0.174123 -0.012502 0.174123 0.174123 0.012502
s5l4_disc         _walkstruct        thingflags=0x48 model3d=54epad.3do size=.192028 movesize=.192028
# DESC: 
# BBOX: -0.011495 -0.015768 -0.018985 0.011495 0.015768 0.018985
8t88_head2        none               orient=(0/0/0) type=cog collide=1 model3d=8tbighed.3do size=.073202 movesize=.073202
# DESC: 
# BBOX: -0.4 -0.050015 -0.398 0.4 0.050015 0.4
s4l3_8x8door      _walkstruct        model3d=d8x8.3do size=.617892 movesize=.617892 soundclass=lg_door.snd
# DESC: 
# BBOX: -0.012965 -0.007189 -0.036159 0.012965 0.007189 0.036159
s4l3switch0       _powerup           thingflags=0x10 model3d=levr-1.3do cog=s4l3_forceswitch0.cog
# DESC: 
# BBOX: -0.012965 -0.007189 -0.036159 0.012965 0.007189 0.036159
s4l3switch1       _powerup           thingflags=0x10 model3d=levr-1.3do cog=s4l3_forceswitch1.cog
# DESC: 
# BBOX: -0.012965 -0.007189 -0.036159 0.012965 0.007189 0.036159
s4l3switch2       _powerup           thingflags=0x10 model3d=levr-1.3do cog=s4l3_forceswitch2.cog
# DESC: 
# BBOX: -0.75 -0.499 -0.034859 0.75 0.499 0.034859
baydoor_a         _walkstruct        model3d=bayd_a.3do size=.951508 movesize=.951508
# DESC: 
# BBOX: -0.032914 -0.035695 -3.2E-5 0.032825 0.029144 0.056184
dtroophead        _decor             model3d=darktroophead.3do size=.063718 movesize=.063718
# DESC: 
# BBOX: -0.36642 -0.41509 -0.08587 0.36642 0.41509 0.08587
r41_flt           _zwalkstruct       model3d=r41_1_flt.3do size=.510666 movesize=.510666
# DESC: 
# BBOX: -0.39751 -0.94488 -0.28956 0.39751 0.94488 0.28956
transport_flt     _zwalkstruct       model3d=trnsprt1_flt.3do size=1.098054 movesize=1.098054
# DESC: 
# BBOX: 0 0 0 0 0 0
+upenergylo       none               orient=(0/0/0) type=particle move=physics timer=.8 vel=(0/0/5) typeflags=0x3e material=00red.mat range=.05 elementsize=.005 count=2
# DESC: 
# BBOX: 0 0 0 0 0 0
+mailocsting      +gamaxe            damage=15 force=25
# DESC: 
# BBOX: -0.080669 -0.095318 -0.091964 0.082473 0.08505 0.212669
mynok             _humanactor        thingflags=0x20000400 model3d=my.3do size=.1 movesize=.1 puppet=my.pup soundclass=mynoc.snd cog=actor_mailoc.cog mass=40 physflags=0x6a06 weapon=+mailocsting maxthrust=.9 maxrotthrust=70 typeflags=0x2020000 aiclass=maidefault.ai
# DESC: 
# BBOX: -0.075265 -0.064442 -0.151527 0.075265 0.064442 0.151527
zstatue           _zwalkstruct       thingflags=0x40 model3d=stat.3do size=.231047 movesize=.231047
# DESC: 
# BBOX: 0 0 0 0 0 0
+ws_claw1         _weapon            size=.001 movesize=.001 mass=100 explode=+axe_exp fleshhit=+axe_exp damage=40 damageclass=0x1 typeflags=0x200d range=.21 force=50
# DESC: 
# BBOX: 0 0 0 0 0 0
+ws_claw2         _weapon            size=.001 movesize=.001 mass=100 explode=+axe_exp fleshhit=+axe_exp damage=50 damageclass=0x1 typeflags=0x200d range=.22 force=50
# DESC: 
# BBOX: -0.088304 -0.080169 -0.161827 0.088612 0.02719 0.102677
wampaswamp        _humanactor        thingflags=0x20000400 model3d=ws.3do size=.1027 movesize=.1027 puppet=ws.pup soundclass=ws.snd cog=actor_ws.cog mass=250 weapon=+ws_claw1 weapon2=+ws_claw2 health=150 maxhealth=150 maxthrust=1 maxrotthrust=70 jumpspeed=1.2 typeflags=0x20001 aiclass=ws_def.ai
# DESC: 
# BBOX: -0.017157 -0.016142 -0.014361 0.017157 0.016142 0.014361
stgib1            _ghostdecor        model3d=stgib1.3do size=.069187 movesize=.069187
# DESC: 
# BBOX: -0.021089 -0.023005 -0.035347 0.016307 0.023005 0.035347
tim's_bkpk11      _powerup           thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk_11m.cog
# DESC: 
# BBOX: -0.2 -0.2 0 0.2 0.2 0
carpet            _walkstruct        thingflags=0x800048 model3d=carpet.3do size=.332843 movesize=.332843
# DESC: 
# BBOX: -0.161554 -0.23891 -0.384493 0.161554 0.23891 0.384493
treestump1-a      _zwalkstruct       thingflags=0x40 model3d=btr1-a.3do size=.504194 movesize=.504194
# DESC: 
# BBOX: -0.376358 -0.419191 -0.848055 0.376357 0.41919 0.848055
treestump2-a      _walkstruct        thingflags=0x48 model3d=btr2-a.3do size=1.012674 movesize=1.012674
# DESC: 
# BBOX: -0.021089 -0.023005 -0.035347 0.016307 0.023005 0.035347
tim's_bkpk2       _powerup           thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk_2m.cog
# DESC: 
# BBOX: -0.021089 -0.023005 -0.035347 0.016307 0.023005 0.035347
tim's_bkpk3       _powerup           thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk_3m.cog
# DESC: 
# BBOX: -0.016776 -0.034744 -0.010952 0.018316 0.034538 0.00282
ysalamari         _civilian          thingflags=0x0 model3d=ys.3do size=.0055 movesize=.0055 puppet=ys.pup soundclass=ug.snd cog=actor_ys.cog mass=60 maxvel=.4 health=30 maxthrust=.5 maxrotthrust=60 aiclass=peddefault.ai
# DESC: 
# BBOX: -0.040553 -0.199597 -0.201059 0.040553 0.199597 0.201059
s5l2vines         _decor             model3d=bvine.3do size=.15 movesize=.15
# DESC: 
# BBOX: -0.034756 -0.02133 -0.007677 0.034756 0.02133 0.007677
stgib3            _ghostdecor        model3d=stgib3.3do size=.08891 movesize=.08891
# DESC: 
# BBOX: -0.174338 -0.220588 -0.001853 0.174338 0.284947 0.817133
treestump1        _zwalkstruct       thingflags=0x40 model3d=btr1.3do size=.896716 movesize=.896716
# DESC: 
# BBOX: -0.03092 -0.030162 -0.030457 0.03092 0.030162 0.030457
lightside         _powerup           thingflags=0x400 model3d=mana1.3do cog=pow_darkside.cog respawn=60
# DESC: 
# BBOX: -0.180762 -0.209203 0.023289 0.180762 0.164571 0.135454
swhead            _zwalkstruct       collide=1 thingflags=0x800440 model3d=swhead.3do size=.22953 movesize=.26953 cog=actor_sl.cog
# DESC: 
# BBOX: -0.405101 -0.598233 -0.873055 0.405101 0.296312 0.873055
treestump2        _zwalkstruct       thingflags=0x40 model3d=btr2.3do size=1.124021 movesize=1.124021
# DESC: 
# BBOX: -0.1648281 -0.1410949 -0.1887941 0.1503295 0.09973905 0.1903286
dianoga           _swimactor         thingflags=0x400 model3d=di.3do size=.165 movesize=.165 puppet=di.pup soundclass=sucker.snd cog=actor_dnga.cog mass=380 physflags=0x2404a0f maxvel=.6 weapon=+drugonbite health=160 maxhealth=160 maxthrust=.7 maxrotthrust=90 typeflags=0x20140 aiclass=di_def.ai
# DESC: 
# BBOX: -0.004485 -0.017964 -0.004485 0.004485 0.017964 0.004485
+plantspittle     _weapon            model3d=vfspittl.3do size=.001 movesize=.001 soundclass=bry.snd maxrotvel=0 vel=(0/4/0) explode=+axe_exp fleshhit=+axe_exp damage=30 mindamage=10 damageclass=0x1 typeflags=0x20440d rate=15
# DESC: 
# BBOX: -0.058684 -0.017148 -0.124887 0.007903 0.018428 0.117391
dartflower        _humanactor        collide=2 model3d=vf.3do size=.075 movesize=.075 treesize=.2 puppet=vf.pup soundclass=vf.snd mass=0 weapon=+plantspittle health=50 maxhealth=50 typeflags=0x20001 aiclass=dfdefault.ai
# DESC: 
# BBOX: -0.174338 -0.180157 -0.025 0.174338 0.180157 0.025
treestump1-b      _ghostdecor        model3d=btr1-b.3do size=.254496 movesize=.254496
# DESC: 
# BBOX: -0.405101 -0.432609 -0.025 0.405101 0.432609 0.025
treestump2-b      _structure         model3d=btr2-b.3do size=.520307 movesize=.520307
# DESC: 
# BBOX: -0.300125 -0.025 -0.001357 0.300125 0.025 0.598643
52stonedoor       _walkstruct        model3d=52utdr.3do size=.720129 movesize=.720129 soundclass=19stone_door.snd
# DESC: 
# BBOX: 0 0 0 0 0 0
_darkjedi         _humanactor        typeflags=0x2000201
# DESC: 
# BBOX: -0.035332 -0.013185 -0.120103 0.035579 0.016413 0.064509
darkmara          _darkjedi          model3d=kydm.3do puppet=mjdarkjedi.pup soundclass=mj.snd mass=170 maxvel=.2 weapon=+gamaxe health=2000 maxhealth=2000 maxthrust=1 maxrotthrust=200 jumpspeed=4 typeflags=0x2008201 error=.1 fov=.4 chance=1 aiclass=darkmara.ai
# DESC: 
# BBOX: -0.035332 -0.01877 -0.120103 0.036644 0.016413 0.064197
darkmaraboss      darkmara           model3d=mjboss.3do
# DESC: 
# BBOX: -0.005058 0 -0.002337 0.005058 0 0.002337
s5l2eyes          _ghostdecor        model3d=52eye.3do size=.055058 movesize=.055058
# DESC: 
# BBOX: -0.037418 -0.063387 -0.017067 0.025236 0.03643 0.012634
+mylimb           _limb              model3d=mylimb.3do size=.07 movesize=.01
# DESC: 
# BBOX: -0.078691 -0.073436 -0.037355 0.048371 0.113983 0.029054
+wslimb           _limb              model3d=wslimb.3do size=.125978 movesize=.01
# DESC: 
# BBOX: 0 0 0 0 0 0
+cycbite          +gamaxe            damage=25 range=.5
# DESC: 
# BBOX: -0.06753 -0.017802 -0.714146 0.097985 0.078244 0.099411
+lurktentacles    _actor             thingflags=0x20000400 model3d=sw.3do size=.178 movesize=.178 puppet=sw.pup cog=actor_slt.cog mass=0 physflags=0x2404a0f maxvel=.6 weapon=+cycbite health=100 maxhealth=100 maxthrust=.4 maxrotthrust=60 typeflags=0x20140
# DESC: 
# BBOX: -0.300125 -0.025 -0.001357 0.300125 0.025 0.598643
undergroundtempledoor _structure         model3d=utdr.3do size=.720129 movesize=.720129 soundclass=54stonedoors.snd
# DESC: 
# BBOX: -0.03567052 -0.03644341 -0.1231553 0.05585343 0.026546 0.06314822
statue1           _humanactor        thingflags=0x404 model3d=s1.3do size=.063 movesize=.063 puppet=statue1.pup soundclass=s0.snd cog=actor_ss1.cog mass=170 maxrotvel=15 maxvel=.2 buoyancy=1 weapon=+gamaxe health=300 maxhealth=300 maxthrust=1 maxrotthrust=40 jumpspeed=4 typeflags=0x28201 error=.1 fov=-.4 chance=1 aiclass=s0i_def.ai
# DESC: 
# BBOX: -0.07839181 -0.04015309 -0.1206315 0.03211147 0.07995691 0.03870553
statue0           _humanactor        thingflags=0x404 model3d=s0.3do size=.0604 movesize=.0604 puppet=statue0.pup soundclass=s0.snd cog=actor_ss0.cog mass=160 maxvel=.3 buoyancy=1 weapon=+gamaxe health=300 maxhealth=300 maxrotthrust=40 jumpspeed=1.6 typeflags=0x20201 error=.6 fov=.5 chance=1 aiclass=s1i_def.ai
# DESC: 
# BBOX: -0.4 -0.05 -0.4 0.4 0.05 0.4
templedoorbasic   _structure         model3d=54d8x8.3do size=.617891 movesize=.617891 soundclass=54stonedoors.snd
# DESC: 
# BBOX: -0.027321 -0.032261 0.000219 0.027321 0.032261 0.126942
floatingchair     _decor             move=physics model3d=chair3m.3do size=.09 movesize=.09 surfdrag=.7 airdrag=1 mass=35 physflags=0x422c angvel=(22.5/11.25/22.5)
# DESC: 
# BBOX: -0.158991 -0.163886 -0.224502 0.158991 0.163886 0.224502
rock2mini         _walkstruct        thingflags=0x48 model3d=rck2mini.3do size=.369958 movesize=.369958
# DESC: 
# BBOX: 0 0 0 0 0 0
+vornbite         +gamaxe            damage=25 range=.15 force=100
# DESC: 
# BBOX: -0.06382909 -0.1247081 -0.07552449 0.06736379 0.1061798 0.07585242
vornskr           _humanactor        model3d=vr.3do size=.0685 movesize=.0685 puppet=vr.pup soundclass=vr.snd mass=300 maxvel=.6 weapon=+vornbite weapon2=+vornbite health=200 maxhealth=200 maxthrust=1 maxrotthrust=120 typeflags=0x2028100 aiclass=vrdefault.ai
# DESC: 
# BBOX: -0.1 -0.0125 -0.15 0.1 0.0125 0.15
s5l3door1         _structure         model3d=53d2x3.3do size=.23071 movesize=.23071 soundclass=03wood_door.snd
# DESC: 
# BBOX: -0.098284 -0.097923 -0.031081 0.097733 0.098093 0.031081
s5l3elev          _walkstruct        thingflags=0x48 model3d=53e2x2.3do size=.192295 movesize=.192295 soundclass=53stoneelev.snd
# DESC: 
# BBOX: -0.036449 -0.016107 -0.120075 0.037661 0.02734 0.064161
kyleactor         _humanactor        model3d=ky-0.3do puppet=mj_cuts.pup soundclass=ky.snd maxvel=.4 health=20000 maxhealth=20000 maxthrust=1 maxrotthrust=60 aiclass=jan.ai
# DESC: 
# BBOX: -0.2 -0.0125 -0.15 0.2 0.0125 0.15
s5l3door2         _structure         model3d=53d4x3.3do size=.300312 movesize=.300312 soundclass=03wood_door.snd
# DESC: 
# BBOX: -0.35 -0.625 -0.05 0.35 0.625 0.05
s5l3_beam0        _walkstruct        thingflags=0x48 model3d=53beam0.3do size=.76807 movesize=.76807
# DESC: 
# BBOX: -0.375 -0.25 -0.05 0.375 0.25 0.05
s5l3_beam1        _walkstruct        thingflags=0x48 model3d=53beam1.3do size=.503459 movesize=.503459
# DESC: 
# BBOX: -0.55 -1.025 -0.05 0.55 1.025 0.05
s5l3_beam2        _walkstruct        thingflags=0x48 model3d=53beam2.3do size=1.214313 movesize=1.214313
# DESC: 
# BBOX: -0.021089 -0.023005 -0.035347 0.016307 0.023005 0.035347
tim's_bkpk7       _powerup           thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk_7m.cog
# DESC: 
# BBOX: -0.021089 -0.023005 -0.035347 0.016307 0.023005 0.035347
tim's_bkpk8       _powerup           thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk_8m.cog
# DESC: 
# BBOX: -0.021089 -0.023005 -0.035347 0.016307 0.023005 0.035347
tim's_bkpk1       _powerup           thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk_1m.cog
# DESC: 
# BBOX: 0 0 0 0 0 0
_statuedebris     none               orient=(0/0/0) type=weapon collide=1 move=physics timer=15 airdrag=3 mass=30 physflags=0x201 typeflags=0x100
# DESC: 
# BBOX: -0.014366 -0.00976 -0.00814 0.014538 0.009633 0.007972
+statue1debris_1  _statuedebris      model3d=s1-d1.3do size=.008301 movesize=.008301
# DESC: 
# BBOX: -0.013278 -0.041702 -0.013703 0.013278 0.041702 0.013703
+statue1debris_2  _statuedebris      timer=20 model3d=s1-d2.3do size=.010207 movesize=.010207
# DESC: 
# BBOX: -0.015753 -0.015778 -0.011138 0.015753 0.015778 0.011138
+statue1debris_3  _statuedebris      timer=19 model3d=s1-d3.3do size=.012147 movesize=.012147
# DESC: 
# BBOX: -0.052358 -0.035478 -0.01031 0.055594 0.022557 0.01031
+statue1debris_4  _statuedebris      timer=18 model3d=s1-d6.3do size=.011599 movesize=.011599
# DESC: 
# BBOX: -0.018981 -0.026507 -0.014317 0.018981 0.026507 0.014317
+statue1debris_5  _statuedebris      timer=25 model3d=s1-d5.3do size=.015215 movesize=.015215
# DESC: 
# BBOX: -0.052358 -0.035478 -0.01031 0.055594 0.022557 0.01031
+statue1debris_6  _statuedebris      timer=10 model3d=s1-d6.3do size=.011599 movesize=.011599
# DESC: 
# BBOX: -0.001953 0.010832 -0.10415 0.003749 0.014182 -0.005192
s1blade           _ghostdecor        model3d=s1blade.3do size=.15497 movesize=.15497
# DESC: 
# BBOX: -0.015774 -0.012607 -0.008304 0.016083 0.009876 0.012121
+statue0debris_1  _statuedebris      model3d=s0-d1.3do size=.008301 movesize=.008301
# DESC: 
# BBOX: -0.013278 -0.041702 -0.013703 0.013278 0.041702 0.013703
+statue0debris_2  _statuedebris      timer=20 model3d=s0-d2.3do size=.010207 movesize=.010207
# DESC: 
# BBOX: -0.015753 -0.015778 -0.011138 0.015753 0.015778 0.011138
+statue0debris_3  _statuedebris      timer=19 model3d=s0-d3.3do size=.012147 movesize=.012147
# DESC: 
# BBOX: -0.052358 -0.035478 -0.01031 0.055594 0.022557 0.01031
+statue0debris_4  _statuedebris      timer=18 model3d=s0-d6.3do size=.011599 movesize=.011599
# DESC: 
# BBOX: -0.018981 -0.026507 -0.014317 0.018981 0.026507 0.014317
+statue0debris_5  _statuedebris      timer=25 model3d=s0-d5.3do size=.015215 movesize=.015215
# DESC: 
# BBOX: -0.052358 -0.035478 -0.01031 0.055594 0.022557 0.01031
+statue0debris_6  _statuedebris      timer=10 model3d=s0-d6.3do size=.011599 movesize=.011599
# DESC: 
# BBOX: -0.000719 0.01684 -0.117316 0.007319 0.027056 0.001032
s0blade           _ghostdecor        model3d=s0blade.3do size=.170383 movesize=.170383
# DESC: 
# BBOX: -0.4 -0.05 -0.4 0.4 0.05 0.4
templedoor1b      _structure         model3d=54d8x81b.3do size=.617891 movesize=.617891 soundclass=54stonedoors.snd
# DESC: 
# BBOX: -0.4 -0.05 -0.4 0.4 0.05 0.4
templedoor1a      _structure         model3d=54d8x81a.3do size=.617891 movesize=.617891 soundclass=54stonedoors.snd
# DESC: 
# BBOX: -0.4 -0.05 -0.4 0.4 0.05 0.4
templedoor3b      _structure         model3d=54d8x83b.3do size=.617891 movesize=.617891 soundclass=54stonedoors.snd
# DESC: 
# BBOX: -0.4 -0.05 -0.4 0.4 0.05 0.4
templedoor3a      _structure         model3d=54d8x83a.3do size=.617891 movesize=.617891 soundclass=54stonedoors.snd
# DESC: 
# BBOX: -0.4 -0.05 -0.4 0.4 0.05 0.4
templedoor2b      _structure         model3d=54d8x82b.3do size=.617891 movesize=.617891 soundclass=54stonedoors.snd
# DESC: 
# BBOX: -0.4 -0.05 -0.4 0.4 0.05 0.4
templedoor2a      _structure         model3d=54d8x82a.3do size=.617891 movesize=.617891 soundclass=54stonedoors.snd
# DESC: 
# BBOX: -0.4 -0.05 -0.4 0.4 0.05 0.4
templedoor4       _structure         model3d=54d8x84.3do size=.617891 movesize=.617891 soundclass=54stonedoors.snd
# DESC: 
# BBOX: -0.09999 -0.199962 -0.031081 0.09999 0.199962 0.031081
s5l4elev2x4       _walkstruct        thingflags=0x48 model3d=54e2x4.3do size=.275719 movesize=.275719 soundclass=54stoneelev.snd
# DESC: 
# BBOX: -0.148021 -0.200008 -0.199979 0.148021 0.200008 0.199979
s5l4funicular     _walkstruct        thingflags=0x48 model3d=sfun.3do size=.369225 movesize=.369225 soundclass=54stoneelev.snd
# DESC: 
# BBOX: -0.198 -0.197998 0.03296 0.198 0.198002 0.05971
s5l4elev4x4       _walkstruct        thingflags=0x48 model3d=54e4x4.3do size=.336311 movesize=.336311 soundclass=54stoneelev.snd
# DESC: 
# BBOX: -0.15 -1.875 -0.012306 0.15 1.875 0.012306
sithcatwalk       _zwalkstruct       thingflags=0x40 model3d=sthcwlk.3do size=2.069134 movesize=2.069134
# DESC: 
# BBOX: -0.174338 -0.220588 -0.001853 0.174338 0.284947 0.817133
s5l4treestump1    _walkstruct        thingflags=0x800048 model3d=54btr1.3do size=.896716 movesize=.896716
# DESC: 
# BBOX: -0.405101 -0.598233 -0.873055 0.405101 0.296312 0.873055
s5l4treestump2    _walkstruct        thingflags=0x800048 model3d=54btr2.3do size=1.124021 movesize=1.124021
# DESC: 
# BBOX: -0.075265 -0.064442 -0.151527 0.075265 0.064442 0.151527
54statue          _walkstruct        thingflags=0x48 model3d=54stat.3do size=.231047 movesize=.231047 soundclass=19stone_door.snd
# DESC: 
# BBOX: -0.068375 -0.11004 -0.03939 0.068375 0.11004 0.038931
coffin1           _walkstruct        thingflags=0x48 model3d=coff-1.3do size=.134448 movesize=.134448
# DESC: 
# BBOX: -0.2 -0.2 -0.8 0.2 0.2 0.8
s5l4_column_1     _structure         model3d=54clm1.3do size=.933381 movesize=.933381
# DESC: 
# BBOX: -0.2 -0.2 -0.8 0.2 0.2 0.8
s5l4_column_2     _structure         model3d=54clm2.3do size=.933381 movesize=.933381
# DESC: 
# BBOX: -0.021089 -0.023005 -0.035347 0.016307 0.023005 0.035347
tim's_bkpk4       _powerup           thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk_4m.cog
# DESC: 
# BBOX: -0.021089 -0.023005 -0.035347 0.016307 0.023005 0.035347
tim's_bkpk5       _powerup           thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk_5m.cog
# DESC: 
# BBOX: -0.03730128 -0.01387423 -0.1184614 0.03811364 0.03983956 0.06454442
darkkyle          _darkjedi          model3d=kk.3do puppet=mjdarkjedi.pup soundclass=ky.snd mass=170 maxvel=.2 weapon=+gamaxe health=20000 maxhealth=20000 maxthrust=1 maxrotthrust=200 jumpspeed=4 typeflags=0x2008201 error=.1 fov=-.4 chance=1 aiclass=darkkyle.ai
# DESC: 
# BBOX: -0.037834 -0.01877 -0.121446 0.03752 0.016413 0.064074
darkkyleboss      darkkyle           model3d=kkboss.3do
# DESC: 
# BBOX: -0.021089 -0.023005 -0.035347 0.016307 0.023005 0.035347
tim's_bkpk6       _powerup           thingflags=0x400 model3d=bpck.3do cog=pow_smbkpk_6m.cog
# DESC: 
# BBOX: -0.058684 -0.017148 -0.124887 0.007903 0.018428 0.117391
dartflower_g      _humanactor        collide=2 model3d=vf.3do size=.075 movesize=.075 treesize=.2 puppet=vf.pup soundclass=vf2.snd mass=0 weapon=+plantspittle health=50 maxhealth=50 typeflags=0x20001 aiclass=dfdefault.ai
# DESC: 
# BBOX: -0.2 -0.0125 -0.2 0.2 0.0125 0.2
54expwall1a       _walkstruct        thingflags=0x48 model3d=54wexp1a.3do size=.333119 movesize=.333119
# DESC: 
# BBOX: -0.119692 -0.234862 -0.063257 0.119692 0.234862 0.063257
coffin2           _walkstruct        thingflags=0x48 model3d=coff-2.3do size=.321087 movesize=.321087
# DESC: 
# BBOX: -0.040553 -0.199597 -0.201059 0.040553 0.199597 0.201059
s5l4vines         _decor             model3d=54bvine.3do size=.15 movesize=.15
# DESC: 
# BBOX: -0.124746 -0.234862 -0.027431 0.124746 0.234862 0.027431
coffin2lid        _walkstruct        thingflags=0x48 model3d=coff-2l.3do size=.317347 movesize=.317347
# DESC: 
# BBOX: -0.01264 -0.299708 -0.01129 0.01264 0.299708 0.01129
s5l4vine02        _structure         model3d=54rope02.3do size=.350187 movesize=.350187
# DESC: 
# BBOX: -0.03512459 -0.05451419 -0.118648 0.04070054 0.05178496 0.04611596
darkundead2       _humanactor        thingflags=0x20000400 model3d=du2.3do size=.0738 movesize=.0738 puppet=du.pup soundclass=du.snd cog=actor_du.cog mass=70 maxvel=.3 weapon=+force_lightning health=200 maxhealth=200 maxrotthrust=80 typeflags=0x2080001 fireoffset=(0/.066/.005) aiclass=du_def.ai
# DESC: 
# BBOX: -0.03512459 -0.05451419 -0.1184103 0.04070424 0.05178496 0.04510913
darkundead        _humanactor        thingflags=0x20000400 model3d=du.3do size=.0738 movesize=.0738 puppet=du.pup soundclass=du.snd cog=actor_du.cog mass=70 maxvel=.3 weapon=+force_lightning health=200 maxhealth=200 maxrotthrust=80 typeflags=0x2080001 fireoffset=(0/.066/.005) aiclass=du_def.ai
# DESC: 
# BBOX: -0.2 -0.0125 -0.2 0.2 0.0125 0.2
54expwall1b       _walkstruct        thingflags=0x48 model3d=54wexp1b.3do size=.333119 movesize=.333119
# DESC: 
# BBOX: 0 0 0 0 0 0
+force_lightning5 +force_lightning   damage=50
# DESC: 
# BBOX: 0 0 0 0 0 0
+s5l4_lightning   _weapon            thingflags=0x1 light=.1 timer=.25 size=.005 movesize=.005 angvel=(0/0/360) explode=+lightninghit fleshhit=+lightninghit trailthing=+force_ltpeice elementsize=.075 trailcylradius=.04 trailrandangle=25 damage=.1 damageclass=0x8 typeflags=0x1800d range=4
# DESC: 
# BBOX: -0.170777 -0.170777 -0.012502 0.170777 0.170777 0.012502
12_epad1          _walkstruct        model3d=12epad.3do size=.224571 movesize=.224571 soundclass=med_elev.snd
# DESC: 
# BBOX: -0.490393 -0.196424 -0.05 0.490393 0.196424 0.05
plathalf          _walkstruct        thingflags=0x48 model3d=plathalf.3do size=.580629 movesize=.580629
# DESC: 
# BBOX: -0.1 -0.0125 -0.15 0.1 0.0125 0.15
09_2x3door1       _structure         model3d=09d2x3_1.3do size=.18071 movesize=.18071 soundclass=sm_door.snd
# DESC: 
# BBOX: -0.148 -0.148 -0.013055 0.148 0.148 0.013695
ctf_3x3elev       _walkstruct        model3d=ctf_e3x3.3do size=.259751 movesize=.259751 soundclass=sm_elev.snd
# DESC: 
# BBOX: -0.1 -0.0125 -0.15 0.1 0.0125 0.15
09_2x3door2       _structure         model3d=09d2x3_2.3do size=.18071 movesize=.18071 soundclass=sm_door.snd
# DESC: 
# BBOX: -0.4 -0.025 -0.4 0.4 0.025 0.4
rounddoor4        _walkstruct        thingflags=0x400848 model3d=dr4.3do size=.400781 movesize=.400781
# DESC: 
# BBOX: -0.030347 -0.072704 -0.024787 0.009211 0.072704 0.024341
npconcrifle       concrifle          typeflags=0x9
# DESC: 
# BBOX: -0.021293 -0.02235 -0.028598 0.01838 0.02235 0.028374
npsupershield     supershield        typeflags=0x9
# DESC: 
# BBOX: -0.03092 -0.030162 -0.030457 0.03092 0.030162 0.030457
npdarkside        darkside           typeflags=0x9
# DESC: 
# BBOX: -0.015415 -0.017797 -0.028888 0.015415 0.017797 0.028888
rbacta            bactatank          typeflags=0x2 respawn=30
# DESC: 
# BBOX: -0.012965 -0.007189 -0.036159 0.012965 0.007189 0.036159
mdm02switch0      _powerup           thingflags=0x10 model3d=levr-1.3do cog=mdm02_forceswitch0.cog
# DESC: 
# BBOX: -0.074978 -0.074978 -0.069146 0.074978 0.074978 0.069146
woodcrate         _walkstruct        thingflags=0x48 model3d=wcrt.3do size=.176588 movesize=.176588
# DESC: 
# BBOX: -0.1 -0.005844 -0.1 0.1 0.005844 0.1
woodcatwalk       _walkstruct        model3d=shutr.3do size=.191542 movesize=.191542
# DESC: 
# BBOX: -0.15 -0.15 -0.00625 0.15 0.15 0.00625
03_3x3door_a      _walkstruct        model3d=03d3x3_3a.3do size=.262224 movesize=.262224
# DESC: 
# BBOX: -0.3 -0.0125 -0.3 0.3 0.0125 0.3
6x6door2          _walkstruct        model3d=d6x6.3do size=.474448 movesize=.474448 soundclass=lg_door.snd
# DESC: 
# BBOX: -0.7 -0.025 0.00207 0.7 0.025 0.80207
shielddoorsnd     _structure         model3d=bigdoor.3do size=1.114867 movesize=1.114867 soundclass=lg_door.snd
# DESC: 
# BBOX: -0.2 -0.0125 -0.15 0.2 0.0125 0.15
s2l1_door1        _structure         model3d=d4x3_s2l1.3do size=.300312 movesize=.300312 soundclass=med_door.snd
# DESC: 
# BBOX: -0.174123 -0.174123 -0.012502 0.174123 0.174123 0.012502
holocronpad       _walkstruct        model3d=hpad.3do size=.224571 movesize=.224571
# DESC: 
# BBOX: -0.333269 -0.125 -0.001666 0.333269 0.125 0.421753
throne            _walkstruct        thingflags=0x48 model3d=throne.3do size=.586502 movesize=.586502
# DESC: 
# BBOX: -0.1 -0.05 0.000731 0.1 0.05 0.600731
palacecolumn      _structure         model3d=06colm.3do size=.672151 movesize=.672151
# DESC: 
# BBOX: -0.030347 -0.072704 -0.024787 0.009211 0.072704 0.024341
+mdm04concrifle   _powerup           thingflags=0x400 model3d=conp.3do cog=pow_concrifle_m.cog typeflags=0x8 respawn=0
# DESC: 
# BBOX: -0.030347 -0.072704 -0.024787 0.009211 0.072704 0.024341
+mdm04concrifledecoy _powerup           thingflags=0x400 model3d=conp.3do cog=pow_concrifle_decoy.cog typeflags=0x8 respawn=0
# DESC: 
# BBOX: 0 0 0 0 0 0
+dustcloud2       +whitecloud        timer=.119 material=dusty.mat range=.01 rate=256 maxthrust=80 elementsize=.01
# DESC: 
# BBOX: -0.15 -0.011875 -0.15 0.15 0.011875 0.15
3x3reac           _walkstruct        model3d=e3x3reac.3do size=.233711 movesize=.233711 soundclass=sm_elev.snd
# DESC: 
# BBOX: -0.200716 -0.195991 0.038027 0.195304 0.200102 0.079681
4x4ingarvator     _zwalkstruct       model3d=e4x4_2i.3do size=.344409 movesize=.344409 soundclass=med_elev.snd
# DESC: 
# BBOX: -0.1 -0.5 -0.012306 0.1 0.5 0.012306
2x10catwalk       _walkstruct        model3d=c2x10.3do size=.51005 movesize=.51005
# DESC: 
# BBOX: -0.052028 -0.060925 0.000731 0.05132 0.060823 0.60066
moisturevaporator _structure         thingflags=0x400 model3d=vapr.3do size=.66097 movesize=.66097
# DESC: 
# BBOX: -0.175 -0.025 -0.175 0.175 0.025 0.175
09_octdoor        _structure         thingflags=0xc model3d=09doct1.3do size=.19204 movesize=.19204 soundclass=med_door.snd
# DESC: 
# BBOX: -0.070489 -0.081337 -0.065817 0.070489 0.081337 0.065817
woodcrate3        _walkstruct        thingflags=0x448 model3d=wcrt6-1.3do size=.154631 movesize=.154631 cog=actor_wc.cog
# DESC: 
# BBOX: -0.25 -1.05 -0.012306 0.25 1.025 0.012306
mdm07catwalk5     _walkstruct        model3d=c5x21_mdm07.3do size=1.187364 movesize=1.187364
# DESC: 
# BBOX: -0.15 -1.2 -0.012306 0.15 1.2 0.012306
mdm07catwalk3     _walkstruct        model3d=c3x24_mdm07.3do size=1.330341 movesize=1.330341
# DESC: 
# BBOX: -0.1 -0.9 -0.012306 0.1 0.9 0.012306
mdm07catwalk18    _walkstruct        model3d=c2x18_mdm07.3do size=.996184 movesize=.996184 soundclass=catwalkfall.snd
# DESC: 
# BBOX: -0.024736 -0.01943 -0.097598 0.024736 0.01943 0.102402
pipe1             _structure         model3d=p2xhf_1.3do size=.105347 movesize=.105347
# DESC: 
# BBOX: -0.623026 -0.283773 -0.601571 0.623026 0.283773 0.601571
t-16              _zwalkstruct       model3d=skyhop.3do size=.988847 movesize=.988847
# DESC: 
# BBOX: 0 0 0 0 0 0
+wcrate_exp       +crate_exp         damage=20 force=50 range=.15
# DESC: 
# BBOX: -12.90447 -31.23819 -7.19836 12.90447 31.23819 7.19836
dread             _zwalkstruct       model3d=dread.3do size=37.47168 movesize=37.47168
# DESC: 
# BBOX: -0.098284 -0.097923 -0.031081 0.097733 0.098093 0.031081
2x2elev_2         _walkstruct        model3d=e2x2_0.3do size=.192295 movesize=.192295 soundclass=sm_elev.snd
# DESC: 
# BBOX: -0.151582 -0.970063 -0.066789 0.151582 0.970063 0.066789
gantry1           _walkstruct        model3d=gan1.3do size=1.080419 movesize=1.080419
# DESC: 
# BBOX: -0.409606 -0.453233 -0.120333 0.409606 0.453233 0.120333
gantry2           _walkstruct        model3d=gan2.3do size=.553288 movesize=.553288
# DESC: 
# BBOX: -0.059157 -0.512454 -1.207433 0.458773 0.574397 0.829692
gantry3           _zwalkstruct       model3d=gan3.3do size=1.305442 movesize=1.305442
# DESC: 
# BBOX: -0.249628 -0.011529 -0.249901 0.249628 0.011529 0.249901
gantrywindow      _walkstruct        model3d=gwndw.3do size=.321489 movesize=.321489
# DESC: 
# BBOX: -0.650468 -1.749941 -0.049964 0.250468 1.749941 0.049964
mdm10catwalk      _zwalkstruct       model3d=mdm10wlk.3do size=1.917435 movesize=1.917435
# DESC: 
# BBOX: -0.038049 -0.029403 -0.124426 0.038049 0.029403 0.124426
gantrypipe        _walkstruct        model3d=mdm10p.3do size=.176557 movesize=.176557
# DESC: 
# BBOX: -0.003958 -0.003824 -0.004924 0.003958 0.003824 0.004924
raillight         _ghostdecor        model3d=raillite.3do size=.055504 movesize=.055504
# DESC: 
# BBOX: -0.038273 -0.031091 -0.073557 0.038273 0.031091 0.073557
gantrypipe2       _walkstruct        model3d=mdm10p2.3do size=.13323 movesize=.13323
# DESC: 
# BBOX: -0.249628 -0.05511 -0.249901 0.249628 0.05511 0.249901
gantrydamwindow   _ghoststructure    model3d=gwndwdam.3do size=.323666 movesize=.323666
# DESC: 
# BBOX: -0.544736 -0.786304 -0.229997 0.544736 0.786304 0.229997
flyingcrow        _zwalkstruct       model3d=crow.3do size=.853084 movesize=.853084 puppet=cr.pup
# DESC: 
# BBOX: -0.08322 -0.046436 -0.09211 0.08322 0.046436 0.09211
emp_chair         _zwalkstruct       model3d=chair.3do size=.114176 movesize=.114176
# DESC: 
# BBOX: -0.057535 -0.059301 0.000733 0.057535 0.059301 0.156623
thrnrpan          _zwalkstruct       model3d=trpnl.3do size=.185498 movesize=.185498
# DESC: 
# BBOX: -0.01264 -0.299708 -0.01129 0.01264 0.299708 0.01129
visiblerod02      _structure         collide=1 model3d=rod02.3do size=.025 movesize=.025
# DESC: 
# BBOX: -0.01264 -0.049041 -0.01129 0.01264 0.049041 0.01129
inviscollide      _structure         collide=1 thingflags=0x10 model3d=41rod.3do size=.025 movesize=.025
# DESC: 
# BBOX: -0.025263 -0.019826 -0.199259 0.025263 0.019826 0.199259
thronecolumn      _structure         model3d=thrncolm.3do size=.22094 movesize=.22094
# DESC: 
# BBOX: -0.143317 -0.201706 -0.078876 0.143317 0.201706 0.078876
waterwell         _zwalkstruct       model3d=wtrw.3do size=.274523 movesize=.274523
# DESC: 
# BBOX: -0.050887 -0.075508 -0.16702 0.050887 0.25464 0.131736
crane             _decor             model3d=cran.3do size=.075 movesize=.075
# DESC: 
# BBOX: -0.024736 -0.01943 -0.097343 0.024736 0.01943 0.102387
pipe5             _structure         model3d=p2xhf_5.3do size=.105333 movesize=.105333
# DESC: 
# BBOX: -0.042032 -0.049632 0.000337 0.042032 0.049632 0.195296
chair3            _walkstruct        thingflags=0x48 model3d=chair3.3do size=.25442 movesize=.25442
# DESC: 
# BBOX: -0.2 -0.1 -0.050028 0.2 0.1 0.050359
table2            _walkstruct        thingflags=0x48 model3d=tbl2.3do size=.279207 movesize=.279207
# DESC: 
# BBOX: -0.15 -0.15 -0.15 0.15 0.15 0.15
woodcratebig      _walkstruct        thingflags=0x48 model3d=wcrt2.3do size=.309808 movesize=.309808
# DESC: 
# BBOX: -0.016813 -0.025219 -0.025219 0.016813 0.025219 0.025219
bottle01          _structure         model3d=bottle1.3do size=.089429 movesize=.089429
# DESC: 
# BBOX: -0.012767 -0.012147 -0.023339 0.012767 0.012147 0.023339
bottle05          _structure         model3d=bottle5.3do size=.077465 movesize=.077465
# DESC: 
# BBOX: -0.024736 -0.01943 -0.097342 0.024736 0.01943 0.102406
pipe3             _structure         model3d=p2xhf_3.3do size=.105352 movesize=.105352
# DESC: 
# BBOX: -0.034039 -0.029553 -0.032378 0.034039 0.029553 0.032378
vaporatordead     _walkstruct        model3d=v_deb1.3do size=.096979 movesize=.096979
# DESC: 
# BBOX: -0.050387 -0.043746 -0.161575 0.050387 0.043746 0.161575
vaporator         _structure         model3d=vap1.3do size=.21925 movesize=.21925
# DESC: 
# BBOX: -0.01459 -0.075274 -0.015287 0.01459 0.075274 0.015287
roofpipe          _ghostdecor        model3d=01rfd0.3do size=.077027 movesize=.077027
# DESC: 
# BBOX: -0.031151 -0.014189 -0.030079 0.031151 0.014189 0.030079
t-16model         _decor             model3d=skyhopmodel.3do size=.049442 movesize=.049442
# DESC: 
# BBOX: -0.016776 -0.034744 -0.010952 0.018316 0.034538 0.00282
ctf_redflag       _powerup           move=none model3d=ys.3do size=.05 movesize=.05 puppet=cr.pup typeflags=0x8
# DESC: 
# BBOX: -0.102494 -0.034109 -0.028188 0.102494 0.034109 0.028188
ysalamiribranch   _walkstruct        thingflags=0x800048 model3d=ysbranch.3do size=.159664 movesize=.159664
# DESC: 
# BBOX: -0.094105 -0.094105 -0.094105 0.094105 0.094105 0.094105
+redsphere        none               orient=(0/0/0) type=cog move=physics thingflags=0x80000000 model3d=redsphr.3do size=.144105 movesize=.144105 angvel=(60/60/60)
# DESC: 
# BBOX:
kyleduelingboss _humanactor move=path model3d=kkduel.3do puppet=mjdueling.pup typeflags=0x80200
# DESC: 
# BBOX:
kylebossactor kyleactor model3d=kkduel.3do puppet=mjdueling.pup typeflags=0x80200
# DESC: 
# BBOX:
kylebryaractor kyleactor model3d=kkbryar.3do
# DESC: 
# BBOX:
maraduelingboss _humanactor move=path model3d=mjduel.3do puppet=mjdueling.pup typeflags=0x80200
# DESC: 
# BBOX:
marabossactor maraactor model3d=mj.3do typeflags=0x80200
# DESC: 
# BBOX:
marablastekactor maraactor model3d=mjblastk.3do
# DESC: 
# BBOX:
rebcommander _humanactor move=path model3d=rm.3do puppet=io.pup
# DESC: 
# BBOX:
nbartender nman2 cog=actor_bt.cog
# DESC: 
# BBOX:
_throwable none orient=(0/0/0) type=debris collide=1 move=physics movesize=.01 surfdrag=3 airdrag=1 mass=25 height=.011 physflags=0x404041 buoyancy=.5
# DESC: 
# BBOX:
throwcrate1 _throwable model3d=crttoss1.3do size=.0364 movesize=.0364 height=.0365
# DESC: 
# BBOX:
throwshrapnel_1 _throwable model3d=shrp_1.3do size=.045 movesize=.045 height=.0451
# DESC: 
# BBOX:
a none model3d=a.3do
