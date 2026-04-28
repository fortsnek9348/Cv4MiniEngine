## Sid Meier's Civilization 4
## Copyright Firaxis Games 2005

## fortsnek: Based off BUG CvMainInterface.py, with large parts deleted/replaced.

from CvPythonExtensions import *
import CvUtil
import ScreenInput
import CvScreenEnums
import CvEventInterface
import time
import itertools

# BUG - Options - start
kHasBUG = False
try:
	import BugCore
	kHasBUG = True
except ImportError:
	pass
	
if kHasBUG:
	import BugOptions
	import BugOptionsScreen
	import BugPath
	import BugUtil
	import CityUtil
	ClockOpt = BugCore.game.NJAGC
	ScoreOpt = BugCore.game.Scores
	MainOpt = BugCore.game.MainInterface
	CityScreenOpt = BugCore.game.CityScreen
	# BUG - Options - end

	# BUG - Limit/Extra Religions - start
	import ReligionUtil
	# BUG - Limit/Extra Religions - end

	# BUG - Align Icons - start
	import Scoreboard
	import PlayerUtil
	# BUG - Align Icons - end

	# BUG - Worst Enemy - start
	import AttitudeUtil
	# BUG - Refuses to Talk - end

	# BUG - Great General Bar - start
	import GGUtil
	# BUG - Great General Bar - end

	# BUG - Great Person Bar - start
	import GPUtil
	# BUG - Great Person Bar - end

	# fortsnek
	import FontUtil

# globals
gc = CyGlobalContext()
ArtFileMgr = CyArtFileMgr()
localText = CyTranslator()

g_NumEmphasizeInfos = 0
g_NumCityTabTypes = 0
g_NumHurryInfos = 0
g_NumUnitClassInfos = 0
g_NumBuildingClassInfos = 0
g_NumProjectInfos = 0
g_NumProcessInfos = 0
g_NumActionInfos = 0
g_eEndTurnButtonState = -1

# BUG - city specialist - start
g_iSuperSpecialistCount = 0
g_iCitySpecialistCount = 0
g_iAngryCitizensCount = 0
SPECIALIST_ROWS = 3
# BUG - city specialist - end

MAX_SELECTED_TEXT = 5
MAX_DISPLAYABLE_BUILDINGS = 15
MAX_DISPLAYABLE_TRADE_ROUTES = 4
MAX_BONUS_ROWS = 10
MAX_CITIZEN_BUTTONS = 8

SELECTION_BUTTON_COLUMNS = 8
SELECTION_BUTTON_ROWS = 2
NUM_SELECTION_BUTTONS = SELECTION_BUTTON_ROWS * SELECTION_BUTTON_COLUMNS

g_iNumBuildingWidgets = MAX_DISPLAYABLE_BUILDINGS
g_iNumTradeRouteWidgets = MAX_DISPLAYABLE_TRADE_ROUTES


g_iNumTradeRoutes = 0
g_iNumBuildings = 0
g_iNumLeftBonus = 0
g_iNumCenterBonus = 0
g_iNumRightBonus = 0

g_szTimeText = ""

# BUG - NJAGC - start
g_bShowTimeTextAlt = False
g_iTimeTextCounter = -1
# BUG - NJAGC - end

# BUG - start
g_mainInterface = None
def onSwitchHotSeatPlayer(argsList):
	#g_mainInterface.resetEndTurnObjects()
	# fortsnek: No multiplayer.
	pass
# BUG - end

g_pSelectedUnit = 0

# Table: score>|delta>|name<|spy<|power>|trade<|religion<|cities>
#  BUG order: FWSZVC?EPTUNBDRAHMQ*LO
# Refuses to talk (BULL)|war/peace treaty|score|delta|master|name|not yet met|spy|power|tech|turns|trade|open borders|defensive pact|religion|attitude|worst enemy|mobilising|cities
#     |waiting for you in multiplayer|ping|sync
# Supported by this script: war/peace treaty|score|delta|master/name|spy|power|tech(turns)|trade|open borders/defensive pact|religion|attitude|worst enemy|mobilising|cities

class ScoreboardColumnId:
	WAR_PEACE     = 1
	SCORE         = 2
	SCORE_DELTA   = 3
	NAME          = 5
	SPY           = 6
	POWER         = 7
	TECH          = 8
	TRADE         = 9
	AGREEMENTS    = 10
	RELIGION      = 11
	ATTITUDE      = 12
	WORST_ENEMY   = 13
	MOBILISING    = 14
	CITIES        = 15



class CvMainInterface:
	"Main Interface Screen"
	
	def __init__(self):
		g_mainInterface = self
		

############## Basic operational functions ###################

