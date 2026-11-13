// OpenBFME. GPL-3.0.
// See DrawablePick.h.

#include "GameClient/DrawablePick.h"
#include "Common/BfmeGame.h"

#include "GameClient/Drawable.h"
#include "GameLogic/Object/PristinePose.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DScriptedModelDraw.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

namespace
{
// one prototype's triangles in model space (the hierarchy's bind pose), with the sub object each belongs to and a bounding sphere
struct ModelTriangles
{
	std::vector<float> xyz;      // 9 floats per triangle
	std::vector<int> subObject;  // per triangle
	float centre[3] = { 0.0f, 0.0f, 0.0f };
	float radius = 0.0f;
	// lane SELECT-1: the first oriented box of the top LOD (RotWK HLodClass::Cast_Ray, RW 0x59C680: with the pick flag (+0x42) set, the first top-LOD
	// child of class 0x1B (OBBoxRenderObjClass) answers the ray alone); its pivot's bind-pose transform, centre and half extents in that pivot's space
	int boxSubObject = -1;
	int boxBone = 0;
	Matrix3D boxPivot;
	float boxCentre[3] = { 0.0f, 0.0f, 0.0f };
	float boxExtent[3] = { 0.0f, 0.0f, 0.0f };
	// PORT-134 (issue #336): the ALIGNED boxes of the same LOD (W3D_BOX_ATTRIBUTE_ORIENTED clear, W3D_BOX_ATTRIBUTE_ALIGNED set: an AABoxRenderObjClass,
	// class 0x1A). DONOR FACTS (BFME2 Code/Libraries/Source/WWVegas/WW3D2): HLodClass::Cast_Ray prefers the first CLASSID_OBBOX child under the pick flag and
	// otherwise casts EVERY child of the top LOD, the aligned boxes included; AABoxRenderObjClass::Cast_Ray tests the collision type, Is_Animation_Hidden and
	// StartBad, then the ray against the cached WORLD box. The mod's art\w3d\bb\bbox.w3d is such a box (chunk 0x740, Attributes 0x2: ALIGNED, no meshes,
	// Center (0, 0, 7.5), Extent (5, 5, 7.5)): the selectable volume of an object whose model is too thin to click, so it must be cast like the meshes.
	struct AlignedBox
	{
		int subObject = -1;
		int bone = 0;
		Matrix3D pivot;
		float centre[3] = { 0.0f, 0.0f, 0.0f };
		float extent[3] = { 0.0f, 0.0f, 0.0f };
	};
	std::vector<AlignedBox> aaboxes;
};

std::mutex g_cacheMutex;
std::map<const RenderObjPrototype *, std::shared_ptr<const ModelTriangles>> g_cache; // keyed by the asset manager's prototype (client cache, never hashed)

void transformPoint(const Matrix3D &m, float x, float y, float z, float *out)
{
	for (int r = 0; r < 3; ++r)
	{
		out[r] = m.Row[r][0] * x + m.Row[r][1] * y + m.Row[r][2] * z + m.Row[r][3];
	}
}

std::shared_ptr<const ModelTriangles> buildTriangles(const RenderObjPrototype &proto)
{
	auto out = std::make_shared<ModelTriangles>();
	const HTreeClass *tree = proto.Tree ? proto.Tree : &proto.DefaultTree;
	HTreePose pose;
	tree->Base_Pose(Matrix3D(), pose);
	const int pivots = (int)pose.Transform.size();
	auto pivot = [&](int i) -> const Matrix3D & {
		static const Matrix3D identity;
		return i >= 0 && i < pivots ? pose.Transform[(size_t)i] : identity;
	};
	float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
	for (size_t s = 0; s < proto.SubObjects.size(); ++s)
	{
		const RenderSubObject &sub = proto.SubObjects[s];
		// BFME2 BoxPrototypeClass::Create (donor boxrobj.cpp): W3D_BOX_ATTRIBUTE_ORIENTED (1) makes an OBBoxRenderObjClass, else an AABoxRenderObjClass (class
		// 0x1A). The pick flag prefers the first oriented box alone (RW 0x59C680); an aligned box is cast with the other children. The aggregates are not
		// searched (RW 0x59C680 walks Lod[LodCount - 1] only)
		if (sub.Type == RenderSubObject::SUB_BOX && sub.Box && !sub.Aggregate)
		{
			const W3dBoxStruct &b = *sub.Box;
			if (out->boxSubObject < 0 && (b.Attributes & 1u) != 0)
			{
				out->boxSubObject = (int)s;
				out->boxBone = sub.BoneIndex;
				out->boxPivot = pivot(sub.BoneIndex);
				out->boxCentre[0] = b.Center.X, out->boxCentre[1] = b.Center.Y, out->boxCentre[2] = b.Center.Z;
				out->boxExtent[0] = b.Extent.X, out->boxExtent[1] = b.Extent.Y, out->boxExtent[2] = b.Extent.Z;
			}
			else if ((b.Attributes & 1u) == 0)
			{
				// PORT-134: an aligned box is its own pick volume (W3D_BOX_ATTRIBUTE_ALIGNED; Attributes 0x2 leaves the collision-type bits 0xFF0 empty, and
				// HLod::Set_Collision_Type (RW 0x53B5C0) gives the box the draw's type, so the empty bits do not keep it out of the ray: donor boxrobj.cpp
				// BoxRenderObjClass's ctor stores (Attributes & MASK) >> SHIFT << 1 and the HLOD overwrites it)
				ModelTriangles::AlignedBox a;
				a.subObject = (int)s;
				a.bone = sub.BoneIndex;
				a.pivot = pivot(sub.BoneIndex);
				a.centre[0] = b.Center.X, a.centre[1] = b.Center.Y, a.centre[2] = b.Center.Z;
				a.extent[0] = b.Extent.X, a.extent[1] = b.Extent.Y, a.extent[2] = b.Extent.Z;
				out->aaboxes.push_back(a);
			}
		}
		const MeshModelClass *mesh = sub.Type == RenderSubObject::SUB_MESH ? sub.Mesh : nullptr;
		if (!mesh || mesh->Is_Hidden())
		{
			continue;
		}
		const bool skin = mesh->Is_Skin() && mesh->Influences.size() == mesh->Vertices.size();
		std::vector<float> world(mesh->Vertices.size() * 3);
		for (size_t v = 0; v < mesh->Vertices.size(); ++v)
		{
			const W3dVectorStruct &p = mesh->Vertices[v];
			float *w = &world[v * 3];
			if (!skin)
			{
				transformPoint(pivot(sub.BoneIndex), p.X, p.Y, p.Z, w);
				continue;
			}
			// a skin's vertex is in its bone's space; BFME's second bone (Vertices2, weights in percent) blends in when it carries weight
			const W3dVertInfStruct &inf = mesh->Influences[v];
			transformPoint(pivot(inf.BoneIdx), p.X, p.Y, p.Z, w);
			if (inf.Weight1 > 0 && mesh->Vertices2.size() == mesh->Vertices.size())
			{
				float second[3];
				const W3dVectorStruct &q = mesh->Vertices2[v];
				transformPoint(pivot(inf.Bone1Idx), q.X, q.Y, q.Z, second);
				const float sum = (float)inf.Weight0 + (float)inf.Weight1;
				const float a = sum > 0.0f ? (float)inf.Weight0 / sum : 1.0f;
				for (int k = 0; k < 3; ++k)
				{
					w[k] = w[k] * a + second[k] * (1.0f - a);
				}
			}
		}
		for (const W3dTriStruct &tri : mesh->Triangles)
		{
			if (tri.Vindex[0] >= mesh->Vertices.size() || tri.Vindex[1] >= mesh->Vertices.size() || tri.Vindex[2] >= mesh->Vertices.size())
			{
				continue;
			}
			for (int c = 0; c < 3; ++c)
			{
				const float *w = &world[(size_t)tri.Vindex[c] * 3];
				for (int k = 0; k < 3; ++k)
				{
					out->xyz.push_back(w[k]);
					lo[k] = std::min(lo[k], w[k]);
					hi[k] = std::max(hi[k], w[k]);
				}
			}
			out->subObject.push_back((int)s);
		}
	}
	if (!out->subObject.empty())
	{
		float r2 = 0.0f;
		for (int k = 0; k < 3; ++k)
		{
			out->centre[k] = (lo[k] + hi[k]) * 0.5f;
		}
		for (size_t i = 0; i < out->xyz.size(); i += 3)
		{
			const float dx = out->xyz[i] - out->centre[0], dy = out->xyz[i + 1] - out->centre[1], dz = out->xyz[i + 2] - out->centre[2];
			r2 = std::max(r2, dx * dx + dy * dy + dz * dz);
		}
		out->radius = std::sqrt(r2);
	}
	return out;
}

// the asset manager destroys a prototype: its cached triangles go (a later prototype may be allocated at the same address)
void evictPrototype(const RenderObjPrototype *proto)
{
	std::lock_guard<std::mutex> lock(g_cacheMutex);
	g_cache.erase(proto);
}

std::shared_ptr<const ModelTriangles> trianglesOf(const RenderObjPrototype &proto)
{
	static const bool observing = (W3DSetPrototypeEvictObserver(&evictPrototype), true);
	(void)observing;
	std::lock_guard<std::mutex> lock(g_cacheMutex);
	auto it = g_cache.find(&proto);
	if (it != g_cache.end())
	{
		return it->second;
	}
	std::shared_ptr<const ModelTriangles> t = buildTriangles(proto);
	g_cache.emplace(&proto, t);
	return t;
}

// Moller-Trumbore, both faces: the ray parameter of the hit, false for a miss
bool rayTriangle(const float *o, const float *d, const float *a, const float *b, const float *c, float &t)
{
	const float e1[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
	const float e2[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
	const float p[3] = { d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0] };
	const float det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
	if (std::fabs(det) < 1e-12f)
	{
		return false;
	}
	const float inv = 1.0f / det;
	const float s[3] = { o[0] - a[0], o[1] - a[1], o[2] - a[2] };
	const float u = (s[0] * p[0] + s[1] * p[1] + s[2] * p[2]) * inv;
	if (u < 0.0f || u > 1.0f)
	{
		return false;
	}
	const float q[3] = { s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0] };
	const float v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) * inv;
	if (v < 0.0f || u + v > 1.0f)
	{
		return false;
	}
	t = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv;
	return t >= 0.0f;
}

