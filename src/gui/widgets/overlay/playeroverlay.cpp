////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2021 Ripose
//
// This file is part of Memento.
//
// Memento is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2 of the License.
//
// Memento is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Memento.  If not, see <https://www.gnu.org/licenses/>.
//
////////////////////////////////////////////////////////////////////////////////

#include "playeroverlay.h"

#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QSettings>

#include "../../../util/constants.h"
#include "../../../util/globalmediator.h"
#include "../../playeradapter.h"
#include "../definition/definitionwidget.h"
#include "playercontrols.h"
#include "playermenu.h"
#include "subtitlewidget.h"

/* Begin Constructor/Destructor */

PlayerOverlay::PlayerOverlay(QWidget *parent)
    : QVBoxLayout(parent),
      m_player(parent),
      m_definition(nullptr)
{
    /* Fix the margins */
    setSpacing(0);
    setContentsMargins(QMargins(0, 0, 0, 0));
    setMargin(0);

    /* Add the menubar */
    m_menu = new PlayerMenu;
    m_menu->hideMenu();
    QGraphicsOpacityEffect *menuEffect = new QGraphicsOpacityEffect;
    menuEffect->setOpacity(0);
    m_menu->setGraphicsEffect(menuEffect);
    addWidget(m_menu);

    /* Add space between the menu and subtitle */
    addStretch();

    /* Add the subtitle */
    m_subtitle = new SubtitleWidget;
    addWidget(m_subtitle, 0, Qt::AlignmentFlag::AlignCenter);

    /* Add the generic widget spacer */
    m_spacer = new QWidget;
    m_spacer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_spacer->setFixedWidth(0);
    m_spacer->setMouseTracking(true);
    addWidget(m_spacer);

    /* Add the player controls */
    m_controls = new PlayerControls;
    m_controls->hide();
    m_controls->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_controls->setVolumeLimit(
        GlobalMediator::getGlobalMediator()->getPlayerAdapter()->getMaxVolume()
    );
    m_player->setMinimumWidth(m_controls->minimumWidth());
    QGraphicsOpacityEffect *controlsEffect = new QGraphicsOpacityEffect;
    controlsEffect->setOpacity(0);
    m_controls->setGraphicsEffect(controlsEffect);
    addWidget(m_controls);

    GlobalMediator *mediator = GlobalMediator::getGlobalMediator();
    connect(
        mediator, &GlobalMediator::interfaceSettingsChanged,
        this, &PlayerOverlay::initSettings
    );
    connect(
        mediator, &GlobalMediator::playerResized,
        this, &PlayerOverlay::repositionSubtitles
    );
    connect(
        mediator, &GlobalMediator::playerMouseMoved,
        this, &PlayerOverlay::showOverlay
    );
    connect(
        mediator, &GlobalMediator::playerCursorHidden,
        this, &PlayerOverlay::hideOverlay
    );
    connect(
        mediator, &GlobalMediator::playerFullscreenChanged,
        this, &PlayerOverlay::hideOverlay
    );
    connect(
        m_menu, &PlayerMenu::aboutToHide,
        this, &PlayerOverlay::hideOverlay
    );

    connect(
        mediator, &GlobalMediator::menuSubtitleSizeIncrease,
        this, &PlayerOverlay::increaseSubScale
    );
    connect(
        mediator, &GlobalMediator::menuSubtitleSizeDecrease,
        this, &PlayerOverlay::decreaseSubScale
    );
    connect(
        mediator, &GlobalMediator::menuSubtitlesMoveUp,
        this, &PlayerOverlay::moveSubsUp
    );
    connect(
        mediator, &GlobalMediator::menuSubtitlesMoveDown,
        this, &PlayerOverlay::moveSubsDown
    );

    connect(
        mediator, &GlobalMediator::termsChanged,
        this, &PlayerOverlay::setTerms
    );
    connect(
        mediator, &GlobalMediator::requestDefinitionDelete,
        this, &PlayerOverlay::deleteDefinitionWidget
    );
    connect(
        mediator, &GlobalMediator::playerSubtitleChanged,
        this, &PlayerOverlay::deleteDefinitionWidget
    );
    connect(
        mediator, &GlobalMediator::playerResized,
        this, &PlayerOverlay::deleteDefinitionWidget
    );
    connect(
        mediator, &GlobalMediator::subtitleExpired,
        this, &PlayerOverlay::deleteDefinitionWidget
    );
    connect(
        mediator, &GlobalMediator::subtitleListShown,
        this, &PlayerOverlay::deleteDefinitionWidget
    );
    connect(
        mediator, &GlobalMediator::subtitleListHidden,
        this, &PlayerOverlay::deleteDefinitionWidget
    );
    connect(
        mediator, &GlobalMediator::subtitleHidden,
        this, &PlayerOverlay::deleteDefinitionWidget
    );
    connect(
        mediator, &GlobalMediator::playerPauseStateChanged, this,
        [=] (const bool paused) {
            if (!paused)
                deleteDefinitionWidget();
        }
    );

    initSettings();
}

