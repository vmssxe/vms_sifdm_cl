#pragma once

#include "vmsXmlUtil.h"
#include "vmsMsXmlUtil.h"

class vmsMsXmlSerializationInputStream : public vmsSerializationInputStream,
	public vmsSerializationInputStreamBinding
{
public:
	typedef std::shared_ptr <vmsMsXmlSerializationInputStream> tSP;
public:
	virtual vmsSerializationInputStream::tSP SelectNode (LPCWSTR pwszName)
	{
		assert (m_spNode != NULL);
		IXMLDOMNodePtr spNode;
		m_spNode->selectSingleNode ((BSTR)pwszName, &spNode);
		if (!spNode)
			return NULL;

		tSP spResult = std::make_shared <vmsMsXmlSerializationInputStream> ();
		spResult->m_spNode = spNode;
		return spResult;
	}

	virtual std::vector <vmsSerializationInputStream::tSP> SelectNodes (const std::wstring& wstrName)
	{
		assert (m_spNode != NULL);
		std::vector <vmsSerializationInputStream::tSP> vResult;
		IXMLDOMNodeListPtr spList;
		m_spNode->selectNodes ((BSTR)wstrName.c_str (), &spList);
		if (!spList)
			return vResult;
		IXMLDOMNodePtr spNode;
		while (SUCCEEDED (spList->nextNode (&spNode)) && spNode != nullptr)
		{
			vResult.push_back (std::make_shared <vmsMsXmlSerializationInputStream> (spNode));
			spNode = NULL;
		}
		return vResult;
	}

	virtual bool ReadValue (LPCWSTR pwszName, std::wstring& val, bool bAttribute = false)
	{
		return getValueText (pwszName, val, bAttribute);
	}

	vmsMsXmlSerializationInputStream ()
	{

	}

	vmsMsXmlSerializationInputStream (IXMLDOMNode *pRootNode) : m_spNode (pRootNode)
	{

	}

	virtual bool BindToStream (std::shared_ptr <std::istream> spStream)
	{
		if (spStream)
		{
			IXMLDOMDocumentPtr spDoc;
			vmsMsXmlUtil::CreateXmlDocumentInstance (spDoc);
			if (!spDoc)
				return false;
			if (FAILED (vmsMsXmlUtil::LoadXmlFromStream (spDoc, spStream.get ())))
				return false;
			IXMLDOMElementPtr spEl;
			spDoc->get_documentElement (&spEl);
			if (!spEl)
				return false;
			m_spNode = spEl;
			assert (m_spNode);
			if (!m_spNode)
				return false;
		}
		else
		{
			m_spNode = nullptr;
		}

		return vmsSerializationInputStreamBinding::BindToStream (spStream);		
	}

protected:
	IXMLDOMNodePtr m_spNode;

	bool getValueText (LPCWSTR pwszName, std::wstring &val, bool bAttribute)
	{
		assert (m_spNode != NULL);
		if (!m_spNode)
			return false;

		if (!bAttribute)
		{
			IXMLDOMNodePtr spItem;
			m_spNode->selectSingleNode ((BSTR)pwszName, &spItem);
			if (!spItem)
				return false;
			CComBSTR bstr;
			spItem->get_text (&bstr);
			val = bstr;
			return true;
		}

		IXMLDOMNamedNodeMapPtr spAttrs;
		m_spNode->get_attributes (&spAttrs);
		if (!spAttrs)
			return false;

		IXMLDOMNodePtr spItem;
		spAttrs->getNamedItem ((BSTR)pwszName, &spItem);
		if (!spItem)
			return false;

		CComVariant vt;
		spItem->get_nodeValue (&vt);
		assert (vt.vt = VT_BSTR);
		if (vt.vt != VT_BSTR)
			return false;

		val = vt.bstrVal;
		return true;
	}
};

class vmsXmlSerializationOutputStream : public vmsSerializationOutputStream,
	public vmsSerializationOutputStreamBinding
{
public:
	typedef std::shared_ptr <vmsXmlSerializationOutputStream> tSP;

	enum Flags
	{
		DoNotWriteXmlTag	= 1,
		XmlSpacePreserve	= 1 << 1,
	};

public:
	vmsXmlSerializationOutputStream (const std::wstring& wstrRootNodeName, unsigned uFlags = 0) : 
		m_wstrNodeName (wstrRootNodeName),
		m_uFlags (uFlags)
	{

	}

	~vmsXmlSerializationOutputStream ()
	{
		if (!m_bFlushedToBoundStream)
			Flush ();
	}

	virtual vmsSerializationOutputStream::tSP CreateNode (LPCWSTR pwszName)
	{
		tSP spNode = std::make_shared <vmsXmlSerializationOutputStream> (pwszName);
		m_mChildNodes.insert (std::make_pair (std::wstring (pwszName), spNode));
		return spNode;
	}

	virtual bool WriteValue (LPCWSTR pwszName, const std::wstring& val, bool bAttribute = false)
	{
		return WriteValueText (pwszName, val, bAttribute);
	}

	virtual void clear () 
	{
		m_mAttributes.clear ();
		m_mValues.clear ();
		m_mChildNodes.clear ();
	}

	virtual bool Flush ()
	{
		if (!m_spBoundStream)
			return false;
		if (m_bFlushedToBoundStream)
			return false;
		if (!(m_uFlags & DoNotWriteXmlTag))
			(*m_spBoundStream) << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
		toStream (*m_spBoundStream);
		return m_bFlushedToBoundStream = true;
	}

protected:
	unsigned m_uFlags;
	std::wstring m_wstrNodeName;
	std::map <std::wstring, std::wstring> m_mAttributes;
	std::map <std::wstring, std::wstring> m_mValues;
	std::multimap <std::wstring, tSP> m_mChildNodes;

	bool WriteValueText (LPCWSTR pwszName, const std::wstring &wstrText, bool bAttribute)
	{
		if (bAttribute)
			m_mAttributes [pwszName] = wstrText;
		else
			m_mValues [pwszName] = wstrText;
		return true;
	}

	void toStream (std::ostream &os)
	{
		os << "<" << vmsXmlUtil::toUtf8 (m_wstrNodeName);

		if (m_uFlags & XmlSpacePreserve)
			os << " xml:space=\"preserve\"";

		for (auto it = m_mAttributes.cbegin (); it != m_mAttributes.cend (); ++it)
			os << " " << vmsXmlUtil::toUtf8 (it->first) << "=\"" << vmsXmlUtil::toUtf8 (it->second) << "\"";

		if (m_mValues.empty () && m_mChildNodes.empty ())
		{
			os << " />";
			return;
		}

		os << ">";

		for (auto it = m_mValues.cbegin (); it != m_mValues.cend (); ++it)
			os << "<" << vmsXmlUtil::toUtf8 (it->first) << ">" << vmsXmlUtil::toUtf8 (it->second) << "</" << vmsXmlUtil::toUtf8 (it->first) << ">";

		for (auto it = m_mChildNodes.cbegin (); it != m_mChildNodes.cend (); ++it)
			it->second->toStream (os);

		os << "</" << vmsXmlUtil::toUtf8 (m_wstrNodeName) << ">";
	}
};