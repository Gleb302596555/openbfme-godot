// OpenBFME. GPL-3.0.
//
// TerrainResourceFeedback (lane OWNERFIX-2, the owner's playtest of 2026-10-17: "retail shows, while placing and on the building, the overlap with other
// resource buildings and how effective the building will be"): the client half of RotWK's TerrainResourceManager. Client only: it reads the logic's claim
// grid and the local player's shroud, it never writes them (the simulation half, the income share of overlapping claims, is TerrainResourceManager /
// TerrainResourceBehavior).
//
// TARGET FACTS (rotwk201_game.exe, caveat S-001; the BFME2 twins are tier A with byte-matched decomp C++ unless marked):
//   * the placement feedback (InGameUI::handleBuildPlacements, BFME2 decomp InGameUIHandleBuildPlacements.cpp): when a building starts waiting for its site
//     and its template has a TerrainResourceBehavior (the first behaviour module answering resourceData(), rva0029B9E5) with MaxIncome > 0, a
//     PlaceTerrainResourceClaimantFeedback is made for the ghost drawable (InGameUI + 0x574 the params: PlaceTerrainResourceClaimantDecal, Font, FontColor);
//   * its update each client frame (RW twin of BFME2 0x4E6455, decomp Rva004E65CFLeaf.cpp Impl::update): fraction = TerrainResourceManager RW 0x75B561
//     (BFME2 rva00359A0C) at the ghost's position, the module's Radius and HighPriority, the local player's index; the text is "GUI:TerrainResourcePercentage
//     Claimable" formatted with floor(fraction * 100 + 0.5) (WWMath::Float_To_Long), its colour (and the decal's) 0xFF00FF00 above 0.75, 0xFFFFFF00 from 0.5,
//     0xFFFF0000 from 0.25, else 0xFF000000; the text sits centred on the pointer's x, its height + 20 above the pointer, clamped into the view (updateStringPos);
//   * the placement decal (createPotentialClaimDecal, RW twin of BFME2 0x4E65CF): PlaceTerrainResourceClaimantDecal as a shadow of type 0x2000, 2 * Radius on
//     both axes, at the ghost drawable's position, setColor 0xFFFF8000 then the colour above and setOpacity 0x80 (neither reaches the decal: below); the
//     projected decal of type 0x2000 (RW 0x50C23A) snaps its centre down to the
//     claim grid (x - fmodf(x, cell), RW 0x492AC8) and gives every terrain vertex the alpha RW 0x75C1A9 (BFME2 rva0035A61F) computes for the local player;
//     lane PLACEDECAL-1: from the decal's diffuse 0xFFFFFFFF (RW 0x50D264): setColor / setOpacity (RW 0x73232B / 0x73228B) leave a type 0x2000 decal's
//     diffuse (their masks 0x1420 / 0x10840), so the colour and the opacity 0x80 never reach it; it is drawn on the terrain only (+ 0x64, RW 0x50C792) with
//     flushDecals' blended shader (RW 0x509A1F: ShaderClass 0x1198B3, texture x diffuse);
//   * RW 0x75B561: the disc of RW 0x75BA1D's arithmetic (centre floor((x - origin) / cell + 0.5), radius ceil(radius / cell)), each cell counted and tested
//     with RW 0x75C02D (BFME2 isCellClaimable) as (cell, HighPriority ? 0x7FFFF43 : 0, 0, force false, the local player); the share claimed / total (0 for none);
//   * RW 0x75C02D with a player >= 0 and no force: the player's cell shroud at the cell's centre ((i + 0.5) * cell + origin) SHROUDED -> no; FREE yes,
//     BLOCKED no; SHARED: fogged -> yes, else no when the player already holds an entry; OWNED: fogged -> yes, no entry -> yes, the first entry's object gone ->
//     yes, that object FOGGED / SHROUDED for the player -> yes, else no;
//   * RW 0x75C1A9 (vx, vy in map vertices of 10 units, the cell of a vertex is (v / 2)): the vertex's own cell not claimable -> alpha 0; else weight 12 minus
//     4 per unclaimable edge neighbour (x +- 1, y +- 1 halved), 2 for (+1, +1) and (-1, -1), 3 for (-1, +1) and (+1, -1), 1 per unclaimable (x +- 2, y +- 2);
//     weight <= 0 -> 0, below 12 -> alpha * weight / 12;
//   * the selection feedback (both editions: TerrainResourceClientBehavior ctor RW 0x652B8C / BFME2 0x652DEB, dtor RW 0x8D0A08 / BFME2 0x8CC55A,
//     createSelectionFeedback slot 17 RW 0x8D0A58 / BFME2 0x8CC5AA, removeSelectionFeedback slot 18 RW 0x8D0A78 / BFME2 0x8CC5CA; called from
//     Drawable::onSelected RW 0x67580E / BFME2 0x6754E3 -> Drawable::createSelectionFeedback RW 0x672ED1 / BFME2 0x672BAB when the object is locally
//     controlled, and from Drawable::onDeselected RW 0x675870 / BFME2 0x675545 -> Drawable::removeSelectionFeedback RW 0x672F0D / BFME2 0x672BE7): the holder
//     at InGameUI + 0x58C counts its users (+ 0x04, show RW 0x8EC271 -> 0x8EC1D6 / BFME2 0x8E7B0C -> 0x8E7A85, hide RW 0x8EC449 -> 0x8EC41B /
//     BFME2 0x8E7D1D -> 0x8E7CEF); while it has any, each client frame (ResourceEntryOwner RW 0x8EC76F / BFME2 0x8E7E3A) every claimant whose owner is the
//     local player or an ally (getPlayersWithRelationship(local, ALLOW_SAME_PLAYER | ALLOW_ALLIES = 3)) and is Visible (the claim's flag, RW 0x8ECB54 /
//     BFME2 0x8E81DE) keeps one decal (TerrainResourceClaimFeedback::Impl::updateClaimedDecalForClaimant, RW 0x8EC930 / BFME2 0x8E7FCA):
//     TerrainResourceClaimDecal, type 0x20, 2 * radius, colour 0xFF6464C8, opacity 0x80, at the claimant's position; a claimant FOGGED / SHROUDED for the
//     local player has none; a claimant gone loses it.
// INFERENCE (stop S-5030): the HUD's user count follows the local selection and the placement (one user per selected object of the local player whose template
//   lists a TerrainResourceClientBehavior, one more while such a template waits for its site: retail shows the other farms' circles while a farm is placed);
//   the device interpolates the vertex alphas bilinearly across a terrain quad (retail splits it into two triangles); the client form never erases a dead owner
//   from a cell (retail's draw-path cell write of RW 0x75C02D): such a cell reads as claimable, the value retail returns.

