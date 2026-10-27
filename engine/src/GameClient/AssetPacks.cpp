// OpenBFME. GPL-3.0.
// See GameClient/AssetPacks.h (lane SDK-0): cosmetic drag-and-drop asset packs.

#include "GameClient/AssetPacks.h"

#include "Common/AsciiString.h"
#include "Common/ArchiveFileSystem.h"
#include "Libraries/WWVegas/WW3D2/shader.h"

#include <cmath>
#include <cstdio>
#include <memory>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <system_error>

namespace fs = std::filesystem;

namespace
{

bool isTextureExtension(const std::string &e)
{
	return e == "dds" || e == "tga" || e == "png" || e == "jpg";
}

// lane SDK-1: the text a pack may carry besides its assets (its readme, its licence, the credits): never read by the game
bool isPackTextFile(const std::string &lowerName, const std::string &extension)
{
	return extension == "txt" || extension == "md" || lowerName == "openbfme-pack.json" || lowerName == "license" || lowerName == "licence" ||
		lowerName == "readme";
}

// the last path component, without the folders ('\' or '/')
std::string fileNameOf(const std::string &path)
{
	const size_t slash = path.find_last_of("\\/");
	return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string stemOf(const std::string &fileName)
{
	const size_t dot = fileName.rfind('.');
	return dot == std::string::npos ? fileName : fileName.substr(0, dot);
}

} // namespace

const char *AssetPacks::notCovered()
{
	return "asset packs replace W3D model files and the textures of models, particles, decals, roads, rivers, water and the sky; the "
		   "terrain tile textures, UI (Apt) textures and non-W3D model formats are not replaced yet (lane SDK-1)";
}

std::vector<std::string> AssetPacks::compareSkeletons(const W3DFileContents &original, const W3DFileContents &replacement)
{
	std::vector<std::string> out;
	auto same = [](const std::string &a, const std::string &b) { return AsciiStringUtil::lowered(a) == AsciiStringUtil::lowered(b); };
	for (const HTreeClass &o : original.HTrees)
	{
		const HTreeClass *r = replacement.Find_HTree(o.Get_Name());
		if (!r)
		{
			out.push_back("it has no hierarchy " + o.Get_Name() + " (the original's skeleton)");
			continue;
		}
		if (r->Num_Pivots() != o.Num_Pivots())
		{
			out.push_back("its hierarchy " + o.Get_Name() + " has " + std::to_string(r->Num_Pivots()) + " bones, the original's " +
				std::to_string(o.Num_Pivots()));
			continue;
		}
		for (int i = 0; i < o.Num_Pivots(); ++i)
		{
			if (!same(r->Get_Pivot(i).Name, o.Get_Pivot(i).Name) || r->Get_Pivot(i).ParentIdx != o.Get_Pivot(i).ParentIdx)
			{
				out.push_back("its hierarchy " + o.Get_Name() + ": bone " + std::to_string(i) + " is " + r->Get_Pivot(i).Name + " (parent " +
					std::to_string(r->Get_Pivot(i).ParentIdx) + "), the original's " + o.Get_Pivot(i).Name + " (parent " +
					std::to_string(o.Get_Pivot(i).ParentIdx) + ")");
				break;
			}
		}
	}
	for (const HLodDefClass &o : original.HLods)
	{
		const HLodDefClass *r = replacement.Find_HLod(o.Name);
		if (!r)
		{
			out.push_back("it has no model " + o.Name + " (the original's HLOD)");
		}
		else if (!same(r->HierarchyName, o.HierarchyName))
		{
			out.push_back("its model " + o.Name + " uses the hierarchy " + r->HierarchyName + ", the original's " + o.HierarchyName);
		}
	}
	auto anims = [&](const auto &list) {
		for (const auto &a : list)
		{
			if (!replacement.Find_Animation(a.Get_Name()))
			{
				out.push_back("it has no animation " + a.Get_Name());
			}
		}
	};
	anims(original.RawAnims);
	anims(original.CompressedAnims);
	return out;
}

bool AssetPacks::checkModels(W3DFileSource &base, std::vector<std::string> &errors)
{
	const size_t errorsBefore = errors.size();
	std::vector<std::string> misfits;
	for (const auto &e : m_models)
	{
		const AssetPackFile &f = e.second;
		const std::string label = "asset pack " + f.pack + ": " + f.relative;
		std::vector<std::uint8_t> mine, theirs;
		std::string error;
		W3DFileContents replacement, original;
		if (!read(f, mine, &error))
		{
			errors.push_back(error);
			continue;
		}
		if (!Load_W3D_File(mine.data(), mine.size(), replacement, &error))
		{
			errors.push_back(label + " is not a W3D file the game can read: " + error);
			continue;
		}
		const std::string path = W3D_Asset_Path(stemOf(e.first), base.FlatAssetLayout());
		if (!base.Exists(path))
		{
			m_notes.push_back(f.pack + ": " + f.relative + " replaces no model of the game or the mod (" + path + "): not used");
			misfits.push_back(e.first); // review r1: not used means not drawn (a hierarchy or HLOD naming a missing file would reach it)
			continue;
		}
		if (!base.Read(path, theirs, &error) || !Load_W3D_File(theirs.data(), theirs.size(), original, &error))
		{
			errors.push_back(label + ": cannot read the model it replaces, " + path + ": " + error);
			continue;
		}
		const std::vector<std::string> why = compareSkeletons(original, replacement);
		if (!why.empty())
		{
			std::string all;
			for (const std::string &w : why)
			{
				all += (all.empty() ? "" : "; ") + w;
			}
			m_notes.push_back(f.pack + ": " + f.relative + " does not fit " + path + " (" + all + "): not used, the game's model is drawn (a "
				"replacement model keeps the original's skeleton, models and animations, or the game's animations and effects would not fit it)");
			m_misfits.push_back(Refusal{ f.pack, f.relative, "different bones or models than the game's (" + all + ")", true });
			misfits.push_back(e.first);
			continue;
		}
		// lane SDK-1b (b): the online rules for models (onlineModelCheck). A hierarchy the file does not hold comes from its own file: the
		// game's for the original, the one drawn (this pack's, else the game's) for the replacement
		std::map<std::pair<std::string, bool>, std::unique_ptr<W3DFileContents>> trees;
		auto treeFrom = [&](const std::string &name, bool drawn) -> const HTreeClass * {
			const std::string k = AsciiStringUtil::lowered(name);
			auto it = trees.find({ k, drawn });
			if (it == trees.end())
			{
				auto c = std::make_unique<W3DFileContents>();
				std::vector<std::uint8_t> hb;
				std::string he;
				const auto own = m_models.find(k + ".w3d");
				const std::string hp = W3D_Asset_Path(k, base.FlatAssetLayout());
				if (drawn && own != m_models.end() ? !read(own->second, hb, &he) : (!base.Exists(hp) || !base.Read(hp, hb, &he)))
					c.reset();
				else if (!Load_W3D_File(hb.data(), hb.size(), *c, &he))
					c.reset();
				it = trees.emplace(std::make_pair(k, drawn), std::move(c)).first;
			}
			return it->second ? it->second->Find_HTree(name) : nullptr;
		};
		auto originalTree = [&](const std::string &name) { return treeFrom(name, false); };
		auto drawnTree = [&](const std::string &name) { return treeFrom(name, true); };
		// a mod file never stops the game: a model the checks cannot read through is left out, named (review r1: a compressed animation
		// throws UndefinedFrameError before its first key)
		std::string online;
		try
		{
			online = onlineModelCheck(original, replacement, stemOf(e.first), originalTree, drawnTree, m_coverage);
		}
		catch (const std::exception &x)
		{
			m_notes.push_back(f.pack + ": " + f.relative + " cannot be checked (" + x.what() + "): not used, the game's model is drawn");
			m_misfits.push_back(Refusal{ f.pack, f.relative, std::string("it cannot be checked: ") + x.what(), true });
			misfits.push_back(e.first);
			continue;
		}
		if (!online.empty())
			refuseOnline(true, e.first, online);
	}
	for (const std::string &k : misfits)
	{
		m_models.erase(k);
	}
	return errors.size() == errorsBefore;
}

bool AssetPacks::addPack(const std::string &folder, std::vector<std::string> &errors)
{
	const size_t errorsBefore = errors.size();
	std::error_code ec;
	const fs::path root = fs::u8path(folder);
	if (!fs::is_directory(root, ec))
	{
		errors.push_back("asset pack folder does not exist: '" + folder + "'");
		return false;
	}
	std::string name = root.filename().u8string();
	if (name.empty())
	{
		name = root.parent_path().filename().u8string();
	}
	std::map<std::string, AssetPackFile> textures, models;
	std::vector<std::string> notes;
	std::vector<fs::path> files;
	for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec))
	{
		std::error_code typeEc;
		if (it->is_symlink(typeEc))
		{
			errors.push_back("asset pack " + name + ": symbolic links are not supported: " + it->path().lexically_relative(root).u8string());
			it.disable_recursion_pending();
			continue;
		}
		if (it->is_regular_file(typeEc))
		{
			files.push_back(it->path());
		}
	}
	if (ec)
	{
		errors.push_back("asset pack " + name + ": could not list " + folder + ": " + ec.message());
	}
	std::sort(files.begin(), files.end()); // a fixed order: the duplicate errors and the notes do not depend on the directory order
	for (const fs::path &p : files)
	{
		AssetPackFile f;
		f.pack = name;
		f.relative = p.lexically_relative(root).generic_u8string();
		f.diskPath = p.u8string();
		std::string ext = AsciiStringUtil::lowered(p.extension().u8string());
		f.extension = ext.empty() ? ext : ext.substr(1);
		std::error_code statEc;
		f.size = (std::uint64_t)fs::file_size(p, statEc);
		if (!statEc)
		{
			f.mtime = (long long)fs::last_write_time(p, statEc).time_since_epoch().count();
		}
		if (statEc)
		{
			errors.push_back("asset pack " + name + ": cannot read " + f.relative + ": " + statEc.message());
			continue;
		}
		const std::string lowerName = AsciiStringUtil::lowered(p.filename().u8string());
		std::map<std::string, AssetPackFile> *into = nullptr;
		std::string key;
		if (isTextureExtension(f.extension))
		{
			f.kind = AssetPackFile::Kind::Texture;
			into = &textures;
			key = stemOf(lowerName);
		}
		else if (f.extension == "w3d")
		{
			f.kind = AssetPackFile::Kind::Model;
			into = &models;
			key = lowerName;
		}
		else
		{
			// lane SDK-1: cosmetic only, so a whitelist: anything but a model, a texture or the pack's own text is refused (game data belongs
			// in a mod, which the lobby and the replays fingerprint)
			if (!isPackTextFile(lowerName, f.extension))
			{
				errors.push_back("asset pack " + name + ": " + f.relative + " is not a model or texture (.w3d, .dds, .tga, .png, .jpg): an asset pack "
					"only changes how the game looks; game data (INI, scripts, maps, archives, sounds, programs) belongs in a mod (-mod)");
			}
			continue;
		}
		auto existing = into->find(key);
		if (existing != into->end())
		{
			errors.push_back("asset pack " + name + ": " + existing->second.relative + " and " + f.relative + " replace the same asset '" + key +
				"': keep one");
			continue;
		}
		(*into)[key] = f;
	}
	if (errors.size() != errorsBefore)
	{
		return false;
	}
	auto merge = [&notes, &name](const std::map<std::string, AssetPackFile> &from, std::map<std::string, AssetPackFile> &into) {
		for (const auto &e : from)
		{
			auto old = into.find(e.first);
			if (old != into.end())
			{
				notes.push_back(name + ": " + e.second.relative + " replaces " + old->second.pack + "'s " + old->second.relative);
			}
			into[e.first] = e.second;
		}
	};
	merge(textures, m_textures);
	merge(models, m_models);
	// lane SDK-1b (b): the visibility deny-list (online only)
	for (const DenyEntry &d : onlineDenyList())
	{
		if (d.model)
			refuseOnline(true, std::string(d.name) + ".w3d", std::string("a visibility-relevant asset: ") + d.why);
		else
			refuseOnline(false, d.name, std::string("a visibility-relevant asset: ") + d.why);
	}
	m_packs.push_back(name);
	m_notes.insert(m_notes.end(), notes.begin(), notes.end());
	return true;
}

