## Sid Meier's Civilization 4
## Copyright Firaxis Games 2005

## fortsnek: Adapted for Cv4MiniEngine

from CvPythonExtensions import *
import CvUtil
import ScreenInput
import CvScreenEnums

# globals
gc = CyGlobalContext()

class CvVictoryMovieScreen:

	def interfaceScreen (self, iVictory):
		game = CyGame()
		if ( game.isNetworkMultiPlayer() or game.isPitbossHost()):
			return
		
		if (iVictory == -1 or len(gc.getVictoryInfo(iVictory).getMovie()) == 0):
			return
		self.createMovieScreen(gc.getVictoryInfo(iVictory).getMovie())
	
	def createMovieScreen(self, movieArtDef):
		screen = CyGInterfaceScreen( "VictoryMovieScreen", CvScreenEnums.VICTORY_MOVIE_SCREEN )
		screen.setAutoSizeBehaviour(HeckTuiAutoSizeBehaviour.Maximise)
		screen.setInitialTitle(u"VictoryMovieScreen")
		#screen.setRenderInterfaceOnly(True)
		#screen.enableWorldSounds( false )
		#screen.showWindowBackground( False )
		

		# Play the movie
		movieFilePath = CyArtFileMgr().getMovieArtInfo(movieArtDef).getPath()
		#screen.playMovie( movieFilePath, -1, -1, -1, -1, 0)
		
		root = ""
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), RectJustilign(EJustilign.Stretch, EJustilign.Center)), # Label
			TableLayoutConfigCell(ivec2(0, 1), ivec2(1, 1), RectJustilign(EJustilign.End, EJustilign.Stretch)), # ExitButton
		]
		table.gap = ivec2(0, 0)
		screen.setTableLayout(root, table)
		screen.newLabel(root, "Label", movieFilePath)
		screen.setLabelHAlign("Label", EAlign.Center)
		screen.newActionButton(root, "ExitButton", WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)
		
		# Show the screen
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)		

	def closeScreen(self):
		screen = CyGInterfaceScreen( "VictoryMovieScreen", CvScreenEnums.VICTORY_MOVIE_SCREEN )
		screen.hideScreen()
		
	def hideScreen(self):
		screen = CyGInterfaceScreen( "VictoryMovieScreen", CvScreenEnums.VICTORY_MOVIE_SCREEN )
		screen.hideScreen()
						
	# Will handle the input for this screen...
	def handleInput (self, inputClass):
		#screen = CyGInterfaceScreen( "VictoryMovieScreen", CvScreenEnums.VICTORY_MOVIE_SCREEN )
		if (inputClass.getNotifyCode() == NotifyCode.NOTIFY_MOVIE_DONE or inputClass.getNotifyCode() == NotifyCode.NOTIFY_CLICKED):
			return self.hideScreen()
		return 0

	def update(self, fDelta):
		return