#pragma once

#include "Common/INIDataTypes.h"
#include "Common/Module.h"
#include "GameClient/SelectionDecals.h"
#include "GameLogic/ObjectTypes.h"

#include <cstdint>
#include <string>
#include <vector>

class Drawable;
class GameLogic;
class ModuleFactory;
class Player;
class TerrainResourceFeedback;
class ThingTemplate;
class TerrainResourceBehaviorModuleData;
struct TerrainResourceUiSettings;

// TerrainResourceClientBehavior (both editions: RW 0x6549B1 / BFME2 0x654C56 friend_newModuleInstance, RW 0x708243 / BFME2 0x706A6E friend_newModuleData;
// ctor RW 0x652B8C / BFME2 0x652DEB, size 0x10, vtable RW 0xC06278 / BFME2 0xBFE710; dtor RW 0x8D0A08 / BFME2 0x8CC55A; slot 17 createSelectionFeedback
// RW 0x8D0A58 / BFME2 0x8CC5AA; slot 18 removeSelectionFeedback RW 0x8D0A78 / BFME2 0x8CC5CA).
class TerrainResourceClientBehavior : public DrawableModule
{
public:
	TerrainResourceClientBehavior(Thing *thing, const ModuleData *moduleData);
	~TerrainResourceClientBehavior() override;

