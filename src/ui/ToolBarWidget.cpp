/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2015 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
**************************************************************************/

#include "ToolBarWidget.h"
#include "ContentsWidget.h"
#include "MainWindow.h"
#include "Menu.h"
#include "TabBarWidget.h"
#include "Window.h"
#include "toolbars/ActionWidget.h"
#include "toolbars/AddressWidget.h"
#include "toolbars/BookmarkWidget.h"
#include "toolbars/GoBackActionWidget.h"
#include "toolbars/GoForwardActionWidget.h"
#include "toolbars/MenuButtonWidget.h"
#include "toolbars/PanelChooserWidget.h"
#include "toolbars/SearchWidget.h"
#include "toolbars/StatusMessageWidget.h"
#include "toolbars/ZoomWidget.h"
#include "../core/BookmarksManager.h"
#include "../core/Utils.h"
#include "../core/WindowsManager.h"

#include <QtGui/QMouseEvent>

namespace Otter
{

ToolBarWidget::ToolBarWidget(int identifier, Window *window, QWidget *parent) : QToolBar(parent),
	m_mainWindow(MainWindow::findMainWindow(parent)),
	m_window(window),
	m_identifier(identifier)
{
	setStyleSheet(QLatin1String("QToolBar {padding:0 3px;spacing:3px;}"));
	setAllowedAreas(Qt::AllToolBarAreas);
	setFloatable(false);

	if (identifier >= 0)
	{
		setToolBarLocked(ToolBarsManager::areToolBarsLocked());
		setup();

		connect(this, SIGNAL(topLevelChanged(bool)), this, SLOT(notifyAreaChanged()));
		connect(ToolBarsManager::getInstance(), SIGNAL(toolBarModified(int)), this, SLOT(toolBarModified(int)));
		connect(ToolBarsManager::getInstance(), SIGNAL(toolBarRemoved(int)), this, SLOT(toolBarRemoved(int)));
		connect(ToolBarsManager::getInstance(), SIGNAL(toolBarsLockedChanged(bool)), this, SLOT(setToolBarLocked(bool)));
	}

	if (m_mainWindow && (parent == m_mainWindow || m_identifier < 0))
	{
		connect(m_mainWindow->getWindowsManager(), SIGNAL(currentWindowChanged(qint64)), this, SLOT(notifyWindowChanged(qint64)));
	}
}

void ToolBarWidget::contextMenuEvent(QContextMenuEvent *event)
{
	if (m_identifier < 0)
	{
		event->ignore();

		return;
	}

	if (m_identifier != ToolBarsManager::TabBar)
	{
		QMenu *menu = createCustomizationMenu(m_identifier);
		menu->exec(event->globalPos());
		menu->deleteLater();

		return;
	}

	QList<QAction*> actions;
	QAction *cycleAction = new QAction(tr("Switch tabs using the mouse wheel"), this);
	cycleAction->setCheckable(true);
	cycleAction->setChecked(!SettingsManager::getValue(QLatin1String("TabBar/RequireModifierToSwitchTabOnScroll")).toBool());
	cycleAction->setEnabled(m_mainWindow->getTabBar());

	actions.append(cycleAction);

	if (m_mainWindow->getTabBar())
	{
		connect(cycleAction, SIGNAL(toggled(bool)), m_mainWindow->getTabBar(), SLOT(setCycle(bool)));
	}

	QMenu menu(this);
	menu.addAction(ActionsManager::getAction(Action::NewTabAction, this));
	menu.addAction(ActionsManager::getAction(Action::NewTabPrivateAction, this));
	menu.addSeparator();
	menu.addMenu(createCustomizationMenu(m_identifier, actions, &menu));
	menu.exec(event->globalPos());

	cycleAction->deleteLater();
}

void ToolBarWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton && m_identifier == ToolBarsManager::TabBar)
	{
		ActionsManager::triggerAction((event->modifiers().testFlag(Qt::ShiftModifier) ? Action::NewTabPrivateAction : Action::NewTabAction), this);
	}
}