// RotWK OBBoxRenderObjClass::Cast_Ray (RW 0x575220: the collision type test, not hidden, not StartBad, then the ray against the world box RW 0x00B28A30):
// the model-space ray against the box on its pivot (slabs in the pivot's frame; the pivot is a rotation plus a translation)
bool rayBox(const ModelTriangles &m, const Matrix3D &p, const float *o, const float *d, float &t)
{
	float ol[3], dl[3];
	for (int c = 0; c < 3; ++c)
	{
		ol[c] = p.Row[0][c] * (o[0] - p.Row[0][3]) + p.Row[1][c] * (o[1] - p.Row[1][3]) + p.Row[2][c] * (o[2] - p.Row[2][3]);
		dl[c] = p.Row[0][c] * d[0] + p.Row[1][c] * d[1] + p.Row[2][c] * d[2];
	}
	float tin = 0.0f, tout = 1e30f;
	for (int c = 0; c < 3; ++c)
	{
		const float lo = m.boxCentre[c] - m.boxExtent[c], hi = m.boxCentre[c] + m.boxExtent[c];
		if (std::fabs(dl[c]) < 1e-12f)
		{
			if (ol[c] < lo || ol[c] > hi)
			{
				return false;
			}
			continue;
		}
		float a = (lo - ol[c]) / dl[c], b = (hi - ol[c]) / dl[c];
		if (a > b)
		{
			std::swap(a, b);
		}
		tin = std::max(tin, a);
		tout = std::min(tout, b);
		if (tin > tout)
		{
			return false;
		}
	}
	t = tin;
	return true;
}
} // namespace

