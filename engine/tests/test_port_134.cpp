// OpenBFME. PORT-134 (issue #336): the pick and the aligned box of a W3D model.
//
// The problem: Eomer's summoned head on a pike (the mod's OCL_SpawnGrenzmarkierung object) is drawn but a click on it selects nothing. Its rows:
//   Draw = W3DScriptedModelDraw            Model = rugrenze (the HEAD mesh and the pole MESH01), Shadow = SHADOW_ALPHA_DECAL_DYNAMIC
//   Draw = W3DScriptedModelDraw "Icon"     ModelConditionState = NONE: Model = bbox
//   KindOf = STRUCTURE IMMOBILE SELECTABLE IGNORE_FOR_VICTORY, Geometry CYLINDER MajorRadius 10 Height 20, Scale 1.05
// art\w3d\bb\bbox.w3d is 484 bytes with no meshes: hierarchy 0x100, one W3D BOX chunk (0x740) "BBOX.BOX01" with Attributes 0x2, Center (0, 0, 7.5),
// Extent (5, 5, 7.5), and the HLOD 0x700. That box is the selectable volume of an object whose model is a pole.
//
// What the target does with it (DONOR BFME2 Code/Libraries/Source/WWVegas/WW3D2, the code the RotWK 2.01 build carries):
//   HLodClass::Cast_Ray (HLodCastRay.cpp): with the pick flag +0x42 set, the first CLASSID_OBBOX child of the top LOD answers alone; without one it
//     casts EVERY top-LOD child and stops at the first hit, the aligned boxes included.
//   AABoxRenderObjClass::Cast_Ray (boxrobj.cpp): the collision type test, Is_Animation_Hidden, StartBad, then CollisionMath::Collide(ray, CachedBox) -
//     the box is axis aligned in WORLD space (update_cached_box keeps the world AABB of the posed box), so the test runs on the world ray.
//   BoxPrototypeClass::Create (boxrobj.cpp): W3D_BOX_ATTRIBUTE_ORIENTED (1) makes an OBBoxRenderObjClass, else an AABoxRenderObjClass (class 0x1A).
//     BoxRenderObjClass's ctor stores (Attributes & COLLISION_TYPE_MASK) >> SHIFT << 1 = 0 for Attributes 0x2; HLod::Set_Collision_Type (RW 0x53B5C0)
//     gives every child the draw's collision type, so the empty bits do not keep the box out of the ray.
// The port cast only the ORIENTED box and the meshes (DrawablePick::rayTest), so a model whose only pick volume is an aligned box was not picked at all
// (bbox.w3d: no meshes, box skipped -> the "Icon" module contributed nothing and the thin pole was all there was to miss).
//
// Synthetic prototypes only: no install, no game files.
#include "doctest.h"

#include "GameClient/DrawablePick.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"
#include "Libraries/WWVegas/WW3D2/htree.h"
#include "Libraries/WWVegas/WW3D2/meshmdl.h"
#include "Libraries/WWVegas/WW3D2/w3d_file.h"

#include <cstring>
#include <deque>
#include <string>

namespace
{
// one synthetic model: the prototype plus the storage its sub objects point into (a deque: the addresses stay put while it fills)
struct SynthModel
{
	RenderObjPrototype proto;
	std::deque<MeshModelClass> meshes;
	std::deque<W3dBoxStruct> boxes;
};

void makeModel(SynthModel &m, const char *name)
{
	m.proto.Name = name;
	m.proto.Type = RenderObjPrototype::PROTO_HLOD;
	m.proto.DefaultTree = HTreeClass::Make_Default(); // one root pivot, as the render objects without a hierarchy get
	m.proto.Tree = &m.proto.DefaultTree;
}

// the visible half of the object: a pole of `width` units, `height` tall, standing at x = centreX (the mod's rugrenze: the HEAD and the MESH01 pole)
void addPole(SynthModel &m, float centreX, float width = 0.8f, float height = 30.0f)
{
	MeshModelClass mesh;
	mesh.MeshName = "MESH01";
	mesh.ContainerName = m.proto.Name;
	mesh.Header.Attributes = 0; // a rigid mesh (not a skin): its vertices ride on the bone of its sub object
	mesh.Vertices = { { centreX - width * 0.5f, 0.0f, 0.0f }, { centreX + width * 0.5f, 0.0f, 0.0f },
		{ centreX + width * 0.5f, 0.0f, height }, { centreX - width * 0.5f, 0.0f, height } };
	W3dTriStruct a = {};
	a.Vindex[0] = 0, a.Vindex[1] = 1, a.Vindex[2] = 2;
	W3dTriStruct b = {};
	b.Vindex[0] = 0, b.Vindex[1] = 2, b.Vindex[2] = 3;
	mesh.Triangles = { a, b };
	m.meshes.push_back(std::move(mesh));

	RenderSubObject sub;
	sub.Name = m.proto.Name + ".MESH01";
	sub.Type = RenderSubObject::SUB_MESH;
	sub.Mesh = &m.meshes.back();
	sub.BoneIndex = 0;
	m.proto.SubObjects.push_back(sub);
}

// the mod's art\w3d\bb\bbox.w3d: one box, Attributes 0x2 (W3D_BOX_ATTRIBUTE_ALIGNED, ORIENTED clear), Center (0, 0, 7.5), Extent (5, 5, 7.5)
void addBox(SynthModel &m, std::uint32_t attributes, float cx, float cy, float cz, float ex, float ey, float ez)
{
	W3dBoxStruct box = {};
	box.Version = W3D_MAKE_VERSION(1, 0);
	std::strncpy(box.Name, "BBOX.BOX01", sizeof(box.Name) - 1);
	box.Attributes = attributes;
	box.Center = { cx, cy, cz };
	box.Extent = { ex, ey, ez };
	m.boxes.push_back(box);

	RenderSubObject sub;
	sub.Name = box.Name;
	sub.Type = RenderSubObject::SUB_BOX;
	sub.Box = &m.boxes.back();
	sub.BoneIndex = 0;
	m.proto.SubObjects.push_back(sub);
}

// the mod's box in its own numbers
void addModBox(SynthModel &m)
{
	addBox(m, 0x2u, 0.0f, 0.0f, 7.5f, 5.0f, 5.0f, 7.5f);
}

const float kIdentity[9] = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };

