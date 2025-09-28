## Sid Meier's Civilization 4
## Copyright Firaxis Games 2005

# Thanks to "Ulf 'ulfn' Norell" from Apolyton for his additions relating to the graph section of this screen

# fortsnek: Adapted for Cv4MiniEngine

from CvPythonExtensions import *
import CvScreenEnums
import CvUtil
import ScreenInput

import string
import time


kHasBUG = False
try:
	import BugCore
	kHasBUG = True
except ImportError:
	pass
	
if kHasBUG:
	import BugUtil
	AdvisorOpt = BugCore.game.Advisors
else:
	class FakeBugUtil:
		def colorText(self, text, colourName):
			return localText.changeTextColor(text, gc.getInfoTypeForString(colourName))
		def getDisplayYear(self, year):
			if year < 0:
				return str(-year) + localText.getText("TXT_KEY_AUTOLOG_BC", ()) if kHasBUG else localText.getText("TXT_KEY_TIME_BC", (-year,))
			else:
				return str(year) + localText.getText("TXT_KEY_AUTOLOG_AD", ()) if kHasBUG else localText.getText("TXT_KEY_TIME_AD", (year,))
	BugUtil = FakeBugUtil()

from PyHelpers import PyPlayer

# globals
gc = CyGlobalContext()
localText = CyTranslator()

(kPage_Graph, kPage_Demographics, kPage_TopCities, kPage_Stats, kPage_Num) = range(5)

(
kGraph_Scores,
kGraph_GNP,
kGraph_Prod,
kGraph_Food,
kGraph_Power,
kGraph_Culture,
kGraph_Espionage,
kGraph_Num,
) = range(8)

(
kPythonButton_Tab,
kPythonButton_Legend,
kPythonButton_GraphType,
kPythonButton_GraphXRangeDropDown,
kPythonButton_GraphXRangeAdjust,
kPythonButton_WondersType,
) = range(6)

(
kWonderDisplayMode_World,
kWonderDisplayMode_National,
kWonderDisplayMode_Project,
) = range(3)

kMaxTopCities = 5

