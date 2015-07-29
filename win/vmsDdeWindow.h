#pragma once
class vmsDdeWindow :
	public CWindowImpl <vmsDdeWindow, CWindow, CFrameWinTraits>
{
public:
	using dde_data_process_fn = std::function <LRESULT (HWND, COPYDATASTRUCT*)>;

public:
	vmsDdeWindow (dde_data_process_fn processFn) :
		m_processFn (processFn)
	{
	}

	virtual ~vmsDdeWindow ()
	{
	}

	BEGIN_MSG_MAP (vmsDdeWindow)
		MESSAGE_HANDLER (WM_COPYDATA, OnCopyData)
		MESSAGE_HANDLER (WM_DESTROY, OnDestroy)
	END_MSG_MAP ()

public:
	bool Create (const std::wstring &title)
	{
		return nullptr != CWindowImpl::Create (nullptr,
			nullptr, title.c_str ());
	}

protected:
	dde_data_process_fn m_processFn;

protected:
	LRESULT OnCopyData (UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		return m_processFn ((HWND) wParam, (COPYDATASTRUCT*) lParam);
	}

	LRESULT OnDestroy (UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		PostQuitMessage (0);
		return 0;
	}
};