#########################################################################################
#########################################################################################
#########################################################################################
#########################################################################################

	def initState (self, screen=None):
		"""
		Initialize screen instance (self.foo) and global variables.
		
		This function is called before drawing the screen (from interfaceScreen() below)
		and anytime the Python modules are reloaded (from CvEventInterface).
		
		THIS FUNCTION MUST NOT ALTER THE SCREEN -- screen.foo()
		"""
		if screen is None:
			screen = CyGInterfaceScreen( "MainInterface", CvScreenEnums.MAIN_INTERFACE )

		# Set up our global variables...
		global g_NumEmphasizeInfos
		global g_NumCityTabTypes
		global g_NumHurryInfos
		global g_NumUnitClassInfos
		global g_NumBuildingClassInfos
		global g_NumProjectInfos
		global g_NumProcessInfos
		global g_NumActionInfos
		
		g_NumEmphasizeInfos = gc.getNumEmphasizeInfos()
		g_NumCityTabTypes = CityTabTypes.NUM_CITYTAB_TYPES
		g_NumHurryInfos = gc.getNumHurryInfos()
		g_NumUnitClassInfos = gc.getNumUnitClassInfos()
		g_NumBuildingClassInfos = gc.getNumBuildingClassInfos()
		g_NumProjectInfos = gc.getNumProjectInfos()
		g_NumProcessInfos = gc.getNumProcessInfos()
		g_NumActionInfos = gc.getNumActionInfos()
		

	def interfaceScreen (self):
		"""
		Draw all of the screen elements.
		
		This function is called once after starting or loading a game.
		
		THIS FUNCTION MUST NOT CREATE ANY INSTANCE OR GLOBAL VARIABLES.
		It may alter existing ones created in __init__() or initState(), however.
		"""
		if CyGame().isPitbossHost():
			return
			
		self.kScoreboardDesc = (
			(ScoreboardColumnId.WAR_PEACE  , EJustilign.Start),
			(ScoreboardColumnId.SCORE      , EJustilign.Start),
			(ScoreboardColumnId.SCORE_DELTA, EJustilign.End),
			(ScoreboardColumnId.NAME       , EJustilign.Start),
			(ScoreboardColumnId.SPY        , EJustilign.Start),
			(ScoreboardColumnId.POWER      , EJustilign.End),
			(ScoreboardColumnId.TECH       , EJustilign.Start),
			(ScoreboardColumnId.TRADE      , EJustilign.Start),
			(ScoreboardColumnId.AGREEMENTS , EJustilign.Start),
			(ScoreboardColumnId.RELIGION   , EJustilign.Start),
			(ScoreboardColumnId.ATTITUDE   , EJustilign.Start),
			(ScoreboardColumnId.WORST_ENEMY, EJustilign.Start),
			(ScoreboardColumnId.MOBILISING , EJustilign.Start),
			(ScoreboardColumnId.CITIES     , EJustilign.End),
		)
		
		# This is the main interface screen, create it as such
		screen = CyGInterfaceScreen("MainInterface", CvScreenEnums.MAIN_INTERFACE)
		self.initState(screen)

		root = ""
		
		# GameHeaderPanel
		screen.newResizableHSplit(root, "splitGameHeaderAndRest", 5, False)
		screen.newPanel("splitGameHeaderAndRest", "GameHeaderPanel")
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 1)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # GameHeaderBar
			TableLayoutConfigCell(ivec2(0, 1)) # GameHeaderPanelBottom
		]
		screen.setTableLayout("GameHeaderPanel", table)
		
		screen.newPanel("GameHeaderPanel", "GameHeaderBar")
		screen.newActionButton("GameHeaderBar", "GameHeaderPanel_btnEventLog", WidgetTypes.WIDGET_ACTION, gc.getControlInfo(ControlTypes.CONTROL_TURN_LOG).getActionInfoIndex(), -1, HeckTuiBorderStyle.NONE)
		screen.setActionButtonLabel("GameHeaderPanel_btnEventLog", u'[' + localText.getText("TXT_KEY_EVENT_LOG", ()) + u']')
		screen.newLabel("GameHeaderBar", "GameHeaderPanel_lblGold", "")
		if kHasBUG:
			screen.newGuage("GameHeaderBar", "GameHeaderPanel_GreatGeneralBar", [gc.getInfoTypeForString("COLOR_NEGATIVE_RATE")], WidgetTypes.WIDGET_HELP_GREAT_GENERAL)
		else:
			screen.newPanel("GameHeaderBar", "GameHeaderPanel_GreatGeneralBar") # dummy
		screen.hide("GameHeaderPanel_GreatGeneralBar") # Ehh... let's free up some space for the misc buttons.
		if kHasBUG:
			screen.newGuage("GameHeaderBar", "GameHeaderPanel_GreatPersonBar", [gc.getInfoTypeForString("COLOR_GREAT_PEOPLE_STORED"), gc.getInfoTypeForString("COLOR_GREAT_PEOPLE_RATE")], WidgetTypes.WIDGET_GP_PROGRESS_BAR)
		else:
			screen.newPanel("GameHeaderBar", "GameHeaderPanel_GreatPersonBar") # dummy
		screen.newGuage("GameHeaderBar", "GameHeaderPanel_ResearchBar", [gc.getInfoTypeForString("COLOR_RESEARCH_STORED"), gc.getInfoTypeForString("COLOR_RESEARCH_RATE")], WidgetTypes.WIDGET_RESEARCH)
		screen.newScrollSeekPanel("GameHeaderBar", "GameHeaderPanel_ResearchSelectionPanel", "GameHeaderPanel_ResearchSelectionContentPanel", HeckTuiAxis.Horizontal)
		#screen.newScrollSeekPanel("GameHeaderBar", "GameHeaderPanel_AdvisorsHSeek", "GameHeaderPanel_AdvisorButtonsPanel", HeckTuiAxis.Horizontal)
		screen.newPanel("GameHeaderBar", "GameHeaderPanel_AdvisorButtonsPanel")
		
		screen.newLabel("GameHeaderBar", "GameHeaderPanel_lblGameTime", "")
		screen.newActionButton("GameHeaderBar", "GameHeaderPanel_MainMenuButton", WidgetTypes.WIDGET_MENU_ICON, -1, -1, HeckTuiBorderStyle.NONE)

		table = TableLayoutConfig()
		table.cols = [
			TableLayoutConfigRowColDesc(0, 0),
			TableLayoutConfigRowColDesc(0, 1),
			TableLayoutConfigRowColDesc(0, 0),
			TableLayoutConfigRowColDesc(0, 3),
			TableLayoutConfigRowColDesc(0, 1),
			TableLayoutConfigRowColDesc(0, 0),
			TableLayoutConfigRowColDesc(0, 0),
			TableLayoutConfigRowColDesc(0, 0)
		]
		table.rows = [TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)),
			TableLayoutConfigCell(ivec2(1, 0)),
			TableLayoutConfigCell(ivec2(2, 0)),
			TableLayoutConfigCell(ivec2(4, 0)),
			TableLayoutConfigCell(ivec2(3, 0)), TableLayoutConfigCell(ivec2(3, 0)),
			TableLayoutConfigCell(ivec2(5, 0)),
			TableLayoutConfigCell(ivec2(6, 0), ivec2(1, 1), RectJustilign(EJustilign.End, EJustilign.Center)),
			TableLayoutConfigCell(ivec2(7, 0), ivec2(1, 1), RectJustilign(EJustilign.End, EJustilign.Center)),
			# Original had GameHeaderPanelBottom in here too, but table layout does not curently handle proportional sizes and spans well in combination.
		]
		table.gap = ivec2(1, 0)
		screen.setTableLayout("GameHeaderBar", table)
		
		# GameHeaderPanel children setup
		screen.setHFlowLayout("GameHeaderPanel_ResearchSelectionContentPanel")
		screen.setLabelHAlign("GameHeaderPanel_lblGameTime", EAlign.Right)
		screen.setActionButtonLabel("GameHeaderPanel_MainMenuButton", u' \u2261\u2261 ')
		
		# GameHeaderPanel / GameHeaderPanelBottom
		screen.newPanel("GameHeaderPanel", "GameHeaderPanelBottom")
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)] #, TableLayoutConfigRowColDesc(0, 0)]
		table.rows = [TableLayoutConfigRowColDesc(0, 1)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # TurnMessageDisplay
			#TableLayoutConfigCell(ivec2(1, 0)), # ScreensList
		]
		screen.setTableLayout("GameHeaderPanelBottom", table)
		#screen.setHFlowLayout("GameHeaderPanelBottom")
		
		screen.newTurnMessageDisplay("GameHeaderPanelBottom", "TurnMessageDisplay")
		#screen.newScrollBarPanel("GameHeaderPanelBottom", "ScreensListPanel", "ScreensList", HeckTuiAxis.Vertical)
		
		#screen.setVFlowLayout("ScreensList")
		
		# root
		#     splitGameHeaderAndRest (HSplit)
		#         GameHeaderPanel
		#             GameHeaderBar
		#             GameHeaderPanelBottom
		#         splitBottomPanel
		#             ...
		#     CityViewRoot (table, initially empty)
		#         GameHeaderBar
		#         splitBottomPanel
		
		# CityViewRoot
		screen.newPanel(root, "CityViewRoot")
		table = TableLayoutConfig()
		table.cols = [
			TableLayoutConfigRowColDesc(0, 1),
		]
		table.rows = [
			TableLayoutConfigRowColDesc(0, 0), # GameHeaderBar
			TableLayoutConfigRowColDesc(0, 0), # hrule
			TableLayoutConfigRowColDesc(0, 1), # splitBottomPanel
			]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)),
			TableLayoutConfigCell(ivec2(0, 1)),
			TableLayoutConfigCell(ivec2(0, 2)),
		]
		screen.setTableLayout("CityViewRoot", table)
		screen.newHRule("CityViewRoot", "CityViewRootHRule")
		
		# Advisor buttons
		borderStyle = HeckTuiBorderStyle.NONE
		parent = "GameHeaderPanel_AdvisorButtonsPanel"
		screen.setHFlowLayout(parent)
		screen.newActionButton(parent, "DomesticAdvisorButton", WidgetTypes.WIDGET_ACTION, gc.getControlInfo(ControlTypes.CONTROL_DOMESTIC_SCREEN).getActionInfoIndex(), -1, borderStyle)
		screen.newActionButton(parent, "FinanceAdvisorButton", WidgetTypes.WIDGET_ACTION, gc.getControlInfo(ControlTypes.CONTROL_FINANCIAL_SCREEN).getActionInfoIndex(), -1, borderStyle)
		screen.newActionButton(parent, "CivicsAdvisorButton", WidgetTypes.WIDGET_ACTION, gc.getControlInfo(ControlTypes.CONTROL_CIVICS_SCREEN).getActionInfoIndex(), -1, borderStyle)
		screen.newActionButton(parent, "ForeignAdvisorButton", WidgetTypes.WIDGET_ACTION, gc.getControlInfo(ControlTypes.CONTROL_FOREIGN_SCREEN).getActionInfoIndex(), -1, borderStyle)
		screen.newActionButton(parent, "MilitaryAdvisorButton", WidgetTypes.WIDGET_ACTION, gc.getControlInfo(ControlTypes.CONTROL_MILITARY_SCREEN).getActionInfoIndex(), -1, borderStyle)
		screen.newActionButton(parent, "TechAdvisorButton", WidgetTypes.WIDGET_ACTION, gc.getControlInfo(ControlTypes.CONTROL_TECH_CHOOSER).getActionInfoIndex(), -1, borderStyle)
		screen.newActionButton(parent, "ReligiousAdvisorButton", WidgetTypes.WIDGET_ACTION, gc.getControlInfo(ControlTypes.CONTROL_RELIGION_SCREEN).getActionInfoIndex(), -1, borderStyle)
		screen.newActionButton(parent, "CorporationAdvisorButton", WidgetTypes.WIDGET_ACTION, gc.getControlInfo(ControlTypes.CONTROL_CORPORATION_SCREEN).getActionInfoIndex(), -1, borderStyle)
		screen.newActionButton(parent, "VictoryAdvisorButton", WidgetTypes.WIDGET_ACTION, gc.getControlInfo(ControlTypes.CONTROL_VICTORY_SCREEN).getActionInfoIndex(), -1, borderStyle)
		screen.newActionButton(parent, "InfoAdvisorButton", WidgetTypes.WIDGET_ACTION, gc.getControlInfo(ControlTypes.CONTROL_INFO).getActionInfoIndex(), -1, borderStyle)
		
		screen.setActionButtonLabel("DomesticAdvisorButton", u'[\u2302]')
		screen.setActionButtonLabel("FinanceAdvisorButton", u"[%c]" % (gc.getCommerceInfo(CommerceTypes.COMMERCE_GOLD).getChar(),))
		screen.setActionButtonLabel("CivicsAdvisorButton", u"[%s]" % (CyTranslator().changeTextColor(u"\u03fe", gc.getInfoTypeForString('COLOR_YELLOW')),))
		screen.setActionButtonLabel("ForeignAdvisorButton", u"[%c]" % (CyGame().getSymbolID(FontSymbols.TRADE_CHAR),))
		screen.setActionButtonLabel("MilitaryAdvisorButton", u"[%c]" % (CyGame().getSymbolID(FontSymbols.STRENGTH_CHAR),))
		screen.setActionButtonLabel("TechAdvisorButton", u"[%c]" % (gc.getCommerceInfo(CommerceTypes.COMMERCE_RESEARCH).getChar(),))
		screen.setActionButtonLabel("ReligiousAdvisorButton", u"[%c]" % (CyGame().getSymbolID(FontSymbols.RELIGION_CHAR),))
		screen.setActionButtonLabel("CorporationAdvisorButton", u"[%c%c]" % (gc.getCommerceInfo(CommerceTypes.COMMERCE_GOLD).getChar(), CyGame().getSymbolID(FontSymbols.SILVER_STAR_CHAR),))
		screen.setActionButtonLabel("VictoryAdvisorButton", u"[\u203c]")
		screen.setActionButtonLabel("InfoAdvisorButton", u"[%]")
		
		#if GameUtil.isEspionage():
		if not gc.getGame().isOption(GameOptionTypes.GAMEOPTION_NO_ESPIONAGE):
			screen.newActionButton(parent, "EspionageAdvisorButton", WidgetTypes.WIDGET_ACTION, gc.getControlInfo(ControlTypes.CONTROL_ESPIONAGE_SCREEN).getActionInfoIndex(), -1, borderStyle)
			screen.setActionButtonLabel("EspionageAdvisorButton", u"[%c]" % (gc.getCommerceInfo(CommerceTypes.COMMERCE_ESPIONAGE).getChar(),))
		
		# Middle and bottom panel
		#screen.newHRule(root, "HRuleHeaderAndWorldView")
		screen.newResizableHSplit("splitGameHeaderAndRest", "splitBottomPanel", 10, True)
		#table = TableLayoutConfig()
		#table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		#table.rows = [TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 1)]
		#table.cells = [TableLayoutConfigCell(ivec2(0, 0)), TableLayoutConfigCell(ivec2(0, 1)), TableLayoutConfigCell(ivec2(0, 2))]
		#screen.setTableLayout(root, table)
		screen.setFillLayout(root)
		
		screen.newPanel("splitBottomPanel", "MiddlePanel")
		screen.setFillLayout("MiddlePanel")

		# Middle split for the default view.
		screen.newResizableVSplit("MiddlePanel", "splitPlotList", 32, False)
		screen.newPanel("splitPlotList", "MainLeftPanel")
		screen.newPanel("splitPlotList", "WorldViewContainerPanel")
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 1)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # WorldView
			#TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), RectJustilign(EJustilign.Start, EJustilign.Start)), # old TurnMessageDisplay
			TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), RectJustilign(EJustilign.End, EJustilign.End)), # Scoreboard bottom-right panel
			TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), RectJustilign(EJustilign.Start, EJustilign.End)), # plot help
			TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), RectJustilign(EJustilign.End, EJustilign.Start)), # getTurnTimerText
		]
		screen.setTableLayout("WorldViewContainerPanel", table)
		screen.newWorldView("WorldViewContainerPanel", "WorldView")
		
		
		# Bottom-right panel
		screen.newPanel("WorldViewContainerPanel", "WorldViewContainerBottomRightPanel")
		#screen.setPanelBackgroundColour("WorldViewContainerBottomRightPanel", None)
		screen.setHFlowLayout("WorldViewContainerBottomRightPanel")
		
		# WorldViewContainerPanel / PlotHelp
		screen.newPlotHelpLabel("WorldViewContainerPanel", "PlotHelpLabel")
		
		# WorldViewContainerPanel / TurnTimerText
		screen.newBoxPanel("WorldViewContainerPanel", "WorldViewTurnTimerBoxPanel", "WorldViewTurnTimerBoxPanelClient", HeckTuiBorderStyle.NONE, HeckTuiColour.Grey100)
		screen.setFillLayout("WorldViewTurnTimerBoxPanelClient")
		screen.newLabel("WorldViewTurnTimerBoxPanelClient", "WorldViewTurnTimerLabel", "")
		
		# Bottom-right panel / Scoreboard
		screen.newPanel("WorldViewContainerBottomRightPanel", "Scoreboard")
		screen.setPanelBackgroundColour("Scoreboard", HeckTuiColour.Black)

		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 0)] * len(self.kScoreboardDesc)
		table.rows = [TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [TableLayoutConfigCell(ivec2(i, 0), ivec2(1, 1), RectJustilign(align, EJustilign.Start)) for i, (colId, align) in enumerate(self.kScoreboardDesc)]
		screen.setTableLayout("Scoreboard", table)
		
		
		# PlotListPanel or CityOutputPanel / SliderPanel
		screen.setVFlowLayout("MainLeftPanel")
		screen.newPanel("MainLeftPanel", "SliderPanel")
		table = TableLayoutConfig()
		table.cols = [
			TableLayoutConfigRowColDesc(0, 1),
			TableLayoutConfigRowColDesc(0, 0),
			TableLayoutConfigRowColDesc(0, 0),
			TableLayoutConfigRowColDesc(0, 0),
			TableLayoutConfigRowColDesc(0, 0),
			TableLayoutConfigRowColDesc(0, 0)
		]
		table.rows = [TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)),
			TableLayoutConfigCell(ivec2(1, 0)),
			TableLayoutConfigCell(ivec2(2, 0)),
			TableLayoutConfigCell(ivec2(3, 0)),
			TableLayoutConfigCell(ivec2(4, 0)),
			TableLayoutConfigCell(ivec2(5, 0))
		]
		screen.setTableLayout("SliderPanel", table)

		for i in range(CommerceTypes.NUM_COMMERCE_TYPES):
			eCommerce = (i + 1) % int(CommerceTypes.NUM_COMMERCE_TYPES)
			prefix = "Slider%d_" % (eCommerce,)
			screen.newLabel("SliderPanel", prefix + "PercentText", "")
			if eCommerce == CommerceTypes.COMMERCE_GOLD:
				screen.newLabel("SliderPanel", prefix + "Inc100", "   ")
				screen.newLabel("SliderPanel", prefix + "Inc", "   ")
				screen.newLabel("SliderPanel", prefix + "Dec", "   ")
				screen.newLabel("SliderPanel", prefix + "Dec100", "   ")
			else:
				screen.newActionButton("SliderPanel", prefix + "Inc100", WidgetTypes.WIDGET_CHANGE_PERCENT, eCommerce, 100, HeckTuiBorderStyle.NONE)
				screen.newActionButton("SliderPanel", prefix + "Inc", WidgetTypes.WIDGET_CHANGE_PERCENT, eCommerce, gc.getDefineINT("COMMERCE_PERCENT_CHANGE_INCREMENTS"), HeckTuiBorderStyle.NONE)
				screen.newActionButton("SliderPanel", prefix + "Dec", WidgetTypes.WIDGET_CHANGE_PERCENT, eCommerce, -gc.getDefineINT("COMMERCE_PERCENT_CHANGE_INCREMENTS"), HeckTuiBorderStyle.NONE)
				screen.newActionButton("SliderPanel", prefix + "Dec100", WidgetTypes.WIDGET_CHANGE_PERCENT, eCommerce, -100, HeckTuiBorderStyle.NONE)
			screen.newLabel("SliderPanel", prefix + "RateText", "")
			
		screen.newHRule("MainLeftPanel", "PlotListPanel_SliderPlotListHRule", HeckTuiBorderStyle.Thin)
		
		screen.newScrollBarPanel("MainLeftPanel", "PlotListScrollPanel", "PlotListPanel", HeckTuiAxis.Vertical)
		screen.setVFlowLayout("PlotListPanel")
			
		# Middle splits for the city view.
		screen.newResizableVSplit("MiddlePanel", "splitCityOutputAndRest", 30, False)
		screen.newPanel("splitCityOutputAndRest", "CityOutputPanel")
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [TableLayoutConfigCell(ivec2(0, 0)), TableLayoutConfigCell(ivec2(0, 1))]
		topBottomDivideTable = table
		screen.setTableLayout("CityOutputPanel", topBottomDivideTable)
		
		## Building the CityOutputPanel
		screen.newPanel("CityOutputPanel", "CityOutputPanelTop")
		screen.setVFlowLayout("CityOutputPanelTop")
		screen.newPanel("CityOutputPanel", "CityOutputPanelBottom")
		screen.setVFlowLayout("CityOutputPanelBottom")
		
		# CityOutputPanelTop / Maintainence
		screen.newPanel("CityOutputPanelTop", "Maintainence")
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0)]
		table.rows = [TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [TableLayoutConfigCell(ivec2(0, 0)), TableLayoutConfigCell(ivec2(1, 0))]
		nameValueTable = table
		self.nameValueTable = nameValueTable
		screen.setTableLayout("Maintainence", nameValueTable)
		
		screen.newLabel("Maintainence", "MaintainenceLabel", localText.getText("INTERFACE_CITY_MAINTENANCE", ()))
		screen.newLabel("Maintainence", "MaintainenceValue", "")
			
		# CityOutputPanelTop / TradePanel
		screen.newLabel("CityOutputPanelTop", "TradePanelLabel", localText.getText("TXT_KEY_CONCEPT_TRADE", ()))
		screen.setLabelHAlign("TradePanelLabel", EAlign.Center)
		screen.newPanel("CityOutputPanelTop", "TradePanel")
		screen.setTableLayout("TradePanel", nameValueTable)
		
		# CityOutputPanelTop / CityBuildingsPanel
		screen.newLabel("CityOutputPanelTop", "CityBuildingsPanelLabel", localText.getText("TXT_KEY_CONCEPT_BUILDINGS", ()))
		screen.setLabelHAlign("CityBuildingsPanelLabel", EAlign.Center)
		screen.newPanel("CityOutputPanelTop", "CityBuildingsPanel")
		
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [TableLayoutConfigCell(ivec2(0, 0)), TableLayoutConfigCell(ivec2(1, 0))]
		table.gap = ivec2(1, 0)
		screen.setTableLayout("CityBuildingsPanel", table)
		
		# CityOutputPanelBottom / NationalityGuage
		screen.newGuage("CityOutputPanelBottom", "NationalityGuage", [], WidgetTypes.WIDGET_HELP_NATIONALITY)
		
		# CityOutputPanelBottom / CultureGuage
		screen.newGuage("CityOutputPanelBottom", "CultureGuage", [gc.getInfoTypeForString("COLOR_CULTURE_STORED"), gc.getInfoTypeForString("COLOR_CULTURE_RATE")], WidgetTypes.WIDGET_HELP_CULTURE)

		# City view middle/right split.
		screen.newResizableVSplit("splitCityOutputAndRest", "splitWorldViewAndCityInput", 40, True)
		screen.newPanel("splitWorldViewAndCityInput", "CityMiddlePanel")
		screen.newPanel("splitWorldViewAndCityInput", "CityInputPanel")
		screen.setTableLayout("CityInputPanel", topBottomDivideTable)
		
		screen.newPanel("CityInputPanel", "CityInputPanelTop")
		screen.newPanel("CityInputPanel", "CityInputPanelBottom")
		screen.setVFlowLayout("CityInputPanelTop")
		screen.setVFlowLayout("CityInputPanelBottom")
		
		## Building the CityInputPanel
		# Religions
		screen.newLabel("CityInputPanelTop", "ReligionPanelHeader", localText.getText("TXT_KEY_TRADE_RELIGIONS", ()))
		screen.setLabelHAlign("ReligionPanelHeader", EAlign.Center)
		screen.newPanel("CityInputPanelTop", "ReligionPanel")
		screen.setTableLayout("ReligionPanel", nameValueTable)
		# Corporations
		screen.newLabel("CityInputPanelTop", "CorporationPanelHeader", localText.getText("TXT_KEY_CONCEPT_CORPORATIONS", ())) # or TXT_KEY_TECH_CORPORATION if not BTS
		screen.setLabelHAlign("CorporationPanelHeader", EAlign.Center)
		screen.newPanel("CityInputPanelTop", "CorporationPanel")
		screen.setTableLayout("CorporationPanel", nameValueTable)
		# Bonuses (ScrollPanel, flex)
		screen.newLabel("CityInputPanelTop", "BonusesPanelHeader", localText.getText("TXT_KEY_WB_BONUSES", ())) # or TXT_KEY_PEDIA_BONUS_YIELDS
		screen.setLabelHAlign("BonusesPanelHeader", EAlign.Center)
		screen.newPanel("CityInputPanelTop", "BonusesPanel")
		screen.setTableLayout("BonusesPanel", nameValueTable)
		# Specialists (fixed)
		screen.newLabel("CityInputPanelBottom", "SpecialistsPanelHeader", localText.getText("TXT_KEY_CONCEPT_SPECIALISTS", ()))
		screen.setLabelHAlign("SpecialistsPanelHeader", EAlign.Center)
		screen.newPanel("CityInputPanelBottom", "SpecialistsPanel")
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 0)]
		table.rows = [TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [TableLayoutConfigCell(ivec2(0, 0)), TableLayoutConfigCell(ivec2(1, 0)), TableLayoutConfigCell(ivec2(2, 0)), TableLayoutConfigCell(ivec2(3, 0))]
		screen.setTableLayout("SpecialistsPanel", table)
		screen.newGuage("CityInputPanelBottom", "SpecialistsGuage", [gc.getInfoTypeForString("COLOR_GREAT_PEOPLE_STORED"), gc.getInfoTypeForString("COLOR_GREAT_PEOPLE_RATE")], WidgetTypes.WIDGET_HELP_GREAT_PEOPLE)
		
		## Building the city middle panel
		# Rows: CityTitlePanel, [hrule], CityFoodProdPanel, [hrule], CityWorldView
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 1)]
		table.cells = [TableLayoutConfigCell(ivec2(0, 0)), TableLayoutConfigCell(ivec2(0, 1)), TableLayoutConfigCell(ivec2(0, 2)), TableLayoutConfigCell(ivec2(0, 3)), TableLayoutConfigCell(ivec2(0, 4))]
		screen.setTableLayout("CityMiddlePanel", table)
		screen.newPanel("CityMiddlePanel", "CityTitlePanel")
		screen.newHRule("CityMiddlePanel", "CityTitleHRule")
		screen.newPanel("CityMiddlePanel", "CityFoodProdPanel")
		screen.newHRule("CityMiddlePanel", "CityWorldViewHRule")
		screen.newWorldView("CityMiddlePanel", "CityWorldView")
		
		# CityTitlePanel
		screen.setFillLayout("CityTitlePanel")
		screen.newLabel("CityTitlePanel", "CityTitleLabel", "")
		screen.setLabelHAlign("CityTitleLabel", EAlign.Center)
		
		# CityFoodProdPanel
		# [YieldInfo] [Guage] [PopEffect]
		screen.newLabel("CityFoodProdPanel", "CityFoodProdPanel_Food_YieldInfo", "")
		screen.setLabelHAlign("CityFoodProdPanel_Food_YieldInfo", EAlign.Right)
		screen.newGuage("CityFoodProdPanel", "CityFoodProdPanel_Food_Guage", [gc.getYieldInfo(YieldTypes.YIELD_FOOD).getColorType(), gc.getYieldInfo(YieldTypes.YIELD_FOOD).getColorType(), gc.getInfoTypeForString("COLOR_NEGATIVE_RATE")], WidgetTypes.WIDGET_HELP_POPULATION)
		screen.newLabel("CityFoodProdPanel", "CityFoodProdPanel_Food_Health", "")
		
		screen.newLabel("CityFoodProdPanel", "CityFoodProdPanel_Prod_YieldInfo", "")
		screen.setLabelHAlign("CityFoodProdPanel_Prod_YieldInfo", EAlign.Right)
		screen.newGuage("CityFoodProdPanel", "CityFoodProdPanel_Prod_Guage", [gc.getYieldInfo(YieldTypes.YIELD_PRODUCTION).getColorType(), gc.getYieldInfo(YieldTypes.YIELD_PRODUCTION).getColorType(),  gc.getYieldInfo(YieldTypes.YIELD_FOOD).getColorType()], WidgetTypes.WIDGET_HELP_PRODUCTION)
		screen.newLabel("CityFoodProdPanel", "CityFoodProdPanel_Prod_Happiness", "")
		
		# CityFoodProdPanel. Layout as a three-column table.
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0)]
		table.rows = [TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [TableLayoutConfigCell(ivec2(0, 0)), TableLayoutConfigCell(ivec2(1, 0)), TableLayoutConfigCell(ivec2(2, 0))]
		screen.setTableLayout("CityFoodProdPanel", table)
		
		# SelInfo
		screen.newResizableVSplit("splitBottomPanel", "splitSelInfo", 30, False)
		screen.newPanel("splitSelInfo", "SelInfoPanel")
		screen.setVFlowLayout("SelInfoPanel")
		# Action panel, for production orders, unit info, and unit orders
		# [Unit stack title button to rename]
		# [Scrollable list of production orders (row buttons), or single unit info (labels), or unit list (labels), or unit orders (labels)]
		screen.newEmptyActionButton("SelInfoPanel", "SelInfo_StackTitleButton", WidgetTypes.WIDGET_UNIT_NAME, -1, -1)
		screen.setFillLayout("SelInfo_StackTitleButton")
		screen.newLabel("SelInfo_StackTitleButton", "SelInfo_StackTitleLabel", "")
		screen.newPanel("SelInfoPanel", "SelInfo_LabelRows")
		screen.setTableLayout("SelInfo_LabelRows", nameValueTable)
		
		# Bottom right slips (Minimap, BottomButtonsPanel)
		screen.newResizableVSplit("splitSelInfo", "splitMinimap", 30, True)
		screen.newPanel("splitMinimap", "BottomButtonsPanel")
		screen.newMinimap("splitMinimap", "Minimap")
		screen.setMinimapHandleMouseMove("Minimap")
		
		# BottomButtonsPanel is ActionPanel | OperationsPanel.
		# ActionPanel is HFlow. OperationsPanel is a 2-col table.
		# BottomButtonsPanel is a table that joins the two.
		
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 0)]
		table.rows = [TableLayoutConfigRowColDesc(0, 1)]
		table.cells = [TableLayoutConfigCell(ivec2(0, 0)), TableLayoutConfigCell(ivec2(1, 0)), TableLayoutConfigCell(ivec2(2, 0))]
		screen.setTableLayout("BottomButtonsPanel", table)
		
		screen.newScrollBarPanel("BottomButtonsPanel", "ActionScrollPanel", "ActionPanel", HeckTuiAxis.Vertical)
		screen.newVRule("BottomButtonsPanel", "ActionsOperationsVRule")
		screen.newPanel("BottomButtonsPanel", "OperationsPanelContainer")
		
		screen.setHWrapLayout("ActionPanel")

		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 0)]
		table.rows = [TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [TableLayoutConfigCell(ivec2(0, 0)), TableLayoutConfigCell(ivec2(1, 0))]
		#screen.setTableLayout("OperationsPanel", table)
		screen.setFillLayout("OperationsPanelContainer")
		
		screen.newPanel("OperationsPanelContainer", "DefaultOperationsPanel")
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 1)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)),
			TableLayoutConfigCell(ivec2(0, 1), ivec2(1, 1), RectJustilign(EJustilign.Stretch, EJustilign.Center)),
			TableLayoutConfigCell(ivec2(0, 1), ivec2(1, 1), RectJustilign(EJustilign.Stretch, EJustilign.Center)),
		]
		screen.setTableLayout("DefaultOperationsPanel", table)
		
		# Bottom-right panel / End Turn Button
		screen.newActionButton("DefaultOperationsPanel", "EndTurnButton", WidgetTypes.WIDGET_END_TURN, -1, -1, HeckTuiBorderStyle.Rounded)
		#screen.setActionButtonLabel("EndTurnButton", u"\u2192")
		screen.setActionButtonLabel("EndTurnButton", u">>")
		screen.setActionButtonBackground("EndTurnButton", HeckTuiColour.Red)
		
		activePlayerI = gc.getGame().getActivePlayer()
		activePlayer = gc.getPlayer(activePlayerI)
		activePlayerColourInfo = gc.getPlayerColorInfo(activePlayer.getPlayerColor())
		playerColourI = activePlayerColourInfo.getTextColorType()
		#szBuffer = gc.getPlayer(activePlayerI).getName() + u'/' + gc.getPlayer(activePlayerI).getCivilizationShortDescription(0)
		szBuffer = gc.getPlayer(activePlayerI).getCivilizationShortDescription(0)
		szBuffer = localText.changeTextColor(szBuffer, playerColourI)
		
		screen.newEmptyActionButton("DefaultOperationsPanel", "ActiveCivFlagButton", WidgetTypes.WIDGET_FLAG, activePlayerI, -1)
		screen.newPanel("ActiveCivFlagButton", "ActiveCivFlagBackPanel")
		screen.setVFlowLayout("ActiveCivFlagButton")
		screen.setVFlowLayout("ActiveCivFlagBackPanel")
		
		screen.newPanel("ActiveCivFlagBackPanel", "ActiveCivFlagTopBar")
		screen.setVFlowLayout("ActiveCivFlagTopBar")
		screen.newClickThroughLabel("ActiveCivFlagTopBar", "ActiveCivFlagTopBarLabel", "\n")
		screen.newClickThroughLabel("ActiveCivFlagBackPanel", "ActiveCivFlagMiddleText", szBuffer)
		screen.newPanel("ActiveCivFlagBackPanel", "ActiveCivFlagBottomBar")
		screen.setVFlowLayout("ActiveCivFlagBottomBar")
		screen.newClickThroughLabel("ActiveCivFlagBottomBar", "ActiveCivFlagBottomBarLabel", "\n")
		screen.setLabelWidgetData("ActiveCivFlagTopBarLabel", WidgetTypes.WIDGET_FLAG, activePlayerI, -1)
		screen.setLabelWidgetData("ActiveCivFlagBottomBarLabel", WidgetTypes.WIDGET_FLAG, activePlayerI, -1)
		screen.setPanelBackgroundColour("ActiveCivFlagTopBar", HeckTuiColour.fromColourType(activePlayerColourInfo.getColorTypePrimary()))
		screen.setPanelBackgroundColour("ActiveCivFlagBottomBar", HeckTuiColour.fromColourType(activePlayerColourInfo.getColorTypeSecondary()))
		
		
		

		# Bottom-right panel / OperationsPanel / CityOperationsPanel
		screen.newScrollBarPanel("OperationsPanelContainer", "CityOperationsScrollPanel", "CityOperationsPanel", HeckTuiAxis.Vertical)
		screen.setVFlowLayout("CityOperationsPanel")
		screen.newActionButton("CityOperationsPanel", "ConscriptButton", WidgetTypes.WIDGET_CONSCRIPT, -1, -1, HeckTuiBorderStyle.Bracketed)
		screen.setActionButtonLabel("ConscriptButton", localText.getText("TXT_KEY_DRAFT", ()))
		for i in range(gc.getNumHurryInfos()):
			screen.newActionButton("CityOperationsPanel", "HurryButton" + str(i), WidgetTypes.WIDGET_HURRY, i, -1, HeckTuiBorderStyle.Bracketed)
		screen.newActionCheckBox("CityOperationsPanel", "AutomateProduction", WidgetTypes.WIDGET_AUTOMATE_PRODUCTION, -1)
		screen.newActionCheckBox("CityOperationsPanel", "AutomateCitizens", WidgetTypes.WIDGET_AUTOMATE_CITIZENS, -1)
		for i in range(6):
			screen.newActionCheckBox("CityOperationsPanel", "EmphasiseButton" + str(i), WidgetTypes.WIDGET_EMPHASIZE, i)
		
		

		# This should show the screen immidiately and pass input to the game
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, True)
		
		
		return 0

	# Will update the screen (every 250 MS)
	def updateScreen(self):
		
		global g_szTimeText
		global g_iTimeTextCounter

		screen = CyGInterfaceScreen( "MainInterface", CvScreenEnums.MAIN_INTERFACE )
		
		pHeadSelectedCity = CyInterface().getHeadSelectedCity()

		if pHeadSelectedCity:
			ePlayer = pHeadSelectedCity.getOwner()
		else:
			ePlayer = gc.getGame().getActivePlayer()

		if ePlayer < 0 or ePlayer >= gc.getMAX_PLAYERS():
			return 0

		# This should recreate the minimap on load games and returns if already exists -JW
		#fortsnek: screen.initMinimap( xResolution - 210, xResolution - 9, yResolution - 131, yResolution - 9, -0.1 )

		#messageControl = CyMessageControl()
		
		self.updateGameDataBar(screen, gc.getGame().getActivePlayer())
		self.updateEndTurnButton(screen)
		self.updateMiddleShowHide(screen)
		self.updatePlotListButtons(screen)
		self.updateSelectionButtons(screen, pHeadSelectedCity)
		self.updateSliders(screen, ePlayer, pHeadSelectedCity)
		self.updateCityTrade(screen, pHeadSelectedCity)
		self.updateCityBuildings(screen, pHeadSelectedCity)
		self.updateCityCultureBars(screen, pHeadSelectedCity)
		self.updateCityInputPanel(screen, pHeadSelectedCity)
		self.updateSelectionInfo(screen, pHeadSelectedCity)
		if pHeadSelectedCity:
			self.updateCityMiddlePanel(screen, pHeadSelectedCity)
			
		screen.updateTurnMessageDisplay("TurnMessageDisplay")
		
		self.updateScoreboard(screen)
		
		self.updateTurnTimerText(screen)

		return 0
	
	def updateGameDataBar(self, screen, ePlayer):
		self.updateTimeText()
		screen.setLabelText("GameHeaderPanel_lblGameTime", g_szTimeText)
		
		if kHasBUG and MainOpt.isGoldRateWarning():
			pPlayer = gc.getPlayer(ePlayer)
			iGold = pPlayer.getGold()
			iGoldRate = pPlayer.calculateGoldRate()
			if iGold < 0:
				szText = BugUtil.getText("TXT_KEY_MISC_NEG_GOLD", iGold)
				if iGoldRate != 0:
					if iGold + iGoldRate >= 0:
						szText += BugUtil.getText("TXT_KEY_MISC_POS_GOLD_PER_TURN", iGoldRate)
					elif iGoldRate >= 0:
						szText += BugUtil.getText("TXT_KEY_MISC_POS_WARNING_GOLD_PER_TURN", iGoldRate)
					else:
						szText += BugUtil.getText("TXT_KEY_MISC_NEG_GOLD_PER_TURN", iGoldRate)
			else:
				szText = BugUtil.getText("TXT_KEY_MISC_POS_GOLD", iGold)
				if iGoldRate != 0:
					if iGoldRate >= 0:
						szText += BugUtil.getText("TXT_KEY_MISC_POS_GOLD_PER_TURN", iGoldRate)
					elif iGold + iGoldRate >= 0:
						szText += BugUtil.getText("TXT_KEY_MISC_NEG_WARNING_GOLD_PER_TURN", iGoldRate)
					else:
						szText += BugUtil.getText("TXT_KEY_MISC_NEG_GOLD_PER_TURN", iGoldRate)
			if pPlayer.isStrike():
				szText += BugUtil.getPlainText("TXT_KEY_MISC_STRIKE")
		else:
			szText = CyGameTextMgr().getGoldStr(ePlayer)
		
		screen.setLabelText("GameHeaderPanel_lblGold", szText)
		
		player = gc.getPlayer(ePlayer)
		team = gc.getTeam(player.getTeam())
		
		#screen.newGuage("GameHeaderPanel", "GameHeaderPanel_GreatGeneralBar", [gc.getInfoTypeForString("COLOR_NEGATIVE_RATE")], WidgetTypes.WIDGET_HELP_GREAT_GENERAL)
		iCombatExp = player.getCombatExperience()
		iThresholdExp = player.greatPeopleThreshold(True)
		iNeededExp = iThresholdExp - iCombatExp

		# Using BUG
		if kHasBUG:
			screen.setGuageValues("GameHeaderPanel_GreatGeneralBar", GGUtil.getGreatGeneralText(iNeededExp), iThresholdExp, [iCombatExp, iNeededExp])
		
		
		# Research bar
		screen.hide("GameHeaderPanel_ResearchBar")
		screen.hide("GameHeaderPanel_ResearchSelectionPanel")
		
		if player.getCurrentResearch() != -1:
			researchProgress = team.getResearchProgress(player.getCurrentResearch())
			overflowResearch = (player.getOverflowResearch() * player.calculateResearchModifier(player.getCurrentResearch())) / 100
			researchCost = team.getResearchCost(player.getCurrentResearch())
			researchRate = player.calculateResearchRate(-1)

			screen.show("GameHeaderPanel_ResearchBar")
			screen.setGuageValues("GameHeaderPanel_ResearchBar", CyGameTextMgr().getResearchStr(ePlayer), researchCost, [researchProgress + overflowResearch, researchRate])
		else:
			screen.show("GameHeaderPanel_ResearchSelectionPanel")
			iCount = 0
			
			prefix = "GameHeaderPanel_ResearchSelectionBar_Button[]"
			screen.delByPrefix(prefix)
			
			first = True
			for i in range(gc.getNumTechInfos()):
				if player.canResearch(i, False):
					name = prefix + str(i)
					if not first:
						screen.newLabel("GameHeaderPanel_ResearchSelectionContentPanel", name + "sep", u" | ")
					else:
						first = False
					screen.newActionButton("GameHeaderPanel_ResearchSelectionContentPanel", name, WidgetTypes.WIDGET_RESEARCH, i, -1, HeckTuiBorderStyle.NONE)
					screen.setActionButtonLabel(name, u"%s (%d)" % (gc.getTechInfo(i).getDescription(), player.getResearchTurnsLeft(i, True)))
				
		if kHasBUG:
			#screen.newGuage("GameHeaderPanel", "GameHeaderPanel_GreatPersonBar", [gc.getInfoTypeForString("COLOR_GREAT_PEOPLE_STORED"), gc.getInfoTypeForString("COLOR_GREAT_PEOPLE_RATE")], WidgetTypes.WIDGET_GP_PROGRESS_BAR)
			screen.setGuageValues("GameHeaderPanel_GreatPersonBar", u'xxx', 100, [0, 0])
			
			# Using BUG
			pGPCity, iGPTurns = GPUtil.getDisplayCity()
			szText = GPUtil.getGreatPeopleText(pGPCity, iGPTurns, 1000, MainOpt.isGPBarTypesNone(), MainOpt.isGPBarTypesOne(), True)
			iCityID = pGPCity.getID() if pGPCity else -1

			if iCityID >= 0:
				# Can be a different owner if looking at other players!
				owner = gc.getPlayer(pGPCity.getOwner())
				threshold = owner.greatPeopleThreshold(False)
				progress = pGPCity.getGreatPeopleProgress()
				rate = pGPCity.getGreatPeopleRate()
				screen.setGuageValues("GameHeaderPanel_GreatPersonBar", szText, threshold, [progress, rate])
			else:
				screen.setGuageValues("GameHeaderPanel_GreatPersonBar", szText, 1, [0, 0])
	
	def updateMiddleShowHide(self, screen):
		interfaceController = CyInterface()
		isCityView = interfaceController.isCityScreenUp()
		screen.setVisible("splitPlotList", not isCityView)
		screen.setVisible("splitCityOutputAndRest", isCityView)
		screen.setVisible("WorldViewContainerBottomRightPanel", not isCityView)
		screen.setVisible("TurnMessageDisplay", not isCityView)
		
		# root
		#     splitGameHeaderAndRest (HSplit)
		#         GameHeaderPanel
		#             GameHeaderBar
		#             GameHeaderPanelBottom
		#         splitBottomPanel
		#             ...
		#     CityViewRoot (table, initially empty)
		#         GameHeaderBar
		#         splitBottomPanel
		
		screen.setVisible("CityViewRoot", isCityView)
		screen.setVisible("splitGameHeaderAndRest", not isCityView)
		screen.setParent("GameHeaderBar", "CityViewRoot" if isCityView else "GameHeaderPanel")
		screen.moveToFirst("GameHeaderBar")
		screen.setParent("splitBottomPanel", "CityViewRoot" if isCityView else "splitGameHeaderAndRest")
	
	# Default + City view left panel.
	def updateSliders(self, screen, ePlayer, pHeadSelectedCity):
		isCityScreen = CyInterface().isCityScreenUp()
		screen.setParent("SliderPanel", "CityOutputPanelTop" if isCityScreen else "MainLeftPanel")
		screen.moveToFirst("SliderPanel")
	
		for i in range(CommerceTypes.NUM_COMMERCE_TYPES):
			eCommerce = (i + 1) % int(CommerceTypes.NUM_COMMERCE_TYPES)
			prefix = "Slider%d_" % (eCommerce,)
			visible = gc.getPlayer(ePlayer).isCommerceFlexible(eCommerce) or (isCityScreen and eCommerce == CommerceTypes.COMMERCE_GOLD)
			screen.setVisible(prefix + "PercentText", visible)
			screen.setVisible(prefix + "Inc", visible)
			screen.setVisible(prefix + "Dec", visible)
			screen.setVisible(prefix + "Inc100", visible)
			screen.setVisible(prefix + "Dec100", visible)
			screen.setVisible(prefix + "RateText", visible)
			if visible:
				commerceChar = gc.getCommerceInfo(eCommerce).getChar()
				screen.setLabelText(prefix + "PercentText", u"%c: %d%% " % (commerceChar, gc.getPlayer(ePlayer).getCommercePercent(eCommerce)))
				if not isCityScreen:
					screen.setLabelText(prefix + "RateText", localText.getText("TXT_KEY_MISC_POS_GOLD_PER_TURN", (gc.getPlayer(ePlayer).getCommerceRate(CommerceTypes(eCommerce)), )))
				else:
					szBuffer = u" %3d.%02d" % (pHeadSelectedCity.getCommerceRate(eCommerce), pHeadSelectedCity.getCommerceRateTimes100(eCommerce) % 100)
					iHappiness = pHeadSelectedCity.getCommerceHappinessByType(eCommerce)
					if iHappiness != 0:
						szBuffer += u"+%d%c" % (abs(iHappiness), CyGame().getSymbolID(FontSymbols.HAPPY_CHAR if iHappiness >= 0 else FontSymbols.UNHAPPY_CHAR))

					screen.setLabelText(prefix + "RateText", szBuffer)
					screen.setLabelWidgetData(prefix + "RateText", WidgetTypes.WIDGET_COMMERCE_MOD_HELP, eCommerce, -1)

		if isCityScreen:
			iMaintenance = pHeadSelectedCity.getMaintenanceTimes100()
			screen.setLabelText("MaintainenceValue", u"-%d.%02d %c" % (iMaintenance/100, iMaintenance%100, gc.getCommerceInfo(CommerceTypes.COMMERCE_GOLD).getChar()))
			screen.setLabelWidgetData("MaintainenceValue", WidgetTypes.WIDGET_HELP_MAINTENANCE, -1, -1)

	# Default view left panel.
	def updatePlotListButtons(self, screen):
		screen.delByPrefix("PlotListBtn[]")
		
		interfaceController = CyInterface()
		pPlot = interfaceController.getSelectionPlot()
		interfaceController.cacheInterfacePlotUnits(pPlot)
		for i in range(interfaceController.getNumCachedInterfacePlotUnits()):
			pLoopUnit = interfaceController.getCachedInterfacePlotUnit(i)
			if pLoopUnit:
				screen.newPlotListUnitButton("PlotListPanel", "PlotListBtn[]%d" % (i,), pLoopUnit)
			
	def updateCityMiddlePanel(self, screen, pHeadSelectedCity):
		# Title
		pieces = []
		if (pHeadSelectedCity.isCapital()):
			pieces.append(u"%c" %(CyGame().getSymbolID(FontSymbols.STAR_CHAR)))
		elif (pHeadSelectedCity.isGovernmentCenter()):
			pieces.append(u"%c" %(CyGame().getSymbolID(FontSymbols.SILVER_STAR_CHAR)))

		if (pHeadSelectedCity.isPower()):
			pieces.append(u"%c" %(CyGame().getSymbolID(FontSymbols.POWER_CHAR)))
			
		pieces.append(u"%s: %d" %(pHeadSelectedCity.getName(), pHeadSelectedCity.getPopulation()))

		if (pHeadSelectedCity.isOccupation()):
			pieces.append(u"(%c:%d)" %(CyGame().getSymbolID(FontSymbols.OCCUPATION_CHAR), pHeadSelectedCity.getOccupationTimer()))
			
		screen.setLabelText("CityTitleLabel", u'  '.join(pieces))
		screen.setLabelWidgetData("CityTitleLabel", WidgetTypes.WIDGET_CITY_NAME, -1, -1)
		
		iFoodDifference = pHeadSelectedCity.foodDifference(True)
		iProductionDiffNoFood = pHeadSelectedCity.getCurrentProductionDifference(True, True)
		iProductionDiffJustFood = (pHeadSelectedCity.getCurrentProductionDifference(False, True) - iProductionDiffNoFood)
		
		## Food
		# Food rates
		if kHasBUG and not pHeadSelectedCity.isDisorder() and not pHeadSelectedCity.isFoodProduction():
			# BUG - Food Assist - start
			if (CityScreenOpt.isShowFoodAssist()):
				iFoodYield = pHeadSelectedCity.getYieldRate(YieldTypes.YIELD_FOOD)
				iFoodEaten = pHeadSelectedCity.foodConsumption(False, 0)
				if iFoodYield == iFoodEaten:
					szBuffer = localText.getText("INTERFACE_CITY_FOOD_STAGNATE", (iFoodYield, iFoodEaten))
				elif iFoodYield > iFoodEaten:
					szBuffer = localText.getText("INTERFACE_CITY_FOOD_GROW", (iFoodYield, iFoodEaten, iFoodYield - iFoodEaten))
				else:
					szBuffer = localText.getText("INTERFACE_CITY_FOOD_SHRINK", (iFoodYield, iFoodEaten, iFoodYield - iFoodEaten))
			else:
				szBuffer = u"%d%c - %d %c" %(pHeadSelectedCity.getYieldRate(YieldTypes.YIELD_FOOD), gc.getYieldInfo(YieldTypes.YIELD_FOOD).getChar(), pHeadSelectedCity.foodConsumption(False, 0), CyGame().getSymbolID(FontSymbols.EATEN_FOOD_CHAR))
			# BUG - Food Assist - end
			# draw label below
		else:
			szBuffer = u"%d %c" % (iFoodDifference, gc.getYieldInfo(YieldTypes.YIELD_FOOD).getChar())
		# draw label below

		screen.setLabelText("CityFoodProdPanel_Food_YieldInfo", szBuffer + u' ') #*BugDll.widget("WIDGET_FOOD_MOD_HELP", -1, -1) )
		
		# Guage
		# BUG - Food Assist - start
		foodBarLabel = ""
		if kHasBUG:
			if ( CityUtil.willGrowThisTurn(pHeadSelectedCity) or (iFoodDifference != 0) or not (pHeadSelectedCity.isFoodProduction() ) ):
				if (CityUtil.willGrowThisTurn(pHeadSelectedCity)):
					foodBarLabel = localText.getText("INTERFACE_CITY_GROWTH", ())
				elif (iFoodDifference > 0):
					foodBarLabel = localText.getText("INTERFACE_CITY_GROWING", (pHeadSelectedCity.getFoodTurnsLeft(), ))	
				elif (iFoodDifference < 0):
					if (CityScreenOpt.isShowFoodAssist()):
						iTurnsToStarve = pHeadSelectedCity.getFood() / -iFoodDifference + 1
						if iTurnsToStarve > 1:
							foodBarLabel = localText.getText("INTERFACE_CITY_SHRINKING", (iTurnsToStarve, ))
						else:
							foodBarLabel = localText.getText("INTERFACE_CITY_STARVING", ()) 
					else:
						foodBarLabel = localText.getText("INTERFACE_CITY_STARVING", ()) 
			# BUG - Food Assist - end
				else:
					foodBarLabel = localText.getText("INTERFACE_CITY_STAGNANT", ())	
		else:	
			if ( (iFoodDifference != 0) or not (pHeadSelectedCity.isFoodProduction() ) ):
				if (iFoodDifference > 0):
					foodBarLabel = localText.getText("INTERFACE_CITY_GROWING", (pHeadSelectedCity.getFoodTurnsLeft(), ))	
				elif (iFoodDifference < 0):
					foodBarLabel = localText.getText("INTERFACE_CITY_STARVING", ())	
				else:
					foodBarLabel = localText.getText("INTERFACE_CITY_STAGNANT", ())	
		
		if (iFoodDifference < 0):
			if ( pHeadSelectedCity.getFood() + iFoodDifference > 0 ):
				iDeltaFood = pHeadSelectedCity.getFood() + iFoodDifference
			else:
				iDeltaFood = 0
			if ( -iFoodDifference < pHeadSelectedCity.getFood() ):
				iExtraFood = -iFoodDifference
			else:
				iExtraFood = pHeadSelectedCity.getFood()
			screen.setGuageValues("CityFoodProdPanel_Food_Guage", foodBarLabel, pHeadSelectedCity.growthThreshold(), [iDeltaFood, 0, iExtraFood])
		else:
			screen.setGuageValues("CityFoodProdPanel_Food_Guage", foodBarLabel, pHeadSelectedCity.growthThreshold(), [pHeadSelectedCity.getFood(), iFoodDifference, 0])

		# Health
		if (pHeadSelectedCity.healthRate(False, 0) < 0):
			szBuffer = localText.getText("INTERFACE_CITY_HEALTH_BAD", (pHeadSelectedCity.goodHealth(), pHeadSelectedCity.badHealth(False), pHeadSelectedCity.healthRate(False, 0)))
		elif (pHeadSelectedCity.badHealth(False) > 0):
			szBuffer = localText.getText("INTERFACE_CITY_HEALTH_GOOD", (pHeadSelectedCity.goodHealth(), pHeadSelectedCity.badHealth(False)))
		else:
			szBuffer = localText.getText("INTERFACE_CITY_HEALTH_GOOD_NO_BAD", (pHeadSelectedCity.goodHealth(), ))
			
		screen.setLabelText("CityFoodProdPanel_Food_Health", u' ' + szBuffer)
		screen.setLabelWidgetData("CityFoodProdPanel_Food_Health", WidgetTypes.WIDGET_HELP_HEALTH, -1, -1)
		
		## Prod
		# Prod rate
		if (pHeadSelectedCity.isProductionProcess()):
			label = u"%d %c" %(pHeadSelectedCity.getYieldRate(YieldTypes.YIELD_PRODUCTION), gc.getYieldInfo(YieldTypes.YIELD_PRODUCTION).getChar())
		elif (pHeadSelectedCity.isFoodProduction() and (iProductionDiffJustFood > 0)):
			label = u"%d %c + %d %c" %(iProductionDiffJustFood, gc.getYieldInfo(YieldTypes.YIELD_FOOD).getChar(), iProductionDiffNoFood, gc.getYieldInfo(YieldTypes.YIELD_PRODUCTION).getChar())
		else:
			label = u"%d %c" %(iProductionDiffNoFood, gc.getYieldInfo(YieldTypes.YIELD_PRODUCTION).getChar())
			
		screen.setLabelText("CityFoodProdPanel_Prod_YieldInfo", label + u' ')
		screen.setLabelWidgetData("CityFoodProdPanel_Prod_YieldInfo", WidgetTypes.WIDGET_PRODUCTION_MOD_HELP, -1, -1)
		
		# Guage
		if (pHeadSelectedCity.getOrderQueueLength() > 0):
			if (pHeadSelectedCity.isProductionProcess()):
				szBuffer = pHeadSelectedCity.getProductionName()
