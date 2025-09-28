## Sid Meier's Civilization 4
## Copyright Firaxis Games 2005

## fortsnek: Adapted for Cv4MiniEngine.

from CvPythonExtensions import *
import PyHelpers
import CvUtil
import ScreenInput
import CvScreenEnums

kHasBUG = False
try:
	import BugUtil
	kHasBUG = True
except ImportError:
	pass

PyPlayer = PyHelpers.PyPlayer

# globals
gc = CyGlobalContext()
localText = CyTranslator()

kPythonInfoButton = 0

class CvCorporationScreen:
	"Corporation Advisor Screen"

	def __init__(self):
		
		self.SCREEN_NAME = "CorporationScreen"
		
		self.iCorporationExamined = -1
		self.iCorporationSelected = -1
		self.iActivePlayer = -1
		
		self.bScreenUp = False
		
		self.nextAnonCtrlId = 1

	def getScreen(self):
		return CyGInterfaceScreen(self.SCREEN_NAME, CvScreenEnums.CORPORATION_SCREEN)

	def interfaceScreen(self):

		self.iActivePlayer = gc.getGame().getActivePlayer()

		self.bScreenUp = True

		screen = self.getScreen()
		if screen.isActive():
			return
		#screen.setRenderInterfaceOnly(True);
		screen.setInitialTitle(localText.getText("TXT_KEY_CORPORATION_SCREEN_TITLE", ()).upper())
	
		root = ""
	
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # TopTable
			TableLayoutConfigCell(ivec2(0, 1)), # CityTable
			TableLayoutConfigCell(ivec2(0, 2), ivec2(1, 1), RectJustilign(EJustilign.Stretch, EJustilign.Stretch)) # Footer
		]
		table.gap = ivec2(0, 0)
		screen.setTableLayout(root, table)
		
		screen.newPanel(root, "TopTable")
		
		
		cols = [
			localText.getText("TXT_KEY_WONDER_CITY", ()),
		] + [u"%c" % gc.getCorporationInfo(iRel).getChar() for iRel in range(gc.getNumCorporationInfos())
		] + [u""]
		
		screen.newRichTextTable(root, "CityTable", cols, WidgetTypes.WIDGET_GENERAL, -1, -1)
		screen.setRichTextTableExpandColumn("CityTable", len(cols) - 1)
		screen.newPanel(root, "Footer")

		screen.newPanel(root, "Footer")
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), RectJustilign(EJustilign.End, EJustilign.Center)), # Exit
		]
		table.gap = ivec2(1, 0)
		screen.setTableLayout("Footer", table)

		screen.newActionButton("Footer", "ExitButton", WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)
		screen.setActionButtonLabel("ExitButton", localText.getText("TXT_KEY_PEDIA_SCREEN_EXIT", ()).upper())

		self.iCorporationExamined = -1
		self.iCorporationSelected = -1

		self.drawInfo(screen, "TopTable")
		self.updateCityInfo(screen, "CityTable", self.iCorporationExamined)
		self.updateInfoButtons(screen)
		
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)
		
	def genAnonCtrlName(self):
		name = "Anon" + str(self.nextAnonCtrlId)
		self.nextAnonCtrlId += 1
		return name

	# Draws the Corporation buttons and information		
	def drawInfo(self, screen, root):
		screen = self.getScreen()
		
		numInfos = gc.getNumCorporationInfos()
		numCols = 1 + numInfos
	
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 0)] + [TableLayoutConfigRowColDesc(0, 1)] * (numCols - 1)
		table.rows = [TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), RectJustilign(EJustilign.Start, EJustilign.Stretch))] + \
			[TableLayoutConfigCell(ivec2(i, 0), ivec2(1, 1), RectJustilign(EJustilign.Center, EJustilign.Stretch)) for i in range(1, numCols)]
		
		table.gap = ivec2(1, 0)
		screen.setTableLayout(root, table)
		
		# buttons at the top
		screen.newPanel(root, self.genAnonCtrlName())
		for i in range(numInfos):
			info = gc.getCorporationInfo(i)
			name = "InfoButton" + str(i)
			screen.newActionButton(root, name, WidgetTypes.WIDGET_PYTHON, kPythonInfoButton, i)

		# Great Person	
		screen.newPanel(root, self.genAnonCtrlName())	
		for i in range(numInfos):
			szGreatPerson = ""
			for iBuilding in range(gc.getNumBuildingInfos()):
				if (gc.getBuildingInfo(iBuilding).getFoundsCorporation() == i):
					break
			for iUnit in range(gc.getNumUnitInfos()):
				if gc.getUnitInfo(iUnit).getBuildings(iBuilding) or gc.getUnitInfo(iUnit).getForceBuildings(iBuilding):
					szGreatPerson = gc.getUnitInfo(iUnit).getDescription()
					break
			screen.newLabel(root, self.genAnonCtrlName(), szGreatPerson)

		# Bonuses
		screen.newPanel(root, self.genAnonCtrlName())
		for i in range(numInfos):
			bonusList = []
			for iRequired in range(gc.getDefineINT("NUM_CORPORATION_PREREQ_BONUSES")):
				eBonus = gc.getCorporationInfo(i).getPrereqBonus(iRequired)
				if -1 != eBonus:
					bonusList.append(u"%c" % (gc.getBonusInfo(eBonus).getChar(),))
			name = self.genAnonCtrlName()
			screen.newLabel(root, name, u" ".join(bonusList))
			screen.setLabelHAlign(name, EAlign.Center)
			screen.setLabelEnableWrapping(name)

		# Date Founded:
		screen.newLabel(root, self.genAnonCtrlName(), localText.getText("TXT_KEY_RELIGION_SCREEN_DATE_FOUNDED", ()))
		for i in range(numInfos):
			if (gc.getGame().getCorporationGameTurnFounded(i) < 0):
				szFounded = localText.getText("TXT_KEY_RELIGION_SCREEN_NOT_FOUNDED", ())
			else:
				szFounded = CyGameTextMgr().getTimeStr(gc.getGame().getCorporationGameTurnFounded(i), false)

			screen.newLabel(root, self.genAnonCtrlName(), szFounded)
			
		player = gc.getPlayer(self.iActivePlayer)
					
		# Headquarters
		screen.newLabel(root, self.genAnonCtrlName(), localText.getText("TXT_KEY_CORPORATION_SCREEN_HEADQUARTERS", ()))
		for i in range(numInfos):
			pHeadquarters = gc.getGame().getHeadquarters(i)
			if pHeadquarters.isNone():
				szFounded = u"-"
			elif not pHeadquarters.isRevealed(player.getTeam(), False):
				szFounded = localText.getText("TXT_KEY_UNKNOWN", ())
			else:
				szFounded = pHeadquarters.getName() + u" (%s)" % gc.getPlayer(pHeadquarters.getOwner()).getCivilizationAdjective(0)
			screen.newLabel(root, self.genAnonCtrlName(), szFounded)
			
		# City counts, like in the religion screen.
		if kHasBUG:
			cityList = PyPlayer(self.iActivePlayer).getCityList()
			screen.newLabel(root, self.genAnonCtrlName(), "%s [%i]:" % (localText.getText("TXT_KEY_BUG_RELIGIOUS_CITY", ()), len(cityList)))
			
			counts = [0] * numInfos
			for pLoopCity in cityList:
				for corpI in pLoopCity.getCorporations():
					counts[corpI] += 1
					
			# Get exec unit classes.
			unitClasses = [None] * numInfos
			for unitClassI in range(gc.getNumUnitClassInfos()):
				unitClass = gc.getUnitClassInfo(unitClassI)
				defUnitI = unitClass.getDefaultUnitIndex()
				defUnit = gc.getUnitInfo(defUnitI)
				for corpI in range(numInfos):
					if defUnit.getCorporationSpreads(corpI):
						assert unitClasses[corpI] is None
						unitClasses[corpI] = unitClassI

			for corpI in range(numInfos):
				pHeadquarters = gc.getGame().getHeadquarters(corpI)
				if pHeadquarters.isNone():
					szFounded = u"-"
				else:
					if unitClasses[corpI] is not None:
						execCount = player.getUnitClassCount(unitClasses[corpI])
						execPlusProdCount = player.getUnitClassCountPlusMaking(unitClasses[corpI])
					else:
						execCount = 0
						execPlusProdCount = 0
					szFounded = u"%s (%d+%d%c)" % (counts[corpI], execCount, execPlusProdCount - execCount, gc.getYieldInfo(YieldTypes.YIELD_PRODUCTION).getChar())
					
				screen.newLabel(root, self.genAnonCtrlName(), szFounded)
			
			
	def updateInfoButtons(self, screen):
		for i in range(gc.getNumCorporationInfos()):
			info = gc.getCorporationInfo(i)
			name = "InfoButton" + str(i)
			screen.setActionButtonLabel(name, u"[%c] %c\n%s" % (u"\u25cf" if i == self.iCorporationSelected else u" ", info.getChar(), info.getDescription()))

	# Draws the city list
	def updateCityInfo(self, screen, root, iCorporation):
	
		if (not self.bScreenUp):
			return
			
		screen.clearRichTextTableRows(root)
		
		numInfos = gc.getNumCorporationInfos()

		# Loop through the cities
		for pLoopCity in PyPlayer(self.iActivePlayer).getCityList():
		
			row = []
			intKeys = []

			# Constructing the City name...
			szCityName = u""
			if pLoopCity.isCapital():
				szCityName += u"%c" % CyGame().getSymbolID(FontSymbols.STAR_CHAR)
			szCityName += pLoopCity.getName()
				
			row.append(szCityName)
			intKeys.append(0)
			
			lCorporations = pLoopCity.getCorporations()
			lHeadquarters = pLoopCity.getHeadquarters()
			
			for i in range(numInfos):
				text = ""
				key = 0
				if i in lCorporations:
					info = gc.getCorporationInfo(i)
					key = 2 if i in lHeadquarters else 1
					text = u"%c" % (info.getHeadquarterChar() if key >= 2 else info.getChar())
				row.append(text)
				intKeys.append(key)
			
			
			text = ""
			if (iCorporation == -1):
				bFirst = True
				for iI in range(len(lCorporations)):
					szTempBuffer = CyGameTextMgr().getCorporationHelpCity(lCorporations[iI], pLoopCity.GetCy(), False, False)
					if (szTempBuffer):
						if (not bFirst):
							text += u", "
						text += szTempBuffer
						bFirst = False
			else:
				text += CyGameTextMgr().getCorporationHelpCity(iCorporation, pLoopCity.GetCy(), False, True)
				
			row.append(text)
			intKeys.append(0)
			
			screen.addRichTextTableRow(root, row, intKeys)

	def getCorporationButtonName(self, iCorporation):
		szName = self.BUTTON_NAME + str(iCorporation)
		return szName
				
	def getCorporationTextName(self, iCorporation):
		szName = self.CORPORATION_NAME + str(iCorporation)
		return szName
								
	# Will handle the input for this screen...
	def handleInput (self, inputClass):
		if inputClass.iData1 == kPythonInfoButton:
			return self.CorporationScreenButton(inputClass)
		
		return 0
		
	def update(self, fDelta):
		return

	# Corporation Button
	def CorporationScreenButton( self, inputClass ):	
		if ( inputClass.getNotifyCode() == NotifyCode.NOTIFY_CLICKED ) :
			if self.iCorporationSelected == inputClass.iData2:
				self.iCorporationSelected = -1
			else:					
				self.iCorporationSelected = inputClass.iData2
			self.iCorporationExamined = self.iCorporationSelected
		elif ( inputClass.getNotifyCode() == NotifyCode.NOTIFY_CURSOR_MOVE_ON ) :
			self.iCorporationExamined = inputClass.iData2
		elif ( inputClass.getNotifyCode() == NotifyCode.NOTIFY_CURSOR_MOVE_OFF ) :
			self.iCorporationExamined = self.iCorporationSelected
		screen = self.getScreen()
		self.updateCityInfo(screen, "CityTable", self.iCorporationExamined)
		self.updateInfoButtons(screen)
		return 0
		
				