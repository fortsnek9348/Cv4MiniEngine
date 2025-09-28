#fortsnek: Based on BUG military advisor. Deploy tab only.
##
## Copyright (c) 2008 The BUG Mod.
##
## Author: Ruff_Hi (Situation Report tab)
##         EmperorFool (Deployment and Strategic Advantages tabs)

from CvPythonExtensions import *
import CvUtil
import PyHelpers
import re
import sys

kHasBUG = False
try:
	import BugUtil
except ImportError:
	pass
	
if kHasBUG:
	import PlayerUtil
	import UnitGrouper
	# BUG - Mac Support - start
	BugUtil.fixSets(globals())
	# BUG - Mac Support - end
else:
	from itertools import groupby
	class PlayerUtil:
		def _players(alive):
			return [gc.getPlayer(i) for i in range(gc.getMAX_PLAYERS()) if gc.getPlayer(i).isAlive() == alive]
		players = staticmethod(_players)
		def _playerUnits(player):
			unit, it = player.firstUnit(False)
			while unit:
				if not unit.isDead():
					yield unit
				unit, it = player.nextUnit(it, False)
		playerUnits = staticmethod(_playerUnits)

PyPlayer = PyHelpers.PyPlayer

# globals
gc = CyGlobalContext()
localText = CyTranslator()

# For debugging
kAllVisible = False