const AssetPackFile *AssetPacks::texture(const std::string &textureName) const
{
	if (m_suspended || m_textures.empty())
	{
		return nullptr;
	}
	const std::string key = stemOf(AsciiStringUtil::lowered(fileNameOf(textureName)));
	if (m_online && m_refusedTextures.count(key))
	{
		return nullptr; // lane SDK-1b (b): offline only
	}
	auto it = m_textures.find(key);
	return it == m_textures.end() ? nullptr : &it->second;
}

const AssetPackFile *AssetPacks::model(const std::string &w3dPath) const
{
	if (m_suspended || m_models.empty())
	{
		return nullptr;
	}
	const std::string key = AsciiStringUtil::lowered(fileNameOf(w3dPath));
	if (m_online && m_refusedModels.count(key))
	{
		return nullptr; // lane SDK-1b (b): offline only
	}
	auto it = m_models.find(key);
	return it == m_models.end() ? nullptr : &it->second;
}

bool AssetPacks::read(const AssetPackFile &file, std::vector<std::uint8_t> &out, std::string *error)
{
	std::error_code ec;
	const fs::path p = fs::u8path(file.diskPath);
	const std::uint64_t size = (std::uint64_t)fs::file_size(p, ec);
	const long long mtime = ec ? 0 : (long long)fs::last_write_time(p, ec).time_since_epoch().count();
	if (ec || size != file.size || mtime != file.mtime)
	{
		if (error)
		{
			*error = "asset pack file changed or vanished since the packs were loaded (restart to use it): " + file.pack + "/" + file.relative;
		}
		return false;
	}
	std::ifstream in(p, std::ios::binary);
	out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	if (out.size() != file.size)
	{
		if (error)
		{
			*error = "cannot read asset pack file " + file.pack + "/" + file.relative;
		}
		return false;
	}
	return true;
}

