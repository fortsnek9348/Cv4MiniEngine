from CvPythonExtensions import *
import CvUtil
import ScreenInput
import CvScreenEnums

# globals
gc = CyGlobalContext()
localText = CyTranslator()

class CvDomesticAdvisor:
	"Domestic Advisor Screen"
	def __init__(self):
		self.listSelectedCities = []
		
		
		
	# Screen construction function
	def interfaceScreen(self):
	
		# From BUG
		self.bulletIcon = u"%c" % (CyGame().getSymbolID(FontSymbols.BULLET_CHAR))
		self.happyIcon = u"%c" % (CyGame().getSymbolID(FontSymbols.HAPPY_CHAR))
		self.religionIcon = u"%c" % (CyGame().getSymbolID(FontSymbols.RELIGION_CHAR))
		self.cultureIcon = u"%c" % (gc.getCommerceInfo(CommerceTypes.COMMERCE_CULTURE).getChar())
		self.researchIcon = u"%c" % (gc.getCommerceInfo(CommerceTypes.COMMERCE_RESEARCH).getChar())
		self.goldIcon = u"%c" % (gc.getCommerceInfo(CommerceTypes.COMMERCE_GOLD).getChar())
		self.hammerIcon = u"%c" % (gc.getYieldInfo(YieldTypes.YIELD_PRODUCTION).getChar())
		self.espionageIcon = u"%c" % (gc.getCommerceInfo(CommerceTypes.COMMERCE_ESPIONAGE).getChar())
		
		# From BUG
		self.SPECIALIST_ICON_DICT = [
			self.bulletIcon, # Citizen
			self.religionIcon, # Priest
			self.cultureIcon, # Artist
			self.researchIcon, # Scientist
			self.goldIcon, # Merchant
			self.hammerIcon, # Engineer
			self.espionageIcon, # Engineer
		]
	
		player = gc.getPlayer(gc.getGame().getActivePlayer())
		
		# Create a new screen, called DomesticAdvisur, using the file CvDomesticAdvisor.py for input
		screen = CyGInterfaceScreen("DomesticAdvisor", CvScreenEnums.DOMESTIC_ADVISOR)

		screen.setInitialTitle(localText.getText("TXT_KEY_DOMESTIC_ADVISOR_TITLE", ()))
		#screen.setRenderInterfaceOnly(True)
		screen.setModal(False)
		#screen.setDimensions(15, 100, self.nScreenWidth, self.nScreenHeight)
		
		root = ""
		
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # Table
			TableLayoutConfigCell(ivec2(0, 1), ivec2(1, 1), RectJustilign(EJustilign.Stretch, EJustilign.Stretch)) # Footer
		]
		table.gap = ivec2(0, 1)
		screen.setTableLayout(root, table)
		
		screen.newRichTextTable(root, "CityTable", [
			localText.getText("TXT_KEY_DOMESTIC_ADVISOR_NAME", ()),
			localText.getText("TXT_KEY_POPULATION", ()),
			u"%c" % (CyGame().getSymbolID(FontSymbols.HAPPY_CHAR),),
			u"%c" % (CyGame().getSymbolID(FontSymbols.HEALTHY_CHAR),),
			u"%c" % (gc.getYieldInfo(YieldTypes.YIELD_FOOD).getChar(),),
			u"%c" % (gc.getYieldInfo(YieldTypes.YIELD_PRODUCTION).getChar(),),
			u"%c" % (gc.getCommerceInfo(CommerceTypes.COMMERCE_GOLD).getChar(),),
			u"%c" % (gc.getCommerceInfo(CommerceTypes.COMMERCE_RESEARCH).getChar(),),
			u"%c" % (gc.getCommerceInfo(CommerceTypes.COMMERCE_ESPIONAGE).getChar(),),
			u"%c" % (gc.getCommerceInfo(CommerceTypes.COMMERCE_CULTURE).getChar(),),
			u"%c" % (CyGame().getSymbolID(FontSymbols.TRADE_CHAR),),
			u"%c" % (CyGame().getSymbolID(FontSymbols.BAD_GOLD_CHAR),),
			u"%c" % (CyGame().getSymbolID(FontSymbols.GREAT_PEOPLE_CHAR),),
			u"%c" % (CyGame().getSymbolID(FontSymbols.DEFENSE_CHAR)),
			localText.getText("TXT_KEY_DOMESTIC_ADVISOR_PRODUCING", ()),
			u"%c" % CyGame().getSymbolID(FontSymbols.OCCUPATION_CHAR), # liberate
		], WidgetTypes.WIDGET_PYTHON, -1, -1)
		
		screen.newPanel(root, "Footer")
		
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 0)]
		table.rows = [TableLayoutConfigRowColDesc(0, 1)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # SpecialistsPanel
			TableLayoutConfigCell(ivec2(1, 0)), # DomesticSplit
			TableLayoutConfigCell(ivec2(2, 0)) # Exit
		]
		screen.setTableLayout("Footer", table)
		
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 0)]
		table.rows = [TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), RectJustilign(EJustilign.Center, EJustilign.Stretch)), # name
			TableLayoutConfigCell(ivec2(0, 1), ivec2(1, 1), RectJustilign(EJustilign.Center, EJustilign.Stretch)), # dec/inc
			TableLayoutConfigCell(ivec2(0, 2), ivec2(1, 1), RectJustilign(EJustilign.Center, EJustilign.Stretch)), # count
		]
		table.repeatAxis = HeckTuiAxis.Horizontal
		table.gap = ivec2(1, 0)
		screen.newPanel("Footer", "SpecialistsPanel")
		screen.setTableLayout("SpecialistsPanel", table)
		#screen.setHFlowLayout("SpecialistsPanel")
		
		#if (bCanLiberate or gc.getPlayer(gc.getGame().getActivePlayer()).canSplitEmpire()):
		screen.newActionButton("Footer", "DomesticSplit", WidgetTypes.WIDGET_ACTION, gc.getControlInfo(ControlTypes.CONTROL_FREE_COLONY).getActionInfoIndex(), -1)
		screen.setActionButtonLabel("DomesticSplit", u"%c" % CyGame().getSymbolID(FontSymbols.OCCUPATION_CHAR))

		screen.newActionButton("Footer", "ExitButton", WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)
		screen.setActionButtonLabel("ExitButton", localText.getText("TXT_KEY_PEDIA_SCREEN_EXIT", ()).upper())

		self.updateCityTable(screen)
		self.updateSpecialists(screen)

		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)
		
	def updateCityTable(self, screen):
		screen.clearRichTextTableRows("CityTable")
		screen.hide("DomesticSplit")
		player = gc.getPlayer(CyGame().getActivePlayer())
		self.cityIdList = []
		(pLoopCity, iter) = player.firstCity(False)
		while pLoopCity:
			self.appendCity(screen, pLoopCity)
			(pLoopCity, iter) = player.nextCity(iter, False)
		self.updateSelectionFromEngineToTable(screen)
			
	def updateSpecialists(self, screen):
		prefix = "SpecialistControl_"
		screen.delByPrefix(prefix)
		activeCityI = screen.getRichTextTableActiveRowIndex("CityTable")
		if activeCityI < 0:
			return # hide specialists
			
		player = gc.getPlayer(CyGame().getActivePlayer())
		city = player.getCity(self.cityIdList[activeCityI])
		
		#table = TableLayoutConfig()
		#table.cols = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(1, 0)]
		#table.rows = [TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 0)]
		#table.cells = [
		#	TableLayoutConfigCell(ivec2(0, 0), ivec2(2, 1), RectJustilign(EJustilign.Center, EJustilign.Stretch)), # name
		#	TableLayoutConfigCell(ivec2(0, 1), ivec2(1, 1), RectJustilign(EJustilign.End, EJustilign.Stretch)), # dec
		#	TableLayoutConfigCell(ivec2(1, 1), ivec2(1, 1), RectJustilign(EJustilign.Start, EJustilign.Stretch)), # inc
		#	TableLayoutConfigCell(ivec2(0, 2), ivec2(2, 1), RectJustilign(EJustilign.Center, EJustilign.Stretch)), # count
		#]
		#table.repeatAxis = HeckTuiAxis.Horizontal
		#table.gap = ivec2(0, 0)

		# Citizen Buttons
		for i in range(gc.getNumSpecialistInfos()):
			info = gc.getSpecialistInfo(i)
			if not info.isVisible():
				continue
			specialistPrefix = prefix + str(i) + '_'
			
			#parent = specialistPrefix + "Panel"
			#screen.newPanel("SpecialistsPanel", parent)
			#screen.setTableLayout(parent, table)
			parent = "SpecialistsPanel"
			incdecPanel = specialistPrefix + "IncDecPanel"
			
			screen.newLabel(parent, specialistPrefix + "Name", localText.getText(gc.getSpecialistInfo(i).getTextKey(), ()))
			screen.setLabelWidgetData(specialistPrefix + "Name", WidgetTypes.WIDGET_CITIZEN, i, -1)
			screen.newPanel(parent, incdecPanel)
			screen.setHFlowLayout(incdecPanel)
			screen.newActionButton(incdecPanel, specialistPrefix + "Inc", WidgetTypes.WIDGET_CHANGE_SPECIALIST, i, 1, HeckTuiBorderStyle.NONE)
			screen.newLabel(incdecPanel, specialistPrefix + "Gap", self.SPECIALIST_ICON_DICT[i]) # gap for centering around the slash below
			screen.newActionButton(incdecPanel, specialistPrefix + "Dec", WidgetTypes.WIDGET_CHANGE_SPECIALIST, i, -1, HeckTuiBorderStyle.NONE)
			screen.newLabel(parent, specialistPrefix + "Count", str(city.getSpecialistCount(i)) + "/" + str(city.getMaxSpecialistCount(i)))
		
	
	def appendCity(self, screen, pLoopCity):
		# From original code
		szName = pLoopCity.getName()
		if pLoopCity.isCapital():
			szName += (u"%c" % CyGame().getSymbolID(FontSymbols.STAR_CHAR))
		elif pLoopCity.isGovernmentCenter():
			szName += (u"%c" % CyGame().getSymbolID(FontSymbols.SILVER_STAR_CHAR))
		
		szDecoration = u""
		for iReligion in range(gc.getNumReligionInfos()):
			if pLoopCity.isHasReligion(iReligion):
				if pLoopCity.isHolyCityByType(iReligion):
					szDecoration += (u"%c" % gc.getReligionInfo(iReligion).getHolyCityChar())
				else:
					szDecoration += (u"%c" % gc.getReligionInfo(iReligion).getChar())
						
		for iCorporation in range(gc.getNumCorporationInfos()):
			if pLoopCity.isHeadquartersByType(iCorporation):
				szDecoration += (u"%c" % gc.getCorporationInfo(iCorporation).getHeadquarterChar())
			elif pLoopCity.isActiveCorporation(iCorporation):
				szDecoration += (u"%c" % gc.getCorporationInfo(iCorporation).getChar())
				
		if len(szDecoration):
			szName = szName + u' ' + szDecoration
				
		# Happiness...
		iNetHappy = pLoopCity.happyLevel() - pLoopCity.unhappyLevel(0)
		szHappyText = unicode(iNetHappy)
		if iNetHappy > 0:
			szHappyText = localText.getText("TXT_KEY_COLOR_POSITIVE", ()) + szHappyText + localText.getText("TXT_KEY_COLOR_REVERT", ())
		elif iNetHappy < 0:
			szHappyText = localText.getText("TXT_KEY_COLOR_NEGATIVE", ()) + szHappyText + localText.getText("TXT_KEY_COLOR_REVERT", ())
			
			
		# Health...
		iNetHealth = pLoopCity.goodHealth() - pLoopCity.badHealth(0)
		szHealthText = unicode(iNetHealth)
		if iNetHealth > 0:
			szHealthText = localText.getText("TXT_KEY_COLOR_POSITIVE", ()) + szHealthText + localText.getText("TXT_KEY_COLOR_REVERT", ())
		elif iNetHealth < 0:
			szHealthText = localText.getText("TXT_KEY_COLOR_NEGATIVE", ()) + szHealthText + localText.getText("TXT_KEY_COLOR_REVERT", ())
		
		# Food		
		iNetFood = pLoopCity.foodDifference(true)
		szFoodText = unicode(iNetFood)
		if iNetFood > 0:
			szFoodText = localText.getText("TXT_KEY_COLOR_POSITIVE", ()) + szFoodText + localText.getText("TXT_KEY_COLOR_REVERT", ())
		elif iNetFood < 0:
			szFoodText = localText.getText("TXT_KEY_COLOR_NEGATIVE", ()) + szFoodText + localText.getText("TXT_KEY_COLOR_REVERT", ())
			
		# Culture status...
		szCulture = unicode(pLoopCity.getCommerceRate(CommerceTypes.COMMERCE_CULTURE))
		iCultureTimes100 = pLoopCity.getCultureTimes100(CyGame().getActivePlayer())
		iCultureRateTimes100 = pLoopCity.getCommerceRateTimes100(CommerceTypes.COMMERCE_CULTURE)
		if iCultureRateTimes100 > 0:
			iCultureLeftTimes100 = 100 * pLoopCity.getCultureThreshold() - iCultureTimes100
			if iCultureLeftTimes100 > 0:
				szCulture += u" (" + unicode((iCultureLeftTimes100  + iCultureRateTimes100 - 1) / iCultureRateTimes100) + u")"
				
		# Great Person
		iGreatPersonRate = pLoopCity.getGreatPeopleRate()
		szGreatPerson = unicode(iGreatPersonRate)
		if iGreatPersonRate > 0:
			iGPPLeft = gc.getPlayer(gc.getGame().getActivePlayer()).greatPeopleThreshold(false) - pLoopCity.getGreatPeopleProgress()
			if iGPPLeft > 0:
				iTurnsLeft = iGPPLeft / pLoopCity.getGreatPeopleRate()
				if iTurnsLeft * pLoopCity.getGreatPeopleRate() <  iGPPLeft:
					iTurnsLeft += 1
				szGreatPerson += u" (" + unicode(iTurnsLeft) + u")"
				
		isLiberate = pLoopCity.getLiberationPlayer(False) != -1
					
		cells = [
			szName,
			unicode(pLoopCity.getPopulation()),
			szHappyText,
			szHealthText,
			szFoodText,
			unicode(pLoopCity.getYieldRate(YieldTypes.YIELD_PRODUCTION)),
			unicode(pLoopCity.getCommerceRate(CommerceTypes.COMMERCE_GOLD)),
			unicode(pLoopCity.getCommerceRate(CommerceTypes.COMMERCE_RESEARCH)),
			unicode(pLoopCity.getCommerceRate(CommerceTypes.COMMERCE_ESPIONAGE)),
			szCulture,
			unicode(pLoopCity.getTradeYield(YieldTypes.YIELD_COMMERCE)),
			unicode(pLoopCity.getMaintenance()),
			szGreatPerson,
			unicode(pLoopCity.plot().getNumDefenders(pLoopCity.getOwner())),
			pLoopCity.getProductionName() + u" (" + unicode(pLoopCity.getGeneralProductionTurnsLeft()) + u")",
			u"%c" % CyGame().getSymbolID(FontSymbols.OCCUPATION_CHAR) if isLiberate else u""
		]
		
		keys = [
			0, # szName,
			pLoopCity.getPopulation(),
			iNetHappy, # szHappyText,
			iNetHealth, # szHealthText,
			iNetFood, # szFoodText,
			pLoopCity.getYieldRate(YieldTypes.YIELD_PRODUCTION),
			pLoopCity.getCommerceRate(CommerceTypes.COMMERCE_GOLD),
			pLoopCity.getCommerceRate(CommerceTypes.COMMERCE_RESEARCH),
			pLoopCity.getCommerceRate(CommerceTypes.COMMERCE_ESPIONAGE),
			iCultureTimes100, # szCulture,
			pLoopCity.getTradeYield(YieldTypes.YIELD_COMMERCE),
			pLoopCity.getMaintenance(),
			iGreatPersonRate, # szGreatPerson,
			pLoopCity.plot().getNumDefenders(pLoopCity.getOwner()),
			0, # pLoopCity.getProductionName() + u" (" + unicode(pLoopCity.getGeneralProductionTurnsLeft()) + u")",
			1 if isLiberate else 0,
		]
		
		screen.addRichTextTableRow("CityTable", cells, keys)
		
		if isLiberate or gc.getPlayer(gc.getGame().getActivePlayer()).canSplitEmpire():
			screen.show("DomesticSplit")
		
		self.cityIdList.append(pLoopCity.getID())
	
	def updateSelectionFromTableToEngine(self, screen):
		player = gc.getPlayer(CyGame().getActivePlayer())
		activeCityI = screen.getRichTextTableActiveRowIndex("CityTable")
		selectedCityIndices = screen.getRichTextTableSelectedRows("CityTable")
		
		CyInterface().clearSelectedCities()
		
		isActiveCitySelected = False
		for cityI in selectedCityIndices:
			if cityI != activeCityI: # Add active last
				CyInterface().addSelectedCity(player.getCity(self.cityIdList[cityI]))
			else:
				isActiveCitySelected = True
				
		if isActiveCitySelected:
			CyInterface().addSelectedCity(player.getCity(self.cityIdList[activeCityI]))
			
	def updateSelectionFromEngineToTable(self, screen):
		player = gc.getPlayer(CyGame().getActivePlayer())
		
		selectedCityIds = []
		(pLoopCity, iter) = player.firstCity(False)
		while pLoopCity:
			if CyInterface().isCitySelected(pLoopCity):
				selectedCityIds.append(pLoopCity.getID())
			(pLoopCity, iter) = player.nextCity(iter, False)

		cityIdToIndex = { cityId : i for i, cityId in enumerate(self.cityIdList) }
			
		selectedCityIndices = [cityIdToIndex[cityId] for cityId in selectedCityIds]
		activeCity = CyInterface().getHeadSelectedCity()
		if activeCity is not None and activeCity.getID() != -1:
			activeCityI = cityIdToIndex[activeCity.getID()]
		else:
			activeCityI = -1

		screen.setRichTextTableSelectedRows("CityTable", selectedCityIndices, activeCityI)
				
	# Will handle the input for this screen...
	def handleInput(self, inputClass):
		if inputClass.getNotifyCode() == NotifyCode.NOTIFY_LISTBOX_ITEM_SELECTED:
			screen = CyGInterfaceScreen("DomesticAdvisor", CvScreenEnums.DOMESTIC_ADVISOR)
			self.updateSelectionFromTableToEngine(screen)
			self.updateSpecialists(screen)
			
			activeCityI = screen.getRichTextTableActiveRowIndex("CityTable")
			if activeCityI >= 0:
				CyInterface().lookAtCityOffset(self.cityIdList[activeCityI])
				
				
		
			#if (inputClass.getMouseX() == 0):
			#	screen = CyGInterfaceScreen( "DomesticAdvisor", CvScreenEnums.DOMESTIC_ADVISOR )
			#	screen.hideScreen()
			#	
			#	CyInterface().selectCity(gc.getPlayer(inputClass.getData1()).getCity(inputClass.getData2()), true);
			#	
			#	popupInfo = CyPopupInfo()
			#	popupInfo.setButtonPopupType(ButtonPopupTypes.BUTTONPOPUP_PYTHON_SCREEN)
			#	popupInfo.setText(u"showDomesticAdvisor")
			#	popupInfo.addPopup(inputClass.getData1())		
			#else:
			#	self.updateAppropriateCitySelection()
			#	self.updateSpecialists()
		elif inputClass.getNotifyCode() == NotifyCode.NOTIFY_CLICKED:
			if inputClass.getButtonType() == WidgetTypes.WIDGET_ACTION: # DomesticSplit
				screen = CyGInterfaceScreen("DomesticAdvisor", CvScreenEnums.DOMESTIC_ADVISOR)
				screen.hideScreen()
			
		return 0
						
	def update(self, t):
		# Set by CvMainInterface.
		if CyInterface().isDirty(InterfaceDirtyBits.Domestic_Advisor_DIRTY_BIT):
			CyInterface().setDirty(InterfaceDirtyBits.Domestic_Advisor_DIRTY_BIT, False)
			screen = CyGInterfaceScreen("DomesticAdvisor", CvScreenEnums.DOMESTIC_ADVISOR)
			self.updateCityTable(screen)
			self.updateSpecialists(screen)
		
		return