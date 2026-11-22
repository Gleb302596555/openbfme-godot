// OpenBFME. GPL-3.0.
// Object shadow resolution for shadow maps and decal shadows (issue #135, TOP20 #5).
//
// TARGET FACTS (RotWK game.dat):
//   RW 0x4B931E: W3DModelDraw::allocateShadows, called on state switches across 7 model draw classes
//     (W3DScriptedModelDraw vtable 0xBE1420, W3DHordeModelDraw 0xBE0FB0, W3DDefaultModelDraw 0xBE0BC8,
//     W3DTreeDraw 0xBE1698, W3DDebrisModelDraw 0xBE0DA0, W3DPropModelDraw 0xBE1508, W3DFloorDraw 0xBE0EF8).
//   RW 0x4C4EE0..0x4C5175: W3DScriptedModelDraw::setModelState and RW 0x47A0AD W3DHordeModelDraw::setModelState:
//     Branch A (RW 0x4C4EE0): condition state shadow (hasShadow, type != 0) takes precedence; sets isRotatingWithObject = true (RW 0x4C4F50).
//     Branch B (RW 0x4C4FD6): ThingTemplate shadow (+0x5F2 != 0).
//     ShadowForceDisable (RW 0x4C4EF8 / 0x4C5013): when true, no shadow is created.
//   RW 0x50BF6A: W3DProjectedShadowManager::addShadow:
//     RW 0x50BF93: texture name defaults to "shadow" if absent or length <= 1.
//     RW 0x50BFF3..0x50C066: size fallback from model bounding box if sizeX or sizeY == 0.
//     RW 0x50C07B: center = DecalShadowCentre(drawablePos, offsetX, offsetY, angle).
//   RW 0x672D28: Drawable::shadowsEnabled (TOP20 #24: false when dead with SlowDeathBehavior/ShadowWhenDead = No).
//   RW 0x4C50D2, RW 0x73232B: ShadowUseHouseColor calls Shadow::setColor with player color.
//   RW 0x740108..0x740175: ThingTemplate constructor field defaults (+0x4F8..+0x514, +0x5BC..+0x5C4, +0x5FD..+0x5FF).

#include "GameClient/ObjectShadow.h"

#include "Common/Thing/ThingTemplate.h"
#include "GameClient/Drawable.h"
#include "GameClient/LogicSnapshot.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.h"
#include "GameLogic/Object/Object.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"

#include <algorithm>
#include <cmath>