namespace
{
// the ray in model space: world = basis * (scale * model) + position (Thing basis: row-major, columns X, Y, Z)
void modelSpaceRay(const float *basis, float scale, const Coord3D &at, const Coord3D &o, const Coord3D &d, float *om, float *dm)
{
	const float rel[3] = { o.x - at.x, o.y - at.y, o.z - at.z };
	const float dw[3] = { d.x, d.y, d.z };
	const float inv = scale != 0.0f ? 1.0f / scale : 1.0f;
	for (int c = 0; c < 3; ++c)
	{
		// the basis is a rotation: its transpose is its inverse
		om[c] = (basis[0 * 3 + c] * rel[0] + basis[1 * 3 + c] * rel[1] + basis[2 * 3 + c] * rel[2]) * inv;
		dm[c] = (basis[0 * 3 + c] * dw[0] + basis[1 * 3 + c] * dw[1] + basis[2 * 3 + c] * dw[2]) * inv;
	}
}

// the world ray against an axis-aligned box: donor AABoxRenderObjClass::Cast_Ray (boxrobj.cpp) tests CollisionMath::Collide(RayClass, AABoxClass) against
// the cached world box, i.e. the slabs of an axis-aligned box in world space
bool rayAlignedBox(const float *centre, const float *extent, const float *o, const float *d, float &t)
{
	float tin = 0.0f, tout = 1e30f;
	for (int c = 0; c < 3; ++c)
	{
		const float lo = centre[c] - extent[c], hi = centre[c] + extent[c];
		if (std::fabs(d[c]) < 1e-12f)
		{
			if (o[c] < lo || o[c] > hi)
			{
				return false;
			}
			continue;
		}
		float a = (lo - o[c]) / d[c], b = (hi - o[c]) / d[c];
		if (a > b)
		{
			std::swap(a, b);
		}
		tin = std::max(tin, a);
		tout = std::min(tout, b);
		if (tin > tout)
		{
			return false;
		}
	}
	t = tin;
	return true;
}

// a box's pivot as the draw shows it: the hierarchy's bind pose, or the draw's track-0 animation frame on an animated bone (HLod::Update_Sub_Object_Transforms,
// RW 0x59C680's first call; no blend: S-1200)
Matrix3D posedPivot(const RenderObjPrototype &proto, int bone, const Matrix3D &bind, const W3DScriptedModelDraw *draw)
{
	if (bone <= 0 || !draw)
	{
		return bind;
	}
	W3DDrawPoseView view;
	draw->poseView(view);
	const HTreeClass *tree = proto.Tree ? proto.Tree : &proto.DefaultTree;
	std::vector<Matrix3D> pose;
	if (view.clipKey0 && PristinePose::evaluate(*tree, static_cast<const HAnimClass *>(view.clipKey0), (int)view.frame0, 1.0f, pose, nullptr)
		&& bone < (int)pose.size())
	{
		return pose[(size_t)bone];
	}
	return bind;
}
} // namespace

