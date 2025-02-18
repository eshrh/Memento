////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2020 Ripose
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

#include "termwidget.h"
#include "ui_termwidget.h"

#include <QMenu>

#include "../../../audio/audioplayer.h"
#include "../../../dict/dictionary.h"
#include "../../../util/globalmediator.h"
#include "../../../util/iconfactory.h"
#include "../subtitlelistwidget.h"
#include "glossarywidget.h"
#include "pitchwidget.h"
#include "tagwidget.h"

/**
 * Kanji stylesheet format string.
 * @param 1 The color of links. Should be the same as text color.
 */
#define KANJI_STYLE_STRING      (QString(\
        "<style>"\
            "a {"\
                "color: %1;"\
                "border: 0;"\
                "text-decoration: none;"\
            "}"\
        "</style>"\
    ))

/**
 * Kanji link format string. Used for making kanji clickable.
 * @param 1 A single kanji such that when clicked, the kanji will be emitted in
 *          in the linkActivated signal.
 */
#define KANJI_FORMAT_STRING     (QString("<a href=\"%1\">%1</a>"))

#if __APPLE__
    #define EXPRESSION_STYLE    (QString("QLabel { font-size: 30pt; }"))
    #define READING_STYLE       (QString("QLabel { font-size: 18pt; }"))
#else
    #define EXPRESSION_STYLE    (QString("QLabel { font-size: 20pt; }"))
    #define READING_STYLE       (QString("QLabel { font-size: 12pt; }"))
#endif

/* Begin Constructor/Destructor */

TermWidget::TermWidget(const Term               *term,
                       const QList<AudioSource> *sources,
                       QWidget                  *parent)
    : QWidget(parent),
      m_ui(new Ui::TermWidget),
      m_term(term),
      m_client(GlobalMediator::getGlobalMediator()->getAnkiClient()),
      m_sources(sources)
{
    m_ui->setupUi(this);

    m_ui->labelKanji->setStyleSheet(EXPRESSION_STYLE);
    m_ui->labelKana->setStyleSheet(READING_STYLE);

    m_layoutTermTags = new FlowLayout(-1, 6);
    m_layoutFreqTags = new FlowLayout(-1, 6);
    m_layoutPitches  = new QVBoxLayout;
    m_layoutGlossary = new QVBoxLayout;

    m_ui->verticalLayout->addLayout(m_layoutTermTags);
    m_ui->verticalLayout->addLayout(m_layoutFreqTags);
    m_ui->verticalLayout->addLayout(m_layoutPitches);
    m_ui->verticalLayout->addLayout(m_layoutGlossary);

    m_ui->verticalLayout->addStretch();

    IconFactory *factory = IconFactory::create();

    m_ui->buttonAddCard->setIcon(factory->getIcon(IconFactory::Icon::plus));
    m_ui->buttonAddCard->setVisible(false);

    m_ui->buttonAnkiOpen->setIcon(
        factory->getIcon(IconFactory::Icon::hamburger)
    );
    m_ui->buttonAnkiOpen->setVisible(false);

    m_ui->buttonAudio->setIcon(factory->getIcon(IconFactory::Icon::audio));
    m_ui->buttonAudio->setVisible(!m_sources->isEmpty());

    initUi(*term);

    connect(
        m_ui->labelKanji, &QLabel::linkActivated,
        this,             &TermWidget::searchKanji
    );
    connect(
        m_ui->buttonAddCard, &QToolButton::clicked,
        this,                &TermWidget::addNote
    );
    connect(
        m_ui->buttonAnkiOpen, &QToolButton::clicked,
        this,                 &TermWidget::searchAnki
    );
    connect(
        m_ui->buttonAudio, &QToolButton::customContextMenuRequested,
        this,              &TermWidget::showAudioSources
    );
    connect(m_ui->buttonAudio, &QToolButton::clicked, this,
        [=] {
            if (!m_sources->isEmpty())
            {
                const AudioSource &src = m_sources->first();
                playAudio(src.url, src.md5);
            }
        }
    );
}

TermWidget::~TermWidget()
{
    delete m_ui;
}

/* End Constructor/Destructor */
/* Begin Initializers */

void TermWidget::initUi(const Term &term)
{
    if (term.reading.isEmpty())
    {
        m_ui->labelKana->hide();
    }
    else
    {
        m_ui->labelKana->show();
    }
    m_ui->labelKana->setText(term.reading);
    QString kanjiLabelText = KANJI_STYLE_STRING.arg(
            m_ui->labelKanji->palette().color(
                m_ui->labelKanji->foregroundRole()
            ).name()
        );
    for (const QString &ch : term.expression)
    {
        kanjiLabelText += isKanji(ch) ? KANJI_FORMAT_STRING.arg(ch) : ch;
    }
    m_ui->labelKanji->setText(kanjiLabelText);
    m_ui->labelJisho->setText(generateJishoLink(term.expression));

    for (const Frequency &freq : term.frequencies)
    {
        TagWidget *tag = new TagWidget(freq);
        m_layoutFreqTags->addWidget(tag);
    }

    for (const Pitch &pitch : term.pitches)
    {
        m_layoutPitches->addWidget(new PitchWidget(pitch));
    }

    for (const Tag &termTag : term.tags)
    {
        TagWidget *tag = new TagWidget(termTag);
        m_layoutTermTags->addWidget(tag);
    }

    for (size_t i = 0; i < term.definitions.size(); ++i)
    {
        m_layoutGlossary->addWidget(
            new GlossaryWidget(i + 1, term.definitions[i])
        );
    }
}

