// OpenBFME. GPL-3.0.
// See GameClient/GUI/Skirmish/LivingWorldSetupText.h.

#include "GameClient/GUI/Skirmish/LivingWorldSetupText.h"
#include "Common/ModEngineFeatures.h"

#include <algorithm>

namespace LivingWorldSetupText
{
std::u16string format(const std::u16string &fmt, const std::vector<FormatArg> &args)
{
	std::u16string out;
	size_t next = 0;
	for (size_t i = 0; i < fmt.size(); ++i)
	{
		if (fmt[i] != u'%' || i + 1 >= fmt.size())
		{
			out.push_back(fmt[i]);
			continue;
		}
		size_t j = i + 1;
		if (fmt[j] == u'%')
		{
			out.push_back(u'%');
			i = j;
			continue;
		}
		if (fmt[j] == u'l' && j + 1 < fmt.size())
		{
			++j;
		}
		const char16_t c = fmt[j];
		if (c != u'd' && c != u'i' && c != u's')
		{
			out.push_back(fmt[i]);
			continue;
		}
		i = j;
		if (next >= args.size())
		{
			continue; // retail reads past its arguments; nothing is printed here
		}
		const FormatArg &a = args[next++];
		if (c == u's')
		{
			out += a.isString ? a.text : std::u16string();
		}
		else
		{
			const std::string n = std::to_string(a.isString ? 0 : a.number);
			out.append(n.begin(), n.end());
		}
	}
	return out;
}

std::u16string bonusList(const LivingWorldBonusBlock &b, const GameTextSource *text)
{
	// RW 0x7F2440: every positive field in this order, LW:ListSeparator between them; the siege and hero discounts are formatted without a value
	struct Row
	{
		int value;
		const char *label;
		bool withValue;
	};
	const Row rows[] = {
		{ b.attack, "LW:RegionAttackBonus", true },
		{ b.defense, "LW:RegionDefenseBonus", true },
		{ b.experience, "LW:RegionExperienceBonus", true },
		{ b.resource, "LW:RegionBonusResource", true },
		{ b.treasury, "LW:RegionTreasuryBonus", true },
		{ b.legendary, "LW:RegionLegendaryBonus", true },
		{ b.extraResources, "LW:RegionExtraResourcesBonus", true },
		{ b.buildingDiscount, "LW:RegionBuildingDiscountBonus", true },
		{ b.freeInnUnits, "LW:RegionFreeInnUnitsBonus", true },
		{ b.siegeDiscount, "LW:RegionSeigeDiscountBonus", false },
		{ b.heroDiscount, "LW:RegionHeroDiscountBonus", false },
		{ b.freeBuilders, "LW:RegionFreeBuildersBonus", true },
		{ b.barracksDiscount, "LW:RegionBarracksUnitDiscountBonus", true },
	};
	const std::u16string separator = fetchOrMissing(text, "LW:ListSeparator");
	std::u16string out;
	int shown = 0;
	for (const Row &r : rows)
	{
		if (r.value <= 0)
		{
			continue;
		}
		if (shown++ > 0)
		{
			out += separator;
		}
		const std::u16string label = fetchOrMissing(text, r.label);
		out += r.withValue ? format(label, { FormatArg(r.value) }) : format(label, {});
	}
	if (out.empty())
	{
		out = fetchOrMissing(text, "LW:NoBonus");
	}
	return out;
}

std::u16string territoryDescription(const LivingWorldSetupCampaign &campaign, int regionIndex, const GameTextSource *text)
{
	if (regionIndex < 0 || regionIndex >= (int)campaign.regions.size())
	{
		return std::u16string();
	}
	const LivingWorldSetupRegion &r = campaign.regions[(size_t)regionIndex];
	// RW 0x976C4D: the DisplayName (RW 0x7F1556), "\n", RW 0x7F2A76
	std::u16string out = r.displayLabel.empty() ? std::u16string() : fetchOrMissing(text, r.displayLabel);
	out += u"\n";
	return out + regionDescription(campaign, regionIndex, text);
}

std::u16string regionDescription(const LivingWorldSetupCampaign &campaign, int regionIndex, const GameTextSource *text)
{
	if (regionIndex < 0 || regionIndex >= (int)campaign.regions.size())
	{
		return std::u16string();
	}
	const LivingWorldSetupRegion &r = campaign.regions[(size_t)regionIndex];
	std::u16string out = fetchOrMissing(text, "LW:TerritoryBonusHeader");
	out += bonusList(r.bonus, text);
	if (r.createAutoFort)
	{
		out += fetchOrMissing(text, "LW:FortressMap");
	}
	out += format(fetchOrMissing(text, r.buildPlots == 1 ? "LW:NumberOfBuildPlotsSingle" : "LW:NumberOfBuildPlotsPlural"), { FormatArg(r.buildPlots) });
	for (const LivingWorldSetupTerritory &t : campaign.territories)
	{
		if (std::find(t.regions.begin(), t.regions.end(), r.name) == t.regions.end())
		{
			continue;
		}
		const std::u16string name = t.displayLabel.empty() ? std::u16string() : fetchOrMissing(text, t.displayLabel);
		out += format(fetchOrMissing(text, "LW:TerritoryPartOfRegion"), { FormatArg(name) });
		out += format(fetchOrMissing(text, "LW:UnifiedRegionBonus"), { FormatArg(bonusList(t.bonus, text)) });
		break; // RW 0x60EE09: the first territory that holds the region
	}
	return out;
}

namespace
{
bool endsWithAscii(const std::u16string &s, const char *suffix, size_t suffixLen)
{
	if (s.size() < suffixLen) return false;
	const size_t start = s.size() - suffixLen;
	for (size_t i = 0; i < suffixLen; ++i)
		if (s[start + i] != (char16_t)(unsigned char)suffix[i]) return false;
	return true;
}
} // namespace

std::u16string adjustScenarioTextFor8Players(const std::u16string &text, const LivingWorldScenarioEntry &scenario, bool isTitle)
{
	// PYSAGE 1.9.8.5 .wtrdisp 0x00F2F0E8..0x00F2F3BA (hooks RW 0x9760B4, RW 0x9765B3, RW 0x976BA8)
	if (!ModEngineFeatures::has(ModEngineFeature::EnnorathWotr8Players) || scenario.maxPlayers != 8 || text.size() > 0x200)
	{
		return text;
	}
	const bool is042Or043 = scenario.displayLabel == "LWScenario:WOTRScenario042" || scenario.displayLabel == "LWScenario:WOTRScenario043";
	// Pass 1 (.wtrdisp 0x00F2F15D..0x00F2F20D): scan for standalone '2' + ('-' | U+2013 | U+2014) + ('8' | '6' | '2')
	for (size_t i = 2; i < text.size(); ++i)
	{
		if (text[i - 2] != u'2') continue;
		const char16_t dash = text[i - 1];
		if (dash != u'-' && dash != (char16_t)0x2013 && dash != (char16_t)0x2014) continue;
		if (i > 2 && text[i - 3] >= u'0' && text[i - 3] <= u'9') continue;
		if (i + 1 < text.size() && text[i + 1] >= u'0' && text[i + 1] <= u'9') continue;
		const char16_t upper = text[i];
		if (upper == u'8') return text;
		if (upper == u'6' || (upper == u'2' && is042Or043))
		{
			std::u16string out = text;
			out[i] = u'8';
			return out;
		}
	}
	// Pass 2 (.wtrdisp 0x00F2F212..0x00F2F390): fallback for WOTRScenario042 / WOTRScenario043
	if (!is042Or043 || text.empty()) return text;
	if (isTitle)
	{
		size_t baseLen = text.size();
		if (endsWithAscii(text, " (2)", 4)) baseLen -= 4;
		else if (endsWithAscii(text, " (1vs1)", 7)) baseLen -= 7;
		else if (endsWithAscii(text, " (1v1)", 6)) baseLen -= 6;
		std::u16string out = text.substr(0, baseLen);
		out.push_back(u' ');
		out.push_back(u'(');
		out.push_back(u'2');
		out.push_back((char16_t)0x2013);
		out.push_back(u'8');
		out.push_back(u')');
		return out;
	}
	// Description mode (.wtrdisp 0x00F2F2E4..0x00F2F38D): first line ending with ": 2" -> ": 2\u20138"
	size_t eol = 0;
	while (eol < text.size() && text[eol] != u'\r' && text[eol] != u'\n') ++eol;
	while (eol > 0 && (text[eol - 1] == u' ' || text[eol - 1] == u'\t')) --eol;
	if (eol == 0 || text[eol - 1] != u'2') return text;
	size_t colon = eol - 1;
	while (colon > 0 && (text[colon - 1] == u' ' || text[colon - 1] == u'\t')) --colon;
	if (colon <= 1 || text[colon - 1] != u':') return text;
	std::u16string out;
	out.reserve(text.size() + 2);
	out.append(text, 0, eol);
	out.push_back((char16_t)0x2013);
	out.push_back(u'8');
	out.append(text, eol, std::u16string::npos);
	return out;
}

std::u16string scenarioDisplayName(const LivingWorldScenarioEntry &scenario, const GameTextSource *text)
{
	const std::u16string name = scenario.displayLabel.empty() ? std::u16string() : fetchOrMissing(text, scenario.displayLabel);
	return adjustScenarioTextFor8Players(name, scenario, true);
}

std::u16string scenarioDescription(const LivingWorldScenarioEntry &scenario, int victoryType, const GameTextSource *text)
{
	const std::u16string rawDescription = scenario.descriptionLabel.empty() ? std::u16string() : fetchOrMissing(text, scenario.descriptionLabel);
	const std::u16string description = adjustScenarioTextFor8Players(rawDescription, scenario, false);
	if (victoryType < 0 || scenario.victoryTypes.empty())
	{
		return description; // no game info (RW 0x97606B's first branch); a campaign without victory types has nothing to show
	}
	const LivingWorldScenarioEntry::VictoryTypeText &v =
		scenario.victoryTypes[(size_t)victoryType < scenario.victoryTypes.size() ? (size_t)victoryType : 0]; // RW 0x932BF7
	return format(u"%s\n\n%s: %s\n%s", { FormatArg(description), FormatArg(fetchOrMissing(text, "RULE:VictoryType")),
										  FormatArg(v.gameTypeLabel.empty() ? std::u16string() : fetchOrMissing(text, v.gameTypeLabel)),
										  FormatArg(v.objectivesLabel.empty() ? std::u16string() : fetchOrMissing(text, v.objectivesLabel)) });
}
} // namespace LivingWorldSetupText