	void createSelectionFeedback() override;
	void removeSelectionFeedback() override;

	bool isShowing() const { return m_showing; } ///< +0x0C
	void setFeedback(TerrainResourceFeedback *feedback) { m_feedback = feedback; }
	TerrainResourceFeedback *feedback() const { return m_feedback; }

	static void registerClass(ModuleFactory &modules);

private:
	bool m_showing = false; ///< +0x0C: true while this drawable contributes a user to the TerrainResourceClaimFeedback holder
	TerrainResourceFeedback *m_feedback = nullptr;
};

namespace TerrainResourceClaimView
{
// RW 0x75C02D's client form (see above); `player` < 0 skips the shroud as retail does
bool claimable(const GameLogic &logic, int x, int y, std::uint32_t mask, ObjectID ownId, bool force, int player);
// RW 0x75B561 (BFME2 rva00359A0C)
float claimableFraction(const GameLogic &logic, float x, float y, float radius, bool highPriority, int player);
// RW 0x75C1A9 (BFME2 rva0035A61F): `color` ARGB of the decal at a vertex, returned with its alpha weighted
std::uint32_t vertexColor(const GameLogic &logic, std::uint32_t color, int vx, int vy, int player);
// rva0029B9E5: the first behaviour module data of the template that is a TerrainResourceBehavior's, or null
const TerrainResourceBehaviorModuleData *resourceDataOf(const ThingTemplate &tt);
// the colour band of a claimable fraction (Impl::update)
std::uint32_t bandColor(float fraction);
} // namespace TerrainResourceClaimView

struct ResourceClaimDecal
{
	ObjectID id = INVALID_ID;
	Coord3D position{ 0.0f, 0.0f, 0.0f };
	float size = 0.0f;               ///< 2 * radius
	std::uint32_t color = 0xFF6464C8u;
	int opacity = 0x80;
};

// a segment of a claim circle's outline (TerrainClaimSegment, decal type 0x40, RW 0x8E9744): a square `size` wide at `position`, turned by `angle`,
// in the claimant owner's indicator colour (RW 0x68B6F5) at opacity 255
struct ResourceClaimArcSegment
{
	Coord3D position{ 0.0f, 0.0f, 0.0f };
	float size = 0.0f;
	float angle = 0.0f;
	std::uint32_t color = 0xFF000000u;
	int opacity = 255;
};

struct ResourcePlacementFeedback
{
	bool active = false;
	std::string templateName;
	float fraction = 0.0f;
	int percent = 0;
	std::u16string text;             ///< GUI:TerrainResourcePercentageClaimable formatted with the percent
	std::uint32_t color = 0;         ///< the band colour, ARGB (the text's; RotWK sets it on the decal too, where it has no effect: lane PLACEDECAL-1)
	int pointerX = 0, pointerY = 0;  ///< the pointer in window pixels (updateStringPos: the text's centre x, its bottom 20 pixels above)
	Coord3D decalCenter{ 0.0f, 0.0f, 0.0f }; ///< the ghost's position snapped down to the claim grid (type 0x2000)
	float decalSize = 0.0f;          ///< 2 * Radius
	int decalOpacity = 0x80;         ///< the setOpacity RotWK calls (RW 0x73228B), which leaves a type 0x2000 decal as it is (lane PLACEDECAL-1)
	// the alpha (0 .. 255) of every map vertex the decal covers: maskW x maskH vertices from (maskVx0, maskVy0), 10 units apart (RW 0x75C1A9 from the decal's
	// diffuse 0xFFFFFFFF: lane PLACEDECAL-1)
	int maskVx0 = 0, maskVy0 = 0, maskW = 0, maskH = 0;
	std::vector<std::uint8_t> maskAlpha;
	unsigned version = 0;            ///< bumped when the mask or the colour changed
};

