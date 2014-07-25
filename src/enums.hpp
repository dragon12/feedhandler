
#ifndef __ENUMS_H__
#define __ENUMS_H__

#include <iostream>
enum class side
{
	bid = 0,
	ask = 1
};
inline std::ostream& operator<<(std::ostream &os, side s)
{
	return (os << (s == side::bid ? "bid" : "ask"));
}

#endif
