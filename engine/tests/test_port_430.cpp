// OpenBFME. GPL-3.0.
// PORT-430 (fork issue 430, P-109): a model texture named `None` (GBFARM_BIB / ANGPORTER.BUCKET / CAH_skull).
//
// What RotWK 2.01 does with such a name, read in `game.dat` (MD5 `daed3668006cd90f01c34e5a7da1901f`) with
// `tools/mod/rw.py dis|xref|str`:
//   * The mesh chunk W3D_CHUNK_TEXTURE_NAME carries the name as written; the data spells it `None`.
//   * The compiled-texture loader RW 0x530D29 (reached from the texture creation RW 0x531BF9 -> RW 0x53193E) copies the
//     name (RW 0x530D40), finds the last dot with strrchr (RW 0x530D45) and, only when it found one, writes the
//     candidates after it: .dds (RW 0x530D61, string RW 0xBDC7BC), .tga (RW 0x530D82, string RW 0xBDC7EC), .jpg
//     (RW 0x530DA3, string RW 0xBDC7B4) and the second stream .png (RW 0x530DC4, string RW 0xBDC7C4). Without a dot
//     `test edi, edi / je 0x530DE1` (RW 0x530D59) returns with both file pointers (+0x10 file, +0x1C alpha) still null.
//   * A null file pointer is exactly the state in which the texture creation RW 0x53193E builds the 1 x 1 texture of
//     format 0x15 filled with 0xFFFF00FF (RW 0x531958 -> RW 0x531B02 -> RW 0x531B07 .. 0x531B44, pixel push
//     RW 0x531B44). That is retail's whole behaviour for the name: one magenta texel, no file access.
//   * Retail logs nothing: `game.dat` 2.01 holds no "texture not found" string at all (every string of the image
//     scanned). "Unable to open %s" (RW 0xBEAC50) is the file system's own line, and its one caller RW 0x575D79 is not
//     a texture path.
//   * `None` also is the engine's own word for "the value is absent": AsciiString::isNone (RW 0x437720) pushes
//     RW 0xBD3BF8 (`None`), calls the _stricmp body RW 0x4372C0 and turns the result into a bool; its callers drop what
//     they tested (RW 0x4C218C, RW 0x4C31BE, RW 0x4C7B44, RW 0x4C821F in the draw modules, RW 0x4B62EC in the
//     model-condition parser) and never look a file up for it.
// The port used to search `art\compiledtextures\no\none.*` and print "texture not found: None" — a report for a
// lookup retail never makes. The fix is one flag in the shared loader (`TextureResolution::Probed`,
// Libraries/WWVegas/WW3D2/textureloader.{h,cpp}): a name without a dot is not probed, so the two consumers keep
// drawing retail's magenta (RW 0x53193E) and stop reporting.
// Synthetic data only: no retail file is read, so these tests pass without the game and the mod.

#include "doctest.h"

