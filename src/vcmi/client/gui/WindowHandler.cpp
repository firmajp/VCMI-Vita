/*
 * WindowHandler.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#ifdef VCMI_VITA
#include "lib/vita/VitaBattleEvents.h"
#endif
#include "WindowHandler.h"

#include "GameEngine.h"
#include "CIntObject.h"
#include "CursorHandler.h"
#include "EventDispatcher.h"

#include "render/Canvas.h"
#include "render/IScreenHandler.h"
#include "render/Colors.h"

void WindowHandler::popWindow(std::shared_ptr<IShowActivatable> top)
{
	if (windowsStack.back() != top)
		throw std::runtime_error("Attempt to pop non-top window from stack!");

	ENGINE->events().cancelControllerInput(false);
	top->deactivate();
	disposed.push_back(top);
	windowsStack.pop_back();
	if(!windowsStack.empty())
		windowsStack.back()->activate();

	totalRedraw();
}

void WindowHandler::pushWindow(std::shared_ptr<IShowActivatable> newInt)
{
	if (newInt == nullptr)
		throw std::runtime_error("Attempt to push null window onto windows stack!");

	if (vstd::contains(windowsStack, newInt))
		throw std::runtime_error("Attempt to add already existing window to stack!");

	ENGINE->events().cancelControllerInput(true);
	if(!windowsStack.empty())
		windowsStack.back()->deactivate();
	windowsStack.push_back(newInt);
	ENGINE->cursor().set(Cursor::Map::POINTER);
	newInt->activate();
	totalRedraw();
}

bool WindowHandler::isTopWindowPopup() const
{
	if (windowsStack.empty())
		return false;

	return windowsStack.back()->isPopupWindow();
}

void WindowHandler::popWindows(int howMany)
{
	if(!howMany)
		return; //senseless but who knows...

	assert(windowsStack.size() >= howMany);
	ENGINE->events().cancelControllerInput(false);
	windowsStack.back()->deactivate();
	for(int i = 0; i < howMany; i++)
	{
		disposed.push_back(windowsStack.back());
		windowsStack.pop_back();
	}

	if(!windowsStack.empty())
	{
		windowsStack.back()->activate();
		totalRedraw();
	}
	ENGINE->fakeMouseMove();
}

std::shared_ptr<IShowActivatable> WindowHandler::topWindowImpl() const
{
	if(windowsStack.empty())
		return nullptr;

	return windowsStack.back();
}

bool WindowHandler::isTopWindow(std::shared_ptr<IShowActivatable> window) const
{
	assert(window != nullptr);
	return !windowsStack.empty() && windowsStack.back() == window;
}

bool WindowHandler::isTopWindow(IShowActivatable * window) const
{
	assert(window != nullptr);
	return !windowsStack.empty() && windowsStack.back().get() == window;
}

void WindowHandler::totalRedraw()
{
	totalRedrawRequested = true;
}

void WindowHandler::requestRedraw(CIntObject * object)
{
#ifdef VCMI_VITA
	VitaBattleEvents::requests.fetch_add(1, std::memory_order_relaxed);
	if(vstd::contains(pendingRedraws, object)) VitaBattleEvents::coalesced.fetch_add(1, std::memory_order_relaxed);
#endif
	if(!vstd::contains(pendingRedraws, object))
		pendingRedraws.push_back(object);

	hasPendingRedraws = true;
}

void WindowHandler::cancelRedraw(CIntObject * object)
{
	// every destroyed widget passes here, so the common case must stay cheap
	if(!hasPendingRedraws)
		return;

	vstd::erase(pendingRedraws, object);
	hasPendingRedraws = !pendingRedraws.empty();
}

void WindowHandler::processPendingRedraws()
{
#ifdef VCMI_VITA
	VitaBattleEvents::Scope event(VitaBattleEvents::PendingPaint);
#endif
	std::vector<CIntObject *> pending;
	pending.swap(pendingRedraws);
	hasPendingRedraws = false;

	if(pending.empty())
		return;

	Canvas target = ENGINE->screenHandler().getScreenCanvas();

#ifdef VCMI_VITA
	// Classify before painting: showAll() can mutate the widget tree.
	std::vector<uint8_t> skip(pending.size(), 0);
	for(size_t i = 0; i < pending.size(); ++i)
	{
		auto * object = pending[i];
		if(!object->isActive())
		{
			skip[i] = 3;
			VitaBattleEvents::pendingInactive.fetch_add(1, std::memory_order_relaxed);
			continue;
		}
		for(auto * node = object; node; node = node->parent)
		{
			if(totalRedrawRequested && (overlay.get() == node || std::any_of(windowsStack.begin(), windowsStack.end(),
				[node](const auto & window) { return window.get() == node; })))
			{
				skip[i] = 1;
				VitaBattleEvents::pendingFull.fetch_add(1, std::memory_order_relaxed);
				break;
			}
			if(node != object && node->isActive() && vstd::contains(pending, node))
			{
				skip[i] = 2;
				VitaBattleEvents::pendingAncestor.fetch_add(1, std::memory_order_relaxed);
				break;
			}
			// A parent paint only covers children participating in SHOWALL propagation.
			if(!(node->recActions & CIntObject::SHOWALL))
				break;
		}
	}
	for(size_t i = 0; i < pending.size(); ++i)
		if(!skip[i])
			pending[i]->showAll(target);
#else
	for(CIntObject * object : pending)
		object->showAll(target);
#endif
}

void WindowHandler::totalRedrawImpl()
{
#ifdef VCMI_VITA
	VitaBattleEvents::Scope event(VitaBattleEvents::FullPaint);
#endif
	logGlobal->debug("totalRedraw requested!");

	Canvas target = ENGINE->screenHandler().getScreenCanvas();

	for(auto & elem : windowsStack)
		elem->showAll(target);

	if(overlay)
		overlay->showAll(target);
}

void WindowHandler::simpleRedraw()
{
	processPendingRedraws();

	if (totalRedrawRequested)
		totalRedrawImpl();
	else
		simpleRedrawImpl();

	totalRedrawRequested = false;
}

void WindowHandler::simpleRedrawImpl()
{
	Canvas target = ENGINE->screenHandler().getScreenCanvas();

	if(!windowsStack.empty())
		windowsStack.back()->show(target); //blit active interface/window

	if(overlay)
		overlay->showAll(target);
}

void WindowHandler::onScreenResize()
{
	for(const auto & entry : windowsStack)
		entry->onScreenResize();

	totalRedraw();
}

void WindowHandler::onFrameRendered()
{
	disposed.clear();
}

size_t WindowHandler::count() const
{
	return windowsStack.size();
}

void WindowHandler::clear()
{
	if(!windowsStack.empty())
		windowsStack.back()->deactivate();

	windowsStack.clear();
	disposed.clear();

	// covered windows reclaim their layers on redraw, and only the top one was deactivated
	for(size_t i = 0; i < static_cast<size_t>(GpuRenderLayer::COUNT); ++i)
		ENGINE->screenHandler().releaseLayer(static_cast<GpuRenderLayer>(i));
}

void WindowHandler::setOverlay(std::shared_ptr<IShowActivatable> newOverlay)
{
	if(overlay)
		overlay->deactivate();

	overlay = std::move(newOverlay);

	if(overlay)
		overlay->activate();

	totalRedraw();
}

std::vector<std::shared_ptr<IShowActivatable>> WindowHandler::detachAll()
{
	if(!windowsStack.empty())
		windowsStack.back()->deactivate();

	auto result = std::move(windowsStack);
	windowsStack.clear();
	disposed.clear();
	return result;
}

void WindowHandler::attachAll(std::vector<std::shared_ptr<IShowActivatable>> windows)
{
	clear();

	windowsStack = std::move(windows);

	if(!windowsStack.empty())
		windowsStack.back()->activate();

	totalRedraw();
}