# BUG - Whip Assist - start
			else:
				HURRY_WHIP = gc.getInfoTypeForString("HURRY_POPULATION")
				HURRY_BUY = gc.getInfoTypeForString("HURRY_GOLD")
				if (kHasBUG and CityScreenOpt.isShowWhipAssist() and pHeadSelectedCity.canHurry(HURRY_WHIP, False)):
					iHurryPop = pHeadSelectedCity.hurryPopulation(HURRY_WHIP)
					iOverflow = pHeadSelectedCity.hurryProduction(HURRY_WHIP) - pHeadSelectedCity.productionLeft()
					if CityScreenOpt.isWhipAssistOverflowCountCurrentProduction():
						iOverflow += pHeadSelectedCity.getCurrentProductionDifference(False, True)
					iMaxOverflow = max(pHeadSelectedCity.getProductionNeeded(), pHeadSelectedCity.getCurrentProductionDifference(False, False))
					iLost = max(0, iOverflow - iMaxOverflow)
					iOverflow = min(iOverflow, iMaxOverflow)
					iItemModifier = pHeadSelectedCity.getProductionModifier()
					iBaseModifier = pHeadSelectedCity.getBaseYieldRateModifier(YieldTypes.YIELD_PRODUCTION, 0)
					iTotalModifier = pHeadSelectedCity.getBaseYieldRateModifier(YieldTypes.YIELD_PRODUCTION, iItemModifier)
					iLost *= iBaseModifier
					iLost /= max(1, iTotalModifier)
					iOverflow = (iBaseModifier * iOverflow) / max(1, iTotalModifier)
					if iLost > 0:
						if pHeadSelectedCity.isProductionUnit():
							iGoldPercent = gc.getDefineINT("MAXED_UNIT_GOLD_PERCENT")
						elif pHeadSelectedCity.isProductionBuilding():
							iGoldPercent = gc.getDefineINT("MAXED_BUILDING_GOLD_PERCENT")
						elif pHeadSelectedCity.isProductionProject():
							iGoldPercent = gc.getDefineINT("MAXED_PROJECT_GOLD_PERCENT")
						else:
							iGoldPercent = 0
						iOverflowGold = iLost * iGoldPercent / 100
						szBuffer = localText.getText("INTERFACE_CITY_PRODUCTION_WHIP_PLUS_GOLD", (pHeadSelectedCity.getProductionNameKey(), pHeadSelectedCity.getProductionTurnsLeft(), iHurryPop, iOverflow, iOverflowGold))
					else:
						szBuffer = localText.getText("INTERFACE_CITY_PRODUCTION_WHIP", (pHeadSelectedCity.getProductionNameKey(), pHeadSelectedCity.getProductionTurnsLeft(), iHurryPop, iOverflow))
				elif (kHasBUG and CityScreenOpt.isShowWhipAssist() and pHeadSelectedCity.canHurry(HURRY_BUY, False)):
					iHurryCost = pHeadSelectedCity.hurryGold(HURRY_BUY)
					szBuffer = localText.getText("INTERFACE_CITY_PRODUCTION_BUY", (pHeadSelectedCity.getProductionNameKey(), pHeadSelectedCity.getProductionTurnsLeft(), iHurryCost))
				else:
					szBuffer = localText.getText("INTERFACE_CITY_PRODUCTION", (pHeadSelectedCity.getProductionNameKey(), pHeadSelectedCity.getProductionTurnsLeft()))
