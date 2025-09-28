## Sid Meier's Civilization 4
## Copyright Firaxis Games 2005

## fortsnek: Adapted for Cv4MiniEngine.

from CvPythonExtensions import *
import PyHelpers
import ScreenInput
import CvScreenEnums
import CvUtil
import CvGameUtils
import CvScreensInterface

# globals
gc = CyGlobalContext()
localText = CyTranslator()

class CvDanQuayle:

	def __init__(self):
		self.SCREEN_NAME = "CvDanQuayle"
		self.WIDGET_ID = "CvDanQuayleWidget"
		
		self.leaders = ["TXT_KEY_DQ_LEADER_NAME_1", 
						"TXT_KEY_DQ_LEADER_NAME_2",
						"TXT_KEY_DQ_LEADER_NAME_3",
						"TXT_KEY_DQ_LEADER_NAME_4",
						"TXT_KEY_DQ_LEADER_NAME_5",
						"TXT_KEY_DQ_LEADER_NAME_6",
						"TXT_KEY_DQ_LEADER_NAME_7",
						"TXT_KEY_DQ_LEADER_NAME_8",
						"TXT_KEY_DQ_LEADER_NAME_9",
						"TXT_KEY_DQ_LEADER_NAME_10",
						"TXT_KEY_DQ_LEADER_NAME_11",
						"TXT_KEY_DQ_LEADER_NAME_12",
						"TXT_KEY_DQ_LEADER_NAME_13",
						"TXT_KEY_DQ_LEADER_NAME_14",
						"TXT_KEY_DQ_LEADER_NAME_15",
						"TXT_KEY_DQ_LEADER_NAME_16",
						"TXT_KEY_DQ_LEADER_NAME_17",
						"TXT_KEY_DQ_LEADER_NAME_18",
						"TXT_KEY_DQ_LEADER_NAME_19",
						"TXT_KEY_DQ_LEADER_NAME_20"]

		self.nWidgetCount = 0

	def getScreen(self):
		return CyGInterfaceScreen(self.SCREEN_NAME, CvScreenEnums.DAN_QUAYLE_SCREEN)

	def interfaceScreen (self):

		replayInfo = CyGame().getReplayInfo()
		if replayInfo.isNone():
			replayInfo = CyReplayInfo()
			replayInfo.createInfo(gc.getGame().getActivePlayer())
		
		screen = self.getScreen()
		if screen.isActive():
			return
		#screen.setRenderInterfaceOnly(True);
		screen.setInitialTitle(localText.getText("TXT_KEY_GAME_END_SCREEN_TITLE", ()).upper())
	
		iScore = replayInfo.getNormalizedScore()
		iMaxScore = ((100 + gc.getDefineINT("SCORE_VICTORY_PERCENT")) * (gc.getDefineINT("SCORE_POPULATION_FACTOR") + gc.getDefineINT("SCORE_LAND_FACTOR") + gc.getDefineINT("SCORE_WONDER_FACTOR") + gc.getDefineINT("SCORE_TECH_FACTOR"))) / 100
		if iMaxScore > 0:
			iNormalScore = iScore/float(iMaxScore)
		else:
			iNormalScore = 0
			
		if iNormalScore > 1.5:
			szLeaderText = self.leaders[0]
		elif iNormalScore > 1.4:
			szLeaderText = self.leaders[1]
		elif iNormalScore > 1.3:
			szLeaderText = self.leaders[2]
		elif iNormalScore > 1.2:
			szLeaderText = self.leaders[3]
		elif iNormalScore > 1.1:
			szLeaderText = self.leaders[4]
		elif iNormalScore > 1.05:
			szLeaderText = self.leaders[5]
		elif iNormalScore > 1.0:
			szLeaderText = self.leaders[6]
		elif iNormalScore > 0.95:
			szLeaderText = self.leaders[7]
		elif iNormalScore > 0.9:
			szLeaderText = self.leaders[8]
		elif iNormalScore > 0.85:
			szLeaderText = self.leaders[9]
		elif iNormalScore > 0.8:
			szLeaderText = self.leaders[10]
		elif iNormalScore > 0.75:
			szLeaderText = self.leaders[11]
		elif iNormalScore > 0.7:
			szLeaderText = self.leaders[12]
		elif iNormalScore > 0.65:
			szLeaderText = self.leaders[13]
		elif iNormalScore > 0.6:
			szLeaderText = self.leaders[14]
		elif iNormalScore > 0.55:
			szLeaderText = self.leaders[15]
		elif iNormalScore > 0.5:
			szLeaderText = self.leaders[16]
		elif iNormalScore > 0.4:
			szLeaderText = self.leaders[17]
		elif iNormalScore > 0.3:
			szLeaderText = self.leaders[18]
		else:
			szLeaderText = self.leaders[19]

		root = ""
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # DQTextLabel
			TableLayoutConfigCell(ivec2(0, 1)), # ScoreLabel
			TableLayoutConfigCell(ivec2(0, 2)), # LeaderList
			TableLayoutConfigCell(ivec2(0, 3), ivec2(1, 1), RectJustilign(EJustilign.End, EJustilign.Stretch)), # ExitButton
		]
		table.gap = ivec2(0, 1)
		screen.setTableLayout(root, table)
		
		screen.newLabel(root, "DQTextLabel", localText.getText("TXT_KEY_DQ_TEXT_STRING", (replayInfo.getLeaderName(), szLeaderText, )))
		screen.newLabel(root, "ScoreLabel", u"<font=4>" + localText.getObjectText("TXT_KEY_VICTORY_SCORE", 0) + u" : " + unicode(iScore) + u"</font>")
		screen.newLabel(root, "LeaderList", u'\n'.join([localText.getText(name, ()) if name != szLeaderText else localText.getColorText(name, (), gc.getInfoTypeForString("COLOR_YELLOW")) for name in self.leaders]))
		screen.newActionButton(root, "ExitButton", WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)
			
		screen.showScreen( PopupStates.POPUPSTATE_IMMEDIATE, False)
			

	# returns a unique ID for a widget in this screen
	def getNextWidgetName(self):
		szName = self.WIDGET_ID + str(self.nWidgetCount)
		self.nWidgetCount += 1
		return szName

			
	# Will handle the input for this screen...
	def handleInput (self, inputClass):
		return 0

	def update(self, fDelta):
		return
