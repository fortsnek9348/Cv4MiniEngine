# fortsnek: This is an override for the BUG foreign advisor (will only be used if BUG is present).

from CvPythonExtensions import *
import CvUtil
import ScreenInput
import CvScreenEnums
import math
import sys

kHasBUG = False
try:
	import BugUtil
	kHasBUG = True
except ImportError:
	pass
	
if kHasBUG:
	import AttitudeUtil
	import FavoriteCivicDetector
	import TradeUtil
	import BugCore
	import FontUtil

if kHasBUG:
	AdvisorOpt = BugCore.game.Advisors

# globals
gc = CyGlobalContext()
localText = CyTranslator()

NUM_FOREIGN_SCREENS = 5

(
	FOREIGN_BONUS_SCREEN,
	FOREIGN_TECH_SCREEN,
	FOREIGN_RELATIONS_SCREEN, # BUG style GLANCE
	FOREIGN_ACTIVE_TRADE_SCREEN, # Not implemented
	FOREIGN_INFO_SCREEN,  # BUG style
) = range(NUM_FOREIGN_SCREENS)

#kDefaultScreen = FOREIGN_RELATIONS_SCREEN
kDefaultScreen = FOREIGN_BONUS_SCREEN
kScreenName = "ForeignAdvisor"

kCellVCenterAlign = None

# UI Note: I originally had proportional table columns (which expand to the width of the container), but combining wrapped text with other proportional columns
# does not work well because of the table layout algorithm. Initially, thw window is the width of the screen and the proportional columns are sized.
# In the final iteration, the wrapped text is wrapped to a short width, so the table layout gives the extra width to columns with less text. So you end up
# with lots of spare space with text wrapped more than it needs to be.
# Forunately, setting the columns to autosize works just fine. Because the text is so long, the table expands to the width of the screen anyway.