# BUG - Whip Assist - end
		else:
			szBuffer = u''

		if not pHeadSelectedCity.isProductionProcess():
			iNeeded = pHeadSelectedCity.getProductionNeeded()
			iStored = pHeadSelectedCity.getProduction()
			screen.setGuageValues("CityFoodProdPanel_Prod_Guage", szBuffer, iNeeded, [iStored, iProductionDiffNoFood, iProductionDiffJustFood])
		else:
			screen.setGuageValues("CityFoodProdPanel_Prod_Guage", szBuffer, 1, [0, 0, 0])
		
		# Happy
		if (pHeadSelectedCity.isDisorder()):
			szBuffer = u"%d%c" %(pHeadSelectedCity.angryPopulation(0), CyGame().getSymbolID(FontSymbols.ANGRY_POP_CHAR))
		elif (pHeadSelectedCity.angryPopulation(0) > 0):
			szBuffer = localText.getText("INTERFACE_CITY_UNHAPPY", (pHeadSelectedCity.happyLevel(), pHeadSelectedCity.unhappyLevel(0), pHeadSelectedCity.angryPopulation(0)))
		elif (pHeadSelectedCity.unhappyLevel(0) > 0):
			szBuffer = localText.getText("INTERFACE_CITY_HAPPY", (pHeadSelectedCity.happyLevel(), pHeadSelectedCity.unhappyLevel(0)))
		else:
			szBuffer = localText.getText("INTERFACE_CITY_HAPPY_NO_UNHAPPY", (pHeadSelectedCity.happyLevel(), ))

