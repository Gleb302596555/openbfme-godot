# Port 135: AttachUpdate investigation

DONOR BFME2 0x491968 (`Code/GameEngine/Source/GameLogic/Object/Update/AttachUpdateModuleDataCtor.cpp`) initializes the scan range and teleport flag. DONOR BFME2 0x491D2F (`Code/GameEngine/Source/GameLogic/Object/Update/AttachUpdate.cpp`) attaches, follows, and kills the attachment when its parent disappears or dies.

RotWK 2.01, read through `rw.py dis`:

* RW 0x89502A loads float 10 from 0xBD83D8, 0x89503E stores ScanRange; 0x895043 stores AlwaysTeleport true; 0x895047 clears AnchorToTopOfGeometry.
* RW 0x8953CB tries attachment; 0x8953D0–0x8953D5 returns one frame if no parent id. RW 0x8953DE looks up the parent; 0x8953E8 branches on missing parent, 0x8953EA–0x8953F1 on its effectively-dead bit, both to 0x895435.
* RW 0x8953FA computes the anchor; 0x895406–0x89540D skip movement when equal. RW 0x895412–0x895420 teleports when enabled; 0x89542D–0x895433 otherwise sets position. Both return one frame at 0x895425–0x89542C.
* RW 0x895435 clears the id, 0x895448 reports the death event, 0x895453 kills the attachment, 0x895458–0x89545D returns sleep forever. No timer or modifier test occurs in this update. This does not exclude a separate module or external action killing the object.

The existing port already has the attachment, follow and death paths in `GameLogic/Module/AttachUpdate.cpp`; the data defaults differ in `AttachUpdate.h`. Eowyn's marker has an AttachUpdate but no lifetime or deletion module. Do not introduce an invented timer; its original in-game lifetime remains for the owner to verify. Separate outstanding mechanisms: player object-target validation (`CommandXlat::evaluateGUISpecialPower`, S-2000; RW 0x82D9A0–0x82D9D1), and the Disguise chain (`SpecialDisguiseUpdate`, RW 0x8B4760): not addressed here.

Ennorath definitions explicitly missing both defaults include EowynSpawnAttachPing, LurtzAttachSpy, MorgulSeucheAttachSpy, SpionageAvariObject, LorienWarriorMarkObject, FledermuseDolGuldursObject, SpionageKhamulObject, MorgomirAttachSpy, MilitrischeStudienPing, KankraSpinnenlarvenAttach, SmaugAttachSpy, AzogAttachSpy, HaradObserverObject, HaradSpyTrackingEye, KhamulCurseWeapon and Level1Spy / Level2Spy. MorgulSeucheAttachSpyAgain / End, HeilenderGesangPing, CreateAHeroOilDrum_Level1 / Level2 / Level3 and WatcherHittingArm / WatcherCaldera explicitly set a range but omit the teleport flag. PalantirShard and PalantirShard01–07 explicitly set both fields, so those definitions do not change from these defaults; they remain relevant neighbours for regression testing. Inherited objects were not enumerated.