class CvInfoScreen:
	"Info Screen! Contains the Demographics, Wonders / Top Cities and Statistics Screens"

	def __init__(self, screenId):
		self.screenId = screenId
		self.DEMO_SCREEN_NAME = "DemographicsScreen"

		self.ANON_WIDGET_ID = "DemoScreenAnon_"
		self.nWidgetCount = 0

		
		self.graphEnd	    = CyGame().getGameTurn() - 1
		self.graphZoom	    = self.graphEnd - CyGame().getStartTurn()

		# This is used to allow the wonders screen to refresh without redrawing everything
		self.iNumWondersPermanentWidgets = 0

	
		self.iActiveTab = kPage_Graph


	def initText(self):

		###### TEXT ######
		self.SCREEN_TITLE = localText.getText("TXT_KEY_INFO_SCREEN", ()).upper()
		self.SCREEN_GRAPH_TITLE = u"<font=4b>" + localText.getText("TXT_KEY_INFO_GRAPH", ()).upper() + u"</font>"
		self.SCREEN_DEMOGRAPHICS_TITLE = u"<font=4b>" + localText.getText("TXT_KEY_DEMO_SCREEN_TITLE", ()).upper() + u"</font>"
		self.SCREEN_TOP_CITIES_TITLE = u"<font=4b>" + localText.getText("TXT_KEY_WONDERS_SCREEN_TOP_CITIES_TEXT", ()).upper() + u"</font>"
		self.SCREEN_STATS_TITLE = u"<font=4b>" + localText.getText("TXT_KEY_INFO_SCREEN_STATISTICS_TITLE", ()).upper() + u"</font>"

		self.EXIT_TEXT = u"<font=4>" + localText.getText("TXT_KEY_PEDIA_SCREEN_EXIT", ()).upper() + u"</font>"

		self.TEXT_GRAPH = u"<font=3>" + localText.getText("TXT_KEY_INFO_GRAPH", ()).upper() + u"</font>"
		self.TEXT_DEMOGRAPHICS = u"<font=3>" + localText.getText("TXT_KEY_DEMO_SCREEN_TITLE", ()).upper() + u"</font>"
		self.TEXT_DEMOGRAPHICS_SMALL = localText.getText("TXT_KEY_DEMO_SCREEN_TITLE", ())
		self.TEXT_TOP_CITIES = u"<font=3>" + localText.getText("TXT_KEY_WONDERS_SCREEN_TOP_CITIES_TEXT", ()).upper() + u"</font>"
		self.TEXT_STATS = u"<font=3>" + localText.getText("TXT_KEY_INFO_SCREEN_STATISTICS_TITLE", ()).upper() + u"</font>"
		self.TEXT_GRAPH_YELLOW = u"<font=3>" + localText.getColorText("TXT_KEY_INFO_GRAPH", (), gc.getInfoTypeForString("COLOR_YELLOW")).upper() + u"</font>"
		self.TEXT_DEMOGRAPHICS_YELLOW = u"<font=3>" + localText.getColorText("TXT_KEY_DEMO_SCREEN_TITLE", (), gc.getInfoTypeForString("COLOR_YELLOW")).upper() + u"</font>"
		self.TEXT_TOP_CITIES_YELLOW = u"<font=3>" + localText.getColorText("TXT_KEY_WONDERS_SCREEN_TOP_CITIES_TEXT", (), gc.getInfoTypeForString("COLOR_YELLOW")).upper() + u"</font>"
		self.TEXT_STATS_YELLOW = u"<font=3>" + localText.getColorText("TXT_KEY_INFO_SCREEN_STATISTICS_TITLE", (), gc.getInfoTypeForString("COLOR_YELLOW")).upper() + u"</font>"

		self.TEXT_SHOW_ALL_PLAYERS =  localText.getText("TXT_KEY_SHOW_ALL_PLAYERS", ())
		self.TEXT_SHOW_ALL_PLAYERS_GRAY = localText.getColorText("TXT_KEY_SHOW_ALL_PLAYERS", (), gc.getInfoTypeForString("COLOR_PLAYER_GRAY")).upper()
		
		self.TEXT_ENTIRE_HISTORY = localText.getText("TXT_KEY_INFO_ENTIRE_HISTORY", ())
		
		self.TEXT_SCORE = localText.getText("TXT_KEY_GAME_SCORE", ())
		self.TEXT_POWER = localText.getText("TXT_KEY_POWER", ())
		self.TEXT_CULTURE = localText.getObjectText("TXT_KEY_COMMERCE_CULTURE", 0)
		self.TEXT_ESPIONAGE = localText.getObjectText("TXT_KEY_ESPIONAGE_CULTURE", 0)

		self.TEXT_VALUE = localText.getText("TXT_KEY_DEMO_SCREEN_VALUE_TEXT", ())
		self.TEXT_RANK = localText.getText("TXT_KEY_DEMO_SCREEN_RANK_TEXT", ())
		self.TEXT_AVERAGE = localText.getText("TXT_KEY_DEMOGRAPHICS_SCREEN_RIVAL_AVERAGE", ())
		self.TEXT_BEST = localText.getText("TXT_KEY_INFO_RIVAL_BEST", ())
		self.TEXT_WORST = localText.getText("TXT_KEY_INFO_RIVAL_WORST", ())

		self.TEXT_ECONOMY = localText.getText("TXT_KEY_DEMO_SCREEN_ECONOMY_TEXT", ())
		self.TEXT_INDUSTRY = localText.getText("TXT_KEY_DEMO_SCREEN_INDUSTRY_TEXT", ())
		self.TEXT_AGRICULTURE = localText.getText("TXT_KEY_DEMO_SCREEN_AGRICULTURE_TEXT", ())
		self.TEXT_MILITARY = localText.getText("TXT_KEY_DEMO_SCREEN_MILITARY_TEXT", ())
		self.TEXT_LAND_AREA = localText.getText("TXT_KEY_DEMO_SCREEN_LAND_AREA_TEXT", ())
		self.TEXT_POPULATION = localText.getText("TXT_KEY_DEMO_SCREEN_POPULATION_TEXT", ())
		self.TEXT_HAPPINESS = localText.getText("TXT_KEY_DEMO_SCREEN_HAPPINESS_TEXT", ())
		self.TEXT_HEALTH = localText.getText("TXT_KEY_DEMO_SCREEN_HEALTH_TEXT", ())
		self.TEXT_IMP_EXP = localText.getText("TXT_KEY_DEMO_SCREEN_EXPORTS_TEXT", ()) + " - " + localText.getText("TXT_KEY_DEMO_SCREEN_IMPORTS_TEXT", ())

		self.TEXT_ECONOMY_MEASURE = (u"  %c" % CyGame().getSymbolID(FontSymbols.BULLET_CHAR)) + localText.getText("TXT_KEY_DEMO_SCREEN_ECONOMY_MEASURE", ())
		self.TEXT_INDUSTRY_MEASURE = (u"  %c" % CyGame().getSymbolID(FontSymbols.BULLET_CHAR)) + localText.getText("TXT_KEY_DEMO_SCREEN_INDUSTRY_MEASURE", ())
		self.TEXT_AGRICULTURE_MEASURE = (u"  %c" % CyGame().getSymbolID(FontSymbols.BULLET_CHAR)) + localText.getText("TXT_KEY_DEMO_SCREEN_AGRICULTURE_MEASURE", ())
		self.TEXT_MILITARY_MEASURE = ""
		self.TEXT_LAND_AREA_MEASURE = (u"  %c" % CyGame().getSymbolID(FontSymbols.BULLET_CHAR)) + localText.getText("TXT_KEY_DEMO_SCREEN_LAND_AREA_MEASURE", ())
		self.TEXT_POPULATION_MEASURE = ""
		self.TEXT_HAPPINESS_MEASURE = "%"
		self.TEXT_HEALTH_MEASURE = (u"  %c" % CyGame().getSymbolID(FontSymbols.BULLET_CHAR)) + localText.getText("TXT_KEY_DEMO_SCREEN_POPULATION_MEASURE", ())
		self.TEXT_IMP_EXP_MEASURE = (u"  %c" % CyGame().getSymbolID(FontSymbols.BULLET_CHAR)) + localText.getText("TXT_KEY_DEMO_SCREEN_ECONOMY_MEASURE", ())

		self.TEXT_TIME_PLAYED = localText.getText("TXT_KEY_INFO_SCREEN_TIME_PLAYED", ())
		self.TEXT_CITIES_BUILT = localText.getText("TXT_KEY_INFO_SCREEN_CITIES_BUILT", ())
		self.TEXT_CITIES_RAZED = localText.getText("TXT_KEY_INFO_SCREEN_CITIES_RAZED", ())
		self.TEXT_NUM_GOLDEN_AGES = localText.getText("TXT_KEY_INFO_SCREEN_NUM_GOLDEN_AGES", ())
		self.TEXT_NUM_RELIGIONS_FOUNDED = localText.getText("TXT_KEY_INFO_SCREEN_RELIGIONS_FOUNDED", ())

		self.TEXT_CURRENT = localText.getText("TXT_KEY_CURRENT", ())
		self.TEXT_UNITS = localText.getText("TXT_KEY_CONCEPT_UNITS", ())
		self.TEXT_BUILDINGS = localText.getText("TXT_KEY_CONCEPT_BUILDINGS", ())
		self.TEXT_KILLED = localText.getText("TXT_KEY_INFO_SCREEN_KILLED", ())
		self.TEXT_LOST = localText.getText("TXT_KEY_INFO_SCREEN_LOST", ())
		self.TEXT_BUILT = localText.getText("TXT_KEY_INFO_SCREEN_BUILT", ())

		self.TEXT_IMPROVEMENTS = localText.getText("TXT_KEY_CONCEPT_IMPROVEMENTS", ())

		self.graphTypeLabels = [
			self.TEXT_SCORE,
			self.TEXT_ECONOMY,
			self.TEXT_INDUSTRY,
			self.TEXT_AGRICULTURE,
			self.TEXT_POWER,
			self.TEXT_CULTURE,
			self.TEXT_ESPIONAGE,
		]


	def getScreen(self):
		return CyGInterfaceScreen(self.DEMO_SCREEN_NAME, self.screenId)

	def hideScreen(self):
		self.getScreen().hideScreen()

	def getLastTurn(self):
		return gc.getGame().getReplayMessageTurn(gc.getGame().getNumReplayMessages() - 1)

	# Screen construction function
	def showScreen(self, iTurn, iTabID, iEndGame):

		self.initText();

		self.iStartTurn = 0
		for iI in range(gc.getGameSpeedInfo(gc.getGame().getGameSpeedType()).getNumTurnIncrements()):
			self.iStartTurn += gc.getGameSpeedInfo(gc.getGame().getGameSpeedType()).getGameTurnInfo(iI).iNumGameTurnsPerIncrement
		self.iStartTurn *= gc.getEraInfo(gc.getGame().getStartEra()).getStartPercent()
		self.iStartTurn /= 100

		self.iTurn = 0
		
		iTurn = min(iTurn, self.getLastTurn())

		# Create a new screen
		screen = self.getScreen()
		if screen.isActive():
			return
		#screen.setRenderInterfaceOnly(True)
		screen.setInitialTitle(self.SCREEN_TITLE)
		screen.setAutoSizeBehaviour(HeckTuiAutoSizeBehaviour.Maximise)
		
		self.isPageBuilt = [False] * kPage_Num
		
		self.iActivePlayer = CyGame().getActivePlayer()
		self.pActivePlayer = gc.getPlayer(self.iActivePlayer)
		self.iActiveTeam = self.pActivePlayer.getTeam()
		self.pActiveTeam = gc.getTeam(self.iActiveTeam)
		
		iDemographicsMission = -1
		# See if Espionage allows graph to be shown for each player
		if not (gc.getGame().isOption(GameOptionTypes.GAMEOPTION_NO_ESPIONAGE)):
			for iMissionLoop in range(gc.getNumEspionageMissionInfos()):
				if (gc.getEspionageMissionInfo(iMissionLoop).isSeeDemographics()):
					iDemographicsMission = iMissionLoop
				
		# Determine who this active player knows
		self.aiPlayersMet = []
		self.iNumPlayersMet = 0
		for iLoopPlayer in range(gc.getMAX_CIV_PLAYERS()):
			pLoopPlayer = gc.getPlayer(iLoopPlayer)
			iLoopPlayerTeam = pLoopPlayer.getTeam()
			if (gc.getTeam(iLoopPlayerTeam).isEverAlive()):
				if (self.pActiveTeam.isHasMet(iLoopPlayerTeam) or CyGame().isDebugMode() or iEndGame != 0):
					if (	iDemographicsMission == -1 or
							self.pActivePlayer.canDoEspionageMission(iDemographicsMission, iLoopPlayer, None, -1) or
							iEndGame != 0 or
							iLoopPlayerTeam == CyGame().getActiveTeam()):
						self.aiPlayersMet.append(iLoopPlayer)
						self.iNumPlayersMet += 1

		
		# Reset variables
		self.graphMinStartTurn = CyGame().getStartTurn()
		self.graphMaxEndTurn = CyGame().getGameTurn() - 1
		self.graphStartTurn = 0
		self.graphInclLastTurn = self.graphMaxEndTurn
		self.graphPlayerVisibility = [True] * len(self.aiPlayersMet)
		self.graphTypeI = kGraph_Scores
		
		self.iActiveTab = -1

		self.wonderDisplayMode = kWonderDisplayMode_World
		
		root = ""
		
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # Content
			TableLayoutConfigCell(ivec2(0, 1), ivec2(1, 1), RectJustilign(EJustilign.Stretch, EJustilign.Stretch)) # Footer
		]
		table.gap = ivec2(0, 0)
		screen.setTableLayout(root, table)
		
		screen.newPanel(root, "Content")
		screen.newPanel(root, "Footer")
		
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 1)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), RectJustilign(EJustilign.Start, EJustilign.Stretch)), # FooterTabs
			TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), RectJustilign(EJustilign.End, EJustilign.Stretch)) # Exit
		]
		screen.setTableLayout("Footer", table)
		
		screen.newPanel("Footer", "FooterTabs")
		screen.setHFlowLayout("FooterTabs")

		for i in range(kPage_Num):
			screen.newActionButton("FooterTabs", "Tab" + str(i), WidgetTypes.WIDGET_PYTHON, kPythonButton_Tab, i)
			screen.newPanel("Content", "Page" + str(i))
			
		screen.newActionButton("Footer", "ExitButton", WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)
		screen.setActionButtonLabel("ExitButton", self.EXIT_TEXT)
		
		screen.setFillLayout("Content")

		self.switchPage(screen, min(max(iTabID, 0), kGraph_Num - 1))
		
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)
		
	def getPageRoot(self, pageI):
		return "Page" + str(pageI)
		
	def switchPage(self, screen, pageI):
		self.iActiveTab = pageI
	
		buildFuncs = [
			CvInfoScreen.buildGraphPage,
			CvInfoScreen.buildDemographicsPage,
			CvInfoScreen.buildCitiesWondersPage,
			CvInfoScreen.buildStatisticsPage,
		]
		if not self.isPageBuilt[pageI]:
			self.isPageBuilt[pageI] = True
			buildFuncs[pageI](self, screen, self.getPageRoot(pageI))
		for i in range(kPage_Num):
			screen.setVisible(self.getPageRoot(i), i == pageI)
		self.updateButtonOptionGroup(screen, "Tab", pageI, (self.SCREEN_GRAPH_TITLE, self.SCREEN_DEMOGRAPHICS_TITLE, self.SCREEN_TOP_CITIES_TITLE, self.SCREEN_STATS_TITLE))

	def updateButtonOptionGroup(self, screen, prefix, selectedI, labelKeys):
		for i, label in enumerate(labelKeys):
			text = localText.getText(labelKeys[i], ()).upper()
			if i == selectedI:
				text = BugUtil.colorText(text, "COLOR_YELLOW")
			screen.setActionButtonLabel(prefix + str(i), text)

	###
		
	def buildGraphPage(self, screen, root):
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # Graph
			TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), RectJustilign(EJustilign.Start, EJustilign.Start)) # Legend
		]
		table.gap = ivec2(0, 0)
		screen.setTableLayout(root, table)
		
		screen.newLineGraph(root, "Graph")
		
		self.buildGraph(screen)
		
		screen.newPanel(root, "Legend")
		screen.setVFlowLayout("Legend")
		
		screen.newPanel("Legend", "XRangePanel")
		screen.setHFlowLayout("XRangePanel")
		#screen.newCombobox("XRangePanel", "GraphTypeDropDown", , 0, WidgetTypes.WIDGET_PYTHON, kPythonButton_GraphTypeDropDown, -1)
		
		for i, label in enumerate(self.graphTypeLabels):
			screen.newActionButton("XRangePanel", "GraphType" + str(i), WidgetTypes.WIDGET_PYTHON, kPythonButton_GraphType, i)
		self.updateGraphTypeButtons(screen)
		
		maxTurnRange = self.graphMaxEndTurn - self.graphMinStartTurn + 1
		
		self.historyTurnRanges = [maxTurnRange] + [n for n in range(50, maxTurnRange, 50)]
		screen.newCombobox("XRangePanel", "HistoryDropDown", [self.TEXT_ENTIRE_HISTORY] + [unicode(n) for n in self.historyTurnRanges[1:]], 0, WidgetTypes.WIDGET_PYTHON, kPythonButton_GraphXRangeDropDown, -1)
		
		screen.newActionButton("XRangePanel", "XRangeLeft", WidgetTypes.WIDGET_PYTHON, kPythonButton_GraphXRangeAdjust, -1)
		screen.setActionButtonLabel("XRangeLeft", u" &lt; ")
		
		screen.newActionButton("XRangePanel", "XRangeRight", WidgetTypes.WIDGET_PYTHON, kPythonButton_GraphXRangeAdjust, +1)
		screen.setActionButtonLabel("XRangeRight", u" &gt; ")
		
		screen.newPanel("Legend", "LegendHFlow")
		screen.setHFlowLayout("LegendHFlow")
		screen.newPanel("LegendHFlow", "LegendVFlow")
		screen.setVFlowLayout("LegendVFlow")
		
		for i, iPlayer in enumerate(self.aiPlayersMet):
			ctrlName = "LegendPlayer" + str(i)
			screen.newActionButton("LegendVFlow", ctrlName, WidgetTypes.WIDGET_PYTHON, kPythonButton_Legend, i, HeckTuiBorderStyle.NONE)
			screen.setActionButtonHAlign(ctrlName, EAlign.Left)
			
		self.updateGraphLegendButtons(screen)
				
	def buildGraph(self, screen):
		firstTurn	= self.graphMinStartTurn
		lastTurn	= self.graphMaxEndTurn
		#lastTurn = self.getLastTurn()
		
		graphTypeI = self.graphTypeI
		screen.clearLineGraph("Graph")
		
		for iPlayer in reversed(self.aiPlayersMet):
			r = range(firstTurn, lastTurn + 1)
			if graphTypeI == kGraph_Scores:
				values = [gc.getPlayer(iPlayer).getScoreHistory(turn) for turn in r]
			elif graphTypeI == kGraph_GNP:
				values = [gc.getPlayer(iPlayer).getEconomyHistory(turn) for turn in r]
			elif graphTypeI == kGraph_Prod:
				values = [gc.getPlayer(iPlayer).getIndustryHistory(turn) for turn in r]
			elif graphTypeI == kGraph_Food:
				values = [gc.getPlayer(iPlayer).getAgricultureHistory(turn) for turn in r]
			elif graphTypeI == kGraph_Power:
				values = [gc.getPlayer(iPlayer).getPowerHistory(turn) for turn in r]
			elif graphTypeI == kGraph_Culture:
				values = [gc.getPlayer(iPlayer).getCultureHistory(turn) for turn in r]
			else: #if graphTypeI == kGraph_Espionage:
				values = [gc.getPlayer(iPlayer).getEspionageHistory(turn) for turn in r]
			screen.addLineGraphSeries("Graph", values, gc.getPlayerColorInfo(gc.getPlayer(iPlayer).getPlayerColor()).getColorTypePrimary())
			
		screen.setLineGraphXRange("Graph", self.graphStartTurn, self.graphInclLastTurn)
		screen.setLineGraphXLabels("Graph", [self.getTurnDate(self.graphStartTurn + (self.graphInclLastTurn - self.graphStartTurn) * i / 4) for i in range(5)])
		
	def updateGraphTypeButtons(self, screen):
		for i, label in enumerate(self.graphTypeLabels):
			if i == self.graphTypeI:
				label = BugUtil.colorText(label, "COLOR_YELLOW")
			screen.setActionButtonLabel("GraphType" + str(i), label)
		
	def updateGraphLegendButtons(self, screen):
		for i, iPlayer in enumerate(self.aiPlayersMet):
			ctrlName = "LegendPlayer" + str(i)
			name = gc.getPlayer(iPlayer).getName()
			if self.graphPlayerVisibility[i]:
				mark = u"\u25a0 "
				mark = localText.changeTextColor(mark, gc.getPlayerColorInfo(gc.getPlayer(iPlayer).getPlayerColor()).getColorTypePrimary())
				#name = localText.changeTextColor(name, gc.getPlayerColorInfo(gc.getPlayer(iPlayer).getPlayerColor()).getTextColorType())
			else:
				mark = u"  "
			screen.setActionButtonLabel(ctrlName, mark + name)
		
	def getTurnDate(self,turn):
	    year = CyGame().getTurnYear(turn)
	    if year < 0:
			return localText.getText("TXT_KEY_TIME_BC", (-year,))
	    else:
			return localText.getText("TXT_KEY_TIME_AD", (year,))
	
	###

	def buildDemographicsPage(self, screen, root):
		######## DATA ########

		iNumActivePlayers = 0

		pPlayer = gc.getPlayer(self.iActivePlayer)

		iEconomyGameAverage = 0
		iIndustryGameAverage = 0
		iAgricultureGameAverage = 0
		iMilitaryGameAverage = 0
		iLandAreaGameAverage = 0
		iPopulationGameAverage = 0
		iHappinessGameAverage = 0
		iHealthGameAverage = 0
		iNetTradeGameAverage = 0

		# Lists of Player values - will be used to determine rank, strength and average per city
		aiGroupEconomy = []
		aiGroupIndustry = []
		aiGroupAgriculture = []
		aiGroupMilitary = []
		aiGroupLandArea = []
		aiGroupPopulation = []
		aiGroupHappiness = []
		aiGroupHealth = []
		aiGroupNetTrade = []

		# Loop through all players to determine Rank and relative Strength
		for iPlayerLoop in range(gc.getMAX_PLAYERS()):

			if (gc.getPlayer(iPlayerLoop).isAlive() and not gc.getPlayer(iPlayerLoop).isBarbarian()):

				iNumActivePlayers += 1

				pCurrPlayer = gc.getPlayer(iPlayerLoop)
				
				iValue = pCurrPlayer.calculateTotalCommerce()
				if iPlayerLoop == self.iActivePlayer:
					iEconomy = iValue
				else:
					iEconomyGameAverage += iValue
				aiGroupEconomy.append((iValue, iPlayerLoop))
				
				iValue = pCurrPlayer.calculateTotalYield(YieldTypes.YIELD_PRODUCTION)
				if iPlayerLoop == self.iActivePlayer:
					iIndustry = iValue
				else:
					iIndustryGameAverage += iValue
				aiGroupIndustry.append((iValue, iPlayerLoop))

				iValue = pCurrPlayer.calculateTotalYield(YieldTypes.YIELD_FOOD)
				if iPlayerLoop == self.iActivePlayer:
					iAgriculture = iValue
				else:
					iAgricultureGameAverage += iValue
				aiGroupAgriculture.append((iValue, iPlayerLoop))

				iValue = pCurrPlayer.getPower() * 1000
				if iPlayerLoop == self.iActivePlayer:
					iMilitary = iValue
				else:
					iMilitaryGameAverage += iValue
				aiGroupMilitary.append((iValue, iPlayerLoop))

				iValue = pCurrPlayer.getTotalLand() * 1000
				if iPlayerLoop == self.iActivePlayer:
					iLandArea = iValue
				else:
					iLandAreaGameAverage += iValue
				aiGroupLandArea.append((iValue, iPlayerLoop))

				iValue = pCurrPlayer.getRealPopulation()
				if iPlayerLoop == self.iActivePlayer:
					iPopulation = iValue
				else:
					iPopulationGameAverage += iValue
				aiGroupPopulation.append((iValue, iPlayerLoop))

				iValue = self.getHappyValue(pCurrPlayer)
				if iPlayerLoop == self.iActivePlayer:
					iHappiness = iValue
				else:
					iHappinessGameAverage += iValue
				aiGroupHappiness.append((iValue, iPlayerLoop))

				iValue = self.getHealthValue(pCurrPlayer)
				if iPlayerLoop == self.iActivePlayer:
					iHealth = iValue
				else:
					iHealthGameAverage += iValue
				aiGroupHealth.append((iValue, iPlayerLoop))
					
				iValue = pCurrPlayer.calculateTotalExports(YieldTypes.YIELD_COMMERCE) - pCurrPlayer.calculateTotalImports(YieldTypes.YIELD_COMMERCE)
				if iPlayerLoop == self.iActivePlayer:
					iNetTrade = iValue
				else:
					iNetTradeGameAverage += iValue
				aiGroupNetTrade.append((iValue, iPlayerLoop))
					
		iEconomyRank = self.getRank(aiGroupEconomy)
		iIndustryRank = self.getRank(aiGroupIndustry)
		iAgricultureRank = self.getRank(aiGroupAgriculture)
		iMilitaryRank = self.getRank(aiGroupMilitary)
		iLandAreaRank = self.getRank(aiGroupLandArea)
		iPopulationRank = self.getRank(aiGroupPopulation)
		iHappinessRank = self.getRank(aiGroupHappiness)
		iHealthRank = self.getRank(aiGroupHealth)
		iNetTradeRank = self.getRank(aiGroupNetTrade)

		iEconomyGameBest	= self.getBest(aiGroupEconomy)
		iIndustryGameBest	= self.getBest(aiGroupIndustry)
		iAgricultureGameBest	= self.getBest(aiGroupAgriculture)
		iMilitaryGameBest	= self.getBest(aiGroupMilitary)
		iLandAreaGameBest	= self.getBest(aiGroupLandArea)
		iPopulationGameBest	= self.getBest(aiGroupPopulation)
		iHappinessGameBest	= self.getBest(aiGroupHappiness)
		iHealthGameBest		= self.getBest(aiGroupHealth)
		iNetTradeGameBest	= self.getBest(aiGroupNetTrade)

		iEconomyGameWorst	= self.getWorst(aiGroupEconomy)
		iIndustryGameWorst	= self.getWorst(aiGroupIndustry)
		iAgricultureGameWorst	= self.getWorst(aiGroupAgriculture)
		iMilitaryGameWorst	= self.getWorst(aiGroupMilitary)
		iLandAreaGameWorst	= self.getWorst(aiGroupLandArea)
		iPopulationGameWorst	= self.getWorst(aiGroupPopulation)
		iHappinessGameWorst	= self.getWorst(aiGroupHappiness)
		iHealthGameWorst	= self.getWorst(aiGroupHealth)
		iNetTradeGameWorst	= self.getWorst(aiGroupNetTrade)

		iEconomyGameAverage = iEconomyGameAverage / max(1, iNumActivePlayers - 1)
		iIndustryGameAverage = iIndustryGameAverage / max(1, iNumActivePlayers - 1)
		iAgricultureGameAverage = iAgricultureGameAverage / max(1, iNumActivePlayers - 1)
		iMilitaryGameAverage = iMilitaryGameAverage / max(1, iNumActivePlayers - 1)
		iLandAreaGameAverage = iLandAreaGameAverage / max(1, iNumActivePlayers - 1)
		iPopulationGameAverage = iPopulationGameAverage / max(1, iNumActivePlayers - 1)
		iHappinessGameAverage = iHappinessGameAverage / max(1, iNumActivePlayers - 1)
		iHealthGameAverage = iHealthGameAverage / max(1, iNumActivePlayers - 1)
		iNetTradeGameAverage = iNetTradeGameAverage / max(1, iNumActivePlayers - 1)


		######## TEXT ########


		# Create Table
		
		sHappiness = str(iHappiness) + self.TEXT_HAPPINESS_MEASURE
		sHappinessGameBest = str(iHappinessGameBest) + self.TEXT_HAPPINESS_MEASURE
		sHappinessGameAverage = str(iHappinessGameAverage) + self.TEXT_HAPPINESS_MEASURE
		sHappinessGameWorst = str(iHappinessGameWorst) + self.TEXT_HAPPINESS_MEASURE

		def numstr(x):
			return '{:,}'.format(x)

		rows = [
			[self.TEXT_DEMOGRAPHICS_SMALL, self.TEXT_VALUE, self.TEXT_BEST, self.TEXT_AVERAGE, self.TEXT_WORST, self.TEXT_RANK],
			[self.TEXT_ECONOMY + u"\n" + self.TEXT_ECONOMY_MEASURE, numstr(iEconomy)    , numstr(iEconomyGameBest)    , numstr(iEconomyGameAverage)    , numstr(iEconomyGameWorst)    , numstr(iEconomyRank)    ],                                                                              
			[self.TEXT_INDUSTRY + u"\n" + self.TEXT_INDUSTRY_MEASURE   , numstr(iIndustry)   , numstr(iIndustryGameBest)   , numstr(iIndustryGameAverage)   , numstr(iIndustryGameWorst)   , numstr(iIndustryRank)   ],                                                                               
			[self.TEXT_AGRICULTURE + u"\n" + self.TEXT_AGRICULTURE_MEASURE, numstr(iAgriculture), numstr(iAgricultureGameBest), numstr(iAgricultureGameAverage), numstr(iAgricultureGameWorst), numstr(iAgricultureRank)],                                                                               
			[self.TEXT_MILITARY   , numstr(iMilitary)   , numstr(iMilitaryGameBest)   , numstr(iMilitaryGameAverage)   , numstr(iMilitaryGameWorst)   , numstr(iMilitaryRank)   ],                                                                               
			[self.TEXT_LAND_AREA + u"\n" + self.TEXT_LAND_AREA_MEASURE  , numstr(iLandArea)   , numstr(iLandAreaGameBest)   , numstr(iLandAreaGameAverage)   , numstr(iLandAreaGameWorst)   , numstr(iLandAreaRank)   ],                                                                               
			[self.TEXT_POPULATION , numstr(iPopulation) , numstr(iPopulationGameBest) , numstr(iPopulationGameAverage) , numstr(iPopulationGameWorst) , numstr(iPopulationRank) ],                                                                               
			[self.TEXT_HAPPINESS  , sHappiness          , sHappinessGameBest          , sHappinessGameAverage          , sHappinessGameWorst          , numstr(iHappinessRank)  ],
			[self.TEXT_HEALTH + u"\n" + self.TEXT_HEALTH_MEASURE    , numstr(iHealth)     , numstr(iHealthGameBest)     , numstr(iHealthGameAverage)     , numstr(iHealthGameWorst)     , numstr(iHealthRank)     ],                                                                               
			[self.TEXT_IMP_EXP + u"\n" + self.TEXT_IMP_EXP_MEASURE    , numstr(iNetTrade)   , numstr(iNetTradeGameBest)   , numstr(iNetTradeGameAverage)   , numstr(iNetTradeGameWorst)   , numstr(iNetTradeRank)   ],                                                                               
		]

		numCols = len(rows[0])

		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 2)] + [TableLayoutConfigRowColDesc(0, 1)] * (numCols - 1)
		table.rows = [TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [TableLayoutConfigCell(ivec2(c, 0)) for c in range(numCols)]
		table.gap = ivec2(1, 1)
		screen.setTableLayout(root, table)
		
		for row in rows:
			for col in row:
				screen.newLabel(root, self.getNextWidgetName(), col)
			for i in range(len(row), numCols):
				screen.newLabel(root, self.getNextWidgetName(), u"")

	def getHappyValue(self, pPlayer):
		iHappy = pPlayer.calculateTotalCityHappiness()
		iUnhappy = pPlayer.calculateTotalCityUnhappiness()
		return (iHappy * 100) / max(1, iHappy + iUnhappy)	 

	def getHealthValue(self, pPlayer):
		iGood = pPlayer.calculateTotalCityHealthiness()
		iBad = pPlayer.calculateTotalCityUnhealthiness()
		return (iGood * 100) / max(1, iGood + iBad)	 
		
	def getRank(self, aiGroup):
		aiGroup.sort()
		aiGroup.reverse()		
		iRank = 1
		for (iLoopValue, iLoopPlayer) in aiGroup:
			if iLoopPlayer == self.iActivePlayer:
				return iRank
			iRank += 1
		return 0

	def getBest(self, aiGroup):
		bFirst = true
		iBest = 0
		for (iLoopValue, iLoopPlayer) in aiGroup:
			if iLoopPlayer != self.iActivePlayer:
				if bFirst or iLoopValue > iBest:
					iBest = iLoopValue
					bFirst = false
		return iBest

	def getWorst(self, aiGroup):
		bFirst = true
		iWorst = 0
		for (iLoopValue, iLoopPlayer) in aiGroup:
			if iLoopPlayer != self.iActivePlayer:
				if bFirst or iLoopValue < iWorst:
					iWorst = iLoopValue
					bFirst = false
		return iWorst

	###

	def buildCitiesWondersPage(self, screen, root):
		# City Members
		self.szCityNames = [u""] * kMaxTopCities
		self.iCitySizes = [-1] * kMaxTopCities
		self.szCityDescs = [u""] * kMaxTopCities
		self.iCityValues = [0] * kMaxTopCities
		self.pCityPointers = [0] * kMaxTopCities
		self.calculateTopCities()
		self.determineCityData()

		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)] * 2
		table.rows = [TableLayoutConfigRowColDesc(0, 1)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # TopCitiesPanel
			TableLayoutConfigCell(ivec2(1, 0)), # WondersPanel
		]
		table.gap = ivec2(1, 0)
		screen.setTableLayout(root, table)

		screen.newPanel(root, "TopCitiesPanel")
		screen.newPanel(root, "WondersPanel")
		
		screen.setVFlowLayout("TopCitiesPanel")

		for name, desc, pCity in zip(self.szCityNames, self.szCityDescs, self.pCityPointers):
			cityPanelName = self.getNextWidgetName()
			screen.newBoxPanel("TopCitiesPanel", self.getNextWidgetName(), cityPanelName, HeckTuiBorderStyle.Double)
			screen.setVFlowLayout(cityPanelName)
			screen.newLabel(cityPanelName, self.getNextWidgetName(), name)
			screen.newLabel(cityPanelName, self.getNextWidgetName(), desc)
			wonderPanelName = self.getNextWidgetName()
			#screen.newScrollSeekPanel(root, self.getNextWidgetName(), wonderPanelName, HeckTuiAxis.Horizontal)
			screen.newPanel(cityPanelName, wonderPanelName)
			screen.setVFlowLayout(wonderPanelName)
			for iBuildingLoop in range(gc.getNumBuildingInfos()):
				pBuilding = gc.getBuildingInfo(iBuildingLoop)
				# If this building is a wonder...
				if (isWorldWonderClass(gc.getBuildingInfo(iBuildingLoop).getBuildingClassType())):
					if pCity.getNumBuilding(iBuildingLoop) > 0:
						screen.newLabel(wonderPanelName, self.getNextWidgetName(), pBuilding.getDescription())

		screen.setVFlowLayout("WondersPanel")
		screen.newPanel("WondersPanel", "WondersButtonsPanel")
		screen.setHFlowLayout("WondersButtonsPanel")
		for i in range(3):
			screen.newActionButton("WondersButtonsPanel", "WonderTypeButton" + str(i), WidgetTypes.WIDGET_PYTHON, kPythonButton_WondersType, i)
		self.updateWonderTypeButtons(screen)

		screen.newRichTextTable("WondersPanel", "WondersTable", [
			localText.getText("TXT_KEY_WONDER_NAME" if kHasBUG else "TXT_KEY_DOMESTIC_ADVISOR_NAME", ()),
			localText.getText("TXT_KEY_WONDER_DATE" if kHasBUG else "TXT_KEY_HALL_OF_FAME_SORT_BY_DATE", ()),
			localText.getText("TXT_KEY_WONDER_OWNER" if kHasBUG else "TXT_KEY_PITBOSS_CIV", ()),
			localText.getText("TXT_KEY_WONDER_CITY" if kHasBUG else "TXT_KEY_CITY", ()),
		], WidgetTypes.WIDGET_GENERAL, -1, -1)
		screen.setRichTextTableExpandColumn("WondersTable", 0)

		self.updateWondersTable(screen)

	def updateWonderTypeButtons(self, screen):
		self.updateButtonOptionGroup(screen, "WonderTypeButton", self.wonderDisplayMode, ("TXT_KEY_TOP_CITIES_SCREEN_WORLD_WONDERS", "TXT_KEY_TOP_CITIES_SCREEN_NATIONAL_WONDERS", "TXT_KEY_PEDIA_CATEGORY_PROJECT"))

	def calculateTopCities(self):
		# Calculate the top 5 cities
		for iPlayerLoop in range(gc.getMAX_PLAYERS()):
			apCityList = PyPlayer(iPlayerLoop).getCityList()
			
			for pCity in apCityList:
				iTotalCityValue = ((pCity.getCulture() / 5) + (pCity.getFoodRate() + pCity.getProductionRate() \
					+ pCity.calculateGoldRate())) * pCity.getPopulation()

				for iRankLoop in range(kMaxTopCities):
					if (iTotalCityValue > self.iCityValues[iRankLoop] and not pCity.isBarbarian()):
						self.addCityToList(iRankLoop, pCity, iTotalCityValue)
						break

	# Recursive
	def addCityToList(self, iRank, pCity, iTotalCityValue):

		if (iRank >= kMaxTopCities):
			return

		pTempCity = self.pCityPointers[iRank]

		# Verify a city actually exists at this rank
		if (pTempCity):

			iTempCityValue = self.iCityValues[iRank]

			self.addCityToList(iRank+1, pTempCity, iTempCityValue)

			self.pCityPointers[iRank] = pCity
			self.iCityValues[iRank] = iTotalCityValue

		else:
			self.pCityPointers[iRank] = pCity
			self.iCityValues[iRank] = iTotalCityValue

	def determineCityData(self):

		self.iNumCities = 0

		for iRankLoop in range(kMaxTopCities):

			pCity = self.pCityPointers[iRankLoop]

			# If this city exists and has data we can use
			if (pCity):

				pPlayer = gc.getPlayer(pCity.getOwner())

				iTurnYear = CyGame().getTurnYear(pCity.getGameTurnFounded())

				if (iTurnYear < 0):
					szTurnFounded = localText.getText("TXT_KEY_TIME_BC", (-iTurnYear,))#"%d %s" %(-iTurnYear, self.TEXT_BC)
				else:
					szTurnFounded = localText.getText("TXT_KEY_TIME_AD", (iTurnYear,))#"%d %s" %(iTurnYear, self.TEXT_AD)

				if (pCity.isRevealed(gc.getGame().getActiveTeam()) or gc.getTeam(pPlayer.getTeam()).isHasMet(gc.getGame().getActiveTeam())):
					self.szCityNames[iRankLoop] = pCity.getName().upper()
					self.szCityDescs[iRankLoop] = ("%s, %s" %(pPlayer.getCivilizationAdjective(0), localText.getText("TXT_KEY_MISC_FOUNDED_IN", (szTurnFounded,))))
				else:
					self.szCityNames[iRankLoop] = localText.getText("TXT_KEY_UNKNOWN", ()).upper()
					self.szCityDescs[iRankLoop] = ("%s" %(localText.getText("TXT_KEY_MISC_FOUNDED_IN", (szTurnFounded,)), ))
				self.iCitySizes[iRankLoop] = pCity.getPopulation()
				#self.aaCitiesXY[iRankLoop] = [pCity.getX(), pCity.getY()]

				self.iNumCities += 1
			else:

				self.szCityNames[iRankLoop] = ""
				self.iCitySizes[iRankLoop] = -1
				self.szCityDescs[iRankLoop] = ""
				#self.aaCitiesXY[iRankLoop] = [-1, -1]

		return

	def updateWondersTable(self, screen):
		self.pActivePlayer = gc.getPlayer(CyGame().getActivePlayer())

		# (desc, year?, playerI?, city?)
		rows = []

		kInProductionYear = 9999
		kNoYear = 9998

		# Loop through players to determine Wonders
		for iPlayerLoop in range(gc.getMAX_PLAYERS()):

			pPlayer = gc.getPlayer(iPlayerLoop)
			iPlayerTeam = pPlayer.getTeam()

			# No barbs and only display national wonders for the active player's team
 			if (pPlayer and not pPlayer.isBarbarian() and ((self.wonderDisplayMode != kWonderDisplayMode_National) or (iPlayerTeam == gc.getTeam(gc.getPlayer(self.iActivePlayer).getTeam()).getID()))):

				# Loop through this player's cities and determine if they have any wonders to display
				apCityList = PyPlayer(iPlayerLoop).getCityList()
				for pCity in apCityList:

					pCityPlot = CyMap().plot(pCity.getX(), pCity.getY())
					
					# Check to see if active player can see this city
					szCityName = ""
					if (pCityPlot.isActiveVisible(False)):
						szCityName = pCity.getName()
					
					# Loop through projects to find any under construction
					if (self.wonderDisplayMode == kWonderDisplayMode_Project):
						for iProjectLoop in range(gc.getNumProjectInfos()):

							iProjectProd = pCity.getProductionProject()
							pProject = gc.getProjectInfo(iProjectLoop)

							# Project is being constructed
							if (iProjectProd == iProjectLoop):

								# Project Mode
								if (iPlayerTeam == gc.getTeam(gc.getPlayer(self.iActivePlayer).getTeam()).getID()):
									rows.append([kInProductionYear, pProject.getDescription(), iPlayerLoop, pCity])
									#self.aaWondersBeingBuilt.append([iProjectProd, pPlayer.getCivilizationShortDescription(0)])

					# Loop through buildings
					else:

						for iBuildingLoop in range(gc.getNumBuildingInfos()):

							iBuildingProd = pCity.getProductionBuilding()

							pBuilding = gc.getBuildingInfo(iBuildingLoop)

							# World Wonder Mode
							if (self.wonderDisplayMode == kWonderDisplayMode_World and isWorldWonderClass(gc.getBuildingInfo(iBuildingLoop).getBuildingClassType())):

								# Is this city building a wonder?
								if (iBuildingProd == iBuildingLoop):

									# Only show our wonders under construction
									if (iPlayerTeam == gc.getPlayer(self.iActivePlayer).getTeam()):

										#self.aaWondersBeingBuilt.append([iBuildingProd, pPlayer.getCivilizationShortDescription(0)])
										rows.append([kInProductionYear, pBuilding.getDescription(), iPlayerLoop, pCity])

								if (pCity.getNumBuilding(iBuildingLoop) > 0):
									if (iPlayerTeam == gc.getPlayer(self.iActivePlayer).getTeam() or gc.getTeam(gc.getPlayer(self.iActivePlayer).getTeam()).isHasMet(iPlayerTeam)):								
										#self.aaWondersBuilt.append([pCity.getBuildingOriginalTime(iBuildingLoop),iBuildingLoop,pPlayer.getCivilizationShortDescription(0),szCityName])
										rows.append([pCity.getBuildingOriginalTime(iBuildingLoop), pBuilding.getDescription(), iPlayerLoop, pCity])
									else:
										#self.aaWondersBuilt.append([pCity.getBuildingOriginalTime(iBuildingLoop),iBuildingLoop,localText.getText("TXT_KEY_UNKNOWN", ()),localText.getText("TXT_KEY_UNKNOWN", ())])
										rows.append([pCity.getBuildingOriginalTime(iBuildingLoop), pBuilding.getDescription(), None, None])
	#								print("Adding World wonder to list: %s, %d, %s" %(pCity.getBuildingOriginalTime(iBuildingLoop),iBuildingLoop,pPlayer.getCivilizationAdjective(0)))

							# National/Team Wonder Mode
							elif (self.wonderDisplayMode == kWonderDisplayMode_National and (isNationalWonderClass(gc.getBuildingInfo(iBuildingLoop).getBuildingClassType()) or isTeamWonderClass(gc.getBuildingInfo(iBuildingLoop).getBuildingClassType()))):

								# Is this city building a wonder?
								if (iBuildingProd == iBuildingLoop):

									# Only show our wonders under construction
									if (iPlayerTeam == gc.getPlayer(self.iActivePlayer).getTeam()):

										#self.aaWondersBeingBuilt.append([iBuildingProd, pPlayer.getCivilizationShortDescription(0)])
										rows.append([kInProductionYear, pBuilding.getDescription(), iPlayerLoop, pCity])

								if (pCity.getNumBuilding(iBuildingLoop) > 0):

	#								print("Adding National wonder to list: %s, %d, %s" %(pCity.getBuildingOriginalTime(iBuildingLoop),iBuildingLoop,pPlayer.getCivilizationAdjective(0)))
									if (iPlayerTeam == gc.getPlayer(self.iActivePlayer).getTeam() or gc.getTeam(gc.getPlayer(self.iActivePlayer).getTeam()).isHasMet(iPlayerTeam)):								
										#self.aaWondersBuilt.append([pCity.getBuildingOriginalTime(iBuildingLoop),iBuildingLoop,pPlayer.getCivilizationShortDescription(0), szCityName])
										rows.append([pCity.getBuildingOriginalTime(iBuildingLoop), pBuilding.getDescription(), iPlayerLoop, pCity])
									else:
										#self.aaWondersBuilt.append([pCity.getBuildingOriginalTime(iBuildingLoop),iBuildingLoop,localText.getText("TXT_KEY_UNKNOWN", ()), localText.getText("TXT_KEY_UNKNOWN", ())])
										rows.append([pCity.getBuildingOriginalTime(iBuildingLoop), pBuilding.getDescription(), None, None])

		# This array used to store which players have already used up a team's slot so team projects don't get added to list more than once
		aiTeamsUsed = []

		# Project Mode
		if (self.wonderDisplayMode == kWonderDisplayMode_Project):

			# Loop through players to determine Projects
			for iPlayerLoop in range(gc.getMAX_PLAYERS()):

				pPlayer = gc.getPlayer(iPlayerLoop)
				iTeamLoop = pPlayer.getTeam()

				# Block duplicates
				if (iTeamLoop not in aiTeamsUsed):

					aiTeamsUsed.append(iTeamLoop)
					pTeam = gc.getTeam(iTeamLoop)

					if (pTeam.isAlive() and not pTeam.isBarbarian()):

						# Loop through projects
						for iProjectLoop in range(gc.getNumProjectInfos()):
							pProject = gc.getProjectInfo(iProjectLoop)
							for iI in range(pTeam.getProjectCount(iProjectLoop)):
								if (iTeamLoop == self.iActiveTeam or self.pActiveTeam.isHasMet(iTeamLoop)):
									rows.append([kNoYear, pProject.getDescription(), iPlayerLoop, pCity])
									#rows.append([-9999,iProjectLoop,True,gc.getPlayer(iPlayerLoop).getCivilizationShortDescription(0),None, iPlayerLoop])
								else:
									rows.append([kNoYear, pProject.getDescription(), None, None])
									#rows.append([-9999,iProjectLoop,False,localText.getText("TXT_KEY_UNKNOWN", ()),None, 9999])

		screen.clearRichTextTableRows("WondersTable")

		rows.sort()

		for year, desc, playerI, city in reversed(rows):
			if year == kInProductionYear:
				yearStr = u"<font=2>%c</font>" % gc.getYieldInfo(YieldTypes.YIELD_PRODUCTION).getChar()
			elif year == kNoYear:
				yearStr = u""
			else:
				yearStr = BugUtil.getDisplayYear(year)

			if playerI is not None:
				ownerName = gc.getPlayer(playerI).getCivilizationShortDescription(0)
				color = -1
				ePlayerColor = gc.getPlayer(playerI).getPlayerColor()
				if ePlayerColor != -1:
					playerColor = gc.getPlayerColorInfo(ePlayerColor)
					if playerColor:
						color = playerColor.getColorTypePrimary()
				ownerName = localText.changeTextColor(ownerName, color)
				cityName = localText.changeTextColor(city.getName(), color)
			else:
				ownerName = localText.getText("TXT_KEY_UNKNOWN", ())
				cityName = u""
			
			screen.addRichTextTableRow("WondersTable", [desc, yearStr, ownerName, cityName], [0, year, 0, 0])

	###

	def buildStatisticsPage(self, screen, root):
		## fortsnek: Code adapted from BUG.

		# Bottom Chart

		iNumUnits = gc.getNumUnitInfos()
		iNumBuildings = gc.getNumBuildingInfos()
		iNumImprovements = gc.getNumImprovementInfos()

		self.iNumUnitStatsChartCols = 5
		self.iNumBuildingStatsChartCols = 2
		self.iNumImprovementStatsChartCols = 2

		self.iNumUnitStatsChartRows = iNumUnits
		self.iNumBuildingStatsChartRows = iNumBuildings
		self.iNumImprovementStatsChartRows = iNumImprovements