# BUG - Anger Display - start
		if (kHasBUG and CityScreenOpt.isShowAngerCounter() and pHeadSelectedCity.getTeam() == gc.getGame().getActiveTeam()):
			iAngerTimer = max(pHeadSelectedCity.getHurryAngerTimer(), pHeadSelectedCity.getConscriptAngerTimer())
			if iAngerTimer > 0:
				szBuffer += u" (%i)" % iAngerTimer
# BUG - Anger Display - end

		screen.setLabelText("CityFoodProdPanel_Prod_Happiness", u' ' + szBuffer)
		screen.setLabelWidgetData("CityFoodProdPanel_Prod_Happiness", WidgetTypes.WIDGET_HELP_HAPPINESS, -1, -1)
	
	# City view left panel.
	def updateCityTrade(self, screen, pHeadSelectedCity):
		if CyInterface().isCityScreenUp():
			iNumTradeRoutes = 0
			prefix = "TradeRow[]"
			screen.delByPrefix(prefix)
			for i in range(gc.getDefineINT("MAX_TRADE_ROUTES")):
				pLoopCity = pHeadSelectedCity.getTradeCity(i)
				if pLoopCity and pLoopCity.getOwner() >= 0:
					player = gc.getPlayer(pLoopCity.getOwner())
					szLeftBuffer = u"<color=%d,%d,%d,%d>%s</color>" %(player.getPlayerTextColorR(), player.getPlayerTextColorG(), player.getPlayerTextColorB(), player.getPlayerTextColorA(), pLoopCity.getName() )
					szRightBuffer = u""

					for j in range(YieldTypes.NUM_YIELD_TYPES):
						iTradeProfit = pHeadSelectedCity.calculateTradeYield(j, pHeadSelectedCity.calculateTradeProfit(pLoopCity))

						if iTradeProfit != 0:
							szRightBuffer += u"%+d %c" % (iTradeProfit, gc.getYieldInfo(j).getChar())
						# no negatives?

					rowPrefix = prefix + "%i_" % (iNumTradeRoutes,)
					screen.newLabel("TradePanel", rowPrefix + "Name", szLeftBuffer)
					screen.newLabel("TradePanel", rowPrefix + "Value", szRightBuffer)
					screen.setLabelWidgetData(rowPrefix + "Name", WidgetTypes.WIDGET_HELP_TRADE_ROUTE_CITY, i, -1)
					screen.setLabelWidgetData(rowPrefix + "Value", WidgetTypes.WIDGET_HELP_TRADE_ROUTE_CITY, i, -1)
					iNumTradeRoutes += 1
					
	def updateCityBuildings(self, screen, pHeadSelectedCity):
		if not CyInterface().isCityScreenUp():
			return
		iNumTradeRoutes = 0
		prefix = "CityBuildingRow[]"
		screen.delByPrefix(prefix)
		iNumBuildings = 0
		for i in range(gc.getNumBuildingInfos()):
			if pHeadSelectedCity.getNumBuilding(i) <= 0:
				continue
			for k in range(pHeadSelectedCity.getNumBuilding(i)):
				valueStrs = []

				if pHeadSelectedCity.getNumActiveBuilding(i) > 0:
					iHealth = pHeadSelectedCity.getBuildingHealth(i)
					if iHealth != 0:
						valueStrs.append(u"+%d %c" % (abs(iHealth), CyGame().getSymbolID(FontSymbols.HEALTHY_CHAR if iHealth > 0 else FontSymbols.UNHEALTHY_CHAR)))

					iHappiness = pHeadSelectedCity.getBuildingHappiness(i)
					if iHappiness != 0:
						valueStrs.append(u"+%d %c" % (abs(iHappiness), CyGame().getSymbolID(FontSymbols.HAPPY_CHAR if iHappiness > 0 else FontSymbols.UNHAPPY_CHAR)))

					for j in range(YieldTypes.NUM_YIELD_TYPES):
						iYield = gc.getBuildingInfo(i).getYieldChange(j) + pHeadSelectedCity.getNumBuilding(i) * pHeadSelectedCity.getBuildingYieldChange(gc.getBuildingInfo(i).getBuildingClassType(), j)
						if iYield != 0:
							valueStrs.append(u"%+d %c" % (iYield, gc.getYieldInfo(j).getChar()))

				for j in range(CommerceTypes.NUM_COMMERCE_TYPES):
					iCommerce = pHeadSelectedCity.getBuildingCommerceByBuilding(j, i) / pHeadSelectedCity.getNumBuilding(i)

					if iCommerce != 0:
						valueStrs.append(u"%+d %c" % (iCommerce, gc.getCommerceInfo(j).getChar()))

				rowPrefix = prefix + "%i_" % (iNumBuildings,)
				screen.newLabel("CityBuildingsPanel", rowPrefix + "_Name", gc.getBuildingInfo(i).getDescription() + " ")
				screen.newLabel("CityBuildingsPanel", rowPrefix + "_Value", ", ".join(valueStrs))
				screen.setLabelWidgetData(rowPrefix + "_Name", WidgetTypes.WIDGET_HELP_BUILDING, i, -1);
				screen.setLabelWidgetData(rowPrefix + "_Value", WidgetTypes.WIDGET_HELP_BUILDING, i, -1);
				iNumBuildings += 1
					
	def updateCityCultureBars(self, screen, pHeadSelectedCity):
		if not CyInterface().isCityScreenUp():
			return
			
		plot = pHeadSelectedCity.plot()
		colours = []
		ratios = []
		for h in range(gc.getMAX_PLAYERS()):
			if gc.getPlayer(h).isAlive():
				ratio = plot.getCulture(h)
				if ratio != 0:
					colours.append(gc.getPlayerColorInfo(gc.getPlayer(h).getPlayerColor()).getTextColorType())
					ratios.append(ratio)
		screen.setGuageColours("NationalityGuage", colours)
		label = u"%d%% %s" % (plot.calculateCulturePercent(pHeadSelectedCity.getOwner()), gc.getPlayer(pHeadSelectedCity.getOwner()).getCivilizationAdjective(0))
		screen.setGuageValues("NationalityGuage", label, sum(ratios), ratios)
		
			
		label = ""
		if pHeadSelectedCity.getCultureLevel != CultureLevelTypes.NO_CULTURELEVEL:
			iRate = pHeadSelectedCity.getCommerceRateTimes100(CommerceTypes.COMMERCE_CULTURE)
			if iRate % 100 == 0:
				label = localText.getText("INTERFACE_CITY_COMMERCE_RATE", (gc.getCommerceInfo(CommerceTypes.COMMERCE_CULTURE).getChar(), gc.getCultureLevelInfo(pHeadSelectedCity.getCultureLevel()).getTextKey(), iRate/100))
			else:
				szRate = u"+%d.%02d" % (iRate / 100, iRate % 100)
				label = localText.getText("INTERFACE_CITY_COMMERCE_RATE_FLOAT", (gc.getCommerceInfo(CommerceTypes.COMMERCE_CULTURE).getChar(), gc.getCultureLevelInfo(pHeadSelectedCity.getCultureLevel()).getTextKey(), szRate))
				
		maxValue = pHeadSelectedCity.getCultureThreshold()
		stored = (pHeadSelectedCity.getCultureTimes100(pHeadSelectedCity.getOwner()) + 50) / 100
		rate = pHeadSelectedCity.getCommerceRate(CommerceTypes.COMMERCE_CULTURE)
		screen.setGuageValues("CultureGuage", label, maxValue, [stored, rate])
	
	# Religions, corps, bonuses, specialists
	def updateCityInputPanel(self, screen, pHeadSelectedCity):
		if not CyInterface().isCityScreenUp():
			return
			
		screen.delByPrefix("Religion[]")
		hasAny = False
		for i in range(gc.getNumReligionInfos()):
			if pHeadSelectedCity.isHasReligion(i):
				if pHeadSelectedCity.isHolyCityByType(i):
					label = u"%c" % (gc.getReligionInfo(i).getHolyCityChar(),)
				else:
					label = u"%c" % (gc.getReligionInfo(i).getChar(),)

				values = []
				for j in range(CommerceTypes.NUM_COMMERCE_TYPES):
					iCommerce = pHeadSelectedCity.getReligionCommerceByReligion(j, i)
					if iCommerce != 0:
						values.append(u"%+d %c" %(iCommerce, gc.getCommerceInfo(j).getChar()))

				iHappiness = pHeadSelectedCity.getReligionHappiness(i)

				if iHappiness != 0:
					values.append(u"+%d %c" % (abs(iHappiness), CyGame().getSymbolID(FontSymbols.HAPPY_CHAR if iHappiness > 0 else FontSymbols.UNHAPPY_CHAR)))
				
				rowPrefix = "Religion[]%d" % (i,)
				screen.newLabel("ReligionPanel", rowPrefix + "_Name", label)
				screen.newLabel("ReligionPanel", rowPrefix + "_Value", u", ".join(values))
				screen.setLabelWidgetData(rowPrefix + "_Name", WidgetTypes.WIDGET_HELP_RELIGION_CITY, i, -1)
				screen.setLabelWidgetData(rowPrefix + "_Value", WidgetTypes.WIDGET_HELP_RELIGION_CITY, i, -1)
				hasAny = True
		if not hasAny:
			screen.newLabel("ReligionPanel", "Religion[]0", "None")
			screen.newLabel("ReligionPanel", "Religion[]1", "")

		screen.delByPrefix("Corporation[]")
		hasAny = False
		for i in range(gc.getNumCorporationInfos()):
			if pHeadSelectedCity.isHasCorporation(i):
				if pHeadSelectedCity.isHeadquartersByType(i):
					label = u"%c" % (gc.getCorporationInfo(i).getHeadquarterChar(),)
				else:
					label = u"%c" % (gc.getCorporationInfo(i).getChar(),)
				
				values = []
				for j in range(YieldTypes.NUM_YIELD_TYPES):
					iYield = pHeadSelectedCity.getCorporationYieldByCorporation(j, i)
					if iYield != 0:
						values.append(u"%+d %c" % (iYield, gc.getYieldInfo(j).getChar(),))
				
				for j in range(CommerceTypes.NUM_COMMERCE_TYPES):
					iCommerce = pHeadSelectedCity.getCorporationCommerceByCorporation(j, i)
					if iCommerce != 0:
						values.append(u"%+d %c" % (iYield, gc.getCommerceInfo(j).getChar(),))

				rowPrefix = "Corporation[]%d" % (i,)
				screen.newLabel("CorporationPanel", rowPrefix + "_Name", label)
				screen.newLabel("CorporationPanel", rowPrefix + "_Value", u", ".join(values))
				screen.setLabelWidgetData(rowPrefix + "_Name", WidgetTypes.WIDGET_HELP_CORPORATION_CITY, i, -1)
				screen.setLabelWidgetData(rowPrefix + "_Value", WidgetTypes.WIDGET_HELP_CORPORATION_CITY, i, -1)
				hasAny = True
		if not hasAny:
			screen.newLabel("CorporationPanel", "Corporation[]0", "None")
			screen.newLabel("CorporationPanel", "Corporation[]1", "")
		
		screen.delByPrefix("Bonus[]")
		hasAny = False
		for i in range(gc.getNumBonusInfos()):
			if pHeadSelectedCity.hasBonus(i):
				label = u"%c" % (gc.getBonusInfo(i).getChar())

				if pHeadSelectedCity.getNumBonuses(i) != 1:
					label += u" (%d)" % (pHeadSelectedCity.getNumBonuses(i))

				values = []
				iHappiness = pHeadSelectedCity.getBonusHappiness(i)
				if iHappiness != 0:
					values.append(u"+%d %c" % (abs(iHappiness), CyGame().getSymbolID(FontSymbols.HAPPY_CHAR if iHappiness > 0 else FontSymbols.UNHAPPY_CHAR)))
					
				iHealth = pHeadSelectedCity.getBonusHealth(i)
				if iHealth != 0:
					values.append(u"+%d %c" % (abs(iHealth), CyGame().getSymbolID(FontSymbols.HEALTHY_CHAR if iHealth > 0 else FontSymbols.UNHEALTHY_CHAR)))

				rowPrefix = "Bonus[]%d" % (i,)
				screen.newLabel("BonusesPanel", rowPrefix + "_Name", label)
				screen.newLabel("BonusesPanel", rowPrefix + "_Value", u", ".join(values))
				screen.setLabelWidgetData(rowPrefix + "_Name", WidgetTypes.WIDGET_PEDIA_JUMP_TO_BONUS, i, -1)
				screen.setLabelWidgetData(rowPrefix + "_Value", WidgetTypes.WIDGET_PEDIA_JUMP_TO_BONUS, i, -1)
				hasAny = True
		if not hasAny:
			screen.newLabel("BonusesPanel", "Bonus[]0", "None")
			screen.newLabel("BonusesPanel", "Bonus[]1", "")
			
		prefix = "SpecialistCell[]"
		screen.delByPrefix(prefix)
		numAngryCitizens = pHeadSelectedCity.angryPopulation(0)
		
		countFormat = "%3d "
		
		for i in reversed(range(gc.getNumSpecialistInfos())):
			if not gc.getSpecialistInfo(i).isVisible():
				continue
		
			n = pHeadSelectedCity.getSpecialistCount(i)
			
			adjustmentCount = max(n, pHeadSelectedCity.getForceSpecialistCount(i)) if pHeadSelectedCity.isCitizensAutomated() else n
			
			showButtons = pHeadSelectedCity.getOwner() == gc.getGame().getActivePlayer() or gc.getGame().isDebugMode()
			showIncButton = showButtons and (pHeadSelectedCity.isSpecialistValid(i, 1) and (pHeadSelectedCity.isCitizensAutomated() or adjustmentCount < (pHeadSelectedCity.getPopulation() + pHeadSelectedCity.totalFreeSpecialists())))
			showDecButton = showButtons and adjustmentCount > 0
			
			#if n or showIncButton or showDecButton:
			rowPrefix = prefix + "Specialist" + str(i)
			screen.newLabel("SpecialistsPanel", rowPrefix + "Name", localText.getText(gc.getSpecialistInfo(i).getTextKey(), ()))
			screen.newLabel("SpecialistsPanel", rowPrefix + "Count", countFormat % (n,) if n else '')
			screen.setLabelWidgetData(rowPrefix + "Name", WidgetTypes.WIDGET_DISABLED_CITIZEN, i, -1)
			screen.setLabelWidgetData(rowPrefix + "Count", WidgetTypes.WIDGET_DISABLED_CITIZEN, i, -1)

			if showIncButton:
				screen.newActionButton("SpecialistsPanel", rowPrefix + "Dec", WidgetTypes.WIDGET_CHANGE_SPECIALIST, i, 1, HeckTuiBorderStyle.NONE)
			else:
				screen.newLabel("SpecialistsPanel", rowPrefix + "Dummy0", "")

			if showDecButton:
				screen.newActionButton("SpecialistsPanel", rowPrefix + "Inc", WidgetTypes.WIDGET_CHANGE_SPECIALIST, i, -1, HeckTuiBorderStyle.NONE)
			else:
				screen.newLabel("SpecialistsPanel", rowPrefix + "Dummy1", "")
				
		if numAngryCitizens != 0:
			screen.newLabel("SpecialistsPanel", prefix + "AngryName", localText.getText("TXT_KEY_MISC_ANGRY_CITIZEN", ()))
			screen.newLabel("SpecialistsPanel", prefix + "AngryCount", countFormat % (numAngryCitizens,))
			screen.newLabel("SpecialistsPanel", prefix + "AngryDummy0", "")
			screen.newLabel("SpecialistsPanel", prefix + "AngryDummy1", "")
			for suffix in ("AngryName", "AngryCount", "AngryDummy0", "AngryDummy1"):
				screen.setLabelWidgetData(prefix + suffix, WidgetTypes.WIDGET_ANGRY_CITIZEN, -1, -1)
			
		for i in range(gc.getNumSpecialistInfos()):
			n = pHeadSelectedCity.getFreeSpecialistCount(i)
			if n:
				rowPrefix = prefix + "FreeSpecialist" + str(i)
				screen.newLabel("SpecialistsPanel", rowPrefix + "Name", localText.getText(gc.getSpecialistInfo(i).getTextKey(), ()))
				screen.newLabel("SpecialistsPanel", rowPrefix + "Count", countFormat % (n,))
				screen.newLabel("SpecialistsPanel", rowPrefix + "Dummy0", "")
				screen.newLabel("SpecialistsPanel", rowPrefix + "Dummy1", "")
				for suffix in ("Name", "Count", "Dummy0", "Dummy1"):
					screen.setLabelWidgetData(rowPrefix + suffix, WidgetTypes.WIDGET_FREE_CITIZEN, i, -1)
				
		gppStored = pHeadSelectedCity.getGreatPeopleProgress()
		gppRate = pHeadSelectedCity.getGreatPeopleRate()
		visible = gppStored or gppRate
		screen.setVisible("SpecialistsGuage", visible)
		if visible:
			if kHasBUG:
				label = GPUtil.getGreatPeopleText(pHeadSelectedCity, GPUtil.getCityTurns(pHeadSelectedCity), 230, MainOpt.isGPBarTypesNone(), MainOpt.isGPBarTypesOne(), False)
			else:
				label = localText.getText("INTERFACE_CITY_GREATPEOPLE_RATE", (CyGame().getSymbolID(FontSymbols.GREAT_PEOPLE_CHAR), pHeadSelectedCity.getGreatPeopleRate()))
			cost = gc.getPlayer(pHeadSelectedCity.getOwner()).greatPeopleThreshold(False)
			screen.setGuageValues("SpecialistsGuage", label, cost, [gppStored, gppRate])
			
	def updateSelectionInfo(self, screen, pHeadSelectedCity):
		interfaceController = CyInterface()
		pHeadSelectedUnit = interfaceController.getHeadSelectedUnit()
		prefix = "SelInfo_"
		screen.delByPrefix(prefix + "ProdOrderRow_")
		screen.delByPrefix(prefix + "StackList_")
		screen.delByPrefix(prefix + "OrderList_")
		screen.delByPrefix(prefix + "Stats_")
		screen.hide("SelInfo_StackTitleButton")
		
		screen.hide("DefaultOperationsPanel")
		screen.hide("CityOperationsScrollPanel")
		
		if pHeadSelectedCity is not None:
			self.updateCitySelInfo(screen, prefix, pHeadSelectedCity)
		else:
			screen.show("DefaultOperationsPanel")
			if pHeadSelectedUnit is not None:
				self.updateUnitSelInfo(screen, prefix, pHeadSelectedUnit)
			else:
				pass
			
	def updateCitySelInfo(self, screen, prefix, pHeadSelectedCity):
		
		for i in range(CyInterface().getNumOrdersQueued()):
			szLeftBuffer = u""
			szRightBuffer = u""
			
			if CyInterface().getOrderNodeType(i) == OrderTypes.ORDER_TRAIN:
				szLeftBuffer = gc.getUnitInfo(CyInterface().getOrderNodeData1(i)).getDescription()
				szRightBuffer = "(" + str(pHeadSelectedCity.getUnitProductionTurnsLeft(CyInterface().getOrderNodeData1(i), i)) + ")"
				if CyInterface().getOrderNodeSave(i):
					szLeftBuffer = u"*" + szLeftBuffer
			elif CyInterface().getOrderNodeType(i) == OrderTypes.ORDER_CONSTRUCT:
				szLeftBuffer = gc.getBuildingInfo(CyInterface().getOrderNodeData1(i)).getDescription()
				szRightBuffer = "(" + str(pHeadSelectedCity.getBuildingProductionTurnsLeft(CyInterface().getOrderNodeData1(i), i)) + ")"
			elif CyInterface().getOrderNodeType(i) == OrderTypes.ORDER_CREATE:
				szLeftBuffer = gc.getProjectInfo(CyInterface().getOrderNodeData1(i)).getDescription()
				szRightBuffer = "(" + str(pHeadSelectedCity.getProjectProductionTurnsLeft(CyInterface().getOrderNodeData1(i), i)) + ")"
			elif CyInterface().getOrderNodeType(i) == OrderTypes.ORDER_MAINTAIN:
				szLeftBuffer = gc.getProcessInfo(CyInterface().getOrderNodeData1(i)).getDescription()

			rowPrefix = prefix + "ProdOrderRow_%d" % (i,)
			screen.newEmptyActionButton("SelInfoPanel", rowPrefix, WidgetTypes.WIDGET_HELP_SELECTED, i, -1)
			screen.setTableLayout(rowPrefix, self.nameValueTable)
			screen.newClickThroughLabel(rowPrefix, rowPrefix + "_Name", szLeftBuffer)
			screen.newClickThroughLabel(rowPrefix, rowPrefix + "_Value", szRightBuffer)
			
		screen.show("CityOperationsScrollPanel")
		screen.setEnabled("ConscriptButton", pHeadSelectedCity.canConscript())
		for i in range(gc.getNumHurryInfos()):
			name = "HurryButton" + str(i)
			screen.setEnabled(name, pHeadSelectedCity.canHurry(i, False))
			iHurryPopulation = pHeadSelectedCity.hurryPopulation(i)
			iHurryGold = pHeadSelectedCity.hurryGold(i)
			if iHurryPopulation != 0:
				screen.setActionButtonLabel(name, localText.getText("TXT_KEY_MISC_HURRY_POP", (iHurryPopulation,)))
			elif iHurryGold != 0:
				screen.setActionButtonLabel(name, localText.getText("TXT_KEY_MISC_HURRY_GOLD", (iHurryGold,)))
			else:
				screen.setActionButtonLabel(name, localText.getText("TXT_KEY_MISC_HURRY_PROD", (pHeadSelectedCity.getProductionNameKey(),)))
			
		screen.setCheckBoxValue("AutomateProduction", pHeadSelectedCity.isProductionAutomated())
		screen.setCheckBoxValue("AutomateCitizens", pHeadSelectedCity.isCitizensAutomated())
		

		for i in range(6):
			screen.setCheckBoxValue("EmphasiseButton" + str(i), pHeadSelectedCity.AI_isEmphasize(i))
			
			
	def updateUnitSelInfo(self, screen, prefix, pHeadSelectedUnit):
		screen.show("SelInfo_StackTitleButton")
		
		#screen.newPanel("SelInfoPanel", "SelInfo_LabelRows")
		
		isConsistentGroupSelection = CyInterface().mirrorsSelectionGroup()
		numUnits = CyInterface().getLengthSelectionList()
		group = pHeadSelectedUnit.getGroup() if isConsistentGroupSelection else None
		numOrders = group.getLengthMissionQueue() if group is not None else 0
		isShowOrders = isConsistentGroupSelection and numOrders > 1
		isShowUnitList = numUnits > 1 and not isShowOrders
		
		if numUnits <= 0:
			pass
		elif numUnits == 1:
			screen.setLabelText("SelInfo_StackTitleLabel", localText.getText("INTERFACE_PANE_UNIT_NAME", (pHeadSelectedUnit.getName(), )))

		else:
			# fortsnek: getSelectionUnit is O(n). Cache this list for isShowUnitList too!
			unitList = [CyInterface().getSelectionUnit(i) for i in range(numUnits)]
		
			# BUG - Stack Movement Display - start
			# Modified by fortsnek
			szBuffer = localText.getText("TXT_KEY_UNIT_STACK", (CyInterface().getLengthSelectionList(), ))
			if kHasBUG and MainOpt.isShowStackMovementPoints():
				moves = [pUnit.movesLeft() for pUnit in unitList]
				iMinMoves = min(moves)
				iMaxMoves = max(moves)
				
				if iMinMoves == iMaxMoves:
					fMinMoves = float(iMinMoves) / gc.getMOVE_DENOMINATOR()
					szBuffer += u" %.1f %c" % (fMinMoves, CyGame().getSymbolID(FontSymbols.MOVES_CHAR))
				else:
					fMinMoves = float(iMinMoves) / gc.getMOVE_DENOMINATOR()
					fMaxMoves = float(iMaxMoves) / gc.getMOVE_DENOMINATOR()
					szBuffer += u" %.1f - %.1f %c" % (fMinMoves, fMaxMoves, CyGame().getSymbolID(FontSymbols.MOVES_CHAR))
			# BUG - Stack Movement Display - end
			
			if isShowUnitList:
				# fortsnek: I expect this implementation to be faster, as the vanilla implementation calls CyInterface().countEntities(i) for each gc unit info, which is O(n^2) in total.
				key = lambda unit: unit.getUnitType()
				unitList.sort(key=key)
			
				rowPrefix = prefix + "StackList_"
			
				for unitTypeI, g in itertools.groupby(unitList, key):
					iCount = sum(1 for unit in g)
					szLeftBuffer = gc.getUnitInfo(unitTypeI).getDescription()
					szRightBuffer = u"(" + str(iCount) + u")" if iCount != 1 else u""
					screen.newLabel("SelInfo_LabelRows", rowPrefix + "_Name" + str(unitTypeI), szLeftBuffer) #, "", WidgetTypes.WIDGET_HELP_SELECTED, i, -1, CvUtil.FONT_LEFT_JUSTIFY )
					screen.newLabel("SelInfo_LabelRows", rowPrefix + "_Value" + str(unitTypeI), szRightBuffer) #, "", WidgetTypes.WIDGET_HELP_SELECTED, i, -1, CvUtil.FONT_RIGHT_JUSTIFY )
			
			screen.setLabelText("SelInfo_StackTitleLabel", szBuffer)
			
		if isShowOrders:
			rowPrefix = prefix + "OrderList_"
			for i in range(numOrders):
				szLeftBuffer = u""
				szRightBuffer = u""
			
				if gc.getMissionInfo(group.getMissionType(i)).isBuild() and i == 0:
					szLeftBuffer = gc.getBuildInfo(group.getMissionData1(i)).getDescription()
					szRightBuffer = localText.getText("INTERFACE_CITY_TURNS", (group.plot().getBuildTurnsLeft(group.getMissionData1(i), 0, 0), ))								
				else:
					szLeftBuffer = u"%s..." % (gc.getMissionInfo(group.getMissionType(i)).getDescription())
					
				screen.newLabel("SelInfo_LabelRows", rowPrefix + "_Name" + str(i), szLeftBuffer) #, "", WidgetTypes.WIDGET_HELP_SELECTED, i, -1, CvUtil.FONT_LEFT_JUSTIFY )
				screen.newLabel("SelInfo_LabelRows", rowPrefix + "_Value" + str(i), szRightBuffer) #, "", WidgetTypes.WIDGET_HELP_SELECTED, i, -1, CvUtil.FONT_RIGHT_JUSTIFY )
		elif numUnits == 1:
			# List single unit stats.
			rowPrefix = prefix + "Stats_"
			
			szBuffer = u""

			szLeftBuffer = u""
			szRightBuffer = u""
			
			if (pHeadSelectedUnit.getDomainType() == DomainTypes.DOMAIN_AIR):
				if (pHeadSelectedUnit.airBaseCombatStr() > 0):
					szLeftBuffer = localText.getText("INTERFACE_PANE_AIR_STRENGTH", ())
					if (pHeadSelectedUnit.isFighting()):
						szRightBuffer = u"?/%d" %(pHeadSelectedUnit.airBaseCombatStr(),)
					elif (pHeadSelectedUnit.isHurt()):
						szRightBuffer = u"%.1f/%d" %(((float(pHeadSelectedUnit.airBaseCombatStr() * pHeadSelectedUnit.currHitPoints())) / (float(pHeadSelectedUnit.maxHitPoints()))), pHeadSelectedUnit.airBaseCombatStr(),)
					else:
						szRightBuffer = u"%d" %(pHeadSelectedUnit.airBaseCombatStr(),)
			else:
				if (pHeadSelectedUnit.canFight()):
					szLeftBuffer = localText.getText("INTERFACE_PANE_STRENGTH", ())
					if (pHeadSelectedUnit.isFighting()):
						szRightBuffer = u"?/%d" %(pHeadSelectedUnit.baseCombatStr(),)
					elif (pHeadSelectedUnit.isHurt()):
						szRightBuffer = u"%.1f/%d" %(((float(pHeadSelectedUnit.baseCombatStr() * pHeadSelectedUnit.currHitPoints())) / (float(pHeadSelectedUnit.maxHitPoints()))), pHeadSelectedUnit.baseCombatStr(),)
					else:
						szRightBuffer = u"%d" %(pHeadSelectedUnit.baseCombatStr(),)

			if len(szLeftBuffer):
				screen.newLabel("SelInfo_LabelRows", rowPrefix + "_StrengthName", szLeftBuffer)
				screen.newLabel("SelInfo_LabelRows", rowPrefix + "_StrengthValue", szRightBuffer)

			szLeftBuffer = u""
			szRightBuffer = u""
			
