## Sid Meier's Civilization 4
## Copyright Firaxis Games 2005

## fortsnek: Adapted for Cv4MiniEngine.

from CvPythonExtensions import *
import PyHelpers
import CvUtil
import ScreenInput
import CvScreenEnums

# BUG
# BUG - start
#import BugUtil
kHasBUG = False
try:
	import BugCore
	kHasBUG = True
except ImportError:
	pass
	
if kHasBUG:
	import PlayerUtil
	import ReligionUtil
	AdvisorOpt = BugCore.game.Advisors
# BUG - end

PyPlayer = PyHelpers.PyPlayer

# globals
gc = CyGlobalContext()
localText = CyTranslator()

class CvReligionScreen:
	"Religion Advisor Screen"
	
	kPythonReligionButton = 0
	kPythonCancelButton = 1

	def __init__(self):
		
		self.SCREEN_NAME = "ReligionScreen"
		
		self.nextAnonCtrlId = 1
		
		self.iReligionExamined = -1
		self.iReligionSelected = -1
		self.iReligionOriginal = -1
		self.iActivePlayer = -1
		
		self.bScreenUp = False
		self.bBUGConstants = False

			
	def getScreen(self):
		return CyGInterfaceScreen(self.SCREEN_NAME, CvScreenEnums.RELIGION_SCREEN)

	def interfaceScreen(self):
		self.iActivePlayer = gc.getGame().getActivePlayer()

		self.bScreenUp = True

		screen = self.getScreen()
		if screen.isActive():
			return
		#screen.setRenderInterfaceOnly(True);
		screen.setInitialTitle(localText.getText("TXT_KEY_RELIGION_SCREEN_TITLE", ()).upper())
		
		root = ""
	
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # ReligionTable
			TableLayoutConfigCell(ivec2(0, 1)), # CityTable
			TableLayoutConfigCell(ivec2(0, 2), ivec2(1, 1), RectJustilign(EJustilign.Stretch, EJustilign.Stretch)) # Footer
		]
		table.gap = ivec2(0, 1)
		screen.setTableLayout(root, table)
		
		screen.newPanel(root, "ReligionTable")
		
		cols = [
			localText.getText("TXT_KEY_WONDER_CITY", ()),
		] + [u"%c" % gc.getReligionInfo(iRel).getChar() for iRel in range(gc.getNumReligionInfos()) if gc.getGame().getReligionGameTurnFounded(iRel) >= 0
		]
		
		if kHasBUG:
			cols += [type.icon for type in ReligionUtil.getUnitTypes()] + [type.icon for type in ReligionUtil.getBuildingTypes()]
		cols += [u""]
		
		screen.newRichTextTable(root, "CityTable", cols, WidgetTypes.WIDGET_GENERAL, -1, -1)
		screen.setRichTextTableExpandColumn("CityTable", len(cols) - 1)
		screen.newPanel(root, "Footer")

		screen.newPanel(root, "Footer")
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), RectJustilign(EJustilign.Start, EJustilign.Center)), # StatusText
			TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), RectJustilign(EJustilign.Center, EJustilign.Center)), # Cancel
			TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), RectJustilign(EJustilign.End, EJustilign.Center)), # Convert
			TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), RectJustilign(EJustilign.End, EJustilign.Center)), # Exit
		]
		table.gap = ivec2(1, 0)
		screen.setTableLayout("Footer", table)

		screen.newLabel("Footer", "StatusText", "")
		screen.newActionButton("Footer", "CancelButton", WidgetTypes.WIDGET_PYTHON, CvReligionScreen.kPythonCancelButton, 0)
		screen.setActionButtonLabel("CancelButton", localText.getText("TXT_KEY_SCREEN_CANCEL", ()).upper())
		screen.newActionButton("Footer", "ConvertButton", WidgetTypes.WIDGET_CONVERT, 1000, 1)
		screen.setActionButtonLabel("ConvertButton", localText.getText("TXT_KEY_RELIGION_CONVERT", ()).upper())
		screen.newActionButton("Footer", "ExitButton", WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)
		screen.setActionButtonLabel("ExitButton", localText.getText("TXT_KEY_PEDIA_SCREEN_EXIT", ()).upper())
		
		# Draw Religion info
		self.drawReligionInfo(screen, "ReligionTable")

		self.iReligionSelected = gc.getPlayer(self.iActivePlayer).getStateReligion()
		self.iReligionExamined = self.iReligionSelected
		self.iReligionOriginal = self.iReligionSelected
		
		self.updateReligionButtons(screen)
		self.updateFooter(screen)
		self.updateCityTable(screen, "CityTable", self.iReligionExamined)
		
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)
		
	def genAnonCtrlName(self):
		name = "Anon" + str(self.nextAnonCtrlId)
		self.nextAnonCtrlId += 1
		return name
		
	# BUG constants
	def BUGConstants(self):

		if self.bBUGConstants:
			return

		self.bBUGConstants = True

		# BUG additions
		self.hammerIcon = u"%c" %(gc.getYieldInfo(YieldTypes.YIELD_PRODUCTION).getChar())

		# Special symbols for building, wonder and project views
		self.objectIsPresent = "x"
		self.objectIsNotPresent = "-"
		self.objectCanBeBuild = "o"
		self.objectUnderConstruction = self.hammerIcon
		
		# add the colors dependant on the statuses
		self.objectHave = localText.changeTextColor (self.objectIsPresent, gc.getInfoTypeForString("COLOR_GREEN")) #"x"
		self.objectNotPossible = localText.changeTextColor (self.objectIsNotPresent, gc.getInfoTypeForString("COLOR_RED")) #"-"
		self.objectPossible = localText.changeTextColor (self.objectCanBeBuild, gc.getInfoTypeForString("COLOR_BLUE")) #"o"
		self.objectHaveObsolete = localText.changeTextColor (self.objectIsPresent, gc.getInfoTypeForString("COLOR_WHITE")) #"x"
		self.objectNotPossibleConcurrent = localText.changeTextColor (self.objectIsNotPresent, gc.getInfoTypeForString("COLOR_YELLOW")) #"-"
		self.objectPossibleConcurrent = localText.changeTextColor (self.objectCanBeBuild, gc.getInfoTypeForString("COLOR_YELLOW")) #"o"

		self.szCities = localText.getText("TXT_KEY_BUG_RELIGIOUS_CITY", ())
		self.szTemples = localText.getText("TXT_KEY_BUG_RELIGIOUS_TEMPLE", ())
		self.szMonastaries = localText.getText("TXT_KEY_BUG_RELIGIOUS_MONASTARY", ())
		self.szMissionaries = localText.getText("TXT_KEY_BUG_RELIGIOUS_MISSIONARY", ())

		self.sCity = localText.getText("TXT_KEY_WONDER_CITY", ())

	# Draws the religion buttons and information		
	def drawReligionInfo(self, screen, root):
	
		numReligions = gc.getNumReligionInfos()
		numCols = 1 + numReligions + 1
		#numRows = 1 + 7
	
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 0)] + [TableLayoutConfigRowColDesc(0, 1)] * (numCols - 1)
		table.rows = [TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), RectJustilign(EJustilign.Start, EJustilign.Stretch))] + \
			[TableLayoutConfigCell(ivec2(i, 0), ivec2(1, 1), RectJustilign(EJustilign.Center, EJustilign.Stretch)) for i in range(1, numCols)]
		
		table.gap = ivec2(1, 0)
		screen.setTableLayout(root, table)
		
		screen.newPanel(root, root + "DummyPanel")
		
		# Religion buttons at the top
		for i in range(numReligions):
			info = gc.getReligionInfo(i)
			name = "ReligionButton" + str(i)
			screen.newActionButton(root, name, WidgetTypes.WIDGET_PYTHON, CvReligionScreen.kPythonReligionButton, i)
			screen.setEnabled(name, gc.getGame().getReligionGameTurnFounded(i) >= 0)
		
		name = "ReligionButtonNone"
		screen.newActionButton(root, name, WidgetTypes.WIDGET_PYTHON, CvReligionScreen.kPythonReligionButton, -1)
		
		# Date Founded:
		screen.newLabel(root, self.genAnonCtrlName(), localText.getText("TXT_KEY_RELIGION_SCREEN_DATE_FOUNDED", ()))
		for i in range(numReligions):
			if gc.getGame().getReligionGameTurnFounded(i) < 0:
				szFounded = "" #localText.getText("TXT_KEY_RELIGION_SCREEN_NOT_FOUNDED", ())
			else:
				szFounded = CyGameTextMgr().getTimeStr(gc.getGame().getReligionGameTurnFounded(i), false)
			screen.newLabel(root, self.genAnonCtrlName(), szFounded)
			
		
		screen.newPanel(root, self.genAnonCtrlName())
		
		# Holy City...
		screen.newLabel(root, self.genAnonCtrlName(), localText.getText("TXT_KEY_RELIGION_SCREEN_HOLY_CITY", ()))

		for i in range(numReligions):
			pHolyCity = gc.getGame().getHolyCity(i)
			if gc.getGame().getReligionGameTurnFounded(i) < 0:
				szFounded = ""
			elif pHolyCity.isNone():
				szFounded = localText.getText("TXT_KEY_NONE", ())
			elif not pHolyCity.isRevealed(gc.getPlayer(self.iActivePlayer).getTeam(), False):
				szFounded = localText.getText("TXT_KEY_UNKNOWN", ())
			else:
				szFounded = "(%s)" % gc.getPlayer(pHolyCity.getOwner()).getCivilizationAdjective(0) + u"\n" + pHolyCity.getName()
			name = self.genAnonCtrlName()
			screen.newLabel(root, name, szFounded)
			screen.setLabelHAlign(name, EAlign.Center)
		
		screen.newPanel(root, self.genAnonCtrlName())
		
		# Influence...
		screen.newLabel(root, self.genAnonCtrlName(), localText.getText("TXT_KEY_RELIGION_SCREEN_INFLUENCE", ()))

		for i in range(numReligions):
			if (gc.getGame().getReligionGameTurnFounded(i) < 0):
				szFounded = ""
			else:
				szFounded = str(gc.getGame().calculateReligionPercent(i)) + "%"
			screen.newLabel(root, self.genAnonCtrlName(), szFounded)
			
		screen.newPanel(root, self.genAnonCtrlName())
	
		# Count the number of temples and monastery
		self.BUGConstants()
		iPlayer = PyPlayer(self.iActivePlayer)
		cityList = iPlayer.getCityList()