namespace DrawablePick
{
Result rayTestModel(const RenderObjPrototype &model, const float *basis, float scale, const Coord3D &at, const W3DScriptedModelDraw *draw,
	const Coord3D &o, const Coord3D &d, float *t)
{
	const std::shared_ptr<const ModelTriangles> tris = trianglesOf(model);
	if (tris->subObject.empty() && tris->boxSubObject < 0 && tris->aaboxes.empty())
	{
		return Result::NotDrawn;
	}
	static const std::set<int> noHidden;
	const std::set<int> &hidden = draw ? draw->hiddenSubObjects() : noHidden;
	bool hit = false;
	float best = 0.0f;
	float om[3], dm[3];
	modelSpaceRay(basis, scale, at, o, d, om, dm);
	if (tris->boxSubObject >= 0)
	{
		// lane SELECT-1: the box answers alone (RW 0x59C680 returns the box's Cast_Ray, hit or not; a hidden box misses: RW 0x575220's vslot 0x198)
		const Matrix3D pivotNow = posedPivot(model, tris->boxBone, tris->boxPivot, draw);
		float th;
		if (!hidden.count(tris->boxSubObject) && rayBox(*tris, pivotNow, om, dm, th))
		{
			*t = th;
			return Result::Hit;
		}
		return Result::Miss;
	}
	if (!tris->subObject.empty())
	{
		// the bounding sphere first
		const float oc[3] = { om[0] - tris->centre[0], om[1] - tris->centre[1], om[2] - tris->centre[2] };
		const float a = dm[0] * dm[0] + dm[1] * dm[1] + dm[2] * dm[2];
		const float b = 2.0f * (oc[0] * dm[0] + oc[1] * dm[1] + oc[2] * dm[2]);
		const float c = oc[0] * oc[0] + oc[1] * oc[1] + oc[2] * oc[2] - tris->radius * tris->radius;
		if (a > 0.0f && b * b - 4.0f * a * c >= 0.0f)
		{
			const size_t count = tris->subObject.size();
			for (size_t i = 0; i < count; ++i)
			{
				if (!hidden.empty() && hidden.count(tris->subObject[i]))
				{
					continue;
				}
				const float *v = &tris->xyz[i * 9];
				float th;
				if (rayTriangle(om, dm, v, v + 3, v + 6, th) && (!hit || th < best))
				{
					hit = true;
					best = th; // the same parameter as in world space: the map from model to world is linear in t
				}
			}
		}
	}
	// PORT-134: the aligned boxes, cast with the meshes (donor HLodClass::Cast_Ray's general child loop). The box is axis aligned in WORLD space, not in model
	// space: donor AABoxRenderObjClass keeps a cached world box (the AABB of the posed box: R_world = basis * R_pivot, extent_w[k] = sum |R_world[k][j]| * e[j]),
	// and Cast_Ray tests the world ray against it. So does this
	const float atv[3] = { at.x, at.y, at.z };
	const float ow[3] = { o.x, o.y, o.z };
	const float dw[3] = { d.x, d.y, d.z };
	for (const ModelTriangles::AlignedBox &box : tris->aaboxes)
	{
		if (!hidden.empty() && hidden.count(box.subObject))
		{
			continue;
		}
		const Matrix3D pivotNow = posedPivot(model, box.bone, box.pivot, draw);
		// the box's centre through its pivot, then through the drawable (at + scale * basis * p); R_world = basis * R_pivot
		float p[3], centre[3], extent[3];
		for (int r = 0; r < 3; ++r)
		{
			p[r] = pivotNow.Row[r][0] * box.centre[0] + pivotNow.Row[r][1] * box.centre[1] + pivotNow.Row[r][2] * box.centre[2] + pivotNow.Row[r][3];
		}
		for (int r = 0; r < 3; ++r)
		{
			centre[r] = atv[r] + scale * (basis[r * 3 + 0] * p[0] + basis[r * 3 + 1] * p[1] + basis[r * 3 + 2] * p[2]);
			extent[r] = 0.0f;
			for (int k = 0; k < 3; ++k)
			{
				const float w = basis[r * 3 + 0] * pivotNow.Row[0][k] + basis[r * 3 + 1] * pivotNow.Row[1][k] + basis[r * 3 + 2] * pivotNow.Row[2][k];
				extent[r] += std::fabs(scale * w) * box.extent[k];
			}
		}
		float th;
		if (rayAlignedBox(centre, extent, ow, dw, th) && (!hit || th < best))
		{
			hit = true;
			best = th;
		}
	}
	if (!hit)
	{
		return Result::Miss;
	}
	*t = best;
	return Result::Hit;
}

Result rayTest(const Drawable &drawable, const Coord3D &o, const Coord3D &d, float *t)
{
	bool drawn = false, unknown = false, hit = false;
	float best = 0.0f;
	const float *basis = drawable.getBasis();
	const float scale = drawable.getInstanceScale();
	for (const DrawEntry &e : drawable.entries())
	{
		if (e.moduleHidden || e.conditionHidden || e.missingModel)
		{
			continue;
		}
		const RenderObjPrototype *proto = nullptr;
		if (e.kind == W3D_DRAWKIND_MODEL && e.draw)
			proto = e.draw->currentModel();
		else if (e.kind == W3D_DRAWKIND_FLOOR && !e.staticModel.empty() && bfme1Mounted())
		{
			// community PR #36, BFME1 only (docs/BFME1.md): BFME1's building plots expose their clickable surface through W3DFloorDraw. RotWK / BFME2
			// floors stay unpickable (S-1200)
			std::string error;
			proto = drawable.drawServices().assets.Create_Render_Obj(e.staticModel, &error);
		}
		else
			continue; // batched trees/props and effect draws are not pick targets
		if (!proto)
		{
			unknown = unknown || (e.draw && e.draw->currentModelState() == nullptr);
			continue;
		}
		// PORT-134 (issue #336): every draw module of the drawable is cast, the second one included: the mod's head on a pike shows the selectable box of
		// art\w3d\bb\bbox.w3d through a second W3DScriptedModelDraw (its "Icon" module) while its visible model (rugrenze: a thin pole) is not clickable
		float th;
		const Result r = rayTestModel(*proto, basis, scale, drawable.entryPosition(e), e.draw.get(), o, d, &th);
		if (r == Result::NotDrawn)
		{
			continue;
		}
		drawn = true;
		if (r == Result::Hit && (!hit || th < best))
		{
			hit = true;
			best = th;
		}
	}
	if (hit)
	{
		*t = best;
		return Result::Hit;
	}
	if (drawn)
	{
		return Result::Miss;
	}
	return unknown ? Result::Unknown : Result::NotDrawn;
}

const char *stopLine()
{
	return "[S-1200] picking (InGameHud): the ray meets the drawn W3D model's oriented bounding box when its top LOD has one (RotWK RW 0x59C680 / 0x575220, lane "
		   "SELECT-1), else its aligned boxes and its triangles (PORT-134: an AABox child, class 0x1A, is cast too: the mod's bbox.w3d is one); a box on an animated bone is posed with the draw's track-0 animation frame (no blend), the triangles stand on the hierarchy's bind pose, mesh translucency is not "
		   "tested, both faces; trees, props and floors pick nothing";
}

size_t cachedModels()
{
	std::lock_guard<std::mutex> lock(g_cacheMutex);
	return g_cache.size();
}

void clearModelCache()
{
	std::lock_guard<std::mutex> lock(g_cacheMutex);
	g_cache.clear();
}
} // namespace DrawablePick
