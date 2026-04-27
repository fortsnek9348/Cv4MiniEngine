#include "MainInterfaceControls.h"
#include "CvApp.h"
#include "TuiTextCode.h"
#include "CvTuiInterface.h"
#include "CvTuiMainInterface.h"

#include <Cv4CommonEngineLib/CvTranslator.h>

#include <CvGameCoreDLL/CvSelectionGroup.h>
#include <CvGameCoreDLL/CvGlobals.h>
#include <CvGameCoreDLL/CvUnit.h>
#include <CvGameCoreDLL/CvGameAI.h>

#include <ranges>

using namespace hecktui;
using namespace cvengine;

namespace
{
	struct RichPrinter
	{
		Pixel def{};

		std::vector<Pixel> pixels;

		[[nodiscard]] Colour setTextColour(Colour c)
		{
			return std::exchange(def.colour.text, c);
		}

		[[nodiscard]] Colour setBackColour(Colour c)
		{
			return std::exchange(def.colour.back, c);
		}

		RichPrinter& operator+=(std::wstring_view str)
		{
			pixels.append_range(str | std::views::transform([this](wchar_t c) {
				Pixel pixel = def;
				pixel.c = c;
				return pixel;
			}));
			return *this;
		}
	};
}

PlotListUnitButton::PlotListUnitButton(CvUnit* unit) : EmptyButton({}), mUnit(unit)
{
	if (!mUnit)
		std::abort();

	setEnableRightClick(true);

	mCanFocus = false;
	mIsMouseInteractable = true;
	setBorderStyle(EBorderStyle::None);

	

	//RichPrinter richPrinter{
	//	.def{
	//		.colour{.text = mUnit->IsSelected() ? EColour(226) : EColour::Silver, .back = Colour() },
	//		}
	//};

	std::wstring text;

	std::wstring name = unit->getName();

	if (unit->getOwner() == gGlobals.getGame().getActivePlayer())
	{
		//const int iDenom = unit->movesLeft() % gGlobals.getMOVE_DENOMINATOR() > 0 ? 1 : 0;
		//const int iCurrMoves = ((unit->movesLeft() / gGlobals.getMOVE_DENOMINATOR()) + iDenom);
		const int wholeMovesLeft = unit->movesLeft() / gGlobals.getMOVE_DENOMINATOR();
		const int hundredthsMovesLeft = unit->movesLeft() % gGlobals.getMOVE_DENOMINATOR() * 100 / gGlobals.getMOVE_DENOMINATOR();

		//const bool zeroMovesLeft = unit->movesLeft() == 0;
		//const bool partialMovesLeft = wholeMovesLeft < unit->baseMoves();

		const TeamTypes team = gGlobals.getGame().getActiveTeam();

		// Replicating from CvMainInterface.py.
		const int str = unit->canFight() ? (unit->getDomainType() == DOMAIN_AIR ? unit->airBaseCombatStr() : unit->baseCombatStr()) : 0;
		const int hp = unit->currHitPoints();
		const int maxHP = unit->maxHitPoints();
		const bool canFight = unit->canFight();
		const bool isFighting = unit->isFighting();
		const bool isHurt = unit->isHurt();

		if (canFight)
		{
			std::wstring strText;
			if (isFighting)
				strText += std::format(L"?/{}", str);
			else if (isHurt)
				strText += std::format(L"{:.1f}/{}", double(str) * hp / maxHP, str);
			else
				strText += std::format(L"{}", str);
			//richPrinter += L" ";
			strText = CvTranslator::changeTextColor(std::move(strText), gGlobals.getInfoTypeForString(isHurt ? "COLOR_YELLOW" : "COLOR_POSITIVE_TEXT"));
			text += strText;
			text += cvengine::lookupSymbolChar(CvTranslator::getInstance().symbols.at(STRENGTH_CHAR));
			text += L" ";
		}


		const char* const overlayColour =
			unit->getTeam() != team || unit->isWaiting() ? "COLOR_LIGHT_GREY" :
			unit->canMove() ? (unit->hasMoved() ? "COLOR_YELLOW" : "COLOR_POSITIVE_TEXT") :
			"COLOR_NEGATIVE_TEXT";

		std::wstring movesText;

		if (hundredthsMovesLeft)
			movesText = std::format(L"{}.{:02}/{}", wholeMovesLeft, hundredthsMovesLeft, unit->baseMoves());
		else
			movesText = std::format(L"{}/{}", wholeMovesLeft, unit->baseMoves());
		movesText = CvTranslator::changeTextColor(std::move(movesText), gGlobals.getInfoTypeForString(overlayColour));
		text += movesText;
		text += cvengine::lookupSymbolChar(CvTranslator::getInstance().symbols.at(MOVES_CHAR));

		//richPrinter += L" ";

		//text = CvTranslator().changeTextColor(std::move(text), gGlobals.getInfoTypeForString(overlayColour));
	}
	else
	{
		//richPrinter.def.colour.text = Colour();
		setEnabled(false);
	}



	const MissionTypes mission = (MissionTypes)unit->getGroup()->getMissionType(0);
	const ActivityTypes activity = unit->getGroup()->getActivityType();
	const AutomateTypes automation = unit->getGroup()->getAutomateType();

	struct ActivityGlyph
	{
		std::wstring_view text{};
		const char* fg = "COLOR_LIGHT_GREY";
		const char* bg{};
	};

	ActivityGlyph activityGlyph{};

	switch (activity)
	{
	case ACTIVITY_INTERCEPT:
		activityGlyph = { L"i", "COLOR_YELLOW", "COLOR_WATER_TEXT1" };
		break;
	case ACTIVITY_PATROL:
		activityGlyph = { L"p", "COLOR_YELLOW", "COLOR_TECH_BLUE" };
		break;
	case ACTIVITY_PLUNDER:
		activityGlyph = { L"P", "COLOR_RED", "COLOR_TECH_BLUE" }; break;
		break;
	case ACTIVITY_HEAL:
		activityGlyph = { L"+", "COLOR_RED", "COLOR_WHITE" }; break;
		break;
	case ACTIVITY_SENTRY:
		activityGlyph = { L"S", "COLOR_YELLOW", "COLOR_CITY_BROWN" };
		break;
	case ACTIVITY_HOLD:
		activityGlyph = { L"-", "COLOR_WHITE", "COLOR_BLACK" };
		break;
	default:
		switch (automation)
		{
		case AUTOMATE_EXPLORE:
			activityGlyph = { L"E~", "COLOR_WHITE", "COLOR_BLACK" };
			break;
		case AUTOMATE_BUILD:
			activityGlyph = { L"B~", "COLOR_YELLOW", "COLOR_WHITE" };
			break;
		case AUTOMATE_CITY:
			activityGlyph = { L"c~", "COLOR_YELLOW", "COLOR_WHITE" };
			break;
		case AUTOMATE_NETWORK:
			activityGlyph = { L"n~", "COLOR_YELLOW", "COLOR_WHITE" };
			break;
		case AUTOMATE_RELIGION:
			activityGlyph = { L"r~", "COLOR_MAGENTA" };
			break;
		default:
			if (mission == MISSION_BUILD)
				activityGlyph = { L"B~", "COLOR_YELLOW", "COLOR_WHITE" };
			else if (mission == MISSION_MOVE_TO || mission == MISSION_MOVE_TO_UNIT)
				activityGlyph = { L"-->", "COLOR_GREEN" };
			else if (unit->isWaiting())
			{
				if (unit->isFortifyable())
					activityGlyph = { L"F", "COLOR_BLACK", "COLOR_LIGHT_GREY" };
				else
					activityGlyph = { L"zzz", "COLOR_BLACK", "COLOR_LIGHT_GREY" };
			}
			else
			{
				// The unit is probably not doing anything.
			}
			break;
		}
	}

	std::wstring activityText(activityGlyph.text);
	if (activityGlyph.fg)
		activityText = CvTranslator::changeTextColor(std::move(activityText), gGlobals.getInfoTypeForString(activityGlyph.fg));
	if (activityGlyph.bg)
		activityText = CvTranslator::changeBackColor(std::move(activityText), gGlobals.getInfoTypeForString(activityGlyph.bg));

	text += L' ';
	text += activityText;

	text += L' ';
	text += name;

	mRichLabel = std::make_shared<RichLabel>(L"", false);
	mRichLabel->colouring = { kTransparent, kTransparent };
	mRichLabel->setLabel(text);
	addChild(mRichLabel);

	auto layout = std::make_unique<FillLayout>();
	layout->marginTopLeft = { 1, 0 };
	layout->marginBottomRight = { 1, 0 };
	setLayout(std::move(layout));
}