#include "Libraries/WWVegas/WW3D2/assetmgr.h"
#include "Libraries/WWVegas/WW3D2/meshmdl.h"
#include "Libraries/WWVegas/WW3D2/textureloader.h"
#include "W3dSynth.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace
{

// A file system with exactly the files it is given, recording every path it is asked about: the record is the proof
// that a name is not looked up at all (retail's loader builds no candidate without a dot, RW 0x530D59).
struct RecordingFileSystem
{
	std::vector<std::string> Files;
	std::vector<std::string> Asked;

	std::function<bool(const std::string &)> Callback()
	{
		return [this](const std::string &path) {
			Asked.push_back(path);
			for (const std::string &f : Files)
			{
				if (f == path)
				{
					return true;
				}
			}
			return false;
		};
	}

	bool askedFor(const std::string &needle) const
	{
		for (const std::string &a : Asked)
		{
			if (a.find(needle) != std::string::npos)
			{
				return true;
			}
		}
		return false;
	}
};

// An FX material chunk list (W3D_CHUNK_SHADER_MATERIALS) with one material and its string properties: the shape of
// CAH_skull's NormalMapped.fx material, whose DiffuseTexture parameter is `None`.
std::vector<std::uint8_t> fxMaterials(const std::string &typeName, const std::vector<std::pair<std::string, std::string>> &textureParams)
{
	w3dtest::ChunkWriter list_;
	w3dtest::ChunkWriter material;
	w3dtest::ChunkWriter one;
	W3dShaderMaterialHeaderStruct header = {};
	header.Number = 0;
	w3dtest::setName(header.TypeName, sizeof(header.TypeName), typeName.c_str());
	one.chunk(W3D_CHUNK_SHADER_MATERIAL_HEADER, w3dtest::ChunkWriter::of(header));
	for (const std::pair<std::string, std::string> &p : textureParams)
	{
		w3dtest::ChunkWriter body;
		body.u32(W3DSHADERMATERIAL_PROPERTY_TEXTURE);
		// property name: u32 length (with the NUL) then the characters
		const std::string &name = p.first;
		body.u32((std::uint32_t)(name.size() + 1));
		for (char c : name) body.u8((std::uint8_t)c);
		body.u8(0);
		// value: the same string shape
		const std::string &value = p.second;
		body.u32((std::uint32_t)(value.size() + 1));
		for (char c : value) body.u8((std::uint8_t)c);
		body.u8(0);
		one.chunk(W3D_CHUNK_SHADER_MATERIAL_PROPERTY, body.bytes);
	}
	material.wrapper(W3D_CHUNK_SHADER_MATERIAL, one);
	list_.wrapper(W3D_CHUNK_SHADER_MATERIALS, material);
	return list_.bytes;
}

// One triangle, one texture entry, pass 0 stage 0 names it: the shape of the retail meshes that carry `None`.
w3dsynth::SynthMesh oneTriangleMesh(const std::string &meshName, const std::string &container, const std::string &textureName)
{
	w3dsynth::SynthMesh m;
	m.Name = meshName;
	m.Container = container;
	m.Vertices = { { 0.0f, 0.0f, 0.0f }, { 10.0f, 0.0f, 0.0f }, { 0.0f, 10.0f, 0.0f } };
	m.Triangles = { 0, 1, 2 };
	W3dShaderStruct shader = {};
	shader.DepthCompare = 3;
	shader.DepthMask = 1;
	shader.ColorMask = 1;
	shader.PriGradient = 1;
	shader.Texturing = 1;
	shader.SrcBlend = 1;
	shader.DestBlend = 0;
	m.Shaders.push_back(shader);
	w3dsynth::SynthVMat vm;
	vm.Name = "VM";
	m.VertexMaterials.push_back(vm);
	w3dsynth::SynthTexture tex;
	tex.Name = textureName;
	m.Textures.push_back(tex);
	w3dsynth::SynthPass pass;
	pass.VertexMaterialIds = { 0 };
	pass.ShaderIds = { 0 };
	w3dsynth::SynthStage stage;
	stage.TextureIds = { 0 };
	pass.Stages.push_back(stage);
	m.Passes.push_back(pass);
	return m;
}

} // namespace

TEST_CASE("PORT-430 a name without an extension is the name retail's loader never opens")
{
	// RW 0x530D29 replaces the three characters after the last dot (RW 0x530D45 strrchr); with no dot it builds no
	// candidate at all (RW 0x530D59). `None` is such a name.
	CHECK(W3D_Texture_Name_Is_Probed("none.tga"));
	CHECK(W3D_Texture_Name_Is_Probed("GUShadowFaxH.tga"));
	CHECK(W3D_Texture_Name_Is_Probed("AWSkull01.tga"));
	CHECK_FALSE(W3D_Texture_Name_Is_Probed("None"));
	CHECK_FALSE(W3D_Texture_Name_Is_Probed("NONE")); // isNone compares without case (RW 0x4372C0's _stricmp)
	CHECK_FALSE(W3D_Texture_Name_Is_Probed(""));
}

