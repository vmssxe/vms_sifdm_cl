#pragma once

#include "vmsInternetProxyInfo.h"
#include "vmsInternetUserAgentInfo.h"
#include "vmsHttpRequestInitData.h"

class vmsInternetOperationInitializationData : public vmsThreadSafe
{
public:
	typedef std::shared_ptr <vmsInternetOperationInitializationData> tSP;
	typedef std::shared_ptr <const vmsInternetOperationInitializationData> tcSP;

	virtual void setProxyInfo (vmsInternetProxyInfo::tSP spProxyInfo)
	{
		vmsTHREAD_SAFE_SCOPE;
		m_spProxyInfo = spProxyInfo;
	}

	virtual vmsInternetProxyInfo::tcSP getProxyInfo () const
	{
		vmsTHREAD_SAFE_SCOPE;
		return m_spProxyInfo;
	}

	virtual vmsInternetProxyInfo::tSP getProxyInfo ()
	{
		vmsTHREAD_SAFE_SCOPE;
		return m_spProxyInfo;
	}

	virtual void setUserAgentInfo (vmsInternetUserAgentInfoBase::tSP spUserAgentInfo)
	{
		vmsTHREAD_SAFE_SCOPE;
		m_spUserAgentInfo = spUserAgentInfo;
	}

	virtual vmsInternetUserAgentInfoBase::tcSP getUserAgentInfo () const
	{
		vmsTHREAD_SAFE_SCOPE;
		return m_spUserAgentInfo;
	}

	virtual vmsInternetUserAgentInfoBase::tSP getUserAgentInfo ()
	{
		vmsTHREAD_SAFE_SCOPE;
		return m_spUserAgentInfo;
	}

	virtual void setHttpInitData (const std::shared_ptr <vmsHttpRequestInitData> &data)
	{
		vmsTHREAD_SAFE_SCOPE;
		m_httpData = data;
	}

	std::shared_ptr <vmsHttpRequestInitData> getHttpData () const
	{
		vmsTHREAD_SAFE_SCOPE;
		return m_httpData;
	}

	void readDataFrom (vmsInternetOperationInitializationData *p)
	{
		setProxyInfo (p->getProxyInfo ());
		setUserAgentInfo (p->getUserAgentInfo ());
	}

	vmsInternetOperationInitializationData () {}
	virtual ~vmsInternetOperationInitializationData () {}

protected:
	vmsInternetProxyInfo::tSP m_spProxyInfo;
	vmsInternetUserAgentInfoBase::tSP m_spUserAgentInfo;
	std::shared_ptr <vmsHttpRequestInitData> m_httpData;
};


class vmsInternetOperationInitializationDataReceiver
{
public:
	vmsInternetOperationInitializationDataReceiver () {}

	virtual ~vmsInternetOperationInitializationDataReceiver () {}

	virtual void setInternetOperationInitializationData (vmsInternetOperationInitializationData::tcSP spData)
	{
		m_spInetOpInitData = spData;
	}

	virtual vmsInternetOperationInitializationData::tcSP getInternetOperationInitializationData () const
	{
		return m_spInetOpInitData;
	}

	void readDataFrom (const vmsInternetOperationInitializationDataReceiver *p)
	{
		setInternetOperationInitializationData (p->getInternetOperationInitializationData ());
	}

protected:
	vmsInternetOperationInitializationData::tcSP m_spInetOpInitData;
};