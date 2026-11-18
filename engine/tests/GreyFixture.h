// OpenBFME test util (lane PORT-530 / Local 72, fork issue 530): the button matrix's grey-button fixtures.
//
// The matrix sweep (engine/tests/test_hud_factb_units.cpp unitsSweep) presses every button of every faction on the mod's
// data; a button the port greys (ButtonState::Restricted) is either a requirement the sweep does not meet or a port
// bug. Port 220 (fork issue 529) writes docs/ennorath/MATRIX_GREY.tsv — one line per gating condition of every grey
// button — and this header parses it and plans the fixtures the sweep gives before pressing again:
//
//   upgrade  the named upgrade: a player upgrade through Player::addCompletedUpgrade, an object upgrade through
//            Object::giveUpgrade on the fresh unit (the sweep tells the two apart by the upgrade's type);
//   science  the named science through PlayerScience::grantScience (RW 0x6AE3A7);
//   object   the named object made for the player near the unit (Rig::make);
//   model    the button / module that sets the model condition, pressed through the HUD (the satisfied-by column
//            names it; the sweep falls back to the weapon toggle, then the mount toggle), never by writing the bits;
//   level    the unit raised to the named level through ExperienceTracker::gainExpForLevel (RW 0x79DA0A);
//   option / other / none  not faked: the button keeps its grey verdict with the reason.
//
// A button still grey with every condition met is GREY-BUG (the port's availability rule differs from RotWK's
// ControlBar::getCommandAvailability, RW 0x942733); a condition the sweep cannot give keeps DISABLED: needs ...
// (tools/matrix_run.py CATEGORIES). The table does not exist until Port 220 lands: with no table the sweep keeps its
// old verdicts, so the non-matrix sweeps (factb units*, factc units*) do not change.
//
// Table contract (docs/ennorath/MATRIX_GREY.tsv, tab-separated, one line per condition):
//   faction, object, button, condition, value, satisfied-by, note
//   condition is upgrade / science / object / model / level / option / other / none; value is the upgrade, science,
//   object or level (the required level number) the button needs; satisfied-by says how a test gives it (for a model
//   condition, the button or module that sets it). Lines starting with '#' and the header line are skipped.

