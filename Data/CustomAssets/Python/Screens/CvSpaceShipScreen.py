## Sid Meier's Civilization 4
## Copyright Firaxis Games 2005

## fortsnek: Adapted for Cv4MiniEngine.

from CvPythonExtensions import *
import PyHelpers
import CvUtil
import ScreenInput
import CvScreenEnums
import string

PyPlayer = PyHelpers.PyPlayer
PyInfo = PyHelpers.PyInfo

# globals
gc = CyGlobalContext()
localText = CyTranslator()

kPython_AddComponentButton = 0
kPython_LaunchButton = 1

class CvSpaceShipScreen:
	"Spaceship Screen"
	def interfaceScreen (self, iFinishedProject):
		
		#create screen
		screen = CyGInterfaceScreen( "SpaceShipScreen", CvScreenEnums.SPACE_SHIP_SCREEN)
		
		#setup panel
		#screen.setAutoSizeBehaviour(HeckTuiAutoSizeBehaviour.Maximise)

		self.activeProject = iFinishedProject

		#main panel

		root = ""
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # TopInfo
			TableLayoutConfigCell(ivec2(0, 1), ivec2(1, 1), RectJustilign(EJustilign.Center, EJustilign.Center)), # MyLittleSpaceship
			TableLayoutConfigCell(ivec2(1, 1), ivec2(1, 1), RectJustilign(EJustilign.Stretch, EJustilign.Stretch)), # ProjectUI
		]
		table.gap = ivec2(1, 1)
		screen.setTableLayout(root, table)
		
		screen.newPanel(root, "TopInfoPanel")

		
		
		screen.setVFlowLayout("TopInfoPanel")
		
		#create list of spaceship components
		self.componentProjects = []
		self.spaceVictory = -1
		for i in range(gc.getNumProjectInfos()):
			component = gc.getProjectInfo(i)
			if (component.isSpaceship()):
				self.spaceVictory = component.getVictoryPrereq()
				self.componentProjects.append(i);
		
		screen.newLabel("TopInfoPanel", "FinishedLabel", u"")
		screen.newLabel("TopInfoPanel", "FinishedLabel2", u"")
		self.updateText(screen)
		
		# Cv4MiniEngine extension: Return CvTeam::hasLaunched.
		hasLaunched = screen.setSpaceShip(self.activeProject)         
		screen.newLabel(root, "MyLittleSpaceship", r"""
               =====|\=
             _      ||
            / \     ||
            | |=====||
           /| |\    ||
            | |     ||
            | |     ||
       __  /   \    ||
       |  / | | \   ||
       |  | | | |   ||
       |  | | | |   ||
       |___/\_/\____||
      |##############|
 ============================
""" if not hasLaunched else r"""  
             _
            / \            .
  .         | |
           /| |\     .
            | |     
   .        | |     
           /   \   .
 .        / | | \          .
          | | | |   
      .   | | | |   .
           /\ /\        .
           /\ /\  .
           \/ \/
""")
				
		#component panels
		self.numComponents = len(self.componentProjects)
		
		#screen.newScrollBarPanel(root, "ProjectUIScrollPanel", "ProjectUIPanel", HeckTuiAxis.Vertical)
		screen.newPanel(root, "ProjectUIPanel")
		
		#screen.setSpaceShip(self.activeProject)
		self.buildComponentPanel(screen, "ProjectUIPanel")
		
		#show spaceship screen
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)
						
		return 0
		
	def updateText(self, screen):
		#title text
		if(self.activeProject >= 0):
			# Adding a component.
			screen.setLabelText("FinishedLabel", "<color=255,255,0><font=4>" + localText.getText("TXT_KEY_WONDER_SCREEN_TEXT", (gc.getProjectInfo(self.activeProject).getDescription(),)) + "</font></color>")
			screen.setLabelText("FinishedLabel2", "<color=255,255,0><font=4>" + localText.getText("TXT_KEY_SPACE_SHIP_SCREEN_ADD_COMPONENT", ()) + "</font></color>")
		else:
			#check if landed on alpha centauri
			activeTeam = gc.getGame().getActiveTeam()
			victoryCountdown = gc.getTeam(activeTeam).getVictoryCountdown(self.spaceVictory)
			gameState = gc.getGame().getGameState()
			if(((gameState == GameStateTypes.GAMESTATE_EXTENDED) and (victoryCountdown > 0)) or (victoryCountdown == 0)):
				screen.setLabelText("FinishedLabel", "<color=255,255,0><font=4>" + localText.getText("TXT_KEY_SPACE_SHIP_ALPHA_CENTAURI", ()) + "</font></color>")
			else: #normal welcome
				screen.setLabelText("FinishedLabel", "<color=255,255,0><font=4>" + localText.getText("TXT_KEY_SPACE_SHIP_SCREEN_WELCOME", ()) + "</font></color>")
				screen.setLabelText("FinishedLabel2", "<color=255,255,0><font=4>" + localText.getText("TXT_KEY_SPACESHIP_CHANCE_OF_SUCCESS", (gc.getTeam(gc.getGame().getActiveTeam()).getLaunchSuccessRate(self.spaceVictory), )) + "</font></color>")
		
		
	def buildComponentPanel(self, screen, root):
		#self.removeComponentsPanel()
		
		screen.setVFlowLayout(root)
		
		screen.newPanel(root, "ComponentPanelsLayout")
		screen.newPanel(root, "LabelsLayout")
		screen.newPanel(root, "ButtonsLayout")
		
		screen.setHWrapLayout("ComponentPanelsLayout")
		screen.setVFlowLayout("LabelsLayout")
		screen.setHWrapLayout("ButtonsLayout")
		
		
		#check if already landed spaceship
		activeTeam = gc.getGame().getActiveTeam()
		victoryCountdown = gc.getTeam(activeTeam).getVictoryCountdown(self.spaceVictory)
		gameState = gc.getGame().getGameState()
		if(not (((gameState == GameStateTypes.GAMESTATE_EXTENDED) and (victoryCountdown > 0)) or (victoryCountdown == 0))):
				
			#loop through each panel
			for i in range(self.numComponents):
				index = self.componentProjects[i]
				component = gc.getProjectInfo(index)
				
				#panel color
				completed = gc.getTeam(activeTeam).getProjectCount(index)
				required = component.getVictoryMinThreshold(self.spaceVictory)
				colour = "COLOR_CITY_BLUE"
				if(completed >= required): #green
					colour = "COLOR_TECH_GREEN"
				else: #check if can build
					canBuild = True
					if(not gc.getTeam(activeTeam).isHasTech(component.getTechPrereq())):
						canBuild = False
					else:
						for j in range(gc.getNumProjectInfos()):
							if(gc.getTeam(activeTeam).getProjectCount(j) < component.getProjectsNeeded(j)):
								canBuild = False
								
					if(not canBuild): #grey
						colour = "COLOR_LIGHT_GREY"

				#panel
				panelName = "ComponentPanel" + str(i)
				screen.newBoxPanel("ComponentPanelsLayout", "ComponentBoxPanel" + str(i), panelName, HeckTuiBorderStyle.Double, HeckTuiColour.fromColourType(gc.getInfoTypeForString(colour)))
				screen.setVFlowLayout(panelName)
				screen.newLabel(panelName, panelName + "Desc", "<color=255,255,0><font=3b>" + component.getDescription() + "</font></color>")

				#completed
				totalAllowed = component.getMaxTeamInstances()
				screen.newLabel(panelName, panelName + "Completed", localText.getText("TXT_KEY_SPACE_SHIP_SCREEN_COMPLETED_LABEL", (completed, totalAllowed)))
				
				#required
				screen.newLabel(panelName, panelName + "Req", localText.getText("TXT_KEY_SPACE_SHIP_SCREEN_REQUIRED_LABEL", (required,)))
				
				#in production
				inProduction = gc.getTeam(activeTeam).getProjectMaking(index)
				screen.newLabel(panelName, panelName + "InProd", localText.getText("TXT_KEY_SPACE_SHIP_SCREEN_IN_PRODUCTION_LABEL", (inProduction,)))

				# Not doing this in Cv4MiniEngine.
				#type button if necessary
				#if(screen.spaceShipCanChangeType(index) and (victoryCountdown < 0)):
				#   screen.setButtonGFC("ComponentTypeButton" + str(i), localText.getText("TXT_KEY_SPACE_SHIP_SCREEN_TYPE_BUTTON", ()), "", xPosition + self.componentTypeXOffset, yPosition + self.componentTypeYOffset, self.componentTypeWidth, self.componentTypeHeight, WidgetTypes.WIDGET_GENERAL, self.TYPE_BUTTON, i, ButtonStyles.BUTTON_STYLE_STANDARD )
				
				#zoom button
				#if(victoryCountdown < 0):
				#   screen.setButtonGFC("ComponentZoomButton" + str(i), localText.getText("TXT_KEY_SPACE_SHIP_SCREEN_ZOOM_BUTTON", ()), "", xPosition + self.componentZoomXOffset, yPosition + self.componentZoomYOffset, self.componentZoomWidth, self.componentZoomHeight, WidgetTypes.WIDGET_GENERAL, self.ZOOM_BUTTON, i, ButtonStyles.BUTTON_STYLE_STANDARD )
				
				#add button
				if(index == self.activeProject):
					screen.newActionButton(panelName, panelName + "AddButton", WidgetTypes.WIDGET_PYTHON, kPython_AddComponentButton, i)
					screen.setActionButtonLabel(panelName + "AddButton", localText.getText("TXT_KEY_SPACE_SHIP_SCREEN_ADD_BUTTON", ()))
				
				#component model
				#modelType = gc.getTeam(activeTeam).getProjectDefaultArtType(index)
				#screen.addSpaceShipWidgetGFC("ComponentModel" + str(i), xPosition + self.componentModelXOffset, yPosition + self.componentModelYOffset, self.componentModelWidth, self.componentModelHeight, index, modelType, WidgetTypes.WIDGET_GENERAL, -1, -1)
				
			#launch button
			activeTeam = gc.getGame().getActiveTeam()
			if(victoryCountdown > 0):
				victoryDate = CyGameTextMgr().getTimeStr(gc.getGame().getGameTurn() + victoryCountdown, false)
				screen.newLabel("LabelsLayout", "ArrivalLabel1", "<color=255,255,0><font=3b>" + localText.getText("TXT_KEY_SPACE_SHIP_SCREEN_ARRIVAL", ()) + ": " + victoryDate + "</font></color>")
				screen.newLabel("LabelsLayout", "ArrivalLabel2", "<color=255,255,0><font=3b>" + localText.getText("TXT_KEY_REPLAY_SCREEN_TURNS", ()) + ": " + str(victoryCountdown) + "</font></color>")
			elif(gc.getTeam(gc.getGame().getActiveTeam()).canLaunch(self.spaceVictory)):
				delay = gc.getTeam(gc.getGame().getActiveTeam()).getVictoryDelay(self.spaceVictory)
				screen.newLabel("LabelsLayout", "LaunchLabel", "<color=255,255,0><font=3b>" + localText.getText("TXT_KEY_SPACE_SHIP_SCREEN_TRAVEL_TIME_LABEL", (delay,)) + "</font></color>")
				screen.newActionButton("ButtonsLayout", "LaunchButton", WidgetTypes.WIDGET_PYTHON, kPython_LaunchButton, -1)
				screen.setActionButtonLabel("LaunchButton", localText.getText("TXT_KEY_SPACE_SHIP_SCREEN_LAUNCH_BUTTON", ()))
			
						
		#exit button
		screen.newActionButton("ButtonsLayout", "ExitButton", WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)
		screen.setActionButtonLabel("ExitButton", localText.getText("TXT_KEY_PEDIA_SCREEN_EXIT", ()))
			
		
	# Will handle the input for this screen...
	def handleInput (self, inputClass):
		if(inputClass.getNotifyCode() == NotifyCode.NOTIFY_CLICKED):
			data1 = inputClass.getData1()
			data2 = inputClass.getData2()
			#if(data1 == self.TYPE_BUTTON):
			#   screen = CyGInterfaceScreen( "SpaceShipScreen", CvScreenEnums.SPACE_SHIP_SCREEN)
			#   xPosition = self.X_SCREEN + self.componentPanelXOffset
			#   yPosition = self.Y_SCREEN + self.componentPanelYOffset + data2 * self.componentPanelHeight
			#   index = self.componentProjects[data2]
			#   screen.spaceShipChangeType(index)
			#   activeTeam = gc.getGame().getActiveTeam()
			#   modelType = gc.getTeam(activeTeam).getProjectDefaultArtType(index)
			#   screen.addSpaceShipWidgetGFC("ComponentModel" + str(data2), xPosition + self.componentModelXOffset, yPosition + self.componentModelYOffset, self.componentModelWidth, self.componentModelHeight, index, modelType, WidgetTypes.WIDGET_GENERAL, -1, -1)
			#elif(data1 == self.ZOOM_BUTTON):
			#   screen = CyGInterfaceScreen( "SpaceShipScreen", CvScreenEnums.SPACE_SHIP_SCREEN)
			#   screen.spaceShipZoom(self.componentProjects[data2])
			if data1 == kPython_AddComponentButton:
				screen = CyGInterfaceScreen( "SpaceShipScreen", CvScreenEnums.SPACE_SHIP_SCREEN)
				#screen.spaceShipFinalize()
				
				#adjust interface
				screen.hide("ComponentPanel" + str(data2) + "AddButton")
				self.activeProject = -1
				#self.rebuildComponentPanel()
			elif data1 == kPython_LaunchButton:
				screen = CyGInterfaceScreen( "SpaceShipScreen", CvScreenEnums.SPACE_SHIP_SCREEN)
				screen.spaceShipLaunch()
				
				#self.removeComponentsPanel()
				
		return 0

	def update(self, fDelta):
		return

	def onClose(self):
		return
