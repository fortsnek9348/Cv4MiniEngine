## Sid Meier's Civilization 4
## Copyright Firaxis Games 2005

## fortsnek: Adapted for Cv4MiniEngine.

from CvPythonExtensions import *
import CvUtil
import ScreenInput
import PyHelpers
import time

# BUG - start
kHasBUG = False
try:
	import BugCore
	kHasBUG = True
except ImportError:
	pass
	
if kHasBUG:
	import AttitudeUtil
	import BugPath
	import BugUtil
	import ColorUtil
	import GameUtil
	import PlayerUtil
	import TechUtil
	AdvisorOpt = BugCore.game.Advisors
# BUG - end
else:
	class FakeColorUtil:
		def keyToType(self, key):
			return gc.getInfoTypeForString(colourName)
	class FakeBugUtil:
		def debug(self, *args):
			pass
		def getText(self, key, *args):
			return localText.getText(key, args)
		def colorText(self, text, colourName):
			return localText.changeTextColor(text, gc.getInfoTypeForString(colourName))
	class FakeGameUtil:
		def getCultureThreshold(self, level):
			return gc.getGame().getCultureThreshold(level)
	BugUtil = FakeBugUtil()
	ColorUtil = FakeColorUtil()
	GameUtil = FakeGameUtil()

if kHasBUG:
# BUFFY - start
	import os
	import GameSetUpCheck
	import Buffy

	BUFFYOpt = BugCore.game.BUFFY

	worldWrapString = {
		'Flat': "TXT_KEY_MAP_WRAP_FLAT",
		'Cylindrical': "TXT_KEY_MAP_WRAP_CYLINDER",
		'Toroidal': "TXT_KEY_MAP_WRAP_TOROID"
	}
# BUFFY - end

# BUG - Mac Support - start
if kHasBUG:
	BugUtil.fixSets(globals())
# BUG - Mac Support - end

PyPlayer = PyHelpers.PyPlayer

# globals
gc = CyGlobalContext()
localText = CyTranslator()

VICTORY_CONDITION_SCREEN = 0
GAME_SETTINGS_SCREEN = 1
UN_RESOLUTION_SCREEN = 2
UN_MEMBERS_SCREEN = 3

kPythonTabButton = 0
kPythonSpaceshipScreenButton = 1
kPythonVoteSourceButton = 2
kPythonVoteTypeButton = 3

class CvVictoryScreen:
	"Keeps track of victory conditions"

	def __init__(self, screenId):
		self.screenId = screenId
		self.SCREEN_NAME = "VictoryScreen"

# BUG Additions Start
		self.VoteType = 1  # 1 = Pope or GenSec, 2 = Diplomatic Victory
		self.VoteBody = 1  # 1 = AP, 2 = UN
# BUG Additions End

		## Start HOF MOD V1.61.001  2/8
		self.BuffyWarningTextLoaded = False
		## End HOF MOD V1.61.001  2/8

		self.nWidgetCount = 0
		self.iActivePlayer = -1
		self.bVoteTab = False

		self.iScreen = -1

		self.ApolloTeamsChecked = set()
		self.ApolloTeamCheckResult = {}

	def getScreen(self):
		return CyGInterfaceScreen(self.SCREEN_NAME, self.screenId)

	def hideScreen(self):
		screen = self.getScreen()
		screen.hideScreen()

	def interfaceScreen(self):

		# Create a new screen
		screen = self.getScreen()
		if screen.isActive():
			return
		#screen.setRenderInterfaceOnly(True);
		screen.setInitialTitle(localText.getText("TXT_KEY_VICTORY_SCREEN_TITLE", ()).upper())
		
		self.iActivePlayer = CyGame().getActivePlayer()
		if self.iScreen < 0:
			self.iScreen = VICTORY_CONDITION_SCREEN
			
		root = ""
	
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # Content
			TableLayoutConfigCell(ivec2(0, 1), ivec2(1, 1), RectJustilign(EJustilign.Stretch, EJustilign.Stretch)) # Footer
		]
		table.gap = ivec2(0, 1)
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

		for i in range(4):
			screen.newActionButton("FooterTabs", "Tab" + str(i), WidgetTypes.WIDGET_PYTHON, kPythonTabButton, i)
			screen.newPanel("Content", "Page" + str(i))
			
		screen.newActionButton("Footer", "ExitButton", WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)
		screen.setActionButtonLabel("ExitButton", localText.getText("TXT_KEY_PEDIA_SCREEN_EXIT", ()).upper())
		
		screen.setFillLayout("Content")

		self.buildVictoryConditionScreen(screen, "Page" + str(VICTORY_CONDITION_SCREEN))
		self.buildGameSettingsScreen(screen, "Page" + str(GAME_SETTINGS_SCREEN))
		self.buildVotingScreen(screen, "Page" + str(UN_RESOLUTION_SCREEN))
		self.rebuildMembersScreen(screen, "Page" + str(UN_MEMBERS_SCREEN))

		self.switchPage(screen)
			
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)
		
	def switchPage(self, screen):
		for i in range(4):
			screen.hide("Page" + str(i))
		screen.show("Page" + str(self.iScreen))
		self.updatePageButtons(screen)
		
	def updatePageButtons(self, screen):
		for i in range(4):
			text = localText.getText(("TXT_KEY_MAIN_MENU_VICTORIES", "TXT_KEY_MAIN_MENU_SETTINGS", "TXT_KEY_VOTING_TITLE", "TXT_KEY_MEMBERS_TITLE")[i], ()).upper()
			if i == self.iScreen:
				text = BugUtil.colorText(text, "COLOR_YELLOW")
			screen.setActionButtonLabel("Tab" + str(i), text)

	def buildVictoryConditionScreen(self, screen, root):
		self.ANON_WIDGET_ID = root + "_Anon"
		
		activePlayer = PyHelpers.PyPlayer(self.iActivePlayer)
		iActiveTeam = gc.getPlayer(self.iActivePlayer).getTeam()

		# checking if apollo has been built - clear arrays / lists / whatever they are called
		self.ApolloTeamsChecked = set()
		self.ApolloTeamCheckResult = {}

		# Conquest
		nRivals = -1
# BUG Additions Start
		nknown = 0
		nVassaled = 0
# BUG Additions End
		for i in range(gc.getMAX_CIV_TEAMS()):
			if (gc.getTeam(i).isAlive() and not gc.getTeam(i).isMinorCiv() and not gc.getTeam(i).isBarbarian()):
				nRivals += 1
# BUG Additions Start
				if i != iActiveTeam:
					if gc.getTeam(i).isHasMet(iActiveTeam):
						nknown += 1
					if gc.getTeam(i).isVassal(iActiveTeam):
						nVassaled += 1
