// OpenBFME. GPL-3.0.
// See Common/FunctionCurve.h (editions: both RotWK 2.01 and BFME2 1.06).

#include "Common/FunctionCurve.h"

#include "Common/INI.h"
#include "Common/INIException.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace
{
constexpr float kRadsPerDegree = 0.0174532925f;

const char *kCurvePaddingNames[] = {
	"HOLD",
	"EXTRAPOLATE",
	"CYCLE",
	"CYCLE_OFFSET",
	"MIRROR",
	nullptr
};

static float scanTangentAngle(INI *ini, const char *token)
{
	float deg = ini->scanReal(token);
	return std::tan(deg * kRadsPerDegree);
}
} // namespace

FunctionCurve::FunctionCurve(float constantValue)
{
	addKey(0.0f, constantValue);
}

void FunctionCurve::addKey(float time, float value, const float *inTangent, const float *outTangent)
{
	CurveKey key;
	key.m_time = time;
	key.m_value = value;
	if (inTangent)
	{
		key.m_inTangent = *inTangent;
	}
	if (outTangent)
	{
		key.m_outTangent = *outTangent;
	}

	if (m_keys.empty())
	{
		m_keys.push_back(key);
		return;
	}

	if (time < m_keys.back().m_time)
	{
		throw INIException(3, "Function curve time must be increasing");
	}

	// If tangents were omitted, default them based on secant slope
	CurveKey &prev = m_keys.back();
	float dt = time - prev.m_time;
	if (dt > 1e-6f)
	{
		float slope = (value - prev.m_value) / dt;
		if (!outTangent)
		{
			prev.m_outTangent = slope;
		}
		if (!inTangent)
		{
			key.m_inTangent = slope;
		}
		if (m_keys.size() == 1 && !inTangent)
		{
			prev.m_inTangent = slope;
		}
		if (!outTangent)
		{
			key.m_outTangent = slope;
		}
	}

	m_keys.push_back(key);
}

float FunctionCurve::operator()(float t) const
{
	if (m_keys.empty())
	{
		return 0.0f;
	}
	if (m_keys.size() == 1)
	{
		return m_keys.front().m_value;
	}

	float t0 = m_keys.front().m_time;
	float t1 = m_keys.back().m_time;
	float span = t1 - t0;

	if (t < t0)
	{
		switch (m_inPadding)
		{
		case CurvePadding::HOLD:
			return m_keys.front().m_value;
		case CurvePadding::EXTRAPOLATE:
			return m_keys.front().m_value + (t - t0) * m_keys.front().m_inTangent;
		case CurvePadding::CYCLE:
			if (span > 1e-6f)
			{
				float offset = std::fmod(t - t0, span);
				if (offset < 0.0f)
				{
					offset += span;
				}
				t = t0 + offset;
			}
			break;
		case CurvePadding::CYCLE_OFFSET:
			if (span > 1e-6f)
			{
				float cycles = std::floor((t - t0) / span);
				float offset = t - t0 - cycles * span;
				float deltaVal = m_keys.back().m_value - m_keys.front().m_value;
				return (*this)(t0 + offset) + cycles * deltaVal;
			}
			break;
		case CurvePadding::MIRROR:
			if (span > 1e-6f)
			{
				float cycles = std::floor((t - t0) / (2.0f * span));
				float rem = t - t0 - cycles * 2.0f * span;
				if (rem > span)
				{
					t = t1 - (rem - span);
				}
				else
				{
					t = t0 + rem;
				}
			}
			break;
		}
	}
	else if (t > t1)
	{
		switch (m_outPadding)
		{
		case CurvePadding::HOLD:
			return m_keys.back().m_value;
		case CurvePadding::EXTRAPOLATE:
			return m_keys.back().m_value + (t - t1) * m_keys.back().m_outTangent;
		case CurvePadding::CYCLE:
			if (span > 1e-6f)
			{
				float offset = std::fmod(t - t0, span);
				t = t0 + offset;
			}
			break;
		case CurvePadding::CYCLE_OFFSET:
			if (span > 1e-6f)
			{
				float cycles = std::floor((t - t0) / span);
				float offset = t - t0 - cycles * span;
				float deltaVal = m_keys.back().m_value - m_keys.front().m_value;
				return (*this)(t0 + offset) + cycles * deltaVal;
			}
			break;
		case CurvePadding::MIRROR:
			if (span > 1e-6f)
			{
				float cycles = std::floor((t - t0) / (2.0f * span));
				float rem = t - t0 - cycles * 2.0f * span;
				if (rem > span)
				{
					t = t1 - (rem - span);
				}
				else
				{
					t = t0 + rem;
				}
			}
			break;
		}
	}

	// Find the segment [k0, k1] where k0.m_time <= t <= k1.m_time
	size_t idx = 0;
	while (idx + 1 < m_keys.size() && m_keys[idx + 1].m_time < t)
	{
		++idx;
	}
	if (idx + 1 >= m_keys.size())
	{
		return m_keys.back().m_value;
	}

	const CurveKey &k0 = m_keys[idx];
	const CurveKey &k1 = m_keys[idx + 1];
	float dt = k1.m_time - k0.m_time;
	if (dt <= 1e-6f)
	{
		return k0.m_value;
	}

	float u = (t - k0.m_time) / dt;
	float u2 = u * u;
	float u3 = u2 * u;

	// Cubic Hermite basis functions
	float h00 = 2.0f * u3 - 3.0f * u2 + 1.0f;
	float h10 = u3 - 2.0f * u2 + u;
	float h01 = -2.0f * u3 + 3.0f * u2;
	float h11 = u3 - u2;

	float m0 = k0.m_outTangent * dt;
	float m1 = k1.m_inTangent * dt;

	return h00 * k0.m_value + h10 * m0 + h01 * k1.m_value + h11 * m1;
}

