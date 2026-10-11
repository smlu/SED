#pragma once

#include <QDialog>
#include <QString>

namespace sed::ui
{

QString aboutVersionText();
QString aboutHomepageUrl();
QString aboutQtVersionText();

class AboutDialog final : public QDialog
{
public:
    explicit AboutDialog(QWidget* parent = nullptr);
};

} // namespace sed::ui