# BUG Additions End

		# Population
		totalPop = gc.getGame().getTotalPopulation()
		ourPop = activePlayer.getTeam().getTotalPopulation()
		if (totalPop > 0):
			popPercent = (ourPop * 100.0) / totalPop
		else:
			popPercent = 0.0

		iBestPopTeam = -1
		bestPop = 0
		for iLoopTeam in range(gc.getMAX_CIV_TEAMS()):
			if (gc.getTeam(iLoopTeam).isAlive() and not gc.getTeam(iLoopTeam).isMinorCiv() and not gc.getTeam(iLoopTeam).isBarbarian()):
				if (iLoopTeam != iActiveTeam and (activePlayer.getTeam().isHasMet(iLoopTeam) or gc.getGame().isDebugMode())):
					teamPop = gc.getTeam(iLoopTeam).getTotalPopulation()
					if (teamPop > bestPop):
						bestPop = teamPop
						iBestPopTeam = iLoopTeam

		# Score
		ourScore = gc.getGame().getTeamScore(iActiveTeam)

		iBestScoreTeam = -1
		bestScore = 0
		for iLoopTeam in range(gc.getMAX_CIV_TEAMS()):
			if (gc.getTeam(iLoopTeam).isAlive() and not gc.getTeam(iLoopTeam).isMinorCiv() and not gc.getTeam(iLoopTeam).isBarbarian()):
				if (iLoopTeam != iActiveTeam and (activePlayer.getTeam().isHasMet(iLoopTeam) or gc.getGame().isDebugMode())):
					teamScore = gc.getGame().getTeamScore(iLoopTeam)
					if (teamScore > bestScore):
						bestScore = teamScore
						iBestScoreTeam = iLoopTeam

		# Land Area
		totalLand = gc.getMap().getLandPlots()
		ourLand = activePlayer.getTeam().getTotalLand()
		if (totalLand > 0):
			landPercent = (ourLand * 100.0) / totalLand
		else:
			landPercent = 0.0

		iBestLandTeam = -1
		bestLand = 0
		for iLoopTeam in range(gc.getMAX_CIV_TEAMS()):
			if (gc.getTeam(iLoopTeam).isAlive() and not gc.getTeam(iLoopTeam).isMinorCiv() and not gc.getTeam(iLoopTeam).isBarbarian()):
				if (iLoopTeam != iActiveTeam and (activePlayer.getTeam().isHasMet(iLoopTeam) or gc.getGame().isDebugMode())):
					teamLand = gc.getTeam(iLoopTeam).getTotalLand()
					if (teamLand > bestLand):
						bestLand = teamLand
						iBestLandTeam = iLoopTeam

		# Religion
		iOurReligion = -1
		ourReligionPercent = 0
		for iLoopReligion in range(gc.getNumReligionInfos()):
			if (activePlayer.getTeam().hasHolyCity(iLoopReligion)):
				religionPercent = gc.getGame().calculateReligionPercent(iLoopReligion)
				if (religionPercent > ourReligionPercent):
					ourReligionPercent = religionPercent
					iOurReligion = iLoopReligion

		iBestReligion = -1
		bestReligionPercent = 0
		for iLoopReligion in range(gc.getNumReligionInfos()):
			if (iLoopReligion != iOurReligion):
				religionPercent = gc.getGame().calculateReligionPercent(iLoopReligion)
				if (religionPercent > bestReligionPercent):
					bestReligionPercent = religionPercent
					iBestReligion = iLoopReligion

		# Total Culture
		ourCulture = activePlayer.getTeam().countTotalCulture()

		iBestCultureTeam = -1
		bestCulture = 0
		for iLoopTeam in range(gc.getMAX_CIV_TEAMS()):
			if (gc.getTeam(iLoopTeam).isAlive() and not gc.getTeam(iLoopTeam).isMinorCiv() and not gc.getTeam(iLoopTeam).isBarbarian()):
				if (iLoopTeam != iActiveTeam and (activePlayer.getTeam().isHasMet(iLoopTeam) or gc.getGame().isDebugMode())):
					teamCulture = gc.getTeam(iLoopTeam).countTotalCulture()
					if (teamCulture > bestCulture):
						bestCulture = teamCulture
						iBestCultureTeam = iLoopTeam

		# Vote
		aiVoteBuildingClass = []
		for i in range(gc.getNumBuildingInfos()):
			for j in range(gc.getNumVoteSourceInfos()):
				if (gc.getBuildingInfo(i).getVoteSourceType() == j):
					iUNTeam = -1
					bUnknown = true 
					for iLoopTeam in range(gc.getMAX_CIV_TEAMS()):
						if (gc.getTeam(iLoopTeam).isAlive() and not gc.getTeam(iLoopTeam).isMinorCiv() and not gc.getTeam(iLoopTeam).isBarbarian()):
							if (gc.getTeam(iLoopTeam).getBuildingClassCount(gc.getBuildingInfo(i).getBuildingClassType()) > 0):
								iUNTeam = iLoopTeam
								if (iLoopTeam == iActiveTeam or gc.getGame().isDebugMode() or activePlayer.getTeam().isHasMet(iLoopTeam)):
									bUnknown = False
								break

					aiVoteBuildingClass.append((gc.getBuildingInfo(i).getBuildingClassType(), iUNTeam, bUnknown))

		self.bVoteTab = (len(aiVoteBuildingClass) > 0)

		#self.deleteAllWidgets()	
		#screen = self.getScreen()

		# Start filling in the table below
		
		numColumns = 5
		numNonHeaderColumns = numColumns - 1
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)] + [TableLayoutConfigRowColDesc(0, 0)] * numNonHeaderColumns
		table.rows = [TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # Row header
			TableLayoutConfigCell(ivec2(1, 0)), # Active player name
			TableLayoutConfigCell(ivec2(2, 0)), # Active player stats
			TableLayoutConfigCell(ivec2(3, 0)), # Rival player name
			TableLayoutConfigCell(ivec2(4, 0)), # Rival player stats
		]
		table.gap = ivec2(1, 0)
		screen.setTableLayout(root, table)
		
		def appendRow(items):
			for item in items:
				screen.newLabel(root, self.getNextWidgetName(), item)
			for i in range(len(items), numColumns):
				screen.newPanel(root, self.getNextWidgetName())
				
		def appendButtonRow(header, btnLabel, btnWidgetType, btnData1, items):
			name = self.getNextWidgetName()
			screen.newPanel(root, name)
			screen.setHFlowLayout(name, EJustilign.Center)
			screen.newLabel(name, self.getNextWidgetName(), header)
			btnName = self.getNextWidgetName()
			screen.newActionButton(name, btnName, btnWidgetType, btnData1, -1)
			screen.setActionButtonLabel(btnName, btnLabel)
		
			for item in items:
				screen.newLabel(root, self.getNextWidgetName(), item)
			for i in range(len(items), numColumns - 1):
				screen.newPanel(root, self.getNextWidgetName())
				
		def findSpaceship(iLoopVC):
			# Using BUG logic.
			if not self.isApolloBuilt():
				return False
			for i in range(gc.getNumProjectInfos()):
				if (gc.getProjectInfo(i).getVictoryThreshold(iLoopVC) > 0):
					return self.isApolloBuiltbyTeam(activePlayer.getTeam())
			return False

		for iLoopVC in range(gc.getNumVictoryInfos()):
			victory = gc.getVictoryInfo(iLoopVC)
			if gc.getGame().isVictoryValid(iLoopVC):

				szVictoryType = u"<font=4b>" + victory.getDescription().upper() + u"</font>"
				if (victory.isEndScore() and (gc.getGame().getMaxTurns() > gc.getGame().getElapsedGameTurns())):
					szVictoryType += "    (" + localText.getText("TXT_KEY_MISC_TURNS_LEFT", (gc.getGame().getMaxTurns() - gc.getGame().getElapsedGameTurns(), )) + ")"

				bSpaceshipFound = findSpaceship(iLoopVC)
				
				isHeaderCommitted = [False]
				
				def commitHeader():
					if isHeaderCommitted[0]:
						return
					isHeaderCommitted[0] = True
					# Spaceship button goeson the header row.
					if bSpaceshipFound:
						victoryDelay = gc.getTeam(iActiveTeam).getVictoryCountdown(iLoopVC)
						showArrivalTime = victoryDelay > 0 and gc.getGame().getGameState() != GameStateTypes.GAMESTATE_EXTENDED
						appendButtonRow(szVictoryType, localText.getText("TXT_KEY_GLOBELAYER_STRATEGY_VIEW", ()), WidgetTypes.WIDGET_PYTHON, kPythonSpaceshipScreenButton, [
							localText.getText("TXT_KEY_SPACE_SHIP_SCREEN_ARRIVAL", ()) + ":",
							CyGameTextMgr().getTimeStr(gc.getGame().getGameTurn() + victoryDelay, False),
							localText.getText("TXT_KEY_REPLAY_SCREEN_TURNS", ()) + ":",
							unicode(victoryDelay),
						] if showArrivalTime else [])
					else:
						appendRow([szVictoryType])

				if (victory.isTargetScore() and gc.getGame().getTargetScore() != 0):
					commitHeader()
					appendRow([
						localText.getText("TXT_KEY_VICTORY_SCREEN_TARGET_SCORE", (gc.getGame().getTargetScore(),)),
						activePlayer.getTeam().getName() + ":",
						u"%d" % ourScore,
					] + ([
						gc.getTeam(iBestScoreTeam).getName() + ":",
						u"%d" % bestScore
					] if iBestScoreTeam != -1 else []))

				if (victory.isEndScore()):
					commitHeader()
					appendRow([
						localText.getText("TXT_KEY_VICTORY_SCREEN_HIGHEST_SCORE", (CyGameTextMgr().getTimeStr(gc.getGame().getStartTurn() + gc.getGame().getMaxTurns(), False),)),
						activePlayer.getTeam().getName() + ":",
						u"%d" % ourScore,
					] + ([
						gc.getTeam(iBestScoreTeam).getName() + ":",
						u"%d" % bestScore
					] if iBestScoreTeam != -1 else []))

				if (victory.isConquest()):
					commitHeader()
					row = [u""] * numColumns
					row[0] = localText.getText("TXT_KEY_VICTORY_SCREEN_ELIMINATE_ALL", ())
					row[1] = localText.getText("TXT_KEY_VICTORY_SCREEN_RIVALS_LEFT", ())
					row[2] = unicode(nRivals)
# BUG Additions Start
					if kHasBUG and AdvisorOpt.isVictories():
						if nVassaled != 0:
							sString = localText.getText("TXT_KEY_BUG_VICTORY_VASSALED", (nVassaled, ))
							row[3] = sString
						if nRivals - nknown != 0:
							sString = localText.getText("TXT_KEY_BUG_VICTORY_UNKNOWN", (nRivals - nknown, ))
							row[4] = sString
# BUG Additions End

					appendRow(row)

				if (gc.getGame().getAdjustedPopulationPercent(iLoopVC) > 0):
					commitHeader()
					row = [u""] * numColumns
					row[0] = localText.getText("TXT_KEY_VICTORY_SCREEN_PERCENT_POP", (gc.getGame().getAdjustedPopulationPercent(iLoopVC), ))
					row[1] = activePlayer.getTeam().getName() + ":"
					row[2] = u"%.2f%%" % popPercent
					if (iBestPopTeam != -1):
						row[3] = gc.getTeam(iBestPopTeam).getName() + ":"
						row[4] = u"%.2f%%" % (bestPop * 100 / totalPop)
					appendRow(row)

				if (gc.getGame().getAdjustedLandPercent(iLoopVC) > 0):
					commitHeader()
					row = [u""] * numColumns
					row[0] = localText.getText("TXT_KEY_VICTORY_SCREEN_PERCENT_LAND", (gc.getGame().getAdjustedLandPercent(iLoopVC), ))
					row[1] = activePlayer.getTeam().getName() + ":"
					row[2] = u"%.2f%%" % landPercent
					if (iBestLandTeam != -1):
						row[3] = gc.getTeam(iBestLandTeam).getName() + ":"
						row[4] = u"%.2f%%" % (bestLand * 100 / totalLand)
					appendRow(row)

				if (victory.getReligionPercent() > 0):
					commitHeader()
					row = [u""] * numColumns
					row[0] = localText.getText("TXT_KEY_VICTORY_SCREEN_PERCENT_RELIGION", (victory.getReligionPercent(), ))
					if (iOurReligion != -1):
						row[1] = gc.getReligionInfo(iOurReligion).getDescription() + ":"
						row[2] = u"%d%%" % ourReligionPercent
					else:
						row[1] = activePlayer.getTeam().getName() + ":"
						row[2] = u"No Holy City"
					if (iBestReligion != -1):
						row[3] = gc.getReligionInfo(iBestReligion).getDescription() + ":"
						row[4] = u"%d%%" % religionPercent
					appendRow(row)

				if (victory.getTotalCultureRatio() > 0):
					commitHeader()
					row = [u""] * numColumns
					row[0] = localText.getText("TXT_KEY_VICTORY_SCREEN_PERCENT_CULTURE", (int((100.0 * bestCulture) / victory.getTotalCultureRatio()), ))
					row[1] = activePlayer.getTeam().getName() + ":"
					row[2] = unicode(ourCulture)
					if (iBestLandTeam != -1):
						row[3] = gc.getTeam(iBestCultureTeam).getName() + ":"
						row[4] = unicode(bestCulture)
					appendRow(row)

				iBestBuildingTeam = -1
				bestBuilding = 0
				for iLoopTeam in range(gc.getMAX_CIV_TEAMS()):
					if (gc.getTeam(iLoopTeam).isAlive() and not gc.getTeam(iLoopTeam).isMinorCiv() and not gc.getTeam(iLoopTeam).isBarbarian()):
						if (iLoopTeam != iActiveTeam and (activePlayer.getTeam().isHasMet(iLoopTeam) or gc.getGame().isDebugMode())):
							teamBuilding = 0
							for i in range(gc.getNumBuildingClassInfos()):
								if (gc.getBuildingClassInfo(i).getVictoryThreshold(iLoopVC) > 0):
									teamBuilding += gc.getTeam(iLoopTeam).getBuildingClassCount(i)
							if (teamBuilding > bestBuilding):
								bestBuilding = teamBuilding
								iBestBuildingTeam = iLoopTeam

				for i in range(gc.getNumBuildingClassInfos()):
					if (gc.getBuildingClassInfo(i).getVictoryThreshold(iLoopVC) > 0):
						commitHeader()
						row = [u""] * numColumns
						szNumber = unicode(gc.getBuildingClassInfo(i).getVictoryThreshold(iLoopVC))
						row[0] = localText.getText("TXT_KEY_VICTORY_SCREEN_BUILDING", ())
						row[1] = activePlayer.getTeam().getName() + u":"
						row[2] = activePlayer.getTeam().getBuildingClassCount(i)
						if (iBestBuildingTeam != -1):
							row[3] = gc.getTeam(iBestBuildingTeam).getName() + ":"
							row[4] = gc.getTeam(iBestBuildingTeam).getBuildingClassCount(i)
						appendRow(row)

				iBestProjectTeam = -1
				bestProject = -1
				for iLoopTeam in range(gc.getMAX_CIV_TEAMS()):
					if (gc.getTeam(iLoopTeam).isAlive() and not gc.getTeam(iLoopTeam).isMinorCiv() and not gc.getTeam(iLoopTeam).isBarbarian()):
						if (iLoopTeam != iActiveTeam
						and (activePlayer.getTeam().isHasMet(iLoopTeam) or gc.getGame().isDebugMode())
						and self.isApolloBuiltbyTeam(gc.getTeam(iLoopTeam))):
							teamProject = 0
							for i in range(gc.getNumProjectInfos()):
								if (gc.getProjectInfo(i).getVictoryThreshold(iLoopVC) > 0):
									teamProject += gc.getTeam(iLoopTeam).getProjectCount(i)
							if (teamProject > bestProject):
								bestProject = teamProject
								iBestProjectTeam = iLoopTeam

