#pragma once
class vmsHttpRequestInitData
{
public:
	// each header must has its corresponding CRLF
	void request_headers (const std::string &headers)
	{
		m_requestHeaders = headers;
	}

	std::string request_headers () const
	{
		return m_requestHeaders;
	}

protected:
	std::string m_requestHeaders;
};