class TerrainResourceFeedback
{
public:
	// `selectionUsers`: claimDecalUsers (S-5030)
	void updateSelection(const GameLogic &logic, int localPlayer, int selectionUsers);
	// `pending` the template waiting for its site (null: none), `ghost` its drawable's position, `format` the GUI:TerrainResourcePercentageClaimable text
	void updatePlacement(const GameLogic &logic, int localPlayer, const ThingTemplate *pending, const Coord3D &ghost, const std::u16string &format,
		int pointerX = 0, int pointerY = 0);
	const std::vector<ResourceClaimDecal> &claimedDecals() const { return m_claimed; }
	// the outline arcs of the own / allied claim circles (RW 0x8EC1D6 -> 0x75B8CC, 0x8E99EE, 0x8E9744): the parts of each Visible claimant's circle no other
	// such circle covers, cut into TerrainClaimSegment decals
	const std::vector<ResourceClaimArcSegment> &claimArcSegments() const { return m_arcs; }
	struct ClaimCircle
	{
		Coord3D center{ 0.0f, 0.0f, 0.0f };
		float radius = 0.0f;
		std::uint32_t color = 0xFF000000u;
	};
	// RW 0x8E99EE (the open arcs, steps of RW 0xC790A0 = pi / 320) and RW 0x8E9744 (the segments) for a set of circles
	static std::vector<ResourceClaimArcSegment> arcSegments(const std::vector<ClaimCircle> &circles);
	// lane PLACEDECAL-1: the placement claim decal and the claims, in RotWK's draw order, as the selection decals' alpha decals (the shaders' grid,
	// BuildMergeDecalGrid)
	void appendDecals(const std::string &claimTexture, const std::string &placeTexture, std::vector<SelectionDecal> &out) const;
	const ResourcePlacementFeedback &placement() const { return m_placement; }
	// TerrainResourceClaimFeedback::show (RW 0x8EC271 -> 0x8EC1D6 / BFME2 0x8E7B0C -> 0x8E7A85) and hide (RW 0x8EC449 -> 0x8EC41B / BFME2 0x8E7D1D -> 0x8E7CEF):
	// the user count at Impl + 0x04 driven by TerrainResourceClientBehavior::createSelectionFeedback / removeSelectionFeedback / ~TerrainResourceClientBehavior
	void show();
	void hide();
	int users() const { return m_users; }
	static bool hasClientBehavior(const ThingTemplate &tt);
	static bool hasClientBehavior(const Drawable &d);
	// the users of the claim decals (S-5030): the selected objects of `local` whose template lists a TerrainResourceClientBehavior, plus one while `pending`
	// (the template waiting for its site, null: none) lists one
	static int claimDecalUsers(const GameLogic &logic, const Player *local, const std::vector<ObjectID> &selected, const ThingTemplate *pending);
	static std::string stopLine(); ///< "[S-5030] ..." (the HUD's stops)
	static constexpr float kVertexSpacing = 10.0f; ///< MAP_XY_FACTOR (RW 0xBD83D4 = 0.1 the inverse)

private:
	int m_users = 0; ///< TerrainResourceClaimFeedback::Impl + 0x04
	std::vector<ResourceClaimDecal> m_claimed;
	std::vector<ResourceClaimArcSegment> m_arcs;
	std::vector<ClaimCircle> m_arcCircles;           // the circles m_arcCache was built from
	std::vector<ResourceClaimArcSegment> m_arcCache;
	ResourcePlacementFeedback m_placement;
	std::vector<std::int8_t> m_claimMemo; ///< lane FOGFX-1: updatePlacement's per-cell answers of RW 0x75C02D (-1 not asked yet)
};
