## Sid Meier's Civilization 4
## Copyright Firaxis Games 2005

# fortsnek: BUG finance advisor adapted to Cv4MiniEngine.

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

if kHasBUG:
	import PlayerUtil
	import TradeUtil

# globals
gc = CyGlobalContext()
localText = CyTranslator()

class BugFinanceAdvisor:

	def __init__(self):
		self.SCREEN_NAME = "FinanceAdvisor"
		self.WIDGET_ID = "FinanceAdvisorWidget"
		self.nWidgetCount = 0

	def getScreen(self):
		return CyGInterfaceScreen(self.SCREEN_NAME, CvScreenEnums.FINANCE_ADVISOR)

	def interfaceScreen(self):

		self.iActiveLeader = CyGame().getActivePlayer()

		player = gc.getPlayer(self.iActiveLeader)
	
		screen = self.getScreen()
		if screen.isActive():
			return
		#screen.setRenderInterfaceOnly(True);
		screen.setInitialTitle(localText.getText("TXT_KEY_FINANCIAL_ADVISOR_TITLE", ()).upper())

		root = ""
		
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # Content
			TableLayoutConfigCell(ivec2(0, 1), ivec2(1, 1), RectJustilign(EJustilign.Stretch, EJustilign.Stretch)) # Footer
		]
		table.gap = ivec2(0, 1)
		screen.setTableLayout(root, table)
		
		# Two-row table for Treasury and 3-col below.
		screen.newPanel(root, "Content")
		
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 1)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # TreasuryLabel
			TableLayoutConfigCell(ivec2(0, 1)) # DetailTable
		]
		table.gap = ivec2(0, 1)
		screen.setTableLayout("Content", table)
		
		screen.newLabel("Content", "TreasuryLabel", u"")
	
		screen.newPanel("Content", "DetailTable")
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 1)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # CommercePanel
			TableLayoutConfigCell(ivec2(1, 0)), # IncomePanel
			TableLayoutConfigCell(ivec2(2, 0)), # ExpensesPanel
		]
		table.gap = ivec2(1, 0)
		screen.setTableLayout("DetailTable", table)
		screen.newPanel("DetailTable", "CommercePanel")
		screen.newPanel("DetailTable", "IncomePanel")
		screen.newPanel("DetailTable", "ExpensesPanel")
		
		screen.newPanel(root, "Footer")
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), RectJustilign(EJustilign.End, EJustilign.Stretch)), # EXIT
		]
		table.gap = ivec2(1, 0)
		screen.setTableLayout("Footer", table)

		screen.newActionButton("Footer", "ExitButton", WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)
		screen.setActionButtonLabel("ExitButton", localText.getText("TXT_KEY_PEDIA_SCREEN_EXIT", ()).upper())

		# draw the contents
		#self.drawContents()
		
		self.buildUI(screen)
		
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)

	def buildUI(self, screen):
		# fortsnek: Adapting code from BUG/Civ4.
		player = gc.getPlayer(self.iActiveLeader)
	
		numCities = player.getNumCities()	
					
		totalUnitCost = player.calculateUnitCost()
		totalUnitSupply = player.calculateUnitSupply()
		totalMaintenance = player.getTotalMaintenance()
		totalCivicUpkeep = player.getCivicUpkeep([], False)
		totalPreInflatedCosts = player.calculatePreInflatedCosts()
		totalInflatedCosts = player.calculateInflatedCosts()
		goldCommerce = player.getCommerceRate(CommerceTypes.COMMERCE_GOLD)
		if not player.isCommerceFlexible(CommerceTypes.COMMERCE_RESEARCH):
			goldCommerce += player.calculateBaseNetResearch()
		gold = player.getGold()
		goldFromCivs = player.getGoldPerTurn()
		goldPerTurn = player.calculateGoldRate()

		szText = localText.getText("TXT_KEY_FINANCIAL_ADVISOR_TREASURY", (gold, )).upper()
		if gold < 0:
			if goldPerTurn != 0:
				if gold + goldPerTurn >= 0:
					szText += localText.getText("TXT_KEY_MISC_POS_GOLD_PER_TURN", (goldPerTurn,))
				elif goldPerTurn >= 0:
					szText += localText.getText("TXT_KEY_MISC_POS_WARNING_GOLD_PER_TURN", (goldPerTurn,))
				else:
					szText += localText.getText("TXT_KEY_MISC_NEG_GOLD_PER_TURN", (goldPerTurn,))
		else:
			if goldPerTurn != 0:
				if goldPerTurn >= 0:
					szText += localText.getText("TXT_KEY_MISC_POS_GOLD_PER_TURN", (goldPerTurn,))
				elif gold + goldPerTurn >= 0:
					szText += localText.getText("TXT_KEY_MISC_NEG_WARNING_GOLD_PER_TURN", (goldPerTurn,))
				else:
					szText += localText.getText("TXT_KEY_MISC_NEG_GOLD_PER_TURN", (goldPerTurn,))
		screen.setLabelText("TreasuryLabel", szText)
		screen.setLabelHAlign("TreasuryLabel", EAlign.Center)
		screen.setLabelWidgetData("TreasuryLabel", WidgetTypes.WIDGET_HELP_FINANCE_GOLD_RESERVE, -1, -1)
		
		screen.setVFlowLayout("CommercePanel")
		screen.setVFlowLayout("IncomePanel")
		screen.setVFlowLayout("ExpensesPanel")
		
		name = self.getNextWidgetName()
		screen.newLabel("CommercePanel", name, localText.getText("TXT_KEY_CONCEPT_COMMERCE", ()).upper())
		screen.setLabelHAlign(name, EAlign.Center)
		
		name = self.getNextWidgetName()
		screen.newLabel("IncomePanel", name, localText.getText("TXT_KEY_FINANCIAL_ADVISOR_INCOME_HEADER", ()).upper())
		screen.setLabelHAlign(name, EAlign.Center)
		
		name = self.getNextWidgetName()
		screen.newLabel("ExpensesPanel", name, localText.getText("TXT_KEY_FINANCIAL_ADVISOR_EXPENSES_HEADER", ()).upper())
		screen.setLabelHAlign(name, EAlign.Center)
	
		
		#### Commerce
		
		screen.newPanel("CommercePanel", "CommerceTable")
		basicTable = TableLayoutConfig()
		basicTable.cols = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0)]
		basicTable.rows = [TableLayoutConfigRowColDesc(0, 0)]
		basicTable.cells = [
			TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), RectJustilign(EJustilign.Start, EJustilign.Stretch)),
			TableLayoutConfigCell(ivec2(1, 0), ivec2(1, 1), RectJustilign(EJustilign.End, EJustilign.Stretch)),
		]
		basicTable.gap = ivec2(1, 0)
		screen.setTableLayout("CommerceTable", basicTable)
		
		iCommerce = 0
		
		if kHasBUG:
			# sum all worked tiles' commerce yields for player
			# move to MapUtil?
			iWorkedTileCount = 0
			iWorkedTiles = 0
			for city in PlayerUtil.playerCities(player):
			#city, it = player.firstCity(False)
			#while city:
				#if not city.isNone() and not city.isDisorder():
				if not city.isDisorder():
					for i in range(gc.getNUM_CITY_PLOTS()):
						plot = city.getCityIndexPlot(i)
						if plot and not plot.isNone() and plot.hasYield():
							if city.isWorkingPlot(plot):
								iWorkedTileCount += 1
								iWorkedTiles += plot.getYield(YieldTypes.YIELD_COMMERCE)
				#city, it = player.nextCity(it, False)
			
			screen.newLabel("CommerceTable", self.getNextWidgetName(), localText.getText("TXT_KEY_CONCEPT_WORKED_TILES", (iWorkedTileCount,)))
			screen.newLabel("CommerceTable", self.getNextWidgetName(), unicode(iWorkedTiles))
			
			iCommerce += iWorkedTiles
			
			# trade
			iDomesticTrade, _, iForeignTrade, _ = TradeUtil.calculateTradeRoutes(player)
			
			if iDomesticTrade > 0:
				if TradeUtil.isFractionalTrade():
					iDomesticTrade //= 100
				screen.newLabel("CommerceTable", self.getNextWidgetName(), localText.getText("TXT_KEY_CONCEPT_DOMESTIC_TRADE", ()))
				screen.newLabel("CommerceTable", self.getNextWidgetName(), unicode(iDomesticTrade))
				iCommerce += iDomesticTrade
			
			if iForeignTrade > 0:
				if TradeUtil.isFractionalTrade():
					iForeignTrade //= 100
				screen.newLabel("CommerceTable", self.getNextWidgetName(), localText.getText("TXT_KEY_CONCEPT_FOREIGN_TRADE", ()))
				screen.newLabel("CommerceTable", self.getNextWidgetName(), unicode(iForeignTrade))
				iCommerce += iForeignTrade
			
			# corporations
			iCorporations = 0
			for city in PlayerUtil.playerCities(player):
				if not city.isDisorder():
					iCorporations += city.getCorporationYield(YieldTypes.YIELD_COMMERCE)
			
			if iCorporations > 0:
				screen.newLabel("CommerceTable", self.getNextWidgetName(), localText.getText("TXT_KEY_CONCEPT_CORPORATIONS", ()))
				screen.newLabel("CommerceTable", self.getNextWidgetName(), unicode(iCorporations))
				iCommerce += iCorporations
			
			# specialists
			iSpecialists = 0
			for city in PlayerUtil.playerCities(player):
				if not city.isDisorder():
					for eSpec in range(gc.getNumSpecialistInfos()):
						iSpecialists += player.specialistYield(eSpec, YieldTypes.YIELD_COMMERCE) * (city.getSpecialistCount(eSpec) + city.getFreeSpecialistCount(eSpec))
			
			if iSpecialists > 0:
				screen.newLabel("CommerceTable", self.getNextWidgetName(), localText.getText("TXT_KEY_CONCEPT_SPECIALISTS", ()))
				screen.newLabel("CommerceTable", self.getNextWidgetName(), unicode(iSpecialists))
				iCommerce += iSpecialists
			
			# buildings
			iTotalCommerce = player.calculateTotalYield(YieldTypes.YIELD_COMMERCE)
			# buildings includes 50% capital bonus for Bureaucracy civic
			iBuildings = iTotalCommerce - iCommerce
			if iBuildings > 0:
				screen.newLabel("CommerceTable", self.getNextWidgetName(), localText.getText("TXT_KEY_CONCEPT_BUILDINGS", ()))
				screen.newLabel("CommerceTable", self.getNextWidgetName(), unicode(iBuildings))
				iCommerce += iBuildings
			
			screen.newLabel("CommerceTable", self.getNextWidgetName(), localText.getText("TXT_KEY_BUG_FINANCIAL_ADVISOR_COMMERCE", ()))
			screen.newLabel("CommerceTable", self.getNextWidgetName(), unicode(iCommerce))
		
		# Slider percentages
		
		for iI in range(CommerceTypes.NUM_COMMERCE_TYPES):
			eCommerce = (iI + 1) % int(CommerceTypes.NUM_COMMERCE_TYPES)
		
			if (player.isCommerceFlexible(eCommerce)):
				name = self.getNextWidgetName()
				screen.newPanel("CommerceTable", name)
				screen.setHFlowLayout(name)
				# These change the UI on update.
				#screen.newActionButton(name, self.getNextWidgetName(), WidgetTypes.WIDGET_CHANGE_PERCENT, eCommerce, gc.getDefineINT("COMMERCE_PERCENT_CHANGE_INCREMENTS"), HeckTuiBorderStyle.NONE)
				#screen.newActionButton(name, self.getNextWidgetName(), WidgetTypes.WIDGET_CHANGE_PERCENT, eCommerce, -gc.getDefineINT("COMMERCE_PERCENT_CHANGE_INCREMENTS"), HeckTuiBorderStyle.NONE)
				screen.newLabel(name, self.getNextWidgetName(), gc.getCommerceInfo(eCommerce).getDescription() + u" (" + unicode(player.getCommercePercent(eCommerce)) + u"%)")
				screen.newLabel("CommerceTable", self.getNextWidgetName(), unicode(player.getCommerceRate(CommerceTypes(eCommerce))))

		screen.newLabel("CommerceTable", self.getNextWidgetName(), gc.getCommerceInfo(CommerceTypes.COMMERCE_GOLD).getDescription() + u" (" + unicode(player.getCommercePercent(CommerceTypes.COMMERCE_GOLD)) + u"%)")
		screen.newLabel("CommerceTable", self.getNextWidgetName(), unicode(goldCommerce))
		
		#### Income
		iTaxRate = player.getCommercePercent(CommerceTypes.COMMERCE_GOLD)
		
		multipliers = []
		for eBldg in range(gc.getNumBuildingInfos()):
			info = gc.getBuildingInfo(eBldg)
			iMultiplier = info.getCommerceModifier(CommerceTypes.COMMERCE_GOLD)
			if iMultiplier > 0:
				multipliers.append([eBldg, iMultiplier, 0, 0.0])
		

		screen.newPanel("IncomePanel", "IncomeTable")
		screen.setTableLayout("IncomeTable", basicTable)
		
		if kHasBUG:
			iBuildingCount = 0
			iHeadquartersCount = 0
			iShrinesCount = 0
			fTaxes = 0.0
			fBuildings = 0.0
			fHeadquarters = 0.0
			fShrines = 0.0
			fCorporations = 0.0
			fSpecialists = 0.0
			iWealthCount = 0
			fWealth = 0.0
			eWealth = gc.getInfoTypeForString("PROCESS_WEALTH")
			# ignores
			#   CyCity.getReligionCommerce() -- excludes shrines
			#   CyPlayer.getFreeCityCommerce()
			#   CyPlayer.getSpecialistExtraCommerce() * (CyCity.getSpecialistPopulation() + CyCity.getNumGreatPeople())
			for city in PlayerUtil.playerCities(player):
				if not city.isDisorder():
					fCityTaxes = city.getYieldRate(YieldTypes.YIELD_COMMERCE) * iTaxRate / 100.0
					fTaxes += fCityTaxes
					
					fCityBuildings = 0.0
					fCityHeadquarters = 0.0
					fCityShrines = 0.0
					for eBldg in range(gc.getNumBuildingInfos()):
						iCount = city.getNumRealBuilding(eBldg)
						if iCount > 0:
							iBuildingGold = city.getBuildingCommerceByBuilding(CommerceTypes.COMMERCE_GOLD, eBldg)
							if iBuildingGold > 0:
								info = gc.getBuildingInfo(eBldg)
								if info.getFoundsCorporation() != -1:
									fCityHeadquarters += iBuildingGold
									iHeadquartersCount += 1
								elif info.getGlobalReligionCommerce() != -1:
									fCityShrines += iBuildingGold
									iShrinesCount += 1
								else:
									fCityBuildings += iBuildingGold
									iBuildingCount += iCount
					fBuildings += fCityBuildings
					fHeadquarters += fCityHeadquarters
					fShrines += fCityShrines
					
					fCityCorporations = city.getCorporationCommerce(CommerceTypes.COMMERCE_GOLD)
					fCorporations += fCityCorporations
					
					fCitySpecialists = city.getSpecialistCommerce(CommerceTypes.COMMERCE_GOLD)
					fSpecialists += fCitySpecialists
					
					fCityWealth = 0.0
					if city.isProductionProcess() and city.getProductionProcess() == eWealth:
						fCityWealth = city.getProductionToCommerceModifier(CommerceTypes.COMMERCE_GOLD) * city.getYieldRate(YieldTypes.YIELD_PRODUCTION) / 100.0
						fWealth += fCityWealth
						iWealthCount += 1
					
					# buildings don't multiply wealth
					fCityTotel = fCityTaxes + fCityBuildings + fCityHeadquarters + fCityCorporations + fCitySpecialists
					for entry in multipliers:
						eBldg, iMultiplier, _, _ = entry
						iCount = city.getNumRealBuilding(eBldg)
						if iCount > 0:
							entry[2] += iCount
							entry[3] += iCount * fCityTotel * iMultiplier / 100.0
			
			iTotalMinusTaxes = int(fBuildings) + int(fCorporations) + int(fSpecialists) + int(fWealth)
			for _, _, _, fGold in multipliers:
				iTotalMinusTaxes += int(fGold)
		
			screen.newLabel("IncomeTable", self.getNextWidgetName(), localText.getText("TXT_KEY_FINANCIAL_ADVISOR_TAXES", ()))
			screen.newLabel("IncomeTable", self.getNextWidgetName(), unicode(goldCommerce - iTotalMinusTaxes))
			
			if fBuildings > 0.0:
				screen.newLabel("IncomeTable", self.getNextWidgetName(), localText.getText("TXT_KEY_CONCEPT_BUILDINGS", ()) + " (%d)" % iBuildingCount)
				screen.newLabel("IncomeTable", self.getNextWidgetName(), unicode(int(fBuildings)))
			
			if fHeadquarters > 0.0:
				screen.newLabel("IncomeTable", self.getNextWidgetName(), localText.getText("TXT_KEY_CORPORATION_HEADQUARTERS", ()) + " (%d)" % iHeadquartersCount)
				screen.newLabel("IncomeTable", self.getNextWidgetName(), unicode(int(fHeadquarters)))
			
			if fCorporations > 0.0:
				screen.newLabel("IncomeTable", self.getNextWidgetName(), localText.getText("TXT_KEY_CONCEPT_CORPORATIONS", ()))
				screen.newLabel("IncomeTable", self.getNextWidgetName(), unicode(int(fCorporations)))
			
			if fShrines > 0.0:
				screen.newLabel("IncomeTable", self.getNextWidgetName(), localText.getText("TXT_KEY_CONCEPT_RELIGIOUS_SHRINES", ()) + " (%d)" % iShrinesCount)
				screen.newLabel("IncomeTable", self.getNextWidgetName(), unicode(int(fShrines)))
			
			if fSpecialists > 0.0:
				screen.newLabel("IncomeTable", self.getNextWidgetName(), localText.getText("TXT_KEY_CONCEPT_SPECIALISTS", ()))
				screen.newLabel("IncomeTable", self.getNextWidgetName(), unicode(int(fSpecialists)))
			
			for eBldg, iMultiplier, iCount, fGold in multipliers:
				if iCount > 0 and fGold > 0.0:
					fAverage = fGold / iCount
					szDescription = gc.getBuildingInfo(eBldg).getDescription() + u" " + localText.getText("TXT_KEY_BUG_FINANCIAL_ADVISOR_BUILDING_COUNT_AVERAGE", (iCount, "%.2f" % fAverage))
					screen.newLabel("IncomeTable", self.getNextWidgetName(), szDescription)
					screen.newLabel("IncomeTable", self.getNextWidgetName(), unicode(int(fGold)))
			
			if fWealth > 0.0 and iWealthCount > 0:
				screen.newLabel("IncomeTable", self.getNextWidgetName(), localText.getText("TXT_KEY_PROCESS_WEALTH", ()) + " (%d)" % iWealthCount)
				screen.newLabel("IncomeTable", self.getNextWidgetName(), unicode(int(fWealth)))
			
			iIncome = goldCommerce
			if (goldFromCivs > 0):
				szText = unicode(goldFromCivs) + " : " + localText.getText("TXT_KEY_FINANCIAL_ADVISOR_PER_TURN", ())
				screen.newLabel("IncomeTable", self.getNextWidgetName(), localText.getText("TXT_KEY_FINANCIAL_ADVISOR_PER_TURN", ()))
				screen.newLabel("IncomeTable", self.getNextWidgetName(), unicode(goldFromCivs))
				iIncome += goldFromCivs
			
			screen.newLabel("IncomeTable", self.getNextWidgetName(), localText.getText("TXT_KEY_FINANCIAL_ADVISOR_INCOME", ()))
			screen.newLabel("IncomeTable", self.getNextWidgetName(), unicode(iIncome))
		else:
			screen.newLabel("IncomeTable", self.getNextWidgetName(), localText.getText("TXT_KEY_FINANCIAL_ADVISOR_TAXES", ()))
			screen.newLabel("IncomeTable", self.getNextWidgetName(), unicode(goldCommerce))
			iIncome = goldCommerce

			if (goldFromCivs > 0):
				screen.newLabel("IncomeTable", self.getNextWidgetName(), localText.getText("TXT_KEY_FINANCIAL_ADVISOR_PER_TURN", ()))
				screen.newLabel("IncomeTable", self.getNextWidgetName(), unicode(goldFromCivs))
				iIncome += goldFromCivs

			screen.newLabel("IncomeTable", self.getNextWidgetName(), localText.getText("TXT_KEY_FINANCIAL_ADVISOR_INCOME", ()))
			screen.newLabel("IncomeTable", self.getNextWidgetName(), unicode(iIncome))
			
			
		#### Expenses
		
		screen.newPanel("ExpensesPanel", "ExpensesTable")
		screen.setTableLayout("ExpensesTable", basicTable)
		
		iExpenses = 0
		
		screen.newLabel("ExpensesTable", self.getNextWidgetName(), localText.getText("TXT_KEY_FINANCIAL_ADVISOR_UNITCOST", ()), WidgetTypes.WIDGET_HELP_FINANCE_UNIT_COST, self.iActiveLeader, 1)
		screen.newLabel("ExpensesTable", self.getNextWidgetName(), unicode(totalUnitCost), WidgetTypes.WIDGET_HELP_FINANCE_UNIT_COST, self.iActiveLeader, 1)
		iExpenses += totalUnitCost
		
		screen.newLabel("ExpensesTable", self.getNextWidgetName(), localText.getText("TXT_KEY_FINANCIAL_ADVISOR_UNITSUPPLY", ()), WidgetTypes.WIDGET_HELP_FINANCE_AWAY_SUPPLY, self.iActiveLeader, 1)
		screen.newLabel("ExpensesTable", self.getNextWidgetName(), unicode(totalUnitSupply), WidgetTypes.WIDGET_HELP_FINANCE_AWAY_SUPPLY, self.iActiveLeader, 1)
		iExpenses += totalUnitSupply
		
		screen.newLabel("ExpensesTable", self.getNextWidgetName(), localText.getText("TXT_KEY_FINANCIAL_ADVISOR_MAINTENANCE", ()), WidgetTypes.WIDGET_HELP_FINANCE_CITY_MAINT, self.iActiveLeader, 1)
		screen.newLabel("ExpensesTable", self.getNextWidgetName(), unicode(totalMaintenance), WidgetTypes.WIDGET_HELP_FINANCE_CITY_MAINT, self.iActiveLeader, 1)
		iExpenses += totalMaintenance
		
		screen.newLabel("ExpensesTable", self.getNextWidgetName(), localText.getText("TXT_KEY_FINANCIAL_ADVISOR_CIVICS", ()), WidgetTypes.WIDGET_HELP_FINANCE_CIVIC_UPKEEP, self.iActiveLeader, 1)
		screen.newLabel("ExpensesTable", self.getNextWidgetName(), unicode(totalCivicUpkeep), WidgetTypes.WIDGET_HELP_FINANCE_CIVIC_UPKEEP, self.iActiveLeader, 1)
		iExpenses += totalCivicUpkeep
		
		if (goldFromCivs < 0):
			screen.newLabel("ExpensesTable", self.getNextWidgetName(), localText.getText("TXT_KEY_FINANCIAL_ADVISOR_COST_PER_TURN", ()), WidgetTypes.WIDGET_HELP_FINANCE_FOREIGN_INCOME, self.iActiveLeader, 1)
			screen.newLabel("ExpensesTable", self.getNextWidgetName(), unicode(-goldFromCivs), WidgetTypes.WIDGET_HELP_FINANCE_FOREIGN_INCOME, self.iActiveLeader, 1)
			iExpenses -= goldFromCivs
		
		iInflation = totalInflatedCosts - totalPreInflatedCosts
		screen.newLabel("ExpensesTable", self.getNextWidgetName(), localText.getText("TXT_KEY_FINANCIAL_ADVISOR_INFLATION", ()), WidgetTypes.WIDGET_HELP_FINANCE_INFLATED_COSTS, self.iActiveLeader, 1)
		screen.newLabel("ExpensesTable", self.getNextWidgetName(), unicode(iInflation), WidgetTypes.WIDGET_HELP_FINANCE_INFLATED_COSTS, self.iActiveLeader, 1)
		iExpenses += iInflation
		
		screen.newLabel("ExpensesTable", self.getNextWidgetName(), localText.getText("TXT_KEY_FINANCIAL_ADVISOR_EXPENSES", ()), WidgetTypes.WIDGET_GENERAL, -1, -1)
		screen.newLabel("ExpensesTable", self.getNextWidgetName(), unicode(iExpenses), WidgetTypes.WIDGET_GENERAL, -1, -1)
		
	# returns a unique ID for a widget in this screen
	def getNextWidgetName(self):
		szName = self.WIDGET_ID + str(self.nWidgetCount)
		self.nWidgetCount += 1
		return szName

	
	# Will handle the input for this screen...
	def handleInput(self, inputClass):
		return 0

	def update(self, fDelta):
		#if (CyInterface().isDirty(InterfaceDirtyBits.Financial_Screen_DIRTY_BIT) == True):
		#	CyInterface().setDirty(InterfaceDirtyBits.Financial_Screen_DIRTY_BIT, False)
		#	self.drawContents()
		return
											