################################################### CALCULATE STATS ###################################################

		iMinutesPlayed = CyGame().getMinutesPlayed()
		iHoursPlayed = iMinutesPlayed / 60
		iMinutesPlayed = iMinutesPlayed - (iHoursPlayed * 60)

		szMinutesString = str(iMinutesPlayed)
		if (iMinutesPlayed < 10):
			szMinutesString = "0" + szMinutesString
		szHoursString = str(iHoursPlayed)
		if (iHoursPlayed < 10):
			szHoursString = "0" + szHoursString

		szTimeString = szHoursString + ":" + szMinutesString

		iNumCitiesBuilt = CyStatistics().getPlayerNumCitiesBuilt(self.iActivePlayer)

		iNumCitiesRazed = CyStatistics().getPlayerNumCitiesRazed(self.iActivePlayer)

		iNumReligionsFounded = 0
		for iReligionLoop in range(gc.getNumReligionInfos()):
			if (CyStatistics().getPlayerReligionFounded(self.iActivePlayer, iReligionLoop)):
				iNumReligionsFounded += 1

		aiUnitsBuilt = []
		for iUnitLoop in range(iNumUnits):
			aiUnitsBuilt.append(CyStatistics().getPlayerNumUnitsBuilt(self.iActivePlayer, iUnitLoop))

		aiUnitsKilled = []
		for iUnitLoop in range(iNumUnits):
			aiUnitsKilled.append(CyStatistics().getPlayerNumUnitsKilled(self.iActivePlayer, iUnitLoop))

		aiUnitsLost = []
		for iUnitLoop in range(iNumUnits):
			aiUnitsLost.append(CyStatistics().getPlayerNumUnitsLost(self.iActivePlayer, iUnitLoop))

		aiBuildingsBuilt = []
		for iBuildingLoop in range(iNumBuildings):
			aiBuildingsBuilt.append(CyStatistics().getPlayerNumBuildingsBuilt(self.iActivePlayer, iBuildingLoop))

		aiUnitsCurrent = []
		for iUnitLoop in range(iNumUnits):
			aiUnitsCurrent.append(0)

		apUnitList = PyPlayer(self.iActivePlayer).getUnitList()
		for pUnit in apUnitList:
			iType = pUnit.getUnitType()
			aiUnitsCurrent[iType] += 1

		aiImprovementsCurrent = []
		for iImprovementLoop in range(iNumImprovements):
			aiImprovementsCurrent.append(0)

		iGridW = CyMap().getGridWidth()
		iGridH = CyMap().getGridHeight()
		for iX in range(iGridW):
			for iY in range(iGridH):
				plot = CyMap().plot(iX, iY)
				if (plot.getOwner() == self.iActivePlayer):
					iType = plot.getImprovementType()
					if (iType != ImprovementTypes.NO_IMPROVEMENT):
						aiImprovementsCurrent[iType] += 1