# BUG Additions Start
				if kHasBUG and AdvisorOpt.isVictories():
					bApolloShown = False
					for i in range(gc.getNumProjectInfos()):
						if (gc.getProjectInfo(i).getVictoryThreshold(iLoopVC) > 0):
							if not self.isApolloBuilt():
								commitHeader()
								row = [u""] * numColumns
								row[0] = localText.getText("TXT_KEY_PROJECT_APOLLO_PROGRAM", ())
								row[1] = activePlayer.getTeam().getName() + ":"
								row[2] = localText.getText("TXT_KEY_VICTORY_SCREEN_NOT_BUILT", ())
								appendRow(row)
								break
							else:
								commitHeader()
								row = [u""] * numColumns
								if not bApolloShown:
									bApolloShown = True
									row[0] = localText.getText("TXT_KEY_PROJECT_APOLLO_PROGRAM", ())

									if self.isApolloBuiltbyTeam(activePlayer.getTeam()):
										row[1] = localText.getText("TXT_KEY_VICTORY_SCREEN_BUILT", (activePlayer.getTeam().getName(), ))
									else:
										row[1] = activePlayer.getTeam().getName() + ":"
										row[2] = localText.getText("TXT_KEY_VICTORY_SCREEN_NOT_BUILT", ())

									if (iBestProjectTeam != -1):
										row[3] = localText.getText("TXT_KEY_VICTORY_SCREEN_BUILT", (gc.getTeam(iBestProjectTeam).getName(), ))

								appendRow(row)
								iReqTech = gc.getProjectInfo(i).getTechPrereq()

								if (gc.getProjectInfo(i).getVictoryMinThreshold(iLoopVC) == gc.getProjectInfo(i).getVictoryThreshold(iLoopVC)):
									szNumber = unicode(gc.getProjectInfo(i).getVictoryThreshold(iLoopVC))
								else:
									szNumber = unicode(gc.getProjectInfo(i).getVictoryMinThreshold(iLoopVC)) + u"-" + unicode(gc.getProjectInfo(i).getVictoryThreshold(iLoopVC))

								row = [u""] * numColumns
								sSSPart = localText.getText("TXT_KEY_VICTORY_SCREEN_BUILDING", (szNumber, gc.getProjectInfo(i).getTextKey()))
								row[0] = sSSPart

								if self.isApolloBuiltbyTeam(activePlayer.getTeam()):

									bHasTech = gc.getTeam(iActiveTeam).isHasTech(iReqTech)
									sSSPlayer = activePlayer.getTeam().getName() + ":"
									sSSCount = "%i [+%i]" % (activePlayer.getTeam().getProjectCount(i), activePlayer.getTeam().getProjectMaking(i))

									iHasTechColor = -1
									iSSColor = 0
									if activePlayer.getTeam().getProjectCount(i) == gc.getProjectInfo(i).getVictoryThreshold(iLoopVC):
										sSSCount = "%i" % (activePlayer.getTeam().getProjectCount(i))
										iSSColor = ColorUtil.keyToType("COLOR_GREEN")
									elif activePlayer.getTeam().getProjectCount(i) >= gc.getProjectInfo(i).getVictoryMinThreshold(iLoopVC):
										iSSColor = ColorUtil.keyToType("COLOR_YELLOW")

									if iSSColor > 0:
										sSSPlayer = localText.changeTextColor(sSSPlayer, iSSColor)
										sSSCount = localText.changeTextColor(sSSCount, iSSColor)

									row[1] = sSSPlayer
									if bHasTech:
										row[2] = sSSCount

									#check if spaceship
									#if (gc.getProjectInfo(i).isSpaceship()):
									#	bSpaceshipFound = True
								
								# add AI space ship info
								if (iBestProjectTeam != -1):
									pTeam = gc.getTeam(iBestProjectTeam)
									sSSPlayer = gc.getTeam(iBestProjectTeam).getName() + ":"
									sSSCount = "%i" % (pTeam.getProjectCount(i))

									if kHasBUG:
										Techs = TechUtil.getVisibleKnownTechs(pTeam.getLeaderID(), self.iActivePlayer)
										bHasTech = iReqTech in Techs
									else:
										bHasTech = False

									iHasTechColor = -1
									iSSColor = 0
									if pTeam.getProjectCount(i) == gc.getProjectInfo(i).getVictoryThreshold(iLoopVC):
										iSSColor = ColorUtil.keyToType("COLOR_GREEN")
									elif pTeam.getProjectCount(i) >= gc.getProjectInfo(i).getVictoryMinThreshold(iLoopVC):
										iSSColor = ColorUtil.keyToType("COLOR_YELLOW")
									elif bHasTech:
										iSSColor = ColorUtil.keyToType("COLOR_PLAYER_ORANGE")

									if iSSColor > 0:
										sSSPlayer = localText.changeTextColor(sSSPlayer, iSSColor)
										sSSCount = localText.changeTextColor(sSSCount, iSSColor)

									row[3] = sSSPlayer
									row[4] = sSSCount

								appendRow(row)

				else: # vanilla BtS SShip display
					for i in range(gc.getNumProjectInfos()):
						if (gc.getProjectInfo(i).getVictoryThreshold(iLoopVC) > 0):
							commitHeader()
							row = [u""] * numColumns
							if (gc.getProjectInfo(i).getVictoryMinThreshold(iLoopVC) == gc.getProjectInfo(i).getVictoryThreshold(iLoopVC)):
								szNumber = unicode(gc.getProjectInfo(i).getVictoryThreshold(iLoopVC))
							else:
								szNumber = unicode(gc.getProjectInfo(i).getVictoryMinThreshold(iLoopVC)) + u"-" + unicode(gc.getProjectInfo(i).getVictoryThreshold(iLoopVC))
							row[0] = localText.getText("TXT_KEY_VICTORY_SCREEN_BUILDING", (szNumber, gc.getProjectInfo(i).getTextKey()))
							row[1] = activePlayer.getTeam().getName() + ":"
							row[2] = unicode(activePlayer.getTeam().getProjectCount(i))
							
							#check if spaceship
							#if (gc.getProjectInfo(i).isSpaceship()):
							#	bSpaceshipFound = True

							if (iBestProjectTeam != -1):
								row[3] = gc.getTeam(iBestProjectTeam).getName() + ":"
								row[4] = unicode(gc.getTeam(iBestProjectTeam).getProjectCount(i))

							appendRow(row)
# BUG Additions End
						
			
				if (victory.isDiploVote()):
					for (iVoteBuildingClass, iUNTeam, bUnknown) in aiVoteBuildingClass:
						commitHeader()
						row = [u""] * numColumns
						row[0] = localText.getText("TXT_KEY_VICTORY_SCREEN_ELECTION", (gc.getBuildingClassInfo(iVoteBuildingClass).getTextKey(), ))
						if (iUNTeam != -1):
							if bUnknown:
								szName = localText.getText("TXT_KEY_TOPCIVS_UNKNOWN", ())
							else:
								szName = gc.getTeam(iUNTeam).getName()
							row[1] = localText.getText("TXT_KEY_VICTORY_SCREEN_BUILT", (szName, ))
						else:
							row[1] = localText.getText("TXT_KEY_VICTORY_SCREEN_NOT_BUILT", ())
						#bEntriesFound = True
						appendRow(row)
					
				if (victory.getCityCulture() != CultureLevelTypes.NO_CULTURELEVEL and victory.getNumCultureCities() > 0):
					ourBestCities = self.getListCultureCities(self.iActivePlayer, victory)[0:victory.getNumCultureCities()]
					
					iBestCulturePlayer = -1
					bestCityCulture = 0
# BUG - 3.19 Culture Threshold - start
					maxCityCulture = GameUtil.getCultureThreshold(victory.getCityCulture())
