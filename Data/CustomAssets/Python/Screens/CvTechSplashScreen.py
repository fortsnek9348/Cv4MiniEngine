from CvPythonExtensions import *
import CvUtil
import ScreenInput
import string

localText = CyTranslator()

class CvTechSplashScreen:
	"Splash screen for techs"
	def __init__(self, iScreenID):
		self.nScreenId = iScreenID
		self.iTech = -1
		self.SCREEN_NAME = "TechSplashScreen"
		
	def interfaceScreen(self, iTech):
		self.nTechs = CyGlobalContext().getNumTechInfos()
		self.iTech = iTech
		
		# Create screen
		
		screen = self.getScreen()
		
		techInfo = CyGlobalContext().getTechInfo(self.iTech)
		
	
		
		# The window:
		
		# ---------------------------------------------
		#                    ALPHABET
		# ---------------------------------------------
		# Tech Icon / Tech Quote
		# ---------------------------------------------
		# Help Desc
		# ---------------------------------------------
		# Units: 
		# Buildings: 
		# Improvements: 
		# Leads To: 
		# ---------------------------------------------
		#                   [ Continue ]
		
		screen.setInitialTitle(techInfo.getDescription().upper())
		
		root = ""
		
		#table = TableLayoutConfig()
		#table.cols = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 1)]
		#table.rows = [TableLayoutConfigRowColDesc(0, 0)] + [TableLayoutConfigRowColDesc(0, 1)] * 3
		#table.cells = [
		#	TableLayoutConfigCell(ivec2(0, 0), ivec2(2, 1)), # quote
		#	TableLayoutConfigCell(ivec2(0, 1), ivec2(1, 2)), # special
		#	TableLayoutConfigCell(ivec2(0, 3), ivec2(1, 1)), # leads
		#	TableLayoutConfigCell(ivec2(1, 1), ivec2(1, 1)), # units
		#	TableLayoutConfigCell(ivec2(1, 2), ivec2(1, 1)), # buildings
		#	TableLayoutConfigCell(ivec2(1, 3), ivec2(1, 1)), # improvements
		#]
		#screen.setTableLayout(root, table)
		screen.setVFlowLayout(root)

		screen.newLabel(root, "QuoteLabel", techInfo.getQuote())
		# localText.getText("TXT_KEY_PEDIA_SPECIAL_ABILITIES", ())
		screen.newLabel(root, "HelpLabel", CyGameTextMgr().getTechHelp(self.iTech, True, False, False, True, -1)[1:])
		
		gc = CyGlobalContext()
		units = [unitI for unitI in (gc.getCivilizationInfo(gc.getGame().getActiveCivilizationType()).getCivilizationUnits(classI) for classI in range(gc.getNumUnitClassInfos()))
			if unitI != -1 and isTechRequiredForUnit(self.iTech, unitI)]
		# WidgetTypes.WIDGET_PEDIA_JUMP_TO_UNIT, eLoopUnit, 1, False )
		
		screen.newLabel(root, "UnitsLabel", localText.getText("TXT_KEY_PEDIA_UNITS_ENABLED", ()) + u": " + u", ".join(gc.getUnitInfo(unitI).getDescription() for unitI in units))
		#screen.newLabel(root, "LeadsLabel", localText.getText("TXT_KEY_PEDIA_SPECIAL_ABILITIES", ()) + u'\n' + CyGameTextMgr().getTechHelp(self.iTech, True, False, False, True, -1)[1:])

		# Exit Button
		screen.newActionButton(root, "Exit", WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)
		screen.setActionButtonLabel("Exit", localText.getText("TXT_KEY_SCREEN_CONTINUE", ()))
		
		screen.setSound(techInfo.getSound())
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)
		#screen.enableWorldSounds(False)

	# returns a unique ID for this screen
	def getScreen(self):
		screen = CyGInterfaceScreen(self.SCREEN_NAME + str(self.iTech), self.nScreenId)
		return screen
					
	def handleInput( self, inputClass ):
		if ( inputClass.getData() == int(InputTypes.KB_RETURN) ):
			self.getScreen().hideScreen()
			return 1
		return 0
		
	def update(self, fDelta):
		return

		
		