################################################### TOP PANEL ###################################################

		screen.setVFlowLayout(root)
		screen.newPanel(root, "StatsTopPanel")
		screen.newPanel(root, "StatsBottomPanel")

		# Leaderhead panel
		screen.setVFlowLayout("StatsTopPanel")
		player = gc.getPlayer(gc.getGame().getActivePlayer())
		def centeredLabel(text):
				name = self.getNextWidgetName()
				screen.newLabel("StatsTopPanel", name, text)
				screen.setLabelHAlign(name, EAlign.Center)
		centeredLabel(u"<font=4b>" + player.getName() + u"</font>")
		centeredLabel(self.TEXT_TIME_PLAYED + u": " + szTimeString)
		centeredLabel(self.TEXT_CITIES_BUILT + u": " + str(iNumCitiesBuilt))
		centeredLabel(self.TEXT_CITIES_RAZED + u": " + str(iNumCitiesRazed))
		centeredLabel(self.TEXT_NUM_RELIGIONS_FOUNDED + u": " + str(iNumReligionsFounded))
		
################################################### BOTTOM PANEL ###################################################

		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 1)]
		table.cells = [TableLayoutConfigCell(ivec2(0, 0))]
		table.repeatAxis = HeckTuiAxis.Horizontal
		table.gap = ivec2(1, 0)
		screen.setTableLayout("StatsBottomPanel", table)

		screen.newRichTextTable("StatsBottomPanel", "StatsUnitsTable", [self.TEXT_UNITS, self.TEXT_CURRENT, self.TEXT_BUILT, self.TEXT_KILLED, self.TEXT_LOST], WidgetTypes.WIDGET_GENERAL, -1, -1)
		screen.newRichTextTable("StatsBottomPanel", "StatsBuildingsTable", [self.TEXT_BUILDINGS, self.TEXT_BUILT], WidgetTypes.WIDGET_GENERAL, -1, -1)
		if kHasBUG and AdvisorOpt.isShowImprovements():
			screen.newRichTextTable("StatsBottomPanel", "StatsImprovementsTable", [self.TEXT_IMPROVEMENTS, self.TEXT_CURRENT], WidgetTypes.WIDGET_GENERAL, -1, -1)

		def nzstr(s):
			return u"" if not s else unicode(s)

		# Add Units to table
		for iUnitLoop in range(iNumUnits):
			szUnitName = gc.getUnitInfo(iUnitLoop).getDescription()
			iNumUnitsCurrent = aiUnitsCurrent[iUnitLoop]
			iNumUnitsBuilt = aiUnitsBuilt[iUnitLoop]
			iNumUnitsKilled = aiUnitsKilled[iUnitLoop]
			iNumUnitsLost = aiUnitsLost[iUnitLoop]
			if iNumUnitsCurrent or iNumUnitsBuilt or iNumUnitsKilled or iNumUnitsLost:
				screen.addRichTextTableRow("StatsUnitsTable",
								  [szUnitName, nzstr(iNumUnitsCurrent), nzstr(iNumUnitsBuilt), nzstr(iNumUnitsKilled), nzstr(iNumUnitsLost)],
								  [0, iNumUnitsCurrent, iNumUnitsBuilt, iNumUnitsKilled, iNumUnitsLost]
								  )
				
		# Add Buildings to table
		for iBuildingLoop in range(iNumBuildings):
			szBuildingName = gc.getBuildingInfo(iBuildingLoop).getDescription()
			iNumBuildingsBuilt = aiBuildingsBuilt[iBuildingLoop]
			if iNumBuildingsBuilt:
				screen.addRichTextTableRow("StatsBuildingsTable",
								  [szBuildingName, str(iNumBuildingsBuilt)],
								  [0, iNumBuildingsBuilt]
								  )