# BUG - 3.19 Culture Threshold - end
					for iLoopPlayer in range(gc.getMAX_PLAYERS()):
						if (gc.getPlayer(iLoopPlayer).isAlive() and not gc.getPlayer(iLoopPlayer).isMinorCiv() and not gc.getPlayer(iLoopPlayer).isBarbarian()):
							if (iLoopPlayer != self.iActivePlayer and (activePlayer.getTeam().isHasMet(gc.getPlayer(iLoopPlayer).getTeam()) or gc.getGame().isDebugMode())):
								theirBestCities = self.getListCultureCities(iLoopPlayer, victory)[0:victory.getNumCultureCities()]
								
								iTotalCulture = 0
								for loopCity in theirBestCities:
									if loopCity[0] >= maxCityCulture:
										iTotalCulture += maxCityCulture
									else:
										iTotalCulture += loopCity[0]
								
								if (iTotalCulture >= bestCityCulture):
									bestCityCulture = iTotalCulture
									iBestCulturePlayer = iLoopPlayer

					if (iBestCulturePlayer != -1):
						theirBestCities = self.getListCultureCities(iBestCulturePlayer, victory)[0:(victory.getNumCultureCities())]
					else:
						theirBestCities = []
						
					row = [u""] * numColumns
					row[0] = localText.getText("TXT_KEY_VICTORY_SCREEN_CITY_CULTURE", (victory.getNumCultureCities(), gc.getCultureLevelInfo(victory.getCityCulture()).getTextKey()))

					for i in range(victory.getNumCultureCities()):
						if (len(ourBestCities) > i):
							row[1] = ourBestCities[i][1].getName() + ":"
# BUG Additions Start
							if kHasBUG and AdvisorOpt.isVictories():
								if ourBestCities[i][2] == -1:
									sString = "%i (-)" % (ourBestCities[i][0])
								elif ourBestCities[i][2] > 100:
									sString = "%i (100+)" % (ourBestCities[i][0])
								elif ourBestCities[i][2] < 1:
									sString = "%i (L)" % (ourBestCities[i][0])
								else:
									sString = "%i (%i)" % (ourBestCities[i][0], ourBestCities[i][2])
							else:
								sString = "%i" % (ourBestCities[i][0])

							row[2] = sString
# BUG Additions End

						if (len(theirBestCities) > i):
							row[3] = theirBestCities[i][1].getName() + ":"

# BUG Additions Start
							if kHasBUG and AdvisorOpt.isVictories():
								if theirBestCities[i][2] == -1:
									sString = "%i (-)" % (theirBestCities[i][0])
								elif theirBestCities[i][2] > 100:
									sString = "%i (100+)" % (theirBestCities[i][0])
								elif theirBestCities[i][2] < 1:
									sString = "%i (L)" % (theirBestCities[i][0])
								else:
									sString = "%i (%i)" % (theirBestCities[i][0], theirBestCities[i][2])
							else:
								sString = "%i" % (theirBestCities[i][0])

							row[4] = sString
# BUG Additions End
						commitHeader()
						appendRow(row)
						row[0] = u""
					#bEntriesFound = True
					
				if isHeaderCommitted[0]:
					appendRow([u" "])

	def buildGameSettingsScreen(self, screen, root):
		self.ANON_WIDGET_ID = root + "_Anon"
		
		## Start HOF MOD V1.61.001  3/8
		failedHOFChecks = False
		showHOFSettingChecks = kHasBUG and BUFFYOpt.isWarningsSettings()
		self.getBuffyWarningText()

		# EF: Mac only?
		if showHOFSettingChecks and GameSetUpCheck.isXOTMScenario():
			showHOFSettingChecks = False
		## End HOF MOD V1.61.001  3/8

		activePlayer = gc.getPlayer(self.iActivePlayer)
		
		tables = []
		settingsLines = []

		tables.append((localText.getText("TXT_KEY_MAIN_MENU_SETTINGS", ()).upper(), settingsLines))
		
		if kHasBUG and showHOFSettingChecks and BugPath.isMac():
			failedHOFChecks = True
			showHOFSettingChecks = False
			settingsLines.append(self.BuffyWarningMac)
				
		settingsLines.append(localText.getText("TXT_KEY_LEADER_CIV_DESCRIPTION", (activePlayer.getNameKey(), activePlayer.getCivilizationShortDescriptionKey())))
		settingsLines.append(u"     (" + CyGameTextMgr().parseLeaderTraits(activePlayer.getLeaderType(), activePlayer.getCivilizationType(), True, False) + u")")
		settingsLines.append(" ")
		settingsLines.append(localText.getText("TXT_KEY_SETTINGS_DIFFICULTY", (gc.getHandicapInfo(activePlayer.getHandicapType()).getTextKey(), )))

		settingsLines.append(" ")
