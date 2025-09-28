## Sid Meier's Civilization 4
## Copyright Firaxis Games 2005
# Author - Jon Shafer
# Top Civilizations screen

# fortsnek: Adapted for Cv4MiniEngine.

import PyHelpers
import CvUtil
import CvScreenEnums
import random
from CvPythonExtensions import *

PyPlayer = PyHelpers.PyPlayer
gc = CyGlobalContext()
localText = CyTranslator()

NUM_CIVILIZATIONS = 8

class CvTopCivs:
	"The Greatest Civilizations screen"
	
	def __init__(self):
		pass

	def showScreen(self):
			  
		'Use a popup to display the opening text'
		if ( CyGame().isPitbossHost() ):
			return

		# Text
		self.TITLE_TEXT = localText.getText("TXT_KEY_TOPCIVS_TITLE", ()).upper()
		#self.EXIT_TEXT = localText.getText("TXT_KEY_PEDIA_SCREEN_EXIT", ()).upper()
		
		self.HistorianList = [	localText.getText("TXT_KEY_TOPCIVS_HISTORIAN1", ()),
					localText.getText("TXT_KEY_TOPCIVS_HISTORIAN2", ()),
					localText.getText("TXT_KEY_TOPCIVS_HISTORIAN3", ()),
					localText.getText("TXT_KEY_TOPCIVS_HISTORIAN4", ()),
					localText.getText("TXT_KEY_TOPCIVS_HISTORIAN5", ()),
					localText.getText("TXT_KEY_TOPCIVS_HISTORIAN6", ()),
					localText.getText("TXT_KEY_TOPCIVS_HISTORIAN7", ()),
					localText.getText("TXT_KEY_TOPCIVS_HISTORIAN8", ()),
					localText.getText("TXT_KEY_TOPCIVS_HISTORIAN9", ()),
					localText.getText("TXT_KEY_TOPCIVS_HISTORIAN10", ()),
					localText.getText("TXT_KEY_TOPCIVS_HISTORIAN11", ())
				    ]
					
		self.RankList =     [	localText.getText("TXT_KEY_TOPCIVS_RANK1", ()),
					localText.getText("TXT_KEY_TOPCIVS_RANK2", ()),
					localText.getText("TXT_KEY_TOPCIVS_RANK3", ()),
					localText.getText("TXT_KEY_TOPCIVS_RANK4", ()),
					localText.getText("TXT_KEY_TOPCIVS_RANK5", ()),
					localText.getText("TXT_KEY_TOPCIVS_RANK6", ()),
					localText.getText("TXT_KEY_TOPCIVS_RANK7", ()),
					localText.getText("TXT_KEY_TOPCIVS_RANK8", ())
				    ]

		self.TypeList =    [	localText.getText("TXT_KEY_TOPCIVS_WEALTH", ()),
					localText.getText("TXT_KEY_TOPCIVS_POWER", ()),
					localText.getText("TXT_KEY_TOPCIVS_TECH", ()),
					localText.getText("TXT_KEY_TOPCIVS_CULTURE", ()),
					localText.getText("TXT_KEY_TOPCIVS_SIZE", ()),
				    ]

		# Randomly choose what category and what historian will be used
		szTypeRand = random.choice(self.TypeList)
		szHistorianRand = random.choice(self.HistorianList)
		
		# Create screen
		
		self.screen = CyGInterfaceScreen( "CvTopCivs", CvScreenEnums.TOP_CIVS )

		root = ""
		self.screen.setVFlowLayout(root)
		
		# Create panels

		# Top
		szHeaderText = ""
		self.screen.newLabel(root, "HeaderLabel", szHeaderText);
		
		# Title Text
		self.screen.setInitialTitle(self.TITLE_TEXT)
		
		# 1 Text
		szText = localText.getText("TXT_KEY_TOPCIVS_TEXT1", (szHistorianRand, )) + u"\n" + localText.getText("TXT_KEY_TOPCIVS_TEXT2", (szTypeRand, ))
		self.screen.newLabel(root, "DescLabel", szText);
		self.screen.setLabelHAlign("DescLabel", EAlign.Center)
		
		self.makeList(szTypeRand)
		
		self.screen.newActionButton(root, "ExitButton", WidgetTypes.WIDGET_CLOSE_SCREEN, -1, -1)
		
		self.screen.setSound("AS2D_TOP_CIVS")
		self.screen.showScreen(PopupStates.POPUPSTATE_QUEUED, False)
		#self.screen.showWindowBackground( False )
		#self.screen.setDimensions(self.screen.centerX(self.X_SCREEN), self.screen.centerY(self.Y_SCREEN), self.W_SCREEN, self.H_SCREEN)
		
	def makeList(self, szType):

		# Determine the list of top civs
		
		# Will eventually Store [iValue, iPlayerID]
		self.aiTopCivsValues = []
		
		# Loop through all players except the barbs
		for iPlayerLoop in range(gc.getMAX_PLAYERS()-1):
				
			if (gc.getPlayer(iPlayerLoop).isAlive()):
				
				if (szType == localText.getText("TXT_KEY_TOPCIVS_WEALTH", ())):

					self.aiTopCivsValues.append([gc.getPlayer(iPlayerLoop).getGold(), iPlayerLoop])
					print("Player %d Num Gold: %d" %(iPlayerLoop, gc.getPlayer(iPlayerLoop).getGold()))
					
				if (szType == localText.getText("TXT_KEY_TOPCIVS_POWER", ())):

					self.aiTopCivsValues.append([gc.getPlayer(iPlayerLoop).getPower(), iPlayerLoop])

				if (szType == localText.getText("TXT_KEY_TOPCIVS_TECH", ())):

					iPlayerNumTechs = 0
					iNumTotalTechs = gc.getNumTechInfos()

					for iTechLoop in range(iNumTotalTechs):

						bPlayerHasTech = gc.getTeam(gc.getPlayer(iPlayerLoop).getTeam()).isHasTech(iTechLoop)

						if (bPlayerHasTech):
							iPlayerNumTechs = iPlayerNumTechs + 1
							
					self.aiTopCivsValues.append([iPlayerNumTechs, iPlayerLoop])

				if (szType == localText.getText("TXT_KEY_TOPCIVS_CULTURE", ())):

					self.aiTopCivsValues.append([gc.getPlayer(iPlayerLoop).countTotalCulture(), iPlayerLoop])

				if (szType == localText.getText("TXT_KEY_TOPCIVS_SIZE", ())):

					self.aiTopCivsValues.append([gc.getPlayer(iPlayerLoop).getTotalLand(), iPlayerLoop])

		# Lowest to Highest
		self.aiTopCivsValues.sort()
		# Switch it around - want the best to be first
		self.aiTopCivsValues.reverse()

		self.printList(szType)
		
	def printList(self, szType):
		
		# Print out the list
		for iRankLoop in range(8):
			
			if (iRankLoop > len(self.aiTopCivsValues)-1):
				return
			
			iPlayer = self.aiTopCivsValues[iRankLoop][1]
			iValue = self.aiTopCivsValues[iRankLoop][0]
			
			szPlayerName = gc.getPlayer(iPlayer).getNameKey()
			
			if (szPlayerName != ""):
				
				pActivePlayerTeam = gc.getTeam(gc.getPlayer(CyGame().getActivePlayer()).getTeam())
				iPlayerTeam = gc.getPlayer(iPlayer).getTeam()
				szCivText = ""
				
				# Does the Active player know this player exists?
				if (iPlayer == CyGame().getActivePlayer() or pActivePlayerTeam.isHasMet(iPlayerTeam)):
					szCivText = localText.getText("TXT_KEY_TOPCIVS_TEXT3", (szPlayerName, self.RankList[iRankLoop]))
					
				else:
					szCivText = localText.getText("TXT_KEY_TOPCIVS_UNKNOWN", ())
					
				szWidgetName = "Text" + str(iRankLoop)
				szWidgetDesc = "%d) %s" % (iRankLoop + 1, szCivText)

				self.screen.newLabel("", "Label" + str(iRankLoop), unicode(szWidgetDesc))
				self.screen.setLabelHAlign("Label" + str(iRankLoop), EAlign.Center)
				
	def turnChecker(self, iTurnNum):

		# Check to see if this is a turn when the screen should pop up (every 50 turns)
		if (not CyGame().isNetworkMultiPlayer() and CyGame().getActivePlayer()>=0):
			if (iTurnNum % 50 == 0 and iTurnNum > 0 and gc.getPlayer(CyGame().getActivePlayer()).isAlive()):
				self.showScreen()

	#####################################################################################################################################
	      
	def handleInput( self, inputClass ):
		self.screen = CyGInterfaceScreen( "CvTopCivs", CvScreenEnums.TOP_CIVS )		

		#if ( inputClass.getFunctionName() == "Exit" and inputClass.getNotifyCode() == NotifyCode.NOTIFY_CLICKED ):
		#	self.screen.hideScreen()
		#	return 1
		if ( inputClass.getData() == int(InputTypes.KB_RETURN) ):
			self.screen.hideScreen()
			return 1
		return 0

	def update(self, fDelta):
		return