#BUG: improvements - start
		if kHasBUG and AdvisorOpt.isShowImprovements():
			# Add Improvements to table	
			for iImprovementLoop in range(iNumImprovements):
				iNumImprovementsCurrent = aiImprovementsCurrent[iImprovementLoop]
				if (iNumImprovementsCurrent > 0):
					szImprovementName = gc.getImprovementInfo(iImprovementLoop).getDescription()
					screen.addRichTextTableRow("StatsImprovementsTable",
							  [szImprovementName, str(iNumImprovementsCurrent)],
							  [0, iNumImprovementsCurrent]
							  )
#BUG: improvements - end

	# returns a unique ID for a widget in this screen
	def getNextWidgetName(self):
		szName = self.ANON_WIDGET_ID + str(self.nWidgetCount)
		self.nWidgetCount += 1
		return szName

	# handle the input for this screen...
	def handleInput (self, inputClass):
		screen = self.getScreen()
		if inputClass.eNotifyCode == NotifyCode.NOTIFY_LISTBOX_ITEM_SELECTED:
			if inputClass.iData1 == kPythonButton_GraphXRangeDropDown:
				numTurns = self.historyTurnRanges[screen.getComboboxSelectedIndex("HistoryDropDown")]
				self.graphStartTurn = self.graphInclLastTurn - (numTurns - 1)
				if self.graphStartTurn < self.graphMinStartTurn:
					shift = self.graphMinStartTurn - self.graphStartTurn
					self.graphStartTurn += shift
					self.graphInclLastTurn += shift
				screen.setLineGraphXRange("Graph", self.graphStartTurn, self.graphInclLastTurn)
				screen.setLineGraphXLabels("Graph", [self.getTurnDate(self.graphStartTurn + (self.graphInclLastTurn - self.graphStartTurn) * i / 4) for i in range(5)])
		if inputClass.eNotifyCode == NotifyCode.NOTIFY_CLICKED:
			if inputClass.iData1 == kPythonButton_GraphType:
				self.graphTypeI = inputClass.iData2
				self.buildGraph(screen)
				self.updateGraphTypeButtons(screen)
			elif inputClass.iData1 == kPythonButton_Legend:
				i = inputClass.iData2
				self.graphPlayerVisibility[i] = not self.graphPlayerVisibility[i]
				screen.setLineGraphSeriesVisible("Graph", len(self.aiPlayersMet) - 1 - i, self.graphPlayerVisibility[i]) # series are in reverse order
				self.updateGraphLegendButtons(screen)
			elif inputClass.iData1 == kPythonButton_GraphXRangeAdjust:
				numTurns = self.historyTurnRanges[screen.getComboboxSelectedIndex("HistoryDropDown")]
				self.graphStartTurn += inputClass.iData2 * numTurns
				self.graphStartTurn = min(self.graphStartTurn, self.graphMaxEndTurn - (numTurns - 1))
				self.graphStartTurn = max(self.graphStartTurn, self.graphMinStartTurn)
				self.graphInclLastTurn = self.graphStartTurn + numTurns - 1
				screen.setLineGraphXRange("Graph", self.graphStartTurn, self.graphInclLastTurn)
				screen.setLineGraphXLabels("Graph", [self.getTurnDate(self.graphStartTurn + (self.graphInclLastTurn - self.graphStartTurn) * i / 4) for i in range(5)])
			elif inputClass.iData1 == kPythonButton_Tab:
				self.switchPage(screen, inputClass.iData2)
			elif inputClass.iData1 == kPythonButton_WondersType:
				self.wonderDisplayMode = inputClass.iData2
				self.updateWonderTypeButtons(screen)
				self.updateWondersTable(screen)
		return 0

	def update(self, fDelta):
		return