# BUG - start
		iCities = [0] * numReligions
		iTemple = [0] * numReligions
		iMonastery = [0] * numReligions
		iMissionaries_Active = [0] * numReligions
		iMissionaries_Construct = [0] * numReligions
		
		for pLoopCity in cityList:
			lHolyCity = pLoopCity.getHolyCity()
			lReligions = pLoopCity.getReligions()

			for iRel in range(numReligions):
				# count the number of cities
				if iRel in lReligions:
					iCities[iRel] += 1

				if kHasBUG:
					# count the number of temples
					iBldg = ReligionUtil.getBuilding(iRel, ReligionUtil.BUILDING_TEMPLE)
					if self.calculateBuilding(pLoopCity, iBldg) == self.objectHave:
						iTemple[iRel] += 1

					# count the number of monasteries
					iBldg = ReligionUtil.getBuilding(iRel, ReligionUtil.BUILDING_MONASTERY)
					if self.calculateBuilding(pLoopCity, iBldg) == self.objectHave:
						iMonastery[iRel] += 1

					# count the number of missionaries under construction
					iUnit = ReligionUtil.getUnit(iRel, ReligionUtil.UNIT_MISSIONARY)
					if pLoopCity.GetCy().getFirstUnitOrder(iUnit) != -1:
						iMissionaries_Construct[iRel] += 1

		if kHasBUG:# number of cities...
			screen.newLabel(root, self.genAnonCtrlName(), "%s [%i]:" % (self.szCities, len(cityList)))
			for iRel in range(numReligions):
				if (gc.getGame().getReligionGameTurnFounded(iRel) >= 0):
					szFounded = "%i" % (iCities[iRel])
				else:
					szFounded = ""
				screen.newLabel(root, self.genAnonCtrlName(), szFounded)
			screen.newPanel(root, self.genAnonCtrlName())
			
			# count the number of active missionaries
			for iUnit in PlayerUtil.playerUnits(self.iActivePlayer):  
				for iRel in range(numReligions):
					if iUnit.getUnitType() == ReligionUtil.getUnit(iRel, ReligionUtil.UNIT_MISSIONARY):
						iMissionaries_Active[iRel] += 1
					
			# number of temples...
			screen.newLabel(root, self.genAnonCtrlName(), self.szTemples)
			for iRel in range(numReligions):
				if (gc.getGame().getReligionGameTurnFounded(iRel) >= 0):
					szFounded = "%i" % (iTemple[iRel])
				else:
					szFounded = ""
				screen.newLabel(root, self.genAnonCtrlName(), szFounded)
			screen.newPanel(root, self.genAnonCtrlName())

			# number of monasteries...
			screen.newLabel(root, self.genAnonCtrlName(), self.szMonastaries)
			for iRel in range(numReligions):
				if (gc.getGame().getReligionGameTurnFounded(iRel) >= 0):
					szFounded = "%i" % (iMonastery[iRel])
				else:
					szFounded = ""
				screen.newLabel(root, self.genAnonCtrlName(), szFounded)
			screen.newPanel(root, self.genAnonCtrlName())

			# number of missionaries...
			screen.newLabel(root, self.genAnonCtrlName(), self.szMissionaries)
			for iRel in range(numReligions):
				if (gc.getGame().getReligionGameTurnFounded(iRel) >= 0):
					szFounded = "%i [%i]" % (iMissionaries_Active[iRel], iMissionaries_Construct[iRel])
				else:
					szFounded = ""
				screen.newLabel(root, self.genAnonCtrlName(), szFounded)
				
		screen.newPanel(root, self.genAnonCtrlName())

	def updateCityTable(self, screen, root, religionI):
		screen.clearRichTextTableRows(root)
		
		iPlayer = PyPlayer(self.iActivePlayer)
		cityList = iPlayer.getCityList()
		
		row = []
		intKeys = []
		
		# Loop through the cities
		for iCity in range(len(cityList)):
			pLoopCity = cityList[iCity]

			# Does this preserve list allocation?
			del row[:]
			del intKeys[:]
			row.append(pLoopCity.getName())
			intKeys.append(0)

			lHolyCity = pLoopCity.getHolyCity()
			lReligions = pLoopCity.getReligions()

			for iRel in range(gc.getNumReligionInfos()):
				szReligionIcon = ""
				if (gc.getGame().getReligionGameTurnFounded(iRel) >= 0):
					if iRel in lHolyCity:
						szReligionIcon = u"<font=2>%c</font>" %(gc.getReligionInfo(iRel).getHolyCityChar())
					elif iRel in lReligions:
						szReligionIcon = u"<font=2>%c</font>" %(gc.getReligionInfo(iRel).getChar())
					row.append(szReligionIcon)
					intKeys.append(0)

			if kHasBUG:
				if ReligionUtil.isValid(religionI):
					# check for missionaries
					for i in range(ReligionUtil.getNumUnitTypes()):
						iUnit = ReligionUtil.getUnit(religionI, i)
						if pLoopCity.GetCy().getFirstUnitOrder(iUnit) != -1:
							sUnit = self.objectUnderConstruction
						elif pLoopCity.GetCy().canTrain(iUnit, False, False):
							sUnit = self.objectPossible
						else:
							sUnit = self.objectNotPossible
						row.append(sUnit)
						intKeys.append(0)

					# check for temples, cathedral, monasteries, shrine
					for i in range(ReligionUtil.getNumBuildingTypes()):
						iBldg = ReligionUtil.getBuilding(religionI, i)
						sBldg = self.calculateBuilding(pLoopCity, iBldg)
						row.append(sBldg)
						intKeys.append(0)
						
				else:
					n = ReligionUtil.getNumUnitTypes() + ReligionUtil.getNumBuildingTypes()
					row += [""] * n
					intKeys += [0] * n

			if (religionI == -1):
				bFirst = True
				sHelp = ""
				for iI in range(len(lReligions)):
					szTempBuffer = CyGameTextMgr().getReligionHelpCity(lReligions[iI], pLoopCity.GetCy(), False, False, False, True)
					if (szTempBuffer):
						if (not bFirst):
							sHelp += u", "
						sHelp += szTempBuffer
						bFirst = False
			else:
				sHelp = CyGameTextMgr().getReligionHelpCity(religionI, pLoopCity.GetCy(), False, False, True, False)

			row.append(sHelp)
			intKeys.append(0)
			
			screen.addRichTextTableRow(root, row, intKeys)


	def updateReligionButtons(self, screen):
		for i in range(gc.getNumReligionInfos()):
			info = gc.getReligionInfo(i)
			name = "ReligionButton" + str(i)
			screen.setActionButtonLabel(name, u"[%c] %c\n%s" % (u"\u25cf" if i == self.iReligionSelected else u" ", info.getChar(), info.getDescription()))
			
		name = "ReligionButtonNone"
		screen.setActionButtonLabel(name, u"[%c]\n%s" % (u"\u25cf" if -1 == self.iReligionSelected else u" ", localText.getText("TXT_KEY_RELIGION_SCREEN_NO_STATE", ())))
		
	def updateFooter(self, screen):
		#screen.hide("StatusText")
		screen.hide("CancelButton")
		screen.hide("ConvertButton")
		screen.hide("ExitButton")
		
		religionI = self.iReligionExamined
		
		if not self.canConvert(religionI) or religionI == self.iReligionOriginal:
			screen.show("ExitButton")
			szAnarchyTime = CyGameTextMgr().setConvertHelp(self.iActivePlayer, religionI)
		else:
			screen.show("CancelButton")
			screen.show("ConvertButton")
			screen.setActionButtonWidgetData("ConvertButton", WidgetTypes.WIDGET_CONVERT, religionI, 1)
			szAnarchyTime = localText.getText("TXT_KEY_ANARCHY_TURNS", (gc.getPlayer(self.iActivePlayer).getReligionAnarchyLength(), ))

		# Turns of Anarchy Text...
		screen.setLabelText("StatusText", szAnarchyTime)


	def getReligionButtonName(self, iReligion):
		szName = self.BUTTON_NAME + str(iReligion)
		return szName
				
	def getReligionTextName(self, iReligion):
		szName = self.RELIGION_NAME + str(iReligion)
		return szName
						
	def canConvert(self, iReligion):
		iCurrentReligion = gc.getPlayer(self.iActivePlayer).getStateReligion()
		if (iReligion == gc.getNumReligionInfos()):
			iConvertReligion = -1
		else:
			iConvertReligion = iReligion
						
		return (iConvertReligion != iCurrentReligion and gc.getPlayer(self.iActivePlayer).canConvert(iConvertReligion))		
		
	# Will handle the input for this screen...
	def handleInput (self, inputClass):
		if inputClass.getButtonType() == WidgetTypes.WIDGET_CONVERT:
			self.ReligionConvert(inputClass)
		elif inputClass.iData1 == CvReligionScreen.kPythonReligionButton:
			return self.ReligionScreenButton(inputClass)
		elif inputClass.iData1 == CvReligionScreen.kPythonCancelButton:
			return self.ReligionCancel(inputClass)
		#if inputClass.getNotifyCode() == NotifyCode.NOTIFY_CLICKED):
		#	screen = self.getScreen()
		#	iIndex = screen.getSelectedPullDownID(self.DEBUG_DROPDOWN_ID)
		#	self.iActivePlayer = screen.getPullDownData(self.DEBUG_DROPDOWN_ID, iIndex)
		#	self.drawReligionInfo()			
		#	self.drawCityInfo(self.iReligionSelected)
		#	return 1
		#elif (self.ReligionScreenInputMap.has_key(inputClass.getFunctionName())):	
		#	'Calls function mapped in ReligionScreenInputMap'
		#	# only get from the map if it has the key
		#	
		#	# get bound function from map and call it
		#	self.ReligionScreenInputMap.get(inputClass.getFunctionName())(inputClass)
		#	return 1
		return 0
		
	def update(self, fDelta):
		return

	# Religion Button
	def ReligionScreenButton( self, inputClass ):	
		religionI = inputClass.iData2
		if ( inputClass.eNotifyCode == NotifyCode.NOTIFY_CLICKED ) :
			if (religionI < 0 or gc.getGame().getReligionGameTurnFounded(religionI) >= 0) :
				self.iReligionSelected = religionI
				self.iReligionExamined = self.iReligionSelected
				#self.drawCityInfo(self.iReligionSelected)
				
		elif ( inputClass.eNotifyCode == NotifyCode.NOTIFY_CURSOR_MOVE_ON ) :
			if ( religionI < 0 or gc.getGame().getReligionGameTurnFounded(religionI) >= 0) :
				self.iReligionExamined = religionI
				#self.drawCityInfo(self.iReligionExamined)
		elif ( inputClass.eNotifyCode == NotifyCode.NOTIFY_CURSOR_MOVE_OFF ) :
			self.iReligionExamined = self.iReligionSelected
			#self.drawCityInfo(self.iReligionSelected)
			
		screen = self.getScreen()
		self.updateReligionButtons(screen)
		self.updateFooter(screen)
		self.updateCityTable(screen, "CityTable", self.iReligionExamined)
		return 0

	def ReligionConvert(self, inputClass):
		screen = self.getScreen()
		if (inputClass.getNotifyCode() == NotifyCode.NOTIFY_CLICKED) :
			screen.hideScreen()
		
	def ReligionCancel(self, inputClass):
		screen = self.getScreen()
		if (inputClass.getNotifyCode() == NotifyCode.NOTIFY_CLICKED) :
			self.iReligionSelected = self.iReligionOriginal
			if -1 == self.iReligionSelected:
				self.iReligionSelected = gc.getNumReligionInfos()
			self.iReligionExamined = self.iReligionSelected
			self.updateReligionButtons(screen)
			self.updateFooter(screen)
			self.updateCityTable(screen, "CityTable", self.iReligionExamined)
		
	def calculateBuilding (self, city, bldg):
		if city.getNumBuilding(bldg) > 0:
			return self.objectHave
#			if city.getNumActiveBuilding(bldg) > 0:
#				return self.objectHave
#			else:
#				return self.objectHaveObsolete
		elif city.GetCy().getFirstBuildingOrder(bldg) != -1:
			return self.objectUnderConstruction
		elif city.GetCy().canConstruct(bldg, False, False, False):
			return self.objectPossible
		elif city.GetCy().canConstruct(bldg, True, False, False):
			return self.objectPossibleConcurrent
		else:
			return self.objectNotPossible