# BUG - Unit Movement Fraction - start
			szLeftBuffer = localText.getText("INTERFACE_PANE_MOVEMENT", ())
			if kHasBUG and MainOpt.isShowUnitMovementPointsFraction():
				szRightBuffer = u"%d" %(pHeadSelectedUnit.baseMoves(),)
				if (pHeadSelectedUnit.movesLeft() == 0):
					szRightBuffer = u"0/" + szRightBuffer
				elif (pHeadSelectedUnit.movesLeft() == pHeadSelectedUnit.baseMoves() * gc.getMOVE_DENOMINATOR()):
					pass
				else:
					fCurrMoves = float(pHeadSelectedUnit.movesLeft()) / gc.getMOVE_DENOMINATOR()
					szRightBuffer = (u"%.1f/" % fCurrMoves) + szRightBuffer
			else:
				if ( (pHeadSelectedUnit.movesLeft() % gc.getMOVE_DENOMINATOR()) > 0 ):
					iDenom = 1
				else:
					iDenom = 0
				iCurrMoves = ((pHeadSelectedUnit.movesLeft() / gc.getMOVE_DENOMINATOR()) + iDenom )
				if (pHeadSelectedUnit.baseMoves() == iCurrMoves):
					szRightBuffer = u"%d" %(pHeadSelectedUnit.baseMoves(),)
				else:
					szRightBuffer = u"%d/%d" %(iCurrMoves, pHeadSelectedUnit.baseMoves(),)