class CvExoticForeignAdvisor:
	"Foreign Advisor Screen"

	def __init__(self):
		self.iScreen = -1
		self.pageList = [
			("TXT_KEY_FOREIGN_ADVISOR_RESOURCES", "BonusPage", CvExoticForeignAdvisor.buildBonusPage),
			("TXT_KEY_FOREIGN_ADVISOR_TECHS"    , "TechsPage", CvExoticForeignAdvisor.buildTechsPage),
			("TXT_KEY_FOREIGN_ADVISOR_GLANCE"   , "GlancePage", CvExoticForeignAdvisor.buildGlancePage),
			("TXT_KEY_FOREIGN_ADVISOR_ACTIVE"   , "ActiveTradePage", None),
			("TXT_KEY_FOREIGN_ADVISOR_INFO"     , "InfoPage", CvExoticForeignAdvisor.buildInfoPage),
		]
		
			
	def killScreen(self):
		self.getScreen().hideScreen()
		self.iScreen = kDefaultScreen

	def getScreen(self):
		return CyGInterfaceScreen(kScreenName, CvScreenEnums.FOREIGN_ADVISOR)

	def interfaceScreen(self, iScreen):
		# Init this here in case headless engine is running.
		global kCellVCenterAlign
		kCellVCenterAlign = RectJustilign(EJustilign.Stretch, EJustilign.Center)
	
		if iScreen < 0:
			iScreen = self.iScreen
		if iScreen < 0 or iScreen >= NUM_FOREIGN_SCREENS:
			iScreen = kDefaultScreen
		if iScreen == FOREIGN_ACTIVE_TRADE_SCREEN:
			iScreen = FOREIGN_BONUS_SCREEN

		if self.iScreen != iScreen:
			self.killScreen()
			self.iScreen = iScreen
		
		screen = self.getScreen()
		#if screen.isActive():
			#print >> sys.stderr, "Early exit"
		#	return
		#print >> sys.stderr, "xxx"
		#screen.setRenderInterfaceOnly(True);
		
		self.isScreenBuild = [False] * NUM_FOREIGN_SCREENS

		self.iActiveLeader = CyGame().getActivePlayer()
		self.objActiveLeader = gc.getPlayer(self.iActiveLeader)
		self.iActiveTeam = self.objActiveLeader.getTeam()
		self.objActiveTeam = gc.getTeam(self.iActiveTeam)
		self.iSelectedLeader = self.iActiveLeader
		
		screen.setInitialTitle(localText.getText("TXT_KEY_FOREIGN_ADVISOR_TITLE", ()).upper())

		root = ""
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [TableLayoutConfigCell(ivec2(0, 0))] * NUM_FOREIGN_SCREENS + [
			TableLayoutConfigCell(ivec2(0, 1), ivec2(1, 1), RectJustilign(EJustilign.Start, EJustilign.Center)),
			TableLayoutConfigCell(ivec2(0, 1), ivec2(1, 1), RectJustilign(EJustilign.End, EJustilign.Center)),
		]
		screen.setTableLayout(root, table)
		
		

		CyInterface().setDirty(InterfaceDirtyBits.Foreign_Screen_DIRTY_BIT, False)

		for i, kv in enumerate(self.pageList):
			key, name, f = kv
			screen.newPanel(root, name)
			screen.hide(name)
			#if f is not None:
			#	f(self, screen, name)
				
		screen.newPanel(root, "Footer")
		screen.setHFlowLayout("Footer")
		
		for i, kv in enumerate(self.pageList):
			key, name, f = kv
			if f is not None:
				screen.newActionButton("Footer", "PageBtn" + str(i), WidgetTypes.WIDGET_PYTHON, 0, i)
				screen.setActionButtonLabel("PageBtn" + str(i), localText.getText(key, ()).upper())
		
		screen.newActionButton(root, "ExitButton", WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)
		screen.setActionButtonLabel("ExitButton", localText.getText("TXT_KEY_PEDIA_SCREEN_EXIT", ()).upper())
		
		self.updatePageDisplay(screen)

		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)
		
	def updatePageDisplay(self, screen):
		for i, kv in enumerate(self.pageList):
			key, name, f = kv
			if self.iScreen == i and not self.isScreenBuild[i]:
				self.isScreenBuild[i] = True
				f(self, screen, name)
			screen.setVisible(name, self.iScreen == i)
		
	# from BUG
	# Find all the surplus resources
	def getSurplusBonuses(self):
		activePlayer = gc.getPlayer(self.iActiveLeader)

		tradeData = TradeData()
		tradeData.ItemType = TradeableItems.TRADE_RESOURCES
		listSurplus = []
		
		for iLoopBonus in range(gc.getNumBonusInfos()):
			tradeData.iData = iLoopBonus
			for iLoopPlayer in range(gc.getMAX_PLAYERS()):
				currentPlayer = gc.getPlayer(iLoopPlayer)
				if ( currentPlayer.isAlive() and not currentPlayer.isBarbarian()
														  and not currentPlayer.isMinorCiv() 
														  and gc.getTeam(currentPlayer.getTeam()).isHasMet(activePlayer.getTeam())
														  and iLoopPlayer != self.iActiveLeader 
														  and activePlayer.canTradeItem(iLoopPlayer, tradeData, False)
														  and activePlayer.getNumTradeableBonuses(iLoopBonus) > 1 ):
					listSurplus.append(iLoopBonus)
					break
		return listSurplus
		
	def buildBonusPage(self, screen, page):
		activePlayer = gc.getPlayer(self.iActiveLeader)
		surplusBonuses = self.getSurplusBonuses()
		
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = [TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 1)]
		table.cells = [TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), kCellVCenterAlign), TableLayoutConfigCell(ivec2(0, 1), ivec2(1, 1), kCellVCenterAlign), TableLayoutConfigCell(ivec2(0, 2), ivec2(1, 1), kCellVCenterAlign)] #, ivec2(1, 1), RectJustilign(EJustilign.Stretch, EJustilign.Stretch))]
		screen.setTableLayout(page, table)

		screen.newLabel(page, "SurplusBonusesList", localText.getText("TXT_KEY_FOREIGN_ADVISOR_SURPLUS_RESOURCES", ()) + u": " + u", ".join("%s(%d)" % (gc.getBonusInfo(i).getDescription(), activePlayer.getNumTradeableBonuses(i) - 1) for i in surplusBonuses))
		#screen.newLabel(page, "TradeTableLabel", localText.getText("TXT_KEY_FOREIGN_ADVISOR_TRADE_TABLE", ()))
		screen.newLabel(page, "TradeTableLabel", " ")
		screen.newScrollBarPanel(page, "TradeTableScrollPanel", "TradeTable", HeckTuiAxis.Vertical)
		#screen.newPanel(page, "TradeTable")
		
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 0)] + [TableLayoutConfigRowColDesc(0, 0)] * 8
		table.gap = ivec2(1, 1)
		rows = [TableLayoutConfigRowColDesc(0, 0)]
		cells = [TableLayoutConfigCell(ivec2(i, 0), ivec2(1, 1), kCellVCenterAlign) for i in range(9)]
		
		exportStr = localText.getText("TXT_KEY_FOREIGN_ADVISOR_EXPORT", ())
		importStr = localText.getText("TXT_KEY_FOREIGN_ADVISOR_IMPORT", ())
		activeStr = localText.getText("TXT_KEY_FOREIGN_ADVISOR_ACTIVE", ())
		willStr = localText.getText("TXT_KEY_FOREIGN_ADVISOR_FOR_TRADE_2", ())
		wontStr = localText.getText("TXT_KEY_FOREIGN_ADVISOR_NOT_FOR_TRADE_2", ())
		goldStr = u"%c" % gc.getCommerceInfo(CommerceTypes.COMMERCE_GOLD).getChar()
		screen.newLabel("TradeTable", "TradeTableHeader0", localText.getText("TXT_KEY_FOREIGN_ADVISOR_LEADER", ()))
		screen.newLabel("TradeTable", "TradeTableHeader1", exportStr + u' ' + willStr)
		screen.newLabel("TradeTable", "TradeTableHeader2", exportStr + u' ' + wontStr)
		screen.newLabel("TradeTable", "TradeTableHeader3", importStr + u' ' + willStr)
		screen.newLabel("TradeTable", "TradeTableHeader4", importStr + u' ' + wontStr)
		screen.newLabel("TradeTable", "TradeTableHeader5", importStr + u' ' + goldStr)
		screen.newLabel("TradeTable", "TradeTableHeader6", activeStr + u' ' + exportStr)
		screen.newLabel("TradeTable", "TradeTableHeader7", activeStr + u' ' + importStr)
		screen.newLabel("TradeTable", "TradeTableHeader8", activeStr + u' ' + goldStr)
		
		rowI = 1
		
		# build the foreign trade table
		for tradePlayerI in range(gc.getMAX_PLAYERS()):
			tradePlayer = gc.getPlayer(tradePlayerI)
			isPossibleTradePartner = tradePlayer.isAlive() and not tradePlayer.isBarbarian() and not tradePlayer.isMinorCiv() \
				and gc.getTeam(tradePlayer.getTeam()).isHasMet(activePlayer.getTeam()) \
				and tradePlayerI != self.iActiveLeader
			if not isPossibleTradePartner:
				continue
				
			rows += [TableLayoutConfigRowColDesc(0, 0)]

			message = None
			if not activePlayer.canTradeNetworkWith(tradePlayerI):
				message = localText.getText("TXT_KEY_FOREIGN_ADVISOR_NOT_CONNECTED", ())
				cells += [TableLayoutConfigCell(ivec2(0, rowI), ivec2(1, 1), kCellVCenterAlign), TableLayoutConfigCell(ivec2(1, rowI), ivec2(4, 1), kCellVCenterAlign)] + [TableLayoutConfigCell(ivec2(i, rowI), ivec2(1, 1), kCellVCenterAlign) for i in range(5, 9)]
			else:
				cells += [TableLayoutConfigCell(ivec2(i, rowI), ivec2(1, 1), kCellVCenterAlign) for i in range(9)]
				
			rowI += 1
				
			maxImportGold = 0
			if gc.getTeam(activePlayer.getTeam()).isGoldTrading() or gc.getTeam(tradePlayer.getTeam()).isGoldTrading():
				maxImportGold = tradePlayer.AI_maxGoldPerTurnTrade(self.iActiveLeader)

			exportWillTradeList = []
			exportWontTradeList = []
			importWillTradeList = []
			importWontTradeList = []
			activeExportList = []
			activeImportList = []
			activeGold = 0
			
			for iLoopBonus in range(gc.getNumBonusInfos()):
				tradeData = TradeData()
				tradeData.ItemType = TradeableItems.TRADE_RESOURCES
				tradeData.iData = iLoopBonus
				if activePlayer.canTradeItem(tradePlayerI, tradeData, False):
					if activePlayer.canTradeItem(tradePlayerI, tradeData, not tradePlayer.isHuman()):
						exportWillTradeList.append(iLoopBonus)
					else: # used
						exportWontTradeList.append(iLoopBonus)
				if tradePlayer.canTradeItem(self.iActiveLeader, tradeData, False):
					if tradePlayer.canTradeItem(self.iActiveLeader, tradeData, not tradePlayer.isHuman()): # will trade
						importWillTradeList.append(iLoopBonus)
					else: # won't trade
						importWontTradeList.append(iLoopBonus)
						
			for iLoopDeal in range(gc.getGame().getIndexAfterLastDeal()):
				deal = gc.getGame().getDeal(iLoopDeal)
				if deal.isNone():
					continue
					
				dealPlayers = (deal.getFirstPlayer(), deal.getSecondPlayer())
				activeIsFirst = dealPlayers[0] == self.iActiveLeader
				if not activeIsFirst:
					dealPlayers = reversed(dealPlayers)
				
				if dealPlayers != (self.iActiveLeader, tradePlayerI):
					continue

				getLengthTrades = (CyDeal.getLengthFirstTrades, CyDeal.getLengthSecondTrades)
				getTrade = (CyDeal.getFirstTrade, CyDeal.getSecondTrade)
				
				if not activeIsFirst:
					getLengthTrades = reversed(getLengthTrades)
					getTrade = reversed(getTrade)
					
				for iLoopTradeItem in range(getLengthTrades[0](deal)):
					tradeData = getTrade[0](deal, iLoopTradeItem)
					if tradeData.ItemType == TradeableItems.TRADE_GOLD_PER_TURN:
						activeGold -= tradeData.iData
					if tradeData.ItemType == TradeableItems.TRADE_RESOURCES:
						activeExportList.append(tradeData.iData)

				for iLoopTradeItem in range(getLengthTrades[1](deal)):
					tradeData = getTrade[1](deal, iLoopTradeItem)
					if tradeData.ItemType == TradeableItems.TRADE_GOLD_PER_TURN:
						activeGold += tradeData.iData
					if tradeData.ItemType == TradeableItems.TRADE_RESOURCES:
						activeImportList.append(tradeData.iData)
						
			def makeBonusListString(L):
				return ", ".join(gc.getBonusInfo(i).getDescription() for i in L)
				
			rowPrefix = "TradeTableRow" + str(tradePlayerI) + "_"
			#screen.newLabel("TradeTable", rowPrefix + "Name", tradePlayer.getName())
			screen.newActionButton("TradeTable", rowPrefix + "Name", WidgetTypes.WIDGET_LEADERHEAD, tradePlayerI, self.iActiveLeader)
			screen.setActionButtonForceDisableShiftClickHeck(rowPrefix + "Name")
			if message is None:
				screen.newLabel("TradeTable", rowPrefix + "ExportWill", makeBonusListString(exportWillTradeList))
				screen.newLabel("TradeTable", rowPrefix + "ExportWont", makeBonusListString(exportWontTradeList))
				screen.newLabel("TradeTable", rowPrefix + "ImportWill", makeBonusListString(importWillTradeList))
				screen.newLabel("TradeTable", rowPrefix + "ImportWont", makeBonusListString(importWontTradeList))
				for suffix in ("ExportWill", "ExportWont", "ImportWill", "ImportWont"):
					screen.setLabelEnableWrapping(rowPrefix + suffix)
			else:
				screen.newLabel("TradeTable", rowPrefix + "Message", message)
			screen.newLabel("TradeTable", rowPrefix + "ImportGold", str(maxImportGold))
			screen.newLabel("TradeTable", rowPrefix + "ActiveExport", makeBonusListString(activeExportList))
			screen.newLabel("TradeTable", rowPrefix + "ActiveImport", makeBonusListString(activeImportList))
			screen.newLabel("TradeTable", rowPrefix + "ActiveGold", str(activeGold))

		table.rows = rows
		table.cells = cells
		screen.setTableLayout("TradeTable", table)
		
	def buildTechsPage(self, screen, page):
		activePlayer = gc.getPlayer(self.iActiveLeader)

		screen.newScrollBarPanel(page, "TechTableScrollPanel", "TechTable", HeckTuiAxis.Vertical)
		screen.setFillLayout(page)
		
		parent = "TechTable"
		
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 0)] + [TableLayoutConfigRowColDesc(0, 0)] * 7
		table.gap = ivec2(1, 1)
		rows = [TableLayoutConfigRowColDesc(0, 0)]
		cells = [TableLayoutConfigCell(ivec2(i, 0)) for i in range(8)]

		goldStr = u"%c" % gc.getCommerceInfo(CommerceTypes.COMMERCE_GOLD).getChar()
		screen.newLabel(parent, parent + "Header0", localText.getText("TXT_KEY_FOREIGN_ADVISOR_LEADER", ()))
		screen.newLabel(parent, parent + "Header1", localText.getText("TXT_KEY_FOREIGN_ADVISOR_WANTS", ()))
		screen.newLabel(parent, parent + "Header2", localText.getText("TXT_KEY_FOREIGN_ADVISOR_CANT_TRADE", ()))
		screen.newLabel(parent, parent + "Header3", localText.getText("TXT_KEY_FOREIGN_ADVISOR_CAN_RESEARCH", ()))
		screen.newLabel(parent, parent + "Header4", goldStr)
		screen.newLabel(parent, parent + "Header5", localText.getText("TXT_KEY_FOREIGN_ADVISOR_FOR_TRADE_2", ()))
		screen.newLabel(parent, parent + "Header6", localText.getText("TXT_KEY_FOREIGN_ADVISOR_NOT_FOR_TRADE_2", ()))
		screen.newLabel(parent, parent + "Header7", localText.getText("TXT_KEY_FOREIGN_ADVISOR_CANT_TRADE", ()))
		
		rowI = 1
		
		# build the foreign trade table
		for tradePlayerI in range(gc.getMAX_PLAYERS()):
			tradePlayer = gc.getPlayer(tradePlayerI)
			isPossibleTradePartner = tradePlayer.isAlive() and not tradePlayer.isBarbarian() and not tradePlayer.isMinorCiv() \
				and gc.getTeam(tradePlayer.getTeam()).isHasMet(activePlayer.getTeam()) \
				and tradePlayerI != self.iActiveLeader
			if not isPossibleTradePartner:
				continue
				
			rows += [TableLayoutConfigRowColDesc(0, 0)]

			message = None
			if not gc.getTeam(activePlayer.getTeam()).isTechTrading() and not gc.getTeam(tradePlayer.getTeam()).isTechTrading():
				message = localText.getText("TXT_KEY_FOREIGN_ADVISOR_NO_TECH_TRADING", ())
				cells += [TableLayoutConfigCell(ivec2(0, rowI), ivec2(1, 1), kCellVCenterAlign), TableLayoutConfigCell(ivec2(1, rowI), ivec2(2, 1), kCellVCenterAlign)] + [TableLayoutConfigCell(ivec2(i, rowI), ivec2(1, 1), kCellVCenterAlign) for i in range(3, 8)]
			else:
				cells += [TableLayoutConfigCell(ivec2(i, rowI), ivec2(1, 1), kCellVCenterAlign) for i in range(8)]
				
			
			rowI += 1
			
			maxGold = 0
			if gc.getTeam(activePlayer.getTeam()).isGoldTrading() or gc.getTeam(tradePlayer.getTeam()).isGoldTrading():
					maxGold = gc.getPlayer(tradePlayerI).AI_maxGoldTrade(self.iActiveLeader)
				
			wantsList = []
			wantsCantTradeList = []
			canResearchList = []
			willTradeList = []
			wontTradeList = []
			cantTradeList = []
			
			if message is None:
				for iLoopTech in range(gc.getNumTechInfos()):
					tradeData = TradeData()
					tradeData.ItemType = TradeableItems.TRADE_TECHNOLOGIES
					tradeData.iData = iLoopTech
					if activePlayer.canTradeItem(tradePlayerI, tradeData, False) and activePlayer.getTradeDenial(tradePlayerI, tradeData) == DenialTypes.NO_DENIAL:
						wantsList.append(iLoopTech)
					elif gc.getTeam(activePlayer.getTeam()).isHasTech(iLoopTech) and tradePlayer.canResearch(iLoopTech, False):
						wantsCantTradeList.append(iLoopTech)
					elif tradePlayer.canResearch(iLoopTech, False):
						canResearchList.append(iLoopTech)
					
					if tradePlayer.canTradeItem(self.iActiveLeader, tradeData, False):
						if (tradePlayer.getTradeDenial(self.iActiveLeader, tradeData) == DenialTypes.NO_DENIAL):
							willTradeList.append(iLoopTech)
						else:
							wontTradeList.append(iLoopTech)
					elif gc.getTeam(tradePlayer.getTeam()).isHasTech(iLoopTech) and activePlayer.canResearch(iLoopTech, False):
						cantTradeList.append(iLoopTech)
		
			def makeTechListString(L):
				return ", ".join(gc.getTechInfo(i).getDescription() for i in L)
				
			rowPrefix = parent + "Row" + str(tradePlayerI) + "_"
			
			screen.newActionButton(parent, rowPrefix + "Name", WidgetTypes.WIDGET_LEADERHEAD, tradePlayerI, self.iActiveLeader)
			screen.setActionButtonForceDisableShiftClickHeck(rowPrefix + "Name")
			if message is None:
				screen.newLabel(parent, rowPrefix + "WantsWill", makeTechListString(wantsList))
				screen.newLabel(parent, rowPrefix + "WantsCant", makeTechListString(wantsCantTradeList))
				for suffix in ("WantsWill", "WantsCant"):
					screen.setLabelEnableWrapping(rowPrefix + suffix)
			else:
				screen.newLabel(parent, rowPrefix + "Message", message)
			screen.newLabel(parent, rowPrefix + "CanResearch", makeTechListString(canResearchList))
			screen.newLabel(parent, rowPrefix + "Gold", str(maxGold))
			screen.newLabel(parent, rowPrefix + "WillTrade", makeTechListString(willTradeList))
			screen.newLabel(parent, rowPrefix + "WontTrade", makeTechListString(wontTradeList))
			screen.newLabel(parent, rowPrefix + "CantTrade", makeTechListString(cantTradeList))
			for suffix in ("CanResearch", "WillTrade", "WontTrade", "CantTrade"):
				screen.setLabelEnableWrapping(rowPrefix + suffix)

		table.rows = rows
		table.cells = cells
		screen.setTableLayout(parent, table)
	
	def buildGlancePage(self, screen, page):
		# List the players
		glancePlayerList = []

		for iLoopPlayer in range(gc.getMAX_PLAYERS()):
			if (gc.getPlayer(iLoopPlayer).isAlive() and gc.getTeam(gc.getPlayer(iLoopPlayer).getTeam()).isHasMet(gc.getPlayer(self.iActiveLeader).getTeam())
				and not gc.getPlayer(iLoopPlayer).isBarbarian() and not gc.getPlayer(iLoopPlayer).isMinorCiv()):
				glancePlayerList.append(iLoopPlayer)
				
		teamStr = localText.getText("TXT_KEY_PITBOSS_TEAM", ())
		vassalStr = localText.getText("TXT_KEY_MISC_VASSAL_SHORT", ())
		masterStr = localText.getText("TXT_KEY_MISC_MASTER", ())
		openBordersStr = u"%c" % (CyGame().getSymbolID(FontSymbols.OPEN_BORDERS_CHAR),)
		defensivePactStr = u"%c" % (CyGame().getSymbolID(FontSymbols.DEFENSIVE_PACT_CHAR),)
		if kHasBUG:
			warStr = FontUtil.getChar("war")
			peaceStr = FontUtil.getChar("peace")
		
		def makeGlanceString(fromI, toI):
			if fromI == toI:
				return ""

			fromPlayer = gc.getPlayer(fromI)
			toPlayer = gc.getPlayer(toI)
			fromTeamI = fromPlayer.getTeam()
			toTeamI = toPlayer.getTeam()
			fromTeam = gc.getTeam(fromTeamI)
			toTeam = gc.getTeam(toTeamI)
			
			pieces = []
			if fromTeamI == toTeamI:
				pieces.append(teamStr)
			if fromTeam.isVassal(toTeamI):
				pieces.append(vassalStr)
			if toTeam.isVassal(fromTeamI):
				pieces.append(masterStr)
			if fromTeam.isDefensivePact(toTeamI):
				pieces.append(defensivePactStr)
			if fromTeam.isOpenBorders(toTeamI):
				pieces.append(openBordersStr)
			if kHasBUG:
				if fromTeam.isAtWar(toTeamI):
					pieces.append(warStr)
				if fromTeam.isForcePeace(toTeamI):
					pieces.append(peaceStr)
				
			lines = []

			if kHasBUG and AttitudeUtil.getAttitudeCount(fromI, toI) != None:
				lines.append(AttitudeUtil.getAttitudeText(fromI, toI, AdvisorOpt.isShowGlanceNumbers(), AdvisorOpt.isShowGlanceSmilies(), True, False))

			if len(pieces):
				lines.append(u" ".join(pieces))

			return u'\n'.join(lines)

		parent = "GlanceTable"

		screen.newScrollBarPanel(page, parent + "ScrollPanel", parent, HeckTuiAxis.Vertical)
		screen.setFillLayout(page)
		
		# Not sure what check BUG uses if any... it seems you can get the "attitude" string of a human to an AI. So, exclude humans.
		aiPlayerList = [i for i in glancePlayerList if not gc.getPlayer(i).isHuman()]

		numCols = 1 + len(aiPlayerList)
		numRows = 1 + len(glancePlayerList)

		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 0)] + [TableLayoutConfigRowColDesc(0, 1)] * (numCols - 1)
		table.gap = ivec2(1, 0)
		table.rows = [TableLayoutConfigRowColDesc(0, 0)] * numRows
		table.cells = [TableLayoutConfigCell(ivec2(c, r), ivec2(1, 1), kCellVCenterAlign) for r in range(numRows) for c in range(numCols)]
		screen.setTableLayout(parent, table)

		screen.newLabel(parent, parent + "Header", u"")
		for playerI in aiPlayerList:
			player = gc.getPlayer(playerI)
			colourI = gc.getPlayerColorInfo(player.getPlayerColor()).getTextColorType()
			screen.newActionButton(parent, parent + "Header" + str(playerI), WidgetTypes.WIDGET_LEADERHEAD, playerI, self.iActiveLeader, HeckTuiBorderStyle.NONE)
			#screen.setActionButtonLabel(parent + "Header" + str(playerI), CyTranslator().changeTextColor(gc.getPlayer(playerI).getName()[0:4], colourI))
			screen.setActionButtonForceDisableShiftClickHeck(parent + "Header")
		
		for toPlayerI in glancePlayerList:
			rowPrefix = parent + "Row" + str(toPlayerI) + "_"
			screen.newActionButton(parent, rowPrefix + "Header", WidgetTypes.WIDGET_LEADERHEAD, toPlayerI, self.iActiveLeader)#, HeckTuiBorderStyle.NONE)
			screen.setActionButtonForceDisableShiftClickHeck(rowPrefix + "Header")
			for fromPlayerI in aiPlayerList:
				text = makeGlanceString(fromPlayerI, toPlayerI)
				name = rowPrefix + str(fromPlayerI)
				screen.newActionButton(parent, name, WidgetTypes.WIDGET_LEADERHEAD, fromPlayerI, toPlayerI, HeckTuiBorderStyle.NONE)
				screen.setActionButtonLabel(name, text)
				
		
	
	def buildInfoPage(self, screen, page):
		activePlayer = gc.getPlayer(self.iActiveLeader)
		
		parent = "InfoTable"

		screen.newScrollBarPanel(page, parent + "ScrollPanel", parent, HeckTuiAxis.Vertical)
		#screen.newPanel(page, parent)
		screen.setFillLayout(page)
		
		numCivicOpts = 5
		
		table = TableLayoutConfig()
		tableCols = [TableLayoutConfigRowColDesc(0, 0)]
		screen.newLabel(parent, parent + "HeaderL", localText.getText("TXT_KEY_FOREIGN_ADVISOR_ABBR_LEADER", ()))
		if kHasBUG:
			tableCols += [TableLayoutConfigRowColDesc(0, 2)]
			screen.newLabel(parent, parent + "HeaderA", localText.getText("TXT_KEY_FOREIGN_ADVISOR_ABBR_ATTITUDE", ()))
		tableCols += [TableLayoutConfigRowColDesc(0, 2)]
		screen.newLabel(parent, parent + "HeaderR", u"%c" % (CyGame().getSymbolID(FontSymbols.RELIGION_CHAR)))
		if kHasBUG:
			tableCols += [TableLayoutConfigRowColDesc(0, 1)] * 2
			screen.newLabel(parent, parent + "HeaderT",  u"%c" % (CyGame().getSymbolID(FontSymbols.TRADE_CHAR)))
			screen.newLabel(parent, parent + "HeaderTC",  u"%c%c" % (CyGame().getSymbolID(FontSymbols.TRADE_CHAR), gc.getYieldInfo(YieldTypes.YIELD_COMMERCE).getChar()))
		tableCols += [TableLayoutConfigRowColDesc(0, 1)] * (numCivicOpts + 1)
		screen.newLabel(parent, parent + "HeaderC0", localText.getText("TXT_KEY_CIVICOPTION_ABBR_GOVERNMENT", ()))
		screen.newLabel(parent, parent + "HeaderC1", localText.getText("TXT_KEY_CIVICOPTION_ABBR_LEGAL", ()))
		screen.newLabel(parent, parent + "HeaderC2", localText.getText("TXT_KEY_CIVICOPTION_ABBR_LABOR", ()))
		screen.newLabel(parent, parent + "HeaderC3", localText.getText("TXT_KEY_CIVICOPTION_ABBR_ECONOMY", ()))
		screen.newLabel(parent, parent + "HeaderC4", localText.getText("TXT_KEY_CIVICOPTION_ABBR_RELIGION", ()))
		
		if kHasBUG and FavoriteCivicDetector.isDetectionNecessary():
			fcHeaderText = localText.getText("TXT_KEY_FOREIGN_ADVISOR_POSSIBLE_FAV_CIVICS", ())
		else:
			fcHeaderText = localText.getText("TXT_KEY_PEDIA_FAV_CIVIC", ())
			
		screen.newLabel(parent, parent + "HeaderFC", fcHeaderText)
		
		numCols = len(tableCols)
		table.cols = tableCols
		table.gap = ivec2(1, 0)
		rows = [TableLayoutConfigRowColDesc(0, 0)]
		cells = [TableLayoutConfigCell(ivec2(i, 0)) for i in range(numCols)]
		
		rowI = 1
		
		# BUG code, mostly
		def buildInfoRow(iLoopPlayer):
			objLoopPlayer = gc.getPlayer(iLoopPlayer)
			iLoopTeam = objLoopPlayer.getTeam()
			objLoopTeam = gc.getTeam(iLoopTeam)
			bIsActivePlayer = (iLoopPlayer == self.iActiveLeader)

			if not (objLoopPlayer.isAlive()
				#and (self.objActiveTeam.isHasMet(iLoopTeam) or gc.getGame().isDebugMode())
				and not objLoopPlayer.isBarbarian()
				and not objLoopPlayer.isMinorCiv()):
				
				return False
				
			localRows = rows
			localRows += [TableLayoutConfigRowColDesc(0, 0)]
			
			rowPrefix = parent + "Row" + str(rowI)
				
			objLeaderHead = gc.getLeaderHeadInfo(objLoopPlayer.getLeaderType())
			if kHasBUG:
				objAttitude = AttitudeUtil.Attitude(iLoopPlayer, self.iActiveLeader)

			# Player panel

			# Panels always created but essentially blank if unmet
			if not self.objActiveTeam.isHasMet(iLoopTeam):
				screen.newLabel(parent, rowPrefix + "Name", u"Unknown")
			else:
				screen.newActionButton(parent, rowPrefix + "Name", WidgetTypes.WIDGET_LEADERHEAD, iLoopPlayer, self.iActiveLeader)
				screen.setActionButtonForceDisableShiftClickHeck(rowPrefix + "Name")

			localCells = cells
			localCells += [TableLayoutConfigCell(ivec2(i, rowI), ivec2(1, 1), kCellVCenterAlign) for i in range(numCols)]
			
			#screen.attachPanel(playerPanelName, infoPanelName, "", "", False, False, PanelStyles.PANEL_STYLE_EMPTY)

			# Attitude
			if kHasBUG:
				screen.newLabel(parent, rowPrefix + "Att", objAttitude.getText(True, True, False, False) if not bIsActivePlayer else u"")

			# Religion
			nReligion = objLoopPlayer.getStateReligion()
			if (nReligion != -1):
				objReligion = gc.getReligionInfo (nReligion)

				if (objLoopPlayer.hasHolyCity (nReligion)):
					szPlayerReligion = u"%c" %(objReligion.getHolyCityChar())
				elif objReligion:
					szPlayerReligion = u"%c" %(objReligion.getChar())

				if kHasBUG and not bIsActivePlayer:
					iDiploModifier = 0
					if (nReligion == self.objActiveLeader.getStateReligion()):
						iDiploModifier = objAttitude.getModifier("TXT_KEY_MISC_ATTITUDE_SAME_RELIGION")
					else:
						iDiploModifier = objAttitude.getModifier("TXT_KEY_MISC_ATTITUDE_DIFFERENT_RELIGION")
					if (iDiploModifier):
						if (iDiploModifier > 0):
							szColor = "COLOR_GREEN"
						else:
							szColor = "COLOR_RED"
						szPlayerReligion = localText.changeTextColor(szPlayerReligion + " [%+d]" % (iDiploModifier), gc.getInfoTypeForString(szColor))
				szPlayerReligion = "<font=2>" + szPlayerReligion + "</font>"
			else:
				szPlayerReligion = ""
		
			screen.newLabel(parent, rowPrefix + "Religion", szPlayerReligion)

			if kHasBUG:
				def calculateTrade (nPlayer, nTradePartner):
					# Trade status...
					iDomesticYield, iDomesticCount, iForeignYield, iForeignCount = TradeUtil.calculateTradeRoutes(nPlayer, nTradePartner)
					return iDomesticYield + iForeignYield, iDomesticCount + iForeignCount

				# Trade
				if (bIsActivePlayer or objLoopPlayer.canHaveTradeRoutesWith(self.iActiveLeader)):
					(iTradeCommerce, iTradeRoutes) = calculateTrade(self.iActiveLeader, iLoopPlayer)
					if TradeUtil.isFractionalTrade():
						iTradeCommerce //= 100
					szTradeYield = u"%d %c" % (iTradeCommerce, gc.getYieldInfo(YieldTypes.YIELD_COMMERCE).getChar())
					szTradeRoutes = u"%d" % (iTradeRoutes)
				else:
					szTradeYield = u"-"
					szTradeRoutes = u"-"

					
				screen.newLabel(parent, rowPrefix + "TradeRoutes", szTradeRoutes)
				screen.newLabel(parent, rowPrefix + "TradeGold", szTradeYield)
			
			# Civics
			for nCivicOption in range(gc.getNumCivicOptionInfos()):
				nCivic = objLoopPlayer.getCivics(nCivicOption)
				screen.newLabel(parent, rowPrefix + "TradeC" + str(nCivicOption), gc.getCivicInfo(nCivic).getDescription())

			# Favorite Civic
			
			text = ""
			
			if (not bIsActivePlayer):
				nFavoriteCivic = objLeaderHead.getFavoriteCivic()
				if kHasBUG and FavoriteCivicDetector.isDetectionNecessary():
					objFavorite = FavoriteCivicDetector.getFavoriteCivicInfo(iLoopPlayer)
					if objFavorite.isKnown():
						# We know it. Fall through to standard procedure after setting it.
						nFavoriteCivic = objFavorite.getFavorite()
					else:
						iNumPossibles = objFavorite.getNumPossibles()
						#BugUtil.debug("CvExoticForeignAdvisor::drawInfoRows() Number of Guesses: %d" %(iNumPossibles))
						if iNumPossibles > 5:
							# Too many possibilities; display question mark
							text = u"?"
						else:
							# Loop over possibles and display all
							text = u" or ".join(gc.getCivicInfo(nFavoriteCivic).getDescription() for nFavoriteCivic in objFavorite.getPossibles())
					
				if len(text) <= 0 and nFavoriteCivic != -1 and not gc.getGame().isOption(GameOptionTypes.GAMEOPTION_RANDOM_PERSONALITIES):
					objCivicInfo = gc.getCivicInfo(nFavoriteCivic)
					text = objCivicInfo.getDescription()
					if kHasBUG and self.objActiveLeader.isCivic (nFavoriteCivic):
						iDiploModifier = objAttitude.getModifier("TXT_KEY_MISC_ATTITUDE_FAVORITE_CIVIC")
						if (iDiploModifier):
							if (iDiploModifier > 0):
								szColor = "COLOR_GREEN"
							else:
								szColor = "COLOR_RED"
							text += " <font=2>" + localText.changeTextColor(" [%+d]" % (iDiploModifier), gc.getInfoTypeForString(szColor)) + "</font>"

			screen.newLabel(parent, rowPrefix + "FC", text)
			
			return True
		
		# display the active player's row at the top
		if buildInfoRow(self.iActiveLeader):
			rowI += 1
		# loop through all other players and add their rows; show known first
		lKnownPlayers = []
		lUnknownPlayers = []
		for iLoopPlayer in range(gc.getMAX_PLAYERS()):
			if iLoopPlayer != self.iActiveLeader:
				objLoopPlayer = gc.getPlayer(iLoopPlayer)
				if self.objActiveTeam.isHasMet(objLoopPlayer.getTeam()):
					lKnownPlayers.append(iLoopPlayer)
				else:
					lUnknownPlayers.append(iLoopPlayer)
		for iLoopPlayer in lKnownPlayers:
			if buildInfoRow(iLoopPlayer):
				rowI += 1
		for iLoopPlayer in lUnknownPlayers:
			if buildInfoRow(iLoopPlayer):
				rowI += 1
		
		table.rows = rows
		table.cells = cells
		screen.setTableLayout(parent, table)
	
	def handleInput (self, inputClass):
		if inputClass.eNotifyCode == NotifyCode.NOTIFY_CLICKED:
			if inputClass.iButtonType == WidgetTypes.WIDGET_LEADERHEAD:
				if inputClass.uiFlags & MouseFlags.MOUSE_RBUTTONUP:
					if inputClass.iData1 != self.iActiveLeader:
						self.getScreen().hideScreen()
					return 1
			if inputClass.iButtonType == WidgetTypes.WIDGET_PYTHON:
				if inputClass.uiFlags & MouseFlags.MOUSE_LBUTTONUP:
					self.iScreen = inputClass.iData2
					self.updatePageDisplay(self.getScreen())
					return 1
		return 0

	def update(self, fDelta):
		# TODO: Do we need updates?
		#if (CyInterface().isDirty(InterfaceDirtyBits.Foreign_Screen_DIRTY_BIT) == True):
		#	CyInterface().setDirty(InterfaceDirtyBits.Foreign_Screen_DIRTY_BIT, False)
		#	self.drawContents(False)
		return