namespace
{

// lane SDK-1b (b, Sol r1): what the renderer draws of a W3D file named `stem` (WW3DAssetManager::Create_Render_Obj, assetmgr.cpp): each HLOD's
// top LOD (Lod.back()) and its aggregates; a sub-object resolves in this file only when its container is the file's stem (Resolve_Sub_Object
// looks a "container.name" up in the container's own file). A file without an HLOD draws its meshes as bare meshes. The names that resolve
// elsewhere (other files' meshes, emitters, boxes) go to `elsewhere`, lower case.
struct DrawnMesh
{
	const MeshModelClass *mesh;
	const HTreeClass *tree; // nullptr: as it is (a bare mesh)
	int bone;
};

void drawnParts(const W3DFileContents &file, const std::string &stem, const std::function<const HTreeClass *(const std::string &)> &hierarchy,
	std::vector<DrawnMesh> &meshes, std::set<std::string> &elsewhere)
{
	const std::string own = AsciiStringUtil::lowered(stem);
	for (const HLodDefClass &h : file.HLods)
	{
		// the renderer's hierarchy: the one in the file named after it (WW3DAssetManager::Get_HTree), else the default tree (assetmgr.cpp:485)
		static const HTreeClass fallbackTree = HTreeClass::Make_Default();
		const HTreeClass *tree = AsciiStringUtil::lowered(h.HierarchyName) == own ? file.Find_HTree(h.HierarchyName) : hierarchy(h.HierarchyName);
		if (!tree)
			tree = &fallbackTree;
		auto add = [&](const HLodDefClass::SubObjectArrayClass &array) {
			for (size_t i = 0; i < array.ModelName.size(); ++i)
			{
				const std::string name = AsciiStringUtil::lowered(array.ModelName[i]);
				const size_t dot = name.find('.');
				const bool here = dot != std::string::npos && name.substr(0, dot) == own;
				const MeshModelClass *mesh = here ? file.Find_Mesh(name) : nullptr;
				if (here && !mesh && file.Find_Box(name))
					continue; // a collision box (the HUD's and picking's; never drawn)
				if (mesh && tree)
					meshes.push_back(DrawnMesh{ mesh, tree, i < array.BoneIndex.size() ? array.BoneIndex[i] : 0 });
				else
					elsewhere.insert(name);
			}
		};
		if (!h.Lod.empty())
			add(h.Lod.back());
		add(h.Aggregates);
	}
	if (file.HLods.empty())
		for (const MeshModelClass &m : file.Meshes)
			meshes.push_back(DrawnMesh{ &m, nullptr, 0 });
}

// a mesh's surfaces as the renderer draws them (Translate_W3D_Shader; an FX material's AlphaTestEnable / BlendMode)
struct MeshLook
{
	bool drawn = false;   // something reaches the screen: not hidden (the file's flag; a drawable may show it by name), not depth NEVER,
	                      // not (ZERO, ONE)
	bool solid = true;    // every surface opaque, no alpha test, no blending
	float alphaFloor = 1; // the lowest vertex material opacity and vertex colour alpha (they scale the see-through surfaces' alpha)
};

MeshLook lookOf(const MeshModelClass &m)
{
	MeshLook out;
	bool anyShown = m.Shaders.empty();
	for (const W3dShaderStruct &s : m.Shaders)
	{
		const W3DRenderState st = Translate_W3D_Shader(s);
		const bool shown = !st.NeverDraws && st.Blend != W3D_GODOT_BLEND_NONE;
		anyShown = anyShown || shown;
		if (st.AlphaTest || st.Blend != W3D_GODOT_BLEND_OPAQUE)
			out.solid = false;
	}
	for (const MeshShaderMaterialDef &d : m.ShaderMaterials)
		for (const MeshShaderMaterialProperty &p : d.Properties)
		{
			const std::string n = AsciiStringUtil::lowered(p.Name);
			if ((n.find("alphatest") != std::string::npos && (p.Bool || p.Int)) || (n == "blendmode" && p.Int != 0) ||
				(n == "alphablendingenable" && (p.Bool || p.Int))) // simple.fx: drawn MIX (GodotW3DMaterial.cpp)
				out.solid = false;
		}
	out.drawn = anyShown && !m.Triangles.empty() && !m.Is_Hidden();
	for (const MeshVertexMaterialDef &v : m.VertexMaterials)
		out.alphaFloor = std::min(out.alphaFloor, v.Info.Opacity);
	for (const MeshMaterialPassData &pass : m.Passes)
		for (const W3dRGBAStruct &c : pass.DCG)
			out.alphaFloor = std::min(out.alphaFloor, c.A / 255.0f);
	return out;
}

std::set<std::string> texturesOf(const MeshModelClass &m)
{
	std::set<std::string> out;
	for (const MeshTextureDef &t : m.Textures)
		out.insert(stemOf(AsciiStringUtil::lowered(fileNameOf(t.Name))));
	for (const MeshShaderMaterialDef &d : m.ShaderMaterials)
		for (const MeshShaderMaterialProperty &p : d.Properties)
		{
			const std::string n = AsciiStringUtil::lowered(p.Name);
			if (!p.StringValue.empty() && n.find("normal") == std::string::npos && n.find("bump") == std::string::npos &&
				n.find("spec") == std::string::npos && n.find("env") == std::string::npos)
				out.insert(stemOf(AsciiStringUtil::lowered(fileNameOf(p.StringValue))));
		}
	return out;
}

} // namespace

