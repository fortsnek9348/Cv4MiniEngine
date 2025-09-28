from CvPythonExtensions import *
import CvUtil
import ScreenInput
import CvScreenEnums
import string
import CvScreensInterface
import sys

# globals
gc = CyGlobalContext()
localText = CyTranslator()

class CvCivicsScreen:
	def __init__(self):
		self.anonCtrlId = 0

	def getScreen(self):
		return CyGInterfaceScreen("CivicsScreen", CvScreenEnums.CIVICS_SCREEN)

	def setActivePlayer(self, iPlayer):
		self.iPlayer = iPlayer
		self.activePlayer = gc.getPlayer(iPlayer)
		self.m_paeCurrentCivics = [self.activePlayer.getCivics(i) for i in range(gc.getNumCivicOptionInfos())]
		self.m_paeDisplayCivics = self.m_paeCurrentCivics[:]
		self.m_paeOriginalCivics = self.m_paeCurrentCivics[:]

	def interfaceScreen(self):
		self.setActivePlayer(gc.getGame().getActivePlayer())
	
		screen = self.getScreen()
		
		screen.setInitialTitle(localText.getText("TXT_KEY_CIVICS_SCREEN_TITLE", ()).upper())
		
		# Table layout of VFlow columns
		# Each civic option is a VFlow of civics and the info panel.
		# Each civic is a label|button. Three-state: selected, unselected, disabled. The disabled state only disables selection, not highlight.
		# Event handling is needed to update help text.

		numCivicOptions = gc.getNumCivicOptionInfos()
		numCivics = gc.getNumCivicInfos()
		
		# Bit of a hack. Really, the label needs to report a min/pref width of 1 whilst still returning the correct height for measurement.
		# but then again, if you want the window to start with the max size, you'd need to measure all descriptions.
		kMaxCivicDescHeight = 12

		table = TableLayoutConfig()
		table.cols = ([TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(2, 0)] * numCivicOptions)[:numCivicOptions*2-1]
		table.rows = [
			TableLayoutConfigRowColDesc(0, 0), # header, selections
			TableLayoutConfigRowColDesc(1, 0), # gap
			TableLayoutConfigRowColDesc(0, 0), # highlight header
			TableLayoutConfigRowColDesc(kMaxCivicDescHeight, 1), # highlight desc
			TableLayoutConfigRowColDesc(1, 0), # gap
			TableLayoutConfigRowColDesc(0, 0), # revolt desc
			TableLayoutConfigRowColDesc(0, 0), # footer buttons
		]
		table.cells = [TableLayoutConfigCell(ivec2(c * 2, [0, 2, 3][r])) for c in range(numCivicOptions) for r in range(3)] + [# header, selections, highlight header, highlight desc
			TableLayoutConfigCell(ivec2(0, 5), ivec2(len(table.cols), 1)), # revolt desc
			TableLayoutConfigCell(ivec2(0, 6), ivec2(len(table.cols), 1), RectJustilign(EJustilign.Center, EJustilign.Center)), # footer Cancel
			TableLayoutConfigCell(ivec2(0, 6), ivec2(len(table.cols), 1), RectJustilign(EJustilign.End, EJustilign.Center)), # footer Revolution
			TableLayoutConfigCell(ivec2(0, 6), ivec2(len(table.cols), 1), RectJustilign(EJustilign.End, EJustilign.Center)), # footer Exit
		]
		
		root = ""
		screen.setTableLayout(root, table)
		#screen.setFillLayout(root)
		
		def anon():
			self.anonCtrlId += 1
			return "anon" + str(self.anonCtrlId)

		for civicOptI in range(numCivicOptions):
			optCtrlName = "CivicOptionSelectionPanel" + str(civicOptI)
			screen.newPanel(root, optCtrlName)
			screen.setVFlowLayout(optCtrlName)
			
			labelName = anon()
			screen.newLabel(optCtrlName, labelName, gc.getCivicOptionInfo(civicOptI).getDescription().upper())
			screen.setLabelHAlign(labelName, EAlign.Center)
			
			tableName = "CivicOptionSelectionTable" + str(civicOptI)
			screen.newPanel(optCtrlName, tableName)
			table = TableLayoutConfig()
			table.cols = [TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 1)]
			table.rows = [TableLayoutConfigRowColDesc(0, 0)]
			table.cells = [
				TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), RectJustilign(EJustilign.Center, EJustilign.Center)),
				TableLayoutConfigCell(ivec2(1, 0), ivec2(1, 1), RectJustilign(EJustilign.Start, EJustilign.Stretch)),
			]
			screen.setTableLayout(tableName, table)

			for civicI in range(numCivics):
				civic = gc.getCivicInfo(civicI)
				if civic.getCivicOptionType() == civicOptI:
					name = "CivicSelectionMark" + str(civicI)
					screen.newLabel(tableName, name, "")
					name = "CivicSelectionButton" + str(civicI)
					screen.newActionButton(tableName, name, WidgetTypes.WIDGET_PYTHON, 0, civicI, HeckTuiBorderStyle.NONE)
					screen.setActionButtonLabel(name, civic.getDescription())
					
			labelName = "CivicSelHeaderLabel" + str(civicOptI)
			screen.newLabel(root, labelName, "")
			screen.setLabelHAlign(labelName, EAlign.Center)
			
			labelName = "CivicSelDescLabel" + str(civicOptI)
			screen.newLabel(root, labelName, "")
			screen.setLabelEnableWrapping(labelName)
			
		screen.newLabel(root, "RevoltDesc", "RevoltDesc")
		screen.setLabelHAlign("RevoltDesc", EAlign.Center)
		screen.newActionButton(root, "CancelButton", WidgetTypes.WIDGET_PYTHON, 1, -1)
		screen.setActionButtonLabel("CancelButton", localText.getText("TXT_KEY_SCREEN_CANCEL", ()).upper())
		screen.newActionButton(root, "RevoltButton", WidgetTypes.WIDGET_PYTHON, 2, 0) # Originally WIDGET_REVOLUTION, but that action doesn't do anything.
		screen.setActionButtonLabel("RevoltButton", localText.getText("TXT_KEY_CONCEPT_REVOLUTION", ()).upper())
		screen.newActionButton(root, "ExitButton", WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)
		screen.setActionButtonLabel("ExitButton", localText.getText("TXT_KEY_PEDIA_SCREEN_EXIT", ()).upper())
			
		self.updateControls()
		
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)
		return 0
		
	def trySelectCivic(self, civicI):
		if not self.activePlayer.canDoCivics(civicI):
			return
		
		civic = gc.getCivicInfo(civicI)
		civicOptI = civic.getCivicOptionType()
		self.m_paeCurrentCivics[civicOptI] = civicI
		self.m_paeDisplayCivics[civicOptI] = civicI # consistency
		self.updateControls()
		
	def buildHelpText(self, civicI):
		civic = gc.getCivicInfo(civicI)
		civicOptI = civic.getCivicOptionType()

		szHelpText = u""

		# Upkeep string
		if civic.getUpkeep() != -1 and not self.activePlayer.isNoCivicUpkeep(civicOptI):
			szHelpText = gc.getUpkeepInfo(civic.getUpkeep()).getDescription()
		else:
			szHelpText = localText.getText("TXT_KEY_CIVICS_SCREEN_NO_UPKEEP", ())

		szHelpText += CyGameTextMgr().parseCivicInfo(civicI, False, True, True)
		
		return szHelpText

	def updateControls(self):
		screen = self.getScreen()
		numCivicOptions = gc.getNumCivicOptionInfos()
		numCivics = gc.getNumCivicInfos()
		for civicI in range(numCivics):
			civic = gc.getCivicInfo(civicI)
			i = civic.getCivicOptionType()
			name = "CivicSelectionMark" + str(civicI)
			screen.setLabelText(name, "[X] " if civicI == self.m_paeCurrentCivics[i] else "[ ] " if self.activePlayer.canDoCivics(civicI) else "    ")
			
		for civicOptI in range(numCivicOptions):
			civicI = self.m_paeCurrentCivics[civicOptI]
			civic = gc.getCivicInfo(civicI)
			labelName = "CivicSelHeaderLabel" + str(civicOptI)
			screen.setLabelText(labelName, civic.getDescription())
			
			civicI = self.m_paeDisplayCivics[civicOptI]
			labelName = "CivicSelDescLabel" + str(civicOptI)
			screen.setLabelText(labelName, self.buildHelpText(civicI))
			
		# Update revolt desc
		bChange = self.m_paeCurrentCivics != self.m_paeOriginalCivics
		screen.setVisible("CancelButton", bChange)
		
		# Make the revolution button
		canRevolt = self.activePlayer.canRevolution(0) and bChange
		screen.setVisible("RevoltButton", canRevolt)
		screen.setVisible("ExitButton", not canRevolt)

		# Anarchy
		iTurns = self.activePlayer.getCivicAnarchyLength(self.m_paeDisplayCivics);
		
		if self.activePlayer.canRevolution(0):
			szText = localText.getText("TXT_KEY_ANARCHY_TURNS", (iTurns,))
		else:
			szText = CyGameTextMgr().setRevolutionHelp(self.iPlayer)

		# Maintenance	
		szText += u'\n'		
		szText += localText.getText("TXT_KEY_CIVIC_SCREEN_UPKEEP", (self.activePlayer.getCivicUpkeep(self.m_paeDisplayCivics, True), ))

		screen.setLabelText("RevoltDesc", szText)
	
	# Will handle the input for this screen...
	def handleInput(self, inputClass):
		if inputClass.eNotifyCode == NotifyCode.NOTIFY_CLICKED and not (inputClass.uiFlags & MouseFlags.MOUSE_LBUTTONUP):
			return 0
			
		if inputClass.iData1 == 0: # CivicSelectionButton
			civicI = inputClass.iData2
			civic = gc.getCivicInfo(civicI)
			civicOptI = civic.getCivicOptionType()
			if inputClass.eNotifyCode == NotifyCode.NOTIFY_CLICKED:
				self.trySelectCivic(civicI)
				return 1
			elif inputClass.eNotifyCode == NotifyCode.NOTIFY_CURSOR_MOVE_ON:
				#print >> sys.stderr, civicI
				self.m_paeDisplayCivics[civicOptI] = civicI
				self.updateControls()
				return 1
			elif inputClass.eNotifyCode == NotifyCode.NOTIFY_CURSOR_MOVE_OFF:
				if self.m_paeDisplayCivics[civicOptI] == civicI:
					self.m_paeDisplayCivics[civicOptI] = self.m_paeCurrentCivics[civicOptI]
					self.updateControls()
				return 1
		elif inputClass.iData1 == 1: # Cancel
			if inputClass.eNotifyCode == NotifyCode.NOTIFY_CLICKED:
				self.m_paeCurrentCivics = self.m_paeOriginalCivics[:]
				self.m_paeDisplayCivics = self.m_paeOriginalCivics[:]
				self.updateControls()
				return 1
		elif inputClass.iData1 == 2: # Revolt
			if inputClass.eNotifyCode == NotifyCode.NOTIFY_CLICKED:
				if self.activePlayer.canRevolution(0):
					CyMessageControl().sendUpdateCivics(self.m_paeDisplayCivics)			
				self.getScreen().hideScreen()
				return 1
				
		return 0
		
	def update(self, fDelta):
		return




