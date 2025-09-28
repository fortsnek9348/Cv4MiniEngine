## Sid Meier's Civilization 4
## Copyright Firaxis Games 2005

## fortsnek: Adapted for Cv4MiniEngine.

from CvPythonExtensions import *
import CvUtil
import ScreenInput
from CvScreenEnums import *
import CvReplayScreen
import CvScreensInterface

# globals
gc = CyGlobalContext()
localText = CyTranslator()

class CvHallOfFameScreen:
	"Top scores and more"

	def __init__(self, screenId):
		self.screenId = screenId
		self.SCREEN_NAME = "HallOfFameScreen"

		self.WIDGET_ID = "HallOfFameWidget"
		
		self.nWidgetCount = 0
				
		self.bAllowReplay = False
		
		
	def getScreen(self):
		return CyGInterfaceScreen(self.SCREEN_NAME, self.screenId)

	def hideScreen(self):
		screen = self.getScreen()
		screen.hideScreen()									
		
	# Screen construction function
	def interfaceScreen(self, bAllowReplay):
						
		# Create a new screen
		screen = self.getScreen()
		if screen.isActive():
			return
		#screen.setRenderInterfaceOnly(True)
		screen.setInitialTitle(localText.getText("TXT_KEY_HALL_OF_FAME_SCREEN_TITLE", ()).upper())
		
		#screen.setAlwaysShown(True)
	
		self.bAllowReplay = bAllowReplay
		self.iLeaderFilter = -1
		self.iHandicapFilter = -1
		self.iWorldFilter = -1
		self.iClimateFilter = -1
		self.iSeaLevelFilter = -1
		self.iEraFilter = -1
		self.iSpeedFilter = -1
		self.iVictoryFilter = -1
		if gc.getGame().isGameMultiPlayer():
			self.iMultiplayerFilter = 1
		else:
			self.iMultiplayerFilter = 0

		self.EXIT_TEXT = u"<font=4>" + localText.getText("TXT_KEY_PEDIA_SCREEN_EXIT", ()).upper() + u"</font>"

		self.hallOfFame = CyHallOfFameInfo()
		self.hallOfFame.loadReplays()

		# Set the background widget and exit button
		root = ""
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # Table
			TableLayoutConfigCell(ivec2(0, 1)), # ExitButton
		]
		screen.setTableLayout(root, table)
		

		
		self.drawContents(screen, root)
		screen.newActionButton(root, "ExitButton", WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)
		
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)
		
	def isDisplayed(self, replayInfo):
		return ((self.iLeaderFilter == -1 or self.iLeaderFilter == replayInfo.getLeader(replayInfo.getActivePlayer())) 
			and (self.iHandicapFilter == -1 or self.iHandicapFilter == replayInfo.getDifficulty()) 
			and (self.iWorldFilter == -1 or self.iWorldFilter == replayInfo.getWorldSize()) 
			and (self.iClimateFilter == -1 or self.iClimateFilter == replayInfo.getClimate()) 
			and (self.iSeaLevelFilter == -1 or self.iSeaLevelFilter == replayInfo.getSeaLevel()) 
			and (self.iEraFilter == -1 or self.iEraFilter == replayInfo.getEra()) 
			and (self.iSpeedFilter == -1 or self.iSpeedFilter == replayInfo.getGameSpeed()) 
			and (self.iVictoryFilter == -1 or self.iVictoryFilter == replayInfo.getVictoryType()) 
			and ((self.iMultiplayerFilter == 1) == replayInfo.isMultiplayer()))
		
		
	def drawContents(self, screen, root):
				
		
		
		#screen.addTableControlGFC(self.TABLE_ID, 10, 2, 2 * self.DROPDOWN_SPACING_Y + self.DROPDOWN_Y, 1018, 545, True, True, 16, 16, TableStyles.TABLE_STYLE_STANDARD);
		#screen.enableSelect(self.TABLE_ID, False)
		#screen.enableSort(self.TABLE_ID)
		#screen.setTableColumnHeader(self.TABLE_ID, 0, "", 20)
		#screen.setTableColumnHeader(self.TABLE_ID, 1, localText.getText("TXT_KEY_PITBOSS_LEADER", ()), 202)
		#screen.setTableColumnHeader(self.TABLE_ID, 2, localText.getText("TXT_KEY_NORMALIZED_SCORE", ()), 100)
		#screen.setTableColumnHeader(self.TABLE_ID, 3, localText.getText("TXT_KEY_HALL_OF_FAME_SORT_BY_DATE", ()), 88)
		#screen.setTableColumnHeader(self.TABLE_ID, 4, localText.getText("TXT_KEY_GAME_SCORE", ()), 100)
		#screen.setTableColumnHeader(self.TABLE_ID, 5, localText.getText("TXT_KEY_CONCEPT_VICTORY", ()), 100)
		#screen.setTableColumnHeader(self.TABLE_ID, 6, localText.getText("TXT_KEY_PITBOSS_DIFFICULTY", ()), 100)
		#screen.setTableColumnHeader(self.TABLE_ID, 7, localText.getText("TXT_KEY_HOF_SCREEN_SIZE", ()), 100)
		#screen.setTableColumnHeader(self.TABLE_ID, 8, localText.getText("TXT_KEY_HOF_SCREEN_STARTING_ERA", ()), 100)
		#screen.setTableColumnHeader(self.TABLE_ID, 9, localText.getText("TXT_KEY_HOF_SCREEN_GAME_SPEED", ()), 105)
		
		screen.newRichTextTable(root, "Table", [
			localText.getText("TXT_KEY_PITBOSS_LEADER", ()),
			localText.getText("TXT_KEY_NORMALIZED_SCORE", ()),
			localText.getText("TXT_KEY_HALL_OF_FAME_SORT_BY_DATE", ()),
			localText.getText("TXT_KEY_GAME_SCORE", ()),
			localText.getText("TXT_KEY_CONCEPT_VICTORY", ()),
			localText.getText("TXT_KEY_PITBOSS_DIFFICULTY", ()),
			localText.getText("TXT_KEY_HOF_SCREEN_SIZE", ()),
			localText.getText("TXT_KEY_HOF_SCREEN_STARTING_ERA", ()),
			localText.getText("TXT_KEY_HOF_SCREEN_GAME_SPEED", ()),
		], WidgetTypes.WIDGET_PYTHON, -1, -1)
		screen.setRichTextTableExpandColumn("Table", 0)



		# count the filtered replays

		self.tableRowIndexToReplayId = []
		infoList = []
		for i in range(self.hallOfFame.getNumGames()):
			replayInfo = self.hallOfFame.getReplayInfo(i)
			if self.isDisplayed(replayInfo):
				szVictory = u""
				if replayInfo.getVictoryType() <= 0:
					szVictory = localText.getText("TXT_KEY_NONE", ())
				else:
					szVictory = gc.getVictoryInfo(replayInfo.getVictoryType()).getDescription()
					
				iValue = -replayInfo.getNormalizedScore()

				infoList.append((iValue,
						localText.getText("TXT_KEY_LEADER_CIV_DESCRIPTION", (replayInfo.getLeaderName(), replayInfo.getShortCivDescription())),
						replayInfo.getNormalizedScore(),
						replayInfo.getFinalDate(),
						replayInfo.getFinalScore(), 
						szVictory,
						gc.getHandicapInfo(replayInfo.getDifficulty()).getDescription(),
						gc.getWorldInfo(replayInfo.getWorldSize()).getDescription(),
#						gc.getClimateInfo(replayInfo.getClimate()).getDescription(),
#						gc.getSeaLevelInfo(replayInfo.getSeaLevel()).getDescription(),
						gc.getEraInfo(replayInfo.getEra()).getDescription(),
						gc.getGameSpeedInfo(replayInfo.getGameSpeed()).getDescription(),
						replayInfo.getFinalTurn()
						))
				self.tableRowIndexToReplayId.append(i)
		#infoList.sort()
						
		for i, row in enumerate(infoList):
		
			screen.addRichTextTableRow("Table", [
				infoList[i][1],
				u"%d" % infoList[i][2],
				infoList[i][3],
				u"%d" % infoList[i][4],
				infoList[i][5],
				infoList[i][6],
				infoList[i][7],
				infoList[i][8],
				infoList[i][9],
			], [
				0,
				infoList[i][2],
				infoList[i][10],
				infoList[i][4],
				0,
				0,
				0,
				0,
				0,
			])
																				
	# handle the input for this screen...
	def handleInput (self, inputClass):
		#if (inputClass.getNotifyCode() == NotifyCode.NOTIFY_LISTBOX_ITEM_SELECTED):
		#	if (inputClass.getFunctionName() == self.LEADER_DROPDOWN_ID):			
		#		screen = self.getScreen()
		#		iIndex = screen.getSelectedPullDownID(self.LEADER_DROPDOWN_ID)
		#		self.iLeaderFilter = screen.getPullDownData(self.LEADER_DROPDOWN_ID, iIndex)
		#		self.drawContents()
		#	elif (inputClass.getFunctionName() == self.DIFFICULTY_DROPDOWN_ID):			
		#		screen = self.getScreen()
		#		iIndex = screen.getSelectedPullDownID(self.DIFFICULTY_DROPDOWN_ID)
		#		self.iHandicapFilter = screen.getPullDownData(self.DIFFICULTY_DROPDOWN_ID, iIndex)
		#		self.drawContents()
		#	elif (inputClass.getFunctionName() == self.MAPSIZE_DROPDOWN_ID):			
		#		screen = self.getScreen()
		#		iIndex = screen.getSelectedPullDownID(self.MAPSIZE_DROPDOWN_ID)
		#		self.iWorldFilter = screen.getPullDownData(self.MAPSIZE_DROPDOWN_ID, iIndex)
		#		self.drawContents()
		#	elif (inputClass.getFunctionName() == self.CLIMATE_DROPDOWN_ID):			
		#		screen = self.getScreen()
		#		iIndex = screen.getSelectedPullDownID(self.CLIMATE_DROPDOWN_ID)
		#		self.iClimateFilter = screen.getPullDownData(self.CLIMATE_DROPDOWN_ID, iIndex)
		#		self.drawContents()
		#	elif (inputClass.getFunctionName() == self.SEALEVEL_DROPDOWN_ID):			
		#		screen = self.getScreen()
		#		iIndex = screen.getSelectedPullDownID(self.SEALEVEL_DROPDOWN_ID)
		#		self.iSeaLevelFilter = screen.getPullDownData(self.SEALEVEL_DROPDOWN_ID, iIndex)
		#		self.drawContents()
		#	elif (inputClass.getFunctionName() == self.ERA_DROPDOWN_ID):			
		#		screen = self.getScreen()
		#		iIndex = screen.getSelectedPullDownID(self.ERA_DROPDOWN_ID)
		#		self.iEraFilter = screen.getPullDownData(self.ERA_DROPDOWN_ID, iIndex)
		#		self.drawContents()
		#	elif (inputClass.getFunctionName() == self.SPEED_DROPDOWN_ID):			
		#		screen = self.getScreen()
		#		iIndex = screen.getSelectedPullDownID(self.SPEED_DROPDOWN_ID)
		#		self.iSpeedFilter = screen.getPullDownData(self.SPEED_DROPDOWN_ID, iIndex)
		#		self.drawContents()
		#	elif (inputClass.getFunctionName() == self.VICTORY_DROPDOWN_ID):			
		#		screen = self.getScreen()
		#		iIndex = screen.getSelectedPullDownID(self.VICTORY_DROPDOWN_ID)
		#		self.iVictoryFilter = screen.getPullDownData(self.VICTORY_DROPDOWN_ID, iIndex)
		#		self.drawContents()
		#	elif (inputClass.getFunctionName() == self.MULTIPLAYER_DROPDOWN_ID):			
		#		screen = self.getScreen()
		#		iIndex = screen.getSelectedPullDownID(self.MULTIPLAYER_DROPDOWN_ID)
		#		self.iMultiplayerFilter = screen.getPullDownData(self.MULTIPLAYER_DROPDOWN_ID, iIndex)
		#		self.drawContents()
		#	elif (inputClass.getFunctionName() == self.SORT_DROPDOWN_ID):			
		#		screen = self.getScreen()
		#		iIndex = screen.getSelectedPullDownID(self.SORT_DROPDOWN_ID)
		#		self.iSortBy = screen.getPullDownData(self.SORT_DROPDOWN_ID, iIndex)
		#		self.drawContents()
		#elif (inputClass.getNotifyCode() == NotifyCode.NOTIFY_CLICKED):
		#	if (inputClass.getFunctionName() == self.EXIT_ID):
		#		screen = self.getScreen()
		#		screen.hideScreen()
		#	elif (inputClass.getFunctionName() == self.REPLAY_BUTTON_ID and self.bAllowReplay):
		#		iRow = inputClass.getID()
		#		if iRow < len(self.infoList):
		#			CvScreensInterface.replayScreen.replayInfo = self.hallOfFame.getReplayInfo(self.infoList[iRow][10])
		#			CvScreensInterface.replayScreen.showScreen(True)
				
		if inputClass.getNotifyCode() == NotifyCode.NOTIFY_LISTBOX_ITEM_SELECTED:
			screen = self.getScreen()
			i = screen.getRichTextTableActiveRowIndex("Table")
			if i >= 0:
				CvScreensInterface.replayScreen.replayInfo = self.hallOfFame.getReplayInfo(self.tableRowIndexToReplayId[i])
				CvScreensInterface.replayScreen.showScreen(True)

		return 0

	def update(self, fDelta):
		return					



