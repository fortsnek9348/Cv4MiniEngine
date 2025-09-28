## Sid Meier's Civilization 4
## Copyright Firaxis Games 2005

## fortsnek: Adapted for Cv4MiniEngine.

import CvUtil
from CvPythonExtensions import *

localText = CyTranslator()
gc = CyGlobalContext()

class CvDawnOfMan:
	"Dawn of man screen"
	def __init__(self, iScreenID):
		self.iScreenID = iScreenID
	
	def interfaceScreen(self):
		'Use a popup to display the opening text'
		if ( CyGame().isPitbossHost() ):
			return
		
		
		self.player = gc.getPlayer(gc.getGame().getActivePlayer())
		self.EXIT_TEXT = localText.getText("TXT_KEY_SCREEN_CONTINUE", ())
		
		# Create screen
		
		screen = CyGInterfaceScreen( "CvDawnOfMan", self.iScreenID )		
		
		#screen.showWindowBackground( False )
		#screen.setDimensions(self.X_SCREEN, screen.centerY(self.Y_SCREEN), self.W_SCREEN, self.H_SCREEN)
		#screen.enableWorldSounds( false )
		
		root = ""
		
		# Create panels
		
		screen.setVFlowLayout(root)
		szNameText = "<color=255,255,0,255>" + u"<font=3b>" + gc.getLeaderHeadInfo(self.player.getLeaderType()).getDescription().upper() + u"</font>"
		szNameText += "\n- " + self.player.getCivilizationDescription(0) + " -\n"
		szNameText += u"<font=2>" + CyGameTextMgr().parseLeaderTraits(self.player.getLeaderType(), self.player.getCivilizationType(), True, False) + u"</font>"
		szNameText += u"</color>"
		screen.newLabel(root, "LeaderName", szNameText)
		screen.setLabelHAlign("LeaderName", EAlign.Center)
		
		screen.newLabel(root, "StartingTechs", localText.getText("TXT_KEY_FREE_TECHS", ()) + u":")

		for iTech in range(gc.getNumTechInfos()):
			if (gc.getCivilizationInfo(self.player.getCivilizationType()).isCivilizationFreeTechs(iTech)):
				screen.newLabel(root, "StartingTech" + str(iTech), gc.getTechInfo(iTech).getDescription())
				screen.setLabelWidgetData("StartingTech" + str(iTech), WidgetTypes.WIDGET_PEDIA_JUMP_TO_TECH, iTech, 1)

		self.Text_BoxText = CyGameTextMgr().parseCivInfos(self.player.getCivilizationType(), True)
		screen.newLabel(root, "CivInfo", self.Text_BoxText)

		# Main Body text
		szDawnTitle = u"<font=3>" + localText.getText("TXT_KEY_DAWN_OF_MAN_SCREEN_TITLE", ()).upper() + u"</font>"
		screen.newLabel(root, "DawnTitle", szDawnTitle)
		
		bodyString = localText.getText("TXT_KEY_DAWN_OF_MAN_TEXT", (CyGameTextMgr().getTimeStr(gc.getGame().getGameTurn(), false), self.player.getCivilizationAdjectiveKey(), self.player.getNameKey()))
		screen.newLabel(root, "BodyText", bodyString)

		screen.newActionButton(root, "ExitButton", WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)
		screen.setActionButtonLabel("ExitButton", self.EXIT_TEXT)
		
		#pActivePlayer = gc.getPlayer(CyGame().getActivePlayer())
		#pLeaderHeadInfo = gc.getLeaderHeadInfo(pActivePlayer.getLeaderType())
		#screen.setSoundId(CyAudioGame().Play2DSoundWithId(pLeaderHeadInfo.getDiploPeaceMusicScriptIds(0)))
		
		
		# POPUPSTATE_QUEUED is not implemented for screens, and I don't know what it means. Wait for the player to become turn-active?
		#screen.showScreen(PopupStates.POPUPSTATE_QUEUED, False)
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)
		
	def handleInput( self, inputClass ):
		return 0
	
	def update(self, fDelta):
		return
		
	def onClose(self):
		CyInterface().setSoundSelectionReady(True)		
		return 0
		