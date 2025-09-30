## Sid Meier's Civilization 4
## Copyright Firaxis Games 2005

# fortsnek: Pretty much rewritten for Cv4MiniEngine

from CvPythonExtensions import *
import CvUtil
import ScreenInput
import CvScreenEnums
import math

# globals
gc = CyGlobalContext()
localText = CyTranslator()

class CvUnVictoryScreen:

	def __init__(self):
		self.iScreen = -1
		self.WIDGET_ID = "UNVictoryWidget"
		self.SCREEN_NAME = "UNVictory"
		self.EXIT_ID = "UNVictoryExitWidget"
		self.BACKGROUND_ID = "UNVictoryBackground"
								
	def killScreen(self):
		if (self.iScreen >= 0):
			screen = self.getScreen()
			screen.hideScreen()
			self.iScreen = -1
		return

	def getScreen(self):
		return CyGInterfaceScreen(self.SCREEN_NAME + str(self.iScreen), CvScreenEnums.UN_SCREEN)

	def interfaceScreen(self):
	
		self.EXIT_TEXT = u"<font=4>" + localText.getText("TXT_KEY_PEDIA_SCREEN_EXIT", ()).upper() + u"</font>"

		self.iActiveLeader = CyGame().getActivePlayer()
		
		screen = self.getScreen()
		if screen.isActive():
			return
		#screen.setRenderInterfaceOnly(True)

		table = TableLayoutConfig()
		table.cols = [
			TableLayoutConfigRowColDesc(0, 1)
		]
		table.rows = [
			TableLayoutConfigRowColDesc(0, 1),
			TableLayoutConfigRowColDesc(0, 0),
		]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), RectJustilign(EJustilign.Center, EJustilign.Center)), # footer Cancel
			TableLayoutConfigCell(ivec2(0, 1), ivec2(1, 1), RectJustilign(EJustilign.End, EJustilign.End)), # footer Exit
		]
		
		root = ""
		screen.setTableLayout(root, table)
		
		screen.newPanel(root, "ContentPanel")

		screen.newActionButton(root, self.EXIT_ID, WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)
		screen.setActionButtonLabel(self.EXIT_ID, localText.getText("TXT_KEY_PEDIA_SCREEN_EXIT", ()).upper())

		# Draw leader heads
		self.drawContents("ContentPanel")
		
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)
				
	# Drawing Leaderheads
	def drawContents(self, root):
	
		# Get the Players
		playerActive = gc.getPlayer(self.iActiveLeader)
		
		screen = self.getScreen()
		
		screen.setVFlowLayout(root)
		
		screen.newBoxPanel(root, "WinnerBox", "WinnerBoxClient", HeckTuiBorderStyle.Rounded, HeckTuiColour.Grey100)
		screen.setFillLayout("WinnerBoxClient")
		screen.newLabel("WinnerBoxClient", "LblWinner", playerActive.getName())
		screen.setLabelHAlign("LblWinner", EAlign.Center)

		# count the leaders
		# Count all other leaders
		for iPlayer in range(gc.getMAX_PLAYERS()):
			player = gc.getPlayer(iPlayer)
			if (player.isAlive() and iPlayer != self.iActiveLeader and not player.isBarbarian() and not player.isMinorCiv()):
				name = "LblPlayer" + str(iPlayer)
				screen.newLabel(root, name, player.getName())
				screen.setLabelHAlign(name, EAlign.Center)
		
		
	# Handles the input for this screen...
	def handleInput (self, inputClass):			 
		return 0

	def update(self, fDelta):
		return