TEST_CASE("PORT-430 `None` is never looked up, and no file of that name is used")
{
	RecordingFileSystem fs;
	fs.Files = { "art\\compiledtextures\\no\\none.dds", "art\\compiledtextures\\no\\none.tga", "art\\textures\\none.tga" };

	const TextureResolution res = Resolve_Packed_W3D_Texture("None", fs.Callback());
	CHECK_FALSE(res.Probed); // no dot: RW 0x530D59 leaves both file pointers null
	CHECK_FALSE(res.Found);
	CHECK(res.Path.empty());
	CHECK(res.Tried.empty());
	// The loader is not even asked: retail would open `None.dds` / `None.tga` / `None.jpg` / `None.png` for no name of
	// this shape, so a file that happens to carry that name is not what the model shows (RW 0x53193E's magenta is).
	CHECK(fs.Asked.empty());

	// The neighbour: a real texture name still walks the loader's candidates, so the flag does not short-circuit the
	// ordinary path (RW 0x530D61 .dds, RW 0x530D82 .tga, RW 0x530DA3 .jpg, RW 0x530DC4 .png).
	RecordingFileSystem packed;
	packed.Files = { "art\\compiledtextures\\gu\\gushadowfaxh.jpg" };
	const TextureResolution found = Resolve_Packed_W3D_Texture("GUShadowFaxH.tga", packed.Callback());
	CHECK(found.Probed);
	CHECK(found.Found);
	CHECK(found.Path == "art\\compiledtextures\\gu\\gushadowfaxh.jpg");
	CHECK(packed.askedFor("gushadowfaxh.dds"));
	CHECK(packed.askedFor("gushadowfaxh.tga"));
}

TEST_CASE("PORT-430 an ordinary missing texture still walks every candidate (the fix does not shortcut real misses)")
{
	// A name WITH an extension that resolves to nothing stays a miss the caller reports, exactly as before this change:
	// the four candidates are the ones the loader builds (RW 0x530D61 / 0x530D82 / 0x530DA3 / 0x530DC4).
	RecordingFileSystem fs;
	const TextureResolution res = Resolve_Packed_W3D_Texture("CUCrow.tga", fs.Callback());
	CHECK(res.Probed);
	CHECK_FALSE(res.Found);
	CHECK(fs.askedFor("cucrow.dds"));
	CHECK(fs.askedFor("cucrow.tga"));
	CHECK(fs.askedFor("cucrow.jpg"));
	REQUIRE(res.Tried.size() == 4);
	CHECK(res.Tried[0] == "art\\compiledtextures\\cu\\cucrow.dds");
	CHECK(res.Tried[1] == "art\\compiledtextures\\cu\\cucrow.tga");
	CHECK(res.Tried[3] == "art\\compiledtextures\\cu\\cucrow.jpg");
}

TEST_CASE("PORT-430 a mesh whose TEXTURE_NAME is `None` parses with the name kept")
{
	// The fix is in the loader, not in the mesh reader: the mesh reader stores the chunk's string as written (the port's
	// MeshModelClass::read_textures, meshmdlio.cpp; retail's is the reader RW 0x18AE50's sibling), and the material stage
	// hands that string to the loader, where RW 0x530D59 decides there is no file.
	const std::vector<std::uint8_t> bytes = w3dsynth::meshChunk(oneTriangleMesh("GBFARM_BIB", "EUSKLADN_A", "None"));
	W3DFileContents contents;
	std::string error;
	REQUIRE(Load_W3D_File(bytes.data(), bytes.size(), contents, &error));
	REQUIRE(contents.Meshes.size() == 1);
	const MeshModelClass &mesh = contents.Meshes[0];
	REQUIRE(mesh.Textures.size() == 1);
	CHECK(mesh.Textures[0].Name == "None");
	REQUIRE(mesh.Passes.size() == 1);
	REQUIRE(mesh.Passes[0].Stages.size() == 1);
	REQUIRE(mesh.Passes[0].Stages[0].TextureIds.size() == 1);
	CHECK(mesh.Passes[0].Stages[0].TextureIds[0] == 0);
	CHECK_FALSE(W3D_Texture_Name_Is_Probed(mesh.Textures[0].Name));
}