bool AssetPacks::modelBounds(const W3DFileContents &file, const std::string &stem, const std::function<const HTreeClass *(const std::string &)> &hierarchy,
	float mn[3], float mx[3])
{
	bool any = false;
	auto add = [&](const Vector3 &p) {
		const float v[3] = { p.X, p.Y, p.Z };
		for (int i = 0; i < 3; ++i)
		{
			if (!any || v[i] < mn[i])
				mn[i] = v[i];
			if (!any || v[i] > mx[i])
				mx[i] = v[i];
		}
		any = true;
	};
	std::vector<DrawnMesh> meshes;
	std::set<std::string> elsewhere;
	drawnParts(file, stem, hierarchy, meshes, elsewhere);
	for (const DrawnMesh &d : meshes)
	{
		const MeshModelClass &mesh = *d.mesh;
		if (!lookOf(mesh).drawn)
			continue;
		HTreePose pose;
		if (d.tree)
			d.tree->Base_Pose(Matrix3D(), pose);
		auto placed = [&](uint32 v) {
			const W3dVectorStruct &w = mesh.Vertices[v];
			const Vector3 p(w.X, w.Y, w.Z);
			if (!d.tree)
				return p;
			int bone = d.bone;
			if (mesh.Is_Skin() && v < mesh.Influences.size())
				bone = mesh.Influences[v].BoneIdx;
			if (bone < 0 || bone >= pose.Num_Pivots())
				bone = 0;
			return pose.Transform[(size_t)bone].Transform_Point(p);
		};
		// the triangles' corners, a triangle with no area left out: what is drawn, not what is stored
		for (const W3dTriStruct &t : mesh.Triangles)
		{
			if (t.Vindex[0] >= mesh.Vertices.size() || t.Vindex[1] >= mesh.Vertices.size() || t.Vindex[2] >= mesh.Vertices.size())
				continue;
			const Vector3 a = placed(t.Vindex[0]), b = placed(t.Vindex[1]), c = placed(t.Vindex[2]);
			const float u[3] = { b.X - a.X, b.Y - a.Y, b.Z - a.Z }, w[3] = { c.X - a.X, c.Y - a.Y, c.Z - a.Z };
			const float n[3] = { u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0] };
			if (n[0] * n[0] + n[1] * n[1] + n[2] * n[2] <= 1e-12f) // tolerance: a zero-area triangle draws no pixel
				continue;
			add(a);
			add(b);
			add(c);
		}
	}
	return any;
}

