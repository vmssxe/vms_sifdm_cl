#pragma once
#include "vmsCrashCatcher.h"
#include "vmsCrashReporter.h"
#include "DlgUnhandledException.h"
class vmsAppCrashReporter :
	protected vmsCrashReporter,
	public vmsCrashCatcher
{
public:
	vmsAppCrashReporter(void) {}

	vmsAppCrashReporter (const std::wstring& wstrAppName, const std::wstring& wstrAppVersion,
			const std::wstring& targetHost, const std::wstring& targetPath) :
		m_wstrAppName (wstrAppName), 
		m_wstrAppVersion (wstrAppVersion),
		m_targetHost (targetHost),
		m_targetPath (targetPath) {}

	~vmsAppCrashReporter(void) {}

protected:
	virtual void onCrashDumpCreated () override
	{
		vmsCommandLine cl;
		TCHAR tsz[MAX_PATH] = _T ("");
		GetModuleFileName (NULL, tsz, _countof (tsz));
		cl.setExe (tsz);
		tstring tstrArgs = _T ("-submitDump=\"");
		tstrArgs += m_tstrDumpFile;
		tstrArgs += _T ("\" -faultModule=\"");
		tstrArgs += m_tstrFaultModuleName;
		if (m_tstrFaultModuleName.empty ())
		{
			tstrArgs += _T ("\" -gettingFaultModuleNameError=\"");
			tstrArgs += _itot (m_dwGettingFaultModuleNameError, tsz, 10);
		}
		tstrArgs += _T ("\" -faultModuleCrashAddr=");
		_stprintf_s <MAX_PATH> (tsz, _T ("%I64x"), (UINT64)m_dwpFaultModuleCrashAddress);
		tstrArgs += tsz;
		if (!m_tstrAppRestartArgs.empty ())
		{
			tstrArgs += _T (" -appRestartArgs=\"");
			tstrArgs += m_tstrAppRestartArgs;
			tstrArgs += _T ("\"");
		}
		cl.setArgs (tstrArgs.c_str ());
		cl.Execute ();
		ExitProcess (1);
	}

public:
	bool CheckIfSubmitDumpIsRequestedByCommandLine(bool& bContinue)
	{
		vmsCommandLineParser cp;

		const vmsCommandLineParser::Argument *pArgDump = cp.findArgument (_T ("submitDump"));
		if (!pArgDump)
			return false;

		const vmsCommandLineParser::Argument *pArgFaultModule = cp.findArgument (_T ("faultModule"));
		const vmsCommandLineParser::Argument *pArgGettingFaultModuleNameError = cp.findArgument (_T ("gettingFaultModuleNameError"));
		const vmsCommandLineParser::Argument *pArgFaultModuleCrashAddr = cp.findArgument (_T ("faultModuleCrashAddr"));
		const vmsCommandLineParser::Argument *pArgAppRestartArgs = cp.findArgument (_T ("appRestartArgs"));

		if (pArgAppRestartArgs)
			setAppRestartArgs (pArgAppRestartArgs->second.c_str ());

		bContinue = true;

		if (GetFileAttributes (pArgDump->second.c_str ()) == DWORD (-1))
			return true;

		bContinue = false;

		CoInitialize (NULL);

		bool bSendDumpToServer, restart_app;
		std::wstring user_description;
		show_on_crashed_ui (restart_app, bSendDumpToServer, user_description);

		if (restart_app)
		{
			vmsCommandLine cl;
			TCHAR tsz[MAX_PATH] = _T ("");
			GetModuleFileName (NULL, tsz, _countof (tsz));
			cl.setExe (tsz);
			cl.setArgs (m_tstrAppRestartArgs.c_str ());
			cl.Execute ();
		}

		if (bSendDumpToServer)
		{
			std::string strXml;
			UINT64 uCrashAddr = 0;
			if (pArgFaultModuleCrashAddr)
				_stscanf (pArgFaultModuleCrashAddr->second.c_str (), _T ("%I64x"), &uCrashAddr);
			assert (!m_wstrAppName.empty () && !m_wstrAppVersion.empty ());
			vmsCrashReporter::GenerateXml (m_wstrAppName.c_str (), m_wstrAppVersion.c_str (),
				user_description.c_str (), pArgFaultModule ? pArgFaultModule->second.c_str () : NULL, 
				pArgGettingFaultModuleNameError ? (DWORD)_ttoi (pArgGettingFaultModuleNameError->second.c_str ()) : 0, 
				(DWORD_PTR)uCrashAddr, NULL, strXml);

			SubmitDumpToServer (m_targetHost.c_str (), m_targetPath.c_str (), pArgDump->second.c_str (), strXml.c_str ());
		}

		DeleteFile (pArgDump->second.c_str ());

		CoUninitialize ();

		return true;
	}

	void setAppRestartArgs (LPCTSTR ptszArgs) 
	{
		m_tstrAppRestartArgs = ptszArgs;
	}

	void InitializeCrashCatcher ()
	{
		vmsCrashCatcher::Initialize ();
	}

	void setAppNameAndVersion (const std::wstring& wstrAppName, const std::wstring& wstrAppVersion)
	{
		m_wstrAppName = wstrAppName;
		m_wstrAppVersion = wstrAppVersion;
	}

	void setTargetHostAndPath (const std::wstring& targetHost, const std::wstring& targetPath)
	{
		m_targetHost = targetHost;
		m_targetPath = targetPath;
	}

protected:
	tstring m_tstrAppRestartArgs;
	std::wstring m_wstrAppName;
	std::wstring m_wstrAppVersion;
	std::wstring m_targetHost, m_targetPath;

protected:
	virtual void show_on_crashed_ui (bool& restart_app, bool& send_crash_report, 
		std::wstring& user_description)
	{
		CDlgUnhandledException dlgUnhExc (m_wstrAppName);
		send_crash_report = IDOK == dlgUnhExc.DoModal ();
		restart_app = dlgUnhExc.m_bRestartApp;
		user_description = dlgUnhExc.m_wstrDescription;
	}
};

