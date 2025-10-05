#pragma once
#ifndef XTEA_SNIFFER_H
#define XTEA_SNIFFER_H

#include "pin.H"
#include <string>

namespace XteaSniffer
{
	int InitXteaSniffer(std::string pid, std::string filename);

} // namespace XteaSniffer
#endif