#		screen.appendListBoxStringNoUpdate(szSettingsTable, gc.getMap().getMapScriptName(), WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_LEFT_JUSTIFY )

		## Start HOF MOD V1.61.001  4/8
		if kHasBUG:
			if (showHOFSettingChecks and not GameSetUpCheck.isMapScriptOK()):
				settingsLines.append(gc.getMap().getMapScriptName() + " " + self.BuffyWarningNotAllowed)
				failedHOFChecks = True
			else:
				settingsLines.append(gc.getMap().getMapScriptName())

			if (showHOFSettingChecks and GameSetUpCheck.getBalanced()):
				settingsLines.append(self.BuffyWarningBalancedResoucesNotAllowed)
				failedHOFChecks = True

			if (showHOFSettingChecks and not GameSetUpCheck.getWorldWrapSettingOK()):
				text = localText.getText(worldWrapString[GameSetUpCheck.getWorldWrap()],())
				text += " " + self.BuffyWarningNotAllowed
				settingsLines.append(text)
				failedHOFChecks = True
			else:
				text = localText.getText(worldWrapString[GameSetUpCheck.getWorldWrap()],())
				settingsLines.append(text)
		## End HOF MOD V1.61.001  4/8

		settingsLines.append(localText.getText("TXT_KEY_SETTINGS_MAP_SIZE", (gc.getWorldInfo(gc.getMap().getWorldSize()).getTextKey(), )) + u" (%d x %d)" % (gc.getMap().getGridWidth(), gc.getMap().getGridHeight()))
		settingsLines.append(localText.getText("TXT_KEY_SETTINGS_CLIMATE", (gc.getClimateInfo(gc.getMap().getClimate()).getTextKey(), )))
		settingsLines.append(localText.getText("TXT_KEY_SETTINGS_SEA_LEVEL", (gc.getSeaLevelInfo(gc.getMap().getSeaLevel()).getTextKey(), )))
		settingsLines.append(" ")
		settingsLines.append(localText.getText("TXT_KEY_SETTINGS_STARTING_ERA", (gc.getEraInfo(gc.getGame().getStartEra()).getTextKey(), )))
		settingsLines.append(localText.getText("TXT_KEY_SETTINGS_GAME_SPEED", (gc.getGameSpeedInfo(gc.getGame().getGameSpeedType()).getTextKey(), )))

		## Start HOF MOD V1.61.001  5/8
		if showHOFSettingChecks:
			text = None
			for iVCLoop in range(gc.getNumVictoryInfos()):
				if not gc.getGame().isVictoryValid(iVCLoop):
					if text is not None:
						text += ", "
					else:
						text = self.BuffyWarningVictoryConditions + " "
					text += gc.getVictoryInfo(iVCLoop).getDescription()
			if text is not None:
				failedHOFChecks = True
				settingsLines.append(" ")
				settingsLines.append(text)
			
			if GameSetUpCheck.crcResult != 0:
				failedHOFChecks = True
				if GameSetUpCheck.crcResult==1:
					text = self.BuffyWarningCheckSumMissing
				elif GameSetUpCheck.crcResult==2:
					text = self.BuffyWarningCheckSumDifferent
				else:
					text = self.BuffyWarningCheckSumFailed
				settingsLines.append(" ")
				settingsLines.append(BugUtil.colorText(text, "COLOR_WARNING_TEXT"))
		## End HOF MOD V1.61.001  5/8

		optionsLines = []
		tables.append((localText.getText("TXT_KEY_MAIN_MENU_CUSTOM_SETUP_OPTIONS", ()).upper(), optionsLines))

		## Start HOF MOD V1.61.001  6/8
		if (showHOFSettingChecks and gc.getGame().isGameMultiPlayer()):
			optionsLines.append(self.BuffyWarningMultiPlayerNotAllowed)
			failedHOFChecks = True

		if showHOFSettingChecks:
			invalidOptions = GameSetUpCheck.getInvalidGameOptions()
			if len(invalidOptions) > 0:
				failedHOFChecks = True
				for i in range(GameOptionTypes.NUM_GAMEOPTION_TYPES):
					szDescription = gc.getGameOptionInfo(i).getDescription()
					if i not in invalidOptions:
						if gc.getGame().isOption(i):
							optionsLines.append(szDescription)
					else:
						if invalidOptions[i]:
							optionsLines.append(BugUtil.colorText(szDescription, "COLOR_YELLOW") + u" " + self.BuffyWarningRequired)
						else:
							optionsLines.append(szDescription + u" " + self.BuffyWarningNotAllowed)
			else:
				for i in range(GameOptionTypes.NUM_GAMEOPTION_TYPES):
					optionsLines.append(gc.getGameOptionInfo(i).getDescription())
		else:
			for i in range(GameOptionTypes.NUM_GAMEOPTION_TYPES):
				if gc.getGame().isOption(i):
					optionsLines.append(gc.getGameOptionInfo(i).getDescription())

		if showHOFSettingChecks and not Buffy.isDllPresent():
			failedHOFChecks = True
			optionsLines.append(u"\n" + self.BuffyWarningNoDll)
		
		if showHOFSettingChecks and not Buffy.isDllInCorrectPath():
			failedHOFChecks = True
			text = "\n" + self.BuffyWarningInstallLocation + "\n" + gc.getGame().getExePath() + "\Mods"
			optionsLines.append(text)

		if (gc.getGame().isOption(GameOptionTypes.GAMEOPTION_ADVANCED_START)):
			szNumPoints = u"%s %d" % (localText.getText("TXT_KEY_ADVANCED_START_POINTS", ()), gc.getGame().getNumAdvancedStartPoints())
			optionsLines.append(szNumPoints)

		if (gc.getGame().isGameMultiPlayer()):
			for i in range(gc.getNumMPOptionInfos()):
				if (gc.getGame().isMPOption(i)):
					optionsLines.append(gc.getMPOptionInfo(i).getDescription())

			if (gc.getGame().getMaxTurns() > 0):
				szMaxTurns = u"%s %d" % (localText.getText("TXT_KEY_TURN_LIMIT_TAG", ()), gc.getGame().getMaxTurns())
				optionsLines.append(szMaxTurns)

			if (gc.getGame().getMaxCityElimination() > 0):
				szMaxCityElimination = u"%s %d" % (localText.getText("TXT_KEY_CITY_ELIM_TAG", ()), gc.getGame().getMaxCityElimination())
				optionsLines.append(szMaxCityElimination)

		rivalsMetLines = []
		tables.append((localText.getText("TXT_KEY_RIVALS_MET", ()).upper(), rivalsMetLines))

	
		## Start HOF MOD V1.61.001  7/8
		if kHasBUG and showHOFSettingChecks:
			civCounts = [0] * gc.getNumLeaderHeadInfos()
			opponentCount = -1
			for player in PlayerUtil.players(barbarian=False, minor=False):
				civCounts[player.getLeaderType()] += 1
				opponentCount += 1

			if (GameSetUpCheck.isMapSizeOK() and not GameSetUpCheck.isOpponentCountOK(opponentCount)):
				failedHOFChecks = True
				zsMapSize = localText.getText("TXT_KEY_SETTINGS_MAP_SIZE", (gc.getWorldInfo(gc.getMap().getWorldSize()).getTextKey(), ))
				minOpponents, maxOpponents = GameSetUpCheck.getValidOpponentCountRange()
				if (opponentCount < minOpponents):
					text = BugUtil.getText("TXT_KEY_BUFFYWARNING_TOO_FEW_OPPONENTS", (minOpponents, zsMapSize))
				elif (opponentCount > maxOpponents):
					text = BugUtil.getText("TXT_KEY_BUFFYWARNING_TOO_MANY_OPPONENTS", (maxOpponents, zsMapSize))
				rivalsMetLines.append(text)

			for iLoopCivs, count in enumerate(civCounts):
				if count > 1:
					failedHOFChecks = True
					zsLeader = gc.getLeaderHeadInfo(iLoopCivs).getText()
					text = BugUtil.getText("TXT_KEY_BUFFYWARNING_MULT_LEADERS", (zsLeader, count))
					rivalsMetLines.append(text)

			if gc.getGame().isTeamGame():
				failedHOFChecks = True
				rivalsMetLines.append(self.BuffyWarningNoTeams)
		## End HOF MOD V1.61.001  7/8

		for iLoopPlayer in range(gc.getMAX_CIV_PLAYERS()):
			player = gc.getPlayer(iLoopPlayer)
			if (player.isEverAlive() and iLoopPlayer != self.iActivePlayer and (gc.getTeam(player.getTeam()).isHasMet(activePlayer.getTeam()) or gc.getGame().isDebugMode()) and not player.isBarbarian() and not player.isMinorCiv()):
				rivalsMetLines.append(localText.getText("TXT_KEY_LEADER_CIV_DESCRIPTION", (player.getNameKey(), player.getCivilizationShortDescriptionKey())))
				rivalsMetLines.append(u"     (" + CyGameTextMgr().parseLeaderTraits(player.getLeaderType(), player.getCivilizationType(), True, False) + ")")
				rivalsMetLines.append(" ")

		## Start HOF MOD V1.61.001  8/8
		if failedHOFChecks:
			tables.append((self.BuffyWarning, [self.BuffyWarningNotValid]))
		## End HOF MOD V1.61.001  8/8

		# Okay, now build the UI. Table layout of columns.
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [TableLayoutConfigCell(ivec2(0, 0))]
		table.repeatAxis = HeckTuiAxis.Horizontal
		table.gap = ivec2(1, 0)
		screen.setTableLayout(root, table)
		
		for title, lines in tables:
			panelName = self.getNextWidgetName()
			screen.newPanel(root, panelName)
			screen.setVFlowLayout(panelName)
			screen.newLabel(panelName, self.getNextWidgetName(), title)
			labelContainerName = self.getNextWidgetName()
			screen.newScrollBarPanel(panelName, self.getNextWidgetName(), labelContainerName, HeckTuiAxis.Vertical)
			screen.setFillLayout(labelContainerName)
			screen.newLabel(labelContainerName, self.getNextWidgetName(), u"\n".join(lines))
		

	def buildVotingScreen(self, screen, root):
		self.ANON_WIDGET_ID = root + "_Anon"
		
		activePlayer = gc.getPlayer(self.iActivePlayer)
		iActiveTeam = activePlayer.getTeam()

		aiVoteBuildingClass = []
		for i in range(gc.getNumBuildingInfos()):
			for j in range(gc.getNumVoteSourceInfos()):
				if (gc.getBuildingInfo(i).getVoteSourceType() == j):
					iUNTeam = -1
					bUnknown = true
					for iLoopTeam in range(gc.getMAX_CIV_TEAMS()):
						if (gc.getTeam(iLoopTeam).isAlive() and not gc.getTeam(iLoopTeam).isMinorCiv() and not gc.getTeam(iLoopTeam).isBarbarian()):
							if (gc.getTeam(iLoopTeam).getBuildingClassCount(gc.getBuildingInfo(i).getBuildingClassType()) > 0):
								iUNTeam = iLoopTeam
								if (iLoopTeam == iActiveTeam or gc.getGame().isDebugMode() or gc.getTeam(activePlayer.getTeam()).isHasMet(iLoopTeam)):
									bUnknown = false
								break

					aiVoteBuildingClass.append((gc.getBuildingInfo(i).getBuildingClassType(), iUNTeam, bUnknown))

		if (len(aiVoteBuildingClass) == 0):
			return

		rows = []

		for (iVoteBuildingClass, iUNTeam, bUnknown) in aiVoteBuildingClass:
			header = localText.getText("TXT_KEY_VICTORY_SCREEN_ELECTION", (gc.getBuildingClassInfo(iVoteBuildingClass).getTextKey(), ))
			if (iUNTeam != -1):
				if bUnknown:
					szName = localText.getText("TXT_KEY_TOPCIVS_UNKNOWN", ())
				else:
					szName = gc.getTeam(iUNTeam).getName()
				value = localText.getText("TXT_KEY_VICTORY_SCREEN_BUILT", (szName, ))
			else:
				value = localText.getText("TXT_KEY_VICTORY_SCREEN_NOT_BUILT", ())
			rows.append((header, value))

		for i in range(gc.getNumVoteSourceInfos()):
			if (gc.getGame().canHaveSecretaryGeneral(i) and -1 != gc.getGame().getSecretaryGeneral(i)):
				rows.append((gc.getVoteSourceInfo(i).getSecretaryGeneralText(), gc.getTeam(gc.getGame().getSecretaryGeneral(i)).getName()))

			for iLoop in range(gc.getNumVoteInfos()):
				if gc.getGame().countPossibleVote(iLoop, i) > 0:
					info = gc.getVoteInfo(iLoop)
					if gc.getGame().isChooseElection(iLoop):
						header = info.getDescription()
						if gc.getGame().isVotePassed(iLoop):
							value = localText.getText("TXT_KEY_POPUP_PASSED", ())
						else:
							value = localText.getText("TXT_KEY_POPUP_ELECTION_OPTION", (u"", gc.getGame().getVoteRequired(iLoop, i), gc.getGame().countPossibleVote(iLoop, i)))
						rows.append((header, value))

		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0)]
		table.rows = [TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)),
			TableLayoutConfigCell(ivec2(1, 0)),
		]
		table.gap = ivec2(1, 0)
		screen.setTableLayout(root, table)
		
		for header, value in rows:
			screen.newLabel(root, self.getNextWidgetName(), header)
			screen.newLabel(root, self.getNextWidgetName(), value)