namespace ObjectShadow
{

// lane PORT-91 (issue #239; RW 0x4C7848, W3DModelDraw::doDrawModule): a contained object's model draw keeps its shadow only while the contain of its
// container answers true for vslot 0x2C. The module data's ShowShadowWhileContained wins over the contain's answer
static bool containedShadowSuppressedCore(bool contained, bool containAllows, const W3DModelDrawModuleData *module)
{
	if (!contained)
	{
		return false; // RW 0x4C7848: not contained, the shadow stands
	}
	if (module && module->m_showShadowWhileContained)
	{
		return false; // the model draw module data says the shadow stays
	}
	return !containAllows; // RW 0x9188EB false in every contain table of 2.01 but the HordeContain family's RW 0x86C737
}

bool containedShadowSuppressed(const ::Object *object, const W3DModelDrawModuleData *module)
{
	if (!object)
	{
		return false; // unknown: nothing is suppressed (the caller could not resolve the logic object)
	}
	const Object *container = object->getContainedBy();
	if (!container)
	{
		return containedShadowSuppressedCore(false, false, module);
	}
	const ContainModuleInterface *c = container->getContain();
	return containedShadowSuppressedCore(true, c && c->shadowWhileContainedAllowed(), module);
}

bool containedShadowSuppressed(const ObjectSnapshot *record, const W3DModelDrawModuleData *module)
{
	if (!record)
	{
		return false; // no record (no snapshot): nothing is suppressed
	}
	return containedShadowSuppressedCore(record->containedBy != INVALID_ID, record->shadowWhileContainedAllowed, module);
}

namespace
{
float readFloat(const ThingTemplate *tt, const char *name, float defVal)
{
	if (!tt) return defVal;
	const FieldValue *v = tt->findField(name);
	if (!v) return defVal;
	if (const float *f = std::get_if<float>(v)) return *f;
	if (const long long *i = std::get_if<long long>(v)) return (float)*i;
	return defVal;
}

unsigned readUnsigned(const ThingTemplate *tt, const char *name, unsigned defVal)
{
	if (!tt) return defVal;
	const FieldValue *v = tt->findField(name);
	if (!v) return defVal;
	if (const long long *i = std::get_if<long long>(v)) return (unsigned)*i;
	if (const float *f = std::get_if<float>(v)) return (unsigned)*f;
	return defVal;
}

bool readBool(const ThingTemplate *tt, const char *name, bool defVal)
{
	if (!tt) return defVal;
	const FieldValue *v = tt->findField(name);
	if (!v) return defVal;
	if (const bool *b = std::get_if<bool>(v)) return *b;
	if (const long long *i = std::get_if<long long>(v)) return *i != 0;
	return defVal;
}

std::string readString(const ThingTemplate *tt, const char *name, const std::string &defVal)
{
	if (!tt) return defVal;
	const FieldValue *v = tt->findField(name);
	if (!v) return defVal;
	if (const std::string *s = std::get_if<std::string>(v)) return *s;
	return defVal;
}
} // namespace

bool getModelBoundingBox(const RenderObjPrototype *proto, float &extentX, float &extentY)
{
	if (!proto || proto->SubObjects.empty())
	{
		return false;
	}

	const HTreeClass *tree = proto->Tree ? proto->Tree : &proto->DefaultTree;
	HTreePose pose;
	tree->Base_Pose(Matrix3D(), pose);
	const int pivots = (int)pose.Transform.size();
	auto pivot = [&](int i) -> const Matrix3D & {
		static const Matrix3D identity;
		return i >= 0 && i < pivots ? pose.Transform[(size_t)i] : identity;
	};

	float minX = 1e30f, maxX = -1e30f;
	float minY = 1e30f, maxY = -1e30f;
	bool found = false;

	for (const RenderSubObject &sub : proto->SubObjects)
	{
		if (sub.Type != RenderSubObject::SUB_MESH || !sub.Mesh || sub.Mesh->Is_Hidden())
		{
			continue;
		}
		const MeshModelClass &mesh = *sub.Mesh;
		const Matrix3D &piv = pivot(sub.BoneIndex);
		if (!mesh.Vertices.empty())
		{
			for (const W3dVectorStruct &p : mesh.Vertices)
			{
				float w[3];
				for (int r = 0; r < 3; ++r)
				{
					w[r] = piv.Row[r][0] * p.X + piv.Row[r][1] * p.Y + piv.Row[r][2] * p.Z + piv.Row[r][3];
				}
				minX = std::min(minX, w[0]);
				maxX = std::max(maxX, w[0]);
				minY = std::min(minY, w[1]);
				maxY = std::max(maxY, w[1]);
				found = true;
			}
		}
		else
		{
			minX = std::min(minX, mesh.Header.Min.X);
			maxX = std::max(maxX, mesh.Header.Max.X);
			minY = std::min(minY, mesh.Header.Min.Y);
			maxY = std::max(maxY, mesh.Header.Max.Y);
			found = true;
		}
	}

	if (!found)
	{
		for (const RenderSubObject &sub : proto->SubObjects)
		{
			if (sub.Type == RenderSubObject::SUB_BOX && sub.Box && !sub.Aggregate && (sub.Box->Attributes & 1u) != 0)
			{
				extentX = sub.Box->Extent.X;
				extentY = sub.Box->Extent.Y;
				return true;
			}
		}
		return false;
	}

	if (minX > maxX || minY > maxY)
	{
		return false;
	}

	extentX = (maxX - minX) * 0.5f;
	extentY = (maxY - minY) * 0.5f;
	return true;
}

bool resolveDecalShadow(const ThingTemplate *tt, const W3DModelDrawModuleData *module, const ModelConditionInfo *state, DecalShadowDescriptor &out)
{
	out = DecalShadowDescriptor();
	// RW 0x4C4EF8 / RW 0x4C5013: ShadowForceDisable skips shadow creation
	if (module && module->m_shadowForceDisable)
	{
		return false;
	}

	// Branch A (RW 0x4C4EE0..0x4C4FD1): per-condition state shadow takes precedence if defined
	if (state && state->hasShadow && (state->shadow.type & kDecalMask) != 0)
	{
		out.enabled = true;
		out.style = (std::uint32_t)(state->shadow.type & kDecalMask);
		// RW 0x50BF93: default texture "shadow" if empty or single char
		out.texture = state->shadow.texture.size() <= 1 ? "shadow" : state->shadow.texture;
		out.sizeX = state->shadow.sizeX;
		out.sizeY = state->shadow.sizeY;
		out.offsetX = state->shadow.offsetX;
		out.offsetY = state->shadow.offsetY;
		out.sunAngle = state->shadow.sunAngle;
		out.maxHeight = state->shadow.maxHeight > 0.0f ? state->shadow.maxHeight : 20.0f;
		out.isRotatingWithObject = true; // RW 0x4C4F50: condition shadow sets isRotatingWithObject = true
		out.overrideLODVisibility = state->shadow.overrideLODVisibility;
		out.opacityStart = state->shadowOpacityStart;
		out.opacityFadeInTime = state->shadowOpacityFadeInTime;
		out.opacityPeak = state->shadowOpacityPeak;
		out.opacityFadeOutTime = state->shadowOpacityFadeOutTime;
		out.opacityEnd = state->shadowOpacityEnd;
		out.useHouseColor = false;
		return true;
	}

	// Branch B (RW 0x4C4FD6..0x4C5175, RW 0x47A846..0x47A934, RW 0x4B931E): ThingTemplate shadow
	if (!tt)
	{
		return false;
	}
	const FieldValue *shField = tt->findField("Shadow");
	const long long *bits = shField ? std::get_if<long long>(shField) : nullptr;
	if (!bits || (*bits & kDecalMask) == 0)
	{
		return false;
	}

	out.enabled = true;
	out.style = (std::uint32_t)(*bits & kDecalMask);

	// RW 0x50BF93..0x50BFBB: ShadowTexture defaults to "shadow" if absent or length <= 1
	std::string tex = readString(tt, "ShadowTexture", "shadow");
	out.texture = tex.size() <= 1 ? "shadow" : tex;

	out.sizeX = readFloat(tt, "ShadowSizeX", 0.0f);
	out.sizeY = readFloat(tt, "ShadowSizeY", 0.0f);
	out.offsetX = readFloat(tt, "ShadowOffsetX", 0.0f);
	out.offsetY = readFloat(tt, "ShadowOffsetY", 0.0f);
	out.sunAngle = readFloat(tt, "ShadowSunAngle", 0.0f);
	out.maxHeight = readFloat(tt, "ShadowMaxHeight", 20.0f); // RW 0x74015E
	out.overrideLODVisibility = readBool(tt, "ShadowOverrideLODVisibility", false);
	out.useHouseColor = readBool(tt, "ShadowUseHouseColor", false);
	out.isRotatingWithObject = readBool(tt, "ShadowIsRotatingWithObject", false); // RW 0x740172
	out.opacityStart = readUnsigned(tt, "ShadowOpacityStart", 0);
	out.opacityFadeInTime = readFloat(tt, "ShadowOpacityFadeInTime", 0.0f);
	out.opacityPeak = readUnsigned(tt, "ShadowOpacityPeak", 255); // RW 0x740136 default 0xFF
	out.opacityFadeOutTime = readFloat(tt, "ShadowOpacityFadeOutTime", 0.0f);
	out.opacityEnd = readUnsigned(tt, "ShadowOpacityEnd", 0);
	return true;
}

unsigned evaluateDecalOpacity(const DecalShadowDescriptor &desc, unsigned currentFrame, unsigned startFrame, unsigned fadeOutStartFrame)
{
	// RW 0x4B1BC6 / RW 0x50B8AD: style == 0x400 (SHADOW_ALPHA_DECAL_DYNAMIC), == 0x800 (SHADOW_ADDITIVE_DECAL_DYNAMIC), == 0x10000
	const bool isDynamic = (desc.style == 0x0400u || desc.style == 0x0800u || desc.style == 0x10000u);
	if (!isDynamic)
	{
		return desc.opacityPeak; // static decals only use peak opacity (RW 0x73228B)
	}

	// Dynamic curve evaluation (RW 0x7323CE, RW 0x50B860)
	// RW 0xD9F624: 0.03f ms to frames (30 logic fps / 1000 ms)
	// 0xA3CFA4: _ftol truncation towards zero (not round)
	const unsigned fadeInFrames = (unsigned)std::max(0.0f, desc.opacityFadeInTime * 0.03f);
	const unsigned fadeOutFrames = (unsigned)std::max(0.0f, desc.opacityFadeOutTime * 0.03f);

	if (fadeOutStartFrame != ~0u && currentFrame >= fadeOutStartFrame)
	{
		if (fadeOutFrames == 0)
		{
			return desc.opacityEnd;
		}
		const unsigned dt = currentFrame - fadeOutStartFrame;
		if (dt >= fadeOutFrames)
		{
			return desc.opacityEnd;
		}
		const float progress = (float)dt / (float)fadeOutFrames;
		const float val = (float)desc.opacityPeak + progress * (float)((int)desc.opacityEnd - (int)desc.opacityPeak);
		return (unsigned)std::ceil(val); // RW 0x50B9E0 call msvcr71 ceil
	}

	if (currentFrame < startFrame)
	{
		return 0; // RW 0x50B902: before startFrame opacity is 0
	}
	const unsigned t = currentFrame - startFrame;
	if (fadeInFrames == 0 || t >= fadeInFrames)
	{
		return desc.opacityPeak;
	}
	const float progress = (float)t / (float)fadeInFrames;
	const float val = (float)desc.opacityStart + progress * (float)((int)desc.opacityPeak - (int)desc.opacityStart);
	return (unsigned)std::ceil(val); // RW 0x50B964 call msvcr71 ceil
}

namespace
{
struct ShadowCurveRef
{
	bool &initialized;
	UnsignedInt &startFrame;
	UnsignedInt &fadeOutStartFrame;
	const ModelConditionInfo *&lastState;
	const RenderObjPrototype *&lastModel;
	std::string &lastModelName;
	bool &lastStateHasShadow;
};

bool buildOneDecalShadow(const Drawable &drawable, const W3DModelDrawModuleData *moduleData, const ModelConditionInfo *state,
	const RenderObjPrototype *proto, const Coord3D &basePos, float baseAngle, ShadowCurveRef &curve,
	const std::function<void(float &, float &)> &fallbackSize, bool useShadowDecals, unsigned currentFrame,
	const ::Object *logicObject, SelectionDecal &out)
{
	// lane PORT-91 (issue #239; RW 0x4C7848): a contained object's decal shadow waits for the contain's slot 0x2C (or ShowShadowWhileContained)
	if (containedShadowSuppressed(logicObject, moduleData))
	{
		return false;
	}

	DecalShadowDescriptor desc;
	if (!resolveDecalShadow(drawable.getTemplate(), moduleData, state, desc))
	{
		return false;
	}
	// RW 0x50BF7B .. 0x50BF8E: addShadow makes no decal shadow unless [RW 0xDE4364] + 0x61 (UseShadowDecals) or the shadow's + 0x24
	// (ShadowOverrideLODVisibility, template + 0x5FD, RW 0x4C5050)
	if (!useShadowDecals && !desc.overrideLODVisibility)
	{
		return false;
	}

	// Size resolution: if sizeX or sizeY is 0, query model box for this draw module first, then fallback (RW 0x50BFF3..0x50C066)
	if (desc.sizeX == 0.0f || desc.sizeY == 0.0f)
	{
		if (proto)
		{
			float ex = 0.0f, ey = 0.0f;
			if (getModelBoundingBox(proto, ex, ey))
			{
				if (desc.sizeX == 0.0f) desc.sizeX = ex * 2.0f;
				if (desc.sizeY == 0.0f) desc.sizeY = ey * -2.0f;
			}
		}
		if ((desc.sizeX == 0.0f || desc.sizeY == 0.0f) && fallbackSize)
		{
			fallbackSize(desc.sizeX, desc.sizeY);
		}
	}
	if (desc.sizeX == 0.0f && desc.sizeY == 0.0f)
	{
		return false;
	}

	// Texture file name (append .tga if no extension, RW 0x50BF6A)
	const std::string file = desc.texture.find('.') == std::string::npos ? desc.texture + ".tga" : desc.texture;

	// Orientation: rotating with object vs fixed top-down (RW 0x4C4F50, RW 0x4C50CE)
	const float angle = desc.isRotatingWithObject ? baseAngle : 0.0f;
	const Coord3D center = DecalShadowCentre(basePos, desc.offsetX, desc.offsetY, angle);

	// Color resolution: house color (RW 0x4C50D2, RW 0x73232B) vs white
	std::uint32_t rgb = 0xFFFFFFu;
	if (desc.useHouseColor)
	{
		const std::uint32_t house = drawable.indicatorColor() != 0 ? drawable.indicatorColor() : drawable.getHouseColor();
		if ((house & 0xFFFFFFu) != 0)
		{
			rgb = house & 0xFFFFFFu;
		}
	}

	const unsigned frame = currentFrame != 0 ? currentFrame : drawable.syncFrame();

	const std::string modelName = state && !state->modelNames.empty() ? state->modelName(0) : "";
	const bool stateHasShadow = state != nullptr && state->hasShadow;

	// Model change or state shadow override change resets shadow curve (RW 0x4C4610..0x4C4679, RW 0x4C4EE0..0x4C4FBC)
	if (!curve.initialized)
	{
		curve.initialized = true;
		curve.lastState = state;
		curve.lastModel = proto;
		curve.lastModelName = modelName;
		curve.lastStateHasShadow = stateHasShadow;
	}
	else
	{
		const bool modelChanged = (proto != nullptr && curve.lastModel != nullptr)
			? (proto != curve.lastModel)
			: (modelName != curve.lastModelName);
		const bool shadowOverrideChanged = (stateHasShadow != curve.lastStateHasShadow) ||
			(stateHasShadow && state != curve.lastState);

		if (modelChanged || shadowOverrideChanged)
		{
			curve.startFrame = frame;
			curve.lastState = state;
			curve.lastModel = proto;
			curve.lastModelName = modelName;
			curve.lastStateHasShadow = stateHasShadow;
			curve.fadeOutStartFrame = ~0u;
		}
		else if (state != curve.lastState)
		{
			curve.lastState = state;
		}
	}

	// Opacity resolution: static peak opacity vs dynamic curve (RW 0x4B1BC6, RW 0x7323CE, RW 0x50B860)
	unsigned opacity = desc.opacityPeak;
	if (desc.style == 0x0400u || desc.style == 0x0800u || desc.style == 0x10000u)
	{
		opacity = evaluateDecalOpacity(desc, frame, curve.startFrame, curve.fadeOutStartFrame);
	}

	out = SelectionDecal();
	out.object = drawable.getObjectID();
	out.drawable = (unsigned)drawable.getID();
	out.position = center;
	out.drawZ = basePos.z;
	out.size = desc.sizeX;
	out.sizeY = desc.sizeY;
	out.angle = angle;
	out.texture = file;
	out.style = desc.style;
	out.color = ((std::uint32_t)opacity << 24) | (rgb & 0xFFFFFFu);
	out.terrainOnly = false; // RW 0x50CDE4: projects on terrain and wall/bridge geometry
	return true;
}
} // namespace

void buildDrawableDecalShadows(const Drawable &drawable, std::vector<SelectionDecal> &out, bool useShadowDecals,
	const std::function<void(float &, float &)> &fallbackSize, unsigned currentFrame, const ::Object *logicObject)
{
	out.clear();
	// TOP20 #24, RW 0x672D28: shadowsEnabled false when dead with SlowDeathBehavior/ShadowWhenDead = No
	if (!drawable.shadowsEnabled())
	{
		return;
	}

	// Shroud & client visibility gates (RW 0x4B66CD, RW 0x4B675E)
	if (drawable.clientHidden() || drawable.shroudLook().hidden || drawable.stealthHidden())
	{
		return;
	}

	if (drawable.entries().empty())
	{
		const ModelConditionInfo *state = drawable.testModelState();
		bool init = drawable.shadowStateInitialized();
		UnsignedInt startFrame = drawable.shadowStartFrame();
		UnsignedInt fadeOutFrame = drawable.shadowFadeOutStartFrame();
		const ModelConditionInfo *lastState = drawable.lastShadowState();
		const RenderObjPrototype *lastModel = drawable.lastShadowModel();
		std::string lastModelName = drawable.lastShadowModelName();
		bool lastHasShadow = drawable.lastShadowStateHasShadow();
		ShadowCurveRef curve{ init, startFrame, fadeOutFrame, lastState, lastModel, lastModelName, lastHasShadow };
		SelectionDecal sd;
		if (buildOneDecalShadow(drawable, nullptr, state, nullptr, *drawable.getPosition(), drawable.getOrientation(),
				curve, fallbackSize, useShadowDecals, currentFrame, logicObject, sd))
		{
			out.push_back(std::move(sd));
		}
		drawable.setShadowStateInitialized(init);
		drawable.setShadowStartFrame(startFrame);
		drawable.setShadowFadeOutStartFrame(fadeOutFrame);
		drawable.setLastShadowState(lastState);
		drawable.setLastShadowModel(lastModel);
		drawable.setLastShadowModelName(lastModelName);
		drawable.setLastShadowStateHasShadow(lastHasShadow);
		return;
	}

	// Both editions (RW 0x4C4EE0..0x4C5175): each visible W3DModelDraw entry owns its decal shadow.
	// Branch A (state->hasShadow) creates a shadow even when Model = Invisible; Branch B (ThingTemplate shadow) requires m_renderObject != NULL.
	for (const DrawEntry &e : drawable.entries())
	{
		if (e.kind != W3D_DRAWKIND_MODEL || !e.draw || e.moduleHidden || e.conditionHidden)
		{
			continue;
		}
		const W3DModelDrawModuleData *moduleData = dynamic_cast<const W3DModelDrawModuleData *>(e.data);
		const ModelConditionInfo *state = e.draw->currentModelState();
		if (!state && drawable.testModelState())
		{
			state = drawable.testModelState();
		}
		const RenderObjPrototype *proto = e.draw->currentModel();
		const bool hasStateShadow = (state != nullptr && state->hasShadow && (state->shadow.type & kDecalMask) != 0);
		if (!hasStateShadow && proto == nullptr)
		{
			continue; // RW 0x4C4FD6: Branch B skips when m_renderObject == NULL (e.g. Model = None)
		}
		Coord3D basePos = *drawable.getPosition();
		float baseAngle = drawable.getOrientation();
		if (!e.attachToBone.empty())
		{
			Matrix3D entryWorld;
			if (drawable.entryTransform(e, entryWorld))
			{
				basePos = Coord3D{ entryWorld.Row[0][3], entryWorld.Row[1][3], entryWorld.Row[2][3] };
				baseAngle = std::atan2(entryWorld.Row[1][0], entryWorld.Row[0][0]);
			}
		}
		if (!e.shadowStateInitialized && drawable.shadowStartFrame() != 0)
		{
			e.shadowStartFrame = drawable.shadowStartFrame();
		}
		if (drawable.shadowFadeOutStartFrame() != ~0u && e.shadowFadeOutStartFrame == ~0u)
		{
			e.shadowFadeOutStartFrame = drawable.shadowFadeOutStartFrame();
		}
		ShadowCurveRef curve{ e.shadowStateInitialized, e.shadowStartFrame, e.shadowFadeOutStartFrame,
			e.lastShadowState, e.lastShadowModel, e.lastShadowModelName, e.lastShadowStateHasShadow };
		SelectionDecal sd;
		if (buildOneDecalShadow(drawable, moduleData, state, proto, basePos, baseAngle,
				curve, fallbackSize, useShadowDecals, currentFrame, logicObject, sd))
		{
			if (out.empty())
			{
				drawable.setShadowStateInitialized(e.shadowStateInitialized);
				drawable.setShadowStartFrame(e.shadowStartFrame);
				drawable.setShadowFadeOutStartFrame(e.shadowFadeOutStartFrame);
				drawable.setLastShadowState(e.lastShadowState);
				drawable.setLastShadowModel(e.lastShadowModel);
				drawable.setLastShadowModelName(e.lastShadowModelName);
				drawable.setLastShadowStateHasShadow(e.lastShadowStateHasShadow);
			}
			out.push_back(std::move(sd));
		}
	}
	// Local 89 (#715): the terrain decal of the first draw module (RW 0x4B94BC / 0x4B95AD: a SHADOW_ALPHA_DECAL decal shadow, white, its opacity the
	// RadiusDecal one; drawn while shadows are on and the draw is not hidden, RW 0x4B9662 .. 0x4B958E, and not under the shroud, its +5 from the draw's
	// +0x49). INFERENCE: the decal is world-aligned (no ShadowIsRotatingWithObject for it)
	const DrawEntry &first = drawable.entries().front();
	if (first.draw && !first.moduleHidden && !first.conditionHidden)
	{
		if (const W3DScriptedModelDraw::TerrainDecal *decal = first.draw->terrainDecal())
		{
			if (decal->sizeX != 0.0f || decal->sizeY != 0.0f)
			{
				SelectionDecal sd;
				sd.object = drawable.getObjectID();
				sd.drawable = (unsigned)drawable.getID();
				sd.position = DecalShadowCentre(*drawable.getPosition(), decal->offsetX, decal->offsetY, 0.0f);
				sd.drawZ = drawable.getPosition()->z;
				sd.size = decal->sizeX;
				sd.sizeY = decal->sizeY;
				sd.texture = decal->texture.find('.') == std::string::npos ? decal->texture + ".tga" : decal->texture;
				sd.style = 0x0020u; // SHADOW_ALPHA_DECAL (RW 0x4B950E)
				const unsigned alpha = (unsigned)std::clamp(decal->opacity, 0, 255);
				sd.color = (alpha << 24) | 0xFFFFFFu;
				out.push_back(std::move(sd));
			}
		}
	}
}

bool buildDrawableDecalShadow(const Drawable &drawable, SelectionDecal &out, bool useShadowDecals,
	const std::function<void(float &, float &)> &fallbackSize, unsigned currentFrame, const ::Object *logicObject)
{
	std::vector<SelectionDecal> decals;
	buildDrawableDecalShadows(drawable, decals, useShadowDecals, fallbackSize, currentFrame, logicObject);
	if (decals.empty())
	{
		return false;
	}
	out = std::move(decals.front());
	return true;
}

} // namespace ObjectShadow