void ToolBarWidget::setup()
{
	TabBarWidget *tabBar = ((m_identifier == ToolBarsManager::TabBar && m_mainWindow) ? m_mainWindow->getTabBar() : NULL);
	const ToolBarDefinition definition = ToolBarsManager::getToolBarDefinition(m_identifier);

	setVisible(definition.visibility != AlwaysHiddenToolBar);

	if (m_identifier == ToolBarsManager::TabBar)
	{
		for (int i = (actions().count() - 1); i >= 0; --i)
		{
			if (widgetForAction(actions().at(i)) != tabBar)
			{
				removeAction(actions().at(i));
			}
		}
	}
	else
	{
		clear();
	}

	setToolButtonStyle(definition.buttonStyle);

	if (definition.iconSize > 0)
	{
		setIconSize(QSize(definition.iconSize, definition.iconSize));
	}

	if (!definition.bookmarksPath.isEmpty())
	{
		updateBookmarks();

		connect(BookmarksManager::getInstance(), SIGNAL(modelModified()), this, SLOT(updateBookmarks()));

		return;
	}

	for (int i = 0; i < definition.actions.count(); ++i)
	{
		if (definition.actions.at(i).action == QLatin1String("separator"))
		{
			addSeparator();
		}
		else
		{
			if (m_identifier == ToolBarsManager::TabBar && tabBar && definition.actions.at(i).action == QLatin1String("TabBarWidget"))
			{
				addWidget(tabBar);
			}
			else
			{
				addWidget(createWidget(definition.actions.at(i), m_window, this));
			}
		}
	}
}

void ToolBarWidget::toolBarModified(int identifier)
{
	if (identifier == m_identifier)
	{
		setup();
	}
}

void ToolBarWidget::toolBarRemoved(int identifier)
{
	if (identifier == m_identifier)
	{
		deleteLater();
	}
}

void ToolBarWidget::notifyAreaChanged()
{
	if (m_mainWindow)
	{
		emit areaChanged(m_mainWindow->toolBarArea(this));
	}
}

void ToolBarWidget::notifyWindowChanged(qint64 identifier)
{
	m_window = m_mainWindow->getWindowsManager()->getWindowByIdentifier(identifier);

	emit windowChanged(m_window);
}

void ToolBarWidget::updateBookmarks()
{
	const ToolBarDefinition definition = ToolBarsManager::getToolBarDefinition(m_identifier);

	clear();

	BookmarksItem *item = (definition.bookmarksPath.startsWith(QLatin1Char('#')) ? BookmarksManager::getBookmark(definition.bookmarksPath.mid(1).toULongLong()) : BookmarksManager::getModel()->getItem(definition.bookmarksPath));

	if (!item)
	{
		return;
	}

	for (int i = 0; i < item->rowCount(); ++i)
	{
		BookmarksItem *bookmark = dynamic_cast<BookmarksItem*>(item->child(i));

		if (bookmark)
		{
			if (static_cast<BookmarksModel::BookmarkType>(bookmark->data(BookmarksModel::TypeRole).toInt()) == BookmarksModel::SeparatorBookmark)
			{
				addSeparator();
			}
			else
			{
				addWidget(new BookmarkWidget(bookmark, this));
			}
		}
	}
}

void ToolBarWidget::setToolBarLocked(bool locked)
{
	setMovable(!locked);
}