std::string AssetPacks::onlineModelCheck(const W3DFileContents &original, const W3DFileContents &replacement, const std::string &stem,
	const std::function<const HTreeClass *(const std::string &)> &originalHierarchy,
	const std::function<const HTreeClass *(const std::string &)> &drawnHierarchy, const TextureCoverage &coverage)
{
	// 0. its hierarchies' rest pose: no bone moved over kOnlineMaxBoneMove of the skeleton's size, or turned over kOnlineMaxTurnDegrees (a
	//    skeleton file replaced alone moves every model on it)
	for (const HTreeClass &o : original.HTrees)
	{
		const HTreeClass *r = replacement.Find_HTree(o.Get_Name());
		if (!r || r->Num_Pivots() != o.Num_Pivots())
			continue; // compareSkeletons' finding
		HTreePose po, pr;
		o.Base_Pose(Matrix3D(), po);
		r->Base_Pose(Matrix3D(), pr);
		float lo[3] = { 0, 0, 0 }, hi[3] = { 0, 0, 0 };
		for (int i = 0; i < po.Num_Pivots(); ++i)
			for (int k = 0; k < 3; ++k)
			{
				lo[k] = std::min(lo[k], po.Transform[(size_t)i].Row[k][3]);
				hi[k] = std::max(hi[k], po.Transform[(size_t)i].Row[k][3]);
			}
		float span = 1.0f;
		for (int k = 0; k < 3; ++k)
			span = std::max(span, hi[k] - lo[k]);
		const float cosTurn = std::cos(kOnlineMaxTurnDegrees * 3.14159265f / 180.0f);
		for (int i = 0; i < po.Num_Pivots(); ++i)
		{
			const Matrix3D &a = po.Transform[(size_t)i], &b = pr.Transform[(size_t)i];
			float d2 = 0.0f;
			for (int k = 0; k < 3; ++k)
				d2 += (b.Row[k][3] - a.Row[k][3]) * (b.Row[k][3] - a.Row[k][3]);
			char buf[220];
			if (std::sqrt(d2) > span * kOnlineMaxBoneMove)
			{
				std::snprintf(buf, sizeof buf, "its hierarchy %s moves the bone %s by %.1f units (online: at most %.0f%% of the skeleton's size)",
					o.Get_Name().c_str(), o.Get_Pivot(i).Name.c_str(), std::sqrt(d2), kOnlineMaxBoneMove * 100.0f);
				return buf;
			}
			for (int c = 0; c < 3; ++c)
			{
				const float dot = a.Row[0][c] * b.Row[0][c] + a.Row[1][c] * b.Row[1][c] + a.Row[2][c] * b.Row[2][c];
				if (dot < cosTurn)
				{
					std::snprintf(buf, sizeof buf, "its hierarchy %s turns the bone %s by over %.0f degrees", o.Get_Name().c_str(),
						o.Get_Pivot(i).Name.c_str(), kOnlineMaxTurnDegrees);
					return buf;
				}
			}
		}
	}
	// 1. its animations: every frame of the original's keeps each bone's place (kOnlineMaxBoneMove of the animation's reach), its turn
	//    (kOnlineMaxAnimTurnDegrees), its visibility and its fade (kOnlineMaxAlphaDelta); a pack animation file can move a unit or hide its meshes
	std::vector<const HAnimClass *> anims;
	for (const HRawAnimClass &a : original.RawAnims)
		anims.push_back(&a);
	for (const HCompressedAnimClass &a : original.CompressedAnims)
		anims.push_back(&a);
	for (const HAnimClass *o : anims)
	{
		const HAnimClass *r = replacement.Find_Animation(o->Get_Name());
		if (!r)
			continue; // compareSkeletons' finding
		const int pivots = std::min(o->Get_Num_Pivots(), r->Get_Num_Pivots());
		float lo[3] = { 0, 0, 0 }, hi[3] = { 0, 0, 0 };
		for (int p = 0; p < pivots; ++p)
			for (int f = 0; f < o->Get_Num_Frames(); ++f)
			{
				if (!o->Frame_Is_Defined(p, (float)f))
					continue;
				Vector3 t;
				o->Get_Translation(t, p, (float)f);
				const float v[3] = { t.X, t.Y, t.Z };
				for (int k = 0; k < 3; ++k)
				{
					lo[k] = std::min(lo[k], v[k]);
					hi[k] = std::max(hi[k], v[k]);
				}
			}
		const float reach = std::max({ 1.0f, hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2] });
		const float cosHalfTurn = std::cos(kOnlineMaxAnimTurnDegrees * 0.5f * 3.14159265f / 180.0f);
		// every frame of either (review r1: a longer replacement plays its extra frames), the shorter one held at its last frame
		const int frames = std::max(o->Get_Num_Frames(), r->Get_Num_Frames());
		for (int p = 0; p < pivots; ++p)
			for (int f = 0; f < frames; ++f)
			{
				const float fo = (float)std::min(f, std::max(0, o->Get_Num_Frames() - 1));
				const float frame = (float)std::min(f, std::max(0, r->Get_Num_Frames() - 1));
				if (!o->Frame_Is_Defined(p, fo))
					continue;
				if (!r->Frame_Is_Defined(p, frame))
				{
					char buf[200];
					std::snprintf(buf, sizeof buf, "its animation %s has no value for bone %d at frame %d where the original has one", o->Get_Name().c_str(), p, f);
					return buf;
				}
				Vector3 ta, tb;
				o->Get_Translation(ta, p, fo);
				r->Get_Translation(tb, p, frame);
				const float d = std::sqrt((tb.X - ta.X) * (tb.X - ta.X) + (tb.Y - ta.Y) * (tb.Y - ta.Y) + (tb.Z - ta.Z) * (tb.Z - ta.Z));
				Quaternion qa, qb;
				const bool ha = o->Get_Orientation(qa, p, fo), hb = r->Get_Orientation(qb, p, frame);
				const float dot = std::fabs(qa.X * qb.X + qa.Y * qb.Y + qa.Z * qb.Z + qa.W * qb.W);
				char buf[260];
				if (d > reach * kOnlineMaxBoneMove)
				{
					std::snprintf(buf, sizeof buf, "its animation %s moves bone %d by %.1f units from the original's at frame %d (online: at most %.0f%% of "
						"the animation's reach, %.1f units)", o->Get_Name().c_str(), p, d, f, kOnlineMaxBoneMove * 100.0f, reach * kOnlineMaxBoneMove);
					return buf;
				}
				if (ha && hb && dot < cosHalfTurn)
				{
					std::snprintf(buf, sizeof buf, "its animation %s turns bone %d %.0f degrees from the original's at frame %d (online: at most %.0f)",
						o->Get_Name().c_str(), p, 2.0f * std::acos(std::min(1.0f, dot)) * 180.0f / 3.14159265f, f, kOnlineMaxAnimTurnDegrees);
					return buf;
				}
				if (o->Get_Visibility(p, fo) != r->Get_Visibility(p, frame) ||
					std::fabs(o->Get_Fade(p, fo) - r->Get_Fade(p, frame)) > (float)kOnlineMaxAlphaDelta)
				{
					std::snprintf(buf, sizeof buf, "its animation %s shows, hides or fades bone %d unlike the original at frame %d", o->Get_Name().c_str(), p, f);
					return buf;
				}
			}
	}
	std::vector<DrawnMesh> theirs, mine;
	std::set<std::string> theirsElsewhere, mineElsewhere;
	drawnParts(original, stem, originalHierarchy, theirs, theirsElsewhere);
	drawnParts(replacement, stem, drawnHierarchy, mine, mineElsewhere);
	// 2. it draws nothing the original does not name from elsewhere (another file's model, an emitter)
	for (const std::string &n : mineElsewhere)
		if (!theirsElsewhere.count(n))
			return "it draws " + n + ", which the original does not";
	// 3. a see-through surface no more see-through than the original's most see-through one: its textures (as drawn) by their share of
	//    see-through pixels, its opacity (the vertex materials and colours); an original drawn solid allows kOnlineMaxAlphaDelta
	double share = 0.0;
	float alphaFloor = 1.0f;
	for (const DrawnMesh &d : theirs)
	{
		const MeshLook l = lookOf(*d.mesh);
		if (!l.drawn || l.solid)
			continue;
		alphaFloor = std::min(alphaFloor, l.alphaFloor);
		for (const std::string &t : texturesOf(*d.mesh))
		{
			double c = 0.0;
			std::string error;
			if (!coverage)
				return "its textures cannot be checked here (no image decoder)";
			if (!coverage(t, true, c, error))
				return "the original's texture " + t + " cannot be checked: " + error;
			share = std::max(share, c);
		}
	}
	for (const DrawnMesh &d : mine)
	{
		const MeshLook l = lookOf(*d.mesh);
		if (!l.drawn || l.solid)
			continue;
		char buf[240];
		if (l.alphaFloor < alphaFloor - (float)kOnlineMaxAlphaDelta)
		{
			std::snprintf(buf, sizeof buf, "its see-through mesh %s has an opacity of %.2f, the original's %.2f (online: at most %.2f lower)",
				d.mesh->Get_Name().c_str(), l.alphaFloor, alphaFloor, kOnlineMaxAlphaDelta);
			return buf;
		}
		for (const std::string &t : texturesOf(*d.mesh))
		{
			double c = 0.0;
			std::string error;
			if (!coverage)
				return "its textures cannot be checked here (no image decoder)";
			if (!coverage(t, false, c, error))
				return "its see-through mesh " + d.mesh->Get_Name() + "'s texture " + t + " cannot be checked: " + error;
			if (c > share + kOnlineMaxAlphaDelta)
			{
				std::snprintf(buf, sizeof buf, "its see-through mesh %s draws the texture %s, %.0f%% see-through; the original's see-through surfaces %.0f%% "
					"(online: at most %.0f points more)", d.mesh->Get_Name().c_str(), t.c_str(), c * 100.0, share * 100.0, kOnlineMaxAlphaDelta * 100.0);
				return buf;
			}
		}
	}
	// 4. a mesh the original draws keeps its hidden flag (a drawable shows and hides sub-objects by name: ShowSubObject / HideSubObject)
	for (const DrawnMesh &d : theirs)
		if (const MeshModelClass *m = replacement.Find_Mesh(d.mesh->Get_Name()))
			if (m->Is_Hidden() != d.mesh->Is_Hidden())
				return "its mesh " + m->Get_Name() + (m->Is_Hidden() ? " is hidden where the original's is shown" : " is shown where the original's is hidden");
	// 5. the drawn box: the size along each axis, and where it sits
	float a0[3], a1[3], b0[3], b1[3];
	if (!modelBounds(original, stem, originalHierarchy, a0, a1))
		return "";
	if (!modelBounds(replacement, stem, drawnHierarchy, b0, b1))
		return "it draws nothing where the original draws a model";
	float span = 1.0f;
	for (int i = 0; i < 3; ++i)
		span = std::max(span, a1[i] - a0[i]);
	for (int i = 0; i < 3; ++i)
	{
		const float r = a1[i] - a0[i], p = b1[i] - b0[i];
		if (r > 0.5f && (p > r * kOnlineMaxScale || p * kOnlineMaxScale < r))
		{
			char buf[160];
			std::snprintf(buf, sizeof buf, "it is %.2f times the original's size along %c (online: at most %.0f times either way)", p / r, "xyz"[i],
				kOnlineMaxScale);
			return buf;
		}
		const float moved = std::fabs((b0[i] + b1[i]) - (a0[i] + a1[i])) * 0.5f;
		if (moved > span * kOnlineMaxOffset)
		{
			char buf[200];
			std::snprintf(buf, sizeof buf, "its centre is %.1f units from the original's along %c (online: at most %.0f%% of the original's size)",
				moved, "xyz"[i], kOnlineMaxOffset * 100.0f);
			return buf;
		}
	}
	return "";
}