PlayerOverlay::~PlayerOverlay()
{
    deleteDefinitionWidget();
}

/* End Constructor/Destructor */
/* Begin Initializers */

void PlayerOverlay::initSettings()
{
    QSettings settings;
    settings.beginGroup(SETTINGS_INTERFACE);

    m_subOffset = settings.value(
            SETTINGS_INTERFACE_SUB_OFFSET,
            SETTINGS_INTERFACE_SUB_OFFSET_DEFAULT
        ).toDouble();

    settings.endGroup();

    repositionSubtitles();
}

/* End Initializers */
/* Begin Definition Widget Helpers */

void PlayerOverlay::setTerms(const QList<Term *> *terms)
{
    deleteDefinitionWidget();

    m_definition = new DefinitionWidget(terms, m_player);
    setDefinitionWidgetLocation();
    if (m_definition)
    {
        m_definition->show();
    }
}

void PlayerOverlay::setDefinitionWidgetLocation()
{
    const QPoint mousePos = m_player->mapFromGlobal(QCursor::pos());

    int x = mousePos.x() - (m_definition->width() / 2);
    if (x < 0)
    {
        x = 0;
    }
    else if (x > m_player->width() - m_definition->width())
    {
        x = m_player->width() - m_definition->width();
    }

    int y = m_subtitle->pos().y() - m_definition->height();
    if (y < 0)
    {
        y = m_subtitle->pos().y() +
            m_subtitle->height();
    }

    if (y + m_definition->height() > m_player->height())
    {
        deleteDefinitionWidget();
    }
    else
    {
        m_definition->move(x, y);
    }
}

void PlayerOverlay::deleteDefinitionWidget()
{
    if (m_definition)
    {
        m_definition->hide();
        m_definition->deleteLater();
    }
    m_definition = nullptr;
}

/* End Definition Widget Helpers */
/* Begin Subtitle Widget Helpers */

void PlayerOverlay::repositionSubtitles()
{
    int height = m_player->height() * m_subOffset;
    if (m_controls->isVisible())
    {
        height -= m_controls->height();
        if (height < 0)
        {
            height = 0;
        }
    }
    m_spacer->setFixedHeight(height);
}

void PlayerOverlay::updateSubScale(const double inc)
{
    QSettings settings;
    settings.beginGroup(SETTINGS_INTERFACE);
    double scale = settings.value(
        SETTINGS_INTERFACE_SUB_SCALE,
        SETTINGS_INTERFACE_SUB_SCALE_DEFAULT
    ).toDouble();
    scale += inc;
    if (scale <= 0.0)
    {
        scale = 0.0;
    }
    else if (scale >= 1.0)
    {
        scale = 1.0;
    }
    settings.setValue(SETTINGS_INTERFACE_SUB_SCALE, scale);
    Q_EMIT GlobalMediator::getGlobalMediator()->interfaceSettingsChanged();
}