# BUG - Unit Movement Fraction - end

			screen.newLabel("SelInfo_LabelRows", rowPrefix + "_MovementName", szLeftBuffer)
			screen.newLabel("SelInfo_LabelRows", rowPrefix + "_MovementValue", szRightBuffer)

			if (pHeadSelectedUnit.getLevel() > 0):
				szLeftBuffer = localText.getText("INTERFACE_PANE_LEVEL", ())
				szRightBuffer = u"%d" %(pHeadSelectedUnit.getLevel())
				screen.newLabel("SelInfo_LabelRows", rowPrefix + "_LevelName", szLeftBuffer)
				screen.newLabel("SelInfo_LabelRows", rowPrefix + "_LevelValue", szRightBuffer)

			if ((pHeadSelectedUnit.getExperience() > 0) and not pHeadSelectedUnit.isFighting()):
				szLeftBuffer = localText.getText("INTERFACE_PANE_EXPERIENCE", ())
				szRightBuffer = u"(%d/%d)" %(pHeadSelectedUnit.getExperience(), pHeadSelectedUnit.experienceNeeded())
				screen.newLabel("SelInfo_LabelRows", rowPrefix + "_ExpName", szLeftBuffer)
				screen.newLabel("SelInfo_LabelRows", rowPrefix + "_ExpValue", szRightBuffer)

			for i in range(gc.getNumPromotionInfos()):
				if (pHeadSelectedUnit.isHasPromotion(i)):
					screen.newLabel("SelInfo_LabelRows", rowPrefix + "_PromoName" + str(i), gc.getPromotionInfo(i).getDescription())
					screen.newLabel("SelInfo_LabelRows", rowPrefix + "_PromoValue" + str(i), "")

	# Other default + city view stuff.
	def updateSelectionButtons(self, screen, pHeadSelectedCity):
		self.updateActionListButtons(screen, pHeadSelectedCity)
	
	def updateActionListButtons(self, screen, pHeadSelectedCity):
		interfaceController = CyInterface()
		pHeadSelectedUnit = interfaceController.getHeadSelectedUnit()
		
		prefix = "ActionListBtn[]"
		screen.delByPrefix(prefix)
		if pHeadSelectedCity:
			if pHeadSelectedCity.getOwner() == gc.getGame().getActivePlayer() or gc.getGame().isDebugMode():
				# Units to construct
				for i in range(g_NumUnitClassInfos):
					eLoopUnit = gc.getCivilizationInfo(pHeadSelectedCity.getCivilizationType()).getCivilizationUnits(i)
					if pHeadSelectedCity.canTrain(eLoopUnit, False, True):
						name = "%s[]Unit_%d" % (prefix, i)
						screen.newActionButton("ActionPanel", name, WidgetTypes.WIDGET_TRAIN, i, -1)
						if not pHeadSelectedCity.canTrain(eLoopUnit, False, False):
							screen.disable(name)

				# Buildings to construct
				for i in range(g_NumBuildingClassInfos):
					if not isLimitedWonderClass(i):
						eLoopBuilding = gc.getCivilizationInfo(pHeadSelectedCity.getCivilizationType()).getCivilizationBuildings(i)
						if pHeadSelectedCity.canConstruct(eLoopBuilding, False, True, False):
							name = "%s[]Building_%d" % (prefix, i)
							screen.newActionButton("ActionPanel", name, WidgetTypes.WIDGET_CONSTRUCT, i, -1)
							if not pHeadSelectedCity.canConstruct(eLoopBuilding, False, False, False):
								screen.disable(name)

				# Wonders to construct
				for i in range(g_NumBuildingClassInfos):
					if isLimitedWonderClass(i):
						eLoopBuilding = gc.getCivilizationInfo(pHeadSelectedCity.getCivilizationType()).getCivilizationBuildings(i)
						if pHeadSelectedCity.canConstruct(eLoopBuilding, False, True, False):
							name = "%s[]Wonder_%d" % (prefix, i)
							screen.newActionButton("ActionPanel", name, WidgetTypes.WIDGET_CONSTRUCT, i, -1)							
							if not pHeadSelectedCity.canConstruct(eLoopBuilding, False, False, False):
								screen.disable(name)

				# Projects
				for i in range(g_NumProjectInfos):
					if pHeadSelectedCity.canCreate(i, False, True):
						name = "%s[]Project_%d" % (prefix, i)
						screen.newActionButton("ActionPanel", name, WidgetTypes.WIDGET_CREATE, i, -1)						
						if not pHeadSelectedCity.canCreate(i, False, False):
							screen.disable(name)

				# Processes
				for i in range(g_NumProcessInfos):
					if pHeadSelectedCity.canMaintain(i, False):
						name = "%s[]Process_%d" % (prefix, i)
						screen.newActionButton("ActionPanel", name, WidgetTypes.WIDGET_MAINTAIN, i, -1)

		elif pHeadSelectedUnit:
			if interfaceController.getInterfaceMode() == InterfaceModeTypes.INTERFACEMODE_SELECTION:
				if pHeadSelectedUnit.getOwner() == gc.getGame().getActivePlayer():
					actions = interfaceController.getActionsToShow()
					for actionIndex in actions:
						name = "%s%d" % (prefix, actionIndex)
						screen.newActionButton("ActionPanel", name, WidgetTypes.WIDGET_ACTION, actionIndex, -1)
						if not interfaceController.canHandleAction(actionIndex, False):
							screen.disable(name)
						#if ( pHeadSelectedUnit.isActionRecommended(i) ):#or gc.getActionInfo(i).getCommandType() == CommandTypes.COMMAND_PROMOTION ):

					if interfaceController.canCreateGroup():
						screen.newActionButton("ActionPanel", prefix + "CreateGroup", WidgetTypes.WIDGET_CREATE_GROUP, -1, -1)

					if interfaceController.canDeleteGroup():
						screen.newActionButton("ActionPanel", prefix + "DeleteGroup", WidgetTypes.WIDGET_DELETE_GROUP, -1, -1)

	def updateEndTurnButton(self, screen):

		global g_eEndTurnButtonState

		if CyInterface().shouldDisplayEndTurnButton() and CyInterface().getShowInterface() == InterfaceVisibility.INTERFACE_SHOW:
			eState = CyInterface().getEndTurnState()
			
			bShow = False
			
			if eState == EndTurnButtonStates.END_TURN_OVER_HIGHLIGHT:
				screen.setActionButtonBackground("EndTurnButton", HeckTuiColour.Red)
				bShow = True
			elif eState == EndTurnButtonStates.END_TURN_OVER_DARK:
				screen.setActionButtonBackground("EndTurnButton", HeckTuiColour.Red)
				bShow = True
			elif eState == EndTurnButtonStates.END_TURN_GO:
				screen.setActionButtonBackground("EndTurnButton", HeckTuiColour.Green)
				bShow = True
			
			screen.setVisible("EndTurnButton", bShow)

			if g_eEndTurnButtonState == eState:
				return
				
			g_eEndTurnButtonState = eState
		else:
			screen.setVisible("EndTurnButton", False)

	def updateScoreboard(self, screen):
		prefix = "ScoreboardRow[]"
		screen.delByPrefix(prefix)
		
		# Check visible
		if not (CyInterface().getShowInterface() != InterfaceVisibility.INTERFACE_HIDE_ALL and CyInterface().getShowInterface() != InterfaceVisibility.INTERFACE_MINIMAP_ONLY):
			return
		if not (CyInterface().isScoresVisible() and not CyInterface().isCityScreenUp() and not CyEngine().isGlobeviewUp()):
			return
			
		# Check settings, adapted from BUG.
		
		bEspionageAllowed = not gc.getGame().isOption(GameOptionTypes.GAMEOPTION_NO_ESPIONAGE) # GameUtil.isEspionage()

		bShowPower = True
		if bEspionageAllowed:
			iDemographicsMission = -1
			for iMissionLoop in range(gc.getNumEspionageMissionInfos()):
				if (gc.getEspionageMissionInfo(iMissionLoop).isSeeDemographics()):
					iDemographicsMission = iMissionLoop
					break
			if (iDemographicsMission == -1):
				bShowPower = False
		
		# Determine player order, my own way.
		# Sort according to: (master team rank, is vassal, team rank, player rank, player index)
		kMaxTeams = gc.getMAX_TEAMS()
		kMaxPlayers = gc.getMAX_PLAYERS()
		kMaxCivTeams = gc.getMAX_CIV_TEAMS()
		kMaxCivPlayers = gc.getMAX_CIV_PLAYERS()
		game = gc.getGame()
		teamRanks = [kMaxCivTeams] * kMaxCivTeams
		for i in range(kMaxCivTeams):
			teamRanks[game.getRankTeam(i)] = i
		playerRanks = [kMaxCivPlayers] * kMaxCivPlayers
		for i in range(kMaxCivPlayers):
			playerRanks[game.getRankPlayer(i)] = i
		#print([game.getRankPlayer(i) for i in range(kMaxPlayers)])
		masters = [None] * kMaxTeams
		for i in range(kMaxPlayers):
			masters[i] = i # Default to master of self
		
		playerInfoList = list()
		for ePlayer in range(gc.getMAX_CIV_PLAYERS()):
			player = gc.getPlayer(ePlayer)
			if not player.isAlive():
				continue
			teamI = player.getTeam()
			team = gc.getTeam(teamI)
			if not (gc.getTeam(gc.getGame().getActiveTeam()).isHasMet(teamI) or team.isHuman()):
				continue
			playerRank = playerRanks[ePlayer]
			if team.isAVassal():
				for iOwnerTeam in range(kMaxTeams):
					if team.isVassal(iOwnerTeam):
						masters[teamI] = iOwnerTeam
						break
			playerInfoList.append((teamRanks[masters[teamI]], 1 if team.isAVassal() else 0, teamRanks[ePlayer], playerRanks[ePlayer], ePlayer))
		
		playerInfoList.sort()
		
		# Determine player order
		#playerOrder = []
		#i = gc.getMAX_CIV_TEAMS() - 1
		#while i > -1:
		#	eTeam = gc.getGame().getRankTeam(i)
		#	if gc.getTeam(gc.getGame().getActiveTeam()).isHasMet(eTeam) or gc.getTeam(eTeam).isHuman() or gc.getGame().isDebugMode():
		#		j = gc.getMAX_CIV_PLAYERS() - 1
		#		while j > -1:
		#			ePlayer = gc.getGame().getRankPlayer(j)
		#			if not CyInterface().isScoresMinimized() or gc.getGame().getActivePlayer() == ePlayer:
		#				if gc.getPlayer(ePlayer).isEverAlive() and not gc.getPlayer(ePlayer).isBarbarian() and gc.getPlayer(ePlayer).isAlive():
		#						if gc.getPlayer(ePlayer).getTeam() == eTeam:
		#							playerOrder.append(ePlayer)
		#			j -= 1
		#	i -= 1
		#	
		#playerOrder.reverse()
		
		playerOrder = [stuff[-1] for stuff in playerInfoList]

		# Now, build the rows.
		game = gc.getGame()
		iGameTurn = game.getGameTurn()
		activePlayerI = game.getActivePlayer()
		activeTeamI = game.getActiveTeam()
		iPlayerPower = gc.getPlayer(activePlayerI).getPower()
		for ePlayer in playerOrder:
			pieces = dict()
			
			eTeam = gc.getPlayer(ePlayer).getTeam()
			isAtWar = gc.getTeam(eTeam).isAtWar(gc.getGame().getActiveTeam())
			isPeaceTreaty = gc.getTeam(activeTeamI).isForcePeace(eTeam)
			player = gc.getPlayer(ePlayer)

			if kHasBUG:
				pieces[ScoreboardColumnId.WAR_PEACE] = FontUtil.getChar(FontSymbols.WAR_CHAR) if isAtWar else FontUtil.getChar(FontSymbols.PEACE_CHAR) if isPeaceTreaty else u''
			else:
				pieces[ScoreboardColumnId.WAR_PEACE] = "("  + localText.getColorText("TXT_KEY_CONCEPT_WAR", (), gc.getInfoTypeForString("COLOR_RED")).upper() + ")" if isAtWar else u''
			
			# Score adapted from BUG.
			iScore = game.getPlayerScore(ePlayer)
			if ePlayer >= activePlayerI:
				iGameTurn -= 1
				
			if not kHasBUG or ScoreOpt.isScoreDeltaIncludeCurrentTurn():
				iScoreDelta = iScore
			elif iGameTurn >= 0:
				iScoreDelta = player.getScoreHistory(iGameTurn)
			else:
				iScoreDelta = 0
				
			iPrevGameTurn = iGameTurn - 1
			if iPrevGameTurn >= 0:
				iScoreDelta -= player.getScoreHistory(iPrevGameTurn)
				
			if iScoreDelta != 0:
				szScoreDelta = localText.changeTextColor(u"%+d" % iScoreDelta, gc.getInfoTypeForString("COLOR_RED" if iScoreDelta < 0 else "COLOR_GREEN"))
			else:
				szScoreDelta = ''
				
			pieces[ScoreboardColumnId.SCORE] = str(iScore)
			pieces[ScoreboardColumnId.SCORE_DELTA] = szScoreDelta
			
			# Name
			playerColourI = gc.getPlayerColorInfo(player.getPlayerColor()).getTextColorType()
			szBuffer = '    ' if masters[eTeam] != eTeam else ''
			szBuffer += gc.getPlayer(ePlayer).getName() + u'/' + gc.getPlayer(ePlayer).getCivilizationShortDescription(0)
			if ePlayer == activePlayerI:
				szBuffer = u'[' + szBuffer + u']' # maybe there should be a way to escape brackets...
			szBuffer = localText.changeTextColor(szBuffer, playerColourI)
			pieces[ScoreboardColumnId.NAME] = szBuffer
			#pieces[ScoreboardColumnId.NAME] = u' ' + gc.getPlayer(ePlayer).getName() + u'/' + gc.getPlayer(ePlayer).getCivilizationDescription(0)
			
			# Espionage
			hasEspionage = bEspionageAllowed and gc.getTeam(eTeam).getEspionagePointsAgainstTeam(activeTeamI) < gc.getTeam(activeTeamI).getEspionagePointsAgainstTeam(eTeam)
			pieces[ScoreboardColumnId.SPY] = u" %c" % (gc.getCommerceInfo(CommerceTypes.COMMERCE_ESPIONAGE).getChar()) if hasEspionage else u''
			
			# Power. Adapted from BUG.
			szBuffer = ""
			if (activePlayerI != ePlayer and (not bEspionageAllowed or gc.getActivePlayer().canDoEspionageMission(iDemographicsMission, ePlayer, None, -1))):
				iPower = gc.getPlayer(ePlayer).getPower()
				if iPower > 0: # avoid divide by zero
					fPowerRatio = float(iPlayerPower) / float(iPower)
					#cPower = game.getSymbolID(FontSymbols.STRENGTH_CHAR)
					
					szBuffer = localText.changeTextColor(u" %.2f" % (fPowerRatio,), gc.getInfoTypeForString("COLOR_GREEN" if fPowerRatio >= 1.2 else "COLOR_RED" if fPowerRatio <= 0.8 else "COLOR_WHITE"))
			pieces[ScoreboardColumnId.POWER] = szBuffer
	
			# Tech
			bEspionageCanSeeResearch = False
			if bEspionageAllowed:
				for iMissionLoop in range(gc.getNumEspionageMissionInfos()):
					if (gc.getEspionageMissionInfo(iMissionLoop).isSeeResearch()):
						bEspionageCanSeeResearch = gc.getActivePlayer().canDoEspionageMission(iMissionLoop, ePlayer, None, -1)
						break
			
			szBuffer = ""
			if (eTeam == activeTeamI and (gc.getTeam(activeTeamI).getNumMembers() > 1)) or gc.getTeam(eTeam).isVassal(activeTeamI) or game.isDebugMode() or bEspionageCanSeeResearch:
				if player.getCurrentResearch() != -1:
					szBuffer = u" %s (%d)" % (gc.getTechInfo(player.getCurrentResearch()).getDescription(), player.getResearchTurnsLeft(gc.getPlayer(ePlayer).getCurrentResearch(), True))
			pieces[ScoreboardColumnId.TECH] = szBuffer
			
			# Trade
			szBuffer = ""
			if player.canTradeNetworkWith(activePlayerI) and ePlayer != activePlayerI:
				szBuffer = u" %c" % (CyGame().getSymbolID(FontSymbols.TRADE_CHAR))
			pieces[ScoreboardColumnId.TRADE] = szBuffer
	
			# Agreements
			szBuffer = ""
			if gc.getTeam(eTeam).isOpenBorders(activeTeamI):
				szBuffer += u" %c" % (CyGame().getSymbolID(FontSymbols.OPEN_BORDERS_CHAR))
			if gc.getTeam(eTeam).isDefensivePact(activeTeamI):
				szBuffer += u" %c" % (CyGame().getSymbolID(FontSymbols.DEFENSIVE_PACT_CHAR))
			pieces[ScoreboardColumnId.AGREEMENTS] = szBuffer
			
			# Religion
			szBuffer = ""
			religion = player.getStateReligion()
			if religion != -1:
				szBuffer = u" %c" % (gc.getReligionInfo(religion).getHolyCityChar() if player.hasHolyCity(player.getStateReligion()) else gc.getReligionInfo(religion).getChar())
			pieces[ScoreboardColumnId.RELIGION] = szBuffer
				
			# Attitude (adapted from BUG)
			szBuffer = ""
			if not player.isHuman() and activePlayerI != ePlayer:
				szBuffer =  unichr(ord(unichr(CyGame().getSymbolID(FontSymbols.POWER_CHAR) + 4)) + player.AI_getAttitude(activePlayerI))
			pieces[ScoreboardColumnId.ATTITUDE] = szBuffer
			
			# Worst Enemy (adapted from BUG)
			# BUG - Worst Enemy - start
			szBuffer = ""
			if kHasBUG and AttitudeUtil.isWorstEnemy(ePlayer, activePlayerI):
				szBuffer = u"%c" % (CyGame().getSymbolID(FontSymbols.ANGRY_POP_CHAR))
			pieces[ScoreboardColumnId.WORST_ENEMY] = szBuffer
			# BUG - Worst Enemy - end
			# BUG - WHEOOH - start
			szBuffer = ""
			if kHasBUG and PlayerUtil.isWHEOOH(ePlayer, activePlayerI):
				szBuffer = u"%c" % (CyGame().getSymbolID(FontSymbols.OCCUPATION_CHAR))
			pieces[ScoreboardColumnId.MOBILISING] = szBuffer
			# BUG - WHEOOH - end
			# BUG - Num Cities - start
			szBuffer = ""
			if kHasBUG:
				if PlayerUtil.canSeeCityList(ePlayer):
					szBuffer = u" %d" % PlayerUtil.getNumCities(ePlayer)
				else:
					szBuffer = localText.changeTextColor(u" %d" % PlayerUtil.getNumRevealedCities(ePlayer), gc.getInfoTypeForString("COLOR_CYAN"))
			pieces[ScoreboardColumnId.CITIES] = szBuffer
			# BUG - Num Cities - end
		
			# Now actually create the controls.
			rowPrefix = prefix + str(ePlayer)
			for colId, _ in self.kScoreboardDesc:
				name = rowPrefix + '_' + str(colId)
				text = pieces[colId]
				screen.newActionButton("Scoreboard", name, WidgetTypes.WIDGET_CONTACT_CIV, ePlayer, -1, HeckTuiBorderStyle.NONE)
				screen.setActionButtonLabel(name, text)
		
	def updateTurnTimerText(self, screen):
		text = CyGameTextMgr().getTurnTimerText()
		screen.setLabelText("WorldViewTurnTimerLabel", text)
		if len(text) > 0:
			screen.show("WorldViewTurnTimerLabel")
		else:
			screen.hide("WorldViewTurnTimerLabel")
		
		
	# Will redraw the interface
	def redraw(self):
		screen = CyGInterfaceScreen("MainInterface", CvScreenEnums.MAIN_INTERFACE)

		# Check Dirty Bits, see what we need to redraw...
		#if (CyInterface().isDirty(InterfaceDirtyBits.PercentButtons_DIRTY_BIT) == True):
		#	# Percent Buttons
		#	self.updatePercentButtons()
		#	CyInterface().setDirty(InterfaceDirtyBits.PercentButtons_DIRTY_BIT, False)
		#if (CyInterface().isDirty(InterfaceDirtyBits.Flag_DIRTY_BIT) == True):
		#	# Percent Buttons
		#	self.updateFlag()
		#	CyInterface().setDirty(InterfaceDirtyBits.Flag_DIRTY_BIT, False)
		#if ( CyInterface().isDirty(InterfaceDirtyBits.MiscButtons_DIRTY_BIT) == True ):
		#	# Miscellaneous buttons (civics screen, etc)
		#	self.updateMiscButtons()
		#	CyInterface().setDirty(InterfaceDirtyBits.MiscButtons_DIRTY_BIT, False)
		#if ( CyInterface().isDirty(InterfaceDirtyBits.InfoPane_DIRTY_BIT) == True ):
		#	# Info Pane Dirty Bit
		#	# This must come before updatePlotListButtons so that the entity widget appears in front of the stats
		#	self.updateInfoPaneStrings()
		#	CyInterface().setDirty(InterfaceDirtyBits.InfoPane_DIRTY_BIT, False)
		#if ( CyInterface().isDirty(InterfaceDirtyBits.PlotListButtons_DIRTY_BIT) == True ):
