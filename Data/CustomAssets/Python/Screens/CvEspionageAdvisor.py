## Sid Meier's Civilization 4
## Copyright Firaxis Games 2005
## Improvements to this screen by Almightix - thanks
## fortsnek: Adapted for Cv4MiniEngine.
from CvPythonExtensions import *
from PyHelpers import PyPlayer
import CvUtil
import ScreenInput
import CvScreenEnums

# BUG - Better Espionage - start
kHasBUG = False
try:
	import BugCore
	kHasBUG = True
except ImportError:
	pass

if kHasBUG:
	import BugUtil
	import FontUtil
	import SpyUtil
	EspionageOpt = BugCore.game.BetterEspionage
# BUG - Better Espionage - end

# globals
gc = CyGlobalContext()
ArtFileMgr = CyArtFileMgr()
localText = CyTranslator()

CITYMISSION_CITY = 0
CITYMISSION_MISSION = 1

kPythonButtonCivButton = 0
kPythonButtonWeightInc = 1
kPythonButtonWeightDec = 2
kPythonButtonGroupingFlip = 3
kPythonButtonFirstTable = 4

class CvEspionageAdvisor:

	def __init__(self):
		self.SCREEN_NAME = "EspionageAdvisor"

		self.nWidgetCount = 0

		self.iDirtyBit = 0

		self.iTargetPlayer = -1

		self.iActiveCityID = -1
		self.iActiveMissionID = -1

		self.drawMissionTabConstantsDone = 0
		self.drawSpyvSpyTabConstantsDone = 0
		self.CityMissionToggle = CITYMISSION_CITY

	
		# mission / city widgets - initialized to avoid errors with 'handle input'
		# they get set to proper values in def drawMissionTab(self)
		self.szMissionsTitleText = ""
		self.szCitiesTitleText = ""

		self.EPScreenTab = -1

	def getScreen(self):
		return CyGInterfaceScreen(self.SCREEN_NAME, CvScreenEnums.ESPIONAGE_ADVISOR)

	def interfaceScreen(self):
		if not kHasBUG:
			self.iRatioColor = gc.getInfoTypeForString("COLOR_WHITE")
			self.iGoodRatioColor = gc.getInfoTypeForString("COLOR_GREEN")
			self.iBadRatioColor = gc.getInfoTypeForString("COLOR_RED")
	
		self.iTargetPlayer = -1
		self.iActiveCityID = -1
		self.iActiveMissionID = -1
		self.iActivePlayer = CyGame().getActivePlayer()
		
		# mission constants
		for iMissionLoop in range(gc.getNumEspionageMissionInfos()):
			pMission = gc.getEspionageMissionInfo(iMissionLoop)
			if pMission.getCost() != -1 and pMission.isPassive():
				if pMission.isInvestigateCity():
					self.MissionInvestigateCity = iMissionLoop
				elif pMission.isSeeDemographics():
					self.MissionSeeDemo = iMissionLoop
				elif pMission.isSeeResearch():
					self.MissionSeeResearch = iMissionLoop
				else:
					self.MissionCityVisibility = iMissionLoop

		screen = self.getScreen()
		if screen.isActive():
			return
		#screen.setRenderInterfaceOnly(True);
		screen.setInitialTitle(localText.getText("TXT_KEY_ESPIONAGE_SCREEN", ()).upper())
	
		root = ""
	
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # Content
			TableLayoutConfigCell(ivec2(0, 1), ivec2(1, 1), RectJustilign(EJustilign.End, EJustilign.Stretch)) # Footer
		]
		table.gap = ivec2(0, 1)
		screen.setTableLayout(root, table)
		
		screen.newPanel(root, "Content")
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 1)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # CivTable
			TableLayoutConfigCell(ivec2(1, 0)), # EspionagePanel
		]
		table.gap = ivec2(1, 0)
		screen.setTableLayout("Content", table)
		
		screen.newScrollBarPanel("Content", "CivScrollPanel", "CivTable", HeckTuiAxis.Vertical)
		screen.newPanel("Content", "EspionagePanel")

		screen.newActionButton(root, "ExitButton", WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)
		screen.setActionButtonLabel("ExitButton", localText.getText("TXT_KEY_PEDIA_SCREEN_EXIT", ()).upper())

		self.drawCivList(screen, "CivTable")
		self.drawEspionagePanel(screen, "EspionagePanel")
		self.refreshEspionagePanel(screen)
		self.updateCivButtons(screen)
		self.updateWeights(screen)
		
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)
		
	def drawCivList(self, screen, root):
		pActivePlayer = gc.getPlayer(self.iActivePlayer)
		pActiveTeam = gc.getTeam(pActivePlayer.getTeam())

		self.aiKnownPlayers = []

		for iLoop in range(gc.getMAX_PLAYERS()):
			pPlayer = gc.getPlayer(iLoop)
			if pPlayer.getTeam() != pActivePlayer.getTeam() and not pPlayer.isBarbarian():
				if pPlayer.isAlive() and pActiveTeam.isHasMet(pPlayer.getTeam()):
					self.aiKnownPlayers.append(iLoop)

		if self.iTargetPlayer == -1 and len(self.aiKnownPlayers):
			self.iTargetPlayer = self.aiKnownPlayers[0]
			
		# [[X] Wang Kon] | [+][-] Weight: 0  | 143%  | 307 EPs (+1) / 513 EPs (+5)
		
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 0)] * 2
		table.rows = [TableLayoutConfigRowColDesc(0, 0)] * 3
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 3), RectJustilign(EJustilign.Stretch, EJustilign.Center)), # CivButton
			TableLayoutConfigCell(ivec2(1, 0), ivec2(1, 1), RectJustilign(EJustilign.Stretch, EJustilign.Center)), # WeightPanel
			TableLayoutConfigCell(ivec2(1, 2), ivec2(1, 1), RectJustilign(EJustilign.Stretch, EJustilign.Center)), # PercentLabel
			TableLayoutConfigCell(ivec2(1, 1), ivec2(1, 1), RectJustilign(EJustilign.Stretch, EJustilign.Center)), # EPsLabel
		]
		table.gap = ivec2(1, 0)
		screen.setTableLayout(root, table)
		
		iRatioColor = EspionageOpt.getDefaultRatioColor() if kHasBUG else self.iRatioColor
		iGoodRatioColor = EspionageOpt.getGoodRatioColor() if kHasBUG else self.iGoodRatioColor
		iBadRatioColor = EspionageOpt.getBadRatioColor() if kHasBUG else self.iBadRatioColor
			
		for playerI in self.aiKnownPlayers:
			pTargetPlayer = gc.getPlayer(playerI)
			iTargetTeam = pTargetPlayer.getTeam()
			
			screen.newActionButton(root, "CivButton" + str(playerI), WidgetTypes.WIDGET_PYTHON, kPythonButtonCivButton, playerI) # , HeckTuiBorderStyle.NONE
			screen.setActionButtonHAlign("CivButton" + str(playerI), EAlign.Left)

			name = "WeightPanel" + str(playerI)
			screen.newPanel(root, name)
			screen.setHFlowLayout(name)
			screen.newActionButton(name, name + "Inc", WidgetTypes.WIDGET_PYTHON, kPythonButtonWeightInc, playerI, HeckTuiBorderStyle.NONE)
			screen.newActionButton(name, name + "Dec", WidgetTypes.WIDGET_PYTHON, kPythonButtonWeightDec, playerI, HeckTuiBorderStyle.NONE)
			screen.newLabel(name, name + "Label", u"")
			screen.setActionButtonLabel(name + "Inc", "[+]")
			screen.setActionButtonLabel(name + "Dec", "[-]")
			
			iMultiplier = self.getEspionageMultiplier(self.iActivePlayer, playerI)
			szText = u"%i%%" % (iMultiplier,)
			if (iBadRatioColor >= 0 and iMultiplier >= (EspionageOpt.getBadRatioCutoff() if kHasBUG else 0)):
				szText = localText.changeTextColor(szText, iBadRatioColor)
			elif (iGoodRatioColor >= 0 and iMultiplier <= (EspionageOpt.getGoodRatioCutoff() if kHasBUG else 0)):
				szText = localText.changeTextColor(szText, iGoodRatioColor)
			elif (iRatioColor >= 0):
				szText = localText.changeTextColor(szText, iRatioColor)
				
			# Symbols for 'Demographics' and 'Research'
			iPlayerEPs = self.getPlayerEPs(self.iActivePlayer, playerI)
			iDemoCost = pActivePlayer.getEspionageMissionCost(self.MissionSeeDemo, playerI, None, -1)
			iTechCost = pActivePlayer.getEspionageMissionCost(self.MissionSeeResearch, playerI, None, -1)
			
			if iPlayerEPs >= iDemoCost:
				szText += u" [%]"
			if iPlayerEPs >= iTechCost:
				szText += u" [%c]" % (gc.getCommerceInfo(CommerceTypes.COMMERCE_RESEARCH).getChar(),)

			screen.newLabel(root, "MultiplierLabel" + str(playerI), szText)

			screen.newLabel(root, "Spending" + str(playerI), u"")

	def updateCivButtons(self, screen):
		for playerI in self.aiKnownPlayers:
			pTargetPlayer = gc.getPlayer(playerI)

			name = (u"[\u25cf] " if playerI == self.iTargetPlayer else u"[ ] ") + CyTranslator().changeTextColor(pTargetPlayer.getName(), gc.getPlayerColorInfo(pTargetPlayer.getPlayerColor()).getTextColorType())
			screen.setActionButtonLabel("CivButton" + str(playerI), name)
			
			
			
	def updateWeights(self, screen):
		pActivePlayer = gc.getPlayer(self.iActivePlayer)
		for playerI in self.aiKnownPlayers:
			pTargetPlayer = gc.getPlayer(playerI)
			iTargetTeam = pTargetPlayer.getTeam()
			name = "WeightPanel" + str(playerI)
			screen.setLabelText(name + "Label", u' ' + localText.getText("TXT_KEY_ESPIONAGE_SCREEN_SPENDING_WEIGHT", ()) + u": %d" % (pActivePlayer.getEspionageSpendingWeightAgainstTeam(iTargetTeam)))
			
			# EPs
			iPlayerEPs = self.getPlayerEPs(self.iActivePlayer, playerI)
			# EPs Against
			iTargetEPs = self.getPlayerEPs(playerI, self.iActivePlayer)
			szText = localText.getText("TXT_KEY_ESPIONAGE_NUM_EPS", (iPlayerEPs,))
			szText = u"<font=2>" + localText.getText("TXT_KEY_ESPIONAGE_NUM_EPS", (iTargetEPs, )) + "</font>"
			
			iUsSpending = pActivePlayer.getEspionageSpending(iTargetTeam)
			if iUsSpending is None or iUsSpending == 0:
				szUsSpending = u""
			else:
				szUsSpending = localText.changeTextColor(u"(%+i)" % (iUsSpending,), gc.getInfoTypeForString("COLOR_GREEN" if iUsSpending > 0 else "COLOR_YELLOW"))

			# EP Spending Against (Points per turn)
			if kHasBUG:
				iThemSpending = SpyUtil.getDifferenceByPlayer(playerI, self.iActivePlayer)
				if iThemSpending is None or iThemSpending == 0:
					szThemSpending = u""
				else:
					szThemSpending = localText.changeTextColor(u"(%+i)" % (iThemSpending,), gc.getInfoTypeForString("COLOR_GREEN" if iThemSpending > 0 else "COLOR_YELLOW"))
				text = u"%s %s / %s %s"  % (localText.getText("TXT_KEY_ESPIONAGE_NUM_EPS", (iPlayerEPs,)), szUsSpending, localText.getText("TXT_KEY_ESPIONAGE_NUM_EPS", (iTargetEPs,)), szThemSpending)
			else:
				text = u"%s %s"  % (localText.getText("TXT_KEY_ESPIONAGE_NUM_EPS", (iPlayerEPs,)), szUsSpending)
				
			screen.setLabelText("Spending" + str(playerI), text)
			
	def drawEspionagePanel(self, screen, root):
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0)]
		table.rows = [TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 1)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 2)), # FirstPanel
			TableLayoutConfigCell(ivec2(1, 0)), # PassivePanel
			TableLayoutConfigCell(ivec2(1, 1)), # SecondPanel
		]
		table.gap = ivec2(0, 0)
		screen.setTableLayout(root, table)
		
		screen.newPanel(root, "FirstPanel")
		screen.newPanel(root, "PassivePanel")
		screen.newPanel(root, "SecondPanel")
		
		#self.CityMissionToggle = CITYMISSION_CITY
		
		screen.setVFlowLayout("FirstPanel")
		screen.newActionButton("FirstPanel", "FirstButton", WidgetTypes.WIDGET_PYTHON, kPythonButtonGroupingFlip, 0)
		screen.setActionButtonLabel("FirstButton", localText.getText("TXT_KEY_CONCEPT_CITIES" if self.CityMissionToggle == CITYMISSION_CITY else "TXT_KEY_ESPIONAGE_SCREEN_MISSIONS", ()))
		screen.newRichTextTable("FirstPanel", "FirstTable", [u""], WidgetTypes.WIDGET_PYTHON, kPythonButtonFirstTable, -1)
		
		screen.setVFlowLayout("PassivePanel")
		screen.newLabel("PassivePanel", "PassiveLabel", localText.getText("TXT_KEY_ESPIONAGE_SCREEN_PASSIVE_EFFECTS", ()))
		screen.setLabelHAlign("PassiveLabel", EAlign.Center)
		# Dummy header for min width.
		screen.newRichTextTable("PassivePanel", "PassiveTable", [u"", u"XX"], WidgetTypes.WIDGET_PYTHON, kPythonButtonFirstTable, -1)
		screen.setRichTextTableColumnHAlign("PassiveTable", 1, EAlign.Right)
		
		screen.setVFlowLayout("SecondPanel")
		screen.newActionButton("SecondPanel", "SecondButton", WidgetTypes.WIDGET_PYTHON, kPythonButtonGroupingFlip, 0)
		screen.setActionButtonLabel("SecondButton", localText.getText("TXT_KEY_CONCEPT_CITIES" if self.CityMissionToggle != CITYMISSION_CITY else "TXT_KEY_ESPIONAGE_SCREEN_MISSIONS", ()))
		screen.newRichTextTable("SecondPanel", "SecondTable", [u"", u"XX"], WidgetTypes.WIDGET_GENERAL, -1, -1)
		screen.setRichTextTableColumnHAlign("SecondTable", 1, EAlign.Right)

	def drawMissionTab(self, screen, root):
		screen = self.getScreen()

