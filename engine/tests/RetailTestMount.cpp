// OpenBFME unit tests. GPL-3.0.

#include "RetailTestMount.h"

#include "Common/ModMount.h"
#include "Common/RetailArchivePolicy.h"
#include "GameEngineDevice/Win32Device/Common/Win32BIGFileSystem.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>

#ifndef OPENBFME_TEST_DATA_DIR
#define OPENBFME_TEST_DATA_DIR "tests/data"
#endif

namespace retailtest
{

namespace
{

std::string envOrEmpty(const char *name)
{
	const char *v = std::getenv(name);
	return v ? std::string(v) : std::string();
}

std::unique_ptr<Mount> build(const std::vector<std::pair<std::string, std::string>> &installsWanted)
{
	std::unique_ptr<Mount> m = std::make_unique<Mount>();
	std::vector<RetailInstall> installs;
	for (const auto &want : installsWanted)
	{
		RetailInstall inst;
		inst.label = want.first;
		inst.root = want.second;
		std::string policyId = (want.first == "rotwk") ? "rotwk-201" : "bfme2-106";
		if (!RetailArchivePolicy::loadBuiltin(policyId, inst.policy, &m->error))
		{
			return m;
		}
		installs.push_back(inst);
	}
	m->fs = std::make_unique<Win32BIGFileSystem>();
	RetailMountOptions options; // md5 verification is the smoke test's job; the size/name policy still runs
	RetailMountReport report = MountRetailArchives(*m->fs, installs, options);
	if (!report.ok)
	{
		for (const std::string &e : report.errors)
		{
			m->error += e + "\n";
		}
		m->fs.reset();
		return m;
	}
	for (const MountedArchive &a : report.mounted)
	{
		m->archives.push_back(a.install + ":" + a.canonicalPath);
	}
	m->identityArchives = report.mounted;
	// the button sweep (docs/ennorath/BUTTON_SWEEP.md): OPENBFME_SWEEP_OVERLAY=<a mod's install folder> mounts the mod over this base for the whole process, as
	// EnnorathScenario.h does, so the FACT sweeps run on the mod's data. Only the sweep's own runs set it; a failed mount is an error, never the base alone.
	const std::string overlay = envOrEmpty("OPENBFME_SWEEP_OVERLAY");
	if (!overlay.empty() && installs.size() == 2)
	{
		static std::vector<std::unique_ptr<Win32BIGFileSystem>> bases; // the overlay's file system reads through the base's archives for the process
		bases.push_back(std::move(m->fs));
		m->fs = std::make_unique<Win32BIGFileSystem>();
		ModMountOptions modOptions;
		modOptions.rotwkInstallRoot = installs[0].root;
		const ModMountReport mounted = MountInstallOverlay(*m->fs, report, overlay, modOptions);
		if (!mounted.ok)
		{
			m->error = "the sweep's overlay did not mount: " + overlay + "\n";
			for (const std::string &e : mounted.errors)
			{
				m->error += e + "\n";
			}
			m->fs.reset();
		}
	}
	return m;
}

} // namespace

Mount *pureMount()
{
	static std::unique_ptr<Mount> mount;
	static bool tried = false;
	std::string rotwk = envOrEmpty("ROTWK_INSTALL");
	std::string bfme2 = envOrEmpty("BFME2_INSTALL");
	if (rotwk.empty() || bfme2.empty())
	{
		return nullptr;
	}
	if (!tried)
	{
		tried = true;
		mount = build({ { "rotwk", rotwk }, { "bfme2", bfme2 } });
	}
	return mount.get();
}

std::unique_ptr<Mount> freshPureMount()
{
	std::string rotwk = envOrEmpty("ROTWK_INSTALL");
	std::string bfme2 = envOrEmpty("BFME2_INSTALL");
	if (rotwk.empty() || bfme2.empty())
	{
		return nullptr;
	}
	return build({ { "rotwk", rotwk }, { "bfme2", bfme2 } });
}

Mount *bfme2Mount()
{
	static std::unique_ptr<Mount> mount;
	static bool tried = false;
	std::string bfme2 = envOrEmpty("BFME2_INSTALL");
	if (bfme2.empty())
	{
		return nullptr;
	}
	if (!tried)
	{
		tried = true;
		mount = build({ { "bfme2", bfme2 } });
	}
	return mount.get();
}

std::string bfme2Install()
{
	return envOrEmpty("BFME2_INSTALL");
}

std::string dataDir()
{
	return OPENBFME_TEST_DATA_DIR;
}

bool readLocalFile(const std::string &path, std::vector<unsigned char> &out, std::string *error)
{
	std::ifstream in(path, std::ios::binary);
	if (!in)
	{
		if (error)
		{
			*error = "cannot open " + path;
		}
		return false;
	}
	out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	return true;
}

std::string installLanguage()
{
	const std::string rotwk = envOrEmpty("ROTWK_INSTALL");
	return rotwk.empty() ? std::string() : DetectRetailLanguageOfInstall(rotwk);
}

bool skipUnlessEnglish(const char *what)
{
	std::string language = installLanguage();
	const std::string rotwk = envOrEmpty("ROTWK_INSTALL");
	const std::string voice = rotwk.empty() ? std::string() : DetectRetailVoiceLanguageOfInstall(rotwk);
	if (language == "English" && (voice.empty() || voice == "English"))
	{
		return false;
	}
	if (!voice.empty() && voice != language)
	{
		language += " with " + voice + " voices";
	}
	std::printf("SKIP: %s is pinned to the English retail data; the install is %s: not run\n", what, language.empty() ? "of no known language" : language.c_str());
	std::fflush(stdout);
	return true;
}

void printSkip(const char *testName)
{
	// lane CONTRIB-1 (GitHub #23): the binary-fact tests need RW_GAME_DAT (the RotWK game.dat) and name it in their test name; the install
	// variables were named for them too, which read as wrong when both installs were set
	if (std::strstr(testName, "RW_GAME_DAT") != nullptr)
	{
		std::printf("SKIP: %s: needs the variables it names (RW_GAME_DAT = the path of RotWK's game.dat); not run\n", testName);
	}
	else
	{
		std::printf("SKIP: %s needs ROTWK_INSTALL and BFME2_INSTALL (retail files); not run\n", testName);
	}
	std::fflush(stdout);
}

} // namespace retailtest
