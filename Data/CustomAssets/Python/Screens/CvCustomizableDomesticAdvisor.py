import CvDomesticAdvisor

# Dummy stuff to stop BUG init complaining

# Reposition if resolution (or language) changes
g_bMustCreatePositions = True
def forcePositionCalc (*args):
	global g_bMustCreatePositions
	g_bMustCreatePositions = True

# used to access the customizing flag
g_advisor = None
def isCustomizing():
	return g_advisor.customizing

# widget help text
def getEditHelpText(eWidgetType, iData1, iData2, bOption):
	if isCustomizing():
		return BugUtil.getPlainText("TXT_KEY_CDA_STOP_EDITING")
	else:
		return BugUtil.getPlainText("TXT_KEY_CDA_START_EDITING")

CvCustomizableDomesticAdvisor = CvDomesticAdvisor.CvDomesticAdvisor