class CvMilitaryAdvisor:
	"Shows the BUG Version of the Military Advisor"

	def __init__(self, screenId):
		self.screenId = screenId

		self.iPlayerPower = 0
		self.iDemographicsMission = -1

				
		self.nWidgetCount = 0
		self.iActivePlayer = -1

		self.minimapInitialized = False
		
		# This gets you the contents of the unit tree and the grouping selection.
		if kHasBUG:
			self.grouper = UnitGrouper.StandardGrouper()

						
	def getScreen(self):
		return CyGInterfaceScreen("MilitaryScreen", self.screenId)

	def hideScreen(self):
		screen = self.getScreen()
		screen.hideScreen()
										
	def interfaceScreen(self):
		# Create a new screen
		screen = self.getScreen()
		if screen.isActive():
			return

		#screen.setRenderInterfaceOnly(True);

		self.iActivePlayer = CyGame().getActivePlayer()
		
		self.selectedLeaders = { self.iActivePlayer }
		self.unitSelectionSet = set()
		self.unitList = list()
		
		# Player List | [First Level Grouping |v] ||
		#	          | [Second Level Grouping|v] ||
		#	          | [X] Show individual units ||
		#             |---------------------------||
		#             |                           ||        MINIMAP
		#             |         UNIT TREE         ||        
		#             |                           ||
		#             |                           ||
		#
		#   [                     Combat Experience                     ]    EXIT
		
		#
		# Player List
		#
		# [First Level Grouping |v]  |
		# [Second Level Grouping|v]  |
		# [X] Show individual units  |
		# ---------------------------|
		#                            |        MINIMAP
		#          UNIT TREE         |        
		#                            |
		#                            |
		#
		# [                     Combat Experience                     ]    EXIT
		
		kVerticalPlayerList = True

		screen.setInitialTitle(localText.getText("TXT_KEY_MILITARY_ADVISOR_TITLE", ()).upper())
		screen.setAutoSizeBehaviour(HeckTuiAutoSizeBehaviour.Maximise)
		
		root = ""

		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1)]
		table.rows = ([] if kVerticalPlayerList else [TableLayoutConfigRowColDesc(0, 0)]) + [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)), # PlayerList
			TableLayoutConfigCell(ivec2(0, 1)), # Content
			TableLayoutConfigCell(ivec2(0, 2)), # Footer
		] if not kVerticalPlayerList else [
			TableLayoutConfigCell(ivec2(0, 0)), # Content
			TableLayoutConfigCell(ivec2(0, 1)), # Footer
		]
		table.gap = ivec2(1, 1)
		screen.setTableLayout(root, table)
		
		if not kVerticalPlayerList:
			screen.newScrollBarPanel(root, "PlayerListScrollPanel", "PlayerListPanel", HeckTuiAxis.Horizontal)
		screen.newResizableVSplit(root, "ContentSplit", 60 if kVerticalPlayerList else 40, False)
		screen.newPanel(root, "Footer")
		
		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 0)] + ([TableLayoutConfigRowColDesc(0, 1)] if kVerticalPlayerList else [])
		table.rows = [TableLayoutConfigRowColDesc(0, 0), TableLayoutConfigRowColDesc(0, 1)]
		if kVerticalPlayerList:
			table.cells = [
				TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 2)), # PlayerListPanel
				TableLayoutConfigCell(ivec2(1, 0)), # DisplayOptionsPanel
				TableLayoutConfigCell(ivec2(1, 1)), # UnitTreePanel
			]
		else:
			table.cells = [
				TableLayoutConfigCell(ivec2(0, 0)), # DisplayOptionsPanel
				TableLayoutConfigCell(ivec2(0, 1)), # UnitTreePanel
			]
		table.gap = ivec2(1, 1)
		screen.newPanel("ContentSplit", "LeftPanel")
		screen.setTableLayout("LeftPanel", table)
		
		if kHasBUG:
			groupingTitles = [grouping.title for grouping in self.grouper]
		
		if kVerticalPlayerList:
			screen.newScrollBarPanel("LeftPanel", "PlayerListScrollPanel", "PlayerListPanel", HeckTuiAxis.Vertical)
			screen.setVFlowLayout("PlayerListPanel")
		else:
			screen.setHFlowLayout("PlayerListPanel")
		
		screen.newPanel("LeftPanel", "DisplayOptionsPanel")
		screen.newScrollBarPanel("LeftPanel", "UnitTreePanel", "UnitTree", HeckTuiAxis.Vertical)
		screen.newMinimap("ContentSplit", "Minimap")
		
		screen.setVFlowLayout("UnitTree")
		
		for iLoopPlayer in range(gc.getMAX_PLAYERS()):
			player = gc.getPlayer(iLoopPlayer)
			if player.isAlive() and (kAllVisible or gc.getTeam(player.getTeam()).isHasMet(gc.getPlayer(self.iActivePlayer).getTeam())):
				screen.newActionButton("PlayerListPanel", "Player" + str(iLoopPlayer), WidgetTypes.WIDGET_PYTHON, 0, iLoopPlayer)
				screen.setActionButtonHAlign("Player" + str(iLoopPlayer), EAlign.Left)
				self.updateButtonLabel(screen, iLoopPlayer)
				
		screen.setVFlowLayout("DisplayOptionsPanel")
		if kHasBUG:
			screen.newCombobox("DisplayOptionsPanel", "FirstGroupingCombobox", groupingTitles, 4, WidgetTypes.WIDGET_PYTHON, -1, -1)
			screen.newCombobox("DisplayOptionsPanel", "SecondGroupingCombobox", groupingTitles, 0, WidgetTypes.WIDGET_PYTHON, -1, -1)
		screen.newActionCheckBox("DisplayOptionsPanel", "ShowIndividualUnits", WidgetTypes.WIDGET_PYTHON, -1)	
		#screen.newHRule("DisplayOptionsPanel", "DisplayOptionsPanelHRule")
		screen.setCheckBoxLabel("ShowIndividualUnits", localText.getText("TXT_KEY_MILITARY_ADVISOR_UNIT_TOGGLE_ON", ()))

		table = TableLayoutConfig()
		table.cols = [TableLayoutConfigRowColDesc(0, 1), TableLayoutConfigRowColDesc(0, 0)]
		table.rows = [TableLayoutConfigRowColDesc(0, 0)]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0), ivec2(1, 1), RectJustilign(EJustilign.Stretch, EJustilign.Center)), # GreatGeneralGuage
			TableLayoutConfigCell(ivec2(1, 0)), # ExitButton
		]
		table.gap = ivec2(3, 1)
		screen.setTableLayout("Footer", table)
		
		screen.newGuage("Footer", "GameHeaderPanel_GreatGeneralBar", [gc.getInfoTypeForString("COLOR_NEGATIVE_RATE")], WidgetTypes.WIDGET_HELP_GREAT_GENERAL)
		
		player = gc.getPlayer(self.iActivePlayer)
		iCombatExp = player.getCombatExperience()
		iThresholdExp = player.greatPeopleThreshold(True)
		iNeededExp = iThresholdExp - iCombatExp
		text = localText.getText("TXT_KEY_MISC_GREAT_GENERAL", (iCombatExp, iThresholdExp))
		# GGUtil.getGreatGeneralText(iNeededExp)
		screen.setGuageValues("GameHeaderPanel_GreatGeneralBar", text, iThresholdExp, [iCombatExp, iNeededExp])
		
		screen.newActionButton("Footer", "ExitButton", WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)
		screen.setActionButtonLabel("ExitButton", localText.getText("TXT_KEY_PEDIA_SCREEN_EXIT", ()).upper())
		
		self.rebuildUnitList()
		
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)
		
	def updateButtonLabel(self, screen, iLoopPlayer):
		player = gc.getPlayer(iLoopPlayer)
		if player.isAlive() and (kAllVisible or gc.getTeam(player.getTeam()).isHasMet(gc.getPlayer(self.iActivePlayer).getTeam())):
			name = (u"[\u25cf] " if iLoopPlayer in self.selectedLeaders else u"[ ] ") + CyTranslator().changeTextColor(player.getName(), gc.getPlayerColorInfo(player.getPlayerColor()).getTextColorType())
			screen.setActionButtonLabel("Player" + str(iLoopPlayer), name)

	# TODO: UI is fully rebuilt when just selecting groups/units. Would be faster if the UI was only updated.
	def rebuildUnitList(self):
		screen = self.getScreen()
		
		prefix = "UnitTreeItem[]"
		screen.delByPrefix(prefix)
		
		self.unitKeyList = list()
		self.unitGroupingList = list()

		if kHasBUG:
			_, activePlayer, iActiveTeam, activeTeam = PlayerUtil.getActivePlayerAndTeamAndIDs()
			stats = UnitGrouper.GrouperStats(self.grouper)
		else:
			activePlayer = gc.getActivePlayer()
			iActiveTeam = gc.getGame().getActiveTeam()
			activeTeam = gc.getTeam(iActiveTeam)
		
		allUnits = list()
		
		for player in PlayerUtil.players(alive=True):
			for unit in PlayerUtil.playerUnits(player):
				plot = unit.plot()
				if plot.isNone():
					continue
				if not kAllVisible and (not plot.isVisible(iActiveTeam, False) or unit.isInvisible(iActiveTeam, False)):
					continue
				if unit.getVisualOwner() in self.selectedLeaders:
					allUnits.append(unit)
					if kHasBUG:
						stats.processUnit(activePlayer, activeTeam, unit)
		
		screen.clearMinimapMarkers("Minimap")
		
		isShowingIndividualUnits = screen.getCheckBoxValue("ShowIndividualUnits")
		
		reReplaceImg = re.compile("<img=.+></img>")
		
		indent = u"   "
		
		def emitGroupButton(indentLevel, title, units):
			bGroupSelected = len(self.unitSelectionSet) >= len(units) and { (loopUnit.getOwner(), loopUnit.getID()) for loopUnit in units }.issubset(self.unitSelectionSet)
			szDescription = indent * indentLevel + ("[ ]", "[X]")[bGroupSelected] + ' ' + title + u" (%d)" % len(units)
			groupI = len(self.unitGroupingList)
			name = prefix + "Group" + str(groupI)
			screen.newActionButton("UnitTree", name, WidgetTypes.WIDGET_PYTHON, 1, groupI, HeckTuiBorderStyle.NONE)
			screen.setActionButtonLabel(name, szDescription)
			screen.setActionButtonHAlign(name, EAlign.Left)
			self.unitGroupingList.append((len(self.unitKeyList), len(units), bGroupSelected))
			
		emitGroupButton(0, localText.getText("TXT_KEY_PEDIA_ALL_UNITS", ()).upper(), allUnits)
		
		if kHasBUG:
			groupingA = stats.getGrouping(self.grouper[screen.getComboboxSelectedIndex("FirstGroupingCombobox")].key).itergroups()
		else:
			class UnitHolder:
				__slots__ = ['unit']
				def __init__(self, unit):
					self.unit = unit
			class GroupDesc:
				__slots__ = ['title']
				def __init__(self, title):
					self.title = title
			class EvaluatedGroup:
				__slots__ = ['group', 'units']
				def __init__(self, group, units):
					self.group = group
					self.units = units
			getUnitKey = lambda unit: gc.getUnitInfo(unit.getUnitType()).getUnitCombatType()
			allUnits.sort(key=getUnitKey)
			noneText = localText.getText("TXT_KEY_NONE", ())
			groupingA = [EvaluatedGroup(GroupDesc(gc.getUnitCombatInfo(key).getDescription() if key >= 0 else noneText), [UnitHolder(unit) for unit in group])  for key, group in groupby(allUnits, key=getUnitKey)]
				
		
		ctrlId = 0
		for groupA in groupingA:
			unitsA = [unitObj.unit for unitObj in groupA.units]
			if len(unitsA) <= 0:
				continue
				
			emitGroupButton(1, groupA.group.title, unitsA)
			
			if kHasBUG:
				statsB = UnitGrouper.GrouperStats(self.grouper)
				for unit in unitsA:
					statsB.processUnit(activePlayer, activeTeam, unit)
				groupingB = statsB.getGrouping(self.grouper[screen.getComboboxSelectedIndex("SecondGroupingCombobox")].key).itergroups()
			else:
				getUnitKey = lambda unit: unit.getUnitType()
				unitsA.sort(key=getUnitKey)
				groupingB = [EvaluatedGroup(GroupDesc(gc.getUnitInfo(key).getDescription()), [UnitHolder(unit) for unit in group])  for key, group in groupby(unitsA, key=getUnitKey)]
			
			for groupB in groupingB:
				unitsB = [unitObj.unit for unitObj in groupB.units]
				if len(unitsB) <= 0:
					continue
					
				emitGroupButton(2, groupB.group.title, unitsB)
				
				for loopUnit in unitsB:
					unitKey = (loopUnit.getOwner(), loopUnit.getID())
					bUnitSelected = unitKey in self.unitSelectionSet
					if isShowingIndividualUnits:
						szDescription = CyGameTextMgr().getSpecificUnitHelp(loopUnit, True, False)

						# Get rid of the promotions (they're just <img> placeholders at the moment in the output)
						szDescription = reReplaceImg.sub("", szDescription)

						if loopUnit.isWaiting():
							szDescription = '*' + szDescription
						
						if bUnitSelected:
							szDescription = indent * 3 + u"[X] " + szDescription
						else:
							szDescription = indent * 3 + u"[ ] " + szDescription

						ctrlId += 1
						name = prefix + str(ctrlId)

						screen.newActionButton("UnitTree", name, WidgetTypes.WIDGET_PYTHON, 3, len(self.unitKeyList), HeckTuiBorderStyle.NONE)
						screen.setActionButtonLabel(name, u"  " + szDescription)
						screen.setActionButtonHAlign(name, EAlign.Left)
						
					self.unitKeyList.append(unitKey)
					if bUnitSelected:
						iPlayer = loopUnit.getVisualOwner()
						#iColor = gc.getPlayerColorInfo(gc.getPlayer(iPlayer).getPlayerColor()).getColorTypePrimary()
						iColor = gc.getPlayerColorInfo(gc.getPlayer(iPlayer).getPlayerColor()).getTextColorType()
						screen.addMinimapMarker("Minimap", loopUnit.getX(), loopUnit.getY(), iColor, 'X')

		self.unitSelectionSet = self.unitSelectionSet & set(self.unitKeyList)

	# handle the input for this screen...
	def handleInput(self, inputClass):
		if inputClass.getNotifyCode() == NotifyCode.NOTIFY_CLICKED and inputClass.getFlags() & MouseFlags.MOUSE_LBUTTONUP:
			if inputClass.getButtonType() == WidgetTypes.WIDGET_PYTHON:
				if inputClass.getData1() < 0: # group options
					self.rebuildUnitList()
					return 1
				elif inputClass.getData1() == 0: # player sel
					playerI = inputClass.getData2()
					if inputClass.bShift:
						if playerI in self.selectedLeaders:
							self.selectedLeaders.remove(playerI)
						else:
							self.selectedLeaders.add(playerI)
					else:
						self.selectedLeaders = { playerI }
					screen = self.getScreen()
					for iLoopPlayer in range(gc.getMAX_PLAYERS()):
						self.updateButtonLabel(screen, iLoopPlayer)
						self.rebuildUnitList()
					return 1
				elif inputClass.getData1() == 1: # group sel
					firstUnitI, unitCount, bGroupSelected = self.unitGroupingList[inputClass.getData2()]
					groupUnitList = self.unitKeyList[firstUnitI:firstUnitI+unitCount]
					if bGroupSelected:
						self.unitSelectionSet.difference_update(groupUnitList)
					else:
						self.unitSelectionSet.update(groupUnitList)
					self.rebuildUnitList()
					return 1
				elif inputClass.getData1() == 3: # unit sel
					unitKey = self.unitKeyList[inputClass.getData2()]
					if unitKey in self.unitSelectionSet:
						self.unitSelectionSet.remove(unitKey)
					else:
						self.unitSelectionSet.add(unitKey)
					self.rebuildUnitList()
					return 1
		if inputClass.getNotifyCode() == NotifyCode.NOTIFY_LISTBOX_ITEM_SELECTED:
			if inputClass.getButtonType() == WidgetTypes.WIDGET_PYTHON:
				self.rebuildUnitList()
				return 1
		return 0
		
	# Called from CvScreensInterface.
	def minimapClicked(self):
		self.hideScreen()


	def update(self, fDelta):
		return