TEST_CASE("PORT-430 the FX parameter `None` of CAH_skull reaches the loader as a name it does not open")
{
	// CAH_skull's texture comes from an FX material parameter, not from a TEXTURE_NAME chunk (P-99: NormalMapped.fx
	// names `None` beside AWSkull01.tga). Both paths end in the same loader, so one flag covers both: the parameter's
	// string is handed to the resolver unchanged and nothing is looked up for it, while the material's real parameter
	// keeps its own lookup.
	w3dsynth::SynthMesh mesh = oneTriangleMesh("CAH_SKULL", "CAH_skull", "AWCape.tga");
	mesh.Passes[0].ShaderMaterialIds = { 0 };
	const std::vector<std::uint8_t> plain = w3dsynth::meshChunk(mesh);

	// the helper writes one W3D_CHUNK_MESH wrapper; the FX material list is a child of it, so append it to that body
	const std::uint32_t meshId = (std::uint32_t)plain[0] | ((std::uint32_t)plain[1] << 8) | ((std::uint32_t)plain[2] << 16) | ((std::uint32_t)plain[3] << 24);
	REQUIRE(meshId == W3D_CHUNK_MESH);
	const std::uint32_t declared = (std::uint32_t)plain[4] | ((std::uint32_t)plain[5] << 8) | ((std::uint32_t)plain[6] << 16) | ((std::uint32_t)plain[7] << 24);
	REQUIRE((declared & 0x80000000u) == 0x80000000u); // the wrapper bit: the helper writes a chunk with children
	REQUIRE((declared & 0x7FFFFFFFu) == plain.size() - 8); // and the body length is the whole file after the header

	const std::vector<std::uint8_t> fx = fxMaterials("NormalMapped.fx", { { "DiffuseTexture", "None" }, { "NormalMap", "AWSkull01.tga" } });
	w3dtest::ChunkWriter file;
	file.u32(W3D_CHUNK_MESH);
	file.u32((std::uint32_t)((plain.size() - 8) + fx.size()) | 0x80000000u);
	file.bytes.insert(file.bytes.end(), plain.begin() + 8, plain.end());
	file.bytes.insert(file.bytes.end(), fx.begin(), fx.end());

	W3DFileContents contents;
	std::string error;
	REQUIRE(Load_W3D_File(file.bytes.data(), file.bytes.size(), contents, &error));
	REQUIRE(contents.Meshes.size() == 1);
	const MeshModelClass &loaded = contents.Meshes[0];
	REQUIRE(loaded.ShaderMaterials.size() == 1);
	const MeshShaderMaterialDef &material = loaded.ShaderMaterials[0];
	CHECK(material.TypeName == "NormalMapped.fx");
	REQUIRE(material.Properties.size() == 2);
	CHECK(material.Properties[0].Name == "DiffuseTexture");
	CHECK(material.Properties[0].StringValue == "None");
	CHECK(material.Properties[1].Name == "NormalMap");
	CHECK(material.Properties[1].StringValue == "AWSkull01.tga");

	RecordingFileSystem fs;
	fs.Files = { "art\\compiledtextures\\no\\none.tga" };
	const TextureResolution diffuse = Resolve_Packed_W3D_Texture(material.Properties[0].StringValue, fs.Callback());
	CHECK_FALSE(diffuse.Probed);
	CHECK_FALSE(diffuse.Found);
	CHECK(fs.Asked.empty());

	// the material's real texture parameter keeps its own lookup, so only the `None` parameter changes
	const TextureResolution normal = Resolve_Packed_W3D_Texture(material.Properties[1].StringValue, fs.Callback());
	CHECK(normal.Probed);
	CHECK(fs.askedFor("awskull01.dds"));
}
