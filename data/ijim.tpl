_actor            none              type=actor collide=3 move=physics thingflags=0x1 airdrag=1.000000 mass=150.000000 physflags=0x4A4F maxrotvel=200.000000 maxvel=1.000000 health=20.000000 maxhealth=20.000000 maxrotthrust=100.000000 maxheadvel=600.000000 maxheadyaw=60.000000 jumpspeed=1.500000 typeflags=0x1 minheadpitch=-60.000000 maxheadpitch=60.000000 lightoffset=(0.000000/0.070000/0.040000)
_indyactor        _actor            type=player collide=1 thingflags=0x20004400 size=0.040000 movesize=0.040000 collwidth=0.040000 collheight=0.080000 puppet=indy.pup soundclass=indy.snd cog=actor_indy.cog surfdrag=3.000000 airdrag=0.500000 staticdrag=0.300000 health=1000.000000 maxhealth=1000.000000 maxthrust=0.520000 maxrotthrust=70.000000 jumpspeed=1.200000 typeflags=0x8000001 eyeoffset=(0.000000/0.010000/0.073500)
shirtplayer       _indyactor        model3d=gen_in_shirt.3do
_decor            none              type=cog collide=1 move=path
_ghostdecor       _decor            collide=0
2mstake           _ghostdecor       model3d=cyn_tent_stake_2meter.3do size=0.020000 movesize=0.020000 collwidth=0.020000 collheight=0.020000
4mstake           _ghostdecor       model3d=cyn_tent_stake_4meter.3do size=0.020000 movesize=0.020000 collwidth=0.020000 collheight=0.020000
tumbleweed        _ghostdecor       model3d=cyn_weed_tumble.3do size=0.056569 movesize=0.056569 collwidth=0.056569 collheight=0.056569
granruinf         _ghostdecor       model3d=cyn_granarys_ruins_f.3do size=0.141156 movesize=0.141156 collwidth=0.141156 collheight=0.141156
granruinb         _ghostdecor       model3d=cyn_granarys_ruins_b.3do size=0.215291 movesize=0.215291 collwidth=0.215291 collheight=0.215291
cut_photo_vo      _decor            collide=3 thingflags=0x80000 model3d=photo_so_vo1.3do size=0.000438 movesize=0.000438 collwidth=0.000438 collheight=0.000438
ghost             none              type=ghost move=path
cut_photo_bab     _decor            collide=3 thingflags=0x80000 model3d=photo_so_bab.3do size=0.001227 movesize=0.001227 collwidth=0.001227 collheight=0.001227
_structure        _decor            collide=3
_wstructmetal     _structure        thingflags=0x400040
jeep_pyr          _wstructmetal     move=physics thingflags=0x404400 model3d=pyr_jeep.3do size=0.195000 movesize=0.090000 collwidth=0.192000 collheight=0.090000 cog=jeep_pyr.cog
_sstructwood      _structure        thingflags=0x20C0
sifter            _sstructwood      model3d=cyn_sifter_artifax.3do size=0.122056 movesize=0.122056 collwidth=0.122056 collheight=0.122056
_staticstruct     _structure        thingflags=0xC0
cut_table         _staticstruct     model3d=cyn_table_meeting.3do size=0.104201 movesize=0.104201 collwidth=0.104201 collheight=0.104201
_cutactor         _actor            collide=1 thingflags=0x20084000 size=0.020000 movesize=0.020000 surfdrag=3.200000 staticdrag=0.001000 physflags=0xA4F maxrotvel=100.000000 health=1000.000000 maxhealth=1000.000000 maxthrust=0.520000 maxrotthrust=450.000000 typeflags=0x8110009 eyeoffset=(0.000000/0.010000/0.070000) aiclass=actor.ai
chop_actor        _cutactor         thingflags=0x80000 model3d=cyn_chopr.3do collwidth=0.020000 collheight=0.020000 puppet=default.pup airdrag=2.000000 mass=1.000000 physflags=0x3202 maxthrust=0.800000 maxrotthrust=200.000000
sophia            _cutactor         thingflags=0x20084400 model3d=gen_so.3do collwidth=0.020000 collheight=0.020000 puppet=sophia.pup soundclass=sophia.snd cog=actor_sophia.cog eyeoffset=(0.000000/0.015200/0.069300) voicecolor=(0.745000/0.353000/0.980000/1.000000/0.745000/0.353000/0.980000/1.000000/0.961000/0.843000/1.000000/1.000000/0.961000/0.843000/1.000000/1.000000)
indy_sh_actor     _cutactor         thingflags=0x20084400 model3d=gen_in_shirt.3do collwidth=0.020000 collheight=0.020000 puppet=indy_actor.pup soundclass=indy.snd cog=actor_indyactor.cog aiclass=none.ai
sophia_arm        _cutactor         model3d=0so_throwphoto.3do collwidth=0.020000 collheight=0.020000 puppet=default.pup mass=0.000000 physflags=0x3202
indy_arm          _cutactor         model3d=0in_photopoint.3do collwidth=0.020000 collheight=0.020000 puppet=default.pup mass=0.000000 physflags=0x3202
cut_photo_vo2     _decor            collide=3 thingflags=0x80000 model3d=photo_so_vo2.3do size=0.002523 movesize=0.002523 collwidth=0.002523 collheight=0.002523
_pushstruct2m     _structure        thingflags=0x44000040
pushblck          _pushstruct2m     model3d=cyn_block_push.3do size=0.173205 movesize=0.173205 collwidth=0.173205 collheight=0.173205
granruinc         _ghostdecor       model3d=cyn_granarys_ruins_c.3do size=0.197231 movesize=0.197231 collwidth=0.197231 collheight=0.197231
_walkstruct       _structure        thingflags=0x40
granruinh         _walkstruct       model3d=cyn_granarys_ruins_h.3do size=0.089022 movesize=0.089022 collwidth=0.089022 collheight=0.089022
_swingstruct      _structure        thingflags=0x80000080 size=0.100000 movesize=0.100000
whip_branch       _swingstruct      model3d=cyn_strut_brnch.3do collwidth=0.100000 collheight=0.100000
cynplata          _staticstruct     thingflags=0xC1 light=(0.100000/0.100000/0.050000/0.001000) model3d=cyn_pltfrm_a.3do size=0.141973 movesize=0.141973 collwidth=0.141973 collheight=0.141973
cynplatb          _staticstruct     thingflags=0xC1 light=(0.200000/0.200000/0.300000/0.001000) model3d=cyn_pltfrm_b.3do size=0.141973 movesize=0.141973 collwidth=0.141973 collheight=0.141973
_item             none              type=item collide=3 move=physics size=0.020000 movesize=0.020000 surfdrag=3.000000 airdrag=1.000000 staticdrag=0.005000 mass=0.100000 physflags=0x41 buoyancy=0.500000
goldingot         _item             thingflags=0x401 light=(0.200000/0.200000/0.200000/0.001000) model3d=gen_ingots_gold.3do collwidth=0.020000 collheight=0.020000 cog=item_ingotgold.cog height=0.004100
skellay           _ghostdecor       model3d=skeleton_lay.3do size=0.100562 movesize=0.100562 collwidth=0.100562 collheight=0.100562
cactus            _staticstruct     model3d=cyn_cactus.3do size=0.119379 movesize=0.119379 collwidth=0.119379 collheight=0.119379
potsherd          _item             thingflags=0x400 model3d=cyn_potshard.3do collwidth=0.020000 collheight=0.020000 cog=item_potsherd.cog height=0.003180
_sprite           none              type=sprite move=physics sprite=default.spr
sparkle           _sprite           sprite=gen_sparkle.spr
silvercoins       _item             thingflags=0x401 light=(0.200000/0.200000/0.200000/0.001000) model3d=gen_coins_slvr.3do collwidth=0.020000 collheight=0.020000 cog=item_coinsilver.cog height=0.003835
_climbstruct      _structure        thingflags=0x88
whip_branchclimb  _climbstruct      model3d=tem_strut_brnch.3do size=0.030000 movesize=0.030000 collwidth=0.030000 collheight=0.030000
viga              _ghostdecor       model3d=cyn_strut_whip.3do size=0.012500 movesize=0.012500 collwidth=0.012500 collheight=0.012500
goldcoins         _item             thingflags=0x401 light=(0.200000/0.200000/0.200000/0.001000) model3d=gen_coins_gold.3do collwidth=0.020000 collheight=0.020000 cog=item_coingold.cog height=0.004356
_flyactor         _actor            physflags=0x3202 maxthrust=2.000000
vulture           _flyactor         thingflags=0x401 model3d=gen_vu.3do size=0.075000 movesize=0.075000 collwidth=0.075000 collheight=0.075000 puppet=vulture.pup cog=actor_vulture.cog staticdrag=0.010000 mass=2.000000 maxthrust=0.600000 maxrotthrust=150.000000 typeflags=0x10100000 aiclass=vulture.ai
_hintbase         none              type=hint
hintnumber2       _hintbase         userval=2.000000
hintnumber4       _hintbase         userval=4.000000
hintnumber6       _hintbase         userval=6.000000
hintnumber8       _hintbase         userval=8.000000
hintnumber10      _hintbase         userval=10.000000
hintnumber12      _hintbase         userval=12.000000
hintnumber14      _hintbase         userval=14.000000
hintnumber16      _hintbase         userval=16.000000
hintnumber18      _hintbase         userval=18.000000
hintnumber20      _hintbase         userval=20.000000
hintnumber22      _hintbase         userval=22.000000
hintnumber24      _hintbase         userval=24.000000
hintnumber26      _hintbase         userval=26.000000
hintnumber28      _hintbase         userval=28.000000
hintnumber30      _hintbase         userval=30.000000
hintnumber32      _hintbase         userval=32.000000
hintnumber34      _hintbase         userval=34.000000
hintnumber1       _hintbase         userval=1.000000
gemblu            _item             thingflags=0x401 light=(0.200000/0.200000/0.200000/0.001000) model3d=gen_gem_blue.3do collwidth=0.020000 collheight=0.020000 cog=item_gemblue.cog height=0.007848
silveridol        _item             thingflags=0x401 light=(0.200000/0.200000/0.200000/0.001000) model3d=gen_idol_slvr.3do collwidth=0.020000 collheight=0.020000 cog=item_idolsilver.cog height=0.014000
gemred            _item             thingflags=0x401 light=(0.200000/0.200000/0.200000/0.001000) model3d=gen_gem_red.3do collwidth=0.020000 collheight=0.020000 cog=item_gemred.cog height=0.007848
goldidol          _item             thingflags=0x401 light=(0.200000/0.200000/0.200000/0.001000) model3d=gen_idol_gold.3do collwidth=0.020000 collheight=0.020000 cog=item_idolgold.cog height=0.014000
1mountplateau     _staticstruct     model3d=cyn_plateau_mount.3do size=0.155503 movesize=0.155503 collwidth=0.155503 collheight=0.155503
cynplate          _staticstruct     model3d=cyn_plate_cl.3do size=0.015000 movesize=0.015000 collwidth=0.015000 collheight=0.015000
_structblock      _structure        thingflags=0x14
fadeplat          _structblock      thingflags=0x4 model3d=gen_fadeplate.3do
ijplateb          _staticstruct     model3d=cyn_plate_ij_b.3do size=0.080000 movesize=0.080000 collwidth=0.080000 collheight=0.080000
baretree          _staticstruct     model3d=gen_tree_bare.3do size=0.420000 movesize=0.420000 collwidth=0.420000 collheight=0.420000
_gen_ai_slow      _actor            thingflags=0x8000000 surfdrag=3.200000 staticdrag=0.005000 mass=25.000000 maxvel=0.500000 health=100.000000 maxhealth=100.000000 maxthrust=0.450000
_snake            _gen_ai_slow      thingflags=0x8000400 collwidth=0.030000 collheight=0.001000 puppet=snake.pup soundclass=snake.snd cog=actor_snake.cog mass=15.000000 maxrotvel=400.000000 typeflags=0x20000 eyeoffset=(0.000000/0.080000/-0.050000) aiclass=snake.ai
_weapon           none              type=weapon collide=1 move=physics thingflags=0x20000000 mass=0.000001 buoyancy=0.500000 typeflags=0xD damageclass=0x1
+bughit           _weapon           vel=(0.000000/3.000000/0.000000) damage=1.000000 damageclass=0x20000000 range=0.150000
+snake_c_bite     +bughit           damage=124.875000 damageclass=0x100
snake_rattle      _snake            model3d=gen_rs.3do size=0.060000 movesize=0.060000 puppet=rattlesnake.pup weapon=+snake_c_bite aiclass=rattlesnake.ai
granruine         _ghostdecor       model3d=cyn_granarys_ruins_e.3do size=0.172699 movesize=0.172699 collwidth=0.172699 collheight=0.172699
chopshad          _ghostdecor       model3d=cyn_chopr_shadow.3do
_sstructmetal     _structure        thingflags=0x4000C0
wheelbarrow       _sstructmetal     model3d=bab_whlbrw_wood.3do size=0.115561 movesize=0.115561 collwidth=0.115561 collheight=0.115561
dirchair          _decor            model3d=bab_chair_dirctr.3do size=0.048511 movesize=0.048511 collwidth=0.048511 collheight=0.048511
shovel            _sstructmetal     model3d=gen_shovel.3do size=0.058500 movesize=0.058500 collwidth=0.058500 collheight=0.058500
_decorwood        _decor            thingflags=0x2000
bucket_wood       _decorwood        model3d=gen_bucket_wood.3do size=0.027011 movesize=0.027011 collwidth=0.027011 collheight=0.027011
pick              _sstructwood      model3d=gen_pick.3do size=0.049000 movesize=0.049000 collwidth=0.049000 collheight=0.049000
_sstructearth     _structure        thingflags=0x8000C0
smltenttop        _sstructearth     model3d=gen_tentsml_brwn_top.3do size=0.393700 movesize=0.393700 collwidth=0.393700 collheight=0.393700
smltenta          _sstructearth     model3d=gen_tentsml_brwna.3do size=0.251247 movesize=0.251247 collwidth=0.251247 collheight=0.251247
smltentb          _sstructearth     model3d=gen_tentsml_brwnb.3do size=0.336341 movesize=0.336341 collwidth=0.336341 collheight=0.336341
smltentc          _sstructearth     model3d=gen_tentsml_brwnc.3do size=0.251247 movesize=0.251247 collwidth=0.251247 collheight=0.251247
smltentd          _sstructearth     model3d=gen_tentsml_brwnd.3do size=0.336341 movesize=0.336341 collwidth=0.336341 collheight=0.336341
smltentfloor      _ghostdecor       model3d=gen_tentsml_brwn_floor.3do size=0.451885 movesize=0.451885 collwidth=0.451885 collheight=0.451885
_structwood       _structure        thingflags=0x2000
smltentropole     _structwood       model3d=gen_tentsml_brwn_ropole.3do size=0.560868 movesize=0.560868 collwidth=0.560868 collheight=0.560868
collruinf         _structblock      model3d=gen_ruinsf.3do size=0.148199 movesize=0.148199 collwidth=0.148199 collheight=0.148199
collruinb         _structblock      model3d=gen_ruinsb.3do size=0.255360 movesize=0.255360 collwidth=0.255360 collheight=0.255360
collruinc         _structblock      model3d=gen_ruinsc.3do size=0.201396 movesize=0.201396 collwidth=0.201396 collheight=0.201396
collruine         _structblock      model3d=gen_ruinse.3do size=0.177863 movesize=0.177863 collwidth=0.177863 collheight=0.177863
whip_branchplus   _staticstruct     model3d=cyn_strut_brnch.3do size=0.030000 movesize=0.030000 collwidth=0.030000 collheight=0.030000
fallfoam_3x5      _sprite           orient=(90.000000/0.000000/0.000000) sprite=waterfallfoam_3x5.spr
walkplayer        _indyactor        model3d=gen_in.3do
_explosion        none              type=explosion thingflags=0x1 damageclass=0x4
+fists_hit        _explosion        timer=0.009000 soundclass=fists.snd
+fists            _weapon           size=0.030000 movesize=0.030000 mass=1.000000 vel=(0.000000/4.000000/0.000000) explode=+fists_hit damage=50.000000 typeflags=0x200D damageclass=0x8 range=0.075000
+whip             _weapon           size=0.030000 movesize=0.030000 vel=(0.000000/1.000000/0.000000) damage=15.000000 typeflags=0x200D damageclass=0x10 range=0.450000
+whip_actor       _actor            collide=0 move=none model3d=gen_wh.3do puppet=whip.pup
+whipc_actor      _actor            collide=0 move=none model3d=gen_wc.3do size=0.202480 movesize=0.202480 collwidth=0.202480 collheight=0.202480 puppet=whipc.pup
+pistol_hit       _explosion        timer=0.250000 typeflags=0x31 spritething=+pis_spr_hit spritestart=(0.100000/0.100000/1.000000) spriteend=(0.100000/0.100000/1.000000)
+pistol           _weapon           timer=5.000000 model3d=gen_tracer.3do size=0.020000 movesize=0.020000 collwidth=0.020000 collheight=0.020000 vel=(0.000000/4.000000/0.000000) explode=+pistol_hit damage=100.000000 mindamage=5.000000 typeflags=0x64C40D rate=150.000000
+pis_spr_hit      _sprite           timer=0.250000 sprite=pistol_hit.spr
+pis_spr_smoke1   _sprite           timer=1.500000 sprite=pistol_smoke1.spr
+pis_spr_smoke2   _sprite           timer=1.500000 sprite=pistol_smoke2.spr
+pistol_smoke1    _explosion        light=(0.070000/0.070000/0.070000/0.080000) timer=0.349000 typeflags=0x11 spritething=+pis_spr_smoke1 spritestart=(0.010000/0.010000/1.000000) spriteend=(0.170000/0.170000/0.000000)
+pistol_smoke2    _explosion        light=(0.070000/0.070000/0.070000/0.080000) timer=0.349000 typeflags=0x11 spritething=+pis_spr_smoke2 spritestart=(0.010000/0.010000/1.000000) spriteend=(0.170000/0.170000/0.000000)
+tokarev          +pistol           timer=6.000000 vel=(0.000000/4.500000/0.000000) damage=125.000000 mindamage=10.000000 rate=100.000000
+mauser           +pistol           timer=7.000000 vel=(0.000000/5.000000/0.000000) damage=125.000000 mindamage=15.000000 rate=100.000000
+simonov          +pistol           timer=10.000000 vel=(0.000000/7.000000/0.000000) damage=150.000000 mindamage=20.000000 rate=25.000000
+machete_hit      _explosion        timer=0.009000
+machete          _weapon           size=0.065000 movesize=0.065000 vel=(0.000000/0.100000/0.000000) explode=+machete_hit damage=100.000000 typeflags=0x64240D damageclass=0x20 range=0.080000
+submachine       +pistol           timer=4.000000 mindamage=10.000000 rate=100.000000
+shotgun          _weapon           timer=3.000000 size=0.020000 movesize=0.020000 vel=(0.000000/3.500000/0.000000) explode=+pistol_hit damage=250.000000 mindamage=5.000000 typeflags=0x65440D rate=250.000000
+bazooka_exp_smoke _explosion        timer=1.100000 typeflags=0x11 spritething=+baz_spr_exp_smoke spritestart=(0.500000/0.500000/1.000000) spriteend=(1.100000/1.100000/0.000000)
+bazooka_exp_fire _explosion        timer=0.800000 creatething=+bazooka_exp_smoke typeflags=0x1039 fadetime=0.400000 babytime=0.300000 spritething=+baz_spr_exp_fire spritestart=(0.300000/0.300000/1.000000) spriteend=(0.800000/0.800000/0.000000)
_debris           none              type=weapon collide=1 move=physics timer=1.500000 surfdrag=3.000000 airdrag=1.000000 mass=1.000000 physflags=0x21 vel=(0.000000/1.500000/0.500000) angvel=(100.000000/0.000000/100.000000) damage=5.000000 typeflags=0x80000 damageclass=0x8000000
xpldshrapa        _debris           model3d=gen_dbre_xplode_a.3do size=0.004550 movesize=0.004550 collwidth=0.004550 collheight=0.004550
xpldshrapb        _debris           model3d=gen_dbre_xplode_b.3do size=0.010693 movesize=0.010693 collwidth=0.010693 collheight=0.010693
+bazooka_exp_debris _explosion        timer=0.400000 creatething=+bazooka_exp_fire typeflags=0x20A blasttime=0.349000 babytime=0.100000 debris=xpldshrapa debris=xpldshrapb debris=xpldshrapa debris=xpldshrapb debris=xpldshrapa debris=xpldshrapb debris=xpldshrapa debris=xpldshrapb debris=xpldshrapa debris=xpldshrapb debris=xpldshrapa debris=xpldshrapb
stoneshrapa       _debris           model3d=gen_dbre_stnwala.3do size=0.012500 movesize=0.012500 collwidth=0.012500 collheight=0.012500
stoneshrapb       _debris           model3d=gen_dbre_stnwalb.3do size=0.043652 movesize=0.043652 collwidth=0.043652 collheight=0.043652
stoneshrapc       _debris           model3d=gen_dbre_stnwalc.3do size=0.012601 movesize=0.012601 collwidth=0.012601 collheight=0.012601
+bazooka_exp      _explosion        timer=0.300000 soundclass=gen_explode.snd creatething=+bazooka_exp_debris typeflags=0x43F damage=1500.000000 blasttime=0.250000 babytime=0.009000 range=0.500000 debris=stoneshrapa debris=stoneshrapb debris=stoneshrapc debris=stoneshrapa debris=stoneshrapb debris=stoneshrapc debris=stoneshrapa debris=stoneshrapb debris=stoneshrapc spritething=+baz_spr_exp_blast spritestart=(0.075000/0.075000/1.000000) spriteend=(1.000000/1.000000/1.000000)
+bazooka          _weapon           timer=5.000000 model3d=gen_rckt_bzooka.3do size=0.025318 movesize=0.025318 collwidth=0.025318 collheight=0.025318 puppet=default.pup mass=5.000000 vel=(0.000000/2.000000/0.000000) explode=+bazooka_exp typeflags=0x26450C
+bazooka_chicken  +bazooka          timer=30.000000 model3d=gen_chicken.3do vel=(0.000000/0.200000/0.000000)
+baz_spr_exhaust  _sprite           timer=0.349000 sprite=bazooka_exhaust.spr
+bazooka_exhaust  _explosion        timer=0.500000 typeflags=0x67 damage=1000.000000 blasttime=0.449000 range=0.200000 spritething=+baz_spr_exhaust spritestart=(0.100000/0.100000/1.000000) spriteend=(0.200000/0.200000/0.000000)
+baz_spr_exp_blast _sprite           timer=0.300000 sprite=bazooka_expblast.spr
+baz_spr_exp_fire _sprite           timer=0.699000 sprite=bazooka_expfire.spr
+baz_spr_exp_smoke _sprite           timer=1.000000 sprite=bazooka_expsmoke.spr
+baz_spr_smoke    _sprite           timer=1.500000 sprite=bazooka_smoke.spr
+bazooka_smoke    _explosion        timer=0.349000 typeflags=0x31 spritething=+baz_spr_smoke spritestart=(0.010000/0.010000/0.800000) spriteend=(0.450000/0.450000/0.000000)
+baz_spr_rocket   _sprite           sprite=bazooka_rocket.spr
+grenade          _weapon           thingflags=0x20000400 timer=4.000000 model3d=weap_grenade_inv.3do size=0.004562 movesize=0.004562 collwidth=0.004562 collheight=0.004562 soundclass=grenade.snd cog=class_grenade.cog surfdrag=3.000000 airdrag=2.000000 mass=1.000000 height=0.004560 physflags=0x221 vel=(0.000000/2.000000/1.000000) explode=+bazooka_exp typeflags=0x280300
+satchel          _weapon           thingflags=0x20000400 timer=10.000000 model3d=weap_satchel_inv.3do size=0.005761 movesize=0.005761 collwidth=0.005761 collheight=0.005761 soundclass=satchel.snd cog=class_grenade.cog airdrag=0.150000 mass=5.000000 height=0.005761 physflags=0x41 vel=(0.000000/0.000000/-0.001000) buoyancy=0.250000 explode=+bazooka_exp typeflags=0x1380
+imp1             _weapon           size=0.050000 movesize=0.050000 vel=(0.000000/4.000000/0.000000) damage=10.000000 typeflags=0x20200D damageclass=0x1000 range=0.060000
+imp1_glow        _sprite           sprite=imp1_glow.spr
+imp1_spr_fire    _sprite           timer=1.000000 sprite=imp1_fire.spr
+imp1_fire        _explosion        timer=1.000000 typeflags=0x1 spritething=+imp1_spr_fire spritestart=(0.001000/0.001000/1.000000) spriteend=(0.500000/0.500000/0.000000)
+imp2_glow        _sprite           sprite=imp2_glow.spr
_particle         none              type=particle move=physics timer=1.000000 typeflags=0x11 material=dflt.mat rate=1.000000
+imp2_dust        _particle         orient=(0.000000/0.000000/1.022018) timer=0.250000 typeflags=0x59 material=aet_4sprite_glow_fairy_dust.mat range=0.040000 minsize=0.010000 rate=20.000000 maxthrust=0.500000 elementsize=0.005000 count=50
+imp2_trail       _particle         timer=0.500000 typeflags=0x19 material=aet_4sprite_glow_fairy_dust.mat range=0.025000 minsize=0.010000 maxthrust=0.500000 elementsize=0.005000 count=7
+imp3_glow        _sprite           sprite=imp3_glow.spr
+imp4             _weapon           size=0.050000 movesize=0.050000 vel=(0.000000/4.000000/0.000000) damage=10.000000 typeflags=0x20200D damageclass=0x4000 range=0.020000
+imp4_glow        _sprite           sprite=imp4_glow.spr
+imp5             _weapon           size=0.050000 movesize=0.050000 vel=(0.000000/4.000000/0.000000) damage=10.000000 typeflags=0x20200D damageclass=0x5000 range=0.020000
+imp5_glow        _sprite           sprite=imp5_glow.spr
+uw_spr_blast     _sprite           timer=0.300000 sprite=aet_expfloater.spr
+uw_spr_explode   _sprite           timer=0.800000 sprite=uw_explode.spr
+uw_explode       _explosion        timer=0.800000 typeflags=0x1039 fadetime=0.300000 spritething=+uw_spr_explode spritestart=(0.300000/0.300000/1.000000) spriteend=(1.200000/1.200000/0.000000)
+uw_blast         _explosion        timer=0.300000 soundclass=gen_explode_uw.snd creatething=+uw_explode typeflags=0x43F damage=1500.000000 blasttime=0.250000 babytime=0.250000 range=0.250000 spritething=+uw_spr_blast spritestart=(0.075000/0.075000/1.000000) spriteend=(1.500000/1.500000/1.000000)
zippo_glow        _sprite           thingflags=0x1 light=(0.855000/0.600000/0.400000/0.600000) sprite=zippo.spr
+exhaust          _sprite           timer=1.000000 sprite=exhaust.spr
+bubbles          _sprite           timer=4.099000 sprite=bubbles.spr
+ripples          _sprite           orient=(90.000000/0.000000/0.000000) timer=2.099000 sprite=waterripple.spr
+spray            _particle         timer=0.750000 mass=1.000000 physflags=0x201 vel=(0.000000/0.000000/0.700000) typeflags=0x10 material=gen_1particle_water_splash.mat range=0.100000 rate=5.000000 maxthrust=10.000000 elementsize=0.002000 count=256
+bloodsplort      _sprite           timer=0.500000 sprite=gen_bloodsplort.spr
+vulcansplort     _sprite           timer=0.500000 sprite=gen_vulcansplort.spr
lavadeathsparks   _particle         timer=5.000000 airdrag=2.000000 mass=1.000000 physflags=0x201 vel=(0.000000/0.000000/0.005000) typeflags=0x19 material=aet_4sprite_glow_fairy_dust.mat range=0.100000 rate=5.000000 maxthrust=10.000000 elementsize=0.010000 count=50
_polyline         none              type=polyline
+plcog            _polyline
+plcogend         _polyline
+laserbeam        _polyline
+lightning        _polyline
+1947_mark        _sprite           sprite=x1947.spr
+alien_mark       _sprite           sprite=xalien.spr
+ankh_mark        _sprite           sprite=xankh.spr
+ark_mark         _sprite           sprite=xark.spr
+cccr_mark        _sprite           sprite=xcccr.spr
+check_mark       _sprite           sprite=xcheck.spr
+cir_mark         _sprite           sprite=xcircle.spr
+cross_mark       _sprite           sprite=xcross.spr
+eye_mark         _sprite           sprite=xeye.spr
+five_mark        _sprite           sprite=xfive.spr
+grail_mark       _sprite           sprite=xgrail.spr
+guy_mark         _sprite           sprite=xguy.spr
+happy_mark       _sprite           sprite=xhappy.spr
+hawk_mark        _sprite           sprite=xhawk.spr
+here_mark        _sprite           sprite=xhere.spr
+indy_mark        _sprite           sprite=xindy.spr
+indy2_mark       _sprite           sprite=xindy2.spr
+la_mark          _sprite           sprite=xla.spr
+love_mark        _sprite           sprite=xlove.spr
+portrait_mark    _sprite           sprite=xportrait.spr
+pound_mark       _sprite           sprite=xpound.spr
+pyramid_mark     _sprite           sprite=xpyramid.spr
+question_mark    _sprite           sprite=xquestion.spr
+r2d2_mark        _sprite           sprite=xr2d2.spr
+skull_mark       _sprite           sprite=xskull.spr
+snake_mark       _sprite           sprite=xsnake.spr
+square_mark      _sprite           sprite=xsquare.spr
+star_mark        _sprite           sprite=xstar.spr
+stones_mark      _sprite           sprite=xstones.spr
+sunrise_mark     _sprite           sprite=xsunrise.spr
+tentacle_mark    _sprite           sprite=xtentacle.spr
+tictactoe_mark   _sprite           sprite=xtictactoe.spr
+triangle_mark    _sprite           sprite=xtriangle.spr
+usa_mark         _sprite           sprite=xusa.spr
+x_mark           _sprite           sprite=xmark.spr
+whip_ring        _explosion        timer=0.400000 typeflags=0x31 spritething=+whip_spr_ring spritestart=(0.050000/0.050000/1.000000) spriteend=(0.300000/0.300000/0.500000)
+whip_rays        _explosion        timer=0.400000 creatething=+whip_ring typeflags=0x3B blasttime=0.250000 babytime=0.100000 spritething=+whip_spr_rays spritestart=(0.050000/0.050000/1.000000) spriteend=(0.300000/0.300000/0.500000)
+whip_flash       _explosion        timer=0.250000 creatething=+whip_rays typeflags=0x7F damage=200.000000 blasttime=0.250000 babytime=0.050000 range=0.250000 spritething=+whip_spr_flash spritestart=(0.050000/0.050000/1.000000) spriteend=(0.600000/0.600000/0.500000)
+whip_electric    _weapon           timer=0.170000 size=0.030000 movesize=0.030000 vel=(0.000000/3.000000/0.000000) explode=+whip_flash damage=500.000000 typeflags=0x10D damageclass=0x800
+imp1_long        _weapon           size=0.050000 movesize=0.050000 vel=(0.000000/4.000000/0.000000) damage=10.000000 typeflags=0x20200D damageclass=0x1000 range=0.135000
+imp4_sparks1     _particle         mass=0.500000 physflags=0x201 material=gen_a4sprite_rbblast.mat range=0.020000 rate=18.000000 maxthrust=50.000000 elementsize=0.010000 count=20
+imp4_sparks2     +imp4_sparks1     material=gen_a4sprite_flash_purple.mat
splashenter       _ghostdecor       thingflags=0x1 light=(0.500000/0.500000/0.500000/0.050000) timer=0.666000 model3d=gen_sfx_splash.3do size=0.025113 movesize=0.025113 collwidth=0.025113 collheight=0.025113
hat               _item             model3d=gen_hat.3do size=0.016000 movesize=0.016000 collwidth=0.020000 collheight=0.020000 puppet=default.pup height=0.009000 buoyancy=1.000000
chopperdustfx     _particle         timer=2.000000 vel=(0.200000/0.200000/-0.200000) angvel=(0.000000/360.000000/0.000000) material=cyn_4dust_particle_for_helicopter_scene.mat range=0.400000 minsize=0.200000 elementsize=0.005000 count=30
_debrisnc         _debris           collide=0
stnshrapa_nc_ns   _debrisnc         thingflags=0x1 light=(0.100000/0.100000/0.100000/1.000000) model3d=gen_dbre_stnwala.3do size=0.012500 movesize=0.012500 collwidth=0.012500 collheight=0.012500 angvel=(0.000000/0.000000/0.000000)
h2ofx             _particle         thingflags=0x8000000 airdrag=2.100000 mass=0.200000 physflags=0x201 material=gen_4particle_snow.mat range=0.070000 rate=15.000000 maxthrust=11.000000 elementsize=0.020000 count=60
titanich2ofx      h2ofx             timer=2.000000 mass=0.210000 vel=(0.000000/0.000000/0.300000) material=gen_4sfx_particle_waterfall.mat range=0.100000 maxthrust=9.000000 elementsize=0.008000 count=80
_pstruct2mwood    _structure        thingflags=0x44002040
2x2comcrate       _pstruct2mwood    model3d=vol_crate_2x2cyrlc.3do size=0.173205 movesize=0.173205 collwidth=0.173205 collheight=0.173205
radiotower        _sstructmetal     model3d=bab_tower_radio.3do size=0.564707 movesize=0.564707 collwidth=0.564707 collheight=0.564707
fence_door        _wstructmetal     model3d=bab_doors_fence.3do size=0.250950 movesize=0.250950 collwidth=0.250950 collheight=0.250950
fence_track       _ghostdecor       model3d=bab_doors_fnctrk.3do size=0.600817 movesize=0.600817 collwidth=0.600817 collheight=0.600817
lamphang          _ghostdecor       model3d=bab_lamp_hang.3do size=0.015500 movesize=0.015500 collwidth=0.015500 collheight=0.015500
hlfm_lamp_cord    _ghostdecor       model3d=bab_cord_hlfmlamp.3do size=0.025000 movesize=0.025000 collwidth=0.025000 collheight=0.025000
_structmetal      _structure        thingflags=0x400000
holedoor          _structmetal      model3d=bab_door_hole.3do size=0.179400 movesize=0.179400 collwidth=0.179400 collheight=0.179400
bannera           _staticstruct     model3d=bab_banner_cccpa.3do size=0.213600 movesize=0.213600 collwidth=0.213600 collheight=0.213600
lever             _walkstruct       model3d=gen_lever.3do size=0.060010 movesize=0.060010 collwidth=0.060010 collheight=0.060010 puppet=default.pup
trckdoors         _sstructmetal     thingflags=0x4000C1 light=(0.300000/0.300000/0.300000/0.001000) model3d=bab_doors_truck.3do size=0.200000 movesize=0.200000 collwidth=0.200000 collheight=0.200000
outhanglamp       _ghostdecor       model3d=sol_lamp_hang_out.3do size=0.031623 movesize=0.031623 collwidth=0.031623 collheight=0.031623
3m_lamp_cord      _ghostdecor       model3d=bab_cord_3mlamp.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000
litengine         _sstructmetal     thingflags=0x4000C1 light=(0.300000/0.300000/0.300000/0.001000) model3d=bab_ngin_lite.3do size=0.337315 movesize=0.337315 collwidth=0.337315 collheight=0.337315 puppet=default.pup
placque_b         _sstructmetal     model3d=bab_plaks_cccpb.3do size=0.070887 movesize=0.070887 collwidth=0.070887 collheight=0.070887
placque_a         _sstructmetal     model3d=bab_plaks_cccpa.3do size=0.070887 movesize=0.070887 collwidth=0.070887 collheight=0.070887
ruina             _staticstruct     model3d=bab_walls_ruin_a.3do size=0.222411 movesize=0.222411 collwidth=0.222411 collheight=0.222411
8mstake           _ghostdecor       model3d=cyn_tent_stake_8meter.3do size=0.020000 movesize=0.020000 collwidth=0.020000 collheight=0.020000
_humanactor       _actor            thingflags=0x28004000 size=0.040000 movesize=0.040000 collwidth=0.035000 collheight=0.089000 surfdrag=3.000000 airdrag=0.500000 staticdrag=0.005000 maxvel=0.500000 health=100.000000 maxhealth=1000.000000 maxthrust=0.400000 maxrotthrust=50.000000 typeflags=0x8000001 eyeoffset=(0.000000/0.012000/0.071500)
_ai_actor_normal  _humanactor       surfdrag=5.000000 maxthrust=0.865000 maxrotthrust=700.000000
_ai_tokarev       _ai_actor_normal  thingflags=0x28004400 size=0.037000 movesize=0.037000 puppet=com_1h.pup cog=actor_aitokarev.cog health=200.000000 maxhealth=200.000000 aiclass=com_1h.ai
khaki_tokarev4    _ai_tokarev       model3d=com_1h_kaki.3do soundclass=russki4.snd
_ai_actor_fast    _humanactor       surfdrag=5.000000 maxthrust=1.000000 maxrotthrust=750.000000
_ai_simonov       _ai_actor_fast    thingflags=0x28004400 size=0.037000 movesize=0.037000 puppet=com_2h.pup cog=actor_aisimonov.cog health=250.000000 maxhealth=250.000000 aiclass=com_2h.ai
khaki_simonov     _ai_simonov       model3d=com_2h_kaki.3do soundclass=russki1.snd
ustrutbabee       _swingstruct      model3d=bab_strut_u.3do collwidth=0.100000 collheight=0.100000
khaki_tokarev5    _ai_tokarev       model3d=com_1h_kaki.3do soundclass=russki5.snd
khaki_tokarev2    _ai_tokarev       model3d=com_1h_kaki.3do soundclass=russki2.snd
khaki_tokarev     _ai_tokarev       model3d=com_1h_kaki.3do soundclass=russki1.snd voicecolor=(0.765000/0.608000/0.196000/1.000000/0.765000/0.608000/0.196000/1.000000/0.980000/1.000000/0.922000/1.000000/0.980000/1.000000/0.922000/1.000000)
1m_lamp_cord      _ghostdecor       model3d=bab_cord_1mlamp.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
khaki_simonov_actor _cutactor         model3d=com_2h_kaki.3do collwidth=0.020000 collheight=0.020000 puppet=com_2h.pup voicecolor=(0.765000/0.608000/0.196000/1.000000/0.765000/0.608000/0.196000/1.000000/0.980000/1.000000/0.922000/1.000000/0.980000/1.000000/0.922000/1.000000)
turner_actor      _cutactor         thingflags=0x20084400 model3d=gen_tu.3do collwidth=0.020000 collheight=0.020000 puppet=turner.pup soundclass=turner.snd cog=actor_turner.cog mass=0.000000 maxrotthrust=90.000000 eyeoffset=(0.000000/0.011000/0.071400) voicecolor=(0.078000/0.471000/1.000000/1.000000/0.078000/0.471000/1.000000/1.000000/0.922000/1.000000/1.000000/1.000000/0.922000/1.000000/1.000000/1.000000)
lift_stone        _staticstruct     thingflags=0xC1 light=(0.300000/0.300000/0.300000/0.001000) model3d=bab_lift_stone.3do size=0.400000 movesize=0.100000 collwidth=0.050000 collheight=0.050000 soundclass=bab_turnerdoor.snd
bullblok          _pushstruct2m     thingflags=0x44000041 light=(0.400000/0.400000/0.400000/0.001000) model3d=bab_bull_block.3do size=0.173205 movesize=0.173205 collwidth=0.173205 collheight=0.173205
wallbrick         _staticstruct     thingflags=0xC1 light=(0.180000/0.170000/0.210000/0.001000) model3d=bab_wall_brick.3do size=0.286445 movesize=0.286445 collwidth=0.286445 collheight=0.286445 soundclass=gen_stonedoor.snd
_decormetal       _decor            thingflags=0x400000
flamebowl         _decormetal       thingflags=0x400001 light=(0.000000/0.000000/0.000000/0.300000) model3d=teo_bowl_flame.3do size=0.031101 movesize=0.031101 collwidth=0.031101 collheight=0.031101
babbut            _decor            thingflags=0x1 light=(0.600000/0.600000/0.600000/0.001000) model3d=bab_button_halfm.3do size=0.024500 movesize=0.024500 collwidth=0.024500 collheight=0.024500 soundclass=gen_stonebutn.snd
_aimstruct        _structure        type=actor health=50.000000 maxhealth=50.000000 typeflags=0x120
_boomthingwood    _aimstruct        thingflags=0x400 cog=actor_boomthingwood.cog
woodshrapa        _debris           model3d=gen_dbre_wodwala.3do size=0.015532 movesize=0.015532 collwidth=0.015532 collheight=0.015532
woodshrapb        _debris           model3d=gen_dbre_wodwalb.3do size=0.012500 movesize=0.012500 collwidth=0.012500 collheight=0.012500
woodshrapc        _debris           model3d=gen_dbre_wodwalc.3do size=0.013297 movesize=0.013297 collwidth=0.013297 collheight=0.013297
+gen_exp_wood     _explosion        light=(0.000000/0.000000/0.000000/1.000000) timer=0.500000 sprite=gen_explode3.spr soundclass=gen_explode.snd typeflags=0xB7 damage=1000.000000 blasttime=0.400000 range=0.500000 debris=woodshrapa debris=woodshrapb debris=woodshrapc debris=woodshrapa debris=woodshrapb debris=woodshrapc debris=woodshrapa debris=woodshrapb debris=woodshrapc debris=woodshrapa debris=woodshrapb debris=woodshrapc debris=woodshrapa debris=woodshrapb debris=woodshrapc
tntcrate          _boomthingwood    model3d=bab_crate_tnt.3do size=0.150000 movesize=0.150000 collwidth=0.061237 collheight=0.061237 explode=+gen_exp_wood
rounddoor         _sstructmetal     thingflags=0x4000C1 light=(1.000000/1.000000/1.000000/0.001000) model3d=bab_door_round.3do size=0.148000 movesize=0.148000 collwidth=0.148000 collheight=0.148000
elgearsb          _sstructmetal     model3d=bab_elgears_b.3do size=0.065983 movesize=0.065983 collwidth=0.065983 collheight=0.065983
elgears           _ghostdecor       model3d=bab_elgears.3do size=0.065983 movesize=0.065983 collwidth=0.065983 collheight=0.065983 puppet=default.pup
brgbroka          _staticstruct     model3d=bab_brdg_brka.3do size=0.141833 movesize=0.141833 collwidth=0.141833 collheight=0.141833
brgbrokd          _staticstruct     model3d=bab_brdg_brkd.3do size=0.134052 movesize=0.134052 collwidth=0.134052 collheight=0.134052
brgbroke          _staticstruct     model3d=bab_brdg_brke.3do size=0.124905 movesize=0.124905 collwidth=0.124905 collheight=0.124905
brgbrokb          _staticstruct     model3d=bab_brdg_brkb.3do size=0.130219 movesize=0.130219 collwidth=0.130219 collheight=0.130219
mardukidolmove    _decor            thingflags=0x1 light=(0.750000/0.750000/0.750000/0.001000) model3d=bab_idol_mrdk.3do size=0.023000 movesize=0.023000 collwidth=0.023000 collheight=0.023000
hidedoor          _staticstruct     thingflags=0xC1 light=(0.400000/0.300000/0.200000/0.001000) model3d=teo_door_2x2mhide.3do size=0.142222 movesize=0.142222 collwidth=0.142222 collheight=0.142222 soundclass=gen_stonedoor.snd
brgblka           _pushstruct2m     model3d=bab_brdg_blcka.3do size=0.173205 movesize=0.173205 collwidth=0.173205 collheight=0.173205
brgblkb           _staticstruct     model3d=bab_brdg_blckb.3do size=0.206155 movesize=0.206155 collwidth=0.206155 collheight=0.206155
ustrutclimb       _climbstruct      model3d=bab_strut_u.3do size=0.040000 movesize=0.040000 collwidth=0.040000 collheight=0.040000
_pushstruct1m     _structure        thingflags=0x40001040
1x1babrock        _pushstruct1m     model3d=bab_rock_1x1push.3do size=0.085737 movesize=0.085737 collwidth=0.085737 collheight=0.085737
tabletamove       _structure        thingflags=0x1 light=(0.600000/0.600000/0.600000/0.001000) model3d=bab_tblt_a.3do size=0.070887 movesize=0.070887 collwidth=0.070887 collheight=0.070887
tabletbmove       _structure        thingflags=0x1 light=(0.600000/0.600000/0.600000/0.001000) model3d=bab_tblt_b.3do size=0.042720 movesize=0.042720 collwidth=0.042720 collheight=0.042720
tabletcmove       _structure        thingflags=0x1 light=(0.600000/0.600000/0.600000/0.001000) model3d=bab_tblt_c.3do size=0.015000 movesize=0.015000 collwidth=0.015000 collheight=0.015000
spike_block       _staticstruct     model3d=teo_block_spike.3do size=0.179258 movesize=0.179258 collwidth=0.179258 collheight=0.179258
vol_actor         _cutactor         thingflags=0x20084400 model3d=gen_vo.3do collwidth=0.020000 collheight=0.020000 puppet=volodnikov.pup soundclass=volodnikov.snd cog=actor_volodnikov.cog typeflags=0x8118009 eyeoffset=(0.000000/0.014200/0.070100) voicecolor=(0.314000/0.784000/0.157000/1.000000/0.314000/0.784000/0.157000/1.000000/1.000000/1.000000/0.196000/1.000000/1.000000/1.000000/0.196000/1.000000)
radioman          _cutactor         model3d=bab_rm.3do collwidth=0.020000 collheight=0.020000 puppet=radioman.pup mass=0.000000 voicecolor=(0.765000/0.608000/0.196000/1.000000/0.765000/0.608000/0.196000/1.000000/0.980000/1.000000/0.922000/1.000000/0.980000/1.000000/0.922000/1.000000)
kitchen_table     _sstructwood      model3d=bab_table_ktchn.3do size=0.126957 movesize=0.126957 collwidth=0.126957 collheight=0.126957
tracksm           _staticstruct     model3d=bab_doors_smltrk.3do size=0.027500 movesize=0.027500 collwidth=0.027500 collheight=0.027500
killtruk          _actor            thingflags=0x441 light=(0.750000/0.750000/0.750000/0.001000) model3d=bab_tr.3do size=0.325000 movesize=0.325000 collwidth=0.125000 collheight=0.325000 puppet=default.pup soundclass=truck.snd surfdrag=3.000000 staticdrag=0.300000 mass=5000.000000 height=0.157000 physflags=0x1004A5F maxvel=0.400000 maxthrust=0.350000 maxrotthrust=0.000000 typeflags=0x100009
flatbed           _sstructmetal     model3d=bab_truck_flatbed.3do size=0.352527 movesize=0.352527 collwidth=0.352500 collheight=0.157000
_sstructsnow      _structure        thingflags=0x100C0
door_canopy       _sstructsnow      model3d=bab_canopy_door.3do size=0.176423 movesize=0.176423 collwidth=0.176423 collheight=0.176423
khaki_tokarev3    _ai_tokarev       model3d=com_1h_kaki.3do soundclass=russki3.snd
elvtrgears        _staticstruct     model3d=inf_elvtr_gears.3do size=0.225000 movesize=0.225000 collwidth=0.225000 collheight=0.225000 puppet=default.pup
elvtrcap          _sstructmetal     model3d=inf_elvtr_cap.3do size=0.236656 movesize=0.236656 collwidth=0.236656 collheight=0.236656
skelcof           _ghostdecor       model3d=skeleton_coffin.3do size=0.010713 movesize=0.010713 collwidth=0.010713 collheight=0.010713
hintnumber36      _hintbase         userval=36.000000
hintnumber38      _hintbase         userval=38.000000
hintnumber40      _hintbase         userval=40.000000
hintnumber42      _hintbase         userval=42.000000
hintnumber44      _hintbase         userval=44.000000
hintnumber46      _hintbase         userval=46.000000
hintnumber48      _hintbase         userval=48.000000
hintnumber50      _hintbase         userval=50.000000
hintnumber51      _hintbase         userval=51.000000
hintnumber52      _hintbase         userval=52.000000
hintnumber53      _hintbase         userval=53.000000
hintnumber54      _hintbase         userval=54.000000
hintnumber55      _hintbase         userval=55.000000
hintnumber56      _hintbase         userval=56.000000
hintnumber57      _hintbase         userval=57.000000
hintnumber59      _hintbase         userval=59.000000
hintnumber58      _hintbase         userval=58.000000
silveringot       _item             thingflags=0x401 light=(0.200000/0.200000/0.200000/0.001000) model3d=gen_ingots_slvr.3do collwidth=0.020000 collheight=0.020000 cog=item_ingotsilver.cog height=0.004100
_gen_ai_avrg      _actor            thingflags=0x8000000 surfdrag=1.500000 airdrag=2.000000 staticdrag=0.005000 mass=30.000000 health=100.000000 maxhealth=100.000000 maxthrust=0.450000 maxrotthrust=200.000000
+scorpbite        +bughit           damage=5.000000 damageclass=0x100
scorpian          _gen_ai_avrg      thingflags=0x8000400 model3d=gen_sc.3do size=0.050000 movesize=0.050000 collwidth=0.052700 collheight=0.052700 puppet=scorpian.pup soundclass=scorpian.snd cog=actor_scorpian.cog weapon=+scorpbite typeflags=0x80020000 eyeoffset=(0.000000/0.000000/-0.040000) aiclass=scorpion.ai
nssortable        _sstructmetal     model3d=cyn_table_sorting_noshad.3do size=0.090000 movesize=0.090000 collwidth=0.090000 collheight=0.090000
mettub            _sstructmetal     model3d=jep_tub.3do size=0.058310 movesize=0.058310 collwidth=0.058310 collheight=0.058310
hlthbig           _item             thingflags=0x400 model3d=gen_icon_hlthbig.3do collwidth=0.020000 collheight=0.020000 cog=item_healthbig.cog height=0.011313
bonepileb         _ghostdecor       model3d=gen_bones_pileb.3do size=0.009409 movesize=0.009409 collwidth=0.009409 collheight=0.009409
bonepilea         _ghostdecor       model3d=gen_bones_pilea.3do size=0.011461 movesize=0.011461 collwidth=0.011461 collheight=0.011461
+spiderbite       +bughit           damage=2.500000 damageclass=0x100
spider            _gen_ai_avrg      thingflags=0x8000400 model3d=gen_sp.3do size=0.032500 movesize=0.032500 collwidth=0.032500 collheight=0.032500 puppet=spider.pup soundclass=spider.snd cog=actor_spider.cog weapon=+spiderbite typeflags=0x80000000 eyeoffset=(0.000000/0.000000/-0.020000) aiclass=spider.ai
_itemex           _item             thingflags=0x1 light=(0.250000/0.250000/0.250000/0.001000) movesize=0.003000 surfdrag=0.250000 staticdrag=0.150000 mass=5.000000 physflags=0x51
poisonkit         _itemex           thingflags=0x401 model3d=gen_icon_bite.3do movesize=0.002000 collwidth=0.003000 collheight=0.003000 cog=item_poisonkit.cog
gnr8tr            _sstructmetal     model3d=jep_gnr8tr.3do size=0.075333 movesize=0.075333 collwidth=0.075333 collheight=0.075333
2x1comcrate       _sstructwood      model3d=vol_crate_2x1cyrlcb.3do size=0.122474 movesize=0.122474 collwidth=0.122474 collheight=0.122474
sealamp           _sstructmetal     model3d=sea_lamp_desk.3do size=0.020657 movesize=0.020657 collwidth=0.020657 collheight=0.020657
stnshrpb          _ghostdecor       model3d=gen_dbre_stnwalb.3do size=0.043652 movesize=0.043652 collwidth=0.043652 collheight=0.043652
stnshrpbm         _ghostdecor       model3d=gen_dbre_stnwalbm.3do size=0.006250 movesize=0.006250 collwidth=0.006250 collheight=0.006250
stnshrpbs         _ghostdecor       model3d=gen_dbre_stnwalbs.3do size=0.003125 movesize=0.003125 collwidth=0.003125 collheight=0.003125
stnshrpc          _ghostdecor       model3d=gen_dbre_stnwalc.3do size=0.012601 movesize=0.012601 collwidth=0.012601 collheight=0.012601
stnshrpcm         _ghostdecor       model3d=gen_dbre_stnwalcm.3do size=0.006250 movesize=0.006250 collwidth=0.006250 collheight=0.006250
stnshrpcs         _ghostdecor       model3d=gen_dbre_stnwalcs.3do size=0.003125 movesize=0.003125 collwidth=0.003125 collheight=0.003125
stnshrpam         _ghostdecor       model3d=gen_dbre_stnwalam.3do size=0.007778 movesize=0.007778 collwidth=0.007778 collheight=0.007778
stnshrpas         _ghostdecor       model3d=gen_dbre_stnwalas.3do size=0.003867 movesize=0.003867 collwidth=0.003867 collheight=0.003867
bksnglra          _sstructwood      model3d=sol_book_snglra.3do size=0.025000 movesize=0.025000 collwidth=0.025000 collheight=0.025000
_boomthing        _aimstruct        thingflags=0x400 cog=actor_boomthing.cog
metalshrapa       _debris           model3d=gen_dbre_mtlwala.3do size=0.018717 movesize=0.018717 collwidth=0.018717 collheight=0.018717
metalshrapb       _debris           model3d=gen_dbre_mtlwalb.3do size=0.012500 movesize=0.012500 collwidth=0.012500 collheight=0.012500
metalshrapc       _debris           model3d=gen_dbre_mtlwalc.3do size=0.015166 movesize=0.015166 collwidth=0.015166 collheight=0.015166
+barrel_exp       _explosion        light=(0.200000/0.150000/0.150000/0.500000) timer=0.750000 sprite=gen_explode2.spr soundclass=gen_explode.snd typeflags=0x17 damage=1000.000000 blasttime=0.699000 range=0.500000 debris=metalshrapa debris=metalshrapb debris=metalshrapc debris=metalshrapa debris=metalshrapb debris=metalshrapc debris=metalshrapa debris=metalshrapb debris=metalshrapc debris=metalshrapa debris=metalshrapb debris=metalshrapc debris=metalshrapa debris=metalshrapb debris=metalshrapc
1moil_ra_boom     _boomthing        model3d=lag_drum_1moilra.3do size=0.061033 movesize=0.061033 collwidth=0.061033 collheight=0.061033 explode=+barrel_exp
_pstruct1mwood    _structure        thingflags=0x40003040
1x1comcrate       _pstruct1mwood    model3d=vol_crate_1x1cyrlc.3do size=0.086172 movesize=0.086172 collwidth=0.086172 collheight=0.086172
bbbaaaaa          _staticstruct     model3d=gen_bbb_sheep.3do size=0.038660 movesize=0.038660 collwidth=0.038660 collheight=0.038660
keyhole           _decor            model3d=gen_keyhole.3do size=0.024500 movesize=0.024500 collwidth=0.024500 collheight=0.024500
1moila            _boomthing        model3d=sol_drum_1moila.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000 explode=+barrel_exp
hlfmoila          _boomthing        model3d=sol_drum_hlfmoila.3do size=0.025000 movesize=0.025000 collwidth=0.025000 collheight=0.025000 explode=+barrel_exp
gemgre            _item             thingflags=0x401 light=(0.200000/0.200000/0.200000/0.001000) model3d=gen_gem_green.3do collwidth=0.020000 collheight=0.020000 cog=item_gemgreen.cog height=0.007848
spider_wall       spider            cog=actor_stickyspider.cog physflags=0x91 typeflags=0x80120000 aiclass=stickyspider.ai
invisiplat1m      _structblock      model3d=gen_invisiplate1m.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
_decorblock       _decor            thingflags=0x14
inviscube1m       _decorblock       model3d=gen_box_1minvis.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
_ai_mauser        _ai_actor_normal  thingflags=0x28004400 size=0.037000 movesize=0.037000 puppet=com_1h.pup cog=actor_aimauser.cog health=200.000000 maxhealth=200.000000 aiclass=com_1h.ai
khaki_mauser      _ai_mauser        model3d=com_1h_kaki.3do soundclass=russki1.snd
hlthsml           _itemex           thingflags=0x401 model3d=gen_icon_hlthsml.3do collwidth=0.003000 collheight=0.003000 cog=item_healthsml.cog
bigtenttop        _sstructearth     model3d=gen_tentbig_brwn_top.3do size=0.578792 movesize=0.578792 collwidth=0.578792 collheight=0.578792
bigtenta          _sstructearth     model3d=gen_tentbig_brwna.3do size=0.427931 movesize=0.427931 collwidth=0.427931 collheight=0.427931
bigtentb          _sstructearth     model3d=gen_tentbig_brwnb.3do size=0.336342 movesize=0.336342 collwidth=0.336342 collheight=0.336342
bigtentropole     _structwood       model3d=gen_tentbig_brwn_ropole.3do size=0.711216 movesize=0.711216 collwidth=0.711216 collheight=0.711216
bigtentfloor      _ghostdecor       model3d=gen_tentbig_brwn_floor.3do
bigtentd          _sstructearth     model3d=gen_tentbig_brwnd.3do size=0.336339 movesize=0.336339 collwidth=0.336339 collheight=0.336339
bigtentc          _sstructearth     model3d=gen_tentbig_brwnc.3do size=0.427931 movesize=0.427931 collwidth=0.427931 collheight=0.427931
medtentfloor      _ghostdecor       model3d=gen_tentmed_brwn_floor.3do size=0.502195 movesize=0.502195 collwidth=0.502195 collheight=0.502195
medtentropole     _structwood       model3d=gen_tentmed_brwn_ropole.3do size=0.626300 movesize=0.626300 collwidth=0.626300 collheight=0.626300
medtenttop        _sstructearth     model3d=gen_tentmed_brwn_top.3do size=0.441588 movesize=0.441588 collwidth=0.441588 collheight=0.441588
medtenta          _sstructearth     model3d=gen_tentmed_brwna.3do size=0.336341 movesize=0.336341 collwidth=0.336341 collheight=0.336341
medtentb          _sstructearth     model3d=gen_tentmed_brwnb.3do size=0.336341 movesize=0.336341 collwidth=0.336341 collheight=0.336341
medtentc          _sstructearth     model3d=gen_tentmed_brwnc.3do size=0.336341 movesize=0.336341 collwidth=0.336341 collheight=0.336341
medtentd          _sstructearth     model3d=gen_tentmed_brwnd.3do size=0.336341 movesize=0.336341 collwidth=0.336341 collheight=0.336341
inviskilltruk     _wstructmetal     thingflags=0x400050 model3d=bab_tr_invisbox.3do size=0.330129 movesize=0.330129 collwidth=0.330129 collheight=0.330129
herbbig           _item             thingflags=0x400 model3d=gen_herb_big.3do collwidth=0.020000 collheight=0.020000 cog=item_yerbisma.cog height=0.020500
herbsml           _item             thingflags=0x400 model3d=gen_herb_small.3do collwidth=0.020000 collheight=0.020000 cog=item_yerbita.cog height=0.010276
invsbox1x1        _structblock      model3d=gen_box_1minvis_col.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
torchflame        _decor            thingflags=0x400 model3d=gen_torch_flame.3do size=0.028004 movesize=0.028004 collwidth=0.028004 collheight=0.028004 soundclass=gen_flame.snd cog=class_torch.cog
redsparks         _particle         timer=0.500000 mass=0.500000 physflags=0x201 material=gen_a4sprite_rbblast.mat range=0.020000 rate=18.000000 maxthrust=50.000000 elementsize=0.028000 count=20
bluesparks        redsparks         material=gen_a4sprite_flash_purple.mat
tokarev           _itemex           thingflags=0x401 model3d=weap_tokarev_inv.3do movesize=0.002000 collwidth=0.003000 collheight=0.003000 cog=item_tokarev.cog
simonov           _itemex           thingflags=0x401 model3d=weap_simonov_inv.3do collwidth=0.003000 collheight=0.003000 cog=item_simonov.cog
mauser            _itemex           thingflags=0x401 model3d=weap_mauser_inv.3do movesize=0.002000 collwidth=0.003000 collheight=0.003000 cog=item_mauser.cog
bronzkeymove      _decor            model3d=gen_bronz_key.3do size=0.007905 movesize=0.007905 collwidth=0.007905 collheight=0.007905
2x2babcrate       _pushstruct2m     thingflags=0x44000041 light=(0.400000/0.400000/0.400000/0.001000) model3d=bab_brdg_blcka.3do size=0.173205 movesize=0.173205 collwidth=0.173205 collheight=0.173205
stoneshrapa_nc    _debrisnc         model3d=gen_dbre_stnwala.3do size=0.012500 movesize=0.012500 collwidth=0.012500 collheight=0.012500
stoneshrapam_nc   _debrisnc         model3d=gen_dbre_stnwalam.3do size=0.007778 movesize=0.007778 collwidth=0.007778 collheight=0.007778
stoneshrapas_nc   _debrisnc         model3d=gen_dbre_stnwalas.3do size=0.003867 movesize=0.003867 collwidth=0.003867 collheight=0.003867
stoneshrapb_nc    _debrisnc         model3d=gen_dbre_stnwalb.3do size=0.043652 movesize=0.043652 collwidth=0.043652 collheight=0.043652
stoneshrapbs_nc   _debrisnc         model3d=gen_dbre_stnwalbs.3do size=0.003125 movesize=0.003125 collwidth=0.003125 collheight=0.003125
stnshrpa          _ghostdecor       model3d=gen_dbre_stnwala.3do size=0.012500 movesize=0.012500 collwidth=0.012500 collheight=0.012500
+gen_exp_metal    _explosion        light=(0.000000/0.000000/0.000000/1.000000) timer=0.500000 sprite=gen_explode3.spr soundclass=gen_explode.snd typeflags=0xB7 damage=1000.000000 blasttime=0.400000 range=0.500000 debris=metalshrapa debris=metalshrapb debris=metalshrapc debris=metalshrapa debris=metalshrapb debris=metalshrapc debris=metalshrapa debris=metalshrapb debris=metalshrapc debris=metalshrapa debris=metalshrapb debris=metalshrapc debris=metalshrapa debris=metalshrapb debris=metalshrapc
+bouldersmoke_spr _sprite           timer=3.000000 sprite=bouldersmoke.spr
jeep_actor_pyr    _cutactor         thingflags=0x20084400 model3d=gen_ij_pyr.3do collwidth=0.190000 collheight=0.090000 puppet=indy_jeep.pup cog=jeep_cutscene_pyr.cog physflags=0x4004A4F eyeoffset=(0.045400/0.025400/0.069500) aiclass=none.ai
3mpinetree        _sstructwood      model3d=riv_tree_3mpine.3do size=0.151625 movesize=0.151625 collwidth=0.151625 collheight=0.151625
9mptree           _sstructwood      model3d=riv_tree_9mpine.3do size=0.448697 movesize=0.448697 collwidth=0.448697 collheight=0.448697
12mptree          _sstructwood      model3d=riv_tree_12mpine.3do size=0.592245 movesize=0.592245 collwidth=0.592245 collheight=0.592245
watrock5m         _staticstruct     model3d=riv_wrock5m.3do size=0.415795 movesize=0.415795 collwidth=0.415795 collheight=0.415795
markgreen         _sstructmetal     model3d=riv_markergreen.3do size=0.290627 movesize=0.290627 collwidth=0.290627 collheight=0.290627
markred           _sstructmetal     model3d=riv_markerred.3do size=0.249066 movesize=0.249066 collwidth=0.249066 collheight=0.249066
6mpinetree        _sstructwood      model3d=riv_tree_6mpine.3do size=0.300326 movesize=0.300326 collwidth=0.300326 collheight=0.300326
bigrocksplash     _sprite           thingflags=0x401 sprite=riv_rocksplash_big.spr cog=riv_splashparticle.cog
smlrocksplash     _sprite           thingflags=0x401 sprite=riv_rocksplash_sml.spr cog=riv_splashparticle.cog
medrocksplash     _sprite           thingflags=0x401 sprite=riv_rocksplash_med.spr cog=riv_splashparticle.cog
bigrocksplasha    _sprite           thingflags=0x401 sprite=riv_rocksplash_big_a.spr cog=riv_splashparticle.cog
rocksb            _staticstruct     thingflags=0x4C0 model3d=riv_rocks_scaryb.3do size=0.245173 movesize=0.245173 collwidth=0.245173 collheight=0.245173 cog=class_sharpboulder.cog
rocksa            _staticstruct     thingflags=0x4C0 model3d=riv_rocks_scarya.3do size=0.291510 movesize=0.291510 collwidth=0.291510 collheight=0.291510 cog=class_sharpboulder.cog
rocksc            _staticstruct     thingflags=0x4C0 model3d=riv_rocks_scaryc.3do size=0.076552 movesize=0.076552 collwidth=0.076552 collheight=0.076552 cog=class_sharpboulder.cog
rivrocka          _staticstruct     model3d=riv_rock_a.3do size=0.398702 movesize=0.398702 collwidth=0.398702 collheight=0.398702
rivtabl           _staticstruct     model3d=riv_table_radio.3do size=0.129347 movesize=0.129347 collwidth=0.129347 collheight=0.129347
lilhanglamp       _ghostdecor       model3d=sol_lamp_hang.3do size=0.032500 movesize=0.032500 collwidth=0.032500 collheight=0.032500
seachair          _sstructmetal     model3d=sea_chair_metl.3do size=0.061749 movesize=0.061749 collwidth=0.061749 collheight=0.061749
seadesk           _sstructwood      model3d=sea_desk_cabn.3do size=0.108253 movesize=0.108253 collwidth=0.093000 collheight=0.055000
wallk             _staticstruct     model3d=riv_frame_wall_k.3do size=0.100000 movesize=0.100000 collwidth=0.100000 collheight=0.100000
snow_simonov5     _ai_simonov       model3d=com_2h_snow.3do soundclass=russki5.snd
grdshk            _sstructwood      model3d=riv_shck_grd.3do size=0.280992 movesize=0.280992 collwidth=0.280992 collheight=0.280992
snow_tokarev      _ai_tokarev       model3d=com_1h_snow.3do soundclass=russki1.snd
bordgate          _sstructwood      model3d=riv_border_gate.3do size=0.650000 movesize=0.650000 collwidth=0.620000 collheight=0.620000
_actor_car        _actor            thingflags=0x404000 puppet=default.pup surfdrag=2.000000 staticdrag=0.010000 mass=1000.000000 health=100000.000000 maxhealth=100000.000000 maxthrust=0.500000 maxrotthrust=350.000000 typeflags=0x8 aiclass=none.ai
snowtrk_ai_actor  _actor_car        model3d=riv_truck.3do size=0.352527 movesize=0.352527 collwidth=0.352500 collheight=0.157000 height=0.152000
notrsps           _sstructmetal     model3d=riv_rusign.3do size=0.099900 movesize=0.099900 collwidth=0.099900 collheight=0.099900
3mstandee         _sstructwood      model3d=riv_sntree_3m.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000
6mstandee         _sstructwood      model3d=riv_sntree_6m.3do size=0.300000 movesize=0.300000 collwidth=0.300000 collheight=0.300000
9mstandee         _sstructwood      model3d=riv_sntree_9m.3do size=0.450000 movesize=0.450000 collwidth=0.450000 collheight=0.450000
chute_indy        _cutactor         thingflags=0x0 model3d=gen_in_chute.3do collwidth=0.020000 collheight=0.020000 puppet=default.pup airdrag=2.000000 mass=1.000000 physflags=0x3202 maxthrust=0.800000 maxrotthrust=4.000000
planeriv          _ghostdecor       model3d=riv_plane.3do size=0.336184 movesize=0.336184 collwidth=0.336184 collheight=0.336184
cndla             _item             thingflags=0x400 model3d=riv_cndl_awind.3do collwidth=0.020000 collheight=0.020000 cog=item_candle.cog height=0.007000
4mbranch          _ghostdecor       model3d=riv_tree_4mbrnch.3do size=0.450694 movesize=0.450694 collwidth=0.450694 collheight=0.450694
3mbranch          _ghostdecor       model3d=riv_tree_3mbrnch.3do size=0.328824 movesize=0.328824 collwidth=0.328824 collheight=0.328824
2mbranch          _ghostdecor       model3d=riv_tree_2mbrnch.3do size=0.225000 movesize=0.225000 collwidth=0.225000 collheight=0.225000
16mpinetree       _sstructwood      model3d=riv_tree_16mpine.3do size=0.558850 movesize=0.558850 collwidth=0.558850 collheight=0.558850
hldr              _staticstruct     model3d=riv_door_hole.3do size=0.179327 movesize=0.179327 collwidth=0.179327 collheight=0.179327
cndlb             _item             thingflags=0x400 model3d=riv_cndl_bwind.3do collwidth=0.020000 collheight=0.020000 cog=item_candle.cog height=0.007000
elvtrpull         _sstructwood      model3d=riv_pulley_elvtr.3do size=0.434633 movesize=0.434633 collwidth=0.434633 collheight=0.434633
wheelpull         _sstructwood      model3d=riv_pulley_wheel.3do size=0.100000 movesize=0.100000 collwidth=0.100000 collheight=0.100000
lckrdoora         _sstructmetal     model3d=riv_door_lockera.3do size=0.133330 movesize=0.133330 collwidth=0.133330 collheight=0.133330
russflag          _staticstruct     model3d=riv_flag_russia.3do size=0.125036 movesize=0.125036 collwidth=0.125036 collheight=0.125036
antguard          _sstructmetal     model3d=riv_antna_guard.3do size=0.196102 movesize=0.196102 collwidth=0.196102 collheight=0.196102
beds4m            _sstructmetal     model3d=riv_bed_bunk_4m.3do size=0.274465 movesize=0.274465 collwidth=0.274465 collheight=0.274465
woodbund          _sstructwood      model3d=riv_wood_bundle.3do size=0.062818 movesize=0.062818 collwidth=0.062818 collheight=0.062818
snow_mauser       _ai_mauser        model3d=com_1h_snow.3do soundclass=russki1.snd
stove             _sstructwood      model3d=sol_stove.3do size=0.335293 movesize=0.335293 collwidth=0.335293 collheight=0.335293
bnkdoor           _sstructmetal     model3d=riv_bunkdoor.3do size=0.206912 movesize=0.206912 collwidth=0.206912 collheight=0.206912 soundclass=riv_bunkdoor.snd
lckrdoorb         _sstructmetal     model3d=riv_door_lockerb.3do size=0.133330 movesize=0.133330 collwidth=0.133330 collheight=0.133330
_wstruct2mwood    _structure        thingflags=0x4002040
gen2-1hang        _wstruct2mwood    model3d=gen_crate_2x1cyrlc.3do size=0.121865 movesize=0.121865 collwidth=0.121865 collheight=0.121865
cashbox           _item             thingflags=0x401 light=(0.200000/0.200000/0.200000/0.001000) model3d=riv_box_cash.3do collwidth=0.020000 collheight=0.020000 cog=item_cashbox.cog
rivbut            _decor            model3d=riv_buttn_psh.3do size=0.024500 movesize=0.024500 collwidth=0.024500 collheight=0.024500
walla             _staticstruct     model3d=riv_frame_wall_a.3do size=0.200000 movesize=0.200000 collwidth=0.200000 collheight=0.200000
raftplayer        _indyactor        model3d=gen_ir.3do size=0.100000 movesize=0.100000 collwidth=0.100000 collheight=0.058000 puppet=indy_raft.pup soundclass=indyraft.snd cog=actor_indyraft.cog surfdrag=0.250000 staticdrag=0.000000 height=0.030000 physflags=0x2005253 maxrotvel=135.000000 maxvel=0.500000 buoyancy=-0.500000 eyeoffset=(0.000000/0.029900/0.071400)
guardladr         _ghostdecor       model3d=riv_ladder.3do size=0.257148 movesize=0.257148 collwidth=0.257148 collheight=0.257148
indy_actor        _cutactor         thingflags=0x20084400 model3d=gen_in.3do collwidth=0.020000 collheight=0.020000 puppet=indy_actor.pup soundclass=indy.snd cog=actor_indyactor.cog aiclass=none.ai
monkelevtr        _wstructmetal     model3d=riv_elvtr_monk.3do size=0.150000 movesize=0.100000 collwidth=0.249872 collheight=0.249872
3mladr            _ghostdecor       model3d=riv_ladder_3m.3do
6mladr            _ghostdecor       model3d=riv_ladder_6m.3do
_gen_ai_fast      _actor            thingflags=0x8000000 surfdrag=3.000000 airdrag=0.300000 staticdrag=0.005000 mass=100.000000 maxvel=1.100000 health=250.000000 maxhealth=250.000000 maxthrust=1.200000 maxrotthrust=250.000000
+wolfbite         +bughit           damage=50.000000
arctic_wolf       _gen_ai_fast      thingflags=0x8000400 model3d=gen_aw.3do size=0.036000 movesize=0.036000 collwidth=0.036721 collheight=0.036721 puppet=wolf.pup soundclass=wolf.snd cog=actor_wolf.cog surfdrag=5.000000 physflags=0x425F orientspeed=0.400000 weapon=+wolfbite health=750.000000 maxhealth=750.000000 maxthrust=2.550000 typeflags=0x128001 aiclass=wolf.ai
cndldmove         _decor            model3d=riv_cndl_dwind.3do size=0.021265 movesize=0.021265 collwidth=0.021265 collheight=0.021265 soundclass=riv_candle.snd
elvgroove         _sstructwood      model3d=riv_lift_groove.3do size=0.115000 movesize=0.115000 collwidth=0.115000 collheight=0.115000
elvwheel          _sstructmetal     model3d=riv_lift_wheel.3do size=0.187500 movesize=0.187500 collwidth=0.187500 collheight=0.187500
elvtrans          _sstructmetal     model3d=riv_lift_trans.3do size=0.100000 movesize=0.100000 collwidth=0.100000 collheight=0.100000
elvbase           _sstructmetal     model3d=riv_lift_base.3do size=0.060000 movesize=0.060000 collwidth=0.060000 collheight=0.060000
bnkrtabl          _sstructwood      model3d=riv_table_bnkr.3do size=0.143351 movesize=0.143351 collwidth=0.143351 collheight=0.143351
entrydome         _staticstruct     model3d=shs_dome_entry.3do size=0.390455 movesize=0.390455 collwidth=0.390455 collheight=0.390455
2mplank           _sstructwood      model3d=shs_plank_2m.3do size=0.112500 movesize=0.112500 collwidth=0.112500 collheight=0.112500
railpost          _sstructwood      model3d=shw_post_rail.3do size=0.067361 movesize=0.067361 collwidth=0.067361 collheight=0.067361
2m_rail           _ghostdecor       model3d=shw_rail_2m.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
shslampstrutclimb _climbstruct      model3d=shs_strut_lamp.3do size=0.059222 movesize=0.059222 collwidth=0.059222 collheight=0.059222
chainlit          _decormetal       model3d=shs_lamp_chain_lit.3do size=0.070000 movesize=0.070000 collwidth=0.070000 collheight=0.070000
5m_brace          _ghostdecor       model3d=sol_brace_5m.3do size=0.007500 movesize=0.007500 collwidth=0.007500 collheight=0.007500
skelsit           _ghostdecor       model3d=skeleton_sit.3do size=0.029102 movesize=0.029102 collwidth=0.029102 collheight=0.029102
snow_mauser2      _ai_mauser        model3d=com_1h_snow.3do soundclass=russki2.snd
medrocksplasha    _sprite           thingflags=0x401 sprite=riv_rocksplash_med_a.spr cog=riv_splashparticle.cog
medrocksplashb    _sprite           thingflags=0x401 sprite=riv_rocksplash_med_b.spr cog=riv_splashparticle.cog
smlrocksplashb    _sprite           thingflags=0x401 sprite=riv_rocksplash_sml_b.spr cog=riv_splashparticle.cog
smlrocksplasha    _sprite           thingflags=0x401 sprite=riv_rocksplash_sml_a.spr cog=riv_splashparticle.cog
inviscubeaim      _aimstruct        model3d=gen_box_1minvis.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
bigrocksplashb    _sprite           thingflags=0x401 sprite=riv_rocksplash_big_b.spr cog=riv_splashparticle.cog
drvwhl            _decormetal       model3d=shw_drive_whel.3do size=0.219620 movesize=0.219620 collwidth=0.219620 collheight=0.219620 puppet=default.pup
drivbelt          _sstructmetal     model3d=shw_drive_belt.3do size=0.560603 movesize=0.560603 collwidth=0.560603 collheight=0.560603
hamount           _staticstruct     model3d=shw_mount_hamr.3do size=0.117260 movesize=0.117260 collwidth=0.117260 collheight=0.117260
_wstructwood      _structure        thingflags=0x2040
hammilla          _wstructwood      thingflags=0x2041 light=(0.400000/0.400000/0.400000/0.001000) model3d=shw_hamr_mill.3do size=0.405000 movesize=0.405000 collwidth=0.405000 collheight=0.405000
hammillb          _wstructwood      thingflags=0x2041 light=(0.400000/0.400000/0.400000/0.001000) model3d=shw_hamr_mill.3do size=0.405000 movesize=0.405000 collwidth=0.405000 collheight=0.405000
hammilld          _wstructwood      thingflags=0x2041 light=(0.400000/0.400000/0.400000/0.001000) model3d=shw_hamr_mill.3do size=0.405000 movesize=0.405000 collwidth=0.405000 collheight=0.405000
crvcam            _sstructwood      thingflags=0x20C1 light=(0.400000/0.400000/0.400000/0.001000) model3d=shw_drive_cam.3do size=2.800446 movesize=2.800446 collwidth=2.800446 collheight=2.800446
4m_rail           _ghostdecor       model3d=shw_rail_4m.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
3m_rail           _ghostdecor       model3d=shw_rail_3m.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
_pstructwood      _structure        thingflags=0x40002040
winebarrel        _pstructwood      model3d=shs_barrel_wine.3do size=0.125000 movesize=0.125000 collwidth=0.125000 collheight=0.125000
brokhamra         _sstructwood      model3d=shw_hamr_broke_a.3do size=0.195256 movesize=0.195256 collwidth=0.195256 collheight=0.195256
brokhamrb         _sstructwood      model3d=shw_hamr_broke_b.3do size=0.178536 movesize=0.178536 collwidth=0.178536 collheight=0.178536
brokhamrc         _sstructwood      model3d=shw_hamr_broke_c.3do size=0.407738 movesize=0.407738 collwidth=0.407738 collheight=0.407738
cndlc             _item             thingflags=0x400 model3d=riv_cndl_cwind.3do collwidth=0.020000 collheight=0.020000 cog=item_candle.cog height=0.007000
2m_brace          _ghostdecor       model3d=sol_brace_2m.3do size=0.007500 movesize=0.007500 collwidth=0.007500 collheight=0.007500
drivbas           _sstructmetal     model3d=shw_drive_base.3do size=0.133510 movesize=0.133510 collwidth=0.133510 collheight=0.133510
snow_simonov3     _ai_simonov       model3d=com_2h_snow.3do soundclass=russki3.snd
snow_mauser4      _ai_mauser        model3d=com_1h_snow.3do soundclass=russki4.snd
rivrcksnoc        _staticstruct     model3d=riv_rock_snow_c.3do size=0.084599 movesize=0.084599 collwidth=0.084599 collheight=0.084599
rivrcksnod        _staticstruct     model3d=riv_rock_snow_d.3do size=0.262684 movesize=0.262684 collwidth=0.262684 collheight=0.262684
rivrcksnob        _staticstruct     model3d=riv_rock_snow_b.3do size=0.108744 movesize=0.108744 collwidth=0.108744 collheight=0.108744
rivrcksnoa        _staticstruct     model3d=riv_rock_snow_a.3do size=0.171275 movesize=0.171275 collwidth=0.171275 collheight=0.171275
_staticstruct2m   _structure        thingflags=0x40000C0
bordblckb         _staticstruct2m   model3d=riv_border_blok_b.3do size=0.173205 movesize=0.173205 collwidth=0.173205 collheight=0.173205
bordblcka         _staticstruct2m   model3d=riv_border_blok_a.3do size=0.173205 movesize=0.173205 collwidth=0.173205 collheight=0.173205
temple            _sstructwood      model3d=riv_templ_candl.3do size=0.550000 movesize=0.550000 collwidth=0.550000 collheight=0.550000
brdgegears        _sstructmetal     model3d=riv_gears.3do size=0.535258 movesize=0.535258 collwidth=0.535258 collheight=0.535258 puppet=default.pup
rivbrdgstll       _sstructmetal     model3d=riv_bridge_still.3do size=0.469845 movesize=0.469845 collwidth=0.469845 collheight=0.469845
windtwr           _sstructmetal     model3d=riv_tower.3do size=0.722253 movesize=0.722253 collwidth=0.722253 collheight=0.722253
cndlabre          _sstructwood      model3d=riv_stnd_cndl.3do size=0.099528 movesize=0.099528 collwidth=0.099528 collheight=0.099528
armstnd           _sstructmetal     model3d=riv_stnd_arm.3do size=0.050249 movesize=0.050249 collwidth=0.050249 collheight=0.050249
cndlflame         _decor            thingflags=0x400 model3d=gen_sfx_lilflame.3do size=0.007170 movesize=0.007170 collwidth=0.007170 collheight=0.007170 cog=class_candle.cog
brdglndng         _sstructmetal     model3d=riv_bridge_landing.3do size=0.219374 movesize=0.219374 collwidth=0.219374 collheight=0.219374
markredcalm       _staticstruct     model3d=riv_markred_calm.3do size=0.105000 movesize=0.105000 collwidth=0.105000 collheight=0.105000 puppet=default.pup
_sstructblock     _staticstruct     thingflags=0xD4
2m_railcol        _sstructblock     thingflags=0x80 model3d=shw_rail_2mcollide.3do size=0.116028 movesize=0.116028 collwidth=0.116028 collheight=0.116028
4m_railcol        _sstructblock     thingflags=0x80 model3d=shw_rail_4mcollide.3do size=0.207997 movesize=0.207997 collwidth=0.207997 collheight=0.207997
3m_railcol        _sstructblock     thingflags=0x80 model3d=shw_rail_3mcollide.3do size=0.161129 movesize=0.161129 collwidth=0.161129 collheight=0.161129
invisiplat2m      _structblock      model3d=gen_invisiplate2m.3do size=0.100000 movesize=0.100000 collwidth=0.100000 collheight=0.100000
clock_post        _sstructmetal     model3d=shs_clock_post.3do size=0.095000 movesize=0.095000 collwidth=0.095000 collheight=0.095000
lmpbrkt           _ghostdecor       model3d=riv_lamp_bracket.3do
hintnumber3       _hintbase         userval=3.000000
hintnumber5       _hintbase         userval=5.000000
hintnumber7       _hintbase         userval=7.000000
hintnumber9       _hintbase         userval=9.000000
hintnumber11      _hintbase         userval=11.000000
hintnumber13      _hintbase         userval=13.000000
hintnumber15      _hintbase         userval=15.000000
hintnumber17      _hintbase         userval=17.000000
hintnumber19      _hintbase         userval=19.000000
hintnumber21      _hintbase         userval=21.000000
hintnumber23      _hintbase         userval=23.000000
hintnumber25      _hintbase         userval=25.000000
hintnumber27      _hintbase         userval=27.000000
hintnumber29      _hintbase         userval=29.000000
hintnumber31      _hintbase         userval=31.000000
hintnumber33      _hintbase         userval=33.000000
hintnumber35      _hintbase         userval=35.000000
hintnumber37      _hintbase         userval=37.000000
hintnumber39      _hintbase         userval=39.000000
hintnumber41      _hintbase         userval=41.000000
1mhangledge       _structblock      model3d=gen_1m_hangledge.3do size=0.055902 movesize=0.055902 collwidth=0.055902 collheight=0.055902
invisiplat5m      _structblock      model3d=riv_invisiplate_5m.3do size=0.254951 movesize=0.254951 collwidth=0.254951 collheight=0.254951
invisibox2tall    _structblock      model3d=riv_invisibox_2tall.3do size=0.125225 movesize=0.125225 collwidth=0.125225 collheight=0.125225
spritlemist       h2ofx             airdrag=1.500000 mass=0.300000 range=0.090000 elementsize=0.005000 count=90
+raft_splash      _sprite           timer=0.500000 sprite=raft_splash.spr
+raft_wake        _sprite           timer=3.000000 sprite=raft_wake.spr
paddlesplash      _particle         timer=0.400000 mass=0.500000 physflags=0x1 typeflags=0x10 material=gen_4sfx_particle_waterfall.mat range=0.010000 rate=5.000000 maxthrust=5.000000 elementsize=0.002000 count=4
rockchute         _ghostdecor       model3d=riv_chute_grnd.3do size=0.056640 movesize=0.056640 collwidth=0.056640 collheight=0.056640
_structsnow       _structure        thingflags=0x10000
chute4rock        _structsnow       model3d=riv_chute_rock.3do size=0.167205 movesize=0.167205 collwidth=0.167205 collheight=0.167205
pullever          _walkstruct       model3d=gen_lever_pull.3do size=0.053965 movesize=0.053965 collwidth=0.053965 collheight=0.053965 puppet=default.pup
pushlever         _walkstruct       model3d=gen_lever_push.3do size=0.054097 movesize=0.054097 collwidth=0.054097 collheight=0.054097 puppet=default.pup
snow              _particle         timer=10.000000 vel=(0.000000/0.050000/-0.100000) material=gen_4particle_snow.mat range=0.750000 minsize=0.100000 elementsize=0.005000 count=30
skitosnow         snow              thingflags=0x1 light=(1.000000/1.000000/1.000000/0.001000) timer=5.500000 range=2.000000 count=60
timsnow           snow              thingflags=0x1 light=(1.000000/1.000000/1.000000/0.001000) timer=5.500000 range=0.500000 count=20
stained_class     _particle         mass=0.500000 physflags=0x201 material=riv_4glass_stained_broken.mat range=0.020000 rate=18.000000 maxthrust=50.000000 elementsize=0.028000 count=40
limpraftmove      _decor            model3d=riv_raft_inflt1.3do size=0.004810 movesize=0.004810 collwidth=0.004810 collheight=0.004810
patchkitmove      _decor            model3d=riv_kit_ptch.3do size=0.007500 movesize=0.007500 collwidth=0.007500 collheight=0.007500
patchkit          _itemex           model3d=riv_kit_ptch.3do size=0.007500 movesize=0.007500 collwidth=0.007500 collheight=0.007500 cog=item_patchkit.cog
cndlamove         _decor            model3d=riv_cndl_awind.3do size=0.021265 movesize=0.021265 collwidth=0.021265 collheight=0.021265 soundclass=riv_candle.snd
cndlbmove         _decor            model3d=riv_cndl_bwind.3do size=0.021265 movesize=0.021265 collwidth=0.021265 collheight=0.021265 soundclass=riv_candle.snd
cndlcmove         _decor            model3d=riv_cndl_cwind.3do size=0.021265 movesize=0.021265 collwidth=0.021265 collheight=0.021265 soundclass=riv_candle.snd
rivbridge         _sstructmetal     model3d=riv_bridge_anim.3do size=0.202000 movesize=0.202000 collwidth=0.202000 collheight=0.202000 puppet=default.pup
skitodevil        _particle         timer=7.000000 vel=(0.000000/0.000000/-0.150000) angvel=(0.000000/180.000000/0.000000) material=gen_4particle_snow.mat range=0.300000 count=35
lilskitodevil     _particle         timer=6.000000 vel=(0.000000/0.000000/-0.200000) angvel=(0.000000/180.000000/0.000000) material=gen_4particle_snow.mat range=0.100000 count=35
markredwind       _staticstruct     model3d=riv_markred_wind.3do size=0.105000 movesize=0.105000 collwidth=0.105000 collheight=0.105000 puppet=default.pup
dustcloud         _sprite           timer=2.000000 sprite=gen_dustcloud.spr
clockdome         _staticstruct     model3d=shs_dome_clock.3do size=0.923309 movesize=0.923309 collwidth=0.923309 collheight=0.923309
belldome          _staticstruct     model3d=shs_dome_bell.3do size=0.662495 movesize=0.662495 collwidth=0.662495 collheight=0.662495
towergate         _sstructmetal     model3d=shs_gate_tower.3do size=0.230705 movesize=0.230705 collwidth=0.230705 collheight=0.230705 soundclass=shw_grill.snd
mondoor           _sstructwood      model3d=shs_doors_mnstry.3do size=0.201747 movesize=0.201747 collwidth=0.201747 collheight=0.201747
yindoor           _sstructwood      thingflags=0x20C1 light=(0.300000/0.300000/0.300000/0.001000) model3d=shs_door_crtyrd_a.3do size=0.180904 movesize=0.180904 collwidth=0.180904 collheight=0.180904
yangdoor          _sstructwood      thingflags=0x20C1 light=(0.300000/0.300000/0.300000/0.001000) model3d=shs_door_crtyrd_b.3do size=0.168453 movesize=0.168453 collwidth=0.168453 collheight=0.168453
monkbed           _sstructwood      model3d=shs_bed_monk.3do size=0.169115 movesize=0.169115 collwidth=0.169115 collheight=0.169115
monkbench         _sstructwood      model3d=shs_bench_monk.3do size=0.105623 movesize=0.105623 collwidth=0.105623 collheight=0.105623
bookcabinet       _sstructwood      model3d=shs_cabinet_book.3do size=0.146714 movesize=0.146714 collwidth=0.146714 collheight=0.146714
6mladder          _sstructwood      thingflags=0x20C1 light=(0.100000/0.100000/0.100000/0.001000) model3d=shs_ladder_6m.3do size=0.600000 movesize=0.050000 collwidth=0.010000 collheight=0.010000
shsbut            _decor            thingflags=0x1 light=(0.400000/0.400000/0.400000/0.001000) model3d=shs_button_halfm.3do size=0.024500 movesize=0.024500 collwidth=0.024500 collheight=0.024500 soundclass=shs_woodbutn.snd
2x2door           _sstructwood      thingflags=0x20C1 light=(0.300000/0.300000/0.300000/0.001000) model3d=shs_door_2x2a.3do size=0.140542 movesize=0.140542 collwidth=0.140542 collheight=0.140542
2x3doorshs        _sstructwood      model3d=shs_door_2x3a.3do size=0.148500 movesize=0.148500 collwidth=0.148500 collheight=0.148500 soundclass=shs_wooddor.snd
4mdbeam           _sstructwood      model3d=shs_beam_4md.3do size=0.203101 movesize=0.203101 collwidth=0.203101 collheight=0.203101
hangthing         _sstructwood      model3d=shs_crnr_lamp.3do size=0.108972 movesize=0.108972 collwidth=0.108972 collheight=0.108972
chainlamp         _decormetal       model3d=shs_lamp_chain.3do size=0.070000 movesize=0.070000 collwidth=0.070000 collheight=0.070000
4mchain           _sstructmetal     model3d=shs_chain_4mhng.3do size=0.200000 movesize=0.200000 collwidth=0.200000 collheight=0.200000
dark_glow         _sprite           sprite=gen_darkglow.spr
hangstrut         _sstructwood      model3d=shs_supprt_lamp.3do size=0.059222 movesize=0.059222 collwidth=0.059222 collheight=0.059222
4mladder          _sstructwood      thingflags=0x20C1 light=(0.100000/0.100000/0.100000) model3d=shs_ladder_4m.3do size=0.400000 movesize=0.050000 collwidth=0.010000 collheight=0.010000
steplad           _sstructwood      model3d=shs_step_ladder.3do size=0.200000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
liteshut          _sstructmetal     model3d=shs_shtr_light.3do size=0.200000 movesize=0.200000 collwidth=0.200000 collheight=0.200000 puppet=default.pup
shslampstrut      _swingstruct      model3d=shs_strut_lamp.3do collwidth=0.100000 collheight=0.100000
+ice_chckn_fire   _explosion        timer=0.500000 typeflags=0x1 spritething=+ice_chicken_spr spritestart=(0.050000/0.050000/1.000000) spriteend=(0.075000/0.075000/0.000000)
+icedart_sm       +bughit           model3d=gen_si_proj.3do size=0.010000 movesize=0.010000 collwidth=0.003693 collheight=0.003693 creatething=+ice_chckn_fire damage=20.000000
+ice_exp_cloud    _explosion        timer=1.100000 typeflags=0x11 spritething=+ice_spr_exp_cloud spritestart=(0.150000/0.200000/0.300000) spriteend=(0.350000/0.400000/0.000000)
+ice_exp_shards   _explosion        timer=0.800000 creatething=+ice_exp_cloud typeflags=0x39 babytime=0.009000 spritething=+ice_spr_exp_shards spritestart=(0.300000/0.300000/1.000000) spriteend=(0.300000/0.300000/1.000000)
iceshrapa_nc      _debrisnc         model3d=gen_dbra_ice.3do size=0.004006 movesize=0.004006 collwidth=0.004006 collheight=0.004006
iceshrapb_nc      _debrisnc         model3d=gen_dbrb_ice.3do size=0.003481 movesize=0.003481 collwidth=0.003481 collheight=0.003481
+ice_exp_blast    _explosion        timer=0.300000 soundclass=gen_iceexplode.snd creatething=+ice_exp_shards typeflags=0x1F damage=1000.000000 damageclass=0x80000000 blasttime=0.250000 babytime=0.009000 range=0.250000 debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapb_nc spritething=+ice_spr_exp_blast spritestart=(0.075000/0.075000/1.000000) spriteend=(1.000000/1.000000/1.000000)
ice_monster       _actor            thingflags=0x401 model3d=gen_si.3do size=0.051000 movesize=0.051000 collwidth=0.051396 collheight=0.051396 puppet=icemonster.pup soundclass=icemonster.snd cog=actor_icemonster.cog surfdrag=2.500000 staticdrag=0.010000 mass=100.000000 weapon=+icedart_sm health=150.000000 maxhealth=150.000000 maxthrust=0.700000 maxrotthrust=375.000000 typeflags=0x20120 eyeoffset=(0.000000/0.010000/0.020000) fireoffset=(0.000000/0.025000/0.018000) explode=+ice_exp_blast aiclass=icemonster.ai
budbox            _sstructwood      model3d=shs_budbox.3do size=0.089409 movesize=0.089409 collwidth=0.089409 collheight=0.089409
jmpbma            _sstructwood      thingflags=0x20C1 light=(0.100000/0.100000/0.100000/0.001000) model3d=shs_beam_jmpa.3do size=0.460000 movesize=0.100000 collwidth=0.435893 collheight=0.435893
jmpbmc            _sstructwood      thingflags=0x20C1 light=(0.100000/0.100000/0.100000/0.001000) model3d=shs_beam_jmpc.3do size=0.340000 movesize=0.100000 collwidth=0.331666 collheight=0.331666
jmpbmb            _sstructwood      thingflags=0x20C1 light=(0.100000/0.100000/0.100000/0.001000) model3d=shs_beam_jmpb.3do size=0.400000 movesize=0.100000 collwidth=0.331666 collheight=0.331666
shspshblk         _pushstruct2m     model3d=shs_blok_push.3do size=0.173205 movesize=0.173205 collwidth=0.173205 collheight=0.173205
1mchain           _sstructmetal     model3d=shs_chain_1mhng.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
jmpbmd            _sstructwood      thingflags=0x20C1 light=(0.100000/0.100000/0.100000/0.001000) model3d=shs_beam_jmpd.3do size=0.300000 movesize=0.100000 collwidth=0.287228 collheight=0.287228
trapbmb           _sstructwood      thingflags=0x20C1 light=(0.100000/0.100000/0.100000/0.001000) model3d=shs_beam_trap.3do size=0.400000 movesize=0.100000 collwidth=0.331666 collheight=0.331666
sealdoorr         _sstructwood      model3d=shs_sealdoor_r.3do size=0.148861 movesize=0.148861 collwidth=0.148861 collheight=0.148861
sealdoorl         _sstructwood      model3d=shs_sealdoor_l.3do size=0.179191 movesize=0.179191 collwidth=0.179191 collheight=0.179191
elvtr             _sstructmetal     model3d=shs_elvtr_broke.3do size=0.222922 movesize=0.222922 collwidth=0.222922 collheight=0.222922
4msnoladr         _sstructwood      thingflags=0x20C1 light=(0.100000/0.100000/0.100000/0.001000) model3d=shs_ladr_4msnow.3do size=0.400000 movesize=0.050000 collwidth=0.010000 collheight=0.010000
4mbeam            _sstructwood      model3d=shs_beam_4m.3do size=0.203101 movesize=0.203101 collwidth=0.203101 collheight=0.203101
prssrpad          _sstructmetal     model3d=shs_pad_prsr.3do size=0.182003 movesize=0.182003 collwidth=0.182003 collheight=0.182003
flowerb           _sstructmetal     model3d=shs_flowerpot_youthb.3do size=0.142356 movesize=0.142356 collwidth=0.142356 collheight=0.142356
bulb              _item             thingflags=0x400 model3d=shs_bulb_pot.3do collwidth=0.020000 collheight=0.020000 cog=item_bulb.cog height=0.006200
monkbodya         _staticstruct     model3d=shs_msa.3do size=0.105312 movesize=0.105312 collwidth=0.105312 collheight=0.105312
2mslotladder      _sstructwood      model3d=shs_ladr_2mslot.3do size=0.200000 movesize=0.050000 collwidth=0.010000 collheight=0.010000
2mrope            _staticstruct     model3d=shs_rope_2m.3do size=0.100654 movesize=0.100654 collwidth=0.100654 collheight=0.100654
4mplank           _sstructwood      model3d=shs_plank_4m.3do size=0.206534 movesize=0.206534 collwidth=0.206534 collheight=0.206534
young_lady        _cutactor         thingflags=0x20084400 model3d=shs_yl.3do collwidth=0.020000 collheight=0.020000 puppet=younglady.pup soundclass=young_lady.snd cog=actor_younglady.cog eyeoffset=(0.000000/0.012800/0.066200) voicecolor=(0.980000/0.490000/0.490000/1.000000/0.980000/0.490000/0.490000/1.000000/1.000000/0.941000/0.980000/1.000000/1.000000/0.941000/0.980000/1.000000)
bell_doors        _sstructwood      model3d=shs_door_bell.3do size=0.278448 movesize=0.278448 collwidth=0.278448 collheight=0.278448
bardool           _sstructwood      model3d=shs_door_barl.3do size=0.600000 movesize=0.600000 collwidth=0.299000 collheight=0.299000 puppet=default.pup
bardoorr          _sstructwood      model3d=shs_door_barr.3do size=0.600000 movesize=0.600000 collwidth=0.299000 collheight=0.299000 puppet=default.pup
barlock           _sstructwood      model3d=shs_bar_lock.3do size=0.610000 movesize=0.610000 collwidth=0.040000 collheight=0.040000 puppet=default.pup
bell              _structmetal      thingflags=0x400001 light=(1.000000/1.000000/1.000000/0.001000) model3d=shs_bell_monastery.3do size=0.360555 movesize=0.360555 collwidth=0.360555 collheight=0.360555
4mrope            _staticstruct     model3d=shs_rope_4m.3do size=0.200328 movesize=0.200328 collwidth=0.200328 collheight=0.200328
bloomflower       _ghostdecor       model3d=shs_flower_bloom.3do size=0.039333 movesize=0.039333 collwidth=0.039333 collheight=0.039333 puppet=default.pup
flower            _ghostdecor       model3d=shs_flower_put.3do size=0.024631 movesize=0.024631 collwidth=0.024631 collheight=0.024631
bardoolcol        _structwood       model3d=shs_door_barl_col.3do size=0.328862 movesize=0.328862 collwidth=0.328862 collheight=0.328862
bardoorrcol       _structwood       model3d=shs_door_barr_col.3do size=0.328862 movesize=0.328862 collwidth=0.328862 collheight=0.328862
flyingcane        _ghostdecor       model3d=0yl_cane_throwcane.3do size=0.012313 movesize=0.012313 collwidth=0.012313 collheight=0.012313 puppet=default.pup
bellringer_actor  _cutactor         thingflags=0x20484000 model3d=shs_ri.3do collwidth=0.020000 collheight=0.020000 puppet=default.pup mass=0.000000 typeflags=0x110109
old_lady          _cutactor         thingflags=0x20084400 model3d=shs_ol.3do size=0.032500 movesize=0.032500 collwidth=0.020000 collheight=0.020000 puppet=oldlady.pup soundclass=old_lady.snd cog=actor_oldlady.cog typeflags=0x8118009 eyeoffset=(0.000000/0.041800/0.044100) voicecolor=(0.980000/0.490000/0.490000/1.000000/0.980000/0.490000/0.490000/1.000000/1.000000/0.941000/0.980000/1.000000/1.000000/0.941000/0.980000/1.000000)
4mbars            _sstructmetal     model3d=shw_bars_4m.3do size=0.300000 movesize=0.300000 collwidth=0.300000 collheight=0.300000
brokladder        _ghostdecor       model3d=shs_ladr_broke.3do size=0.223277 movesize=0.223277 collwidth=0.223277 collheight=0.223277
flowerchn         _ghostdecor       model3d=shs_flowerpot_chain.3do size=0.500000 movesize=0.500000 collwidth=0.500000 collheight=0.500000
flowert           _swingstruct      model3d=shs_flowerpot_youtht.3do collwidth=0.100000 collheight=0.100000
minutehand        _sstructmetal     model3d=shs_hand_minute.3do size=0.240664 movesize=0.240664 collwidth=0.240664 collheight=0.240664
hourhand          _sstructmetal     model3d=shs_hand_hour.3do size=0.200250 movesize=0.200250 collwidth=0.200250 collheight=0.200250
clock_tab         _staticstruct     model3d=shs_tab_clock.3do size=0.100282 movesize=0.100282 collwidth=0.100282 collheight=0.100282
sunlt             _staticstruct     model3d=shs_shaft_sun.3do
padslot           _sstructmetal     model3d=shs_pad_slot.3do size=0.103078 movesize=0.103078 collwidth=0.103078 collheight=0.103078
4mslotladder      _sstructwood      model3d=shs_ladr_4mslot.3do size=0.400000 movesize=0.050000 collwidth=0.212132 collheight=0.212132
ice_monster_wall  ice_monster       model3d=gen_si_icicle.3do size=0.020000 movesize=0.020000 puppet=default.pup cog=actor_stickyicemonster.cog physflags=0x91 typeflags=0x120100 aiclass=stickyicemonster.ai
hanglamp          _decormetal       model3d=shs_lamp_hang.3do size=0.049497 movesize=0.049497 collwidth=0.049497 collheight=0.049497
geartick          _sstructwood      model3d=shs_gear_tick.3do size=0.300000 movesize=0.500000 collwidth=0.170000 collheight=0.170000 puppet=default.pup
clock_armb        _sstructmetal     model3d=shs_clock_armb.3do size=0.399656 movesize=0.399656 collwidth=0.399656 collheight=0.399656 puppet=default.pup
clock_baseb       _staticstruct     model3d=shs_clock_baseb.3do size=0.750000 movesize=0.750000 collwidth=0.127500 collheight=0.127500
clock_gearsb      _structmetal      model3d=shs_clock_gearsb.3do size=0.153683 movesize=0.153683 collwidth=0.153683 collheight=0.153683 puppet=default.pup
clock_padb        _sstructmetal     model3d=shs_clock_padb.3do size=0.143614 movesize=0.143614 collwidth=0.143614 collheight=0.143614 puppet=default.pup
clock_pada        _sstructmetal     model3d=shs_clock_pada.3do size=0.143614 movesize=0.143614 collwidth=0.143614 collheight=0.143614 puppet=default.pup
clock_wheelb      _structure        model3d=shs_clock_wheelb.3do size=0.160020 movesize=0.160020 collwidth=0.160020 collheight=0.160020 puppet=default.pup
clock_levera      _sstructmetal     model3d=shs_clock_levera.3do size=0.498554 movesize=0.498554 collwidth=0.498554 collheight=0.498554 puppet=default.pup
clock_leverb      _sstructmetal     model3d=shs_clock_leverb.3do size=0.515655 movesize=0.515655 collwidth=0.515655 collheight=0.515655 puppet=default.pup
clock_blockb      _pushstruct2m     model3d=shs_clock_blockb.3do size=0.170903 movesize=0.170903 collwidth=0.170903 collheight=0.170903 puppet=default.pup
clock_axel        _actor            thingflags=0x0 model3d=shs_clock_axel.3do size=0.825242 movesize=0.825242 collwidth=0.825242 collheight=0.825242 puppet=default.pup mass=0.000000 physflags=0x2A00 typeflags=0x100D08
clock_wheel       _sstructmetal     model3d=shs_clock_wheelt.3do size=0.269258 movesize=0.269258 collwidth=0.269258 collheight=0.269258 puppet=default.pup
clock_baset       _staticstruct     model3d=shs_clock_baset.3do size=0.750000 movesize=0.750000 collwidth=0.127500 collheight=0.127500
_pushstruct       _structure        thingflags=0x40000040
clock_blockt      _pushstruct       model3d=shs_clock_blockt.3do size=0.170367 movesize=0.170367 collwidth=0.170367 collheight=0.170367 puppet=default.pup
clock_houra       _sstructmetal     model3d=shs_clock_houra.3do size=0.244387 movesize=0.244387 collwidth=0.244387 collheight=0.244387 puppet=default.pup
clock_hourb       _sstructmetal     model3d=shs_clock_hourb.3do size=0.211483 movesize=0.211483 collwidth=0.211483 collheight=0.211483 puppet=default.pup
clock_mnut        _sstructmetal     model3d=shs_clock_mnut.3do size=0.090139 movesize=0.090139 collwidth=0.090139 collheight=0.090139 puppet=default.pup
clock_star        _sstructmetal     model3d=shs_clock_star.3do size=0.184124 movesize=0.184124 collwidth=0.184124 collheight=0.184124 puppet=default.pup
clock_armt        _sstructmetal     model3d=shs_clock_armt.3do size=0.399656 movesize=0.399656 collwidth=0.399656 collheight=0.399656 puppet=default.pup
invsbox1x1x2      _decorblock       model3d=gen_box_1x1x2minvis.3do size=0.122474 movesize=0.122474 collwidth=0.122474 collheight=0.122474
invisistar        _sstructmetal     thingflags=0x4000D0 model3d=shs_invisistar.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000
snow_simonov      _ai_simonov       model3d=com_2h_snow.3do soundclass=russki1.snd
snow_tokarev5     _ai_tokarev       model3d=com_1h_snow.3do soundclass=russki5.snd
sealstnd          _sstructwood      model3d=shs_stand_seal.3do size=0.058890 movesize=0.058890 collwidth=0.058890 collheight=0.058890
sealkeymove       _decor            model3d=shs_key_sealdoor.3do size=0.018192 movesize=0.018192 collwidth=0.018192 collheight=0.018192
8mrope            _staticstruct     model3d=shs_rope_8m.3do size=0.400164 movesize=0.400164 collwidth=0.400164 collheight=0.400164
ped               _staticstruct     model3d=shs_ped.3do size=0.072887 movesize=0.072887 collwidth=0.072887 collheight=0.072887
dngrdoor          _sstructwood      model3d=shs_door_danger.3do size=0.140564 movesize=0.140564 collwidth=0.140564 collheight=0.140564
lady              _decor            collide=3 model3d=shw_ladystatue.3do size=0.225180 movesize=0.225180 collwidth=0.225180 collheight=0.225180 soundclass=shs_statue.snd
hintnumber60      _hintbase         userval=60.000000
goldidolmove      _decor            thingflags=0x1 light=(0.150000/0.150000/0.150000/0.001000) model3d=gen_idol_gold.3do size=0.014000 movesize=0.014000 collwidth=0.014000 collheight=0.014000
swinglog          _sstructwood      model3d=shs_log_swing.3do size=0.430000 movesize=0.430000 collwidth=0.430000 collheight=0.430000
hanglit           _decormetal       model3d=shs_lamp_hang_lit.3do size=0.045000 movesize=0.045000 collwidth=0.045000 collheight=0.045000
bronzkey          _item             thingflags=0x400 model3d=gen_bronz_key.3do collwidth=0.020000 collheight=0.020000 cog=item_bronzekey.cog height=0.007905
6mslotladder      _sstructwood      model3d=shs_ladr_6mslot.3do size=0.600000 movesize=0.050000 collwidth=0.308221 collheight=0.308221
2mchain           _sstructmetal     model3d=shs_chain_2mhng.3do size=0.100000 movesize=0.100000 collwidth=0.100000 collheight=0.100000
monktable         _sstructwood      model3d=shs_table_monk.3do size=0.122474 movesize=0.122474 collwidth=0.122474 collheight=0.122474
bookstand         _sstructwood      model3d=shs_pulpit_read.3do size=0.080000 movesize=0.080000 collwidth=0.080000 collheight=0.080000
bookb             _staticstruct     model3d=shs_book_snglb.3do size=0.025000 movesize=0.025000 collwidth=0.025000 collheight=0.025000
oldladykeymove    _decor            model3d=shs_key_lady.3do size=0.005000 movesize=0.005000 collwidth=0.005000 collheight=0.005000
2mboard           _sstructwood      model3d=shs_board_2m.3do size=0.103833 movesize=0.103833 collwidth=0.103833 collheight=0.103833
brickshrpa        _ghostdecor       model3d=gen_dbre_brickwala.3do size=0.012500 movesize=0.012500 collwidth=0.012500 collheight=0.012500
brickshrpb        _ghostdecor       model3d=gen_dbre_brickwalb.3do size=0.014193 movesize=0.014193 collwidth=0.014193 collheight=0.014193
invscubehlfm      _decorblock       model3d=gen_box_hlfminvis.3do size=0.025000 movesize=0.025000 collwidth=0.025000 collheight=0.025000
brickshrpc        _ghostdecor       model3d=gen_dbre_brickwalc.3do size=0.012601 movesize=0.012601 collwidth=0.012601 collheight=0.012601
snow_tokarev2     _ai_tokarev       model3d=com_1h_snow.3do soundclass=russki2.snd
snow_tokarev4     _ai_tokarev       model3d=com_1h_snow.3do soundclass=russki4.snd
snow_tokarev3     _ai_tokarev       model3d=com_1h_snow.3do soundclass=russki3.snd
snow_simonov2     _ai_simonov       model3d=com_2h_snow.3do soundclass=russki2.snd
snow_simonov4     _ai_simonov       model3d=com_2h_snow.3do soundclass=russki4.snd
cutscene_ice_monster _actor            model3d=gen_si_icicle.3do size=0.020000 movesize=0.020000 collwidth=0.029317 collheight=0.029317 puppet=default.pup surfdrag=2.500000 staticdrag=0.010000 mass=100.000000 physflags=0x91 weapon=+icedart_sm health=150.000000 maxhealth=150.000000 maxthrust=0.600000 maxrotthrust=375.000000 typeflags=0x20100 fireoffset=(0.000000/0.025000/0.018000) aiclass=actor.ai
shsledge          _staticstruct     model3d=shs_ledge_walk.3do size=0.352172 movesize=0.352172 collwidth=0.352172 collheight=0.352172
shs_lampglow      _sprite           thingflags=0x1 light=(0.855000/0.600000/0.400000/0.600000) sprite=shs_lampglow.spr
_sstruct2mwood    _structure        thingflags=0x40020C0
jmpbmbclmb        _sstruct2mwood    thingflags=0x40020C1 light=(0.100000/0.100000/0.100000/0.001000) model3d=shs_beam_jmpb_2m.3do size=0.200000 movesize=0.100000 collwidth=0.173206 collheight=0.173206
brickshrpbm       _ghostdecor       model3d=gen_dbre_brickwalbm.3do size=0.006250 movesize=0.006250 collwidth=0.006250 collheight=0.006250
brickshrpbs       _ghostdecor       model3d=gen_dbre_brickwalbs.3do size=0.003125 movesize=0.003125 collwidth=0.003125 collheight=0.003125
brickshrpcm       _ghostdecor       model3d=gen_dbre_brickwalcm.3do size=0.006250 movesize=0.006250 collwidth=0.006250 collheight=0.006250
invsbox2x2        _structblock      model3d=gen_box_2x2minvis.3do size=0.173205 movesize=0.173205 collwidth=0.173205 collheight=0.173205
writable          _structwood       model3d=shs_table_write.3do size=0.108546 movesize=0.108546 collwidth=0.108546 collheight=0.108546
monkchair         _decorwood        model3d=shs_chair_monk.3do size=0.060000 movesize=0.060000 collwidth=0.060000 collheight=0.060000
scroll_open       _staticstruct     model3d=shs_scroll_open.3do size=0.005982 movesize=0.005982 collwidth=0.005982 collheight=0.005982
scale             _decormetal       model3d=shs_scale_monk.3do size=0.040000 movesize=0.040000 collwidth=0.040000 collheight=0.040000
scroll_closed     _staticstruct     model3d=shs_scroll_close.3do size=0.006366 movesize=0.006366 collwidth=0.006366 collheight=0.006366
bookc             _staticstruct     model3d=shs_book_snglc.3do size=0.025000 movesize=0.025000 collwidth=0.025000 collheight=0.025000
bellover          _structblock      model3d=shs_bell_invisiguard.3do size=0.322102 movesize=0.322102 collwidth=0.322102 collheight=0.322102
+iceclaw_lg       +bughit           damage=175.000000
ice_boss          _actor            thingflags=0x8400400 model3d=shw_ib.3do size=0.350000 movesize=0.350000 collwidth=0.200000 collheight=0.200000 puppet=iceboss.pup soundclass=iceboss.snd cog=actor_iceboss.cog surfdrag=3.000000 airdrag=0.300000 staticdrag=0.005000 mass=500.000000 physflags=0x2DF orientspeed=0.200000 weapon=+iceclaw_lg health=500.000000 maxhealth=500.000000 maxthrust=1.400000 maxrotthrust=175.000000 typeflags=0x1020100 eyeoffset=(0.000000/0.160000/0.010000) aiclass=iceboss.ai
flska             _decor            model3d=shw_flsk_a.3do size=0.012000 movesize=0.012000 collwidth=0.012000 collheight=0.012000
flskb             _decor            model3d=shw_flsk_b.3do size=0.022500 movesize=0.022500 collwidth=0.022500 collheight=0.022500
flskc             _decor            model3d=shw_flsk_c.3do size=0.034000 movesize=0.034000 collwidth=0.034000 collheight=0.034000
flskd             _decor            model3d=shw_flsk_d.3do size=0.016000 movesize=0.016000 collwidth=0.016000 collheight=0.016000
flske             _decor            model3d=shw_flsk_e.3do size=0.019000 movesize=0.019000 collwidth=0.019000 collheight=0.019000
booksa            _staticstruct     model3d=shs_book_bloka.3do size=0.022000 movesize=0.022000 collwidth=0.022000 collheight=0.022000
booksb            _staticstruct     model3d=shs_book_blokb.3do size=0.020000 movesize=0.020000 collwidth=0.020000 collheight=0.020000
booka             _staticstruct     model3d=shs_book_sngla.3do size=0.025000 movesize=0.025000 collwidth=0.025000 collheight=0.025000
monkstool         _sstructwood      model3d=shs_stool_monk.3do size=0.052500 movesize=0.052500 collwidth=0.052500 collheight=0.052500
sitmonk           _decor            model3d=shw_monksit.3do size=0.059678 movesize=0.059678 collwidth=0.059678 collheight=0.059678
blokice           _pushstruct2m     model3d=shw_blck_ice.3do size=0.171473 movesize=0.171473 collwidth=0.171473 collheight=0.171473
shwplta           _sstructwood      model3d=shw_pltfrm_a.3do size=0.143614 movesize=0.143614 collwidth=0.143614 collheight=0.143614
shwpltb           _sstructwood      model3d=shw_pltfrm_b.3do size=0.139754 movesize=0.139754 collwidth=0.139754 collheight=0.139754
shwpltd           _sstructwood      model3d=shw_pltfrm_d.3do size=0.122474 movesize=0.122474 collwidth=0.122474 collheight=0.122474
crnrdrft          _sstructsnow      model3d=shs_sndrift_corner.3do size=0.210948 movesize=0.210948 collwidth=0.210948 collheight=0.210948
grgylstrut        _swingstruct      model3d=shw_strut_grgyl.3do collwidth=0.100000 collheight=0.100000
blcny_rena        _sstructwood      model3d=shw_blcny_arena.3do size=0.261020 movesize=0.261020 collwidth=0.261020 collheight=0.261020
icedoor           _sstructmetal     thingflags=0x4000C1 light=(0.200000/0.200000/0.500000/0.001000) model3d=shw_ice_door.3do size=0.290560 movesize=0.290560 collwidth=0.290560 collheight=0.290560
snocrtbrl         _pstruct2mwood    model3d=shs_snocrate.3do size=0.173205 movesize=0.173205 collwidth=0.173205 collheight=0.173205
shwpltc           _sstructwood      model3d=shw_pltfrm_c.3do size=0.160078 movesize=0.160078 collwidth=0.160078 collheight=0.160078
shwimpstnd        _staticstruct     model3d=shw_stand_imp.3do size=0.062500 movesize=0.062500 collwidth=0.062500 collheight=0.062500
imp1move          _walkstruct       model3d=imp1_vibrat_inv.3do size=0.025605 movesize=0.025605 collwidth=0.025605 collheight=0.025605
shw2x3_door       _sstructwood      model3d=shs_door_2x3a.3do size=0.148500 movesize=0.148500 collwidth=0.148500 collheight=0.148500 soundclass=shw_wooddor.snd
busha             _ghostdecor       model3d=gen_bush_standupa.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000
bushb             _ghostdecor       model3d=gen_bush_standupb.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000
bigrock           _staticstruct     model3d=tem_rock_a.3do size=0.171275 movesize=0.171275 collwidth=0.171275 collheight=0.171275
birchtree         _sstructwood      model3d=gen_tree_basic.3do size=0.500899 movesize=0.500899 collwidth=0.500899 collheight=0.500899
smlrock           _staticstruct     model3d=tem_rock_c.3do size=0.085045 movesize=0.085045 collwidth=0.085045 collheight=0.085045
iceshrapa         _debris           model3d=gen_dbra_ice.3do size=0.004006 movesize=0.004006 collwidth=0.004006 collheight=0.004006
snwbrkshrapas_nc  _debrisnc         model3d=gen_dbre_brickwalas.3do size=0.003867 movesize=0.003867 collwidth=0.003867 collheight=0.003867
snwbrkshrapbs_nc  _debrisnc         model3d=gen_dbre_brickwalbs.3do size=0.003125 movesize=0.003125 collwidth=0.003125 collheight=0.003125
+ice_chicken_spr  _sprite           timer=0.500000 sprite=icechickensmoke.spr
+ice_spr_exp_blast _sprite           timer=0.300000 sprite=aet_expfloater.spr
+ice_spr_exp_shards _sprite           timer=0.699000 sprite=ice_explode.spr
+ice_spr_exp_cloud _sprite           timer=1.000000 sprite=ice_cloud.spr
+ice_exp_blast_sm _explosion        timer=0.300000 soundclass=gen_iceexplode.snd typeflags=0x17 damage=1000.000000 damageclass=0x80000000 blasttime=0.250000 range=0.250000 debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapa_nc debris=iceshrapb_nc spritething=+ice_spr_exp_shards spritestart=(0.300000/0.300000/1.000000) spriteend=(0.300000/0.300000/1.000000)
ibossbreath       _explosion        timer=0.500000 sprite=shw_icebossbreath.spr typeflags=0x1
ibossdartfx       _explosion        timer=0.300000 soundclass=gen_iceexplode.snd typeflags=0x17 damage=10.000000 damageclass=0x80000000 blasttime=0.250000 range=0.250000 spritething=+ice_spr_exp_shards spritestart=(0.300000/0.300000/1.000000) spriteend=(0.300000/0.300000/1.000000)
+icedart_lg       +bughit           model3d=gen_ib_proj.3do size=0.020000 movesize=0.020000 collwidth=0.011078 collheight=0.011078 creatething=ibossbreath explode=ibossdartfx damage=40.000000
+nopainhit        +bughit           size=0.050000 movesize=0.050000 vel=(0.000000/10.000000/0.000000) damage=0.000000
ibossclawhit      _particle         airdrag=3.200000 mass=1.000000 physflags=0x201 vel=(0.000000/0.000000/0.800000) typeflags=0x40019 material=shs_4sprite_ice_boss_mana.mat range=0.060000 rate=30.000000 maxthrust=3.000000 elementsize=0.020000 count=40
ibosssparks       _particle         timer=2.750000 airdrag=0.300000 mass=1.000000 typeflags=0x19 material=gen_4sfx_particle_waterfall.mat range=0.025000 rate=200.000000 maxthrust=2.000000 elementsize=0.013000 count=6
ibossimpact       _particle         airdrag=3.200000 mass=1.000000 physflags=0x201 vel=(0.000000/0.000000/1.200000) typeflags=0x19 material=gen_4sfx_particle_waterfall.mat range=0.070000 rate=10.000000 maxthrust=3.000000 elementsize=0.040000 count=40
ibosscrash        _explosion        timer=0.500000 sprite=shw_icebosscloud.spr typeflags=0x1 spritestart=(0.075000/0.075000/0.600000) spriteend=(0.600000/0.600000/0.600000)
ibossdeath        _particle         airdrag=2.000000 mass=0.210000 physflags=0x201 vel=(0.000000/0.000000/0.600000) typeflags=0x19 material=shs_4sprite_ice_boss_mana.mat range=0.050000 rate=15.000000 maxthrust=12.000000 elementsize=0.080000 count=15
ibossexpl         _explosion        timer=0.500000 sprite=shw_icebossexp.spr typeflags=0x1 spritestart=(0.075000/0.075000/0.600000) spriteend=(0.600000/0.600000/0.600000)
ibossdebris       _particle         airdrag=1.500000 mass=1.000000 physflags=0x201 vel=(0.000000/0.000000/0.750000) typeflags=0x19 material=gen_4sfx_particle_waterfall.mat range=0.050000 rate=10.000000 maxthrust=6.000000 elementsize=0.050000 count=20
iceshrapc_nc      _debrisnc         model3d=gen_dbrc_ice.3do size=0.005423 movesize=0.005423 collwidth=0.005423 collheight=0.005423
ibossrocks        metalshrapc       vel=(0.000000/0.000000/0.000000)
splshboat         _ghostdecor       model3d=sea_splash_boat.3do
shs_bellglow      _sprite           orient=(90.000000/0.000000/0.000000) timer=2.000000 sprite=shs_bellglow.spr
shs_sunburst      _sprite           timer=2.000000 sprite=shs_sunburst.spr
shs_flowerdust    _particle         timer=0.500000 typeflags=0x19 material=aet_4sprite_glow_fairy_dust.mat range=0.025000 minsize=0.010000 maxthrust=0.500000 elementsize=0.001000 count=7
+icechickenexpla  _particle         timer=3.000000 airdrag=1.000000 mass=0.100000 physflags=0x201 vel=(0.500000/0.500000/0.500000) material=gen_4particle_snow.mat range=0.250000 minsize=0.250000 elementsize=0.020000 count=30
+icechickenexplb  +icechickenexpla  vel=(0.500000/-0.500000/0.500000)
+icechickenexplc  +icechickenexpla  vel=(-0.500000/0.500000/0.500000)
+icechickenexpld  +icechickenexpla  vel=(-0.500000/-0.500000/0.500000)
plant             _decor            model3d=shs_bulb_leafstem.3do size=0.020000 movesize=0.020000 collwidth=0.010910 collheight=0.010910
bud               _decor            model3d=shs_bulb_bud.3do size=0.020000 movesize=0.020000 collwidth=0.018560 collheight=0.018560 puppet=default.pup
imp_idlemagic     _particle         orient=(0.000000/0.000000/1.022018) timer=0.800000 typeflags=0x59 material=aet_4sprite_glow_fairy_dust.mat range=0.030000 minsize=0.000100 rate=20.000000 maxthrust=0.500000 elementsize=0.005000 count=4
dustcloud4ever    _sprite           sprite=gen_dustcloud.spr
+ripples_vert     _sprite           timer=2.099000 sprite=ripplevert.spr
lensflarew        _sprite           sprite=gen_lensflarewhite.spr
palmtree          _sstructwood      model3d=gen_tree_palm.3do size=0.307585 movesize=0.307585 collwidth=0.307585 collheight=0.307585
tikibut           _staticstruct     model3d=lag_statue_tikia.3do size=0.349844 movesize=0.299844 collwidth=0.299844 collheight=0.299844
tikistairs        _walkstruct       model3d=lag_statue_tikisteps.3do size=0.172474 movesize=0.122474 collwidth=0.122474 collheight=0.122474
zero              _sstructmetal     model3d=lag_plane_sunk.3do size=0.613951 movesize=0.613951 collwidth=0.613951 collheight=0.613951
treec             _ghostdecor       model3d=gen_tree_standupc.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000
treea             _ghostdecor       model3d=gen_tree_standupa.3do size=0.200000 movesize=0.200000 collwidth=0.200000 collheight=0.200000
treed             _ghostdecor       model3d=gen_tree_standupd.3do size=0.225000 movesize=0.225000 collwidth=0.225000 collheight=0.225000
gatetotema        _sstructwood      model3d=lag_totem_gatea.3do size=0.171864 movesize=0.171864 collwidth=0.171864 collheight=0.171864
gatetotemb        _sstructwood      model3d=lag_totem_gateb.3do size=0.171864 movesize=0.171864 collwidth=0.171864 collheight=0.171864
gatetotemc        _sstructwood      model3d=lag_totem_gatec.3do size=0.268370 movesize=0.268370 collwidth=0.268370 collheight=0.268370
gatetotemd        _sstructwood      model3d=lag_totem_gated.3do size=0.268370 movesize=0.268370 collwidth=0.268370 collheight=0.268370
tikicolumn        _staticstruct     model3d=tem_column_tiki.3do size=1.000000 movesize=1.000000 collwidth=1.000000 collheight=1.000000
_swimactor        _actor            thingflags=0x32000000 airdrag=0.500000 staticdrag=0.010000 physflags=0x4A03 maxthrust=0.400000 typeflags=0x40
+sharkbite        +bughit           damage=100.000000
shark             _swimactor        thingflags=0x32000400 model3d=gen_sh.3do size=0.150000 movesize=0.150000 collwidth=0.083212 collheight=0.083212 puppet=shark.pup soundclass=shark.snd cog=actor_shark.cog surfdrag=1.500000 airdrag=0.400000 mass=120.000000 maxvel=1.200000 weapon=+sharkbite health=400.000000 maxhealth=400.000000 maxthrust=1.000000 maxrotthrust=200.000000 typeflags=0x20040 eyeoffset=(0.000000/0.130000/0.000000) aiclass=shark.ai
broketikid        _staticstruct     model3d=tem_column_tikibrkd.3do size=0.410152 movesize=0.410152 collwidth=0.410152 collheight=0.410152
lagdemon          _ghostdecor       model3d=lag_demon_head.3do size=0.155000 movesize=0.155000 collwidth=0.155000 collheight=0.155000
totem             _staticstruct     model3d=vol_totem_horns.3do size=0.130231 movesize=0.130231 collwidth=0.130231 collheight=0.130231
cranemast         _sstructmetal     thingflags=0x80 model3d=lag_mast_crane.3do size=0.680000 movesize=0.680000 collwidth=0.680000 collheight=0.680000
whipcrane         _sstructmetal     model3d=lag_crane_whip.3do size=0.559365 movesize=0.559365 collwidth=0.559365 collheight=0.559365
smokestack        _sstructmetal     model3d=lag_stack_smoke.3do size=0.337294 movesize=0.337294 collwidth=0.337294 collheight=0.337294
1moil_ja_noboom   _structure        model3d=lag_drum_1moilja.3do size=0.061033 movesize=0.061033 collwidth=0.061033 collheight=0.061033
1moil_jb_noboom   _structure        model3d=lag_drum_1moiljb.3do size=0.061033 movesize=0.061033 collwidth=0.061033 collheight=0.061033
hlfmoil_ja_noboom _structure        model3d=lag_drum_hlfmoilja.3do size=0.032094 movesize=0.032094 collwidth=0.032094 collheight=0.032094
2mjapancrate      _pstruct2mwood    model3d=lag_crate_2mjapan.3do size=0.223205 movesize=0.173205 collwidth=0.173205 collheight=0.173205
2mlagplank        _sstructwood      model3d=lag_plank_2m.3do size=0.112500 movesize=0.112500 collwidth=0.112500 collheight=0.112500
hlfmoil_jb_noboom _structure        model3d=lag_drum_hlfmoiljb.3do size=0.032094 movesize=0.032094 collwidth=0.032094 collheight=0.032094
seagull           _flyactor         thingflags=0x401 model3d=gen_sg.3do size=0.037500 movesize=0.037500 collwidth=0.037500 collheight=0.037500 puppet=seagull.pup soundclass=seagull.snd cog=actor_seagull.cog staticdrag=0.010000 mass=2.000000 maxthrust=0.350000 maxrotthrust=150.000000 typeflags=0x10100000 aiclass=seagull.ai
7sandbags         _staticstruct     model3d=lag_sndbg_7stack.3do size=0.187596 movesize=0.187596 collwidth=0.187596 collheight=0.187596
2sandbags         _staticstruct     model3d=lag_sndbg_2stack.3do size=0.081023 movesize=0.081023 collwidth=0.081023 collheight=0.081023
seaweeda          _ghostdecor       model3d=gen_weed_seaa.3do size=0.100000 movesize=0.100000 collwidth=0.100000 collheight=0.100000
lag_rocka         _staticstruct     model3d=lag_rock_a.3do size=0.171275 movesize=0.171275 collwidth=0.171275 collheight=0.171275
cham_door         _sstructmetal     model3d=lag_door_chmbr.3do size=0.134838 movesize=0.134838 collwidth=0.134838 collheight=0.134838
underdoor         _sstructmetal     model3d=lag_door_uw.3do size=0.134838 movesize=0.134838 collwidth=0.134838 collheight=0.134838
boxcrnk           _sstructmetal     model3d=lag_box_crnk.3do size=0.042868 movesize=0.042868 collwidth=0.042868 collheight=0.042868
4mlagplank        _sstructwood      model3d=lag_plank_4m.3do size=0.206534 movesize=0.206534 collwidth=0.206534 collheight=0.206534
1moil_ja          _boomthing        model3d=lag_drum_1moilja.3do size=0.061033 movesize=0.061033 collwidth=0.061033 collheight=0.061033 explode=+gen_exp_metal
lagbut            _decor            model3d=lag_button_halfm.3do size=0.042726 movesize=0.042726 collwidth=0.042726 collheight=0.042726
bronzedoors       _sstructmetal     model3d=lag_doors_bronz.3do size=0.214944 movesize=0.214944 collwidth=0.214944 collheight=0.214944
palmshrub         _ghostdecor       model3d=olv_shrub_palm_a.3do size=0.062500 movesize=0.062500 collwidth=0.062500 collheight=0.062500
medrock           _staticstruct     model3d=tem_rock_b.3do size=0.108744 movesize=0.108744 collwidth=0.108744 collheight=0.108744
whipvine          _swingstruct      model3d=lag_strut_vine.3do collwidth=0.100000 collheight=0.100000
torpedo           _sstructmetal     move=physics model3d=lag_trpedo.3do size=0.151327 movesize=0.151327 collwidth=0.151327 collheight=0.151327 puppet=default.pup
chutetent         _sstructwood      model3d=lag_tent_chute.3do size=0.368381 movesize=0.368381 collwidth=0.368381 collheight=0.368381
campcot           _sstructmetal     model3d=lag_cot_camp.3do size=0.109545 movesize=0.109545 collwidth=0.109545 collheight=0.109545
+komodobite       +bughit           damage=75.000000
komodo            _gen_ai_avrg      thingflags=0x8000400 model3d=gen_li.3do size=0.062000 movesize=0.062000 collwidth=0.062000 collheight=0.062000 puppet=komodo.pup soundclass=komodo.snd cog=actor_komodo.cog maxvel=0.650000 weapon=+komodobite health=200.000000 maxhealth=200.000000 typeflags=0x20001 eyeoffset=(0.000000/0.000000/-0.035000) aiclass=komodo.ai
cranestrut        _swingstruct      model3d=lag_crane_strut.3do collwidth=0.100000 collheight=0.100000
+fishbite         +bughit           damage=10.000000
fish              _swimactor        model3d=gen_fi.3do size=0.028057 movesize=0.028057 collwidth=0.028057 collheight=0.028057 puppet=fish.pup soundclass=piranha.snd surfdrag=5.500000 mass=5.000000 height=0.100000 maxrotvel=35.000000 weapon=+fishbite health=125.000000 maxhealth=125.000000 maxthrust=0.500000 maxrotthrust=175.000000 typeflags=0x20040 eyeoffset=(0.000000/0.025000/0.000000) aiclass=fish.ai
lagradio          _sstructmetal     model3d=lag_radio.3do size=0.036113 movesize=0.036113 collwidth=0.036113 collheight=0.036113
lag_rockd         _staticstruct     model3d=lag_rock_d.3do size=0.437807 movesize=0.437807 collwidth=0.437807 collheight=0.437807
seaweedb          _ghostdecor       model3d=gen_weed_seab.3do size=0.075000 movesize=0.075000 collwidth=0.075000 collheight=0.075000
crab              _actor            model3d=gen_bc.3do size=0.011000 movesize=0.011000 collwidth=0.011000 collheight=0.011000 puppet=crab.pup surfdrag=1.500000 airdrag=0.200000 staticdrag=0.005000 mass=10.000000 buoyancy=0.800000 health=10.000000 maxthrust=0.350000 maxrotthrust=150.000000 typeflags=0x100000 aiclass=crab.ai
padkey            _item             thingflags=0x400 model3d=lag_key_pdlck.3do collwidth=0.020000 collheight=0.020000 cog=item_padlockkey.cog height=0.002660 buoyancy=0.000000
hugerock          _staticstruct     model3d=tem_rock_d.3do size=0.437807 movesize=0.437807 collwidth=0.437807 collheight=0.437807
brnchb            _wstructwood      model3d=jep_branch_wall_b.3do size=0.760819 movesize=0.760819 collwidth=0.760819 collheight=0.760819
fieldshovel       _item             thingflags=0x400 model3d=lag_shovel_fold.3do collwidth=0.020000 collheight=0.020000 cog=item_shovel.cog height=0.039000
burntbodya        _ghostdecor       model3d=gen_burnt_bodya.3do size=0.015987 movesize=0.015987 collwidth=0.015987 collheight=0.015987
ltchhook          _ghostdecor       model3d=lag_ltch_hook.3do size=0.022500 movesize=0.022500 collwidth=0.022500 collheight=0.022500
ltchlock          _decormetal       model3d=lag_ltch_lock.3do size=0.026000 movesize=0.026000 collwidth=0.026000 collheight=0.026000
rstytlgrf         _sstructmetal     thingflags=0x80 model3d=lag_telgrf_rsty.3do size=0.072253 movesize=0.072253 collwidth=0.072253 collheight=0.072253
ltchbar           _ghostdecor       model3d=lag_ltch_bar.3do size=0.015000 movesize=0.015000 collwidth=0.015000 collheight=0.015000
rstcrnkmove       _walkstruct       model3d=lag_crnk_rsty.3do size=0.081133 movesize=0.081133 collwidth=0.081133 collheight=0.081133 puppet=default.pup
opncrate          _wstructwood      model3d=lag_crate_opn.3do size=0.038000 movesize=0.025000 collwidth=0.025000 collheight=0.025000
crtlid            _wstructwood      model3d=lag_crate_lid.3do size=0.043038 movesize=0.043038 collwidth=0.043038 collheight=0.043038
armdevicemove     _decor            model3d=lag_dvice_armng.3do size=0.011236 movesize=0.011236 collwidth=0.011236 collheight=0.011236
propellormove     _decor            model3d=lag_plane_prop.3do size=0.059223 movesize=0.059223 collwidth=0.059223 collheight=0.059223
machete           _item             thingflags=0x400 model3d=weap_machete_sheath.3do collwidth=0.020000 collheight=0.020000 cog=item_machete.cog height=0.002000
hammer            _item             thingflags=0x400 model3d=lag_hammer_small.3do collwidth=0.020000 collheight=0.020000 cog=item_hammer.cog height=0.003710 buoyancy=0.000000
htchcvr           _sstructmetal     model3d=lag_hatch_cvr.3do size=0.153512 movesize=0.153512 collwidth=0.153512 collheight=0.153512
rstywheel         _wstructmetal     thingflags=0x80 model3d=lag_wheel_rsty.3do size=0.084452 movesize=0.084452 collwidth=0.084452 collheight=0.084452
temspikes         _wstructwood      model3d=tem_pikes_floor.3do size=0.105091 movesize=0.105091 collwidth=0.105091 collheight=0.105091
temspiketorso     _staticstruct     model3d=tem_torso_pike.3do size=0.105091 movesize=0.105091 collwidth=0.105091 collheight=0.105091
mine              _decor            thingflags=0x400 model3d=gen_mine.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000 cog=class_mine.cog
olvbutpull        _decor            model3d=olv_button_pull.3do size=0.042726 movesize=0.042726 collwidth=0.042726 collheight=0.042726 soundclass=gen_stonebutn_pull.snd
smlpalmtree       _sstructwood      model3d=gen_tree_palmsml.3do size=0.230689 movesize=0.230689 collwidth=0.230689 collheight=0.230689
wine              _staticstruct     model3d=sea_ration_wine.3do size=0.028377 movesize=0.028377 collwidth=0.028377 collheight=0.028377
enginebox         _sstructblock     model3d=sea_ngin_box.3do size=0.638602 movesize=0.638602 collwidth=0.638602 collheight=0.638602
supports          _sstructwood      model3d=tem_bridge_ropebase.3do size=0.073993 movesize=0.073993 collwidth=0.073993 collheight=0.073993
fallbridge        _sstructwood      model3d=tem_bridge_fall.3do size=0.458673 movesize=0.458673 collwidth=0.458673 collheight=0.458673
sandflat          _staticstruct     model3d=lag_sand_flt.3do size=0.707107 movesize=0.707107 collwidth=0.707107 collheight=0.707107
fieldshovelmove   _decor            model3d=lag_shovel_fold.3do size=0.045750 movesize=0.045750 collwidth=0.045750 collheight=0.045750
stnshrapas_nc_ns  _debrisnc         thingflags=0x1 light=(0.100000/0.100000/0.100000/1.000000) model3d=gen_dbre_stnwalas.3do size=0.003867 movesize=0.003867 collwidth=0.003867 collheight=0.003867 angvel=(0.000000/0.000000/0.000000)
dirt              _sprite           timer=2.000000 sprite=gen_dirt.spr
workstand         _sstructmetal     model3d=vol_lights_workstnd.3do size=0.170816 movesize=0.170816 collwidth=0.170816 collheight=0.170816
worklights        _decormetal       model3d=vol_lights_workbulb.3do size=0.066833 movesize=0.066833 collwidth=0.066833 collheight=0.066833
medallion         _sstructmetal     model3d=vol_gong_hang.3do size=0.477624 movesize=0.477624 collwidth=0.477624 collheight=0.477624
hangblock         _pushstruct2m     model3d=vol_block_2x2hang.3do size=0.173205 movesize=0.173205 collwidth=0.173205 collheight=0.173205
door              _staticstruct     thingflags=0xC1 light=(0.200000/0.100000/0.100000/0.001000) model3d=vol_door_2x3m.3do size=0.178895 movesize=0.178895 collwidth=0.178895 collheight=0.178895 soundclass=vol_door.snd
portcullisb       _structmetal      thingflags=0x400001 light=(0.200000/0.200000/0.200000/0.200000) model3d=vol_gate_portcullis_b.3do size=0.304836 movesize=0.304836 collwidth=0.304836 collheight=0.304836 soundclass=vol_co-op_gate.snd
voltorch          _decormetal       model3d=vol_torch_wall.3do size=0.025000 movesize=0.025000 collwidth=0.025000 collheight=0.025000
columnhead        _staticstruct     model3d=vol_head_column.3do size=0.065000 movesize=0.065000 collwidth=0.065000 collheight=0.065000
demondoor         _staticstruct     thingflags=0xC1 light=(0.200000/0.100000/0.100000/0.001000) model3d=vol_door_demon.3do size=0.168232 movesize=0.168232 collwidth=0.168232 collheight=0.168232 soundclass=vol_door.snd
demonb            _staticstruct     model3d=vol_demonb.3do size=0.155000 movesize=0.155000 collwidth=0.155000 collheight=0.155000
stalagb           _staticstruct     model3d=vol_stlgmt_b.3do size=0.341630 movesize=0.341630 collwidth=0.341630 collheight=0.341630
stalaga           _staticstruct     model3d=vol_stlgmt_a.3do size=0.522337 movesize=0.522337 collwidth=0.522337 collheight=0.522337
portcullis        _structmetal      model3d=vol_gate_portcullis.3do size=0.251430 movesize=0.251430 collwidth=0.251430 collheight=0.251430 soundclass=vol_gate.snd
stalagmite        _ghostdecor       thingflags=0x1 light=(0.100000/0.100000/0.200000/0.001000) model3d=cyn_stalagmitee.3do size=0.500000 movesize=0.500000 collwidth=0.500000 collheight=0.500000
6mbeam            _sstructwood      model3d=vol_beam_6m.3do size=0.302076 movesize=0.302076 collwidth=0.302076 collheight=0.302076
6mplank           _sstructwood      model3d=vol_plank_6m.3do size=0.304395 movesize=0.304395 collwidth=0.304395 collheight=0.304395
keyboat           _sstructwood      model3d=vol_boat_key.3do size=0.570000 movesize=0.100000 collwidth=0.402034 collheight=0.402034
boatstand         _sstructwood      model3d=vol_stand_boat.3do size=0.225000 movesize=0.225000 collwidth=0.225000 collheight=0.225000 soundclass=vol_piers.snd
shark2            shark             model3d=gen_sh2.3do
volbut            _decor            thingflags=0x1 light=(0.200000/0.200000/0.200000/0.001000) model3d=vol_button_halfm.3do size=0.024500 movesize=0.024500 collwidth=0.024500 collheight=0.024500 soundclass=vol_stonebutn.snd
winch             _sstructmetal     model3d=vol_winch.3do size=0.315705 movesize=0.315705 collwidth=0.315705 collheight=0.315705 puppet=default.pup
winchplatform     _sstructwood      model3d=vol_platform_winch.3do size=0.370000 movesize=0.150000 collwidth=0.280192 collheight=0.280192 soundclass=vol_commie_elev.snd
20mplateau        _staticstruct     thingflags=0x8000C0 model3d=tem_plateau_brdg20m.3do size=1.220656 movesize=1.220656 collwidth=1.220656 collheight=1.220656
jungle_tokarev    _ai_tokarev       model3d=com_1h_jung.3do soundclass=russki1.snd
tmask             _staticstruct     model3d=vol_mask_ledge.3do size=0.763217 movesize=0.763217 collwidth=0.763217 collheight=0.763217
switchbox         _structmetal      thingflags=0x400001 light=(0.200000/0.200000/0.200000/0.001000) model3d=vol_box_switch.3do size=0.066052 movesize=0.066052 collwidth=0.066052 collheight=0.066052
strtbase          _staticstruct     thingflags=0xC1 light=(0.300000/0.100000/0.100000/0.001000) model3d=vol_bridge_base.3do size=0.379636 movesize=0.379636 collwidth=0.379636 collheight=0.379636
brdgstrt          _swingstruct      model3d=vol_strut_bridge.3do collwidth=0.100000 collheight=0.100000
1moilb            _boomthing        model3d=sol_drum_1moilb.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000 explode=+barrel_exp
platb             _staticstruct     model3d=vol_pltfrm_b.3do size=0.141973 movesize=0.141973 collwidth=0.141973 collheight=0.141973
tem2x2x1          _staticstruct     model3d=tem_block_2x2x1.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000
tooth             _staticstruct     model3d=vol_mask_tooth.3do size=0.055902 movesize=0.055902 collwidth=0.055902 collheight=0.055902
strtposts         _staticstruct     model3d=vol_strut_posts.3do size=0.509902 movesize=0.509902 collwidth=0.509902 collheight=0.509902
plata             _staticstruct     model3d=vol_pltfrm_a.3do size=0.141973 movesize=0.141973 collwidth=0.141973 collheight=0.141973
smlboulder        _actor            thingflags=0x40 model3d=gen_boulder_sml.3do size=0.124368 movesize=0.124368 collwidth=0.124368 collheight=0.124368 puppet=boulder.pup surfdrag=1.000000 staticdrag=0.020000 mass=1000.000000 physflags=0x4ACF typeflags=0x300D08
bushc             _ghostdecor       model3d=gen_bush_standupc.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000
firemove          _decor            model3d=pyr_kndlng_fire.3do
invisiplat        _structblock      model3d=gen_invisiplate.3do
jailgatl          _sstructmetal     model3d=vol_gate_jail_l.3do size=0.200000 movesize=0.100000 collwidth=0.195481 collheight=0.195481
jailgatr          _sstructmetal     model3d=vol_gate_jail_r.3do size=0.200000 movesize=0.100000 collwidth=0.195481 collheight=0.195481
keysharkgatemove  _decor            model3d=vol_key_sharkgate.3do size=0.002000 movesize=0.002000 collwidth=0.002000 collheight=0.002000
lavabub           _ghostdecor       model3d=gen_bubble_lava.3do size=0.025000 movesize=0.025000 collwidth=0.025000 collheight=0.025000
3stalagmitesbox   _staticstruct     model3d=cyn_stalagmitea_box.3do size=0.450000 movesize=0.450000 collwidth=0.450000 collheight=0.450000
4stalagmitesbox   _staticstruct     model3d=cyn_stalagmiteb_box.3do size=0.248664 movesize=0.248664 collwidth=0.248664 collheight=0.248664
stalagmitebox     _staticstruct     model3d=cyn_stalagmitee_box.3do size=0.500000 movesize=0.500000 collwidth=0.500000 collheight=0.500000
lavatrapblock     _staticstruct     thingflags=0xC1 light=(0.200000/0.200000/0.200000/0.200000) model3d=vol_block_cork.3do size=0.600000 movesize=0.335410 collwidth=0.335410 collheight=0.335410 soundclass=vol_piers.snd
floor_door        _staticstruct     thingflags=0xC1 light=(0.200000/0.200000/0.300000/0.200000) model3d=vol_door_floor.3do size=0.198011 movesize=0.198011 collwidth=0.198011 collheight=0.198011 soundclass=vol_piers.snd
catbamc           _sstructwood      model3d=vol_ctwlk_bmboo_c.3do size=0.231840 movesize=0.231840 collwidth=0.231840 collheight=0.231840
catbama           _sstructwood      model3d=vol_ctwlk_bmboo_a.3do size=0.292617 movesize=0.292617 collwidth=0.292617 collheight=0.292617
catbamb           _sstructwood      model3d=vol_ctwlk_bmboo_b.3do size=0.283945 movesize=0.283945 collwidth=0.283945 collheight=0.283945
deathaltar        _staticstruct     model3d=vol_altar_lava.3do size=0.305164 movesize=0.305164 collwidth=0.305164 collheight=0.305164
hangslab          _staticstruct     model3d=vol_slab_2x2hang.3do size=0.153054 movesize=0.153054 collwidth=0.153054 collheight=0.153054
lava_chan         _ghostdecor       model3d=vol_lava_chan.3do
sophiavol         _ghostdecor       model3d=vol_ladr_rope_so.3do size=0.500000 movesize=0.020000 collwidth=0.026138 collheight=0.026138 puppet=default.pup
bucketwat         _decormetal       model3d=gen_bucket_mtlwatr.3do size=0.030896 movesize=0.030896 collwidth=0.030896 collheight=0.030896
guard_tokarev     _ai_tokarev       model3d=com_1h_gard.3do soundclass=russki1.snd
tramapier         _staticstruct     model3d=vol_pier_trama.3do size=0.318519 movesize=0.318519 collwidth=0.318519 collheight=0.318519
trambpier         _staticstruct     model3d=vol_pier_tramb.3do size=0.318519 movesize=0.318519 collwidth=0.318519 collheight=0.318519
lavahead          _staticstruct     model3d=vol_head_lavab.3do size=0.357386 movesize=0.357386 collwidth=0.357386 collheight=0.357386
stalagmited       _ghostdecor       model3d=cyn_stalagmited.3do size=0.900000 movesize=0.900000 collwidth=0.900000 collheight=0.900000
hotlavahead       _staticstruct     thingflags=0xC1 light=(0.300000/0.200000/0.200000/0.200000) model3d=vol_head_lavaa.3do size=0.357386 movesize=0.357386 collwidth=0.357386 collheight=0.357386
tramplat          _sstructmetal     model3d=vol_pltfrm_tram.3do size=0.481047 movesize=0.481047 collwidth=0.481047 collheight=0.481047
release           _sstructmetal     model3d=vol_release_arm.3do size=0.201246 movesize=0.201246 collwidth=0.201246 collheight=0.201246 puppet=default.pup
tramcable         _sstructmetal     model3d=vol_cable_tram.3do size=0.300042 movesize=0.300042 collwidth=0.300042 collheight=0.300042
tramcar           _sstructmetal     model3d=vol_tram_car.3do size=0.280000 movesize=0.080000 collwidth=0.284649 collheight=0.284649 soundclass=vol_funicular_run.snd
trampivotpiece    _sstructmetal     model3d=vol_tram_piece.3do size=0.061581 movesize=0.061581 collwidth=0.061581 collheight=0.061581
jungle_simonov    _ai_simonov       model3d=com_2h_jung.3do soundclass=russki1.snd
spetz_simonov     _ai_simonov       model3d=com_2h_forc.3do soundclass=russki1.snd
jungle_simonov3   _ai_simonov       model3d=com_2h_jung.3do soundclass=russki3.snd
guard_mauser      _ai_mauser        model3d=com_1h_gard.3do soundclass=russki1.snd
console_radio     _staticstruct     model3d=bab_console_radio.3do size=0.075000 movesize=0.075000 collwidth=0.075000 collheight=0.075000
1moil_jb          _boomthing        model3d=lag_drum_1moiljb.3do size=0.061033 movesize=0.061033 collwidth=0.061033 collheight=0.061033 explode=+gen_exp_metal
2x2crate          _pstruct2mwood    model3d=cyn_crate_2x2.3do size=0.173205 movesize=0.173205 collwidth=0.173205 collheight=0.173205
armwhp            _staticstruct     model3d=vol_whip_arm.3do size=0.023970 movesize=0.023970 collwidth=0.023970 collheight=0.023970 puppet=default.pup
lekk              _staticstruct     model3d=vol_lekk.3do size=0.343619 movesize=0.343619 collwidth=0.343619 collheight=0.343619
lekkstrut         _aimstruct        thingflags=0x80000008 model3d=vol_lekkarm.3do size=0.088966 movesize=0.088966 collwidth=0.088966 collheight=0.088966 health=99999.000000 maxhealth=99999.000000 typeflags=0x100
boatking          _ghostdecor       model3d=vol_boat_king.3do size=0.012857 movesize=0.012857 collwidth=0.012857 collheight=0.012857
lava_right        _ghostdecor       model3d=vol_lava_right.3do
lava_left         _ghostdecor       model3d=vol_lava_left.3do
portcullispoles   _structmetal      model3d=vol_gate_portcullis_poles.3do size=0.410000 movesize=0.410000 collwidth=0.410000 collheight=0.410000
3mhangledge       _structblock      model3d=gen_3m_hangledge.3do size=0.152069 movesize=0.152069 collwidth=0.152069 collheight=0.152069
2.5mhangledge     _structblock      model3d=gen_2hlfm_hangledge.3do size=0.127475 movesize=0.127475 collwidth=0.127475 collheight=0.127475
winchup           _sstructmetal     model3d=vol_winch_up.3do size=0.202500 movesize=0.202500 collwidth=0.202500 collheight=0.202500 puppet=default.pup
guard_tokarev2    _ai_tokarev       model3d=com_1h_gard.3do soundclass=russki2.snd
jungle_tokarev2   _ai_tokarev       model3d=com_1h_jung.3do soundclass=russki2.snd
2x1frglcomcrate   _sstructwood      model3d=vol_crate_2x1cyrlcf.3do size=0.122474 movesize=0.122474 collwidth=0.122474 collheight=0.122474
rackb             _decormetal       model3d=sea_rack_b.3do size=0.069282 movesize=0.069282 collwidth=0.069282 collheight=0.069282
rackc             _decormetal       model3d=sea_rack_c.3do size=0.069282 movesize=0.069282 collwidth=0.069282 collheight=0.069282
radarc            _decormetal       model3d=sea_radar_scope_c.3do size=0.036486 movesize=0.036486 collwidth=0.036486 collheight=0.036486
submachine        _itemex           thingflags=0x401 model3d=weap_ppsh41_inv.3do collwidth=0.003000 collheight=0.003000 cog=item_submachine.cog
grenades          _item             thingflags=0x400 model3d=weap_grenade_pack.3do collwidth=0.020000 collheight=0.020000 cog=item_grenade.cog height=0.007700
sharkbars         _sstructmetal     model3d=vol_bars_shark.3do size=0.256083 movesize=0.256083 collwidth=0.256083 collheight=0.256083 soundclass=vol_shark_bars.snd
crwlbox           _sstruct2mwood    model3d=vol_crateinsout.3do size=0.172401 movesize=0.172401 collwidth=0.172401 collheight=0.172401
boxtop            _sstructwood      model3d=vol_crateinsout_lid.3do size=0.141243 movesize=0.141243 collwidth=0.141243 collheight=0.141243
tramwheel         _item             thingflags=0x400 model3d=vol_tram_wheel.3do collwidth=0.020000 collheight=0.020000 cog=item_tramwheel.cog height=0.018500
keycuffsmove      _decor            model3d=vol_key_cuffs.3do size=0.021132 movesize=0.021132 collwidth=0.021132 collheight=0.021132
tram_indy         _cutactor         model3d=gen_in_shirt_tram.3do collwidth=0.020000 collheight=0.020000 puppet=default.pup aiclass=none.ai
_ai_submachine    _ai_actor_fast    thingflags=0x28004400 size=0.037000 movesize=0.037000 puppet=com_2h.pup cog=actor_aisubmachine.cog health=250.000000 maxhealth=250.000000 aiclass=com_2h.ai
jungle_submach2   _ai_submachine    model3d=com_2h_jung.3do soundclass=russki2.snd
lavasparks        _particle         airdrag=2.100000 mass=0.210000 physflags=0x201 vel=(0.000000/0.000000/0.600000) material=aet_4sprite_glow_fairy_dust.mat range=0.040000 rate=15.000000 maxthrust=11.000000 elementsize=0.020000 count=10
h2ominifx         h2ofx             mass=0.210000 range=0.040000 elementsize=0.005000 count=30
lavaplume         _particle         timer=3.500000 airdrag=10.000000 mass=0.000001 physflags=0x201 vel=(0.000000/0.000000/10.000000) typeflags=0x91 material=aet_4sprite_glow_fairy_dust.mat range=0.200000 minsize=0.050000 elementsize=0.005000 count=40
plumea            _staticstruct     model3d=vol_plume_lava_a.3do
plumeb            _staticstruct     model3d=vol_plume_lava_b.3do
explosiondust     _particle         timer=1.500000 airdrag=20.000000 mass=0.000010 physflags=0x201 vel=(-0.200000/0.000000/0.500000) material=gen_4particle_snow.mat range=0.200000 minsize=0.100000 elementsize=0.003500 count=70
stoneshrapc_nc    _debrisnc         model3d=gen_dbre_stnwalc.3do size=0.012601 movesize=0.012601 collwidth=0.012601 collheight=0.012601
+dummy_bazooka    _weapon           timer=5.000000 vel=(0.000000/5.000000/0.000000) explode=+bazooka_exp typeflags=0xC
dart              _decorwood        model3d=pru_dart.3do size=0.050000 movesize=0.050000 collwidth=0.020396 collheight=0.020396
+dartsmoke_spr    _sprite           timer=0.500000 sprite=dartsmoke.spr
slashleaves       _particle         mass=10.000000 physflags=0x201 vel=(0.000000/0.000000/-0.500000) material=gen_4leaf_cuttings.mat range=0.020000 rate=18.000000 maxthrust=30.000000 elementsize=0.030000 count=10
flamesparks1      _particle         timer=0.349000 vel=(0.000000/0.000000/1.000000) material=aet_4sprite_glow_fairy_dust.mat range=0.005000 minsize=0.000100 elementsize=0.005000 count=1
debcatbbama       _debris           timer=4.000000 model3d=vol_ctwlk_bmboo_bdbrea.3do size=0.160078 movesize=0.160078 collwidth=0.160078 collheight=0.160078 vel=(0.000000/0.000000/0.000000)
debcatcbama       _debris           timer=4.000000 model3d=vol_ctwlk_bmboo_cdbrea.3do size=0.188775 movesize=0.188775 collwidth=0.188775 collheight=0.188775 vel=(0.000000/0.000000/0.000000)
debcatcbamb       _debris           timer=4.000000 model3d=vol_ctwlk_bmboo_cdbreb.3do size=0.199695 movesize=0.199695 collwidth=0.199695 collheight=0.199695 vel=(0.000000/0.000000/0.000000)
debcatbbamc       _debris           timer=4.000000 model3d=vol_ctwlk_bmboo_bdbrec.3do size=0.217034 movesize=0.217034 collwidth=0.217034 collheight=0.217034 vel=(0.000000/0.000000/0.000000)
debcatbbamb       _debris           timer=4.000000 model3d=vol_ctwlk_bmboo_bdbreb.3do size=0.224429 movesize=0.224429 collwidth=0.224429 collheight=0.224429 vel=(0.000000/0.000000/0.000000)
debcatcbamc       _debris           timer=4.000000 model3d=vol_ctwlk_bmboo_cdbrec.3do size=0.185737 movesize=0.185737 collwidth=0.185737 collheight=0.185737 vel=(0.000000/0.000000/0.000000)
trampivot         _sstructmetal     model3d=vol_tram_pivot.3do size=0.048040 movesize=0.048040 collwidth=0.048040 collheight=0.048040 puppet=default.pup
stnshrapbm_nc_ns  _debrisnc         thingflags=0x1 light=(0.100000/0.100000/0.100000/1.000000) model3d=gen_dbre_stnwalbm.3do size=0.006250 movesize=0.006250 collwidth=0.006250 collheight=0.006250 angvel=(0.000000/0.000000/0.000000)
stnshrapcs_nc_ns  _debrisnc         thingflags=0x1 light=(0.100000/0.100000/0.100000/1.000000) model3d=gen_dbre_stnwalcs.3do size=0.003125 movesize=0.003125 collwidth=0.003125 collheight=0.003125 angvel=(0.000000/0.000000/0.000000)
jungle_mauser2    _ai_mauser        model3d=com_1h_jung.3do soundclass=russki2.snd
+com_grenade      +grenade          angvel=(90.000000/45.000000/90.000000)
_ai_grenade       _ai_actor_normal  thingflags=0x28004400 size=0.037000 movesize=0.037000 puppet=com_1h.pup cog=actor_aigrenade.cog weapon=+com_grenade health=350.000000 maxhealth=350.000000 maxrotthrust=800.000000 typeflags=0x8020001 aiclass=com_1hgrenade.ai
agent_grenade4    _ai_grenade       model3d=com_1h_agnt.3do soundclass=russki4.snd
khaki_simonov4    _ai_simonov       model3d=com_2h_kaki.3do soundclass=russki4.snd
jungle_simonov2   _ai_simonov       model3d=com_2h_jung.3do soundclass=russki2.snd
grenade           _item             orient=(54.989609/180.000000/-90.000000) thingflags=0x400 model3d=weap_grenade_inv.3do collwidth=0.020000 collheight=0.020000 cog=item_grenade.cog height=0.003000
cavehut           _sstructwood      model3d=tem_hut_cave.3do size=0.736546 movesize=0.736546 collwidth=0.736546 collheight=0.736546
brokepots         _ghostdecor       model3d=tem_pots_brk.3do size=0.010638 movesize=0.010638 collwidth=0.010638 collheight=0.010638
skspike           _sstructwood      model3d=tem_skeleton_spike.3do size=0.094001 movesize=0.094001 collwidth=0.094001 collheight=0.094001
20mholesbridge    _sstructwood      model3d=tem_bridge_20mholes.3do size=1.023693 movesize=1.023693 collwidth=1.023693 collheight=1.023693
tikildoor         _staticstruct     model3d=tem_doors_tikil.3do size=0.321911 movesize=0.321911 collwidth=0.321911 collheight=0.321911 soundclass=tem_bossdoor.snd
squat_stat        _staticstruct     model3d=tem_statue_squat.3do size=0.254755 movesize=0.254755 collwidth=0.254755 collheight=0.254755
20mropebridge     _sstructwood      model3d=tem_bridge_20mrope.3do size=1.009009 movesize=1.009009 collwidth=1.009009 collheight=1.009009
10mbrokebridge    _sstructwood      model3d=tem_bridge_10mbrk.3do size=0.590950 movesize=0.590950 collwidth=0.590950 collheight=0.590950
2mlavapad         _staticstruct     model3d=tem_pad_2mlava.3do size=0.132164 movesize=0.132164 collwidth=0.132164 collheight=0.132164
doorfrm           _staticstruct     model3d=tem_frame_door.3do size=0.400000 movesize=0.400000 collwidth=0.400000 collheight=0.400000
mazedoor          _staticstruct     model3d=tem_door_maze.3do size=0.363352 movesize=0.363352 collwidth=0.363352 collheight=0.363352 soundclass=tem_mazedoor.snd
broketikib        _staticstruct     model3d=tem_column_tikibrkb.3do size=0.236854 movesize=0.236854 collwidth=0.236854 collheight=0.236854
broketikia        _staticstruct     model3d=tem_column_tikibrka.3do size=0.278792 movesize=0.278792 collwidth=0.278792 collheight=0.278792
monkeykeymove     _decor            model3d=tem_key_mnky.3do size=0.007000 movesize=0.007000 collwidth=0.007000 collheight=0.007000
ldoorl            _sstructwood      model3d=tem_door_lockl.3do size=0.222728 movesize=0.222728 collwidth=0.222728 collheight=0.222728 soundclass=tem_monkeydoor.snd
ldoorr            _sstructwood      model3d=tem_door_lockr.3do size=0.228695 movesize=0.228695 collwidth=0.228695 collheight=0.228695 soundclass=tem_monkeydoor.snd
tikirdoor         _staticstruct     model3d=tem_doors_tikir.3do size=0.321911 movesize=0.321911 collwidth=0.321911 collheight=0.321911 soundclass=tem_bossdoor.snd
+lavahit_md       +bughit           size=0.010000 movesize=0.010000 damage=50.000000 damageclass=0x80 range=0.100000
+lw_debris_s      _debris           timer=120.000000 model3d=tem_lw_debris_s.3do size=0.003125 movesize=0.003125 collwidth=0.003125 collheight=0.003125 physflags=0x11 vel=(0.000000/0.250000/0.100000) angvel=(0.000000/0.000000/0.000000) damage=50.000000
+lw_debris_m      _debris           timer=120.000000 model3d=tem_lw_debris_m.3do size=0.006250 movesize=0.006250 collwidth=0.006250 collheight=0.006250 physflags=0x11 vel=(0.000000/0.250000/0.100000) angvel=(0.000000/0.000000/0.000000) damage=50.000000
+lw_exp           _explosion        timer=1.100000 soundclass=gen_explode.snd typeflags=0x3F damage=100.000000 damageclass=0x1 blasttime=1.000000 range=0.500000 debris=+lw_debris_s debris=+lw_debris_s debris=+lw_debris_s debris=+lw_debris_s debris=+lw_debris_s debris=+lw_debris_s debris=+lw_debris_s debris=+lw_debris_s debris=+lw_debris_m debris=+lw_debris_m debris=+lw_debris_m debris=+lw_debris_m debris=+lw_debris_m debris=+lw_debris_m debris=+lw_debris_m debris=+lw_debris_m spritething=+baz_spr_exp_smoke spritestart=(0.250000/0.250000/1.000000) spriteend=(0.550000/0.550000/0.000000)
lava_warrior      _actor            thingflags=0x8000401 model3d=tem_lw.3do size=0.090000 movesize=0.090000 collwidth=0.099701 collheight=0.099701 puppet=lavawarrior.pup soundclass=lavawarrior.snd cog=actor_lavawarrior.cog surfdrag=7.000000 airdrag=0.300000 staticdrag=0.001000 mass=5000.000000 weapon=+lavahit_md health=100.000000 maxhealth=100.000000 maxthrust=0.600000 maxrotthrust=225.000000 typeflags=0x816412D eyeoffset=(0.000000/0.021600/0.083400) lightoffset=(0.000000/0.021600/0.083400) explode=+lw_exp aiclass=lavawarrior_sleep.ai
tikikeya          _sstructwood      model3d=tem_key_notaba.3do size=0.078899 movesize=0.078899 collwidth=0.078899 collheight=0.078899
tikimska          _ghostdecor       model3d=vol_tiki_a.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
tikimskc          _ghostdecor       model3d=vol_tiki_c.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
temstrtclimb      _climbstruct      model3d=tem_strut_u.3do size=0.095164 movesize=0.095164 collwidth=0.095164 collheight=0.095164
shrkdoor          _staticstruct     model3d=tem_sharkdoor.3do size=0.317974 movesize=0.317974 collwidth=0.317974 collheight=0.317974 soundclass=tem_sharkdoor.snd
1mtem_ped         _staticstruct     model3d=tem_block_1x1push.3do size=0.086603 movesize=0.086603 collwidth=0.086603 collheight=0.086603
20mholebridge     _sstructwood      model3d=tem_bridge_20mhole.3do size=1.023698 movesize=1.023698 collwidth=1.023698 collheight=1.023698
25mholebridge     _sstructwood      model3d=tem_brdg_25mhole.3do size=1.583864 movesize=1.583864 collwidth=1.583864 collheight=1.583864
1mmazerock        _staticstruct     model3d=tem_plateau_maze1m.3do size=0.231355 movesize=0.231355 collwidth=0.231355 collheight=0.231355
sharkkey          _item             thingflags=0x400 model3d=tem_sharkey.3do size=0.022000 movesize=0.022000 collwidth=0.020000 collheight=0.020000 cog=item_sharkkey.cog height=0.005500
demon             _staticstruct     model3d=vol_demon.3do size=0.155000 movesize=0.155000 collwidth=0.155000 collheight=0.155000
+monkeyhit        +bughit           damage=3.000000
monkey            _actor            thingflags=0x8000000 model3d=gen_mo.3do size=0.050000 movesize=0.050000 collwidth=0.050017 collheight=0.050017 puppet=monkey.pup soundclass=monkey.snd surfdrag=1.750000 airdrag=0.300000 staticdrag=0.010000 mass=50.000000 weapon=+monkeyhit health=75.000000 maxhealth=75.000000 maxthrust=0.700000 maxrotthrust=300.000000 typeflags=0x100001 aiclass=monkey.ai
pltoa             _staticstruct2m   model3d=tem_plto_lavaa.3do size=0.379144 movesize=0.379144 collwidth=0.379144 collheight=0.379144
pltob             _staticstruct2m   model3d=tem_plto_lavab.3do size=0.735748 movesize=0.735748 collwidth=0.735748 collheight=0.735748
pltoj             _staticstruct2m   model3d=tem_plto_lavaj.3do size=0.420927 movesize=0.420927 collwidth=0.420927 collheight=0.420927
pltoc             _staticstruct2m   model3d=tem_plto_lavac.3do size=0.324478 movesize=0.324478 collwidth=0.324478 collheight=0.324478
pltod             _staticstruct2m   model3d=tem_plto_lavad.3do size=0.385095 movesize=0.385095 collwidth=0.385095 collheight=0.385095
pltoe             _staticstruct2m   model3d=tem_plto_lavae.3do size=0.453597 movesize=0.453597 collwidth=0.453597 collheight=0.453597
pltof             _staticstruct2m   model3d=tem_plto_lavaf.3do size=0.340074 movesize=0.340074 collwidth=0.340074 collheight=0.340074
rockbridge        _sstructwood      model3d=tem_bridge_rock.3do size=1.205197 movesize=1.205197 collwidth=1.205197 collheight=1.205197
imp2move          _decor            model3d=imp2_invis_inv.3do size=0.017526 movesize=0.017526 collwidth=0.017526 collheight=0.017526
temrockg          _staticstruct     model3d=tem_rock_g.3do size=0.437807 movesize=0.437807 collwidth=0.437807 collheight=0.437807
temrocke          _staticstruct     model3d=tem_rock_e.3do size=0.108744 movesize=0.108744 collwidth=0.108744 collheight=0.108744
tikimskg          _ghostdecor       model3d=vol_tiki_g.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
firestature       _staticstruct     model3d=vol_statue_fire.3do size=0.464383 movesize=0.464383 collwidth=0.464383 collheight=0.464383
temspikeskull     _sstructwood      model3d=tem_skull_pike.3do size=0.105091 movesize=0.105091 collwidth=0.105091 collheight=0.105091
columnbase        _walkstruct       model3d=tem_column_base.3do size=0.301040 movesize=0.301040 collwidth=0.301040 collheight=0.301040
columnblock       _staticstruct     model3d=tem_column_block.3do size=0.273383 movesize=0.273383 collwidth=0.273383 collheight=0.273383
columnlift        _staticstruct     model3d=tem_column_lift.3do size=0.277308 movesize=0.277308 collwidth=0.277308 collheight=0.277308
columnhooks       _staticstruct     model3d=tem_column_hooks.3do size=0.107500 movesize=0.107500 collwidth=0.107500 collheight=0.107500
tikikeybmove      _walkstruct       model3d=tem_key_statueb.3do size=0.086603 movesize=0.086603 collwidth=0.086603 collheight=0.086603 soundclass=tem_tikikeymove.snd
burntbodyb        _ghostdecor       model3d=gen_burnt_bodyb.3do size=0.011824 movesize=0.011824 collwidth=0.011824 collheight=0.011824
temimpstnd        _staticstruct     model3d=tem_stand_imp.3do size=0.062500 movesize=0.062500 collwidth=0.062500 collheight=0.062500
headdoorb         _staticstruct     model3d=tem_head_doorb.3do size=0.198433 movesize=0.198433 collwidth=0.198433 collheight=0.198433
headdoora         _staticstruct     model3d=tem_head_doora.3do size=0.198433 movesize=0.198433 collwidth=0.198433 collheight=0.198433
temledgeb         _staticstruct     model3d=tem_ledge_walkb.3do size=0.352172 movesize=0.352172 collwidth=0.352172 collheight=0.352172
sharkkeymove      _decor            model3d=tem_sharkey.3do size=0.022067 movesize=0.022067 collwidth=0.022067 collheight=0.022067
gush              _staticstruct     model3d=tem_water_gush.3do
temledgec         _staticstruct     model3d=tem_ledge_walkc.3do size=0.176086 movesize=0.176086 collwidth=0.176086 collheight=0.176086
temrockf          _staticstruct     model3d=tem_rock_f.3do size=0.084599 movesize=0.084599 collwidth=0.084599 collheight=0.084599
squat_statb       _staticstruct     model3d=tem_statue_squatb.3do size=0.217486 movesize=0.217486 collwidth=0.217486 collheight=0.217486
ff_2x3_half_a     _sprite           orient=(90.000000/0.000000/0.000000) sprite=waterfallfoam_2x3_half_a.spr
fallfoam_4x4      _sprite           orient=(90.000000/0.000000/0.000000) sprite=waterfallfoam_4x4.spr
ff_2x3_half_b     _sprite           orient=(90.000000/0.000000/0.000000) sprite=waterfallfoam_2x3_half_b.spr
hintnumber43      _hintbase         userval=43.000000
hintnumber45      _hintbase         userval=45.000000
pltog             _staticstruct2m   model3d=tem_plto_lavag.3do size=0.424824 movesize=0.424824 collwidth=0.424824 collheight=0.424824
nicebould         _staticstruct     model3d=tem_boulder.3do size=0.320900 movesize=0.300000 collwidth=0.175000 collheight=0.175000
pltob2m           _staticstruct2m   model3d=tem_plto_lavab_2mclimb.3do size=0.433393 movesize=0.433393 collwidth=0.433393 collheight=0.433393
fallfoam_3x5b     _sprite           orient=(90.000000/0.000000/0.000000) sprite=waterfallfoam_3x5b.spr
fallfoam_1x5      _sprite           orient=(90.000000/0.000000/0.000000) sprite=waterfallfoam_1x5.spr
grndebris         _debris           collide=0 model3d=tem_dbre.3do size=0.007606 movesize=0.007606 collwidth=0.007606 collheight=0.007606
10manimbridge     _wstructwood      model3d=tem_bridge_10manim.3do size=0.302741 movesize=0.302741 collwidth=0.302741 collheight=0.302741 puppet=default.pup
10mbridge         _sstructwood      model3d=tem_brdg_10still.3do size=0.509639 movesize=0.509639 collwidth=0.509639 collheight=0.509639
tikikeymove       _walkstruct       model3d=tem_key_statue.3do size=0.086603 movesize=0.086603 collwidth=0.086603 collheight=0.086603 soundclass=tem_tikikeymove.snd
stnshrapb_nc_ns   _debrisnc         thingflags=0x1 light=(0.100000/0.100000/0.100000/1.000000) model3d=gen_dbre_stnwalb.3do size=0.043652 movesize=0.043652 collwidth=0.043652 collheight=0.043652 angvel=(0.000000/0.000000/0.000000)
stnshrapc_nc_ns   _debrisnc         thingflags=0x1 light=(0.100000/0.100000/0.100000/1.000000) model3d=gen_dbre_stnwalc.3do size=0.012601 movesize=0.012601 collwidth=0.012601 collheight=0.012601 angvel=(0.000000/0.000000/0.000000)
boulder_mc        _actor            thingflags=0x400 model3d=tem_boulder.3do size=0.040000 movesize=0.040000 collwidth=0.175000 collheight=0.175000 puppet=default.pup surfdrag=3.000000 physflags=0x1004A5F maxvel=0.400000 maxthrust=0.350000 maxrotthrust=0.000000 jumpspeed=0.400000 aiclass=truck.ai
stoneshrapbm_nc   _debrisnc         model3d=gen_dbre_stnwalbm.3do size=0.006250 movesize=0.006250 collwidth=0.006250 collheight=0.006250
+debris_exp       _explosion        timer=0.200000 typeflags=0x482 blasttime=0.100000 range=0.500000 debris=stoneshrapa debris=stoneshrapb debris=stoneshrapc debris=stoneshrapa debris=stoneshrapb debris=stoneshrapc debris=stoneshrapa debris=stoneshrapb debris=stoneshrapc debris=stoneshrapa debris=stoneshrapb debris=stoneshrapc debris=stoneshrapa debris=stoneshrapb debris=stoneshrapc
+dummy_debris     +dummy_bazooka    explode=+debris_exp
+lavahit_lg       +bughit           thingflags=0x20000400 size=0.100000 movesize=0.100000 cog=weap_ailavahit.cog vel=(0.000000/10.000000/0.000000) damage=15.000000
lava_boss         _actor            thingflags=0x401 light=(2.000000/0.000000/0.000000/1.000000) model3d=tem_lb.3do size=0.225000 movesize=0.225000 collwidth=0.300000 collheight=0.370000 puppet=lavaboss.pup soundclass=lavaboss.snd cog=actor_lavaboss.cog surfdrag=3.000000 airdrag=0.300000 staticdrag=0.005000 mass=8000.000000 weapon=+lavahit_lg health=500.000000 maxhealth=500.000000 maxthrust=0.700000 maxrotthrust=80.000000 aiclass=lavaboss.ai
tem_spr_boss_smoke _sprite           sprite=bazooka_expsmoke.spr
lavashrapa        _debrisnc         model3d=gen_dbre_lavaa.3do size=0.012500 movesize=0.012500 collwidth=0.012500 collheight=0.012500
lavashrapb        _debrisnc         model3d=gen_dbre_lavab.3do size=0.014193 movesize=0.014193 collwidth=0.014193 collheight=0.014193
lavashrapc        _debrisnc         model3d=gen_dbre_lavac.3do size=0.012601 movesize=0.012601 collwidth=0.012601 collheight=0.012601
+lava_exp         _explosion        timer=0.300000 typeflags=0x2 blasttime=0.250000 range=0.500000 debris=lavashrapa debris=lavashrapb debris=lavashrapc debris=lavashrapa debris=lavashrapb debris=lavashrapc debris=lavashrapa debris=lavashrapb debris=lavashrapc debris=lavashrapa debris=lavashrapb debris=lavashrapc debris=lavashrapa debris=lavashrapb debris=lavashrapc
coollava_bfc      _structure        model3d=tem_lb_cool.3do size=0.326500 movesize=0.326500 collwidth=0.326500 collheight=0.326500 puppet=default.pup
coolava           _ghostdecor       model3d=tem_lava_cools.3do
poolava           _ghostdecor       model3d=tem_lava_pool.3do
lavabubopp        _ghostdecor       model3d=gen_bubble_lavab.3do size=0.025000 movesize=0.025000 collwidth=0.025000 collheight=0.025000
+ripples2         _sprite           orient=(90.000000/0.000000/0.000000) timer=2.099000 sprite=waterripple2.spr
+lavahit_sm       +bughit
+lava_guy_smoke   _explosion        timer=1.100000 typeflags=0x11 spritething=+baz_spr_exp_smoke spritestart=(0.250000/0.250000/1.000000) spriteend=(0.550000/0.550000/0.000000)
+lava_guy_fire    _explosion        timer=0.800000 soundclass=lavaguyexplode.snd creatething=+lava_guy_smoke typeflags=0x103F damage=50.000000 damageclass=0x80000000 fadetime=0.400000 blasttime=0.750000 babytime=0.300000 range=0.500000 spritething=+baz_spr_exp_fire spritestart=(0.150000/0.150000/1.000000) spriteend=(0.400000/0.400000/0.000000)
lava_guy          _actor            thingflags=0x8000401 light=(2.000000/0.000000/0.000000/0.500000) model3d=gen_lg.3do size=0.016000 movesize=0.016000 collwidth=0.015960 collheight=0.015960 puppet=lavaguy.pup soundclass=lavaguy.snd cog=actor_lavaguy.cog surfdrag=1.500000 staticdrag=0.010000 mass=30.000000 weapon=+lavahit_sm health=15.000000 maxhealth=15.000000 maxthrust=0.450000 maxrotthrust=200.000000 typeflags=0x1000120 explode=+lava_guy_fire aiclass=lavaguy.ai
lbossfntn         _particle         airdrag=2.000000 mass=0.500000 physflags=0x201 vel=(0.000000/0.000000/2.000000) typeflags=0x19 material=aet_4sprite_glow_fairy_dust.mat range=0.080000 rate=10.000000 maxthrust=5.000000 elementsize=0.032000 count=30
lbosssparksmd     lbossfntn         physflags=0x200 vel=(0.000000/0.000000/0.030000) range=0.060000 rate=2.000000 maxthrust=2.500000 elementsize=0.022000 count=15
lbosssparkslg     lbossfntn         mass=0.210000 vel=(0.000000/0.000000/0.600000) range=0.050000 rate=15.000000 maxthrust=12.000000 elementsize=0.080000 count=15
+gen_exp_lava_md  _explosion        light=(0.000000/0.000000/0.000000/1.000000) timer=0.500000 sprite=gen_explode3.spr soundclass=gen_explode.snd typeflags=0xB7 damage=150.000000 blasttime=0.400000 range=0.500000 debris=metalshrapa debris=metalshrapb debris=metalshrapc debris=metalshrapa debris=metalshrapb debris=metalshrapc debris=metalshrapa debris=metalshrapb debris=metalshrapc debris=metalshrapa debris=metalshrapb debris=metalshrapc debris=metalshrapa debris=metalshrapb debris=metalshrapc
mirrorstatue      _sstructmetal     model3d=teo_statue_mirror.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000
2x2doora          _staticstruct     thingflags=0xC1 light=(0.400000/0.300000/0.200000/0.001000) model3d=teo_door_2x2a.3do size=0.142222 movesize=0.142222 collwidth=0.142222 collheight=0.142222 soundclass=gen_stonedoor.snd
teobut            _decor            model3d=teo_button_halfm.3do size=0.024500 movesize=0.024500 collwidth=0.024500 collheight=0.024500 soundclass=gen_stonebutn.snd
shortpie          _staticstruct     model3d=teo_door_floorb.3do size=0.200390 movesize=0.200390 collwidth=0.200390 collheight=0.200390
longpie           _staticstruct     model3d=teo_door_floora.3do size=0.180710 movesize=0.180710 collwidth=0.180710 collheight=0.180710
geardoorb         _staticstruct     thingflags=0xC1 light=(0.400000/0.300000/0.200000/0.001000) model3d=teo_door_gearb.3do size=0.149000 movesize=0.149000 collwidth=0.149000 collheight=0.149000 soundclass=gen_stonedoor.snd
geardoorc         _staticstruct     thingflags=0xC1 light=(0.400000/0.300000/0.200000/0.001000) model3d=teo_door_gearc.3do size=0.180595 movesize=0.180595 collwidth=0.180595 collheight=0.180595 soundclass=gen_stonedoor.snd
geardoord         _staticstruct     thingflags=0xC1 light=(0.400000/0.300000/0.200000/0.001000) model3d=teo_door_geard.3do size=0.149000 movesize=0.149000 collwidth=0.149000 collheight=0.149000 soundclass=gen_stonedoor.snd
geardoora         _staticstruct     thingflags=0xC1 light=(0.400000/0.300000/0.200000/0.001000) model3d=teo_door_geara.3do size=0.149000 movesize=0.149000 collwidth=0.149000 collheight=0.149000 soundclass=gen_stonedoor.snd
pushgearc         _pushstruct1m     model3d=teo_gear_pushc.3do size=0.124701 movesize=0.124701 collwidth=0.124701 collheight=0.124701 soundclass=gen_stonedoor.snd
pushgearb         _pushstruct1m     model3d=teo_gear_pushb.3do size=0.124701 movesize=0.124701 collwidth=0.124701 collheight=0.124701 soundclass=gen_stonedoor.snd
pushgeara         _pushstruct1m     model3d=teo_gear_pusha.3do size=0.124701 movesize=0.124701 collwidth=0.124701 collheight=0.124701 soundclass=gen_stonedoor.snd
pushgeard         _pushstruct1m     model3d=teo_gear_pushd.3do size=0.124699 movesize=0.124699 collwidth=0.124699 collheight=0.124699 soundclass=gen_stonedoor.snd
snakecolumn       _pushstruct       model3d=teo_column_snake.3do size=0.187083 movesize=0.187083 collwidth=0.187083 collheight=0.187083
slntdoor          _staticstruct     thingflags=0xC1 light=(0.350000/0.350000/0.350000/0.001000) model3d=teo_door_ptch.3do size=0.180629 movesize=0.180629 collwidth=0.180629 collheight=0.180629 soundclass=gen_stonedoor.snd
mirrorcolumn      _staticstruct     model3d=teo_column_mirror.3do size=0.269258 movesize=0.269258 collwidth=0.269258 collheight=0.269258
crushrock         _staticstruct     model3d=teo_block_crush.3do size=0.172052 movesize=0.172052 collwidth=0.172052 collheight=0.172052
stone_plat        _staticstruct     model3d=teo_pltfrm_stone.3do size=0.292617 movesize=0.292617 collwidth=0.292617 collheight=0.060000 soundclass=gen_stonedoor.snd
gearstatue        _staticstruct     model3d=teo_gear_statue.3do size=0.158114 movesize=0.158114 collwidth=0.158114 collheight=0.158114
wood_clmn         _sstructwood      model3d=teo_clmn_wood.3do size=0.331662 movesize=0.331662 collwidth=0.331662 collheight=0.331662 soundclass=gen_stonedoor.snd
1x6_clmn          _staticstruct     model3d=teo_clmn_6m.3do size=0.308221 movesize=0.308221 collwidth=0.308221 collheight=0.308221 soundclass=gen_stonedoor.snd
iconbase          _staticstruct     model3d=teo_base_icon.3do size=0.165233 movesize=0.165233 collwidth=0.165233 collheight=0.165233 soundclass=gen_stonedoor.snd
teopullbut        _decor            model3d=teo_bttn_pull.3do size=0.042726 movesize=0.042726 collwidth=0.042726 collheight=0.042726 soundclass=gen_stonebutn_pull.snd
h2okey            _item             thingflags=0x400 model3d=teo_key_water.3do size=0.030000 movesize=0.030000 collwidth=0.020000 collheight=0.020000 cog=item_h2okey.cog height=0.030000
switch_gear       _decormetal       model3d=teo_switch_gear.3do size=0.062500 movesize=0.062500 collwidth=0.062500 collheight=0.062500 soundclass=gen_stonebutn.snd
surstat           _staticstruct     model3d=teo_statue_srndr.3do size=0.197800 movesize=0.197800 collwidth=0.197800 collheight=0.197800
switch_slot       _staticstruct     model3d=teo_switch_slot.3do size=0.058966 movesize=0.058966 collwidth=0.058966 collheight=0.058966 soundclass=gen_stonebutn.snd
dyblacklight      _ghostdecor       type=ghost thingflags=0x1 light=(0.000000/0.000000/0.000000/0.300000)
elvtrcolumn       _staticstruct     model3d=teo_column_elvtr.3do size=0.286534 movesize=0.286534 collwidth=0.286534 collheight=0.286534
h2olock           _staticstruct     thingflags=0x80 model3d=teo_hole_key.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
sunleft           _staticstruct     model3d=teo_door_sunl.3do size=0.300000 movesize=0.300000 collwidth=0.300000 collheight=0.300000
sunrite           _staticstruct     model3d=teo_door_sunr.3do size=0.300000 movesize=0.300000 collwidth=0.300000 collheight=0.300000
brdidolmove       _decor            model3d=teo_idol_bird.3do size=0.018500 movesize=0.018500 collwidth=0.018500 collheight=0.018500
fshidolmove       _decor            model3d=teo_idol_fish.3do size=0.022500 movesize=0.022500 collwidth=0.022500 collheight=0.022500
jagidolmove       _decor            model3d=teo_idol_jag.3do size=0.026000 movesize=0.026000 collwidth=0.026000 collheight=0.026000
doorpost          _sstructmetal     model3d=teo_door_post.3do size=0.125000 movesize=0.125000 collwidth=0.125000 collheight=0.125000
spetz_tokarev3    _ai_tokarev       model3d=com_1h_forc.3do soundclass=russki3.snd
spetz_tokarev4    _ai_tokarev       model3d=com_1h_forc.3do soundclass=russki4.snd
spetz_tokarev2    _ai_tokarev       model3d=com_1h_forc.3do soundclass=russki2.snd
spetz_tokarev     _ai_tokarev       model3d=com_1h_forc.3do soundclass=russki1.snd
_ai_shotgun       _ai_actor_fast    thingflags=0x28004400 size=0.037000 movesize=0.037000 puppet=com_2h.pup cog=actor_aishotgun.cog health=300.000000 maxhealth=300.000000 aiclass=com_2h.ai
spetz_shotgun5    _ai_shotgun       model3d=com_2h_forc.3do soundclass=russki5.snd
crocstrut         _swingstruct      model3d=vol_strut_croc.3do collwidth=0.100000 collheight=0.100000
ch1sun            _ghostdecor       thingflags=0x1 light=(1.000000/1.000000/1.000000/0.100000) model3d=teo_sunshaft_ch1.3do size=0.750000 movesize=0.750000 collwidth=0.750000 collheight=0.750000
ch2sun            _ghostdecor       thingflags=0x1 light=(1.000000/1.000000/1.000000/0.100000) model3d=teo_sunshaft_ch2.3do size=0.350000 movesize=0.350000 collwidth=0.350000 collheight=0.350000
ch3sun            _ghostdecor       thingflags=0x1 light=(1.000000/1.000000/1.000000/0.100000) model3d=teo_sunshaft_ch3.3do size=0.350000 movesize=0.350000 collwidth=0.350000 collheight=0.350000
ch4sun            _ghostdecor       thingflags=0x1 light=(1.000000/1.000000/1.000000/0.100000) model3d=teo_sunshaft_ch4.3do size=0.500000 movesize=0.500000 collwidth=0.500000 collheight=0.500000
ch5sun            _ghostdecor       thingflags=0x1 light=(1.000000/1.000000/1.000000/0.100000) model3d=teo_sunshaft_ch5.3do size=0.350000 movesize=0.350000 collwidth=0.350000 collheight=0.350000
mirrormove        _decor            model3d=teo_mirror.3do size=0.085037 movesize=0.085037 collwidth=0.085037 collheight=0.085037
+piranhabite      +bughit           damage=25.000000
piranha           fish              model3d=gen_pi.3do weapon=+piranhabite
teosunny          _staticstruct     thingflags=0xC1 light=(1.000000/1.000000/1.000000/0.100000) model3d=teo_shaft_cone.3do
spetz_mauser4     _ai_mauser        model3d=com_1h_forc.3do soundclass=russki4.snd
spetz_shotgun3    _ai_shotgun       model3d=com_2h_forc.3do soundclass=russki3.snd
spetz_mauser2     _ai_mauser        model3d=com_1h_forc.3do soundclass=russki2.snd
+snake_a_bite     +bughit           damage=41.625000 damageclass=0x100
snake_d           _snake            model3d=gen_sn_d.3do size=0.060000 movesize=0.060000 weapon=+snake_a_bite
h2okeymove        _decor            model3d=teo_key_water.3do size=0.030000 movesize=0.030000 collwidth=0.030000 collheight=0.030000
shotgun           _itemex           thingflags=0x401 model3d=weap_toz34_inv.3do collwidth=0.003000 collheight=0.003000 cog=item_shotgun.cog
eyecandy          _particle         mass=0.500000 physflags=0x201 material=aet_4sprite_glow_fairy_dust.mat range=0.250000 rate=18.000000 maxthrust=50.000000 elementsize=0.010000 count=30
fountainhead      _staticstruct     model3d=olv_fountain_head.3do size=0.698659 movesize=0.698659 collwidth=0.698659 collheight=0.698659
fountainjaw       _staticstruct     model3d=olv_fountain_jaw.3do size=0.209105 movesize=0.209105 collwidth=0.209105 collheight=0.209105
3mrock            _staticstruct     model3d=olv_rock_commona.3do size=0.298044 movesize=0.298044 collwidth=0.298044 collheight=0.298044
1mrock            _staticstruct     model3d=olv_rock_commonc.3do size=0.154300 movesize=0.154300 collwidth=0.154300 collheight=0.154300
lefty             _walkstruct       model3d=olv_statue_lefty.3do size=0.555398 movesize=0.555398 collwidth=0.555398 collheight=0.555398
snakebridge       _staticstruct     model3d=olv_bridge_snake.3do size=0.286881 movesize=0.286881 collwidth=0.286881 collheight=0.286881
keydoor           _walkstruct       model3d=olv_door_keystone.3do size=0.251247 movesize=0.251247 collwidth=0.251247 collheight=0.251247 soundclass=olv_stonedoor.snd
lion              _staticstruct     model3d=olv_statue_lion.3do size=0.350000 movesize=0.350000 collwidth=0.350000 collheight=0.350000
toungeguy         _staticstruct     model3d=olv_statue_tounge.3do size=0.465994 movesize=0.465994 collwidth=0.465994 collheight=0.465994
owly              _staticstruct     model3d=olv_statue_owl.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000
2mrock            _staticstruct     model3d=olv_rock_commonb.3do size=0.210132 movesize=0.210132 collwidth=0.210132 collheight=0.210132
halfmrock         _staticstruct     model3d=olv_rock_commond.3do size=0.203738 movesize=0.203738 collwidth=0.203738 collheight=0.203738
head              _staticstruct     model3d=olv_statue_head.3do size=0.391312 movesize=0.391312 collwidth=0.391312 collheight=0.391312
bat               _staticstruct     model3d=olv_statue_bat.3do size=0.415000 movesize=0.415000 collwidth=0.415000 collheight=0.415000
yucca             _ghostdecor       model3d=olv_shrub_yucca.3do size=0.037500 movesize=0.037500 collwidth=0.037500 collheight=0.037500
olvbut            _decor            model3d=olv_button_halfm.3do size=0.024500 movesize=0.024500 collwidth=0.024500 collheight=0.024500 soundclass=gen_stonebutn.snd
rockheadpath      _pushstruct       model3d=olv_rock_head.3do size=0.144376 movesize=0.144376 collwidth=0.144376 collheight=0.144376 puppet=default.pup
pushrocklp        _pushstruct2m     model3d=olv_rock_pushlp.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000
rockhead          _pushstruct       move=physics model3d=olv_rock_head.3do size=0.144376 movesize=0.144376 collwidth=0.144376 collheight=0.144376 puppet=default.pup
rockstrut         _swingstruct      model3d=olv_strut_rock.3do collwidth=0.100000 collheight=0.100000
snake_c           _snake            model3d=gen_sn_c.3do size=0.060000 movesize=0.060000 weapon=+snake_c_bite
+snake_b_bite     +bughit           damage=83.250000 damageclass=0x100
snake_a           _snake            model3d=gen_sn_a.3do size=0.060000 movesize=0.060000 weapon=+snake_b_bite
fallfloora        _staticstruct     model3d=olv_flr_brksqra.3do size=0.073739 movesize=0.073739 collwidth=0.073739 collheight=0.073739
fallfloorb        _staticstruct     model3d=olv_flr_brksqrb.3do size=0.077055 movesize=0.077055 collwidth=0.077055 collheight=0.077055
fallfloorc        _staticstruct     model3d=olv_flr_brksqrc.3do size=0.070089 movesize=0.070089 collwidth=0.070089 collheight=0.070089
fallfloord        _staticstruct     model3d=olv_flr_brksqrd.3do size=0.071894 movesize=0.071894 collwidth=0.071894 collheight=0.071894
snaketotem        _staticstruct     model3d=olv_totem_snake.3do size=0.501248 movesize=0.501248 collwidth=0.501248 collheight=0.501248
snake_b           _snake            model3d=gen_sn_b.3do size=0.060000 movesize=0.060000 weapon=+snake_b_bite
platformtotem     _staticstruct     model3d=olv_totem_platform.3do size=1.016120 movesize=1.016120 collwidth=1.016120 collheight=1.016120
quetzdoor         _staticstruct     model3d=olv_door_boss.3do size=0.707549 movesize=0.707549 collwidth=0.707549 collheight=0.707549 soundclass=olv_bossdoor.snd
joh               _ghostdecor       thingflags=0x1 light=(0.000000/0.100000/0.500000/0.600000) model3d=gen_jewel.3do size=0.017595 movesize=0.017595 collwidth=0.017595 collheight=0.017595
olvelvtrb         _staticstruct     model3d=olv_pltfrm_elvtr_b.3do size=0.143614 movesize=0.143614 collwidth=0.143614 collheight=0.143614
olvelvtra         _staticstruct     model3d=olv_pltfrm_elvtr_a.3do size=0.143614 movesize=0.143614 collwidth=0.143614 collheight=0.143614
olvelvtrc         _staticstruct     model3d=olv_pltfrm_elvtr_c.3do size=0.143614 movesize=0.143614 collwidth=0.143614 collheight=0.143614
olvelvtrd         _staticstruct     model3d=olv_pltfrm_elvtr_d.3do size=0.143614 movesize=0.143614 collwidth=0.143614 collheight=0.143614
olvspike          _staticstruct     collide=1 thingflags=0xC1 light=(0.200000/0.200000/0.200000/0.001000) model3d=olv_spike.3do size=0.020000 movesize=0.020000 collwidth=0.020000 collheight=0.020000
imp3move          _decor            model3d=imp3_levit_inv.3do size=0.015241 movesize=0.015241 collwidth=0.015241 collheight=0.015241
ropeindy          _cutactor         model3d=gen_in_rope.3do collwidth=0.020000 collheight=0.020000 puppet=default.pup
olvspikebot       _sstructmetal     collide=0 thingflags=0x4000C1 light=(0.200000/0.200000/0.200000/0.001000) model3d=olv_spike_bot.3do size=0.280000 movesize=0.280000 collwidth=0.280000 collheight=0.280000
olvplasma         _staticstruct     thingflags=0x80440 model3d=olv_plasma.3do cog=gen_plasmamat.cog
olvimpstnd        _staticstruct     model3d=olv_stand_imp.3do size=0.062500 movesize=0.062500 collwidth=0.062500 collheight=0.062500
+jaguarbite       +bughit           damage=50.000000
jaguar            _gen_ai_fast      thingflags=0x8000400 model3d=gen_jg.3do size=0.049765 movesize=0.049765 collwidth=0.049765 collheight=0.049765 puppet=jaguar.pup soundclass=jaguar.snd cog=actor_jaguar.cog surfdrag=4.500000 mass=200.000000 physflags=0x425F orientspeed=0.400000 weapon=+jaguarbite health=750.000000 maxhealth=750.000000 maxthrust=2.550000 typeflags=0x128001 aiclass=jaguar.ai
temple_jaguar     jaguar            surfdrag=6.000000 maxthrust=3.400000
khaki_submach2    _ai_submachine    model3d=com_2h_kaki.3do soundclass=russki2.snd
khaki_submach3    _ai_submachine    model3d=com_2h_kaki.3do soundclass=russki3.snd
khaki_simonov5    _ai_simonov       model3d=com_2h_kaki.3do soundclass=russki5.snd
khaki_mauser2     _ai_mauser        model3d=com_1h_kaki.3do soundclass=russki2.snd
khaki_simonov3    _ai_simonov       model3d=com_2h_kaki.3do soundclass=russki3.snd
khaki_shotgun3    _ai_shotgun       model3d=com_2h_kaki.3do soundclass=russki3.snd
khaki_mauser4     _ai_mauser        model3d=com_1h_kaki.3do soundclass=russki4.snd
khaki_grenade3    _ai_grenade       model3d=com_1h_kaki.3do soundclass=russki3.snd
olv_waterpour     _sprite           sprite=olv_waterpour.spr
olv_waterspray    _sprite           sprite=olv_waterspray.spr
h2odrain          _staticstruct     model3d=olv_water_drain.3do
waterfall         _ghostdecor       model3d=olv_water_fall.3do size=0.480000 movesize=0.480000 collwidth=0.480000 collheight=0.480000
longwaterfall     _ghostdecor       model3d=olv_water_fall_long.3do size=0.680000 movesize=0.680000 collwidth=0.680000 collheight=0.680000
stnshrapbs_nc_ns  _debrisnc         thingflags=0x1 light=(0.100000/0.100000/0.100000/1.000000) model3d=gen_dbre_stnwalbs.3do size=0.003125 movesize=0.003125 collwidth=0.003125 collheight=0.003125 angvel=(0.000000/0.000000/0.000000)
quetzfntn         _particle         airdrag=2.000000 mass=0.500000 physflags=0x201 vel=(0.000000/0.000000/2.000000) typeflags=0x40019 material=aet_4sprite_glow_fairy_dust.mat range=0.080000 rate=10.000000 maxthrust=5.000000 elementsize=0.032000 count=30
quetzsparkslg     quetzfntn         vel=(0.000000/0.000000/2.300000) range=0.050000 maxthrust=8.000000 elementsize=0.050000 count=40
snake_q           _snake            model3d=gen_sn_a.3do size=0.060000 movesize=0.060000 mass=25.000000 weapon=+snake_a_bite typeflags=0x30020000
+quetzhit         +bughit           vel=(0.000000/2.000000/0.000000) damage=250.000000 range=0.500000 force=0.500000
quetzalcoatl      _actor            thingflags=0x401 model3d=olv_qu_head.3do size=0.250000 movesize=0.250000 collwidth=0.250000 collheight=0.250000 puppet=quetzalcoatl.pup soundclass=quetz.snd cog=actor_quetzalcoatl.cog surfdrag=3.000000 staticdrag=0.001000 mass=1000.000000 height=0.126000 weapon=+quetzhit health=1000.000000 maxhealth=1000.000000 maxthrust=0.750000 maxrotthrust=115.000000 maxheadvel=100.000000 typeflags=0x201 eyeoffset=(0.000000/0.150000/0.200000) aiclass=quetzalcoatl.ai
quetz01           _actor            model3d=olv_qu_01.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000 physflags=0x0 typeflags=0x100C08
quetz02           quetz01           model3d=olv_qu_02.3do
quetz03           quetz01           model3d=olv_qu_03.3do
quetz04           quetz01           model3d=olv_qu_04.3do
quetz05           quetz01           model3d=olv_qu_05.3do
quetz06           quetz01           model3d=olv_qu_06.3do
quetz07           quetz01           model3d=olv_qu_07.3do
quetz08           quetz01           model3d=olv_qu_08.3do
quetz09           quetz01           model3d=olv_qu_09.3do
quetz10           quetz01           model3d=olv_qu_10.3do
quetz11           quetz01           model3d=olv_qu_11.3do
quetz12           quetz01           model3d=olv_qu_12.3do
quetz13           quetz01           model3d=olv_qu_13.3do
quetz14           quetz01           model3d=olv_qu_14.3do
quetz15           quetz01           model3d=olv_qu_15.3do
quetz16           quetz01           model3d=olv_qu_16.3do
quetz17           quetz01           model3d=olv_qu_17.3do
quetz18           quetz01           model3d=olv_qu_18.3do
quetz19           quetz01           model3d=olv_qu_19.3do
quetz20           quetz01           model3d=olv_qu_20.3do
quetz21           quetz01           model3d=olv_qu_21.3do
quetz22           quetz01           model3d=olv_qu_22.3do
quetz23           quetz01           model3d=olv_qu_23.3do
quetz24           quetz01           model3d=olv_qu_24.3do
quetz_tail        quetz01           model3d=olv_qu_tail.3do
+explode2_b       _explosion        light=(0.000000/0.000000/0.000000/1.000000) timer=1.500000 sprite=gen_explode2_b.spr soundclass=gen_explode.snd typeflags=0x17 blasttime=1.000000 range=0.500000
+explode4_b       _explosion        light=(0.000000/0.000000/0.000000/1.000000) timer=1.500000 sprite=gen_explode4_b.spr soundclass=gen_explode.snd typeflags=0x17 blasttime=1.000000 range=0.500000
qubita            _debris           model3d=olv_qu_dbrsa.3do size=0.012500 movesize=0.012500 collwidth=0.012500 collheight=0.012500
qubitb            _debris           model3d=olv_qu_dbrsb.3do size=0.003867 movesize=0.003867 collwidth=0.003867 collheight=0.003867
qubitc            _debris           model3d=olv_qu_dbrsc.3do size=0.024789 movesize=0.024789 collwidth=0.024789 collheight=0.024789
qubitd            _debris           model3d=olv_qu_dbrsd.3do size=0.003125 movesize=0.003125 collwidth=0.003125 collheight=0.003125
qubite            _debris           model3d=olv_qu_dbrse.3do size=0.012601 movesize=0.012601 collwidth=0.012601 collheight=0.012601
stoneshrapas      _debris           model3d=gen_dbre_stnwalas.3do size=0.003867 movesize=0.003867 collwidth=0.003867 collheight=0.003867
stoneshrapbs      _debris           model3d=gen_dbre_stnwalbs.3do size=0.003125 movesize=0.003125 collwidth=0.003125 collheight=0.003125
khaki_shotgun     _ai_shotgun       model3d=com_2h_kaki.3do soundclass=russki1.snd
khaki_submach     _ai_submachine    model3d=com_2h_kaki.3do soundclass=russki1.snd
smostack          _sstructmetal     model3d=sea_stck_smoke.3do size=0.339992 movesize=0.339992 collwidth=0.339992 collheight=0.339992
steamengine       _ghostdecor       move=physics model3d=sea_ngin_steam.3do size=0.224195 movesize=0.224195 collwidth=0.224195 collheight=0.224195 puppet=engine.pup
bannerc           _staticstruct     model3d=bab_banner_cccpc.3do size=0.090139 movesize=0.090139 collwidth=0.090139 collheight=0.090139
2mseacratenopush  _sstructwood      model3d=sea_crate_2m.3do size=0.173205 movesize=0.173205 collwidth=0.173205 collheight=0.173205
2mseacrate        _pstruct2mwood    model3d=sea_crate_2m.3do size=0.173205 movesize=0.173205 collwidth=0.173205 collheight=0.173205
smoketube         _sstructmetal     thingflags=0x4000C1 light=(0.250000/0.250000/0.250000/0.001000) model3d=sea_tube_smoke.3do size=0.415331 movesize=0.415331 collwidth=0.415331 collheight=0.415331
sealadder         _ghostdecor       model3d=sea_ladr_scape.3do size=0.200000 movesize=0.200000 collwidth=0.200000 collheight=0.200000
cabindoor         _sstructmetal     model3d=sea_door_cabin.3do size=0.125000 movesize=0.125000 collwidth=0.125000 collheight=0.125000
cabindoorf        _sstructmetal     model3d=sea_door_cabinf.3do size=0.125000 movesize=0.125000 collwidth=0.125000 collheight=0.125000
airvant           _sstructmetal     model3d=sea_vent_air.3do size=0.157381 movesize=0.157381 collwidth=0.157381 collheight=0.157381
tiedown           _decor            model3d=sea_tiedwn_norope.3do size=0.032500 movesize=0.032500 collwidth=0.032500 collheight=0.032500
wcagelight        _decormetal       model3d=sea_light_ycage.3do size=0.020000 movesize=0.020000 collwidth=0.020000 collheight=0.020000
radar             _sstructmetal     move=physics model3d=sea_radar_dish.3do size=0.269965 movesize=0.269965 collwidth=0.269965 collheight=0.269965 puppet=radar.pup
radioant          _sstructmetal     model3d=sea_radio_antna.3do size=0.499792 movesize=0.499792 collwidth=0.499792 collheight=0.499792
xmast             _sstructmetal     model3d=sea_mast_cross.3do size=1.227460 movesize=1.227460 collwidth=1.227460 collheight=1.227460
boathoist         _sstructmetal     model3d=sea_hoist_boat.3do size=0.332126 movesize=0.332126 collwidth=0.332126 collheight=0.332126
bunkbed           _sstructmetal     model3d=sea_bed_bunk.3do size=0.190000 movesize=0.190000 collwidth=0.187966 collheight=0.187966
captaindoor       _sstructwood      model3d=sea_door_captain.3do size=0.144942 movesize=0.144942 collwidth=0.144942 collheight=0.144942
gntrygrdrs        _sstructmetal     model3d=sea_gntry_grdrs.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
gntrybrdg         _sstructmetal     model3d=sea_gntry_brdg.3do size=0.057500 movesize=0.057500 collwidth=0.057500 collheight=0.057500
rcagelight        _decormetal       model3d=sea_light_rcage.3do size=0.020000 movesize=0.020000 collwidth=0.020000 collheight=0.020000
hoistcrank        _decormetal       model3d=sea_hoist_crnk.3do size=0.060126 movesize=0.060126 collwidth=0.060126 collheight=0.060126 puppet=default.pup
pumphous          _sstructmetal     thingflags=0x80 model3d=sea_pump_watr.3do size=0.107500 movesize=0.107500 collwidth=0.107500 collheight=0.107500 puppet=default.pup
sailor_tokarev4   _ai_tokarev       model3d=com_1h_sail.3do soundclass=russki4.snd
sailor_mauser3    _ai_mauser        model3d=com_1h_sail.3do soundclass=russki3.snd
sailor_simonov2   _ai_simonov       model3d=com_2h_sail.3do soundclass=russki2.snd
nh3mseacrate      _sstructwood      model3d=sea_crate_3m_nohang.3do size=0.269258 movesize=0.269258 collwidth=0.269258 collheight=0.269258
pupmmach          _sstructmetal     thingflags=0x80 model3d=sea_pump_machine.3do size=0.140404 movesize=0.140404 collwidth=0.140404 collheight=0.140404 puppet=default.pup
crane             _sstructmetal     model3d=sea_crane_gntry.3do size=0.053977 movesize=0.053977 collwidth=0.053977 collheight=0.053977
lnchboat          _sstructmetal     model3d=sea_lnch_boat.3do size=0.285074 movesize=0.285074 collwidth=0.285074 collheight=0.285074
sink              _staticstruct     model3d=sea_sink_jail.3do size=0.047037 movesize=0.047037 collwidth=0.047037 collheight=0.047037
gntrystrut        _sstructmetal     model3d=sea_gntry_strut.3do size=0.072035 movesize=0.072035 collwidth=0.072035 collheight=0.072035
machete_object    _decor            model3d=weap_machete_inv.3do size=0.002000 movesize=0.002000 collwidth=0.002000 collheight=0.002000
tblchart          _sstructmetal     model3d=sea_table_chart.3do size=0.105505 movesize=0.105505 collwidth=0.105505 collheight=0.105505
bedcapt           _sstructmetal     model3d=sea_bed_captain.3do size=0.127165 movesize=0.127165 collwidth=0.127165 collheight=0.127165
cntrlcrane        _wstructmetal     thingflags=0x80 model3d=sea_cntrl_crane.3do size=0.175241 movesize=0.175241 collwidth=0.175241 collheight=0.175241
xbut              _sstructmetal     model3d=sea_cntrl_xbut.3do size=0.042608 movesize=0.042608 collwidth=0.042608 collheight=0.042608
ybut              _sstructmetal     model3d=sea_cntrl_ybut.3do size=0.042608 movesize=0.042608 collwidth=0.042608 collheight=0.042608
zbut              _sstructmetal     model3d=sea_cntrl_zbut.3do size=0.042608 movesize=0.042608 collwidth=0.042608 collheight=0.042608
seabut            _decor            model3d=sea_button_halfm.3do size=0.012500 movesize=0.012500 collwidth=0.012500 collheight=0.012500
sailor_tokarev3   _ai_tokarev       model3d=com_1h_sail.3do soundclass=russki3.snd
imp1              _item             thingflags=0x400 model3d=imp1_vibrat_inv.3do collwidth=0.020000 collheight=0.020000 cog=item_imp1.cog height=0.006500
struthull         _sstructmetal     model3d=sea_strut_hull.3do size=0.180000 movesize=0.180000 collwidth=0.180000 collheight=0.180000
catwalk           _sstructmetal     model3d=sea_catwalk_ngin.3do size=1.100000 movesize=0.500000 collwidth=0.820503 collheight=0.820503
catpoles          _ghostdecor       thingflags=0x1 light=(0.250000/0.250000/0.250000/0.001000) model3d=sea_catwalk_ngina.3do size=0.911970 movesize=0.911970 collwidth=0.911970 collheight=0.911970
seafan            _sstructmetal     model3d=sea_fan_wall.3do size=0.025000 movesize=0.025000 collwidth=0.025000 collheight=0.025000 puppet=default.pup
anchor            _sstructmetal     model3d=sea_winch_anchor.3do size=0.449583 movesize=0.449583 collwidth=0.449583 collheight=0.449583
strutpost         _swingstruct      model3d=sea_strut_post.3do collwidth=0.100000 collheight=0.100000
grate             _sstructmetal     model3d=sea_grate_vent.3do size=0.140007 movesize=0.140007 collwidth=0.140007 collheight=0.140007
cratehead         _decorwood        model3d=sea_crate_head.3do size=0.115106 movesize=0.115106 collwidth=0.115106 collheight=0.115106
bruiser           _ai_actor_fast    thingflags=0x28004400 model3d=com_2g.3do size=0.050000 movesize=0.050000 puppet=com_2g.pup soundclass=russki2.snd cog=actor_aibruiser.cog mass=200.000000 health=400.000000 maxhealth=400.000000 eyeoffset=(0.000000/0.005000/0.088200) aiclass=com_1hbruiser.ai
gcagelight        _decormetal       model3d=sea_light_gcage.3do size=0.020000 movesize=0.020000 collwidth=0.020000 collheight=0.020000
lensflareg        _sprite           sprite=gen_lensflaregreen.spr
lensflarer        _sprite           sprite=gen_lensflarered.spr
la_luna           _sprite           sprite=sea_luna.spr
boatcrank         _item             thingflags=0x400 model3d=gen_icon_crnk.3do collwidth=0.020000 collheight=0.020000 cog=item_boatcrank.cog
racka             _decormetal       model3d=sea_rack_a.3do size=0.069282 movesize=0.069282 collwidth=0.069282 collheight=0.069282
whip_object       _decor            model3d=weap_whip_inv.3do size=0.028702 movesize=0.028702 collwidth=0.028702 collheight=0.028702
revolver_object   _decor            model3d=weap_revolver_inv.3do size=0.002390 movesize=0.002390 collwidth=0.002390 collheight=0.002390
radare            _decormetal       model3d=sea_radar_scope_e.3do size=0.065574 movesize=0.065574 collwidth=0.065574 collheight=0.065574
radara            _decormetal       model3d=sea_radar_scope_a.3do size=0.062386 movesize=0.062386 collwidth=0.062386 collheight=0.062386
radarb            _decormetal       model3d=sea_radar_scope_b.3do size=0.046368 movesize=0.046368 collwidth=0.046368 collheight=0.046368
radard            _decormetal       model3d=sea_radar_scope_d.3do size=0.051962 movesize=0.051962 collwidth=0.051962 collheight=0.051962
hatchcvr          _sstructmetal     model3d=sea_cover_hatch.3do size=0.155159 movesize=0.155159 collwidth=0.155159 collheight=0.155159
pull              _sstructmetal     model3d=sea_pulley.3do size=0.037819 movesize=0.037819 collwidth=0.037819 collheight=0.037819
boat_indy         _cutactor         thingflags=0x80000 model3d=gen_in_boat.3do collwidth=0.020000 collheight=0.020000 puppet=default.pup mass=500.000000 buoyancy=-1.000000 maxthrust=0.800000 maxrotthrust=70.000000
deckant           _sstructmetal     model3d=sea_antna_deck.3do size=0.400000 movesize=0.400000 collwidth=0.400000 collheight=0.400000
sailor_simonov    _ai_simonov       model3d=com_2h_sail.3do soundclass=russki1.snd
sailor_tokarev    _ai_tokarev       model3d=com_1h_sail.3do soundclass=russki1.snd voicecolor=(0.765000/0.608000/0.196000/1.000000/0.765000/0.608000/0.196000/1.000000/0.980000/1.000000/0.922000/1.000000/0.980000/1.000000/0.922000/1.000000)
sailor_tokarev5   _ai_tokarev       model3d=com_1h_sail.3do soundclass=russki5.snd
sailor_simonov4   _ai_simonov       model3d=com_2h_sail.3do soundclass=russki4.snd
flata             _staticstruct     model3d=sea_flat_a.3do size=0.065000 movesize=0.065000 collwidth=0.065000 collheight=0.065000
wallb             _staticstruct     model3d=riv_frame_wall_b.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
walld             _staticstruct     model3d=riv_frame_wall_d.3do size=0.062500 movesize=0.062500 collwidth=0.062500 collheight=0.062500
scope             _sstructmetal     model3d=sea_radar_scope.3do size=0.085000 movesize=0.085000 collwidth=0.085000 collheight=0.085000
imp3              _item             thingflags=0x400 model3d=imp3_levit_inv.3do collwidth=0.020000 collheight=0.020000 cog=item_imp3.cog height=0.008760
messtabl          _sstructwood      model3d=sea_table_mess.3do size=0.143351 movesize=0.143351 collwidth=0.143351 collheight=0.143351
foodshlf          _sstructmetal     model3d=sea_shelf_food.3do size=0.060260 movesize=0.060260 collwidth=0.060260 collheight=0.060260
jar               _staticstruct     model3d=sea_ration_jar.3do size=0.019209 movesize=0.019209 collwidth=0.019209 collheight=0.019209
boxo              _staticstruct     model3d=sea_ration_boxo.3do size=0.034821 movesize=0.034821 collwidth=0.034821 collheight=0.034821
can               _sstructmetal     model3d=sea_ration_can.3do size=0.018028 movesize=0.018028 collwidth=0.018028 collheight=0.018028
cereal            _staticstruct     model3d=sea_ration_cereal.3do size=0.031150 movesize=0.031150 collwidth=0.031150 collheight=0.031150
captain           _cutactor         model3d=sea_ca.3do collwidth=0.020000 collheight=0.020000 puppet=captain.pup
helm              _sstructwood      model3d=sea_wheel_steer.3do size=0.152135 movesize=0.152135 collwidth=0.152135 collheight=0.152135
speed             _sstructmetal     model3d=sea_telgrf_speed.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000
seastove          _sstructmetal     model3d=lag_stove.3do size=0.096140 movesize=0.096140 collwidth=0.096140 collheight=0.096140
cutbord           _sstructwood      model3d=lag_cut_board.3do size=0.052670 movesize=0.052670 collwidth=0.052670 collheight=0.052670
steerbase         _sstructmetal     model3d=sea_base_steer.3do size=0.056789 movesize=0.056789 collwidth=0.056789 collheight=0.056789
speedbase         _sstructmetal     model3d=sea_base_speed.3do size=0.028723 movesize=0.028723 collwidth=0.028723 collheight=0.028723
flatb             _staticstruct     model3d=sea_flat_b.3do size=0.082500 movesize=0.082500 collwidth=0.082500 collheight=0.082500
seaplasma         _staticstruct     thingflags=0x80440 model3d=sea_plasma.3do cog=gen_plasmamat.cog
1moil_ra          _sstructmetal     model3d=lag_drum_1moilra.3do size=0.061033 movesize=0.061033 collwidth=0.061033 collheight=0.061033
1moil_rb          _sstructmetal     model3d=lag_drum_1moilrb.3do size=0.061033 movesize=0.061033 collwidth=0.061033 collheight=0.061033
1moil_rc          _sstructmetal     model3d=lag_drum_1moilrc.3do size=0.061033 movesize=0.061033 collwidth=0.061033 collheight=0.061033
steamenvelope     _sstructblock     model3d=sea_ngin_steam_nvelop.3do size=0.703468 movesize=0.703468 collwidth=0.703468 collheight=0.703468
strutclamp        _sstructmetal     model3d=sea_strut_clamp.3do size=0.012500 movesize=0.012500 collwidth=0.012500 collheight=0.012500
bksnglgc          _sstructwood      model3d=sol_book_snglgc.3do size=0.025000 movesize=0.025000 collwidth=0.025000 collheight=0.025000
spctls            _decormetal       model3d=shs_spctcls.3do size=0.001713 movesize=0.001713 collwidth=0.001713 collheight=0.001713
lckrsml           _sstructmetal     model3d=sea_locker_small.3do size=0.075000 movesize=0.075000 collwidth=0.075000 collheight=0.075000
sailor_simonov3   _ai_simonov       model3d=com_2h_sail.3do soundclass=russki3.snd
sailor_mauser5    _ai_mauser        model3d=com_1h_sail.3do soundclass=russki5.snd
_ai_fists         _ai_actor_normal  thingflags=0x28004400 size=0.037000 movesize=0.037000 puppet=com_1h.pup cog=actor_aifists.cog health=150.000000 maxhealth=150.000000 aiclass=com_fists.ai
guard_fists3      _ai_fists         model3d=com_1h_gard.3do soundclass=russki3.snd
2mhangledge       _structblock      model3d=gen_2m_hangledge.3do size=0.136931 movesize=0.136931 collwidth=0.136931 collheight=0.136931
catrails          _structmetal      thingflags=0x400001 light=(0.250000/0.250000/0.250000/0.001000) model3d=sea_catwalk_rails.3do size=1.100000 movesize=0.500000 collwidth=0.836542 collheight=0.836542
boatcrankmove     _decor            model3d=gen_icon_crnk.3do size=0.040148 movesize=0.040148 collwidth=0.040148 collheight=0.040148 puppet=default.pup
imp2              _item             thingflags=0x400 model3d=imp2_invis_inv.3do collwidth=0.020000 collheight=0.020000 cog=item_imp2.cog height=0.013090
sailor_tokarev2   _ai_tokarev       model3d=com_1h_sail.3do soundclass=russki2.snd
sailor_simonov5   _ai_simonov       model3d=com_2h_sail.3do soundclass=russki5.snd
sailor_mauser     _ai_mauser        model3d=com_1h_sail.3do soundclass=russki1.snd
sailor_mauser2    _ai_mauser        model3d=com_1h_sail.3do soundclass=russki2.snd
sailor_mauser4    _ai_mauser        model3d=com_1h_sail.3do soundclass=russki4.snd
8mcolumnbrkb      _staticstruct     model3d=pyr_column_8mbrkb.3do size=0.237609 movesize=0.237609 collwidth=0.237609 collheight=0.237609
8mcolumnbrkc      _staticstruct     model3d=pyr_column_8mbrkc.3do size=0.098317 movesize=0.098317 collwidth=0.098317 collheight=0.098317
8mcolumn          _staticstruct     model3d=pyr_column_8m.3do size=0.311769 movesize=0.311769 collwidth=0.311769 collheight=0.311769
8mcolumnbrk       _staticstruct     model3d=pyr_column_8mbrk.3do size=0.138682 movesize=0.138682 collwidth=0.138682 collheight=0.138682
2mpyr_pushblock   _pushstruct2m     model3d=pyr_block_2x2push.3do size=0.169741 movesize=0.169741 collwidth=0.169741 collheight=0.169741
fboxwall          _sstructmetal     model3d=pyr_firebx_wall.3do size=0.134629 movesize=0.134629 collwidth=0.134629 collheight=0.134629 puppet=default.pup
fireboxprpl       _sstructmetal     model3d=pyr_firebx_lens_prpl.3do size=0.135386 movesize=0.135386 collwidth=0.135386 collheight=0.135386 puppet=default.pup
padlswtch         _sstructmetal     model3d=pyr_padl_swch.3do size=0.178483 movesize=0.178483 collwidth=0.178483 collheight=0.178483 puppet=default.pup
sparkle_purple    _sprite           sprite=gen_sparklepurple.spr
kindlingmove      _decor            model3d=pyr_kndlng_cone.3do size=0.057358 movesize=0.057358 collwidth=0.057358 collheight=0.057358
baseblock         _staticstruct     model3d=pyr_block_base.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000 soundclass=pyr_jackalbase.snd
oneyeblock        _staticstruct     model3d=pyr_block_cyclops.3do size=0.182277 movesize=0.182277 collwidth=0.182277 collheight=0.182277
gemeyemove        _decor            model3d=pyr_gem_eye.3do size=0.008000 movesize=0.008000 collwidth=0.008000 collheight=0.008000
fireboxgrn        _sstructmetal     model3d=pyr_firebx_lens_green.3do size=0.135386 movesize=0.135386 collwidth=0.135386 collheight=0.135386 puppet=default.pup
sparkle_green     _sprite           sprite=gen_sparklegreen.spr
firebox           _sstructmetal     model3d=pyr_firebx_lens.3do size=0.135362 movesize=0.135362 collwidth=0.135362 collheight=0.135362 puppet=default.pup
fireboxgld        _sstructmetal     model3d=pyr_firebx_lens_gold.3do size=0.135386 movesize=0.135386 collwidth=0.135386 collheight=0.135386 puppet=default.pup
elvplat           _sstructmetal     model3d=pyr_elvatr_pltfrm.3do size=0.142843 movesize=0.142843 collwidth=0.142843 collheight=0.142843
elvframe          _sstructmetal     model3d=pyr_elvatr_frame.3do size=0.703763 movesize=0.703763 collwidth=0.703763 collheight=0.703763
elvgears          _sstructmetal     model3d=pyr_elvatr_gears.3do size=0.131719 movesize=0.131719 collwidth=0.131719 collheight=0.131719
deslngin          _wstructmetal     model3d=pyr_ngin_desl.3do size=0.300000 movesize=0.300000 collwidth=0.300000 collheight=0.300000 puppet=default.pup
nginchainmove     _staticstruct     model3d=pyr_ngin_chain.3do size=0.069808 movesize=0.069808 collwidth=0.069808 collheight=0.069808
nginlever         _wstructmetal     model3d=pyr_ngin_lever.3do size=0.030000 movesize=0.030000 collwidth=0.030000 collheight=0.030000 puppet=default.pup
doorlock          _sstructmetal     model3d=pyr_door_lock.3do size=0.135000 movesize=0.135000 collwidth=0.135000 collheight=0.135000 soundclass=pyr_handlock.snd
wierddoor         _staticstruct     model3d=pyr_door.3do size=0.210089 movesize=0.210089 collwidth=0.210089 collheight=0.210089 soundclass=pyr_fancydoor.snd
sparkle_yellow    _sprite           sprite=gen_sparkleyellow.spr
sparkle_red       _sprite           sprite=gen_sparklered.spr
3m_tracks         _ghostdecor       model3d=sol_track_mine3m.3do size=0.005000 movesize=0.005000 collwidth=0.005000 collheight=0.005000
railbrk           _ghostdecor       model3d=pyr_rail_brk.3do
45dr_tracks       _ghostdecor       model3d=sol_track_mine45dr.3do size=0.005000 movesize=0.005000 collwidth=0.005000 collheight=0.005000
5m_tracks         _ghostdecor       model3d=sol_track_mine5m.3do size=0.005000 movesize=0.005000 collwidth=0.005000 collheight=0.005000
litepole          _sstructwood      model3d=pyr_light_pole.3do size=0.245000 movesize=0.245000 collwidth=0.245000 collheight=0.245000
15dl_tracks       _ghostdecor       model3d=sol_track_mine15dl.3do size=0.005000 movesize=0.005000 collwidth=0.005000 collheight=0.005000
10m_tracks        _ghostdecor       model3d=sol_track_mine10m.3do size=0.005000 movesize=0.005000 collwidth=0.005000 collheight=0.005000
rrtieflat         _ghostdecor       model3d=sol_rrtie_flat.3do
rrtie             _ghostdecor       model3d=sol_rrtie_mine.3do size=0.005000 movesize=0.005000 collwidth=0.005000 collheight=0.005000
torch             _staticstruct     model3d=pyr_torch.3do size=0.052500 movesize=0.052500 collwidth=0.052500 collheight=0.052500
pltspike          _staticstruct     model3d=pyr_spike_pltfrm.3do size=0.122860 movesize=0.122860 collwidth=0.122860 collheight=0.122860
bttnb             _staticstruct     model3d=pyr_buttn_pushb.3do size=0.061656 movesize=0.061656 collwidth=0.061656 collheight=0.061656 soundclass=gen_stonebutn.snd
tombdoor          _staticstruct     model3d=pyr_tombdoor.3do size=0.143614 movesize=0.143614 collwidth=0.143614 collheight=0.143614 soundclass=pyr_tombdoor.snd
sarc              _staticstruct     model3d=pyr_scfgs_tomb.3do size=0.159374 movesize=0.159374 collwidth=0.159374 collheight=0.159374
sarclid           _staticstruct     model3d=pyr_scfgs_lid.3do size=0.121244 movesize=0.121244 collwidth=0.121244 collheight=0.121244
tresl             _sstructmetal     model3d=pyr_tresl_car.3do size=0.108657 movesize=0.108657 collwidth=0.108657 collheight=0.108657
kid               _cutactor         thingflags=0x20084400 model3d=pyr_by.3do collwidth=0.020000 collheight=0.020000 puppet=kid.pup soundclass=kid.snd cog=actor_kid.cog eyeoffset=(0.000000/0.020400/0.052800) voicecolor=(0.000000/0.941000/0.784000/1.000000/0.000000/0.941000/0.784000/1.000000/0.902000/1.000000/1.000000/1.000000/0.902000/1.000000/1.000000/1.000000)
wallanternr       _ghostdecor       model3d=sol_light_wallr.3do
cardoorb          _sstructmetal     model3d=sol_door_carb.3do size=0.247803 movesize=0.247803 collwidth=0.247803 collheight=0.247803
cardoora          _sstructmetal     model3d=sol_door_cara.3do size=0.247803 movesize=0.247803 collwidth=0.247803 collheight=0.247803
15dr_tracks       _ghostdecor       model3d=sol_track_mine15dr.3do size=0.005000 movesize=0.005000 collwidth=0.005000 collheight=0.005000
minecar           _actor            move=none thingflags=0x4001 model3d=sol_car_mine.3do size=0.070000 movesize=0.070000 collwidth=0.136226 collheight=0.136226 typeflags=0x344D0C lightoffset=(0.000000/0.200000/0.000000) lightintensity=(0.427000/0.300000/0.350000)
minecar_brk       _wstructmetal     model3d=pyr_minecar_brk.3do size=0.136226 movesize=0.136226 collwidth=0.136226 collheight=0.136226
minecarwheelmove  _decor            model3d=pyr_minecar_whl.3do size=0.020000 movesize=0.020000 collwidth=0.020000 collheight=0.020000
sphinx            _staticstruct     model3d=pyr_sphinx_man.3do size=0.225976 movesize=0.225976 collwidth=0.225976 collheight=0.225976
track_pile        _sstructmetal     model3d=sol_track_pile.3do size=0.021794 movesize=0.021794 collwidth=0.021794 collheight=0.021794
45dl_tracks       _ghostdecor       model3d=sol_track_mine45dl.3do size=0.005000 movesize=0.005000 collwidth=0.005000 collheight=0.005000
45dl_brace        _ghostdecor       model3d=sol_brace_45dl.3do size=0.007500 movesize=0.007500 collwidth=0.007500 collheight=0.007500
15dl_brace        _ghostdecor       model3d=sol_brace_15dl.3do size=0.007500 movesize=0.007500 collwidth=0.007500 collheight=0.007500
2m_tracks         _ghostdecor       model3d=sol_track_mine2m.3do size=0.005000 movesize=0.005000 collwidth=0.005000 collheight=0.005000
1m_tracks         _ghostdecor       model3d=sol_track_mine1m.3do size=0.005000 movesize=0.005000 collwidth=0.005000 collheight=0.005000
hlfm_tracks       _ghostdecor       model3d=sol_track_minehlfm.3do size=0.005000 movesize=0.005000 collwidth=0.005000 collheight=0.005000
1hlfm_tracks      _ghostdecor       model3d=sol_track_mine1hlfm.3do size=0.005000 movesize=0.005000 collwidth=0.005000 collheight=0.005000
1hlfm_brace       _ghostdecor       model3d=sol_brace_1hlfm.3do size=0.007500 movesize=0.007500 collwidth=0.007500 collheight=0.007500
1m_brace          _ghostdecor       model3d=sol_brace_1m.3do size=0.007500 movesize=0.007500 collwidth=0.007500 collheight=0.007500
track_bundle      _sstructmetal     model3d=sol_track_bundle.3do size=0.166136 movesize=0.166136 collwidth=0.166136 collheight=0.166136
fountain          _sstructwood      model3d=pyr_fntn_bucket.3do size=0.269488 movesize=0.269488 collwidth=0.269488 collheight=0.269488
ttrstand          _sstructwood      model3d=pyr_teeter_stand.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
ttrstill          _sstructwood      model3d=pyr_teeter_still.3do size=0.096845 movesize=0.096845 collwidth=0.096845 collheight=0.096845
ttrfull           _sstructwood      model3d=pyr_teeter_full.3do size=0.135086 movesize=0.135086 collwidth=0.135086 collheight=0.135086 puppet=default.pup
ttrempty          _sstructwood      model3d=pyr_teeter_empty.3do size=0.103841 movesize=0.103841 collwidth=0.103841 collheight=0.103841 puppet=default.pup
water             _staticstruct     model3d=pyr_plane_water.3do
sumpdump          _ghostdecor       model3d=pyr_dump.3do size=0.069450 movesize=0.069450 collwidth=0.069450 collheight=0.069450
splashin          _ghostdecor       thingflags=0x1 light=(0.500000/0.500000/0.500000/0.050000) model3d=pyr_splash.3do
hintnumber49      _hintbase         userval=49.000000
hornsign          _structmetal      model3d=pyr_sign_hrnr.3do size=0.128099 movesize=0.128099 collwidth=0.128099 collheight=0.128099
ruinsb            _ghostdecor       model3d=pyr_ruins_b.3do size=0.135000 movesize=0.135000 collwidth=0.135000 collheight=0.135000
jeep_cs_pyr       _actor            thingflags=0x4401 model3d=gen_ij_pyr.3do size=0.040000 movesize=0.040000 collwidth=0.089662 collheight=0.089662 puppet=default.pup soundclass=jeep.snd cog=jeep_cutscene_pyr.cog surfdrag=3.000000 physflags=0x1004A5F maxvel=0.400000 maxthrust=0.350000 maxrotthrust=60.000000 jumpspeed=0.400000 eyeoffset=(0.000000/-0.032000/0.120000) lightoffset=(0.000000/0.200000/0.000000) lightintensity=(0.500000/0.500000/0.500000) aiclass=truck.ai
pyrhut            _wstructmetal     model3d=pyr_hut.3do size=0.514587 movesize=0.514587 collwidth=0.514587 collheight=0.514587
gen2mpush         _pushstruct2m     model3d=gen_crate_2x2push.3do size=0.173205 movesize=0.173205 collwidth=0.173205 collheight=0.173205
gen1mpush         _pstruct1mwood    model3d=gen_crate_1x1push.3do size=0.086172 movesize=0.086172 collwidth=0.086172 collheight=0.086172
hutwall           _wstructmetal     model3d=pyr_wall.3do size=0.250013 movesize=0.250013 collwidth=0.250013 collheight=0.250013
huthole           _wstructmetal     model3d=pyr_hole.3do size=0.250007 movesize=0.250007 collwidth=0.250007 collheight=0.250007
woodbucket        _item             thingflags=0x400 model3d=gen_bucket_wood.3do size=0.027500 movesize=0.027500 collwidth=0.020000 collheight=0.020000 cog=item_bucket.cog
hornskel          _staticstruct     model3d=pyr_hornersk.3do size=0.113822 movesize=0.113822 collwidth=0.113822 collheight=0.113822
watchmove         _decor            model3d=pyr_watch.3do size=0.001474 movesize=0.001474 collwidth=0.001474 collheight=0.001474
dbrisc            _walkstruct       model3d=pyr_dbre_blokc.3do size=0.099935 movesize=0.099935 collwidth=0.099935 collheight=0.099935
dbrisb            _walkstruct       model3d=pyr_dbre_blokb.3do size=0.088296 movesize=0.088296 collwidth=0.088296 collheight=0.088296
el_sol            _sprite           sprite=pyr_sol.spr
hutwin            _ghostdecor       model3d=pyr_hut_window.3do size=0.040300 movesize=0.040300 collwidth=0.040300 collheight=0.040300
dwnsarclid        _staticstruct     model3d=pyr_srcfgs_lid_b.3do size=0.121244 movesize=0.121244 collwidth=0.121244 collheight=0.121244
bbbrug            _staticstruct     model3d=gen_bbb_carpet.3do size=0.102916 movesize=0.102916 collwidth=0.102916 collheight=0.102916
bbbplacque        _staticstruct     model3d=gen_bbb_plak.3do size=0.104709 movesize=0.104709 collwidth=0.104709 collheight=0.104709
pyr_eyeblock      _staticstruct     model3d=pyr_block_eye.3do size=0.099000 movesize=0.099000 collwidth=0.099000 collheight=0.099000 soundclass=pyr_eyeblock.snd
2x2platform       _staticstruct     model3d=pyr_pltfrm_2x2m.3do size=0.141973 movesize=0.141973 collwidth=0.141973 collheight=0.141973
6msinkblock       _staticstruct     model3d=pyr_block_6msink.3do size=0.331662 movesize=0.331662 collwidth=0.331662 collheight=0.331662 soundclass=pyr_sinkblock.snd
4msinkblock       _staticstruct     model3d=pyr_block_4msink.3do size=0.244949 movesize=0.244949 collwidth=0.244949 collheight=0.244949 soundclass=pyr_sinkblock.snd
pyrplasma         _staticstruct     thingflags=0x80440 model3d=pyr_plasma_jewel.3do size=0.800000 movesize=0.800000 collwidth=0.800000 collheight=0.800000 cog=gen_plasmamat.cog
starrypltfrm      _staticstruct     model3d=pyr_pltfrm_2x2star.3do size=0.141973 movesize=0.141973 collwidth=0.141973 collheight=0.141973
crocstrutclimb    _climbstruct      model3d=vol_strut_croc.3do size=0.040000 movesize=0.040000 collwidth=0.040000 collheight=0.040000
2mpyr_pushblckhng _pushstruct2m     model3d=pyr_2x2pushb.3do size=0.169741 movesize=0.169741 collwidth=0.169741 collheight=0.169741
kindling          _decor            thingflags=0x400 model3d=pyr_kndlng_cone.3do size=0.048000 movesize=0.048000 collwidth=0.057358 collheight=0.057358 cog=item_kindling.cog
wallanterny       _ghostdecor       model3d=sol_light_wally.3do
45dr_brace        _ghostdecor       model3d=sol_brace_45dr.3do size=0.007500 movesize=0.007500 collwidth=0.007500 collheight=0.007500
trestle           _wstructwood      model3d=sol_tresl_mine.3do size=0.213102 movesize=0.213102 collwidth=0.213102 collheight=0.213102
stompblck         _staticstruct     model3d=pyr_stompblck.3do size=0.194454 movesize=0.194454 collwidth=0.194454 collheight=0.194454 soundclass=pyr_stomper.snd
+hyenabite        +bughit           damage=75.000000
hyena             _gen_ai_fast      thingflags=0x400 model3d=gen_hy.3do size=0.050000 movesize=0.050000 collwidth=0.052944 collheight=0.052944 puppet=hyena.pup soundclass=hyena.snd cog=actor_hyena.cog surfdrag=4.500000 airdrag=0.030000 mass=200.000000 physflags=0x425F orientspeed=0.400000 weapon=+hyenabite health=500.000000 maxhealth=500.000000 maxthrust=2.550000 typeflags=0x28001 eyeoffset=(0.000000/0.070000/0.025000) aiclass=hyena.ai
khaki_grenade     _ai_grenade       model3d=com_1h_kaki.3do soundclass=russki1.snd
snakesarc         _staticstruct     model3d=pyr_srcfgs_snake.3do size=0.151265 movesize=0.151265 collwidth=0.151265 collheight=0.151265 puppet=default.pup
ngininvisi        _structblock      model3d=pyr_ngin_invsbl.3do size=0.235173 movesize=0.235173 collwidth=0.235173 collheight=0.235173
trcks15dr         _ghostdecor       model3d=pyr_track_mine15dr.3do size=0.155483 movesize=0.155483 collwidth=0.155483 collheight=0.155483
rlrdtflt          _ghostdecor       model3d=pyr_rrtie_flat.3do size=0.127476 movesize=0.127476 collwidth=0.127476 collheight=0.127476
elvpad            _sstructmetal     model3d=pyr_elvatr_pad.3do size=0.170000 movesize=0.170000 collwidth=0.170000 collheight=0.170000
ruinsa            _ghostdecor       model3d=pyr_ruins_a.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000
ruinsc            _ghostdecor       model3d=pyr_ruins_c.3do size=0.197231 movesize=0.197231 collwidth=0.197231 collheight=0.197231
ruinse            _ghostdecor       model3d=pyr_ruins_e.3do size=0.172699 movesize=0.172699 collwidth=0.172699 collheight=0.172699
ruinsf            _ghostdecor       model3d=pyr_ruins_f.3do size=0.095000 movesize=0.095000 collwidth=0.095000 collheight=0.095000
ruinsg            _ghostdecor       model3d=pyr_ruins_g.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000
jeepstop          _structblock      thingflags=0x80D4 model3d=gen_invisiplate3x2.3do size=0.100000 movesize=0.100000 collwidth=0.100000 collheight=0.100000
collruing         _structblock      model3d=gen_ruinsg.3do size=0.326101 movesize=0.326101 collwidth=0.326101 collheight=0.326101
collruina         _structblock      model3d=gen_ruinsa.3do size=0.235381 movesize=0.235381 collwidth=0.235381 collheight=0.235381
jeeppost          _sstructwood      model3d=pyr_post_jeep.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000
jeepplayer        _indyactor        model3d=gen_ij.3do collwidth=0.190000 collheight=0.090000 puppet=indy_jeep.pup soundclass=indyjeep.snd cog=actor_indyjeep.cog physflags=0x4004A5F eyeoffset=(0.045400/0.025400/0.069500)
jeepplayerpyr     jeepplayer        model3d=gen_ij_pyr.3do soundclass=indyjeeppyr.snd
mineplayer        _indyactor        thingflags=0x20004401 model3d=gen_im.3do collwidth=0.098000 collheight=0.098000 puppet=indy_mine.pup soundclass=indymine.snd cog=actor_indymine.cog airdrag=0.250000 physflags=0x1004A5F maxvel=1.200000 orientspeed=0.250000 maxthrust=2.500000 maxrotthrust=4.000000 jumpspeed=0.300000 typeflags=0x5 eyeoffset=(0.008600/0.035500/0.078300) lightoffset=(0.000000/0.200000/0.000000) lightintensity=(0.500000/0.500000/0.500000)
fadeplatb         _structblock      thingflags=0x4 model3d=gen_fadeplate_b.3do size=0.300000 movesize=0.300000 collwidth=0.300000 collheight=0.300000
bazooka           _item             thingflags=0x400 model3d=weap_bazooka_inv.3do collwidth=0.020000 collheight=0.020000 cog=item_bazooka.cog height=0.005106
rockets           _item             thingflags=0x400 model3d=weap_rocket_pack.3do collwidth=0.020000 collheight=0.020000 cog=item_bazooka.cog height=0.006200
metalshrapa_nc    _debrisnc         model3d=gen_dbre_mtlwala.3do size=0.018717 movesize=0.018717 collwidth=0.018717 collheight=0.018717
metalshrapb_nc    _debrisnc         model3d=gen_dbre_mtlwalb.3do size=0.012500 movesize=0.012500 collwidth=0.012500 collheight=0.012500
metalshrapc_nc    _debrisnc         model3d=gen_dbre_mtlwalc.3do size=0.015166 movesize=0.015166 collwidth=0.015166 collheight=0.015166
+jeep_exp_blast   _explosion        timer=0.300000 soundclass=gen_explode.snd creatething=+bazooka_exp_debris typeflags=0xBF damage=1000.000000 blasttime=0.250000 babytime=0.009000 range=1.000000 debris=metalshrapa_nc debris=metalshrapb_nc debris=metalshrapc_nc debris=metalshrapa_nc debris=metalshrapb_nc debris=metalshrapc_nc debris=metalshrapa_nc debris=metalshrapb_nc debris=metalshrapc_nc spritething=+baz_spr_exp_blast spritestart=(0.075000/0.075000/1.000000) spriteend=(1.000000/1.000000/1.000000)
wreckjeep         _sstructmetal     move=physics model3d=jep_jeep_wreck.3do size=0.040000 movesize=0.040000 collwidth=0.071081 collheight=0.071081 surfdrag=10.000000 airdrag=1.000000 staticdrag=4.000000 mass=400.000000 physflags=0x41
minecarsparks     _particle         timer=0.050000 mass=0.500000 material=gen_a4sprite_rbblast.mat range=0.010000 rate=100.000000 maxthrust=5.000000 elementsize=0.008000 count=5
h2ofluefx         _particle         thingflags=0x8000000 airdrag=2.100000 mass=0.260000 physflags=0x201 material=gen_4sfx_particle_waterfall.mat range=0.040000 rate=15.000000 maxthrust=11.000000 elementsize=0.010000 count=50
commietruck       _actor            thingflags=0x400 model3d=com_tr.3do size=0.325000 movesize=0.325000 collwidth=0.150951 collheight=0.150951 puppet=com_truck.pup soundclass=truck.snd cog=actor_aivehicle.cog surfdrag=3.000000 staticdrag=0.300000 mass=5000.000000 height=0.157000 physflags=0x1004A5F maxvel=0.400000 maxthrust=0.700000 maxrotthrust=160.000000 jumpspeed=0.400000 typeflags=0x100008 eyeoffset=(0.000000/-0.270000/0.150000) aiclass=com_truck.ai
staticflatbed     _sstructmetal     model3d=gen_truck_flatbed.3do size=0.338933 movesize=0.338933 collwidth=0.338933 collheight=0.338933
2turnswitch       _sstructmetal     model3d=sol_switch_2turn.3do size=0.080000 movesize=0.080000 collwidth=0.080000 collheight=0.080000 soundclass=sol_turnswitch.snd
switchbase        _sstructmetal     model3d=sol_switch_base.3do size=0.131955 movesize=0.131955 collwidth=0.131955 collheight=0.131955
lantern           _ghostdecor       model3d=gen_lantern_lit.3do size=0.045000 movesize=0.045000 collwidth=0.045000 collheight=0.045000
15dr_brace        _ghostdecor       model3d=sol_brace_15dr.3do size=0.007500 movesize=0.007500 collwidth=0.007500 collheight=0.007500
25m_tracks        _ghostdecor       model3d=sol_track_mine25m.3do size=0.005000 movesize=0.005000 collwidth=0.005000 collheight=0.005000
45dl_track_cmbo   _ghostdecor       model3d=sol_track_cmbo45dl.3do size=0.005000 movesize=0.005000 collwidth=0.005000 collheight=0.005000
4turnswitch       _sstructmetal     model3d=sol_switch_4turn.3do size=0.080000 movesize=0.080000 collwidth=0.080000 collheight=0.080000 soundclass=sol_turnswitch2.snd
beams_bundle      _sstructwood      model3d=sol_beams_bundle.3do size=0.245359 movesize=0.245359 collwidth=0.245359 collheight=0.245359
1turnswitch       _sstructmetal     model3d=sol_switch_1turn.3do size=0.080000 movesize=0.080000 collwidth=0.080000 collheight=0.080000 soundclass=sol_turnswitch.snd
obstat            _staticstruct     model3d=sol_statue_oblsk.3do size=0.613375 movesize=0.613375 collwidth=0.613375 collheight=0.613375
gen2x1crate       _staticstruct     model3d=gen_crate_2x1.3do size=0.122474 movesize=0.122474 collwidth=0.122474 collheight=0.122474
fusebox           _sstructmetal     model3d=sol_box_fuse.3do size=0.167071 movesize=0.167071 collwidth=0.167071 collheight=0.167071
boxpipe           _sstructmetal     model3d=sol_box_pipe.3do size=0.104005 movesize=0.104005 collwidth=0.104005 collheight=0.104005
1stncl            _ghostdecor       model3d=sol_stncl_1nmbr.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
2stncl            _ghostdecor       model3d=sol_stncl_2nmbr.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
3stncl            _ghostdecor       model3d=sol_stncl_3nmbr.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
4stncl            _ghostdecor       model3d=sol_stncl_4nmbr.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
5stncl            _ghostdecor       model3d=sol_stncl_5nmbr.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
bkblokg           _sstructwood      model3d=sol_book_blokg.3do size=0.035791 movesize=0.035791 collwidth=0.035791 collheight=0.035791
door_cntrl        _sstructwood      model3d=sol_door_cntrl.3do size=0.149000 movesize=0.149000 collwidth=0.149000 collheight=0.149000 soundclass=sol_ctrlrmdoor.snd
panelswitch       _sstructmetal     model3d=sol_panel_switch.3do size=0.061751 movesize=0.061751 collwidth=0.061751 collheight=0.061751
opnsarc           _staticstruct     model3d=sol_srcfgs_open.3do size=0.060000 movesize=0.060000 collwidth=0.060000 collheight=0.060000
45dr_track_cmbo   _ghostdecor       model3d=sol_track_cmbo45dr.3do size=0.005000 movesize=0.005000 collwidth=0.005000 collheight=0.005000
10m_brace         _ghostdecor       model3d=sol_brace_10m.3do size=0.007500 movesize=0.007500 collwidth=0.007500 collheight=0.007500
ctrbeamtp         _wstructwood      model3d=sol_ctrroom_beamtp.3do size=0.100197 movesize=0.100197 collwidth=0.100197 collheight=0.100197
ramp              _sstructwood      model3d=sol_wramp.3do size=0.257694 movesize=0.257694 collwidth=0.257694 collheight=0.257694
ctrstps           _sstructwood      model3d=sol_ctrroom_steps.3do size=0.135208 movesize=0.135208 collwidth=0.135208 collheight=0.135208
ctrbeam           _sstructwood      model3d=sol_ctrroom_beam.3do size=0.112500 movesize=0.112500 collwidth=0.112500 collheight=0.112500
toolbarrel        _decorwood        model3d=sol_barrel_tools.3do size=0.045416 movesize=0.045416 collwidth=0.045416 collheight=0.045416
benttrack         _sstructwood      model3d=sol_track_bent.3do size=0.054436 movesize=0.054436 collwidth=0.054436 collheight=0.054436
30cmbarrel        _decorwood        model3d=sol_barrel_30cm.3do size=0.023374 movesize=0.023374 collwidth=0.023374 collheight=0.023374
5turnswitch       _sstructmetal     model3d=sol_switch_5turn.3do size=0.080000 movesize=0.080000 collwidth=0.080000 collheight=0.080000 soundclass=sol_turnswitch.snd
sign_dngr         _sstructwood      model3d=sol_sign_dngr.3do size=0.071281 movesize=0.071281 collwidth=0.071281 collheight=0.071281
sawhorse          _sstructwood      model3d=sol_sawhorse.3do size=0.058325 movesize=0.058325 collwidth=0.058325 collheight=0.058325
supcar            _wstructmetal     model3d=sol_car_supply.3do size=0.055038 movesize=0.055038 collwidth=0.055038 collheight=0.055038
solcrtlid         _decorwood        model3d=sol_crate_lid.3do size=0.043038 movesize=0.043038 collwidth=0.043038 collheight=0.043038
fusemove          _decor            model3d=sol_fuse_rplc.3do size=0.012375 movesize=0.012375 collwidth=0.012375 collheight=0.012375
crashedcar        _sstructmetal     model3d=sol_minecar_crash.3do size=0.067090 movesize=0.067090 collwidth=0.067090 collheight=0.067090
4mbord            _wstructwood      model3d=sol_bord_4m.3do size=0.201944 movesize=0.201944 collwidth=0.201944 collheight=0.201944
lampstrut         _swingstruct      model3d=sol_strut_lmp.3do collwidth=0.100000 collheight=0.100000
solvase           _staticstruct     model3d=sol_pot.3do size=0.054749 movesize=0.054749 collwidth=0.054749 collheight=0.054749
art1mpush         _pstruct1mwood    model3d=gen_crate_1x1art.3do size=0.086603 movesize=0.086603 collwidth=0.086603 collheight=0.086603
bucket            _decormetal       model3d=gen_bucket_metal.3do size=0.027011 movesize=0.027011 collwidth=0.027011 collheight=0.027011
mine_2commcar     _actor            thingflags=0x401 model3d=com_mc.3do size=0.040000 movesize=0.040000 collwidth=0.095771 collheight=0.095771 puppet=com_mine.pup soundclass=indymine.snd cog=actor_aivehicle.cog surfdrag=3.000000 physflags=0x1004A5F maxvel=0.400000 maxthrust=0.350000 maxrotthrust=160.000000 jumpspeed=0.400000 typeflags=0x100008 eyeoffset=(0.000000/-0.032000/0.120000) lightoffset=(0.000000/0.200000/0.000000) lightintensity=(0.500000/0.500000/0.500000) aiclass=com_minecar.ai
boxfuse           _sstructmetal     model3d=sol_box_w-fuse.3do size=0.167071 movesize=0.167071 collwidth=0.167071 collheight=0.167071
2mbord            _wstructwood      model3d=sol_bord_2m.3do size=0.103833 movesize=0.103833 collwidth=0.103833 collheight=0.103833
door_shed         _sstructwood      model3d=sol_door_shed.3do size=0.128914 movesize=0.128914 collwidth=0.128914 collheight=0.128914 soundclass=sol_oilshed.snd
pipe              _sstructmetal     model3d=sol_pipe_duck.3do size=0.014950 movesize=0.014950 collwidth=0.014950 collheight=0.014950
keyoil            _item             thingflags=0x400 model3d=sol_key_oil.3do size=0.021250 movesize=0.021250 collwidth=0.020000 collheight=0.020000 cog=item_oilkey.cog height=0.003200
frmgemb           _staticstruct     model3d=sol_frame_gemb.3do size=0.368273 movesize=0.368273 collwidth=0.368273 collheight=0.368273
frmgema           _staticstruct     model3d=sol_frame_gema.3do size=0.368273 movesize=0.368273 collwidth=0.368273 collheight=0.368273
frmgemc           _staticstruct     model3d=sol_frame_gemc.3do size=0.368273 movesize=0.368273 collwidth=0.368273 collheight=0.368273
door_brd          _staticstruct     model3d=sol_stdoor.3do size=0.187500 movesize=0.187500 collwidth=0.187500 collheight=0.187500 soundclass=sol_birddoor.snd
gemcmove          _decor            model3d=sol_gem_c.3do size=0.019081 movesize=0.019081 collwidth=0.019081 collheight=0.019081
gemamove          _decor            model3d=sol_gem_a.3do size=0.019081 movesize=0.019081 collwidth=0.019081 collheight=0.019081
gembmove          _decor            model3d=sol_gem_b.3do size=0.019081 movesize=0.019081 collwidth=0.019081 collheight=0.019081
scflda            _sstructmetal     model3d=sol_scfld_a.3do size=0.538517 movesize=0.538517 collwidth=0.538517 collheight=0.538517
king              _staticstruct     model3d=sol_statue_king.3do size=0.673925 movesize=0.673925 collwidth=0.673925 collheight=0.673925 puppet=default.pup
scfldprts         _wstructwood      model3d=sol_parts_scfld.3do size=0.235850 movesize=0.235850 collwidth=0.235850 collheight=0.235850
scfldf            _sstructmetal     model3d=sol_scfld_f.3do size=0.567891 movesize=0.567891 collwidth=0.567891 collheight=0.567891
pyrclmn           _staticstruct     model3d=sol_pymd_clmn.3do size=0.634537 movesize=0.634537 collwidth=0.634537 collheight=0.634537 soundclass=sol_column.snd
pyrim             _staticstruct     model3d=sol_pymd_rim.3do size=0.495606 movesize=0.495606 collwidth=0.495606 collheight=0.495606
pyrhle            _staticstruct     model3d=sol_pymd_hole.3do size=0.188746 movesize=0.188746 collwidth=0.188746 collheight=0.188746
solplasma         _staticstruct     thingflags=0x80440 model3d=sol_plasma.3do cog=gen_plasmamat.cog
khaki_shotgun4    _ai_shotgun       model3d=com_2h_kaki.3do soundclass=russki4.snd
khaki_simonov2    _ai_simonov       model3d=com_2h_kaki.3do soundclass=russki2.snd
trackend          _sstructwood      model3d=sol_trackend.3do size=0.135208 movesize=0.135208 collwidth=0.135208 collheight=0.135208
mcheadbonk        _structblock      thingflags=0x84D4 model3d=gen_invisiplate1m.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000 cog=class_mcheadbonk.cog
solspkpltfrm      _staticstruct     model3d=sol_spkpltfrm.3do size=0.012500 movesize=0.012500 collwidth=0.012500 collheight=0.012500
solclmn           _staticstruct     model3d=sol_clmn_nubn.3do size=0.311769 movesize=0.311769 collwidth=0.311769 collheight=0.311769
solclmnbrkb       _staticstruct     model3d=sol_clmn_nubnbrkb.3do size=0.237609 movesize=0.237609 collwidth=0.237609 collheight=0.237609
sol1mblck         _staticstruct     model3d=sol_block_1m.3do size=0.086603 movesize=0.086603 collwidth=0.086603 collheight=0.086603
bazooka_actor     _cutactor         thingflags=0x20084400 model3d=com_2h_kaki.3do collwidth=0.020000 collheight=0.020000 puppet=com_2h.pup cog=actor_aibazooka.cog
solclmnbrka       _staticstruct     model3d=sol_clmn_nubnbrka.3do size=0.138682 movesize=0.138682 collwidth=0.138682 collheight=0.138682
3turnswitch       _sstructmetal     model3d=sol_switch_3turn.3do size=0.080000 movesize=0.080000 collwidth=0.080000 collheight=0.080000 soundclass=sol_turnswitch.snd
gema              _item             thingflags=0x400 model3d=sol_gem_a.3do collwidth=0.020000 collheight=0.020000 cog=item_gema.cog height=0.019000
grainc            _ghostdecor       model3d=shw_sack_grainc.3do size=0.013424 movesize=0.013424 collwidth=0.013424 collheight=0.013424
graina            _ghostdecor       model3d=shw_sack_graina.3do size=0.044793 movesize=0.044793 collwidth=0.044793 collheight=0.044793
panelswitchb      _sstructmetal     model3d=sol_panel_switchb.3do size=0.290269 movesize=0.290269 collwidth=0.290269 collheight=0.290269
panelswitchc      _sstructmetal     model3d=sol_panel_switchc.3do size=0.207485 movesize=0.207485 collwidth=0.207485 collheight=0.207485
khaki_shotgun5    _ai_shotgun       model3d=com_2h_kaki.3do soundclass=russki5.snd
6sidecrate        _sstructwood      model3d=gen_crate_2x2_6sides.3do size=0.173205 movesize=0.173205 collwidth=0.173205 collheight=0.173205
brdstrt           _swingstruct      model3d=sol_strut_bird.3do collwidth=0.100000 collheight=0.100000
pyramidkey        _item             thingflags=0x400 model3d=sol_pymd_key.3do size=0.024500 movesize=0.024500 collwidth=0.020000 collheight=0.020000 cog=item_pyramidkey.cog height=0.024500
sphinxgold        _staticstruct     model3d=sol_sphinx.3do size=0.112988 movesize=0.112988 collwidth=0.112988 collheight=0.112988
strutwhip         _swingstruct      model3d=cyn_strut_whip.3do collwidth=0.100000 collheight=0.100000
invisiplat9m      _structblock      model3d=sol_invisiplate_9m.3do size=0.450694 movesize=0.450694 collwidth=0.450694 collheight=0.450694
invisiplat575m    _structblock      model3d=sol_invisiplate_575m.3do size=0.288585 movesize=0.288585 collwidth=0.288585 collheight=0.288585
khaki_grenade2    _ai_grenade       model3d=com_1h_kaki.3do soundclass=russki2.snd
khaki_grenade4    _ai_grenade       model3d=com_1h_kaki.3do soundclass=russki4.snd
khaki_submach5    _ai_submachine    model3d=com_2h_kaki.3do soundclass=russki5.snd
khaki_grenade5    _ai_grenade       model3d=com_1h_kaki.3do soundclass=russki5.snd
satchels          _item             thingflags=0x400 model3d=weap_satchel_pack.3do collwidth=0.020000 collheight=0.020000 cog=item_satchel.cog height=0.015500
kidgood           _decorwood        model3d=kidgood.3do size=0.012268 movesize=0.012268 collwidth=0.012268 collheight=0.012268
woodshrapa_nc     _debrisnc         model3d=gen_dbre_wodwala.3do size=0.015532 movesize=0.015532 collwidth=0.015532 collheight=0.015532
woodshrapb_nc     _debrisnc         model3d=gen_dbre_wodwalb.3do size=0.012500 movesize=0.012500 collwidth=0.012500 collheight=0.012500
woodshrapc_nc     _debrisnc         model3d=gen_dbre_wodwalc.3do size=0.013297 movesize=0.013297 collwidth=0.013297 collheight=0.013297
+gen_exp_stone    _explosion        light=(0.000000/0.000000/0.000000/1.000000) timer=0.500000 sprite=gen_explode3.spr soundclass=gen_explode.snd typeflags=0xB7 damage=1000.000000 blasttime=0.400000 range=0.500000 debris=stoneshrapa debris=stoneshrapb debris=stoneshrapc debris=stoneshrapa debris=stoneshrapb debris=stoneshrapc debris=stoneshrapa debris=stoneshrapb debris=stoneshrapc debris=stoneshrapa debris=stoneshrapb debris=stoneshrapc debris=stoneshrapa debris=stoneshrapb debris=stoneshrapc
oilcanmove        _decor            model3d=sol_oilcan_oz.3do size=0.027649 movesize=0.027649 collwidth=0.027649 collheight=0.027649
pyramidkeymove    _decor            model3d=sol_pymd_key.3do size=0.024500 movesize=0.024500 collwidth=0.024500 collheight=0.024500
plasmaj           _staticstruct     thingflags=0x80441 light=(0.000000/0.100000/0.500000/0.001000) model3d=gen_plasma_jewel.3do cog=gen_plasmamat.cog
pulleverbr        _sstructmetal     model3d=sol_lever_pull.3do size=0.053965 movesize=0.053965 collwidth=0.053965 collheight=0.053965 puppet=default.pup
pushleverbr       _sstructmetal     model3d=sol_lever_push.3do size=0.054097 movesize=0.054097 collwidth=0.054097 collheight=0.054097 puppet=default.pup
gemb              _item             thingflags=0x400 model3d=sol_gem_b.3do collwidth=0.020000 collheight=0.020000 cog=item_gemb.cog height=0.019000
khaki_shotgun2    _ai_shotgun       model3d=com_2h_kaki.3do soundclass=russki2.snd
2marpushstatue    _pushstruct       model3d=nub_block_mrdk.3do size=0.206155 movesize=0.100000 collwidth=0.206155 collheight=0.206155
2mnubpushblock    _pushstruct2m     model3d=nub_block_2x2push.3do size=0.173205 movesize=0.173205 collwidth=0.173205 collheight=0.173205
+lightningbolt    +bughit           thingflags=0x20000400 timer=0.360000 size=0.100000 movesize=0.050000 soundclass=weap_ailightning.snd cog=weap_ailightning.cog vel=(0.000000/5.000000/0.000000) damageclass=0x100000 range=5.000000
+robohit_lght     +lightningbolt    movesize=0.040000 damage=50.000000
+robot_exp        _explosion        timer=0.300000 soundclass=gen_explode.snd creatething=+bazooka_exp_debris typeflags=0x43F damage=200.000000 blasttime=0.250000 babytime=0.009000 range=0.250000 debris=metalshrapa debris=metalshrapb debris=metalshrapc debris=metalshrapa debris=metalshrapb debris=metalshrapc debris=metalshrapa debris=metalshrapb debris=metalshrapc debris=metalshrapa debris=metalshrapb debris=metalshrapc debris=metalshrapa debris=metalshrapb debris=metalshrapc spritething=+baz_spr_exp_blast spritestart=(0.075000/0.075000/1.000000) spriteend=(1.000000/1.000000/1.000000)
robot_guard       _flyactor         thingflags=0x8400400 model3d=nub_rg.3do size=0.150000 movesize=0.150000 collwidth=0.149803 collheight=0.149803 puppet=robotguard.pup soundclass=robotguard.snd cog=actor_robotguard.cog surfdrag=1.000000 airdrag=0.500000 staticdrag=0.010000 mass=100.000000 weapon=+robohit_lght health=500.000000 maxhealth=500.000000 maxthrust=0.700000 maxrotthrust=450.000000 typeflags=0x20120 eyeoffset=(0.000000/0.050000/-0.050000) fireoffset=(0.000000/0.036100/0.007900) explode=+robot_exp aiclass=robotguard.ai
4x8platform       _staticstruct     model3d=nub_pltfrm_4x8m.3do size=0.446572 movesize=0.446572 collwidth=0.446572 collheight=0.446572
turtplat          _sstructmetal     model3d=nub_elevator_platform.3do size=0.167439 movesize=0.167439 collwidth=0.167439 collheight=0.167439 soundclass=nub_elev.snd
_pstructmetal     _structure        thingflags=0x40400040
2mnubpushstatue   _pstructmetal     model3d=nub_block_statue.3do size=0.176139 movesize=0.176139 collwidth=0.176139 collheight=0.176139
broke4x8plat      _staticstruct     model3d=nub_pltfrm_4x8brk.3do size=0.406051 movesize=0.406051 collwidth=0.406051 collheight=0.406051
cow_statue        _sstructmetal     model3d=nub_statue_cow.3do size=0.692839 movesize=0.692839 collwidth=0.692839 collheight=0.692839
croc_machine      _sstructmetal     model3d=nub_cr.3do size=0.227882 movesize=0.227882 collwidth=0.227882 collheight=0.227882
croc_door         _sstructmetal     model3d=nub_cr_door.3do size=0.158137 movesize=0.158137 collwidth=0.158137 collheight=0.158137 soundclass=nub_crocgate.snd
labtower          _sstructmetal     model3d=nub_tower_lab.3do size=0.458258 movesize=0.458258 collwidth=0.458258 collheight=0.458258
4mpyramid         _sstructmetal     model3d=nub_pyramid_4x4m.3do size=0.372701 movesize=0.372701 collwidth=0.372701 collheight=0.372701
weight_elevator   _staticstruct     model3d=nub_weight_elvtr.3do size=0.250817 movesize=0.250817 collwidth=0.250817 collheight=0.250817 soundclass=nub_counterweight.snd
trapdoor          _sstructmetal     model3d=nub_door_trap.3do size=0.335410 movesize=0.335410 collwidth=0.335410 collheight=0.335410
stonedoor         _staticstruct     model3d=nub_door_stone.3do size=0.180629 movesize=0.180629 collwidth=0.180629 collheight=0.180629
horus             _sstructmetal     model3d=nub_horus.3do size=0.286930 movesize=0.286930 collwidth=0.143465 collheight=0.143465
aethdoor          _sstructmetal     model3d=nub_door_aetherium.3do size=0.235002 movesize=0.235002 collwidth=0.235002 collheight=0.235002
aethdoorarm       _sstructmetal     model3d=nub_door_aetharm.3do size=0.239423 movesize=0.239423 collwidth=0.239423 collheight=0.239423
dyblacklight2     _ghostdecor       type=ghost thingflags=0x1 light=(0.000000/0.000000/0.000000/0.400000)
anubis            _sstructmetal     model3d=nub_anubis.3do size=0.100490 movesize=0.100490 collwidth=0.100490 collheight=0.100490
anubisarmmove     _decor            model3d=nub_anubis_arm.3do size=0.074087 movesize=0.074087 collwidth=0.074087 collheight=0.074087
ledge             _sstructmetal     model3d=nub_ledge_chapel.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000
3mledge           _sstructmetal     model3d=nub_ledge_3mchpl.3do size=0.254951 movesize=0.254951 collwidth=0.254951 collheight=0.254951
brkledge          _sstructmetal     model3d=nub_ledge_broke.3do size=0.204229 movesize=0.204229 collwidth=0.204229 collheight=0.204229
nubbut            _decor            model3d=nub_button_halfm.3do size=0.042726 movesize=0.042726 collwidth=0.042726 collheight=0.042726 soundclass=gen_stonebutn.snd
wingframe         _sstructmetal     model3d=nub_frame_wings.3do size=0.200000 movesize=0.200000 collwidth=0.200000 collheight=0.200000
elevkey           _item             thingflags=0x400 model3d=nub_key_elvtr.3do collwidth=0.020000 collheight=0.020000 cog=item_elevatorkey.cog height=0.010500
birdstrut         _staticstruct     model3d=nub_strut_bird.3do size=0.020000 movesize=0.020000 collwidth=0.020000 collheight=0.020000
kyhole            _sstructmetal     model3d=nub_khole.3do size=0.024500 movesize=0.024500 collwidth=0.024500 collheight=0.024500
2mstep            _wstructmetal     model3d=nub_step_2m.3do size=0.114564 movesize=0.114564 collwidth=0.114564 collheight=0.114564
turtle            _sstructmetal     model3d=nub_engine_turtle.3do size=0.086510 movesize=0.086510 collwidth=0.086510 collheight=0.086510
turtlegearmove    _decor            model3d=nub_gear_turtle.3do size=0.028630 movesize=0.028630 collwidth=0.028630 collheight=0.028630
nubpullbut        _decor            model3d=nub_button_pull.3do size=0.024500 movesize=0.024500 collwidth=0.024500 collheight=0.024500 soundclass=gen_stonebutn_pull.snd
brdstrtclimb      _climbstruct      model3d=sol_strut_bird.3do size=0.035000 movesize=0.035000 collwidth=0.035000 collheight=0.035000
nubbgear          _sstructmetal     model3d=nub_gear_big.3do size=0.565000 movesize=0.565000 collwidth=0.565000 collheight=0.565000
nubmgear          _sstructmetal     model3d=nub_gear_med.3do size=0.140000 movesize=0.140000 collwidth=0.140000 collheight=0.140000
nubsgear          _sstructmetal     model3d=nub_gear_sml.3do size=0.057063 movesize=0.057063 collwidth=0.057063 collheight=0.057063
nubpost           _sstructmetal     model3d=nub_gear_pst.3do size=0.010000 movesize=0.010000 collwidth=0.010000 collheight=0.010000
nubdgear          _sstructmetal     model3d=nub_gear_drv.3do size=0.030000 movesize=0.030000 collwidth=0.030000 collheight=0.030000
taprod2           _ghostdecor       model3d=nub_taprod2m.3do size=0.010375 movesize=0.010375 collwidth=0.010375 collheight=0.010375
probeball         _decormetal       model3d=nub_probe_ball.3do size=0.075000 movesize=0.075000 collwidth=0.075000 collheight=0.075000 soundclass=nub_probe.snd
h2o_up            _staticstruct     thingflags=0x4C0 model3d=nub_water_rise.3do size=1.511622 movesize=1.511622 collwidth=1.511622 collheight=1.511622
robdoora          _sstructmetal     model3d=nub_door_robota.3do size=0.236617 movesize=0.236617 collwidth=0.236617 collheight=0.236617
robdoorb          _sstructmetal     model3d=nub_door_robotb.3do size=0.236617 movesize=0.236617 collwidth=0.236617 collheight=0.236617
elevator_robot    _sstructmetal     model3d=nub_elvtr_robot.3do size=0.319730 movesize=0.319730 collwidth=0.319730 collheight=0.319730
robstat           _sstructmetal     model3d=nub_rb_statue.3do size=0.337240 movesize=0.337240 collwidth=0.337240 collheight=0.337240
nubbutb           _decor            model3d=nub_button_halfmb.3do size=0.024500 movesize=0.024500 collwidth=0.024500 collheight=0.024500 soundclass=gen_stonebutn_pull.snd
imp4plug          _decor            model3d=imp4_energ_plug.3do size=0.025030 movesize=0.025030 collwidth=0.025030 collheight=0.025030
imp4              _item             thingflags=0x400 model3d=imp4_energ_inv.3do size=0.023850 movesize=0.023850 collwidth=0.020000 collheight=0.020000 cog=item_imp4.cog height=0.023850
ghost_actor       _cutactor         collide=0 thingflags=0x20004000 puppet=default.pup physflags=0x400000
bosseyemove       _decor            model3d=nub_rb_eye.3do size=0.028680 movesize=0.028680 collwidth=0.028680 collheight=0.028680
gate_elevator     _ghostdecor       model3d=nub_gate_elvatr.3do size=0.205010 movesize=0.205010 collwidth=0.205010 collheight=0.205010 puppet=default.pup
collgate_elev     _sstructmetal     model3d=nub_gate_elvatr_still.3do size=0.155000 movesize=0.155000 collwidth=0.155000 collheight=0.155000
sarcbox           _structblock      model3d=nub_invsbox_scfgs.3do size=0.159609 movesize=0.159609 collwidth=0.159609 collheight=0.159609
closesarc         _ghostdecor       model3d=nub_srcfgs_close.3do size=0.070997 movesize=0.070997 collwidth=0.070997 collheight=0.070997 puppet=default.pup
opensarc          _ghostdecor       model3d=nub_srcfgs_open.3do size=0.159609 movesize=0.159609 collwidth=0.159609 collheight=0.159609 puppet=default.pup
tetr_e            _walkstruct       model3d=nub_slab_tetrtotr_e.3do size=0.211024 movesize=0.211024 collwidth=0.211024 collheight=0.211024
tetr_d            _walkstruct       model3d=nub_slab_tetrtotr_d.3do size=0.211024 movesize=0.211024 collwidth=0.211024 collheight=0.211024
tetr_c            _walkstruct       model3d=nub_slab_tetrtotr_c.3do size=0.207666 movesize=0.207666 collwidth=0.207666 collheight=0.207666
tetr_b            _walkstruct       model3d=nub_slab_tetrtotr_b.3do size=0.211024 movesize=0.211024 collwidth=0.211024 collheight=0.211024
tetr_a            _walkstruct       model3d=nub_slab_tetrtotr_a.3do size=0.211024 movesize=0.211024 collwidth=0.211024 collheight=0.211024
gate_weight       _sstructmetal     model3d=nub_gate_weight.3do size=0.248307 movesize=0.248307 collwidth=0.248307 collheight=0.248307
nubbutpada        _decor            model3d=nub_button_padl_a.3do size=0.042726 movesize=0.042726 collwidth=0.042726 collheight=0.042726 soundclass=gen_stonebutn.snd
nubbutpadb        _decor            model3d=nub_button_padl_b.3do size=0.042726 movesize=0.042726 collwidth=0.042726 collheight=0.042726 soundclass=gen_stonebutn_pull.snd
sunny             _staticstruct     model3d=nub_shaft_sun.3do size=0.175000 movesize=0.175000 collwidth=0.175000 collheight=0.175000
slbtrap           _staticstruct     model3d=nub_slab_trap.3do size=0.133463 movesize=0.133463 collwidth=0.133463 collheight=0.133463
nubropeladder     _ghostdecor       model3d=0lad.3do size=0.149960 movesize=0.149960 collwidth=0.149960 collheight=0.149960 puppet=default.pup
anustat           _swingstruct      model3d=sol_as_strut.3do collwidth=0.100000 collheight=0.100000
+robohit_sc       +bughit           damage=150.000000
scissor_robot     _actor            thingflags=0x8400400 model3d=gen_r2.3do size=0.120000 movesize=0.120000 collwidth=0.080881 collheight=0.080881 puppet=robot_sc.pup soundclass=robot_sc.snd cog=actor_robotsc.cog surfdrag=2.500000 staticdrag=0.010000 mass=75.000000 weapon=+robohit_sc health=250.000000 maxhealth=250.000000 maxthrust=0.750000 maxrotthrust=300.000000 typeflags=0x20120 explode=+robot_exp aiclass=robot_sc.ai
throne            _sstructmetal     model3d=nub_throne.3do size=0.156052 movesize=0.156052 collwidth=0.156052 collheight=0.156052
+robohit_sm       +bughit           damage=100.000000
small_robot       _flyactor         thingflags=0x8400400 model3d=gen_ro.3do size=0.060000 movesize=0.060000 collwidth=0.055093 collheight=0.055093 puppet=robot_sm.pup soundclass=robot_sm.snd cog=actor_robotsm.cog surfdrag=2.500000 staticdrag=0.010000 mass=50.000000 weapon=+robohit_sm health=200.000000 maxhealth=200.000000 maxthrust=0.700000 maxrotthrust=300.000000 typeflags=0x20120 eyeoffset=(0.000000/0.000000/0.025000) explode=+robot_exp aiclass=robot_sm.ai
paddla            _sstructmetal     model3d=nub_padlplug_a.3do size=0.158430 movesize=0.158430 collwidth=0.158430 collheight=0.158430
paddlb            _sstructmetal     model3d=nub_padlplug_b.3do size=0.158430 movesize=0.158430 collwidth=0.158430 collheight=0.158430
hintnumber47      _hintbase         userval=47.000000
anubisarm         _decor            collide=3 move=physics thingflags=0x400 model3d=nub_anubis_arm.3do size=0.035000 movesize=0.035000 collwidth=0.074087 collheight=0.074087 cog=item_anubisarm.cog surfdrag=100.000000 airdrag=1.000000 mass=500.000000 height=0.020000 physflags=0x41
1x6ladder         _ghostdecor       model3d=nub_ladder_1x6.3do size=0.300000 movesize=0.300000 collwidth=0.300000 collheight=0.300000
blu               _staticstruct     model3d=pru_plane_water.3do
turtlegear        _item             thingflags=0x400 model3d=nub_gear_turtle.3do size=0.029200 movesize=0.029200 collwidth=0.020000 collheight=0.020000 cog=item_turtlegear.cog height=0.029200
elevkeymove       _decor            model3d=nub_key_elvtr.3do size=0.010500 movesize=0.010500 collwidth=0.010500 collheight=0.010500
bowdoor           _staticstruct     model3d=nub_door_bow.3do size=0.266927 movesize=0.266927 collwidth=0.266927 collheight=0.266927
voldnikov         _humanactor       collide=1 thingflags=0x28004400 model3d=gen_vo.3do size=0.035000 movesize=0.035000 puppet=volodnikov.pup soundclass=volodnikov.snd cog=actor_volodnikov.cog surfdrag=3.200000 airdrag=0.300000 staticdrag=0.001000 mass=200.000000 physflags=0xA4F weapon=+pistol maxthrust=0.520000 maxrotthrust=200.000000 typeflags=0x8118009 voicecolor=(0.314000/0.784000/0.157000/1.000000/0.314000/0.784000/0.157000/1.000000/1.000000/1.000000/0.196000/1.000000/1.000000/1.000000/0.196000/1.000000) aiclass=volodnikov.ai
volodnikov_nub    voldnikov         surfdrag=5.000000 maxvel=1.100000 maxthrust=1.100000 maxrotthrust=360.000000 typeflags=0x100009 aiclass=volodnikov_nub.ai
jungle_submach5   _ai_submachine    model3d=com_2h_jung.3do soundclass=russki5.snd
jungle_submach4   _ai_submachine    model3d=com_2h_jung.3do soundclass=russki4.snd
jungle_tokarev3   _ai_tokarev       model3d=com_1h_jung.3do soundclass=russki3.snd
jungle_submach    _ai_submachine    model3d=com_2h_jung.3do soundclass=russki1.snd
+robotglow_md     _sprite           thingflags=0x1 sprite=robotglow_md.spr
+shortelechit     _explosion        timer=0.699000 sprite=gen_shortlightninghit.spr typeflags=0x1
+robotglow_lg     _sprite           thingflags=0x1 sprite=robotglow_lg.spr
+robotglow_xsm    _sprite           thingflags=0x1 sprite=robotglow_xsm.spr
bullbubbles       _particle         thingflags=0x10000000 timer=4.500000 vel=(0.000000/0.000000/0.200000) angvel=(0.000000/30.000000/0.000000) material=gen_4sprite_bubble.mat range=0.030000 minsize=0.050000 elementsize=0.010000 count=5
rbossfntnlg       lbossfntn         material=nub_a4sfx_rboss_hit.mat elementsize=0.020000
rbosselecglow     _explosion        light=(1.000000/1.000000/1.000000/4.000000) timer=1.799000 sprite=nub_rbossarcglow.spr typeflags=0x2 blasttime=0.899000
rbosselecarc      _explosion        timer=0.699000 sprite=nub_rbossarcs.spr typeflags=0x33 blasttime=0.349000
+lightninghit     _explosion        timer=1.000000 sprite=gen_lightninghit.spr typeflags=0x1
+tower_glow       _sprite           timer=10.000000 sprite=nub_towerglow.spr
+robohit_lg       +bughit           damage=250.000000
robot_boss        _actor            thingflags=0x8400400 model3d=nub_rb.3do size=0.350000 movesize=0.350000 collwidth=0.350000 collheight=0.350000 puppet=robotboss.pup soundclass=robotboss.snd cog=actor_robotboss.cog surfdrag=1.000000 staticdrag=0.001000 mass=5000.000000 physflags=0xA4F weapon=+robohit_lg health=500.000000 maxhealth=500.000000 maxthrust=0.180000 maxrotthrust=150.000000 typeflags=0x9020100 eyeoffset=(0.000000/0.180000/0.150000) aiclass=robotboss.ai
+laserbolt        +bughit           thingflags=0x20000400 timer=1.000000 size=0.010000 movesize=0.008000 soundclass=weap_ailaser.snd cog=weap_ailaser.cog vel=(0.000000/5.000000/0.000000) damageclass=0x200000 range=5.000000
+robohit_lasr     +laserbolt        timer=0.500000 size=0.100000 movesize=0.010000 damage=125.000000
robodead          _sstructmetal     model3d=nub_rb_dead.3do size=0.342453 movesize=0.342453 collwidth=0.342453 collheight=0.342453
rbosssparks       _particle         timer=0.200000 airdrag=0.300000 mass=1.000000 typeflags=0x19 material=minecar_4sprite_front_lamp_lensflare.mat range=0.010000 rate=200.000000 maxthrust=80.000000 elementsize=0.014000 count=30
rbossfntnmd       _particle         timer=1.200000 airdrag=0.100000 mass=1.000000 typeflags=0x19 material=nub_a4sfx_rboss_hit.mat range=0.020000 rate=400.000000 maxthrust=25.000000 elementsize=0.016000 count=40
redflash          _sprite           sprite=gen_redflash.spr
+robot_blast      _explosion        light=(0.000000/0.000000/0.000000/1.000000) timer=0.550000 sprite=nub_robotblast.spr typeflags=0x17 blasttime=0.349000 range=0.500000
elvgearactor      _cutactor         thingflags=0x20084001 light=(0.600000/0.600000/0.600000/0.001000) model3d=inf_elvtr_gears.3do collwidth=0.020000 collheight=0.020000 puppet=default.pup physflags=0x3202
inf_probe         _sstructmetal     model3d=inf_probe_ball.3do size=0.287891 movesize=0.287891 collwidth=0.287891 collheight=0.287891
rmpgear           _sstructmetal     thingflags=0x4000C1 light=(0.500000/0.500000/0.500000/0.001000) model3d=inf_ramp_gear.3do size=0.474342 movesize=0.474342 collwidth=0.474342 collheight=0.474342 soundclass=inf_elev.snd
mtlpad            _sstructmetal     model3d=inf_pad_metal.3do size=0.127476 movesize=0.127476 collwidth=0.127476 collheight=0.127476
big6mdoor         _sstructmetal     thingflags=0x4000C1 light=(0.750000/0.750000/0.750000/0.001000) model3d=inf_door_6m.3do size=0.321025 movesize=0.321025 collwidth=0.321025 collheight=0.321025
inf_plat6m        _sstructmetal     model3d=inf_platform_6m.3do size=0.318443 movesize=0.318443 collwidth=0.318443 collheight=0.318443
inf_plat4m        _sstructmetal     model3d=inf_platform_4m.3do size=0.226729 movesize=0.226729 collwidth=0.226729 collheight=0.226729
mtlpadb           _sstructmetal     model3d=inf_pad_metal_b.3do size=0.127476 movesize=0.127476 collwidth=0.127476 collheight=0.127476
pstnrod           _sstructmetal     model3d=inf_piston_rod.3do size=0.215847 movesize=0.215847 collwidth=0.215847 collheight=0.215847
chap_plat         _sstructmetal     thingflags=0x4000C1 light=(0.500000/0.500000/0.500000/0.001000) model3d=inf_chapel_elvtr.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000 soundclass=inf_elev.snd
pie               _sstructmetal     model3d=inf_floor_pie.3do size=0.197067 movesize=0.197067 collwidth=0.197067 collheight=0.197067
mr_leggs          _structmetal      thingflags=0x400001 light=(1.000000/1.000000/1.000000/0.001000) model3d=inf_mr_legs.3do size=0.205945 movesize=0.205945 collwidth=0.205945 collheight=0.205945
inf_plata         _sstructmetal     model3d=inf_pltfrm_a.3do size=0.225000 movesize=0.225000 collwidth=0.225000 collheight=0.225000
inf_platc         _sstructmetal     model3d=inf_pltfrm_c.3do size=0.486756 movesize=0.486756 collwidth=0.486756 collheight=0.486756
crnrdoor          _sstructmetal     thingflags=0x4000C1 light=(0.350000/0.350000/0.350000/0.001000) model3d=inf_door_crnr.3do size=0.180836 movesize=0.180836 collwidth=0.180836 collheight=0.180836 soundclass=nub_elev.snd
levmach           _sstructmetal     model3d=inf_levers.3do size=0.065470 movesize=0.065470 collwidth=0.065470 collheight=0.065470 puppet=default.pup
cageoff           _cutactor         thingflags=0x80000 model3d=inf_cage_off.3do collwidth=0.020000 collheight=0.020000 puppet=default.pup airdrag=2.000000 mass=1.000000 physflags=0x3202 maxthrust=0.800000 maxrotthrust=200.000000
cage              _cutactor         thingflags=0x80000 model3d=inf_cage.3do collwidth=0.020000 collheight=0.020000 puppet=default.pup airdrag=2.000000 mass=1.000000 physflags=0x3202 maxthrust=0.800000 maxrotthrust=200.000000
impstand          _staticstruct     model3d=imp_stands.3do size=0.106913 movesize=0.106913 collwidth=0.106913 collheight=0.106913
imp4move          _decor            model3d=imp4_energ_inv.3do size=0.025488 movesize=0.025488 collwidth=0.025488 collheight=0.025488
mr_torso          _structmetal      thingflags=0x400001 light=(1.000000/1.000000/1.000000/0.001000) model3d=inf_mr_torso.3do size=0.056660 movesize=0.056660 collwidth=0.056660 collheight=0.056660
cofndoor          _sstructmetal     thingflags=0x4000C1 light=(0.350000/0.350000/0.350000/0.001000) model3d=inf_door_cofn.3do size=0.149250 movesize=0.149250 collwidth=0.149250 collheight=0.149250 soundclass=nub_elev.snd
infstrutball      _swingstruct      model3d=inf_strut_rotate.3do collwidth=0.100000 collheight=0.100000
gearspk           _sstructmetal     model3d=inf_gear_spoke.3do size=0.512348 movesize=0.512348 collwidth=0.512348 collheight=0.512348
antstrt           _swingstruct      model3d=inf_strut_antna.3do collwidth=0.100000 collheight=0.100000
infplasma         _staticstruct     thingflags=0x80440 model3d=inf_plasma.3do cog=gen_plasmamat.cog
roboheadmove      _decor            model3d=inf_robo_head_anim.3do size=0.041290 movesize=0.041290 collwidth=0.041290 collheight=0.041290 puppet=default.pup
inf_platjoh       _sstructmetal     model3d=inf_pltfrm_joh.3do size=0.256390 movesize=0.256390 collwidth=0.256390 collheight=0.256390
marduk_horse      _cutactor         thingflags=0x1 light=(1.000000/1.000000/1.000000/0.001000) model3d=inf_mr.3do size=0.125000 movesize=0.125000 collwidth=0.020000 collheight=0.020000 puppet=actor_mardukhorse.pup soundclass=mardukhorse.snd maxthrust=0.500000 maxrotthrust=100.000000
octodoor          _sstructmetal     thingflags=0x4000C1 light=(0.750000/0.750000/0.750000/0.001000) model3d=inf_door_octo.3do size=0.114564 movesize=0.114564 collwidth=0.114564 collheight=0.114564 soundclass=nub_elev.snd
infstruttri       _climbstruct      model3d=inf_strut_clamp.3do size=0.128087 movesize=0.128087 collwidth=0.128087 collheight=0.128087
turner            _humanactor       thingflags=0x28004400 model3d=gen_tu.3do puppet=turnerai.pup soundclass=turner.snd cog=actor_aiturner.cog airdrag=0.300000 health=1000.000000 maxthrust=0.650000 maxrotthrust=350.000000 voicecolor=(0.078000/0.471000/1.000000/1.000000/0.078000/0.471000/1.000000/1.000000/0.922000/1.000000/1.000000/1.000000/0.922000/1.000000/1.000000/1.000000) aiclass=turner.ai
infdown_button    _decor            thingflags=0x1 light=(0.750000/0.750000/0.750000/0.001000) model3d=bab_button_down.3do size=0.042726 movesize=0.042726 collwidth=0.042726 collheight=0.042726
infcall_button    _decor            thingflags=0x1 light=(0.750000/0.750000/0.750000/0.001000) model3d=inf_button_call.3do size=0.024500 movesize=0.024500 collwidth=0.024500 collheight=0.024500
infupbutton       _decor            thingflags=0x1 light=(0.750000/0.750000/0.750000/0.001000) model3d=bab_button_up.3do size=0.042726 movesize=0.042726 collwidth=0.042726 collheight=0.042726
clampy            _sstructmetal     model3d=inf_robot_stand.3do size=0.304791 movesize=0.304791 collwidth=0.304791 collheight=0.304791 puppet=default.pup
inftripod         _sstructmetal     model3d=inf_tripod_light.3do size=0.190267 movesize=0.190267 collwidth=0.190267 collheight=0.190267
rmpgearcap        _sstructmetal     model3d=inf_ramp_gear_cap.3do size=0.103078 movesize=0.103078 collwidth=0.103078 collheight=0.103078
+lightningfx      _explosion        timer=0.250000 sprite=gen_lightningfx.spr typeflags=0x1
+floaterhitaet    +lightningbolt    timer=0.200000 movesize=0.022000 creatething=+lightningfx damage=50.000000
+floater_exp      _explosion        light=(0.000000/0.000000/0.000000/1.000000) timer=0.800000 soundclass=floaterexplode.snd typeflags=0xB7 damage=300.000000 blasttime=0.400000 range=0.500000 spritething=+expfloaterspr spritestart=(0.500000/0.500000/1.000000) spriteend=(0.500000/0.500000/1.000000)
aetherium_float   _flyactor         thingflags=0x8400401 model3d=aet_fa.3do size=0.080000 movesize=0.080000 collwidth=0.049083 collheight=0.049083 puppet=floateraet.pup soundclass=floateraet.snd cog=actor_floateraet.cog staticdrag=0.010000 mass=50.000000 weapon=+floaterhitaet health=225.000000 maxhealth=225.000000 maxthrust=0.100000 typeflags=0x20120 fireoffset=(0.000000/0.038000/-0.076000) explode=+floater_exp aiclass=floater.ai
+floaterhitinf    +lightningbolt    timer=0.349000 movesize=0.020000 creatething=+lightningfx damage=50.000000
inf_float         aetherium_float   model3d=inf_fr.3do puppet=floaterinf.pup soundclass=floaterinf.snd cog=actor_floaterinf.cog weapon=+floaterhitinf health=150.000000 maxhealth=150.000000 fireoffset=(0.000000/0.000000/-0.025000)
+bladebughit      +bughit           damage=10.000000 damageclass=0x100
bladebug          _gen_ai_avrg      thingflags=0x8000400 model3d=gen_cb.3do size=0.027500 movesize=0.027500 collwidth=0.032210 collheight=0.032210 puppet=bug.pup soundclass=bladebug.snd cog=actor_bug.cog weapon=+bladebughit health=150.000000 maxhealth=150.000000 typeflags=0x80000001 eyeoffset=(0.000000/0.010000/0.000000) aiclass=bladebug.ai
roboprobe         _sstructmetal     model3d=inf_robo_probe.3do size=0.100000 movesize=0.100000 collwidth=0.100000 collheight=0.100000
robohead_actor    _cutactor         collide=0 thingflags=0x0 model3d=gen_box_hlfminvis.3do collwidth=0.020000 collheight=0.020000 typeflags=0x110088 voicecolor=(0.345000/0.671000/0.655000/1.000000/0.345000/0.671000/0.655000/1.000000/0.941000/1.000000/1.000000/1.000000/0.941000/1.000000/1.000000/1.000000)
+expfloaterspr    _sprite           timer=0.800000 sprite=aet_expfloater.spr
+robotglow_sm     _sprite           thingflags=0x1 sprite=robotglow_sm.spr
+lightningzap     _explosion        timer=1.000000 sprite=gen_lightningzap.spr typeflags=0x1
energywhirlpool   _particle         timer=30.000000 vel=(0.000000/0.000000/0.400000) angvel=(0.000000/180.000000/0.000000) material=aet_4sprite_glow_fairy_dust.mat range=0.450000 minsize=0.020000 rate=0.010000 maxthrust=10.000000 elementsize=0.010000 count=20
imp5move          _decor            model3d=imp5_dial_inv.3do size=0.022367 movesize=0.022367 collwidth=0.022367 collheight=0.022367
4mring            _staticstruct     model3d=aet_ring_4m.3do
reality           _staticstruct     model3d=aet_sphere_room.3do size=0.600000 movesize=0.600000 collwidth=0.610000 collheight=0.610000
daiscage          _sstructmetal     model3d=aet_dais_cage.3do size=0.219666 movesize=0.219666 collwidth=0.219666 collheight=0.219666
aet_purpleflash   _sprite           orient=(90.000000/0.000000/0.000000) sprite=aet_purpleflash.spr
brdggear          _sstructmetal     model3d=aet_bridge_gear.3do size=0.200000 movesize=0.200000 collwidth=0.200000 collheight=0.200000
brdgarm           _sstructmetal     model3d=aet_bridge_arm.3do size=1.945000 movesize=0.350000 collwidth=1.029942 collheight=1.029942 soundclass=aet_bridgearm.snd
dgem              _sstructmetal     model3d=aet_door_gem.3do size=0.285836 movesize=0.285836 collwidth=0.285836 collheight=0.285836 soundclass=aet_gemdoor.snd
realitysm         _staticstruct     model3d=aet_por_small.3do size=0.350000 movesize=0.350000 collwidth=0.330003 collheight=0.330003
brgbrokc          _staticstruct     model3d=bab_brdg_brkc.3do size=0.105909 movesize=0.105909 collwidth=0.105909 collheight=0.105909
clmne             _staticstruct     model3d=aet_clmn_e.3do size=0.270000 movesize=0.270000 collwidth=0.270000 collheight=0.270000
clmnd             _staticstruct     model3d=aet_clmn_d.3do size=0.320000 movesize=0.150000 collwidth=0.287228 collheight=0.287228
clmnc             _staticstruct     model3d=aet_clmn_c.3do size=0.206155 movesize=0.206155 collwidth=0.206155 collheight=0.206155
clmnb             _staticstruct     model3d=aet_clmn_b.3do size=0.231625 movesize=0.231625 collwidth=0.231625 collheight=0.231625
clmna             _staticstruct     model3d=aet_clmn_a.3do size=0.198997 movesize=0.198997 collwidth=0.198997 collheight=0.198997
aet_light         _sprite           orient=(90.000000/0.000000/0.000000) sprite=aet_light.spr
membrane          _staticstruct     model3d=aet_membrn_por.3do
aetplasma         _staticstruct     model3d=aet_plasma.3do
+glowbughit       +lightningbolt    movesize=0.020000 creatething=+lightningfx damage=50.000000
glowbug           _flyactor         thingflags=0x401 light=(0.850000/0.850000/0.550000) model3d=aet_gf.3do size=0.050000 movesize=0.050000 collwidth=0.055862 collheight=0.055862 puppet=glowbug.pup soundclass=glowbug.snd cog=actor_glowbug.cog staticdrag=0.010000 mass=50.000000 weapon=+glowbughit health=1000.000000 maxhealth=1000.000000 maxthrust=0.400000 maxrotthrust=300.000000 typeflags=0x20000 eyeoffset=(0.000000/0.025000/-0.013000) fireoffset=(0.000000/0.030000/-0.055862) aiclass=glowbug.ai
aetimpstnd        _sstructmetal     model3d=aet_stand_imp.3do size=0.062500 movesize=0.062500 collwidth=0.062500 collheight=0.062500
aet_probeb        _aimstruct        thingflags=0x80000008 model3d=aet_probe_b.3do size=0.188099 movesize=0.188099 collwidth=0.188099 collheight=0.188099 health=99999.000000 maxhealth=99999.000000 typeflags=0x100
aet_probe2m       _aimstruct        thingflags=0x80000008 model3d=aet_probe_2m.3do size=0.188099 movesize=0.188099 collwidth=0.188099 collheight=0.188099 health=99999.000000 maxhealth=99999.000000 typeflags=0x100
rckflrc           _staticstruct     model3d=aet_floor_c.3do
membranesm        _staticstruct     model3d=aet_membrn_small.3do size=0.109890 movesize=0.109890 collwidth=0.109890 collheight=0.109890
fltstage          _staticstruct     model3d=aet_stage_a.3do size=0.503090 movesize=0.503090 collwidth=0.503090 collheight=0.503090
wndwstage         _staticstruct     model3d=aet_stage_b.3do size=0.484526 movesize=0.484526 collwidth=0.484526 collheight=0.484526
drstage           _staticstruct     model3d=aet_stage_c.3do size=0.512348 movesize=0.512348 collwidth=0.512348 collheight=0.512348
sun               _ghostdecor       model3d=aet_sun.3do size=1.060660 movesize=1.060660 collwidth=1.060660 collheight=1.060660
squaredr          _sstructmetal     model3d=aet_door_sqr.3do size=0.282537 movesize=0.282537 collwidth=0.282537 collheight=0.282537 soundclass=aet_gemdoor.snd
mardukbombexp     _explosion        light=(1.000000/1.000000/1.000000/0.500000) timer=0.300000 soundclass=aet_mophiabombexp.snd typeflags=0xB7 damage=100.000000 blasttime=0.250000 range=0.400000 spritething=mardukbombspr spritestart=(0.500000/0.500000/1.000000) spriteend=(0.500000/0.500000/1.000000)
+mardukhit        +bughit           thingflags=0x20000401 light=(1.000000/1.000000/1.000000/0.400000) model3d=aet_ball_litng.3do size=0.030000 movesize=0.030000 collwidth=0.030000 collheight=0.030000 cog=weap_aibombmophia.cog vel=(0.000000/1.500000/0.000000) explode=mardukbombexp damage=200.000000 typeflags=0x80000D damageclass=0x100000
marduk            _flyactor         thingflags=0x401 model3d=aet_wm.3do size=0.050000 movesize=0.050000 collwidth=0.073339 collheight=0.073339 puppet=marduk.pup soundclass=marduk.snd cog=actor_marduk.cog surfdrag=1.000000 airdrag=4.500000 staticdrag=0.010000 mass=200.000000 weapon=+mardukhit health=500.000000 maxhealth=500.000000 maxthrust=3.500000 maxrotthrust=300.000000 typeflags=0x20001 eyeoffset=(0.000000/0.060000/0.030000) fireoffset=(0.000000/0.150000/0.000000) voicecolor=(0.345000/0.671000/0.655000/1.000000/0.345000/0.671000/0.655000/1.000000/0.941000/1.000000/1.000000/1.000000/0.941000/1.000000/1.000000/1.000000) aiclass=marduk.ai
sqflra            _staticstruct     model3d=aet_floor_a.3do
lincolnjersey     _staticstruct     model3d=aet_tunnel.3do
bttnkeya          _sstructmetal     model3d=aet_button_keya.3do size=0.042726 movesize=0.042726 collwidth=0.042726 collheight=0.042726 soundclass=aet_button.snd
bttnkeyb          _sstructmetal     model3d=aet_button_keyb.3do size=0.047083 movesize=0.047083 collwidth=0.047083 collheight=0.047083 soundclass=aet_button.snd
bladebugaet       _gen_ai_avrg      thingflags=0x8000401 light=(0.350000/0.350000/0.550000/1.000000) model3d=gen_sb.3do size=0.027500 movesize=0.027500 collwidth=0.032210 collheight=0.032210 puppet=bug.pup soundclass=bladebug.snd cog=actor_bugaet.cog weapon=+bladebughit health=200.000000 maxhealth=200.000000 typeflags=0x80000001 eyeoffset=(0.000000/0.010000/0.000000) aiclass=bladebug.ai
aetherealgem      _item             thingflags=0x401 light=(0.200000/0.200000/0.200000/0.001000) model3d=aet_gem04.3do collwidth=0.020000 collheight=0.020000 cog=item_gemaethereal.cog
daiscageb         _sstructmetal     model3d=aet_dais_cage_b.3do size=0.236093 movesize=0.236093 collwidth=0.236093 collheight=0.236093
rckstrip          _staticstruct     model3d=aet_floor_d.3do
brckstrip         _staticstruct     model3d=aet_floor_e.3do size=0.206155 movesize=0.206155 collwidth=0.206155 collheight=0.206155
brckflrb          _staticstruct     model3d=aet_floor_b.3do size=0.282843 movesize=0.282843 collwidth=0.282843 collheight=0.282843
flatoff           _sstructmetal     model3d=bab_truck_flatoff.3do size=0.352527 movesize=0.352527 collwidth=0.352527 collheight=0.352527
triwedge          _sstructmetal     model3d=aet_wedge.3do size=0.316476 movesize=0.316476 collwidth=0.316476 collheight=0.316476
brbrchr           _sstructmetal     model3d=guy_barberchair.3do size=0.085178 movesize=0.085178 collwidth=0.085178 collheight=0.085178
invisifloor2m     _structblock      model3d=gen_invisifloor2m.3do size=0.141421 movesize=0.141421 collwidth=0.141421 collheight=0.141421
realityb          _staticstruct     model3d=aet_sphere_roomb.3do size=0.640000 movesize=0.640000 collwidth=0.610000 collheight=0.610000
aetbttn           _sstructmetal     model3d=aet_button_new.3do size=0.024500 movesize=0.024500 collwidth=0.024500 collheight=0.024500 soundclass=aet_button.snd
aet_probe         _aimstruct        thingflags=0x80000008 model3d=aet_probe_ball.3do size=1.300000 movesize=1.300000 collwidth=1.300000 collheight=1.300000 health=99999.000000 maxhealth=99999.000000 typeflags=0x100
porfloor          _staticstruct     model3d=aet_pocket_plane.3do size=0.282843 movesize=0.282843 collwidth=0.282843 collheight=0.282843
indyvolod         _cutactor         model3d=gen_invo.3do collwidth=0.020000 collheight=0.020000 puppet=default.pup
floorthing        _ghostdecor       model3d=aet_floor_thing.3do
trsrchst          _staticstruct     model3d=aet_treasure_chest.3do size=0.018747 movesize=0.018747 collwidth=0.018747 collheight=0.018747
bugsparks         _particle         timer=2.000000 mass=0.500000 physflags=0x2200 material=aet_4sprite_glow_fairy_dust.mat range=0.020000 rate=18.000000 maxthrust=10.000000 elementsize=0.015000 count=2
marduktrail       _particle         timer=0.750000 airdrag=1.000000 mass=1.000000 vel=(0.000000/0.000000/-0.150000) angvel=(0.000000/90.000000/0.000000) typeflags=0x19 material=gen_4sprite_marduck_dandruff.mat range=0.040000 rate=200.000000 maxthrust=1.000000 elementsize=0.010000 count=2
mardukbombspr     _sprite           timer=0.300000 sprite=aet_mophiaexp.spr
mophiabombfx      _explosion        timer=0.500000 spritething=mophiabombemit
mophiasparkslg    _particle         airdrag=5.000000 mass=0.200000 physflags=0x201 vel=(0.000000/0.000000/0.600000) typeflags=0x19 material=gen_a4sprite_flash_purple.mat range=0.050000 rate=15.000000 maxthrust=11.000000 elementsize=0.200000 count=15
mardukhittrail    _particle         timer=2.000000 airdrag=3.000000 mass=1.000000 physflags=0x40001 vel=(0.000000/0.000000/0.450000) typeflags=0x19 material=gen_a4sprite_flash_purple.mat range=0.025000 rate=1000.000000 maxthrust=0.500000 elementsize=0.020000 count=6
mardukhitsparks   _particle         airdrag=5.000000 mass=0.200000 physflags=0x201 vel=(0.000000/0.000000/0.400000) typeflags=0x19 material=gen_a4sprite_flash_purple.mat range=0.040000 rate=12.000000 maxthrust=7.000000 elementsize=0.150000 count=10
aet_flash         _sprite           sprite=aet_flash.spr
+whip_spr_flash   _sprite           timer=0.200000 sprite=whip_flash.spr
+whip_spr_rays    _sprite           timer=0.349000 sprite=whip_rays.spr
+whip_spr_ring    _sprite           timer=0.349000 sprite=whip_ring.spr
+mophiahit        +bughit           damage=150.000000
mophia            _flyactor         thingflags=0x401 model3d=aet_sm.3do size=0.280000 movesize=0.280000 collwidth=0.280000 collheight=0.280000 puppet=mophia.pup soundclass=mophia.snd cog=actor_mophia.cog surfdrag=1.000000 airdrag=4.500000 staticdrag=0.010000 mass=200.000000 weapon=+mophiahit health=500.000000 maxhealth=500.000000 maxthrust=3.000000 maxrotthrust=250.000000 typeflags=0x8000001 eyeoffset=(0.000000/0.056000/0.220000) aiclass=mophia.ai
mophia_med        mophia            model3d=aet_sm_70.3do size=0.200000 movesize=0.200000 collwidth=0.200000 collheight=0.200000 puppet=mophia_med.pup eyeoffset=(0.000000/0.042000/0.155000) aiclass=mophia_med.ai
mophia_sml        mophia            model3d=aet_sm_45.3do size=0.128000 movesize=0.128000 collwidth=0.128000 collheight=0.128000 puppet=mophia_sml.pup eyeoffset=(0.000000/0.030000/0.100000) aiclass=mophia_sml.ai
mophia_xsml       mophia            model3d=aet_sm_25.3do size=0.082000 movesize=0.082000 collwidth=0.082000 collheight=0.082000 puppet=mophia_xsml.pup eyeoffset=(0.000000/0.010000/0.050000) aiclass=mophia_xsml.ai
mardukcharge      _particle         airdrag=3.200000 mass=1.000000 physflags=0x201 vel=(0.000000/0.000000/1.200000) typeflags=0x40019 material=gen_a4sprite_flash_purple.mat range=0.070000 rate=10.000000 maxthrust=3.000000 elementsize=0.040000 count=40
mardukkeymove     _decor            model3d=aet_marduk_key.3do size=0.008336 movesize=0.008336 collwidth=0.008336 collheight=0.008336
mardukkey         _item             thingflags=0x400 model3d=aet_marduk_key.3do collwidth=0.020000 collheight=0.020000 cog=item_mardukkey.cog height=0.008500
marduk2           _flyactor         thingflags=0x401 model3d=aet_wm.3do size=0.100000 movesize=0.100000 collwidth=0.150000 collheight=0.100000 puppet=marduk.pup soundclass=marduk.snd cog=actor_marduk2.cog surfdrag=1.000000 airdrag=4.500000 staticdrag=0.010000 mass=200.000000 weapon=+mardukhit health=500.000000 maxhealth=500.000000 maxthrust=3.500000 maxrotthrust=300.000000 typeflags=0x20001 eyeoffset=(0.000000/0.060000/0.030000) fireoffset=(0.000000/0.150000/0.000000) voicecolor=(0.345000/0.671000/0.655000/1.000000/0.345000/0.671000/0.655000/1.000000/0.941000/1.000000/1.000000/1.000000/0.941000/1.000000/1.000000/1.000000) aiclass=marduk2.ai
+mophiabeam       +laserbolt        timer=0.649000 cog=weap_ailasermophia.cog vel=(0.000000/14.000000/0.000000) damage=100.000000
mophiabombexp     _explosion        light=(0.000000/0.000000/0.000000/1.000000) timer=0.300000 soundclass=aet_mophiabombexp.snd typeflags=0xB7 damage=150.000000 blasttime=0.250000 range=0.400000 spritething=mophiabombspr spritestart=(0.500000/0.500000/1.000000) spriteend=(0.500000/0.500000/1.000000)
+mophiabomb       _weapon           thingflags=0x20000400 model3d=aet_ball_litng.3do size=0.030000 movesize=0.030000 collwidth=0.030000 collheight=0.030000 soundclass=aet_mophiabomb.snd cog=weap_aibombmophia.cog surfdrag=3.000000 airdrag=1.800000 mass=1.000000 height=0.010000 physflags=0x1 vel=(0.000000/8.500000/0.200000) buoyancy=0.250000 explode=mophiabombexp damage=10.000000 typeflags=0x20000C
mophiaglow        _sprite           thingflags=0x1 sprite=aet_mophiaglow.spr
mophiaglowsm      mophiaglow        sprite=aet_mophiaglowsm.spr
mophiaglowxsm     mophiaglow        sprite=aet_mophiaglowxsm.spr
mophiasparks      _particle         mass=0.200000 vel=(0.000000/-0.050000/0.000000) typeflags=0x19 material=gen_a4sprite_flash_purple.mat range=0.300000 rate=3.000000 maxthrust=0.350000 elementsize=0.040000 count=8
mophiasparkssm    _particle         mass=0.200000 vel=(0.000000/-0.025000/0.000000) typeflags=0x19 material=gen_a4sprite_flash_purple.mat range=0.100000 rate=3.000000 maxthrust=0.250000 elementsize=0.020000 count=6
mophiahurtexp     _particle         timer=0.899000 airdrag=2.000000 mass=0.500000 vel=(0.000000/0.000000/1.200000) typeflags=0x19 material=nub_a4sfx_rboss_hit.mat range=0.080000 rate=30.000000 maxthrust=3.500000 elementsize=0.050000 count=30
mophiahurting     _particle         timer=0.200000 airdrag=0.300000 mass=1.000000 typeflags=0x19 material=nub_a4sfx_rboss_hit.mat range=0.050000 rate=80.000000 maxthrust=60.000000 elementsize=0.025000 count=40
mophiabombemit    _sprite           thingflags=0x1 sprite=gen_lightninghit.spr
mophiabombspr     _sprite           timer=0.300000 sprite=aet_mophiaexp.spr
purpleflash       _sprite           sprite=gen_purpleflash.spr
mophiablast       _explosion        light=(0.000000/0.000000/0.000000/1.000000) timer=0.400000 sprite=aet_mophiablast.spr typeflags=0x17 blasttime=0.239000 range=0.500000
jeep              _wstructmetal     move=physics thingflags=0x404400 model3d=bab_jeep.3do size=0.195000 movesize=0.090000 collwidth=0.192000 collheight=0.090000 cog=jeep_bab.cog
1x1crate          _pstruct1mwood    model3d=cyn_crate_1x1.3do size=0.086603 movesize=0.086603 collwidth=0.086603 collheight=0.086603
exbridge          _sstructwood      model3d=jep_brdg_xpld.3do size=0.262116 movesize=0.262116 collwidth=0.262116 collheight=0.262116
jepzero           _sstructmetal     model3d=jep_plane_zero.3do size=0.612032 movesize=0.612032 collwidth=0.612032 collheight=0.612032
palmtreeb         _sstructwood      model3d=gen_tree_palmb.3do size=0.322684 movesize=0.322684 collwidth=0.322684 collheight=0.322684
plank             _decor            thingflags=0x400 model3d=jep_plank_2m.3do size=0.070000 movesize=0.070000 collwidth=0.112500 collheight=0.112500 cog=item_plank.cog
flgtent           _staticstruct     model3d=jep_flag_tent.3do size=0.081009 movesize=0.081009 collwidth=0.081009 collheight=0.081009
ropeladr          _ghostdecor       model3d=0lad.3do size=0.149960 movesize=0.149960 collwidth=0.149960 collheight=0.149960 puppet=default.pup
jeep_cs           _actor            thingflags=0x4401 model3d=gen_ij.3do size=0.040000 movesize=0.040000 collwidth=0.089662 collheight=0.089662 puppet=default.pup soundclass=jeep.snd cog=jeep_cutscene_bab.cog surfdrag=3.000000 physflags=0x1004A5F maxvel=0.400000 maxthrust=0.350000 maxrotthrust=60.000000 jumpspeed=0.400000 eyeoffset=(0.000000/-0.032000/0.120000) lightoffset=(0.000000/0.200000/0.000000) lightintensity=(0.500000/0.500000/0.500000) aiclass=truck.ai
ladderindy        _cutactor         model3d=gen_in_ladder.3do collwidth=0.020000 collheight=0.020000 puppet=default.pup
wtrcntnr          _sstructmetal     model3d=jep_cntnr_water.3do size=0.041272 movesize=0.041272 collwidth=0.041272 collheight=0.041272
foldtabl          _sstructwood      model3d=jep_table_fold.3do size=0.083815 movesize=0.083815 collwidth=0.083815 collheight=0.083815
trnksoph          _sstructwood      model3d=jep_trunk_sophia.3do size=0.106468 movesize=0.106468 collwidth=0.106468 collheight=0.106468
islandb           _staticstruct     model3d=jep_island_b.3do size=4.069284 movesize=4.069284 collwidth=4.069284 collheight=4.069284
islandc           _staticstruct     model3d=jep_island_c.3do size=4.638004 movesize=4.638004 collwidth=4.638004 collheight=4.638004
islanda           _staticstruct     model3d=jep_island_a.3do size=1.798119 movesize=1.798119 collwidth=1.798119 collheight=1.798119
palmleafs         _ghostdecor       model3d=olv_shrub_palm_b.3do size=0.062500 movesize=0.062500 collwidth=0.062500 collheight=0.062500
brncha            _wstructwood      model3d=jep_branch_wall_a.3do size=1.270567 movesize=1.270567 collwidth=1.270567 collheight=1.270567
shad2             _ghostdecor       model3d=gen_shadow_2x2m.3do
jeepstopbig       _structblock      thingflags=0x80D4 model3d=gen_invisiplate3x2.3do size=0.100000 movesize=0.100000 collwidth=0.100000 collheight=0.100000
jep2mplnk         _sstructwood      model3d=jep_plank_2m.3do size=0.112500 movesize=0.112500 collwidth=0.112500 collheight=0.112500
decor_boulder     _decor            model3d=gen_boulder.3do size=0.161679 movesize=0.161679 collwidth=0.161679 collheight=0.161679 puppet=default.pup
prulavatrapblock  _staticstruct     thingflags=0xC1 light=(0.200000/0.200000/0.200000/0.200000) model3d=pru_block_cork.3do size=0.335410 movesize=0.335410 collwidth=0.335410 collheight=0.335410 soundclass=vol_piers.snd
+centbite         +bughit           damage=0.200000 damageclass=0x100
centipede         _actor            thingflags=0x40000040 model3d=gen_cp.3do size=0.025000 movesize=0.025000 collwidth=0.034000 collheight=0.034000 puppet=centipede.pup soundclass=centipede.snd cog=actor_centipede.cog surfdrag=1.500000 staticdrag=0.000100 mass=10.000000 weapon=+centbite health=5.000000 maxhealth=5.000000 maxthrust=0.100000 maxrotthrust=200.000000 typeflags=0x81000000 eyeoffset=(0.000000/0.008000/-0.024000) aiclass=centipede.ai
whipspike         _decor            model3d=tem_pikes_floor.3do size=0.050000 movesize=0.050000 collwidth=0.105091 collheight=0.105091
grdstat           _staticstruct     model3d=pru_statue_guard.3do size=0.197800 movesize=0.197800 collwidth=0.197800 collheight=0.197800
sqrkey            _item             thingflags=0x400 model3d=pru_key_sqr.3do collwidth=0.020000 collheight=0.020000 cog=item_squarekey.cog height=0.009000
dark4mvine        _ghostdecor       model3d=gen_vines_4mvert_b.3do size=0.200000 movesize=0.200000 collwidth=0.200000 collheight=0.200000
dark6mvine        _ghostdecor       model3d=gen_vines_6mvert_b.3do size=0.300000 movesize=0.300000 collwidth=0.300000 collheight=0.300000
treeb             _ghostdecor       model3d=gen_tree_standupb.3do size=0.175000 movesize=0.175000 collwidth=0.175000 collheight=0.175000
uvine             _ghostdecor       model3d=gen_vines_u.3do size=0.210009 movesize=0.210009 collwidth=0.210009 collheight=0.210009
darkuvine         _ghostdecor       model3d=gen_vines_u_b.3do size=0.210009 movesize=0.210009 collwidth=0.210009 collheight=0.210009
bushd             _ghostdecor       model3d=gen_bush_standupd.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000
treetopc          _ghostdecor       model3d=gen_tree_standtopc.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000
bushh             _ghostdecor       model3d=gen_bush_standuph.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000
treetopa          _ghostdecor       model3d=gen_tree_standtopa.3do size=0.200000 movesize=0.200000 collwidth=0.200000 collheight=0.200000
3stalagmites      _ghostdecor       model3d=cyn_stalagmitea.3do size=0.450000 movesize=0.450000 collwidth=0.450000 collheight=0.450000
1stalagmite       _staticstruct     model3d=cyn_stalagmitec.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
raididolmove      _decor            collide=3 model3d=pru_idol.3do size=0.014700 movesize=0.014700 collwidth=0.014700 collheight=0.014700
rockstrutclimb    _climbstruct      model3d=olv_strut_rock.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
altar             _staticstruct     model3d=pru_altar_idol.3do size=0.090139 movesize=0.090139 collwidth=0.090139 collheight=0.090139
root              _decor            model3d=pru_root.3do size=0.050000 movesize=0.050000 collwidth=0.050000 collheight=0.050000
rootstrut         _swingstruct      model3d=pru_strut_root.3do collwidth=0.100000 collheight=0.100000
dbrsblck          _walkstruct       model3d=pru_dbrs_blck.3do size=0.061237 movesize=0.061237 collwidth=0.061237 collheight=0.061237
prueagle          _structure        move=physics model3d=pru_strut_eagle.3do size=0.107020 movesize=0.107020 collwidth=0.107020 collheight=0.107020
grgylstrtclimb    _climbstruct      model3d=pru_strut_grgyl.3do size=0.025000 movesize=0.025000 collwidth=0.025000 collheight=0.025000
prutorch          _structwood       model3d=pru_torch_fire.3do size=0.054032 movesize=0.054032 collwidth=0.054032 collheight=0.054032
felltreeswing     _swingstruct      model3d=riv_tree_fell.3do collwidth=0.100000 collheight=0.100000
bushg             _ghostdecor       model3d=gen_bush_standupg.3do size=0.150000 movesize=0.150000 collwidth=0.150000 collheight=0.150000
shirtraftplayer   raftplayer        model3d=gen_ir_shirt.3do
2x1blk            _walkstruct       model3d=pru_blck_2x1.3do size=0.122474 movesize=0.122474 collwidth=0.122474 collheight=0.122474
boulder           _actor            thingflags=0x40 model3d=gen_boulder.3do size=0.178000 movesize=0.178000 collwidth=0.161679 collheight=0.161679 puppet=boulder.pup surfdrag=1.000000 staticdrag=0.020000 mass=1000.000000 physflags=0x4AEE typeflags=0x300D08
headblck          _pushstruct       thingflags=0x40000000 model3d=pru_block_head.3do size=0.144376 movesize=0.144376 collwidth=0.144376 collheight=0.144376
prupush           _pushstruct2m     model3d=pru_block_push.3do size=0.173205 movesize=0.173205 collwidth=0.173205 collheight=0.173205
dytenmeterlight   _ghostdecor       type=ghost thingflags=0x1 light=(0.000000/0.000000/0.000000/1.000000)
swim_piranha      piranha           physflags=0x404A03
idoldoor          _staticstruct     model3d=pru_door_idol.3do size=0.282537 movesize=0.282537 collwidth=0.282537 collheight=0.282537 soundclass=pru_door.snd
idoldoorb         _staticstruct     model3d=pru_door_idol_b.3do size=0.282537 movesize=0.282537 collwidth=0.282537 collheight=0.282537 soundclass=pru_door.snd
deeprising        _staticstruct     thingflags=0xC1 light=(0.200000/0.300000/0.400000/0.001000) model3d=pru_water_rise.3do
prurubbleb        _walkstruct       model3d=pru_rubbl_idolb.3do size=0.333995 movesize=0.333995 collwidth=0.333995 collheight=0.333995
prurubblea        _staticstruct     model3d=pru_rubbl_idola.3do size=0.353123 movesize=0.353123 collwidth=0.353123 collheight=0.353123
prupike           _sstructmetal     model3d=pru_pike_metal.3do size=0.100080 movesize=0.100080 collwidth=0.100080 collheight=0.100080
sapito            _decor            model3d=pru_sapito.3do size=0.085699 movesize=0.085699 collwidth=0.085699 collheight=0.085699
blckb             _staticstruct     model3d=pru_brdg_blkb.3do size=0.130219 movesize=0.130219 collwidth=0.130219 collheight=0.130219
blckd             _staticstruct     model3d=pru_brdg_blkd.3do size=0.134052 movesize=0.134052 collwidth=0.134052 collheight=0.134052
blcke             _staticstruct     model3d=pru_brdg_blke.3do size=0.124905 movesize=0.124905 collwidth=0.124905 collheight=0.124905
sqrkeymove        _decor            model3d=pru_key_sqr.3do size=0.022023 movesize=0.022023 collwidth=0.022023 collheight=0.022023
altarblck         _staticstruct     model3d=pru_block_altar.3do size=0.335410 movesize=0.335410 collwidth=0.335410 collheight=0.335410
pedestal          _walkstruct       model3d=pru_pedestal.3do size=0.072887 movesize=0.072887 collwidth=0.072887 collheight=0.072887
2mmvblk           _walkstruct       model3d=pru_blck_2move.3do size=0.188746 movesize=0.188746 collwidth=0.188746 collheight=0.188746 soundclass=pru_stairpitblocks.snd
4mmvblk           _walkstruct       model3d=pru_blck_4move.3do size=0.256174 movesize=0.256174 collwidth=0.256174 collheight=0.256174 soundclass=pru_stairpitblocks.snd
6mmvblksh         _walkstruct       model3d=pru_blck_6move_hangside.3do size=0.340037 movesize=0.340037 collwidth=0.340037 collheight=0.340037 soundclass=pru_stairpitblocks.snd
8mmvblk           _walkstruct       model3d=pru_blck_8move.3do size=0.430842 movesize=0.430842 collwidth=0.430842 collheight=0.430842 soundclass=pru_stairpitblocks.snd
10mmvblk          _walkstruct       model3d=pru_blck_10move.3do size=0.525000 movesize=0.525000 collwidth=0.525000 collheight=0.525000 soundclass=pru_stairpitblocks.snd
12mmvblk          _walkstruct       model3d=pru_blck_12move.3do size=0.620987 movesize=0.620987 collwidth=0.620987 collheight=0.620987 soundclass=pru_stairpitblocks.snd
2minvisirock      _sstructblock     thingflags=0x4D4 model3d=gen_invisirocks_scary2m.3do size=0.141421 movesize=0.141421 collwidth=0.141421 collheight=0.141421 cog=class_sharpboulder.cog
1minvisirock      _sstructblock     thingflags=0x4D4 model3d=gen_invisirocks_scary1m.3do size=0.070711 movesize=0.070711 collwidth=0.070711 collheight=0.070711 cog=class_sharpboulder.cog
ohleaves          _staticstruct     model3d=pru_leaf_clng.3do size=0.141421 movesize=0.141421 collwidth=0.141421 collheight=0.141421
climbvine         _ghostdecor       model3d=pru_ladder_vine.3do size=0.400000 movesize=0.400000 collwidth=0.400000 collheight=0.400000
3minvisirock      _sstructblock     thingflags=0x4D4 model3d=gen_invisirocks_scary3m.3do size=0.212132 movesize=0.212132 collwidth=0.212132 collheight=0.212132 cog=class_sharpboulder.cog
istrpdoor         _walkstruct       model3d=pru_door_trap.3do size=0.142222 movesize=0.142222 collwidth=0.142222 collheight=0.142222 soundclass=pru_trapdoor.snd
istrpdoorb        _walkstruct       model3d=pru_door_trapb.3do size=0.142222 movesize=0.142222 collwidth=0.142222 collheight=0.142222 soundclass=pru_trapdoor.snd
whipsspikeclimb   _climbstruct      model3d=pru_strut_pike.3do size=0.100000 movesize=0.100000 collwidth=0.100000 collheight=0.100000
splshblck         _ghostdecor       model3d=gen_splash_block.3do size=0.274071 movesize=0.274071 collwidth=0.274071 collheight=0.274071
jeep_actor_indy   _cutactor         thingflags=0x20084400 model3d=gen_ij.3do collwidth=0.190000 collheight=0.090000 puppet=indy_jeep.pup cog=jeep_cutscene_bab.cog physflags=0x4004A4F eyeoffset=(0.045400/0.025400/0.069500) aiclass=none.ai
jeeppru           _wstructmetal     move=physics thingflags=0x404400 model3d=bab_jeep.3do size=0.195000 movesize=0.090000 collwidth=0.192000 collheight=0.090000 cog=jeep_pru.cog
limpraft          _item             thingflags=0x400 model3d=riv_raft_inflt1.3do collwidth=0.020000 collheight=0.020000 cog=item_raft.cog height=0.005000
physrbl           _decor            move=physics model3d=pru_strut_eagle.3do size=0.010000 movesize=0.010000 collwidth=0.107020 collheight=0.107020 mass=20.000000 physflags=0x1
bldrsmash         _ghostdecor       model3d=pru_boulder_smash.3do size=0.270574 movesize=0.270574 collwidth=0.270574 collheight=0.270574
bldrnvelop        _staticstruct     model3d=pru_boulder_smash_nvelop.3do size=0.234879 movesize=0.234879 collwidth=0.234879 collheight=0.234879
whirlpoola        _particle         timer=2.000000 vel=(0.000000/0.000000/-0.200000) angvel=(0.000000/180.000000/0.000000) material=gen_4sprite_bubble.mat range=0.010000 minsize=0.020000 elementsize=0.005000 count=20
whirlpoolb        whirlpoola        vel=(0.000000/0.000000/-0.050000) range=0.030000 minsize=0.030000 count=30
vinedebris        _particle         mass=10.000000 physflags=0x201 vel=(0.000000/0.000000/-0.500000) material=pru_underwater_vine_pieces.mat range=0.020000 rate=18.000000 maxthrust=30.000000 elementsize=0.030000 count=10