void PlayerOverlay::increaseSubScale()
{
    updateSubScale(0.005);
}

void PlayerOverlay::decreaseSubScale()
{
    updateSubScale(-0.005);
}

void PlayerOverlay::moveSubtitles(const double inc)
{
    QSettings settings;
    settings.beginGroup(SETTINGS_INTERFACE);
    double offset = settings.value(
        SETTINGS_INTERFACE_SUB_OFFSET,
        SETTINGS_INTERFACE_SUB_OFFSET_DEFAULT
    ).toDouble();
    offset += inc;
    if (offset <= 0.0)
    {
        offset = 0.0;
    }
    else if (offset >= 1.0)
    {
        offset = 1.0;
    }
    settings.setValue(SETTINGS_INTERFACE_SUB_OFFSET, offset);
    Q_EMIT GlobalMediator::getGlobalMediator()->interfaceSettingsChanged();
}

void PlayerOverlay::moveSubsUp()
{
    moveSubtitles(0.005);
}

void PlayerOverlay::moveSubsDown()
{
    moveSubtitles(-0.005);
}

/* End Subtitle Widget Helpers */
/* Begin General Widget Properties */

bool PlayerOverlay::underMouse() const
{
    return m_player->childrenRegion().contains(
        m_player->mapFromGlobal(QCursor::pos())
    );
}

#define FADE_TIME 250

void PlayerOverlay::hideOverlay()
{
    if (underMouse() || m_menu->menuOpen() || m_definition)
    {
        return;
    }

    if (m_menu->menuVisible())
    {
        QPropertyAnimation *menuFade =
            new QPropertyAnimation(m_menu->graphicsEffect(), "opacity");
        menuFade->setDuration(FADE_TIME);
        menuFade->setStartValue(1);
        menuFade->setEndValue(0);
        menuFade->setEasingCurve(QEasingCurve::InBack);
        connect(
            menuFade, &QPropertyAnimation::finished,
            m_menu, &PlayerMenu::hideMenu
        );
        menuFade->start(QPropertyAnimation::DeleteWhenStopped);
    }

    if (m_controls->isVisible())
    {
        QPropertyAnimation *controlFade =
            new QPropertyAnimation(m_controls->graphicsEffect(), "opacity");
        controlFade->setDuration(FADE_TIME);
        controlFade->setStartValue(1);
        controlFade->setEndValue(0);
        controlFade->setEasingCurve(QEasingCurve::InBack);
        connect(
            controlFade, &QPropertyAnimation::finished,
            m_controls, &QWidget::hide
        );
        connect(
            controlFade, &QPropertyAnimation::finished,
            this, &PlayerOverlay::repositionSubtitles
        );
        controlFade->start(QPropertyAnimation::DeleteWhenStopped);
    }
}

void PlayerOverlay::showOverlay()
{
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    if (!m_menu->menuVisible() && !m_menu->window()->isFullScreen())
#else
    if (!m_menu->menuVisible())
#endif
    {
        m_menu->showMenu();
        QPropertyAnimation *menuFade =
            new QPropertyAnimation(m_menu->graphicsEffect(), "opacity");
        menuFade->setDuration(FADE_TIME);
        menuFade->setStartValue(0);
        menuFade->setEndValue(1);
        menuFade->setEasingCurve(QEasingCurve::InBack);
        menuFade->start(QPropertyAnimation::DeleteWhenStopped);
    }

    if (m_controls->isHidden())
    {
        m_controls->show();
        QPropertyAnimation *controlFade =
            new QPropertyAnimation(m_controls->graphicsEffect(), "opacity");
        controlFade->setDuration(FADE_TIME);
        controlFade->setStartValue(0);
        controlFade->setEndValue(1);
        controlFade->setEasingCurve(QEasingCurve::InBack);
        controlFade->start(QPropertyAnimation::DeleteWhenStopped);

        repositionSubtitles();
    }
}

#undef FADE_TIME

/* End General Widget Properties */