#		BugUtil.debug("CvEspionage Advisor: drawMissionsTab")

		pActivePlayer = gc.getPlayer(self.iActivePlayer)
		pActiveTeam = gc.getTeam(pActivePlayer.getTeam())

		self.drawMissionTabConstants()

		self.szLeftPaneWidget = self.getNextWidgetName()
		screen.addPanel( self.szLeftPaneWidget, "", "", true, true,
			self.X_LEFT_PANE, self.Y_LEFT_PANE, self.W_LEFT_PANE, self.H_LEFT_PANE, PanelStyles.PANEL_STYLE_MAIN)

		self.szScrollPanel = self.getNextWidgetName()
		screen.addPanel( self.szScrollPanel, "", "", true, true,
			self.X_SCROLL, self.Y_SCROLL, self.W_SCROLL, self.H_SCROLL, PanelStyles.PANEL_STYLE_EMPTY)

		self.aiKnownPlayers = []
		self.aiUnknownPlayers = []
		self.iNumEntries= 0

		for iLoop in range(gc.getMAX_PLAYERS()):
			pPlayer = gc.getPlayer(iLoop)
			if (pPlayer.getTeam() != pActivePlayer.getTeam() and not pPlayer.isBarbarian()):
				if (pPlayer.isAlive()):
					if (pActiveTeam.isHasMet(pPlayer.getTeam())):
						self.aiKnownPlayers.append(iLoop)
						self.iNumEntries = self.iNumEntries + 1

						if (self.iTargetPlayer == -1):
							self.iTargetPlayer = iLoop

		while(self.iNumEntries < 17):
			self.iNumEntries = self.iNumEntries + 1
			self.aiUnknownPlayers.append(self.iNumEntries)

		############################
		#### Total EPs Per Turn Text
		############################

		if not kHasBUG and not EspionageOpt.isEnabled():
			self.szTotalPaneWidget = self.getNextWidgetName()
			screen.addPanel( self.szTotalPaneWidget, "", "", true, true,
				self.X_TOTAL_PANE, self.Y_TOTAL_PANE, self.W_TOTAL_PANE, self.H_TOTAL_PANE, PanelStyles.PANEL_STYLE_MAIN )

			self.szMakingText = self.getNextWidgetName()
			szText = u"<font=4>" + localText.getText("TXT_KEY_ESPIONAGE_SCREEN_TOTAL_NUM_EPS", (pActivePlayer.getCommerceRate(CommerceTypes.COMMERCE_ESPIONAGE), )) + "</font>"
			screen.setLabel(self.szMakingText, "Background", szText, CvUtil.FONT_LEFT_JUSTIFY, self.X_MAKING_TEXT, self.Y_MAKING_TEXT, self.Z_CONTROLS, FontTypes.GAME_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1 )

		############################
		#### Right Panel
		############################

		self.szRightPaneWidget = self.getNextWidgetName()
		screen.addPanel( self.szRightPaneWidget, "", "", true, true,
			self.X_RIGHT_PANE, self.Y_RIGHT_PANE, self.W_RIGHT_PANE, self.H_RIGHT_PANE, PanelStyles.PANEL_STYLE_MAIN )

		if (self.iTargetPlayer != -1):
			self.szCitiesTitleText = self.getNextWidgetName()
			if self.CityMissionToggle == CITYMISSION_CITY:
				szText = u"<font=4>" + localText.getText("TXT_KEY_CONCEPT_CITIES", ()) + "</font>"
			else:
				szText = u"<font=4>" + localText.getText("TXT_KEY_ESPIONAGE_SCREEN_MISSIONS", ()) + "</font>"

			if kHasBUG and EspionageOpt.isEnabled():
				szText = localText.changeTextColor(szText, gc.getInfoTypeForString("COLOR_YELLOW"))
				screen.setText(self.szCitiesTitleText, "Background", szText, CvUtil.FONT_LEFT_JUSTIFY, self.X_CITY_LIST, self.Y_CITY_LIST - 40, self.Z_CONTROLS, FontTypes.GAME_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1 )
			else:
				screen.setLabel(self.szCitiesTitleText, "Background", szText, CvUtil.FONT_LEFT_JUSTIFY, self.X_CITY_LIST, self.Y_CITY_LIST - 40, self.Z_CONTROLS, FontTypes.GAME_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1 )

			self.szEffectsTitleText = self.getNextWidgetName()
			szText = u"<font=4>" + localText.getText("TXT_KEY_ESPIONAGE_SCREEN_PASSIVE_EFFECTS", ()) + "</font>"
			screen.setLabel(self.szEffectsTitleText, "Background", szText, CvUtil.FONT_LEFT_JUSTIFY, self.X_EFFECTS_LIST, self.Y_EFFECTS_LIST - 40, self.Z_CONTROLS, FontTypes.GAME_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1 )

			self.szMissionsTitleText = self.getNextWidgetName()
			if self.CityMissionToggle == CITYMISSION_MISSION:
				szText = u"<font=4>" + localText.getText("TXT_KEY_CONCEPT_CITIES", ()) + "</font>"
			else:
				szText = u"<font=4>" + localText.getText("TXT_KEY_ESPIONAGE_SCREEN_MISSIONS", ()) + "</font>"

			if kHasBUG and EspionageOpt.isEnabled():
				szText = localText.changeTextColor(szText, gc.getInfoTypeForString("COLOR_YELLOW"))
				screen.setText(self.szMissionsTitleText, "Background", szText, CvUtil.FONT_LEFT_JUSTIFY, self.X_MISSIONS_LIST, self.Y_MISSIONS_LIST - 40, self.Z_CONTROLS, FontTypes.GAME_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1 )
			else:
				screen.setLabel(self.szMissionsTitleText, "Background", szText, CvUtil.FONT_LEFT_JUSTIFY, self.X_MISSIONS_LIST, self.Y_MISSIONS_LIST - 40, self.Z_CONTROLS, FontTypes.GAME_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1 )

			self.szEffectsCostTitleText = self.getNextWidgetName()
			szText = u"<font=4>" + localText.getText("TXT_KEY_ESPIONAGE_SCREEN_COST", ()) + "</font>"
			screen.setLabel(self.szEffectsCostTitleText, "Background", szText, CvUtil.FONT_LEFT_JUSTIFY, self.X_EFFECTS_COSTS_LIST, self.Y_EFFECTS_COSTS_LIST - 40, self.Z_CONTROLS, FontTypes.GAME_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1 )

			self.szMissionsCostTitleText = self.getNextWidgetName()
			szText = u"<font=4>" + localText.getText("TXT_KEY_ESPIONAGE_SCREEN_COST", ()) + "</font>"
			screen.setLabel(self.szMissionsCostTitleText, "Background", szText, CvUtil.FONT_LEFT_JUSTIFY, self.X_MISSIONS_COSTS_LIST, self.Y_MISSIONS_COSTS_LIST - 40, self.Z_CONTROLS, FontTypes.GAME_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1 )

			############################
			#### Left Leaders Panel
			############################

			self.drawMissionTab_LeftLeaderPanal(screen)

		return

	def drawMissionTabConstants(self):

		# skip this is we have already done it
		if kHasBUG and EspionageOpt.isEnabled():
			if self.drawMissionTabConstantsDone == 2:
				return
		else:
			if self.drawMissionTabConstantsDone == 1:
				return

		if kHasBUG and EspionageOpt.isEnabled():
			self.drawMissionTabConstantsDone = 2
		else:
			self.drawMissionTabConstantsDone = 1

		self.MissionLeaderPanel_X_LeaderIcon = 21
		self.MissionLeaderPanel_X_LeaderNamePanel = 5
		self.MissionLeaderPanel_X_LeaderName = 55
		self.MissionLeaderPanel_X_Multiplier = 190
		self.MissionLeaderPanel_X_CounterEP = 220
		self.MissionLeaderPanel_X_EPoints = 300
		self.MissionLeaderPanel_X_PassiveMissions = 380
		self.MissionLeaderPanel_X_WghtInc = 53
		self.MissionLeaderPanel_X_WghtDec = 68
		self.MissionLeaderPanel_X_Wght = 85
		self.MissionLeaderPanel_X_EPointsTurn = self.MissionLeaderPanel_X_EPoints + 4
		self.MissionLeaderPanel_X_EspionageIcon = 3

		if kHasBUG and EspionageOpt.isEnabled():
			self.MissionLeaderPanel_X_EPointsTurn = self.MissionLeaderPanel_X_EPoints + 4
		else:
			self.MissionLeaderPanel_X_EPointsTurn = 247

		self.MissionLeaderPanelTopRow = 0
		self.MissionLeaderPanelBottomRow = 15
		self.MissionLeaderPanelMiddle = 6

		# mission constants
		for iMissionLoop in range(gc.getNumEspionageMissionInfos()):
			pMission = gc.getEspionageMissionInfo(iMissionLoop)
			if (pMission.getCost() != -1
			and pMission.isPassive()):
				if pMission.isInvestigateCity():
					self.MissionInvestigateCity = iMissionLoop
				elif pMission.isSeeDemographics():
					self.MissionSeeDemo = iMissionLoop
				elif pMission.isSeeResearch():
					self.MissionSeeResearch = iMissionLoop
				else:
					self.MissionCityVisibility = iMissionLoop

		if kHasBUG and EspionageOpt.isEnabled():
			self.X_LEFT_PANE = 25
			self.Y_LEFT_PANE = 70 - 5
			self.W_LEFT_PANE = 400 + 60
			self.H_LEFT_PANE = 620

			self.X_SCROLL = self.X_LEFT_PANE + 20
			self.Y_SCROLL= 90 - 5
			self.W_SCROLL= 360 + 60
			self.H_SCROLL= 580

			############################
			#### Right Panel
			############################

			self.X_RIGHT_PANE = self.X_LEFT_PANE + self.W_LEFT_PANE + 10
			self.Y_RIGHT_PANE = self.Y_LEFT_PANE
			self.W_RIGHT_PANE = 550 - 50
			self.H_RIGHT_PANE = self.H_LEFT_PANE

			self.X_CITY_LIST = self.X_RIGHT_PANE + 20
			self.Y_CITY_LIST = self.Y_RIGHT_PANE + 60
			self.W_CITY_LIST = 160
			self.H_CITY_LIST = self.H_RIGHT_PANE - 90

			self.X_EFFECTS_LIST = self.X_CITY_LIST + self.W_CITY_LIST + 10
			self.Y_EFFECTS_LIST = self.Y_CITY_LIST
			self.W_EFFECTS_LIST = 210
			self.H_EFFECTS_LIST = 100

			self.X_EFFECTS_COSTS_LIST = self.X_EFFECTS_LIST + self.W_EFFECTS_LIST + 10
			self.Y_EFFECTS_COSTS_LIST = self.Y_EFFECTS_LIST
			self.W_EFFECTS_COSTS_LIST = 60
			self.H_EFFECTS_COSTS_LIST = self.H_EFFECTS_LIST

			self.X_MISSIONS_LIST = self.X_CITY_LIST + self.W_CITY_LIST + 10
			self.Y_MISSIONS_LIST = self.Y_EFFECTS_LIST + self.H_EFFECTS_LIST + 50
			self.W_MISSIONS_LIST = self.W_EFFECTS_LIST
			self.H_MISSIONS_LIST = self.H_CITY_LIST -  + self.H_EFFECTS_LIST - 50

			self.X_MISSIONS_COSTS_LIST = self.X_MISSIONS_LIST + self.W_MISSIONS_LIST + 10
			self.Y_MISSIONS_COSTS_LIST = self.Y_MISSIONS_LIST
			self.W_MISSIONS_COSTS_LIST = self.W_EFFECTS_COSTS_LIST
			self.H_MISSIONS_COSTS_LIST = self.H_MISSIONS_LIST

			############################
			#### Left Leaders Panel
			############################

			self.W_LEADER = 128
			self.H_LEADER = 128

			self.W_NAME_PANEL = 220
			self.H_NAME_PANEL = 30

		else:
			self.X_LEFT_PANE = 25
			self.Y_LEFT_PANE = 70
			self.W_LEFT_PANE = 400
			self.H_LEFT_PANE = 620

			self.X_SCROLL = self.X_LEFT_PANE + 20
			self.Y_SCROLL= 90
			self.W_SCROLL= 360
			self.H_SCROLL= 580

			############################
			#### Total EPs Per Turn Text
			############################

			self.X_TOTAL_PANE = self.X_LEFT_PANE + self.W_LEFT_PANE + 20
			self.Y_TOTAL_PANE = self.Y_LEFT_PANE
			self.W_TOTAL_PANE = 550
			self.H_TOTAL_PANE = 60

			self.X_MAKING_TEXT = 490
			self.Y_MAKING_TEXT = 85

			############################
			#### Right Panel
			############################

			self.X_RIGHT_PANE = self.X_TOTAL_PANE
			self.Y_RIGHT_PANE = self.Y_TOTAL_PANE + self.H_TOTAL_PANE + 20
			self.W_RIGHT_PANE = self.W_TOTAL_PANE
			self.H_RIGHT_PANE = self.H_LEFT_PANE - self.H_TOTAL_PANE - 20

			self.X_CITY_LIST = self.X_RIGHT_PANE + 40
			self.Y_CITY_LIST = self.Y_RIGHT_PANE + 60
			self.W_CITY_LIST = 160
			self.H_CITY_LIST = self.H_RIGHT_PANE - 90

			self.X_EFFECTS_LIST = self.X_CITY_LIST + self.W_CITY_LIST + 20
			self.Y_EFFECTS_LIST = self.Y_CITY_LIST
			self.W_EFFECTS_LIST = 210
			self.H_EFFECTS_LIST = (self.H_CITY_LIST / 3) - 50

			self.X_EFFECTS_COSTS_LIST = self.X_EFFECTS_LIST + self.W_EFFECTS_LIST + 10
			self.Y_EFFECTS_COSTS_LIST = self.Y_EFFECTS_LIST
			self.W_EFFECTS_COSTS_LIST = 60
			self.H_EFFECTS_COSTS_LIST = self.H_EFFECTS_LIST

			self.X_MISSIONS_LIST = self.X_CITY_LIST + self.W_CITY_LIST + 20
			self.Y_MISSIONS_LIST = self.Y_EFFECTS_LIST + self.H_EFFECTS_LIST + 50
			self.W_MISSIONS_LIST = self.W_EFFECTS_LIST
			self.H_MISSIONS_LIST = (self.H_CITY_LIST * 2 / 3) #- 45

			self.X_MISSIONS_COSTS_LIST = self.X_MISSIONS_LIST + self.W_MISSIONS_LIST + 10
			self.Y_MISSIONS_COSTS_LIST = self.Y_MISSIONS_LIST
			self.W_MISSIONS_COSTS_LIST = self.W_EFFECTS_COSTS_LIST
			self.H_MISSIONS_COSTS_LIST = self.H_MISSIONS_LIST

			############################
			#### Left Leaders Panel
			############################

			self.W_LEADER = 128
			self.H_LEADER = 128

			self.W_NAME_PANEL = 220
			self.H_NAME_PANEL = 30

		return

	def drawMissionTab_LeftLeaderPanal(self, screen):
		pActivePlayer = gc.getPlayer(self.iActivePlayer)
		pActiveTeam = gc.getTeam(pActivePlayer.getTeam())

		# the following are needed for each leader
		self.MissionLeaderPanelWidgets = [""] * gc.getMAX_PLAYERS()  # updated by refresh
		self.EPWeightWidgets = [""] * gc.getMAX_PLAYERS()     # updated by refresh
		self.EPSpendingWidgets = [""] * gc.getMAX_PLAYERS()   # updated by refresh
		self.EPIconWidgets = [""] * gc.getMAX_PLAYERS()       # updated by refresh
		self.LeaderImageWidgets = [""] * gc.getMAX_PLAYERS()  # updated by handle input

		# The following only occur once
		self.CityListBoxWidget = self.getNextWidgetName()     # updated by refresh
		self.EffectsTableWidget = self.getNextWidgetName()    # updated by refresh
		self.MissionsTableWidget = self.getNextWidgetName()   # updated by refresh

		# only required for BUG
		if kHasBUG and EspionageOpt.isEnabled():
			iRatioColor = EspionageOpt.getDefaultRatioColor() if kHasBUG else self.iRatioColor
			iGoodRatioColor = EspionageOpt.getGoodRatioColor() if kHasBUG else self.iGoodRatioColor
			iBadRatioColor = EspionageOpt.getBadRatioColor() if kHasBUG else self.iBadRatioColor

			# show AI spending toggle
			self.ShowAISpendingWidget = self.getNextWidgetName()
			szText = u"<font=2>" + localText.getText("TXT_KEY_ESPIONAGE_AI_SPENDING_TOGGLE", ()) + "</font>"
			if self.bShowAISpending:
				szText = localText.changeTextColor(szText, gc.getInfoTypeForString("COLOR_YELLOW"))
			screen.setText(self.ShowAISpendingWidget, "Background", szText, CvUtil.FONT_RIGHT_JUSTIFY, self.X_LEFT_PANE + self.W_LEFT_PANE - 20, self.Y_LEFT_PANE + 8, self.Z_CONTROLS, FontTypes.GAME_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1)

		for iPlayerID in self.aiKnownPlayers:
			pTargetPlayer = gc.getPlayer(iPlayerID)
			iTargetTeam = pTargetPlayer.getTeam()

			# leader panel / container
			szLeaderPanel = self.getNextWidgetName()
			self.MissionLeaderPanelWidgets[iPlayerID] = szLeaderPanel

			screen.attachPanel(self.szScrollPanel, szLeaderPanel, "", "", True, False, PanelStyles.PANEL_STYLE_STANDARD)

			# EP Spending, Weight. EP Icon - all of these are handled by the 'refresh' procedure
			self.EPWeightWidgets[iPlayerID] = self.getNextWidgetName()
			self.EPSpendingWidgets[iPlayerID] = self.getNextWidgetName()
			self.EPIconWidgets[iPlayerID] = self.getNextWidgetName()

			# leader image
			szName = self.getNextWidgetName()
			screen.attachSeparator(szLeaderPanel, szName, true, 30)

			szName = self.getNextWidgetName()
			self.LeaderImageWidgets[iPlayerID] = szName  # updated by handle input so needs to be stored

			screen.addCheckBoxGFCAt(szLeaderPanel, szName, gc.getLeaderHeadInfo(gc.getPlayer(iPlayerID).getLeaderType()).getButton(), ArtFileMgr.getInterfaceArtInfo("BUTTON_HILITE_SQUARE").getPath(),
				self.MissionLeaderPanel_X_LeaderIcon, self.MissionLeaderPanelTopRow, 32, 32, WidgetTypes.WIDGET_GENERAL, self.iLeaderImagesID, iPlayerID, ButtonStyles.BUTTON_STYLE_LABEL, False)
			if (self.iTargetPlayer == iPlayerID):
				screen.setState(szName, true)

			# leader name
			szName = self.getNextWidgetName()
			screen.attachPanelAt( szLeaderPanel, szName, "", "", true, false, PanelStyles.PANEL_STYLE_MAIN,
				self.MissionLeaderPanel_X_LeaderNamePanel, self.MissionLeaderPanelTopRow, self.W_NAME_PANEL, self.H_NAME_PANEL, WidgetTypes.WIDGET_GENERAL, -1, -1 )

			szName = self.getNextWidgetName()

			if kHasBUG and EspionageOpt.isEnabled():
				szTempBuffer = u"<color=%d,%d,%d,%d>%s</color>" %(pTargetPlayer.getPlayerTextColorR(), pTargetPlayer.getPlayerTextColorG(), pTargetPlayer.getPlayerTextColorB(), pTargetPlayer.getPlayerTextColorA(), pTargetPlayer.getName())
			else:
				szMultiplier = self.getEspionageMultiplierText(self.iActivePlayer, iPlayerID)
				szTempBuffer = u"<color=%d,%d,%d,%d>%s (%s)</color>" %(pTargetPlayer.getPlayerTextColorR(), pTargetPlayer.getPlayerTextColorG(), pTargetPlayer.getPlayerTextColorB(), pTargetPlayer.getPlayerTextColorA(), pTargetPlayer.getName(), szMultiplier)

			szText = u"<font=2>" + szTempBuffer + "</font>"
			screen.setLabelAt( szName, szLeaderPanel, szText, 0, self.MissionLeaderPanel_X_LeaderName, self.MissionLeaderPanelTopRow, self.Z_CONTROLS, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1 );

			# EPoints Multiplier
			if kHasBUG and EspionageOpt.isEnabled():
				iMultiplier = self.getEspionageMultiplier(self.iActivePlayer, iPlayerID)
				szName = self.getNextWidgetName()
				szText = u"<font=2>%i%s</font>" %(iMultiplier, "%")

				if (iBadRatioColor >= 0 and iMultiplier >= (EspionageOpt.getBadRatioCutoff() if kHasBUG else 0)):
					szText = localText.changeTextColor(szText, iBadRatioColor)
				elif (iGoodRatioColor >= 0 and iMultiplier <= (EspionageOpt.getGoodRatioCutoff() if kHasBUG else 0)):
					szText = localText.changeTextColor(szText, iGoodRatioColor)
				elif (iRatioColor >= 0):
					szText = localText.changeTextColor(szText, iRatioColor)

				screen.setLabelAt( szName, szLeaderPanel, szText, CvUtil.FONT_RIGHT_JUSTIFY, self.MissionLeaderPanel_X_Multiplier, self.MissionLeaderPanelTopRow, self.Z_CONTROLS, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1 );

			# EPoints Multiplier Against
			if (EspionageOpt.isEnabled()
			and EspionageOpt.isShowCalculatedInformation()
			and self.bShowAISpending):
				iMultiplier = self.getEspionageMultiplier(iPlayerID, self.iActivePlayer)
				szName = self.getNextWidgetName()
				szText = u"<font=2>%i%s</font>" %(iMultiplier, "%")
				screen.setLabelAt( szName, szLeaderPanel, szText, CvUtil.FONT_RIGHT_JUSTIFY, self.MissionLeaderPanel_X_Multiplier, self.MissionLeaderPanelBottomRow, self.Z_CONTROLS, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1 );

			# Counter Espionage (both for and against)
			if kHasBUG and EspionageOpt.isEnabled():
				# for
				iCounterEsp = self.getCounterEspionageTurnsLeft(self.iActivePlayer, iPlayerID)
				self.showCounterEspionage(screen, szLeaderPanel, iCounterEsp, self.MissionLeaderPanelTopRow)

				# against
				if self.bShowAISpending:
					iCounterEsp = self.getCounterEspionageTurnsLeft(iPlayerID, self.iActivePlayer)
					self.showCounterEspionage(screen, szLeaderPanel, iCounterEsp, self.MissionLeaderPanelBottomRow)

			# EPs
			szName = self.getNextWidgetName()
			iPlayerEPs = self.getPlayerEPs(self.iActivePlayer, iPlayerID)
			szText = u"<font=2>" + localText.getText("TXT_KEY_ESPIONAGE_NUM_EPS", (iPlayerEPs ,)) + "</font>"
			screen.setLabelAt( szName, szLeaderPanel, szText, CvUtil.FONT_RIGHT_JUSTIFY, self.MissionLeaderPanel_X_EPoints, self.MissionLeaderPanelTopRow, self.Z_CONTROLS, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1 );

			# EPs Against
			if (EspionageOpt.isEnabled()
			and self.bShowAISpending):
				szName = self.getNextWidgetName()  #"PointsAgainstText%d" %(iPlayerID)
				iTargetEPs = self.getPlayerEPs(iPlayerID, self.iActivePlayer)
				szText = u"<font=2>" + localText.getText("TXT_KEY_ESPIONAGE_NUM_EPS", (iTargetEPs, )) + "</font>"
				screen.setLabelAt( szName, szLeaderPanel, szText, CvUtil.FONT_RIGHT_JUSTIFY, self.MissionLeaderPanel_X_EPoints, self.MissionLeaderPanelBottomRow, self.Z_CONTROLS, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1 );

			# EP Spending Against (Points per turn)
			if kHasBUG and EspionageOpt.isEnabled() and self.bShowAISpending:
				szName = self.getNextWidgetName()  #"AmountAgainstText%d" %(iPlayerID)
				iSpending = SpyUtil.getDifferenceByPlayer(iPlayerID, self.iActivePlayer)
				if (iSpending is None
				or iSpending == 0):
					szText = u""
				else:
					if iSpending > 0:
						szText = u"<font=2>(+%i)</font>" %(iSpending)
						szText = localText.changeTextColor(szText, gc.getInfoTypeForString("COLOR_GREEN"))
					else:
						szText = u"<font=2>(%i)</font>" %(iSpending)
						szText = localText.changeTextColor(szText, gc.getInfoTypeForString("COLOR_YELLOW"))
				screen.setLabelAt( szName, szLeaderPanel, szText, CvUtil.FONT_LEFT_JUSTIFY, self.MissionLeaderPanel_X_EPointsTurn, self.MissionLeaderPanelBottomRow, self.Z_CONTROLS, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1 );

			# EP Weights
			iSize = 16
			szName = self.getNextWidgetName()
			screen.setImageButtonAt( szName, szLeaderPanel, ArtFileMgr.getInterfaceArtInfo("INTERFACE_BUTTONS_PLUS").getPath(), self.MissionLeaderPanel_X_WghtInc, self.MissionLeaderPanelBottomRow, iSize, iSize, WidgetTypes.WIDGET_GENERAL, self.iIncreaseButtonID, iPlayerID );
			szName = self.getNextWidgetName()
			screen.setImageButtonAt( szName, szLeaderPanel, ArtFileMgr.getInterfaceArtInfo("INTERFACE_BUTTONS_MINUS").getPath(), self.MissionLeaderPanel_X_WghtDec, self.MissionLeaderPanelBottomRow, iSize, iSize, WidgetTypes.WIDGET_GENERAL, self.iDecreaseButtonID, iPlayerID );

			# Symbols for 'Demographics' and 'Research'
			if kHasBUG and EspionageOpt.isEnabled():
				# Active Player
				iDemoCost = pActivePlayer.getEspionageMissionCost(self.MissionSeeDemo, iPlayerID, None, -1)
				iTechCost = pActivePlayer.getEspionageMissionCost(self.MissionSeeResearch, iPlayerID, None, -1)
				self.showPassiveMissionIcons(screen, szLeaderPanel, iPlayerEPs, iDemoCost, iTechCost, self.MissionLeaderPanelTopRow)

				# Target Player
				if (EspionageOpt.isShowCalculatedInformation() and self.bShowAISpending):
					iDemoCost = pTargetPlayer.getEspionageMissionCost(self.MissionSeeDemo, self.iActivePlayer, None, -1)
					iTechCost = pTargetPlayer.getEspionageMissionCost(self.MissionSeeResearch, self.iActivePlayer, None, -1)
					self.showPassiveMissionIcons(screen, szLeaderPanel, iTargetEPs, iDemoCost, iTechCost, self.MissionLeaderPanelBottomRow)

		for iPlayerID in self.aiUnknownPlayers:
			szLeaderPanel = self.getNextWidgetName()
			szName = self.getNextWidgetName()
			screen.attachPanel(self.szScrollPanel, szLeaderPanel, "", "", True, False, PanelStyles.PANEL_STYLE_STANDARD)
			screen.attachSeparator(szLeaderPanel, szName, true, 30)




	def getEspionageMultiplier(self, iCurrentPlayer, iTargetPlayer):
		pCurrentPlayer = gc.getPlayer(iCurrentPlayer)
		iCurrentTeamID = pCurrentPlayer.getTeam()
		pTargetPlayer = gc.getPlayer(iTargetPlayer)
		iTargetTeamID = pTargetPlayer.getTeam()

		iMultiplier = getEspionageModifier(iCurrentTeamID, iTargetTeamID)
		return iMultiplier

	
	def getCounterEspionageTurnsLeft(self, iCurrentPlayer, iTargetPlayer):
		pCurrentTeam = gc.getTeam(gc.getPlayer(iCurrentPlayer).getTeam())
		iTargetTeamID = gc.getPlayer(iTargetPlayer).getTeam()

		iCurrentCounterEsp = pCurrentTeam.getCounterespionageTurnsLeftAgainstTeam(iTargetTeamID)
		return iCurrentCounterEsp

	def showCounterEspionage(self, screen, szLeaderPanel, iCounterEspTurns, iRow):
		szName = self.getNextWidgetName()
		if iCounterEspTurns > 0:
			szText = u"<font=2>[%i]</font>" %(iCounterEspTurns)
		else:
			szText = u""
		screen.setLabelAt(szName, szLeaderPanel, szText, CvUtil.FONT_RIGHT_JUSTIFY, self.MissionLeaderPanel_X_CounterEP, iRow, self.Z_CONTROLS, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1 )

	def showPassiveMissionIcons(self, screen, szLeaderPanel, iEPoints, iDemoCost, iTechCost, iRow):
		if kHasBUG:
			# can see demographics icon
			if iEPoints >= iDemoCost:
				szText = FontUtil.getChar("ss life support")
			else:
				szText = FontUtil.getChar("space")

			# can see research icon
			if iEPoints >= iTechCost:
				szText += FontUtil.getChar("commerce research")
			else:
				szText += FontUtil.getChar("space")

		szName = self.getNextWidgetName()
		screen.setLabelAt(szName, szLeaderPanel, szText, CvUtil.FONT_LEFT_JUSTIFY, self.MissionLeaderPanel_X_PassiveMissions, iRow, self.Z_CONTROLS, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1 );






	def refreshEspionagePanel(self, screen):
		screen.clearRichTextTableRows("FirstTable")
		screen.clearRichTextTableRows("PassiveTable")
		screen.clearRichTextTableRows("SecondTable")
		
		screen.setRichTextTableFlags("FirstTable", showHeader=False, enableMultiSelect=False, showColLines=False)
		screen.setRichTextTableFlags("PassiveTable", showHeader=False, enableSelection=False, showColLines=False)
		screen.setRichTextTableFlags("SecondTable", showHeader=False, enableSelection=False, showColLines=False)
		
	
		if self.iActivePlayer == -1 or self.iTargetPlayer == -1:
			return
			
		pActivePlayer = gc.getPlayer(self.iActivePlayer)
		pActiveTeam = gc.getTeam(pActivePlayer.getTeam())

		pTargetPlayer = gc.getPlayer(self.iTargetPlayer)
		pyTargetPlayer = PyPlayer(self.iTargetPlayer)
		
		self.tableIdList = []

		# FirstTable
		if self.CityMissionToggle == CITYMISSION_CITY:
			# Loop through target's cities, see which are visible and add them to the list
			index = 0
			for pyCity in pyTargetPlayer.getCityList():
				pCity = pyCity.GetCy()
				szCityName = self.getCityNameText(pCity, self.iActivePlayer, self.iTargetPlayer)

				if pCity.isRevealed(pActivePlayer.getTeam(), False):
					screen.addRichTextTableRow("FirstTable", [szCityName], [0])
					self.tableIdList.append(pCity.getID())
 
					if self.iActiveCityID == -1 or pTargetPlayer.getCity(self.iActiveCityID).isNone():
						self.iActiveCityID = pCity.getID()

					if self.iActiveCityID == pCity.getID():
						screen.setRichTextTableSelectedRows("FirstTable", [], index)
						
					index += 1
		else: # CITYMISSION_MISSION:
			# active missions only
			index = 0
			for iMissionLoop in range(gc.getNumEspionageMissionInfos()):
				pMission = gc.getEspionageMissionInfo(iMissionLoop)
				if pMission.getCost() != -1 and pMission.isTargetsCity():
					screen.addRichTextTableRow("FirstTable", [pMission.getDescription()], [0])
					self.tableIdList.append(iMissionLoop)

					if self.iActiveMissionID == -1:
						self.iActiveMissionID = iMissionLoop

					if self.iActiveMissionID == iMissionLoop:
						screen.setRichTextTableSelectedRows("FirstTable", [], index)

					index += 1

		
		# Loop through passive Missions
		for iMissionLoop in range(gc.getNumEspionageMissionInfos()):
			pMission = gc.getEspionageMissionInfo(iMissionLoop)
			if pMission.getCost() != -1 and pMission.isPassive():
				if self.CityMissionToggle == CITYMISSION_CITY or (self.CityMissionToggle == CITYMISSION_MISSION and not pMission.isTargetsCity()):
					szText, szCost = self.getTableTextCost(self.iActivePlayer, self.iTargetPlayer, iMissionLoop, self.iActiveCityID)
					screen.addRichTextTableRow("PassiveTable", [szText, szCost], [0, 0])

		if self.CityMissionToggle == CITYMISSION_CITY:
			# Loop through active Missions
			# Primary list is cities, secondary list is missions
			for iMissionLoop in range(gc.getNumEspionageMissionInfos()):
				pMission = gc.getEspionageMissionInfo(iMissionLoop)
				if pMission.getCost() != -1 and not pMission.isPassive():
					szText, szCost = self.getTableTextCost(self.iActivePlayer, self.iTargetPlayer, iMissionLoop, self.iActiveCityID)
					screen.addRichTextTableRow("SecondTable", [szText, szCost], [0, 0])
		else: # CITYMISSION_MISSION:
			# Loop through target's cities, see which are visible and add them to the list
			# Primary list is missions, secondary list is cities
			apCityList = pyTargetPlayer.getCityList()
			for pyCity in apCityList:
				pCity = pyCity.GetCy()
				if pCity.isRevealed(pActivePlayer.getTeam(), False):
					szText, szCost = self.getTableTextCost(self.iActivePlayer, self.iTargetPlayer, self.iActiveMissionID, pCity.getID())
					screen.addRichTextTableRow("SecondTable", [szText, szCost], [0, 0])

	def refreshMissionTab_LeftLeaderPanel(self, screen, pActivePlayer, iPlayerID):
		pTargetPlayer = gc.getPlayer(iPlayerID)
		iTargetTeam = pTargetPlayer.getTeam()

		szLeaderPanel = self.MissionLeaderPanelWidgets[iPlayerID]
		szEPWeight = self.EPWeightWidgets[iPlayerID]
		szEPSpending = self.EPSpendingWidgets[iPlayerID]
		szEPIcon = self.EPIconWidgets[iPlayerID]

		# EP Weight
		szText = u"<font=2>" + localText.getText("TXT_KEY_ESPIONAGE_SCREEN_SPENDING_WEIGHT", ()) + ": %d</font>" %(pActivePlayer.getEspionageSpendingWeightAgainstTeam(iTargetTeam))
		screen.setLabelAt(szEPWeight, szLeaderPanel, szText, 0, self.MissionLeaderPanel_X_Wght, self.MissionLeaderPanelBottomRow, self.Z_CONTROLS, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1 );

		# EP Spending (Points per turn)
		iSpending = pActivePlayer.getEspionageSpending(iTargetTeam)
		if kHasBUG and EspionageOpt.isEnabled():
			iY = self.MissionLeaderPanelTopRow
			if (iSpending > 0):
				szText = u"<font=2>(+%i)</font>" %(iSpending)
				szText = localText.changeTextColor(szText, gc.getInfoTypeForString("COLOR_GREEN"))
			else:
				szText = u""
		else:
			iY = self.MissionLeaderPanelBottomRow
			if (iSpending > 0):
				szText = u"<font=2><color=0,255,0,0>%s</color></font>" %(localText.getText("TXT_KEY_ESPIONAGE_NUM_EPS_PER_TURN", (iSpending, )))
			else:
				szText = u"<font=2><color=192,0,0,0>%s</color></font>" %(localText.getText("TXT_KEY_ESPIONAGE_NUM_EPS_PER_TURN", (iSpending, )))
		screen.setLabelAt(szEPSpending, szLeaderPanel, szText, CvUtil.FONT_LEFT_JUSTIFY, self.MissionLeaderPanel_X_EPointsTurn, iY, self.Z_CONTROLS, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1 );

		# Espionage Icon
		if (iSpending > 0):
			szText = u"<font=2>%c</font>" %(gc.getCommerceInfo(CommerceTypes.COMMERCE_ESPIONAGE).getChar())
		else:
			szText = u""
		screen.setLabelAt(szEPIcon, szLeaderPanel, szText, 0, self.MissionLeaderPanel_X_EspionageIcon, self.MissionLeaderPanelMiddle, self.Z_CONTROLS, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_GENERAL, -1, -1 );


	def getCityNameText(self, pCity, iActivePlayer, iTargetPlayer):
		if not kHasBUG or not EspionageOpt.isEnabled():
			return pCity.getName()

		szCityName = pCity.getName()
		iPlayerEPs = self.getPlayerEPs(iActivePlayer, iTargetPlayer)
		pActivePlayer = gc.getPlayer(iActivePlayer)
		pPlot = pCity.plot()

		if not pCity.isRevealed(pActivePlayer.getTeam(), false):
			szCityName = "-- %s --" %(szCityName)

		return szCityName

	def getPlayerEPs(self, iCurrentPlayer, iTargetPlayer):
		pCurrentPlayer = gc.getPlayer(iCurrentPlayer)
		pCurrentTeam = gc.getTeam(pCurrentPlayer.getTeam())
		pTargetPlayer = gc.getPlayer(iTargetPlayer)
		iTargetTeam = pTargetPlayer.getTeam()
		EPs = pCurrentTeam.getEspionagePointsAgainstTeam(iTargetTeam)
		return EPs

	def getTableTextCost(self, iActivePlayer, iTargetPlayer, iMission, iCity):

		pActivePlayer = gc.getPlayer(iActivePlayer)
		pActiveTeam = gc.getTeam(pActivePlayer.getTeam())
		pTargetPlayer = gc.getPlayer(iTargetPlayer)
		pMission = gc.getEspionageMissionInfo(iMission)

		szText = ""
		szCost = ""

		if pMission.getCost() == -1:
			return szText, szCost

		iTargetTeam = pTargetPlayer.getTeam()
		iPlayerEPs = self.getPlayerEPs(iActivePlayer, iTargetPlayer)
		if kHasBUG and EspionageOpt.isEnabled():
			iPossibleColor = EspionageOpt.getPossibleMissionColor()
			iCloseColor = EspionageOpt.getCloseMissionColor()
			iClosePercent = EspionageOpt.getCloseMissionPercent()
		else:
			iPossibleColor = -1
			iCloseColor = -1
			iClosePercent = -1

		pPlot = None
		szCityName= ""
		bHideCost = False
		if (iCity != -1
		and pMission.isTargetsCity()):
			pActiveCity = gc.getPlayer(iTargetPlayer).getCity(iCity)
			pPlot = pActiveCity.plot()
			szCityName = pActiveCity.getName()
			szCityName = self.getCityNameText(pActiveCity, iActivePlayer, iTargetPlayer)
			if not pActiveCity.isRevealed(pActivePlayer.getTeam(), false):
				bHideCost = True

		if not bHideCost:
			iCost = pActivePlayer.getEspionageMissionCost(iMission, iTargetPlayer, pPlot, -1)
		else:
			iCost = 0

		if (self.CityMissionToggle == CITYMISSION_CITY # secondary list is mission names
		or (pMission.isPassive()
		and not pMission.isTargetsCity())):
			szTechText = ""
			if (pMission.getTechPrereq() != -1):
				szTechText = " (%s)" %(gc.getTechInfo(pMission.getTechPrereq()).getDescription())

			szText = pMission.getDescription() + szTechText
		else: # secondary list is city names
			szText = szCityName

		if iCost > 0:
			szCost = unicode(str(iCost))
			if kHasBUG and EspionageOpt.isEnabled():
				if (iPossibleColor >= 0 and iPlayerEPs >= iCost):
					szCost = localText.changeTextColor(szCost, iPossibleColor)
					szText = localText.changeTextColor(szText, iPossibleColor)
				elif (iCloseColor >= 0 and iPlayerEPs >= (iCost * float(100 - iClosePercent) / 100)):
					szCost = localText.changeTextColor(szCost, iCloseColor)
					szText = localText.changeTextColor(szText, iCloseColor)

		if (pMission.getTechPrereq() != -1):
			pTeam = gc.getTeam(pActivePlayer.getTeam())
			if (not pTeam.isHasTech(pMission.getTechPrereq())):
				szText = u"<color=255,0,0,0>%s</color>" %(szText)
				return szText, szCost

		return szText, szCost




	def formatSpending(self, iSpending):
		if iSpending == 0:
			return u"<font=2>-</font>"
		elif iSpending > 0:
			return localText.changeTextColor(u"<font=2>+%i</font>" %(iSpending), gc.getInfoTypeForString("COLOR_GREEN"))
		else:
			return localText.changeTextColor(u"<font=2>%i</font>" %(iSpending), gc.getInfoTypeForString("COLOR_YELLOW"))

	# Will handle the input for this screen...
	def handleInput(self, inputClass):
		screen = self.getScreen()
		
		if inputClass.getNotifyCode() == NotifyCode.NOTIFY_LISTBOX_ITEM_SELECTED:
			if inputClass.iData1 == kPythonButtonFirstTable:
				rowI = screen.getRichTextTableActiveRowIndex("FirstTable")
				if self.CityMissionToggle == CITYMISSION_CITY:
					self.iActiveCityID = self.tableIdList[rowI]
				else:
					self.iActiveMissionID = self.tableIdList[rowI]
				self.refreshEspionagePanel(screen)
		elif inputClass.getNotifyCode() == NotifyCode.NOTIFY_CLICKED:
			if inputClass.iData1 == kPythonButtonCivButton:
				self.iTargetPlayer = inputClass.iData2
				self.iActiveCityID = -1
				self.updateCivButtons(screen)
				self.refreshEspionagePanel(screen)
			elif inputClass.iData1 == kPythonButtonGroupingFlip:
				self.CityMissionToggle = 1 - self.CityMissionToggle
				self.refreshEspionagePanel(screen)
			elif inputClass.iData1 in (kPythonButtonWeightInc, kPythonButtonWeightDec):
				iPlayerID = inputClass.iData2
				iTargetTeam = gc.getPlayer(iPlayerID).getTeam()
				delta = 1 if inputClass.iData1 == kPythonButtonWeightInc else -1
				if gc.getPlayer(self.iActivePlayer).getEspionageSpendingWeightAgainstTeam(iTargetTeam) + delta >= 0:
					CyMessageControl().sendEspionageSpendingWeightChange(iTargetTeam, delta)
				CyInterface().setDirty(InterfaceDirtyBits.Espionage_Advisor_DIRTY_BIT, True)
				self.updateWeights(screen)

		return 0

	def update(self, fDelta):
		if (CyInterface().isDirty(InterfaceDirtyBits.Espionage_Advisor_DIRTY_BIT) == True):
			CyInterface().setDirty(InterfaceDirtyBits.Espionage_Advisor_DIRTY_BIT, False)
			screen = self.getScreen()
			self.updateWeights(screen)
		return
