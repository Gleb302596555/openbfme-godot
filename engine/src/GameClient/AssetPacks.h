// OpenBFME. GPL-3.0.
//
// Asset packs (modding SDK piece 1, lane SDK-0): drag-and-drop replacements of retail models and textures, cosmetic only. OpenBFME feature,
// not a retail one (retail has no such layer; mods replace assets through -mod, Common/ModMount.h, and those replacements ARE data).
//
// A pack is a folder. Every file in it (any subfolder, any case) whose name is a retail asset's file name replaces that asset for drawing:
//  * textures: <stem>.dds / .tga / .png / .jpg replace the W3D texture <stem> whatever its retail extension or folder (the texture
//    lookup of Libraries/WWVegas/WW3D2/textureloader.cpp: art\compiledtextures\xx\, art\textures\);
//  * models: <name>.w3d replaces the W3D file <name>.w3d (meshes, HLODs, hierarchies and animations it holds), whatever its folder
//    (WW3DAssetManager's W3D_Asset_Path: art\w3d\xx\).
// Several packs: the LAST one given wins a name. One pack holding a name twice (two folders, or two texture formats of one stem) is an
// error naming both files. Every file is listed and sized at load; a file that changes afterwards is a read error.
//
// Why cosmetic only: the packs reach the RENDER side only (AssetPackW3DFileSource for the drawables' / instancer's asset managers and
// W3DMaterialFactory's textures). The simulation's asset manager (LiveGame's m_simAssetSource: launch bones RW 0x6756A1, PristinePose)
// reads the mounted ArchiveFileSystem, which packs never touch; so launch positions, the logic state and the profile identity are the same
// with or without packs, and a pack may be used online. (Picking uses the drawn model, so a pack can change what a click hits: input, sent
// as ordinary commands, never a desync.)
//
// Lane SDK-1 adds: a whitelist (a pack holds models, textures and its own text only; anything else refuses the pack), the skeleton check
// (checkModels, run on the mounted files) and the textures of particles, decals, roads, rivers, water and the sky (GodotDevice/
// GodotAssetPackImage.h). Precedence: base game < --mod-install < -mod < asset packs (a pack draws over a mod's file of the same name too).
//
// Not covered yet (reported in AssetPacks::notCovered()): terrain tile, UI (Apt) textures, and model formats other than W3D.

#pragma once

class ArchiveFileSystem;

#include "Libraries/WWVegas/WW3D2/assetmgr.h"

#include <cstdint>
#include <functional>
#include <set>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct AssetPackFile
{
	enum class Kind
	{
		Texture,
		Model
	};
	Kind kind = Kind::Texture;
	std::string pack;     // the pack folder's name
	std::string relative; // the file below the pack folder, '/' separators, on-disk case
	std::string diskPath;
	std::string extension; // lower case, without the dot ("dds", "w3d")
	std::uint64_t size = 0;
	long long mtime = 0;
};

class AssetPacks
{
public:
	// Adds one pack folder after the ones added before (a later pack wins a name). False + errors (nothing of the pack added) when the folder
	// cannot be read or holds a name twice.
	bool addPack(const std::string &folder, std::vector<std::string> &errors);

	bool empty() const { return m_suspended || (m_textures.empty() && m_models.empty()); }
	// lane SDK-1b (a): packs are offline only for now: a LAN or online game suspends them (every lookup answers nothing, so every peer draws the
	// game's own assets) until the online checks exist. The packs stay loaded; loaded() says whether there are any.
	void setSuspended(bool on) { m_suspended = on; }
	bool suspended() const { return m_suspended; }
	bool loaded() const { return !m_textures.empty() || !m_models.empty(); }
	// the replacement of a W3D texture name ("GUMaArms.tga", "gumaarms": the extension and the case do not matter), or nullptr
	const AssetPackFile *texture(const std::string &textureName) const;
	// the replacement of a W3D file, by any virtual path ending in its file name ("art\w3d\gu\gumaarms_skn.w3d"), or nullptr
	const AssetPackFile *model(const std::string &w3dPath) const;
	// reads a pack file, checking it is still the file listed at load (size and last-write time)
	static bool read(const AssetPackFile &file, std::vector<std::uint8_t> &out, std::string *error);

	const std::vector<std::string> &packs() const { return m_packs; }
	size_t textureCount() const { return m_textures.size(); }
	size_t modelCount() const { return m_models.size(); }
	// "<pack>: <file> replaces <earlier pack>'s" lines (a later pack winning a name) and the files no lookup can use (unknown extensions)
	const std::vector<std::string> &notes() const { return m_notes; }
	static const char *notCovered();

