## Sid Meier's Civilization 4
## Copyright Firaxis Games 2005

## fortsnek: Adapted for Cv4MiniEngine.

from CvPythonExtensions import *
import CvUtil
import ScreenInput
import time
import re
import CvScreensInterface
import sys
import bisect

# globals
gc = CyGlobalContext()
localText = CyTranslator()

class CvReplayScreen:
	"Replay Screen for end of game"

	def __init__(self, screenId):
		self.screenId = screenId
		self.REPLAY_SCREEN_NAME = "ReplayScreen"
		
		self.WIDGET_ID = "Anon"
		self.nWidgetCount = 0
		
		self.replayInfo = None
		
	def setReplayInfo(self, replayInfo):
		self.replayInfo = replayInfo
		
	def getScreen(self):
		return CyGInterfaceScreen(self.REPLAY_SCREEN_NAME, self.screenId)

	def hideScreen(self):
		screen = self.getScreen()
		screen.hideScreen()	
		
	# Screen construction function
	def showScreen(self, bFromHallOfFame):
	
		# Create a new screen
		screen = self.getScreen()
		if screen.isActive():
			return
		#screen.setRenderInterfaceOnly(True);
		

		self.EXIT_TEXT = u"<font=4>" + localText.getText("TXT_KEY_PEDIA_SCREEN_EXIT", ()).upper() + u"</font>"

		self.iLastTurnShown = -1
		self.bFromHallOfFame = bFromHallOfFame
		self.bDone = False
		
		if not bFromHallOfFame:
			self.replayInfo = CyGame().getReplayInfo()
			if self.replayInfo.isNone():
				self.replayInfo = CyReplayInfo()
				self.replayInfo.createInfo(gc.getGame().getActivePlayer())
				
		self.iTurn = self.replayInfo.getInitialTurn()
		
		# Header...
		screen.setInitialTitle(localText.getText("TXT_KEY_REPLAY_SCREEN_TITLE", ()).upper())
		screen.setAutoSizeBehaviour(HeckTuiAutoSizeBehaviour.Maximise)
		
		root = ""
		
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # Content
			TableLayoutConfigCell(ivec2(0, 1)), # TimePanel
			TableLayoutConfigCell(ivec2(0, 2), ivec2(1, 1), RectJustilign(EJustilign.End, EJustilign.Center)), # Exit
		]
		table.gap = ivec2(1, 1)
		screen.setTableLayout(root, table)
		
		screen.newPanel(root, "Content")
		screen.newPanel(root, "TimePanel")
		screen.newActionButton(root, "ExitButton", WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)
		
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 2), TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 1)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # LeftContent
			TableLayoutConfigCell(ivec2(1, 0)), # EventLog
		]
		table.gap = ivec2(1, 1)
		screen.setTableLayout("Content", table)
		
		screen.newPanel("Content", "LeftContent")
		screen.newScrollBarPanel("Content", "EventLogScrollPanel", "EventLogPanel", HeckTuiAxis.Vertical)
		screen.setFillLayout("EventLogPanel")
		screen.newLabel("EventLogPanel", "EventLog", u"")
		screen.setLabelEnableWrapping("EventLog")
		
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 3), TableLayoutConfigRowColDesc(0, 1)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # Minimap
			TableLayoutConfigCell(ivec2(0, 1)), # Graph
		]
		table.gap = ivec2(1, 1)
		screen.setTableLayout("LeftContent", table)

		screen.newMinimap("LeftContent", "Minimap")
		screen.newLineGraph("LeftContent", "Graph")
		
		for playerI in range(self.replayInfo.getNumPlayers()):
			screen.addLineGraphSeries("Graph", [self.replayInfo.getPlayerScore(playerI, i) for i in range(self.replayInfo.getFinalTurn() + 1)],
				self.replayInfo.getColor(playerI))
		screen.setGraphProportionalMode("Graph")

		screen.setFillLayout("TimePanel")
		screen.newSlider("TimePanel", "TimeSlider", self.replayInfo.getFinalTurn())
			
		screen.setMinimapBaseTextureToReplayInfo("Minimap", self.replayInfo)
		#screen.updateMinimapSection(True, False)
		#screen.setMinimapMode(MinimapModeTypes.MINIMAPMODE_REPLAY)
		
		self.cultureChangesI = 0
		
		self.buildCultureLookup()

		self.buildText()
		self.updateUI(screen)
		
		#self.showEvents(self.iTurn, False)
		
		
		
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)
		
	def buildCultureLookup(self):
		
		self.cultureChangesTurnList = []
		self.cultureChangesData = []
		self.flashEventTurnList = []
		self.flashEventData = []
		
		#clearColour = -1 # gc.getInfoTypeForString("COLOR_CLEAR")
		
		replayInfo = self.replayInfo
		
		cultureMap = [[-1] * replayInfo.getMapWidth() for y in range(replayInfo.getMapHeight())]
	
		for iLoopEvent in range(replayInfo.getNumReplayMessages()):
			turn = replayInfo.getReplayMessageTurn(iLoopEvent)
			iX = replayInfo.getReplayMessagePlotX(iLoopEvent)
			iY = replayInfo.getReplayMessagePlotY(iLoopEvent)
			if replayInfo.getReplayMessageType(iLoopEvent) == ReplayMessageTypes.REPLAY_MESSAGE_PLOT_OWNER_CHANGE:
				iPlayer = replayInfo.getReplayMessagePlayer(iLoopEvent)
				newColour = replayInfo.getColor(iPlayer) if iPlayer != -1 else -1
				self.cultureChangesTurnList.append(turn)
				self.cultureChangesData.append((iX, iY, cultureMap[iY][iX], newColour))
				cultureMap[iY][iX] = newColour
			else:
				self.flashEventTurnList.append(turn)
				self.flashEventData.append((iX, iY))
				
	def updateUI(self, screen):
		iTurn = screen.getSliderValue("TimeSlider")

		screen.setLabelText("EventLog", u'\n'.join(reversed(self.lines[:self.turnLookup[iTurn] + 1])))
		
		screen.setLineGraphXRange("Graph", 0, iTurn)
		
		# Update minimap culture values. O(n) in the number of owner changes since the previous turn, bi-directional.
		cultureChangesData = self.cultureChangesData
		cultureChangesSourceIndex = self.cultureChangesI # the current culture map includes all change strictly before this index.
		cultureChangesTargetIndex = bisect.bisect_right(self.cultureChangesTurnList, iTurn)
		while cultureChangesSourceIndex < cultureChangesTargetIndex:
			iX, iY, oldColour, newColour = cultureChangesData[cultureChangesSourceIndex]
			screen.setMinimapPlotCulture("Minimap", iX, iY, newColour)
			cultureChangesSourceIndex += 1
		while cultureChangesTargetIndex < cultureChangesSourceIndex:
			iX, iY, oldColour, newColour = cultureChangesData[cultureChangesSourceIndex - 1]
			screen.setMinimapPlotCulture("Minimap", iX, iY, oldColour)
			cultureChangesSourceIndex -= 1
		self.cultureChangesI = cultureChangesSourceIndex
		
		screen.clearMinimapMarkers("Minimap")
		flashEventI = bisect.bisect_right(self.flashEventTurnList, iTurn) - 1
		colourI = gc.getInfoTypeForString("COLOR_WHITE")
		while flashEventI >= 0 and iTurn - self.flashEventTurnList[flashEventI] <= 10:
			iX, iY = self.flashEventData[flashEventI]
			screen.addMinimapMarker("Minimap", iX, iY, colourI, u'\u2588')
			flashEventI -= 1
		
	def buildText(self):
		lines = []
		turnLookup = [None] * (self.replayInfo.getFinalTurn() + 1)
		
		replayInfo = self.replayInfo
		calendar = replayInfo.getCalendar()
		startYear = replayInfo.getStartYear()
		speed = replayInfo.getGameSpeed()
		
		for iLoopEvent in range(replayInfo.getNumReplayMessages()):
			turn = replayInfo.getReplayMessageTurn(iLoopEvent)
			szEventDate = CyGameTextMgr().getDateStr(turn, False, calendar, startYear, speed)

			szText = replayInfo.getReplayMessageText(iLoopEvent)
			iX = replayInfo.getReplayMessagePlotX(iLoopEvent)
			iY = replayInfo.getReplayMessagePlotY(iLoopEvent)
			eMessageType = replayInfo.getReplayMessageType(iLoopEvent)
			eColor = replayInfo.getReplayMessageColor(iLoopEvent)

			if len(szText):
				turnLookup[turn] = len(lines)
				lines.append(localText.changeTextColor(u"<font=2>" + szEventDate + u": " + szText + u"</font>", eColor))

		for i in range(1, len(turnLookup)):
			if turnLookup[i] is None:
				turnLookup[i] = turnLookup[i - 1]
				
		self.lines = lines
		self.turnLookup = turnLookup
		
	
				
		
	
	# returns a unique ID for a widget in this screen
	def getNextWidgetName(self):
		szName = self.WIDGET_ID + str(self.nWidgetCount)
		self.nWidgetCount += 1
		return szName
		
		
	def initGraph(self):
		screen = self.getScreen()
		for iPlayer in range(self.replayInfo.getNumPlayers()):
			screen.addGraphLayer(self.szGraph, iPlayer, self.replayInfo.getColor(iPlayer));

		screen.setGraphLabelX(self.szGraph, localText.getText("TXT_KEY_REPLAY_SCREEN_TURNS", ()));
		screen.setGraphLabelY(self.szGraph, localText.getText("TXT_KEY_REPLAY_SCREEN_SCORE", ()));
		screen.setGraphYDataRange(self.szGraph, 0.0, 1.0);
		

	# handle the input for this screen...
	def handleInput (self, inputClass):
	
		#szWidgetName = inputClass.getFunctionName() + str(inputClass.getID())
		#
		#if (inputClass.getNotifyCode() == NotifyCode.NOTIFY_CLICKED):
		#	if (inputClass.getFunctionName() == self.EXIT_ID):
		#		screen = self.getScreen()
		#		screen.hideScreen()
		#
		#	elif (szWidgetName == self.szPlayId):
		#		self.setPlaying(not self.bPlaying)
		#		if self.bPlaying:
		#			if self.iTurn >= self.replayInfo.getFinalTurn():
		#				self.resetData()
		#				self.showEvents(self.replayInfo.getInitialTurn()-1, False)
		#			else:
		#				self.showEvents(self.iTurn + 1, False)
		#	elif (szWidgetName == self.szForwardId):
		#		if (not self.bPlaying):
		#			self.showEvents(self.iTurn + 1, False)
		if (inputClass.getNotifyCode() == NotifyCode.NOTIFY_SLIDER_NEWSTOP):
			self.updateUI(self.getScreen())
		
		return 0

	def update(self, fDelta):
	
		#screen = self.getScreen()
		#
		#screen.updateMinimap(fDelta)
		#
		#if self.bPlaying:
		#	if self.iTurn < self.replayInfo.getFinalTurn():
		#		self.fLastUpdate += max(fDelta, 0.02)
		#		iTurnJump = int(self.fLastUpdate / self.TIME_STEP[self.iSpeed - 1])
		#			
		#		if (iTurnJump > 0):
		#			iTurnJump = 1  # we used to allow showing multiple turns at once, but it didn't work very well
		#			self.fLastUpdate = 0.0
		#			self.showEvents(self.iTurn + iTurnJump, False)
		#
		#	elif self.iTurn >= self.replayInfo.getFinalTurn():
		#		self.setPlaying(False)
		#		self.fLastUpdate = 0.0
		
		return