# BUG Additions Start
	def rebuildMembersScreen(self, screen, root):
		self.ANON_WIDGET_ID = root + "_Anon"
		screen.delByPrefix(root + "_")
	
		iRelVote, iRelVoteIdx, iUNVote, iUNVoteIdx  = self.getVoteAvailable()

		if kHasBUG and AdvisorOpt.isMembers():
			if  iRelVote == -1: self.VoteBody = 2 # AP Not active
			elif iUNVote == -1: self.VoteBody = 1 # UN Not active

			if self.VoteBody == 1:
				iVoteBody = iRelVote
				iVoteIdx = iRelVoteIdx
			else:
				iVoteBody = iUNVote
				iVoteIdx = iUNVoteIdx

			self.rebuildMembersScreen_BUG(screen, root, iRelVote, iUNVote, iVoteBody, iVoteIdx)
		else:
			self.rebuildMembersScreen_NonBUG(screen, root)

	def getVoteAvailable(self):

		iRelVote = -1
		iRelVoteIdx = -1
		iUNVote = -1
		iUNVoteIdx = -1

		for i in range(gc.getNumVoteSourceInfos()):
			if gc.getGame().isDiploVote(i):
				if (gc.getGame().getVoteSourceReligion(i) != -1):
					iRelVote = i
				else:
					iUNVote = i

			if (gc.getGame().canHaveSecretaryGeneral(i)
			and gc.getGame().getSecretaryGeneral(i) != -1):
				for j in range(gc.getNumVoteInfos()):
					if gc.getVoteInfo(j).isVoteSourceType(i):
						if gc.getVoteInfo(j).isSecretaryGeneral():
							if (gc.getGame().getVoteSourceReligion(i) != -1):
								iRelVoteIdx = j
							else:
								iUNVoteIdx = j

							break

		BugUtil.debug("CvVictoryScreen: Rel Vote %i, UN Vote %i, Rel Vote Idx %i, UN Vote Idx %i", iRelVote, iRelVoteIdx, iUNVote, iUNVoteIdx)

		return iRelVote, iRelVoteIdx, iUNVote, iUNVoteIdx

	def rebuildMembersScreen_BUG(self, screen, root, iRelVote, iUNVote, iActiveVote, iVoteIdx):
		if (iRelVote == -1 and iUNVote == -1): return  # neither AP or UN are active

		activePlayer = gc.getPlayer(self.iActivePlayer)
		iActiveTeam = activePlayer.getTeam()

		# heading
		kVoteSource = gc.getVoteSourceInfo(iActiveVote)
		sTableHeader = u"<font=4b>" + kVoteSource.getDescription().upper() + u"</font>"
		if (gc.getGame().getVoteSourceReligion(iActiveVote) != -1):
			sTableHeader += " (" + gc.getReligionInfo(gc.getGame().getVoteSourceReligion(iActiveVote)).getDescription() + ")"

		# determine the two candidates, add to header
		iCandTeam1 = -1
		iCandTeam2 = -1
		for j in range(gc.getMAX_TEAMS()):
			BugUtil.debug("CvVictoryScreen: Team %i", j)

			if (gc.getTeam(j).isAlive()
			and gc.getGame().isTeamVoteEligible(j, iActiveVote)):
				BugUtil.debug("CvVictoryScreen: Team %i, %s <- vote eligible ", j, gc.getTeam(j).getName())
				if iCandTeam1 == -1:
					iCandTeam1 = j
				else:
					iCandTeam2 = j

		# get the first player for each team
		# going to use that to calculation attitude - too hard to calc attitude for team
		iCandPlayer1 = self.getPlayerOnTeam(iCandTeam1)
		iCandPlayer2 = self.getPlayerOnTeam(iCandTeam2)

		# candidate known returns -1 if there is no candidate, 0 if not known or 1 if known
		iCand1Known, sCand1Name = self.getCandStatusName(iCandTeam1)
		iCand2Known, sCand2Name = self.getCandStatusName(iCandTeam2)

		#screen.setTableText(szHeading, 1, iRow, sCand1Name, "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_CENTER_JUSTIFY)
		#screen.setTableText(szHeading, 2, iRow, sCand2Name, "", WidgetTypes.WIDGET_GENERAL, -1, -1, CvUtil.FONT_CENTER_JUSTIFY)

		# set up the member table

		# initialize the candidate votes and total vote counter
		iVoteTotal = [0] * 2
		iVoteCand = [0] * 2

		lMembers = []
		iAPUNTeam = self.getAP_UN_OwnerTeam()

		for j in range(gc.getMAX_PLAYERS()):
			pPlayer = gc.getPlayer(j)
			if (pPlayer.isAlive()
			and not pPlayer.isBarbarian()):
				iPlayer = j
				lPlayerName = pPlayer.getName()
				lPlayerVotes = 10000 - pPlayer.getVotes(iVoteIdx, iActiveVote)   # so that it sorts from most votes to least

				if (gc.getGame().canHaveSecretaryGeneral(iActiveVote)
				and iAPUNTeam == pPlayer.getTeam()
				and gc.getGame().getSecretaryGeneral(iActiveVote) == -1):
					lPlayerStatus = 0
					lPlayerLabel = gc.getVoteSourceInfo(iActiveVote).getSecretaryGeneralText()
				elif (gc.getGame().canHaveSecretaryGeneral(iActiveVote)
				and gc.getGame().getSecretaryGeneral(iActiveVote) == pPlayer.getTeam()):
					lPlayerStatus = 1
					lPlayerLabel = gc.getVoteSourceInfo(iActiveVote).getSecretaryGeneralText()
				elif (pPlayer.isFullMember(iActiveVote)):
					lPlayerStatus = 2
					lPlayerLabel = localText.getText("TXT_KEY_VOTESOURCE_FULL_MEMBER", ())
				elif (pPlayer.isVotingMember(iActiveVote)):
					lPlayerStatus = 3
					lPlayerLabel = localText.getText("TXT_KEY_VOTESOURCE_VOTING_MEMBER", ())
				else:
					lPlayerStatus = 4
					lPlayerLabel = localText.getText("TXT_KEY_VOTESOURCE_NON_VOTING_MEMBER", ())

				lMembers.append([lPlayerStatus, lPlayerVotes, iPlayer, lPlayerLabel])

		lMembers.sort()
		
		rows = []

		for lMember in lMembers:
			lMemberStatus = lMember[0]
			lMemberVotes = 10000 - lMember[1]
			iMember = lMember[2]
			lMemberLabel = lMember[3]

			# player name
			bKnown, szPlayerText = self.getPlayerStatusName(iMember)

			if (lMemberVotes > 0
			and bKnown):
				szPlayerText += localText.getText("TXT_KEY_VICTORY_SCREEN_PLAYER_VOTES", (lMemberVotes, iActiveVote), )

			row = [szPlayerText, u"", u"", u"", u"", u""]

			if iMember != self.iActivePlayer:
				# player attitude to candidate #1
				szText = AttitudeUtil.getAttitudeText(iMember, iCandPlayer1, True, True, False, False)
				if szText != None and iCand1Known == 1 and bKnown:
					row[1] = (szText, WidgetTypes.WIDGET_LEADERHEAD, iMember, iCandPlayer1)

				# player attitude to candidate #2
				szText = AttitudeUtil.getAttitudeText(iMember, iCandPlayer2, True, True, False, False)
				if szText != None and iCand2Known == 1 and bKnown:
					row[3] = (szText, WidgetTypes.WIDGET_LEADERHEAD, iMember, iCandPlayer2)

			iVote = self.getVotesForWhichCandidate(iMember, iCandTeam1, iCandTeam2, self.VoteType)
			iVote_Column = -1

			if iVote != -1:
				sVote = str(lMemberVotes)
				iVoteTotal[iVote - 1] += lMemberVotes

				# number of votes for Candidate #1
				if (iVote == 1
				and lMemberVotes > 0
				and iCand1Known == 1):
					row[2] = sVote

				# number of votes for Candidate #2
				if (iVote == 2
				and lMemberVotes > 0
				and iCand2Known == 1):
					row[4] = sVote

			# player status
			if bKnown:
				row[5] = lMemberLabel
				
			rows.append(row)

			# store the candidates own votes
			if (iCandTeam1 == gc.getPlayer(iMember).getTeam()
			and iCand1Known == 1):
				iVoteCand[0] = lMemberVotes
			if (iCandTeam2 == gc.getPlayer(iMember).getTeam()
			and iCand2Known == 1):
				iVoteCand[1] = lMemberVotes

		# calculate the maximum number of votes
		iMaxVotes = 0
		for iLoop in range(gc.getNumVoteInfos()):
			if gc.getGame().countPossibleVote(iLoop, iActiveVote) > 0:
				iMaxVotes = gc.getGame().countPossibleVote(iLoop, iActiveVote)
				break

		row = [u""] * 5
		iVoteReq = self.getVoteReq(iActiveVote, self.VoteType)
		sVoteReq = "%i" % (iVoteReq)
		sString = u"<font=3b>" + localText.getText("TXT_KEY_BUG_VICTORY_VOTES_TOTAL", ()) + "</font> "
		if (iCand1Known != 0
		and iCand2Known != 0):
			sString +=  localText.getText("TXT_KEY_BUG_VICTORY_VOTES_REQUIRED", (sVoteReq,))
		row[0] = sString

		if iCand1Known == 1:
			# color code the totals
			sVoteTotal = str(iVoteTotal[0])
			iColor = self.getVoteTotalColor(iVoteReq, iVoteTotal[0], iVoteCand[0], iVoteTotal[0] > iVoteTotal[1], self.VoteType == 2)
			if iColor != -1:
				sVoteTotal = localText.changeTextColor(sVoteTotal, iColor)
			row[2] = sVoteTotal

		if iCand2Known == 1:
			sVoteTotal = str(iVoteTotal[1])
			iColor = self.getVoteTotalColor(iVoteReq, iVoteTotal[1], iVoteCand[1], iVoteTotal[1] > iVoteTotal[0], self.VoteType == 2)
			if iColor != -1:
				sVoteTotal = localText.changeTextColor(sVoteTotal, iColor)
			row[4] = sVoteTotal

		rows.append(row)
		# add a blank row
		rows.append([u" "] + [u""] * 4)

		# SecGen vote prediction
		if iVoteTotal[0] > iVoteTotal[1]:
			iWinner = 0
			sWin = sCand1Name
		else:
			iWinner = 1
			sWin = sCand2Name
		iLoser = 1 - iWinner

		fVotePercent = 100.0 * iVoteTotal[iWinner] / iMaxVotes
		fMargin = 100.0 * (iVoteTotal[iWinner] - iVoteTotal[iLoser]) / iMaxVotes
		
		if self.VoteType == 1:
			sSecGen = gc.getVoteSourceInfo(iActiveVote).getSecretaryGeneralText()
		else:
			sSecGen = localText.getText("TXT_KEY_BUG_VICTORY_DIPLOMATIC", ())

		# display SecGen vote prediction
		if (iCand1Known != 0 and iCand2Known != 0):
			sString = sSecGen + ":"
			rows.append([sString])

			sString = "     " + localText.getText("TXT_KEY_BUG_VICTORY_BUG_POLL_RESULT", (sWin, self.formatPercent(fVotePercent), self.formatPercent(fMargin)))
			rows.append([sString])

			# BUG Poll statistical error
			iRandError = 3.5 + gc.getASyncRand().get(10, "Election Results Statistical Error") / 10.0
			sString = localText.getText("TXT_KEY_BUG_VICTORY_BUG_POLL_ERROR", (self.formatPercent(iRandError), ))
			rows.append([sString])

		rows.append([u" "] + [u""] * 4)

		# add info about vote timing
		iVoteTimer = gc.getGame().getVoteTimer(iActiveVote)
		sString = localText.getText("TXT_KEY_BUG_VICTORY_TURNS_NEXT_VOTE", (iVoteTimer,) )
		sString = u"<font=2>" + sString + "</font>"
		rows.append([sString])

		iSecGenTimer = gc.getGame().getSecretaryGeneralTimer(iActiveVote)
		sString = localText.getText("TXT_KEY_BUG_VICTORY_VOTES_NEXT_ELECTION", (iSecGenTimer,) )
		sString = u"<font=2>" + sString + "</font>"
		rows.append([sString])
		
		# Right. We have the headers and the rows.
		# Layout as table.
		screen.newPanel(root, root + "_DataPanel")
		screen.newPanel(root, root + "_ButtonPanel")
		
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # _DataPanel
			TableLayoutConfigCell(ivec2(0, 1)), # _ButtonPanel
		]
		table.gap = ivec2(0, 1)
		screen.setTableLayout(root, table)

		# _DataPanel
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)] + [TableLayoutConfigRowColDesc(0, 0)] * 5
		table.rows = [TableLayoutConfigRowColDesc(0, 0)] * (1 + len(rows))
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1)), # Title
			TableLayoutConfigCell(ivec2(1, 0), ivec2(2, 1)), # Candidate1
			TableLayoutConfigCell(ivec2(3, 0), ivec2(2, 1)), # Candidate2
		] + [TableLayoutConfigCell(ivec2(c, r + 1)) for r in range(len(rows)) for c in range(6)]
		table.gap = ivec2(1, 0)
		parent = root + "_DataPanel"
		screen.setTableLayout(parent, table)
		screen.newLabel(parent, self.getNextWidgetName(), sTableHeader)
		screen.newLabel(parent, self.getNextWidgetName(), sCand1Name)
		screen.newLabel(parent, self.getNextWidgetName(), sCand2Name)

		for row in rows:
			for cell in row:
				text = cell
				if isinstance(cell, tuple):
					text = cell[0]
				name = self.getNextWidgetName()
				screen.newLabel(parent, name, text)
				if isinstance(cell, tuple):
					screen.setLabelWidgetData(name, cell[1], cell[2], cell[3])
			for i in range(len(row), 6):
				screen.newLabel(parent, self.getNextWidgetName(), u"")
		
		# _ButtonPanel
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 1)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), RectJustilign(EJustilign.Start, EJustilign.Stretch)), # SourcePanel
			TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), RectJustilign(EJustilign.End, EJustilign.Stretch)), # TypePanel
		]
		parent = root + "_ButtonPanel"
		screen.setTableLayout(parent, table)
		
		screen.newPanel(parent, root + "_SourcePanel")
		screen.newPanel(parent, root + "_TypePanel")
		screen.setHFlowLayout(root + "_SourcePanel")
		screen.setHFlowLayout(root + "_TypePanel")
		
		# set up the vote selection texts
		sText = gc.getVoteSourceInfo(iActiveVote).getSecretaryGeneralText()
		if self.VoteType == 1:
			sText = BugUtil.colorText(sText, "COLOR_YELLOW")
		screen.newActionButton(root + "_TypePanel", root + "_SecretaryGeneralButton1", WidgetTypes.WIDGET_PYTHON, kPythonVoteTypeButton, 1)
		screen.setActionButtonLabel(root + "_SecretaryGeneralButton1", sText)

		sText = localText.getText("TXT_KEY_BUG_VICTORY_DIPLOMATIC", ())
		if self.VoteType == 2:
			sText = BugUtil.colorText(sText, "COLOR_YELLOW")
		screen.newActionButton(root + "_TypePanel", root + "_SecretaryGeneralButton2", WidgetTypes.WIDGET_PYTHON, kPythonVoteTypeButton, 2)
		screen.setActionButtonLabel(root + "_SecretaryGeneralButton2", sText)

		if iRelVote != -1 and iUNVote != -1:  # both AP and UN are active
			sText = gc.getVoteSourceInfo(iRelVote).getDescription()
			if iActiveVote == iRelVote:
				sText = BugUtil.colorText(sText, "COLOR_YELLOW")
			screen.newActionButton(root + "_SourcePanel", root + "_SourceButtonRel", WidgetTypes.WIDGET_PYTHON, kPythonVoteSourceButton, iRelVote)
			screen.setActionButtonLabel(root + "_SourceButtonRel", sText)

			sText = gc.getVoteSourceInfo(iUNVote).getDescription()
			if iActiveVote == iUNVote:
				sText = BugUtil.colorText(sText, "COLOR_YELLOW")
			screen.newActionButton(root + "_SourcePanel", root + "_SourceButtonUN", WidgetTypes.WIDGET_PYTHON, kPythonVoteSourceButton, iUNVote)
			screen.setActionButtonLabel(root + "_SourceButtonUN", sText)

	def rebuildMembersScreen_NonBUG(self, screen, root):
		activePlayer = gc.getPlayer(self.iActivePlayer)
		iActiveTeam = activePlayer.getTeam()
		
		rows = []

		for i in range(gc.getNumVoteSourceInfos()):
			if gc.getGame().isDiploVote(i):
				kVoteSource = gc.getVoteSourceInfo(i)
				if len(rows):
					rows.append([u" "] * 2)
				row = [kVoteSource.getDescription().upper(), u""]
				if (gc.getGame().getVoteSourceReligion(i) != -1):
					row[1] = gc.getReligionInfo(gc.getGame().getVoteSourceReligion(i)).getDescription()
				rows.append(row)

				iSecretaryGeneralVote = -1
				if (gc.getGame().canHaveSecretaryGeneral(i) and -1 != gc.getGame().getSecretaryGeneral(i)):
					for j in range(gc.getNumVoteInfos()):
						print j
						if gc.getVoteInfo(j).isVoteSourceType(i):
							print "votesource"
							if gc.getVoteInfo(j).isSecretaryGeneral():
								print "secgen"
								iSecretaryGeneralVote = j
								break

				print iSecretaryGeneralVote
				for j in range(gc.getMAX_PLAYERS()):
					if gc.getPlayer(j).isAlive() and not gc.getPlayer(j).isBarbarian() and gc.getTeam(iActiveTeam).isHasMet(gc.getPlayer(j).getTeam()):
						szPlayerText = gc.getPlayer(j).getName()
						if (-1 != iSecretaryGeneralVote):
							szPlayerText += localText.getText("TXT_KEY_VICTORY_SCREEN_PLAYER_VOTES", (gc.getPlayer(j).getVotes(iSecretaryGeneralVote, i), )) 
						if (gc.getGame().canHaveSecretaryGeneral(i) and gc.getGame().getSecretaryGeneral(i) == gc.getPlayer(j).getTeam()):
							rows.append((szPlayerText, gc.getVoteSourceInfo(i).getSecretaryGeneralText()))
						elif (gc.getPlayer(j).isFullMember(i)):
							rows.append((szPlayerText, localText.getText("TXT_KEY_VOTESOURCE_FULL_MEMBER", ())))
						elif (gc.getPlayer(j).isVotingMember(i)):
							rows.append((szPlayerText, localText.getText("TXT_KEY_VOTESOURCE_VOTING_MEMBER", ())))

		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0)]
		table.rows = [TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)),
			TableLayoutConfigCell(ivec2(1, 0)),
		]
		table.gap = ivec2(1, 0)
		screen.setTableLayout(root, table)
		
		for a, b in rows:
			screen.newLabel(root, self.getNextWidgetName(), a)
			screen.newLabel(root, self.getNextWidgetName(), b)

	def formatPercent(self, f):
		return "%.1f%%" % f

	def getVoteReq(self, i, iVote):
		iMaxVotes = 0
		iMinVotes = 999999
		for iLoop in range(gc.getNumVoteInfos()):
			iVoteReq = gc.getGame().getVoteRequired(iLoop, i)
			if iVoteReq > 0:
				if iVoteReq > iMaxVotes:
					iMaxVotes = iVoteReq
				if iVoteReq < iMinVotes:
					iMinVotes = iVoteReq

		if iVote == 1:
			return iMinVotes
		else:
			return iMaxVotes

	def getCandStatusName(self, iCand):
		# iCand is a team
		if iCand == -1: # there is no candidate
			return -1, "-"

		activePlayer = gc.getPlayer(self.iActivePlayer)
		iActiveTeam = activePlayer.getTeam()

		if iActiveTeam == iCand:
			return 1, gc.getTeam(iCand).getName()

		if gc.getTeam(iActiveTeam).isHasMet(iCand):
			return 1, gc.getTeam(iCand).getName()
		else:
			return 0, localText.getText("TXT_KEY_TOPCIVS_UNKNOWN", ())

	def getPlayerStatusName(self, iPlayer):
		if iPlayer == -1: # there is no player
			return False, "-"

		pPlayer = gc.getPlayer(iPlayer)
		iPlayerTeam = pPlayer.getTeam()
		activePlayer = gc.getPlayer(self.iActivePlayer)
		iActiveTeam = activePlayer.getTeam()

		if iActiveTeam == iPlayerTeam:
			return True, pPlayer.getName()

		if gc.getTeam(iActiveTeam).isHasMet(iPlayerTeam):
			return True, pPlayer.getName()
		else:
			return False, localText.getText("TXT_KEY_TOPCIVS_UNKNOWN", ())

	def getVoteTotalColor(self, iVoteReq, iVoteTotal, iVoteCand, bWinner, bVictoryVote):
		print "%i %i %i" % (iVoteReq, iVoteTotal, iVoteCand)
		if not bWinner:
			return -1
		if (iVoteCand > iVoteReq
		and bVictoryVote):
			return ColorUtil.keyToType("COLOR_RED")
		if iVoteTotal > iVoteReq:
			return ColorUtil.keyToType("COLOR_GREEN")
		return -1


	def getBuffyWarningText(self):
		if self.BuffyWarningTextLoaded:
			return
		else:
			self.BuffyWarningTextLoaded = True

		self.BuffyWarning = BugUtil.getText("TXT_KEY_BUFFYWARNING")
		self.BuffyWarningNotValid = BugUtil.getText("TXT_KEY_BUFFYWARNING_NOT_VALID")
		self.BuffyWarningNoTeams = BugUtil.getText("TXT_KEY_BUFFYWARNING_NO_TEAMS")
		self.BuffyWarningMac = BugUtil.getText("TXT_KEY_BUFFYWARNING_MAC")
		self.BuffyWarningInstallLocation = BugUtil.getText("TXT_KEY_BUFFYWARNING_INSTALL_LOCATION")
		self.BuffyWarningMultiPlayerNotAllowed = BugUtil.getText("TXT_KEY_BUFFYWARNING_MULTI_PLAYER_NOT_ALLOWED")
		self.BuffyWarningNoDll = BugUtil.getText("TXT_KEY_BUFFYWARNING_DLL_MISSING")
		self.BuffyWarningSkippedCheckSum = BugUtil.getText("TXT_KEY_BUFFYWARNING_CHECKSUM_SKIPPED")
		self.BuffyWarningCheckSumMissing = BugUtil.getText("TXT_KEY_BUFFYWARNING_CHECKSUM_MISSING")
		self.BuffyWarningCheckSumDifferent = BugUtil.getText("TXT_KEY_BUFFYWARNING_CHECKSUM_DIFFERENT")
		self.BuffyWarningCheckSumFailed = BugUtil.getText("TXT_KEY_BUFFYWARNING_CHECKSUM_FAILED")
		self.BuffyWarningVictoryConditions = BugUtil.getText("TXT_KEY_BUFFYWARNING_VICTORY_CONDITIONS")
		self.BuffyWarningRequired = BugUtil.getText("TXT_KEY_BUFFYWARNING_REQUIRED")
		self.BuffyWarningNotAllowed = BugUtil.getText("TXT_KEY_BUFFYWARNING_NOT_ALLOWED")
		self.BuffyWarningBalancedResoucesNotAllowed = BugUtil.getText("TXT_KEY_BUFFYWARNING_BALANCED_RESOUCES_NOT_ALLOWED")

	
