#include "main_window/common.hpp"

namespace sed
{

void MainWindow::setupEditCoreActions(QMenu* editMenu)
{
    if (!editMenu)
    {
        return;
    }

    // Step 1: create history and delete actions before submenus reuse them.
    setupEditHistoryActions();
    setupEditDeleteAction();

    // Step 2: add global Edit menu actions that are not tied to a specific entity submenu.
    editMenu->addAction(undoAction_);
    editMenu->addAction(redoAction_);
    editMenu->addSeparator();
    setupEditClipboardActions(editMenu);
    editMenu->addSeparator();

    auto* preferencesAction = new QAction("&Preferences...", this);
    preferencesAction->setObjectName("edit.preferences");
    preferencesAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    connect(preferencesAction, &QAction::triggered, this, [this]()
        {
            showPreferencesDialog();
        });
    editMenu->addAction(preferencesAction);

    findAction_ = new QAction("&Find...", this);
    findAction_->setObjectName("edit.find");
    findAction_->setShortcut(QKeySequence::Find);
    findAction_->setShortcutContext(Qt::ApplicationShortcut);
    connect(findAction_, &QAction::triggered, this, &MainWindow::showLevelQueryDialog);
    editMenu->addAction(findAction_);
    this->addAction(findAction_);

    findNextAction_ = new QAction("Find &Next", this);
    findNextAction_->setObjectName("edit.find_next");
    // JED reserves F3 for Grid to View. Keep Find Next collision-free while
    // retaining a familiar sequential-search binding.
    findNextAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    findNextAction_->setShortcutContext(Qt::ApplicationShortcut);
    connect(findNextAction_, &QAction::triggered, this, &MainWindow::findNextLevelQuery);
    editMenu->addAction(findNextAction_);
    this->addAction(findNextAction_);
}

} // namespace sed
