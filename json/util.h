#pragma once
#include "config.h"

template <class TObject>
std::string vmsJsonSerializeObject (TObject& obj, unsigned flags = 0)
{
	auto ss = std::make_shared <std::stringstream> ();
	JsonSerializationOutputStreamClass json;
	json.BindToStream (ss);
	if (!obj.Serialize (&vmsSerializationIoStream (&json), flags))
		return std::string ();
	json.Flush ();
	return ss->str ();
}