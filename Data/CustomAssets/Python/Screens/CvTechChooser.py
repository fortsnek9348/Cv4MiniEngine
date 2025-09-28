from CvPythonExtensions import *
import CvUtil
import ScreenInput
import CvScreenEnums
import CvScreensInterface

kHasBUG = False
try:
	import TechPrefs
	kHasBUG = True
except ImportError:
	pass

# globals
gc = CyGlobalContext()
localText = CyTranslator()

kTechWidgetWidth = 30
kTechWidgetHeight = 3
kTechLayoutRowGap = 2
kTechLayoutColGap = 5
kTechLayoutPerColX = kTechWidgetWidth + kTechLayoutColGap
kTechLayoutPerAltRowY = (kTechWidgetHeight + kTechLayoutRowGap) # integer div

kMaxPortRelY = kTechWidgetHeight / 2
kMinPortRelY = -kMaxPortRelY

# fortsnek: BUG needs to have these

if kHasBUG:
	# BUG - Tech Era Colors - start
	def getEraDescription(eWidgetType, iData1, iData2, bOption):
		return gc.getEraInfo(iData1).getDescription()
	# BUG - Tech Era Colors - end

	# BUG - GP Tech Prefs - start
	def resetTechPrefs(args=[]):
		CvScreensInterface.techChooser.resetTechPrefs()

	def getAllTechPrefsHover(widgetType, iData1, iData2, bOption):
		return buildTechPrefsHover("TXT_KEY_BUG_TECH_PREFS_ALL", CvScreensInterface.techChooser.pPrefs.getAllFlavorTechs(iData1))

	def getCurrentTechPrefsHover(widgetType, iData1, iData2, bOption):
		return buildTechPrefsHover("TXT_KEY_BUG_TECH_PREFS_CURRENT", CvScreensInterface.techChooser.pPrefs.getCurrentFlavorTechs(iData1))

	def getFutureTechPrefsHover(widgetType, iData1, iData2, bOption):
		pPlayer = gc.getPlayer(CvScreensInterface.techChooser.iCivSelected)
		sTechs = set()
		for i in range(gc.getNumTechInfos()):
			if (pPlayer.isResearchingTech(i)):
				sTechs.add(CvScreensInterface.techChooser.pPrefs.getTech(i))
		return buildTechPrefsHover("TXT_KEY_BUG_TECH_PREFS_FUTURE", CvScreensInterface.techChooser.pPrefs.getCurrentWithFlavorTechs(iData1, sTechs))

	def buildTechPrefsHover(key, lTechs):
		szText = BugUtil.getPlainText(key) + "\n"
		for pTech in lTechs:
			szText += "<img=%s size=24></img>" % pTech.getInfo().getButton().replace(" ", "_")
		return szText
	# BUG - GP Tech Prefs - end