	// lane SDK-1: checks every replacement model against the file it replaces in `base` (the mounted game and mods, the simulation's view):
	// a replacement must keep the original's hierarchies (bone names, order and parents), its HLODs' hierarchy and its animations, since the
	// animations, the effects' bones and the logic's launch bones (which keep reading the original) all name them. A model that does not
	// fit is left out with a note naming why (the game's model is drawn: the rest of the pack still loads); a model replacing no file is a
	// note. False + errors when a pack model cannot be read or is not a W3D file.
	bool checkModels(W3DFileSource &base, std::vector<std::string> &errors);
	// the reasons `replacement` does not fit `original` (empty = it fits); the checks of checkModels
	static std::vector<std::string> compareSkeletons(const W3DFileContents &original, const W3DFileContents &replacement);
	// lane SDK-1b (b): the axis-aligned box of what the renderer draws of a W3D file named `stem` (each HLOD's top LOD and aggregates, a sub-object
	// from this file; no HLOD: its meshes), in model space: the corners of every triangle with an area, of every mesh that reaches the screen,
	// through the rest pose of its hierarchy (a skin's vertex by its influence bone, a rigid mesh by its HLOD bone). `hierarchy` answers a
	// hierarchy by name when the file holds none. False when nothing is drawn.
	static bool modelBounds(const W3DFileContents &file, const std::string &stem, const std::function<const HTreeClass *(const std::string &)> &hierarchy,
		float mn[3], float mx[3]);
	// lane SDK-1b (b): why `replacement` may not replace `original` (a file named `stem`) online; empty = it may (the model rules below)
	// `coverage` answers the see-through share (alphaCoverage) of a texture stem: the game's own file (`original`) or the one drawn (a pack's
	// own, else the game's); false + error when it cannot be read
	using TextureCoverage = std::function<bool(const std::string &stem, bool original, double &share, std::string &error)>;
	// `originalHierarchy` / `drawnHierarchy` answer a hierarchy the file does not hold: the game's, and the one drawn (a pack's own, else the game's)
	static std::string onlineModelCheck(const W3DFileContents &original, const W3DFileContents &replacement, const std::string &stem,
		const std::function<const HTreeClass *(const std::string &)> &originalHierarchy,
		const std::function<const HTreeClass *(const std::string &)> &drawnHierarchy, const TextureCoverage &coverage);
	// the image decoder checkModels hands onlineModelCheck (the Godot side's); none set: a see-through pack mesh is refused online, saying so
	void setTextureCoverage(TextureCoverage coverage) { m_coverage = std::move(coverage); }