void PlotListUnitButton::onClick(ModifierKeyState modifierKeyState)
{
	CvTuiMainInterface& mainInterface = *CvTuiInterface::getInstance().getTuiMainInterface();
	mainInterface.doUnitListUIUnitSelection(mUnit, modifierKeyState.shift, modifierKeyState.ctrl);
	//ScreenInteractive::Active()->PostEvent(kUIEvent_UnitSelectionChanged);
	mainInterface.onGameStateChanged(CvTuiMainInterface::EGameStateChangeReason::UnitSelection);
}

void PlotListUnitButton::onRightClick(hecktui::ModifierKeyState modifierKeyState)
{
	CvTuiMainInterface& mainInterface = *CvTuiInterface::getInstance().getTuiMainInterface();
	modifierKeyState.shift = true;
	const auto oldState = CvApp::getInstance().getUI().exchangeLastModifierKeysState(modifierKeyState);
	mainInterface.doUnitListUIUnitSelection(mUnit, modifierKeyState.shift, modifierKeyState.ctrl);
	mainInterface.onGameStateChanged(CvTuiMainInterface::EGameStateChangeReason::UnitSelection);
	(void)CvApp::getInstance().getUI().exchangeLastModifierKeysState(oldState);
}

LayoutSizeInfo PlotListUnitButton::measureThis() const noexcept
{
	return {};
}

void PlotListUnitButton::drawThis(ivec2 offset, hecktui::Framebuffer& fb)
{
	const bool sel = mUnit->IsSelected();

	setBackgroundColour(sel ? EColour(19) : EColour::Black);

	EmptyButton::drawThis(offset, fb);

	const ivec2 position = getRect().min + offset;

	if (sel)
	{
		fb.draw(position, {
			.c = L'[',
			.colour{.text{}, .back{} },
			});
		fb.draw(position + ivec2(getRect().size().x - 1, 0), {
			.c = L']',
			.colour{.text{}, .back{} },
			});
	}
}
