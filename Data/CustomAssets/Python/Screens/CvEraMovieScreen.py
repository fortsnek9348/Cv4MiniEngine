## Sid Meier's Civilization 4
## Copyright Firaxis Games 2005

# fortsnek: Adapted for Cv4MiniEngine.

from CvPythonExtensions import *
import PyHelpers
import CvUtil
import ScreenInput
import CvScreenEnums
import string

#PyPlayer = PyHelpers.PyPlayer
#PyInfo = PyHelpers.PyInfo

# globals
gc = CyGlobalContext()
localText = CyTranslator()

class CvEraMovieScreen:
	"Wonder Movie Screen"
	def interfaceScreen (self, iEra):
		if (CyInterface().noTechSplash()):
			return 0
				
		#player = PyPlayer(CyGame().getActivePlayer())
			
		screen = CyGInterfaceScreen( "EraMovieScreen" + str(iEra), CvScreenEnums.ERA_MOVIE_SCREEN)

                		
		# Header...
		screen.setInitialTitle(localText.getText("TXT_KEY_ERA_SPLASH_SCREEN", (gc.getEraInfo(iEra).getTextKey(), )))

		root = ""
		screen.setVFlowLayout(root)
		
		# Play the movie
		if iEra == 1:
			szMovie = "Art/Movies/Era/Era01-Classical.dds"
		elif iEra == 2:
			szMovie = "Art/Movies/Era/Era02-Medeival.dds"
		elif iEra == 3:
			szMovie = "Art/Movies/Era/Era03-Renaissance.dds"
		elif iEra == 4:
			szMovie = "Art/Movies/Era/Era04-Industrial.dds"
		else:
			szMovie = "Art/Movies/Era/Era05-Modern.dds"

		screen.newLabel(root, "EraMovieMovie", szMovie)
		screen.setLabelHAlign("EraMovieMovie", EAlign.Center)
		
		screen.newActionButton(root, "ExitButton", WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)
		screen.setActionButtonLabel("ExitButton", localText.getText("TXT_KEY_MAIN_MENU_OK", ()))
		
		#screen.showWindowBackground(True)
		#screen.setRenderInterfaceOnly(False);
		screen.setSound("AS2D_NEW_ERA")
		#screen.showScreen(PopupStates.POPUPSTATE_MINIMIZED, False)
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)
				
		return 0
		
	# Will handle the input for this screen...
	def handleInput (self, inputClass):
		return 0

	def update(self, fDelta):
		return