/* End Initializers */
/* Begin User Input Handlers */

void TermWidget::addNote()
{
    m_ui->buttonAddCard->setEnabled(false);

    Term *term = new Term(*m_term);
    term->definitions.clear();
    for (size_t i = 0; i < m_layoutGlossary->count(); ++i)
    {
        GlossaryWidget *widget =
            (GlossaryWidget *)m_layoutGlossary->itemAt(i)->widget();
        if (widget->isChecked())
        {
            term->definitions.append(TermDefinition(m_term->definitions[i]));
        }
        widget->setCheckable(false);
    }

    GlobalMediator *mediator = GlobalMediator::getGlobalMediator();
    term->context =
        mediator->getSubtitleListWidget()->getPrimaryContext("\n");
    term->context2 =
        mediator->getSubtitleListWidget()->getSecondaryContext("\n");

    AnkiReply *reply = m_client->addNote(term);
    connect(reply, &AnkiReply::finishedInt, this,
        [=] (const int id, const QString &error) {
            if (!error.isEmpty())
            {
                Q_EMIT GlobalMediator::getGlobalMediator()
                    ->showCritical("Error Adding Note", error);
            }
            else
            {
                m_ui->buttonAnkiOpen->setVisible(true);
                m_ui->buttonAddCard->setVisible(false);
            }
        }
    );
}

void TermWidget::searchAnki()
{
    QString deck = m_client->getConfig()->termDeck;
    AnkiReply *reply = m_client->openBrowse(deck, m_term->expression);
    connect(reply, &AnkiReply::finishedIntList, this,
        [=] (const QList<int> &value, const QString &error) {
            if (!error.isEmpty())
            {
                Q_EMIT GlobalMediator::getGlobalMediator()->showCritical(
                    "Error Opening Anki", error
                );
            }
        }
    );
}

void TermWidget::playAudio(QString url, const QString &hash)
{
    m_ui->buttonAudio->setEnabled(false);
    AudioPlayerReply *reply =
        GlobalMediator::getGlobalMediator()->getAudioPlayer()->playAudio(
            url.replace(REPLACE_EXPRESSION, m_term->expression)
               .replace(REPLACE_READING, m_term->reading),
            hash
        );
    m_ui->buttonAudio->setEnabled(reply == nullptr);

    if (reply)
    {
        connect(reply, &AudioPlayerReply::result, this,
            [=] (const bool success) {
                if (!success)
                {
                    IconFactory *factory = IconFactory::create();
                    m_ui->buttonAudio->setIcon(
                        factory->getIcon(IconFactory::noaudio)
                    );
                }
                m_ui->buttonAudio->setEnabled(true);
            }
        );
    }
}

void TermWidget::showAudioSources(const QPoint &pos)
{
    QMenu contextMenu("Audio Sources", m_ui->buttonAudio);
    for (const AudioSource &src : *m_sources)
    {
        contextMenu.addAction(src.name, this,
            [=] {
                playAudio(src.url, src.md5);
            }
        );
    }
    contextMenu.exec(m_ui->buttonAudio->mapToGlobal(pos));
}

void TermWidget::searchKanji(const QString &ch)
{
    Kanji *kanji =
        GlobalMediator::getGlobalMediator()->getDictionary()->searchKanji(ch);
    if (kanji)
    {
        kanji->title       = m_term->title;
        kanji->sentence    = m_term->sentence;
        kanji->startTime   = m_term->startTime;
        kanji->endTime     = m_term->endTime;
        kanji->sentence2   = m_term->sentence2;
        kanji->clozePrefix = m_term->clozePrefix;
        kanji->clozeBody   = m_term->clozeBody;
        kanji->clozeSuffix = m_term->clozeSuffix;
        Q_EMIT kanjiSearched(kanji);
    }
}

/* End User Input Handlers */
/* Begin Setters */

void TermWidget::setAddable(bool value)
{
    m_ui->buttonAddCard->setVisible(value);
    m_ui->buttonAnkiOpen->setVisible(!value);
    for (size_t i = 0; i < m_layoutGlossary->count(); ++i)
    {
        GlossaryWidget *widget =
            (GlossaryWidget *)m_layoutGlossary->itemAt(i)->widget();
        widget->setCheckable(value);
    }
}

/* End Setters */
/* Begin Helpers */

inline QString TermWidget::generateJishoLink(const QString &exp)
{
    return
        QString("<a href=\"https://jisho.org/search/%1\">Jisho</a>").arg(exp);
}

#define KANJI_UNICODE_LOWER_COMMON  "\u4e00"
#define KANJI_UNICODE_UPPER_COMMON  "\u9faf"
#define KANJI_UNICODE_LOWER_RARE    "\u3400"
#define KANJI_UNICODE_UPPER_RARE    "\u4dbf"

inline bool TermWidget::isKanji(const QString &ch)
{
    return
        ch >= KANJI_UNICODE_LOWER_COMMON && ch <= KANJI_UNICODE_UPPER_COMMON ||
        ch >= KANJI_UNICODE_LOWER_RARE   && ch <= KANJI_UNICODE_UPPER_RARE;
}

#undef KANJI_UNICODE_LOWER_COMMON
#undef KANJI_UNICODE_UPPER_COMMON
#undef KANJI_UNICODE_LOWER_RARE
#undef KANJI_UNICODE_UPPER_RARE

/* End Helpers */