# BUG Additions Start
#	def getListCultureCities(self, iPlayer):
	def getListCultureCities(self, iPlayer, victory):
		maxCityCulture = GameUtil.getCultureThreshold(victory.getCityCulture())
# BUG Additions End

		if iPlayer >= 0:
			player = PyPlayer(iPlayer)
			if player.isAlive():
				cityList = player.getCityList()
# BUG Additions Start
#				listCultureCities = len(cityList) * [(0, 0)]
				listCultureCities = len(cityList) * [(0, 0, 0)]
# BUG Additions End
				i = 0
				for city in cityList:
# BUG Additions Start
					pCity = city.GetCy()
					iRate = pCity.getCommerceRateTimes100(CommerceTypes.COMMERCE_CULTURE)
					if iRate == 0:
						iTurns = -1
					else:
						iCultureLeftTimes100 = 100 * maxCityCulture - pCity.getCultureTimes100(city.getOwner())
						iTurns = int((iCultureLeftTimes100 + iRate - 1) / iRate)
					listCultureCities[i] = (city.getCulture(), city, iTurns)
#					listCultureCities[i] = (city.getCulture(), city)
# BUG Additions Start
					i += 1
				listCultureCities.sort()
				listCultureCities.reverse()
				return listCultureCities
		return []

# BUG Additions Start
	def getVotesForWhichCandidate(self, iPlayer, iCand1, iCand2, iVote):
		# returns are 1 = vote for candidate 1
		#             2 = vote for candidate 2
		#            -1 = abstain

		# iVote = 1 means vote for SecGen or Pope
		# iVote = 2 means vote for diplomatic victory
		
		# fortsnek: Bug fix: We now handle the case where one of the candidates is -1 (all checks for NO_TEAM)
		#           Axolotl4 AD-1020 Single religion palace test.CivBeyondSwordSave
		#           CvPlayerAI::AI_diploVote

		# candidates are teams!!!

		# * AI votes for itself if it can
		# * AI votes for a team member if it can
		# * AI votes for its master, if it is a vassal
		# * if the AI attitude to one of the candidates is 'friendly' and the other is 'pleased' or less, AI votes for 'friend'
		# * if both candidates are at 'friendly' status, votes for one with highest attitude
		# * if neither candidate is at 'friendly', abstains

		iPTeam = gc.getPlayer(iPlayer).getTeam()
		iPCand1 = self.getPlayerOnTeam(iCand1)
		iPCand2 = self.getPlayerOnTeam(iCand2)

		# * player votes for its own team if it can
		if iPTeam == iCand1: return 1
		if iPTeam == iCand2: return 2

		# if player is human, votes for self or abstains
		if iPlayer == self.iActivePlayer: return -1

		# * AI votes for its master, if it is a vassal
		if iCand1 != TeamTypes.NO_TEAM and gc.getTeam(iPTeam).isVassal(iCand1): return 1
		if iCand2 != TeamTypes.NO_TEAM and gc.getTeam(iPTeam).isVassal(iCand2): return 2

		# get player category (friendly) to candidates
		iC1Cat = AttitudeUtil.getAttitudeCategory(iPlayer, iPCand1) if iPCand1 != TeamTypes.NO_TEAM else -1 # fortsnek: -1 is below furious
		iC2Cat = AttitudeUtil.getAttitudeCategory(iPlayer, iPCand2) if iPCand2 != TeamTypes.NO_TEAM else -1 

		# the cut-off for SecGen votes is pleased (3)
		# the cut-off for Diplo victory votes is friendly (4)
		if iVote == 1:  # vote for SecGen or Pope
			iCutOff = 3
		else:
			iCutOff = 4

		# * if neither candidate is at 'friendly', abstains
		# assumes friendly = 4, pleased = 3, etc
		if (iC1Cat < iCutOff
		and iC2Cat < iCutOff):
			return -1

		# * if the AI attitude to one of the candidates is 'friendly' and the other is 'pleased' or less, AI votes for 'friend'
		if (iC1Cat >= iCutOff
		and iC1Cat > iC2Cat):
			return 1

		if (iC2Cat >= iCutOff
		and iC2Cat > iC1Cat):
			return 2

		# if the code gets to here, then both candidates are at or above the cutoff
		# and they are both at the same category (ie both friendly)
		# need to decide on straight attitude (visible only)

		# get player attitude to candidates
		iC1Att = AttitudeUtil.getAttitudeCount(iPlayer, iPCand1) if iPCand1 != TeamTypes.NO_TEAM else -9999
		iC2Att = AttitudeUtil.getAttitudeCount(iPlayer, iPCand2) if iPCand2 != TeamTypes.NO_TEAM else -9999

		# * if both candidates are at 'friendly' status, votes for one with highest attitude
		if iC2Att < iC1Att: # ties go to Candidate #1
			return 1
		else:
			return 2

		return -1

	def getPlayerOnTeam(self, iTeam):
		for i in range(gc.getMAX_PLAYERS()):
			if iTeam == gc.getPlayer(i).getTeam():
				return i

		return -1

	def getAP_UN_OwnerTeam(self):
		for i in range(gc.getNumBuildingInfos()):
			for j in range(gc.getNumVoteSourceInfos()):
				if (gc.getBuildingInfo(i).getVoteSourceType() == j):
					for iLoopTeam in range(gc.getMAX_CIV_TEAMS()):
						if (gc.getTeam(iLoopTeam).isAlive() and not gc.getTeam(iLoopTeam).isMinorCiv() and not gc.getTeam(iLoopTeam).isBarbarian()):
							if (gc.getTeam(iLoopTeam).getBuildingClassCount(gc.getBuildingInfo(i).getBuildingClassType()) > 0):
								return iLoopTeam
								break
		return -1

	def canBuildSSComponent(self, vTeam, vComponent):
		if(not vTeam.isHasTech(vComponent.getTechPrereq())):
			return False
		else:
			for j in range(gc.getNumProjectInfos()):
				if(vTeam.getProjectCount(j) < vComponent.getProjectsNeeded(j)):
					return False
		return True

	def isApolloBuilt(self):
		activePlayer = gc.getPlayer(self.iActivePlayer)
		iActiveTeam = activePlayer.getTeam()

		# check if anyone has built the apollo project (PROJECT_APOLLO_PROGRAM)
		for iLoopTeam in range(gc.getMAX_CIV_TEAMS()):
			pLoopTeam = gc.getTeam(iLoopTeam)
			if (pLoopTeam.isAlive()
			and not pLoopTeam.isMinorCiv()
			and not pLoopTeam.isBarbarian()):
				if iLoopTeam == iActiveTeam:
					bContact = True
				elif (gc.getTeam(iActiveTeam).isHasMet(iLoopTeam)
				or gc.getGame().isDebugMode()):
					bContact = True
				else:
					bContact = False

				if bContact:
					if self.isApolloBuiltbyTeam(pLoopTeam):
						return True
		return False

	def isApolloBuiltbyTeam(self, vTeam):
		iTeam = vTeam.getID()