#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace greyfix
{

struct Condition
{
	std::string condition;   ///< upgrade / science / object / model / level / option / other / none
	std::string value;       ///< the upgrade, science, object or level the button needs
	std::string satisfiedBy; ///< how a test gives it (for a model condition, the button or module that sets it)
	std::string note;
};

struct Row
{
	std::string faction;
	std::string object;
	std::string button;
	std::vector<Condition> conditions; ///< one per gating condition, in table order
};

/// (object, button) -> the row of its gating conditions. An object+button pair is unique within one faction run, so
/// the faction column is kept on the row but is not part of the key.
typedef std::map<std::pair<std::string, std::string>, Row> Table;

inline std::vector<std::string> splitTabs(const std::string &line)
{
	std::vector<std::string> out;
	for (size_t a = 0;;)
	{
		const size_t b = line.find('\t', a);
		out.push_back(line.substr(a, b == std::string::npos ? std::string::npos : b - a));
		if (b == std::string::npos)
		{
			break;
		}
		a = b + 1;
	}
	return out;
}

inline Table parseGreyTable(const std::string &text)
{
	Table table;
	for (size_t a = 0; a <= text.size();)
	{
		const size_t b = text.find('\n', a);
		std::string line = text.substr(a, b == std::string::npos ? std::string::npos : b - a);
		a = b == std::string::npos ? text.size() + 1 : b + 1;
		if (!line.empty() && line.back() == '\r')
		{
			line.pop_back();
		}
		if (line.empty() || line[0] == '#')
		{
			continue;
		}
		const std::vector<std::string> f = splitTabs(line);
		if (f.size() < 5 || f[0] == "faction")
		{
			continue; // the header line, or a line too short to name a condition
		}
		Row &row = table[std::make_pair(f[1], f[2])];
		if (row.conditions.empty())
		{
			row.faction = f[0];
			row.object = f[1];
			row.button = f[2];
		}
		Condition c;
		c.condition = f[3];
		c.value = f[4];
		c.satisfiedBy = f.size() > 5 ? f[5] : std::string();
		c.note = f.size() > 6 ? f[6] : std::string();
		row.conditions.push_back(c);
	}
	return table;
}

inline const Row *findRow(const Table &table, const std::string &object, const std::string &button)
{
	const auto it = table.find(std::make_pair(object, button));
	return it == table.end() ? nullptr : &it->second;
}

enum class Kind
{
	Upgrade,
	Science,
	Object,
	Model,
	Level,
	Unfixable ///< option / other / none / unknown: the sweep does not fake it
};

inline Kind kindOf(const std::string &condition)
{
	if (condition == "upgrade")
	{
		return Kind::Upgrade;
	}
	if (condition == "science")
	{
		return Kind::Science;
	}
	if (condition == "object")
	{
		return Kind::Object;
	}
	if (condition == "model")
	{
		return Kind::Model;
	}
	if (condition == "level")
	{
		return Kind::Level;
	}
	return Kind::Unfixable;
}

/// The row's conditions as retail's ControlBar::getCommandAvailability (RW 0x942733; the port: ControlBar::evaluate) reads them (owner 2026-11-18:
/// 434 NEED_UPGRADE buttons were never pressed because the option was taken for a condition of its own). The table lists every gating field
/// of the data; RW tests them so:
///   * option NEED_UPGRADE (RW 0x9428A3 .. 0x942993): the NeededUpgrade list alone (every entry, or one with NeededUpgradeAny); an empty list
///     never restricts. Met by the list's upgrade rows: dropped. So is NeededUpgradeAny ("other"): giving every entry meets "any one";
///   * option NEED_SPECIAL_POWER_SCIENCE: the power's RequiredSciences, listed as their own science rows: dropped;
///   * option MOUNTED_ONLY (RW 0x9427BD .. 0x9427E5): restricted without the model condition MOUNTED: the model condition MOUNTED;
///   * options UNMOUNTED_ONLY (RW 0x9427BD), NOT_QUEUEABLE (a production in the queue), SINGLE_USE_COMMAND (RW 0x942826, after its use),
///     HIDE_WHILE_DISABLED (hides a restricted button: no gate of its own), NEEDS_CASTLE_KINDOF (RW 0x942993: hidden, never restricted),
///     ON_GROUND_ONLY (RW 0x9427EB: the AI's vslot 0x228, not ported, stop S-7427: the port never restricts by it): each restricts only
///     while a state is present that a fresh unit lacks: dropped;
///   * a DisableOnModelCondition mask (RW 0x942490): restricts while the object HAS the condition: not a need: dropped;
///   * an upgrade's BuildCost ("other"): its price; RW asks only whether the upgrade is complete: dropped;
///   * a power's PreventActivationConditions ("other", RW 0x943343 .. 0x943433): restricts while the object has the status: dropped.
/// Anything else is kept as listed (an unknown option or other stays unfixable).
inline std::vector<Condition> effective(const Row &row)
{
	std::vector<Condition> out;
	for (const Condition &c : row.conditions)
	{
		if (c.condition == "option")
		{
			if (c.value == "MOUNTED_ONLY")
			{
				out.push_back(Condition{ "model", "MOUNTED", c.satisfiedBy, c.note + " (RW 0x9427BD: MOUNTED_ONLY needs the model condition MOUNTED)" });
				continue;
			}
			static const char *const kDropped[] = { "NEED_UPGRADE", "NEED_SPECIAL_POWER_SCIENCE", "UNMOUNTED_ONLY", "NOT_QUEUEABLE", "SINGLE_USE_COMMAND",
			                                        "HIDE_WHILE_DISABLED", "NEEDS_CASTLE_KINDOF", "ON_GROUND_ONLY" };
			bool dropped = false;
			for (const char *d : kDropped)
			{
				dropped = dropped || c.value == d;
			}
			if (dropped)
			{
				continue;
			}
		}
		if (c.condition == "model" && c.note.find("DisableOnModelCondition") != std::string::npos)
		{
			continue;
		}
		if (c.condition == "other" && (c.note.find("NeededUpgradeAny") != std::string::npos || c.note.find(".BuildCost") != std::string::npos ||
		                               c.note.find("PreventActivationConditions") != std::string::npos))
		{
			continue;
		}
		out.push_back(c);
	}
	return out;
}

/// Every condition of the row (as RW reads it: effective) is one the sweep can give. A row with no conditions names no gate the sweep can meet.
inline bool fixable(const Row &listed)
{
	Row row = listed;
	row.conditions = effective(listed);
	if (row.conditions.empty())
	{
		return false;
	}
	for (const Condition &c : row.conditions)
	{
		if (kindOf(c.condition) == Kind::Unfixable)
		{
			return false;
		}
	}
	return true;
}

/// The verdict of a button whose condition the sweep cannot give: the first unmet condition, in table order.
inline std::string needsDetail(const Row &listed)
{
	const std::vector<Condition> conditions = effective(listed);
	if (conditions.empty())
	{
		// every listed gate restricts only while a state is present (effective): the grey has a cause the table does not name
		return listed.conditions.empty() ? "DISABLED: needs none" : "DISABLED: needs none (every listed gate restricts only while a state is present, RW 0x942733)";
	}
	const Condition &c = conditions.front();
	for (const Condition &u : conditions)
	{
		if (kindOf(u.condition) == Kind::Unfixable)
		{
			return "DISABLED: needs " + u.condition + (u.value.empty() ? std::string() : " " + u.value); // the condition the sweep cannot give
		}
	}
	return "DISABLED: needs " + c.condition + (c.value.empty() ? std::string() : " " + c.value);
}

/// The HUD button that sets a model condition, from the satisfied-by column (2026-11-17): the column carries a note after the name
/// ("Command_GorkilToggleWeapon (weapon-set toggle)"), and "-" or an "Object:ModuleTag" names no button the sweep can press: "" then.
inline std::string setterButton(const std::string &satisfiedBy)
{
	const std::string name = satisfiedBy.substr(0, satisfiedBy.find(" ("));
	if (name.empty() || name == "-" || name.find(':') != std::string::npos)
	{
		return std::string();
	}
	return name;
}

/// The verdict of a button whose fixture ran but did not give one of its conditions (2026-11-17: the sweep checks each condition after
/// the fixture; before, a weapon or mount toggle pressed for a USER_n condition was reported as "met" and the button as GREY-BUG).
inline std::string unmetDetail(const Condition &c, const std::string &why)
{
	return "DISABLED: needs " + c.condition + (c.value.empty() ? std::string() : " " + c.value) + " (" + why + ")";
}

/// The verdict of a button still grey with every condition met: the port's availability rule differs from RotWK's.
inline std::string bugDetail(const Row &listed)
{
	const std::vector<Condition> conditions = effective(listed);
	std::string out = "GREY-BUG: still restricted with";
	for (const Condition &c : conditions)
	{
		out += " " + c.condition + (c.value.empty() ? std::string() : " " + c.value) + ",";
	}
	if (!conditions.empty())
	{
		out.pop_back(); // the trailing comma of the last condition
	}
	return out + " met";
}

struct Action
{
	Kind kind;
	std::string value;
	std::string satisfiedBy;
};

/// The fixtures in the order the sweep gives them: the required object first (a technology seller the upgrade's
/// RequiredObjectFilter wants, or the object the button wants near), then the upgrades, the sciences and the level
/// (all monotonic unlocks on the player or the fresh unit), and the model toggle last (it may swap the command set,
/// so the button is re-revealed after it).
inline std::vector<Action> fixturePlan(const Row &row)
{
	std::vector<Action> plan;
	if (!fixable(row))
	{
		return plan; // an unfixable row plans nothing: the sweep does not fake option / other / none
	}
	const std::vector<Condition> conditions = effective(row);
	for (Kind want : { Kind::Object, Kind::Upgrade, Kind::Science, Kind::Level, Kind::Model })
	{
		for (const Condition &c : conditions)
		{
			if (kindOf(c.condition) == want)
			{
				plan.push_back(Action{ want, c.value, c.satisfiedBy });
			}
		}
	}
	return plan;
}

} // namespace greyfix