class CvTechChooser:
	"Tech Chooser Screen"

	def __init__(self):
		# Tech widget states: CanResearch (None), Complete (0), Queued [1..]
		
		# Tech widget graph layout:
		# Rows are overlapping.
		# Alternating rows cannot be compact. There must be a gap for connections.
		# If you had even widget height, rows can't evenly overlap:
		#        0
		#        0
		#        0 1
		#        0 1
		#        - 1
		#        2 1
		#        2
		#        2
		#        2
		# Odd widget height works:
		#        0
		#        0
		#        0 1
		#        - 1
		#        2 1
		#        2
		#        2
		# The layout algorithm is no layout algorithm. Tech widgets will be placed explicitly. Knowing positions beforehand will be useful for connections.
		#
		# Connections are strictly cardinal, left-to-right, and if a the target is at a different hight, the connection bends in the middle only.
		# Techs are placed so that nothing overlaps.
		# Each tech widget has three IN and OUT "ports". One for each line. Use some height threshold to choose which ones to use.
		
		# Tech widgets are three-line bordered. Just one text line. The name.
		
		# --------------------------------------------
		# | [3] Industrialism (6) (req Electricity)  |
		# --------------------------------------------
		pass
		
		
		
	def getScreen(self):
		return CyGInterfaceScreen("TechChooser", CvScreenEnums.TECH_CHOOSER)

	def hideScreen(self):
		self.getScreen().hideScreen()
		
	
	# Screen construction function
	def interfaceScreen(self):
		self.iCivSelected = gc.getGame().getActivePlayer()
		if CyGame().isPitbossHost():
			return

		screen = self.getScreen()
		#screen.setRenderInterfaceOnly(True)
		
		screen.showScreen(PopupStates.POPUPSTATE_IMMEDIATE, False)
		
		# Keep the original scroll position.
		if screen.isPersistent():
			self.updateTechRecords(False)
			return
		screen.setPersistent(True)
		
		screen.setInitialTitle(localText.getText("TXT_KEY_TECH_CHOOSER_TITLE", ()).upper())

		
		#screen.setText( "TechChooserExit", "Background", u"<font=4>" + CyTranslator().getText("TXT_KEY_PEDIA_SCREEN_EXIT", ()).upper() + "</font>", CvUtil.FONT_RIGHT_JUSTIFY, 994, 726, 0, FontTypes.TITLE_FONT, WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1 )

		root = ""

		table = TableLayoutConfig()
		table.cols = [
			TableLayoutConfigRowColDesc(0, 1),
		]
		table.rows = [
			TableLayoutConfigRowColDesc(0, 1),
			TableLayoutConfigRowColDesc(0, 0),
		]
		table.cells = [
			TableLayoutConfigCell(ivec2(0, 0)),
			TableLayoutConfigCell(ivec2(0, 1), ivec2(1, 1), RectJustilign(EJustilign.End, EJustilign.Center)),
		]
		screen.setTableLayout(root, table)

		screen.newScrollBarPanel(root, "TechScrollPanel", "TechPanel", HeckTuiAxis.Horizontal)
		#screen.setHFlowLayout("TechPanel")
		
		screen.newActionButton(root, "ExitButton", WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)
		screen.setActionButtonLabel("ExitButton", CyTranslator().getText("TXT_KEY_PEDIA_SCREEN_EXIT", ()).upper())
		
		techRects = [None] * gc.getNumTechInfos()
		
		techLabelColour = gc.getInfoTypeForString('COLOR_BLACK')
		
		columnWidths = [0] * max(gc.getTechInfo(techI).getGridX() for techI in range(gc.getNumTechInfos()))
		
		for techI in range(gc.getNumTechInfos()):
			tech = gc.getTechInfo(techI)
			gridX = tech.getGridX() - 1
			columnWidths[gridX] = max(columnWidths[gridX], len(CvTechChooser.buildTechLabel(tech, None, None)) + len("|99.  (9999)|"))
			
		columnPositions = [0] * (len(columnWidths) + 1)
		for i, w in enumerate(columnWidths):
			w += kTechLayoutColGap
			columnPositions[i + 1] = columnPositions[i] + w

		for techI in range(gc.getNumTechInfos()):
			tech = gc.getTechInfo(techI)
			
			#label = gc.getTechInfo(techI).getDescription()
			#
			#labelledPreReqs = [gc.getTechInfo(tech.getPrereqAndTechs(j)).getDescription() for j in range(gc.getNUM_AND_TECH_PREREQS()) if tech.getPrereqAndTechs(j) > -1]
			#if len(labelledPreReqs):
			#	label +=  u" (req "
			#	label += u", ".join(labelledPreReqs)
			#	label += u')'
			#	
			#label = CyTranslator().changeTextColor(label, techLabelColour)
			
			name = "TechBtn" + str(techI)
			screen.newActionButton("TechPanel", name, WidgetTypes.WIDGET_TECH_TREE, techI, -1)
			#screen.setActionButtonLabel(name, label)
			screen.setActionButtonHAlign(name, EAlign.Left)
			
			gridX = tech.getGridX() - 1
			gridY = tech.getGridY() - 1
			#tuiX = gridX * kTechLayoutPerColX
			tuiX = columnPositions[gridX]
			tuiY = (gridY * kTechLayoutPerAltRowY) / 2 # do division here as kTechLayoutPerAltRowY may be odd.
			
			width = columnWidths[gridX]
			
			rect = iaabb2.sized(ivec2(tuiX, tuiY), ivec2(width, kTechWidgetHeight))
			
			techRects[techI] = rect
			
			screen.setControlRect(name, rect)
			
			
			
		screen.newCanvas("TechPanel", "TechGraph")
		screen.setControlRect("TechGraph", iaabb2.sized(ivec2(0, 0), ivec2(max(r.maximum.x for r in techRects), max(r.maximum.y for r in techRects) + 1)))
		for techI in range(gc.getNumTechInfos()):
			tech = gc.getTechInfo(techI)
			targetPos = techRects[techI].position
			targetPos = ivec2(targetPos.x - 1, targetPos.y + kTechWidgetHeight / 2)
			#if tech.getDescription() == "Bronze Working":
			for j in range(gc.getNUM_OR_TECH_PREREQS()):
				eTech = tech.getPrereqOrTechs(j)
				if eTech > -1:
				#if eTech > -1 and gc.getTechInfo(eTech).getDescription() == "Mining":
					sourcePos = techRects[eTech].maximum
					sourcePos = ivec2(sourcePos.x, sourcePos.y - (kTechWidgetHeight - kTechWidgetHeight / 2))
					
					gridYDiff = gc.getTechInfo(eTech).getGridY() - tech.getGridY()
					delta = min(max(gridYDiff / 2, kMinPortRelY), kMaxPortRelY)
					sourcePos.y -= delta
					
					screen.drawOrthoGraphEdge("TechGraph", sourcePos, ivec2(targetPos.x, targetPos.y + delta))
					
		# NextWar has some lines overlapping tech buttons. But the graph behind them.
		screen.moveToFirst("TechGraph")
					
		self.updateTechRecords(True)
		
		
		
	@staticmethod
	def buildTechLabel(tech, queuePosition, accTurns):
		label = tech.getDescription()
		
		labelledPreReqs = [gc.getTechInfo(tech.getPrereqAndTechs(j)).getDescription() for j in range(gc.getNUM_AND_TECH_PREREQS()) if tech.getPrereqAndTechs(j) > -1]
		if len(labelledPreReqs):
			label +=  u" (req "
			label += u", ".join(labelledPreReqs)
			label += u')'
			
		if queuePosition is not None:
			label = u"%d. %s (%d)" % (queuePosition, label, accTurns)
			
		return label
		
	# This is called from CvScreenInterface.py. Same purpose as updateScreen of other screens.
	# Update the state of techs.
	def updateTechRecords(self, bForce):
		if CyGame().isPitbossHost():
			return
			
		screen = self.getScreen()
			
		(kState_Has, kState_Active, kState_Cant, kState_Avail) = range(4)
		
		kColours = [
			gc.getInfoTypeForString('COLOR_TECH_GREEN'),
			gc.getInfoTypeForString('COLOR_CITY_BLUE'),
			gc.getInfoTypeForString('COLOR_TECH_RED'),
			gc.getInfoTypeForString('COLOR_LIGHT_GREY'),
		]
		
		kLabelColour = gc.getInfoTypeForString('COLOR_BLACK')
		kActiveLabelColour = gc.getInfoTypeForString('COLOR_WHITE')

		activePlayerI = gc.getGame().getActivePlayer()
		activePlayer = gc.getPlayer(activePlayerI)
		
		maxPosition = 0
		researchQueueLookup = dict()
		for techI in range(gc.getNumTechInfos()):
			if gc.getTeam(activePlayerI).isHasTech(techI):
				state = kState_Has
			elif activePlayer.getCurrentResearch() == techI or activePlayer.isResearchingTech(techI):
				state = kState_Active
			else:
				state = kState_Cant

			if state == kState_Active:
				pos = activePlayer.getQueuePosition(techI)
				maxPosition = max(maxPosition, pos)
				researchQueueLookup[techI] = pos

		researchQueueList = [(-1, 0)] * (maxPosition + 1)
		for techI, pos in researchQueueLookup.items():
			researchQueueList[pos] = (techI, activePlayer.getResearchTurnsLeft(techI, activePlayer.getCurrentResearch() == techI))

		for techI in range(gc.getNumTechInfos()):
			if gc.getTeam(activePlayerI).isHasTech(techI):
				state = kState_Has
			elif activePlayer.getCurrentResearch() == techI or activePlayer.isResearchingTech(techI):
				state = kState_Active
			elif activePlayer.canEverResearch(techI):
				state = kState_Avail
			else:
				state = kState_Cant

			tech = gc.getTechInfo(techI)

			queuePosition = researchQueueLookup.get(techI, None)

			label = CvTechChooser.buildTechLabel(tech, queuePosition, sum(turns for queueTechI, turns in researchQueueList[:queuePosition+1]) if queuePosition is not None else 0)
			
			label = CyTranslator().changeTextColor(label, kActiveLabelColour if state == kState_Active else kLabelColour)
			
			name = "TechBtn" + str(techI)
			#screen.setActionButtonLabel(name, CyTranslator().changeTextColor(label, kColours[state]))
			#screen.setActionButtonLabel(name, CyTranslator().changeTextColor(label, gc.getInfoTypeForString('COLOR_BLACK')))
			screen.setActionButtonLabel(name, label)
			screen.setActionButtonBackground(name, HeckTuiColour.fromColourType(kColours[state]))
			
			
		return

	# Will handle the input for this screen...
	def handleInput(self, inputClass):
		return 0
		
	def update(self, dt):
		return 0
	
	# No advanced start support in Cv4MiniEngine, but this is what's needed here.
	#def onClose(self):
	#	pPlayer = gc.getPlayer(self.iCivSelected)
	#	if (pPlayer.getAdvancedStartPoints() >= 0):
	#		CyInterface().setDirty(InterfaceDirtyBits.Advanced_Start_DIRTY_BIT, true)
	#	return 0
	
	# BUG - GP Tech Prefs - start
	def resetTechPrefs(self):
		if kHasBUG:
			self.pPrefs = TechPrefs.TechPrefs()
