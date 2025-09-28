from CvPythonExtensions import *
import sys
import importlib

gc = CyGlobalContext()
localText = CyTranslator()

def applyInfluenza2(argsList):
	iEvent = argsList[0]
	kTriggeredData = argsList[1]
		
	player = gc.getPlayer(kTriggeredData.ePlayer)
	eventCity = player.getCity(kTriggeredData.iCityId)

	iNumCities = 2 + gc.getGame().getSorenRandNum(3, "Influenza event number of cities")

	listCities = []	
	(loopCity, iter) = player.firstCity(false)
	while(loopCity):
		if loopCity.getPopulation() > 2:
			iDistance = plotDistance(eventCity.getX(), eventCity.getY(), loopCity.getX(), loopCity.getY())
			if iDistance > 0:
				listCities.append((iDistance, loopCity))
		(loopCity, iter) = player.nextCity(iter, false)
		
	#listCities.sort()
	
	# fortsnek: This is the changed code. Avoid non-deterministic comparison.
	listCities.sort(key=lambda kv: (kv[0], kv[1].getID()))
	
	if iNumCities > len(listCities): 
		iNumCities = len(listCities)
				
	for i in range(iNumCities):
		(iDist, loopCity) = listCities[i]
		loopCity.changePopulation(-2)
		szBuffer = localText.getText("TXT_KEY_EVENT_INFLUENZA_HIT_CITY", (loopCity.getNameKey(), ))
		CyInterface().addMessage(kTriggeredData.ePlayer, false, gc.getEVENT_MESSAGE_TIME(), szBuffer, "AS2D_PILLAGE", InterfaceMessageTypes.MESSAGE_TYPE_INFO, None, gc.getInfoTypeForString("COLOR_RED"), loopCity.getX(), loopCity.getY(), true, true)
				
				
####### Immigrants ########

def canTriggerImmigrantCity(argsList):
	ePlayer = argsList[1]
	iCity = argsList[2]
	
	player = gc.getPlayer(ePlayer)
	city = player.getCity(iCity)

	if city.isNone():
		return false

	if ((city.happyLevel() - city.unhappyLevel(0) < 1) or (city.goodHealth() - city.badHealth(true) < 1)):
		return false

	if (city.getCommerceRateTimes100(CommerceTypes.COMMERCE_CULTURE) < 5500):
		return false
		
	# fortsnek: Fix determinism bug due to uninitialised variable in CvCity::getTriggerValue.
	return True

def init():
	sys.stdout.write("Cv4MiniEngineEntryPoint: Patching CvRandomEventInterface\n")
	mod = importlib.import_module("CvRandomEventInterface")
	setattr(mod, "applyInfluenza2", applyInfluenza2)
	setattr(mod, "canTriggerImmigrantCity", canTriggerImmigrantCity)
	
	try:
		def initRootFolder():
			import BugPath
			import BugUtil
			import CvAltRoot
			if not BugPath.setRootDir(CvAltRoot.rootDir):
				BugUtil.error("Directory from CvAltRoot.py is not valid or does not contain CivilizationIV.ini")
			BugPath._rootFolderInitDone = True
		setattr(importlib.import_module("BugPath"), "initRootFolder", initRootFolder)
		sys.stdout.write("Cv4MiniEngineEntryPoint: Patched BugPath.initRootFolder\n")
	except ImportError:
		sys.stdout.write("Cv4MiniEngineEntryPoint: Could not patch BugPath.initRootFolder. This is normal if you don't have BUG Mod.\n")
		pass
	
	
	
