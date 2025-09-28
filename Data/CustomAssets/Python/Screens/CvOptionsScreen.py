# Minimally based on original BTS version. Everything thrown out except the player options.

# For Input see CvOptionsScreenCallbackInterface in Python\EntryPoints\

import CvUtil
from CvPythonExtensions import *

# globals
gc = CyGlobalContext()
UserProfile = CyUserProfile()
localText = CyTranslator()

kCallbackIFace = "CvOptionsScreenCallbackInterface"

class CvOptionsScreen:
	"Options Screen"
	
	def __init__(self):
		self.dialog = None
		self.displayedOptions = set()

#########################################################################################
################################## SCREEN CONSTRUCTION ##################################
#########################################################################################
	
	def initText(self):
		self.szTabControlName = localText.getText("TXT_KEY_OPTIONS_TITLE", ())
		
		#self.szGameOptionsTabName = localText.getText("TXT_KEY_OPTIONS_GAME", ())
		#self.szGraphicsOptionsTabName = localText.getText("TXT_KEY_OPTIONS_GRAPHICS", ())
		#self.szAudioOptionsTabName = localText.getText("TXT_KEY_OPTIONS_AUDIO", ())
		#self.szOtherOptionsTabName = localText.getText("TXT_KEY_OPTIONS_SCREEN_OTHER", ())
		
	def refreshScreen (self):
		#################### Game Options ####################
		
		for i in range(PlayerOptionTypes.NUM_PLAYEROPTION_TYPES):
			if i in self.displayedOptions:
				self.dialog.setCheckBoxValue("GameOptionCheckBox_" + str(i), UserProfile.getPlayerOption(i))
		
		# Languages Dropdown
		#self.getTabControl().setValue("LanguagesDropdownBox", CyGame().getCurrentLanguage())

	def interfaceScreen(self):
		"Initial creation of the screen"
		self.initText()
		
		if self.dialog is not None:
			self.dialog.destroy()
		self.dialog = CyTuiDialog()
		self.dialog.setInitialTitle(self.szTabControlName)

		self.drawGameOptionsTab("")
		
		self.dialog.showAsModalDialog()
		
	def getTabControl(self):
		return self.dialog

	def drawGameOptionsTab(self, root):
		dialog = self.dialog
		
		dialog.setVFlowLayout(root)
		dialog.newPanel(root, "GameOptionsHLayout")

		dialog.setHFlowLayout("GameOptionsHLayout", EJustilign.Start, EJustilign.Start)
		column1Panel = "GameOptionsColumn1"
		column2Panel = "GameOptionsColumn2"
		dialog.newPanel("GameOptionsHLayout", column1Panel)
		dialog.newPanel("GameOptionsHLayout", column2Panel)
		dialog.setVFlowLayout(column1Panel)
		dialog.setVFlowLayout(column2Panel)
		
		i = 0
		for iOptionLoop in range(PlayerOptionTypes.NUM_PLAYEROPTION_TYPES):
			displayed = True
			
			if iOptionLoop == PlayerOptionTypes.PLAYEROPTION_MODDER_1 and gc.getDefineINT("USE_MODDERS_PLAYEROPTION_1") == 0:
				displayed = False
			if iOptionLoop == PlayerOptionTypes.PLAYEROPTION_MODDER_2 and gc.getDefineINT("USE_MODDERS_PLAYEROPTION_2") == 0:
				displayed = False
			if iOptionLoop == PlayerOptionTypes.PLAYEROPTION_MODDER_3 and gc.getDefineINT("USE_MODDERS_PLAYEROPTION_3") == 0:
				displayed = False
				
			if not displayed:
				continue
				
			self.displayedOptions.add(iOptionLoop)
			
			szOptionDesc = gc.getPlayerOptionsInfoByIndex(iOptionLoop).getDescription()
			szHelp = gc.getPlayerOptionsInfoByIndex(iOptionLoop).getHelp()
			szWidgetName = "GameOptionCheckBox_" + str(iOptionLoop)
			bOptionOn = UserProfile.getPlayerOption(iOptionLoop)
			vbox = column1Panel if i + 1 <= (PlayerOptionTypes.NUM_PLAYEROPTION_TYPES + 1) // 2 else column2Panel
			dialog.newCheckBox(vbox, szWidgetName, szOptionDesc, kCallbackIFace, "handleGameOptionsClicked")
			dialog.setCheckBoxValue(szWidgetName, bOptionOn)
			#tab.setToolTip(szWidgetName, szHelp)
			i += 1
			
		dialog.newCombobox(column1Panel, "lstLanguage",
			[localText.getText("TXT_KEY_LANGUAGE_%d" % i, ())  for i in range(CvGameText().getNumLanguages())],
			kCallbackIFace, "handleLanguagesDropdownBoxInput")
		dialog.setComboboxSelectedIndex("lstLanguage", CyGame().getCurrentLanguage())
	
		dialog.newHRule(root, "GamePanelCenter", HeckTuiBorderStyle.Thin)

		#tab.attachHBox("GamePanelCenter", "LangHBox")
		#
		## Languages Dropdown
		#tab.attachLabel("LangHBox", "LangLabel", localText.getText("TXT_KEY_OPTIONS_SCREEN_LANGUAGE", ()))	# Label
		#szDropdownDesc = "LanguagesDropdownBox"
		#
		#tab.attachSpacer("LangHBox")
		#
		#aszDropdownElements = ()
		#for i in range(CvGameText().getNumLanguages()):
		#	szKey = "TXT_KEY_LANGUAGE_%d" % i
		#	aszDropdownElements = aszDropdownElements + (localText.getText(szKey, ()),)
		#			
		#szCallbackFunction = "handleLanguagesDropdownBoxInput"
		#szWidgetName = "LanguagesDropdownBox"
		#iInitialSelection = CyGame().getCurrentLanguage()
		#tab.attachDropDown("LangHBox", szWidgetName, szDropdownDesc, aszDropdownElements, self.callbackIFace, szCallbackFunction, szWidgetName, iInitialSelection)
		#tab.setLayoutFlag(szWidgetName, "LAYOUT_LEFT")

		########## Lower Panel
		
		dialog.newPanel(root, "BottomButtonsPanel")
		dialog.setHFlowLayout("BottomButtonsPanel", EJustilign.Center, EJustilign.Stretch)

		dialog.newButton("BottomButtonsPanel", "GameOptionsResetButton", localText.getText("TXT_KEY_OPTIONS_RESET", ()), kCallbackIFace, "handleGameReset")

		dialog.newButton("BottomButtonsPanel", "GameOptionsExitButton", localText.getText("TXT_KEY_PEDIA_SCREEN_EXIT", ()), kCallbackIFace, "handleExitButtonInput")
		