QWidget* ToolBarWidget::createWidget(const ToolBarActionDefinition &definition, Window *window, ToolBarWidget *toolBar)
{
	if (definition.action == QLatin1String("spacer"))
	{
		QWidget *spacer = new QWidget(toolBar);
		spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

		return spacer;
	}

	if (definition.action == QLatin1String("AddressWidget"))
	{
		return new AddressWidget(window, toolBar);
	}

	if (definition.action == QLatin1String("ClosedWindowsWidget"))
	{
		QAction *closedWindowsAction = new QAction(Utils::getIcon(QLatin1String("user-trash")), tr("Closed Tabs"), toolBar);
		closedWindowsAction->setMenu(new Menu(Menu::ClosedWindowsMenu, toolBar));
		closedWindowsAction->setEnabled(false);

		QToolButton *closedWindowsMenuButton = new QToolButton(toolBar);
		closedWindowsMenuButton->setDefaultAction(closedWindowsAction);
		closedWindowsMenuButton->setAutoRaise(true);
		closedWindowsMenuButton->setPopupMode(QToolButton::InstantPopup);

		return closedWindowsMenuButton;
	}

	if (definition.action == QLatin1String("MenuButtonWidget"))
	{
		return new MenuButtonWidget(toolBar);
	}

	if (definition.action == QLatin1String("PanelChooserWidget"))
	{
		return new PanelChooserWidget(toolBar);
	}

	if (definition.action == QLatin1String("SearchWidget"))
	{
		return new SearchWidget(window, toolBar);
	}

	if (definition.action == QLatin1String("StatusMessageWidget"))
	{
		return new StatusMessageWidget(toolBar);
	}

	if (definition.action == QLatin1String("TabBarWidget"))
	{
		if (!toolBar || toolBar->getIdentifier() != ToolBarsManager::TabBar)
		{
			return NULL;
		}

		MainWindow *mainWindow = MainWindow::findMainWindow(toolBar);

		if (!mainWindow || mainWindow->getTabBar())
		{
			return NULL;
		}

		return new TabBarWidget(toolBar);
	}

	if (definition.action == QLatin1String("ZoomWidget"))
	{
		return new ZoomWidget(toolBar);
	}

	if (definition.action.startsWith(QLatin1String("bookmarks:")))
	{
		BookmarksItem *bookmark = (definition.action.startsWith(QLatin1String("bookmarks:/")) ? BookmarksManager::getModel()->getItem(definition.action.mid(11)) : BookmarksManager::getBookmark(definition.action.mid(11).toULongLong()));

		if (bookmark)
		{
			return new BookmarkWidget(bookmark, toolBar);
		}
	}

	if (definition.action.endsWith(QLatin1String("Action")))
	{
		const int identifier = ActionsManager::getActionIdentifier(definition.action.left(definition.action.length() - 6));

		if (identifier >= 0)
		{
			if (identifier == Action::GoBackAction)
			{
				return new GoBackActionWidget(window, toolBar);
			}

			if (identifier == Action::GoForwardAction)
			{
				return new GoForwardActionWidget(window, toolBar);
			}

			return new ActionWidget(identifier, window, toolBar);
		}
	}

	return NULL;
}

QMenu* ToolBarWidget::createCustomizationMenu(int identifier, QList<QAction*> actions, QWidget *parent)
{
	const ToolBarDefinition definition = ToolBarsManager::getToolBarDefinition(identifier);

	QMenu *menu = new QMenu(parent);
	menu->setTitle(tr("Customize"));

	QMenu *toolBarMenu = menu->addMenu(definition.title.isEmpty() ? tr("(Untitled)") : definition.title);
	toolBarMenu->addAction(tr("Configure..."), ToolBarsManager::getInstance(), SLOT(configureToolBar()))->setData(identifier);

	QAction *resetAction = toolBarMenu->addAction(tr("Reset to Defaults..."), ToolBarsManager::getInstance(), SLOT(resetToolBar()));
	resetAction->setData(identifier);
	resetAction->setEnabled(definition.canReset);

	if (!actions.isEmpty())
	{
		toolBarMenu->addSeparator();
		toolBarMenu->addActions(actions);

		for (int i = 0; i < actions.count(); ++i)
		{
			actions.at(i)->setParent(toolBarMenu);
		}
	}

	toolBarMenu->addSeparator();

	QAction *removeAction = toolBarMenu->addAction(Utils::getIcon(QLatin1String("list-remove")), tr("Remove..."), ToolBarsManager::getInstance(), SLOT(removeToolBar()));
	removeAction->setData(identifier);
	removeAction->setEnabled(!definition.isDefault);

	menu->addMenu(new Menu(Menu::ToolBarsMenuRole, menu))->setText(tr("Toolbars"));

	return menu;
}

int ToolBarWidget::getIdentifier() const
{
	return m_identifier;
}

int ToolBarWidget::getMaximumButtonSize() const
{
	return ToolBarsManager::getToolBarDefinition(m_identifier).maximumButtonSize;
}

}