#		#	BugUtil.debug("dirty PlotListButtons end - %s %s %s", self.bVanCurrentlyShowing, self.bPLECurrentlyShowing, self.bBUGCurrentlyShowing)
		#	# Plot List Buttons Dirty
		#	self.updatePlotListButtons()
		#	CyInterface().setDirty(InterfaceDirtyBits.PlotListButtons_DIRTY_BIT, False)
#		#	BugUtil.debug("dirty PlotListButtons start - %s %s %s", self.bVanCurrentlyShowing, self.bPLECurrentlyShowing, self.bBUGCurrentlyShowing)
		#if ( CyInterface().isDirty(InterfaceDirtyBits.SelectionButtons_DIRTY_BIT) == True ):
		#	# Selection Buttons Dirty
		#	self.updateSelectionButtons()
		#	CyInterface().setDirty(InterfaceDirtyBits.SelectionButtons_DIRTY_BIT, False)
		#if ( CyInterface().isDirty(InterfaceDirtyBits.ResearchButtons_DIRTY_BIT) == True ):
		#	# Research Buttons Dirty
		#	self.updateResearchButtons()
		#	CyInterface().setDirty(InterfaceDirtyBits.ResearchButtons_DIRTY_BIT, False)
		#if ( CyInterface().isDirty(InterfaceDirtyBits.CitizenButtons_DIRTY_BIT) == True ):
		#	# Citizen Buttons Dirty

# BUG - city specialist - start
		#	self.updateCitizenButtons_hide()
		#	if (CityScreenOpt.isCitySpecialist_Stacker()):
		#		self.updateCitizenButtons_Stacker()
		#	elif (CityScreenOpt.isCitySpecialist_Chevron()):
		#		self.updateCitizenButtons_Chevron()
		#	else:
		#		self.updateCitizenButtons()
# BUG - city specialist - end
			
		#	CyInterface().setDirty(InterfaceDirtyBits.CitizenButtons_DIRTY_BIT, False)
		#if ( CyInterface().isDirty(InterfaceDirtyBits.GameData_DIRTY_BIT) == True ):
		#	# Game Data Strings Dirty
		#	self.updateGameDataStrings()
		#	CyInterface().setDirty(InterfaceDirtyBits.GameData_DIRTY_BIT, False)
		#if ( CyInterface().isDirty(InterfaceDirtyBits.Help_DIRTY_BIT) == True ):
		#	# Help Dirty bit
		#	self.updateHelpStrings()
		#	CyInterface().setDirty(InterfaceDirtyBits.Help_DIRTY_BIT, False)
		if CyInterface().isDirty(InterfaceDirtyBits.CityScreen_DIRTY_BIT):
		#	# Selection Data Dirty Bit
		#	self.updateCityScreen()
			CyInterface().setDirty(InterfaceDirtyBits.Domestic_Advisor_DIRTY_BIT, True)
			CyInterface().setDirty(InterfaceDirtyBits.CityScreen_DIRTY_BIT, False)
		#if ( CyInterface().isDirty(InterfaceDirtyBits.Score_DIRTY_BIT) == True or CyInterface().checkFlashUpdate() ):
		#	# Scores!
		#	self.updateScoreStrings()
		#	CyInterface().setDirty(InterfaceDirtyBits.Score_DIRTY_BIT, False)
		#if ( CyInterface().isDirty(InterfaceDirtyBits.GlobeInfo_DIRTY_BIT) == True ):
		#	# Globeview and Globelayer buttons
		#	CyInterface().setDirty(InterfaceDirtyBits.GlobeInfo_DIRTY_BIT, False)
		#	self.updateGlobeviewButtons()

		return 0


	def updateTimeText( self ):
		
		global g_szTimeText
		
		ePlayer = gc.getGame().getActivePlayer()
		
# BUG - NJAGC - start
		if kHasBUG and ClockOpt.isEnabled():
			"""
			Format: Time - GameTurn/Total Percent - GA (TurnsLeft) Date
			
			Ex: 10:37 - 220/660 33% - GA (3) 1925
			"""
			if (g_bShowTimeTextAlt):
				bShowTime = ClockOpt.isShowAltTime()
				bShowGameTurn = ClockOpt.isShowAltGameTurn()
				bShowTotalTurns = ClockOpt.isShowAltTotalTurns()
				bShowPercentComplete = ClockOpt.isShowAltPercentComplete()
				bShowDateGA = ClockOpt.isShowAltDateGA()
			else:
				bShowTime = ClockOpt.isShowTime()
				bShowGameTurn = ClockOpt.isShowGameTurn()
				bShowTotalTurns = ClockOpt.isShowTotalTurns()
				bShowPercentComplete = ClockOpt.isShowPercentComplete()
				bShowDateGA = ClockOpt.isShowDateGA()
			
			if (not gc.getGame().getMaxTurns() > 0):
				bShowTotalTurns = False
				bShowPercentComplete = False
			
			bFirst = True
			g_szTimeText = ""
			
			# fortsnek: No clock yet.
			bShowTime = False
			if (bShowTime):
				bFirst = False
				g_szTimeText += getClockText()
			
			if (bShowGameTurn):
				if (bFirst):
					bFirst = False
				else:
					g_szTimeText += u" - "
				g_szTimeText += u"%d" %( gc.getGame().getElapsedGameTurns() )
				if (bShowTotalTurns):
					g_szTimeText += u"/%d" %( gc.getGame().getMaxTurns() )
			
			if (bShowPercentComplete):
				if (bFirst):
					bFirst = False
				else:
					if (not bShowGameTurn):
						g_szTimeText += u" - "
					else:
						g_szTimeText += u" "
				g_szTimeText += u"%2.2f%%" %( 100 *(float(gc.getGame().getElapsedGameTurns()) / float(gc.getGame().getMaxTurns())) )
			
			if (bShowDateGA):
				if (bFirst):
					bFirst = False
				else:
					g_szTimeText += u" - "
				szDateGA = unicode(CyGameTextMgr().getInterfaceTimeStr(ePlayer))
				if(ClockOpt.isUseEraColor()):
					iEraColor = ClockOpt.getEraColor(gc.getEraInfo(gc.getPlayer(ePlayer).getCurrentEra()).getType())
					if (iEraColor >= 0):
						szDateGA = localText.changeTextColor(szDateGA, iEraColor)
				g_szTimeText += szDateGA
		else:
			"""
			Original Clock
			Format: Time - 'Turn' GameTurn - GA (TurnsLeft) Date
			
			Ex: 10:37 - Turn 220 - GA (3) 1925
			"""
			g_szTimeText = localText.getText("TXT_KEY_TIME_TURN", (CyGame().getGameTurn(), )) + u" - " + unicode(CyGameTextMgr().getInterfaceTimeStr(ePlayer))
			if (CyUserProfile().isClockOn()):
				g_szTimeText = getClockText() + u" - " + g_szTimeText
# BUG - NJAGC - end

	def handleInput(self, inputClass):
		return 0

	def update(self, fDelta):
		return