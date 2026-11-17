# The 'on all 0 members' upgrade PASS rows (main efc220a5, 2026-11-16)

The old sweep counted an upgrade as PASS when the upgrade reached "all 0 members it affects": the unit was replaced or the upgrade
landed nowhere, and nothing was checked. efc220a5 checks the replacement ("replaced the unit with X"), so these rows are red on any
matrix collected after it. Whether the scene or the engine is wrong is Local 70's question (issue 526, ReplaceSelfUpgrade walls).

- `AngmarWallSegmentSmall Command_AngmarWallUpgradeToHub` (FactionAngmar): the old PASS was false
- `AngmarWallSegmentSmall Command_AngmarWallUpgradeToTower` (FactionAngmar): the old PASS was false
- `ArnorCastleWallSegment Command_ArnorWallUpgradeToHub` (FactionArnor): the old PASS was false
- `ArnorCastleWallSegment Command_ArnorWallUpgradeToRegularGate` (FactionArnor): the old PASS was false
- `ArnorCastleWallSegment Command_ArnorWallUpgradeToTower` (FactionArnor): the old PASS was false
- `ArnorWallSegmentSmall Command_ArnorWallUpgradeToRegularGate` (FactionArnor): the old PASS was false
- `DwarvenCastleWallSegment Command_DwarvenWallUpgradeToPosternGate` (FactionDwarves): the old PASS was false
- `DwarvenWallSegmentSmall Command_DwarvenWallUpgradeToRegularGate` (FactionDwarves): the old PASS was false
- `DwarvenWallSegmentSmall Command_DwarvenWallUpgradeToTower` (FactionDwarves): the old PASS was false
- `GondorCastleWallSegment Command_MenWallUpgradeToHub` (FactionDwarves): the old PASS was false
- `MenWallSegmentSmall Command_MenWallUpgradeToPosternGate` (FactionDwarves): the old PASS was false
- `MenWallSegmentSmall Command_MenWallUpgradeToRegularGate` (FactionDwarves): the old PASS was false
- `MenWallSegmentSmall Command_MenWallUpgradeToTower` (FactionDwarves): the old PASS was false
- `MenWallSegmentSmall Command_MenWallUpgradeToTrebuchet` (FactionDwarves): the old PASS was false
- `ElvenCastleWallSegment Command_ElvenWallUpgradeToAirAltar` (FactionImladris): the old PASS was false
- `ElvenCastleWallSegment Command_ElvenWallUpgradeToFireAltar` (FactionImladris): the old PASS was false
- `ElvenCastleWallSegment Command_ElvenWallUpgradeToHub` (FactionImladris): the old PASS was false
- `ElvenCastleWallSegment Command_ElvenWallUpgradeToRegularGate` (FactionImladris): the old PASS was false
- `ElvenCastleWallSegment Command_ElvenWallUpgradeToSeiteneingang` (FactionImladris): the old PASS was false
- `ElvenCastleWallSegment Command_ElvenWallUpgradeToTower` (FactionImladris): the old PASS was false
- `MenWallSegmentSmall Command_MenWallUpgradeToRegularGate` (FactionImladris): the old PASS was false
- `IsengardCastleWallSegmentPlayer Command_IsengardWallUpgradeToHub` (FactionIsengard): the old PASS was false
- `GondorCastleWallSegment Command_MenWallUpgradeToHub` (FactionMen): the old PASS was false
- `GondorCastleWallSegment Command_MenWallUpgradeToPosternGate` (FactionMen): the old PASS was false
- `GondorCastleWallSegment Command_MenWallUpgradeToRegularGate` (FactionMen): the old PASS was false
- `GondorCastleWallSegment Command_MenWallUpgradeToTower` (FactionMen): the old PASS was false
- `GondorCastleWallSegment Command_MenWallUpgradeToTrebuchet` (FactionMen): the old PASS was false
- `MenWallSegmentSmall Command_MenWallUpgradeToRegularGate` (FactionMen): the old PASS was false
- `GondorCastleWallSegment Command_MenWallUpgradeToHub` (FactionMordor): the old PASS was false
- `GondorCastleWallSegment Command_MenWallUpgradeToPosternGate` (FactionMordor): the old PASS was false
- `GondorCastleWallSegment Command_MenWallUpgradeToRegularGate` (FactionMordor): the old PASS was false
- `GondorCastleWallSegment Command_MenWallUpgradeToTower` (FactionMordor): the old PASS was false
- `GondorCastleWallSegment Command_MenWallUpgradeToTrebuchet` (FactionMordor): the old PASS was false
- `MenWallSegmentSmall Command_MenWallUpgradeToHub` (FactionMordor): the old PASS was false
- `MenWallSegmentSmall Command_MenWallUpgradeToPosternGate` (FactionMordor): the old PASS was false
- `MenWallSegmentSmall Command_MenWallUpgradeToRegularGate` (FactionMordor): the old PASS was false
- `MenWallSegmentSmall Command_MenWallUpgradeToTower` (FactionMordor): the old PASS was false
- `MenWallSegmentSmall Command_MenWallUpgradeToTrebuchet` (FactionMordor): the old PASS was false
- `MordorBattleTower Command_PurchaseUpgradeMordorBattleTowerToUngol` (FactionMordor): the old PASS was false
- `MordorCastleWallSegmentNew Command_MordorWallUpgradeToTower` (FactionMordor): the old PASS was false
- `GondorCastleWallSegment Command_MenWallUpgradeToRegularGate` (FactionRohan): the old PASS was false
- `MenWallSegmentSmall Command_MenWallUpgradeToRegularGate` (FactionRohan): the old PASS was false
- `MenWallSegmentSmall Command_MenWallUpgradeToTrebuchet` (FactionRohan): the old PASS was false
- `RohanCastleWallSegment Command_RohanWallUpgradeToRegularGate` (FactionRohan): the old PASS was false
- `RohanCastleWallSegment Command_RohanWallUpgradeToSeiteneingang` (FactionRohan): the old PASS was false