void FunctionCurve::parseKey(INI *ini, void *, void *, const void *userData)
{
	auto *curve = reinterpret_cast<FunctionCurve *>(const_cast<void *>(userData));
	float time = ini->scanReal(ini->getNextSubToken("T"));
	float value = ini->scanReal(ini->getNextSubToken("V"));
	const float *inTangent = nullptr;
	const float *outTangent = nullptr;
	float inValue = 0.0f;
	float outValue = 0.0f;

	for (int count = 0; count < 2; ++count)
	{
		const char *token = ini->getNextTokenOrNull(ini->getSepsColon());
		if (!token)
		{
			break;
		}

		if (!inTangent && token[0] == 'I' && token[1] == '\0')
		{
			inValue = scanTangentAngle(ini, ini->getNextToken(ini->getSepsColon()));
			inTangent = &inValue;
		}
		else if (!outTangent && token[0] == 'O' && token[1] == '\0')
		{
			outValue = scanTangentAngle(ini, ini->getNextToken(ini->getSepsColon()));
			outTangent = &outValue;
		}
		else
		{
			throw INIException(3, "'I' or 'O' expected, and only one of them");
		}
	}

	curve->addKey(time, value, inTangent, outTangent);
}

void FunctionCurve::parse(INI *ini)
{
	FieldParse fields[] = {
		{ "InPadding", INI::parseIndexList, kCurvePaddingNames, (int)offsetof(FunctionCurve, m_inPadding) },
		{ "OutPadding", INI::parseIndexList, kCurvePaddingNames, (int)offsetof(FunctionCurve, m_outPadding) },
		{ "Key", FunctionCurve::parseKey, this, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->initFromINI(this, fields);
}

void FunctionCurve::parseFunctionCurve(INI *ini, void *, void *store, const void *)
{
	auto *curve = reinterpret_cast<FunctionCurve *>(store);
	const char *token = ini->getNextTokenOrNull();
	if (!token)
	{
		return;
	}

	char *endptr = nullptr;
	float val = std::strtof(token, &endptr);
	if (endptr && endptr != token && *endptr == '\0')
	{
		curve->m_keys.clear();
		curve->addKey(0.0f, val);
		return;
	}

	if (_stricmp(token, "FCurve") == 0 || _stricmp(token, "Keys") == 0)
	{
		curve->parse(ini);
		return;
	}

	throw INIException(3, "'FCurve' expected, got '%s'", token);
}