DrawablePick::Result cast(const SynthModel &m, const float *basis, float scale, const Coord3D &at, const Coord3D &o, const Coord3D &d, float *t)
{
	*t = -1.0f;
	return DrawablePick::rayTestModel(m.proto, basis, scale, at, nullptr, o, d, t);
}

Coord3D at0() { return Coord3D{ 0.0f, 0.0f, 0.0f }; }
} // namespace

TEST_CASE("PORT-134: a model that is only an aligned box is picked (the mod's bbox.w3d, Attributes 0x2, no meshes)")
{
	DrawablePick::clearModelCache();
	SynthModel m;
	makeModel(m, "BBOX");
	addModBox(m);

	float t = 0.0f;
	// a click from the south, 100 units away, at the box's mid height: the near face is y = -5, so the hit is 95 units out
	CHECK(cast(m, kIdentity, 1.0f, at0(), Coord3D{ 0.0f, -100.0f, 7.5f }, Coord3D{ 0.0f, 100.0f, 0.0f }, &t) == DrawablePick::Result::Hit);
	CHECK(t == 95.0f / 100.0f); // the near face at y = -5, the direction is 100 long
	// from above: the top face is z = 15
	CHECK(cast(m, kIdentity, 1.0f, at0(), Coord3D{ 0.0f, 0.0f, 115.0f }, Coord3D{ 0.0f, 0.0f, -100.0f }, &t) == DrawablePick::Result::Hit);
	CHECK(t == 100.0f / 100.0f); // the top face at z = 15, 100 above the eye
	// 5 units to the side of the box and 5 above its top: the model shows the box and the ray misses it
	CHECK(cast(m, kIdentity, 1.0f, at0(), Coord3D{ 10.5f, -100.0f, 7.5f }, Coord3D{ 0.0f, 100.0f, 0.0f }, &t) == DrawablePick::Result::Miss);
	CHECK(cast(m, kIdentity, 1.0f, at0(), Coord3D{ 0.0f, -100.0f, 21.0f }, Coord3D{ 0.0f, 100.0f, 0.0f }, &t) == DrawablePick::Result::Miss);
}

TEST_CASE("PORT-134: a click that misses the thin pole but crosses its box picks the object (the head on a pike)")
{
	DrawablePick::clearModelCache();
	SynthModel m;
	makeModel(m, "RUGRENZE");
	addPole(m, 4.0f); // the pole stands 4 units off the object's axis
	addModBox(m);     // the box is the pick volume around the axis

	float t = 0.0f;
	// the ray goes through the box, 4 units away from the pole: the pole's triangles are the only geometry the pick had before
	CHECK(cast(m, kIdentity, 1.0f, at0(), Coord3D{ 0.0f, -100.0f, 7.5f }, Coord3D{ 0.0f, 100.0f, 0.0f }, &t) == DrawablePick::Result::Hit);
	CHECK(t == 95.0f / 100.0f); // the box's near face at y = -5, the direction is 100 long
	// on the pole itself both answer; the box's near face (y = -5) is nearer than the pole (y = 0), and the nearest hit is the pick's
	CHECK(cast(m, kIdentity, 1.0f, at0(), Coord3D{ 4.0f, -100.0f, 7.5f }, Coord3D{ 0.0f, 100.0f, 0.0f }, &t) == DrawablePick::Result::Hit);
	CHECK(t == 95.0f / 100.0f); // the box's near face at y = -5, nearer than the pole
	// far off both: nothing
	CHECK(cast(m, kIdentity, 1.0f, at0(), Coord3D{ 12.0f, -100.0f, 7.5f }, Coord3D{ 0.0f, 100.0f, 0.0f }, &t) == DrawablePick::Result::Miss);
}