	// ---- lane SDK-1b (b): the online rules -------------------------------------------------------------------------------------------------
	// Packs are invisible to the lobby and replay checksums, so in a LAN / online game a pack may not change what one player sees that another
	// does not. For now a LAN / online game suspends every pack (setSuspended, game.gd); these rules only report (onlineRefusals, the
	// launcher's card: "would be refused online"), and setOnline applies them for the allowlist of checked packs to come. They cannot prove a
	// pack harmless on their own (a mesh can map its texture onto see-through pixels, or blend an opaque texture away). Every refusal has its
	// reason:
	//  * the deny-list (onlineDenyList): visibility-relevant assets named from the retail data;
	//  * a model or skeleton (onlineModelCheck, from checkModels) whose hierarchy's rest pose moves a bone over kOnlineMaxBoneMove of the
	//    skeleton's size or turns it over kOnlineMaxTurnDegrees; whose animations move a bone that far from the original's in any of its
	//    frames (of the animation's reach), turn it over kOnlineMaxAnimTurnDegrees, or show, hide or fade it otherwise; that draws another file's model or an emitter the original does not; that draws a
	//    see-through surface (alpha test, blending) with a texture over kOnlineMaxAlphaDelta more see-through than the original's most
	//    see-through surface's texture (none: solid), or with an opacity over kOnlineMaxAlphaDelta lower; that hides a mesh the original shows
	//    (or the reverse); whose drawn box (modelBounds, the rest pose) is over kOnlineMaxScale times
	//    or under 1 / kOnlineMaxScale of the original's on any axis, or whose centre moved over kOnlineMaxOffset of the original's largest
	//    size (at least 1 unit); or that draws nothing;
	//  * a texture retail draws with alpha test or blending whose see-through share differs from the original's by over kOnlineMaxAlphaDelta
	//    (refuseOnline, from the Godot side: GodotRetailFileSystem decodes the images; alphaCoverage, alphaTextureIndex); one whose original
	//    cannot be read and decoded is refused too.
	// The thresholds come from the real RotWK 2.02 HD Edition (test_sdk1_packs.cpp): its honest models stay within 1.71 (95th percentile),
	// their centres within 0.376 of the original's size, its skeletons' bones within 0.000002 of their size and 0.14 degrees, its
	// animations' bones within 11 degrees, and its alpha-drawn textures within 0.107; online it keeps all but 21 of its files (three siege
	// ball LODs 5.2 times the size, one model 0.12 times, a fur mesh and three banner LODs drawn more see-through, and 13 animations of the
	// dwarf ram made for its own skeleton, which is left out).
	static constexpr float kOnlineMaxScale = 3.0f;
	static constexpr float kOnlineMaxOffset = 0.5f;
	static constexpr float kOnlineMaxBoneMove = 0.1f;
	static constexpr float kOnlineMaxTurnDegrees = 10.0f;
	static constexpr float kOnlineMaxAnimTurnDegrees = 30.0f;
	static constexpr double kOnlineMaxAlphaDelta = 0.25;
	struct DenyEntry
	{
		const char *name; // a texture stem or a model file stem, lower case
		bool model;
		const char *why;
	};
	static const std::vector<DenyEntry> &onlineDenyList();
	struct Refusal
	{
		std::string pack, relative, why;
		bool model = false;
	};
	void setOnline(bool on) { m_online = on; }
	bool online() const { return m_online; }
	// refuses a pack texture (by stem) or model (by file name) online; the deny-list is applied by addPack, the size check by checkModels
	void refuseOnline(bool model, const std::string &key, const std::string &why);
	const std::vector<Refusal> &onlineRefusals() const { return m_onlineRefusals; }
	bool refusedOnline(bool model, const std::string &key) const { return (model ? m_refusedModels : m_refusedTextures).count(key) != 0; }
	// the models left out because they do not fit the original (checkModels), for the launcher's card
	const std::vector<Refusal> &misfits() const { return m_misfits; }
	const std::map<std::string, AssetPackFile> &textures() const { return m_textures; }
	const std::map<std::string, AssetPackFile> &models() const { return m_models; }
	// the share of pixels whose alpha is under 128 (8-bit RGBA, `pixels` of them)
	static double alphaCoverage(const std::uint8_t *rgba, std::size_t pixels);
	// every texture stem (lower case) that some retail mesh draws with alpha test or alpha blending (its fixed-function shaders or its FX
	// shader material's AlphaTestEnable / BlendMode); scans every art\w3d file of `fs`
	static std::set<std::string> alphaTextureIndex(ArchiveFileSystem &fs);

private:
	std::vector<std::string> m_packs;
	std::map<std::string, AssetPackFile> m_textures; // texture stem, lower case
	std::map<std::string, AssetPackFile> m_models;   // file name, lower case ("gumaarms_skn.w3d")
	std::vector<std::string> m_notes;
	bool m_suspended = false;
	bool m_online = false;
	TextureCoverage m_coverage;
	std::map<std::string, std::string> m_refusedTextures, m_refusedModels; // key -> why (online only)
	std::vector<Refusal> m_onlineRefusals, m_misfits;
};

// The render side's W3D source with the packs in front (models only; textures go through W3DMaterialFactory::Set_Asset_Packs). Never give it
// to the simulation's asset manager (LiveGame's m_simAssetSource): see "Why cosmetic only" above.
class AssetPackW3DFileSource : public W3DFileSource
{
public:
	AssetPackW3DFileSource(std::unique_ptr<W3DFileSource> base, const AssetPacks &packs) : Base(std::move(base)), Packs(packs) {}
	bool Exists(const std::string &path) const override { return Packs.model(path) != nullptr || Base->Exists(path); }
	bool Read(const std::string &path, std::vector<std::uint8_t> &out, std::string *error) override
	{
		if (const AssetPackFile *f = Packs.model(path))
		{
			return AssetPacks::read(*f, out, error);
		}
		return Base->Read(path, out, error);
	}
	bool FlatAssetLayout() const override { return Base->FlatAssetLayout(); }

private:
	std::unique_ptr<W3DFileSource> Base;
	const AssetPacks &Packs;
};