// lane SDK-1b (b): visibility-relevant assets, from the retail data (factb-ini data\ini; GameClient sources): online they stay the game's own
const std::vector<AssetPacks::DenyEntry> &AssetPacks::onlineDenyList()
{
	static const std::vector<DenyEntry> list = {
		{ "exfirescroll2", false, "the burning effects play even inside the shroud (fxlist.ini FX_UnitBurningFlame / FX_InfantryBurningFlame / "
								   "FX_CavalryBurningFlame: PlayEvenIfShrouded = Yes, particle system GPUUnitFire; GameClient/FXList.cpp)" },
		{ "exheal", false, "the stealth on / off effect marks where a unit hid or appeared (fxlist.ini FX_ElvenCloakOn: particle system ElvenCloak; "
							"the objects' BecomeStealthedFX)" },
		{ "tscloudmed", false, "the cloud shadows over the map (the maps' default cloud texture; GameClient/MapChunks.cpp)" },
		{ "tsnoiseurb", false, "the macro shading over the map (the maps' default macro texture; GameClient/MapChunks.cpp)" },
		{ "mpbeacon", true, "the multiplayer beacon (object/civilian/civilianprop.ini MultiplayerBeacon)" },
		{ "thering", true, "the One Ring pickup, marked on the radar (crate.ini TheDroppedRing)" },
		{ "expalantirshard", true, "the palantir shard pickups, marked on the radar (crate.ini PalantirShard*)" },
	};
	return list;
}

