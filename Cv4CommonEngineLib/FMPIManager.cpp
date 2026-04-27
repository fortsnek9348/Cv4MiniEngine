#include "FMPIManager.h"

FMPIManager& FMPIManager::getInstance()
{
	static FMPIManager x;
	return x;
}