TEST_CASE("PORT-134 neighbour: an oriented box still answers alone (RW 0x59C680's pick flag), the aligned box does not pre-empt it")
{
	DrawablePick::clearModelCache();
	SynthModel m;
	makeModel(m, "ORIENTED");
	addPole(m, 4.0f);
	// W3D_BOX_ATTRIBUTE_ORIENTED (1): an OBBoxRenderObjClass, the box the pick flag prefers over everything else of the top LOD
	addBox(m, 0x1u, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f, 2.0f);

	float t = 0.0f;
	// the ray crosses the pole but misses the oriented box: the box answered, so the model is a Miss (it is not the triangles' turn)
	CHECK(cast(m, kIdentity, 1.0f, at0(), Coord3D{ 4.0f, -100.0f, 7.5f }, Coord3D{ 0.0f, 100.0f, 0.0f }, &t) == DrawablePick::Result::Miss);
	// on the box: a hit
	CHECK(cast(m, kIdentity, 1.0f, at0(), Coord3D{ 0.0f, -100.0f, 2.0f }, Coord3D{ 0.0f, 100.0f, 0.0f }, &t) == DrawablePick::Result::Hit);
	CHECK(t == 98.0f / 100.0f); // the oriented box's near face at y = -2
}

TEST_CASE("PORT-134 neighbour: nothing to cast is NotDrawn, a mesh that the ray misses is a Miss")
{
	DrawablePick::clearModelCache();
	SynthModel empty;
	makeModel(empty, "EMPTY");
	float t = 0.0f;
	CHECK(cast(empty, kIdentity, 1.0f, at0(), Coord3D{ 0.0f, -100.0f, 7.5f }, Coord3D{ 0.0f, 100.0f, 0.0f }, &t) == DrawablePick::Result::NotDrawn);

	SynthModel pole;
	makeModel(pole, "POLEONLY");
	addPole(pole, 4.0f);
	CHECK(cast(pole, kIdentity, 1.0f, at0(), Coord3D{ 0.0f, -100.0f, 7.5f }, Coord3D{ 0.0f, 100.0f, 0.0f }, &t) == DrawablePick::Result::Miss);
	CHECK(cast(pole, kIdentity, 1.0f, at0(), Coord3D{ 4.0f, -100.0f, 7.5f }, Coord3D{ 0.0f, 100.0f, 0.0f }, &t) == DrawablePick::Result::Hit);
}

TEST_CASE("PORT-134: the box follows the object's placement (basis, scale and position), as the world box of the target does")
{
	DrawablePick::clearModelCache();
	SynthModel m;
	makeModel(m, "BBOX");
	addModBox(m);

	// a quarter turn about Z: R = [[0, -1, 0], [1, 0, 0], [0, 0, 1]]; Scale 1.05 not needed to show the rule, 2 doubles the box
	const float yaw90[9] = { 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
	const Coord3D at{ 100.0f, 200.0f, 0.0f };
	// the box's centre (0, 0, 7.5) turned by R is (0, 0, 7.5): world centre (100, 200, 15); extents 5 -> 10, 7.5 -> 15
	float t = 0.0f;
	CHECK(cast(m, yaw90, 2.0f, at, Coord3D{ 100.0f, 100.0f, 15.0f }, Coord3D{ 0.0f, 100.0f, 0.0f }, &t) == DrawablePick::Result::Hit);
	CHECK(t == 90.0f / 100.0f); // the near face at y = 190
	CHECK(cast(m, yaw90, 2.0f, at, Coord3D{ 111.0f, 100.0f, 15.0f }, Coord3D{ 0.0f, 100.0f, 0.0f }, &t) == DrawablePick::Result::Miss);
	// a 45 degree turn: the world box is the AABB of the turned box, so its extent across X is (5 + 5) / sqrt(2) * 2 ~ 14.14
	const float s = 0.70710678f;
	const float yaw45[9] = { s, -s, 0.0f, s, s, 0.0f, 0.0f, 0.0f, 1.0f };
	CHECK(cast(m, yaw45, 1.0f, at, Coord3D{ 100.0f + 6.5f, 190.0f, 14.0f }, Coord3D{ 0.0f, 100.0f, 0.0f }, &t) == DrawablePick::Result::Hit);
	CHECK(cast(m, yaw45, 1.0f, at, Coord3D{ 100.0f + 8.0f, 190.0f, 14.0f }, Coord3D{ 0.0f, 100.0f, 0.0f }, &t) == DrawablePick::Result::Miss);
}

TEST_CASE("PORT-134: several aligned boxes of one model: the nearest hit is the pick's")
{
	DrawablePick::clearModelCache();
	SynthModel m;
	makeModel(m, "TWIN");
	addBox(m, 0x2u, 0.0f, 0.0f, 5.0f, 5.0f, 5.0f, 5.0f);  // near face y = -5
	addBox(m, 0x2u, 0.0f, 20.0f, 5.0f, 5.0f, 5.0f, 5.0f); // near face y = 15

	float t = 0.0f;
	CHECK(cast(m, kIdentity, 1.0f, at0(), Coord3D{ 0.0f, -100.0f, 5.0f }, Coord3D{ 0.0f, 100.0f, 0.0f }, &t) == DrawablePick::Result::Hit);
	CHECK(t == 95.0f / 100.0f); // the first box's near face at y = -5
}