void AssetPacks::refuseOnline(bool model, const std::string &key, const std::string &why)
{
	std::map<std::string, AssetPackFile> &files = model ? m_models : m_textures;
	std::map<std::string, std::string> &refused = model ? m_refusedModels : m_refusedTextures;
	auto it = files.find(key);
	if (it == files.end() || refused.count(key))
		return;
	refused[key] = why;
	m_onlineRefusals.push_back(Refusal{ it->second.pack, it->second.relative, why, model });
}

double AssetPacks::alphaCoverage(const std::uint8_t *rgba, std::size_t pixels)
{
	if (pixels == 0)
		return 0.0;
	std::size_t see = 0;
	for (std::size_t i = 0; i < pixels; ++i)
		if (rgba[i * 4 + 3] < 128)
			++see;
	return (double)see / (double)pixels;
}

std::set<std::string> AssetPacks::alphaTextureIndex(ArchiveFileSystem &fs)
{
	std::set<std::string> out;
	FilenameList list;
	fs.getFileListInDirectory("art\\w3d\\", "art\\w3d\\", "*.w3d", list, true);
	ArchiveW3DFileSource src(fs);
	auto stem = [](const std::string &name) { return stemOf(AsciiStringUtil::lowered(fileNameOf(name))); };
	for (const std::string &f : list)
	{
		std::vector<std::uint8_t> bytes;
		std::string error;
		W3DFileContents c;
		if (!src.Read(f, bytes, &error) || !Load_W3D_File(bytes.data(), bytes.size(), c, &error))
			continue;
		for (const MeshModelClass &m : c.Meshes)
		{
			const MeshLook look = lookOf(m);
			const bool alpha = look.drawn && !look.solid;
			if (!alpha)
				continue;
			const std::set<std::string> t = texturesOf(m);
			out.insert(t.begin(), t.end());
		}
	}
	return out;
}
