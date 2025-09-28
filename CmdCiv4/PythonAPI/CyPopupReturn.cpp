#include "CyPopupReturn.h"

#include <CvPopupReturn.h>

void CyPopupReturn::registerWithPython(const pybind11::module& m)
{
#define R(x) def(#x, &CyPopupReturn::x)

    pybind11::class_<CyPopupReturn>(m, "CyPopupReturn")
        .R(isNone)
        .R(getSelectedRadioButton)
        .R(getEditBoxString)
        .R(getSpinnerWidgetValue)
        .R(getSelectedPullDownValue)
        .R(getSelectedListBoxValue)
        .R(getButtonClicked)
        ;
}

bool CyPopupReturn::isNone() const
{
    return !ptr;
}

int CyPopupReturn::getSelectedRadioButton(int iGroup) const
{
    return ptr->getSelectedRadioButton(iGroup);
}

std::wstring CyPopupReturn::getEditBoxString(int iGroup) const
{
    return ptr->getEditBoxString(iGroup);
}

int CyPopupReturn::getSpinnerWidgetValue(int iGroup) const
{
    return ptr->getSpinnerWidgetValue(iGroup);
}

int CyPopupReturn::getSelectedPullDownValue(int iGroup) const
{
    return ptr->getSelectedPullDownValue(iGroup);
}

int CyPopupReturn::getSelectedListBoxValue(int iGroup) const
{
    return ptr->getSelectedListBoxValue(iGroup);
}

int CyPopupReturn::getButtonClicked() const
{
    return ptr->getButtonClicked();
}