#		print vTeam.getName()

		if iTeam in self.ApolloTeamsChecked:
			sString = "1: %s" % (self.ApolloTeamCheckResult[iTeam])
#			print sString
#			return self.ApolloTeamCheckResult[iTeam]

		for i in range(gc.getNumProjectInfos()):
			component = gc.getProjectInfo(i)
			if (component.isSpaceship()):
				bApollo = True
				for j in range(gc.getNumProjectInfos()):
					if(vTeam.getProjectCount(j) < component.getProjectsNeeded(j)):
						bApollo = False
#					sString = "2: %s %s %i %i %s" % (component.getDescription(), gc.getProjectInfo(j).getDescription(), vTeam.getProjectCount(j), component.getProjectsNeeded(j), bApollo)
#					print sString

#				sString = "2: %s %s" % (component.getDescription(), bApollo)
#				print sString
				if bApollo:
					self.ApolloTeamCheckResult[iTeam] = True
					self.ApolloTeamsChecked.add(iTeam)
					return True
				break

		self.ApolloTeamCheckResult[iTeam] = False
		self.ApolloTeamsChecked.add(iTeam)
		return False
# BUG Additions End

	# returns a unique ID for a widget in this screen
	def getNextWidgetName(self):
		szName = self.ANON_WIDGET_ID + str(self.nWidgetCount)
		self.nWidgetCount += 1
		return szName

	# handle the input for this screen...
	def handleInput (self, inputClass):
		func = inputClass.iData1
		if (inputClass.getNotifyCode() == NotifyCode.NOTIFY_CLICKED):
			screen = self.getScreen()
			if func == kPythonVoteSourceButton:
				self.VoteBody = inputClass.iData2
				self.rebuildMembersScreen(screen, "Page" + str(UN_MEMBERS_SCREEN))
			elif func == kPythonVoteTypeButton:
				self.VoteType = inputClass.iData2
				self.rebuildMembersScreen(screen, "Page" + str(UN_MEMBERS_SCREEN))
			elif func == kPythonTabButton:
				self.iScreen = inputClass.iData2
				self.switchPage(screen)
			elif func == kPythonSpaceshipScreenButton:
				#close screen
				screen.hideScreen()
				CyInterface().clearSelectedCities()
		
				#popup spaceship screen
				popupInfo = CyPopupInfo()
				popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON_SCREEN)
				popupInfo.setData1(-1)
				popupInfo.setText(u"showSpaceShip")
				popupInfo.addPopup(self.iActivePlayer)
		return 0

	def update(self, fDelta):
		return
