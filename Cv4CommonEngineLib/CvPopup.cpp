#include "inc/Cv4CommonEngineLib/CvPopup.h"

#include <ranges>
#include <stdexcept>

CvPopup::Control& CvPopup::findControl(EControlType type, int iGroup)
{
	auto v = controls | std::views::reverse;
	const auto it = std::ranges::find_if(v, [&](const Control& ctrl) {
		return ctrl.type == type && ctrl.iGroup == iGroup;
	});
	if (it != v.end())
		return *it;
	else
		throw std::runtime_error("Attempt to access non-existant control in popup.");
}

