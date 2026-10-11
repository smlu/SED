#include "ui/components/about_dialog.hpp"

#include "app_icon.hpp"
#include "app_version.hpp"

#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace sed::ui
{

QString aboutVersionText()
{
    return QString::fromLatin1(app::kDisplayVersion);
}

QString aboutHomepageUrl()
{
    return QString::fromLatin1(app::kHomepageUrl);
}

QString aboutQtVersionText()
{
    return QString::fromLatin1(qVersion());
}

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("aboutDialog"));
    setWindowTitle(QStringLiteral("About SED"));
    setWindowIcon(loadApplicationIcon());
    setWindowFlags(
        Qt::Dialog
        | Qt::CustomizeWindowHint
        | Qt::WindowTitleHint
        | Qt::WindowCloseButtonHint
        | Qt::MSWindowsFixedSizeDialogHint);
    setModal(true);
    setFixedSize(520, 410);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(30, 28, 30, 24);
    outer->setSpacing(20);

    auto* hero = new QHBoxLayout();
    hero->setSpacing(20);

    auto* icon = new QLabel(this);
    icon->setObjectName(QStringLiteral("aboutAppIcon"));
    icon->setFixedSize(92, 92);
    icon->setAlignment(Qt::AlignCenter);
    icon->setPixmap(loadApplicationIcon().pixmap(84, 84));
    icon->setAccessibleName(QStringLiteral("SED application icon"));
    hero->addWidget(icon, 0, Qt::AlignTop);

    auto* identityLayout = new QVBoxLayout();
    identityLayout->setSpacing(7);

    auto* title = new QLabel(QStringLiteral("SED"), this);
    title->setObjectName(QStringLiteral("aboutTitle"));
    title->setAccessibleName(QStringLiteral("SED"));
    identityLayout->addWidget(title);

    auto* subtitle = new QLabel(QStringLiteral("Level Editor"), this);
    subtitle->setObjectName(QStringLiteral("aboutSubtitle"));
    identityLayout->addWidget(subtitle);

    auto* version = new QLabel(aboutVersionText(), this);
    version->setObjectName(QStringLiteral("aboutVersion"));
    version->setAlignment(Qt::AlignCenter);
    version->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    version->setAccessibleName(QStringLiteral("Application version"));
    identityLayout->addWidget(version, 0, Qt::AlignLeft);
    identityLayout->addStretch(1);
    hero->addLayout(identityLayout, 1);
    outer->addLayout(hero);

    auto* divider = new QFrame(this);
    divider->setObjectName(QStringLiteral("aboutDivider"));
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Plain);
    outer->addWidget(divider);

    auto* description = new QLabel(
        QStringLiteral(
            "A modern level editor for Jedi Knight: Dark Forces II, "
            "Mysteries of the Sith, and Indiana Jones and the Infernal Machine."),
        this);
    description->setObjectName(QStringLiteral("aboutDescription"));
    description->setWordWrap(true);
    description->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    outer->addWidget(description);

    auto* homepageCard = new QFrame(this);
    homepageCard->setObjectName(QStringLiteral("aboutHomepageCard"));
    auto* homepageLayout = new QHBoxLayout(homepageCard);
    homepageLayout->setContentsMargins(14, 10, 14, 10);
    homepageLayout->setSpacing(10);

    auto* homepageCaption = new QLabel(QStringLiteral("Project homepage"), homepageCard);
    homepageCaption->setObjectName(QStringLiteral("aboutHomepageCaption"));
    homepageLayout->addWidget(homepageCaption);

    auto* homepage = new QLabel(homepageCard);
    homepage->setObjectName(QStringLiteral("aboutHomepage"));
    homepage->setTextFormat(Qt::RichText);
    homepage->setTextInteractionFlags(Qt::TextBrowserInteraction);
    homepage->setOpenExternalLinks(true);
    homepage->setText(
        QStringLiteral("<a href=\"%1\">github.com/smlu/SED &#8599;</a>")
            .arg(aboutHomepageUrl().toHtmlEscaped()));
    homepage->setToolTip(aboutHomepageUrl());
    homepage->setAccessibleName(QStringLiteral("SED project homepage"));
    homepageLayout->addWidget(homepage);
    homepageLayout->addStretch(1);
    outer->addWidget(homepageCard);

    auto* qtAttribution = new QLabel(this);
    qtAttribution->setObjectName(QStringLiteral("aboutQtAttribution"));
    qtAttribution->setTextFormat(Qt::RichText);
    qtAttribution->setTextInteractionFlags(Qt::TextBrowserInteraction);
    qtAttribution->setOpenExternalLinks(true);
    qtAttribution->setText(
        QStringLiteral(
            "Built with Qt %1 under the "
            "<a href=\"https://www.qt.io/licensing/open-source-lgpl-obligations\">LGPL v3</a>.")
            .arg(aboutQtVersionText().toHtmlEscaped()));
    qtAttribution->setToolTip(
        QStringLiteral("Qt %1; GNU Lesser General Public License version 3")
            .arg(aboutQtVersionText()));
    qtAttribution->setAccessibleName(QStringLiteral("Qt version and license attribution"));
    outer->addWidget(qtAttribution);
    outer->addStretch(1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->setObjectName(QStringLiteral("aboutButtons"));
    if (QPushButton* close = buttons->button(QDialogButtonBox::Close))
    {
        close->setDefault(true);
        close->setAutoDefault(true);
    }
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    setStyleSheet(QStringLiteral(R"(
        QDialog#aboutDialog {
            background-color: #191d23;
        }
        QLabel#aboutAppIcon {
            background-color: #111419;
            border: 1px solid #353d48;
            border-radius: 18px;
        }
        QLabel#aboutTitle {
            color: #f5f7fa;
            font-size: 30px;
            font-weight: 700;
        }
        QLabel#aboutSubtitle {
            color: #aeb7c4;
            font-size: 14px;
        }
        QLabel#aboutVersion {
            color: #ffd8d8;
            background-color: #54252a;
            border: 1px solid #8d3b43;
            border-radius: 10px;
            padding: 3px 10px;
            font-weight: 600;
        }
        QFrame#aboutDivider {
            color: #343b45;
            background-color: #343b45;
            max-height: 1px;
        }
        QLabel#aboutDescription {
            color: #cbd2db;
            font-size: 13px;
            line-height: 1.35;
        }
        QFrame#aboutHomepageCard {
            background-color: #222831;
            border: 1px solid #37404c;
            border-radius: 8px;
        }
        QLabel#aboutHomepageCaption {
            color: #909ba9;
        }
        QLabel#aboutHomepage a {
            color: #ff8f92;
            font-weight: 600;
            text-decoration: none;
        }
        QLabel#aboutQtAttribution {
            color: #909ba9;
            font-size: 12px;
        }
        QLabel#aboutQtAttribution a {
            color: #c1cad6;
            text-decoration: none;
        }
        QPushButton {
            min-width: 86px;
            min-height: 28px;
        }
    )"));
}

} // namespace sed::ui
